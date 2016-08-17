#include "Proxy.h"
#include "JavaScriptRequests.h"
#include "utils.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKURLRequest.h>
#include <fstream>

namespace JSBridge
{

namespace
{

const char* kServiceManagerJavaScriptName = "ServiceManager";

std::string toStdString(WKStringRef string)
{
    size_t size = WKStringGetMaximumUTF8CStringSize(string);
    auto buffer = std::make_unique<char[]>(size);
    size_t len = WKStringGetUTF8CString(string, buffer.get(), size);

    return std::string(buffer.get(), len - 1);
}

std::string frameURL(WKBundleFrameRef frame)
{
    WKRetainPtr<WKURLRef> copyRef = adoptWK(WKBundleFrameCopyURL(frame));
    if (!copyRef)
    {
        return "";
    }

    WKRetainPtr<WKStringRef> urlRef = adoptWK(WKURLCopyString(copyRef.get()));
    if (!urlRef)
    {
        return "";
    }

    return toStdString(urlRef.get());
}

void injectWPEQuery(WKBundlePageRef page, WKBundleFrameRef frame)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    JSRetainPtr<JSStringRef> queryStr = adopt(JSStringCreateWithUTF8CString("wpeQuery"));
    JSValueRef wpeObject = JSObjectMakeFunctionWithCallback(context,
        queryStr.get(), JSBridge::onJavaScriptBridgeRequest);

    JSValueRef exc = 0;
    JSObjectSetProperty(context, windowObject, queryStr.get(), wpeObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);

    if (exc)
    {
        fprintf(stderr, "Error: Could not set property wpeQuery!\n");
    }
}

bool enableServiceManager(WKBundlePageRef page, bool enable)
{
    JSValueRef exc = 0;
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef windowObject = JSContextGetGlobalObject(context);
    JSRetainPtr<JSStringRef> serviceManagerStr = adopt(JSStringCreateWithUTF8CString(kServiceManagerJavaScriptName));

    if (!enable)
    {
        if (JSObjectHasProperty(context, windowObject, serviceManagerStr.get()))
        {
            JSObjectDeleteProperty(context, windowObject, serviceManagerStr.get(), &exc);
            if (exc)
            {
                fprintf(stderr, "Error: Could not delete property ServiceManager!\n");
                return false;
            }
        }

        return true;
    }

    const char* jsFile = "/usr/share/injectedbundle/ServiceManager.js";
    std::string content;
    if (!Utils::readFile(jsFile, content))
    {
        fprintf(stderr, "Error: Could not read file %s!\n", jsFile);
        return false;
    }

    (void) Utils::evaluateUserScript(context, content, &exc);
    if (exc)
    {
        fprintf(stderr, "Error: Could not evaluate user script %s!\n", jsFile);
        return false;
    }

    if (exc)
    {
        fprintf(stderr, "Error: Could not evaluate user JavaScript!\n");
        return false;
    }

    if (JSObjectHasProperty(context, windowObject, serviceManagerStr.get()) != true)
    {
        fprintf(stderr, "Error: Could not find ServiceManager object!\n");
        return false;
    }

    JSValueRef smObject = JSObjectGetProperty(context, windowObject, serviceManagerStr.get(), &exc);
    if (exc)
    {
        fprintf(stderr, "Error: Could not get property ServiceManager!\n");
        return false;
    }

    JSRetainPtr<JSStringRef> sendQueryStr = adopt(JSStringCreateWithUTF8CString("sendQuery"));
    JSValueRef sendQueryObject = JSObjectMakeFunctionWithCallback(context,
        sendQueryStr.get(), JSBridge::onJavaScriptServiceManagerRequest);

    JSObjectSetProperty(context, (JSObjectRef) smObject, sendQueryStr.get(), sendQueryObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, &exc);

    if (exc)
    {
        fprintf(stderr, "Error: Could not set property ServiceManager.sendQuery!\n");
        return false;
    }

    return true;
}

} // namespace

struct QueryCallbacks
{
    QueryCallbacks(JSValueRef succ, JSValueRef err)
        : onSuccess(succ)
        , onError(err)
    {}

    void protect(JSContextRef ctx)
    {
        JSValueProtect(ctx, onSuccess);
        JSValueProtect(ctx, onError);
    }

    void unprotect(JSContextRef ctx)
    {
        JSValueUnprotect(ctx, onSuccess);
        JSValueUnprotect(ctx, onError);
    }

    JSValueRef onSuccess;
    JSValueRef onError;
};

Proxy::Proxy()
    : m_lastCallID(0)
    , m_enableServiceManager(false)
    , m_didCommitLoad(false)
{
}

void Proxy::processServiceManager(WKBundlePageRef page)
{
    std::string url = frameURL(WKBundlePageGetMainFrame(page));

    // If enableServiceManager is false, no ServiceManager object must exists
    if (m_enableServiceManager && !m_acl.isServiceManagerAllowed(url))
    {
        fprintf(stdout, "%s:%d Service manager is not allowed to be used for url '%s'!\n", __func__, __LINE__, url.c_str());
        return;
    }

    if (!enableServiceManager(page, m_enableServiceManager))
    {
        fprintf(stderr, "%s:%d Error: Could not %s ServiceManager manager '%s'!\n", __func__, __LINE__, m_enableServiceManager ? "enable" : "disable");
    }
}

