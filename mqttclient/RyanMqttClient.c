#define rlogLevel (rlogLvlDebug) // 日志打印等级

#include "RyanMqttClient.h"
#include "RyanMqttThread.h"
#include "RyanMqttUtile.h"
#include "core_mqtt_serializer.h"

/**
 * @brief 获取报文标识符，报文标识符不可为0
 * 都在sendbuf锁内调用
 * @param client
 * @return uint16_t
 */
static uint16_t RyanMqttGetNextPacketId(RyanMqttClient_t *client)
{
	uint16_t packetId;
	RyanMqttAssert(NULL != client);
	platformCriticalEnter(client->config.userData, &client->criticalLock);
	if (client->packetId >= RyanMqttMaxPacketId || client->packetId < 1)
	{
		client->packetId = 1;
	}
	else
	{
		client->packetId++;
	}
	packetId = client->packetId;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return packetId;
}

static RyanMqttError_e setConfigValue(char **dest, char const *const rest)
{
	RyanMqttAssert(NULL != dest && NULL != rest);

	// if (0 == strcmp(*dest, rest))
	//     return RyanMqttSuccessError;

	platformMemoryFree(*dest);

	RyanMqttStringCopy(dest, (char *)rest, strlen(rest));
	if (NULL == *dest)
	{
		return RyanMqttFailedError;
	}

	return RyanMqttSuccessError;
}

