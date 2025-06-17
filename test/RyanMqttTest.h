#ifndef __RyanMqttTest__
#define __RyanMqttTest__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#define rlogLevel (rlogLvlDebug) // 日志打印等级

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"

#define RyanMqttClientId ("RyanMqttTest888") // 填写mqtt客户端id，要求唯一
// #define RyanMqttHost ("127.0.0.1")           // 填写你的mqtt服务器ip
#define RyanMqttHost ("localhost")           // 填写你的mqtt服务器ip
#define RyanMqttPort (1883)                  // mqtt服务器端口
#define RyanMqttUserName ("test")            // 填写你的用户名,没有填NULL
#define RyanMqttPassword ("test")            // 填写你的密码,没有填NULL

#define delay(ms) usleep((ms) * 1000)
#define getArraySize(arr) ((int32_t)(sizeof(arr) / sizeof((arr)[0])))
#define checkMemory                                                          \
    do                                                                       \
    {                                                                        \
        int area = 0, use = 0;                                               \
        v_mcheck(&area, &use);                                               \
        if (area != 0 || use != 0)                                           \
        {                                                                    \
            rlog_e("内存泄漏");                                              \
            while (1)                                                        \
            {                                                                \
                int area = 0, use = 0;                                       \
                v_mcheck(&area, &use);                                       \
                rlog_w("|||----------->>> area = %d, size = %d", area, use); \
                delay(3000);                                                 \
            }                                                                \
        }                                                                    \
    } while (0)

    // 定义枚举类型

    // 定义结构体类型

    /* extern variables-----------------------------------------------------------*/

    extern RyanMqttError_e RyanMqttInitSync(RyanMqttClient_t **client, RyanMqttBool_e syncFlag, RyanMqttEventHandle mqttEventCallback);
    extern RyanMqttError_e RyanMqttDestorySync(RyanMqttClient_t *client);
    extern void mqttEventBaseHandle(void *pclient, RyanMqttEventId_e event, const void *eventData);
    extern RyanMqttError_e checkAckList(RyanMqttClient_t *client);
    extern void printfArrStr(uint8_t *buf, uint32_t len, char *userData);

    extern RyanMqttError_e RyanMqttDestoryTest();
    extern RyanMqttError_e RyanMqttKeepAliveTest();
    extern RyanMqttError_e RyanMqttPubTest();
    extern RyanMqttError_e RyanMqttReconnectTest();
    extern RyanMqttError_e RyanMqttSubTest();

#ifdef __cplusplus
}
#endif

#endif
