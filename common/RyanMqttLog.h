#ifndef __RyanMqttLog__
#define __RyanMqttLog__

#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>

// 日志等级
#define rlogLvlAssert  0
#define rlogLvlError   1
#define rlogLvlWarning 2
#define rlogLvlInfo    2
#define rlogLvlDebug   3

// 日志打印等级
#ifndef rlogLevel
#define rlogLevel (rlogLvlDebug)
#endif

// 日志tag
#ifndef rlogTag
#define rlogTag "LOG"
#endif

extern void rlog_output_raw(char *const fmt, ...);
extern void rlog_output(char *lvl, uint8_t color, char *fileStr, uint32_t lineNum, char *const fmt, ...);

/**
 * @brief log等级检索
 *
 */
#if (rlogLevel >= rlogLvlDebug)
#define rlog_d(fmt, ...) rlog_output("D", 0, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define rlog_d(...)
#endif

#if (rlogLevel >= rlogLvlInfo)
#define rlog_i(fmt, ...) rlog_output("I", 32, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define rlog_i(...)
#endif

#if (rlogLevel >= rlogLvlWarning)
#define rlog_w(fmt, ...) rlog_output("W", 33, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define rlog_w(...)
#endif

#if (rlogLevel >= rlogLvlError)
#define rlog_e(fmt, ...) rlog_output("E", 31, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define rlog_e(...)
#endif

#define rlog_raw(...) rlog_output_raw(__VA_ARGS__)

#endif
