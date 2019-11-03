add_executable(mp710Sunrise Sunrise.cpp)
target_link_libraries(mp710Sunrise pthread mp710CtrlLib)
set_target_properties(mp710Sunrise PROPERTIES ENABLE_EXPORTS OFF)

install(TARGETS mp710Sunrise RUNTIME DESTINATION bin)
