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
#include <atomic>
#include <vector>
#include <cstdio>
#include <functional>

#include "thirdparty/mongoose/mongoose.h"

#include "tracer/Tracer.h"
#include "mp710Lib/DeviceController.h"

namespace {
void SignalsHandler(int signal);
bool IsSignalRaised(void);
void Broadcast(mg_connection* nc, const char* msg, size_t size);
void EventHandler(mg_connection* nc, int event, void* eventData);
void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Command>& commands);
void OnDeviceUpdate(struct mg_connection* netConnection, bool result, DeviceController::CommandTypesEnum type, unsigned channelIdx, unsigned param);

mg_serve_http_opts serveHttpOpts = {.document_root = "."};
}

int main(int argc, char **argv) {

    signal(SIGTERM, SignalsHandler);
    signal(SIGINT, SignalsHandler);

    constexpr char kDefaultAddress[] = "8000"; // If only port is set then the server will listen on all IPv4 interfaces
    const char* address = kDefaultAddress;

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--workDir") == 0) {
            ++i;
            if (i < argc) {
                serveHttpOpts.document_root = argv[i];
            }
            else {
                Tracer::Log("Invalid argument '--workDir' was ignored.\n");
            }
        }
        else if (strcmp(argv[i], "--port") == 0) {
            ++i;
            if (i < argc) {
                address = argv[i];
            }
            else {
                Tracer::Log("Invalid argument '--workDir' was ignored.\n");
            }
        }
        else if (i != 0) {
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

    static const size_t MAX_QUEUE_SIZE = 100;
    DeviceController::DoneCallback doneCallback = std::bind(&OnDeviceUpdate, netConnection, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    DeviceController deviceController(MAX_QUEUE_SIZE, doneCallback);

    // Turn off all channels
    for (unsigned char channelIdx = 0; channelIdx < DeviceController::CHANNELS_NUMBER; ++channelIdx) {
        deviceController.AddCommand(DeviceController::CommandTypesEnum::SET_BRIGHTNESS, channelIdx, 0);
    }
    deviceController.WaitForCommands(std::chrono::milliseconds(3000U));

    // Turn on all channels
    for (unsigned char channelIdx = 0; channelIdx < DeviceController::CHANNELS_NUMBER; ++channelIdx) {
        deviceController.AddCommand(DeviceController::CommandTypesEnum::SET_BRIGHTNESS, channelIdx, 10);
    }
    deviceController.WaitForCommands(std::chrono::milliseconds(3000U));

    // Turn off all channels
    for (unsigned char channelIdx = 0; channelIdx < DeviceController::CHANNELS_NUMBER; ++channelIdx) {
        deviceController.AddCommand(DeviceController::CommandTypesEnum::SET_BRIGHTNESS, channelIdx, 0);
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

namespace {

std::atomic<bool> NeedToStopPolling(false);

void SignalsHandler(int signal) {
    Tracer::Log("Interrupted by signal %i.\n", signal);
    NeedToStopPolling = true;
}

bool IsSignalRaised(void) {
    return NeedToStopPolling;
}

void Broadcast(mg_connection* nc, const char* msg, size_t size) {
    for (mg_connection *c = mg_next(nc->mgr, nullptr); c != nullptr; c = mg_next(nc->mgr, c)) {
        mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg, size);
    }
}

void EventHandler(mg_connection* nc, int event, void* eventData) {

    DeviceController* deviceController = reinterpret_cast<DeviceController*>(nc->user_data);

    switch (event) {
    case MG_EV_HTTP_REQUEST: {
        /* Usual HTTP request - serve static files */
        struct http_message* hm = reinterpret_cast<http_message*>(eventData);
        //                 if (strncmp(hm->method, "POST", 4) == 0)
        //                 {
        //                   char n1[100], n2[100];
        //   double result;
        //
//   /* Get form variables */
        //   mg_get_http_var(&hm->body, "n1", n1, sizeof(n1));
        //   mg_get_http_var(&hm->body, "n2", n2, sizeof(n2));
        //
//   /* Send headers */
        //   mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
        //
//   /* Compute the result and send it back as a JSON object */
        //   result = strtod(n1, NULL) + strtod(n2, NULL);
        //   mg_printf_http_chunk(nc, "{ \"result\": %lf }", result);
        //   mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
        //
//                   
//                   
//                   
//                 }
        //                 else
        {
            mg_serve_http(nc, hm, serveHttpOpts);
        }

        nc->flags |= MG_F_SEND_AND_CLOSE;
        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
        /* New websocket connection. Send current state. */
        SendUpdate(nc, deviceController->GetLastCommands());
        break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
        /* New websocket message. */
        struct websocket_message* wm = reinterpret_cast<websocket_message*>(eventData);

        unsigned commandType(DeviceController::NOT_SET);
        unsigned channelIdx(DeviceController::CHANNELS_NUMBER);
        unsigned brightness(0);

        int scanResult = sscanf(reinterpret_cast<char*>(wm->data), " { %d , %d , %d }", &commandType, &channelIdx, &brightness);
        if (scanResult == 3 && deviceController != nullptr) {
            deviceController->AddCommand(static_cast<DeviceController::CommandTypesEnum>(commandType), channelIdx, brightness);
        }

        break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
    case MG_EV_CLOSE:
    default:
        break;
    }
}

std::vector<char> SerializeToJson(const std::vector<DeviceController::Command>& commands) {
    static const char ENTRY_TEMPLATE[] = "%s\"%u\": { \"type\":%u, \"channelIdx\":%u, \"param\":%u}%s ";
    static const size_t PARAM_COUNT = 5U;   // id, type, channelIdx, param, separator(comma)
    static const size_t PARAM_SIZE = 5U;    // expect not more than 5 characters in a param value

    const size_t expectedJsonSize = (sizeof(ENTRY_TEMPLATE) + PARAM_COUNT*PARAM_SIZE) * commands.size();

    std::vector<char> result(expectedJsonSize, 0);

    char* bufferPos = result.data();
    size_t bufferSize = result.size();

    bool success = true;

    for (size_t i = 0; i < commands.size(); ++i) {
        const DeviceController::Command& command = commands[i];

        const char* leftSeparator = (0 == i) ?  "{ " : "";
        const char* rightSeparator = (i + 1 != commands.size()) ?  "," : "}";

        int printResult = snprintf(bufferPos, bufferSize, ENTRY_TEMPLATE,
                                   leftSeparator,
                                   static_cast<unsigned>(command.ChannelIdx),
                                   static_cast<unsigned>(command.Type),
                                   static_cast<unsigned>(command.ChannelIdx),
                                   static_cast<unsigned>(command.Param),
                                   rightSeparator);

        if (printResult > 0 && static_cast<unsigned>(printResult) != bufferSize) {
            bufferPos += printResult;
            bufferSize -= printResult;
        }
        else {
            success = false;
            break;
        }
    }

    if (!success) {
        result.resize(0);
        Tracer::Log("Unexpected size of update message.\n");
    }
    else {
        result.resize(bufferPos - result.data());
    }

    return result;
}

void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Command>& commands) {
    std::vector<char> buffer = SerializeToJson(commands);
    mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buffer.data(), buffer.size());
}

void OnDeviceUpdate(struct mg_connection* netConnection, bool result, DeviceController::CommandTypesEnum type, unsigned channelIdx, unsigned param) {
    Tracer::Log("Executed [%u] command %u at channel %u with param %u.\n",
                static_cast<unsigned>(result),
                static_cast<unsigned>(type),
                static_cast<unsigned>(channelIdx),
                static_cast<unsigned>(param));

    std::vector<char> buffer = SerializeToJson(std::vector<DeviceController::Command> {DeviceController::Command(type, channelIdx, param)});

    Broadcast(netConnection, buffer.data(), buffer.size());
}
}
