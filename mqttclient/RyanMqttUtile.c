// #define rlogEnable               // 是否使能日志
#define rlogColorEnable          // 是否使能日志颜色
#define rlogLevel (rlogLvlError) // 日志打印等级
#define rlogTag "RyanMqttUtile"  // 日志tag

#include "RyanMqttUtile.h"

/**
 * @brief 字符串拷贝，需要手动释放内存
 *
 * @param dest
 * @param rest
 * @param strLen
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttStringCopy(char **dest, char *rest, uint32_t strLen)
{
    char *str2 = NULL;
    RyanMqttAssert(NULL != dest);
    RyanMqttAssert(NULL != rest);
    // RyanMqttCheck(0 != strLen, RyanMqttFailedError, rlog_d);

    str2 = (char *)platformMemoryMalloc(strLen + 1);
    if (NULL == str2)
        return RyanMqttNotEnoughMemError;

    memcpy(str2, rest, strLen);
    str2[strLen] = '\0';

    *dest = str2;

    return RyanMqttSuccessError;
}

/**
 * @brief 设置重发标志位
 *
 * @param headerBuf
 * @param dup
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSetPublishDup(char *headerBuf, uint8_t dup)
{

    MQTTHeader header = {0};
    RyanMqttAssert(NULL != headerBuf);

    header.byte = *headerBuf;
    RyanMqttCheck(PUBLISH == header.bits.type, RyanMqttFailedError, rlog_d);

    header.bits.dup = dup;
    *headerBuf = header.byte;

    return RyanMqttSuccessError;
}

/**
 * @brief mqtt读取报文
 *
 * @param client
 * @param buf
 * @param length
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttRecvPacket(RyanMqttClient_t *client, char *recvBuf, int32_t recvLen)
{

    RyanMqttConnectStatus_e connectState = RyanMqttConnectAccepted;
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttAssert(NULL != client);
    RyanMqttAssert(NULL != recvBuf);

    RyanMqttCheck(0 != recvLen, RyanMqttSuccessError, rlog_d);

    result = platformNetworkRecvAsync(client->config.userData, &client->network, recvBuf, recvLen, client->config.recvTimeout);

    switch (result)
    {
    case RyanMqttRecvPacketTimeOutError:
    case RyanMqttSuccessError:
        return result;

    case RyanSocketFailedError:
    default:
        connectState = RyanMqttConnectNetWorkFail;
        RyanMqttEventMachine(client, RyanMqttEventDisconnected, &connectState);
        return RyanSocketFailedError;
    }
}

/**
 * @brief mqtt发送报文
 *
 * @param client
 * @param buf
 * @param length
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSendPacket(RyanMqttClient_t *client, char *sendBuf, int32_t sendLen)
{
    RyanMqttConnectStatus_e connectState = RyanMqttConnectAccepted;
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttAssert(NULL != client);
    RyanMqttAssert(NULL != sendBuf);

    RyanMqttCheck(0 != sendLen, RyanMqttSuccessError, rlog_d);

    result = platformNetworkSendAsync(client->config.userData, &client->network, sendBuf, sendLen, client->config.sendTimeout);
    switch (result)
    {
    case RyanMqttSuccessError:
        RyanMqttRefreshKeepaliveTime(client); // 只要发送数据就刷新 keepalive 时间，可以降低一些心智负担
    case RyanMqttSendPacketTimeOutError:
        return result;

    case RyanSocketFailedError:
    default:
        connectState = RyanMqttConnectNetWorkFail;
        RyanMqttEventMachine(client, RyanMqttEventDisconnected, &connectState);
        return RyanSocketFailedError;
    }
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
    return client->clientState;
}

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
RyanMqttBool_e RyanMqttMatchTopic(const char *topic,
                                  const uint16_t topicLength,
                                  const char *topicFilter,
                                  const uint16_t topicFilterLength)
{

    RyanMqttBool_e topicFilterStartsWithWildcard = RyanMqttFalse,
                   matchFound = RyanMqttFalse,
                   shouldStopMatching = RyanMqttFalse;
    uint16_t topicIndex = 0,
             topicFilterIndex = 0;

    RyanMqttAssert((NULL != topic) && (topicLength != 0u));
    RyanMqttAssert((NULL != topicFilter) && (topicFilterLength != 0u));

    // 确定主题过滤器是否以通配符开头。
    topicFilterStartsWithWildcard = (RyanMqttBool_e)((topicFilter[0] == '+') || (topicFilter[0] == '#'));

    // 不能将 $ 字符开头的主题名匹配通配符 (#或+) 开头的主题过滤器
    if ((topic[0] == '$') && (topicFilterStartsWithWildcard == RyanMqttTrue))
        return RyanMqttFalse;

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
                if ((topicFilterLength >= 3U) &&
                    (topicFilterIndex == (topicFilterLength - 3U)) &&
                    (topicFilter[topicFilterIndex + 1U] == '/') &&
                    (topicFilter[topicFilterIndex + 2U] == '#'))
                    matchFound = RyanMqttTrue;

                // 检查下一个字符是否为"#"或"+"，主题过滤器以"/#"或"/+"结尾。
                // 此检查处理要匹配的情况：
                // -主题过滤器"sport/+"与主题"sport/"。
                // -主题过滤器"sport/#"，主题为"sport/"。
                if ((topicFilterIndex == (topicFilterLength - 2U)) &&
                    (topicFilter[topicFilterIndex] == '/'))
                    // 检查最后一个字符是否为通配符
                    matchFound = (RyanMqttBool_e)((topicFilter[topicFilterIndex + 1U] == '+') || (topicFilter[topicFilterIndex + 1U] == '#'));
            }
        }
        else
        {
            // 检查是否匹配通配符
            RyanMqttBool_e locationIsValidForWildcard;

            // 主题过滤器中的通配符仅在起始位置或前面有"/"时有效。
            locationIsValidForWildcard = (RyanMqttBool_e)((topicFilterIndex == 0u) || (topicFilter[topicFilterIndex - 1U] == '/'));

            if ((topicFilter[topicFilterIndex] == '+') && (locationIsValidForWildcard == RyanMqttTrue))
            {
                RyanMqttBool_e nextLevelExistsInTopicName = RyanMqttFalse;
                RyanMqttBool_e nextLevelExistsinTopicFilter = RyanMqttFalse;

                // 将主题名称索引移动到当前级别的末尾, 当前级别的结束由下一个级别分隔符"/"之前的最后一个字符标识。
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
                    nextLevelExistsinTopicFilter = RyanMqttTrue;

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
                    (topicFilterIndex)++;

                // 如果我们已经到达这里，循环以（*pNameIndex < topicLength）条件终止，
                // 这意味着已经超过主题名称的末尾，因此，我们将索引缩减为主题名称中的最后一个字符。
                else
                    (topicIndex)--;
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
            break;

        // 增量索引
        topicIndex++;
        topicFilterIndex++;
    }

    // 如果已到达两个字符串的末尾，则它们匹配。这表示当主题过滤器在非起始位置包含 "+" 通配符时的情况。
    // 例如，当将 "sport/+/player" 或 "sport/hockey/+" 主题过滤器与 "sport/hockey/player" 主题名称匹配时。
    if (matchFound == RyanMqttFalse)
        matchFound = (RyanMqttBool_e)((topicIndex == topicLength) && (topicFilterIndex == topicFilterLength));

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
RyanMqttError_e RyanMqttMsgHandlerCreate(RyanMqttClient_t *client, char *topic, uint16_t topicLen, RyanMqttQos_e qos, RyanMqttMsgHandler_t **pMsgHandler)
{
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAssert(NULL != topic);
    RyanMqttAssert(NULL != pMsgHandler);
    RyanMqttAssert(RyanMqttQos0 == qos || RyanMqttQos1 == qos || RyanMqttQos2 == qos);

    msgHandler = (RyanMqttMsgHandler_t *)platformMemoryMalloc(sizeof(RyanMqttMsgHandler_t) + topicLen + 1);
    RyanMqttCheck(NULL != msgHandler, RyanMqttNotEnoughMemError, rlog_d);
    memset(msgHandler, 0, sizeof(RyanMqttMsgHandler_t) + topicLen + 1);

    // 初始化链表
    RyanListInit(&msgHandler->list);
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
    RyanMqttAssert(NULL != msgHandler);
    platformMemoryFree(msgHandler);
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
RyanMqttError_e RyanMqttMsgHandlerFind(RyanMqttClient_t *client, char *topic, uint16_t topicLen, RyanMqttBool_e topicMatchedFlag, RyanMqttMsgHandler_t **pMsgHandler)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;

    RyanMqttAssert(NULL != client);
    RyanMqttAssert(NULL != topic && 0 != topicLen);
    RyanMqttAssert(NULL != pMsgHandler);

    platformMutexLock(client->config.userData, &client->msgHandleLock);
    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);

        // 不进行通配符匹配
        if (RyanMqttTrue != topicMatchedFlag)
        {
            // 不相等跳过
            if (topicLen != msgHandler->topicLen)
                continue;

            // 主题名称不相等且没有使能通配符匹配
            if (0 != strncmp(topic, msgHandler->topic, topicLen))
                continue;
        }

        // 进行通配符匹配
        if (RyanMqttTrue != RyanMqttMatchTopic(topic, topicLen, msgHandler->topic, msgHandler->topicLen))
            continue;

        *pMsgHandler = msgHandler;

        result = RyanMqttSuccessError;
        goto __exit;
    }

    result = RyanMqttNoRescourceError;

__exit:
    platformMutexUnLock(client->config.userData, &client->msgHandleLock);
    return result;
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
    RyanListAddTail(&msgHandler->list, &client->msgHandlerList); // 将msgHandler节点添加到链表尾部
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
RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, enum msgTypes packetType, uint16_t packetId, uint16_t packetLen, char *packet, RyanMqttMsgHandler_t *msgHandler, RyanMqttAckHandler_t **pAckHandler)
{
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);
    RyanMqttAssert(NULL != msgHandler);
    RyanMqttAssert(NULL != pAckHandler);

    // 给消息主题添加空格
    ackHandler = (RyanMqttAckHandler_t *)platformMemoryMalloc(sizeof(RyanMqttAckHandler_t) + packetLen + 1);
    RyanMqttCheck(NULL != ackHandler, RyanMqttNotEnoughMemError, rlog_d);
    memset(ackHandler, 0, sizeof(RyanMqttAckHandler_t) + packetLen + 1);

    RyanListInit(&ackHandler->list);
    platformTimerCutdown(&ackHandler->timer, client->config.ackTimeout); // 超时内没有响应将被销毁或重新发送

    ackHandler->repeatCount = 0;
    ackHandler->packetId = packetId;
    ackHandler->packetLen = packetLen;
    ackHandler->packetType = packetType;
    ackHandler->msgHandler = msgHandler;
    ackHandler->packet = (char *)ackHandler + sizeof(RyanMqttAckHandler_t);
    memcpy(ackHandler->packet, packet, packetLen); // 将packet数据保存到ack中

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
RyanMqttError_e RyanMqttAckListNodeFind(RyanMqttClient_t *client, enum msgTypes packetType, uint16_t packetId, RyanMqttAckHandler_t **pAckHandler)
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
        RyanMqttEventMachine(client, RyanMqttEventAckCountWarning, (void *)&client->ackHandlerCount);

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
        client->ackHandlerCount--;
    platformMutexUnLock(client->config.userData, &client->ackHandleLock);

    return RyanMqttSuccessError;
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

/**
 * @brief 清理session
 *
 * @param client
 */
