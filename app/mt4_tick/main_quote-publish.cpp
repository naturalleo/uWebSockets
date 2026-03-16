#include <iostream>

#include "Platform.h"
#include "MT4ManagerAPI.h"
#include "MT4Manager.h"
#include "Common.h"

// uWebSockets
#include <App.h>

#include <unordered_map>
#include <shared_mutex>

using QUOTECALLBACK = std::function<void(std::string, LastQuote&&)>;

QUOTECALLBACK   _quote_cb = nullptr;


struct WsUserData {
	QuoteClientConnectionData qcd;
	std::string               remote_ip;
};
using UwsApp = uWS::App;
using UwsSocket = uWS::WebSocket<false, true, WsUserData>;


std::unordered_map<UINT64, std::vector<UwsSocket*>> uid_session_map;
std::shared_mutex uid_session_map_mutex;
static uWS::Loop* g_uwsLoop = nullptr;

static void remove_ws_from_uid_map(UwsSocket* ws, UINT64 loginid) {
    if (loginid <= 1) return;
    std::unique_lock<std::shared_mutex> lock(uid_session_map_mutex);
    auto it = uid_session_map.find(loginid);
    if (it == uid_session_map.end()) return;
    auto& vec = it->second;
    vec.erase(std::remove(vec.begin(), vec.end(), ws), vec.end());
    if (vec.empty()) uid_session_map.erase(it);
}

int main(int argc, char* argv[]) {

	// MT4管理器
	std::unique_ptr<manager::MT4Manager> quote = std::make_unique<manager::MT4Manager>();
	uint64_t managerLogin = 20;
	std::string strServer("47.129.229.188:1950");
	std::string strPassword("BEd1FNa");
	if (quote->managerInitLogin(strServer, managerLogin, strPassword) != 0)
	{
		std::cerr << "manager api init login error, please connect it before use it";
		return -1;
	}
	UwsApp app;
	int http_port = 9001;

    // ── _quote_cb: 报价广播（外部线程 defer 到 uWS 线程，用 app.publish 全局组播）─────
    _quote_cb = [&app](std::string sym, LastQuote&& quotes) {
        if (!g_uwsLoop) return;
        std::string out = sym + quotes.toString();
        g_uwsLoop->defer([&app, out, sym]() {
            app.publish(sym, out, uWS::OpCode::TEXT);
        });
    };

    // ── WebSocket /ws ─────────────────────────────────────────────
    app.ws<WsUserData>("/*", {

        .open = [](UwsSocket* ws) {
            auto* ud = ws->getUserData();
            auto* pud = &ud->qcd;
            if (pud->loginid > 1) {
                std::unique_lock<std::shared_mutex> lock(uid_session_map_mutex);
                uid_session_map[pud->loginid].push_back(ws);
            }
        },

        .message = [&quote](UwsSocket* ws, std::string_view data, uWS::OpCode) {
            size_t lennn = data.size();
            if (lennn <= 4) {
                if (data == "ping") ws->send("ping", uWS::OpCode::TEXT);
                return;
            }
            auto* ud  = ws->getUserData();
            auto* pud = &ud->qcd;

            // sub:
            if (data.substr(0,4) == "sub:") {
                std::string lss(data.data()+4, lennn-4);
                auto vec = StringSplite(lss, ";");
                if (vec.size()==1) vec = StringSplite(vec[0], ",");
                std::set<std::string> syms(vec.begin(), vec.end());
                for (auto& symbol : syms) {
                    bool is_new = !pud->sublist.count(symbol);
                    if (is_new) {
                        pud->sublist[symbol] = {};
                        // 使用 uWS 内置 pub/sub 订阅 topic
                        ws->subscribe(symbol);
                        quote->SubSymbolAndLast(symbol); // 触发 MT4 订阅
                    }
                    LastQuote lq = quote->SubSymbolAndLast(symbol);
                    if (lq.IsVaild()) {
                        ws->send(symbol + lq.toString(), uWS::OpCode::TEXT);
                    }
                }
                return;
            }
            // margin:
            if (data.substr(0,7) == "margin:") {
                return;
            }
            // auth:
            if (lennn>5 && data.substr(0,5)=="auth:") {
                pud->authed=true;
                ws->send("auth:ok", uWS::OpCode::TEXT);
                return;
            }
            // cmd:
            if (lennn>4 && data.substr(0,4)=="cmd:") {
            	return;
            }
            // unknown
            if (pud->unknows++ > 100) {
                ws->close();
            }
        },

        .close = [](UwsSocket* ws, int code, std::string_view) {
            auto* ud = ws->getUserData();
            auto* pud = &ud->qcd;
            remove_ws_from_uid_map(ws, pud->loginid);
            // uWS 会在连接关闭时自动取消所有 topic 订阅，无需手动处理
        }
        });

	// ── 获取 Loop 指针（在 app.run() 前，当前线程是 uWS 线程）────
	g_uwsLoop = uWS::Loop::get();

	// ── 启动 MT4 pump 回调线程（_quote_cb 已赋值后再启动）────────
	if (quote->startCallBackThread() != RET_OK) {
		std::cerr << "Failed to start MT4 pump callback thread" << std::endl;
		return -1;
	}

	// ── 绑定端口 ─────────────────────────────────────────────────
	app.listen(http_port, [http_port](auto* listenSocket) {
		if (listenSocket) 
			std::cout <<  "Http/WS Server started on port " <<  http_port << std::endl;
		else              
			std::cerr <<  "Failed to listen on port " << http_port << std::endl;
		});
    app.run();
	return 0;
}
