#ifndef __platformTimer__
#define __platformTimer__
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include "luat_mcu.h"

    typedef struct
    {
        uint32_t time;
        uint32_t timeOut;
    } platformTimer_t;

    extern uint32_t platformUptimeMs(void);
    extern void platformTimerInit(platformTimer_t *platformTimer);
    extern void platformTimerCutdown(platformTimer_t *platformTimer, uint32_t timeout);
    extern uint32_t platformTimerRemain(platformTimer_t *platformTimer);

#ifdef __cplusplus
}
#endif

#endif
