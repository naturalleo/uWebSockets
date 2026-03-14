// tick_client.cpp
// 连接 tick_server，接收行情推送，支持动态修改点差 / 订阅控制
//
// 用法:
//   tick_client [-h HOST] [-p PORT] [-s SPREAD] [-v]

#include <App.h>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>

struct Config {
    std::string host    = "localhost";
    int         port    = 9001;
    double      spread  = 0.00002;
    bool        verbose = false;
};
static Config g_cfg;

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "  -h  HOST    server hostname  (default: localhost)\n"
        << "  -p  PORT    server port      (default: 9001)\n"
        << "  -s  SPREAD  initial spread   (default: 0.00002)\n"
        << "  -v          verbose output   (default: off)\n";
}

static std::atomic<long long> g_received{0};

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "-h") == 0 && i+1 < argc) g_cfg.host   = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) g_cfg.port   = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) g_cfg.spread = atof(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0)               g_cfg.verbose = true;
        else { print_usage(argv[0]); return 0; }
    }

    std::cout << "=== tick_client === connecting to "
              << g_cfg.host << ":" << g_cfg.port << "\n";

    // 统计线程：每秒打印接收速率
    std::thread([]() {
        long long last = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long long cur   = g_received.load();
            long long delta = cur - last;
            last = cur;
            std::cout << "[stats] recv/s=" << delta
                      << "  total=" << cur << "\n";
        }
    }).detach();

    uWS::App app;

    // uWS::App::connect 是客户端模式
    struct PerSocket {};
    app.ws<PerSocket>("/*", {
        .open = [](uWS::WebSocket<false, true, PerSocket>* ws) {
            std::cout << "[client] connected\n";
            // 发送初始点差设置
            std::string cmd = "spread:" + std::to_string(g_cfg.spread);
            ws->send(cmd, uWS::OpCode::TEXT);
        },
        .message = [](uWS::WebSocket<false, true, PerSocket>* ws,
                      std::string_view msg, uWS::OpCode) {
            ++g_received;
            if (g_cfg.verbose)
                std::cout << "[recv] " << msg << "\n";
        },
        .close = [](uWS::WebSocket<false, true, PerSocket>*, int code, std::string_view) {
            std::cout << "[client] disconnected, code=" << code << "\n";
        }
    });

    // 建立出站连接
    std::string url = "ws://" + g_cfg.host + ":" + std::to_string(g_cfg.port);
    app.connect(url, [](auto* ws, auto* /*ctx*/) {
        if (!ws) {
            std::cerr << "[client] failed to connect\n";
        }
    });

    app.run();
    return 0;
}
