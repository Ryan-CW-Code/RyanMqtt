#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttThread.h"
#include "RyanMqttLog.h"
#include "RyanMqttUtil.h"

// mqtt标准是1.5倍，大部分mqtt服务器也是这个配置，RyanMqtt设置为1.4倍，给发送心跳包留一定的时间
#define RyanMqttKeepAliveMultiplier (1.4)

void RyanMqttRefreshKeepaliveTime(RyanMqttClient_t *client)
{
	// 服务器在心跳时间的1.5倍内没有收到keeplive消息则会断开连接
	// 这里算 1.4 b倍时间内没有收到心跳就断开连接
	platformCriticalEnter(client->config.userData, &client->criticalLock);
	uint32_t timeout = (uint32_t)(client->config.keepaliveTimeoutS * 1000 * RyanMqttKeepAliveMultiplier);
	RyanMqttTimerCutdown(&client->keepaliveTimer, timeout); // 启动心跳定时器
	platformCriticalExit(client->config.userData, &client->criticalLock);
}

/**
 * @brief mqtt心跳保活
 *
 * @param client
 * @return int32_t
 */
static RyanMqttError_e RyanMqttKeepalive(RyanMqttClient_t *client)
{
	RyanMqttAssert(NULL != client);

	// mqtt没有连接就退出
	if (RyanMqttConnectState != RyanMqttGetClientState(client))
	{
		return RyanMqttNotConnectError;
	}

	uint32_t timeRemain = RyanMqttTimerRemain(&client->keepaliveTimer);

	// 超过设置的 1.4 倍心跳周期，主动通知用户断开连接
	if (0 == timeRemain)
	{
		RyanMqttConnectStatus_e connectState = RyanMqttKeepaliveTimeout;
		RyanMqttEventMachine(client, RyanMqttEventDisconnected, (void *)&connectState);
		RyanMqttLog_d("ErrorCode: %d, strError: %s", RyanMqttKeepaliveTimeout,
			      RyanMqttStrError(RyanMqttKeepaliveTimeout));
		return RyanMqttFailedError;
	}

	// 当剩余时间小于 recvtimeout 时强制发送心跳包
	if (timeRemain > client->config.recvTimeout)
	{
		// 当到达 keepaliveTimeoutS的0.9 倍时间时发送心跳包
		if (timeRemain > client->config.keepaliveTimeoutS * 1000 * (RyanMqttKeepAliveMultiplier - 0.9))
		{
			return RyanMqttSuccessError;
		}

		// 节流时间内不发送心跳报文
		if (RyanMqttTimerRemain(&client->keepaliveThrottleTimer))
		{
			return RyanMqttSuccessError;
		}
	}

	// 发送mqtt心跳包
	{
		// MQTT_PACKET_PINGREQ_SIZE
		uint8_t buffer[2];
		MQTTFixedBuffer_t fixedBuffer = {.pBuffer = buffer, .size = sizeof(buffer)};

		// 序列化数据包
		MQTTStatus_t status = MQTT_SerializePingreq(&fixedBuffer);
		RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d);

		RyanMqttError_e result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
		RyanMqttCheck(RyanMqttSuccessError == result, result, RyanMqttLog_d);

		RyanMqttTimerCutdown(&client->keepaliveThrottleTimer,
				     client->config.recvTimeout + 1500); // 启动心跳检查节流定时器
	}

	return RyanMqttSuccessError;
}

// todo 也可以考虑有ack链表的时候recvTime可以短一些，有坑点
// todo 也可以考虑将发送操作独立出去，异步发送,目前没有遇到性能瓶颈，需要超高性能的时候再考虑吧
/**
 * @brief 遍历ack链表，进行相应的处理
 *
 * @param client
 * @param waitFlag
 *      waitFlag : RyanMqttFalse 表示不需要等待超时立即处理这些数据包。通常在重新连接后立即进行处理
 *      waitFlag : RyanMqttTrue 表示需要等待超时再处理这些消息，一般是稳定连接下的超时处理
 */
