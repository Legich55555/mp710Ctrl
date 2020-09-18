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

#include <libusb-1.0/libusb.h>

#include "../tracer/Tracer.h"

namespace
{

constexpr std::uint16_t kDeviceVendorId = 0x16C0;
constexpr uint16_t kDeviceProductId = 0x05DF;
constexpr int kDeviceConfig = 1;
constexpr int kDeviceInterface = 0;

enum DeviceCommand : std::uint8_t
{
    kWriteToRegister = 0x63,
    kReadRegister = 0x36
};

enum ChangeType : std::uint8_t
{
    kNC = 0x00,  // Не изменяет REG[X]
    kINC = 0x01,  // Увеличение на 1 от заданного значения до 128
    kDEC = 0x02,  // Уменьшение на 1 от заданного значения до 0
    kINC_OFF
    = 0x03,  // Циклическое увеличение на 1 от заданного значения до 128, установка в 0, далее увеличение на 1 и т. д.
    kDEC_ON
    = 0x04,  // Циклическое уменьшение на 1 от заданного значения до 0, установка в 128, далее уменьшение на 1 и т. д.
    kINC_DEC = 0x05,  // Циклическое увеличение на 1 от заданного значения до 128, далее уменьшение на 1 до 0 и т. д.
    kDEC_INC = 0x06,  // Циклическое уменьшение на 1 от заданного значения до 0, далее увеличение на 1 до 128 и т. д.
    kINC_M_DEC
    = 0x07,  // Похоже на INC_DEC и DEC_INC, но изменение скважности нелинейное для лучшего восприятия глазом.
    kDEC_M_INC
    = 0x08,  // Похоже на INC_DEC и DEC_INC, но изменение скважности нелинейное для лучшего восприятия глазом.
    kRUN1 = 0x09,  // Устанавливается значение 128 если бит 0 счётчика программы PRG<0>=0, иначе 0
    kRUN2 = 0x0A,  // Устанавливается значение 0 если бит 0 счётчика программы PRG<0>=0, иначе 128
    kRUN3 = 0x0B,  // Устанавливается значение 128 если счётчик программы PRG<1:0>=X<1:0>, иначе 0
    kRUN4 = 0x0C,  // Устанавливается значение 0 если счётчик программы PRG<1:0>=X<1:0>, иначе 128
    kRUN5 = 0x0D,  // Устанавливается значение 128 если счётчик программы PRG<2:0>=X<2:0>, иначе 0
    kRUN6 = 0x0E,  // Устанавливается значение 0 если счётчик программы PRG<2:0>=X<2:0>, иначе 128
    kRUN7 = 0x0F,  // Устанавливается значение 128 если счётчик программы PRG<3:0>=X<3:0>, иначе 0
    kRUN8 = 0x10,  // Устанавливается значение 0 если счётчик программы PRG<3:0>=X<3:0>, иначе 128

};

// Message:
// 0: 0x63 - write a program to RAM (0x36 read the program)
// 1: <channel index>
// 2: initial value for REG
// 3: action, for possible values:
//          0x00 - do not change the register value
//          0x01 - increase with step 1 from the initial value to 120
//          0x02 - decrease with step 1 from the initial value to 0
//    for more value see documentation for MP710
// 4: hi-byte of 16-bit CMD-loop number ~ change time
// 5: low-byte of 16-bit CMD-loop number
// 6: hi-byte of 16-bit PRG-loop number
// 7: low-byte of 16-bit PRG-loop number

/**
 * @brief The ControlMessage class
 *
 * Message:
 * 0: 0x63 - write a program to RAM (0x36 read the program)
 * 1: <channel index>
 * 2: initial value for REG
 * 3: action, for possible values:
 *          0x00 - do not change the register value
 *          0x01 - increase with step 1 from the initial value to 120
 *          0x02 - decrease with step 1 from the initial value to 0
 *    for more value see documentation for MP710
 * 4: hi-byte of 16-bit CMD-loop param ~ change time
 * 5: low-byte of 16-bit CMD-loop param
 * 6: hi-byte of 16-bit PRG-loop param
 * 7: low-byte of 16-bit PRG-loop param
 *
 */
class ControlMessage
{
public:
    ControlMessage(std::uint8_t channelIdx,
        std::uint8_t regValue,
        ChangeType action,
        std::uint16_t cmd,
        std::uint16_t prg)
        : _commandData{
              DeviceCommand::kWriteToRegister,
              channelIdx,
              regValue,
              action,
              static_cast<std::uint8_t>(cmd >> 8),
              static_cast<std::uint8_t>(cmd & 0xFF),  // CMD
              static_cast<std::uint8_t>(prg >> 8),
              static_cast<std::uint8_t>(prg & 0xFF)  // PRG
          }
    {}

