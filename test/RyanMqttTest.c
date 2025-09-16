#include "RyanMqttTest.h"

/**
 * @brief mqtt事件回调处理函数
 * 事件的详细定义可以查看枚举定义
 *
 * @param pclient
 * @param event
 * @param eventData 查看事件枚举，后面有说明eventData是什么类型
 */
void mqttEventBaseHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{

	switch (event)
	{
	case RyanMqttEventError: break;

	case RyanMqttEventConnected: // 不管有没有使能clearSession，都非常推荐在连接成功回调函数中订阅主题
		RyanMqttLog_i("mqtt连接成功回调 %d", *(int32_t *)eventData);
		break;

	case RyanMqttEventDisconnected: RyanMqttLog_w("mqtt断开连接回调 %d", *(int32_t *)eventData); break;

	case RyanMqttEventSubscribed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_w("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventSubscribedFailed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_w("mqtt订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventUnSubscribed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_w("mqtt取消订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventUnSubscribedFailed: {
		RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
		RyanMqttLog_w("mqtt取消订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventPublished: {
		RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
		RyanMqttLog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
		break;
	}

	case RyanMqttEventData: {
		RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
		RyanMqttLog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
			      msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);

		RyanMqttLog_i("%.*s", msgData->payloadLen, msgData->payload);
		break;
	}

	case RyanMqttEventRepeatPublishPacket: // qos2 / qos1重发事件回调
	{
		RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
		RyanMqttLog_w("发布消息进行重发了，packetType: %d, packetId: %d, topic: %s, qos: %d",
			      ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic,
			      ackHandler->msgHandler->qos);

		// printfArrStr(ackHandler->packet, ackHandler->packetLen, "重发数据: ");
		break;
	}

	case RyanMqttEventReconnectBefore:
		// 如果每次connect都需要修改连接信息，这里是最好的选择。 否则需要注意资源互斥
		RyanMqttLog_i("重连前事件回调");
		break;

	case RyanMqttEventAckCountWarning: // qos2 / qos1的ack链表超过警戒值，不进行释放会一直重发，占用额外内存
	{
		// 根据实际情况清除ack, 这里等待每个ack重发次数到达警戒值后清除。
		// 在资源有限的单片机中也不应频繁发送qos2 / qos1消息
		uint16_t ackHandlerCount = *(uint16_t *)eventData;
		RyanMqttLog_i("ack记数值超过警戒值回调: %d", ackHandlerCount);
		break;
	}

	case RyanMqttEventAckRepeatCountWarning: // 重发次数到达警戒值事件
	{
		RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
		// 这里选择直接丢弃该消息
		RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
		RyanMqttLog_e("ack重发次数超过警戒值回调 packetType: %d, packetId: %d, topic: %s, qos: %d",
			      ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic,
			      ackHandler->msgHandler->qos);
		RyanMqttDiscardAckHandler(client, ackHandler->packetType, ackHandler->packetId);
		break;
	}

	case RyanMqttEventAckHandlerDiscard: {
		RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
		RyanMqttLog_i("ack丢弃回调: packetType: %d, packetId: %d, topic: %s, qos: %d", ackHandler->packetType,
			      ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
		break;
	}

	case RyanMqttEventDestroyBefore:
		RyanMqttLog_w("销毁mqtt客户端前回调");

		RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
		struct RyanMqttTestEventUserData *eventUserData =
			(struct RyanMqttTestEventUserData *)client->config.userData;
		if (RyanMqttTestEventUserDataMagic != eventUserData->magic)
		{
			RyanMqttLog_e("eventUserData野指针");
			break;
		}

		if (eventUserData->syncFlag)
		{
			sem_post(&eventUserData->sem);
		}

		break;

	case RyanMqttEventUnsubscribedData: {
		RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
		RyanMqttLog_i("接收到未匹配任何订阅主题的报文事件 topic: %.*s, packetId: %d, payload len: %d",
			      msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen);
		break;
	}

	default: break;
	}
}

RyanMqttError_e RyanMqttTestInit(RyanMqttClient_t **client, RyanMqttBool_e syncFlag, RyanMqttBool_e autoReconnectFlag,
				 uint16_t keepaliveTimeoutS, RyanMqttEventHandle mqttEventCallback, void *userData)
{
	// 手动避免count的资源竞争了
	static uint32_t count = 0;
	char aaa[64];

	RyanMqttTestEnableCritical();
	count++;
	RyanMqttTestExitCritical();

	RyanMqttSnprintf(aaa, sizeof(aaa), "%s%d", RyanMqttClientId, count);

	struct RyanMqttTestEventUserData *eventUserData =
		(struct RyanMqttTestEventUserData *)malloc(sizeof(struct RyanMqttTestEventUserData));
	if (NULL == eventUserData)
	{
		RyanMqttLog_e("内存不足");
		return RyanMqttNotEnoughMemError;
	}

	RyanMqttMemset(eventUserData, 0, sizeof(struct RyanMqttTestEventUserData));

	eventUserData->magic = RyanMqttTestEventUserDataMagic;
	eventUserData->syncFlag = syncFlag;
	eventUserData->userData = userData;
	if (eventUserData->syncFlag)
	{
		sem_init(&eventUserData->sem, 0, 0);
	}

	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClientConfig_t mqttConfig = {.clientId = aaa,
					     .userName = RyanMqttUserName,
					     .password = RyanMqttPassword,
					     .host = RyanMqttHost,
					     .port = RyanMqttPort,
					     .taskName = "mqttThread",
					     .taskPrio = 16,
					     .taskStack = 4096,
					     .mqttVersion = 4,
					     .ackHandlerRepeatCountWarning = 600,
					     .ackHandlerCountWarning = 60000,
					     .autoReconnectFlag = autoReconnectFlag,
					     .cleanSessionFlag = RyanMqttTrue,
					     .reconnectTimeout = RyanMqttReconnectTimeout,
					     .recvTimeout = RyanMqttRecvTimeout,
					     .sendTimeout = RyanMqttSendTimeout,
					     .ackTimeout = RyanMqttAckTimeout,
					     .keepaliveTimeoutS = keepaliveTimeoutS,
					     .mqttEventHandle =
						     mqttEventCallback ? mqttEventCallback : mqttEventBaseHandle,
					     .userData = eventUserData};

	// 初始化mqtt客户端
	result = RyanMqttInit(client);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 注册需要的事件回调
	result = RyanMqttRegisterEventId(*client, RyanMqttEventAnyId);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 设置mqtt客户端config
	result = RyanMqttSetConfig(*client, &mqttConfig);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);
	// 重复设定一次测试
	result = RyanMqttSetConfig(*client, &mqttConfig);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 获取config测试
	RyanMqttClientConfig_t *mqttConfig22;
	result = RyanMqttGetConfig(*client, &mqttConfig22);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);
	RyanMqttFreeConfigFromGet(mqttConfig22);

	// 设置遗嘱消息
	result = RyanMqttSetLwt(*client, "pub/lwt/test", "this is will", RyanMqttStrlen("this is will"), RyanMqttQos2,
				0);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);
	// 重复设定一次测试
	result = RyanMqttSetLwt(*client, "pub/lwt/test", "this is will", RyanMqttStrlen("this is will"), RyanMqttQos2,
				0);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	// 启动mqtt客户端线程
	result = RyanMqttStart(*client);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_e);

	uint32_t timeout_ms = 30000; // 30 seconds
	uint32_t elapsed = 0;
	while (RyanMqttConnectState != RyanMqttGetState(*client) && elapsed < timeout_ms)
	{
		delay(100);
		elapsed += 100;
	}

	if (RyanMqttConnectState != RyanMqttGetState(*client))
	{
		// 不处理错误,测试代码
		RyanMqttLog_e("Connection timeout after %d ms", timeout_ms);
		return RyanMqttFailedError;
	}

	return RyanMqttSuccessError;
}

