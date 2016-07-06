#include "BundleController.h"
#include "Proxy.h"
#include "JavaScriptRequests.h"

#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>
#include <WebKit/WKString.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKBundlePagePrivate.h>

#include <fstream>

namespace
{

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

void installObjects(WKBundlePageRef page, WKBundleFrameRef frame)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    JSRetainPtr<JSStringRef> queryStr = adopt(JSStringCreateWithUTF8CString("wpeQuery"));
    JSValueRef wpeObject = JSObjectMakeFunctionWithCallback(context,
        queryStr.get(), JSBridge::onJavaScriptBridgeRequest);

    JSValueRef exc = 0;
    JSObjectSetProperty(context, windowObject, queryStr.get(), wpeObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);

    JSRetainPtr<JSStringRef> smStr = adopt(JSStringCreateWithUTF8CString("ServiceManager"));
    JSValueRef smObject = JSObjectMakeFunctionWithCallback(context, smStr.get(), nullptr);

    JSObjectSetProperty(context, windowObject, smStr.get(), smObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);

    JSRetainPtr<JSStringRef> sendQueryStr = adopt(JSStringCreateWithUTF8CString("sendQuery"));
    JSValueRef sendQueryObject = JSObjectMakeFunctionWithCallback(context,
        sendQueryStr.get(), JSBridge::onJavaScriptServiceManagerRequest);

    JSObjectSetProperty(context, smObject, sendQueryStr.get(), sendQueryObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);
}

void didCommitLoad(WKBundlePageRef page,
    WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo)
{
    (void) clientInfo;
    (void) userData;
    installObjects(page, frame);
}

void didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    (void) clientInfo;

    JSBridge::Proxy::singleton().setClient(page);
    WKBundlePageLoaderClientV0 client {
        {0, clientInfo},
        // Version 0.
        0, // didStartProvisionalLoadForFrame;
        0, // didReceiveServerRedirectForProvisionalLoadForFrame;
        0, // didFailProvisionalLoadWithErrorForFrame;
        didCommitLoad, // didCommitLoadForFrame;
        0, // didFinishDocumentLoadForFrame;
        0, // didFinishLoadForFrame;
        0, // didFailLoadWithErrorForFrame;
        0, // didSameDocumentNavigationForFrame;
        0, // didReceiveTitleForFrame;
        0, // didFirstLayoutForFrame;
        0, // didFirstVisuallyNonEmptyLayoutForFrame;
        0, // didRemoveFrameFromHierarchy;
        0, // didDisplayInsecureContentForFrame;
        0, // didRunInsecureContentForFrame;
        0, // didClearWindowObjectForFrame;
        0, // didCancelClientRedirectForFrame;
        0, // willPerformClientRedirectForFrame;
        0, // didHandleOnloadEventsForFrame;
    };

    WKBundlePageSetPageLoaderClient(page, &client.base);
    injectUserScript(page, "/usr/share/injectedbundle/ServiceManager.js");
}

void didReceiveMessageToPage(WKBundleRef bundle,
    WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void* info)
{
    (void) bundle;
    (void) info;
    JSBridge::Proxy::singleton().onMessageFromClient(page, messageName, messageBody);
}

} // namespace

namespace JSBridge
{

void initialize(WKBundleRef bundleRef, WKTypeRef unusedUserData)
{
    (void) unusedUserData;

    WKBundleClientV1 client = {
        {1, nullptr},
        didCreatePage,
        nullptr, // willDestroyPage
        nullptr, // didInitializePageGroup
        nullptr, // didReceiveMessage
        didReceiveMessageToPage
    };

    WKBundleSetClient(bundleRef, &client.base);
}

} // JSBridge