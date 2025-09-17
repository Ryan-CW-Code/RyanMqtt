#include "RyanMqttTest.h"

static RyanMqttQos_e invalidQos()
{
	static bool aa = true;

	if (aa)
	{
		aa = false;
		// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
		return (RyanMqttQos_e)(uintptr_t)10;
	}

	aa = true;
	return RyanMqttSubFail;
}

static RyanMqttError_e RyanMqttBaseApiParamCheckTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttClient_t *validClient = NULL;
	// 准备一个有效的客户端用于某些测试
	result = RyanMqttTestInit(&validClient, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });

	result = RyanMqttInit(NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	result = RyanMqttDestroy(NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// todo 可能增加状态判断
	result = RyanMqttStart(NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// todo 可能增加状态判断
	result = RyanMqttDisconnect(NULL, RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// todo 可能增加状态判断
	result = RyanMqttReconnect(NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	RyanMqttState_e state = RyanMqttGetState(NULL);
	RyanMqttCheckCodeNoReturn(state == RyanMqttInvalidState, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	result = RyanMqttGetKeepAliveRemain(NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	uint32_t keepAliveRemain;
	// NULL客户端指针
	result = RyanMqttGetKeepAliveRemain(NULL, &keepAliveRemain);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL剩余时间指针
	result = RyanMqttGetKeepAliveRemain(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttDiscardAckHandler(NULL, MQTT_PACKET_TYPE_PUBACK, 1);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效包ID (0 是无效的)
	result = RyanMqttDiscardAckHandler(validClient, MQTT_PACKET_TYPE_PUBACK, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 超过最大包ID
	result = RyanMqttDiscardAckHandler(validClient, MQTT_PACKET_TYPE_PUBACK, RyanMqttMaxPacketId + 1);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	RyanMqttEventId_e eventId = RyanMqttEventConnected;
	// NULL客户端指针
	result = RyanMqttGetEventId(NULL, &eventId);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL eventId指针
	result = RyanMqttGetEventId(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttRegisterEventId(NULL, RyanMqttEventConnected);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttCancelEventId(NULL, RyanMqttEventConnected);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttSuccessError;

__exit:
	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttFailedError;
}

static RyanMqttError_e RyanMqttLwtApiParamCheckTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttClient_t *validClient = NULL;
	// 准备一个有效的客户端用于某些测试
	result = RyanMqttTestInit(&validClient, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	RyanMqttDisconnect(validClient, RyanMqttTrue);

	// 增加一个get接口
	// NULL客户端指针
	result = RyanMqttSetLwt(NULL, "test/lwt", "offline", 7, RyanMqttQos1, RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL主题指针
	result = RyanMqttSetLwt(validClient, NULL, "offline", 7, RyanMqttQos1, RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL负载但长度不为0
	result = RyanMqttSetLwt(validClient, "test/lwt", NULL, 7, RyanMqttQos1, RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// payloadLen 超长
	result = RyanMqttSetLwt(validClient, "test/lwt", "offline", RyanMqttMaxPayloadLen + 1, RyanMqttQos1,
				RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效QoS
	result = RyanMqttSetLwt(validClient, "test/lwt", "offline", 7, invalidQos(), RyanMqttTrue);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttSuccessError;

__exit:
	RyanMqttLog_e("result: %d", result);
	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}

	return RyanMqttFailedError;
}

static RyanMqttError_e RyanMqttConfigApiParamCheckTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttClient_t *validClient = NULL;
	// 准备一个有效的客户端用于某些测试
	result = RyanMqttTestInit(&validClient, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	RyanMqttDisconnect(validClient, RyanMqttTrue);

	// NULL客户端指针
	result = RyanMqttGetConfig(NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	RyanMqttClientConfig_t *getConfig = NULL;

	/**
	 * @brief RyanMqttGetConfig
	 *
	 */
	// NULL客户端指针
	result = RyanMqttGetConfig(NULL, &getConfig);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL配置指针
	result = RyanMqttGetConfig(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL配置指针
	result = RyanMqttFreeConfigFromGet(NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	/**
	 * @brief RyanMqttSetConfig
	 *
	 */
	RyanMqttClientConfig_t baseMqttConfig = {.clientId = RyanMqttClientId,
						 .userName = RyanMqttUserName,
						 .password = RyanMqttPassword,
						 .host = RyanMqttHost,
						 .port = RyanMqttPort,
						 .taskName = "mqttThread",
						 .taskPrio = 16,
						 .taskStack = 4096,
						 .mqttVersion = 4,
						 .ackHandlerRepeatCountWarning = 6,
						 .ackHandlerCountWarning = 60000,
						 .autoReconnectFlag = RyanMqttTrue,
						 .cleanSessionFlag = RyanMqttTrue,
						 .reconnectTimeout = 3000,
						 .recvTimeout = 2000,
						 .sendTimeout = 1800,
						 .ackTimeout = 10000,
						 .keepaliveTimeoutS = 120,
						 .mqttEventHandle = NULL,
						 .userData = NULL};

	RyanMqttClientConfig_t mqttConfig = {0};

	// NULL客户端指针
	result = RyanMqttSetConfig(NULL, &mqttConfig);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL配置指针
	result = RyanMqttSetConfig(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

#define checkSetConfigParam(code)                                                                                      \
	do                                                                                                             \
	{                                                                                                              \
		RyanMqttMemcpy(&mqttConfig, &baseMqttConfig, sizeof(RyanMqttClientConfig_t));                          \
		{code};                                                                                                \
		result = RyanMqttSetConfig(validClient, &mqttConfig);                                                  \
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e,                  \
					  { goto __exit; });                                                           \
	} while (0)

	checkSetConfigParam({ mqttConfig.clientId = NULL; });
	checkSetConfigParam({ mqttConfig.host = NULL; });
	checkSetConfigParam({ mqttConfig.taskName = NULL; });
	checkSetConfigParam({
		mqttConfig.recvTimeout = 10;
		mqttConfig.keepaliveTimeoutS = 20 - 1;
	});
	checkSetConfigParam({
		mqttConfig.recvTimeout = 10;
		mqttConfig.sendTimeout = 11;
	});

	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttSuccessError;

__exit:
	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}

	return RyanMqttFailedError;
}

static RyanMqttError_e RyanMqttSubApiParamCheckTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttClient_t *validClient = NULL;
	// 准备一个有效的客户端用于某些测试
	result = RyanMqttTestInit(&validClient, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttSubscribe(NULL, "test/topic", RyanMqttQos1);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL主题指针
	result = RyanMqttSubscribe(validClient, NULL, RyanMqttQos1);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效QoS级别
	result = RyanMqttSubscribe(validClient, "test/topic", invalidQos());
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 分配测试数据
	RyanMqttSubscribeData_t subscribeData[2] = {0};
	subscribeData[0].topic = "test/topic1";
	subscribeData[0].topicLen = strlen("test/topic1");
	subscribeData[0].qos = RyanMqttQos1;
	subscribeData[1].topic = "test/topic2";
	subscribeData[1].topicLen = strlen("test/topic2");
	subscribeData[1].qos = RyanMqttQos2;
	// NULL客户端指针
	result = RyanMqttSubscribeMany(NULL, 2, subscribeData);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效数量
	result = RyanMqttSubscribeMany(validClient, 0, subscribeData);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	result = RyanMqttSubscribeMany(validClient, -1, subscribeData);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL数据指针
	result = RyanMqttSubscribeMany(validClient, 2, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	for (uint32_t i = 0; i < getArraySize(subscribeData); i++)
	{
		// subscribeData 内数据无效
		subscribeData[i].topic = NULL;
		subscribeData[i].topicLen = strlen("test/topic2");
		subscribeData[i].qos = RyanMqttQos2;
		result = RyanMqttSubscribeMany(validClient, 2, subscribeData);
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

		// subscribeData 内数据无效
		subscribeData[i].topic = "test/topic2";
		subscribeData[i].topicLen = 0;
		subscribeData[i].qos = RyanMqttQos2;
		result = RyanMqttSubscribeMany(validClient, 2, subscribeData);
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

		// subscribeData 内数据无效
		subscribeData[i].topic = "test/topic2";
		subscribeData[i].topicLen = strlen("test/topic2");
		subscribeData[i].qos = invalidQos();
		result = RyanMqttSubscribeMany(validClient, 2, subscribeData);
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

		// 恢复 subscribeData 内数据
		subscribeData[i].topic = "test/topic2";
		subscribeData[i].topicLen = strlen("test/topic2");
		subscribeData[i].qos = RyanMqttQos2;
	}

	// NULL客户端指针
	result = RyanMqttUnSubscribe(NULL, "test/topic");
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL主题指针
	result = RyanMqttUnSubscribe(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 分配测试数据
	RyanMqttUnSubscribeData_t unsubscribeData[2] = {0};
	unsubscribeData[0].topic = "test/topic1";
	unsubscribeData[0].topicLen = strlen("test/topic1");
	unsubscribeData[1].topic = "test/topic2";
	unsubscribeData[1].topicLen = strlen("test/topic2");

	// NULL客户端指针
	result = RyanMqttUnSubscribeMany(NULL, 2, unsubscribeData);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效数量
	result = RyanMqttUnSubscribeMany(validClient, 0, unsubscribeData);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL数据指针
	result = RyanMqttUnSubscribeMany(validClient, 2, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	for (uint32_t i = 0; i < getArraySize(unsubscribeData); i++)
	{
		// unsubscribeData 内数据无效
		unsubscribeData[i].topic = NULL;
		unsubscribeData[i].topicLen = strlen("test/topic2");

		result = RyanMqttUnSubscribeMany(validClient, 2, unsubscribeData);
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

		// unsubscribeData 内数据无效
		unsubscribeData[i].topic = "test/topic2";
		unsubscribeData[i].topicLen = 0;
		result = RyanMqttUnSubscribeMany(validClient, 2, unsubscribeData);
		RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

		// 恢复 unsubscribeData 内数据
		unsubscribeData[i].topic = "test/topic2";
		unsubscribeData[i].topicLen = strlen("test/topic2");
	}

	/**
	 * @brief
	 *
	 */
	RyanMqttMsgHandler_t *msgHandles = NULL;
	int32_t subscribeNum = 0;
	int32_t totalCount = 0;
	// NULL客户端指针
	result = RyanMqttGetSubscribeSafe(NULL, &msgHandles, &subscribeNum);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL消息句柄指针
	result = RyanMqttGetSubscribeSafe(validClient, NULL, &subscribeNum);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL数量指针
	result = RyanMqttGetSubscribeSafe(validClient, &msgHandles, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL指针但数量不为0
	result = RyanMqttSafeFreeSubscribeResources(NULL, 5);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttGetSubscribeTotalCount(NULL, &totalCount);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL总数指针
	result = RyanMqttGetSubscribeTotalCount(validClient, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e,
				  { goto __exit; }); // 清理资源

	/**
	 * @brief
	 *
	 */
	RyanMqttMsgHandler_t msgHandlesStatic[3] = {0};
	RyanMqttGetSubscribe(NULL, msgHandlesStatic, getArraySize(msgHandlesStatic), &subscribeNum);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	RyanMqttGetSubscribe(validClient, NULL, getArraySize(msgHandlesStatic), &subscribeNum);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	RyanMqttGetSubscribe(validClient, msgHandlesStatic, 0, &subscribeNum);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	RyanMqttGetSubscribe(validClient, msgHandlesStatic, getArraySize(msgHandlesStatic), NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttSuccessError;

__exit:
	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}

	return RyanMqttFailedError;
}

static RyanMqttError_e RyanMqttPubApiParamCheckTest(void)
{

	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttClient_t *validClient = NULL;
	// 准备一个有效的客户端用于某些测试
	result = RyanMqttTestInit(&validClient, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttPublish(NULL, "test/topic", "payload", 7, RyanMqttQos0, RyanMqttFalse);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL主题指针
	result = RyanMqttPublish(validClient, NULL, "payload", 7, RyanMqttQos0, RyanMqttFalse);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL负载指针但长度不为0
	result = RyanMqttPublish(validClient, "test/topic", NULL, 7, RyanMqttQos0, RyanMqttFalse);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效QoS级别
	result = RyanMqttPublish(validClient, "test/topic", "payload", 7, invalidQos(), RyanMqttFalse);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 超大负载长度
	result = RyanMqttPublish(validClient, "test/topic", "payload", RyanMqttMaxPayloadLen + 1, RyanMqttQos0,
				 RyanMqttFalse);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL客户端指针
	result = RyanMqttPublishWithUserData(NULL, "test/topic", 10, "payload", 7, RyanMqttQos1, RyanMqttFalse, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// NULL主题指针
	result = RyanMqttPublishWithUserData(validClient, NULL, 10, "payload", 7, RyanMqttQos1, RyanMqttFalse, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 主题长度为0
	result = RyanMqttPublishWithUserData(validClient, "test/topic", 0, "payload", 7, RyanMqttQos1, RyanMqttFalse,
					     NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });

	// 无效QoS级别
	result = RyanMqttPublishWithUserData(validClient, "test/topic", strlen("test/topic"), "payload", 7,
					     invalidQos(), RyanMqttFalse, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e,
				  { goto __exit; }); // 清理资源

	// 负载长度>0但负载指针为NULL
	result = RyanMqttPublishWithUserData(validClient, "test/topic", strlen("test/topic"), NULL, 7, RyanMqttQos1,
					     RyanMqttFalse, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttParamInvalidError == result, result, RyanMqttLog_e, { goto __exit; });
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttSuccessError;

__exit:
	// 清理资源
	if (validClient)
	{
		RyanMqttTestDestroyClient(validClient);
	}
	return RyanMqttFailedError;
}

RyanMqttError_e RyanMqttPublicApiParamCheckTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	result = RyanMqttBaseApiParamCheckTest();
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttLwtApiParamCheckTest();
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttConfigApiParamCheckTest();
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttSubApiParamCheckTest();
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	result = RyanMqttPubApiParamCheckTest();
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
