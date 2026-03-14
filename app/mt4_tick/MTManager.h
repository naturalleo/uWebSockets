#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include "MTTradeData.h"


// 前向声明
class CManagerFactory;
class CManagerInterface;
class IMTManagerAPI;
class CMTManagerAPIFactory;

namespace manager {





// 模板基类 - 使用策略模式
template<typename FactoryType, typename APIType>
class MTManager {
public:
    virtual ~MTManager() = default;

    virtual int managerInitLogin(const std::string& srv, uint64_t admin, const std::string& pass) = 0;

    virtual bool handle(UnifiedTradeData* data, uint64_t login) = 0;
    virtual bool handle_balance(UnifiedTradeData* data, uint64_t login) = 0;

protected:
    // 共享的成员变量，通过模板参数指定类型
    std::unique_ptr<FactoryType> _factory;
    APIType* _api_pump = nullptr;
    
    // 构造函数
    MTManager() = default;
    
    // 初始化函数
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    virtual int login() = 0;
};



}