    ControlMessage(std::uint8_t channelIdx, std::uint8_t brightness)
        : ControlMessage(channelIdx, brightness, ChangeType::kINC_M_DEC, 30U, 1U)
    {}

    std::uint8_t* GetData()
    {
        return _commandData;
    }

    size_t GetDataSize() const
    {
        return sizeof(_commandData);
    }

    ControlMessage() = delete;
    ControlMessage(const ControlMessage&) = delete;
    ControlMessage& operator=(const ControlMessage&) = delete;

private:
    std::uint8_t _commandData[8];
};

}  // namespace

const std::uint8_t DeviceController::kChannelNumber = 16U;
const std::uint8_t DeviceController::kBrightnessMax = 128U;

DeviceController::DeviceController(size_t maxCommandQueueSize, OnChangeCallback doneCallback)
    : _maxQueueSize(maxCommandQueueSize)
    , _shouldStop(false)
    , _lastChannelValues(kChannelNumber, 0U)
    , _onChangeCallback(doneCallback)
{
    std::thread workerThread(&DeviceController::WorkerThreadFunc, this);
    _workerThread.swap(workerThread);
}

DeviceController::~DeviceController()
{
    _shouldStop = true;
    _workerThread.join();
}

void DeviceController::RunTransition(DeviceController::TransitionControllerCallback controllerCallback,
    std::chrono::milliseconds duration)
{
    if (controllerCallback == nullptr || duration.count() <= 0) {
        Tracer::Log("Invalid argument.\n");
        return;
    }

    {
        std::unique_lock<std::mutex> queueLock(_queueMutex);

        _commandsQueue.clear();

        _transitionControllerCallback = controllerCallback;
        _transitionDuration = duration;
        _transitionStartState = _lastChannelValues;
        _transitionStartTime = std::chrono::steady_clock::now();
    }
}

void DeviceController::AddCommand(DeviceController::CommandType type,
    uint8_t param,
    const std::vector<int8_t>& channels)
{

    for (const auto channel : channels) {
        AddCommand(type, param, channel);
    }
}

void DeviceController::AddCommand(CommandType type, std::uint8_t param, std::int8_t channelIdx)
{

    AddCommand(Command{type, channelIdx, param});
}

void DeviceController::AddCommand(const Command& command)
{

    std::unique_lock<std::mutex> queueLock(_queueMutex);

    _transitionControllerCallback = nullptr;

    if (_maxQueueSize < _commandsQueue.size()) {
        Tracer::Log("Dropped command because the command queue is full.\n");
        return;
    }

    _commandsQueue.push_back(command);
}

bool DeviceController::WaitForCommands(std::chrono::milliseconds timeout)
{

    while (!_shouldStop) {
        {
            std::unique_lock<std::mutex> queueLock(_queueMutex);
            if (_commandsQueue.empty()) {
                return true;
            }
        }

        {
            std::unique_lock<std::mutex> lock(_queueMutex);
            if (_isQueueEmptyCondition.wait_for(lock, timeout) == std::cv_status::timeout) {
                return _commandsQueue.empty();
            }
        }

        // It is a spurious wakeup
    }

    return false;
}