/**
 * @brief mqtt初始化
 *
 * @param clientConfig
 * @param pClient mqtt客户端指针
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttInit(RyanMqttClient_t **pClient)
{
	RyanMqttClient_t *client = NULL;
	RyanMqttCheck(NULL != pClient, RyanMqttParamInvalidError, rlog_d);

	client = (RyanMqttClient_t *)platformMemoryMalloc(sizeof(RyanMqttClient_t));
	RyanMqttCheck(NULL != client, RyanMqttNotEnoughMemError, rlog_d);
	memset(client, 0, sizeof(RyanMqttClient_t));
	platformCriticalInit(client->config.userData,
			     &client->criticalLock); // 初始化临界区

	client->packetId = 1; // 控制报文必须包含一个非零的 16 位报文标识符
	client->clientState = RyanMqttInitState;
	client->eventFlag = 0;
	client->ackHandlerCount = 0;
	client->lwtFlag = RyanMqttFalse;

	platformMutexInit(client->config.userData, &client->sendLock); // 初始化发送缓冲区互斥锁

	RyanListInit(&client->msgHandlerList);
	platformMutexInit(client->config.userData, &client->msgHandleLock);

	RyanListInit(&client->ackHandlerList);
	platformMutexInit(client->config.userData, &client->ackHandleLock);

	RyanListInit(&client->userAckHandlerList);
	platformMutexInit(client->config.userData, &client->userAckHandleLock);

	RyanMqttSetClientState(client, RyanMqttInitState);

	platformNetworkInit(client->config.userData, &client->network); // 网络接口初始化

	*pClient = client;
	return RyanMqttSuccessError;
}

/**
 * @brief 销毁mqtt客户端
 *  !用户线程直接删除mqtt线程是很危险的行为。所以这里设置标志位，稍后由mqtt线程自己释放所占有的资源。
 *  !mqtt删除自己的延时最大不会超过config里面 recvTimeout + 1秒
 *  !mqtt删除自己前会调用 RyanMqttEventDestoryBefore 事件回调
 *  !调用此函数后就不应该再对该客户端进行任何操作
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDestroy(RyanMqttClient_t *client)
{

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->destoryFlag = RyanMqttTrue;
	platformCriticalExit(client->config.userData, &client->criticalLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 启动mqtt客户端
 * !不要重复调用,重复调用会返回错误
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttStart(RyanMqttClient_t *client)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttInitState == RyanMqttGetClientState(client), RyanMqttFailedError, rlog_d);

	RyanMqttSetClientState(client, RyanMqttStartState);
	// 连接成功，需要初始化 MQTT 线程
	result = platformThreadInit(client->config.userData, &client->mqttThread, client->config.taskName,
				    RyanMqttThread, client, client->config.taskStack, client->config.taskPrio);
	RyanMqttCheckCode(RyanMqttSuccessError == result, RyanMqttNotEnoughMemError, rlog_d,
			  { RyanMqttSetClientState(client, RyanMqttInitState); });

	return RyanMqttSuccessError;
}

/**
 * @brief 断开mqtt服务器连接
 *
 * @param client
 * @param sendDiscFlag
 * RyanMqttTrue表示发送断开连接报文，RyanMqttFalse表示不发送断开连接报文
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDisconnect(RyanMqttClient_t *client, RyanMqttBool_e sendDiscFlag)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

	if (RyanMqttTrue == sendDiscFlag)
	{
		MQTTStatus_t status = MQTTSuccess;
		MQTTFixedBuffer_t fixedBuffer = {0};

		// 获取断开连接的数据包大小
		status = MQTT_GetDisconnectPacketSize(&fixedBuffer.size);
		RyanMqttAssert(MQTTSuccess == status);

		// 申请断开连接数据包的空间
		fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
		RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d);

		// 序列化断开连接数据包
		status = MQTT_SerializeDisconnect(&fixedBuffer);
		RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });

		// 发送断开连接数据包
		RyanMqttError_e result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });

		platformMemoryFree(fixedBuffer.pBuffer);
	}

	RyanMqttConnectStatus_e connectState = RyanMqttConnectUserDisconnected;
	RyanMqttEventMachine(client, RyanMqttEventDisconnected, (void *)&connectState);
	return RyanMqttSuccessError;
}

/**
 * @brief 手动重连mqtt客户端
 * ! 仅在未使能自动连接时，客户端断开连接时用户手动调用
 * ! 否则可能会造成内存泄漏
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttReconnect(RyanMqttClient_t *client)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttDisconnectState == RyanMqttGetClientState(client), RyanMqttConnectError, rlog_d);

	if (RyanMqttTrue == client->config.autoReconnectFlag)
	{
		return RyanMqttNoRescourceError;
	}

	platformThreadStart(client->config.userData, &client->mqttThread);
	return RyanMqttSuccessError;
}

static void RyanMqttClearSubSession(RyanMqttClient_t *client, uint16_t packetId, int32_t count,
				    MQTTSubscribeInfo_t *subscriptionList)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttAckHandler_t *userAckHandler = NULL;

	// 清除所有ack链表
	while (1)
	{
		RyanMqttAckHandler_t *ackHandler = NULL;
		result = RyanMqttAckListNodeFindByUserAckList(client, MQTT_PACKET_TYPE_SUBACK, packetId,
							      &userAckHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
			RyanMqttAckHandlerDestroy(client, userAckHandler);
			continue;
		}

		// 还有可能已经被添加到ack链表了
		result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_SUBACK, packetId, &ackHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler);
			continue;
		}

		break;
	}

	// 清除msg链表
	for (int32_t i = 0; i < count; i++)
	{
		RyanMqttMsgHandlerFindByPackId(client, subscriptionList[i].pTopicFilter,
					       subscriptionList[i].topicFilterLength, packetId, RyanMqttFalse);
	}
}

/**
 * @brief 订阅主题
 *
 * @param client
 * @param topic
 * @param qos
 * 服务端可以授予比订阅者要求的低一些的QoS等级，可在订阅成功回调函数中查看服务端给定的qos等级
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSubscribeMany(RyanMqttClient_t *client, int32_t count,
				      RyanMqttSubscribeData_t subscribeManyData[])
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId = 0;
	RyanMqttMsgHandler_t *msgHandler = NULL;
	RyanMqttMsgHandler_t *msgToListHandler = NULL;
	RyanMqttAckHandler_t *userAckHandler = NULL;

	MQTTStatus_t status = MQTTSuccess;
	MQTTFixedBuffer_t fixedBuffer = {0};
	size_t remainingLength = 0;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

	for (int32_t i = 0; i < count; i++)
	{
		RyanMqttCheck(NULL != subscribeManyData[i].topic, RyanMqttParamInvalidError, rlog_d);
		RyanMqttCheck(RyanMqttQos0 <= subscribeManyData[i].qos && RyanMqttQos2 >= subscribeManyData[i].qos,
			      RyanMqttParamInvalidError, rlog_d);
	}

	MQTTSubscribeInfo_t *subscriptionList = platformMemoryMalloc(sizeof(MQTTSubscribeInfo_t) * count);
	RyanMqttCheck(NULL != subscriptionList, RyanMqttParamInvalidError, rlog_d);
	memset(subscriptionList, 0, sizeof(MQTTSubscribeInfo_t) * count);
	for (int32_t i = 0; i < count; i++)
	{
		subscriptionList[i].qos = (MQTTQoS_t)subscribeManyData[i].qos;
		subscriptionList[i].pTopicFilter = subscribeManyData[i].topic;
		subscriptionList[i].topicFilterLength = subscribeManyData[i].topicLen;
	}

	{ // 获取数据包大小
		status = MQTT_GetSubscribePacketSize(subscriptionList, count, &remainingLength, &fixedBuffer.size);
		RyanMqttAssert(MQTTSuccess == status);

		// 申请数据包的空间
		fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
		RyanMqttCheckCode(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d,
				  { platformMemoryFree(subscriptionList); });

		// 序列化数据包
		packetId = RyanMqttGetNextPacketId(client);
		status = MQTT_SerializeSubscribe(subscriptionList, count, packetId, remainingLength, &fixedBuffer);
		RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, {
			platformMemoryFree(subscriptionList);
			platformMemoryFree(fixedBuffer.pBuffer);
		});
	}

	// ?mqtt空间接到suback时，会查找所有同名的然后删掉，这里不进行同名对比操作
	for (int32_t i = 0; i < count; i++)
	{
		// 创建msg包
		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, packetId,
						  (RyanMqttQos_e)subscriptionList[i].qos, &msgHandler);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

		result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_SUBACK, packetId, 0, NULL, msgHandler,
						  &userAckHandler, RyanMqttFalse);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, {
			RyanMqttMsgHandlerDestory(client, msgHandler);
			goto __exit;
		});

		RyanMqttAckListAddToUserAckList(client, userAckHandler);

		// 创建msg包,允许服务端在发送 SUBACK 报文之前就开始发送与订阅匹配的 PUBLISH 报文。
		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, packetId,
						  (RyanMqttQos_e)subscriptionList[i].qos, &msgToListHandler);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, {
			RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
			RyanMqttAckHandlerDestroy(client, userAckHandler);
			goto __exit;
		});
		RyanMqttMsgHandlerAddToMsgList(client,
					       msgToListHandler); // 将msg信息添加到订阅链表上
		continue;

__exit:
		RyanMqttClearSubSession(client, packetId, i, subscriptionList);

		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
		return RyanMqttNotEnoughMemError;
	}

	// 如果发送失败就清除ack链表,创建ack链表必须在发送前
	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
		RyanMqttClearSubSession(client, packetId, count, subscriptionList);

		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
	});

	platformMemoryFree(subscriptionList);
	platformMemoryFree(fixedBuffer.pBuffer);
	return result;
}

/**
 * @brief 订阅主题
 *
 * @param client
 * @param topic
 * @param qos
 * 服务端可以授予比订阅者要求的低一些的QoS等级，可在订阅成功回调函数中查看服务端给定的qos等级
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSubscribe(RyanMqttClient_t *client, char *topic, RyanMqttQos_e qos)
{
	RyanMqttSubscribeData_t subscribeManyData = {.qos = qos, .topic = topic, .topicLen = strlen(topic)};
	return RyanMqttSubscribeMany(client, 1, &subscribeManyData);
}

static void RyanMqttClearUnSubSession(RyanMqttClient_t *client, uint16_t packetId, int32_t count,
				      MQTTSubscribeInfo_t *subscriptionList)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttAckHandler_t *userAckHandler = NULL;

	// 清除所有ack链表
	while (1)
	{
		RyanMqttAckHandler_t *ackHandler = NULL;
		result = RyanMqttAckListNodeFindByUserAckList(client, MQTT_PACKET_TYPE_UNSUBACK, packetId,
							      &userAckHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
			RyanMqttAckHandlerDestroy(client, userAckHandler);
			continue;
		}

		// 还有可能已经被添加到ack链表了
		result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_UNSUBACK, packetId, &ackHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler);
			continue;
		}

		break;
	}
}

/**
 * @brief 取消订阅指定主题
 *
 * @param client
 * @param topic
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttUnSubscribeMany(RyanMqttClient_t *client, int32_t count,
					RyanMqttUnSubscribeData_t unSubscribeManyData[])
{
	RyanMqttError_e result = RyanMqttFailedError;
	uint16_t packetId;
	RyanMqttMsgHandler_t *subMsgHandler = NULL;
	RyanMqttMsgHandler_t *msgHandler = NULL;
	RyanMqttAckHandler_t *userAckHandler = NULL;
	MQTTStatus_t status = MQTTSuccess;
	MQTTFixedBuffer_t fixedBuffer = {0};
	size_t remainingLength = 0;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

	for (int32_t i = 0; i < count; i++)
	{
		RyanMqttCheck(NULL != unSubscribeManyData[i].topic, RyanMqttParamInvalidError, rlog_d);
	}

	MQTTSubscribeInfo_t *subscriptionList = platformMemoryMalloc(sizeof(MQTTSubscribeInfo_t) * count);
	RyanMqttCheck(NULL != subscriptionList, RyanMqttParamInvalidError, rlog_d);
	memset(subscriptionList, 0, sizeof(MQTTSubscribeInfo_t) * count);
	for (int32_t i = 0; i < count; i++)
	{
		subscriptionList[i].qos = (MQTTQoS_t)RyanMqttQos0; // 无效数据
		subscriptionList[i].pTopicFilter = unSubscribeManyData[i].topic;
		subscriptionList[i].topicFilterLength = unSubscribeManyData[i].topicLen;
	}

	// 获取数据包大小
	status = MQTT_GetUnsubscribePacketSize(subscriptionList, count, &remainingLength, &fixedBuffer.size);
	RyanMqttAssert(MQTTSuccess == status);

	// 申请数据包的空间
	fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
	RyanMqttCheckCode(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d,
			  { platformMemoryFree(subscriptionList); });

	// 序列化数据包
	packetId = RyanMqttGetNextPacketId(client);
	status = MQTT_SerializeUnsubscribe(subscriptionList, count, packetId, remainingLength, &fixedBuffer);
	RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, {
		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
	});

	// 查找当前主题是否已经订阅,没有订阅就取消发送
	for (int32_t i = 0; i < count; i++)
	{
		// !不判断是否订阅，统一都发送取消
		result = RyanMqttMsgHandlerFind(client, subscriptionList[i].pTopicFilter,
						subscriptionList[i].topicFilterLength, RyanMqttFalse, &subMsgHandler);
		if (RyanMqttSuccessError == result)
		{
			subscriptionList[i].qos = (MQTTQoS_t)subMsgHandler->qos;
		}

		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, RyanMqttMsgInvalidPacketId,
						  (RyanMqttQos_e)subscriptionList[i].qos, &msgHandler);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

		result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_UNSUBACK, packetId, 0, NULL, msgHandler,
						  &userAckHandler, RyanMqttFalse);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, {
			RyanMqttMsgHandlerDestory(client, msgHandler);
			goto __exit;
		});

		RyanMqttAckListAddToUserAckList(client, userAckHandler);
		continue;

__exit: {
	RyanMqttClearUnSubSession(client, packetId, i, subscriptionList);
	platformMemoryFree(subscriptionList);
	platformMemoryFree(fixedBuffer.pBuffer);
	return RyanMqttNotEnoughMemError;
}
	}

	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
		RyanMqttClearUnSubSession(client, packetId, count, subscriptionList);

		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
	});

	platformMemoryFree(subscriptionList);
	platformMemoryFree(fixedBuffer.pBuffer);
	return result;
}

/**
 * @brief 取消订阅指定主题
 *
 * @param client
 * @param topic
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttUnSubscribe(RyanMqttClient_t *client, char *topic)
{
	RyanMqttUnSubscribeData_t subscribeManyData = {.topic = topic, .topicLen = strlen(topic)};
	return RyanMqttUnSubscribeMany(client, 1, &subscribeManyData);
}

/**
 * @brief 客户端向服务端发送消息
 *
 * @param client
 * @param topic
 * @param payload
 * @param payloadLen
 * @param QOS
 * @param retain
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttPublish(RyanMqttClient_t *client, char *topic, char *payload, uint32_t payloadLen,
				RyanMqttQos_e qos, RyanMqttBool_e retain)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId = 0;
	MQTTStatus_t status = MQTTSuccess;
	MQTTFixedBuffer_t fixedBuffer = {0};
	size_t remainingLength = 0;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

	if (payloadLen > 0 && NULL == payload)
	{ // 报文支持有效载荷长度为0
		return RyanMqttParamInvalidError;
	}

	// 序列化pub发送包
	MQTTPublishInfo_t publishInfo = {
		.qos = (MQTTQoS_t)qos,
		.pTopicName = topic,
		.topicNameLength = strlen(topic),
		.pPayload = payload,
		.payloadLength = payloadLen,
		.retain = retain,
		.dup = 0,
	};

	// 获取数据包大小
	status = MQTT_GetPublishPacketSize(&publishInfo, &remainingLength, &fixedBuffer.size);
	RyanMqttAssert(MQTTSuccess == status);

	// 申请数据包的空间
	fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
	RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d);

	// qos0不需要packetId
	if (RyanMqttQos0 == qos)
	{
		packetId = 0;
	}
	else
	{
		packetId = RyanMqttGetNextPacketId(client);
	}

	// 序列化数据包
	status = MQTT_SerializePublish(&publishInfo, packetId, remainingLength, &fixedBuffer);
	RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d,
			  { platformMemoryFree(fixedBuffer.pBuffer); });

	if (RyanMqttQos0 == qos)
	{
		// 发送报文
		result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });
		platformMemoryFree(fixedBuffer.pBuffer);
	}
	else
	{
		RyanMqttMsgHandler_t *msgHandler = NULL;
		RyanMqttAckHandler_t *userAckHandler = NULL;
		// qos1 / qos2需要收到预期响应ack,否则数据将被重新发送
		result = RyanMqttMsgHandlerCreate(client, publishInfo.pTopicName, publishInfo.topicNameLength,
						  RyanMqttMsgInvalidPacketId, qos, &msgHandler);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });

		result = RyanMqttAckHandlerCreate(
			client, (RyanMqttQos1 == qos) ? MQTT_PACKET_TYPE_PUBACK : MQTT_PACKET_TYPE_PUBREC, packetId,
			fixedBuffer.size, fixedBuffer.pBuffer, msgHandler, &userAckHandler, RyanMqttTrue);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
			platformMemoryFree(fixedBuffer.pBuffer);
			RyanMqttMsgHandlerDestory(client, msgHandler);
		});

		// 一定要先加再send，要不可能返回消息会比这个更快执行呢
		RyanMqttAckListAddToUserAckList(client, userAckHandler);

		result = RyanMqttSendPacket(client, userAckHandler->packet, userAckHandler->packetLen);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
			RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
			RyanMqttAckHandlerDestroy(client, userAckHandler);
		});
	}

	return RyanMqttSuccessError;
}

/**
 * @brief 获取mqtt客户端状态
 *
 * @param client
 * @return RyanMqttState_e
 */
