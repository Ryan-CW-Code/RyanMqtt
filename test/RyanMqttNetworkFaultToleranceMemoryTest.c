#include "RyanMqttTest.h"

/**
 * @brief 混乱随机的qos消息测试
 *
 * @param count
 * @param delayms
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttNetworkFaultPublishHybridTest(int32_t count, uint32_t delayms)
{

#define RyanMqttPubHybridTestPubTopic "testlinux/aa/pub/adfa/kkk"
#define RyanMqttPubHybridTestSubTopic "testlinux/aa/+/#"

	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	char *pubStr = "hello, this is a mqtt publish test string.";
	uint32_t pubStrLen = strlen(pubStr);

	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 等待订阅主题成功
	RyanMqttSubscribe(client, RyanMqttPubHybridTestSubTopic, RyanMqttQos2);
	delay(2);

	for (int32_t i = 0; i < count; i++)
	{
		if (RyanMqttConnectState != RyanMqttGetState(client))
		{
			RyanMqttLog_e("mqtt 发布测试，销毁mqtt客户端aaaaaaaaaaaaa %d", i);
			goto __exit;
		}

		char *pubTopic = RyanMqttPubHybridTestPubTopic;
		RyanMqttPublishWithUserData(client, pubTopic, RyanMqttStrlen(pubTopic), pubStr, pubStrLen, i % 3,
					   RyanMqttFalse, NULL);

		if (delayms)
		{
			delay(delayms);
		}
	}

	RyanMqttUnSubscribe(client, RyanMqttPubHybridTestSubTopic);

__exit:

	RyanMqttLog_i("mqtt 发布测试，销毁mqtt客户端");
	RyanMqttTestDestroyClient(client);
	return result;
}

static RyanMqttError_e RyanMqttNetworkFaultSubscribeHybridTest(int32_t count, int32_t testCount)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client;
	RyanMqttUnSubscribeData_t *unSubscribeManyData = NULL;
	static RyanMqttSubscribeData_t *subscribeManyData = NULL;

	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	// 生成需要订阅的主题数据
	{
		subscribeManyData =
			(RyanMqttSubscribeData_t *)platformMemoryMalloc(sizeof(RyanMqttSubscribeData_t) * count);
		if (NULL == subscribeManyData)
		{
			RyanMqttLog_e("内存不足");
			result = RyanMqttNotEnoughMemError;
			goto __exit;
		}

		for (int32_t i = 0; i < count; i++)
		{
			subscribeManyData[i].qos = i % 3;
			char *topic = (char *)platformMemoryMalloc(64);
			if (NULL == topic)
			{
				RyanMqttLog_e("内存不足");
				result = RyanMqttNotEnoughMemError;
				goto __exit;
			}
			RyanMqttSnprintf(topic, 64, "test/subscribe/%d", i);
			subscribeManyData[i].topic = topic;
			subscribeManyData[i].topicLen = RyanMqttStrlen(topic);
		}
	}

	// 生成取消所有订阅消息
	unSubscribeManyData = platformMemoryMalloc(sizeof(RyanMqttUnSubscribeData_t) * count);
	if (NULL == unSubscribeManyData)
	{
		RyanMqttLog_e("内存不足");
		result = RyanMqttNotEnoughMemError;
		goto __exit;
	}
	for (int32_t i = 0; i < count; i++)
	{
		char *topic = (char *)platformMemoryMalloc(64);
		if (NULL == topic)
		{
			RyanMqttLog_e("内存不足");
			result = RyanMqttNotEnoughMemError;
			goto __exit;
		}
		RyanMqttSnprintf(topic, 64, "test/subscribe/%d", i);
		unSubscribeManyData[i].topic = topic;
		unSubscribeManyData[i].topicLen = RyanMqttStrlen(topic);
	}

	for (int32_t testCount2 = 0; testCount2 < testCount; testCount2++)
	{
		// 订阅全部主题
		RyanMqttSubscribeMany(client, count - 1, subscribeManyData);

		RyanMqttSubscribe(client, subscribeManyData[count - 1].topic, subscribeManyData[count - 1].qos);
		// 测试重复订阅，不修改qos等级
		RyanMqttSubscribeMany(client, count / 2, subscribeManyData);

		// 测试重复订阅并且修改qos等级
		for (int32_t i = count; i > 0; i--)
		{
			subscribeManyData[count - i].qos = i % 3;
		}
		RyanMqttSubscribeMany(client, count, subscribeManyData);
		RyanMqttUnSubscribeMany(client, count - 1, unSubscribeManyData);
		RyanMqttUnSubscribe(client, unSubscribeManyData[count - 1].topic);

		// 重复取消订阅主题
		RyanMqttUnSubscribeMany(client, count / 2, unSubscribeManyData);
	}

__exit:
	// 删除
	for (int32_t i = 0; i < count; i++)
	{
		if (NULL != subscribeManyData && NULL != subscribeManyData[i].topic)
		{
			platformMemoryFree(subscribeManyData[i].topic);
		}

		if (NULL != unSubscribeManyData && NULL != unSubscribeManyData[i].topic)
		{
			platformMemoryFree(unSubscribeManyData[i].topic);
		}
	}

	if (NULL != subscribeManyData)
	{
		platformMemoryFree(subscribeManyData);
	}

	if (NULL != unSubscribeManyData)
	{
		platformMemoryFree(unSubscribeManyData);
	}

	RyanMqttLog_i("mqtt 订阅测试，销毁mqtt客户端");
	RyanMqttTestDestroyClient(client);
	return result;
}

/**
 * @brief 网络容错内存测试
 *
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttNetworkFaultToleranceMemoryTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	enableRandomNetworkFault();

	result = RyanMqttNetworkFaultPublishHybridTest(2000, 1);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttNetworkFaultSubscribeHybridTest(1000, 5);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	disableRandomNetworkFault();
	return RyanMqttSuccessError;

__exit:
	disableRandomNetworkFault();
	return RyanMqttFailedError;
}
