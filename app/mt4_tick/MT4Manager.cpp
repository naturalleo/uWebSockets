#include "Platform.h"
#include "MT4Manager.h"
#include "ManagerAPI/MT4ManagerAPI.h"
#include "CharCoding.h"

namespace manager {

MT4Manager::MT4Manager() {
    // 初始化MT4管理器
    _factory = std::make_unique<CManagerFactory>();
    initialize();
}

MT4Manager::~MT4Manager() {
    // 清理资源
    cleanup();
}

int MT4Manager::managerInitLogin(const std::string& srv, uint64_t admin, const std::string& pass) {
    _factory->Init();
	if (_factory->IsValid() == false || _factory->WinsockStartup() != RET_OK)
	{
		std::cout <<"MT4 Factory Init Error";
		return -1;
	}
	long ver = _factory->Version();

	std::cout << "MT4 factory version(" << ((ver >> 16) & 0xffff) << "." << (ver & 0xffff) << "),.h version (" << ((ManAPIVersion >> 16) & 0xffff) << "." << (ver & 0xffff) << ")" << std::endl;

	if (_factory->Version() > ManAPIVersion)
	{
		std::cout << "MT4 dll version should less than .h version." << std::endl;
		return -1;
	}

	if ((_api_pump = _factory->Create(ver)) == NULL) {
		std::cout << "Create Manager Interface Failure!" << std::endl;
		return -1;
	}
	m_host = srv;
	m_manager = admin;
	m_password = pass;

    return login(); // 返回成功状态
}

int MT4Manager::login()
{
	int ret = _api_pump->Connect(m_host.c_str());
	if (RET_OK != ret) {
		std::cout << "MT Connect <" << m_host <<"> err<"<< ret <<">:" << _api_pump->ErrorDescription(ret) << std::endl;
		return ret;
	}
	ret = _api_pump->Login(static_cast<int>(m_manager), m_password.c_str());
	if (RET_OK != ret) {
		std::cout << "_api_pump->Login(m_manager, m_password.c_str()) RET_OK != ret" << std::endl;
		return ret;
	}
	ConManager rights = {};
	_api_pump->ManagerRights(&rights);
	return 0;
}
bool MT4Manager::handle(UnifiedTradeData* data, uint64_t login)
{
	return true;
}
bool MT4Manager::handle_balance(UnifiedTradeData* data, uint64_t login)
{
	return true;
}

bool MT4Manager::initialize() 
{
    return true;
}

void MT4Manager::cleanup() {
    // 清理MT4特定的资源
    if (_api_pump) {
        // 清理API资源
        _api_pump->Release();
    }
    if(_factory.get())
    {
    	_factory->WinsockCleanup();
    }
}

}
