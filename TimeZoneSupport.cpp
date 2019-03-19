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
#include "TimeZoneSupport.h"

#include <jansson.h>

#include "sysMgr.h"
#include <libIBus.h>
#include <libIARMCore.h>
#include <xdiscovery.h>

#include "logger.h"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <string.h>
#include <glib.h>
#include <fstream>

using namespace std;

namespace {

#ifdef USE_UPNP_DISCOVERY
#ifdef IARM_USE_DBUS
bool getUpnpResults(char* upnpResults, unsigned int messageLength)
{
    RDKLOG_TRACE("getUpnpResults");
    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t *param = NULL;
    IARM_Result_t ret ;

    /* Allocate enough to store the structure, the message and one more byte for the string termintor */
    IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + messageLength + 1, (void**)&param);
    param->bufLength = messageLength;

    ret = IARM_Bus_Call(_IARM_XUPNP_NAME, IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo,
        (void *)param, sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t) + messageLength + 1);

    if(ret == IARM_RESULT_SUCCESS)
    {
        RDKLOG_TRACE("IARM_RESULT_SUCCESS");
        memcpy(upnpResults, ((char *)param + sizeof(IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t)), param->bufLength);
        upnpResults[param->bufLength] = '\0';
        IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, param);
    }

    return true;
}
#else
bool getUpnpResults(char* upnpResults, unsigned int messageLength)
{
    RDKLOG_TRACE("getUpnpResults");
    IARM_Bus_SYSMGR_GetXUPNPDeviceInfo_Param_t param;
    IARM_Result_t ret ;
    param.bufLength = messageLength;

    ret = IARM_Bus_Call(_IARM_XUPNP_NAME,IARM_BUS_XUPNP_API_GetXUPNPDeviceInfo, (void *)&param,sizeof(param));

    if(ret == IARM_RESULT_SUCCESS)
    {
        RDKLOG_TRACE("IARM_RESULT_SUCCESS");
        memcpy(upnpResults,param.pBuffer,param.bufLength);
        upnpResults[param.bufLength] = '\0';
        IARM_Free(IARM_MEMTYPE_PROCESSSHARE,param.pBuffer);
    }

    return true;
}
#endif
#endif

typedef unordered_map<string, string> StringHash;
typedef vector<StringHash> VectorOfMaps;

void jsonProcessArray(json_t* array, VectorOfMaps& upnpDevices, int& numberOfUpnpResults)
{
    if(!json_is_array(array))
    {
        RDKLOG_ERROR("error: passed object is not an array");
        json_decref(array);
        return;
    }

    for(size_t i = 0; i < json_array_size(array); i++)
    {
        RDKLOG_TRACE("array iteration %d", i);
        json_t *value, *object;
        const char* key;

        object = json_array_get(array, i);
        if(!json_is_object(object))
        {
            RDKLOG_ERROR("error: commit data %d is not an object", i + 1);
            continue;
        }
        StringHash hash;
        json_object_foreach(object, key, value) {
            RDKLOG_TRACE("appending %s:%s to hash", key, json_string_value(value));
            hash[key] = json_string_value(value);
        }

        upnpDevices.push_back(hash);
        numberOfUpnpResults++;
    }
}

VectorOfMaps jsonToListOfMaps(const char* upnpResults)
{
    RDKLOG_TRACE("jsonToListOfMaps");

    int numberOfUpnpResults = 0;
    VectorOfMaps upnpDevices;
    json_t *root = nullptr;

    if (upnpResults != NULL)
    {
        json_error_t error;
        root = json_loads(upnpResults, 0, &error);

        if(!root)
        {
            RDKLOG_WARNING("upnp results from xupnp: %s", upnpResults);
            RDKLOG_ERROR("JSON parsing error on line %d: %s", error.line, error.text);
            return upnpDevices;
        }

        if (json_is_object(root))
        {
            RDKLOG_INFO("root is an object, using the value");
            void *iter = json_object_iter(root);
            json_t* array = json_object_iter_value(iter);
            jsonProcessArray(array, upnpDevices, numberOfUpnpResults);
        } else {
            RDKLOG_INFO("root is an array, using it");
            jsonProcessArray(root, upnpDevices, numberOfUpnpResults);
        }

        json_decref(root);
    }

    RDKLOG_INFO("number of upnp results: %d", numberOfUpnpResults);
    return upnpDevices;
}

