#ifndef __platformTimer__
#define __platformTimer__
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>

    typedef struct
    {
        struct timeval time;
    } platformTimer_t;

    extern void platformTimerInit(platformTimer_t *platformTimer);
    extern void platformTimerCutdown(platformTimer_t *platformTimer, uint32_t timeout);
    extern uint32_t platformTimerRemain(platformTimer_t *platformTimer);

#ifdef __cplusplus
}
#endif

#endif
