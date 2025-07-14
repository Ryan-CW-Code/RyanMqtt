#include "RyanMqttTest.h"

static RyanMqttSubscribeData_t *subscribeManyData = NULL;
static int32_t subTestCount = 0;

static RyanMqttSubscribeData_t *topicIsSubscribeArr(char *topic)
{
	for (int32_t i = 0; i < subTestCount; i++)
	{
		if (0 == strcmp(topic, subscribeManyData[i].topic))
		{
			return &subscribeManyData[i];
		}
	}

	return NULL;
}

static void RyanMqttSubEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
	RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
	switch (event)
	{

	case RyanMqttEventSubscribed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_i("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		RyanMqttSubscribeData_t *subscribeData = topicIsSubscribeArr(msgHandler->topic);
		if (NULL == subscribeData)
		{
			RyanMqttLog_e("mqtt 订阅主题非法 topic: %s", msgHandler->topic);
			RyanMqttDestroy(client);
		}

		if (subscribeData->qos != msgHandler->qos)
		{
			RyanMqttLog_e("mqtt 订阅主题降级 topic: %s, exportQos: %d, qos: %d", msgHandler->topic,
				      subscribeData->qos, msgHandler->qos);
			RyanMqttDestroy(client);
		}

		break;
	}

	case RyanMqttEventSubscribedFaile: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_i("mqtt订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventUnSubscribed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_i("mqtt取消订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		RyanMqttSubscribeData_t *subscribeData = topicIsSubscribeArr(msgHandler->topic);
		if (NULL == subscribeData)
		{
			RyanMqttLog_e("mqtt 订阅主题非法 topic: %s", msgHandler->topic);
		}
		if (subscribeData->qos != msgHandler->qos)
		{
			RyanMqttLog_e("mqtt 取消订阅主题信息不对 topic: %s, exportQos: %d, qos: %d", msgHandler->topic,
				      subscribeData->qos, msgHandler->qos);
			RyanMqttDestroy(client);
		}

		break;
	}

	case RyanMqttEventUnSubscribedFaile: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_w("mqtt取消订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	default: mqttEventBaseHandle(pclient, event, eventData); break;
	}
}

static RyanMqttError_e RyanMqttSubscribeCheckMsgHandle(RyanMqttClient_t *client)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	int32_t subscribeNum = 0;
	RyanMqttMsgHandler_t *msgHandles = NULL;
	delay(100);
	for (int32_t i = 0; i < 600; i++)
	{

		result = RyanMqttGetSubscribeSafe(client, &msgHandles, &subscribeNum);
		if (RyanMqttSuccessError != result)
		{
			if (RyanMqttNoRescourceError == result)
			{
				RyanMqttLog_w("没有订阅的主题");
			}
			else
			{
				RyanMqttLog_e("获取订阅主题数失败！！！");
			}
		}
		else
		{
			RyanMqttLog_i("mqtt客户端已订阅的主题数: %d, 应该订阅主题数: %d", subscribeNum, subTestCount);
			// for (int32_t i = 0; i < subscribeNum; i++)
			//     RyanMqttLog_i("已经订阅主题: %d, topic: %s, QOS: %d", i, msgHandles[i].topic,
			//     msgHandles[i].qos);
			int32_t subscribeTotalCount = 0;
			RyanMqttGetSubscribeTotalCount(client, &subscribeTotalCount);

			if (subscribeNum == subTestCount && subscribeTotalCount == subTestCount)
			{
				break;
			}
		}

		if (i > 500)
		{
			result = RyanMqttFailedError;
			goto __exit;
		}

		RyanMqttSafeFreeSubscribeResources(msgHandles, subscribeNum);
		msgHandles = NULL;
		delay(100);
	}

	// 检查订阅主题是否正确
	for (int32_t i = 0; i < subscribeNum; i++)
	{
		if (NULL == topicIsSubscribeArr(msgHandles[i].topic))
		{
			RyanMqttLog_e("主题不匹配或者qos不对, topic: %s, qos: %d", msgHandles[i].topic,
				      msgHandles[i].qos);
			result = RyanMqttFailedError;
			goto __exit;
		}
	}

__exit:
	if (NULL != msgHandles)
	{
		RyanMqttSafeFreeSubscribeResources(msgHandles, subscribeNum);
	}
	return result;
}