static void RyanMqttAckListScan(RyanMqttClient_t *client, RyanMqttBool_e waitFlag)
{
	RyanList_t *curr, *next;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttTimer_t ackScanRemainTimer;
	uint32_t ackScanThrottleTime = 1000; // 最长一秒
	RyanMqttAssert(NULL != client);

	// mqtt没有连接就退出
	if (RyanMqttConnectState != RyanMqttGetClientState(client))
	{
		return;
	}

	// 节流时间内不检查ack链表
	if (RyanMqttTimerRemain(&client->ackScanThrottleTimer))
	{
		return;
	}

	// 设置scan最大处理时间定时器
	uint32_t ackScanWindowMs;
	if (client->config.recvTimeout > 100)
	{
		ackScanWindowMs = client->config.recvTimeout - 100;
	}
	else
	{
		ackScanWindowMs = client->config.recvTimeout;
	}
	RyanMqttTimerCutdown(&ackScanRemainTimer, ackScanWindowMs);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanListForEachSafe(curr, next, &client->ackHandlerList)
	{
		// 需要再判断一次
		if (RyanMqttConnectState != RyanMqttGetClientState(client))
		{
			continue;
		}

		// 超过最大处理时间，直接跳出处理函数
		if (0 == RyanMqttTimerRemain(&ackScanRemainTimer))
		{
			break;
		}

		// 获取此节点的结构体
		ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

		// ack响应没有超时就不进行处理
		uint32_t ackRemainTime = RyanMqttTimerRemain(&ackHandler->timer);
		if (0 != ackRemainTime)
		{
			if (ackRemainTime < ackScanThrottleTime)
			{
				ackScanThrottleTime = ackRemainTime;
			}

			if (RyanMqttTrue == waitFlag)
			{
				continue;
			}
		}

		switch (ackHandler->packetType)
		{
		// 发送qos1 / qos2消息, 服务器ack响应超时。需要重新发送它们。
		case MQTT_PACKET_TYPE_PUBACK:  // qos1 publish后没有收到puback
		case MQTT_PACKET_TYPE_PUBREC:  // qos2 publish后没有收到pubrec
		case MQTT_PACKET_TYPE_PUBREL:  // qos2 收到pubrec，发送pubrel后没有收到pubcomp
		case MQTT_PACKET_TYPE_PUBCOMP: // 理论不会出现，冗余措施
		{
			// 设置重发标志位
			MQTT_UpdateDuplicatePublishFlag(ackHandler->packet, true);

			// 重发次数超过警告值回调
			if (ackHandler->repeatCount >= client->config.ackHandlerRepeatCountWarning)
			{
				RyanMqttEventMachine(client, RyanMqttEventAckRepeatCountWarning, (void *)ackHandler);
				continue;
			}

			// 重发数据事件回调
			RyanMqttEventMachine(client, RyanMqttEventRepeatPublishPacket, (void *)ackHandler);

			//? 发送失败也是重试,所以这里不进行错误判断
			RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen); // 重新发送数据

			// 重置ack超时时间
			RyanMqttTimerCutdown(&ackHandler->timer, client->config.ackTimeout);
			ackHandler->repeatCount++;
			break;
		}

		// 订阅 / 取消订阅超时就认为失败
		case MQTT_PACKET_TYPE_SUBACK:

			RyanMqttMsgHandlerFindAndDestroyByPackId(client, ackHandler->msgHandler, RyanMqttFalse);

			RyanMqttEventMachine(client, RyanMqttEventSubscribedFailed, (void *)ackHandler->msgHandler);

			RyanMqttAckListRemoveToAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler); // 清除句柄
			break;

		case MQTT_PACKET_TYPE_UNSUBACK: {
			RyanMqttEventMachine(client, RyanMqttEventUnSubscribedFailed, (void *)ackHandler->msgHandler);
			RyanMqttAckListRemoveToAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler); // 清除句柄
			break;
		}

		default: {
			RyanMqttLog_e("不应该出现的值: %d", ackHandler->packetType);
			RyanMqttAssert(NULL); // 不应该为别的值
			break;
		}
		}
	}
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	// 扫描链表没有超时时，才设置scan节流定时器
	if (RyanMqttTimerRemain(&ackScanRemainTimer))
	{
		// 启动ack scan节流定时器
		RyanMqttTimerCutdown(&client->ackScanThrottleTimer, ackScanThrottleTime);
	}
}

