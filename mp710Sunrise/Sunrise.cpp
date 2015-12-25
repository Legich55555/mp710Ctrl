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
#include <cstdint>
#include <signal.h>
#include <getopt.h>
#include <atomic>
#include <cstdio>

#include "../tracer/Tracer.h"
#include "../mp710Lib/DeviceController.h"

namespace {
    void SignalsHandler(int signal);
    bool IsSignalRaised(void);
    void OnDeviceUpdate(bool result, DeviceController::CommandTypesEnum type, unsigned channelIdx, unsigned param);
    
    void SwitchOff();
    void Sunrise(std::chrono::seconds duration);
}

int main(int argc, char **argv) {

  signal(SIGTERM, SignalsHandler);
  signal(SIGINT, SignalsHandler);
  
  if (1 == argc) {
    const std::chrono::seconds SUNRISE_DURATION(60 * 30);
    
    Sunrise(SUNRISE_DURATION);
  }
  else {
    SwitchOff();
  }
  
  return 0;
}

namespace {
    
  std::atomic<bool> NeedToStopPolling(false);
  
  void SignalsHandler(int signal) {
      Tracer::Log("Interrupted by signal %i.\n", signal);
      NeedToStopPolling = true;
  }
  
  bool IsSignalRaised(void) {
      return NeedToStopPolling;
  }
  
  void OnDeviceUpdate(bool result, DeviceController::CommandTypesEnum type, unsigned channelIdx, unsigned param) {
    Tracer::Log("Executed [%u] command %u at channel %u with param %u.\n",
                static_cast<unsigned>(result),
                static_cast<unsigned>(type),
                static_cast<unsigned>(channelIdx),
                static_cast<unsigned>(param));
  }   
  
  const size_t MAX_QUEUE_SIZE = 100;
  const unsigned RED_CHANNEL_IDX = 14;
  const unsigned GREEN_CHANNEL_IDX = 13;
  const unsigned BLUE_CHANNEL_IDX = 12;
  
  void SwitchOff() {
    DeviceController deviceController(MAX_QUEUE_SIZE, OnDeviceUpdate);

    for (unsigned channelIdx = 0; channelIdx < DeviceController::CHANNELS_NUMBER; ++channelIdx) {
      deviceController.AddCommand(DeviceController::SET_BRIGHTNESS, channelIdx, 0);
    }

    if (deviceController.WaitForCommands(std::chrono::seconds(15))) {
      Tracer::Log("All channels are switched off.\n");
    }
    else {
      Tracer::Log("Failed to switch off.\n");
    }
  }
  
  void Sunrise(std::chrono::seconds duration) {
    DeviceController deviceController(MAX_QUEUE_SIZE, OnDeviceUpdate);
    
    const std::chrono::milliseconds durationMs(duration);
    const std::vector<unsigned> channels {RED_CHANNEL_IDX, GREEN_CHANNEL_IDX, BLUE_CHANNEL_IDX};
    const std::chrono::milliseconds stepDuration = durationMs / DeviceController::BRIGHTNESS_MAX / channels.size();

    for (unsigned brightness = 0; brightness <= DeviceController::BRIGHTNESS_MAX; ++brightness) {
      
      for (auto channel : channels) {
        if (IsSignalRaised()) {
          return;
        }
        
        deviceController.AddCommand(DeviceController::SET_BRIGHTNESS, channel, brightness);
        std::this_thread::sleep_for(stepDuration);
      }

    Tracer::Log("Sun is up.\n");
    }
  }
}
