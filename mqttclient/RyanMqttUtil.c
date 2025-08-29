#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttUtil.h"
#include "RyanMqttLog.h"
#include "RyanMqttThread.h"

/**
 * @brief 字符串拷贝，需要手动释放内存
 *
 * @param dest
 * @param rest
 * @param strLen
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttStringCopy(char **dest, const char *rest, uint32_t strLen)
{
	RyanMqttAssert(NULL != dest);
	RyanMqttAssert(NULL != rest);
	// RyanMqttCheck(0 != strLen, RyanMqttFailedError, RyanMqttLog_d);

	char *s = (char *)platformMemoryMalloc(strLen + 1);
	if (NULL == s)
	{
		return RyanMqttNotEnoughMemError;
	}

	RyanMqttMemcpy(s, rest, strLen);
	s[strLen] = '\0';

	*dest = s;
	return RyanMqttSuccessError;
}

/**
 * @brief RyanMqtt 针对 coreMqtt 特殊场景专用
 *
 * @param pNetworkContext
 * @param pBuffer
 * @param bytesToRecv
 * @return int32_t
 */
int32_t coreMqttTransportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
	RyanMqttAssert(NULL != pNetworkContext);
	RyanMqttAssert(NULL != pNetworkContext->client);
	RyanMqttAssert(NULL != pBuffer);
	RyanMqttAssert(bytesToRecv > 0);

	RyanMqttError_e result = RyanMqttRecvPacket(pNetworkContext->client, pBuffer, bytesToRecv);

	switch (result)
	{
	case RyanMqttRecvPacketTimeOutError: return 0;
	case RyanMqttSuccessError: return (int32_t)bytesToRecv;

	case RyanSocketFailedError:
	default: return -1;
	}
}

/**
 * @brief mqtt读取报文
 *
 * @param client
 * @param buf
 * @param length
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttRecvPacket(RyanMqttClient_t *client, uint8_t *recvBuf, uint32_t recvLen)
{
	uint32_t offset = 0;
	int32_t recvResult = 0;
	uint32_t timeOut = client->config.recvTimeout;
	RyanMqttTimer_t timer;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != recvBuf);
	RyanMqttAssert(0 != recvLen);

	RyanMqttTimerCutdown(&timer, timeOut);

	while ((offset < recvLen) && (timeOut > 0))
	{
		recvResult =
			platformNetworkRecvAsync(client->config.userData, &client->network, (char *)(recvBuf + offset),
						 (size_t)(recvLen - offset), (int32_t)timeOut);

		if (recvResult < 0)
		{
			break;
		}

		offset += recvResult;
		timeOut = RyanMqttTimerRemain(&timer);
	}

	// RyanMqttLog_d("offset: %d, recvLen: %d, recvResult: %d", offset, recvLen, recvResult);

	// 错误
	if (recvResult < 0)
	{
		RyanMqttConnectStatus_e connectState = RyanMqttConnectNetWorkFail;
		RyanMqttEventMachine(client, RyanMqttEventDisconnected, &connectState);
		RyanMqttLog_d("recv错误, result: %d", recvResult);
		return RyanSocketFailedError;
	}

	// 发送超时
	if (offset != recvLen)
	{
		return RyanMqttRecvPacketTimeOutError;
	}

	return RyanMqttSuccessError;
}

/**
 * @brief mqtt发送报文
 *
 * @param client
 * @param buf
 * @param length
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSendPacket(RyanMqttClient_t *client, uint8_t *sendBuf, uint32_t sendLen)
{
	uint32_t offset = 0;
	int32_t sendResult = 0;
	uint32_t timeOut = client->config.sendTimeout;
	RyanMqttTimer_t timer;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != sendBuf);
	RyanMqttAssert(0 != sendLen);

	RyanMqttTimerCutdown(&timer, timeOut);

	platformMutexLock(client->config.userData, &client->sendLock); // 获取互斥锁
	while ((offset < sendLen) && (timeOut > 0))
	{
		sendResult =
			platformNetworkSendAsync(client->config.userData, &client->network, (char *)(sendBuf + offset),
						 (size_t)(sendLen - offset), (int32_t)timeOut);
		if (-1 == sendResult)
		{
			break;
		}

		offset += sendResult;
		timeOut = RyanMqttTimerRemain(&timer);
	}
	platformMutexUnLock(client->config.userData, &client->sendLock); // 释放互斥锁

	if (sendResult < 0)
	{
		RyanMqttConnectStatus_e connectState = RyanMqttConnectNetWorkFail;
		RyanMqttEventMachine(client, RyanMqttEventDisconnected, &connectState);
		return RyanSocketFailedError;
	}

	// 发送超时
	if (offset != sendLen)
	{
		return RyanMqttSendPacketTimeOutError;
	}

	// 发送数据成功就刷新 keepalive 时间
	RyanMqttRefreshKeepaliveTime(client);
	return RyanMqttSuccessError;
}

/**
 * @brief 设置mqtt客户端状态
 *
 * @param client
 * @param state
 */
