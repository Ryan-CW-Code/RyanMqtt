#define rlogLevel (rlogLvlDebug) // 日志打印等级

#include "RyanMqttThread.h"

void RyanMqttRefreshKeepaliveTime(RyanMqttClient_t *client)
{
    // 服务器在心跳时间的1.5倍内没有收到keeplive消息则会断开连接
    // 这里算 1.4 b倍时间内没有收到心跳就断开连接
    platformCriticalEnter(client->config.userData, &client->criticalLock);
    platformTimerCutdown(&client->keepaliveTimer, (uint32_t)(1000 * client->config.keepaliveTimeoutS * 1.4)); // 启动心跳定时器
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
    RyanMqttError_e result = RyanMqttFailedError;
    uint32_t timeRemain = 0;
    RyanMqttAssert(NULL != client);

    // mqtt没有连接就退出
    if (RyanMqttConnectState != RyanMqttGetClientState(client))
        return RyanMqttNotConnectError;

    timeRemain = platformTimerRemain(&client->keepaliveTimer);

    // 超过设置的 1.4 倍心跳周期，主动通知用户断开连接
    if (0 == timeRemain)
    {
        RyanMqttConnectStatus_e connectState = RyanMqttKeepaliveTimeout;
        RyanMqttEventMachine(client, RyanMqttEventDisconnected, (void *)&connectState);
        rlog_d("ErrorCode: %d, strError: %s", RyanMqttKeepaliveTimeout, RyanMqttStrError(RyanMqttKeepaliveTimeout));
        return RyanMqttFailedError;
    }

    // 当剩余时间小于 recvtimeout 时强制发送心跳包
    if (timeRemain > client->config.recvTimeout)
    {
        // 当到达 0.9 倍时间时发送心跳包
        if (timeRemain < 1000 * 0.9 * client->config.keepaliveTimeoutS)
            return RyanMqttSuccessError;

        // 节流时间内不发送心跳报文
        if (platformTimerRemain(&client->keepaliveThrottleTimer))
            return RyanMqttSuccessError;
    }

    // 发送mqtt心跳包
    {
        // MQTT_PACKET_PINGREQ_SIZE
        MQTTStatus_t status = MQTTSuccess;
        uint8_t buffer[2] = {0};
        MQTTFixedBuffer_t fixedBuffer = {
            .pBuffer = buffer,
            .size = 2};

        // 序列化数据包
        status = MQTT_SerializePingreq(&fixedBuffer);
        RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

        result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

        platformTimerCutdown(&client->keepaliveThrottleTimer, client->config.recvTimeout + 1500); // 启动心跳检查节流定时器
    }

    return RyanMqttSuccessError;
}

