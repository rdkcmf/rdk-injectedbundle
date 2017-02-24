#include "BundleController.h"
#include "Proxy.h"
#include "AVESupport.h"
#include "WebFilter.h"

#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>
#include <WebKit/WKRetainPtr.h>

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

WKURLRequestRef willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef, uint64_t, WKURLRequestRef request, WKURLResponseRef, const void*)
{
    if (filterRequest(page, request))
        return nullptr;

    WKRetainPtr<WKURLRequestRef> newRequest = request;
    return newRequest.leakRef();
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

    WKBundlePageResourceLoadClientV0 resourceLoadClient {
        {0, clientInfo},
        nullptr, // didInitiateLoadForResource
        willSendRequestForFrame,
        nullptr, // didReceiveResponseForResource
        nullptr, // didReceiveContentLengthForResource
        nullptr, // didFinishLoadForResource
        nullptr // didFailLoadForResource
    };

    WKBundlePageSetResourceLoadClient(page, &resourceLoadClient.base);

    AVESupport::didCreatePage(page);
}

void willDestroyPage(WKBundleRef, WKBundlePageRef page, const void*)
{
    removeWebFiltersForPage(page);
}

void didReceiveMessageToPage(WKBundleRef,
    WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    if (WKStringIsEqualToUTF8CString(messageName, "webfilters"))
    {
        addWebFiltersForPage(page, messageBody);
        return;
    }

    if (AVESupport::didReceiveMessageToPage(messageName, messageBody))
    {
        return;
    }
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
        willDestroyPage,
        nullptr, // didInitializePageGroup
        nullptr, // didReceiveMessage
        didReceiveMessageToPage
    };

    WKBundleSetClient(bundleRef, &client.base);
}

} // JSBridge