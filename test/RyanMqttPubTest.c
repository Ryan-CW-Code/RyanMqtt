#include "RyanMqttTest.h"

static int32_t pubTestPublishedEventCount = 0;
static int32_t pubTestDataEventCount = 0;
static char *pubStr = NULL;
static int32_t pubStrLen = 0;
static RyanMqttQos_e exportQos = RyanMqttSubFail;

static int32_t pubTestDataEventCountNotQos0 = 0;

static void RyanMqttPublishEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
	RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
	switch (event)
	{
	case RyanMqttEventPublished: {
		RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
		RyanMqttLog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		RyanMqttQos_e qos = (RyanMqttQos_e)(uintptr_t)msgHandler->userData;
		if (qos == msgHandler->qos)
		{
			pubTestPublishedEventCount++;
		}
		else
		{
			RyanMqttLog_e("pub测试发送的qos不一致 msgQos: %d, userDataQos: %d", msgHandler->qos, qos);
			RyanMqttTestDestroyClient(client);
			return;
		}
		break;
	}

	case RyanMqttEventData: {
		RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
		// RyanMqttLog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
		//        msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);
		if (RyanMqttSubFail != exportQos && exportQos != msgData->qos)
		{
			RyanMqttLog_e("pub测试收到qos等级不一致 exportQos: %d, msgQos: %d", exportQos, msgData->qos);
			RyanMqttTestDestroyClient(client);
			return;
		}

		if ((msgData->payloadLen == (uint32_t)pubStrLen) && (0 == memcmp(msgData->payload, pubStr, pubStrLen)))
		{
			pubTestDataEventCount++;

			if (RyanMqttQos0 != msgData->qos)
			{
				pubTestDataEventCountNotQos0++;
			}
		}
		else
		{
			RyanMqttLog_e("pub测试收到数据不一致 %.*s", msgData->payloadLen, msgData->payload);
			RyanMqttTestDestroyClient(client);
			return;
		}

		break;
	}

	default: mqttEventBaseHandle(pclient, event, eventData); break;
	}
}

