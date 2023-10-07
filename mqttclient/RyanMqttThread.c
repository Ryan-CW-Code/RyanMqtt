#define rlogEnable 1               // 是否使能日志
#define rlogColorEnable 1          // 是否使能日志颜色
#define rlogLevel (rlogLvlWarning) // 日志打印等级
#define rlogTag "RyanMqttThread"   // 日志tag

#include "RyanMqttLog.h"
#include "MQTTPacket.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtile.h"
#include "RyanMqttThread.h"

// void RyanMqttEventMachine(RyanMqttClient_t *client, RyanMqttEventId_e eventId, void *eventData);

/**
 * @brief mqtt心跳保活
 *
 * @param client
 * @return int32_t
 */
static RyanMqttError_e RyanMqttKeepalive(RyanMqttClient_t *client)
{
    int32_t connectState = RyanMqttConnectAccepted;
    int32_t packetLen = 0;
    RyanMqttAssert(NULL != client);

    // 如果没有连接则不需要心跳保活
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    // 服务器在心跳时间的1.5倍内没有收到消息则会断开连接
    // 在心跳的一半发送keepalive
    if (platformTimerRemain(&client->keepaliveTimer) != 0)
        return RyanMqttSuccessError;

    // 心跳超时，断开连接
    connectState = RyanMqttKeepaliveTimeout;
    RyanMqttCheckCode(2 > client->keepaliveTimeoutCount, RyanMqttKeepaliveTimeout, rlog_d,
                      { RyanMqttEventMachine(client, RyanMqttEventDisconnected, (void *)&connectState); });

    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    packetLen = MQTTSerialize_pingreq((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize);
    if (packetLen > 0)
        RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    client->keepaliveTimeoutCount++;
    platformTimerCutdown(&client->keepaliveTimer, client->config->keepaliveTimeoutS * 1000 / 2); // 启动心跳定时器
    return RyanMqttSuccessError;
}

/**
 * @brief
 *
 * @param client
 * @return int32_t
 */
static RyanMqttError_e RyanMqttGetPayloadLen(RyanMqttClient_t *client, uint32_t *payloadLen)
{

    RyanMqttError_e result = RyanMqttSuccessError;
    uint8_t len = 0;
    uint8_t encodedByte = 0;
    int32_t multiplier = 1;
    RyanMqttAssert(NULL != client);
    // RyanMqttAssert(NULL != payloadLen); payloadLen == 0时会误判

    do
    {
        RyanMqttCheck(++len < 4, RyanMqttFailedError, rlog_d);

        result = RyanMqttRecvPacket(client, (char *)&encodedByte, 1);
        RyanMqttCheck(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_d);

        *payloadLen += (encodedByte & 127) * multiplier; // 根据 MQTT 协议解码数据长度
        multiplier *= 128;

    } while ((encodedByte & 128) != 0);

    RyanMqttCheck(*payloadLen <= RyanMqttMaxPayloadLen, RyanMqttFailedError, rlog_d);

    return RyanMqttSuccessError;
}

/**
 * @brief qos1或者qos2接收消息成功
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubackAndPubcompPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    uint8_t dup = 0;
    uint8_t packetType = 0;
    uint16_t packetId = 0;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 可能会多次收到puback / pubcomp,仅在首次收到时触发发布成功回调函数
    result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { rlog_i("packetType: %d, packetId: %d", packetType, packetId); });

    RyanMqttEventMachine(client, RyanMqttEventPublished, (void *)ackHandler); // 回调函数

    RyanMqttAckListRemove(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
    return result;
}

/**
 * @brief 发布释放处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubrelPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttFailedError;
    uint8_t dup = 0;
    uint8_t packetType = 0;
    uint16_t packetId = 0;
    int32_t packetLen = 0;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 制作确认数据包并发送
    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    packetLen = MQTTSerialize_ack((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, PUBCOMP, 0, packetId);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 每次收到PUBREL都返回消息
    result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    // 删除pubrel记录
    result = RyanMqttAckListNodeFind(client, PUBREL, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttAckListRemove(client, ackHandler);
        RyanMqttAckHandlerDestroy(client, ackHandler);
    }

    return RyanMqttSuccessError;
}

/**
 * @brief 发布收到处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPubrecPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttFailedError;
    uint8_t dup = 0;
    RyanMqttBool_e fastFlag = RyanMqttFalse;
    uint8_t packetType = 0;
    uint16_t packetId = 0;
    int32_t packetLen = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAckHandler_t *ackHandlerPubrec = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
    result = RyanMqttAckListNodeFind(client, PUBREC, packetId, &ackHandlerPubrec);
    if (RyanMqttSuccessError == result)
    {
        // 查找ack链表是否存在pubcomp报文,不存在表示首次接收到
        result = RyanMqttAckListNodeFind(client, PUBCOMP, packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
        {
            fastFlag = RyanMqttTrue;
        }
        // 出现pubrec和pubcomp同时存在的情况,清除pubrec理论上不会出现
        else
        {
            RyanMqttAckListRemove(client, ackHandlerPubrec);
            RyanMqttAckHandlerDestroy(client, ackHandlerPubrec);
        }
    }

    // 制作确认数据包并发送
    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    // 序列化发布释放报文
    packetLen = MQTTSerialize_ack((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, PUBREL, 0, packetId);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 每次收到PUBREC都返回ack
    result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
    if (RyanMqttTrue == fastFlag)
    {
        result = RyanMqttMsgHandlerCreate(ackHandlerPubrec->msgHandler->topic,
                                          strlen(ackHandlerPubrec->msgHandler->topic),
                                          ackHandlerPubrec->msgHandler->qos, &msgHandler);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });

        // 创建一个 ACK 处理程序节点
        result = RyanMqttAckHandlerCreate(client, PUBCOMP, packetId, packetLen, msgHandler, &ackHandler);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                          { platformMemoryFree(msgHandler);
             platformMutexUnLock(client->config->userData, client->sendBufLock); });
    }
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    // 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
    if (RyanMqttTrue == fastFlag)
    {
        result = RyanMqttAckListAdd(client, ackHandler);
        // 保证等待PUBCOMP记录成功后再清除PUBREC记录
        RyanMqttAckListRemove(client, ackHandlerPubrec);
        RyanMqttAckHandlerDestroy(client, ackHandlerPubrec);
    }

    return result;
}

/**
 * @brief 收到服务器发布消息处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttPublishPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t packetLen = 0;
    RyanMqttBool_e deliverMsgFlag = RyanMqttFalse;
    MQTTString topicName = MQTTString_initializer;
    RyanMqttMsgData_t msgData = {0};
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_publish(&msgData.dup, (int *)&msgData.qos, &msgData.retained, &msgData.packetId, &topicName,
                                     (uint8_t **)&msgData.payload, (int *)&msgData.payloadLen, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 查看订阅列表是否包含此消息主题,进行通配符匹配。不包含就直接退出在一定程度上可以防止恶意攻击
    result = RyanMqttMsgHandlerFind(client, topicName.lenstring.data, topicName.lenstring.len, RyanMqttTrue, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    switch (msgData.qos)
    {
    case RyanMqttQos0:
        deliverMsgFlag = RyanMqttTrue;
        break;

    case RyanMqttQos1:
        platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_ack((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, PUBACK, 0, msgData.packetId);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });

        result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });
        platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

        deliverMsgFlag = RyanMqttTrue;

        break;

    case RyanMqttQos2:
    {
        RyanMqttBool_e fastFlag = RyanMqttFalse;
        // 收到publish就期望收到PUBREL，如果PUBREL报文已经存在说明不是首次收到publish不进行qos2消息处理
        result = RyanMqttAckListNodeFind(client, PUBREL, msgData.packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
            fastFlag = RyanMqttTrue;

        platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_ack((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, PUBREC, 0, msgData.packetId);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });

        result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });

        if (RyanMqttTrue == fastFlag)
        {
            result = RyanMqttMsgHandlerCreate(topicName.lenstring.data, topicName.lenstring.len, msgData.qos, &msgHandler);
            RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
                platformMutexUnLock(client->config->userData, client->sendBufLock);
            });

            result = RyanMqttAckHandlerCreate(client, PUBREL, msgData.packetId, packetLen, msgHandler, &ackHandler);
            RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
                platformMemoryFree(msgHandler);
                platformMutexUnLock(client->config->userData, client->sendBufLock);
            });
        }
        platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

        if (RyanMqttTrue == fastFlag)
        {
            result = RyanMqttAckListAdd(client, ackHandler);
            deliverMsgFlag = RyanMqttTrue;
        }
    }

    break;

    default:
        break;
    }

    if (RyanMqttTrue == deliverMsgFlag)
    {
        // 复制主题名字
        result = RyanMqttStringCopy(&msgData.topic, topicName.lenstring.data, topicName.lenstring.len);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
        RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        platformMemoryFree(msgData.topic);
    }

    return result;
}

/**
 * @brief 订阅确认处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttSubackHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t count = 0;
    int32_t grantedQoS = 0;
    uint16_t packetId = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_suback(&packetId, 1, (int *)&count, (int *)&grantedQoS, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // ack链表不存在当前订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, SUBACK, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 订阅失败
    if (RyanMqttSubFail == grantedQoS)
    {
        // mqtt事件回调
        RyanMqttEventMachine(client, RyanMqttEventSubscribedFaile, (void *)ackHandler->msgHandler);
        RyanMqttAckListRemove(client, ackHandler);
        RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
        return RyanMqttSuccessError;
    }

    // 订阅成功
    // 查找是否有同名订阅，如果有就销毁之前的
    result = RyanMqttMsgHandlerFind(client, ackHandler->msgHandler->topic, strlen(ackHandler->msgHandler->topic), RyanMqttFalse, &msgHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttMsgHandlerRemove(msgHandler);
        RyanMqttMsgHandlerDestory(msgHandler);
    }

    // 服务端可以授予比订阅者要求的低一些的 QoS 等级。
    result = RyanMqttMsgHandlerCreate(ackHandler->msgHandler->topic,
                                      strlen(ackHandler->msgHandler->topic),
                                      grantedQoS, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d); // 这里创建失败了不触发回调，等待ack超时触发失败回调函数

    RyanMqttMsgHandlerAdd(client, msgHandler);                                 // 将msg信息添加到订阅链表上
    RyanMqttEventMachine(client, RyanMqttEventSubscribed, (void *)msgHandler); // mqtt回调函数
    RyanMqttAckListRemove(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler

    return result;
}

/**
 * @brief 取消订阅确认处理函数
 *
 * @param client
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttUnSubackHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttFailedError;
    RyanMqttAckHandler_t *ackHandler = NULL;
    uint16_t packetId = 0;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_unsuback(&packetId, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // ack链表不存在当前取消订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, UNSUBACK, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // mqtt事件回调
    RyanMqttEventMachine(client, RyanMqttEventUnSubscribed, (void *)ackHandler->msgHandler);

    RyanMqttAckListRemove(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler

    return result;
}

/**
 * @brief mqtt数据包处理函数
 *
 * @param client
 * @param packetType
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttReadPacketHandler(RyanMqttClient_t *client, uint8_t *packetType)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t fixedHeaderLen = 1;
    uint32_t payloadLen = 0;
    MQTTHeader header = {0};
    RyanMqttAssert(NULL != client);
    // RyanMqttAssert(NULL != packetType); packetType == 0时会误判

    // 1.读取标头字节。 其中包含数据包类型
    result = RyanMqttRecvPacket(client, client->config->recvBuffer, fixedHeaderLen);
    if (RyanMqttRecvPacketTimeOutError == result)
        return RyanMqttRecvPacketTimeOutError;
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 填充联合体标头信息
    header.byte = client->config->recvBuffer[0];
    rlog_d("packetType: %d", header.bits.type);
    RyanMqttCheck(CONNECT <= header.bits.type && DISCONNECT >= header.bits.type, result, rlog_d);

    // 2.读取mqtt报文剩余长度。 这本身是可变的
    result = RyanMqttGetPayloadLen(client, &payloadLen);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 将剩余长度编码成mqtt报文，并放入接收缓冲区,如果消息长度超过缓冲区长度则抛弃此次数据
    fixedHeaderLen += MQTTPacket_encode((uint8_t *)client->config->recvBuffer + fixedHeaderLen, payloadLen);
    RyanMqttCheckCode((fixedHeaderLen + payloadLen) <= client->config->recvBufferSize, RyanMqttRecvBufToShortError, rlog_d,
                      { RyanMqttRecvPacket(client, client->config->recvBuffer, payloadLen); });

    // 3.读取mqtt载荷数据并放到读取缓冲区
    if (payloadLen > 0)
    {
        result = RyanMqttRecvPacket(client, client->config->recvBuffer + fixedHeaderLen, payloadLen);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    }

    // 控制报文类型
    switch (header.bits.type)
    {

    case CONNACK: // 连接报文确认
        break;

    case PUBACK:  // QoS 1消息发布收到确认
    case PUBCOMP: // 发布完成(qos2 第三步)
        result = RyanMqttPubackAndPubcompPacketHandler(client);
        break;

    case PUBREC: // 发布收到(qos2 第一步)
        result = RyanMqttPubrecPacketHandler(client);
        break;

    case PUBREL: // 发布释放(qos2 第二步)
        result = RyanMqttPubrelPacketHandler(client);
        break;

    case SUBACK: // 订阅确认
        result = RyanMqttSubackHandler(client);
        break;

    case UNSUBACK: // 取消订阅确认
        result = RyanMqttUnSubackHandler(client);
        break;

    case PUBLISH: // 接收到订阅消息
        result = RyanMqttPublishPacketHandler(client);
        break;

    case PINGRESP: // 心跳响应
        client->keepaliveTimeoutCount = 0;
        break;

    default:
        break;
    }

    if (packetType)
        *packetType = header.bits.type;

    return result;
}

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
    RyanMqttAssert(NULL != client);

    // 如果链表为空或者mqtt没有连接就退出
    if ((RyanListIsEmpty(&client->ackHandlerList)) || (RyanMqttConnectState != RyanMqttGetClientState(client)))
        return;

    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        // 获取此节点的结构体
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

        // ack响应没有超时就不进行处理
        if (0 != platformTimerRemain(&ackHandler->timer) && RyanMqttTrue == WaitFlag)
            continue;

        switch (ackHandler->packetType)
        {
        // 发送qos1 / qos2消息, 服务器ack响应超时。需要重新发送它们。
        case PUBACK:
        case PUBREC:
        case PUBREL:
        case PUBCOMP:
        {

            if (RyanMqttConnectState != RyanMqttGetClientState(client))
                continue;

            // 重发数据事件回调
            RyanMqttEventMachine(client, RyanMqttEventRepeatPublishPacket, (void *)ackHandler);

            RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen); // 重新发送数据

            // 重置ack超时时间
            platformTimerCutdown(&ackHandler->timer, client->config->ackTimeout);
            ackHandler->repeatCount++;

            // 重发次数超过警告值回调
            if (ackHandler->repeatCount >= client->config->ackHandlerRepeatCountWarning)
                RyanMqttEventMachine(client, RyanMqttEventAckRepeatCountWarning, (void *)ackHandler);

            break;
        }

        // 订阅 / 取消订阅超时就认为失败
        case SUBACK:
        case UNSUBACK:
        {
            RyanMqttEventMachine(client, (SUBACK == ackHandler->packetType) ? RyanMqttEventSubscribedFaile : RyanMqttEventUnSubscribedFaile,
                                 (void *)ackHandler->msgHandler);
            // 清除句柄
            RyanMqttAckListRemove(client, ackHandler);
            RyanMqttAckHandlerDestroy(client, ackHandler);
            break;
        }

        default:
        {
            rlog_e("不应该出现的值: %d", ackHandler->packetType);
            RyanMqttAssert(NULL); // 不应该为别的值
        }

        break;
        }
    }
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
    uint8_t sessionPresent = 0; // 会话存在标志
    uint8_t packetType = 0;     // 接收到的报文类型
    int32_t packetLen = 0;
    int32_t connackRc = 0;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    RyanMqttAssert(NULL != client);
    RyanMqttAssert(NULL != client->network);
    RyanMqttAssert(NULL != client->config);

    RyanMqttCheck(RyanMqttConnectState != RyanMqttGetClientState(client), RyanMqttConnectAccepted, rlog_d);

    // 连接标志位
    connectData.clientID.cstring = client->config->clientId;
    connectData.username.cstring = client->config->userName;
    connectData.password.cstring = client->config->password;
    connectData.keepAliveInterval = client->config->keepaliveTimeoutS;
    connectData.cleansession = client->config->cleanSessionFlag;
    connectData.MQTTVersion = client->config->mqttVersion;

    if (RyanMqttTrue == client->lwtFlag)
    {
        connectData.willFlag = 1;
        connectData.will.qos = client->lwtOptions->qos;
        connectData.will.retained = client->lwtOptions->retain;
        connectData.will.message.lenstring.data = client->lwtOptions->payload;
        connectData.will.message.lenstring.len = client->lwtOptions->payloadLen;
        connectData.will.topicName.cstring = client->lwtOptions->topic;
    }

    // 调用底层的连接函数连接上服务器
    result = platformNetworkConnect(client->config->userData, client->network, client->config->host, client->config->port);
    RyanMqttCheck(RyanMqttSuccessError == result, RyanMqttConnectNetWorkFail, rlog_d);

    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    // 序列化mqtt的CONNECT报文
    packetLen = MQTTSerialize_connect((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, &connectData);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 发送序列化mqtt的CONNECT报文
    result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 等待报文
    // mqtt规范 服务端接收到connect报文后，服务端发送给客户端的第一个报文必须是 CONNACK
    result = RyanMqttReadPacketHandler(client, &packetType);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });
    RyanMqttCheckCode(CONNACK == packetType, RyanMqttConnectDisconnected, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    // 解析CONNACK报文
    result = MQTTDeserialize_connack(&sessionPresent, (uint8_t *)&connackRc, (uint8_t *)client->config->recvBuffer, client->config->recvBufferSize);
    RyanMqttCheckCode(1 == result, RyanMqttDeserializePacketError, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁
    rlog_i("result: %d, packetLen: %d, packetType: %d connackRc: %d", result, packetLen, packetType, connackRc);

    return connackRc;
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
    RyanMqttAssert(NULL != client->network);
    RyanMqttAssert(NULL != client->config);

    switch (eventId)
    {
    case RyanMqttEventConnected:                                                                       // 连接成功
        client->keepaliveTimeoutCount = 0;                                                             // 重置心跳超时计数器
        platformTimerCutdown(&client->keepaliveTimer, (client->config->keepaliveTimeoutS * 1000 / 2)); // 启动心跳定时器
        RyanMqttAckListScan(client, RyanMqttFalse);                                                    // 扫描确认列表，销毁已超时的确认处理程序或重新发送它们
        RyanMqttSetClientState(client, RyanMqttConnectState);
        break;

    case RyanMqttEventDisconnected:                              // 断开连接事件
        RyanMqttSetClientState(client, RyanMqttDisconnectState); // 先将客户端状态设置为断开连接,避免close网络资源时用户依然在使用
        platformNetworkClose(client->config->userData, client->network);

        if (RyanMqttTrue == client->config->cleanSessionFlag)
            RyanMqttCleanSession(client);

        break;

    case RyanMqttEventReconnectBefore: // 重连前回调
        RyanMqttSetClientState(client, RyanMqttReconnectState);
        break;

    default:
        break;
    }

    if (client->config->mqttEventHandle == NULL)
        return;

    if (client->eventFlag & eventId)
        client->config->mqttEventHandle(client, eventId, eventData);
}

/**
 * @brief mqtt运行线程
 *
 * @param argument
 */
void RyanMqttThread(void *argument)
{
    int32_t result = 0;
    RyanMqttClient_t *client = (RyanMqttClient_t *)argument;
    RyanMqttAssert(NULL != client);          // RyanMqttStart前没有调用RyanMqttInit
    RyanMqttAssert(NULL != client->network); // RyanMqttStart前没有调用RyanMqttInit
    RyanMqttAssert(NULL != client->config);  // RyanMqttStart前没有调用RyanMqttSetConfig

    while (1)
    {

        // 销毁客户端
        if (RyanMqttTrue == client->destoryFlag)
        {

            RyanMqttEventMachine(client, RyanMqttEventDestoryBefore, (void *)NULL);

            // 清除网络组件
            if (NULL != client->network)
            {
                platformNetworkClose(client->config->userData, client->network);
                platformMemoryFree(client->network);
                client->network = NULL;
            }

            // 清除互斥锁
            if (NULL != client->sendBufLock)
            {
                platformMutexDestroy(client->config->userData, client->sendBufLock);
                platformMemoryFree(client->sendBufLock);
                client->sendBufLock = NULL;
            }

            // 清除config信息
            if (NULL != client->config)
            {
                if (NULL != client->config->clientId)
                    platformMemoryFree(client->config->clientId);

                if (NULL != client->config->host)
                    platformMemoryFree(client->config->host);

                if (NULL != client->config->port)
                    platformMemoryFree(client->config->port);

                if (NULL != client->config->userName)
                    platformMemoryFree(client->config->userName);

                if (NULL != client->config->password)
                    platformMemoryFree(client->config->password);

                if (NULL != client->config->taskName)
                    platformMemoryFree(client->config->taskName);

                if (NULL != client->config)
                    platformMemoryFree(client->config);
            }

            // 清除遗嘱相关配置
            if (RyanMqttTrue == client->lwtFlag && NULL != client->lwtOptions)
            {
                if (NULL != client->lwtOptions->topic)
                    platformMemoryFree(client->lwtOptions->topic);

                platformMemoryFree(client->lwtOptions);
            }

            // 清除session  ack链表和msg链表
            RyanMqttCleanSession(client);

            platformThread_t mqttThread = *client->mqttThread;
            void *userData = client->config->userData;

            // 清除掉线程动态资源
            platformMemoryFree(client->mqttThread);
            client->mqttThread = NULL;

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
            RyanMqttEventMachine(client, RyanMqttConnectAccepted == result ? RyanMqttEventConnected : RyanMqttEventDisconnected,
                                 (void *)&result);
            break;

        case RyanMqttConnectState: // 连接状态
            rlog_d("连接状态");
            result = RyanMqttReadPacketHandler(client, NULL);
            RyanMqttAckListScan(client, RyanMqttTrue);
            RyanMqttKeepalive(client);
            break;

        case RyanMqttDisconnectState: // 断开连接状态
            rlog_d("断开连接状态");
            if (RyanMqttTrue != client->config->autoReconnectFlag) // 没有使能自动连接就休眠线程
                platformThreadStop(client->config->userData, client->mqttThread);

            rlog_d("触发自动连接，%dms后开始连接\r\n", client->config->reconnectTimeout);
            platformDelay(client->config->reconnectTimeout);
            RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL); // 给上层触发重新连接前事件

            break;

        case RyanMqttReconnectState:
            result = RyanMqttConnect(client);
            RyanMqttEventMachine(client, RyanMqttConnectAccepted == result ? RyanMqttEventConnected : RyanMqttEventDisconnected,
                                 (void *)&result);
            break;

        default:
            RyanMqttAssert(NULL);
            break;
        }
    }
}
