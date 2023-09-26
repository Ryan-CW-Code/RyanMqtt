
#include "platformSystem.h"

void *platformMemoryMalloc(size_t size)
{
    return malloc(size);
}

void platformMemoryFree(void *ptr)
{
    free(ptr);
}

void platformPrint(char *str, uint16_t strLen)
{
    printf("%.*s", strLen, str);
}

/**
 * @brief ms延时
 *
 * @param ms
 */
void platformDelay(uint32_t ms)
{
    osDelay(ms);
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
        .name = "mqttMutex"};

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
 * @brief 进入临界区 / 关中断
 *
 */
void platformCriticalEnter(void)
{
    // rt_enter_critical();
    osKernelLock();
}

/**
 * @brief 退出临界区 / 开中断
 *
 */
void platformCriticalExit(void)
{
    // rt_exit_critical();
    osKernelUnlock();
}