RyanMqttState_e RyanMqttGetState(RyanMqttClient_t *client)
{
	if (NULL == client)
	{
		return RyanMqttInvalidState;
	}

	return RyanMqttGetClientState(client);
}

/**
 * @brief 获取已订阅主题
 *
 * @param client
 * @param msgHandles 存放已订阅主题的空间
 * @param msgHandleSize  存放已订阅主题的空间大小个数
 * @param subscribeNum 函数内部返回已订阅主题的个数
 * @return RyanMqttState_e
 */
// todo RyanMqttGetSubscribe 会立即释放 msgHandleLock，同时提供原始内部 topic 指针。
// 如果另一个线程在这个调用返回后立即取消订阅，用户缓冲区中的指针将悬空
// 将主题字符串复制到调用者提供的存储空间中
RyanMqttError_e RyanMqttGetSubscribe(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles, int32_t msgHandleSize,
				     int32_t *subscribeNum)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanList_t *curr, *next;
	RyanMqttMsgHandler_t *msgHandler = NULL;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != subscribeNum, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(0 < msgHandleSize, RyanMqttParamInvalidError, rlog_d);

	*subscribeNum = 0;

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListForEachSafe(curr, next, &client->msgHandlerList)
	{
		if (*subscribeNum >= msgHandleSize)
		{
			result = RyanMqttNoRescourceError;
			goto __next;
		}

		msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);
		msgHandles[*subscribeNum].topic = msgHandler->topic;
		msgHandles[*subscribeNum].qos = msgHandler->qos;

		(*subscribeNum)++;
	}