/**
 * @brief qos1或者qos2接收消息成功
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubackAndPubcompPacketHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    uint16_t packetId = 0;
    RyanMqttAckHandler_t *ackHandler = NULL;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttAssert(NULL != client);

    // 反序列化ack包
    status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 可能会多次收到 puback / pubcomp,仅在首次收到时触发发布成功回调函数
    result = RyanMqttAckListNodeFind(client, pIncomingPacket->type & 0xF0U, packetId, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { rlog_i("packetType: %02x, packetId: %d", pIncomingPacket->type & 0xF0U, packetId); });

    RyanMqttAckListRemoveToAckList(client, ackHandler);

    RyanMqttEventMachine(client, RyanMqttEventPublished, (void *)ackHandler); // 回调函数
    RyanMqttAckHandlerDestroy(client, ackHandler);                            // 销毁ackHandler
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
    RyanMqttError_e result = RyanMqttFailedError;
    uint16_t packetId = 0;
    RyanMqttAckHandler_t *ackHandler = NULL;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttAssert(NULL != client);

    // 反序列化ack包
    status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 删除pubrel记录
    result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREL, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttAckListRemoveToAckList(client, ackHandler);
        RyanMqttAckHandlerDestroy(client, ackHandler);
    }

    // 制作确认数据包并发送
    uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
    MQTTFixedBuffer_t fixedBuffer = {
        .pBuffer = buffer,
        .size = MQTT_PUBLISH_ACK_PACKET_SIZE};

    // 序列化ack数据包
    status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBCOMP, packetId);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 每次收到PUBREL都返回消息
    result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

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
    RyanMqttError_e result = RyanMqttFailedError;
    uint16_t packetId = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAckHandler_t *ackHandlerPubrec = NULL;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttAssert(NULL != client);

    // 反序列化ack包
    status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 每次收到PUBREC都返回ack,确保服务器可以认为数据包被发送了
    uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
    MQTTFixedBuffer_t fixedBuffer = {
        .pBuffer = buffer,
        .size = MQTT_PUBLISH_ACK_PACKET_SIZE};

    // 序列化ack数据包
    status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBREL, packetId);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
    result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREC, packetId, &ackHandlerPubrec);
    if (RyanMqttSuccessError == result)
    {
        // 查找ack链表是否存在pubcomp报文,不存在表示首次接收到
        result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBCOMP, packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
        {
            // 首次收到消息
            result = RyanMqttMsgHandlerCreate(client, ackHandlerPubrec->msgHandler->topic,
                                              ackHandlerPubrec->msgHandler->topicLen,
                                              ackHandlerPubrec->msgHandler->qos, &msgHandler);
            RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

            result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_PUBCOMP, packetId, MQTT_PUBLISH_ACK_PACKET_SIZE, fixedBuffer.pBuffer, msgHandler, &ackHandler, RyanMqttFalse);
            RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttMsgHandlerDestory(client->config.userData, msgHandler); });
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
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

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
    uint16_t packetId = 0;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttMsgData_t msgData = {0};
    RyanMqttMsgHandler_t *msgHandler = NULL;

    RyanMqttAssert(NULL != client);

    {
        // 反系列化 publish 消息
        MQTTPublishInfo_t publishInfo = {0};
        status = MQTT_DeserializePublish(pIncomingPacket, &packetId, &publishInfo);
        RyanMqttCheck(MQTTSuccess == status, RyanMqttDeserializePacketError, rlog_d);

        msgData.topic = (char *)publishInfo.pTopicName;
        msgData.topicLen = publishInfo.topicNameLength;
        msgData.packetId = packetId;
        msgData.payload = (char *)publishInfo.pPayload;
        msgData.payloadLen = publishInfo.payloadLength;
        msgData.qos = (RyanMqttQos_e)publishInfo.qos;
        msgData.retained = publishInfo.retain;
        msgData.dup = publishInfo.dup;
    }

    // 查看订阅列表是否包含此消息主题,进行通配符匹配。不包含就直接退出在一定程度上可以防止恶意攻击
    result = RyanMqttMsgHandlerFind(client, msgData.topic, msgData.topicLen, RyanMqttTrue, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    switch (msgData.qos)
    {
    case RyanMqttQos0:
        RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        break;

    case RyanMqttQos1:
    {
        // 先分发消息，再回答ack
        RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);

        uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
        MQTTFixedBuffer_t fixedBuffer = {
            .pBuffer = buffer,
            .size = MQTT_PUBLISH_ACK_PACKET_SIZE};

        // 序列化ack数据包
        status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBACK, packetId);
        RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

        result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    }

    break;

    case RyanMqttQos2: // qos2采用方法B
    {
        RyanMqttAckHandler_t *ackHandler = NULL;
        uint8_t buffer[MQTT_PUBLISH_ACK_PACKET_SIZE];
        MQTTFixedBuffer_t fixedBuffer = {
            .pBuffer = buffer,
            .size = MQTT_PUBLISH_ACK_PACKET_SIZE};

        // !序列化ack数据包,必须先执行，因为创建ack需要用到这个报文
        status = MQTT_SerializeAck(&fixedBuffer, MQTT_PACKET_TYPE_PUBREC, packetId);
        RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

        // 上面代码不太可能出错，出错后就让服务器重新发送吧
        // 收到publish就期望收到PUBREL，如果PUBREL报文已经存在说明不是首次收到publish, 不进行qos2 PUBREC消息处理
        result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_PUBREL, msgData.packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
        {
            result = RyanMqttMsgHandlerCreate(client, msgData.topic, msgData.topicLen, msgData.qos, &msgHandler);
            RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

            result = RyanMqttAckHandlerCreate(client, MQTT_PACKET_TYPE_PUBREL, msgData.packetId, MQTT_PUBLISH_ACK_PACKET_SIZE, fixedBuffer.pBuffer, msgHandler, &ackHandler, RyanMqttFalse);
            RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttMsgHandlerDestory(client->config.userData, msgHandler); });
            RyanMqttAckListAddToAckList(client, ackHandler);

            RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        }

        result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, MQTT_PUBLISH_ACK_PACKET_SIZE);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    }

    break;

    default:
        break;
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
    // todo 服务端可以授予比订阅者要求的低一些的 QoS 等级。
    RyanMqttError_e result = RyanMqttSuccessError;
    uint16_t packetId = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttAssert(NULL != client);

    // 反序列化ack包
    status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // 需要判断服务器拒绝
    // RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // ack链表不存在当前订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_SUBACK, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 订阅失败，服务器拒绝
    if (MQTTSuccess != status)
    {
        RyanMqttAckListRemoveToAckList(client, ackHandler);

        // mqtt事件回调
        RyanMqttEventMachine(client, RyanMqttEventSubscribedFaile, (void *)ackHandler->msgHandler);

        RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
        return RyanMqttSuccessError;
    }

    // 订阅成功
    // 查找是否有同名订阅，如果有就销毁之前的
    // result = RyanMqttMsgHandlerFind(client, ackHandler->msgHandler->topic, ackHandler->msgHandler->topicLen, RyanMqttFalse, &msgHandler);
    // if (RyanMqttSuccessError == result)
    // {
    //     RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
    //     RyanMqttMsgHandlerDestory(client, msgHandler);
    // }
    rlog_w("mqtt11111111 topic: %s, qos: %d", ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
    result = RyanMqttMsgHandlerCreate(client, ackHandler->msgHandler->topic,
                                      ackHandler->msgHandler->topicLen,
                                      ackHandler->msgHandler->qos, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d); // 这里创建失败了不触发回调，等待ack超时触发失败回调函数

    rlog_w("mqtt222222222222222 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);

    RyanMqttEventMachine(client, RyanMqttEventSubscribed, (void *)msgHandler); // mqtt回调函数

    RyanMqttMsgHandlerAddToMsgList(client, msgHandler); // 将msg信息添加到订阅链表上

    RyanMqttAckListRemoveToAckList(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler

    return result;
}

/**
 * @brief 取消订阅确认处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttUnSubackHandler(RyanMqttClient_t *client, MQTTPacketInfo_t *pIncomingPacket)
{
    RyanMqttError_e result = RyanMqttFailedError;
    RyanMqttMsgHandler_t *subMsgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    uint16_t packetId = 0;
    MQTTStatus_t status = MQTTSuccess;
    RyanMqttAssert(NULL != client);

    // 反序列化ack包
    status = MQTT_DeserializeAck(pIncomingPacket, &packetId, NULL);
    RyanMqttCheck(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d);

    // ack链表不存在当前取消订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, MQTT_PACKET_TYPE_UNSUBACK, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 查找当前主题是否已经订阅,进行取消订阅
    result = RyanMqttMsgHandlerFind(client, ackHandler->msgHandler->topic, ackHandler->msgHandler->topicLen, RyanMqttFalse, &subMsgHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttMsgHandlerRemoveToMsgList(client, subMsgHandler);
        RyanMqttMsgHandlerDestory(client, subMsgHandler);
    }

    RyanMqttAckListRemoveToAckList(client, ackHandler);

    // mqtt事件回调
    RyanMqttEventMachine(client, RyanMqttEventUnSubscribed, (void *)ackHandler->msgHandler);

    RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler

    return result;
}

/**
 * @brief 将用户空间的ack链表搬到mqtt线程空间
 *
 * @param client
 */
