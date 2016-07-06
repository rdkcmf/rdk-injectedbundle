#include "Proxy.h"
#include "JavaScriptRequests.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>

#include <sstream>

#define CHECK_EXCEPTION(exc) if (exc && *exc) return nullptr;

namespace
{

JSValueRef createTypeErrorException(JSContextRef ctx, const char* descr, const char* file, int line)
{
    // FIXME: WKBundleReportException ?
    std::ostringstream stream;

    stream << descr << " at " << file << ", line: " << line;
    JSRetainPtr<JSStringRef> jsstr = adopt(JSStringCreateWithUTF8CString(stream.str().c_str()));
    auto str = JSValueMakeString(ctx, jsstr.get());

    return str;
}

JSValueRef getCallbackFromArgument(JSContextRef ctx, const JSValueRef argument, const char* valueName, JSValueRef* exception)
{
    JSObjectRef objArgument = JSValueToObject(ctx, argument, exception);
    CHECK_EXCEPTION(exception);

    JSRetainPtr<JSStringRef> reqStr = adopt(JSStringCreateWithUTF8CString(valueName));
    JSValueRef arg = JSObjectGetProperty(ctx, objArgument, reqStr.get(), exception);
    CHECK_EXCEPTION(exception);

    if (!JSValueIsObject(ctx, arg)) {
        *exception = createTypeErrorException(ctx, "Incorrect argument passed!", __FILE__, __LINE__);
        return nullptr;
    }

    return arg;
}

JSStringRef getRequestFromArgument(JSContextRef ctx, const JSValueRef argument, JSValueRef* exception)
{
    JSObjectRef objArgument = JSValueToObject(ctx, argument, exception);
    CHECK_EXCEPTION(exception);

    JSRetainPtr<JSStringRef> reqStr = adopt(JSStringCreateWithUTF8CString("request"));
    JSValueRef request = JSObjectGetProperty(ctx, objArgument, reqStr.get(), exception);
    CHECK_EXCEPTION(exception);

    if (!JSValueIsString(ctx, request)) {
        *exception = createTypeErrorException(ctx, "Incorrect argument passed!", __FILE__, __LINE__);
        return nullptr;
    }

    JSStringRef ret = JSValueToStringCopy(ctx, request, exception);
    CHECK_EXCEPTION(exception);

    return ret;
}

JSValueRef queryFunc(
    const char* name,
    JSContextRef ctx,
    JSObjectRef function,
    JSObjectRef thisObject,
    size_t argc,
    const JSValueRef argv[],
    JSValueRef* exc)
{
    if (UNLIKELY(argc < 1)) {
        // FIXME: is undefined the most appropriate type here?
        // FIXME: WKBundleReportException ?
        *exc = JSValueMakeUndefined(ctx);

        return nullptr;
    }

    const JSValueRef& arg = argv[0];
    if (UNLIKELY(!JSValueIsObject(ctx, arg))) {
        // FIXME: is undefined the most appropriate type here?
        *exc = JSValueMakeUndefined(ctx);

        return nullptr;
    }

    JSRetainPtr<JSStringRef> message = adopt(getRequestFromArgument(ctx, arg, exc));
    CHECK_EXCEPTION(exc);

    JSValueRef succCallback = getCallbackFromArgument(ctx, arg, "onSuccess", exc);
    CHECK_EXCEPTION(exc);

    JSValueRef errCallback = getCallbackFromArgument(ctx, arg, "onFailure", exc);
    CHECK_EXCEPTION(exc);

    JSBridge::Proxy::singleton().sendQuery(
        name,
        ctx,
        message.get(),
        succCallback,
        errCallback);

    return JSValueMakeUndefined(ctx);
}

} // namespace

namespace JSBridge
{

JSValueRef onJavaScriptBridgeRequest(
    JSContextRef ctx,
    JSObjectRef function,
    JSObjectRef thisObject,
    size_t argc,
    const JSValueRef argv[],
    JSValueRef* exc)
{
    return queryFunc("onJavaScriptBridgeRequest", ctx, function, thisObject, argc, argv, exc);
}

JSValueRef onJavaScriptServiceManagerRequest(
    JSContextRef ctx,
    JSObjectRef function,
    JSObjectRef thisObject,
    size_t argc,
    const JSValueRef argv[],
    JSValueRef* exc)
{
    return queryFunc("onJavaScriptServiceManagerRequest", ctx, function, thisObject, argc, argv, exc);
}

} // namespace JSBridge
