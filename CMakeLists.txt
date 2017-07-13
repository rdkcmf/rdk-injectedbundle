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
      AVESupport.cpp
      logger.cpp
      WebFilter.cpp
      RequestHeaders.cpp
      TimeZoneSupport.cpp
    )

if(ENABLE_AAMP_JSBINDING)
set(ComcastInjectedBundle_SOURCES
      ${ComcastInjectedBundle_SOURCES}
      AAMPJSController.cpp
    )
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

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

set(AVE_LIBRARIES
  -lAVEPlayer -lds -ldshalcli -ldl -lm -lIARMBus
)

if(ENABLE_AAMP_JSBINDING)
  target_link_libraries(ComcastInjectedBundle "-laampjsbindings")
endif()

if(USE_NX_CLIENT)
  add_definitions(-DUSE_NX_CLIENT)
  list(APPEND AVE_LIBRARIES -lnxclient)
endif()

target_link_libraries(ComcastInjectedBundle "${GLIB_LIBRARIES}" ${AVE_LIBRARIES} -ljansson -lWPEWebKit)

install(TARGETS ComcastInjectedBundle LIBRARY DESTINATION lib)
install(FILES *.js DESTINATION share/injectedbundle)