static void RyanMqttSyncUserAckHandle(RyanMqttClient_t *client)
{
    RyanMqttAckHandler_t *userAckHandler = NULL;
    RyanList_t *curr = NULL,
               *next = NULL;

    platformMutexLock(client->config.userData, &client->userAckHandleLock);
    RyanListForEachSafe(curr, next, &client->userAckHandlerList)
    {
        // 获取此节点的结构体
        userAckHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckListAddToAckList(client, userAckHandler);
    }
    platformMutexUnLock(client->config.userData, &client->userAckHandleLock);
}

/**
 * @brief mqtt数据包处理函数
 *
 * @param client
 * @param packetType
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttReadPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    MQTTPacketInfo_t pIncomingPacket = {0};

    RyanMqttAssert(NULL != client);

    NetworkContext_t pNetworkContext = {.client = client};
    MQTTStatus_t status = MQTT_GetIncomingPacketTypeAndLength(coreMqttTransportRecv, &pNetworkContext, &pIncomingPacket);

    // 先同步用户接口的ack链表
    RyanMqttSyncUserAckHandle(client);

    if (MQTTSuccess == status)
    {
        // 申请断开连接数据包的空间
        if (pIncomingPacket.remainingLength > 0)
        {
            pIncomingPacket.pRemainingData = platformMemoryMalloc(pIncomingPacket.remainingLength);
            RyanMqttCheck(NULL != pIncomingPacket.pRemainingData, RyanMqttNoRescourceError, rlog_d);
        }
    }
    else if (MQTTNoDataAvailable == status)
    {
        return RyanMqttRecvPacketTimeOutError;
    }
    else
    {
        rlog_e("获取包长度失败");
        return RyanMqttFailedError;
    }

    // 3.读取mqtt载荷数据并放到读取缓冲区
    if (pIncomingPacket.remainingLength > 0)
    {
        result = RyanMqttRecvPacket(client, pIncomingPacket.pRemainingData, pIncomingPacket.remainingLength);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    }

    rlog_d("pIncomingPacket.type: %x ", pIncomingPacket.type & 0xF0U);

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
        MQTTStatus_t status = MQTTSuccess;
        uint16_t packetId;
        bool sessionPresent; // 会话位

        // 反序列化ack包
        status = MQTT_DeserializeAck(&pIncomingPacket, &packetId, &sessionPresent);
        if (MQTTSuccess != status)
            result = RyanMqttFailedError;
    }
    break;

    case MQTT_PACKET_TYPE_PUBACK:  // QoS 1消息发布收到确认
    case MQTT_PACKET_TYPE_PUBCOMP: // 发送QOS2 发布完成
        result = RyanMqttPubackAndPubcompPacketHandler(client, &pIncomingPacket);
        break;

    case MQTT_PACKET_TYPE_PUBREC: // 发送QOS2 发布收到
        result = RyanMqttPubrecPacketHandler(client, &pIncomingPacket);
        break;

    case (MQTT_PACKET_TYPE_PUBREL & 0xF0U): // 接收QOS2 发布释放
        if (pIncomingPacket.type & 0x02U)   // PUBREL 控制报文固定报头的第 3,2,1,0 位必须被设置为 0,0,1,0。必须将其它的任何值都当做是不合法的并关闭网络连接
            result = RyanMqttPubrelPacketHandler(client, &pIncomingPacket);
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
        break;
    }

    if (pIncomingPacket.remainingLength > 0)
        platformMemoryFree(pIncomingPacket.pRemainingData);

    return result;
}

// 也可以考虑有ack链表的时候recvTime可以短一些，有坑点
/**
 * @brief 遍历ack链表，进行相应的处理
 *
 * @param client
 * @param WaitFlag
 *      WaitFlag : RyanMqttFalse 表示不需要等待超时立即处理这些数据包。通常在重新连接后立即进行处理
 *      WaitFlag : RyanMqttTrue 表示需要等待超时再处理这些消息，一般是稳定连接下的超时处理
 */
