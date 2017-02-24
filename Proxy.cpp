#include "Proxy.h"
#include "JavaScriptRequests.h"
#include "utils.h"
#include "logger.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>
#include <fstream>

namespace JSBridge
{

namespace
{

void injectWPEQuery(JSGlobalContextRef context)
{
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    JSRetainPtr<JSStringRef> queryStr = adopt(JSStringCreateWithUTF8CString("wpeQuery"));
    JSValueRef wpeObject = JSObjectMakeFunctionWithCallback(context,
        queryStr.get(), JSBridge::onJavaScriptBridgeRequest);

    JSValueRef exc = 0;
    JSObjectSetProperty(context, windowObject, queryStr.get(), wpeObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);

    if (exc)
    {
        RDKLOG_ERROR("Could not set property wpeQuery!");
    }
}

void injectServiceManager(JSGlobalContextRef context)
{
    JSObjectRef windowObject = JSContextGetGlobalObject(context);
    JSRetainPtr<JSStringRef> serviceManagerStr = adopt(JSStringCreateWithUTF8CString("ServiceManager"));

    const char* jsFile = "/usr/share/injectedbundle/ServiceManager.js";
    std::string content;
    if (!Utils::readFile(jsFile, content))
    {
        RDKLOG_ERROR("Error: Could not read file %s!", jsFile);
        return;
    }

    JSValueRef exc = 0;
    (void) Utils::evaluateUserScript(context, content, &exc);
    if (exc)
    {
        RDKLOG_ERROR("Could not evaluate user script %s!", jsFile);
        return;
    }

    if (JSObjectHasProperty(context, windowObject, serviceManagerStr.get()) != true)
    {
        RDKLOG_ERROR("Could not find ServiceManager object!");
        return;
    }

    JSValueRef smObject = JSObjectGetProperty(context, windowObject, serviceManagerStr.get(), &exc);
    if (exc)
    {
        RDKLOG_ERROR("Could not get property ServiceManager!");
        return;
    }

    JSRetainPtr<JSStringRef> sendQueryStr = adopt(JSStringCreateWithUTF8CString("sendQuery"));
    JSValueRef sendQueryObject = JSObjectMakeFunctionWithCallback(context,
        sendQueryStr.get(), JSBridge::onJavaScriptServiceManagerRequest);

    JSObjectSetProperty(context, (JSObjectRef) smObject, sendQueryStr.get(), sendQueryObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exc);

    if (exc)
    {
        RDKLOG_ERROR("Could not set property ServiceManager.sendQuery!");
    }
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
{
}

void Proxy::didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame)
{
    if (WKBundlePageGetMainFrame(page) != frame)
    {
        RDKLOG_WARNING("Frame is not allowed to inject JavaScript window objects!");
        return;
    }

    // Always inject wpeQuery and ServiceManager to be visible in JavaScript.
    auto context = WKBundleFrameGetJavaScriptContext(frame);
    injectWPEQuery(context);
    injectServiceManager(context);
}

void Proxy::sendQuery(const char* name, JSContextRef ctx,
    JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError)
{
    WKRetainPtr<WKStringRef> mesRef = adoptWK(WKStringCreateWithJSString(messageRef));
    std::string message = Utils::toStdString(mesRef.get());
    uint64_t callID = ++m_lastCallID;

    auto cb = new QueryCallbacks(onSuccess, onError);
    cb->protect(ctx);

    m_queries[callID].reset(cb);

    sendMessageToClient(name, message.c_str(), callID);
}

void Proxy::onMessageFromClient(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "onJavaScriptBridgeResponse"))
    {
        onJavaScriptBridgeResponse(page, messageBody);
        return;
    }

    RDKLOG_ERROR("Unknown message name!");
}

void Proxy::onJavaScriptBridgeResponse(WKBundlePageRef page, WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKArrayGetTypeID())
    {
        RDKLOG_ERROR("Message body must be array!");
        return;
    }

    uint64_t callID = WKUInt64GetValue((WKUInt64Ref) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 0));
    bool success = WKBooleanGetValue((WKBooleanRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 1));
    std::string message = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 2));

    RDKLOG_INFO("callID=%llu succes=%d message='%s'", callID, success, message.c_str());

    auto it = m_queries.find(callID);
    if (it == m_queries.end())
    {
        RDKLOG_ERROR("callID=%llu not found", callID);
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
    RDKLOG_INFO("name=%s callID=%llu message=\'%s\'", name, callID, message);

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