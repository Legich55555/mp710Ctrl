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

#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <list>
#include <vector>
#include <tuple>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>

class DeviceController {
public:
    
    static const unsigned char CHANNELS_NUMBER;
    static const unsigned char BRIGHTNESS_MAX;
    
    enum CommandTypesEnum {SET_BRIGHTNESS = 0, NOT_SET = 0xFFFF};
    
    struct Command
    {
        CommandTypesEnum Type;
        unsigned char ChannelIdx;
        unsigned char Param;
        
        Command() 
            : Type(NOT_SET), ChannelIdx(CHANNELS_NUMBER), Param(0)
        { }

        Command(CommandTypesEnum type, unsigned char channelIdx, unsigned char param)
            : Type(type), ChannelIdx(channelIdx), Param(param)
        { }
    };    
    
    typedef std::function<void(bool result, DeviceController::CommandTypesEnum command, unsigned channelIdx, unsigned param)> DoneCallback;
    
    DeviceController(size_t maxCommandQueueSize, DoneCallback doneCallback);
    ~DeviceController();
    
    void AddCommand(CommandTypesEnum type, unsigned char channelIdx, unsigned char param);
    void AddCommand(const Command& command);
    bool WaitForCommands(std::chrono::milliseconds timeout);
    void Reset();
    std::vector<Command> GetLastCommands() const;
    
    DeviceController(const DeviceController&) = delete;
    DeviceController& operator=(const DeviceController&) = delete;

private:    
    
    bool ExecCommand(const Command& command);
    void WorkerThreadFunc();
    
    const size_t _maxQueueSize;
    volatile bool _shouldStop;
    std::list<Command> _commandsQueue;
    std::vector<Command> _lastCommands;

    std::thread _workerThread;
    mutable std::mutex _queueMutex;
    mutable std::condition_variable _isQueueEmptyCondition;
    
    DoneCallback _doneCallback;    
};

#endif // DEVICECONTROLLER_H