static void RyanMqttAckListScan(RyanMqttClient_t *client, RyanMqttBool_e WaitFlag)
{
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    platformTimer_t ackScanRemainTimer = {0};
    RyanMqttAssert(NULL != client);

    // mqtt没有连接就退出
    if (RyanMqttConnectState != RyanMqttGetClientState(client))
        return;

    // 节流时间内不检查ack链表
    if (platformTimerRemain(&client->ackScanThrottleTimer))
        return;

    // 设置scan最大处理时间定时器
    platformTimerInit(&ackScanRemainTimer);
    platformTimerCutdown(&ackScanRemainTimer, client->config.recvTimeout - 100);

    platformMutexLock(client->config.userData, &client->ackHandleLock);
    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        // 需要再判断一次
        if (RyanMqttConnectState != RyanMqttGetClientState(client))
            continue;

        // 超过最大处理时间，直接跳出处理函数
        if (0 == platformTimerRemain(&ackScanRemainTimer))
            break;

        // 获取此节点的结构体
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

        // ack响应没有超时就不进行处理
        if (RyanMqttTrue == WaitFlag && 0 != platformTimerRemain(&ackHandler->timer))
            continue;

        switch (ackHandler->packetType)
        {
        // 发送qos1 / qos2消息, 服务器ack响应超时。需要重新发送它们。
        case MQTT_PACKET_TYPE_PUBACK:  // qos1 publish后没有收到puback
        case MQTT_PACKET_TYPE_PUBREC:  // qos2 publish后没有收到pubrec
        case MQTT_PACKET_TYPE_PUBREL:  // qos2 收到pubrec，发送pubrel后没有收到pubcomp
        case MQTT_PACKET_TYPE_PUBCOMP: // 理论不会出现，冗余措施
        {
            // 避免 implicit-fallthrough 警告
            if (MQTT_PACKET_TYPE_PUBREC == ackHandler->packetType || MQTT_PACKET_TYPE_PUBACK == ackHandler->packetType)
                MQTT_UpdateDuplicatePublishFlag(ackHandler->packet, true); // 设置重发标志位

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
            platformTimerCutdown(&ackHandler->timer, client->config.ackTimeout);
            ackHandler->repeatCount++;
            break;
        }

        // 订阅 / 取消订阅超时就认为失败
        case MQTT_PACKET_TYPE_SUBACK:
        case MQTT_PACKET_TYPE_UNSUBACK:
        {
            RyanMqttAckListRemoveToAckList(client, ackHandler);

            RyanMqttEventMachine(client, (MQTT_PACKET_TYPE_SUBACK == ackHandler->packetType) ? RyanMqttEventSubscribedFaile : RyanMqttEventUnSubscribedFaile,
                                 (void *)ackHandler->msgHandler);

            // 清除句柄
            RyanMqttAckHandlerDestroy(client, ackHandler);
            break;
        }

        default:
        {
            rlog_e("不应该出现的值: %d", ackHandler->packetType);
            RyanMqttAssert(NULL); // 不应该为别的值
            break;
        }
        }
    }
    platformMutexUnLock(client->config.userData, &client->ackHandleLock);

    // 扫描链表没有超时时，才设置scan节流定时器
    if (platformTimerRemain(&ackScanRemainTimer))
        platformTimerCutdown(&client->ackScanThrottleTimer, 1000); // 启动ack scan节流定时器
}

