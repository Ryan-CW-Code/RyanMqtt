#include "RyanMqttTest.h"

#define MESSAGES_PER_THREAD 1000 // 发送个数
#define CONCURRENT_CLIENTS  20   // 线程数

// 线程测试统计数据
typedef struct
{
	int threadIndex;
	int publishedCount;
	int receivedCount;
	pthread_attr_t attr;
	pthread_t threadId;
} ThreadTestData_t;

// 多线程测试控制结构
typedef struct
{
	int32_t totalPublished;
	int32_t totalReceived;
	int32_t threadIndex;
	int32_t testComplete;
	RyanMqttClient_t *client;
} MultiThreadTestControl_t;

static MultiThreadTestControl_t g_testControl = {0};
static ThreadTestData_t g_threadTestData[CONCURRENT_CLIENTS + 1] = {0};

static bool safeParseTopic(const char *topic, uint32_t topicLen, int *threadId)
{
	const char *prefix = "testThread/";
	const char *suffix = "/tttt";

	size_t prefix_len = strlen(prefix);
	size_t suffix_len = strlen(suffix);

	if (topicLen <= prefix_len + suffix_len)
	{
		return false;
	}
	if (strncmp(topic, prefix, prefix_len) != 0)
	{
		return false;
	}
	if (topicLen < prefix_len + suffix_len)
	{
		return false;
	}
	if (strncmp(topic + topicLen - suffix_len, suffix, suffix_len) != 0)
	{
		return false;
	}

	size_t num_len = topicLen - prefix_len - suffix_len;
	if (num_len == 0 || num_len >= 16)
	{
		return false;
	}

	char num_buf[16] = {0};
	memcpy(num_buf, topic + prefix_len, num_len);

	char *endptr = NULL;
	long val = strtol(num_buf, &endptr, 10);
	if (*endptr != '\0')
	{
		return false;
	}

	*threadId = (int)val;
	return true;
}

// 多线程事件处理函数
static void multiThreadEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
	switch (event)
	{
	case RyanMqttEventPublished: {
		RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
		// RyanMqttLog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);

		int threadId;

		// NOLINTNEXTLINE(cert-err34-c)
		// if (1 == sscanf(msgHandler->topic, "testThread/%d/tttt", &threadId))
		if (safeParseTopic(msgHandler->topic, msgHandler->topicLen, &threadId))
		{

			RyanMqttTestEnableCritical();
			ThreadTestData_t *testData = &g_threadTestData[threadId];
			testData->publishedCount += 1;
			g_testControl.totalPublished += 1;
			RyanMqttTestExitCritical();
		}
	}

	break;

	case RyanMqttEventData: {
		RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
		// RyanMqttLog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
		// 	      msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);

		int threadId;

		// 非线程安全
		// if (1 == sscanf(msgData->topic, "testThread/%d/tttt", &threadId))
		if (safeParseTopic(msgData->topic, msgData->topicLen, &threadId))
		{
			RyanMqttTestEnableCritical();
			ThreadTestData_t *testData = &g_threadTestData[threadId];
			testData->receivedCount += 1;
			g_testControl.totalReceived += 1;
			RyanMqttTestExitCritical();
		}
	}
	break;

	default: mqttEventBaseHandle(pclient, event, eventData); break;
	}
}

