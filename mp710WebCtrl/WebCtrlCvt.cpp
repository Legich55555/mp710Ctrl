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

#include "thirdparty/civetweb/include/civetweb.h"

#include "tracer/Tracer.h"
#include "mp710Lib/DeviceController.h"

namespace
{
void SignalsHandler(int signal);
bool IsSignalRaised(void);
void OnDeviceUpdate(struct mg_connection* netConnection,
    bool result,
    DeviceController::CommandTypesEnum type,
    unsigned channelIdx,
    unsigned param);

void Broadcast(mg_connection* nc, const char* msg, size_t size)
{
    for (mg_connection* c = mg_next(nc->mgr, nullptr); c != nullptr; c = mg_next(nc->mgr, c)) {
        mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg, size);
    }
}

int WebSocketStartHandler(struct mg_connection* conn, void* cbdata)
{
    mg_printf(conn,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
        "close\r\n\r\n");

    mg_printf(conn, "<!DOCTYPE html>\n");
    mg_printf(conn, "<html>\n<head>\n");
    mg_printf(conn, "<meta charset=\"UTF-8\">\n");
    mg_printf(conn, "<title>Embedded websocket example</title>\n");

    mg_printf(conn, "<script>\n");
    mg_printf(conn,
        "function load() {\n"
        "  var wsproto = (location.protocol === 'https:') ? 'wss:' : 'ws:';\n"
        "  connection = new WebSocket(wsproto + '//' + window.location.host + "
        "'/websocket');\n"
        "  websock_text_field = "
        "document.getElementById('websock_text_field');\n"
        "  connection.onmessage = function (e) {\n"
        "    websock_text_field.innerHTML=e.data;\n"
        "  }\n"
        "  connection.onerror = function (error) {\n"
        "    alert('WebSocket error');\n"
        "    connection.close();\n"
        "  }\n"
        "}\n");
    mg_printf(conn, "</script>\n");
    mg_printf(conn, "</head>\n<body onload=\"load()\">\n");
    mg_printf(conn, "<div id='websock_text_field'>No websocket connection yet</div>\n");
    mg_printf(conn, "</body>\n</html>\n");

    return 1;
}

#define MAX_WS_CLIENTS (5)

struct t_ws_client
{
    struct mg_connection* conn;
    int state;
} ws_clients[MAX_WS_CLIENTS];

#define ASSERT(x)                                                                 \
    {                                                                             \
        if (!(x)) {                                                               \
            fprintf(stderr, "Assertion failed in line %u\n", (unsigned)__LINE__); \
        }                                                                         \
    }

int WebSocketConnectHandler(const struct mg_connection* conn, void* cbdata)
{
    struct mg_context* ctx = mg_get_context(conn);
    int reject = 1;
    int i;

    mg_lock_context(ctx);
    for (i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i].conn == NULL) {
            ws_clients[i].conn = (struct mg_connection*)conn;
            ws_clients[i].state = 1;
            mg_set_user_connection_data(ws_clients[i].conn, (void*)(ws_clients + i));
            reject = 0;
            break;
        }
    }
    mg_unlock_context(ctx);

    Tracer::Log("Websocket client %s\r\n\r\n", (reject ? "rejected" : "accepted"));
    return reject;
}

void WebSocketReadyHandler(struct mg_connection* conn, void* cbdata)
{
    const char* text = "Hello from the websocket ready handler";
    struct t_ws_client* client = mg_get_user_connection_data(conn);

    mg_websocket_write(conn, WEBSOCKET_OPCODE_TEXT, text, strlen(text));
    Tracer::Log("Greeting message sent to websocket client\r\n\r\n");
    ASSERT(client->conn == conn);
    ASSERT(client->state == 1);

    client->state = 2;
}

int WebsocketDataHandler(struct mg_connection* conn, int bits, char* data, size_t len, void* cbdata)
{
    struct t_ws_client* client = mg_get_user_connection_data(conn);
    ASSERT(client->conn == conn);
    ASSERT(client->state >= 1);

    Tracer::Log("Websocket got data:\r\n");
    // fwrite(data, len, 1, stdout);

    return 1;
}

