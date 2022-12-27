
#include "platformSystem.h"

void *platformMemoryMalloc(size_t size)
{
    return rt_malloc(size);
}

void platformMemoryFree(void *ptr)
{
    rt_free(ptr);
}

/**
 * @brief ms延时
 *
 * @param ms
 */
void platformDelay(uint32_t ms)
{
    rt_thread_mdelay(ms);
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
    platformThread->thread = rt_thread_create(name,      // 线程name
                                              entry,     // 线程入口函数
                                              param,     // 线程入口函数参数
                                              stackSize, // 线程栈大小
                                              priority,  // 线程优先级
                                              10);       // 线程时间片

    if (RT_NULL == platformThread->thread)
        return RyanMqttNoRescourceError;

    rt_thread_startup(platformThread->thread);
    return RyanMqttSuccessError;
}

/**
 * @brief 销毁指定线程
 *
 * @param userData
 * @param platformThread
 * @return RyanMqttError_e
 */
RyanMqttError_e platformThreadDestroy(void *userData, platformThread_t *platformThread)
{
    rt_thread_delete(platformThread->thread);
    rt_schedule();
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
    rt_thread_resume(platformThread->thread);
    rt_schedule();
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
    rt_thread_suspend(platformThread->thread);
    rt_schedule(); // rtthread挂起线程后应立即调用线程上下文切换函数
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
    platformMutex->mutex = rt_mutex_create("mqttMutex", RT_IPC_FLAG_PRIO);
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
    rt_mutex_delete(platformMutex->mutex);
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
    rt_mutex_take(platformMutex->mutex, RT_WAITING_FOREVER);
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
    rt_mutex_release(platformMutex->mutex);
    return RyanMqttSuccessError;
}

/**
 * @brief 进入临界区 / 关中断
 *
 */
void platformCriticalEnter(void)
{
    rt_enter_critical();
}

/**
 * @brief 退出临界区 / 开中断
 *
 */
void platformCriticalExit(void)
{
    rt_exit_critical();
}