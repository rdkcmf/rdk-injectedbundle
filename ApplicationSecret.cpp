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

#include "ApplicationSecret.h"
#include "logger.h"
#include <string>
#include <cstring>

namespace ApplicationSecret
{

bool enableAppSecret = false;

void initialize()
{
    const char *isAppSecret = getenv("ENABLE_APP_SECRET");
    if (isAppSecret && strcmp(isAppSecret, "1") == 0)
    {
        RDKLOG_INFO("ApplicationSecret::initialize() - ApplicationSecret enabled!");
        enableAppSecret = true;
    }
}

void didCreatePage(WKBundlePageRef page)
{
    if(enableAppSecret && std::getenv("WRT_APPLICATION_SECRET")) {
        std::string js = "document.ethan_application_secret = \"";
        js += std::getenv("WRT_APPLICATION_SECRET");
        js += "\"";
        WKBundlePageAddUserScript(page, WKStringCreateWithUTF8CString(js.c_str()), kWKInjectAtDocumentStart, kWKInjectInAllFrames);
    }
}

} // namespace
