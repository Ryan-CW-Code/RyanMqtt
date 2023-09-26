#ifndef __RyanMqttLog__
#define __RyanMqttLog__

#include <stdio.h>
#include <stdarg.h>
#include "platformSystem.h"

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

/**
 * @brief 日志相关
 *
 */
#if rlogEnable > 0
static void rlog_output(char *lvl, uint8_t color_n, char *const fmt, ...)
{
    // RyanLogPrintf("\033[字背景颜色;字体颜色m  用户字符串 \033[0m" );
    char dbgBuffer[386];
    uint16_t len;

// 打印颜色
#if rlogColorEnable > 0
    len = snprintf(dbgBuffer, sizeof(dbgBuffer), "\033[%dm", color_n);
    platformPrint(dbgBuffer, len);
#endif

    // 打印提示符
    len = snprintf(dbgBuffer, sizeof(dbgBuffer), "[%s/%s]", lvl, rlogTag);
    platformPrint(dbgBuffer, len);

    // 打印用户输入
    va_list args;
    va_start(args, fmt);
    len = vsnprintf(dbgBuffer, sizeof(dbgBuffer), fmt, args);
    va_end(args);
    platformPrint(dbgBuffer, len);

// 打印颜色
#if rlogColorEnable > 0
    platformPrint("\033[0m", sizeof("\033[0m"));
#endif

    platformPrint("\r\n", sizeof("\r\n"));
}

static void rlog_output_raw(char *const fmt, ...)
{
    char dbgBuffer[256];
    uint16_t len;

    va_list args;
    va_start(args, fmt);
    len = vsnprintf(dbgBuffer, sizeof(dbgBuffer), fmt, args);
    va_end(args);

    platformPrint(dbgBuffer, len);
}

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
