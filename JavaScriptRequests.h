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
