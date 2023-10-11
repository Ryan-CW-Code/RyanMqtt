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
    char dbgBuffer[256] = {0};
    uint16_t len = 0;

// 打印颜色
#if rlogColorEnable > 0
    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "\033[%dm", color_n);
#endif

    // 打印提示符
    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "[%s/%s]", lvl, rlogTag);

    // 打印用户输入
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, fmt, args);
    va_end(args);

// 打印颜色
#if rlogColorEnable > 0
    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "\033[0m");
#endif

    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "\r\n");

    platformPrint(dbgBuffer, len);
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
#define rlog_d(fmt, ...) rlog_output("D", 0, " %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define rlog_d(...)
#endif

#if (rlogLevel >= rlogLvlInfo)
#define rlog_i(fmt, ...) rlog_output("I", 32, " %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define rlog_i(...)
#endif

#if (rlogLevel >= rlogLvlWarning)
#define rlog_w(fmt, ...) rlog_output("W", 33, " %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define rlog_w(...)
#endif

#if (rlogLevel >= rlogLvlError)
#define rlog_e(fmt, ...) rlog_output("E", 31, " %s:%d " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define rlog_e(...)
#endif

#define log_d rlog_d

#define rlog_raw(...) rlog_output_raw(__VA_ARGS__)

#endif