/**
 * @brief 固定qos等级发布测试
 *
 * @param qos
 * @param count
 * @param delayms
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPublishTest(RyanMqttQos_e qos, int32_t count, uint32_t delayms)
{
#define RyanMqttPubTestPubTopic "testlinux/aa/pub"
#define RyanMqttPubTestSubTopic "testlinux/+/pub"
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	time_t timeStampNow = 0;

	exportQos = qos;
	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, RyanMqttPublishEventHandle, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 等待订阅主题成功
	result = RyanMqttSubscribe(client, RyanMqttPubTestSubTopic, qos);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	// !不等待topic订阅成功，检查是否正常接收消息
	// for (int32_t i = 0;; i++)
	// {
	// 	int32_t subscribeTotal = 0;

	// 	result = RyanMqttGetSubscribeTotalCount(client, &subscribeTotal);
	// 	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
	// 				  { goto __exit; });
	// 	if (1 == subscribeTotal)
	// 	{
	// 		break;
	// 	}

	// 	if (i > 3000)
	// 	{
	// 		RyanMqttLog_e("订阅主题失败");
	// 		result = RyanMqttFailedError;
	// 		goto __exit;
	// 	}

	// 	delay(1);
	// }

	// 生成随机的数据包大小
	{
		// NOLINTNEXTLINE(cert-err33-c)
		time(&timeStampNow);

		pubStr = (char *)platformMemoryMalloc(2048);
		RyanMqttCheck(NULL != pubStr, RyanMqttNotEnoughMemError, RyanMqttLog_e);
		RyanMqttMemset(pubStr, 0, 2048);

		// NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp)
		srand(timeStampNow);
		// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-msc30-c,cert-msc50-cpp)
		for (uint32_t i = 0; i < (uint32_t)(rand() % 250 + 1 + 100); i++)
		{
			// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-msc30-c,cert-msc50-cpp)
			snprintf(pubStr + 4 * i, 2048 - 4 * i, "%04d", rand());
		}
		pubStrLen = (int32_t)RyanMqttStrlen(pubStr);
	}

	// 开始测试发送数据
	pubTestPublishedEventCount = 0;
	pubTestDataEventCount = 0;
	for (int32_t i = 0; i < count; i++)
	{
		char *pubTopic = RyanMqttPubTestPubTopic;
		result = RyanMqttPublishAndUserData(client, pubTopic, RyanMqttStrlen(pubTopic), pubStr, pubStrLen, qos,
						    RyanMqttFalse,
						    // NOLINTNEXTLINE(performance-no-int-to-ptr)
						    (void *)qos);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		if (delayms)
		{
			delay(delayms);
		}
	}

	// 检查收到的数据是否正确
	for (int32_t i = 0;; i++)
	{
		if (RyanMqttQos0 == qos)
		{
			if (count == pubTestDataEventCount)
			{
				break;
			}
		}
		// qos1和qos2的发送成功数和接收数应该相等,因为是本机回环测试，理论上应该一致
		else if (pubTestPublishedEventCount == count && pubTestPublishedEventCount == pubTestDataEventCount)
		{
			break;
		}

		if (i > 300)
		{
			RyanMqttLog_e("QOS测试失败 Qos: %d, PublishedEventCount: %d, dataEventCount: %d", qos,
				      pubTestPublishedEventCount, pubTestDataEventCount);
			result = RyanMqttFailedError;
			goto __exit;
		}

		delay(100);
	}

	result = RyanMqttUnSubscribe(client, RyanMqttPubTestSubTopic);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	result = checkAckList(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

__exit:
	platformMemoryFree(pubStr);
	pubStr = NULL;
	RyanMqttLog_i("mqtt 发布测试，销毁mqtt客户端");
	RyanMqttTestDestroyClient(client);
	return result;
}

/**
 * @brief 混乱随机的qos消息测试
 *
 * @param count
 * @param delayms
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPublishHybridTest(int32_t count, uint32_t delayms)
{

#define RyanMqttPubHybridTestPubTopic "testlinux/aa/pub/adfa/kkk"
#define RyanMqttPubHybridTestSubTopic "testlinux/aa/+/#"

	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	time_t timeStampNow = 0;
	int32_t sendNeedAckCount = 0;

	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, RyanMqttPublishEventHandle, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 等待订阅主题成功
	result = RyanMqttSubscribe(client, RyanMqttPubHybridTestSubTopic, RyanMqttQos2);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	for (int32_t i = 0;; i++)
	{
		int32_t subscribeTotal = 0;

		result = RyanMqttGetSubscribeTotalCount(client, &subscribeTotal);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });
		if (1 == subscribeTotal)
		{
			break;
		}

		if (i > 3000)
		{
			RyanMqttLog_e("订阅主题失败");
			result = RyanMqttFailedError;
			goto __exit;
		}

		delay(1);
	}

	exportQos = RyanMqttSubFail;

	// 生成随机的数据包大小
	{
		// NOLINTNEXTLINE(cert-err33-c)
		time(&timeStampNow);

		pubStr = (char *)platformMemoryMalloc(2048);
		RyanMqttCheck(NULL != pubStr, RyanMqttNotEnoughMemError, RyanMqttLog_e);
		RyanMqttMemset(pubStr, 0, 2048);

		// NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp)
		srand(timeStampNow);
		// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-msc30-c,cert-msc50-cpp)
		for (uint32_t i = 0; i < (uint32_t)(rand() % 250 + 1 + 100); i++)
		{
			// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-msc30-c,cert-msc50-cpp)
			snprintf(pubStr + 4 * i, 2048 - 4 * i, "%04d", rand());
		}
		pubStrLen = (int32_t)RyanMqttStrlen(pubStr);
	}

	pubTestPublishedEventCount = 0;
	pubTestDataEventCount = 0;
	pubTestDataEventCountNotQos0 = 0;
	for (int32_t i = 0; i < count; i++)
	{
		char *pubTopic = RyanMqttPubHybridTestPubTopic;
		result = RyanMqttPublishAndUserData(
			client, pubTopic, RyanMqttStrlen(pubTopic), pubStr, pubStrLen, i % 3, RyanMqttFalse,
			// NOLINTNEXTLINE(clang-diagnostic-int-to-void-pointer-cast,performance-no-int-to-ptr)
			(void *)(uintptr_t)(i % 3));
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		if (RyanMqttQos0 != i % 3)
		{
			sendNeedAckCount++;
		}

		if (delayms)
		{
			delay(delayms);
		}
	}

	// 检查收到的数据是否正确
	for (int32_t i = 0;; i++)
	{
		// 发送成功数等于需要ack数，同时也等于接收到非qos0的数
		if (pubTestPublishedEventCount == sendNeedAckCount &&
		    pubTestPublishedEventCount == pubTestDataEventCountNotQos0)
		{
			break;
		}

		if (i > 300)
		{
			RyanMqttLog_e("QOS测试失败, PublishedEventCount: %d, pubTestDataEventCountNotQos0: %d",
				      pubTestPublishedEventCount, pubTestDataEventCountNotQos0);
			result = RyanMqttFailedError;
			goto __exit;
		}

		delay(100);
	}

	result = RyanMqttUnSubscribe(client, RyanMqttPubHybridTestSubTopic);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	result = checkAckList(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

__exit:
	platformMemoryFree(pubStr);
	pubStr = NULL;
	RyanMqttLog_i("mqtt 发布测试，销毁mqtt客户端");
	RyanMqttTestDestroyClient(client);
	return result;
}

RyanMqttError_e RyanMqttPubTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	// 发布 & 订阅 qos 测试
	result = RyanMqttPublishTest(RyanMqttQos0, 1000, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttPublishTest(RyanMqttQos1, 1000, 1);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttPublishTest(RyanMqttQos2, 1000, 1);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttPublishHybridTest(1000, 5);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
