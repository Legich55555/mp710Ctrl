cmake_minimum_required(VERSION 2.8)

project(mp710Ctrl)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_C99 -std=c++11 -static-libstdc++ -Wall -fno-exceptions")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(mp710Ctrl AppMain.cpp Tracer.cpp Tracer.h)
target_link_libraries(mp710Ctrl usb-1.0)

install(TARGETS mp710Ctrl RUNTIME DESTINATION bin)
