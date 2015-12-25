cmake_minimum_required(VERSION 2.8)

project(mp710Ctrl)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_C99 -std=c++11 -static-libstdc++ -Wall -fno-exceptions")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_subdirectory(thirdparty)
add_subdirectory(tracer)
add_subdirectory(mp710Lib)
add_subdirectory(mp710WebCtrl)
add_subdirectory(mp710Sunrise)