void timeZoneUpdated(string newZone)
{
    string tzOut;
    string timeZone(newZone);
    if(timeZone.compare("US/Eastern")==0)
        tzOut = "EST05EDT";
    else if(timeZone.compare("US/Central")==0)
        tzOut = "CST06CDT";
    else if(timeZone.compare("US/Mountain")==0)
        tzOut = "MST07MDT";
    else if(timeZone.compare("US/Pacific")==0)
        tzOut = "PST08PDT";
    else if(timeZone.compare("US/Alaska")==0)
        tzOut = "AKST09AKDT";
    else if(timeZone.compare("US/Hawaii")==0)
        tzOut = "HST11HDT";
    else if(timeZone.compare("US/Arizona")==0)
        tzOut = "MST07";
    else if(timeZone.compare("null") != 0 && timeZone.length() > 0)//just in case its already in correct format
        tzOut = timeZone;

    RDKLOG_INFO("timeZoneUpdated timeZone in=%s out=%s", timeZone.c_str(), tzOut.c_str());

    if(tzOut.length() > 0)
    {
        RDKLOG_WARNING("TZ set to %s", tzOut.c_str());
        string *data = new string(tzOut);
        g_timeout_add(0, [](void* data) -> gboolean {
                string* newZone = static_cast<string*>(data);

                RDKLOG_INFO("Setting new timezone: %s", newZone->c_str());
                setenv("TZ", newZone->c_str(), 1);
                delete newZone;
                return G_SOURCE_REMOVE;
            }, data);
    }
    else
    {
        RDKLOG_WARNING("TZ not set");
    }

}

void parseResults(const VectorOfMaps& m_upnpList)
{
    static string oldTimeZone = "null";
    string newTimeZone = "null";

    VectorOfMaps::const_iterator newTz = find_if(m_upnpList.cbegin(), m_upnpList.cend(), [] (const StringHash& hash){
        const auto tzIter = hash.find("timezone");
        return tzIter != hash.end() && !tzIter->second.empty();
        });

    if (newTz != m_upnpList.end()) {
        RDKLOG_INFO("Found timezone: %s", newTz->at("timezone").c_str());
        newTimeZone = newTz->at("timezone");
    }

    if ((oldTimeZone != newTimeZone) && (!newTimeZone.empty()))
    {
        oldTimeZone = newTimeZone;
        RDKLOG_WARNING("Time zone updated: %s", oldTimeZone.c_str());
        timeZoneUpdated(newTimeZone);
    }

    if((newTimeZone.compare("null")==0) && (oldTimeZone.compare("null")==0))
    {
        RDKLOG_WARNING("GRAM mode device. So no TZ info from xupnp");
        std::ifstream fileStream("/opt/persistent/timeZoneDST");
        if (!fileStream.is_open())
        {
            RDKLOG_WARNING("Not able to open timeZoneDST file");
            return;
        }

        std::string tzName;
        if (std::getline(fileStream, tzName) && (!tzName.empty()))
        {
                //TZ setting as per https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html#
            string timeZone = ":"+ tzName;
            string *data = new string(timeZone);
            g_timeout_add(0, [](void* data) -> gboolean {
                    string* timeZoneData = static_cast<string*>(data);
                    RDKLOG_INFO("Setting timezone: %s", timeZoneData->c_str());
                    setenv("TZ", timeZoneData->c_str(), 1);
                    delete timeZoneData;
                    return G_SOURCE_REMOVE;
            }, data);
        }
        else
        {
            RDKLOG_WARNING("Timezone File read is failed");
        }

        fileStream.close();
     }

    RDKLOG_TRACE("upnp results updated");
}

void newUpnpResultsAvailable(const VectorOfMaps& res)
{
    parseResults(res);
}

void requestUpnpList()
{
    RDKLOG_WARNING("requesting upnp results");
    IARM_Bus_SYSMgr_EventData_t eventData;
    eventData.data.xupnpData.deviceInfoLength = 0;
    IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, (IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_REQUEST,(void *)&eventData, sizeof(eventData));
}

void _evtHandler(const char *owner_c, IARM_EventId_t eventId, void *data, size_t /*len*/)
{
    string owner(owner_c);
    if (owner == IARM_BUS_SYSMGR_NAME)
    {
        switch(eventId)
        {
            case IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE:
            {
#ifdef USE_UPNP_DISCOVERY
                IARM_Bus_SYSMgr_EventData_t *eventData = (IARM_Bus_SYSMgr_EventData_t*)data;
                int messageLength = eventData->data.xupnpData.deviceInfoLength;
                RDKLOG_INFO("IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE messageLength : %d", messageLength);
                char upnpResults[messageLength+1];
                memset(upnpResults, 0, messageLength+1);
                getUpnpResults(upnpResults, messageLength);
                newUpnpResultsAvailable(jsonToListOfMaps(upnpResults));
#endif
                break;
            }
        }
    }
}

void registerUpdateListener()
{
    // next time zone source: upnp
#ifdef USE_UPNP_DISCOVERY
    IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE, _evtHandler);
    requestUpnpList();
#endif
}

} //namespace

namespace TimeZoneSupport
{

void initialize(void)
{
    registerUpdateListener();
}

// FIXME: where to call this from?
void shutdown()
{
#ifdef USE_UPNP_DISCOVERY
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_XUPNP_DATA_UPDATE);
#endif
}

}
