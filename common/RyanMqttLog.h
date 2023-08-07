#ifndef __RyanMqttLog__
#define __RyanMqttLog__

#include <stdarg.h>
#include <stdio.h>
#include "RyanMqttPublic.h"

#define RyanLogPrintf(fmt, ...) printf
// #define RyanLogPrintf(fmt, ...)                                                                 \
//     do                                                                                          \
//     {                                                                                           \
//         char str[256];                                                                          \
//         snprintf(str, sizeof(str), fmt, ##__VA_ARGS__);                                         \
//         Ql_UART_Write((Enum_SerialPort)(UART_PORT0), (u8 *)(str), strlen((const char *)(str))); \
//     } while (0)

// 日志等级
#define rlogLvlError 0
#define rlogLvlWarning 1
#define rlogLvlInfo 2
#define rlogLvlDebug 3

// 是否使能日志
#ifndef rlogEnable
#define rlogEnable 1
#endif

// 是否使能日志颜色
#ifndef rlogColorEnable
#define rlogColorEnable 1
#endif

// 日志打印等级
#ifndef rlogLevel
#define rlogLevel (rlogLvlDebug)
#endif

// 日志tag
#ifndef rlogTag
#define rlogTag "LOG"
#endif

// RyanLogPrintf("\033[字背景颜色;字体颜色m  用户字符串 \033[0m" );
#if rlogColorEnable > 0
#define rlogColorStart(color_n) RyanLogPrintf("\033[" #color_n "m")
#define rlogColorEnd RyanLogPrintf("\033[0m")
#else
#define rlogColorStart(color_n)
#define rlogColorEnd
#endif

#if rlogEnable > 0

/**
 * @brief 日志相关
 *
 */
#define rlog_output(lvl, color_n, fmt, ...)   \
    do                                        \
    {                                         \
        rlogColorStart(color_n);              \
        RyanLogPrintf("[" lvl "/" rlogTag "]" \
                      " %s:%d ",              \
                      __FILE__,               \
                      __LINE__);              \
        RyanLogPrintf(fmt, ##__VA_ARGS__);    \
        rlogColorEnd;                         \
        RyanLogPrintf("\r\n");                \
    } while (0)

#define rlog_output_raw(...) RyanLogPrintf(__VA_ARGS__);

#else
#define rlog_output(lvl, color_n, fmt, ...)
#define rlog_output_raw(...)
#endif

/**
 * @brief log等级检索
 *
 */
#if (rlogLevel >= rlogLvlDebug)
#define rlog_d(fmt, ...) rlog_output("D", 0, fmt, ##__VA_ARGS__)
#else
#define rlog_d(...)
#endif

#if (rlogLevel >= rlogLvlInfo)
#define rlog_i(fmt, ...) rlog_output("I", 32, fmt, ##__VA_ARGS__)
#else
#define rlog_i(...)
#endif

#if (rlogLevel >= rlogLvlWarning)
#define rlog_w(fmt, ...) rlog_output("W", 33, fmt, ##__VA_ARGS__)
#else
#define rlog_w(...)
#endif

#if (rlogLevel >= rlogLvlError)
#define rlog_e(fmt, ...) rlog_output("E", 31, fmt, ##__VA_ARGS__)
#else
#define rlog_e(...)
#endif

#define log_d rlog_d

#define rlog_raw(...) rlog_output_raw(__VA_ARGS__)

#endif