void RyanMqttCleanSession(RyanMqttClient_t *client)
{
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAssert(NULL != client);

    // 释放所有msg_handler_list内存
    platformMutexLock(client->config.userData, &client->msgHandleLock);
    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);
        RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
        RyanMqttMsgHandlerDestory(client, msgHandler);
    }
    RyanListDelInit(&client->msgHandlerList);
    platformMutexUnLock(client->config.userData, &client->msgHandleLock);

    // 释放所有ackHandler_list内存
    platformMutexLock(client->config.userData, &client->ackHandleLock);
    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);
        RyanMqttAckListRemoveToAckList(client, ackHandler);
        RyanMqttAckHandlerDestroy(client, ackHandler);
    }
    RyanListDelInit(&client->ackHandlerList);
    client->ackHandlerCount = 0;
    platformMutexUnLock(client->config.userData, &client->ackHandleLock);

    // 释放所有userAckHandler_list内存
    platformMutexLock(client->config.userData, &client->userAckHandleLock);
    RyanListForEachSafe(curr, next, &client->userAckHandlerList)
    {
        userAckHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    }
    RyanListDelInit(&client->userAckHandlerList);
    platformMutexUnLock(client->config.userData, &client->userAckHandleLock);
}

const char *RyanMqttStrError(int32_t state)
{
    const char *str = NULL;

    switch (state)
    {
    case RyanMqttRecvPacketTimeOutError:
        str = "读取数据超时";
        break;

    case RyanMqttParamInvalidError:
        str = "无效参数";
        break;

    case RyanSocketFailedError:
        str = "套接字失败";
        break;

    case RyanMqttSendPacketError:
        str = "数据包发送失败";
        break;

    case RyanMqttSerializePacketError:
        str = "序列化报文失败";
        break;

    case RyanMqttDeserializePacketError:
        str = "反序列化报文失败";
        break;

    case RyanMqttNoRescourceError:
        str = "没有资源";
        break;

    case RyanMqttHaveRescourceError:
        str = "资源已存在";
        break;

    case RyanMqttNotConnectError:
        str = "mqttClient没有连接";
        break;

    case RyanMqttConnectError:
        str = "mqttClient已经连接";
        break;

    case RyanMqttRecvBufToShortError:
        str = "接收缓冲区不足";
        break;

    case RyanMqttSendBufToShortError:
        str = "发送缓冲区不足";
        break;

    case RyanMqttSocketConnectFailError:
        str = "socket连接失败";
        break;

    case RyanMqttNotEnoughMemError:
        str = "动态内存不足";
        break;

    case RyanMqttFailedError:
        str = "mqtt失败, 详细信息请看函数内部";
        break;

    case RyanMqttSuccessError:
        str = "mqtt成功, 详细信息请看函数内部";
        break;

    case RyanMqttConnectRefusedProtocolVersion:
        str = "mqtt断开连接, 服务端不支持客户端请求的 MQTT 协议级别";
        break;

    case RyanMqttConnectRefusedIdentifier:
        str = "mqtt断开连接, 不合格的客户端标识符";
        break;

    case RyanMqttConnectRefusedServer:
        str = "mqtt断开连接, 服务端不可用";
        break;

    case RyanMqttConnectRefusedUsernamePass:
        str = "mqtt断开连接, 无效的用户名或密码";
        break;

    case RyanMqttConnectRefusedNotAuthorized:
        str = "mqtt断开连接, 连接已拒绝，未授权";
        break;

    case RyanMqttConnectClientInvalid:
        str = "mqtt断开连接, 客户端处于无效状态";
        break;
    case RyanMqttConnectNetWorkFail:
        str = "mqtt断开连接, 网络错误";
        break;
    case RyanMqttConnectDisconnected:
        str = "mqtt断开连接, mqtt客户端断开连接";
        break;
    case RyanMqttKeepaliveTimeout:
        str = "mqtt断开连接, 心跳超时断开连接";
        break;
    case RyanMqttConnectUserDisconnected:
        str = "mqtt断开连接, 用户手动断开连接";
        break;
    case RyanMqttConnectTimeout:
        str = "mqtt断开连接, connect超时断开";
        break;

    default:
        str = "未知错误描述";
        break;
    }

    return str;
}