/**
 * @brief mqtt连接函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttConnect(RyanMqttClient_t *client, RyanMqttConnectStatus_e *connectState)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	MQTTStatus_t status;
	MQTTConnectInfo_t connectInfo;
	MQTTPublishInfo_t willInfo;
	MQTTFixedBuffer_t fixedBuffer;
	size_t remainingLength;
	RyanMqttBool_e lwtFlag;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != connectState);

	RyanMqttCheck(RyanMqttConnectState != RyanMqttGetClientState(client), RyanMqttConnectError, RyanMqttLog_d);

	// connect 信息
	{
		connectInfo.pClientIdentifier = client->config.clientId;
		connectInfo.clientIdentifierLength = RyanMqttStrlen(client->config.clientId);
		connectInfo.pUserName = client->config.userName;
		if (connectInfo.pUserName)
		{
			connectInfo.userNameLength = RyanMqttStrlen(client->config.userName);
		}
		else
		{
			connectInfo.userNameLength = 0;
		}

		connectInfo.pPassword = client->config.password;
		if (connectInfo.pPassword)
		{
			connectInfo.passwordLength = RyanMqttStrlen(client->config.password);
		}
		else
		{
			connectInfo.passwordLength = 0;
		}
		connectInfo.keepAliveSeconds = client->config.keepaliveTimeoutS;
		connectInfo.cleanSession = client->config.cleanSessionFlag;

		// 验证lwt信息
		platformMutexLock(client->config.userData, &client->userSessionLock);
		if (NULL != client->lwtOptions)
		{
			lwtFlag = client->lwtOptions->lwtFlag;
			if (lwtFlag)
			{
				willInfo.qos = (MQTTQoS_t)client->lwtOptions->qos;
				willInfo.retain = client->lwtOptions->retain;
				willInfo.pPayload = client->lwtOptions->payload;
				willInfo.payloadLength = client->lwtOptions->payloadLen;
				willInfo.pTopicName = client->lwtOptions->topic;
				willInfo.topicNameLength = RyanMqttStrlen(client->lwtOptions->topic);
				willInfo.dup = RyanMqttFalse;
			}
		}
		else
		{
			lwtFlag = RyanMqttFalse;
		}
		platformMutexUnLock(client->config.userData, &client->userSessionLock);
	}

	// 获取数据包大小
	status = MQTT_GetConnectPacketSize(&connectInfo, RyanMqttTrue == lwtFlag ? &willInfo : NULL, &remainingLength,
					   &fixedBuffer.size);
	RyanMqttAssert(MQTTSuccess == status);

	// 申请数据包的空间
	fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
	RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, RyanMqttLog_d);

	// 序列化数据包
	status = MQTT_SerializeConnect(&connectInfo, RyanMqttTrue == lwtFlag ? &willInfo : NULL, remainingLength,
				       &fixedBuffer);
	RyanMqttCheckCodeNoReturn(MQTTSuccess == status, RyanMqttSerializePacketError, RyanMqttLog_d, {
		result = RyanMqttSerializePacketError;
		goto __exit;
	});

	// 调用底层的连接函数连接上服务器
	result = platformNetworkConnect(client->config.userData, &client->network, client->config.host,
					client->config.port);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanSocketFailedError, RyanMqttLog_d,
				  { goto __exit; });

	// 发送序列化mqtt的CONNECT报文
	result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, result, RyanMqttLog_d, {
		platformNetworkClose(client->config.userData, &client->network);
		goto __exit;
	});

	// 等待报文
	// mqtt规范 服务端接收到connect报文后，服务端发送给客户端的第一个报文必须是 CONNACK
	MQTTPacketInfo_t pIncomingPacket;
	result = RyanMqttGetPacketInfo(client, &pIncomingPacket);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttSerializePacketError, RyanMqttLog_d, {
		platformNetworkClose(client->config.userData, &client->network);
		goto __exit;
	});

	if (MQTT_PACKET_TYPE_CONNACK == (pIncomingPacket.type & 0xF0U))
	{
		uint16_t packetId;
		bool sessionPresent; // 会话位

		// 反序列化ack包，MQTTSuccess 和 MQTTServerRefused都会返回正确的connectState
		status = MQTT_DeserializeAck(&pIncomingPacket, &packetId, &sessionPresent);
		if (MQTTSuccess != status && MQTTServerRefused != status)
		{
			result = RyanMqttFailedError;
		}
		else
		{
			*connectState = pIncomingPacket.pRemainingData[1];
			if (RyanMqttConnectAccepted != *connectState)
			{
				result = RyanMqttFailedError;
			}
			else
			{
				// 服务端无历史会话，客户端这里选择直接进行清空
				if (false == sessionPresent)
				{
					RyanMqttPurgeSession(client);
				}
			}
		}
	}
	else
	{
		*connectState = RyanMqttConnectFirstPackNotConnack;
	}

	platformMemoryFree(pIncomingPacket.pRemainingData);

__exit:
	platformMemoryFree(fixedBuffer.pBuffer);
	return result;
}

/**
 * @brief mqtt事件处理函数
 *
 * @param client
 * @param eventId
 * @param eventData
 */
void RyanMqttEventMachine(RyanMqttClient_t *client, RyanMqttEventId_e eventId, void *eventData)
{
	RyanMqttAssert(NULL != client);

	switch (eventId)
	{
	case RyanMqttEventConnected: // 第一次连接成功
		RyanMqttRefreshKeepaliveTime(client);
		RyanMqttAckListScan(client,
				    RyanMqttFalse); // 扫描确认列表，销毁已超时的确认处理程序或重新发送它们
		RyanMqttSetClientState(client, RyanMqttConnectState);
		break;

	case RyanMqttEventDisconnected: // 断开连接事件
		// 先将客户端状态设置为断开连接,避免close网络资源时用户依然在使用
		RyanMqttSetClientState(client, RyanMqttDisconnectState);
		platformNetworkClose(client->config.userData, &client->network);
		if (RyanMqttTrue == client->config.cleanSessionFlag)
		{
			RyanMqttPurgeSession(client);
		}
		break;

	case RyanMqttEventReconnectBefore: // 重连前回调
		RyanMqttSetClientState(client, RyanMqttReconnectState);
		break;

	default: break;
	}

	if (NULL == client->config.mqttEventHandle)
	{
		return;
	}

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	RyanMqttEventId_e eventFlag = client->eventFlag;
	platformCriticalExit(client->config.userData, &client->criticalLock);

	if (eventFlag & eventId)
	{
		client->config.mqttEventHandle(client, eventId, eventData);
	}
}

