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
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKString.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKBundlePagePrivate.h>

#include "AAMPJSController.h"
#include "logger.h"
#include "utils.h"
#include <fstream>

#define AAMP_UNUSED(x) (void) x

extern "C"
{
    void aamp_LoadJSController(JSGlobalContextRef context);
    void aamp_UnloadJSController(JSGlobalContextRef context);
}

namespace AAMPJSController
{

void initialize()
{
    RDKLOG_INFO("AAMPJSController::initialize()\n");
}

void didCreatePage(WKBundlePageRef page)
{
    AAMP_UNUSED(page);
    RDKLOG_INFO("AAMPJSController::didCreatePage()\n");
}

void didCommitLoad(WKBundlePageRef page, WKBundleFrameRef frame)
{
    RDKLOG_INFO("AAMPJSController::didCommitLoad()\n");
    if (WKBundlePageGetMainFrame(page) != frame)
    {
        RDKLOG_INFO("Frame is not allowed to inject JS window objects!\n");
        return;
    }

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    aamp_LoadJSController(context);
}

void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame)
{
    RDKLOG_INFO("AAMPJSController::didStartProvisionalLoadForFrame()\n");

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    if (mainFrame == frame)
    {
        JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
        aamp_UnloadJSController(context);
    }
}

bool didReceiveMessageToPage(WKStringRef messageName, WKTypeRef messageBody)
{
    AAMP_UNUSED(messageName);
    AAMP_UNUSED(messageBody);
    RDKLOG_INFO("AAMPJSController::didReceiveMessageToPage()\n");
    return false;
}

} // namespace
