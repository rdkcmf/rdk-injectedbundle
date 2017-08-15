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

JSValueRef injectObjectFromScript(JSGlobalContextRef context, const char* scriptName, const char* objectName)
{
    std::string content;
    std::string filePath = std::string("/usr/share/injectedbundle/") + scriptName;
    if (!Utils::readFile(filePath.c_str(), content))
    {
        RDKLOG_ERROR("Could not read file '%s'!", filePath.c_str());
        return 0;
    }

    JSValueRef exc = 0;
    (void) Utils::evaluateUserScript(context, content, &exc);
    if (exc)
    {
        RDKLOG_ERROR("Could not evaluate user script '%s'!", filePath.c_str());
        return 0;
    }

    JSObjectRef windowObject = JSContextGetGlobalObject(context);
    if (!windowObject)
    {
        RDKLOG_ERROR("Could not get global object (window)!");
        return 0;
    }

    JSRetainPtr<JSStringRef> objectNameStr = adopt(JSStringCreateWithUTF8CString(objectName));
    if (!JSObjectHasProperty(context, windowObject, objectNameStr.get()))
    {
        RDKLOG_ERROR("Could not find property wnndow.%s!", objectName);
        return 0;
    }

    JSValueRef object = JSObjectGetProperty(context, windowObject, objectNameStr.get(), &exc);
    if (exc)
    {
        RDKLOG_ERROR("Could not get property window.%s!", objectName);
        return 0;
    }

    return object;
}

void injectServiceManager(JSGlobalContextRef context)
{
    JSValueRef smObject = injectObjectFromScript(context, "ServiceManager.js", "ServiceManager");
    if (!smObject)
    {
        return;
    }

    JSRetainPtr<JSStringRef> sendQueryStr = adopt(JSStringCreateWithUTF8CString("sendQuery"));
    JSValueRef sendQueryObject = JSObjectMakeFunctionWithCallback(context,
        sendQueryStr.get(), JSBridge::onJavaScriptServiceManagerRequest);

    JSValueRef exc = 0;
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

    // Optionally inject XDB to be visible in JavaScript.
    if (getenv("WEBKIT_INSPECTOR_SERVER"))
    {
        injectObjectFromScript(context, "XDB.js", "XDB");
    }
}

void Proxy::sendQuery(const char* name, JSContextRef ctx,
    JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError)
{
    WKRetainPtr<WKStringRef> mesRef = adoptWK(WKStringCreateWithJSString(messageRef));
    std::string message = Utils::toStdString(mesRef.get());

    uint64_t callID = 0;
    if (!JSValueIsNull(ctx, onSuccess) || !JSValueIsNull(ctx, onError))
    {
        callID = ++m_lastCallID;

        auto cb = new QueryCallbacks(onSuccess, onError);
        cb->protect(ctx);

        m_queries[callID].reset(cb);
    }

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

    auto it = m_queries.find(callID);
    if (it == m_queries.end())
    {
        RDKLOG_ERROR("callID=%llu not found", callID);
        return;
    }

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(page));
    JSValueRef cb = success ? it->second->onSuccess : it->second->onError;

    if (!JSValueIsNull(context, cb))
    {
        const size_t argc = 1;
        JSValueRef argv[argc];
        JSRetainPtr<JSStringRef> string = adopt(JSStringCreateWithUTF8CString(message.c_str()));
        argv[0] = JSValueMakeString(context, string.get());
        (void) JSObjectCallAsFunction(context, (JSObjectRef) cb, nullptr, argc, argv, nullptr);
    }

    it->second->unprotect(context);

    m_queries.erase(it);
}

void Proxy::sendMessageToClient(const char* name, const char* message, uint64_t callID)
{
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

void Proxy::clear(WKBundlePageRef page)
{
    if (m_client != page)
        return;

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(page));
    for (auto& kv : m_queries)
    {
        kv.second->unprotect(context);
    }
    m_queries.clear();
}

} // namespace WPEQuery
