#include "RyanMqttTest.h"

static pthread_spinlock_t spin;
uint32_t destroyCount = 0;

uint32_t randomCount = 0;
uint32_t sendRandomCount = 0;
uint32_t memoryRandomCount = 0;
RyanMqttBool_e isEnableRandomNetworkFault = RyanMqttFalse;
RyanMqttBool_e isEnableRandomMemoryFault = RyanMqttFalse;
void enableRandomNetworkFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomNetworkFault = RyanMqttTrue;
	RyanMqttTestExitCritical();
}

void disableRandomNetworkFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomNetworkFault = RyanMqttFalse;
	RyanMqttTestExitCritical();
}

void toggleRandomNetworkFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomNetworkFault = !isEnableRandomNetworkFault;
	RyanMqttTestExitCritical();
}

void enableRandomMemoryFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomMemoryFault = RyanMqttTrue;
	RyanMqttTestExitCritical();
}

void disableRandomMemoryFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomMemoryFault = RyanMqttFalse;
	RyanMqttTestExitCritical();
}

void toggleRandomMemoryFault(void)
{
	RyanMqttTestEnableCritical();
	isEnableRandomMemoryFault = !isEnableRandomMemoryFault;
	RyanMqttTestExitCritical();
}

uint32_t RyanRand(int32_t min, int32_t max)
{
	static uint32_t isSeed = 0;
	static uint32_t seedp = 0;
	if (isSeed > 1024 || 0 == seedp)
	{
		seedp = platformUptimeMs();
		isSeed = 0;
	}

	if (min >= max)
	{
		return min;
	}
	isSeed++;
	return (rand_r(&seedp) % (max - min + 1)) + min;
}

void RyanMqttTestEnableCritical(void)
{
	pthread_spin_lock(&spin);
}

void RyanMqttTestExitCritical(void)
{
	pthread_spin_unlock(&spin);
}

void printfArrStr(uint8_t *buf, uint32_t len, char *userData)
{
	RyanMqttLog_raw("%s len: %d ", userData, len);
	for (uint32_t i = 0; i < len; i++)
	{
		RyanMqttLog_raw("%c", buf[i]);
	}

	RyanMqttLog_raw("\r\n");
}

void RyanMqttTestUtileInit(void)
{
	pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

	// 多线程测试必须设置这个，否则会导致 heap-use-after-free, 原因如下
	// 虽然也有办法解决，不过RyanMqtt目标为嵌入式场景，不想引入需要更多资源的逻辑，嵌入式场景目前想不到有这么频繁而且还是本机emqx的场景。
	// 用户线程send -> emqx回复报文 -> mqtt线程recv。
	// recv线程收到数据后，会释放用户线程send的sendbuf缓冲区。
	// 但是在本机部署的emqx并且多核心同时运行，发送的数据量非常大的情况下会出现mqtt线程recv已经收到数据，但是用户线程send函数还没有返回。
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

	vallocInit();
}

void RyanMqttTestUtileDeInit(void)
{
	pthread_spin_destroy(&spin);
}
