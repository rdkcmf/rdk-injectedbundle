#include "BundleController.h"
#include "Proxy.h"
#include "AVESupport.h"

#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>

namespace
{

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *)
{
    AVESupport::didStartProvisionalLoadForFrame(page, frame);
}

void didCommitLoad(WKBundlePageRef page,
    WKBundleFrameRef frame, WKTypeRef*, const void*)
{
    JSBridge::Proxy::singleton().didCommitLoad(page, frame);
    AVESupport::didCommitLoad(page, frame);
}

void didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    JSBridge::Proxy::singleton().setClient(page);
    WKBundlePageLoaderClientV0 client {
        {0, clientInfo},
        // Version 0.
        didStartProvisionalLoadForFrame, // didStartProvisionalLoadForFrame;
        nullptr, // didReceiveServerRedirectForProvisionalLoadForFrame;
        nullptr, // didFailProvisionalLoadWithErrorForFrame;
        didCommitLoad, // didCommitLoadForFrame;
        nullptr, // didFinishDocumentLoadForFrame;
        nullptr, // didFinishLoadForFrame;
        nullptr, // didFailLoadWithErrorForFrame;
        nullptr, // didSameDocumentNavigationForFrame;
        nullptr, // didReceiveTitleForFrame;
        nullptr, // didFirstLayoutForFrame;
        nullptr, // didFirstVisuallyNonEmptyLayoutForFrame;
        nullptr, // didRemoveFrameFromHierarchy;
        nullptr, // didDisplayInsecureContentForFrame;
        nullptr, // didRunInsecureContentForFrame;
        nullptr, // didClearWindowObjectForFrame;
        nullptr, // didCancelClientRedirectForFrame;
        nullptr, // willPerformClientRedirectForFrame;
        nullptr, // didHandleOnloadEventsForFrame;
    };

    WKBundlePageSetPageLoaderClient(page, &client.base);

    AVESupport::didCreatePage(page);
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