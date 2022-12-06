

#ifndef __RyanMqttLog__
#define __RyanMqttLog__

#ifdef __cplusplus
extern "C"
{
#endif

#include <rtthread.h>
#define RyanMqttTag ("RyanMqtt")

#ifdef RT_USING_ULOG
#include "ulog.h"
#else
#define ulog_d(TAG, ...)            \
    {                               \
        printf("%s", TAG);          \
        printf(fmt, ##__VA_ARGS__); \
        printf("\r\n");             \
    }
#define ulog_i(TAG, ...)            \
    {                               \
        printf("%s", TAG);          \
        printf(fmt, ##__VA_ARGS__); \
        printf("\r\n");             \
    }
#define ulog_w(TAG, ...)            \
    {                               \
        printf("%s", TAG);          \
        printf(fmt, ##__VA_ARGS__); \
        printf("\r\n");             \
    }
#define ulog_e(TAG, ...)            \
    {                               \
        printf("%s", TAG);          \
        printf(fmt, ##__VA_ARGS__); \
        printf("\r\n");             \
    }
#endif

    // 定义枚举类型

    // 定义结构体类型

    /* extern variables-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif
