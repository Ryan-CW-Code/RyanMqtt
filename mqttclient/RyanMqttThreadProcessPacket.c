#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug)  // 日志打印等级

#include "RyanMqttThread.h"
#include "RyanMqttLog.h"
#include "RyanMqttUtil.h"

/**
 * @brief qos1或者qos2接收消息成功确认处理
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubackAndPubcompPacketHandler(RyanMqttClient_t *client,
							     MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);

	// 反序列化ack包
	MQTTStatus_t status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// 可能会多次收到 puback / pubcomp,仅在首次收到时触发发布成功回调函数
	result = RyanMqttAckListNodeFind(client, pIncomingPacket->type & 0xF0U, packetId, &ackHandler);
	RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
		RyanMqttLog_i("packetType: %02x, packetId: %d", pIncomingPacket->type & 0xF0U, packetId);
	});

	RyanMqttEventMachine(client, RyanMqttEventPublished, (void *)ackHandler); // 回调函数

	RyanMqttAckListRemoveToAckList(client, ackHandler);
	RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
	return result;
}

/**
 * @brief 发布释放处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubrelPacketHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);

	// 反序列化ack包
	MQTTStatus_t status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// 删除pubrel记录
	result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREL, packetId, &ackHandler);
	if (RyanMqttSuccessError == result)
	{
		RyanMqttAckListRemoveToAckList(client, ackHandler);
		RyanMqttAckHandlerDestroy(client, ackHandler);
	}

	// 制作确认数据包并发送
	uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
	MQTTFixedBuffer_t fixedBuffer = {.pBuffer = buffer, .size = sizeof(buffer)};

	// 序列化ack数据包
	status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBCOMP, packetId);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// 每次收到PUBREL都返回消息
	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

	return RyanMqttSuccessError;
}

/**
 * @brief 发布收到处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubrecPacketHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	RyanMqttAckHandler_t *ackHandlerPubrec;

	RyanMqttAssert(NULL != client);

	// 反序列化ack包
	MQTTStatus_t status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// 每次收到PUBREC都返回ack,确保服务器可以认为数据包被发送了
	uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
	MQTTFixedBuffer_t fixedBuffer = {.pBuffer = buffer, .size = sizeof(buffer)};

	// 序列化ack数据包
	status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBREL, packetId);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
	result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREC, packetId, &ackHandlerPubrec);
	if (RyanMqttSuccessError == result)
	{
		RyanMqttMsgHandler_t *msgHandler;
		RyanMqttAckHandler_t *ackHandler;

		// 查找ack链表是否存在pubcomp报文,不存在表示首次接收到
		result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBCOMP, packetId, &ackHandler);
		if (RyanMqttSuccessError != result)
		{
			// 首次收到消息
			result = RyanMqttMsgHandlerCreate(client, ackHandlerPubrec->msgHandler->topic,
							  ackHandlerPubrec->msgHandler->topicLen,
							  RyanMqttMsgInvalidPacketId, ackHandlerPubrec->msgHandler->qos,
							  ackHandlerPubrec->msgHandler->userData, &msgHandler);
			RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

			result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_PUBCOMP, packetId,
							  MQTT_PUBLISH_ACK_PACKET_SIZE, fixedBuffer.pBuffer, msgHandler,
							  &ackHandler, RyanMqttFalse);
			RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
					  { RyanMqttMsgHandlerDestroy(client, msgHandler); });
			RyanMqttAckListAddToAckList(client, ackHandler);

			RyanMqttAckListRemoveToAckList(client, ackHandlerPubrec);
			RyanMqttAckHandlerDestroy(client, ackHandlerPubrec);
		}
		// 出现pubrec和pubcomp同时存在的情况,清除pubrec。理论上不会出现（冗余措施）
		else
		{
			RyanMqttAckListRemoveToAckList(client, ackHandlerPubrec);
			RyanMqttAckHandlerDestroy(client, ackHandlerPubrec);
		}
	}

	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
	RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

	return result;
}

/**
 * @brief 收到服务器发布消息处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPublishPacketHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	uint16_t packetId;
	RyanMqttMsgData_t msgData;
	RyanMqttMsgHandler_t *msgHandler;

	RyanMqttAssert(NULL != client);

	{
		// 反系列化 publish 消息
		MQTTPublishInfo_t publishInfo;
		MQTTStatus_t status = MQTT_DeserializePublish(pIncomingPacket, &packetId, &publishInfo);
		RyanMqttCheck(MQTTSuccess == status, RyanMqttDeserializePacketError, RyanMqttLog_d);

		msgData.topic = (char *)publishInfo.pTopicName;
		msgData.topicLen = publishInfo.topicNameLength;
		msgData.packetId = packetId;
		msgData.payload = (char *)publishInfo.pPayload;
		msgData.payloadLen = publishInfo.payloadLength;
		msgData.qos = (RyanMqttQos_e)publishInfo.qos;
		msgData.retained = publishInfo.retain;
		msgData.dup = publishInfo.dup;

		// 查看订阅列表是否包含此消息主题,进行通配符匹配。不包含就直接退出在一定程度上可以防止恶意攻击
		RyanMqttMsgHandler_t tempMsgHandler = {.topic = msgData.topic, .topicLen = msgData.topicLen};
		result = RyanMqttMsgHandlerFind(client, &tempMsgHandler, RyanMqttTrue, &msgHandler);
		RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
				  { RyanMqttLog_w("主题不匹配: %.*s", msgData.topicLen, msgData.topic); });
	}

	switch (msgData.qos)
	{
	case RyanMqttQos0: RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData); break;

	case RyanMqttQos1: {
		// 先分发消息，再回答ack
		RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);

		uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
		MQTTFixedBuffer_t fixedBuffer = {.pBuffer = buffer, .size = sizeof(buffer)};

		// 序列化ack数据包
		MQTTStatus_t status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBACK, packetId);
		RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

		result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
		RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);
	}

	break;

	case RyanMqttQos2: // qos2采用方法B
	{
		RyanMqttAckHandler_t *ackHandler;
		uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
		MQTTFixedBuffer_t fixedBuffer = {.pBuffer = buffer, .size = sizeof(buffer)};

		// !序列化ack数据包,必须先执行，因为创建ack需要用到这个报文
		MQTTStatus_t status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBREC, packetId);
		RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

		// 上面代码不太可能出错，出错后就让服务器重新发送吧
		// 收到publish就期望收到PUBREL，如果PUBREL报文已经存在说明不是首次收到publish,
		// 不进行qos2 PUBREC消息处理
		result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREL, msgData.packetId, &ackHandler);
		if (RyanMqttSuccessError != result)
		{
			result = RyanMqttMsgHandlerCreate(client, msgData.topic, msgData.topicLen,
							  RyanMqttMsgInvalidPacketId, msgData.qos, NULL, &msgHandler);
			RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

			result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_PUBREL, msgData.packetId,
							  MQTT_PUBLISH_ACK_PACKET_SIZE, fixedBuffer.pBuffer, msgHandler,
							  &ackHandler, RyanMqttFalse);
			RyanMqttCheckCode(RyanMqttSuccessError == result, result, RyanMqttLog_d,
					  { RyanMqttMsgHandlerDestroy(client, msgHandler); });
			RyanMqttAckListAddToAckList(client, ackHandler);

			RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
		}

		result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
		RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);
	}

	break;

	default: RyanMqttLog_w("Unhandled QoS level: %d", msgData.qos); break;
	}

	return result;
}

/**
 * @brief 订阅确认处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttSubackHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	uint16_t packetId;
	RyanMqttMsgHandler_t *msgHandler;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttList_t *curr, *next;
	RyanMqttAssert(NULL != client);

	// 反序列化ack包，MQTTSuccess和MQTTServerRefused都是成功的
	MQTTStatus_t status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
	RyanMqttCheck(MQTTSuccess == status || MQTTServerRefused == status, RyanMqttDeserializePacketError,
		      RyanMqttLog_d);

	// 检查ack的msgCount和返回消息的msgCount是否一致
	{
		// MQTT_DeserializeAck会保证 pIncomingPacket->remainingLength >= 3
		uint32_t statusCount = pIncomingPacket->remainingLength - sizeof(uint16_t);
		uint32_t ackMsgCount = 0;

		// ?使用ack或msg遍历都行，使用msg更容易测试出问题，遍历性能也会更好一些
		platformMutexLock(client->config.userData, &client->msgHandleLock);
		RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
		{
			msgHandler = RyanMqttListEntry(curr, RyanMqttMsgHandler_t, list);

			if (packetId == msgHandler->packetId)
			{
				ackMsgCount++;
			}
		}
		platformMutexUnLock(client->config.userData, &client->msgHandleLock);

		// 服务区回复的ack数和记录的ack数不一致就清除所有ack
		RyanMqttCheckCode(ackMsgCount == statusCount, RyanMqttNoRescourceError, RyanMqttLog_d, {
			RyanMqttClearAckSession(client, MQTT_PACKET_TYPE_SUBACK, packetId);
			platformMutexLock(client->config.userData, &client->msgHandleLock);
			RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
			{
				msgHandler = RyanMqttListEntry(curr, RyanMqttMsgHandler_t, list);

				if (packetId == msgHandler->packetId)
				{
					RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
					RyanMqttMsgHandlerDestroy(client, msgHandler);
				}
			}
			platformMutexUnLock(client->config.userData, &client->msgHandleLock);
		});
	}

	RyanMqttQos_e subscriptionQos;
	uint32_t ackMsgIndex = 0;
	const uint8_t *pStatusStart = &pIncomingPacket->pRemainingData[sizeof(uint16_t)];

	// todo 这里效率非常低，订阅属于用的少的功能，暂时可以接受
	// 查找ack句柄
	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->ackHandlerList)
	{
		ackHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);

		if (packetId != ackHandler->packetId || MQTT_PACKET_TYPE_SUBACK != ackHandler->packetType)
		{
			continue;
		}

		// 查找同名订阅并删除,保证订阅主题列表只有一个最新的
		RyanMqttMsgHandlerFindAndDestroyByPackId(client, ackHandler->msgHandler, RyanMqttTrue);

		// 到这里就可以保证没有同名订阅了
		// 查找之前记录的topic句柄，根据服务器授权Qos进行更新
		RyanMqttError_e result =
			RyanMqttMsgHandlerFind(client, ackHandler->msgHandler, RyanMqttFalse, &msgHandler);

		// 几乎不可能，可以查找到 ackHandler 就一定有 msgHandler
		// 没有的话可以打印一条信息吧
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { continue; });

		// 解析服务端授权 QoS（0,1,2）或失败(0x80)
		subscriptionQos = pStatusStart[ackMsgIndex];
		ackMsgIndex++;
		switch (subscriptionQos)
		{
		case RyanMqttQos0:
		case RyanMqttQos1:
		case RyanMqttQos2:

			// 到这里说明订阅成功，更新 QoS 并清除临时 packetId
			platformMutexLock(client->config.userData, &client->msgHandleLock);
			msgHandler->qos = subscriptionQos;
			msgHandler->packetId = RyanMqttMsgInvalidPacketId;
			platformMutexUnLock(client->config.userData, &client->msgHandleLock);

			RyanMqttEventMachine(client, RyanMqttEventSubscribed, (void *)msgHandler); // mqtt回调函数
			break;

		case RyanMqttSubFail:
		default:
			// 订阅失败，服务器拒绝；删除并通知失败
			RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
			RyanMqttMsgHandlerDestroy(client, msgHandler);

			// mqtt事件回调
			RyanMqttEventMachine(client, RyanMqttEventSubscribedFailed, (void *)ackHandler->msgHandler);
			break;
		}

		RyanMqttAckListRemoveToAckList(client, ackHandler);
		RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
	}
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 取消订阅确认处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttUnSubackHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttMsgHandler_t *subMsgHandler;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttList_t *curr, *next;
	uint16_t packetId;

	RyanMqttAssert(NULL != client);

	// 反序列化ack包
	MQTTStatus_t status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
	RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

	// todo 这里效率非常低，订阅属于用的少的功能，暂时可以接受
	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->ackHandlerList)
	{
		ackHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);

		if ((packetId != ackHandler->packetId) || (MQTT_PACKET_TYPE_UNSUBACK != ackHandler->packetType))
		{
			continue;
		}

		// 查找当前主题是否已经订阅,进行取消订阅
		result = RyanMqttMsgHandlerFind(client, ackHandler->msgHandler, RyanMqttFalse, &subMsgHandler);

		if (RyanMqttSuccessError == result)
		{
			ackHandler->msgHandler->qos = subMsgHandler->qos;
			RyanMqttMsgHandlerRemoveToMsgList(client, subMsgHandler);
			RyanMqttMsgHandlerDestroy(client, subMsgHandler);
		}

		// mqtt事件回调
		RyanMqttEventMachine(client, RyanMqttEventUnSubscribed, (void *)ackHandler->msgHandler);

		RyanMqttAckListRemoveToAckList(client, ackHandler);
		RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
	}
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 将用户空间的ack链表搬到mqtt线程空间
 *
 * @param client
 */
