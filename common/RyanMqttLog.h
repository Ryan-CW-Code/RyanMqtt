#ifndef __RyanMqttLog__
#define __RyanMqttLog__

#include <stdint.h>
#include <stdarg.h>

// 日志等级
#define RyanMqttLogLevelAssert  0
#define RyanMqttLogLevelError   1
#define RyanMqttLogLevelWarning 2
#define RyanMqttLogLevelInfo    3
#define RyanMqttLogLevelDebug   4

// 日志打印等级
#ifndef RyanMqttLogLevel
#define RyanMqttLogLevel (RyanMqttLogLevelDebug)
#endif

extern void RyanMqttLogOutPutRaw(char *const fmt, ...);
extern void RyanMqttLogOutPut(const char *lvl, uint8_t color, const char *fileStr, uint32_t lineNum, char *const fmt,
			      ...);

/**
 * @brief log等级检索
 *
 */
#if (RyanMqttLogLevel >= RyanMqttLogLevelDebug)
#define RyanMqttLog_d(fmt, ...) RyanMqttLogOutPut("D", 0, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define RyanMqttLog_d(...)
#endif

#if (RyanMqttLogLevel >= RyanMqttLogLevelInfo)
#define RyanMqttLog_i(fmt, ...) RyanMqttLogOutPut("I", 32, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define RyanMqttLog_i(...)
#endif

#if (RyanMqttLogLevel >= RyanMqttLogLevelWarning)
#define RyanMqttLog_w(fmt, ...) RyanMqttLogOutPut("W", 33, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define RyanMqttLog_w(...)
#endif

#if (RyanMqttLogLevel >= RyanMqttLogLevelError)
#define RyanMqttLog_e(fmt, ...) RyanMqttLogOutPut("E", 31, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define RyanMqttLog_e(...)
#endif

#define RyanMqttLog_raw(...) RyanMqttLogOutPutRaw(__VA_ARGS__)

#endif
