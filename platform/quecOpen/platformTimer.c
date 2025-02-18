

#include "platformTimer.h"

uint32_t platformUptimeMs(void)
{
    if (1000 == osKernelGetTickFreq())
        return (uint32_t)osKernelGetTickCount();
    else
    {
        uint32_t tick = 0;
        tick = osKernelGetTickCount() * 1000;
        return (uint32_t)((tick + osKernelGetTickCount() - 1) / osKernelGetTickCount());
    }
}

/**
 * @brief 初始化定时器
 *
 * @param platformTimer
 */
void platformTimerInit(platformTimer_t *platformTimer)
{
    platformTimer->time = 0;
    platformTimer->timeOut = 0;
}

/**
 * @brief 添加计数时间
 *
 * @param platformTimer
 * @param timeout
 */
void platformTimerCutdown(platformTimer_t *platformTimer, uint32_t timeout)
{
    platformTimer->timeOut = timeout;
    platformTimer->time = platformUptimeMs();
}

/**
 * @brief 计算time还有多长时间超时,考虑了32位溢出判断
 *
 * @param platformTimer
 * @return uint32_t 返回剩余时间，超时返回0
 */
uint32_t platformTimerRemain(platformTimer_t *platformTimer)
{
    uint32_t tnow = platformUptimeMs();
    uint32_t overTime = platformTimer->time + platformTimer->timeOut;
    // uint32_t 没有溢出
    if (overTime >= platformTimer->time)
    {
        // tnow溢出,时间必然已经超时
        if (tnow < platformTimer->time)
            // return (UINT32_MAX - overTime + tnow + 1);
            return 0;

        // tnow没有溢出
        return tnow >= overTime ? 0 : (overTime - tnow);
    }

    // uint32_t 溢出了
    // tnow 溢出了
    if (tnow < platformTimer->time)
        return tnow >= overTime ? 0 : (overTime - tnow + 1);

    // tnow 没有溢出
    return UINT32_MAX - tnow + overTime + 1;
}