static void RyanMqttSyncUserAckHandle(RyanMqttClient_t *client)
{
	RyanMqttAckHandler_t *userAckHandler;
	RyanMqttList_t *curr, *next;

	platformMutexLock(client->config.userData, &client->userSessionLock);
	RyanMqttListForEachSafe(curr, next, &client->userAckHandlerList)
	{
		// 获取此节点的结构体
		userAckHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);
		RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
		RyanMqttAckListAddToAckList(client, userAckHandler);
	}
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
}

RyanMqttError_e RyanMqttGetPacketInfo(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttAssert(NULL != client);

	NetworkContext_t pNetworkContext = {.client = client};
	// todo 可以考虑增加包大小限制，目前不准备加
	MQTTStatus_t status =
		MQTT_GetIncomingPacketTypeAndLength(coreMqttTransportRecv, &pNetworkContext, pIncomingPacket);

	// 先同步用户接口的ack链表
	RyanMqttSyncUserAckHandle(client);

	if (MQTTSuccess == status)
	{
		// 申请断开连接数据包的空间
		if (pIncomingPacket->remainingLength > 0)
		{
			pIncomingPacket->pRemainingData = platformMemoryMalloc(pIncomingPacket->remainingLength);
			RyanMqttCheck(NULL != pIncomingPacket->pRemainingData, RyanMqttNoRescourceError, RyanMqttLog_d);
		}
	}
	else if (MQTTNoDataAvailable == status)
	{
		return RyanMqttRecvPacketTimeOutError;
	}
	else
	{
		RyanMqttLog_e("获取包长度失败");
		return RyanMqttFailedError;
	}

	// 3.读取mqtt载荷数据并放到读取缓冲区
	if (pIncomingPacket->remainingLength > 0)
	{
		result = RyanMqttRecvPacket(client, pIncomingPacket->pRemainingData, pIncomingPacket->remainingLength);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, { goto __exit; });
	}

	return result;

