#include "Proxy.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>

#include <stddef.h>
#include <cstdio>
#include <iterator>
#include <sstream>

namespace JSBridge
{

namespace
{

std::string toStdString(WKStringRef string)
{
    size_t size = WKStringGetMaximumUTF8CStringSize(string);
    auto buffer = std::make_unique<char[]>(size);
    size_t len = WKStringGetUTF8CString(string, buffer.get(), size);

    return std::string(buffer.get(), len - 1);
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

Proxy::Proxy() : m_lastCallID(0)
{
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

    sendMessageToClient(name, callID, message);
}

void Proxy::setClient(WKBundlePageRef bundle)
{
    m_client = bundle;
}

void Proxy::onMessageFromClient(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKGetTypeID(messageBody) != WKArrayGetTypeID())
    {
        fprintf(stderr, "Proxy::%s:%d Error: Message body must be an array!\n", __FUNCTION__, __LINE__);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "JavaScriptBridgeResponse") == false)
    {
        fprintf(stderr, "Proxy::%s:%d Error: Unknown message name!\n", __FUNCTION__, __LINE__);
        return;
    }

    uint64_t callID = WKUInt64GetValue((WKUInt64Ref) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 0));
    bool success = WKBooleanGetValue((WKBooleanRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 1));
    std::string message = toStdString((WKStringRef) WKArrayGetItemAtIndex((WKArrayRef) messageBody, 2));
    printf("Proxy::%s:%d callID=%llu succes=%d message=%s\n", __FUNCTION__, __LINE__, callID, success, message.c_str());

    auto it = m_queries.find(callID);
    if (it != m_queries.end()) {
        WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
        JSGlobalContextRef ctx = WKBundleFrameGetJavaScriptContext(frame);
        JSValueRef cb = success ? it->second->onSuccess : it->second->onError;

        const size_t argc = 1;
        JSValueRef argv[argc];
        JSRetainPtr<JSStringRef> string = adopt(JSStringCreateWithUTF8CString(message.c_str()));
        argv[0] = JSValueMakeString(ctx, string.get());
        JSObjectCallAsFunction(ctx, (JSObjectRef) cb, nullptr, argc, argv, nullptr);

        it->second->unprotect(ctx);

        m_queries.erase(it);
    }
    else
    {
        fprintf(stderr, "Proxy::%s:%d Error: callID=%llu not found\n", __FUNCTION__, __LINE__, callID);
    }
}

Proxy& Proxy::singleton()
{
    static Proxy& singleton = *new Proxy();
    return singleton;
}

void Proxy::sendMessageToClient(const char* name, uint64_t callID, const std::string& message)
{
    printf("Proxy::%s:%d name=%s callID=%llu message=%s\n", __FUNCTION__, __LINE__, name, callID, message.c_str());
    WKRetainPtr<WKStringRef> nameRef = adoptWK(WKStringCreateWithUTF8CString(name));
    WKRetainPtr<WKUInt64Ref> callIDRef = adoptWK(WKUInt64Create(callID));
    WKRetainPtr<WKStringRef> bodyRef = adoptWK(WKStringCreateWithUTF8CString(message.c_str()));

    const int argc = 2;
    WKTypeRef params[argc] = {callIDRef.get(), bodyRef.get()};
    WKRetainPtr<WKArrayRef> arrRef = adoptWK(WKArrayCreate(params, argc));

    WKBundlePagePostMessage(m_client, nameRef.get(), arrRef.get());
}

} // namespace WPEQuery