// 并发发布测试线程
static void *concurrentPublishThread(void *arg)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	char topic[64];
	char payload[256];
	int32_t threadIndex = 0;

	RyanMqttTestEnableCritical();
	threadIndex = g_testControl.threadIndex;
	g_testControl.threadIndex += 1;
	RyanMqttTestExitCritical();

	ThreadTestData_t *testData = &g_threadTestData[threadIndex];

	// 订阅主题
	RyanMqttSnprintf(topic, sizeof(topic), "testThread/%d/tttt", threadIndex);
	result = RyanMqttSubscribe(g_testControl.client, topic, threadIndex % 2 ? RyanMqttQos2 : RyanMqttQos1);
	if (RyanMqttSuccessError != result)
	{
		RyanMqttLog_e("Thread %d: Failed to subscribe", threadIndex);
		goto cleanup;
	}

	// 发布消息
	for (int i = 0; i < MESSAGES_PER_THREAD; i++)
	{
		RyanMqttSnprintf(payload, sizeof(payload), "M %d %d", i, threadIndex);
		RyanMqttQos_e qos = (RyanMqttQos_e)(i % 3);

		result = RyanMqttPublish(g_testControl.client, topic, payload, RyanMqttStrlen(payload), qos,
					 RyanMqttFalse);
		if (RyanMqttSuccessError != result)
		{
			RyanMqttLog_e("Thread %d: Failed to publish message %d", threadIndex, i);
		}
		else
		{
			if (RyanMqttQos0 == qos)
			{
				RyanMqttTestEnableCritical();
				testData->publishedCount += 1;
				g_testControl.totalPublished += 1;
				RyanMqttTestExitCritical();
			}
		}

		delay_us(1100); // 电脑配置不一样需要的时间也就不一样
	}

	// 等待消息处理完成
	int timeoutCount = 0;
	while (testData->publishedCount < MESSAGES_PER_THREAD && testData->receivedCount < MESSAGES_PER_THREAD)
	{
		delay(10);

		// 10秒超时
		timeoutCount++;
		if (timeoutCount > 1000)
		{
			RyanMqttLog_w("Thread %d: Timeout waiting for messages %d, %d", testData->threadIndex,
				      testData->publishedCount, testData->receivedCount);
			break;
		}
	}

cleanup:
	delay(50); // 让mqtt线程运行

	return NULL;
}

// 多客户端并发测试
static RyanMqttError_e multiClientConcurrentTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttLog_i("Starting multi-client concurrent test with %d clients", CONCURRENT_CLIENTS);

	// 初始化测试控制结构
	RyanMqttMemset(&g_testControl, 0, sizeof(g_testControl));

	// 初始化客户端
	result =
		RyanMqttTestInit(&g_testControl.client, RyanMqttTrue, RyanMqttFalse, 120, multiThreadEventHandle, NULL);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 创建测试线程
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		// struct sched_param param;

		// pthread_attr_init(&g_threadTestData[i].attr);

		// // 设置调度策略为实时策略
		// pthread_attr_setschedpolicy(&g_threadTestData[i].attr, SCHED_FIFO);

		// // 获取该策略的最大优先级
		// int max_prio = sched_get_priority_max(SCHED_FIFO);
		// param.sched_priority = max_prio;

		// // 设置优先级
		// pthread_attr_setschedparam(&g_threadTestData[i].attr, &param);

		int result222 = pthread_create(&g_threadTestData[i].threadId, NULL, concurrentPublishThread, NULL);
		// pthread_attr_destroy(&g_threadTestData[i].attr);
		if (result222 != 0)
		{

			RyanMqttLog_e("Failed to create thread %d", i);
			result = RyanMqttFailedError;
			goto cleanup;
		}
	}

	// 等待线程结束
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		pthread_join(g_threadTestData[i].threadId, NULL);
	}

	// 统计结果
	RyanMqttLog_i("Multi-client test results:");
	RyanMqttLog_i("  Total published: %d", g_testControl.totalPublished);
	RyanMqttLog_i("  Total received: %d", g_testControl.totalReceived);

	// 详细统计
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		RyanMqttLog_i("  Thread %d: Published=%d, Received=%d", i, g_threadTestData[i].publishedCount,
			      g_threadTestData[i].receivedCount);
	}

	// 验证结果
	int expectedTotal = CONCURRENT_CLIENTS * MESSAGES_PER_THREAD;
	if (g_testControl.totalPublished != expectedTotal || g_testControl.totalReceived != expectedTotal)
	{
		RyanMqttLog_e("Test failed: Expected %d published and received, got %d and %d", expectedTotal,
			      g_testControl.totalPublished, g_testControl.totalReceived);
		result = RyanMqttFailedError;
	}

	RyanMqttTestDestroyClient(g_testControl.client);

cleanup:
	return result;
}

// 主多线程测试函数
RyanMqttError_e RyanMqttMultiThreadSafetyTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	// 1. 多客户端并发测试
	result = multiClientConcurrentTest();
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 检查内存泄漏
	checkMemory;

	return result;
}
