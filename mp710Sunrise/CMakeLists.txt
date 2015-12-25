add_executable(mp710Sunrise Sunrise.cpp)
target_link_libraries(mp710Sunrise pthread mp710CtrlLib)

install(TARGETS mp710Sunrise RUNTIME DESTINATION bin)
