#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttClient.h"
#include "RyanMqttThread.h"
#include "RyanMqttUtil.h"
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
	int32_t restStrLen = (int32_t)RyanMqttStrlen(rest);

	if (NULL != *dest)
	{
		if ((int32_t)RyanMqttStrlen(*dest) == restStrLen && 0 == RyanMqttStrcmp(*dest, rest))
		{
			return RyanMqttSuccessError;
		}
	}

	char *tmp;
	RyanMqttError_e result = RyanMqttStringCopy(&tmp, (char *)rest, restStrLen);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

	if (NULL != *dest)
	{
		platformMemoryFree(*dest);
	}

	*dest = tmp;
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
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttCheck(NULL != pClient, RyanMqttParamInvalidError, RyanMqttLog_d);

	RyanMqttClient_t *client = (RyanMqttClient_t *)platformMemoryMalloc(sizeof(RyanMqttClient_t));
	RyanMqttCheck(NULL != client, RyanMqttNotEnoughMemError, RyanMqttLog_d);
	RyanMqttMemset(client, 0, sizeof(RyanMqttClient_t));

	client->packetId = 1; // 控制报文必须包含一个非零的 16 位报文标识符
	client->clientState = RyanMqttInitState;
	client->eventFlag = 0;
	client->ackHandlerCount = 0;

	RyanMqttBool_e criticalLockIsOk = RyanMqttFalse;
	RyanMqttBool_e sendLockIsOk = RyanMqttFalse;
	RyanMqttBool_e msgHandleLockIsOk = RyanMqttFalse;
	RyanMqttBool_e ackHandleLockIsOk = RyanMqttFalse;
	RyanMqttBool_e userSessionLockIsOk = RyanMqttFalse;
	RyanMqttBool_e networkIsOk = RyanMqttFalse;

	result = platformCriticalInit(client->config.userData, &client->criticalLock); // 初始化临界区
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	criticalLockIsOk = RyanMqttTrue;

	result = platformMutexInit(client->config.userData, &client->sendLock); // 初始化发送缓冲区互斥锁
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	sendLockIsOk = RyanMqttTrue;

	result = platformMutexInit(client->config.userData, &client->msgHandleLock);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	msgHandleLockIsOk = RyanMqttTrue;

	result = platformMutexInit(client->config.userData, &client->ackHandleLock);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	ackHandleLockIsOk = RyanMqttTrue;

	result = platformMutexInit(client->config.userData, &client->userSessionLock);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	userSessionLockIsOk = RyanMqttTrue;

	result = platformNetworkInit(client->config.userData, &client->network); // 网络接口初始化
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	// networkIsOk = RyanMqttTrue;

	RyanMqttListInit(&client->msgHandlerList);
	RyanMqttListInit(&client->ackHandlerList);
	RyanMqttListInit(&client->userAckHandlerList);

	RyanMqttSetClientState(client, RyanMqttInitState);

	*pClient = client;
	return RyanMqttSuccessError;

__exit:
	if (criticalLockIsOk)
	{
		platformCriticalDestroy(client->config.userData, &client->criticalLock);
	}

	if (sendLockIsOk)
	{
		platformMutexDestroy(client->config.userData, &client->sendLock);
	}

	if (msgHandleLockIsOk)
	{
		platformMutexDestroy(client->config.userData, &client->msgHandleLock);
	}

	if (ackHandleLockIsOk)
	{
		platformMutexDestroy(client->config.userData, &client->ackHandleLock);
	}

	if (userSessionLockIsOk)
	{
		platformMutexDestroy(client->config.userData, &client->userSessionLock);
	}

	if (networkIsOk)
	{
		platformNetworkClose(client->config.userData, &client->network);
		platformNetworkDestroy(client->config.userData, &client->network);
	}

	platformMemoryFree(client);
	return result;
}

