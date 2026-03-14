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


std::unordered_map<std::string, std::vector<std::pair<UwsSocket*, UserSymbolDiff>>> symbol_subscribers;
std::shared_mutex symbol_subscribers_mutex;
std::unordered_map<UINT64, std::vector<UwsSocket*>> uid_session_map;
std::shared_mutex uid_session_map_mutex;
static uWS::Loop* g_uwsLoop = nullptr;

static void ws_send(UwsSocket* ws, std::string_view msg) {
    ws->send(msg, uWS::OpCode::TEXT);
}

static void remove_ws_from_symbol_index(UwsSocket* ws) {
    std::unique_lock<std::shared_mutex> lock(symbol_subscribers_mutex);
    for (auto it = symbol_subscribers.begin(); it != symbol_subscribers.end(); ) {
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [ws](const auto& p){ return p.first == ws; }), vec.end());
        if (vec.empty()) it = symbol_subscribers.erase(it); else ++it;
    }
}
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

    // ── _quote_cb: 报价广播（外部线程调用，defer 到 uWS 线程）─────
    _quote_cb = [](std::string sym, LastQuote&& quotes) {
        if (!g_uwsLoop) return;
        // 先在当前线程查找订阅者列表（只读锁）
        std::vector<std::pair<UwsSocket*, UserSymbolDiff>> targets;
        {
            std::shared_lock<std::shared_mutex> lock(symbol_subscribers_mutex);
            auto it = symbol_subscribers.find(sym);
            if (it == symbol_subscribers.end()) return;
            targets = it->second;
        }
        if (targets.empty()) return;
        // defer 到 uWS 事件循环线程再操作 ws
        g_uwsLoop->defer([sym, q = std::move(quotes), targets]() mutable {
            // 在 uWS 线程内二次确认 ws 仍然存活（防止悬空指针）
            std::vector<UwsSocket*> live_ws;
            {
                std::shared_lock<std::shared_mutex> lock(symbol_subscribers_mutex);
                auto it = symbol_subscribers.find(sym);
                if (it != symbol_subscribers.end()) {
                    for (auto& [sws, _] : it->second)
                        live_ws.push_back(sws);
                }
            }
            for (auto& [ws, diff] : targets) {
                // 只对仍然在订阅表中的 ws 发送
                bool alive = (std::find(live_ws.begin(), live_ws.end(), ws) != live_ws.end());
                if (!alive) continue;
                LastQuote myq = q;
                myq.bid -= diff.bid_diff;
                myq.ask += diff.ask_diff;
                std::string out = sym + myq.toString();
                ws->send(out, uWS::OpCode::TEXT);
            }
            });
        };

    // ── WebSocket /ws ─────────────────────────────────────────────
    app.ws<WsUserData>("/*", {

        .open = [](UwsSocket* ws) {
            auto* ud = ws->getUserData();
            auto* pud = &ud->qcd;
          //  SPDLOG_INFO("ws open: {}, loginid: {}", ud->remote_ip, pud->loginid);
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
            //SPDLOG_DEBUG("ws<=[{}]: {}", ud->remote_ip, std::string(data.data(), std::min(lennn,(size_t)200)));

            // sub:
            if (data.substr(0,4) == "sub:") {
                std::string lss(data.data()+4, lennn-4);
                auto vec = StringSplite(lss, ";");
                if (vec.size()==1) vec = StringSplite(vec[0], ",");
                std::set<std::string> syms(vec.begin(), vec.end());
                for (auto& symbol : syms) {
                    UserSymbolDiff diff = {0};
                    bool is_new = !pud->sublist.count(symbol);
                    if (is_new) {
                        if (pud->loginid > 0)
                            quote->GetUserSymbolSpreadDiff(symbol, pud->loginid, diff);
                        pud->sublist[symbol] = diff;
                        std::unique_lock<std::shared_mutex> lock(symbol_subscribers_mutex);
                        symbol_subscribers[symbol].emplace_back(ws, diff);
                    } else { diff = pud->sublist[symbol]; }
                    LastQuote lq = quote->SubSymbolAndLast(symbol);
                    if (lq.IsVaild()) {
                        LastQuote myq = lq;
                        myq.bid -= diff.bid_diff;
                        myq.ask += diff.ask_diff;
                        ws->send(symbol + myq.toString(), uWS::OpCode::TEXT);
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
                ws->send("auth:ok", uWS::OpCode::TEXT); return;
                return;
            }
            // cmd:
            if (lennn>4 && data.substr(0,4)=="cmd:") {
            	return;
            }
            // unknown
            if (pud->unknows++ > 100) {
               // SPDLOG_ERROR("session [{}] too many unknown cmds, stop", pud->loginid);
                ws->close();
            }
        },

        .close = [](UwsSocket* ws, int code, std::string_view) {
            auto* ud = ws->getUserData();
            auto* pud = &ud->qcd;
            //SPDLOG_INFO("ws close: {}, code: {}", ud->remote_ip, code);
            remove_ws_from_uid_map(ws, pud->loginid);
            remove_ws_from_symbol_index(ws);
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
			std::cerr <<  "Failed to listen on port {}" << http_port << std::endl;
		});
    app.run();
	return 0;
}