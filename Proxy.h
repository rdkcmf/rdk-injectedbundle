#ifndef JSBRIDGE_PROXY_H
#define JSBRIDGE_PROXY_H

#include "BundleController.h"
#include <WebKit/WKBundlePage.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
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
     * Handles JavaScript messages.
     * Messages must be json strings.
     * Called from client.
     */
    void sendQuery(const char* name, JSContextRef ctx, JSStringRef messageRef,
        JSValueRef onSuccess, JSValueRef onError);

    /**
     * After each sendQuery response message should be sent back.
     * Called when the response is received.
     */
    void onMessageFromClient(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody);

private:

    Proxy();
    Proxy(const Proxy&) = delete;
    Proxy& operator=(const Proxy&) = delete;

    /**
     * Sends specific message to client.
     */
    void sendMessageToClient(const char* name, uint64_t callID, const std::string& message);

    /**
     * Maps call identifiers to JavaScript callback functions
     * to call after response is received.
     */
    std::map<int, std::unique_ptr<QueryCallbacks> > m_queries;

    /**
     * Each request must belong to call ID
     * to proper handling responses.
     */
    uint64_t m_lastCallID;

    /**
     * Client of the bridge.
     */
    WKBundlePageRef m_client;
};

} // namespace JSBridge

#endif // JSBRIDGE_PROXY_H
