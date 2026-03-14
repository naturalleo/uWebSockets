#include "stdafx.h"
#include "MangerAPI.h"
#include "MT4Manager.h"
#include "MT5Manager.h"

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
	m_mt5 = new MT5Manager();
	m_mt5->managerInitLogin(srv, admin, pass);

	m_initMT5 = true;
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


