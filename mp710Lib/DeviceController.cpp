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

#include "DeviceController.h"

#include <algorithm>
#include <libusb-1.0/libusb.h>

#include "../tracer/Tracer.h"

namespace {
  const uint16_t DEV_VID = 0x16c0;
  const uint16_t DEV_PID = 0x05df;
  const int DEV_CONFIG = 1;
  const int DEV_INTF = 0;
  unsigned char EP_IN = 0x81;
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

// int main1(int argc, char **argv) {
// 
//   // Register handler for CTRL+C
//   if (signal(SIGINT, sigIntHandler) == SIG_ERR) {
//     Tracer::Log("Failed to setup SIGINT handler.\n");
//   }
//     
//   // Register handler for termination
//   if (signal(SIGTERM, sigIntHandler) == SIG_ERR) {
//     Tracer::Log("Failed to setup SIGTERM handler.\n");
//   }
//   
//   libusb_init(nullptr);
//   libusb_set_debug(nullptr, 3);
//   libusb_device_handle* handle = libusb_open_device_with_vid_pid(nullptr, DEV_VID, DEV_PID);
//   if (handle == nullptr) {
//       Tracer::Log("Failed to open device\n");
//       libusb_exit(nullptr);
//       return EXIT_FAILURE;
//   }
//   
//   if (libusb_kernel_driver_active(handle, DEV_INTF))
//   {
//     libusb_detach_kernel_driver(handle, DEV_INTF);
//   }
//   
//   int ret;
//   if ((ret = libusb_set_configuration(handle, DEV_CONFIG)) < 0)
//   {
//     Tracer::Log("Failed to configure device, error: %i.\n", ret);
//     libusb_close(handle);
//     libusb_exit(nullptr);
//     if (ret == LIBUSB_ERROR_BUSY)
//     {
//         Tracer::Log("Device is busy\n");
//     }
//     
//     return EXIT_FAILURE;
//   }
//   
//   if (libusb_claim_interface(handle,  DEV_INTF) < 0)
//   {
//     Tracer::Log("Failed to claim interface.\n");
//     libusb_close(handle);
//     libusb_exit(nullptr);
//     return EXIT_FAILURE;
//   }
//   
//   unsigned char buf[65];
//         
//   if (argc > 1)
//   {
//     const unsigned char OFF_BRIGHTNESS = 0x0;
//     
//     for (unsigned portIdx = 0; portIdx < 16; ++portIdx)
//     {
//       ControlMessage msg(portIdx, 0);
//       ret = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
//                                     0x9, 0x300, 0, msg.GetData(), 8, 100);
//       libusb_interrupt_transfer(handle, EP_IN, buf, 8, &ret, 100);
//     }
// 
//     Tracer::Log("Set brightness to %d.\n", OFF_BRIGHTNESS);
//   }
//   else 
//   {
//     for (unsigned brightness = 0; brightness <= 128; ++brightness)
//     {
//       for (unsigned portIdx = 0; portIdx < 16; ++portIdx)
//       {
//         ControlMessage msg(portIdx, brightness);
//         ret = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
//                                       0x9, 0x300, 0, msg.GetData(), 8, 100);
//         libusb_interrupt_transfer(handle, EP_IN, buf, 8, &ret, 100);
//       }
// 
//       Tracer::Log("Set brightness to %d.\n", brightness);
//       
//       std::this_thread::sleep_for(std::chrono::seconds(15));
//     }
//   }
//   
//   libusb_attach_kernel_driver(handle, DEV_INTF);
//   libusb_close(handle);
//   libusb_exit(nullptr);  
//   
//   return EXIT_SUCCESS;
// }

const unsigned DeviceController::CHANNELS_NUMBER = 16;
const unsigned DeviceController::BRIGHTNESS_MAX = 128;

DeviceController::DeviceController(size_t maxCommandQueueSize, DoneCallback doneCallback)
    : _maxQueueSize(maxCommandQueueSize),
    _shouldStop(false),
    _lastCommands(CHANNELS_NUMBER),
    _doneCallback(doneCallback)
{
    for (size_t i = 0; i < _lastCommands.size(); ++i) {
        _lastCommands[i].ChannelIdx = i;
    }
    
    std::thread workerThread(&DeviceController::WorkerThreadFunc, this);    
    _workerThread.swap(workerThread); 
}

DeviceController::~DeviceController() {
    _shouldStop = true;
    _workerThread.join();
}

void DeviceController::AddCommand(CommandTypesEnum type, unsigned channelIdx, unsigned param) {
    AddCommand(Command (type, channelIdx, param));
}

void DeviceController::AddCommand(const Command& command) {
    {
      std::unique_lock<std::mutex> queueLock(_queueMutex);
      
      if (_maxQueueSize < _commandsQueue.size()) {
          Tracer::Log("Dropped command because the command queue is full.\n");
          return;
      }

      _commandsQueue.push_back(command);
  }
}

bool DeviceController::WaitForCommands(std::chrono::milliseconds timeout) {

  while (true)
  {
    {
      std::unique_lock<std::mutex> queueLock(_queueMutex);
      if (_commandsQueue.empty())
      {
          return true;
      }
    }
      
    {
      std::unique_lock<std::mutex> lock(_isQueueEmptyMutex);
      if (_isQueueEmptyCondition.wait_for(lock, timeout) == std::cv_status::timeout)
      {
          return false;
      }
    }
      
    {
      std::unique_lock<std::mutex> queueLock(_queueMutex);
      if (_commandsQueue.empty())
      {
          return true;
      }
    }

    // It is a spurious wakeup
  }
}

void DeviceController::Reset() {
}

std::tuple<DeviceController::CommandTypesEnum, unsigned> DeviceController::GetLastCommand(unsigned int channelIdx) const {
   return std::tuple<DeviceController::CommandTypesEnum, unsigned>(DeviceController::NOT_SET, 0); 
}

std::vector<DeviceController::Command> DeviceController::GetLastCommands() const {
    return _lastCommands;
}

bool DeviceController::ExecCommand(const Command& command) {

  libusb_device_handle* handle = libusb_open_device_with_vid_pid(nullptr, DEV_VID, DEV_PID);
  if (nullptr == handle) {
      Tracer::Log("Failed to open device\n");
      return false;
  }
  
  if (libusb_kernel_driver_active(handle, DEV_INTF))
  {
    libusb_detach_kernel_driver(handle, DEV_INTF);
  }
  
  int ret;
  if ((ret = libusb_set_configuration(handle, DEV_CONFIG)) < 0)
  {
    Tracer::Log("Failed to configure device, error: %i.\n", ret);
    if (ret == LIBUSB_ERROR_BUSY)
    {
        Tracer::Log("Device is busy\n");
    }
    
    libusb_close(handle);
    return false;
  }
  
  if (libusb_claim_interface(handle,  DEV_INTF) < 0)
  {
    Tracer::Log("Failed to claim interface.\n");
    
    libusb_close(handle);
    return false;
  }
  
  ControlMessage msg(command.ChannelIdx, command.Param);
  ret = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
                                0x9, 0x300, 0, msg.GetData(), 8, 100);
  
