#pragma once
#include "Platform.h"
#include "MT4ManagerAPI.h"
#include "MTManager.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include "SafeMap.h"
#include "Models.h"

// 前向声明
class CManagerFactory;
class CManagerInterface;

namespace manager {

// MT4管理器 - 使用模板特化
class MT4Manager : public MTManager<CManagerFactory, CManagerInterface> {
public:
    MT4Manager();
    virtual ~MT4Manager();
    
    int managerInitLogin(const std::string& srv, uint64_t admin, const std::string& pass) override;
    
    bool handle(UnifiedTradeData* data, uint64_t login) override;
    bool handle_balance(UnifiedTradeData* data, uint64_t login) override;


    int startCallBackThread();

    void OnConnected()
    {
        _connected = true;
    };
    void OnDisconnected()
    {
        _connected = false;
    };

    void OnTick(const SymbolInfo & tick);



    // 获取用户的品种的  价格差异，已经乘以 Point ， 只需要 Bid/Ask  -/+  即可
    bool GetUserSymbolSpreadDiff(const std::string& symbol, const UINT64 login, UserSymbolDiff& spdiff)
    {
        UserRecord itu = { 0 };
        ConGroup itg = { 0 };
        ConSymbol it = { 0 };
        if (false == _connected ||
            RET_OK != _api_pump->UserRecordGet((int)login, &itu) ||
            RET_OK != _api_pump->GroupRecordGet(itu.group, &itg) ||
            RET_OK != _api_pump->SymbolGet(symbol.c_str() , &it)
            ) return false;

        if (itg.secgroups[it.type].show == 0)
        {
            //SPDLOG_WARN("Login ({}) Symbol {} disabled", login, symbol);
            return false;
        }
        // 正数的点数偏移，  Bid -= bid_diff * point
        int bid_diff = itg.secgroups[it.type].spread_diff / 2 - it.spread_balance;
        // 正数的点数偏移   Ask  += ask_diff * point
        int ask_diff = itg.secgroups[it.type].spread_diff - bid_diff;

        spdiff.bid_diff = it.point * bid_diff;
        spdiff.ask_diff = it.point * ask_diff;
        return true;
    };

    // 订阅，去重
    std::string SubSymbol(const std::string& symbol) {
        if (symbol.length() <= 0) return "";
        LastQuote tt = { 0 };
        if (_subed_quotes.FindAndRef(symbol, tt))
        {
            if (tt.day_open <= 0 || tt.day_high <= 0 || tt.day_low <= 0 || tt.per_close <= 0)
                need_get_high_low.emplace(symbol);
            return tt.toString();
        }
        int ret = _api_pump->SymbolAdd(symbol.data());
        //SPDLOG_INFO("Sub Symbol <{}> res<{}>:{}", symbol, ret, _api_pump->ErrorDescription(ret));
        if (RET_OK == ret) { _subed_quotes.Insert(symbol, LastQuote()); }
        return _api_pump->ErrorDescription(ret);
    };

    LastQuote SubSymbolAndLast(const std::string& symbol) {
        LastQuote tt = { 0 };
        if (symbol.length() <= 0) return tt;
        LastQuote tt_ref;
        if (_subed_quotes.FindAndRef(symbol, tt_ref))
        {
            if (tt_ref.day_open <= 0 || tt_ref.day_high <= 0 || tt_ref.day_low <= 0 || tt_ref.per_close <= 0)
                need_get_high_low.emplace(symbol);
            tt = LastQuote(tt_ref);
            return std::move(tt);
        }
        int ret = _api_pump->SymbolAdd(symbol.data());
        //SPDLOG_INFO("Sub Symbol <{}> res<{}>:{}", symbol, ret, _api_pump->ErrorDescription(ret));
        if (RET_OK == ret) { _subed_quotes.Insert(symbol, LastQuote()); }
        return std::move(tt);
    };


    
protected:
    bool initialize() override;
    void cleanup() override;
    int login() override;
    static unsigned long long _getCurrentMillisecs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    };
private:
    std::string     m_host;
    std::string     m_password;
    uint64_t        m_manager;

    SafeMap<std::string, LastQuote> _subed_quotes;

    // 数据存储相关 - 从子类提取的公共部分
    std::set<std::string> need_get_high_low;  // MT4使用string，需要MT5做转换
};

}