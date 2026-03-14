#pragma once
#include "MTManager.h"
#include <string>

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

protected:
    bool initialize() override;
    void cleanup() override;
    int login() override;

private:
    std::string     m_host;
    std::string     m_password;
    uint64_t        m_manager;
};

}