/**
 * @brief mqtt连接函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttConnect(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    MQTTStatus_t status = MQTTSuccess;
    MQTTConnectInfo_t connectInfo = {0};
    MQTTPublishInfo_t willInfo = {0};
    MQTTFixedBuffer_t fixedBuffer = {0};
    size_t remainingLength = {0};
    RyanMqttAssert(NULL != client);

    RyanMqttCheck(RyanMqttConnectState != RyanMqttGetClientState(client), RyanMqttConnectError, rlog_d);

    // connect 信息
    connectInfo.pClientIdentifier = client->config.clientId;
    connectInfo.clientIdentifierLength = strlen(client->config.clientId);
    connectInfo.pUserName = client->config.userName;
    if (connectInfo.pUserName)
        connectInfo.userNameLength = strlen(client->config.userName);

    connectInfo.pPassword = client->config.password;
    if (connectInfo.pPassword)
        connectInfo.passwordLength = strlen(client->config.password);
    connectInfo.keepAliveSeconds = client->config.keepaliveTimeoutS;
    connectInfo.cleanSession = client->config.cleanSessionFlag;

    if (RyanMqttTrue == client->lwtFlag)
    {
        willInfo.qos = (MQTTQoS_t)client->lwtOptions.qos;
        willInfo.retain = client->lwtOptions.retain;
        willInfo.pPayload = client->lwtOptions.payload;
        willInfo.payloadLength = client->lwtOptions.payloadLen;
        willInfo.pTopicName = client->lwtOptions.topic;
        willInfo.topicNameLength = strlen(client->lwtOptions.topic);
    }

    // 获取数据包大小
    status = MQTT_GetConnectPacketSize(&connectInfo, RyanMqttTrue == client->lwtFlag ? &willInfo : NULL, &remainingLength, &fixedBuffer.size);
    RyanMqttAssert(MQTTSuccess == status);

    // 申请数据包的空间
    fixedBuffer.pBuffer = platformMemoryMalloc(fixedBuffer.size);
    RyanMqttCheck(NULL != fixedBuffer.pBuffer, RyanMqttNoRescourceError, rlog_d);

    // 序列化数据包
    status = MQTT_SerializeConnect(&connectInfo, RyanMqttTrue == client->lwtFlag ? &willInfo : NULL, remainingLength, &fixedBuffer);
    RyanMqttCheckCode(MQTTSuccess == status, RyanMqttSerializePacketError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    // 调用底层的连接函数连接上服务器
    result = platformNetworkConnect(client->config.userData, &client->network, client->config.host, client->config.port);
    RyanMqttCheckCode(RyanMqttSuccessError == result, RyanSocketFailedError, rlog_d, { platformMemoryFree(fixedBuffer.pBuffer); });

    // 发送序列化mqtt的CONNECT报文
    result = RyanMqttSendPacket(client, fixedBuffer.pBuffer, fixedBuffer.size);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        platformNetworkClose(client->config.userData, &client->network);
        platformMemoryFree(fixedBuffer.pBuffer);
    });

    // 等待报文
    // mqtt规范 服务端接收到connect报文后，服务端发送给客户端的第一个报文必须是 CONNACK
    result = RyanMqttReadPacketHandler(client);
    RyanMqttCheckCode(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_d, {
        platformNetworkClose(client->config.userData, &client->network);
        platformMemoryFree(fixedBuffer.pBuffer);
    });

    platformMemoryFree(fixedBuffer.pBuffer);

    return RyanMqttSuccessError;
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
        RyanMqttAckListScan(client, RyanMqttFalse); // 扫描确认列表，销毁已超时的确认处理程序或重新发送它们
        RyanMqttSetClientState(client, RyanMqttConnectState);
        break;

    case RyanMqttEventDisconnected:                              // 断开连接事件
        RyanMqttSetClientState(client, RyanMqttDisconnectState); // 先将客户端状态设置为断开连接,避免close网络资源时用户依然在使用
        platformNetworkClose(client->config.userData, &client->network);

        if (RyanMqttTrue == client->config.cleanSessionFlag)
            RyanMqttCleanSession(client);

        break;

    case RyanMqttEventReconnectBefore: // 重连前回调
        RyanMqttSetClientState(client, RyanMqttReconnectState);
        break;

    default:
        break;
    }

    if (client->config.mqttEventHandle == NULL)
        return;

    if (client->eventFlag & eventId)
        client->config.mqttEventHandle(client, eventId, eventData);
}

// todo 考虑将发送操作独立出去，异步发送
/**
 * @brief mqtt运行线程
 *
 * @param argument
 */
