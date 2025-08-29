#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttUtil.h"
#include "RyanMqttLog.h"
#include "RyanMqttThread.h"

/**
 * @brief 创建ack句柄
 *
 * @param client
 * @param packetType
 * @param packetId
 * @param packetLen
 * @param packet
 * @param msgHandler
 * @param pAckHandler
 * @param isPreallocatedPacket packet是否预分配
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
					 uint16_t packetLen, uint8_t *packet, RyanMqttMsgHandler_t *msgHandler,
					 RyanMqttAckHandler_t **pAckHandler, RyanMqttBool_e isPreallocatedPacket)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	uint32_t mallocSize = sizeof(RyanMqttAckHandler_t);

	// 为非预分配的数据包分配额外空间
	if (RyanMqttTrue != isPreallocatedPacket)
	{
		mallocSize += packetLen + 1;
	}

	// 为非预分配包申请额外空间
	RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)platformMemoryMalloc(mallocSize);
	RyanMqttCheck(NULL != ackHandler, RyanMqttNotEnoughMemError, RyanMqttLog_d);

	ackHandler->packetType = packetType;
	ackHandler->repeatCount = 0;
	ackHandler->packetId = packetId;
	ackHandler->isPreallocatedPacket = isPreallocatedPacket;
	ackHandler->packetLen = packetLen;
	RyanMqttListInit(&ackHandler->list);
	// 超时内没有响应将被销毁或重新发送
	RyanMqttTimerCutdown(&ackHandler->timer, client->config.ackTimeout);
	ackHandler->msgHandler = msgHandler;

	if (RyanMqttTrue != isPreallocatedPacket)
	{
		if (NULL != packet && packetLen > 0)
		{
			ackHandler->packet = (uint8_t *)ackHandler + sizeof(RyanMqttAckHandler_t);
			RyanMqttMemcpy(ackHandler->packet, packet, packetLen); // 将packet数据保存到ack中
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

	RyanMqttMsgHandlerDestroy(client, ackHandler->msgHandler); // 释放msgHandler

	// 释放用户预提供的缓冲区
	if (RyanMqttTrue == ackHandler->isPreallocatedPacket)
	{
		// 不加null判断，因为如果是空，一定是用户程序内存访问越界了
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
	RyanMqttList_t *curr, *next;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	RyanMqttListForEachSafe(curr, next, &client->ackHandlerList)
	{
		ackHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);

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
	uint16_t tmpAckHandlerCount;

	platformMutexLock(client->config.userData, &client->ackHandleLock);
	// 将ack节点添加到链表尾部
	RyanMqttListAddTail(&ackHandler->list, &client->ackHandlerList);
	client->ackHandlerCount++;
	tmpAckHandlerCount = client->ackHandlerCount;
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	if (tmpAckHandlerCount >= client->config.ackHandlerCountWarning)
	{
		RyanMqttEventMachine(client, RyanMqttEventAckCountWarning, (void *)&tmpAckHandlerCount);
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
	RyanMqttListDel(&ackHandler->list);
	if (client->ackHandlerCount > 0)
	{
		client->ackHandlerCount--;
	}
	platformMutexUnLock(client->config.userData, &client->ackHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 检查用户层链表中是否存在ack句柄
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
	RyanMqttList_t *curr, *next;
	RyanMqttAckHandler_t *ackHandler;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != pAckHandler);

	platformMutexLock(client->config.userData, &client->userSessionLock);
	RyanMqttListForEachSafe(curr, next, &client->userAckHandlerList)
	{
		ackHandler = RyanMqttListEntry(curr, RyanMqttAckHandler_t, list);

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
	platformMutexUnLock(client->config.userData, &client->userSessionLock);
	return result;
}

RyanMqttError_e RyanMqttAckListAddToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->userSessionLock);
	RyanMqttListAddTail(&ackHandler->list, &client->userAckHandlerList); // 将ack节点添加到链表尾部
	platformMutexUnLock(client->config.userData, &client->userSessionLock);

	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttAckListRemoveToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != ackHandler);

	platformMutexLock(client->config.userData, &client->userSessionLock);
	RyanMqttListDel(&ackHandler->list);
	platformMutexUnLock(client->config.userData, &client->userSessionLock);

	return RyanMqttSuccessError;
}

void RyanMqttClearAckSession(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttAckHandler_t *ackHandler;

	// 清除所有ack链表
	while (1)
	{
		result = RyanMqttAckListNodeFindByUserAckList(client, packetType, packetId, &ackHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToUserAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler);
			continue;
		}

		// 还有可能已经被添加到ack链表了
		result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
		if (RyanMqttSuccessError == result)
		{
			RyanMqttAckListRemoveToAckList(client, ackHandler);
			RyanMqttAckHandlerDestroy(client, ackHandler);
			continue;
		}

		break;
	}
}
