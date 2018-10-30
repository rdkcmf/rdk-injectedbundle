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
#include "WebFilter.h"
#include "utils.h"
#include "logger.h"

#include <WebKit/WKType.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKString.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKURL.h>

#include <cassert>
#include <unordered_map>
#include <glib.h>
#include <vector>


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

class WebFilter
{
public:
    struct Pattern
    {
        Pattern(std::string&& scheme, std::string&& host, bool block)
            : schemePatternString(std::move(scheme)),
              hostPatternString(std::move(host)),
              block(block)
        {
            schemePattern = schemePatternString.empty() ?
                nullptr : g_pattern_spec_new(schemePatternString.c_str());
            hostPattern = hostPatternString.empty() ?
                nullptr : g_pattern_spec_new(hostPatternString.c_str());
            RDKLOG_TRACE("adding filter pattern,  scheme: [%s]  host : [%s]  block: %s",
                         schemePatternString.c_str(), hostPatternString.c_str(), block ? "true" : "false");
        }

        ~Pattern()
        {
            if (schemePattern) g_pattern_spec_free(schemePattern);
            if (hostPattern) g_pattern_spec_free(hostPattern);
        }

        std::string schemePatternString;
        std::string hostPatternString;
        GPatternSpec* schemePattern;
        GPatternSpec* hostPattern;
        bool block;
    };

    typedef std::vector<Pattern> FilterVector;

    static WebFilter& singleton()
    {
        static WebFilter filter;
        return filter;
    }

    void setFilters(WKBundlePageRef page, FilterVector&& filters)
    {
        if (filters.empty())
            removeFilters(page);
        else
            filtersMap[page] = std::move(filters);
    }

    void removeFilters(WKBundlePageRef page)
    {
        filtersMap.erase(page);
    }

    bool filter(WKBundlePageRef page, WKURLRef url)
    {
        RDKLOG_TRACE("filtering url [%s]", Utils::toStdString(adoptWK(WKURLCopyString(url)).get()).c_str());

        FiltersMap::const_iterator iter = filtersMap.find(page);

        if (iter != filtersMap.end())
        {
            std::string scheme = Utils::toStdString(adoptWK(WKURLCopyScheme(url)).get());
            std::string host = Utils::toStdString(adoptWK(WKURLCopyHostName(url)).get());
            const FilterVector& filters = iter->second;
            for (const auto& f : filters)
            {
                if (f.schemePattern && !g_pattern_match_string(f.schemePattern, scheme.c_str()))
                    continue;
                if (f.hostPattern && !g_pattern_match_string(f.hostPattern, host.c_str()))
                    continue;
                if (f.block)
                {
                    RDKLOG_INFO("filtering, found match:  scheme pattern [%s] host pattern [%s] request blocked",
                            f.schemePatternString.c_str(), f.hostPatternString.c_str());
                    fprintf(stderr,"request blocked: %s\n",Utils::toStdString(adoptWK(WKURLCopyString(url)).get()).c_str());
                } else {
                    RDKLOG_TRACE("filtering, found match:  scheme pattern [%s] host pattern [%s] request not blocked",
                            f.schemePatternString.c_str(), f.hostPatternString.c_str());
                }
                return f.block;
            }
        }
        return false;
    }

private:
    typedef std::unordered_map<WKBundlePageRef, FilterVector> FiltersMap;
    WebFilter() {}
    ~WebFilter() {}

    FiltersMap filtersMap;
};

} // namespace


void setWebFiltersForPage(WKBundlePageRef page, WKTypeRef filters)
{
    RDKLOG_INFO("page: %p", page);

    CHECK_CONDITION(WKGetTypeID(filters) == WKArrayGetTypeID(), "filters are not an array");

    WKArrayRef filterArray = static_cast<WKArrayRef>(filters);
    size_t size = filterArray ? WKArrayGetSize(filterArray) : 0;

    WebFilter::FilterVector filtersToPass;
    filtersToPass.reserve(size);

    for (size_t i = 0; i < size; ++i)
    {
        CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(filterArray, i)) == WKArrayGetTypeID(), "pattern is not an array");
        WKArrayRef patternParams = static_cast<WKArrayRef>(WKArrayGetItemAtIndex(filterArray, i));
        CHECK_CONDITION(WKArrayGetSize(patternParams) == 3, "incorrect pattern params's size");
        CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(patternParams, 0)) == WKStringGetTypeID(), "first parameter is not WKStringRef");
        CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(patternParams, 1)) == WKStringGetTypeID(), "second parameter is not WKStringRef");
        CHECK_CONDITION(WKGetTypeID(WKArrayGetItemAtIndex(patternParams, 2)) == WKBooleanGetTypeID(), "third parameter is not WKBooleanRef");

        filtersToPass.emplace_back(
            Utils::toStdString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(patternParams, 0))),
            Utils::toStdString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(patternParams, 1))),
            WKBooleanGetValue(static_cast<WKBooleanRef>(WKArrayGetItemAtIndex(patternParams, 2))));
    }

    WebFilter::singleton().setFilters(page, std::move(filtersToPass));
}

void removeWebFiltersForPage(WKBundlePageRef page)
{
    RDKLOG_INFO("page: %p", page);
    WebFilter::singleton().removeFilters(page);
}

bool filterRequest(WKBundlePageRef page, WKURLRequestRef request)
{
    RDKLOG_TRACE("page: %p", page);
    WKRetainPtr<WKURLRef> url = adoptWK(WKURLRequestCopyURL(request));
    return WebFilter::singleton().filter(page, url.get());
}
