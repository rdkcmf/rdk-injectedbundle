#ifndef JSBRIDGE_JAVASCRIPT_REQUESTS_H
#define JSBRIDGE_JAVASCRIPT_REQUESTS_H

#include <JavaScriptCore/JSBase.h>

namespace JSBridge
{

/**
 * Emited when need to send JavaScript bridge request.
 * Handles generic messages.
 * @copydoc JSObjectCallAsFunctionCallback
 */
JSValueRef onJavaScriptBridgeRequest(
    JSContextRef,
    JSObjectRef,
    JSObjectRef,
    size_t,
    const JSValueRef*,
    JSValueRef*);

/**
 * Emited when need to send JavaScript service manager request.
 * Queries service manager to fetch a service or service's method.
 * @copydoc JSObjectCallAsFunctionCallback
 */
JSValueRef onJavaScriptServiceManagerRequest(
    JSContextRef,
    JSObjectRef,
    JSObjectRef,
    size_t,
    const JSValueRef*,
    JSValueRef*);

} // namespace JSBridge

#endif // JSBRIDGE_JAVASCRIPT_REQUESTS_H