void WebSocketCloseHandler(const struct mg_connection* conn, void* cbdata)
{
    struct mg_context* ctx = mg_get_context(conn);
    struct t_ws_client* client = mg_get_user_connection_data(conn);
    ASSERT(client->conn == conn);
    ASSERT(client->state >= 1);

    mg_lock_context(ctx);
    client->state = 0;
    client->conn = NULL;
    mg_unlock_context(ctx);

    Tracer::Log("Client droped from the set of webserver connections\r\n\r\n");
}

void InformWebsockets(struct mg_context* ctx)
{
    static unsigned long cnt = 0;
    char text[32];
    int i;

    sprintf(text, "%lu", ++cnt);

    mg_lock_context(ctx);
    for (i = 0; i < MAX_WS_CLIENTS; i++) {
        if (ws_clients[i].state == 2) {
            mg_websocket_write(ws_clients[i].conn, WEBSOCKET_OPCODE_TEXT, text, strlen(text));
        }
    }
    mg_unlock_context(ctx);
}

//     void Broadcast(mg_connection* nc, const char* msg, size_t size);
//     void EventHandler(mg_connection* nc, int event, void* eventData);
//     void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Command>& commands);
//
//     mg_serve_http_opts serveHttpOpts = {.document_root = "."};

int LogMessage(const struct mg_connection* conn, const char* message);

}  // namespace