/**
 * @brief mqtt运行线程
 *
 * @param argument
 */
void RyanMqttThread(void *argument)
{
	RyanMqttClient_t *client = (RyanMqttClient_t *)argument;
	RyanMqttAssert(NULL != client); // RyanMqttStart前没有调用RyanMqttInit

	while (1)
	{
		// 销毁客户端
		if (RyanMqttTrue == client->destroyFlag)
		{
			RyanMqttEventMachine(client, RyanMqttEventDestroyBefore, (void *)NULL);

			// 关闭网络组件
			platformNetworkClose(client->config.userData, &client->network);

			// 销毁网络组件
			platformNetworkDestroy(client->config.userData, &client->network);

			// 清除config信息
			if (NULL != client->config.clientId)
			{
				platformMemoryFree(client->config.clientId);
			}
			if (NULL != client->config.userName)
			{
				platformMemoryFree(client->config.userName);
			}
			if (NULL != client->config.password)
			{
				platformMemoryFree(client->config.password);
			}
			if (NULL != client->config.host)
			{
				platformMemoryFree(client->config.host);
			}
			if (NULL != client->config.taskName)
			{
				platformMemoryFree(client->config.taskName);
			}

			// 清除遗嘱相关配置
			if (NULL != client->lwtOptions)
			{
				if (NULL != client->lwtOptions->payload)
				{
					platformMemoryFree(client->lwtOptions->payload);
				}

				if (NULL != client->lwtOptions->topic)
				{
					platformMemoryFree(client->lwtOptions->topic);
				}

				platformMemoryFree(client->lwtOptions);
			}

			// 清除session  ack链表和msg链表
			RyanMqttPurgeSession(client);

			// 清除互斥锁
			platformMutexDestroy(client->config.userData, &client->sendLock);
			platformMutexDestroy(client->config.userData, &client->msgHandleLock);
			platformMutexDestroy(client->config.userData, &client->ackHandleLock);
			platformMutexDestroy(client->config.userData, &client->userSessionLock);

			// 清除临界区
			platformCriticalDestroy(client->config.userData, &client->criticalLock);

			// 清除掉线程动态资源
			platformThread_t mqttThread;
			RyanMqttMemcpy(&mqttThread, &client->mqttThread, sizeof(platformThread_t));
			void *userData = client->config.userData;

			platformMemoryFree(client);
			client = NULL;

			// 销毁自身线程
			platformThreadDestroy(userData, &mqttThread);
			return;
		}

		// 客户端状态变更状态机
		switch (RyanMqttGetClientState(client))
		{

		case RyanMqttStartState: // 开始状态状态
		case RyanMqttReconnectState: {
			RyanMqttLog_d("开始连接");
			RyanMqttConnectStatus_e connectState;
			RyanMqttError_e result = RyanMqttConnect(client, &connectState);
			RyanMqttEventMachine(client,
					     RyanMqttSuccessError == result ? RyanMqttEventConnected
									    : RyanMqttEventDisconnected,
					     (void *)&connectState);
		}
		break;

		case RyanMqttConnectState: // 连接状态
			RyanMqttLog_d("连接状态");
			RyanMqttProcessPacketHandler(client);
			RyanMqttAckListScan(client, RyanMqttTrue);
			RyanMqttKeepalive(client);
			break;

		case RyanMqttDisconnectState: // 断开连接状态
			RyanMqttLog_d("断开连接状态");
			if (RyanMqttTrue != client->config.autoReconnectFlag) // 没有使能自动连接就休眠线程
			{
				platformThreadStop(client->config.userData, &client->mqttThread);

				// 断连的时候会暂停线程，线程重新启动就是用户手动连接了
				RyanMqttLog_d("手动重新连接\r\n");
				RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL);
			}
			else
			{
				RyanMqttLog_d("触发自动连接，%dms后开始连接\r\n", client->config.reconnectTimeout);
				platformDelay(client->config.reconnectTimeout);
				RyanMqttEventMachine(client, RyanMqttEventReconnectBefore,
						     NULL); // 给上层触发重新连接前事件
			}
			break;

		default: RyanMqttAssert(NULL); break;
		}
	}
}
