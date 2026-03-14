#pragma once
#include <string>
#include <cstdint>
#include <variant>
#include <vector>
#include <algorithm>
#include <iostream>
#include <map>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace manager {

// 交易类型枚举
enum class TradeType {
    BUY,
    SELL,
    BALANCE,  // MT5特有的余额类型
    UNKNOWN
};

// 交易趋势枚举（MT5特有）
enum class TradeTrend {
    IN_TRADE,      // 开仓
    OUT_TRADE,     // 平仓
    NONE
};

// 数据来源类型枚举
enum class DataSource {
    MT4,
    MT5_ORDER,
    MT5_DEAL,
    UNKNOWN
};

// 时间转换工具类
class TimeConverter {
public:
    // 将MT4/MT5时间字符串转换为Unix时间戳
    static time_t stringToUnixTime(const char* timeStr) {
        // 格式: "2025.07.31 11:51:47"
        std::tm tm = {};
        
        // 使用sscanf正确解析格式
        int year, month, day, hour, minute, second;
        if (sscanf_s(timeStr, "%d.%d.%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
            return 0; // 解析失败返回0
        }
        
        // 设置tm结构体
        tm.tm_year = year - 1900;  // tm_year是从1900年开始的
        tm.tm_mon = month - 1;     // tm_mon是从0开始的
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;           // 不使用夏令时
        
        // 直接使用gmtime转换为UTC时间戳
        return _mkgmtime(&tm);
    }
    
    // 将Unix时间戳转换为MT4/MT5格式的字符串
    static void unixTimeToString(time_t unixTime, char* buffer, size_t bufferSize) {
        std::tm* tm = std::localtime(&unixTime);
        if (!tm) {
            strncpy(buffer, "", bufferSize);
            return;
        }
        
        snprintf(buffer, bufferSize, "%04d.%02d.%02d %02d:%02d:%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    
    // 获取当前Unix时间戳
    static time_t getCurrentUnixTime() {
        return std::time(nullptr);
    }
    
    // 计算两个时间之间的差值（秒）
    static time_t timeDifference(const char* time1, const char* time2) {
        time_t t1 = stringToUnixTime(time1);
        time_t t2 = stringToUnixTime(time2);
        return t2 - t1;
    }

    // 计算double类型数字小数点后的位数
    static int getDecimalPlaces(double value) {
        if (value == 0.0) return 0;

        // 转换为字符串，避免浮点精度问题
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.10f", value);

        // 找到小数点位置
        char* decimalPoint = strchr(buffer, '.');
        if (!decimalPoint) return 0;

        // 从右往左找到第一个非零字符
        int length = strlen(buffer);
        int lastNonZero = length - 1;

        // 跳过末尾的0
        while (lastNonZero > 0 && buffer[lastNonZero] == '0') {
            lastNonZero--;
        }

        // 如果最后一位是小数点，说明是整数
        if (buffer[lastNonZero] == '.') return 0;

        // 计算小数点后的位数
        return lastNonZero - (decimalPoint - buffer);
    }

    // 另一种实现方式：使用数学方法（更精确但可能有精度问题）
    static int getDecimalPlacesMath(double value) {
        if (value == 0.0) return 0;

        // 取绝对值
        double absValue = fabs(value);

        // 如果是整数，返回0
        if (absValue == floor(absValue)) return 0;

        // 计算小数点后的位数
        int places = 0;
        double temp = absValue;

        // 最多检查15位小数（double的精度限制）
        for (int i = 0; i < 15; i++) {
            temp *= 10;
            if (fabs(temp - floor(temp)) < 1e-10) {
                places = i + 1;
                break;
            }
        }

        return places;
    }
};

// MT4交易数据结构
struct MT4TradeData {
    uint64_t ticket;           // 订单号
    char openTime[32];         // 开仓时间
    TradeType type;            // 交易类型
    double size;               // 交易量
    char symbol[32];           // 交易品种
    double openPrice;          // 开仓价格
    double stopLoss;           // 止损
    double takeProfit;         // 止盈
    char closeTime[32];        // 平仓时间
    double closePrice;         // 平仓价格
    double commission;         // 手续费
    double taxes;              // 税费
    double swap;               // 库存费
    double profit;             // 盈亏
    char comment[256];         // 注释
    
    MT4TradeData() : ticket(0), type(TradeType::UNKNOWN), size(0.0), 
                     openPrice(0.0), stopLoss(0.0), takeProfit(0.0), 
                     closePrice(0.0), commission(0.0), taxes(0.0), 
                     swap(0.0), profit(0.0) {
        strcpy(openTime, "");
        strcpy(symbol, "");
        strcpy(closeTime, "");
        strcpy(comment, "");
    }
    
    // 设置字符串成员
    void setOpenTime(const char* time) {
        strncpy(openTime, time, sizeof(openTime) - 1);
        openTime[sizeof(openTime) - 1] = '\0';
    }
    
    void setSymbol(const char* sym) {
        strncpy(symbol, sym, sizeof(symbol) - 1);
        symbol[sizeof(symbol) - 1] = '\0';
    }
    
    void setCloseTime(const char* time) {
        strncpy(closeTime, time, sizeof(closeTime) - 1);
        closeTime[sizeof(closeTime) - 1] = '\0';
    }
    
    void setComment(const char* comm) {
        strncpy(comment, comm, sizeof(comment) - 1);
        comment[sizeof(comment) - 1] = '\0';
    }
    
    // 获取开仓时间的Unix时间戳
    time_t getOpenTimeUnix() const {
        return TimeConverter::stringToUnixTime(openTime);
    }
    
    // 获取平仓时间的Unix时间戳
    time_t getCloseTimeUnix() const {
        return TimeConverter::stringToUnixTime(closeTime);
    }
    
    // 计算持仓时间（秒）
    time_t getHoldTime() const {
        return TimeConverter::timeDifference(openTime, closeTime);
    }
};


// 统一交易数据结构（使用联合体适配MT4和MT5）
struct UnifiedTradeData {
};
struct UnifiedTradeData4 :public UnifiedTradeData {
    MT4TradeData mt4;
};

struct UnifiedTradeDataBalance4:public UnifiedTradeData {
    MT4TradeData mt4;
};

} // namespace manager 