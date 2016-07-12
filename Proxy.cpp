#include "Proxy.h"
#include "JavaScriptRequests.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>

#include <fstream>

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

bool evaluateUserScript(JSContextRef context, const char* path, JSValueRef* exc)
{
    std::ifstream file;
    file.open(path);
    if (!file.is_open())
        return false;

    std::string content;
    content.assign((std::istreambuf_iterator<char>(file)),(std::istreambuf_iterator<char>()));
    file.close();

    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString(content.c_str()));
    JSEvaluateScript(context, str.get(), nullptr, nullptr, 0, exc);

    return true;
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
        printf("Error: Could not set property wpeQuery!\n");
    }
}

void enableServiceManager(WKBundlePageRef page, WKBundleFrameRef frame, bool enable)
{
    JSValueRef exc = 0;
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef windowObject = JSContextGetGlobalObject(context);
    JSRetainPtr<JSStringRef> serviceManagerStr = adopt(JSStringCreateWithUTF8CString("ServiceManager"));
    JSRetainPtr<JSStringRef> sendQueryStr = adopt(JSStringCreateWithUTF8CString("sendQuery"));

    if (!enable)
    {
        if (JSObjectHasProperty(context, windowObject, serviceManagerStr.get()))
        {
            JSObjectDeleteProperty(context, windowObject, serviceManagerStr.get(), &exc);
            if (exc)
            {
                printf("Error: Could not delete property ServiceManager!\n");
            }
        }

        return;
    }

    const char* jsFile = "/usr/share/injectedbundle/ServiceManager.js";
    if (evaluateUserScript(context, jsFile, &exc) != true)
    {
        printf("Error: Could not find path to user script %s!\n", jsFile);
        return;
    }

    if (exc)
    {
        printf("Error: Could not evaluate user JavaScript!\n");
        return;
    }

    if (JSObjectHasProperty(context, windowObject, serviceManagerStr.get()) != true)
    {
        printf("Error: Could not find ServiceManager object!\n");
        return;
    }

    JSValueRef smObject = JSObjectGetProperty(context, windowObject, serviceManagerStr.get(), &exc);
    if (exc)
    {
        printf("Error: Could not get property ServiceManager!\n");
        return;
    }

    JSValueRef sendQueryObject = JSObjectMakeFunctionWithCallback(context,
        sendQueryStr.get(), JSBridge::onJavaScriptServiceManagerRequest);

    JSObjectSetProperty(context, smObject, sendQueryStr.get(), sendQueryObject,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum, &exc);

    if (exc)
    {
        printf("Error: Could not set property ServiceManager.sendQuery!\n");
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
    : m_lastCallID(0)
    , m_enableServiceManager(false)
    , m_processedServiceManager(false)
{
}

void Proxy::injectObjects(WKBundlePageRef page, WKBundleFrameRef frame)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    injectWPEQuery(page, frame);
    enableServiceManager(page, frame, m_enableServiceManager);
    m_processedServiceManager = true;
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
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);

    if (WKStringIsEqualToUTF8CString(messageName, "EnableServiceManager"))
    {
        if (WKGetTypeID(messageBody) != WKBooleanGetTypeID())
        {
            fprintf(stderr, "Proxy::%s:%d Error: Message body must be boolean!\n", __FUNCTION__, __LINE__);
            return;
        }

        m_enableServiceManager = WKBooleanGetValue((WKBooleanRef) messageBody);
        // If document has been already processed,
        // needs to process ServiceManager explicitly.
        // Otherwise need to wait when the page is ready to execute JavaScript.
        if (m_processedServiceManager)
        {
            enableServiceManager(page, frame, m_enableServiceManager);
        }

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
    if (it != m_queries.end())
    {
        JSValueRef cb = success ? it->second->onSuccess : it->second->onError;

        const size_t argc = 1;
        JSValueRef argv[argc];
        JSRetainPtr<JSStringRef> string = adopt(JSStringCreateWithUTF8CString(message.c_str()));
        argv[0] = JSValueMakeString(context, string.get());
        JSObjectCallAsFunction(context, (JSObjectRef) cb, nullptr, argc, argv, nullptr);

        it->second->unprotect(context);

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