void RyanMqttSetClientState(RyanMqttClient_t *client, RyanMqttState_e state)
{
	RyanMqttAssert(NULL != client);

	platformCriticalEnter(client->config.userData, &client->criticalLock);
	client->clientState = state;
	platformCriticalExit(client->config.userData, &client->criticalLock);
}

/**
 * @brief 获取mqtt客户端状态
 *
 * @param client
 * @return RyanMqttState_e
 */
RyanMqttState_e RyanMqttGetClientState(RyanMqttClient_t *client)
{
	RyanMqttAssert(NULL != client);
	platformCriticalEnter(client->config.userData, &client->criticalLock);
	RyanMqttState_e state = client->clientState;
	platformCriticalExit(client->config.userData, &client->criticalLock);
	return state;
}

/**
 * @brief 清理session
 *
 * @param client
 */
void RyanMqttPurgeSession(RyanMqttClient_t *client)
{
	RyanMqttList_t *curr, *next;
	RyanMqttAssert(NULL != client);

	// 释放所有msg_handler_list内存
	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->msgHandlerList)
	{
		RyanMqttMsgHandler_t *msgHandler = RyanMqttListEntry(curr, RyanMqttMsgHandler_t, list);
		RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
		RyanMqttMsgHandlerDestroy(client, msgHandler);
	}
	RyanMqttListDelInit(&client->msgHandlerList);
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);

	// 释放所有ackHandler_list内存
	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->ackHandlerList)
	{
		RyanMqttAckHandler_t *ackHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);
		RyanMqttAckListRemoveToAckList(client, ackHandler);
		RyanMqttAckHandlerDestroy(client, ackHandler);
	}
	RyanMqttListDelInit(&client->ackHandlerList);
	client->ackHandlerCount = 0;
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	// 释放所有userAckHandler_list内存
	platformMutexLock(client->config.userData, &client->userSessionLock);
	RyanMqttListForEachSafe(curr, next, &client->userAckHandlerList)
	{
		RyanMqttAckHandler_t *userAckHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);
		RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
		RyanMqttAckHandlerDestroy(client, userAckHandler);
	}
	RyanMqttListDelInit(&client->userAckHandlerList);
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
}

void RyanMqttPurgeConfig(RyanMqttClientConfig_t *clientConfig)
{
	RyanMqttAssert(NULL != clientConfig);

	if (clientConfig->clientId)
	{
		platformMemoryFree(clientConfig->clientId);
	}

	if (clientConfig->userName)
	{
		platformMemoryFree(clientConfig->userName);
	}

	if (clientConfig->password)
	{
		platformMemoryFree(clientConfig->password);
	}

	if (clientConfig->host)
	{
		platformMemoryFree(clientConfig->host);
	}

	if (clientConfig->taskName)
	{
		platformMemoryFree(clientConfig->taskName);
	}
}

/**
 * @brief 初始化计时器
 *
 * @param platformTimer
 */
