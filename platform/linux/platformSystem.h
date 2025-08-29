#ifndef __platformSystem__
#define __platformSystem__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "valloc.h"

#define platformAssert(EX) assert(EX)
#define RyanMqttMemset     memset
#define RyanMqttStrlen     strlen
#define RyanMqttMemcpy     memcpy
#define RyanMqttStrncmp    strncmp
#define RyanMqttSnprintf   snprintf
#define RyanMqttVsnprintf  vsnprintf

typedef struct
{
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} platformThread_t;

typedef struct
{
	pthread_mutex_t mutex;
} platformMutex_t;

typedef struct
{
	pthread_spinlock_t spin;
} platformCritical_t;

#ifdef __cplusplus
}
#endif

#endif
