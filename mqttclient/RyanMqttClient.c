#define rlogLevel (rlogLvlInfo) // 日志打印等级

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtile.h"
#include "RyanMqttThread.h"

/**
 * @brief Returns the next valid MQTT packet identifier for the client.
 *
 * Ensures the packet identifier is non-zero and wraps around to 1 if it exceeds the maximum allowed value.
 * The operation is performed within a critical section to guarantee thread safety.
 *
 * @return uint16_t The next packet identifier to use for MQTT packets.
 */
static uint16_t RyanMqttGetNextPacketId(RyanMqttClient_t *client)
{
    uint16_t packetId;
    RyanMqttAssert(NULL != client);
    platformCriticalEnter(client->config.userData, &client->criticalLock);
    if (client->packetId >= RyanMqttMaxPacketId || client->packetId < 1)
        client->packetId = 1;
    else
        client->packetId++;
    packetId = client->packetId;
    platformCriticalExit(client->config.userData, &client->criticalLock);
    return packetId;
}

/**
 * @brief Sets a configuration string value by copying a new string and freeing the previous value.
 *
 * Frees the memory pointed to by `*dest`, copies the string from `rest` into newly allocated memory, and updates `*dest` to point to the new string.
 *
 * @param dest Pointer to the destination string pointer to update.
 * @param rest Source string to copy.
 * @return RyanMqttSuccessError if the operation succeeds, RyanMqttFailedError if memory allocation fails.
 */
static RyanMqttError_e setConfigValue(char **dest, char const *const rest)
{
    RyanMqttAssert(NULL != dest && NULL != rest);

    // if (0 == strcmp(*dest, rest))
    //     return RyanMqttSuccessError;

    platformMemoryFree(*dest);

    RyanMqttStringCopy(dest, (char *)rest, strlen(rest));
    if (NULL == *dest)
        return RyanMqttFailedError;

    return RyanMqttSuccessError;
}

/**
 * @brief Initializes a new MQTT client instance.
 *
 * Allocates and initializes all resources and internal structures required for an MQTT client, including synchronization primitives, handler lists, timers, and network interfaces. Sets the client to its initial state and returns a pointer to the created client.
 *
 * @param pClient Pointer to where the initialized MQTT client instance will be stored.
 * @return RyanMqttSuccessError on success, or an error code if initialization fails.
 */
RyanMqttError_e RyanMqttInit(RyanMqttClient_t **pClient)
{
    RyanMqttClient_t *client = NULL;
    RyanMqttCheck(NULL != pClient, RyanMqttParamInvalidError, rlog_d);

    client = (RyanMqttClient_t *)platformMemoryMalloc(sizeof(RyanMqttClient_t));
    RyanMqttCheck(NULL != client, RyanMqttNotEnoughMemError, rlog_d);
    memset(client, 0, sizeof(RyanMqttClient_t));
    platformCriticalInit(client->config.userData, &client->criticalLock); // 初始化临界区

    client->packetId = 1; // 控制报文必须包含一个非零的 16 位报文标识符
    client->clientState = RyanMqttInitState;
    client->eventFlag = 0;
    client->ackHandlerCount = 0;
    client->lwtFlag = RyanMqttFalse;

    platformMutexInit(client->config.userData, &client->sendBufLock); // 初始化发送缓冲区互斥锁

    RyanListInit(&client->msgHandlerList);
    platformMutexInit(client->config.userData, &client->msgHandleLock);

    RyanListInit(&client->ackHandlerList);
    platformMutexInit(client->config.userData, &client->ackHandleLock);

    RyanListInit(&client->userAckHandlerList);
    platformMutexInit(client->config.userData, &client->userAckHandleLock);

    RyanMqttSetClientState(client, RyanMqttInitState);
    platformTimerInit(&client->keepaliveTimer);

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
 * @brief Starts the MQTT client and creates the MQTT processing thread.
 *
 * Transitions the client from the initialization state to the start state and initializes the MQTT thread.
 * Returns an error if called when the client is not in the initialization state or if thread creation fails.
 *
 * @return RyanMqttError_e Error code indicating the result of the operation.
 */
RyanMqttError_e RyanMqttStart(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttInitState == RyanMqttGetClientState(client), RyanMqttFailedError, rlog_d);

    RyanMqttSetClientState(client, RyanMqttStartState);
    // 连接成功，需要初始化 MQTT 线程
    result = platformThreadInit(client->config.userData,
                                &client->mqttThread,
                                client->config.taskName,
                                RyanMqttThread,
                                client,
                                client->config.taskStack,
                                client->config.taskPrio);
    RyanMqttCheckCode(RyanMqttSuccessError == result, RyanMqttNotEnoughMemError, rlog_d, { RyanMqttSetClientState(client, RyanMqttInitState); });

    return RyanMqttSuccessError;
}

