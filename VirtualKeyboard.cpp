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

#include <WebKit/WKBundlePage.h>
#include <WebKit/WKString.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKType.h>

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include "VirtualKeyboard.h"
#include "logger.h"
#include "utils.h"
#include <string>
#include <cstring>

namespace VirtualKeyboard
{

bool enableVirtualKeyboard = false;

void initialize()
{
    const char *isVirtualKeyboard = getenv("ENABLE_VIRTUAL_KEYBOARD");
    if (isVirtualKeyboard && strcmp(isVirtualKeyboard, "1") == 0)
    {
        RDKLOG_INFO("VirtualKeyboard::initialize() - VirtualKeyboard enabled!");
        enableVirtualKeyboard = true;
    }
}

static std::string jsToStdString(JSStringRef jsStr)
{
    if (!jsStr)
        return {};

    size_t len = JSStringGetMaximumUTF8CStringSize(jsStr);
    if (!len)
        return {};

    std::unique_ptr<char[]> buffer(new char[len]);
    len = JSStringGetUTF8CString(jsStr, buffer.get(), len);
    if (!len)
        return {};

    return std::string {buffer.get(), len};
}

void didFocusTextField(WKBundlePageRef page, WKBundleFrameRef frame)
{
    if(!enableVirtualKeyboard) {
        return;
    }
    JSValueRef exc = 0;
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSValueRef jsResult = Utils::evaluateUserScript(context, "document.activeElement.className", &exc);
    if(exc)
        return;

    JSStringRef jsResultStr = JSValueToStringCopy(context, jsResult, nullptr);
    if(!jsResultStr)
        return;

    std::string result = jsToStdString(jsResultStr);
    JSStringRelease(jsResultStr);

    WKStringRef messageNameRef = WKStringCreateWithUTF8CString("didFocusTextField");
    WKStringRef classNameRef = WKStringCreateWithUTF8CString(result.c_str());

    WKTypeRef params[] = {classNameRef};
    WKArrayRef arrRef = WKArrayCreate(params, sizeof(params)/sizeof(params[0]));

    WKBundlePagePostMessage(page, messageNameRef, arrRef);
    WKRelease(messageNameRef);
    WKRelease(classNameRef);
    WKRelease(arrRef);
}

} // namespace
