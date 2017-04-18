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
#ifndef JSBRIDGE_PROXY_H
#define JSBRIDGE_PROXY_H

#include "BundleController.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <map>
#include <memory>
#include <string>

namespace JSBridge
{

/**
 * Container for function callbacks per each query.
 */
class QueryCallbacks;

/**
 * Handles requests from JavaScript and returns result asynchronously.
 */
class Proxy
{
public:
    static Proxy& singleton();

    /**
     * Sets client whom need to send message requests.
     */
    void setClient(WKBundlePageRef bundle);

    /**
     * Sends query messages to backend side.
     * @param Name of the request. Will go directly to backend.
     * @param Context
     * @param Message to be sent.
     * @param onSuccess callback.
     * @param onError callback.
     */
    void sendQuery(const char* name, JSContextRef ctx, JSStringRef messageRef,
        JSValueRef onSuccess, JSValueRef onError);

    /**
     * After each sendQuery response message should be sent back.
     * Called when the response is received.
     */
    void onMessageFromClient(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody);

    /**
     * Handles event when need to inject JavaScript objects to window.
     */
    void didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame);

private:

    Proxy();
    Proxy(const Proxy&) = delete;
    Proxy& operator=(const Proxy&) = delete;

    /**
     * Sends specific message to client.
     * @param Name or type of the message. Will go directly to backend.
     * @param Message to send.
     * @param CallID if there are some callbacks to handle responses.
     */
    void sendMessageToClient(const char* name, const char* message, uint64_t callID = 0);

    /**
     * Handles JavaScript bridge response previously sent.
     * Called when request has been processed and returned a result.
     */
    void onJavaScriptBridgeResponse(WKBundlePageRef page, WKTypeRef messageBody);

    /**
     * Maps call identifiers to JavaScript callback functions
     * to call after response is received.
     */
    std::map<int, std::unique_ptr<QueryCallbacks> > m_queries;

    /**
     * Client of the bridge.
     */
    WKBundlePageRef m_client;

    /**
     * Each request must belong to call ID
     * to proper handling responses.
     */
    uint64_t m_lastCallID = {0};
};

} // namespace JSBridge

#endif // JSBRIDGE_PROXY_H
