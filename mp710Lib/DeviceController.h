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
#include <mutex>
#include <thread>
#include <cstdint>
#include <functional>
#include <condition_variable>

class DeviceController
{
public:
    const static std::uint8_t kChannelNumber;
    const static std::uint8_t kBrightnessMax;

    enum CommandType : std::uint8_t
    {
        kNotSet = 0U,
        kSetBrightness = 1U,
        kStartSunrise = 2U,
        kStartSunset = 3U,
    };

    struct Command
    {
        CommandType Type = CommandType::kNotSet;
        std::int8_t ChannelIdx = INT8_MAX;
        std::uint8_t Param1 = 0U;
    };

    struct Channel
    {
        std::int8_t Idx = INT8_MAX;
        std::uint8_t Param1 = 0U;
    };

    typedef std::function<void(bool result, const DeviceController::Command& command)> OnChangeCallback;

    typedef std::function<std::vector<Channel>(const std::vector<std::uint8_t>& startState,
        const std::chrono::milliseconds& duration,
        const std::chrono::milliseconds& elapsed)>
        TransitionControllerCallback;

    DeviceController(size_t commandQueueSize, OnChangeCallback doneCallback);
    ~DeviceController();

    void RunTransition(TransitionControllerCallback controllerCallback, std::chrono::milliseconds duration);
    void AddCommand(CommandType type, std::uint8_t param, const std::vector<std::int8_t>& channels);
    void AddCommand(CommandType type, std::uint8_t param, std::int8_t channelIdx);
    void AddCommand(const Command& command);
    bool WaitForCommands(std::chrono::milliseconds timeout);
    void Reset();
    std::vector<Channel> GetChannelValues() const;

    DeviceController(const DeviceController&) = delete;
    DeviceController& operator=(const DeviceController&) = delete;

private:
    bool ExecCommand(const Command& command);
    void WorkerThreadFunc();

    const size_t _maxQueueSize;
    volatile bool _shouldStop;
    std::list<Command> _commandsQueue;
    std::vector<std::uint8_t> _lastChannelValues;

    std::thread _workerThread;
    mutable std::mutex _queueMutex;
    mutable std::condition_variable _isQueueEmptyCondition;

    OnChangeCallback _onChangeCallback;

    std::vector<std::uint8_t> _transitionStartState;
    std::chrono::steady_clock::time_point _transitionStartTime;
    std::chrono::milliseconds _transitionDuration;
    TransitionControllerCallback _transitionControllerCallback;
};

#endif  // DEVICECONTROLLER_H
