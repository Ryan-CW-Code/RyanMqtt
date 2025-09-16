#ifndef __RyanMqttTestUtile__
#define __RyanMqttTestUtile__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include "valloc.h"
#define malloc  v_malloc
#define calloc  v_calloc
#define free    v_free
#define realloc v_realloc

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtil.h"

#define delay(ms)         usleep((ms) * 1000)
#define delay_us(us)      usleep((us))
#define getArraySize(arr) ((int32_t)(sizeof(arr) / sizeof((arr)[0])))
extern uint32_t destroyCount;

#define checkMemory                                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		for (uint32_t aaa = 0;; aaa++)                                                                         \
		{                                                                                                      \
			RyanMqttTestEnableCritical();                                                                  \
			uint32_t destoryCount2 = destroyCount;                                                         \
			RyanMqttTestExitCritical();                                                                    \
			if (0 == destoryCount2) break;                                                                 \
			if (aaa > 10 * 1000)                                                                           \
			{                                                                                              \
				printf("aaaaaaaa %d\r\n", destoryCount2);                                              \
				break;                                                                                 \
			}                                                                                              \
			delay(1);                                                                                      \
		}                                                                                                      \
		int area = 0, use = 0;                                                                                 \
		v_mcheck(&area, &use);                                                                                 \
		if (area != 0 || use != 0)                                                                             \
		{                                                                                                      \
			RyanMqttLog_e("内存泄漏");                                                                     \
			while (1)                                                                                      \
			{                                                                                              \
				v_mcheck(&area, &use);                                                                 \
				RyanMqttLog_e("|||----------->>> area = %d, size = %d", area, use);                    \
				delay(3000);                                                                           \
			}                                                                                              \
		}                                                                                                      \
	} while (0)

extern uint32_t randomCount;
extern uint32_t sendRandomCount;
extern uint32_t memoryRandomCount;
extern RyanMqttBool_e isEnableRandomNetworkFault;
extern RyanMqttBool_e isEnableRandomMemoryFault;
extern void enableRandomNetworkFault(void);
extern void disableRandomNetworkFault(void);
extern void toggleRandomNetworkFault(void);
extern void enableRandomMemoryFault(void);
extern void disableRandomMemoryFault(void);
extern void toggleRandomMemoryFault(void);
extern uint32_t RyanRand(int32_t min, int32_t max);

// 定义枚举类型

// 定义结构体类型
#define RyanMqttTestEventUserDataMagic (123456789)

/* extern variables-----------------------------------------------------------*/

extern void RyanMqttTestEnableCritical(void);
extern void RyanMqttTestExitCritical(void);
extern void printfArrStr(uint8_t *buf, uint32_t len, char *userData);

extern void RyanMqttTestUtileInit(void);
extern void RyanMqttTestUtileDeInit(void);

#ifdef __cplusplus
}
#endif

#endif