__next:
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
	return result;
}

/**
 * @brief 获取已订阅主题个数
 *
 * @param client
 * @param subscribeTotalCount
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttGetSubscribeTotalCount(RyanMqttClient_t *client, int32_t *subscribeTotalCount)
{
	RyanList_t *curr, *next;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != subscribeTotalCount, RyanMqttParamInvalidError, rlog_d);

	*subscribeTotalCount = 0;

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListForEachSafe(curr, next, &client->msgHandlerList)
	{
		(*subscribeTotalCount)++;
	}
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
	return RyanMqttSuccessError;
}

/**
 * @brief 获取mqtt config
 * 使用完毕后，需要用户释放pclientConfig指针内容
 *
 * @param client
 * @param pclientConfig
 * @return RyanMqttError_e
 */
/* RyanMqttError_e RyanMqttGetConfig(RyanMqttClient_t *client,
RyanMqttClientConfig_t **pclientConfig)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClientConfig_t *clientConfig = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != pclientConfig, RyanMqttParamInvalidError, rlog_d);

    RyanMqttCheck(NULL != client->config, RyanMqttNoRescourceError);

    clientConfig = (RyanMqttClientConfig_t
*)platformMemoryMalloc(sizeof(RyanMqttClientConfig_t)); RyanMqttCheck(NULL !=
clientConfig, RyanMqttNotEnoughMemError);

    memcpy(clientConfig, client->config, sizeof(RyanMqttClientConfig_t));

    result = setConfigValue(&clientConfig->clientId, client->config->clientId);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    result = setConfigValue(&clientConfig->userName, client->config->userName);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    result = setConfigValue(&clientConfig->password, client->config->password);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    result = setConfigValue(&clientConfig->host, client->config->host);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    result = setConfigValue(&clientConfig->port, client->config->port);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    result = setConfigValue(&clientConfig->taskName, client->config->taskName);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    *pclientConfig = clientConfig;

    return RyanMqttSuccessError;
}
 */

