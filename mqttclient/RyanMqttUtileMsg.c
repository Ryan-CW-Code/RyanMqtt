#define RyanMqttLogLevel (RyanMqttLogLevelAssert) // 日志打印等级
// #define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

#include "RyanMqttUtile.h"
#include "RyanMqttLog.h"
#include "RyanMqttThread.h"

/**
 * @brief 根据 MQTT 3.1.1 协议规范确定传递的主题过滤器和主题名称是否匹配的实用程序函数，
 *          应仅在strcmp / strncmp不相等时再进行通配符匹配
 *
 * @param topic 要检查的主题名称
 * @param topicLength 主题名称的长度。
 * @param topicFilter 要检查的主题过滤器。
 * @param topicFilterLength 要检查的主题过滤器长度
 * @return RyanMqttBool_e
 */
static RyanMqttBool_e RyanMqttMatchTopic(const char *topic, const uint16_t topicLength, const char *topicFilter,
					 const uint16_t topicFilterLength)
{

	RyanMqttBool_e topicFilterStartsWithWildcard = RyanMqttFalse, matchFound = RyanMqttFalse,
		       shouldStopMatching = RyanMqttFalse;
	uint16_t topicIndex = 0, topicFilterIndex = 0;

	RyanMqttAssert((NULL != topic) && (topicLength != 0u));
	RyanMqttAssert((NULL != topicFilter) && (topicFilterLength != 0u));

	// 确定主题过滤器是否以通配符开头。
	topicFilterStartsWithWildcard = (RyanMqttBool_e)((topicFilter[0] == '+') || (topicFilter[0] == '#'));

	// 不能将 $ 字符开头的主题名匹配通配符 (#或+) 开头的主题过滤器
	if ((topic[0] == '$') && (topicFilterStartsWithWildcard == RyanMqttTrue))
	{
		return RyanMqttFalse;
	}

	// 匹配主题名称和主题过滤器，允许使用通配符。
	while ((topicIndex < topicLength) && (topicFilterIndex < topicFilterLength))
	{
		// 检查主题名称中的字符是否与主题筛选器字符串中的对应字符匹配。
		if (topic[topicIndex] == topicFilter[topicFilterIndex])
		{
			// 当主题名称已被消耗但主题过滤器中还有剩余字符需要匹配时，此功能处理以下两种情况：
			// -当主题过滤器以"/+"或"/#"字符结尾时，主题名称以"/"结尾。
			// -当主题过滤器以"/#"字符结尾时，主题名称以父级别结尾。
			if (topicIndex == (topicLength - 1U))
			{

				// 检查主题筛选器是否有2个剩余字符，并且以"/#"结尾。
				// 此检查处理将筛选器"sport/#"与主题"sport"匹配的情况。
				// 原因是"#"通配符表示主题名称中的父级和任意数量的子级。
				if ((topicFilterLength >= 3U) && (topicFilterIndex == (topicFilterLength - 3U)) &&
				    (topicFilter[topicFilterIndex + 1U] == '/') &&
				    (topicFilter[topicFilterIndex + 2U] == '#'))
				{
					matchFound = RyanMqttTrue;
				}

				// 检查下一个字符是否为"#"或"+"，主题过滤器以"/#"或"/+"结尾。
				// 此检查处理要匹配的情况：
				// -主题过滤器"sport/+"与主题"sport/"。
				// -主题过滤器"sport/#"，主题为"sport/"。
				if ((topicFilterIndex == (topicFilterLength - 2U)) &&
				    (topicFilter[topicFilterIndex] == '/'))
				{
					// 检查最后一个字符是否为通配符
					matchFound = (RyanMqttBool_e)((topicFilter[topicFilterIndex + 1U] == '+') ||
								      (topicFilter[topicFilterIndex + 1U] == '#'));
				}
			}
		}
		else
		{
			// 检查是否匹配通配符
			RyanMqttBool_e locationIsValidForWildcard;

			// 主题过滤器中的通配符仅在起始位置或前面有"/"时有效。
			locationIsValidForWildcard = (RyanMqttBool_e)((topicFilterIndex == 0u) ||
								      (topicFilter[topicFilterIndex - 1U] == '/'));

			if ((topicFilter[topicFilterIndex] == '+') && (locationIsValidForWildcard == RyanMqttTrue))
			{
				RyanMqttBool_e nextLevelExistsInTopicName = RyanMqttFalse;
				RyanMqttBool_e nextLevelExistsinTopicFilter = RyanMqttFalse;

				// 将主题名称索引移动到当前级别的末尾,
				// 当前级别的结束由下一个级别分隔符"/"之前的最后一个字符标识。
				while (topicIndex < topicLength)
				{
					// 如果我们碰到级别分隔符，则退出循环
					if (topic[topicIndex] == '/')
					{
						nextLevelExistsInTopicName = RyanMqttTrue;
						break;
					}

					(topicIndex)++;
				}

				// 确定主题过滤器是否包含在由"+"通配符表示的当前级别之后的子级别。
				if ((topicFilterIndex < (topicFilterLength - 1U)) &&
				    (topicFilter[topicFilterIndex + 1U] == '/'))
				{
					nextLevelExistsinTopicFilter = RyanMqttTrue;
				}

				// 如果主题名称包含子级别但主题过滤器在当前级别结束，则不存在匹配项。
				if ((nextLevelExistsInTopicName == RyanMqttTrue) &&
				    (nextLevelExistsinTopicFilter == RyanMqttFalse))
				{
					matchFound = RyanMqttFalse;
					shouldStopMatching = RyanMqttTrue;
				}
				// 如果主题名称和主题过滤器有子级别，则将过滤器索引推进到主题过滤器中的级别分隔符，以便在下一个级别进行匹配。
				// 注意：名称索引已经指向主题名称中的级别分隔符。
				else if (nextLevelExistsInTopicName == RyanMqttTrue)
				{
					(topicFilterIndex)++;
				}

				// 如果我们已经到达这里，循环以（*pNameIndex <
				// topicLength）条件终止，
				// 这意味着已经超过主题名称的末尾，因此，我们将索引缩减为主题名称中的最后一个字符。
				else
				{
					(topicIndex)--;
				}
			}

			// "#"匹配主题名称中剩余的所有内容。它必须是主题过滤器中的最后一个字符。
			else if ((topicFilter[topicFilterIndex] == '#') &&
				 (topicFilterIndex == (topicFilterLength - 1U)) &&
				 (locationIsValidForWildcard == RyanMqttTrue))
			{
				// 后续字符不需要检查多级通配符。
				matchFound = RyanMqttTrue;
				shouldStopMatching = RyanMqttTrue;
			}
			else
			{
				// 除"+"或"#"以外的任何字符不匹配均表示主题名称与主题过滤器不匹配。
				matchFound = RyanMqttFalse;
				shouldStopMatching = RyanMqttTrue;
			}
		}

		if ((matchFound == RyanMqttTrue) || (shouldStopMatching == RyanMqttTrue))
		{
			break;
		}

		// 增量索引
		topicIndex++;
		topicFilterIndex++;
	}

	// 如果已到达两个字符串的末尾，则它们匹配。这表示当主题过滤器在非起始位置包含 "+"
	// 通配符时的情况。 例如，当将 "sport/+/player" 或 "sport/hockey/+" 主题过滤器与
	// "sport/hockey/player" 主题名称匹配时。
	if (matchFound == RyanMqttFalse)
	{
		matchFound = (RyanMqttBool_e)((topicIndex == topicLength) && (topicFilterIndex == topicFilterLength));
	}

	return matchFound;
}

