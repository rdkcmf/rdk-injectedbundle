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

#include "RequestHeaders.h"
#include "utils.h"
#include "logger.h"

#include <WebKit/WKType.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKString.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKURL.h>

#include <cassert>
#include <unordered_map>
#include <vector>
#include <glib.h>

#define CHECK_CONDITION(condition, log) \
do { \
    if (!(condition)) { \
        RDKLOG_ERROR(log); \
        assert(false); \
        return; \
    } \
} while(0)

namespace
{

typedef std::vector<std::pair<std::string, std::string>> Headers;
typedef std::unordered_map<WKBundlePageRef, Headers> PageHeaders;
static PageHeaders s_pageHeaders;

} // namespace

void setRequestHeadersToPage(WKBundlePageRef page, WKTypeRef headersRef)
{
    CHECK_CONDITION(WKGetTypeID(headersRef) == WKArrayGetTypeID(), "headers is not an array");
    WKArrayRef array = static_cast<WKArrayRef>(headersRef);
    size_t arraySize = array ? WKArrayGetSize(array) : 0;
    CHECK_CONDITION(arraySize == 2, "incorrect params's size");
    CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(array, 0)) == WKArrayGetTypeID(), "parameter is not an array");
    CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(array, 1)) == WKArrayGetTypeID(), "parameter is not an array");

    WKArrayRef keysArray = static_cast<WKArrayRef>(WKArrayGetItemAtIndex(array, 0));
    WKArrayRef valuesArray = static_cast<WKArrayRef>(WKArrayGetItemAtIndex(array, 1));

    size_t size = WKArrayGetSize(keysArray);
    Headers headers;
    headers.reserve(size);
    for (size_t i = 0; i < size; ++i)
    {
        auto key = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex(keysArray, i));
        auto value = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex(valuesArray, i));
        headers.emplace_back(key, value);
    }

    RDKLOG_INFO("page [%p]: %u headers set", page, size);
    s_pageHeaders[page] = std::move(headers);
}

void removeRequestHeadersFromPage(WKBundlePageRef page)
{
    RDKLOG_INFO("page: %p", page);
    s_pageHeaders.erase(page);
}

void applyRequestHeaders(WKBundlePageRef page, WKURLRequestRef requestRef)
{
    auto it = s_pageHeaders.find(page);
    if (it == s_pageHeaders.end())
        return;

    auto urlRef = adoptWK(WKURLRequestCopyURL(requestRef));
    auto url = adoptWK(WKURLCopyString(urlRef.get()));
    RDKLOG_TRACE("page [%p] %s", page, Utils::toStdString(url.get()).c_str());
    for (const auto& h : it->second)
    {
        RDKLOG_TRACE("%s: %s", h.first.c_str(), h.second.c_str());
        auto key = adoptWK(WKStringCreateWithUTF8CString(h.first.c_str()));
        auto value = adoptWK(WKStringCreateWithUTF8CString(h.second.c_str()));
        WKURLRequestSetHTTPHeaderField(requestRef, key.get(), value.get());
    }
}