//  todo 增加更多校验，比如判断心跳包和recv的关系
/**
 * @brief 设置mqtt config 这是很危险的操作，需要考虑mqtt
 * thread线程和用户线程的资源互斥
 *
 * 推荐在 RyanMqttStart函数前 / 非用户手动触发的事件回调函数中 / mqtt
 * thread处于挂起状态时调用 mqtt thread处于阻塞状态时调用此函数也是很危险的行为
 * 要保证mqtt线程和用户线程的资源互斥
 * 如果修改参数需要重新连接才生效的，这里set不会生效。比如 keepAlive
 *
 * !项目中用户不应频繁调用此函数
 * !
 * 此函数如果返回RyanMqttFailedError,需要立即停止mqtt客户端相关操作.因为操作失败此函数会调用RyanMqttDestroy()销毁客户端
 *
 * @param client
 * @param clientConfig
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != clientConfig->clientId, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != clientConfig->host, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != clientConfig->taskName, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(clientConfig->recvTimeout <= clientConfig->keepaliveTimeoutS * 1000 / 2,
		      RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(clientConfig->recvTimeout >= clientConfig->sendTimeout, RyanMqttParamInvalidError, rlog_d);

	result = setConfigValue(&client->config.clientId, clientConfig->clientId);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

	if (NULL == clientConfig->userName)
	{
		client->config.userName = NULL;
	}
	else
	{
		result = setConfigValue(&client->config.userName, clientConfig->userName);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });
	}

	if (NULL == clientConfig->password)
	{
		client->config.password = NULL;
	}
	else
	{
		result = setConfigValue(&client->config.password, clientConfig->password);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });
	}

	result = setConfigValue(&client->config.host, clientConfig->host);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

	result = setConfigValue(&client->config.taskName, clientConfig->taskName);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

	client->config.port = clientConfig->port;
	client->config.taskPrio = clientConfig->taskPrio;
	client->config.taskStack = clientConfig->taskStack;
	client->config.mqttVersion = clientConfig->mqttVersion;
	client->config.ackHandlerRepeatCountWarning = clientConfig->ackHandlerRepeatCountWarning;
	client->config.ackHandlerCountWarning = clientConfig->ackHandlerCountWarning;
	client->config.autoReconnectFlag = clientConfig->autoReconnectFlag;
	client->config.cleanSessionFlag = clientConfig->cleanSessionFlag;
	client->config.reconnectTimeout = clientConfig->reconnectTimeout;
	client->config.recvTimeout = clientConfig->recvTimeout;
	client->config.sendTimeout = clientConfig->sendTimeout;
	client->config.ackTimeout = clientConfig->ackTimeout;
	client->config.keepaliveTimeoutS = clientConfig->keepaliveTimeoutS;
	client->config.mqttEventHandle = clientConfig->mqttEventHandle;
	client->config.userData = clientConfig->userData;

	return RyanMqttSuccessError;

__exit:
	RyanMqttDestroy(client);
	return RyanMqttFailedError;
}

/**
 * @brief 设置遗嘱的配置信息
 * 此函数必须在发送connect报文前调用，因为遗嘱消息包含在connect报文中
 * 例如 RyanMqttStart前 / RyanMqttEventReconnectBefore事件中
 *
 * @param client
 * @param topicName
 * @param qos
 * @param retain
 * @param payload
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSetLwt(RyanMqttClient_t *client, char *topicName, char *payload, uint32_t payloadLen,
			       RyanMqttQos_e qos, RyanMqttBool_e retain)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(NULL != topicName, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, rlog_d);

	if (payloadLen > 0 && NULL == payload)
	{ // 报文支持有效载荷长度为0
		return RyanMqttParamInvalidError;
	}

	if (NULL != client->lwtOptions.topic)
	{
		platformMemoryFree(client->lwtOptions.topic);
	}

	if (NULL != client->lwtOptions.payload)
	{
		platformMemoryFree(client->lwtOptions.payload);
	}

	memset(&client->lwtOptions, 0, sizeof(lwtOptions_t));

	result = RyanMqttStringCopy(&client->lwtOptions.payload, payload, payloadLen);
	RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

	result = RyanMqttStringCopy(&client->lwtOptions.topic, topicName, strlen(topicName));
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
			  { platformMemoryFree(client->lwtOptions.payload); });

	client->lwtFlag = RyanMqttTrue;
	client->lwtOptions.qos = qos;
	client->lwtOptions.retain = retain;
	client->lwtOptions.payloadLen = payloadLen;

	return RyanMqttSuccessError;
}

/**
 * @brief 丢弃指定ack
 *
 * @param client
 * @param ackHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDiscardAckHandler(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttAckHandler_t *ackHandler = NULL;
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
	RyanMqttCheck(0 < packetId, RyanMqttParamInvalidError, rlog_d);

	// 删除pubrel记录
	platformMutexLock(client->config.userData, &client->ackHandleLock);
	result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttNoRescourceError, rlog_d, { goto __exit; });

	RyanMqttEventMachine(client, RyanMqttEventAckHandlerdiscard, (void *)ackHandler); // 回调函数

	RyanMqttAckListRemoveToAckList(client, ackHandler);
	RyanMqttAckHandlerDestroy(client, ackHandler);

__exit:
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);
	return result;
}

RyanMqttError_e RyanMqttRegisterEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->eventFlag |= eventId;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttCancelEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->eventFlag &= ~eventId;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return RyanMqttSuccessError;
}