/**
 * @brief Disconnects the MQTT client from the server.
 *
 * If requested, sends a DISCONNECT packet to the MQTT server before disconnecting. Updates the client state to reflect the disconnection.
 *
 * @param sendDiscFlag If RyanMqttTrue, a DISCONNECT packet is sent to the server; if RyanMqttFalse, no packet is sent.
 * @return RyanMqttError_e Result of the disconnect operation.
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
        RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

        // 发送断开连接数据包
        RyanMqttError_e result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

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
        return RyanMqttNoRescourceError;

    platformThreadStart(client->config.userData, &client->mqttThread);
    return RyanMqttSuccessError;
}

/**
 * @brief Subscribes the MQTT client to a specified topic with a given QoS level.
 *
 * Allocates and serializes a SUBSCRIBE packet, creates message and acknowledgment handlers, and sends the subscription request to the MQTT broker. The granted QoS may be lower than requested and can be checked in the subscription success callback.
 *
 * @param topic The topic filter to subscribe to.
 * @param qos The requested Quality of Service level for the subscription.
 * @return RyanMqttError_e Result code indicating success or the type of error encountered.
 */
RyanMqttError_e RyanMqttSubscribe(RyanMqttClient_t *client, char *topic, RyanMqttQos_e qos)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    uint16_t packetId = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;
    MQTTSubscribeInfo_t subscriptionList[1] = {0};
    MQTTStatus_t status = MQTTSuccess;
    MQTTFixedBuffer_t fixedBuffer = {0};
    size_t remainingLength = 0;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    subscriptionList[0].qos = (MQTTQoS_t)qos;
    subscriptionList[0].pTopicFilter = topic;
    subscriptionList[0].topicFilterLength = strlen(topic);
    rlog_w("qos: %d", subscriptionList[0].qos);

    // 获取数据包大小
    status = MQTT_GetSubscribePacketSize(subscriptionList, 1, &remainingLength, &fixedBuffer.size);
    RyanMqttAssert(MQTTSuccess == status);

    // 申请数据包的空间
    fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
    RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d);

    // 序列化数据包
    packetId = RyanMqttGetNextPacketId(client);
    status = MQTT_SerializeSubscribe(subscriptionList, 1, packetId, remainingLength, &fixedBuffer);
    RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    // 创建msg包
    result = RyanMqttMsgHandlerCreate(client, subscriptionList[0].pTopicFilter, subscriptionList[0].topicFilterLength, (RyanMqttQos_e)subscriptionList[0].qos, &msgHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_SUBACK,
                                      packetId, 0, NULL, msgHandler, &userAckHandler, RyanMqttFalse);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        platformMemoryFree(fixedBuffer.pBuffer);
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
    });

    RyanMqttAckListAddToUserAckList(client, userAckHandler);

    // 如果发送失败就清除ack链表,创建ack链表必须在发送前
    result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    });

    platformMemoryFree(fixedBuffer.pBuffer);
    return result;
}

/**
 * @brief Unsubscribes the client from a specified MQTT topic.
 *
 * Attempts to remove the subscription for the given topic if it exists. Serializes and sends an UNSUBSCRIBE packet to the MQTT broker, and manages acknowledgment handlers for the operation.
 *
 * @param topic The topic string to unsubscribe from.
 * @return RyanMqttError_e Result code indicating success or the type of failure.
 */
RyanMqttError_e RyanMqttUnSubscribe(RyanMqttClient_t *client, char *topic)
{
    RyanMqttError_e result = RyanMqttFailedError;
    uint16_t packetId;
    RyanMqttMsgHandler_t *subMsgHandler = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;
    MQTTSubscribeInfo_t subscriptionList[1] = {0};
    MQTTStatus_t status = MQTTSuccess;
    MQTTFixedBuffer_t fixedBuffer = {0};
    size_t remainingLength = 0;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    subscriptionList[0].qos = (MQTTQoS_t)RyanMqttQos0; // 无效数据
    subscriptionList[0].pTopicFilter = topic;
    subscriptionList[0].topicFilterLength = strlen(topic);

    // 查找当前主题是否已经订阅,没有订阅就取消发送
    result = RyanMqttMsgHandlerFind(client, subscriptionList[0].pTopicFilter, subscriptionList[0].topicFilterLength, RyanMqttFalse, &subMsgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 获取数据包大小
    status = MQTT_GetUnsubscribePacketSize(subscriptionList, 1, &remainingLength, &fixedBuffer.size);
    RyanMqttAssert(MQTTSuccess == status);

    // 申请数据包的空间
    fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
    RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d);

    // 序列化数据包
    packetId = RyanMqttGetNextPacketId(client);
    status = MQTT_SerializeUnsubscribe(subscriptionList, 1, packetId, remainingLength, &fixedBuffer);
    RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    result = RyanMqttMsgHandlerCreate(client, subMsgHandler->topic, subMsgHandler->topicLen, subMsgHandler->qos, &msgHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_UNSUBACK, packetId, 0, NULL, msgHandler, &userAckHandler, RyanMqttFalse);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        platformMemoryFree(fixedBuffer.pBuffer);
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
    });

    RyanMqttAckListAddToUserAckList(client, userAckHandler);

    result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    });

    platformMemoryFree(fixedBuffer.pBuffer);
    return result;
}