/**
 * @brief 销毁mqtt客户端
 *  !用户线程直接删除mqtt线程是很危险的行为。所以这里设置标志位，稍后由mqtt线程自己释放所占有的资源。
 *  !mqtt删除自己的延时最大不会超过config里面 recvTimeout + 1秒
 *  !mqtt删除自己前会调用 RyanMqttEventDestroyBefore 事件回调
 *  !调用此函数后就不应该再对该客户端进行任何操作
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDestroy(RyanMqttClient_t *client)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->destroyFlag = RyanMqttTrue;
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
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttInitState == RyanMqttGetClientState(client), RyanMqttFailedError, RyanMqttLog_d);

	RyanMqttSetClientState(client, RyanMqttStartState);
	// 连接成功，需要初始化 MQTT 线程
	result = platformThreadInit(client->config.userData, &client->mqttThread, client->config.taskName,
				    RyanMqttThread, client, client->config.taskStack, client->config.taskPrio);
	RyanMqttCheckCode(RyanMqttSuccessError == result, RyanMqttNoRescourceError, RyanMqttLog_d,
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
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, RyanMqttLog_d);

	if (RyanMqttTrue == sendDiscFlag)
	{
		MQTTStatus_t status;
		MQTTFixedBuffer_t fixedBuffer;

		// 获取断开连接的数据包大小
		status = MQTT_GetDisconnectPacketSize(&fixedBuffer.size);
		RyanMqttAssert(MQTTSuccess == status);

		// 申请断开连接数据包的空间
		fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
		RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, RyanMqttLog_d);

		// 序列化断开连接数据包
		status = MQTT_SerializeDisconnect(&fixedBuffer);
		RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });

		// 发送断开连接数据包
		RyanMqttError_e result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
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
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttReconnect(RyanMqttClient_t *client)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttDisconnectState == RyanMqttGetClientState(client), RyanMqttConnectError, RyanMqttLog_d);

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
	RyanMqttClearAckSession(client, MQTT_PACKET_TYPE_SUBACK, packetId);
	// 清除msg链表
	RyanMqttMsgHandler_t tempMsgHandler;
	for (int32_t i = 0; i < count; i++)
	{
		tempMsgHandler.topic = (char *)subscriptionList[i].pTopicFilter;
		tempMsgHandler.topicLen = subscriptionList[i].topicFilterLength;
		tempMsgHandler.packetId = packetId;

		RyanMqttMsgHandlerFindAndDestroyByPackId(client, &tempMsgHandler, RyanMqttFalse);
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
	uint16_t packetId;
	RyanMqttMsgHandler_t *msgHandler;
	RyanMqttMsgHandler_t *msgToListHandler;
	RyanMqttAckHandler_t *userAckHandler;

	MQTTFixedBuffer_t fixedBuffer;

	// 校验参数合法性
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != subscribeManyData, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(count > 0, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, RyanMqttLog_d);
	for (int32_t i = 0; i < count; i++)
	{
		RyanMqttCheck(NULL != subscribeManyData[i].topic, RyanMqttParamInvalidError, RyanMqttLog_d);
		RyanMqttCheck(RyanMqttQos0 <= subscribeManyData[i].qos && RyanMqttQos2 >= subscribeManyData[i].qos,
			      RyanMqttParamInvalidError, RyanMqttLog_d);
	}

	// 申请 coreMqtt 支持的topic格式空间
	MQTTSubscribeInfo_t *subscriptionList = platformMemoryMalloc(sizeof(MQTTSubscribeInfo_t) * count);
	RyanMqttCheck(NULL != subscriptionList, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttMemset(subscriptionList, 0, sizeof(MQTTSubscribeInfo_t) * count);
	for (int32_t i = 0; i < count; i++)
	{
		subscriptionList[i].qos = (MQTTQoS_t)subscribeManyData[i].qos;
		subscriptionList[i].pTopicFilter = subscribeManyData[i].topic;
		subscriptionList[i].topicFilterLength = subscribeManyData[i].topicLen;
	}

	// 序列化数据包
	{
		size_t remainingLength;

		// 获取数据包大小
		MQTTStatus_t status =
			MQTT_GetSubscribePacketSize(subscriptionList, count, &remainingLength, &fixedBuffer.size);
		RyanMqttAssert(MQTTSuccess == status);

		// 申请数据包的空间
		fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
		RyanMqttCheckCode(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, RyanMqttLog_d,
				  { platformMemoryFree(subscriptionList); });

		// 序列化数据包
		packetId = RyanMqttGetNextPacketId(client);
		status = MQTT_SerializeSubscribe(subscriptionList, count, packetId, remainingLength, &fixedBuffer);
		RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d, {
			platformMemoryFree(subscriptionList);
			platformMemoryFree(fixedBuffer.pBuffer);
		});
	}

	// 创建每个msg主题的ack节点
	// ?mqtt空间收到服务器的suback时，会查找所有同名的然后删掉，所以这里不进行同名对比操作
	for (int32_t i = 0; i < count; i++)
	{
		// 创建msg包
		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, packetId,
						  (RyanMqttQos_e)subscriptionList[i].qos, NULL, &msgHandler);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

		result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_SUBACK, packetId, 0, NULL, msgHandler,
						  &userAckHandler, RyanMqttFalse);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
			RyanMqttMsgHandlerDestroy(client, msgHandler);
			goto __exit;
		});

		RyanMqttAckListAddToUserAckList(client, userAckHandler);
		continue;

__exit:
		RyanMqttClearAckSession(client, MQTT_PACKET_TYPE_SUBACK, packetId);

		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
		return RyanMqttNotEnoughMemError;
	}

	for (int32_t i = 0; i < count; i++)
	{
		// 创建msg包,允许服务端在发送 SUBACK 报文之前就开始发送与订阅匹配的 PUBLISH 报文。
		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, packetId,
						  (RyanMqttQos_e)subscriptionList[i].qos, NULL, &msgToListHandler);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
			RyanMqttClearSubSession(client, packetId, i, subscriptionList);

			platformMemoryFree(subscriptionList);
			platformMemoryFree(fixedBuffer.pBuffer);
		});

		// 将msg信息添加到订阅链表上
		RyanMqttMsgHandlerAddToMsgList(client, msgToListHandler);
	}

	// 发送订阅主题包
	// 如果发送失败就清除ack链表,创建ack链表必须在发送前
	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
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
	RyanMqttSubscribeData_t subscribeManyData = {.qos = qos, .topic = topic, .topicLen = RyanMqttStrlen(topic)};
	return RyanMqttSubscribeMany(client, 1, &subscribeManyData);
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
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	RyanMqttMsgHandler_t *subMsgHandler;
	RyanMqttMsgHandler_t *msgHandler;
	RyanMqttAckHandler_t *userAckHandler;
	MQTTFixedBuffer_t fixedBuffer;

	// 校验参数合法性
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != unSubscribeManyData, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(count > 0, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, RyanMqttLog_d);
	for (int32_t i = 0; i < count; i++)
	{
		RyanMqttCheck(NULL != unSubscribeManyData[i].topic, RyanMqttParamInvalidError, RyanMqttLog_d);
	}

	// 申请 coreMqtt 支持的topic格式空间
	MQTTSubscribeInfo_t *subscriptionList = platformMemoryMalloc(sizeof(MQTTSubscribeInfo_t) * count);
	RyanMqttCheck(NULL != subscriptionList, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttMemset(subscriptionList, 0, sizeof(MQTTSubscribeInfo_t) * count);
	for (int32_t i = 0; i < count; i++)
	{
		subscriptionList[i].qos = (MQTTQoS_t)RyanMqttQos0; // 无效数据
		subscriptionList[i].pTopicFilter = unSubscribeManyData[i].topic;
		subscriptionList[i].topicFilterLength = unSubscribeManyData[i].topicLen;
	}

	// 序列化数据包
	{
		size_t remainingLength;

		// 获取数据包大小
		MQTTStatus_t status =
			MQTT_GetUnsubscribePacketSize(subscriptionList, count, &remainingLength, &fixedBuffer.size);
		RyanMqttAssert(MQTTSuccess == status);

		// 申请数据包的空间
		fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
		RyanMqttCheckCode(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, RyanMqttLog_d,
				  { platformMemoryFree(subscriptionList); });

		// 序列化数据包
		packetId = RyanMqttGetNextPacketId(client);
		status = MQTT_SerializeUnsubscribe(subscriptionList, count, packetId, remainingLength, &fixedBuffer);
		RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d, {
			platformMemoryFree(subscriptionList);
			platformMemoryFree(fixedBuffer.pBuffer);
		});
	}

	// 查找当前主题是否已经订阅,没有订阅就取消发送
	for (int32_t i = 0; i < count; i++)
	{
		// ?不判断是否订阅，统一都发送取消
		RyanMqttMsgHandler_t tempMsgHandler = {.topic = (char *)subscriptionList[i].pTopicFilter,
						       .topicLen = subscriptionList[i].topicFilterLength};
		result = RyanMqttMsgHandlerFind(client, &tempMsgHandler, RyanMqttFalse, &subMsgHandler);
		if (RyanMqttSuccessError == result)
		{
			subscriptionList[i].qos = (MQTTQoS_t)subMsgHandler->qos;
		}

		result = RyanMqttMsgHandlerCreate(client, subscriptionList[i].pTopicFilter,
						  subscriptionList[i].topicFilterLength, RyanMqttMsgInvalidPacketId,
						  (RyanMqttQos_e)subscriptionList[i].qos, NULL, &msgHandler);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

		result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_UNSUBACK, packetId, 0, NULL, msgHandler,
						  &userAckHandler, RyanMqttFalse);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
			RyanMqttMsgHandlerDestroy(client, msgHandler);
			goto __exit;
		});

		RyanMqttAckListAddToUserAckList(client, userAckHandler);
		continue;

__exit:
		RyanMqttClearAckSession(client, MQTT_PACKET_TYPE_UNSUBACK, packetId);
		platformMemoryFree(subscriptionList);
		platformMemoryFree(fixedBuffer.pBuffer);
		return RyanMqttNotEnoughMemError;
	}

	// 发送取消订阅主题包
	// 如果发送失败就清除ack链表,创建ack链表必须在发送前
	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
		RyanMqttClearAckSession(client, MQTT_PACKET_TYPE_UNSUBACK, packetId);

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
	RyanMqttUnSubscribeData_t subscribeManyData = {.topic = topic, .topicLen = RyanMqttStrlen(topic)};
	return RyanMqttUnSubscribeMany(client, 1, &subscribeManyData);
}

RyanMqttError_e RyanMqttPublishAndUserData(RyanMqttClient_t *client, char *topic, uint16_t topicLen, char *payload,
					   uint32_t payloadLen, RyanMqttQos_e qos, RyanMqttBool_e retain,
					   void *userData)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	MQTTStatus_t status;
	MQTTFixedBuffer_t fixedBuffer;
	size_t remainingLength;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != topic && topicLen > 0, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, RyanMqttLog_d);

	// 报文支持有效载荷长度为0
	if (payloadLen > 0 && NULL == payload)
	{
		return RyanMqttParamInvalidError;
	}

	// 序列化pub发送包
	MQTTPublishInfo_t publishInfo = {
		.qos = (MQTTQoS_t)qos,
		.pTopicName = topic,
		.topicNameLength = topicLen,
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
	RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, RyanMqttLog_d);

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
	RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d,
			  { platformMemoryFree(fixedBuffer.pBuffer); });

	if (RyanMqttQos0 == qos)
	{
		// 发送报文
		result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });
		platformMemoryFree(fixedBuffer.pBuffer);
	}
	else
	{
		RyanMqttMsgHandler_t *msgHandler;
		RyanMqttAckHandler_t *userAckHandler;
		// qos1 / qos2需要收到预期响应ack,否则数据将被重新发送
		result = RyanMqttMsgHandlerCreate(client, publishInfo.pTopicName, publishInfo.topicNameLength,
						  RyanMqttMsgInvalidPacketId, qos, userData, &msgHandler);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
				  { platformMemoryFree(fixedBuffer.pBuffer); });

		result = RyanMqttAckHandlerCreate(
			client, (RyanMqttQos1 == qos) ? MQTT_PACKET_TYPE_PUBACK : MQTT_PACKET_TYPE_PUBREC, packetId,
			fixedBuffer.size, fixedBuffer.pBuffer, msgHandler, &userAckHandler, RyanMqttTrue);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
			platformMemoryFree(fixedBuffer.pBuffer);
			RyanMqttMsgHandlerDestroy(client, msgHandler);
		});

		// 一定要先加再send，要不可能返回消息会比这个更快执行呢
		RyanMqttAckListAddToUserAckList(client, userAckHandler);

		result = RyanMqttSendPacket(client, userAckHandler->packet, userAckHandler->packetLen);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
			RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
			RyanMqttAckHandlerDestroy(client, userAckHandler);
		});
	}

	return RyanMqttSuccessError;
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
	return RyanMqttPublishAndUserData(client, topic, RyanMqttStrlen(topic), payload, payloadLen, qos, retain, NULL);
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
 * !此函数是非线程安全的，已不推荐使用
 * !请使用 RyanMqttGetSubscribeSafe 代替
 * !如果另一个线程在这个调用返回后立即取消订阅，topic将指向非法内存
 *
 * @param client
 * @param msgHandles 存放已订阅主题的空间
 * @param msgHandleSize  存放已订阅主题的空间大小个数
 * @param subscribeNum 函数内部返回已订阅主题的个数
 * @return RyanMqttState_e
 */
