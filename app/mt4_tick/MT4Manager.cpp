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
	UnifiedTradeData4* _data = (UnifiedTradeData4*)data;
	TradeTransInfo info = { 0 };
	
	// 将symbol转换成大写字母
	std::string symbolStr = UTF8ToGBK(_data->mt4.symbol);
	std::transform(symbolStr.begin(), symbolStr.end(), symbolStr.begin(), ::toupper);
	_snprintf(info.symbol, 12, symbolStr.c_str());
	if(_data->mt4.comment[0] != '\0')	
		_snprintf(info.comment, 256, "%s", _data->mt4.comment);
	
	std::cout << "[DEBUG] MT4 handle - Ticket:" << _data->mt4.ticket << " Symbol:" << _data->mt4.symbol << "->" << info.symbol << " Type:" << (_data->mt4.type == TradeType::BUY ? "BUY" : "SELL") << " Size:" << _data->mt4.size << " Price:" << _data->mt4.openPrice << " Login:" << login << std::endl;
	
	info.orderby = static_cast<int>(login);
	info.cmd = (_data->mt4.type == TradeType::BUY ? OP_BUY: OP_SELL);
	info.type = TT_BR_ORDER_OPEN;
	info.price = _data->mt4.openPrice;
	info.volume = (int)(100 * (_data->mt4.size + 0.00001));
	info.sl = _data->mt4.stopLoss;
	info.tp = _data->mt4.takeProfit;
	int res = _api_pump->TradeTransaction(&info);
	
	// Print all TradeTransInfo parameters in one line
	std::cout << "[DEBUG] TradeTransaction Parameters (Order Open): Type=" << (int)info.type << " Flags=" << (int)info.flags << " Cmd=" << info.cmd << " Order=" << info.order << " OrderBy=" << info.orderby << " Symbol=" << info.symbol << " Volume=" << info.volume << " Price=" << info.price << " SL=" << info.sl << " TP=" << info.tp << " Comment=" << info.comment << " Result=" << res << std::endl;

	if (res != RET_OK && res != RET_TRADE_ACCEPTED && res != RET_TRADE_PROCESS) {
		std::cout << "[ERROR] MT4 handle - TradeTransaction failed, code:" << res << std::endl;
		return false;
	}
	int uuid = info.order;
	int total = 1;
	std::cout << "[DEBUG] MT4 handle - Requesting trade record, order:" << uuid << std::endl;
	
	
	
	if (_data->mt4.getCloseTimeUnix() > 100)
	{
		info.type = TT_BR_ORDER_CLOSE;
		info.order = uuid;
		res = _api_pump->TradeTransaction(&info);
		
		// Print all TradeTransInfo parameters in one line
		std::cout << "[DEBUG] TradeTransaction Parameters (Order Close): Type=" << (int)info.type << " Flags=" << (int)info.flags << " Cmd=" << info.cmd << " Order=" << info.order << " OrderBy=" << info.orderby << " Symbol=" << info.symbol << " Volume=" << info.volume << " Price=" << info.price << " SL=" << info.sl << " TP=" << info.tp << " IE_Dev=" << info.ie_deviation << " Comment=" << info.comment << " Exp=" << info.expiration << " CRC=" << info.crc << " Result=" << res << std::endl;

	}

	TradeRecord* p = _api_pump->TradeRecordsRequest(&uuid, &total);

	if (p)
	{
		std::cout << "[DEBUG] MT4 handle - Updating trade record..." << std::endl;
		p->open_time = static_cast<__time32_t>(_data->mt4.getOpenTimeUnix());
		p->close_time = static_cast<__time32_t>(_data->mt4.getCloseTimeUnix());
		p->commission = _data->mt4.commission;
		p->storage = _data->mt4.swap;
		p->close_price = _data->mt4.closePrice;
		p->profit = _data->mt4.profit;
		p->taxes = _data->mt4.taxes;
		
		std::cout << "[DEBUG] MT4 handle - Record updated - Open:" << p->open_time << " Close:" << p->close_time << " Commission:" << p->commission << " Storage:" << p->storage << " ClosePrice:" << p->close_price << " Profit:" << p->profit << " Taxes:" << p->taxes << std::endl;
		
		res = _api_pump->AdmTradeRecordModify(p);
		if (res != RET_OK) {
			std::cout << "[ERROR] MT4 handle - AdmTradeRecordModify failed, code:" << res << std::endl;
			return false;
		}
		
		std::cout << "[SUCCESS] MT4 handle - Trade record modified" << std::endl;
	}
	else
	{
		std::cout << "[WARNING] MT4 handle - No trade record found for order:" << uuid << std::endl;
	}
	
	std::cout << "[SUCCESS] MT4 handle - All operations completed for ticket:" << _data->mt4.ticket << std::endl;
	return true;
}
bool MT4Manager::handle_balance(UnifiedTradeData* data, uint64_t login)
{
	UnifiedTradeDataBalance4* _data = (UnifiedTradeDataBalance4*)data;
	
	std::cout << "[DEBUG] MT4 handle_balance - Ticket:" << _data->mt4.ticket << " Profit:" << _data->mt4.profit << " Login:" << login << std::endl;
	
	TradeTransInfo info = { 0 };
	info.orderby = static_cast<int>(login);
	info.cmd = OP_BALANCE;
	info.type = TT_BR_BALANCE;
	info.price = _data->mt4.profit;
	if (_data->mt4.comment[0] != '\0')
		_snprintf(info.comment, 256, "%s", _data->mt4.comment);
	
	int res = _api_pump->TradeTransaction(&info);

	if (res != RET_OK && res != RET_TRADE_ACCEPTED && res != RET_TRADE_PROCESS) {
		std::cout << "[ERROR] MT4 handle_balance - TradeTransaction failed, code:" << res << std::endl;
		return false;
	}
	
	int uuid = info.order;
	int total = 1;

	TradeRecord* p = _api_pump->TradeRecordsRequest(&uuid, &total);
	if (p)
	{
		p->open_time = static_cast<__time32_t>(_data->mt4.getOpenTimeUnix());
		res = _api_pump->AdmTradeRecordModify(p);
		if (res != RET_OK) {
			std::cout << "[ERROR] MT4 handle_balance - AdmTradeRecordModify failed, code:" << res << std::endl;
			return false;
		}
		
		std::cout << "[SUCCESS] MT4 handle_balance - Trade record modified" << std::endl;
	}
	else
	{
		std::cout << "[WARNING] MT4 handle_balance - No trade record found for order:" << uuid << std::endl;
	}
	
	std::cout << "[SUCCESS] MT4 handle_balance - All operations completed for ticket:" << _data->mt4.ticket << std::endl;
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
