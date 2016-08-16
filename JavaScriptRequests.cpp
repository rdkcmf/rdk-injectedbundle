#include "Proxy.h"
#include "JavaScriptRequests.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKRetainPtr.h>

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

    return JSValueMakeString(ctx, jsstr.get());
}

JSValueRef getValueFromArgument(JSContextRef ctx, const JSValueRef argument, const char* name, JSValueRef* exc)
{
    JSObjectRef objArgument = JSValueToObject(ctx, argument, exc);
    CHECK_EXCEPTION(exc);

    JSRetainPtr<JSStringRef> valueRef = adopt(JSStringCreateWithUTF8CString(name));
    JSValueRef result = JSObjectGetProperty(ctx, objArgument, valueRef.get(), exc);
    CHECK_EXCEPTION(exc);

    return result;
}

JSStringRef getStringValueFromArgument(JSContextRef ctx, const JSValueRef argument, const char* name, JSValueRef* exc)
{
    JSValueRef value = getValueFromArgument(ctx, argument, name, exc);
    CHECK_EXCEPTION(exc);
    if (!JSValueIsString(ctx, value))
    {
        *exc = createTypeErrorException(ctx, "Incorrect argument passed!", __FILE__, __LINE__);
        return nullptr;
    }

    JSStringRef result = JSValueToStringCopy(ctx, value, exc);
    CHECK_EXCEPTION(exc);

    return result;
}

JSValueRef getObjectValueFromArgument(JSContextRef ctx, const JSValueRef argument, const char* name, JSValueRef* exc)
{
    JSValueRef value = getValueFromArgument(ctx, argument, name, exc);
    CHECK_EXCEPTION(exc);
    if (!JSValueIsObject(ctx, value))
    {
        *exc = createTypeErrorException(ctx, "Incorrect argument passed!", __FILE__, __LINE__);
        return nullptr;
    }

    return value;
}

JSValueRef getArgument(JSContextRef ctx,
    size_t argc, const JSValueRef argv[], JSValueRef* exc)
{
    if (UNLIKELY(argc < 1))
    {
        // FIXME: is undefined the most appropriate type here?
        // FIXME: WKBundleReportException ?
        *exc = JSValueMakeUndefined(ctx);

        return nullptr;
    }

    JSValueRef result = argv[0];
    if (UNLIKELY(!JSValueIsObject(ctx, result)))
    {
        // FIXME: is undefined the most appropriate type here?
        *exc = JSValueMakeUndefined(ctx);

        return nullptr;
    }

    return result;
}

JSValueRef sendQuery(const char* name, JSContextRef ctx,
    size_t argc, const JSValueRef argv[], JSValueRef* exc)
{
    JSValueRef arg = getArgument(ctx, argc, argv, exc);
    if (!arg)
    {
        return nullptr;
    }

    JSRetainPtr<JSStringRef> message = adopt(getStringValueFromArgument(ctx, arg, "request", exc));
    CHECK_EXCEPTION(exc);
    JSValueRef onSuccess = getObjectValueFromArgument(ctx, arg, "onSuccess", exc);
    CHECK_EXCEPTION(exc);
    JSValueRef onFailure = getObjectValueFromArgument(ctx, arg, "onFailure", exc);
    CHECK_EXCEPTION(exc);

    JSBridge::Proxy::singleton().sendQuery(
        name,
        ctx,
        message.get(),
        onSuccess,
        onFailure);

    return JSValueMakeUndefined(ctx);
}

} // namespace

namespace JSBridge
{

JSValueRef onJavaScriptBridgeRequest(
    JSContextRef ctx,
    JSObjectRef,
    JSObjectRef,
    size_t argc,
    const JSValueRef argv[],
    JSValueRef* exc)
{
    return sendQuery("onJavaScriptBridgeRequest", ctx, argc, argv, exc);
}

JSValueRef onJavaScriptServiceManagerRequest(
    JSContextRef ctx,
    JSObjectRef,
    JSObjectRef,
    size_t argc,
    const JSValueRef argv[],
    JSValueRef* exc)
{
    return sendQuery("onJavaScriptServiceManagerRequest", ctx, argc, argv, exc);
}

} // namespace JSBridge
