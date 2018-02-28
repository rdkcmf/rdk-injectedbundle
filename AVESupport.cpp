/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKString.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKBundlePagePrivate.h>

#ifdef USE_INTELCE
#include <gdl.h>
#endif

#include <dlfcn.h>

#include "AVESupport.h"
#include "logger.h"
#include "utils.h"
#include <fstream>
#include <mutex>

#include <curl/curl.h>
#include <glib.h>
#include <kernel/Callbacks.h>
#include <psdk/PSDKEvents.h>
#include <psdkutils/PSDKError.h>

#include <rdk/ds/manager.hpp>

#ifdef USE_NX_CLIENT
#include "nexus_config.h"
#include "nxclient.h"
#endif

extern "C" {

#include <rdk/iarmbus/libIBus.h>
#include <rdk/iarmbus/libIBusDaemon.h>
#include <rdk/iarmbus/libIARMCore.h>

}

#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__ ((visibility ("default")))
#endif

DLL_PUBLIC void* CreateSurface() {  return NULL; }
DLL_PUBLIC void SetSurfacePos(void*, int, int) {}
DLL_PUBLIC void SetSurfaceSize(void*, int, int) {}
DLL_PUBLIC void GetSurfaceScale(double *pScaleX, double *pScaleY)
{
#ifdef USE_INTELCE
    gdl_display_info_t display_info;
    gdl_ret_t rc = GDL_SUCCESS;
    rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &display_info);
    RDKLOG_TRACE("return code: %d, width: %f, height: %f \n", rc, display_info.tvmode.width / 1280.0, display_info.tvmode.height / 720.0);

    if(rc)
    {
        *pScaleX = 1.0;
        *pScaleY = 1.0;
        return;
    }

    *pScaleX = display_info.tvmode.width / 1280.0;
    *pScaleY = display_info.tvmode.height / 720.0;
    return;
#endif

#ifdef USE_NX_CLIENT
    // Special case for SD resolution, upscale to match the video surface size.
    NxClient_DisplaySettings displaySettings;
    NxClient_GetDisplaySettings(&displaySettings);
    if (displaySettings.format == NEXUS_VideoFormat_e480p || displaySettings.format == NEXUS_VideoFormat_eNtsc)
    {
        *pScaleX = 1280.0 / 640.0;
        *pScaleY = 720.0 / 480.0;
        return;
    }
#endif

    *pScaleX = 1.0;
    *pScaleY = 1.0;
}

using namespace psdk;
using namespace psdkutils;

