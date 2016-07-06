cmake_minimum_required(VERSION 2.8)

set(ComcastBundle_SOURCES
      InjectedBundleMain.cpp
      BundleController.cpp
      Proxy.cpp
      JavaScriptRequests.cpp
    )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(ComcastBundle SHARED
    ${ComcastBundle_SOURCES}
)

install(TARGETS ComcastBundle LIBRARY DESTINATION lib)
install(FILES ServiceManager.js DESTINATION share/injectedbundle)