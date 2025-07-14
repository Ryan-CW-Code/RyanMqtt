#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttUtile.h"
#include "RyanMqttLog.h"
#include "RyanMqttThread.h"

/**
 * @brief 创建ack句柄
 *
 * @param client
 * @param packetType
 * @param packetId
 * @param packetLen
 * @param msgHandler
 * @param pAckHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
					 uint16_t packetLen, uint8_t *packet, RyanMqttMsgHandler_t *msgHandler,
					 RyanMqttAckHandler_t **pAckHandler, RyanMqttBool_e isPreallocatedPacket)
{
	RyanMqttAckHandler_t *ackHandler = NULL;
	uint32_t mallocLen = 0;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	mallocLen = sizeof(RyanMqttAckHandler_t);

	if (RyanMqttTrue != isPreallocatedPacket)
	{
		mallocLen += packetLen + 1;
	}

	// 给消息主题添加空格
	ackHandler = (RyanMqttAckHandler_t *)platformMemoryMalloc(mallocLen);
	RyanMqttCheck(NULL != ackHandler, RyanMqttNotEnoughMemError, RyanMqttLog_d);
	memset(ackHandler, 0, mallocLen);

	RyanListInit(&ackHandler->list);
	// 超时内没有响应将被销毁或重新发送
	RyanMqttTimerCutdown(&ackHandler->timer, client->config.ackTimeout);

	ackHandler->isPreallocatedPacket = isPreallocatedPacket;
	ackHandler->repeatCount = 0;
	ackHandler->packetId = packetId;
	ackHandler->packetLen = packetLen;
	ackHandler->packetType = packetType;
	ackHandler->msgHandler = msgHandler;

	if (RyanMqttTrue != isPreallocatedPacket)
	{
		if (packetLen > 0)
		{
			ackHandler->packet = (uint8_t *)ackHandler + sizeof(RyanMqttAckHandler_t);
			memcpy(ackHandler->packet, packet, packetLen); // 将packet数据保存到ack中
		}
		else
		{
			ackHandler->packet = NULL;
		}
	}
	else
	{
		ackHandler->packet = packet;
	}

	*pAckHandler = ackHandler;

	return RyanMqttSuccessError;
}

/**
 * @brief 销毁ack句柄
 *
 * @param client
 * @param ackHandler
 */
void RyanMqttAckHandlerDestroy(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);
	RyanMqttAssert(NULL != ackHandler->msgHandler);

	RyanMqttMsgHandlerDestory(client, ackHandler->msgHandler); // 释放msgHandler

	if (RyanMqttTrue == ackHandler->isPreallocatedPacket && NULL != ackHandler->packet)
	{
		platformMemoryFree(ackHandler->packet);
	}

	platformMemoryFree(ackHandler);
}

/**
 * @brief 检查链表中是否存在ack句柄
 *
 * @param client
 * @param packetType
 * @param packetId
 * @param pAckHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListNodeFind(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
					RyanMqttAckHandler_t **pAckHandler)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanList_t *curr, *next;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanListForEachSafe(curr, next, &client->ackHandlerList)
	{
		ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

		// 对于 qos1 和 qos2 的 mqtt 数据包，使用数据包 ID 和类型作为唯一
		// 标识符，用于确定节点是否已存在并避免重复。
		if ((packetId == ackHandler->packetId) && (packetType == ackHandler->packetType))
		{
			*pAckHandler = ackHandler;
			result = RyanMqttSuccessError;
			goto __exit;
		}
	}
	result = RyanMqttNoRescourceError;

__exit:
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);
	return result;
}

/**
 * @brief 添加等待ack到链表
 *
 * @param client
 * @param ackHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListAddToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	// 将ack节点添加到链表尾部
	RyanListAddTail(&ackHandler->list, &client->ackHandlerList);
	client->ackHandlerCount++;
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	if (client->ackHandlerCount >= client->config.ackHandlerCountWarning)
	{
		RyanMqttEventMachine(client, RyanMqttEventAckCountWarning, (void *)&client->ackHandlerCount);
	}

	return RyanMqttSuccessError;
}

/**
 * @brief 从链表移除ack
 *
 * @param client
 * @param ackHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListRemoveToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	// 将ack节点添加到链表尾部
	RyanListDel(&ackHandler->list);
	if (client->ackHandlerCount > 0)
	{
		client->ackHandlerCount--;
	}
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 检查链表中是否存在ack句柄
 *
 * @param client
 * @param packetType
 * @param packetId
 * @param pAckHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListNodeFindByUserAckList(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
						     RyanMqttAckHandler_t **pAckHandler)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanList_t *curr, *next;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	platformMutexLock(client->config.userData, &client->userAckHandleLock);
	RyanListForEachSafe(curr, next, &client->userAckHandlerList)
	{
		ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

		// 对于 qos1 和 qos2 的 mqtt 数据包，使用数据包 ID 和类型作为唯一
		// 标识符，用于确定节点是否已存在并避免重复。
		if ((packetId == ackHandler->packetId) && (packetType == ackHandler->packetType))
		{
			*pAckHandler = ackHandler;
			result = RyanMqttSuccessError;
			goto __exit;
		}
	}
	result = RyanMqttNoRescourceError;

__exit:
	platformMutexUnLock(client->config.userData, &client->userAckHandleLock);
	return result;
}

/**
 * @brief 添加等待ack到链表
 *
 * @param client
 * @param ackHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListAddToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->userAckHandleLock);
	RyanListAddTail(&ackHandler->list, &client->userAckHandlerList); // 将ack节点添加到链表尾部
	platformMutexUnLock(client->config.userData, &client->userAckHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 从链表移除ack
 *
 * @param client
 * @param ackHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckListRemoveToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->userAckHandleLock);
	RyanListDel(&ackHandler->list);
	platformMutexUnLock(client->config.userData, &client->userAckHandleLock);

	return RyanMqttSuccessError;
}