void Proxy::didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame)
{
    if (WKBundlePageGetMainFrame(page) != frame)
    {
        fprintf(stdout, "%s:%d Frame is not allowed to inject JavaScript window objects!\n", __func__, __LINE__);
        return;
    }

    // Always inject wpeQuery to be visible in JavaScript.
    injectWPEQuery(page, frame);
    processServiceManager(page);
    m_didCommitLoad = true;

    // Sending an event to backend to notify that navigation is made
    // and new page is going to be loaded.
    sendMessageToClient("onNavigate", frameURL(frame).c_str());
}

void Proxy::sendJavaScriptBridgeRequest(JSContextRef ctx,
    JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError)
{
    sendQuery("onJavaScriptBridgeRequest", ctx, messageRef, onSuccess, onError);
}

void Proxy::sendJavaScriptServiceManagerRequest(JSStringRef serviceNameRef,
    JSContextRef ctx, JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError)
{
    std::string serviceName = toStdString(adoptWK(WKStringCreateWithJSString(serviceNameRef)).get());
    std::string url = frameURL(WKBundleFrameForJavaScriptContext(ctx));

    if (!m_acl.isServiceAllowed(serviceName, url))
    {
        fprintf(stdout, "%s:%d Service '%s' is not allowed for url '%s'!\n", __func__, __LINE__, serviceName.c_str(), url.c_str());
        return;
    }

    sendQuery("onJavaScriptServiceManagerRequest", ctx, messageRef, onSuccess, onError);
}

void Proxy::sendQuery(const char* name, JSContextRef ctx,
    JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError)
{
    WKRetainPtr<WKStringRef> mesRef = adoptWK(WKStringCreateWithJSString(messageRef));
    std::string message = toStdString(mesRef.get());
    uint64_t callID = ++m_lastCallID;

    auto cb = new QueryCallbacks(onSuccess, onError);
    cb->protect(ctx);

    m_queries[callID].reset(cb);

    sendMessageToClient(name, message.c_str(), callID);
}

void Proxy::onMessageFromClient(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "onEnableServiceManager"))
    {
        onEnableServiceManager(page, messageBody);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "onServicesACL"))
    {
        onServicesACL(messageBody);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "onJavaScriptBridgeResponse"))
    {
        onJavaScriptBridgeResponse(page, messageBody);
        return;
    }

    fprintf(stderr, "%s:%d Error: Unknown message name!\n", __func__, __LINE__);
}

void Proxy::onEnableServiceManager(WKBundlePageRef page, WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKBooleanGetTypeID())
    {
        fprintf(stderr, "%s:%d Error: Message body must be boolean!\n", __func__, __LINE__);
        return;
    }

    m_enableServiceManager = WKBooleanGetValue((WKBooleanRef) messageBody);

    // If document has been already processed,
    // need to handle ServiceManager explicitly.
    // Otherwise need to wait when the page is ready to execute JavaScript.
    if (m_didCommitLoad)
    {
        processServiceManager(page);
    }
}

void Proxy::onServicesACL(WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKStringGetTypeID())
    {
        fprintf(stderr, "%s:%d Error: Message body must be string!\n", __func__, __LINE__);
        return;
    }

    std::string acl = toStdString((WKStringRef) messageBody);
    if (!acl.empty() && !m_acl.set(acl))
    {
        fprintf(stderr, "%s:%d Error: Could not set ACL!\n", __func__, __LINE__);
    }
}

void Proxy::onJavaScriptBridgeResponse(WKBundlePageRef page, WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKArrayGetTypeID())
    {
        fprintf(stderr, "%s:%d Error: Message body must be array!\n", __func__, __LINE__);
        return;
    }

    uint64_t callID = WKUInt64GetValue((WKUInt64Ref) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 0));
    bool success = WKBooleanGetValue((WKBooleanRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 1));
    std::string message = toStdString((WKStringRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 2));

    fprintf(stdout, "%s:%d callID=%llu succes=%d message=%s\n", __func__, __LINE__, callID, success, message.c_str());

    auto it = m_queries.find(callID);
    if (it == m_queries.end())
    {
        fprintf(stderr, "%s:%d Error: callID=%llu not found\n", __func__, __LINE__, callID);
        return;
    }

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(page));
    JSValueRef cb = success ? it->second->onSuccess : it->second->onError;

    const size_t argc = 1;
    JSValueRef argv[argc];
    JSRetainPtr<JSStringRef> string = adopt(JSStringCreateWithUTF8CString(message.c_str()));
    argv[0] = JSValueMakeString(context, string.get());
    (void) JSObjectCallAsFunction(context, (JSObjectRef) cb, nullptr, argc, argv, nullptr);

    it->second->unprotect(context);

    m_queries.erase(it);
}

void Proxy::sendMessageToClient(const char* name, const char* message, uint64_t callID)
{
    fprintf(stdout, "%s:%d name=%s callID=%llu message=%s\n", __func__, __LINE__, name, callID, message);

    WKRetainPtr<WKStringRef> nameRef = adoptWK(WKStringCreateWithUTF8CString(name));
    WKRetainPtr<WKUInt64Ref> callIDRef = adoptWK(WKUInt64Create(callID));
    WKRetainPtr<WKStringRef> bodyRef = adoptWK(WKStringCreateWithUTF8CString(message));

    WKTypeRef params[] = {callIDRef.get(), bodyRef.get()};
    WKRetainPtr<WKArrayRef> arrRef = adoptWK(WKArrayCreate(params, sizeof(params)/sizeof(params[0])));

    WKBundlePagePostMessage(m_client, nameRef.get(), arrRef.get());
}

Proxy& Proxy::singleton()
{
    static Proxy& singleton = *new Proxy();
    return singleton;
}

void Proxy::setClient(WKBundlePageRef bundle)
{
    m_client = bundle;
}

} // namespace WPEQuery