/*******************************************************************************
#                                                                              #
# This file is part of mp710Ctrl.                                              #
#                                                                              #
# Copyright (C) 2015 Oleg Efremov                                              #
#                                                                              #
# mp710Ctrl is free software; you can redistribute it and/or modify            #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <cstdlib>
#include <signal.h>
#include <atomic>

#include <libusb-1.0/libusb.h>

#include "Tracer.h"

namespace {

  std::atomic<bool> NeedToStopWorkFunc(false);

  void sigIntHandler(int sig) {
    Tracer::Log("Interruption....\n");
    NeedToStopWorkFunc = true;
  }
  
  bool IsSigIntRaised(void) {
    return NeedToStopWorkFunc;
  }
  
}

class ControlMessage 
{
public:
  ControlMessage(unsigned char channelIdx, unsigned char brightness)
    : _commandData {0x63,channelIdx,brightness,0x00,0x08,0xff,0x08,0xff}
  {
  }
  
  unsigned char* GetData()
  {
    return &_commandData[0];
  }
  
  ControlMessage() = delete;
  ControlMessage(const ControlMessage&) = delete;
  ControlMessage& operator=(const ControlMessage&) = delete;
  
private:
  unsigned char _commandData[8];
};

namespace {
  const uint16_t DEV_VID = 0x16c0;
  const uint16_t DEV_PID = 0x05df;
  const int DEV_CONFIG = 1;
  const int DEV_INTF = 0;
  unsigned char EP_IN = 0x81;
}

int main(int argc, char **argv) {

  // Register handler for CTRL+C
  if (signal(SIGINT, sigIntHandler) == SIG_ERR) {
    Tracer::Log("Failed to setup SIGINT handler.\n");
  }
    
  // Register handler for termination
  if (signal(SIGTERM, sigIntHandler) == SIG_ERR) {
    Tracer::Log("Failed to setup SIGTERM handler.\n");
  }
  
  libusb_init(nullptr);
  libusb_set_debug(nullptr, 3);
  libusb_device_handle* handle = libusb_open_device_with_vid_pid(nullptr, DEV_VID, DEV_PID);
  if (handle == nullptr) {
      Tracer::Log("Failed to open device\n");
      libusb_exit(nullptr);
      return EXIT_FAILURE;
  }
  
  if (libusb_kernel_driver_active(handle, DEV_INTF))
  {
    libusb_detach_kernel_driver(handle, DEV_INTF);
  }
  
  int ret;
  if ((ret = libusb_set_configuration(handle, DEV_CONFIG)) < 0)
  {
    Tracer::Log("Failed to configure device, error: %i.\n", ret);
    libusb_close(handle);
    libusb_exit(nullptr);
    if (ret == LIBUSB_ERROR_BUSY)
    {
        Tracer::Log("Device is busy\n");
    }
    
    return EXIT_FAILURE;
  }
  
  if (libusb_claim_interface(handle,  DEV_INTF) < 0)
  {
    Tracer::Log("Failed to claim interface.\n");
    libusb_close(handle);
    libusb_exit(nullptr);
    return EXIT_FAILURE;
  }
  
  for (unsigned brightness = 0; brightness <= 128; ++brightness)
  {
    for (unsigned portIdx = 0; portIdx < 16; ++portIdx)
    {
      unsigned char buf[65];
      
      ControlMessage msg(portIdx, brightness);
      ret = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
                                    0x9, 0x300, 0, msg.GetData(), 8, 100);
      libusb_interrupt_transfer(handle, EP_IN, buf, 8, &ret, 100);
    }
  }
  
  libusb_attach_kernel_driver(handle, DEV_INTF);
  libusb_close(handle);
  libusb_exit(nullptr);  
  
  return EXIT_SUCCESS;
}
