
#ifndef __platformSystem__
#define __platformSystem__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "cmsis_os2.h"
#include "ql_uart.h"
#include "RyanMqttPublic.h"

#define RyanMqttAssert(EX) assert(EX)

    typedef struct
    {
        osThreadId_t thread;
    } platformThread_t;

    typedef struct
    {
        osMutexId_t mutex;
    } platformMutex_t;

    typedef struct
    {
        uint8_t invalid; // 不使用，避免报错
    } platformCritical_t;

    extern void *platformMemoryMalloc(size_t size);
    extern void platformMemoryFree(void *ptr);

    extern void platformPrint(char *str, uint16_t strLen);
    extern void platformDelay(uint32_t ms);

    extern RyanMqttError_e platformThreadInit(void *userData,
                                              platformThread_t *platformThread,
                                              const char *name,
                                              void (*entry)(void *),
                                              void *const param,
                                              uint32_t stackSize,
                                              uint32_t priority);
    extern RyanMqttError_e platformThreadDestroy(void *userData, platformThread_t *platformThread);
    extern RyanMqttError_e platformThreadStart(void *userData, platformThread_t *platformThread);
    extern RyanMqttError_e platformThreadStop(void *userData, platformThread_t *platformThread);

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
