#ifndef __RyanMqttPlatform__
#define __RyanMqttPlatform__

#ifdef __cplusplus
extern "C" {
#endif

#include "platformSystem.h"
#include "platformNetwork.h"
#include "RyanMqttPublic.h"

#ifndef RyanMqttMemset
#define RyanMqttMemset memset
#endif

#ifndef RyanMqttStrlen
#define RyanMqttStrlen strlen
#endif

#ifndef RyanMqttMemcpy
#define RyanMqttMemcpy memcpy
#endif

#ifndef RyanMqttStrncmp
#define RyanMqttStrncmp strncmp
#endif

#ifndef platformAssert
#define platformAssert assert
#endif

#ifndef RyanMqttSnprintf
#define RyanMqttSnprintf snprintf
#endif

#ifndef RyanMqttVsnprintf
#define RyanMqttVsnprintf vsnprintf
#endif

// RyanMqtt内部 timer 接口
typedef struct
{
	uint32_t time;
	uint32_t timeOut;
} RyanMqttTimer_t;

extern void RyanMqttTimerInit(RyanMqttTimer_t *platformTimer);
extern void RyanMqttTimerCutdown(RyanMqttTimer_t *platformTimer, uint32_t timeout);
extern uint32_t RyanMqttTimerGetConfigTimeout(RyanMqttTimer_t *platformTimer);
extern uint32_t RyanMqttTimerRemain(RyanMqttTimer_t *platformTimer);

// 需用户实现的网络接口
extern RyanMqttError_e platformNetworkInit(void *userData, platformNetwork_t *platformNetwork);
extern RyanMqttError_e platformNetworkDestroy(void *userData, platformNetwork_t *platformNetwork);
extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host,
					      uint16_t port);
extern int32_t platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf,
					size_t recvLen, int32_t timeout);
extern int32_t platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf,
					size_t sendLen, int32_t timeout);
extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

// 需用户实现的内存接口
extern void *platformMemoryMalloc(size_t size);
extern void platformMemoryFree(void *ptr);

// 需用户实现的打印接口
extern void platformPrint(char *str, uint16_t strLen);
// 需用户实现的ms延时接口
extern void platformDelay(uint32_t ms);
// 需用户实现的获取开机ms时间戳接口
extern uint32_t platformUptimeMs(void);

// 需用户实现的 RTOS 接口
extern RyanMqttError_e platformThreadInit(void *userData, platformThread_t *platformThread, const char *name,
					  void (*entry)(void *), void *const param, uint32_t stackSize,
					  uint32_t priority);
extern RyanMqttError_e platformThreadDestroy(void *userData, platformThread_t *platformThread);
extern RyanMqttError_e platformThreadStart(void *userData, platformThread_t *platformThread);
extern RyanMqttError_e platformThreadStop(void *userData, platformThread_t *platformThread);

// 互斥锁需支持递归
extern RyanMqttError_e platformMutexInit(void *userData, platformMutex_t *platformMutex);
extern RyanMqttError_e platformMutexDestroy(void *userData, platformMutex_t *platformMutex);
extern RyanMqttError_e platformMutexLock(void *userData, platformMutex_t *platformMutex);
extern RyanMqttError_e platformMutexUnLock(void *userData, platformMutex_t *platformMutex);

extern RyanMqttError_e platformCriticalInit(void *userData, platformCritical_t *platformCritical);
extern RyanMqttError_e platformCriticalDestroy(void *userData, platformCritical_t *platformCritical);
extern RyanMqttError_e platformCriticalEnter(void *userData, platformCritical_t *platformCritical);
extern RyanMqttError_e platformCriticalExit(void *userData, platformCritical_t *platformCritical);
#ifdef __cplusplus
}
#endif

#endif
