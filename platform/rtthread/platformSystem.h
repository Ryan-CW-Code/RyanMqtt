
#ifndef __platformSystem__
#define __platformSystem__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <rtthread.h>
#include "RyanMqttPublic.h"

#ifdef RT_ASSERT
#define RyanMqttAssert(EX) RT_ASSERT(EX)
#else
#define RyanMqttAssert(EX) assert(EX)
#endif

    typedef struct
    {
        rt_thread_t thread;
    } platformThread_t;

    typedef struct
    {
        rt_mutex_t mutex;
    } platformMutex_t;

    extern void *platformMemoryMalloc(size_t size);
    extern void platformMemoryFree(void *ptr);

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

    extern void platformCriticalEnter(void);
    extern void platformCriticalExit(void);

#ifdef __cplusplus
}
#endif

#endif
