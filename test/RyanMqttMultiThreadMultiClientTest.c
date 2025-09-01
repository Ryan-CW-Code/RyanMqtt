#include "RyanMqttTest.h"

#define MESSAGES_PER_THREAD 1000 // 发送个数
#define CONCURRENT_CLIENTS  20   // 线程数

// 线程测试统计数据
typedef struct
{
	pthread_t threadId;
	int threadIndex;
	int publishedCount;
	int receivedCount;
	RyanMqttClient_t *client;
} ThreadTestData_t;

// 多线程测试控制结构
typedef struct
{
	volatile int runningThreads;
	volatile int totalPublished;
	volatile int totalReceived;
	volatile int testComplete;
} MultiThreadTestControl_t;

static MultiThreadTestControl_t g_testControl = {0};

// 多线程事件处理函数
static void multiThreadEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
	RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
	struct RyanMqttTestEventUserData *eventUserData = (struct RyanMqttTestEventUserData *)client->config.userData;
	ThreadTestData_t *testData = (ThreadTestData_t *)eventUserData->userData;

	switch (event)
	{
	case RyanMqttEventPublished: {
		// RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
		// RyanMqttLog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);

		RyanMqttTestEnableCritical();
		testData->publishedCount += 1;
		g_testControl.totalPublished += 1;
		RyanMqttTestExitCritical();
	}

	break;

	case RyanMqttEventData: {
		// RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
		// RyanMqttLog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
		// 	      msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);

		RyanMqttTestEnableCritical();
		testData->receivedCount += 1;
		g_testControl.totalReceived += 1;
		RyanMqttTestExitCritical();
	}
	break;

	default: mqttEventBaseHandle(pclient, event, eventData); break;
	}
}

// 并发发布测试线程
static void *concurrentPublishThread(void *arg)
{
	ThreadTestData_t *testData = (ThreadTestData_t *)arg;
	RyanMqttError_e result = RyanMqttSuccessError;
	char topic[64];
	char payload[256];

	// 初始化客户端
	result =
		RyanMqttTestInit(&testData->client, RyanMqttTrue, RyanMqttFalse, 120, multiThreadEventHandle, testData);
	if (RyanMqttSuccessError != result)
	{
		RyanMqttLog_e("Thread %d: Failed to initialize client", testData->threadIndex);
		return NULL;
	}

	// 订阅主题
	RyanMqttSnprintf(topic, sizeof(topic), "test/thread/%d", testData->threadIndex);
	result = RyanMqttSubscribe(testData->client, topic, RyanMqttQos2);
	// result = RyanMqttSubscribe(testData->client, topic, testData->threadIndex % 2 ? RyanMqttQos2 : RyanMqttQos1);
	if (RyanMqttSuccessError != result)
	{
		RyanMqttLog_e("Thread %d: Failed to subscribe", testData->threadIndex);
		goto cleanup;
	}

	// 发布消息
	for (int i = 0; i < MESSAGES_PER_THREAD; i++)
	{
		RyanMqttSnprintf(payload, sizeof(payload), "Message %d from thread %d", i, testData->threadIndex);

		result = RyanMqttPublish(testData->client, topic, payload, RyanMqttStrlen(payload),
					 i % 2 ? RyanMqttQos2 : RyanMqttQos1, RyanMqttFalse);
		if (RyanMqttSuccessError != result)
		{
			RyanMqttLog_e("Thread %d: Failed to publish message %d", testData->threadIndex, i);
		}

		delay_us(900);
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

	if (testData->client)
	{
		RyanMqttTestDestroyClient(testData->client);
	}

	return NULL;
}

// 多客户端并发测试
static RyanMqttError_e multiClientConcurrentTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	ThreadTestData_t testThreads[CONCURRENT_CLIENTS];

	RyanMqttLog_i("Starting multi-client concurrent test with %d clients", CONCURRENT_CLIENTS);

	// 初始化测试控制结构
	RyanMqttMemset(&g_testControl, 0, sizeof(g_testControl));
	g_testControl.runningThreads = CONCURRENT_CLIENTS;

	// 创建测试线程
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		RyanMqttMemset(&testThreads[i], 0, sizeof(ThreadTestData_t));
		testThreads[i].threadIndex = i;

		if (pthread_create(&testThreads[i].threadId, NULL, concurrentPublishThread, &testThreads[i]) != 0)
		{
			RyanMqttLog_e("Failed to create thread %d", i);
			result = RyanMqttFailedError;
			goto cleanup;
		}
	}

	// 等待线程结束
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		pthread_join(testThreads[i].threadId, NULL);
	}

	// 统计结果
	RyanMqttLog_i("Multi-client test results:");
	RyanMqttLog_i("  Total published: %d", g_testControl.totalPublished);
	RyanMqttLog_i("  Total received: %d", g_testControl.totalReceived);

	// 详细统计
	for (int i = 0; i < CONCURRENT_CLIENTS; i++)
	{
		RyanMqttLog_i("  Thread %d: Published=%d, Received=%d", i, testThreads[i].publishedCount,
			      testThreads[i].receivedCount);
	}

	// 验证结果
	int expectedTotal = CONCURRENT_CLIENTS * MESSAGES_PER_THREAD;
	if (g_testControl.totalPublished != expectedTotal || g_testControl.totalReceived != expectedTotal)
	{
		RyanMqttLog_e("Test failed: Expected %d published and received, got %d and %d", expectedTotal,
			      g_testControl.totalPublished, g_testControl.totalReceived);
		result = RyanMqttFailedError;
	}

cleanup:

	return result;
}

// 主多线程测试函数
RyanMqttError_e RyanMqttMultiThreadMultiClientTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	// 1. 多客户端并发测试
	result = multiClientConcurrentTest();
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 检查内存泄漏
	checkMemory;

	return result;
}