static GSourceFuncs gEventDispatcherSourceFuncs =
{
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer data) -> gboolean
    {
        if (g_source_get_ready_time(source) != -1)
        {
            g_source_set_ready_time(source, -1);
            return callback(data);
        }
        return G_SOURCE_CONTINUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

class DLL_PUBLIC WPECallbackManager :  public psdk::PSDKEventManager
{
    GSource* m_source;
public:
    WPECallbackManager()
        :  _refCount(0)
    {
        m_source = g_source_new(&gEventDispatcherSourceFuncs, sizeof(GSource));
        g_source_set_name(m_source, "PSDK Event dispatcher");
        g_source_set_can_recurse(m_source, TRUE);
        g_source_set_callback(m_source, [](gpointer data) -> gboolean {
            WPECallbackManager &self = *static_cast<WPECallbackManager*>(data);
            self.firePostedEvents();
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_set_priority(m_source, G_PRIORITY_HIGH + 30); // Should match WebCore's shared timer priority
        g_source_attach(m_source, g_main_context_default());
    }

    virtual ~WPECallbackManager()
    {
        g_source_destroy(m_source);
    }

    virtual void eventPosted()
    {
        g_source_set_ready_time(m_source, 0);
    }

    virtual PSDKErrorCode getInterface(InterfaceId, void **)
    {
        return kECInterfaceNotFound;
    }

    PSDKSHAREDPOINTER_METHODS;
    PSDKUSERDATA_METHODS;
    GETTOPLEVELIID_METHOD(PSDKEventManager);
};

DLL_PUBLIC psdk::PSDKEventManager* GetPlatformCallbackManager()
{
    static PSDKSharedPointer<WPECallbackManager> gCallbackManager = NULL;
    if (!gCallbackManager) {
        gCallbackManager = new WPECallbackManager();
    }
    return gCallbackManager;
}

extern "C"
{

    typedef void (*loggerCallback)(const char* strPrefix, const AVELogLevel level, const char* logData);

    void loadAVEJavaScriptBindings(void* context);
    void unloadAVEJavaScriptBindings(void* context);
    void setComcastSessionToken(const char* token);
    void setCCHandleDirectMode(bool on);
    void setPSDKLoggingCallback(loggerCallback cbLogger, const AVELogLevel level);
}

namespace AVESupport
{

struct AVEWk
{
    bool m_enabled;
    bool m_IARMinitialized;
    WKBundlePageRef m_client;
    AVELogLevel m_logLevel;

    AVEWk () : m_enabled(false), m_IARMinitialized(false), m_client(nullptr), m_logLevel(eWarning) {}

} s_wk;


bool initIARM()
{
    char Init_Str[] = "WPE_WEBKIT_PROCESS";
    IARM_Bus_Init(Init_Str);
    IARM_Bus_Connect();
    try
    {
        device::Manager::Initialize();
        s_wk.m_IARMinitialized = true;
        RDKLOG_INFO("initIARM succeded");

    }
    catch (...)
    {
        RDKLOG_ERROR("IARM: Failed to initialize device::Manager");
        // TODO: post corresponding error message to JavaScript console
        s_wk.m_IARMinitialized = false;
    }

    return s_wk.m_IARMinitialized;
}


void aveLogCallback(const char* prefix, const AVELogLevel level, const char* data)
{
    if (level >= s_wk.m_logLevel && s_wk.m_logLevel != eOff)
    {
        RDK::LogLevel rdkLogLvl;
        switch (level)
        {
            case eWarning:  rdkLogLvl = RDK::LogLevel::WARNING_LEVEL; break;
            case eError:    rdkLogLvl = RDK::LogLevel::ERROR_LEVEL; break;
            default:        rdkLogLvl = RDK::LogLevel::INFO_LEVEL; break;
        }

        _LOG(rdkLogLvl, "%s %s", prefix ?: "(null)", data ?: "(null)");
    }

    static bool checkSendAllOnce = true;
    static bool sendAllToBrowser = false;
    if(checkSendAllOnce)
    {
        checkSendAllOnce = false;
        const char* s = getenv("WPE_SEND_ALL_AVE_LOGS");
        if (s && strcasecmp(s,"TRUE")==0)
        {
            sendAllToBrowser = true;
        }
    }

    if (!s_wk.m_client)
        return;

    if(sendAllToBrowser == false)
    {
        if(level != eMetric)
            return;

        if (strstr(data, "HttpRequestEnd")==0 && strstr(data, "TuneTime")==0 && strstr(data, "---------> Resume")==0)
            return;
    }

    WKRetainPtr<WKStringRef> nameRef = adoptWK(WKStringCreateWithUTF8CString("onAVELog"));
    WKRetainPtr<WKStringRef> prefixRef = adoptWK(WKStringCreateWithUTF8CString(prefix));
    WKRetainPtr<WKUInt64Ref> levelRef = adoptWK(WKUInt64Create(level));
    WKRetainPtr<WKStringRef> dataRef = adoptWK(WKStringCreateWithUTF8CString(data));

    WKTypeRef params[] = {prefixRef.get(), levelRef.get(), dataRef.get()};
    WKRetainPtr<WKArrayRef> arrRef = adoptWK(WKArrayCreate(params, sizeof(params)/sizeof(params[0])));

    WKBundlePagePostMessage(s_wk.m_client, nameRef.get(), arrRef.get());
}

void injectUserScript(WKBundlePageRef page, const char* path)
{
    std::ifstream file;
    file.open(path);
    if (!file.is_open())
        return;

    std::string content;
    content.assign((std::istreambuf_iterator<char>(file)),(std::istreambuf_iterator<char>()));
    file.close();

    WKRetainPtr<WKStringRef> str = adoptWK(WKStringCreateWithUTF8CString(content.c_str()));
    WKBundlePageAddUserScript(page, str.get(), kWKInjectAtDocumentStart, kWKInjectInAllFrames);
}

void setAVELogLevel(uint64_t level)
{
    RDKLOG_INFO("setAVELogLevel %d", (int)level);
    s_wk.m_logLevel = static_cast<AVELogLevel>(level);
    setPSDKLoggingCallback(AVESupport::aveLogCallback, (AVELogLevel) level);
}

void installAVELoggingCallback()
{
    static std::once_flag flag;
    std::call_once(flag, [] () {
        setAVELogLevel(s_wk.m_logLevel);
    });
}

void initialize()
{
    RDKLOG_INFO("");
    curl_global_init(CURL_GLOBAL_ALL);
    RDKLOG_INFO("using curl:%s", curl_version());

    setCCHandleDirectMode(true);
    if (initIARM())
    {
        enable(true);
    }
}

void enable(bool on = true)
{
    if (on)
    {
        if (!s_wk.m_IARMinitialized)
        {
            RDKLOG_ERROR("Can't enable AVE : IARM is not initialized");
            return;
        }
    }

    s_wk.m_enabled = on;
}

bool enabled()
{
    return s_wk.m_enabled;
}

void didCreatePage(WKBundlePageRef page)
{
    RDKLOG_INFO("");
    injectUserScript(page, "/usr/share/injectedbundle/AVEXREReceiverBridge.js");
}

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame)
{
    RDKLOG_INFO("");
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    // we don't check for enabled() on purpose here
    if (mainFrame == frame)
    {
        JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
        unloadAVEJavaScriptBindings(context);
    }
}

void didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame)
{
    RDKLOG_INFO("");
    if (enabled())
    {
        WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
        if (mainFrame == frame)
        {
            JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
            loadAVEJavaScriptBindings(context);
            installAVELoggingCallback();
        }
    }
}

void onSetAVESessionToken(WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKStringGetTypeID())
    {
        RDKLOG_ERROR("Param must be a string.");
        return;
    }

    std::string token = Utils::toStdString((WKStringRef) messageBody);
    if (token.empty())
    {
        RDKLOG_ERROR("An empty AVE token was passed.");
        return;
    }

    RDKLOG_INFO("Got session token.");
    RDKLOG_TRACE("Token=%s", token.c_str());

    setComcastSessionToken(token.c_str());

    // Stop memory pressure handler for tune
    static int gResumePressureHandlerTag = 0;
    static int gTuneTimeoutSeconds = 20;
    if (gResumePressureHandlerTag)
        g_source_remove(gResumePressureHandlerTag);
    WKBundleMemoryPressureHandlerStop();
    gResumePressureHandlerTag = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT_IDLE, gTuneTimeoutSeconds, [](gpointer) -> gboolean {
        gResumePressureHandlerTag = 0;
        WKBundleMemoryPressureHandlerStart();
        return G_SOURCE_REMOVE;
    }, nullptr, nullptr);
}

void onSetAVEEnabled(WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKBooleanGetTypeID())
    {
        RDKLOG_ERROR("Unexpected param type.");
        return;
    }
    bool enableAVE = WKBooleanGetValue((WKBooleanRef) messageBody);
    enable(enableAVE);
    RDKLOG_INFO("AVE was %s", enableAVE ? "enabled" : "disabled");
}

void onSetAVELogLevel(WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKUInt64GetTypeID())
    {
        RDKLOG_ERROR("Unexpected param type.");
        return;
    }

    uint64_t level = WKUInt64GetValue((WKUInt64Ref) messageBody);

    setAVELogLevel(level);
}

bool didReceiveMessageToPage(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "setAVESessionToken"))
    {
        onSetAVESessionToken(messageBody);
        return true;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "setAVEEnabled"))
    {
        onSetAVEEnabled(messageBody);
        return true;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "setAVELogLevel"))
    {
        onSetAVELogLevel(messageBody);
        return true;
    }
    return false;
}

void setClient(WKBundlePageRef bundle)
{
    s_wk.m_client = bundle;
}

} // namespace
