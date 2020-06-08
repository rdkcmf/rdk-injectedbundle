##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2017 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
cmake_minimum_required(VERSION 2.8)

set(ComcastInjectedBundle_SOURCES
      InjectedBundleMain.cpp
      BundleController.cpp
      Proxy.cpp
      JavaScriptRequests.cpp
      logger.cpp
      WebFilter.cpp
      RequestHeaders.cpp
      AccessibilitySupport.cpp
      NavMetrics.cpp
    )

if(ENABLE_AVE)
  set(ComcastInjectedBundle_SOURCES
        ${ComcastInjectedBundle_SOURCES}
        AVESupport.cpp
      )
  add_definitions(-DENABLE_AVE)

  #FIXME timezone support requires iARM, which is initialized in AVESupport.cpp
  set(ComcastInjectedBundle_SOURCES
        ${ComcastInjectedBundle_SOURCES}
        TimeZoneSupport.cpp
      )
endif()

if(ENABLE_AAMP_JSBINDING)
set(ComcastInjectedBundle_SOURCES
      ${ComcastInjectedBundle_SOURCES}
      AAMPJSController.cpp
    )
add_definitions(-DENABLE_AAMP_JSBINDING)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(JSC QUIET wpe-javascriptcore)

set(IARM_INCLUDES
  "-I=/usr/include/rdk/iarmmgrs/sysmgr  -I=/usr/include/rdk/iarmbus"
)

add_definitions(
  -DUSE_UPNP_DISCOVERY=1
  -DIARM_USE_DBUS=1
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -Wall -Wextra -Werror ${IARM_INCLUDES}  -z defs")

add_library(ComcastInjectedBundle SHARED
    ${ComcastInjectedBundle_SOURCES}
)

if(ENABLE_RDK_LOGGER)
  add_definitions(-DUSE_RDK_LOGGER)
  target_link_libraries(ComcastInjectedBundle "-lrdkloggers -llog4c")
endif()

if(ENABLE_AVE)
  target_link_libraries(ComcastInjectedBundle "-lAVEPlayer -lds -ldshalcli -ldl -lm")
endif()

if(ENABLE_AAMP_JSBINDING)
  target_link_libraries(ComcastInjectedBundle "-laampjsbindings")
endif()

if(USE_NX_CLIENT)
  add_definitions(-DUSE_NX_CLIENT)
  target_link_libraries(ComcastInjectedBundle "-lnxclient")
endif()

if(USE_INTELCE)
  target_link_libraries(ComcastInjectedBundle "-lgdl")
  add_definitions(-DUSE_INTELCE=1)
endif()

pkg_check_modules(WPE_WEBKIT QUIET wpe-webkit)
if(NOT WPE_WEBKIT_FOUND)
  pkg_check_modules(WPE_WEBKIT QUIET wpe-webkit-0.1)
endif()

target_link_libraries(ComcastInjectedBundle "${GLIB_LIBRARIES}" "${JSC_LIBRARIES}" -ljansson "${WPE_WEBKIT_LIBRARIES}" -lrdkat -lIARMBus -lcurl -lssl)

install(TARGETS ComcastInjectedBundle LIBRARY DESTINATION lib)
install(FILES *.js DESTINATION share/injectedbundle)
