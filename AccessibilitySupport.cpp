/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

#include "AccessibilitySupport.h"
#include "utils.h"
#include "logger.h"

#include <WebKit/WKType.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKString.h>

#include <cassert>

#include <rdkat.h>

#define CHECK_CONDITION(condition, log) \
do { \
    if (!(condition)) { \
        RDKLOG_ERROR(log); \
        assert(false); \
        return; \
    } \
} while(0)

void setPageMediaVolume(void *page, float volume, bool restore)
{
    static double tVolume = 0.0;
    if(page) {
        double t = WKBundlePageGetVolume((WKBundlePageRef)page);
        RDKLOG_INFO("Read mediaVolume is : %lf", t);

        if(!restore) {
            // Backup the original volume, if not done already
            if(tVolume == 0)
                tVolume = t;
        } else {
            // Revert back to the original volume (ignore the "volume" param)
            volume = tVolume;
            tVolume = 0;
        }

        if(volume == t)
            return;

        RDKLOG_INFO("setMediaVolume to : %lf", volume);
        WKBundlePageSetVolume((WKBundlePageRef)page, volume);
    }
}

void passAccessibilitySettingsToRDKAT(WKTypeRef accessibilitySettingsRef)
{
    CHECK_CONDITION(WKGetTypeID(accessibilitySettingsRef) == WKArrayGetTypeID(), "accessibility_settings is not an array");
    WKArrayRef array = static_cast<WKArrayRef>(accessibilitySettingsRef);
    size_t arraySize = array ? WKArrayGetSize(array) : 0;
    CHECK_CONDITION(arraySize == 5, "incorrect params's size");

    struct RDK_AT::TTSConfiguration config;
    config.m_ttsEndPoint = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex(array, 0));
    config.m_ttsEndPointSecured = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex(array, 1));
    config.m_language = Utils::toStdString((WKStringRef) WKArrayGetItemAtIndex(array, 2));
    config.m_rate = (uint8_t)WKUInt64GetValue((WKUInt64Ref)(WKArrayGetItemAtIndex(array, 3)));
    bool enableVoiceGuidance = WKBooleanGetValue((WKBooleanRef)(WKArrayGetItemAtIndex(array, 3)));
    RDKLOG_INFO("accessibility_settings ttsEndPoint=%s, ttsEndPointSecured=%s, language=%s, rate=%d, enableVoiceGuidance=%d.",
            config.m_ttsEndPoint.c_str(),config.m_ttsEndPointSecured.c_str(),config.m_language.c_str(),config.m_rate,enableVoiceGuidance);

    RDK_AT::EnableVoiceGuidance(enableVoiceGuidance);
    RDK_AT::ConfigureTTS(config);
}

