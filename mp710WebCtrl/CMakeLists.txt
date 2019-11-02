add_custom_target(copyHtmlContent 
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/html ${CMAKE_CURRENT_BINARY_DIR})

# WebCtrl which uses mongoose
add_executable(mp710WebCtrl WebCtrl.cpp)
target_link_libraries(mp710WebCtrl mongoose pthread mp710CtrlLib)
add_dependencies(mp710WebCtrl copyHtmlContent)
install(TARGETS mp710WebCtrl RUNTIME DESTINATION bin)

# WebCtrl which uses civetweb
#add_executable(mp710WebCtrlCvt WebCtrlCvt.cpp)
#target_link_libraries(mp710WebCtrlCvt c-library pthread mp710CtrlLib)
#add_dependencies(mp710WebCtrlCvt copyHtmlContent)
#install(TARGETS mp710WebCtrlCvt RUNTIME DESTINATION bin)
