#ifndef JSBRIDGE_PROXY_H
#define JSBRIDGE_PROXY_H

#include "BundleController.h"
#include "ServicesACL.h"
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
     * Sends bridge request packed in messageRef to backend
     * and will call onSuccess/onError depending on returned status code.
     */
    void sendJavaScriptBridgeRequest(JSContextRef ctx, JSStringRef messageRef,
        JSValueRef onSuccess, JSValueRef onError);

    /**
     * Sends bridge service manager request packed in messageRef to backend
     * and will call onSuccess/onError depending on returned status code.
     * Also checks ACL to avoid prohibited requests.
     */
    void sendJavaScriptServiceManagerRequest(JSStringRef serviceName,
        JSContextRef ctx, JSStringRef messageRef, JSValueRef onSuccess, JSValueRef onError);

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
     * Sends specific message to client.
     * @param Name or type of the message. Will go directly to backend.
     * @param Message to send.
     * @param CallID if there are some callbacks to handle responses.
     */
    void sendMessageToClient(const char* name, const char* message, uint64_t callID = 0);

    /**
     * Enables/disables ServiceManager JavaScript object.
     * Called when enableServiceManager is received from xre server.
     */
    void onEnableServiceManager(WKBundlePageRef page, WKTypeRef messageBody);

    /**
     * Sets ACL for services.
     * Called when servicesACL is received from xre server.
     */
    void onServicesACL(WKTypeRef messageBody);

    /**
     * Handles JavaScript bridge response previously sent.
     * Called when request has been processed and returned a result.
     */
    void onJavaScriptBridgeResponse(WKBundlePageRef page, WKTypeRef messageBody);

    /**
     * Checks if ServiceManager js object should be enabled/disabled or allowed.
     */
    void processServiceManager(WKBundlePageRef page);

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
    uint64_t m_lastCallID;

    /**
     * Defines should ServiceManager be enabled or not.
     */
    bool m_enableServiceManager;

    /**
     * Defines if didCommitLoaded already received.
     */
    bool m_didCommitLoad;

    /**
     * Handler to ACL for services.
     */
    ServicesACL m_acl;

};

} // namespace JSBridge

#endif // JSBRIDGE_PROXY_H
