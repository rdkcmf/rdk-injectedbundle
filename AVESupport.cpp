#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKString.h>
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
DLL_PUBLIC void SetSurfacePos(void* surface, int x, int y) {}
DLL_PUBLIC void SetSurfaceSize(void* surface, int width, int height) {}
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

    virtual PSDKErrorCode getInterface(InterfaceId id, void **retValue)
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
        printf("initIARM succeded\n");

    }
    catch (...)
    {
        fprintf(stderr, "IARM: Failed to initialize device::Manager\n");
        // TODO: post corresponding error message to JavaScript console
        s_wk.m_IARMinitialized = false;
    }

    return s_wk.m_IARMinitialized;
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
    if (initIARM())
    {
        enable(true); // TODO: remove to setAVEEnabled message handler
    }
}

void enable(bool on = true)
{
    if (on)
    {
        // RFU: will be used by setAVEEnable
        if (!s_wk.m_IARMinitialized)
        {
            printf("[InjectedBundle] [ERROR] Can't enable AVE : IARM is not initialized\n");
            return;
        }
    }

    s_wk.m_enabled = on;
}

// RFU
bool enabled()
{
    return s_wk.m_enabled;
}

void onCreatePage(WKBundlePageRef page)
{
    printf("[InjectedBundle] AVESupport::onCreatePage\n");

    injectUserScript(page, "/usr/share/injectedbundle/AVEXREReceiverBridge.js");

    if (enabled())
    {
        printf("[InjectedBundle] Injected AVEXREReceiverBridge.js\n");

        WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
        JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);

        loadAVEJavaScriptBindings(context);
    }
}


} // namespace