#ifndef __RyanMqttTest__
#define __RyanMqttTest__

#ifdef __cplusplus
extern "C" {
#endif

#define _GNU_SOURCE // 必须放在所有头文件之前
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include "valloc.h"

#ifndef RyanMqttLogLevel
#define RyanMqttLogLevel (RyanMqttLogLevelError) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级
#endif

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtil.h"

#include "RyanMqttTestUtile.h"

#define RyanMqttClientId ("RyanMqttTest888") // 填写mqtt客户端id，要求唯一
// #define RyanMqttHost ("127.0.0.1")           // 填写你的mqtt服务器ip
#define RyanMqttHost     ("localhost") // 填写你的mqtt服务器ip
#define RyanMqttPort     (1883)        // mqtt服务器端口
#define RyanMqttUserName ("test")      // 填写你的用户名,没有填NULL
#define RyanMqttPassword ("test")      // 填写你的密码,没有填NULL
// #define RyanMqttUserName (NULL) // 填写你的用户名,没有填NULL
// #define RyanMqttPassword (NULL) // 填写你的密码,没有填NULL

#define RyanMqttReconnectTimeout (3000) // 重连间隔时间，单位ms
#define RyanMqttRecvTimeout      (2000) // 接收数据超时时间，单位ms
#define RyanMqttSendTimeout      (1800) // 发送数据超时时间，单位ms
#define RyanMqttAckTimeout       (3000) // 等待ack超时时间，单位ms

// 定义枚举类型

// 定义结构体类型
struct RyanMqttTestEventUserData
{
	uint32_t magic; // 防止野指针
	RyanMqttBool_e syncFlag;
	sem_t sem;
	void *userData;
};
/* extern variables-----------------------------------------------------------*/
extern RyanMqttError_e RyanMqttTestInit(RyanMqttClient_t **client, RyanMqttBool_e syncFlag,
					RyanMqttBool_e autoReconnectFlag, uint16_t keepaliveTimeoutS,
					RyanMqttEventHandle mqttEventCallback, void *userData);
extern RyanMqttError_e RyanMqttTestDestroyClient(RyanMqttClient_t *client);
extern void mqttEventBaseHandle(void *pclient, RyanMqttEventId_e event, const void *eventData);
extern RyanMqttError_e checkAckList(RyanMqttClient_t *client);

extern RyanMqttError_e RyanMqttDestroyTest(void);
extern RyanMqttError_e RyanMqttKeepAliveTest(void);
extern RyanMqttError_e RyanMqttPubTest(void);
extern RyanMqttError_e RyanMqttReconnectTest(void);
extern RyanMqttError_e RyanMqttSubTest(void);
extern RyanMqttError_e RyanMqttWildcardTest(void);
extern RyanMqttError_e RyanMqttMultiThreadMultiClientTest(void);
extern RyanMqttError_e RyanMqttMultiThreadSafetyTest(void);
extern RyanMqttError_e RyanMqttPublicApiParamCheckTest(void);
extern RyanMqttError_e RyanMqttNetworkFaultToleranceMemoryTest(void);
extern RyanMqttError_e RyanMqttNetworkFaultQosResilienceTest(void);
extern RyanMqttError_e RyanMqttMemoryFaultToleranceTest(void);

#ifdef __cplusplus
}
#endif

#endif