int main(int argc, char** argv)
{

    signal(SIGTERM, SignalsHandler);
    signal(SIGINT, SignalsHandler);

    int err = 0;
    if (!mg_check_feature(16)) {
        Tracer::Log("Error: Embedded example built with websocket support, but civetweb library build without.\n");
        err = 1;
    }

    if (err) {
        Tracer::Log("Cannot start CivetWeb - inconsistent build.\n");
        return EXIT_FAILURE;
    }

    const char* options[] = {"document_root",
        ".",
        "listening_ports",
        "8000",
        "request_timeout_ms",
        "10000",
        "error_log_file",
        "error.log",
        "websocket_timeout_ms",
        "3600000",
        nullptr};

    /* Start CivetWeb web server */
    struct mg_callbacks callbacks = {0};
    callbacks.log_message = LogMessage;

    struct mg_context* ctx = mg_start(&callbacks, 0, options);
    if (ctx == NULL) {
        Tracer::Log("Cannot start CivetWeb - mg_start failed.\n");
        return EXIT_FAILURE;
    }

    //     /* Add HTTP site to open a websocket connection */
    //     mg_set_request_handler(ctx, "/websocket", WebSocketStartHandler, 0);
    //
    //     /* WS site for the websocket connection */
    //     mg_set_websocket_handler(ctx,
    //                              "/websocket",
    //                              WebSocketConnectHandler,
    //                              WebSocketReadyHandler,
    //                              WebsocketDataHandler,
    //                              WebSocketCloseHandler,
    //                              0);

    const size_t MAX_PORTS = 32U;

    /* List all listening ports */
    struct mg_server_ports ports[MAX_PORTS] = {0};
    int port_cnt = mg_get_server_ports(ctx, MAX_PORTS, ports);
    if (port_cnt < 0) {
        Tracer::Log("Failed with result %i.\n", port_cnt);
        return EXIT_FAILURE;
    }

    Tracer::Log("There are %i listening ports:\n\n", port_cnt);

    for (int n = 0; n < port_cnt && n < MAX_PORTS; ++n) {
        const struct mg_server_ports& port = ports[n];

        Tracer::Log("\tPort: %i, ssl: %i, protocol: %i\n", port.port, port.is_ssl, port.protocol);
    }

    static const size_t MAX_QUEUE_SIZE = 100;
    DeviceController::DoneCallback doneCallback = std::bind(
        &OnDeviceUpdate, netConnection, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    DeviceController deviceController(MAX_QUEUE_SIZE, doneCallback);

    /* Wait until the server should be closed */
    while (!IsSignalRaised()) {
        InformWebsockets(ctx);
    }

    /* Stop the server */
    mg_stop(ctx);
    Tracer::Log("Server stopped.\n");

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

int LogMessage(const struct mg_connection* conn, const char* message)
{
    Tracer::Log(message);
    return 1;
}

//     void Broadcast(mg_connection* nc, const char* msg, size_t size) {
//         for (mg_connection *c = mg_next(nc->mgr, nullptr); c != nullptr; c = mg_next(nc->mgr, c)) {
//             mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg, size);
//         }
//     }
//
//     void EventHandler(mg_connection* nc, int event, void* eventData) {
//
//         DeviceController* deviceController = reinterpret_cast<DeviceController*>(nc->user_data);
//
//         switch (event) {
//             case MG_EV_HTTP_REQUEST: {
//                 /* Usual HTTP request - serve static files */
//                 struct http_message* hm = reinterpret_cast<http_message*>(eventData);
//                 mg_serve_http(nc, hm, serveHttpOpts);
//                 nc->flags |= MG_F_SEND_AND_CLOSE;
//                 break;
//             }
//             case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
//                 /* New websocket connection. Send current state. */
//                 SendUpdate(nc, deviceController->GetLastCommands());
//                 break;
//             }
//             case MG_EV_WEBSOCKET_FRAME: {
//                 /* New websocket message. Tell everybody. */
//                 struct websocket_message* wm = reinterpret_cast<websocket_message*>(eventData);
//
//                 unsigned commandType(DeviceController::NOT_SET);
//                 unsigned channelIdx(DeviceController::CHANNELS_NUMBER);
//                 unsigned brightness(0);
//
//                 int scanResult = sscanf(reinterpret_cast<char*>(wm->data), " { %d , %d , %d }", &commandType,
//                 &channelIdx, &brightness); if (scanResult == 3 && deviceController != nullptr) {
//                     deviceController->AddCommand(static_cast<DeviceController::CommandTypesEnum>(commandType),
//                     channelIdx, brightness);
//                 }
//
//                 break;
//             }
//             case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
//             case MG_EV_CLOSE:
//             default:
//                 break;
//         }
//     }
//
//     std::vector<char> SerializeToJson(const std::vector<DeviceController::Command>& commands) {
//         static const char ENTRY_TEMPLATE[] = "%s\"%u\": { \"type\":%u, \"channelIdx\":%u, \"param\":%u}%s ";
//         static const size_t PARAM_COUNT = 5U;   // id, type, channelIdx, param, separator(comma)
//         static const size_t PARAM_SIZE = 5U;    // expect not more than 5 characters in a param value
//
//         const size_t expectedJsonSize = (sizeof(ENTRY_TEMPLATE) + PARAM_COUNT*PARAM_SIZE) * commands.size();
//
//         std::vector<char> result(expectedJsonSize, 0);
//
//         char* bufferPos = result.data();
//         size_t bufferSize = result.size();
//
//         bool success = true;
//
//         for (size_t i = 0; i < commands.size(); ++i) {
//             const DeviceController::Command& command = commands[i];
//
//             const char* leftSeparator = (0 == i) ?  "{ " : "";
//             const char* rightSeparator = (i + 1 != commands.size()) ?  "," : "}";
//
//             int printResult = snprintf(bufferPos, bufferSize, ENTRY_TEMPLATE,
//                                        leftSeparator,
//                                        static_cast<unsigned>(command.ChannelIdx),
//                                        static_cast<unsigned>(command.Type),
//                                        static_cast<unsigned>(command.ChannelIdx),
//                                        static_cast<unsigned>(command.Param),
//                                        rightSeparator);
//
//             if (printResult > 0 && static_cast<unsigned>(printResult) != bufferSize) {
//                 bufferPos += printResult;
//                 bufferSize -= printResult;
//             }
//             else {
//                 success = false;
//                 break;
//             }
//         }
//
//         if (!success) {
//             result.resize(0);
//             Tracer::Log("Unexpected size of update message.\n");
//         }
//         else {
//             result.resize(bufferPos - result.data());
//         }
//
//         return result;
//     }
//
//     void SendUpdate(struct mg_connection* nc, const std::vector<DeviceController::Command>& commands) {
//         std::vector<char> buffer = SerializeToJson(commands);
//         mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buffer.data(), buffer.size());
//     }
//
void OnDeviceUpdate(struct mg_connection* netConnection,
    bool result,
    DeviceController::CommandTypesEnum type,
    unsigned channelIdx,
    unsigned param)
{
    Tracer::Log("Executed [%u] command %u at channel %u with param %u.\n",
        static_cast<unsigned>(result),
        static_cast<unsigned>(type),
        static_cast<unsigned>(channelIdx),
        static_cast<unsigned>(param));

    //         std::vector<char> buffer = SerializeToJson(std::vector<DeviceController::Command>
    //         {DeviceController::Command(type, channelIdx, param)});
    //
    //         Broadcast(netConnection, buffer.data(), buffer.size());
}
}  // namespace