void RyanMqttTimerInit(RyanMqttTimer_t *platformTimer)
{
	platformTimer->timeOut = 0;
	platformTimer->time = 0;
}

/**
 * @brief 添加计数时间
 *
 * @param platformTimer
 * @param timeout
 */
void RyanMqttTimerCutdown(RyanMqttTimer_t *platformTimer, uint32_t timeout)
{
	platformTimer->time = platformUptimeMs();
	platformTimer->timeOut = timeout;
}

/**
 * @brief 获取设置的超时时间
 *
 * @param platformTimer
 */
uint32_t RyanMqttTimerGetConfigTimeout(RyanMqttTimer_t *platformTimer)
{
	return platformTimer->timeOut;
}

/**
 * @brief 计算time还有多长时间超时,考虑了32位溢出判断
 *
 * @param platformTimer
 * @return uint32_t 返回剩余时间，超时返回0
 */
uint32_t RyanMqttTimerRemain(RyanMqttTimer_t *platformTimer)
{
	uint32_t elapsed = platformUptimeMs() - platformTimer->time; // 计算内部自动绕回

	// 如果已过超时时间，返回 0
	if (elapsed >= platformTimer->timeOut)
	{
		return 0;
	}

	// 否则返回剩余时间
	return platformTimer->timeOut - elapsed;
}

const char *RyanMqttStrError(int32_t state)
{
	const char *str;

	switch (state)
	{
	case RyanMqttRecvPacketTimeOutError: str = "读取数据超时"; break;
	case RyanMqttParamInvalidError: str = "无效参数"; break;
	case RyanSocketFailedError: str = "套接字失败"; break;
	case RyanMqttSendPacketError: str = "数据包发送失败"; break;
	case RyanMqttSerializePacketError: str = "序列化报文失败"; break;
	case RyanMqttDeserializePacketError: str = "反序列化报文失败"; break;
	case RyanMqttNoRescourceError: str = "没有资源"; break;
	case RyanMqttHaveRescourceError: str = "资源已存在"; break;
	case RyanMqttNotConnectError: str = "mqttClient没有连接"; break;
	case RyanMqttConnectError: str = "mqttClient已经连接"; break;
	case RyanMqttRecvBufToShortError: str = "接收缓冲区不足"; break;
	case RyanMqttSendBufToShortError: str = "发送缓冲区不足"; break;
	case RyanMqttSocketConnectFailError: str = "socket连接失败"; break;
	case RyanMqttNotEnoughMemError: str = "动态内存不足"; break;
	case RyanMqttFailedError: str = "mqtt失败, 详细信息请看函数内部"; break;
	case RyanMqttSuccessError: str = "mqtt成功, 详细信息请看函数内部"; break;
	case RyanMqttConnectRefusedProtocolVersion: str = "mqtt断开连接, 服务端不支持客户端请求的 MQTT 协议级别"; break;
	case RyanMqttConnectRefusedIdentifier: str = "mqtt断开连接, 不合格的客户端标识符"; break;
	case RyanMqttConnectRefusedServer: str = "mqtt断开连接, 服务端不可用"; break;
	case RyanMqttConnectRefusedUsernamePass: str = "mqtt断开连接, 无效的用户名或密码"; break;
	case RyanMqttConnectRefusedNotAuthorized: str = "mqtt断开连接, 连接已拒绝，未授权"; break;
	case RyanMqttConnectClientInvalid: str = "mqtt断开连接, 客户端处于无效状态"; break;
	case RyanMqttConnectNetWorkFail: str = "mqtt断开连接, 网络错误"; break;
	case RyanMqttConnectDisconnected: str = "mqtt断开连接, mqtt客户端断开连接"; break;
	case RyanMqttKeepaliveTimeout: str = "mqtt断开连接, 心跳超时断开连接"; break;
	case RyanMqttConnectUserDisconnected: str = "mqtt断开连接, 用户手动断开连接"; break;
	case RyanMqttConnectTimeout: str = "mqtt断开连接, connect超时断开"; break;
	default: str = "未知错误描述"; break;
	}

	return str;
}
