// tick_recorder.cpp
// 连接 tick_server（或 tick_gateway），将接收到的行情逐行写入 CSV 文件
//
// 用法:
//   tick_recorder [-h HOST] [-p PORT] [-o OUTPUT_FILE] [-v]
//
// 输出格式（CSV）:
//   timestamp_ns,raw_json

#include <App.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <atomic>
#include <thread>

struct Config {
    std::string host    = "localhost";
    int         port    = 9001;
    std::string outfile = "ticks.csv";
    bool        verbose = false;
};
static Config g_cfg;

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "  -h  HOST   server hostname         (default: localhost)\n"
        << "  -p  PORT   server port             (default: 9001)\n"
        << "  -o  FILE   output CSV file         (default: ticks.csv)\n"
        << "  -v         verbose output          (default: off)\n";
}

static std::atomic<long long> g_recorded{0};

static long long now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "-h") == 0 && i+1 < argc) g_cfg.host    = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) g_cfg.port    = atoi(argv[++i]);
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) g_cfg.outfile = argv[++i];
        else if (strcmp(argv[i], "-v") == 0)               g_cfg.verbose = true;
        else { print_usage(argv[0]); return 0; }
    }

    std::cout << "=== tick_recorder ==="
              << "  source=" << g_cfg.host << ":" << g_cfg.port
              << "  output=" << g_cfg.outfile << "\n";

    std::ofstream ofs(g_cfg.outfile, std::ios::app);
    if (!ofs.is_open()) {
        std::cerr << "ERROR: cannot open output file: " << g_cfg.outfile << "\n";
        return 1;
    }
    if (ofs.tellp() == 0) {
        ofs << "timestamp_ns,raw_json\n";
    }

    std::thread([]() {
        long long last = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long long cur   = g_recorded.load();
            long long delta = cur - last;
            last = cur;
            std::cout << "[stats] recorded/s=" << delta
                      << "  total=" << cur << "\n";
        }
    }).detach();

    uWS::App app;

    struct PerSocket {
        std::ofstream* ofs_ptr = nullptr;
    };

    app.ws<PerSocket>("/*", {
        .open = [&ofs](uWS::WebSocket<false, true, PerSocket>* ws) {
            ws->getUserData()->ofs_ptr = &ofs;
            std::cout << "[recorder] connected, recording to "
                      << g_cfg.outfile << "\n";
            ws->send("sub", uWS::OpCode::TEXT);
        },
        .message = [](uWS::WebSocket<false, true, PerSocket>* ws,
                      std::string_view msg, uWS::OpCode) {
            auto* data   = ws->getUserData();
            long long ts = now_ns();
            (*data->ofs_ptr) << ts << "," << msg << "\n";
            ++g_recorded;
            if (g_cfg.verbose)
                std::cout << "[rec] " << ts << " " << msg << "\n";
        },
        .close = [](uWS::WebSocket<false, true, PerSocket>*, int code, std::string_view) {
            std::cout << "[recorder] disconnected, code=" << code << "\n";
        }
    });

    std::string url = "ws://" + g_cfg.host + ":" + std::to_string(g_cfg.port);
    app.connect(url, [](auto* ws, auto*) {
        if (!ws) std::cerr << "[recorder] connection failed\n";
    });

    app.run();

    ofs.flush();
    ofs.close();
    return 0;
}