static RyanMqttError_e RyanMqttSubscribeTest(int32_t count)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	RyanMqttUnSubscribeData_t *unSubscribeManyData = NULL;
	subTestCount = count;

	RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttTrue, 120, RyanMqttSubEventHandle);

	// 生成需要订阅的主题数据
	{
		subscribeManyData = (RyanMqttSubscribeData_t *)malloc(sizeof(RyanMqttSubscribeData_t) * count);
		if (NULL == subscribeManyData)
		{
			RyanMqttLog_e("内存不足");
			return RyanMqttNotEnoughMemError;
		}

		for (int32_t i = 0; i < count; i++)
		{
			subscribeManyData[i].qos = i % 3;
			char *topic = (char *)malloc(64);
			if (NULL == topic)
			{
				RyanMqttLog_e("内存不足");
				return RyanMqttNotEnoughMemError;
			}
			snprintf(topic, 64, "test/subscribe/%d", i);
			subscribeManyData[i].topic = topic;
			subscribeManyData[i].topicLen = strlen(topic);
		}
	}

	// 订阅全部主题
	result = RyanMqttSubscribeMany(client, count - 1, subscribeManyData);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	result = RyanMqttSubscribe(client, subscribeManyData[count - 1].topic, subscribeManyData[count - 1].qos);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	result = RyanMqttSubscribeCheckMsgHandle(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 测试重复订阅，不修改qos等级
	result = RyanMqttSubscribeMany(client, count / 2, subscribeManyData);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	result = RyanMqttSubscribeCheckMsgHandle(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 测试重复订阅并且修改qos等级
	for (int32_t i = count; i > 0; i--)
	{
		subscribeManyData[count - i].qos = i % 3;
	}
	result = RyanMqttSubscribeMany(client, count, subscribeManyData);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	result = RyanMqttSubscribeCheckMsgHandle(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 测试取消所有订阅消息
	unSubscribeManyData = malloc(sizeof(RyanMqttUnSubscribeData_t) * count);
	if (NULL == unSubscribeManyData)
	{
		RyanMqttLog_e("内存不足");
		return RyanMqttNotEnoughMemError;
	}
	for (int32_t i = 0; i < count; i++)
	{
		char *topic = (char *)malloc(64);
		if (NULL == topic)
		{
			RyanMqttLog_e("内存不足");
			return RyanMqttNotEnoughMemError;
		}
		snprintf(topic, 64, "test/subscribe/%d", i);
		unSubscribeManyData[i].topic = topic;
		unSubscribeManyData[i].topicLen = strlen(topic);
	}

	result = RyanMqttUnSubscribeMany(client, count - 1, unSubscribeManyData);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	result = RyanMqttUnSubscribe(client, unSubscribeManyData[count - 1].topic);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 重复取消订阅主题
	result = RyanMqttUnSubscribeMany(client, count / 2, unSubscribeManyData);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	delay(100);
	for (int32_t i = 0; i < 600; i++)
	{
		RyanMqttMsgHandler_t *msgHandles = NULL;
		int32_t subscribeNum = 0;
		result = RyanMqttGetSubscribeSafe(client, &msgHandles, &subscribeNum);
		if (RyanMqttSuccessError != result)
		{
			if (RyanMqttNoRescourceError == result)
			{
				RyanMqttLog_w("没有订阅的主题");
			}
			else
			{
				RyanMqttLog_e("获取订阅主题数失败！！！");
			}
		}
		RyanMqttSafeFreeSubscribeResources(msgHandles, subscribeNum);

		// result = RyanMqttGetSubscribe(client, msgHandles, count, &subscribeNum);
		// if (RyanMqttNoRescourceError == result)
		// {
		// 	RyanMqttLog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", count);
		// }

		if (0 == subscribeNum)
		{
			break;
		}

		if (i > 500)
		{
			result = RyanMqttFailedError;
			goto __exit;
		}

		delay(100);
	}

	result = checkAckList(client);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

__exit:

	// 删除
	for (int32_t i = 0; i < count; i++)
	{
		if (NULL != subscribeManyData)
		{
			free(subscribeManyData[i].topic);
		}
		if (NULL != unSubscribeManyData)
		{
			free(unSubscribeManyData[i].topic);
		}
	}
	if (NULL != subscribeManyData)
	{
		free(subscribeManyData);
	}
	if (NULL != unSubscribeManyData)
	{
		free(unSubscribeManyData);
	}

	RyanMqttLog_i("mqtt 订阅测试，销毁mqtt客户端");
	RyanMqttDestroySync(client);
	return result;
}

RyanMqttError_e RyanMqttSubTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	result = RyanMqttSubscribeTest(1000);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
