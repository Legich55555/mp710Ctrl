add_library(mp710CtrlLib STATIC DeviceController.cpp DeviceController.h )
target_link_libraries(mp710CtrlLib usb-1.0 pthread tracer)
