cmake_minimum_required(VERSION 2.8)

set(ComcastBundle_SOURCES
      InjectedBundleMain.cpp
      BundleController.cpp
      Proxy.cpp
      JavaScriptRequests.cpp
      AVESupport.cpp
      logger.cpp
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Werror")

add_library(ComcastBundle SHARED
    ${ComcastBundle_SOURCES}
)

if(ENABLE_RDK_LOGGER)
  add_definitions(-DUSE_RDK_LOGGER)
  target_link_libraries(ComcastBundle "-lrdkloggers -llog4c")
endif()

set(AVE_LIBRARIES
  -Wl,--no-as-needed
  -lavemedia -lfaxportingkit
  -ljsbindings-jspsdk -lkernel -lxmlreader -ltinyxml2
  -lcrypto -lcryptoModule -lxmlreader -ltinyxml2 -ldl
  -lAVEPlayer -lpsdk -ltrace -lAVEnexusMgr -lnexus -lcrypto -lssl -lcurl
  -lds -ldshalcli -ldl -lm -lIARMBus
)

target_link_libraries(ComcastBundle "${GLIB_LIBRARIES}" ${AVE_LIBRARIES})

install(TARGETS ComcastBundle LIBRARY DESTINATION lib)
install(FILES *.js DESTINATION share/injectedbundle)
