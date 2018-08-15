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

#ifdef ENABLE_AVE
#include "AVESupport.h"
#endif

#include "WebFilter.h"
#include "RequestHeaders.h"
#include "AccessibilitySupport.h"
#ifdef ENABLE_AAMP_JSBINDING
#include "AAMPJSController.h"
#endif

#include "logger.h"
#include "utils.h"

#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleBackForwardListItem.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePageLoaderClient.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKURL.h>

#include <rdkat.h>

namespace
{

bool shouldInjectBindings(WKURLRef url)
{
    if (url == nullptr)
        return false;
    WKRetainPtr<WKStringRef> wkHost = adoptWK(WKURLCopyHostName(url));
    if (wkHost.get() == nullptr)
        return false;
    std::string hostStr = Utils::toStdString(wkHost.get());
    if (hostStr.empty() || hostStr.find("youtube.com") != std::string::npos)
        return false;
    return true;
}

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    if (mainFrame == frame)
    {
        JSBridge::Proxy::singleton().clear(page);

        WKRetainPtr<WKURLRef> wkUrl = adoptWK(WKBundleFrameCopyURL(frame));
        WKRetainPtr<WKStringRef> wkScheme = adoptWK(WKURLCopyScheme(wkUrl.get()));
        if (WKStringIsEqualToUTF8CString(wkScheme.get(), "about"))
        {
            WKBundleBackForwardListClear(WKBundlePageGetBackForwardList(page));
        }
    }

#ifdef ENABLE_AVE
    AVESupport::didStartProvisionalLoadForFrame(page, frame);
#endif

#ifdef ENABLE_AAMP_JSBINDING
    AAMPJSController::didStartProvisionalLoadForFrame(page, frame);
#endif
}

void didCommitLoad(WKBundlePageRef page,
    WKBundleFrameRef frame, WKTypeRef*, const void*)
{
    JSBridge::Proxy::singleton().didCommitLoad(page, frame);

    WKRetainPtr<WKURLRef> wkUrl = adoptWK(WKBundleFrameCopyURL(frame));
    WKRetainPtr<WKStringRef> wkUrlStr = adoptWK(WKURLCopyString(wkUrl.get()));
    std::string url = Utils::toStdString(wkUrlStr.get());

    // Notify RDK_AT about the URL change so that a session will be created
    if(WKBundlePageGetMainFrame(page) == frame)
        RDK_AT::NotifyURLChange(url, setPageMediaVolume, (void*)page);

    if (!shouldInjectBindings(wkUrl.get()))
    {
        RDKLOG_INFO("Skip AVE/AAMP bindings injection for %s",
                    Utils::toStdString(wkUrlStr.get()).c_str());
        return;
    }

#ifdef ENABLE_AVE
    AVESupport::didCommitLoad(page, frame);
#endif

#ifdef ENABLE_AAMP_JSBINDING
    AAMPJSController::didCommitLoad(page, frame);
#endif
}

bool shouldGoToBackForwardListItem(WKBundlePageRef, WKBundleBackForwardListItemRef item, WKTypeRef*, const void*)
{
     if (item && WKURLIsEqual(adoptWK(WKBundleBackForwardListItemCopyURL(item)).get(),
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

static WKBundlePagePolicyAction decidePolicyForNavigationAction(WKBundlePageRef, WKBundleFrameRef, WKBundleNavigationActionRef, WKURLRequestRef, WKTypeRef*, const void*)
{
    return WKBundlePagePolicyActionUse;
}

void didCreatePage(WKBundleRef, WKBundlePageRef page, const void* clientInfo)
{
    JSBridge::Proxy::singleton().setClient(page);

#ifdef ENABLE_AVE
    AVESupport::setClient(page);
#endif

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

    WKBundlePagePolicyClientV0 policyClient {
        {0, clientInfo},
        decidePolicyForNavigationAction,  // decidePolicyForNavigationAction
        nullptr,  // decidePolicyForNewWindowAction
        nullptr,  // decidePolicyForResponse
        nullptr   // unableToImplementPolicy
    };

    WKBundlePageSetPolicyClient(page, &policyClient.base);

#ifdef ENABLE_AVE
    AVESupport::didCreatePage(page);
#endif

}

void willDestroyPage(WKBundleRef, WKBundlePageRef page, const void*)
{
    RDK_AT::Uninitialize();
    removeWebFiltersForPage(page);
    removeRequestHeadersFromPage(page);
}

void didReceiveMessageToPage(WKBundleRef,
    WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    if (WKStringIsEqualToUTF8CString(messageName, "webfilters"))
    {
        setWebFiltersForPage(page, messageBody);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "headers"))
    {
        setRequestHeadersToPage(page, messageBody);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "accessibility_settings"))
    {
        passAccessibilitySettingsToRDKAT(messageBody);
        return;
    }

#ifdef ENABLE_AVE
    if (AVESupport::didReceiveMessageToPage(messageName, messageBody))
    {
        return;
    }
#endif

#ifdef ENABLE_AAMP_JSBINDING
    if (AAMPJSController::didReceiveMessageToPage(messageName, messageBody))
    {
        return;
    }
#endif

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

    RDK_AT::Initialize();
    WKBundleSetClient(bundleRef, &client.base);
}

} // JSBridge