__exit:
	if (NULL != pIncomingPacket->pRemainingData)
	{
		platformMemoryFree(pIncomingPacket->pRemainingData);
		pIncomingPacket->pRemainingData = NULL;
	}

	return result;
}

/**
 * @brief mqtt数据包处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttProcessPacketHandler(RyanMqttClient_t *client)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	MQTTPacketInfo_t pIncomingPacket = {0}; // 下面有非空判断

	RyanMqttAssert(NULL != client);

	result = RyanMqttGetPacketInfo(client, &pIncomingPacket);
	if (RyanMqttRecvPacketTimeOutError == result)
	{
		RyanMqttLog_d("没有待处理的数据包");
		goto __exit;
	}
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttSerializePacketError, RyanMqttLog_d,
				  { goto __exit; });

	RyanMqttLog_d("pIncomingPacket.type: %x ", pIncomingPacket.type & 0xF0U);

	// 控制报文类型
	// 发送者QoS2动作 发布PUBLISH报文 -> 等待PUBREC报文 -> 发送PUBREL报文 -> 等待PUBCOMP报文
	// 接收者QoS2动作 等待PUBLISH报文 -> 发送PUBREC报文 -> 等待PUBREL报文 -> 发送PUBCOMP报文
	switch (pIncomingPacket.type & 0xF0U)
	{
	case MQTT_PACKET_TYPE_PUBLISH: // 接收到订阅消息
		result = RyanMqttPublishPacketHandler(client, &pIncomingPacket);
		break;

	case MQTT_PACKET_TYPE_CONNACK: // 连接报文确认
	{
		// 客户端已处于连接状态时又收到CONNACK报文,应该视为严重错误，断开连接
		RyanMqttLog_e("收到 CONNACK 时已连接，正在断开连接");
		RyanMqttConnectStatus_e connectState = RyanMqttConnectProtocolError;
		RyanMqttEventMachine(client, RyanMqttEventDisconnected, &connectState);
		result = RyanMqttHaveRescourceError;
	}
	break;

	case MQTT_PACKET_TYPE_PUBACK:  // 客户端发送QoS 1消息，服务端发布收到确认
	case MQTT_PACKET_TYPE_PUBCOMP: // 发送QOS2 发布完成
		result = RyanMqttPubackAndPubcompPacketHandler(client, &pIncomingPacket);
		break;

	case MQTT_PACKET_TYPE_PUBREC: // 客户端发送QOS2，服务端发布PUBREC，需要客户端继续发送PUBREL
		result = RyanMqttPubrecPacketHandler(client, &pIncomingPacket);
		break;

	case (MQTT_PACKET_TYPE_PUBREL & 0xF0U): // 客户端接收QOS2 已经发布PUBREC，等待服务器发布释放
		if (pIncomingPacket.type & 0x02U)
		{ // PUBREL 控制报文固定报头的第 3,2,1,0 位必须被设置为
		  // 0,0,1,0。必须将其它的任何值都当做是不合法的并关闭网络连接
			result = RyanMqttPubrelPacketHandler(client, &pIncomingPacket);
		}
		break;

	case MQTT_PACKET_TYPE_SUBACK: // 订阅确认
		result = RyanMqttSubackHandler(client, &pIncomingPacket);
		break;

	case MQTT_PACKET_TYPE_UNSUBACK: // 取消订阅确认
		result = RyanMqttUnSubackHandler(client, &pIncomingPacket);
		break;

	case MQTT_PACKET_TYPE_PINGRESP: // 心跳响应
		RyanMqttRefreshKeepaliveTime(client);
		result = RyanMqttSuccessError;
		break;

	default:
		RyanMqttLog_w("Unhandled packet type: 0x%02X", pIncomingPacket.type & 0xF0U);
		result = RyanMqttDeserializePacketError;
		break;
	}

__exit:
	if (NULL != pIncomingPacket.pRemainingData)
	{
		platformMemoryFree(pIncomingPacket.pRemainingData);
	}

	return result;
}