void RyanMqttThread(void *argument)
{
    int32_t result = 0;
    RyanMqttClient_t *client = (RyanMqttClient_t *)argument;
    RyanMqttAssert(NULL != client); // RyanMqttStart前没有调用RyanMqttInit

    while (1)
    {
        // 销毁客户端
        if (RyanMqttTrue == client->destoryFlag)
        {
            RyanMqttEventMachine(client, RyanMqttEventDestoryBefore, (void *)NULL);

            // 关闭网络组件
            platformNetworkClose(client->config.userData, &client->network);

            // 销毁网络组件
            platformNetworkDestroy(client->config.userData, &client->network);

            // 清除config信息
            if (NULL != client->config.clientId)
                platformMemoryFree(client->config.clientId);
            if (NULL != client->config.userName)
                platformMemoryFree(client->config.userName);
            if (NULL != client->config.password)
                platformMemoryFree(client->config.password);
            if (NULL != client->config.host)
                platformMemoryFree(client->config.host);
            if (NULL != client->config.taskName)
                platformMemoryFree(client->config.taskName);

            // 清除遗嘱相关配置
            if (NULL != client->lwtOptions.payload)
                platformMemoryFree(client->lwtOptions.payload);

            if (NULL != client->lwtOptions.topic)
                platformMemoryFree(client->lwtOptions.topic);

            // 清除session  ack链表和msg链表
            RyanMqttCleanSession(client);

            // 清除互斥锁
            platformMutexDestroy(client->config.userData, &client->sendBufLock);
            platformMutexDestroy(client->config.userData, &client->msgHandleLock);
            platformMutexDestroy(client->config.userData, &client->ackHandleLock);
            platformMutexDestroy(client->config.userData, &client->userAckHandleLock);

            // 清除临界区
            platformCriticalDestroy(client->config.userData, &client->criticalLock);

            // 清除掉线程动态资源
            platformThread_t mqttThread;
            memcpy(&mqttThread, &client->mqttThread, sizeof(platformThread_t));
            void *userData = client->config.userData;

            platformMemoryFree(client);
            client = NULL;

            // 销毁自身线程
            platformThreadDestroy(userData, &mqttThread);
        }

        // 客户端状态变更状态机
        switch (client->clientState)
        {

        case RyanMqttStartState: // 开始状态状态
            rlog_d("初始化状态，开始连接");
            result = RyanMqttConnect(client);
            RyanMqttEventMachine(client, RyanMqttSuccessError == result ? RyanMqttEventConnected : RyanMqttEventDisconnected,
                                 (void *)&result);
            break;

        case RyanMqttConnectState: // 连接状态
            rlog_d("连接状态");
            result = RyanMqttReadPacketHandler(client);
            RyanMqttAckListScan(client, RyanMqttTrue);
            RyanMqttKeepalive(client);
            break;

        case RyanMqttDisconnectState: // 断开连接状态
            rlog_d("断开连接状态");
            if (RyanMqttTrue != client->config.autoReconnectFlag) // 没有使能自动连接就休眠线程
            {
                platformThreadStop(client->config.userData, &client->mqttThread);

                // 断连的时候会暂停线程，线程重新启动就是用户手动连接了
                rlog_d("手动重新连接\r\n");
                RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL);
            }
            else
            {
                rlog_d("触发自动连接，%dms后开始连接\r\n", client->config.reconnectTimeout);
                platformDelay(client->config.reconnectTimeout);
                RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL); // 给上层触发重新连接前事件
            }
            break;

        case RyanMqttReconnectState:
            result = RyanMqttConnect(client);
            RyanMqttEventMachine(client, RyanMqttSuccessError == result ? RyanMqttEventConnected : RyanMqttEventDisconnected,
                                 (void *)&result);
            break;

        default:
            RyanMqttAssert(NULL);
            break;
        }
    }
}