/**
 * @brief 创建msg句柄
 *
 * @param topic
 * @param topicLen
 * @param qos
 * @param pMsgHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttMsgHandlerCreate(RyanMqttClient_t *client, const char *topic, uint16_t topicLen,
					 uint16_t packetId, RyanMqttQos_e qos, RyanMqttMsgHandler_t **pMsgHandler)
{
	RyanMqttMsgHandler_t *msgHandler = NULL;
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != topic);
	RyanMqttAssert(NULL != pMsgHandler);
	RyanMqttAssert(RyanMqttQos0 == qos || RyanMqttQos1 == qos || RyanMqttQos2 == qos);

	msgHandler = (RyanMqttMsgHandler_t *)platformMemoryMalloc(sizeof(RyanMqttMsgHandler_t) + topicLen + 1);
	RyanMqttCheck(NULL != msgHandler, RyanMqttNotEnoughMemError, RyanMqttLog_d);
	memset(msgHandler, 0, sizeof(RyanMqttMsgHandler_t) + topicLen + 1);

	// 初始化链表
	RyanListInit(&msgHandler->list);
	msgHandler->packetId = packetId;
	msgHandler->qos = qos;
	msgHandler->topicLen = topicLen;
	msgHandler->topic = (char *)msgHandler + sizeof(RyanMqttMsgHandler_t);
	memcpy(msgHandler->topic, topic, topicLen); // 将packet数据保存到ack中

	*pMsgHandler = msgHandler;
	return RyanMqttSuccessError;
}

/**
 * @brief 销毁msg 句柄
 *
 * @param msgHandler
 */
