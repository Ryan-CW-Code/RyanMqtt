
#include "platformSystem.h"

/**
 * @brief 申请内存
 *
 * @param size
 * @return void*
 */
inline void *platformMemoryMalloc(size_t size)
{
	return malloc(size);
}

/**
 * @brief 释放内存
 *
 * @param ptr
 */
inline void platformMemoryFree(void *ptr)
{
	free(ptr);
}

/**
 * @brief ms延时
 *
 * @param ms
 */
inline void platformDelay(uint32_t ms)
{
	usleep(ms * 1000);
}

uint32_t platformUptimeMs(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
	{
		return 0;
	}
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 打印字符串函数,可通过串口打印出去
 *
 * @param str
 * @param strLen
 */
inline void platformPrint(char *str, uint16_t strLen)
{
	printf("%.*s", strLen, str);
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
RyanMqttError_e platformThreadInit(void *userData, platformThread_t *platformThread, const char *name,
				   void (*entry)(void *), void *const param, uint32_t stackSize, uint32_t priority)
{

	pthread_attr_t attr = {0};
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stackSize);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // 设置为分离状态

	int ret = pthread_create(&platformThread->thread, &attr, (void *)entry, param);
	if (0 != ret)
	{
		return RyanMqttNoRescourceError;
	}

	pthread_mutex_init(&platformThread->mutex, NULL);
	pthread_cond_init(&platformThread->cond, NULL);

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
	pthread_exit(NULL);
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
	pthread_mutex_lock(&platformThread->mutex);
	pthread_cond_signal(&platformThread->cond);
	pthread_mutex_unlock(&platformThread->mutex);
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
	pthread_mutex_lock(&platformThread->mutex);
	pthread_cond_wait(&platformThread->cond, &platformThread->mutex);
	pthread_mutex_unlock(&platformThread->mutex);
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
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&platformMutex->mutex, &attr);
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
	pthread_mutex_destroy(&platformMutex->mutex);
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
	pthread_mutex_lock(&platformMutex->mutex); // 互斥锁上锁
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
	pthread_mutex_unlock(&platformMutex->mutex); // 互斥锁解锁
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
	pthread_spin_init(&platformCritical->spin, PTHREAD_PROCESS_PRIVATE);
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
	pthread_spin_destroy(&platformCritical->spin);
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
	pthread_spin_lock(&platformCritical->spin);
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
	pthread_spin_unlock(&platformCritical->spin);
	return RyanMqttSuccessError;
}