typedef struct
{
	void *ptr;
	timer_t timerid;
} FreeTimerArg;

static void RyanMqttTestFreeTimerCallback(union sigval arg)
{
	FreeTimerArg *fta = arg.sival_ptr;
	free(fta->ptr);

	RyanMqttTestEnableCritical();
	destroyCount--;
	RyanMqttTestExitCritical();

	timer_t timerid = fta->timerid;
	free(fta); // 释放参数结构

	if (0 != timer_delete(timerid))
	{
		RyanMqttLog_e("timer_delete failed");
	}
}

static void RyanMqttTestScheduleFreeAfterMs(void *ptr, uint32_t delayMs)
{
	RyanMqttTestEnableCritical();
	destroyCount++;
	RyanMqttTestExitCritical();

	timer_t timerid;
	struct sigevent sev = {0};
	struct itimerspec its = {0};

	FreeTimerArg *fta = malloc(sizeof(FreeTimerArg));
	fta->ptr = ptr;

	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = fta;                           // 传递给回调的参数
	sev.sigev_notify_function = RyanMqttTestFreeTimerCallback; // 定时到期时调用的函数

	if (0 != timer_create(CLOCK_MONOTONIC, &sev, &timerid))
	{
		RyanMqttLog_e("timer_create failed");
		free(fta);
		return;
	}

	fta->timerid = timerid;

	// 毫秒转秒和纳秒
	its.it_value.tv_sec = delayMs / 1000;
	its.it_value.tv_nsec = (uint32_t)((delayMs % 1000) * 1000000);

	if (0 != timer_settime(timerid, 0, &its, NULL))
	{
		RyanMqttLog_e("timer_settime failed");

		if (0 != timer_delete(timerid))
		{
			RyanMqttLog_e("timer_delete failed");
		}

		free(fta);
		return;
	}
}

