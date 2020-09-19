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

//#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <signal.h>
#include <atomic>
#include <vector>
//#include <utility>
//#include <cstdio>
#include <string>
#include <functional>

#include "thirdparty/mongoose/mongoose.h"

#include "tracer/Tracer.h"
#include "mp710Lib/DeviceController.h"

namespace
{
void SignalsHandler(int signal);
bool IsSignalRaised(void);
void Broadcast(mg_connection* nc, const char* msg, size_t size);
void EventHandler(mg_connection* nc, int event, void* eventData);
void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Channel>& channels);
void OnDeviceUpdate(struct mg_connection* netConnection, bool result, const DeviceController::Command& command);

mg_serve_http_opts serveHttpOpts = {.document_root = "."};
}  // namespace

int main(int argc, char** argv)
{

    signal(SIGTERM, SignalsHandler);
    signal(SIGINT, SignalsHandler);

    constexpr char kDefaultAddress[] = "8000";  // If only port is set then the server will listen on all IPv4 interfaces
    const char* address = kDefaultAddress;

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--workDir") == 0) {
            ++i;
            if (i < argc) {
                serveHttpOpts.document_root = argv[i];
            } else {
                Tracer::Log("Invalid argument '--workDir' was ignored.\n");
            }
        } else if (strcmp(argv[i], "--port") == 0) {
            ++i;
            if (i < argc) {
                address = argv[i];
            } else {
                Tracer::Log("Invalid argument '--workDir' was ignored.\n");
            }
        } else if (i != 0) {
            Tracer::Log("Invalid argument %s was ignored.\n", argv[i]);
        }
    }

    mg_mgr mgr;
    mg_mgr_init(&mgr, nullptr);

    mg_connection* netConnection = mg_bind(&mgr, address, EventHandler);
    if (nullptr == netConnection) {
        Tracer::Log("Failed to initialize listenning on addres %s.\n", address);
        return EXIT_FAILURE;
    }

    constexpr size_t MAX_QUEUE_SIZE = 100;
    DeviceController::OnChangeCallback doneCallback
        = std::bind(&OnDeviceUpdate, netConnection, std::placeholders::_1, std::placeholders::_2);
    DeviceController deviceController(MAX_QUEUE_SIZE, doneCallback);

    // Turn off all channels
    for (unsigned char channelIdx = 0; channelIdx < DeviceController::kChannelNumber; ++channelIdx) {
        deviceController.AddCommand(DeviceController::CommandType::kSetBrightness, 0, channelIdx);
    }
    deviceController.WaitForCommands(std::chrono::milliseconds(3000U));

    netConnection->user_data = &deviceController;
    mg_set_protocol_http_websocket(netConnection);

    while (!IsSignalRaised()) {
        mg_mgr_poll(&mgr, 200);
    }

    Tracer::Log("Stopping...\n");

    mg_mgr_free(&mgr);

    return EXIT_SUCCESS;
}

namespace
{

std::atomic<bool> NeedToStopPolling(false);

void SignalsHandler(int signal)
{
    Tracer::Log("Interrupted by signal %i.\n", signal);
    NeedToStopPolling = true;
}

bool IsSignalRaised(void)
{
    return NeedToStopPolling;
}

void Broadcast(mg_connection* nc, const char* msg, size_t size)
{
    for (mg_connection* c = mg_next(nc->mgr, nullptr); c != nullptr; c = mg_next(nc->mgr, c)) {
        mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg, size);
    }
}

std::vector<std::uint8_t> ParseCommandMessage(const unsigned char* message, size_t messageSize)
{
    std::vector<char> data(messageSize + 1);
    std::memcpy(data.data(), message, messageSize);
    data[messageSize] = '\0';  // The internal presentation should be a 0-terminated string

    std::vector<std::uint8_t> result;
    result.reserve(18);

    const char* curSign = data.data();
    const char* endSign = curSign + messageSize;

    while (curSign < endSign) {

        char* stopSign;
        unsigned long curValue = std::strtoul(curSign, &stopSign, 10);
        if (0 == curValue) {
            if (curSign == stopSign) {
                Tracer::Log("Failed to parse command '%s' at position %i.\n", data.data(), curSign - data.data());

                result.resize(0);
                break;
            } else if (errno == ERANGE) {
                Tracer::Log("Failed to parse command '%s': to big value at position %i.\n", data.data(), curSign - data.data());
                errno = 0;

                result.resize(0);
                break;
            } else if (errno == EINVAL) {
                Tracer::Log("Failed to parse command '%s': invalid sign at position %i.\n", data.data(), stopSign - data.data());
                errno = 0;

                result.resize(0);
                break;
            }
        }

        if (curValue < UINT8_MAX) {
            result.push_back(static_cast<std::uint8_t>(curValue));
        }

        curSign = stopSign + 1;
    }

    return result;
}

