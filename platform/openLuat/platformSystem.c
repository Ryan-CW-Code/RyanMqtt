
#include "platformSystem.h"

/**
 * @brief 申请内存
 *
 * @param size
 * @return void*
 */
inline void *platformMemoryMalloc(size_t size)
{
    return luat_heap_malloc(size);
}

/**
 * @brief 释放内存
 *
 * @param ptr
 */
inline void platformMemoryFree(void *ptr)
{
    luat_heap_free(ptr);
}

/**
 * @brief ms延时
 *
 * @param ms
 */
inline void platformDelay(uint32_t ms)
{
    osDelay(ms);
}

/**
 * @brief 打印字符串函数,可通过串口打印出去
 *
 * @param str
 * @param strLen
 */
inline void platformPrint(char *str, uint16_t strLen)
{
    luat_debug_print("%.*s", strLen, str);
}

/**
 * @brief 初始化并运行线程
 *
 * @param userData
 * @param platformThread
 * @param name
 * @param entry
 * @param param
 * @param stackSize
 * @param priority
 * @return RyanMqttError_e
 */
RyanMqttError_e platformThreadInit(void *userData,
                                   platformThread_t *platformThread,
                                   const char *name,
                                   void (*entry)(void *),
                                   void *const param,
                                   uint32_t stackSize,
                                   uint32_t priority)
{

    const osThreadAttr_t myTask02_attributes = {
        .name = name,
        .stack_size = stackSize,
        .priority = (osPriority_t)priority,
    };

    platformThread->thread = osThreadNew(entry, param, &myTask02_attributes);

    if (NULL == platformThread->thread)
        return RyanMqttNoRescourceError;

    return RyanMqttSuccessError;
}

/**
 * @brief 销毁自身线程
 *
 * @param userData
 * @param platformThread
 * @return RyanMqttError_e
 */
RyanMqttError_e platformThreadDestroy(void *userData, platformThread_t *platformThread)
{
    osThreadExit();
    return RyanMqttSuccessError;
}

/**
 * @brief 开启线程
 *
 * @param userData
 * @param platformThread
 * @return RyanMqttError_e
 */
RyanMqttError_e platformThreadStart(void *userData, platformThread_t *platformThread)
{
    osThreadResume(platformThread->thread);
    return RyanMqttSuccessError;
}

/**
 * @brief 挂起线程
 *
 * @param userData
 * @param platformThread
 * @return RyanMqttError_e
 */
RyanMqttError_e platformThreadStop(void *userData, platformThread_t *platformThread)
{
    osThreadSuspend(platformThread->thread);
    return RyanMqttSuccessError;
}

/**
 * @brief 互斥锁初始化
 *
 * @param userData
 * @param platformMutex
 * @return RyanMqttError_e
 */
RyanMqttError_e platformMutexInit(void *userData, platformMutex_t *platformMutex)
{

    const osMutexAttr_t myMutex01_attributes = {
        .name = "mqttMutex", .attr_bits = osMutexRecursive | osMutexPrioInherit | osMutexRobust};

    platformMutex->mutex = osMutexNew(&myMutex01_attributes);
    return RyanMqttSuccessError;
}

/**
 * @brief 销毁互斥锁
 *
 * @param userData
 * @param platformMutex
 * @return RyanMqttError_e
 */
RyanMqttError_e platformMutexDestroy(void *userData, platformMutex_t *platformMutex)
{
    osMutexDelete(platformMutex->mutex);
    return RyanMqttSuccessError;
}

/**
 * @brief 阻塞获取互斥锁
 *
 * @param userData
 * @param platformMutex
 * @return RyanMqttError_e
 */
RyanMqttError_e platformMutexLock(void *userData, platformMutex_t *platformMutex)
{
    osMutexAcquire(platformMutex->mutex, osWaitForever);
    return RyanMqttSuccessError;
}

/**
 * @brief 释放互斥锁
 *
 * @param userData
 * @param platformMutex
 * @return RyanMqttError_e
 */
RyanMqttError_e platformMutexUnLock(void *userData, platformMutex_t *platformMutex)
{
    osMutexRelease(platformMutex->mutex);
    return RyanMqttSuccessError;
}

/**
 * @brief 临界区初始化
 *
 * @param userData
 * @param platformCritical
 * @return RyanMqttError_e
 */
RyanMqttError_e platformCriticalInit(void *userData, platformCritical_t *platformCritical)
{
    return RyanMqttSuccessError;
}

/**
 * @brief 销毁临界区
 *
 * @param userData
 * @param platformCritical
 * @return RyanMqttError_e
 */
RyanMqttError_e platformCriticalDestroy(void *userData, platformCritical_t *platformCritical)
{
    return RyanMqttSuccessError;
}

/**
 * @brief 进入临界区
 *
 * @param userData
 * @param platformCritical
 * @return RyanMqttError_e
 */
inline RyanMqttError_e platformCriticalEnter(void *userData, platformCritical_t *platformCritical)
{
    osKernelLock();
    return RyanMqttSuccessError;
}

/**
 * @brief 退出临界区
 *
 * @param userData
 * @param platformCritical
 * @return RyanMqttError_e
 */
inline RyanMqttError_e platformCriticalExit(void *userData, platformCritical_t *platformCritical)
{
    osKernelUnlock();
    return RyanMqttSuccessError;
}
