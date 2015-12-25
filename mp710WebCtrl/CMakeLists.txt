add_custom_target(copyHtmlContent 
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/html ${CMAKE_CURRENT_BINARY_DIR})

add_executable(mp710WebCtrl WebCtrl.cpp)
target_link_libraries(mp710WebCtrl mongoose pthread mp710CtrlLib)
add_dependencies(mp710WebCtrl copyHtmlContent)

install(TARGETS mp710WebCtrl RUNTIME DESTINATION bin)