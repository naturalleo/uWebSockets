// tick_gateway.cpp
// 上游：作为客户端连接 tick_server（上游源）
// 下游：作为服务端监听，将接收到的行情转发给所有下游客户端
//
// 用法:
//   tick_gateway [-u UPSTREAM_HOST] [-U UPSTREAM_PORT] [-p LOCAL_PORT] [-v]

#include <App.h>
#include <iostream>
#include <string>
#include <set>
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <atomic>

struct Config {
    std::string upstream_host = "localhost";
    int         upstream_port = 9001;
    int         local_port    = 9002;
    bool        verbose       = false;
};
static Config g_cfg;

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "  -u  HOST   upstream tick_server host  (default: localhost)\n"
        << "  -U  PORT   upstream tick_server port  (default: 9001)\n"
        << "  -p  PORT   local listen port          (default: 9002)\n"
        << "  -v         verbose output             (default: off)\n";
}

// ── 下游连接池 ────────────────────────────────────────────────────
struct DownstreamData {
    std::string client_id;
    bool        subscribed = true;
};
using DownstreamWS = uWS::WebSocket<false, true, DownstreamData>;

std::mutex                    ds_mutex;
std::set<DownstreamWS*>       ds_connections;
static std::atomic<int>       ds_id_counter{0};
static std::atomic<long long> g_forwarded{0};

static void broadcast(std::string_view msg) {
    std::lock_guard<std::mutex> lock(ds_mutex);
    for (DownstreamWS* ws : ds_connections) {
        auto* data = ws->getUserData();
        if (!data->subscribed) continue;
        ws->send(msg, uWS::OpCode::TEXT);
        ++g_forwarded;
    }
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "-u") == 0 && i+1 < argc) g_cfg.upstream_host = argv[++i];
        else if (strcmp(argv[i], "-U") == 0 && i+1 < argc) g_cfg.upstream_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) g_cfg.local_port    = atoi(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0)               g_cfg.verbose       = true;
        else { print_usage(argv[0]); return 0; }
    }

    std::cout << "=== tick_gateway ==="
              << "  upstream=" << g_cfg.upstream_host << ":" << g_cfg.upstream_port
              << "  local_port=" << g_cfg.local_port << "\n";

    uWS::App app;

    // ── 下游服务端：接受来自 MT4/客户端的连接 ────────────────────
    app.ws<DownstreamData>("/*", {
        .open = [](DownstreamWS* ws) {
            auto* data       = ws->getUserData();
            int id           = ds_id_counter++;
            data->client_id  = "ds_" + std::to_string(id);
            data->subscribed = true;
            { std::lock_guard<std::mutex> lock(ds_mutex); ds_connections.insert(ws); }
            std::cout << data->client_id << " downstream connected\n";
        },
        .message = [](DownstreamWS* ws, std::string_view msg, uWS::OpCode opCode) {
            auto* data = ws->getUserData();
            std::string s(msg);
            if (s == "sub") {
                data->subscribed = true;
                ws->send("{\"type\":\"subscribed\"}", opCode);
            } else if (s == "unsub") {
                data->subscribed = false;
                ws->send("{\"type\":\"unsubscribed\"}", opCode);
            }
        },
        .close = [](DownstreamWS* ws, int code, std::string_view) {
            auto* data = ws->getUserData();
            { std::lock_guard<std::mutex> lock(ds_mutex); ds_connections.erase(ws); }
            std::cout << data->client_id << " downstream disconnected, code=" << code << "\n";
        }
    })
    .listen(g_cfg.local_port, [](auto* s) {
        if (s) std::cout << "Gateway listening on port " << g_cfg.local_port << "\n";
        else   std::cerr << "Failed to listen on port " << g_cfg.local_port << "\n";
    });

    // ── 上游客户端：连接 tick_server，接收行情后广播 ──────────────
    struct UpstreamData {};
    app.ws<UpstreamData>("/*", {
        .open = [](uWS::WebSocket<false, true, UpstreamData>* ws) {
            std::cout << "[upstream] connected to tick_server\n";
            ws->send("sub", uWS::OpCode::TEXT);
        },
        .message = [](uWS::WebSocket<false, true, UpstreamData>*,
                      std::string_view msg, uWS::OpCode) {
            if (g_cfg.verbose)
                std::cout << "[upstream] " << msg << "\n";
            broadcast(msg);
        },
        .close = [](uWS::WebSocket<false, true, UpstreamData>*, int code, std::string_view) {
            std::cerr << "[upstream] disconnected from tick_server, code=" << code << "\n";
        }
    });

    std::string upstream_url = "ws://" + g_cfg.upstream_host
                             + ":" + std::to_string(g_cfg.upstream_port);
    app.connect(upstream_url, [](auto* ws, auto*) {
        if (!ws) std::cerr << "[upstream] connection failed\n";
    });

    app.run();
    return 0;
}
