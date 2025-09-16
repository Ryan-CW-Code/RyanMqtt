#include "RyanMqttTest.h"

static int32_t pubTestPublishedEventCount = 0;
static int32_t pubTestDataEventCount = 0;
static char *pubStr = NULL;
static int32_t pubStrLen = 0;
static char *pubStr2 = "a";
static int32_t pubStr2Len = 1;
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

		if (((msgData->payloadLen == (uint32_t)pubStrLen) &&
		     0 == memcmp(msgData->payload, pubStr, pubStrLen)) ||
		    ((msgData->payloadLen == (uint32_t)pubStr2Len) &&
		     0 == memcmp(msgData->payload, pubStr2, pubStr2Len)))
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
 * @brief 混乱随机的qos消息测试
 *
 * @param count
 * @param delayms
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttNetworkFaultQosResiliencePublishTest(int32_t count, uint32_t delayus, RyanMqttQos_e qos)
{

#define RyanMqttPubHybridTestPubTopic "testlinux/aa/pub/adfa/ddd"
#define RyanMqttPubHybridTestSubTopic "testlinux/aa/+/#"

	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	int32_t sendNeedAckCount = 0;

	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, RyanMqttPublishEventHandle, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 等待订阅主题成功
	result = RyanMqttSubscribe(client, RyanMqttPubHybridTestSubTopic, qos);
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
		pubStr = (char *)platformMemoryMalloc(2048);
		RyanMqttCheck(NULL != pubStr, RyanMqttNotEnoughMemError, RyanMqttLog_e);
		RyanMqttMemset(pubStr, 0, 2048);

		for (uint32_t i = 0; i < RyanRand(20, 220); i++)
		{
			snprintf(pubStr + 4 * i, 2048 - 4 * i, "%c%c%c%c", (char)RyanRand(32, 126),
				 (char)RyanRand(32, 126), (char)RyanRand(32, 126), (char)RyanRand(32, 126));
		}
		pubStrLen = (int32_t)RyanMqttStrlen(pubStr);
	}

	pubTestPublishedEventCount = 0;
	pubTestDataEventCount = 0;
	pubTestDataEventCountNotQos0 = 0;

	for (int32_t i = 0; i < count; i++)
	{
		if (RyanMqttConnectState != RyanMqttGetState(client))
		{
			RyanMqttLog_e("mqtt 发布测试，销毁mqtt客户端 %d", i);
			goto __exit;
		}

		if (delayus)
		{
			delay_us(delayus);
		}

		if (0 == i % (count / 5))
		{
			toggleRandomNetworkFault();
		}

		char *pubTopic = RyanMqttPubHybridTestPubTopic;
		uint32_t randNumber = RyanRand(1, 10);
		result = RyanMqttPublishWithUserData(
			client, pubTopic, RyanMqttStrlen(pubTopic), (0 == i % randNumber) ? pubStr2 : pubStr,
			(0 == i % randNumber) ? pubStr2Len : pubStrLen, qos, RyanMqttFalse,
			// NOLINTNEXTLINE(clang-diagnostic-int-to-void-pointer-cast,performance-no-int-to-ptr)
			(void *)(uintptr_t)(qos));
		if (RyanMqttSuccessError == result)
		{
			if (RyanMqttQos0 != qos)
			{
				sendNeedAckCount++;
			}
		}
		// else
		// {
		// 	RyanMqttLog_e("mqtt 发布测试，网络故障 %d", i);
		// }
	}

	delay(RyanMqttAckTimeout * 1 + 100);

	disableRandomNetworkFault();
	// 检查收到的数据是否正确
	for (int32_t i = 0;; i++)
	{
		if (RyanMqttQos2 == qos)
		{
			// qos2 发送成功数等于需要ack数，同时也等于接收到的数
			if (pubTestPublishedEventCount == sendNeedAckCount &&
			    pubTestPublishedEventCount == pubTestDataEventCount)
			{
				break;
			}
		}
		else if (RyanMqttQos1 == qos)
		{
			if (pubTestPublishedEventCount >= sendNeedAckCount
			    // 不要求接收数据也全部到达，跟emqx服务器关系太大了
			    //   &&  pubTestDataEventCount >= sendNeedAckCount
			)
			{
				break;
			}
		}
		else
		{
			break;
		}

		if (i > (RyanMqttAckTimeout * 5 / 100 + 500))
		{
			RyanMqttLog_e("QOS测试失败, PublishedEventCount: %d, pubTestDataEventCountNotQos0: %d, "
				      "sendNeedAckCount: %d, pubTestDataEventCount: %d",
				      pubTestPublishedEventCount, pubTestDataEventCountNotQos0, sendNeedAckCount,
				      pubTestDataEventCount);
			result = RyanMqttFailedError;
			goto __exit;
		}

		// 测试代码可以临时访问ack链表
		platformMutexLock(client->config.userData, &client->ackHandleLock);
		if (RyanMqttListIsEmpty(&client->ackHandlerList))
		{
			RyanMqttLog_e("ack链表为空, PublishedEventCount: %d, pubTestDataEventCountNotQos0: %d, "
				      "sendNeedAckCount: %d, pubTestDataEventCount: %d, pubStrLen: %d",
				      pubTestPublishedEventCount, pubTestDataEventCountNotQos0, sendNeedAckCount,
				      pubTestDataEventCount, pubStrLen);
		}
		platformMutexUnLock(client->config.userData, &client->ackHandleLock);

		delay(100);
	}

	result = RyanMqttUnSubscribe(client, RyanMqttPubHybridTestSubTopic);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	result = checkAckList(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

__exit:
	disableRandomNetworkFault();
	platformMemoryFree(pubStr);
	pubStr = NULL;
	RyanMqttLog_i("mqtt 测试，销毁mqtt客户端");
	RyanMqttTestDestroyClient(client);
	return result;
}

/**
 * @brief mqtt网络故障QOS韧性测试
 *
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttNetworkFaultQosResilienceTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	result = RyanMqttNetworkFaultQosResiliencePublishTest(500, 1, RyanMqttQos0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttNetworkFaultQosResiliencePublishTest(500, 2, RyanMqttQos1);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttNetworkFaultQosResiliencePublishTest(500, 4, RyanMqttQos2);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
