#pragma once
#include "MTTradeData.h"

namespace manager {


class MT4Manager;
class MT5Manager;

class ManagerService
{
public:
    // 构造函数
    explicit ManagerService();
    ~ManagerService();
    
    // 初始化服务
    bool initialize();
    // 关闭服务
    void shutdown();

    bool initializeMT4(const std::string& srv, uint64_t admin, const std::string& pass);
    bool initializeMT5(const std::string& srv, uint64_t admin, const std::string& pass);

    bool handleMT4(UnifiedTradeData& data);
    bool handleMT5(UnifiedTradeData& data);

private:
	bool m_initMT4 = false;
	bool m_initMT5 = false;

	MT4Manager* m_mt4 = nullptr;
	MT5Manager* m_mt5 = nullptr;
};

}