  unsigned char buf[65];
  libusb_interrupt_transfer(handle, EP_IN, buf, 8, &ret, 100);

  Tracer::Log("Set brightness to %d.\n", command.Param);
  
  libusb_attach_kernel_driver(handle, DEV_INTF);

  libusb_close(handle);
  
  _lastCommands[command.ChannelIdx] = command;
  
  return true;  
}

void DeviceController::WorkerThreadFunc() {
  
  libusb_init(nullptr);
  libusb_set_debug(nullptr, 3);

  while (!_shouldStop) {
        
        Command cmd;
        
        {
            std::unique_lock<std::mutex> queueLock(_queueMutex);
            
            if (_commandsQueue.size() != 0) {
                cmd = _commandsQueue.front();
                _commandsQueue.pop_front();

                auto isSameChannelIdx = [&cmd](const Command& item) {
                  return item.ChannelIdx == cmd.ChannelIdx;
                };

                // Find tha last command with same channelIdx
                auto rIt = std::find_if(_commandsQueue.rbegin(), _commandsQueue.rend(), isSameChannelIdx);
                if (rIt != _commandsQueue.rend()) {
                  cmd = *rIt;
                  
                  _commandsQueue.remove_if(isSameChannelIdx);
                }
            }
            else {
              _isQueueEmptyCondition.notify_one();
            }
        }
        
        if (cmd.Type != NOT_SET) {
            ExecCommand(cmd);
            
            if (_doneCallback != nullptr) {
                _doneCallback(true, cmd.Type, cmd.ChannelIdx, cmd.Param);
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
  }
    
  libusb_exit(nullptr);  
    
}