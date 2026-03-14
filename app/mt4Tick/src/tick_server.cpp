#include <App.h>
#include <iostream>
#include <string>
#include <set>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>
#include <cstdlib>

// ── 命令行参数 ────────────────────────────────────────────────────
struct Config {
    int  num_symbols   = 5;
    int  ticks_per_sec = 10;
    int  port          = 9001;
    bool verbose       = false;
};
static Config g_cfg;

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "  -s  NUM_SYMBOLS    number of symbols          (default: 5)\n"
        << "  -t  TICKS_PER_SEC  ticks per symbol per sec   (default: 10)\n"
        << "  -p  PORT           WebSocket listen port      (default: 9001)\n"
        << "  -v                 verbose: print every push  (default: off)\n"
        << "  -h                 show this help\n";
}

// ── 每个连接的私有数据 ────────────────────────────────────────────
struct PerSocketData {
    std::string client_id;
    double      spread;
    bool        subscribed;
};

using WebSocket = uWS::WebSocket<false, true, PerSocketData>;

std::mutex           conn_mutex;
std::set<WebSocket*> connections;
static std::atomic<long long> g_pushed{0};

// ── 品种状态 ──────────────────────────────────────────────────────
struct SymbolState {
    std::string name;
    double      bid;
    double      ask;
    int         tick_count{0};
};
static std::vector<SymbolState> g_symbols;

static void init_symbols(int n) {
    g_symbols.resize(n);
    for (int i = 0; i < n; ++i) {
        std::ostringstream name;
        name << "SYM_" << std::setw(2) << std::setfill('0') << i;
        g_symbols[i].name = name.str();
        g_symbols[i].bid  = 1.00000 + i * 0.10000;
        g_symbols[i].ask  = g_symbols[i].bid + 0.00010;
    }
}

static std::string make_quote_json(const SymbolState& s, double spread) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(5);
    oss << "{\"symbol\":\"" << s.name << "\","
        << "\"bid\":"    << (s.bid - spread) << ","
        << "\"ask\":"    << (s.ask + spread) << ","
        << "\"spread\":" << spread << "}";
    return oss.str();
}

static void push_symbol_tick(uWS::Loop* loop, int sym_idx) {
    SymbolState& s = g_symbols[sym_idx];
    double move = (s.tick_count % 2 == 0 ? 1.0 : -1.0) * 0.00005;
    s.bid += move;
    s.ask += move;
    ++s.tick_count;

    loop->defer([sym_idx]() {
        const SymbolState& sym = g_symbols[sym_idx];
        std::lock_guard<std::mutex> lock(conn_mutex);
        for (WebSocket* ws : connections) {
            auto* data = ws->getUserData();
            if (!data->subscribed) continue;
            std::string msg = make_quote_json(sym, data->spread);
            ws->send(msg, uWS::OpCode::TEXT);
            ++g_pushed;
            if (g_cfg.verbose)
                std::cout << "[push] " << data->client_id << " <- " << msg << "\n";
        }
    });
}

void quote_source_thread(uWS::Loop* loop) {
    using Clock    = std::chrono::steady_clock;
    using Duration = std::chrono::nanoseconds;

    const int N   = g_cfg.num_symbols;
    const int TPS = g_cfg.ticks_per_sec;
    const Duration sym_period = std::chrono::nanoseconds(1'000'000'000LL / TPS);
    const Duration sym_offset = sym_period / N;

    std::vector<Clock::time_point> next_tick(N);
    auto now = Clock::now();
    for (int i = 0; i < N; ++i)
        next_tick[i] = now + sym_offset * i;

    auto stat_time  = now + std::chrono::seconds(1);
    long long last_pushed = 0;

    while (true) {
        int  next_idx  = 0;
        auto next_time = next_tick[0];
        for (int i = 1; i < N; ++i) {
            if (next_tick[i] < next_time) {
                next_time = next_tick[i];
                next_idx  = i;
            }
        }
        now = Clock::now();
        if (next_time > now)
            std::this_thread::sleep_for(next_time - now);

        push_symbol_tick(loop, next_idx);
        next_tick[next_idx] += sym_period;

        now = Clock::now();
        if (now >= stat_time) {
            long long cur   = g_pushed.load();
            long long delta = cur - last_pushed;
            last_pushed = cur;
            std::cout << "[stats] pushed/s=" << delta
                      << "  symbols=" << N
                      << "  ticks/sym/s=" << TPS
                      << "  total_tps=" << (N * TPS) << "\n";
            stat_time += std::chrono::seconds(1);
        }
    }
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "-s") == 0 && i+1 < argc) g_cfg.num_symbols   = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) g_cfg.ticks_per_sec = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) g_cfg.port          = atoi(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0)               g_cfg.verbose       = true;
        else if (strcmp(argv[i], "-h") == 0) { print_usage(argv[0]); return 0; }
    }
    if (g_cfg.num_symbols   <= 0) { std::cerr << "ERROR: num_symbols must be > 0\n";   return 1; }
    if (g_cfg.ticks_per_sec <= 0) { std::cerr << "ERROR: ticks_per_sec must be > 0\n"; return 1; }

    std::cout << "=== tick_server ==="
              << "  symbols="      << g_cfg.num_symbols
              << "  ticks/sym/s=" << g_cfg.ticks_per_sec
              << "  port="        << g_cfg.port << " ===\n";

    init_symbols(g_cfg.num_symbols);
    auto* loop = uWS::Loop::get();
    uWS::App app;

    app.ws<PerSocketData>("/*", {
        .open = [](WebSocket* ws) {
            auto* data = ws->getUserData();
            static std::atomic<int> counter{0};
            int id           = counter++;
            data->client_id  = "client_" + std::to_string(id);
            data->spread     = 0.00001 * (id + 1);
            data->subscribed = true;
            { std::lock_guard<std::mutex> lock(conn_mutex); connections.insert(ws); }
            std::cout << data->client_id << " connected, spread=" << data->spread << "\n";
        },
        .message = [](WebSocket* ws, std::string_view msg, uWS::OpCode opCode) {
            auto* data = ws->getUserData();
            std::string s(msg);
            if (s.rfind("spread:", 0) == 0) {
                try {
                    data->spread = std::stod(s.substr(7));
                    ws->send("{\"type\":\"spread_updated\",\"spread\":" + std::to_string(data->spread) + "}", opCode);
                } catch (...) {}
            } else if (s == "sub") {
                data->subscribed = true;
                ws->send("{\"type\":\"subscribed\"}", opCode);
            } else if (s == "unsub") {
                data->subscribed = false;
                ws->send("{\"type\":\"unsubscribed\"}", opCode);
            }
        },
        .close = [](WebSocket* ws, int code, std::string_view) {
            auto* data = ws->getUserData();
            { std::lock_guard<std::mutex> lock(conn_mutex); connections.erase(ws); }
            std::cout << data->client_id << " disconnected, code=" << code << "\n";
        }
    })
    .listen(g_cfg.port, [](auto* s) {
        if (s) std::cout << "Listening on port " << g_cfg.port << "\n";
        else   std::cerr << "Failed to listen\n";
    });

    std::thread(quote_source_thread, loop).detach();
    app.run();
    return 0;
}

