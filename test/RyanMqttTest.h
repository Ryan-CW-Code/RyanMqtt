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
#define malloc  v_malloc
#define calloc  v_calloc
#define free    v_free
#define realloc v_realloc

#ifndef RyanMqttLogLevel
#define RyanMqttLogLevel (RyanMqttLogLevelError) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级
#endif

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtil.h"

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

#define delay(ms)         usleep((ms) * 1000)
#define delay_us(us)      usleep((us))
#define getArraySize(arr) ((int32_t)(sizeof(arr) / sizeof((arr)[0])))
extern uint32_t destoryCount;

#define checkMemory                                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		for (uint32_t aaa = 0;; aaa++)                                                                         \
		{                                                                                                      \
			RyanMqttTestEnableCritical();                                                                  \
			uint32_t destoryCount2 = destoryCount;                                                         \
			RyanMqttTestExitCritical();                                                                    \
			if (0 == destoryCount2) break;                                                                 \
			if (aaa > 10 * 1000)                                                                           \
			{                                                                                              \
				printf("aaaaaaaa %d\r\n", destoryCount2);                                              \
				break;                                                                                 \
			}                                                                                              \
			delay(1);                                                                                      \
		}                                                                                                      \
		int area = 0, use = 0;                                                                                 \
		v_mcheck(&area, &use);                                                                                 \
		if (area != 0 || use != 0)                                                                             \
		{                                                                                                      \
			RyanMqttLog_e("内存泄漏");                                                                     \
			while (1)                                                                                      \
			{                                                                                              \
				v_mcheck(&area, &use);                                                                 \
				RyanMqttLog_e("|||----------->>> area = %d, size = %d", area, use);                    \
				delay(3000);                                                                           \
			}                                                                                              \
		}                                                                                                      \
	} while (0)

extern uint32_t randomCount;
extern uint32_t sendRandomCount;
extern RyanMqttBool_e isEnableRandomNetworkFault;
extern void enableRandomNetworkFault(void);
extern void disableRandomNetworkFault(void);
extern void toggleRandomNetworkFault(void);
extern uint32_t RyanRand(int32_t min, int32_t max);

// 定义枚举类型

// 定义结构体类型
#define RyanMqttTestEventUserDataMagic (123456789)
struct RyanMqttTestEventUserData
{
	uint32_t magic; // 防止野指针
	RyanMqttBool_e syncFlag;
	sem_t sem;
	void *userData;
};
/* extern variables-----------------------------------------------------------*/

extern void RyanMqttTestEnableCritical(void);
extern void RyanMqttTestExitCritical(void);
extern RyanMqttError_e RyanMqttTestInit(RyanMqttClient_t **client, RyanMqttBool_e syncFlag,
					RyanMqttBool_e autoReconnectFlag, uint16_t keepaliveTimeoutS,
					RyanMqttEventHandle mqttEventCallback, void *userData);
extern RyanMqttError_e RyanMqttTestDestroyClient(RyanMqttClient_t *client);
extern void mqttEventBaseHandle(void *pclient, RyanMqttEventId_e event, const void *eventData);
extern RyanMqttError_e checkAckList(RyanMqttClient_t *client);
extern void printfArrStr(uint8_t *buf, uint32_t len, char *userData);

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

#ifdef __cplusplus
}
#endif

#endif
