#pragma once
#include <string>

//+=======================================+
//|  // K line
//+=======================================+
struct MtChartBar
{
	long long   t = 0;  // 时间戳, 注意 MT 的时间戳时按照标准时间戳 + 其 设定的时区, 此时间戳需要按 0 时区转时间才能与 MT 保持一致
	double o = 0;       // 开
	double h = 0;       // 最高价格
	double l = 0;       // 最低价格
	double c = 0;       // 收盘价 
	long long  v = 0;   // 报价量 – 柱状图形成期间收到的报价数量。  tick volume
	long long rv = 0;   // 实际交易量 – 在柱形图期间执行的真实交易。 real volume
};


class LastQuote {
public:
	double ask = 0;
	double bid = 0;
	int    digits = 8;
	long long ctm = 0;   // ¸ÄÎª ms
	double day_open = 0;
	double day_high = 0;
	double day_low = 0;
	double per_close = 0;

	MtChartBar lastm1;
	MtChartBar lastm1_p;

	std::string toString() {
		std::string digit_str = std::to_string(digits);
		std::string fma = " {:." + digit_str + "f} {:." + digit_str + "f} {} {:."
			+ digit_str + "f} {:." + digit_str + "f} {:." + digit_str + "f} {:." + digit_str + "f}";
		return std::move(fma);
	}

	bool IsVaild() { return ask > 0 && bid > 0; };

	bool UpdateLastQuote(long long t_msc, double a, double b, long long rv) {
		if (t_msc < ctm) return false;
		// ¸üÐÂ lastm1
		long long t = t_msc / 1000;
		long long cc_tt = t / 60 * 60;
		if (lastm1.t <= 0 || cc_tt != lastm1.t) { 
			lastm1_p = lastm1;  
			lastm1.t = cc_tt;
			lastm1.o = b;
			lastm1.h = b;
			lastm1.l = b;
			lastm1.c = b;
			lastm1.v = 1;
			lastm1.rv = rv;
		}
		else {
			// Ö»ÔÚÓÐÖµÊ±¸üÐÂ
			if (lastm1.h > 0) {
				lastm1.h = b > lastm1.h ? b : lastm1.h; // Math.Max(bid, lastm1.h);
			}
			if (lastm1.l < 0 || lastm1.l > b) {
				lastm1.l = b; // Math.Min(bid, lastm1.l);
			}
			lastm1.c = b;
			lastm1.v += 1;
			lastm1.rv += rv;
		}

		// ¸üÐÂ  day data,  µÃµ½×òÊÕ£¬²ÅÄÜ¸üÐÂ high low 
		if (ctm > 0 && per_close > 0) {
			long long current_day = t / 86400;
			long long previous_day = ctm / 1000 / 86400;
			if (current_day != previous_day) {  // ¿çÌì
				day_high = b;
				day_low = b;
				day_open = b;
				per_close = bid;
			}
			else {
				day_high = b > day_high ? b : day_high;
				day_low = (day_low > 0 && b < day_low) ? b : day_low;
			}
		}
		//  Ìí¼Ó  ±£´æ tick µÄ ÅÐ¶Ï, ÏàÍ¬¼Û¸ñ, 350ms±£´æÒ»¸ö£¬²»Í¬¼Û¸ñ 30ms ±£´æÒ»¸ö-----------
		bool save_it = false;
		long long t_gap = t_msc - ctm;
		if (t_gap > 450 || (t_gap > 190 && (bid != b || ask != a))) { save_it = true; }
		//  -------------------
		ctm = t_msc;
		bid = b;
		ask = a;
		return save_it;
	}
};

struct UserSymbolDiff
{
	//int digits = 8;
	double bid_diff = 0.0;   // Bid -= bid_diff;
	double ask_diff = 0.0;   // Ask += ask_diff;
};

class QuoteClientConnectionData
{
public:
	UINT64 loginid = 0;
	std::string servername;
	std::unordered_map<std::string, UserSymbolDiff>  sublist;
	bool authed = false;
	int  unknows = 0;
};