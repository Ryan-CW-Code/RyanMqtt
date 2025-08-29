#ifndef __platformSystem__
#define __platformSystem__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "luat_debug.h"
#include "luat_malloc.h"
#include "luat_rtos.h"
#include "luat_mcu.h"

#define platformAssert(EX) LUAT_DEBUG_ASSERT(EX)
#define RyanMqttMemset     memset
#define RyanMqttStrlen     strlen
#define RyanMqttMemcpy     memcpy
#define RyanMqttStrncmp    strncmp
#define RyanMqttSnprintf   snprintf
#define RyanMqttVsnprintf  vsnprintf

typedef struct
{
	luat_rtos_task_handle thread;
} platformThread_t;

typedef struct
{
	luat_rtos_mutex_t mutex;
} platformMutex_t;

typedef struct
{
	uint32_t level;
} platformCritical_t;

#ifdef __cplusplus
}
#endif

#endif
