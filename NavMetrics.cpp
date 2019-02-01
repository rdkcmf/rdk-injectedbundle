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
#include "NavMetrics.h"

#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include <string>

#include <glib.h>

#include "utils.h"
#include "logger.h"

namespace NavMetrics
{

static const char kPerfomanceTimingScriptStr[] =
"(function() {"
    "var timingJSON = performance.timing.toJSON();"
    "var navigationStart = timingJSON.navigationStart;"
    "var result = {};"
    "for(var k in timingJSON)"
       "if (timingJSON[k] > navigationStart) result[k] = timingJSON[k] - navigationStart;"
    "var entries = performance.getEntries();"
    "result[\"entriesLen\"] = entries ? entries.length : 0;"
    "return JSON.stringify(result);"
"})()";

static std::string g_timingStr;

static std::string jsToStdString(JSStringRef jsStr)
{
    if (!jsStr)
        return { };

    size_t len = JSStringGetMaximumUTF8CStringSize(jsStr);
    if (!len)
        return { };

    std::unique_ptr<char[]> buffer(new char[len]);
    len = JSStringGetUTF8CString(jsStr, buffer.get(), len);
    if (!len)
        return { };

    return std::string { buffer.get(), len };
}

static std::string getPerfomanceTiming(WKBundlePageRef page)
{
    if (!page)
        return { };

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    if (!mainFrame)
        return { };

    JSGlobalContextRef jsContext = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!jsContext)
        return { };

    JSObjectRef jsGlobalObj = JSContextGetGlobalObject(jsContext);
    JSStringRef jsStr = JSStringCreateWithUTF8CString(kPerfomanceTimingScriptStr);
    JSValueRef jsException = nullptr;
    JSValueRef jsResult = JSEvaluateScript(jsContext, jsStr, jsGlobalObj, nullptr, 0, &jsException);
    JSStringRelease(jsStr);

    if (jsException)
        return { };

    JSStringRef  jsResultStr = JSValueToStringCopy(jsContext, jsResult, nullptr);
    if (!jsResultStr)
        return { };

    std::string result = jsToStdString(jsResultStr);
    JSStringRelease(jsResultStr);

    return result;
}

bool didReceiveMessageToPage(WKBundlePageRef page, WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "getNavigationTiming"))
    {
        if (WKGetTypeID(messageBody) != WKUInt64GetTypeID())
        {
            RDKLOG_ERROR("Unexpected param type.");
            return true;
        }

        if (g_timingStr.empty())
            g_timingStr = getPerfomanceTiming(page);

        RDKLOG_TRACE("Return navigation timing: '%s'", g_timingStr.c_str());

        WKRetainPtr<WKStringRef> nameRef = adoptWK(WKStringCreateWithUTF8CString("onNavigationTiming"));
        WKRetainPtr<WKStringRef> bodyRef = adoptWK(WKStringCreateWithUTF8CString(g_timingStr.c_str()));

        WKTypeRef params[] = {messageBody, bodyRef.get()};
        WKRetainPtr<WKArrayRef> arrRef = adoptWK(WKArrayCreate(params, sizeof(params)/sizeof(params[0])));

        WKBundlePagePostMessage(page, nameRef.get(), arrRef.get());

        g_timingStr.clear();
        return true;
    }
    return false;
}

void didHandleOnloadEventsForFrame(WKBundlePageRef page, WKBundleFrameRef frame)
{
    if (WKBundleFrameIsMainFrame(frame))
        g_timingStr = getPerfomanceTiming(page);
}

};