void DeviceController::Reset()
{}

std::vector<DeviceController::Channel> DeviceController::GetChannelValues() const
{
    std::vector<DeviceController::Channel> channelValues(DeviceController::kChannelNumber);
    for (unsigned i = 0; i < _lastChannelValues.size(); ++i) {
        channelValues[i].Idx = i;
        channelValues[i].Param1 = _lastChannelValues[i];
    }

    return channelValues;
}

bool DeviceController::ExecCommand(const Command& command)
{

    libusb_device_handle* handle = libusb_open_device_with_vid_pid(nullptr, kDeviceVendorId, kDeviceProductId);
    if (nullptr == handle) {
        Tracer::Log("Failed to open device\n");
        return false;
    }

    if (libusb_kernel_driver_active(handle, kDeviceInterface)) {
        libusb_detach_kernel_driver(handle, kDeviceInterface);
    }

    int ret;
    if ((ret = libusb_set_configuration(handle, kDeviceConfig)) < 0) {
        Tracer::Log("Failed to configure device, error: %i.\n", ret);
        if (ret == LIBUSB_ERROR_BUSY) {
            Tracer::Log("Device is busy\n");
        }

        libusb_close(handle);
        return false;
    }

    if (libusb_claim_interface(handle, kDeviceInterface) < 0) {
        Tracer::Log("Failed to claim interface.\n");

        libusb_close(handle);
        return false;
    }

    ControlMessage msg(command.ChannelIdx, command.Param1);
    ret = libusb_control_transfer(handle,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
        0x9,
        0x300,
        0,
        msg.GetData(),
        msg.GetDataSize(),
        100);

    Tracer::Log("Set brightness to %d.\n", command.Param1);

    libusb_attach_kernel_driver(handle, kDeviceInterface);

    libusb_close(handle);

    _lastChannelValues[command.ChannelIdx] = command.Param1;

    return true;
}

void DeviceController::WorkerThreadFunc()
{

    libusb_init(nullptr);
    libusb_set_debug(nullptr, libusb_log_level::LIBUSB_LOG_LEVEL_WARNING);

    while (!_shouldStop) {

        Command cmd;

        {
            std::unique_lock<std::mutex> queueLock(_queueMutex);

            if (_commandsQueue.size() != 0) {
                cmd = _commandsQueue.front();
                _commandsQueue.pop_front();

                auto isSameChannelIdx = [&cmd](const Command& item) { return item.ChannelIdx == cmd.ChannelIdx; };

                // Find the last command with same channelIdx
                auto rIt = std::find_if(_commandsQueue.rbegin(), _commandsQueue.rend(), isSameChannelIdx);
                if (rIt != _commandsQueue.rend()) {
                    cmd = *rIt;

                    _commandsQueue.remove_if(isSameChannelIdx);
                }
            }
        }

        if (cmd.Type != kNotSet) {
            ExecCommand(cmd);

            if (_onChangeCallback != nullptr) {
                _onChangeCallback(true, cmd);
            }
        } else {
            if (_transitionControllerCallback != nullptr) {

                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - _transitionStartTime);

                std::vector<Channel> newState
                    = _transitionControllerCallback(_transitionStartState, _transitionDuration, elapsed);
                for (size_t i = 0; i < newState.size(); ++i) {
                    if (newState[i].Param1 != _lastChannelValues[newState[i].Idx]) {

                        cmd.Type = kSetBrightness;
                        cmd.ChannelIdx = newState[i].Idx;
                        cmd.Param1 = newState[i].Param1;

                        ExecCommand(cmd);

                        if (_onChangeCallback != nullptr) {
                            _onChangeCallback(true, cmd);
                        }
                    }
                }

                if (elapsed >= _transitionDuration) {
                    _transitionControllerCallback = nullptr;
                }
            } else {
                _isQueueEmptyCondition.notify_one();
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    libusb_exit(nullptr);
}
