#include "Platform.h"
#include "MT4Manager.h"
#include "ManagerAPI/MT4ManagerAPI.h"
#include "CharCoding.h"
#include "Models.h"
#include <functional>

// 引用 main_quote.cpp 中的全局回调
extern std::function<void(std::string, LastQuote&&)> _quote_cb;

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
	_connected = false;
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
	_connected = true;
	ConManager rights = {};
	_api_pump->ManagerRights(&rights);
	return 0;
}

int MT4Manager::startCallBackThread()
{
	// 每次切换 '推送前' 刷新一次 symbol，不然掉线重连期间新品种 没得推送
	_api_pump->SymbolsRefresh();
	return _api_pump->PumpingSwitchEx([](int code, int type, void * data, void *param) {
		if (param == NULL) return;
		MT4Manager * pquote = (MT4Manager*)param;
		switch (code)
		{
		case PUMP_START_PUMPING:
			pquote->OnConnected();
			break;
		case PUMP_STOP_PUMPING:
			pquote->OnDisconnected();
			break;
		case PUMP_UPDATE_BIDASK:
		{
			SymbolInfo si[16];
			int total = 0;
			while ((total = pquote->_api_pump->SymbolInfoUpdated(si, 16)) > 0) {
				for (int i = 0; i < total; i++)
				{
					pquote->OnTick(si[i]);
				}
			}
		}
		break;
		case PUMP_UPDATE_SYMBOLS:
			break;
		case PUMP_UPDATE_GROUPS:
			break;
		case PUMP_UPDATE_USERS:
			break;
		case PUMP_UPDATE_TRADES:
			break;
		default:
			break;
		}

	}, CLIENT_FLAGS_HIDENEWS | CLIENT_FLAGS_HIDEMAIL | CLIENT_FLAGS_SENDFULLNEWS, this);
};

void MT4Manager::OnTick(const SymbolInfo & tick)
{
	std::string sym(tick.symbol);
	if (sym.empty()) return;
	if (!_subed_quotes.ContainsKey(sym)) return;

	// 在写锁内原地更新 LastQuote
	long ms = _getCurrentMillisecs() % 1000;
	long long tick_msc = ((long long)tick.lasttime) * 1000L + ms;
	_subed_quotes.Foreach([&](const std::string& k, LastQuote& lq) {
		if (k == sym) {
			lq.digits = tick.digits;
			lq.UpdateLastQuote(tick_msc, tick.ask, tick.bid, 1);
		}
	});

	// 取出更新后的快照，触发广播回调
	if (!_quote_cb) return;
	LastQuote snapshot;
	if (!_subed_quotes.FindAndRef(sym, snapshot)) return;
	_quote_cb(sym, std::move(snapshot));
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
