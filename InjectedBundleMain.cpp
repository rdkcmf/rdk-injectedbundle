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

#ifdef ENABLE_AVE
#include "AVESupport.h"
#endif

#include "TimeZoneSupport.h"

#ifdef ENABLE_AAMP_JSBINDING
#include "AAMPJSController.h"
#endif
#ifdef ENABLE_VIRTUAL_KEYBOARD
#include "VirtualKeyboard.h"
#endif
#ifdef ENABLE_APP_SECRET
#include "ApplicationSecret.h"
#endif
#include "logger.h"

#include <WebKit/WKBundleInitialize.h>

#include <stdlib.h>
#include <stdio.h>

#include <curl/curl.h>

#ifdef ENABLE_IARM

extern "C" {

#include <rdk/iarmbus/libIBus.h>
#include <rdk/iarmbus/libIBusDaemon.h>
#include <rdk/iarmbus/libIARMCore.h>

}

static void initIARM() {
    char Init_Str[] = "WPE_WEBKIT_PROCESS";
    IARM_Bus_Init(Init_Str);
    IARM_Bus_Connect();
}

#endif

extern "C" void WKBundleInitialize(WKBundleRef bundle, WKTypeRef initializationUserData)
{
    RDK::logger_init();

    curl_global_init(CURL_GLOBAL_ALL);
    RDKLOG_INFO("using curl:%s", curl_version());

#ifdef ENABLE_IARM
    initIARM();
#endif

    JSBridge::initialize(bundle, initializationUserData);

#ifdef ENABLE_AVE
    AVESupport::initialize();
#endif

#ifdef ENABLE_IARM
    TimeZoneSupport::initialize();
#endif

#ifdef ENABLE_AAMP_JSBINDING
    AAMPJSController::initialize();
#endif

#ifdef ENABLE_VIRTUAL_KEYBOARD
    VirtualKeyboard::initialize();
#endif

#ifdef ENABLE_APP_SECRET
    ApplicationSecret::initialize();
#endif
}
