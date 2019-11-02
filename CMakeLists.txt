cmake_minimum_required(VERSION 2.8)

project(mp710Ctrl)

if (NOT CMAKE_BUILD_TYPE)
     set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type_lower)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_C99 -std=c++11 -static-libstdc++ -Wall -fno-exceptions -fno-rtti")
if (build_type_lower STREQUAL "release" OR build_type_lower STREQUAL "minsizerel")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -flto -fno-stack-protector -ffunction-sections -fdata-sections -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_CXX_FLAGS}")
    message(STATUS "Release optimizations are enabled")
else()
    message(STATUS "Release optimizations are not enabled")
endif ()

#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -fno-stack-protector ")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_C99 -std=c++11 -static-libstdc++ -Wall -fno-stack-protector -fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections")
## -fno-exceptions -fno-stack-protector -fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections
##set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -fno-exceptions -fno-stack-protector -fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections")

##set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-stack-protector -fno-rtti -ffunction-sections -fdata-sections -Wl,--gc-sections  ")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  ")
##-fno-exceptions -fno-stack-protector -fno-rtti -ffunction-sections -fdata-sections   -Wl,--gc-sections
include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(thirdparty)
add_subdirectory(tracer)
add_subdirectory(mp710Lib)
add_subdirectory(mp710WebCtrl)
add_subdirectory(mp710Sunrise)
