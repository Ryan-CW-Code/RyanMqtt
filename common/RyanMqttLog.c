#include "RyanMqttLog.h"

void rlog_output(char *lvl, uint8_t color_n, char *fileStr, uint32_t lineNum, char *const fmt, ...)
{
    // RyanLogPrintf("\033[字背景颜色;字体颜色m  用户字符串 \033[0m" );
    char dbgBuffer[256];
    uint16_t len = 0;

    // 打印颜色、提示符、打印文件路径、行号
    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "\033[%dm[%s] %s:%d ", color_n, lvl, fileStr, lineNum);

    // platformPrint(dbgBuffer, len);
    // len = 0;

    // 打印用户输入
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, fmt, args);
    va_end(args);

    // 打印颜色
    len += snprintf(dbgBuffer + len, sizeof(dbgBuffer) - len, "\033[0m\r\n");

    platformPrint(dbgBuffer, len);
}

void rlog_output_raw(char *const fmt, ...)
{
    char dbgBuffer[256];
    uint16_t len;

    va_list args;
    va_start(args, fmt);
    len = vsnprintf(dbgBuffer, sizeof(dbgBuffer), fmt, args);
    va_end(args);

    platformPrint(dbgBuffer, len);
}