/**
 * @brief Publishes a message to a specified MQTT topic.
 *
 * Sends a message to the given topic with the specified payload, QoS, and retain flag. For QoS 1 and 2, the function manages acknowledgment handlers to ensure reliable delivery and retransmission if necessary.
 *
 * @param topic The topic to publish to.
 * @param payload The message payload. May be NULL if payloadLen is zero.
 * @param payloadLen Length of the payload in bytes.
 * @param qos Quality of Service level for the message.
 * @param retain Whether the message should be retained by the broker.
 * @return RyanMqttError_e Result of the publish operation.
 */
RyanMqttError_e RyanMqttPublish(RyanMqttClient_t *client, char *topic, char *payload, uint32_t payloadLen, RyanMqttQos_e qos, RyanMqttBool_e retain)
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

    if (payloadLen > 0 && NULL == payload) // 报文支持有效载荷长度为0
        return RyanMqttParamInvalidError;

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

    // 序列化数据包
    packetId = RyanMqttGetNextPacketId(client);
    status = MQTT_SerializePublish(&publishInfo, packetId, remainingLength, &fixedBuffer);
    RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    if (RyanMqttQos0 == qos)
    {
        // 发送报文
        result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });
        platformMemoryFree(fixedBuffer.pBuffer);
    }
    else
    {
        RyanMqttMsgHandler_t *msgHandler = NULL;
        RyanMqttAckHandler_t *userAckHandler = NULL;
        // qos1 / qos2需要收到预期响应ack,否则数据将被重新发送
        result = RyanMqttMsgHandlerCreate(client, publishInfo.pTopicName, publishInfo.topicNameLength, qos, &msgHandler);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

        result = RyanMqttAckHandlerCreate(client, (RyanMqttQos1 == qos) ? MQTT_PACKET_TYPE_PUBACK : MQTT_PACKET_TYPE_PUBREC, packetId, fixedBuffer.size, fixedBuffer.pBuffer, msgHandler, &userAckHandler, RyanMqttTrue);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
            platformMemoryFree(fixedBuffer.pBuffer);
            RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
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
        return RyanMqttInvalidState;

    return RyanMqttGetClientState(client);
}

/**
 * @brief Retrieves the list of currently subscribed topics and their QoS levels.
 *
 * Copies up to `msgHandleSize` subscribed topics and their QoS values into the provided array.
 * The actual number of subscriptions copied is returned via `subscribeNum`.
 *
 * @param msgHandles Array to receive the subscribed topics and QoS values.
 * @param msgHandleSize Maximum number of entries to copy into `msgHandles`.
 * @param subscribeNum Pointer to an integer where the number of subscriptions copied will be stored.
 * @return RyanMqttError_e Returns RyanMqttSuccessError on success, or RyanMqttNoRescourceError if the provided array is too small.
 */
RyanMqttError_e RyanMqttGetSubscribe(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles, int32_t msgHandleSize, int32_t *subscribeNum)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != subscribeNum, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(0 < msgHandleSize, RyanMqttParamInvalidError, rlog_d);

    *subscribeNum = 0;

    platformMutexLock(client->config.userData, &client->msgHandleLock);
    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);
        msgHandles[*subscribeNum].topic = msgHandler->topic;
        msgHandles[*subscribeNum].qos = msgHandler->qos;

        (*subscribeNum)++;

        if (*subscribeNum >= msgHandleSize)
        {
            result = RyanMqttNoRescourceError;
            goto __next;
        }
    }

__next:
    platformMutexUnLock(client->config.userData, &client->msgHandleLock);
    return result;
}

/**
 * @brief Retrieves the total number of subscribed topics for the MQTT client.
 *
 * @param subscribeTotalCount Pointer to an integer where the total count will be stored.
 * @return RyanMqttSuccessError on success, or RyanMqttParamInvalidError if parameters are invalid.
 */
