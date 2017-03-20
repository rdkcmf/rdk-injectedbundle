cmake_minimum_required(VERSION 2.8)

set(ComcastInjectedBundle_SOURCES
      InjectedBundleMain.cpp
      BundleController.cpp
      Proxy.cpp
      JavaScriptRequests.cpp
      AVESupport.cpp
      logger.cpp
      WebFilter.cpp
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -Wall -Wextra -Werror")

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

target_link_libraries(ComcastInjectedBundle "${GLIB_LIBRARIES}" ${AVE_LIBRARIES})

install(TARGETS ComcastInjectedBundle LIBRARY DESTINATION lib)
install(FILES *.js DESTINATION share/injectedbundle)
