

#include "platformTimer.h"

/**
 * @brief 初始化定时器
 *
 * @param platformTimer
 */
void platformTimerInit(platformTimer_t *platformTimer)
{
    platformTimer->time.tv_sec = 0;
    platformTimer->time.tv_usec = 0;
}

/**
 * @brief 添加计数时间
 *
 * @param platformTimer
 * @param timeout
 */
void platformTimerCutdown(platformTimer_t *platformTimer, uint32_t timeout)
{
    struct timeval now = {0};
    gettimeofday(&now, NULL);
    struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};
    timeradd(&now, &interval, &platformTimer->time);
}

/**
 * @brief 计算time还有多长时间超时,考虑了32位溢出判断
 *
 * @param platformTimer
 * @return uint32_t 返回剩余时间，超时返回0
 */
uint32_t platformTimerRemain(platformTimer_t *platformTimer)
{
    struct timeval now = {0}, res = {0};
    gettimeofday(&now, NULL);
    timersub(&platformTimer->time, &now, &res);
    return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}