RyanMqttError_e RyanMqttTestDestroyClient(RyanMqttClient_t *client)
{
	struct RyanMqttTestEventUserData *eventUserData = (struct RyanMqttTestEventUserData *)client->config.userData;

	if (RyanMqttTestEventUserDataMagic != eventUserData->magic)
	{
		RyanMqttLog_e("eventUserData野指针");
	}

	RyanMqttDisconnect(client, RyanMqttTrue);

	// 启动mqtt客户端线程
	RyanMqttDestroy(client);

	if (eventUserData->syncFlag)
	{
		sem_wait(&eventUserData->sem);
		sem_destroy(&eventUserData->sem);

		delay(20); // 等待mqtt线程回收资源
		free(eventUserData);
	}
	else
	{
		RyanMqttTestScheduleFreeAfterMs(eventUserData, RyanMqttRecvTimeout + 20);
	}

	return RyanMqttSuccessError;
}

RyanMqttError_e checkAckList(RyanMqttClient_t *client)
{
	RyanMqttLog_w("等待检查ack链表，等待 recvTime: %d", client->config.recvTimeout);
	delay(RyanMqttRecvTimeout + 50);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	int ackEmpty = RyanMqttListIsEmpty(&client->ackHandlerList);
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);
	if (!ackEmpty)
	{
		RyanMqttLog_e("mqtt空间 ack链表不为空");
		return RyanMqttFailedError;
	}

	platformMutexLock(client->config.userData, &client->userSessionLock);
	int userAckEmpty = RyanMqttListIsEmpty(&client->userAckHandlerList);
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
	if (!userAckEmpty)
	{
		RyanMqttLog_e("用户空间 ack链表不为空");
		return RyanMqttFailedError;
	}

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	int msgEmpty = RyanMqttListIsEmpty(&client->msgHandlerList);
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
	if (!msgEmpty)
	{
		RyanMqttLog_e("mqtt空间 msg链表不为空");
		return RyanMqttFailedError;
	}

	return RyanMqttSuccessError;
}

// 注意测试代码只有特定emqx服务器才可以通过，用户的emqx服务器大概率通过不了，
// 因为有些依赖emqx的配置，比如消息重试间隔，最大飞行窗口，最大消息队列等
// todo 增加session测试
// !当测试程序出错时，并不会回收内存。交由父进程进行回收
int main(void)
{
	RyanMqttTestUtileInit();

	RyanMqttError_e result = RyanMqttSuccessError;

	uint32_t testRunCount = 0;
	uint32_t funcStartMs;
#define runTestWithLogAndTimer(fun)                                                                                    \
	do                                                                                                             \
	{                                                                                                              \
		testRunCount++;                                                                                        \
		RyanMqttLog_raw("┌── [TEST %d] 开始执行: " #fun "()\r\n", testRunCount);                               \
		funcStartMs = platformUptimeMs();                                                                      \
		result = fun();                                                                                        \
		RyanMqttLog_raw("└── [TEST %d] 结束执行: 返回值 = %d %s | 耗时: %d ms\x1b[0m\r\n\r\n", testRunCount,   \
				result, (result == RyanMqttSuccessError) ? "✅" : "❌",                                \
				platformUptimeMs() - funcStartMs);                                                     \
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_e, { goto __exit; });    \
	} while (0)

	uint32_t totalElapsedStartMs = platformUptimeMs();
	runTestWithLogAndTimer(RyanMqttPublicApiParamCheckTest);
	runTestWithLogAndTimer(RyanMqttMemoryFaultToleranceTest);

	runTestWithLogAndTimer(RyanMqttSubTest);
	runTestWithLogAndTimer(RyanMqttPubTest);

	runTestWithLogAndTimer(RyanMqttDestroyTest);

	runTestWithLogAndTimer(RyanMqttNetworkFaultToleranceMemoryTest);
	runTestWithLogAndTimer(RyanMqttNetworkFaultQosResilienceTest);

	runTestWithLogAndTimer(RyanMqttMultiThreadMultiClientTest);
	runTestWithLogAndTimer(RyanMqttMultiThreadSafetyTest);

	runTestWithLogAndTimer(RyanMqttReconnectTest);
	runTestWithLogAndTimer(RyanMqttKeepAliveTest);

	// 暂时不开放出来
	// runTestWithLogAndTimer(RyanMqttWildcardTest);

__exit:

	RyanMqttLog_raw("测试总耗时: %.3f S\r\n", (platformUptimeMs() - totalElapsedStartMs) / 1000.0);

	if (RyanMqttSuccessError == result)
	{
		RyanMqttLog_raw("测试成功---------------------------\r\n");
	}
	else
	{
		RyanMqttLog_raw("测试失败---------------------------\r\n");
	}

	for (uint32_t i = 0; i < 3; i++)
	{
		displayMem();
		delay(300);
	}

	RyanMqttTestUtileDeInit();
	return 0;
}
