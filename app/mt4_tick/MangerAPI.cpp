#include "Platform.h"
#include "MangerAPI.h"
#include "MT4Manager.h"

namespace manager {

ManagerService::ManagerService()
{
	initialize();
}

ManagerService::~ManagerService()
{

}

bool ManagerService::initialize()
{

	return true;
}
bool ManagerService::initializeMT4(const std::string& srv, uint64_t admin, const std::string& pass)
{
	m_mt4 = new MT4Manager();
	m_mt4->managerInitLogin(srv, admin, pass);

	
	m_initMT4 = true;
	return true;
}
bool ManagerService::initializeMT5(const std::string& srv, uint64_t admin, const std::string& pass)
{
	return true;
}
bool ManagerService::handleMT4(UnifiedTradeData& data)
{
    
	return true;
}

bool ManagerService::handleMT5(UnifiedTradeData& data)
{
	return true;
}

void ManagerService::shutdown()
{

}

}