void RyanMqttMsgHandlerDestory(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != msgHandler);
	platformMemoryFree(msgHandler);
}

RyanMqttBool_e RyanMqttMsgIsMatch(RyanMqttMsgHandler_t *msgHandler, const char *topic, uint16_t topicLen,
				  RyanMqttBool_e topicMatchedFlag)
{
	// 不进行通配符匹配
	if (RyanMqttTrue != topicMatchedFlag)
	{
		// 不相等跳过
		if (topicLen != msgHandler->topicLen)
		{
			return RyanMqttFalse;
		}

		// 主题名称不相等且没有使能通配符匹配
		if (0 != strncmp(topic, msgHandler->topic, topicLen))
		{
			return RyanMqttFalse;
		}
	}
	else
	{
		// 进行通配符匹配
		if (RyanMqttTrue != RyanMqttMatchTopic(topic, topicLen, msgHandler->topic, msgHandler->topicLen))
		{
			return RyanMqttFalse;
		}
	}

	return RyanMqttTrue;
}

/**
 * @brief 查找msg句柄
 *
 * @param client
 * @param topic
 * @param topicLen
 * @param topicMatchedFlag
 * @param pMsgHandler
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttMsgHandlerFind(RyanMqttClient_t *client, RyanMqttMsgHandler_t *findMsgHandler,
				       RyanMqttBool_e topicMatchedFlag, RyanMqttMsgHandler_t **pMsgHandler)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanList_t *curr, *next;
	RyanMqttMsgHandler_t *msgHandler = NULL;

	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != findMsgHandler);
	RyanMqttAssert(NULL != findMsgHandler->topic && 0 != findMsgHandler->topicLen);
	RyanMqttAssert(NULL != pMsgHandler);

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListForEachSafe(curr, next, &client->msgHandlerList)
	{
		msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);

		if (RyanMqttFalse ==
		    RyanMqttMsgIsMatch(msgHandler, findMsgHandler->topic, findMsgHandler->topicLen, topicMatchedFlag))
		{
			continue;
		}

		*pMsgHandler = msgHandler;

		result = RyanMqttSuccessError;
		goto __exit;
	}

	result = RyanMqttNoRescourceError;

__exit:
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
	return result;
}

void RyanMqttMsgHandlerFindAndDestoryByPackId(RyanMqttClient_t *client, RyanMqttMsgHandler_t *findMsgHandler,
					      RyanMqttBool_e isSkipMatchingId)
{
	RyanList_t *curr, *next;
	RyanMqttMsgHandler_t *msgHandler = NULL;

	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != findMsgHandler);
	RyanMqttAssert(NULL != findMsgHandler->topic && 0 != findMsgHandler->topicLen);

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListForEachSafe(curr, next, &client->msgHandlerList)
	{
		msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);

		if (RyanMqttFalse == isSkipMatchingId)
		{
			if (msgHandler->packetId != findMsgHandler->packetId)
			{
				continue;
			}
		}
		else
		{
			if (msgHandler->packetId == findMsgHandler->packetId)
			{
				continue;
			}
		}

		if (RyanMqttFalse ==
		    RyanMqttMsgIsMatch(msgHandler, findMsgHandler->topic, findMsgHandler->topicLen, RyanMqttFalse))
		{
			continue;
		}

		// 到这里就是同名订阅了，直接删除
		RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
		RyanMqttMsgHandlerDestory(client, msgHandler);
		break;
	}
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);
}

/**
 * @brief 将msg句柄存入client msg链表
 *
 * @param client
 * @param msgHandler
 * @return int32_t
 */
RyanMqttError_e RyanMqttMsgHandlerAddToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != msgHandler);

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListAddTail(&msgHandler->list,
			&client->msgHandlerList); // 将msgHandler节点添加到链表尾部
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);

	return RyanMqttSuccessError;
}

/**
 * @brief 将msg句柄存入client msg链表
 *
 * @param client
 * @param msgHandler
 * @return int32_t
 */
RyanMqttError_e RyanMqttMsgHandlerRemoveToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler)
{
	RyanMqttAssert(NULL != client);
	RyanMqttAssert(NULL != msgHandler);

	platformMutexLock(client->config.userData, &client->msgHandleLock);
	RyanListDel(&msgHandler->list);
	platformMutexUnLock(client->config.userData, &client->msgHandleLock);

	return RyanMqttSuccessError;
}
