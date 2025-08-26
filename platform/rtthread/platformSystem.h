#ifndef __platformSystem__
#define __platformSystem__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rtthread.h>

#define platformAssert(EX) RT_ASSERT(EX)
#define RyanMqttMemset     rt_memset
#define RyanMqttStrlen     rt_strlen
#define RyanMqttMemcpy     rt_memcpy
#define RyanMqttStrcmp     rt_strcmp

typedef struct
{
	rt_thread_t thread;
} platformThread_t;

typedef struct
{
	struct rt_mutex mutex;
} platformMutex_t;

typedef struct
{
	rt_base_t level;
} platformCritical_t;

#ifdef __cplusplus
}
#endif

#endif
