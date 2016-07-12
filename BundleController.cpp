#include "BundleController.h"
#include "Proxy.h"

#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>

namespace
{

void didCommitLoad(WKBundlePageRef page,
    WKBundleFrameRef frame, WKTypeRef*, const void*)
{
    JSBridge::Proxy::singleton().injectObjects(page, frame);
}

void didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    JSBridge::Proxy::singleton().setClient(page);
    WKBundlePageLoaderClientV0 client {
        {0, clientInfo},
        // Version 0.
        0, // didStartProvisionalLoadForFrame;
        0, // didReceiveServerRedirectForProvisionalLoadForFrame;
        0, // didFailProvisionalLoadWithErrorForFrame;
        didCommitLoad, // didCommitLoadForFrame;
        0, // didFinishDocumentLoadForFrame;
        0, // didFinishLoadForFrame;
        0, // didFailLoadWithErrorForFrame;
        0, // didSameDocumentNavigationForFrame;
        0, // didReceiveTitleForFrame;
        0, // didFirstLayoutForFrame;
        0, // didFirstVisuallyNonEmptyLayoutForFrame;
        0, // didRemoveFrameFromHierarchy;
        0, // didDisplayInsecureContentForFrame;
        0, // didRunInsecureContentForFrame;
        0, // didClearWindowObjectForFrame;
        0, // didCancelClientRedirectForFrame;
        0, // willPerformClientRedirectForFrame;
        0, // didHandleOnloadEventsForFrame;
    };

    WKBundlePageSetPageLoaderClient(page, &client.base);
}

void didReceiveMessageToPage(WKBundleRef,
    WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    JSBridge::Proxy::singleton().onMessageFromClient(page, messageName, messageBody);
}

} // namespace

namespace JSBridge
{

void initialize(WKBundleRef bundleRef, WKTypeRef)
{
    WKBundleClientV1 client = {
        {1, nullptr},
        didCreatePage,
        nullptr, // willDestroyPage
        nullptr, // didInitializePageGroup
        nullptr, // didReceiveMessage
        didReceiveMessageToPage
    };

    WKBundleSetClient(bundleRef, &client.base);
}

} // JSBridge