RyanMqttError_e RyanMqttGetSubscribeTotalCount(RyanMqttClient_t *client, int32_t *subscribeTotalCount)
{
    RyanList_t *curr = NULL,
               *next = NULL;

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
/* RyanMqttError_e RyanMqttGetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t **pclientConfig)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClientConfig_t *clientConfig = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != pclientConfig, RyanMqttParamInvalidError, rlog_d);

    RyanMqttCheck(NULL != client->config, RyanMqttNoRescourceError);

    clientConfig = (RyanMqttClientConfig_t *)platformMemoryMalloc(sizeof(RyanMqttClientConfig_t));
    RyanMqttCheck(NULL != clientConfig, RyanMqttNotEnoughMemError);

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
 * @brief Sets the MQTT client configuration.
 *
 * Updates the client's configuration parameters such as client ID, host, credentials, timeouts, and flags. This operation is not thread-safe and must be performed only when the MQTT thread is not running or is safely suspended to avoid resource conflicts. If the function fails, the client is destroyed and further MQTT operations must be stopped.
 *
 * @return RyanMqttSuccessError on success, RyanMqttFailedError on failure (client is destroyed).
 */
RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig)
{
    RyanMqttError_e result = RyanMqttSuccessError;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->clientId, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->host, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->taskName, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(clientConfig->recvTimeout <= clientConfig->keepaliveTimeoutS * 1000 / 2, RyanMqttParamInvalidError, rlog_d);
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
 * @brief Configures the Last Will and Testament (LWT) message for the MQTT client.
 *
 * Sets the topic, payload, QoS, and retain flag for the LWT message, which will be included in the CONNECT packet sent to the broker. This function must be called before establishing a connection (e.g., before RyanMqttStart or during the RyanMqttEventReconnectBefore event).
 *
 * @param topicName The topic for the LWT message.
 * @param payload The payload for the LWT message. Can be NULL if payloadLen is 0.
 * @param payloadLen The length of the payload in bytes.
 * @param qos The Quality of Service level for the LWT message.
 * @param retain Whether the LWT message should be retained by the broker.
 * @return RyanMqttSuccessError on success, or an error code on failure.
 */
RyanMqttError_e RyanMqttSetLwt(RyanMqttClient_t *client, char *topicName, char *payload, uint32_t payloadLen, RyanMqttQos_e qos, RyanMqttBool_e retain)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topicName, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, rlog_d);

    if (payloadLen > 0 && NULL == payload) // 报文支持有效载荷长度为0
        return RyanMqttParamInvalidError;

    if (NULL != client->lwtOptions.topic)
        platformMemoryFree(client->lwtOptions.topic);

    if (NULL != client->lwtOptions.payload)
        platformMemoryFree(client->lwtOptions.payload);

    memset(&client->lwtOptions, 0, sizeof(lwtOptions_t));

    result = RyanMqttStringCopy(&client->lwtOptions.payload, payload, payloadLen);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    result = RyanMqttStringCopy(&client->lwtOptions.topic, topicName, strlen(topicName));
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(client->lwtOptions.payload); });

    client->lwtFlag = RyanMqttTrue;
    client->lwtOptions.qos = qos;
    client->lwtOptions.retain = retain;
    client->lwtOptions.payloadLen = payloadLen;

    return RyanMqttSuccessError;
}

/**
 * @brief Discards a specified acknowledgment handler from the MQTT client.
 *
 * Removes the given acknowledgment handler from the client's acknowledgment list, invokes the associated event callback, and destroys the handler.
 *
 * @return RyanMqttError_e Result of the operation.
 */
RyanMqttError_e RyanMqttDiscardAckHandler(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler)
{
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

    RyanMqttEventMachine(client, RyanMqttEventAckHandlerdiscard, (void *)ackHandler); // 回调函数

    RyanMqttAckListRemoveToAckList(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler);
    return RyanMqttSuccessError;
}

/**
 * @brief Registers an event ID for the MQTT client.
 *
 * Sets the specified event ID in the client's event flag in a thread-safe manner.
 *
 * @param eventId The event identifier to register.
 * @return RyanMqttSuccessError on success, or RyanMqttParamInvalidError if the client is NULL.
 */
RyanMqttError_e RyanMqttRegisterEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

    platformCriticalEnter(client->config.userData, &client->criticalLock);
    client->eventFlag |= eventId;
    platformCriticalExit(client->config.userData, &client->criticalLock);
    return RyanMqttSuccessError;
}

/**
 * @brief Cancels a registered event ID for the MQTT client.
 *
 * Clears the specified event ID from the client's event flag in a thread-safe manner.
 *
 * @param eventId The event identifier to cancel.
 * @return RyanMqttSuccessError on success, or RyanMqttParamInvalidError if the client is NULL.
 */
RyanMqttError_e RyanMqttCancelEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

    platformCriticalEnter(client->config.userData, &client->criticalLock);
    client->eventFlag &= ~eventId;
    platformCriticalExit(client->config.userData, &client->criticalLock);
    return RyanMqttSuccessError;
}
