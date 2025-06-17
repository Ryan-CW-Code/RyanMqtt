#include "RyanMqttLog.h"

/**
 * @brief Outputs a formatted log message with color, log level, file, and line information.
 *
 * Constructs a log message that includes an ANSI color code, log level label, source file name, and line number, followed by a user-formatted message. The output is color-coded and terminated with a reset sequence and newline, then sent to the platform-specific output function.
 *
 * @param lvl Log level label (e.g., "INFO", "ERROR").
 * @param color_n ANSI color code for the log message.
 * @param fileStr Source file name.
 * @param lineNum Source code line number.
 * @param fmt Format string for the user message, followed by variable arguments.
 */
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

/**
 * @brief Outputs a formatted string directly to the platform output.
 *
 * Formats the input string with variable arguments and sends it to the platform output function without adding any metadata or color formatting.
 *
 * @param fmt Format string for the message, followed by variable arguments.
 */
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