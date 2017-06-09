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
#include "BundleController.h"
#include "Proxy.h"
#include "AVESupport.h"
#include "WebFilter.h"
#include "RequestHeaders.h"

#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleBackForwardListItem.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKURL.h>

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

bool shouldGoToBackForwardListItem(WKBundlePageRef page, WKBundleBackForwardListItemRef item, WKTypeRef*, const void*)
{
    auto backForwardList = WKBundlePageGetBackForwardList(page);
    auto backCount = WKBundleBackForwardListGetBackListCount(backForwardList);
    if (backCount == 1 && WKURLIsEqual(adoptWK(WKBundleBackForwardListItemCopyURL(item)).get(),
                                       adoptWK(WKURLCreateWithUTF8CString("about:blank")).get()))
        return false;
    return true;
}

WKURLRequestRef willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef, uint64_t, WKURLRequestRef request, WKURLResponseRef, const void*)
{
    if (filterRequest(page, request))
        return nullptr;

    applyRequestHeaders(page, request);
    WKRetainPtr<WKURLRequestRef> newRequest = request;
    return newRequest.leakRef();
}

void didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    JSBridge::Proxy::singleton().setClient(page);
    WKBundlePageLoaderClientV1 client {
        {1, clientInfo},
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

        // Version 1.
        nullptr, // didLayoutForFrame
        nullptr, // didNewFirstVisuallyNonEmptyLayout_unavailable
        nullptr, // didNewFirstVisuallyNonEmptyLayout_unavailable
        shouldGoToBackForwardListItem,
        nullptr, // globalObjectIsAvailableForFrame
        nullptr, // globalObjectIsAvailableForFrame
        nullptr, // didReconnectDOMWindowExtensionToGlobalObject
        nullptr // willDestroyGlobalObjectForDOMWindowExtension
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
    removeRequestHeadersFromPage(page);
}

void didReceiveMessageToPage(WKBundleRef,
    WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    if (WKStringIsEqualToUTF8CString(messageName, "webfilters"))
    {
        addWebFiltersForPage(page, messageBody);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "headers"))
    {
        setRequestHeadersToPage(page, messageBody);
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
