#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKString.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKBundlePagePrivate.h>

#include <dlfcn.h>

#include "AVESupport.h"
#include <fstream>

#include <glib.h>
#include <kernel/Callbacks.h>
#include <psdk/PSDK.h>
#include <psdkutils/PSDKError.h>

#include <rdk/ds/manager.hpp>
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
DLL_PUBLIC void GetSurfaceScale(double *pScaleX, double *pScaleY) {
    *pScaleX = 1.0;
    *pScaleY = 1.0;
}

using namespace psdk;
using namespace psdkutils;

class DLL_PUBLIC WPECallbackManager :  public psdk::PSDKEventManager
{
public:
    WPECallbackManager()
        :  _refCount(0) {}
    virtual ~WPECallbackManager() {}
    virtual void eventPosted()
    {
        g_timeout_add(0, [](gpointer data) -> gboolean {
          WPECallbackManager &self = *static_cast<WPECallbackManager*>(data);
          self.firePostedEvents();
          return G_SOURCE_REMOVE;
        }, this);
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
    void loadAVEJavaScriptBindings(void* context);
    void unloadAVEJavaScriptBindings(void* context);
    void setComcastSessionToken(const char* token);
}

namespace AVESupport
{

struct AVEWk
{
    bool m_enabled;
    bool m_IARMinitialized;

    AVEWk () : m_enabled(false), m_IARMinitialized(false) {}

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
        printf("[InjectedBundle] AVESupport::%s:%d initIARM succeded\n", __FUNCTION__, __LINE__);

    }
    catch (...)
    {
        printf("[InjectedBundle] AVESupport::%s:%d IARM: Failed to initialize device::Manager\n", __FUNCTION__, __LINE__);
        // TODO: post corresponding error message to JavaScript console
        s_wk.m_IARMinitialized = false;
    }

    return s_wk.m_IARMinitialized;
}

// FIXME: this should be moved to utils.h upon renaming the namespaces in this file
std::string toStdString(WKStringRef string)
{
    size_t size = WKStringGetMaximumUTF8CStringSize(string);
    auto buffer = std::make_unique<char[]>(size);
    size_t len = WKStringGetUTF8CString(string, buffer.get(), size);

    return std::string(buffer.get(), len - 1);
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

void initialize()
{
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
    if (initIARM())
    {
        enable(true); // TODO: remove to setAVEEnabled message handler
    }
}

void enable(bool on = true)
{
    if (on)
    {
        if (!s_wk.m_IARMinitialized)
        {
            printf("[InjectedBundle] AVESupport::%s:%d [ERROR] Can't enable AVE : IARM is not initialized\n",  __FUNCTION__, __LINE__);
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
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
    injectUserScript(page, "/usr/share/injectedbundle/AVEXREReceiverBridge.js");
}

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame)
{
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
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
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
    if (enabled())
    {
        WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
        if (mainFrame == frame)
        {
            JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
            loadAVEJavaScriptBindings(context);
        }
    }
}

void onSetAVESessionToken(WKTypeRef messageBody)
{
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
    if (WKGetTypeID(messageBody) != WKStringGetTypeID())
    {
        printf("[InjectedBundle] AVESupport::%s:%d [ERROR] Param must be a string.\n", __FUNCTION__, __LINE__);
        return;
    }

    std::string token = toStdString((WKStringRef) messageBody);
    if (token.empty())
    {
        printf("[InjectedBundle] AVESupport::%s:%d [ERROR] An empty AVE token was passed.\n", __FUNCTION__, __LINE__);
        return;
    }

    setComcastSessionToken(token.c_str());

    printf("[InjectedBundle] token=%s\n", token.c_str());
}

void onSetAVEEnabled(WKTypeRef messageBody)
{
    printf("[InjectedBundle] AVESupport::%s:%d\n", __FUNCTION__, __LINE__);
    if (WKGetTypeID(messageBody) != WKBooleanGetTypeID())
    {
        printf("[InjectedBundle] AVESupport::%s:%d [ERROR] Unexpected param type.\n", __FUNCTION__, __LINE__);
        return;
    }
    bool enableAVE = WKBooleanGetValue((WKBooleanRef) messageBody);
    enable(enableAVE);
    printf("\n[InjectedBundle] AVE was %s\n", enableAVE ? "enabled" : "disabled");
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
    return false;
}

} // namespace