std::uint8_t LinearTransform(std::uint8_t startValue, std::uint8_t endValue, const std::int64_t elapsedMs, const std::int64_t durationMs)
{
    return std::clamp(int(startValue + ((endValue - startValue) * elapsedMs / durationMs)), 0, int(DeviceController::kBrightnessMax));
}

std::vector<DeviceController::Channel> SunriseController(const std::vector<std::uint8_t>& startState,
    const std::chrono::milliseconds& duration,
    const std::chrono::milliseconds& elapsed)
{
    constexpr std::uint8_t kRedChannelIdx = 14U;
    constexpr std::uint8_t kGreenChannelIdx = 13U;
    constexpr std::uint8_t kBlueChannelIdx = 12U;

    const std::chrono::milliseconds phaseDuration = duration / 3;

    std::uint8_t redValue = startState[kRedChannelIdx];
    std::uint8_t greenValue = startState[kGreenChannelIdx];
    std::uint8_t blueValue = startState[kBlueChannelIdx];

    // If it is phase #1
    if (elapsed <= phaseDuration) {
        const std::chrono::milliseconds phaseElapsed = elapsed;

        redValue
            = LinearTransform(startState[kRedChannelIdx], DeviceController::kBrightnessMax, phaseElapsed.count(), phaseDuration.count());
    }
    // If it is phase #3
    else if (elapsed > 2 * phaseDuration) {
        const std::chrono::milliseconds phaseElapsed = elapsed - (2 * phaseDuration);

        blueValue
            = LinearTransform(startState[kBlueChannelIdx], DeviceController::kBrightnessMax, phaseElapsed.count(), phaseDuration.count());
        greenValue = DeviceController::kBrightnessMax;
        redValue = DeviceController::kBrightnessMax;
    }
    // If it is phase #2
    else {
        const std::chrono::milliseconds phaseElapsed = elapsed - phaseDuration;

        greenValue
            = LinearTransform(startState[kGreenChannelIdx], DeviceController::kBrightnessMax, phaseElapsed.count(), phaseDuration.count());
        redValue = DeviceController::kBrightnessMax;
    }

    return std::vector<DeviceController::Channel>{DeviceController::Channel{kRedChannelIdx, redValue},
        DeviceController::Channel{kGreenChannelIdx, greenValue},
        DeviceController::Channel{kBlueChannelIdx, blueValue}};
}

std::vector<DeviceController::Channel> SunsetController(const std::vector<std::uint8_t>& startState,
    const std::chrono::milliseconds& duration,
    const std::chrono::milliseconds& elapsed)
{
    constexpr std::uint8_t kRedChannelIdx = 14U;
    constexpr std::uint8_t kGreenChannelIdx = 13U;
    constexpr std::uint8_t kBlueChannelIdx = 12U;

    const std::chrono::milliseconds phaseDuration = duration / 3;

    std::uint8_t redValue = startState[kRedChannelIdx];
    std::uint8_t greenValue = startState[kGreenChannelIdx];
    std::uint8_t blueValue = startState[kBlueChannelIdx];

    // If it is phase #1
    if (elapsed <= phaseDuration) {
        const std::chrono::milliseconds phaseElapsed = elapsed;

        blueValue = LinearTransform(startState[kBlueChannelIdx], 0, phaseElapsed.count(), phaseDuration.count());
    }
    // If it is phase #3
    else if (elapsed > 2 * phaseDuration) {
        const std::chrono::milliseconds phaseElapsed = elapsed - (2 * phaseDuration);

        redValue = LinearTransform(startState[kRedChannelIdx], 0, phaseElapsed.count(), phaseDuration.count());
        Tracer::Log("Red value %u %u %u.\n", unsigned(redValue), unsigned(phaseElapsed.count()), unsigned(phaseDuration.count()));
        greenValue = 0;
        blueValue = 0;
    }
    // If it is phase #2
    else {
        const std::chrono::milliseconds phaseElapsed = elapsed - phaseDuration;

        greenValue = LinearTransform(startState[kGreenChannelIdx], 0, phaseElapsed.count(), phaseDuration.count());
        blueValue = 0;
    }

    return std::vector<DeviceController::Channel>{DeviceController::Channel{kRedChannelIdx, redValue},
        DeviceController::Channel{kGreenChannelIdx, greenValue},
        DeviceController::Channel{kBlueChannelIdx, blueValue}};
}

