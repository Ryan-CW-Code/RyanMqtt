// #define rlogEnable               // 是否使能日志
#define rlogColorEnable          // 是否使能日志颜色
#define rlogLevel (rlogLvlError) // 日志打印等级
#define rlogTag "RyanMqttThread" // 日志tag

#include "RyanMqttThread.h"

void RyanMqttRefreshKeepaliveTime(RyanMqttClient_t *client)
{
    // 服务器在心跳时间的1.5倍内没有收到keeplive消息则会断开连接
    // 这里在用户设置的心跳剩余 1/4 时手动发送保活指令
    platformCriticalEnter(client->config.userData, &client->criticalLock);
    platformTimerCutdown(&client->keepaliveTimer, client->config.keepaliveTimeoutS / 4 * 3 * 1000); // 启动心跳定时器
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
    RyanMqttConnectStatus_e connectState = RyanMqttKeepaliveTimeout;
    int32_t packetLen = 0;
    RyanMqttAssert(NULL != client);

    // 心跳周期到了发送keepalive
    if (0 != platformTimerRemain(&client->keepaliveTimer))
    {
        client->keepaliveTimeoutCount = 0;
        return RyanMqttSuccessError;
    }

    // 发送5次都没有收到服务器的心跳保活响应,主动认为断连
    // 其实服务器也会主动断连的
    if (client->keepaliveTimeoutCount > 5)
    {
        connectState = RyanMqttKeepaliveTimeout;
        RyanMqttEventMachine(client, RyanMqttEventDisconnected, (void *)&connectState);
        rlog_d("ErrorCode: %d, strError: %s", RyanMqttKeepaliveTimeout, RyanMqttStrError(RyanMqttKeepaliveTimeout));
        return RyanMqttKeepaliveTimeout;
    }

    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    packetLen = MQTTSerialize_pingreq((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize);
    if (packetLen > 0)
        RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁

    client->keepaliveTimeoutCount++;
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

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 可能会多次收到 puback / pubcomp,仅在首次收到时触发发布成功回调函数
    result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { rlog_i("packetType: %d, packetId: %d", packetType, packetId); });

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
static RyanMqttError_e RyanMqttPubrelPacketHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttFailedError;
    uint8_t dup = 0;
    uint8_t packetType = 0;
    uint16_t packetId = 0;
    int32_t packetLen = 0;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 删除pubrel记录
    result = RyanMqttAckListNodeFind(client, PUBREL, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttAckListRemoveToAckList(client, ackHandler);
        RyanMqttAckHandlerDestroy(client, ackHandler);
    }

    // 制作确认数据包并发送
    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    packetLen = MQTTSerialize_ack((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, PUBCOMP, 0, packetId);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config.userData, &client->sendBufLock); });

    // 每次收到PUBREL都返回消息
    result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

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
    uint8_t packetType = 0;
    uint16_t packetId = 0;
    int32_t packetLen = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAckHandler_t *ackHandlerPubrec = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_ack(&packetType, &dup, &packetId, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // 只在首次收到pubrec, 并pubcomp不存在于ack链表时，才创建pubcmp到ack链表,再删除pubrec记录
    result = RyanMqttAckListNodeFind(client, PUBREC, packetId, &ackHandlerPubrec);
    if (RyanMqttSuccessError == result)
    {
        // 查找ack链表是否存在pubcomp报文,不存在表示首次接收到
        result = RyanMqttAckListNodeFind(client, PUBCOMP, packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
        {
            result = RyanMqttMsgHandlerCreate(client, ackHandlerPubrec->msgHandler->topic,
                                              ackHandlerPubrec->msgHandler->topicLen,
                                              ackHandlerPubrec->msgHandler->qos, &msgHandler);
            RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

            result = RyanMqttAckHandlerCreate(client, PUBCOMP, packetId, packetLen, client->config.sendBuffer, msgHandler, &ackHandler);
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

    // 每次收到PUBREC都返回ack,制作确认数据包并发送
    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    // 序列化发布释放报文
    packetLen = MQTTSerialize_ack((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, PUBREL, 0, packetId);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
        platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
    });

    result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

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
    MQTTString topicName = MQTTString_initializer;
    RyanMqttMsgData_t msgData = {0};
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_publish(&msgData.dup, (int *)&msgData.qos, &msgData.retained, &msgData.packetId, &topicName,
                                     (uint8_t **)&msgData.payload, (int *)&msgData.payloadLen, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    msgData.topic = topicName.lenstring.data;
    msgData.topicLen = topicName.lenstring.len;

    // 查看订阅列表是否包含此消息主题,进行通配符匹配。不包含就直接退出在一定程度上可以防止恶意攻击
    result = RyanMqttMsgHandlerFind(client, topicName.lenstring.data, topicName.lenstring.len, RyanMqttTrue, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    switch (msgData.qos)
    {
    case RyanMqttQos0:
        RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        break;

    case RyanMqttQos1:
        platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_ack((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, PUBACK, 0, msgData.packetId);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                          { platformMutexUnLock(client->config.userData, &client->sendBufLock); });

        result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
        platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

        RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        break;

    case RyanMqttQos2: // qos2采用方法B
        // 收到publish就期望收到PUBREL，如果PUBREL报文已经存在说明不是首次收到publish, 不进行qos2 PUBREC消息处理
        result = RyanMqttAckListNodeFind(client, PUBREL, msgData.packetId, &ackHandler);
        if (RyanMqttSuccessError != result)
        {
            result = RyanMqttMsgHandlerCreate(client, topicName.lenstring.data, topicName.lenstring.len, msgData.qos, &msgHandler);
            RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

            result = RyanMqttAckHandlerCreate(client, PUBREL, msgData.packetId, packetLen, client->config.sendBuffer, msgHandler, &ackHandler);
            RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttMsgHandlerDestory(client->config.userData, msgHandler); });
            RyanMqttAckListAddToAckList(client, ackHandler);

            RyanMqttEventMachine(client, RyanMqttEventData, (void *)&msgData);
        }

        platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_ack((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, PUBREC, 0, msgData.packetId);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
            platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
        });

        result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
        platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
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
static RyanMqttError_e RyanMqttSubackHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t count = 0;
    int32_t grantedQoS = 0;
    uint16_t packetId = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_suback(&packetId, 1, (int *)&count, (int *)&grantedQoS, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // ack链表不存在当前订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, SUBACK, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 订阅失败
    if (RyanMqttSubFail == grantedQoS)
    {
        RyanMqttAckListRemoveToAckList(client, ackHandler);

        // mqtt事件回调
        RyanMqttEventMachine(client, RyanMqttEventSubscribedFaile, (void *)ackHandler->msgHandler);

        RyanMqttAckHandlerDestroy(client, ackHandler); // 销毁ackHandler
        return RyanMqttSuccessError;
    }

    // 订阅成功
    // 查找是否有同名订阅，如果有就销毁之前的
    result = RyanMqttMsgHandlerFind(client, ackHandler->msgHandler->topic, ackHandler->msgHandler->topicLen, RyanMqttFalse, &msgHandler);
    if (RyanMqttSuccessError == result)
    {
        RyanMqttMsgHandlerRemoveToMsgList(client, msgHandler);
        RyanMqttMsgHandlerDestory(client, msgHandler);
    }

    // 服务端可以授予比订阅者要求的低一些的 QoS 等级。
    result = RyanMqttMsgHandlerCreate(client, ackHandler->msgHandler->topic,
                                      ackHandler->msgHandler->topicLen,
                                      grantedQoS, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d); // 这里创建失败了不触发回调，等待ack超时触发失败回调函数

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
static RyanMqttError_e RyanMqttUnSubackHandler(RyanMqttClient_t *client)
{
    RyanMqttError_e result = RyanMqttFailedError;
    RyanMqttMsgHandler_t *subMsgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    uint16_t packetId = 0;
    RyanMqttAssert(NULL != client);

    result = MQTTDeserialize_unsuback(&packetId, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

    // ack链表不存在当前取消订阅确认节点就直接退出
    result = RyanMqttAckListNodeFind(client, UNSUBACK, packetId, &ackHandler);
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
 * @brief mqtt数据包处理函数
 *
 * @param client
 * @param packetType
 * @return RyanMqttError_e
 */
static RyanMqttError_e RyanMqttReadPacketHandler(RyanMqttClient_t *client, uint8_t *packetType)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    uint32_t fixedHeaderLen = 1;
    uint32_t payloadLen = 0;
    MQTTHeader header = {0};
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;

    RyanMqttAssert(NULL != client);
    // RyanMqttAssert(NULL != packetType); packetType == 0时会误判

    // 1.读取标头字节。 其中包含数据包类型
    result = RyanMqttRecvPacket(client, client->config.recvBuffer, fixedHeaderLen);

    // 同步用户接口的ack链表
    platformMutexLock(client->config.userData, &client->userAckHandleLock);
    RyanListForEachSafe(curr, next, &client->userAckHandlerList)
    {
        // 获取此节点的结构体
        userAckHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckListAddToAckList(client, userAckHandler);
    }
    platformMutexUnLock(client->config.userData, &client->userAckHandleLock);

    if (RyanMqttRecvPacketTimeOutError == result)
        return RyanMqttRecvPacketTimeOutError;
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 填充联合体标头信息
    header.byte = client->config.recvBuffer[0];
    rlog_d("packetType: %d", header.bits.type);
    RyanMqttCheck(CONNECT <= header.bits.type && DISCONNECT >= header.bits.type, result, rlog_d);

    // 2.读取mqtt报文剩余长度。 这本身是可变的
    result = RyanMqttGetPayloadLen(client, &payloadLen);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 将剩余长度编码成mqtt报文，并放入接收缓冲区,如果消息长度超过缓冲区长度则抛弃此次数据
    fixedHeaderLen += MQTTPacket_encode((uint8_t *)client->config.recvBuffer + fixedHeaderLen, payloadLen);
    // 上面不判断是因为recv空间不可能小到 fixedHeaderLen 都无法写入
    // 判断recvBufferSize是否可以存的下数据
    RyanMqttCheckCode((fixedHeaderLen + payloadLen) <= client->config.recvBufferSize, RyanMqttRecvBufToShortError, rlog_d,
                      {
                          while (payloadLen > 0)
                          {
                              if (payloadLen <= client->config.recvBufferSize)
                              {
                                  RyanMqttRecvPacket(client, client->config.recvBuffer, payloadLen);
                                  payloadLen = 0;
                              }
                              else
                              {
                                  RyanMqttRecvPacket(client, client->config.recvBuffer, client->config.recvBufferSize);
                                  payloadLen -= client->config.recvBufferSize;
                              }
                          }
                      });

    // 3.读取mqtt载荷数据并放到读取缓冲区
    if (payloadLen > 0)
    {
        result = RyanMqttRecvPacket(client, client->config.recvBuffer + fixedHeaderLen, payloadLen);
        RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    }

    // uint16_t packLen = fixedHeaderLen + payloadLen;

    // 控制报文类型
    // 发送者QoS2动作 发布PUBLISH报文 -> 等待PUBREC报文 -> 发送PUBREL报文 -> 等待PUBCOMP报文
    // 接收者QoS2动作 等待PUBLISH报文 -> 发送PUBREC报文 -> 等待PUBREL报文 -> 发送PUBCOMP报文
    switch (header.bits.type)
    {
    case PUBLISH: // 接收到订阅消息
        result = RyanMqttPublishPacketHandler(client);
        break;

    case CONNACK: // 连接报文确认
        break;

    case PUBACK:  // QoS 1消息发布收到确认
    case PUBCOMP: // 发布完成
        result = RyanMqttPubackAndPubcompPacketHandler(client);
        break;

    case PUBREC: // 发布收到
        result = RyanMqttPubrecPacketHandler(client);
        break;

    case PUBREL: // 发布释放
        result = RyanMqttPubrelPacketHandler(client);
        break;

    case SUBACK: // 订阅确认
        result = RyanMqttSubackHandler(client);
        break;

    case UNSUBACK: // 取消订阅确认
        result = RyanMqttUnSubackHandler(client);
        break;

    case PINGRESP: // 心跳响应
        RyanMqttRefreshKeepaliveTime(client);
        result = RyanMqttSuccessError;
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

    // mqtt没有连接就退出
    if (RyanMqttConnectState != RyanMqttGetClientState(client))
        return;

    platformMutexLock(client->config.userData, &client->ackHandleLock);
    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        // 获取此节点的结构体
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

        // ack响应没有超时就不进行处理
        if (RyanMqttTrue == WaitFlag && 0 != platformTimerRemain(&ackHandler->timer))
            continue;

        switch (ackHandler->packetType)
        {
        // 发送qos1 / qos2消息, 服务器ack响应超时。需要重新发送它们。
        case PUBACK: // qos1 publish后没有收到puback
        case PUBREC: // qos2 publish后没有收到pubrec
        {
            RyanMqttSetPublishDup(&ackHandler->packet[0], 1); // 设置重发标志位
        }
        case PUBREL:  // qos2 收到pubrec，发送pubrel后没有收到pubcomp
        case PUBCOMP: // 理论不会出现，冗余措施
        {
            if (RyanMqttConnectState != RyanMqttGetClientState(client))
                continue;

            // 重发次数超过警告值回调
            if (ackHandler->repeatCount >= client->config.ackHandlerRepeatCountWarning)
            {
                RyanMqttEventMachine(client, RyanMqttEventAckRepeatCountWarning, (void *)ackHandler);
                continue;
            }

            // 重发数据事件回调
            RyanMqttEventMachine(client, RyanMqttEventRepeatPublishPacket, (void *)ackHandler);

            RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen); // 重新发送数据

            // 重置ack超时时间
            platformTimerCutdown(&ackHandler->timer, client->config.ackTimeout);
            ackHandler->repeatCount++;
            break;
        }

        // 订阅 / 取消订阅超时就认为失败
        case SUBACK:
        case UNSUBACK:
        {

            RyanMqttAckListRemoveToAckList(client, ackHandler);

            RyanMqttEventMachine(client, (SUBACK == ackHandler->packetType) ? RyanMqttEventSubscribedFaile : RyanMqttEventUnSubscribedFaile,
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

    RyanMqttCheck(RyanMqttConnectState != RyanMqttGetClientState(client), RyanMqttConnectAccepted, rlog_d);

    // 连接标志位
    connectData.clientID.cstring = client->config.clientId;
    connectData.username.cstring = client->config.userName;
    connectData.password.cstring = client->config.password;
    connectData.keepAliveInterval = client->config.keepaliveTimeoutS;
    connectData.cleansession = client->config.cleanSessionFlag;
    connectData.MQTTVersion = client->config.mqttVersion;

    if (RyanMqttTrue == client->lwtFlag)
    {
        connectData.willFlag = 1;
        connectData.will.qos = client->lwtOptions.qos;
        connectData.will.retained = client->lwtOptions.retain;
        connectData.will.message.lenstring.data = client->lwtOptions.payload;
        connectData.will.message.lenstring.len = client->lwtOptions.payloadLen;
        connectData.will.topicName.cstring = client->lwtOptions.topic;
    }

    // 调用底层的连接函数连接上服务器
    result = platformNetworkConnect(client->config.userData, &client->network, client->config.host, client->config.port);
    RyanMqttCheck(RyanMqttSuccessError == result, RyanMqttConnectNetWorkFail, rlog_d);

    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    // 序列化mqtt的CONNECT报文
    packetLen = MQTTSerialize_connect((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, &connectData);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config.userData, &client->sendBufLock); });

    // 发送序列化mqtt的CONNECT报文
    result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 等待报文
    // mqtt规范 服务端接收到connect报文后，服务端发送给客户端的第一个报文必须是 CONNACK
    result = RyanMqttReadPacketHandler(client, &packetType);
    RyanMqttCheck(CONNACK == packetType, RyanMqttConnectDisconnected, rlog_d);

    // 解析CONNACK报文
    result = MQTTDeserialize_connack(&sessionPresent, (uint8_t *)&connackRc, (uint8_t *)client->config.recvBuffer, client->config.recvBufferSize);
    RyanMqttCheck(1 == result, RyanMqttDeserializePacketError, rlog_d);

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

    switch (eventId)
    {
    case RyanMqttEventConnected:           // 第一次连接成功
        client->keepaliveTimeoutCount = 0; // 重置心跳超时计数器
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

            // 清除网络组件
            platformNetworkClose(client->config.userData, &client->network);

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
            platformThread_t mqttThread = {0};
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
            if (RyanMqttTrue != client->config.autoReconnectFlag) // 没有使能自动连接就休眠线程
                platformThreadStop(client->config.userData, &client->mqttThread);

            rlog_d("触发自动连接，%dms后开始连接\r\n", client->config.reconnectTimeout);
            platformDelay(client->config.reconnectTimeout);
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