RyanMqttError_e RyanMqttGetSubscribe(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles, int32_t msgHandleSize,
				     int32_t *subscribeNum)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttList_t *curr, *next;
	RyanMqttMsgHandler_t *msgHandler;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != subscribeNum, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(0 < msgHandleSize, RyanMqttParamInvalidError, RyanMqttLog_d);

	*subscribeNum = 0;

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
	{
		if (*subscribeNum >= msgHandleSize)
		{
			result = RyanMqttNoRescourceError;
			break;
		}

		msgHandler = RyanMqttListEntry(curr, RyanMqttMsgHandler_t, list);
		msgHandles[*subscribeNum].topic = msgHandler->topic;
		msgHandles[*subscribeNum].qos = msgHandler->qos;

		(*subscribeNum)++;
	}
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);

	return result;
}

/**
 * @brief 安全的获取已订阅主题列表
 *
 * @param client
 * @param msgHandles
 * @param subscribeNum
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttGetSubscribeSafe(RyanMqttClient_t *client, RyanMqttMsgHandler_t **msgHandles,
					 int32_t *subscribeNum)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttList_t *curr, *next;
	RyanMqttMsgHandler_t *msgHandler;
	int32_t subscribeTotal;

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != subscribeNum, RyanMqttParamInvalidError, RyanMqttLog_d);

	RyanMqttGetSubscribeTotalCount(client, &subscribeTotal);
	if (0 == subscribeTotal)
	{
		*msgHandles = NULL;
		*subscribeNum = 0;
		return RyanMqttSuccessError;
	}

	RyanMqttMsgHandler_t *msgHandlerArr = platformMemoryMalloc(sizeof(RyanMqttMsgHandler_t) * subscribeTotal);
	if (NULL == msgHandlerArr)
	{
		result = RyanMqttNotEnoughMemError;
		goto __exit;
	}

	int32_t subscribeCount = 0;
	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
	{
		if (subscribeCount >= subscribeTotal)
		{
			break;
		}

		msgHandler = RyanMqttListEntry(curr, RyanMqttMsgHandler_t, list);
		msgHandlerArr[subscribeCount].topic = (char *)platformMemoryMalloc(msgHandler->topicLen + 1);
		if (NULL == msgHandlerArr[subscribeCount].topic)
		{
			platformMutexUnLock(client->config.userData, &client->msgHandleLock);
			RyanMqttSafeFreeSubscribeResources(msgHandlerArr, subscribeCount);
			result = RyanMqttNotEnoughMemError;
			goto __exit;
		}

		RyanMqttMemcpy(msgHandlerArr[subscribeCount].topic, msgHandler->topic, msgHandler->topicLen);
		msgHandlerArr[subscribeCount].topic[msgHandler->topicLen] = '\0';
		msgHandlerArr[subscribeCount].qos = msgHandler->qos;
		subscribeCount++;
	}
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
	*msgHandles = msgHandlerArr;
	*subscribeNum = subscribeCount;

__exit:
	return result;
}

/**
 * @brief 安全释放订阅主题列表
 *
 * @param msgHandles
 * @param subscribeNum
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSafeFreeSubscribeResources(RyanMqttMsgHandler_t *msgHandles, int32_t subscribeNum)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(subscribeNum > 0, RyanMqttParamInvalidError, RyanMqttLog_d);

	for (int32_t i = 0; i < subscribeNum; i++)
	{
		platformMemoryFree(msgHandles[i].topic);
	}

	platformMemoryFree(msgHandles);

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
	RyanMqttList_t *curr, *next;
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != subscribeTotalCount, RyanMqttParamInvalidError, RyanMqttLog_d);

	*subscribeTotalCount = 0;

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
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

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
    RyanMqttCheck(NULL != pclientConfig, RyanMqttParamInvalidError, RyanMqttLog_d);

    RyanMqttCheck(NULL != client->config, RyanMqttNoRescourceError);

    clientConfig = (RyanMqttClientConfig_t
*)platformMemoryMalloc(sizeof(RyanMqttClientConfig_t)); RyanMqttCheck(NULL !=
clientConfig, RyanMqttNotEnoughMemError);

    RyanMqttMemcpy(clientConfig, client->config, sizeof(RyanMqttClientConfig_t));

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

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != clientConfig->clientId, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != clientConfig->host, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != clientConfig->taskName, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(clientConfig->recvTimeout <= clientConfig->keepaliveTimeoutS * 1000 / 2,
		      RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(clientConfig->recvTimeout >= clientConfig->sendTimeout, RyanMqttParamInvalidError, RyanMqttLog_d);

	result = setConfigValue(&client->config.clientId, clientConfig->clientId);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

	if (NULL == clientConfig->userName)
	{
		client->config.userName = NULL;
	}
	else
	{
		result = setConfigValue(&client->config.userName, clientConfig->userName);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	}

	if (NULL == clientConfig->password)
	{
		client->config.password = NULL;
	}
	else
	{
		result = setConfigValue(&client->config.password, clientConfig->password);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	}

	result = setConfigValue(&client->config.host, clientConfig->host);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

	result = setConfigValue(&client->config.taskName, clientConfig->taskName);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

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
	return result;
}

/**
 * @brief 设置遗嘱的配置信息
 * 此函数必须在发送connect报文前调用，因为遗嘱消息包含在connect报文中
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

	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(NULL != topicName, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttConnectState != RyanMqttGetClientState(client), RyanMqttFailedError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, RyanMqttLog_d);

	// 报文支持有效载荷长度为0
	if (payloadLen > 0 && NULL == payload)
	{
		return RyanMqttParamInvalidError;
	}

	platformMutexLock(client->config.userData, &client->userSessionLock);
	if (NULL == client->lwtOptions)
	{
		client->lwtOptions = (lwtOptions_t *)platformMemoryMalloc(sizeof(lwtOptions_t));
		RyanMqttCheckCodeNoReturn(NULL != client->lwtOptions, RyanMqttNotEnoughMemError, RyanMqttLog_d, {
			result = RyanMqttNotEnoughMemError;
			goto __exit;
		});
	}
	else
	{
		if (NULL != client->lwtOptions->topic)
		{
			platformMemoryFree(client->lwtOptions->topic);
		}

		if (NULL != client->lwtOptions->payload)
		{
			platformMemoryFree(client->lwtOptions->payload);
		}
	}

	RyanMqttMemset(client->lwtOptions, 0, sizeof(lwtOptions_t));

	if (payloadLen > 0)
	{
		result = RyanMqttStringCopy(&client->lwtOptions->payload, payload, payloadLen);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	}
	else
	{
		client->lwtOptions->payload = NULL;
	}

	result = RyanMqttStringCopy(&client->lwtOptions->topic, topicName, RyanMqttStrlen(topicName));
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });

	client->lwtOptions->lwtFlag = RyanMqttTrue;
	client->lwtOptions->qos = qos;
	client->lwtOptions->retain = retain;
	client->lwtOptions->payloadLen = payloadLen;
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
	return RyanMqttSuccessError;

__exit:
	if (NULL != client->lwtOptions)
	{
		if (NULL != client->lwtOptions->topic)
		{
			platformMemoryFree(client->lwtOptions->topic);
		}

		if (NULL != client->lwtOptions->payload)
		{
			platformMemoryFree(client->lwtOptions->payload);
		}

		platformMemoryFree(client->lwtOptions);
		client->lwtOptions = NULL;
	}
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
	return result;
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
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);
	RyanMqttCheck(0 < packetId, RyanMqttParamInvalidError, RyanMqttLog_d);

	// 删除pubrel记录
	platformMutexLock(client->config.userData, &client->ackHandleLock);
	result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	RyanMqttAckListRemoveToAckList(client, ackHandler);

__exit:
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	if (RyanMqttSuccessError == result)
	{
		RyanMqttEventMachine(client, RyanMqttEventAckHandlerDiscard, (void *)ackHandler); // 回调函数
		RyanMqttAckHandlerDestroy(client, ackHandler);
	}
	return result;
}

RyanMqttError_e RyanMqttRegisterEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->eventFlag |= eventId;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttCancelEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->eventFlag &= ~eventId;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttGetKeepAliveRemain(RyanMqttClient_t *client, uint32_t *keepAliveRemain)
{
	RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, RyanMqttLog_d);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	*keepAliveRemain = RyanMqttTimerRemain(&client->keepaliveTimer);
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return RyanMqttSuccessError;
}