void EventHandler(mg_connection* nc, int event, void* eventData)
{

    DeviceController* deviceController = reinterpret_cast<DeviceController*>(nc->user_data);

    switch (event) {
    case MG_EV_HTTP_REQUEST: {
        /* Usual HTTP request - serve static files */
        struct http_message* hm = reinterpret_cast<http_message*>(eventData);
        mg_serve_http(nc, hm, serveHttpOpts);
        nc->flags |= MG_F_SEND_AND_CLOSE;
        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
        /* New websocket connection. Send current state. */
        SendUpdate(nc, deviceController->GetChannelValues());
        break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
        /* Websocket message. */
        struct websocket_message* wm = reinterpret_cast<websocket_message*>(eventData);

        std::vector<std::uint8_t> commandParams = ParseCommandMessage(wm->data, wm->size);
        if (commandParams.size() > 2 && deviceController != nullptr) {

            DeviceController::CommandType commandType = static_cast<DeviceController::CommandType>(commandParams[0]);
            std::uint8_t commandParam = commandParams[1];

            if (commandType == DeviceController::kSetBrightness) {
                deviceController->AddCommand(
                    commandType, commandParam, std::vector<std::int8_t>(commandParams.begin() + 2, commandParams.end()));
            } else if (commandType == DeviceController::kStartSunrise) {
                deviceController->RunTransition(&SunriseController, std::chrono::seconds(15));
            } else if (commandType == DeviceController::kStartSunset) {
                deviceController->RunTransition(&SunsetController, std::chrono::seconds(15));
            } else {
                Tracer::Log("The command is ignored because the type is unexpected.\n");
            }
        } else {
            Tracer::Log("The command is ignored because it has few parameters.\n");
        }

        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
    case MG_EV_CLOSE:
    default:
        break;
    }
}

std::vector<char> SerializeToJson(const std::vector<DeviceController::Channel>& channelValues)
{
    static const char ENTRY_TEMPLATE[] = "%s\"%u\": { \"type\":%u, \"channelIdx\":%u, \"param\":%u}%s ";
    static const size_t PARAM_COUNT = 5U;  // id, type, channelIdx, param, separator(comma)
    static const size_t PARAM_SIZE = 5U;  // expect not more than 5 characters in a param value

    const size_t expectedJsonSize = (sizeof(ENTRY_TEMPLATE) + PARAM_COUNT * PARAM_SIZE) * channelValues.size();

    std::vector<char> result(expectedJsonSize, 0);

    char* bufferPos = result.data();
    size_t bufferSize = result.size();

    bool success = true;

    for (size_t i = 0; i < channelValues.size(); ++i) {
        const DeviceController::Channel& channel = channelValues[i];

        const char* leftSeparator = (0 == i) ? "{ " : "";
        const char* rightSeparator = (i + 1 != channelValues.size()) ? "," : "}";

        int printResult = snprintf(bufferPos,
            bufferSize,
            ENTRY_TEMPLATE,
            leftSeparator,
            static_cast<unsigned>(channel.Idx),
            static_cast<unsigned>(0),
            static_cast<unsigned>(channel.Idx),
            static_cast<unsigned>(channel.Param1),
            rightSeparator);

        if (printResult > 0 && static_cast<unsigned>(printResult) != bufferSize) {
            bufferPos += printResult;
            bufferSize -= printResult;
        } else {
            success = false;
            break;
        }
    }

    if (!success) {
        result.resize(0);
        Tracer::Log("Unexpected size of update message.\n");
    } else {
        result.resize(bufferPos - result.data());
    }

    return result;
}

void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Channel>& channels)
{
    std::vector<char> buffer = SerializeToJson(channels);
    mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buffer.data(), buffer.size());
}

void OnDeviceUpdate(struct mg_connection* netConnection, bool result, const DeviceController::Command& command)
{
    Tracer::Log("Executed [%u] command %u at channel %u with param %u.\n",
        static_cast<unsigned>(result),
        static_cast<unsigned>(command.Type),
        static_cast<unsigned>(command.ChannelIdx),
        static_cast<unsigned>(command.Param1));

    std::vector<char> buffer
        = SerializeToJson(std::vector<DeviceController::Channel>{DeviceController::Channel{command.ChannelIdx, command.Param1}});

    Broadcast(netConnection, buffer.data(), buffer.size());
}
}  // namespace
