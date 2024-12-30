// #define rlogEnable               // 是否使能日志
#define rlogColorEnable          // 是否使能日志颜色
#define rlogLevel (rlogLvlError) // 日志打印等级
#define rlogTag "RyanMqttClient" // 日志tag

#include "RyanMqttLog.h"
#include "MQTTPacket.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtile.h"
#include "RyanMqttThread.h"

/**
 * @brief 获取报文标识符，报文标识符不可为0
 * 都在sendbuf锁内调用
 * @param client
 * @return uint16_t
 */
static uint16_t RyanMqttGetNextPacketId(RyanMqttClient_t *client)
{
    RyanMqttAssert(NULL != client);
    client->packetId = (client->packetId >= RyanMqttMaxPacketId || client->packetId < 1) ? 1 : client->packetId + 1;
    return client->packetId;
}

static RyanMqttError_e setConfigValue(char **dest, char const *const rest)
{
    if (NULL == dest || NULL == rest)
        return RyanMqttNoRescourceError;

    if (NULL != *dest)
        platformMemoryFree(*dest);

    RyanMqttStringCopy(dest, (char *)rest, strlen(rest));
    if (NULL == *dest)
        return RyanMqttFailedError;

    return RyanMqttSuccessError;
}

/**
 * @brief mqtt初始化
 *
 * @param clientConfig
 * @param pClient mqtt客户端指针
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttInit(RyanMqttClient_t **pClient)
{

    RyanMqttClient_t *client = NULL;
    RyanMqttCheck(NULL != pClient, RyanMqttParamInvalidError, rlog_d);

    client = (RyanMqttClient_t *)platformMemoryMalloc(sizeof(RyanMqttClient_t));
    RyanMqttCheck(NULL != client, RyanMqttNotEnoughMemError, rlog_d);
    memset(client, 0, sizeof(RyanMqttClient_t));

    // 网络接口初始化
    client->network.socket = -1;

    platformMutexInit(client->config.userData, &client->sendBufLock);     // 初始化发送缓冲区互斥锁
    platformCriticalInit(client->config.userData, &client->criticalLock); // 初始化临界区

    client->packetId = 1; // 控制报文必须包含一个非零的 16 位报文标识符
    client->clientState = RyanMqttInitState;
    client->eventFlag = 0;
    client->ackHandlerCount = 0;
    client->lwtFlag = RyanMqttFalse;

    RyanListInit(&client->msgHandlerList);
    platformMutexInit(client->config.userData, &client->msgHandleLock);

    RyanListInit(&client->ackHandlerList);
    platformMutexInit(client->config.userData, &client->ackHandleLock);

    RyanListInit(&client->userAckHandlerList);
    platformMutexInit(client->config.userData, &client->userAckHandleLock);

    RyanMqttSetClientState(client, RyanMqttInitState);
    platformTimerInit(&client->keepaliveTimer);

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
 * @brief 启动mqtt客户端
 * !不要重复调用
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttStart(RyanMqttClient_t *client)
{

    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttInitState == client->clientState, RyanMqttFailedError, rlog_d);

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
 * @brief 断开mqtt服务器连接
 *
 * @param client
 * @param sendDiscFlag RyanMqttTrue表示发送断开连接报文，RyanMqttFalse表示不发送断开连接报文
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDisconnect(RyanMqttClient_t *client, RyanMqttBool_e sendDiscFlag)
{
    RyanMqttConnectStatus_e connectState = RyanMqttConnectUserDisconnected;
    RyanMqttError_e result = RyanMqttFailedError;
    int32_t packetLen = 0;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    if (RyanMqttTrue == sendDiscFlag)
    {
        platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
        // 序列化断开连接数据包并发送
        packetLen = MQTTSerialize_disconnect((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
            platformMutexUnLock(client->config.userData, &client->sendBufLock);
        });

        result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
            platformMutexUnLock(client->config.userData, &client->sendBufLock);
        });
        platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
    }

    connectState = RyanMqttConnectUserDisconnected;
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

    RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL);
    platformThreadStart(client->config.userData, &client->mqttThread);
    return RyanMqttSuccessError;
}

/**
 * @brief 订阅主题
 *
 * @param client
 * @param topic
 * @param qos 服务端可以授予比订阅者要求的低一些的QoS等级，可在订阅成功回调函数中查看服务端给定的qos等级
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSubscribe(RyanMqttClient_t *client, char *topic, RyanMqttQos_e qos)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t packetLen = 0;
    uint16_t packetId = 0;
    int requestedQoS = 0;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;
    MQTTString topicName = MQTTString_initializer;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    requestedQoS = qos;
    topicName.lenstring.data = topic;
    topicName.lenstring.len = strlen(topic);

    result = RyanMqttMsgHandlerCreate(client, topicName.lenstring.data, topicName.lenstring.len, qos, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_subscribe((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, 0, packetId, 1, &topicName, &requestedQoS);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });

    result = RyanMqttAckHandlerCreate(client, SUBACK, packetId, packetLen, client->config.sendBuffer, msgHandler, &userAckHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁

    RyanMqttAckListAddToUserAckList(client, userAckHandler);

    result = RyanMqttSendPacket(client, userAckHandler->packet, userAckHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    });

    return result;
}

/**
 * @brief 取消订阅指定主题
 *
 * @param client
 * @param topic
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttUnSubscribe(RyanMqttClient_t *client, char *topic)
{
    int32_t packetLen = 0;
    RyanMqttError_e result = RyanMqttFailedError;
    uint16_t packetId;
    RyanMqttMsgHandler_t *subMsgHandler = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;
    MQTTString topicName = MQTTString_initializer;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    topicName.lenstring.data = topic;
    topicName.lenstring.len = strlen(topic);

    // 查找当前主题是否已经订阅,没有订阅就取消发送
    result = RyanMqttMsgHandlerFind(client, topicName.lenstring.data, topicName.lenstring.len, RyanMqttFalse, &subMsgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    result = RyanMqttMsgHandlerCreate(client, topicName.lenstring.data, topicName.lenstring.len, RyanMqttQos0, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_unsubscribe((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, 0, packetId, 1, &topicName);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });

    result = RyanMqttAckHandlerCreate(client, UNSUBACK, packetId, packetLen, client->config.sendBuffer, msgHandler, &userAckHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁

    RyanMqttAckListAddToUserAckList(client, userAckHandler);

    result = RyanMqttSendPacket(client, userAckHandler->packet, userAckHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    });

    return result;
}

/**
 * @brief 客户端向服务端发送消息
 *
 * @param client
 * @param topic
 * @param payload
 * @param payloadLen
 * @param QOS
 * @param retain
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttPublish(RyanMqttClient_t *client, char *topic, char *payload, uint32_t payloadLen, RyanMqttQos_e qos, RyanMqttBool_e retain)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    int32_t packetLen = 0;
    int32_t packetId = 0;
    MQTTString topicName = MQTTString_initializer;
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *userAckHandler = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    if (payloadLen > 0 && NULL == payload) // 报文支持有效载荷长度为0
        return RyanMqttParamInvalidError;

    topicName.lenstring.data = topic;
    topicName.lenstring.len = strlen(topic);

    if (RyanMqttQos0 == qos)
    {
        platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_publish((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, 0, qos, retain, packetId,
                                          topicName, (uint8_t *)payload, payloadLen);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                          { platformMutexUnLock(client->config.userData, &client->sendBufLock); });

        result = RyanMqttSendPacket(client, client->config.sendBuffer, packetLen);
        platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁
        return result;
    }

    // qos1 / qos2需要收到预期响应ack,否则数据将被重新发送
    result = RyanMqttMsgHandlerCreate(client, topicName.lenstring.data, topicName.lenstring.len, qos, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    platformMutexLock(client->config.userData, &client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_publish((uint8_t *)client->config.sendBuffer, client->config.sendBufferSize, 0, qos, retain, packetId,
                                      topicName, (uint8_t *)payload, payloadLen);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });

    result = RyanMqttAckHandlerCreate(client, (RyanMqttQos1 == qos) ? PUBACK : PUBREC, packetId, packetLen, client->config.sendBuffer, msgHandler, &userAckHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttMsgHandlerDestory(client->config.userData, msgHandler);
        platformMutexUnLock(client->config.userData, &client->sendBufLock);
    });
    platformMutexUnLock(client->config.userData, &client->sendBufLock); // 释放互斥锁

    RyanMqttAckListAddToUserAckList(client, userAckHandler);

    result = RyanMqttSendPacket(client, userAckHandler->packet, userAckHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        RyanMqttAckListRemoveToUserAckList(client, userAckHandler);
        RyanMqttAckHandlerDestroy(client, userAckHandler);
    });

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
 * @brief 获取已订阅主题
 *
 * @param client
 * @param msgHandles 存放已订阅主题的空间
 * @param msgHandleSize  存放已订阅主题的空间大小个数
 * @param subscribeNum 函数内部返回已订阅主题的个数
 * @return RyanMqttState_e
 */
RyanMqttError_e RyanMqttGetSubscribe(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles, int32_t msgHandleSize, int32_t *subscribeNum)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(1 <= msgHandleSize, RyanMqttParamInvalidError, rlog_d);

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

/**
 * @brief 设置mqtt config 这是很危险的操作，需要考虑mqtt thread线程和用户线程的资源互斥
 *
 * 推荐在 RyanMqttStart函数前 / 非用户手动触发的事件回调函数中 / mqtt thread处于挂起状态时调用
 * mqtt thread处于阻塞状态时调用此函数也是很危险的行为
 * 要保证mqtt线程和用户线程的资源互斥
 * 如果修改参数需要重新连接才生效的，这里set不会生效。比如 keepAlive
 *
 * !项目中用户不应频繁调用此函数
 * ! 此函数如果返回RyanMqttFailedError,需要立即停止mqtt客户端相关操作.因为操作失败此函数会调用RyanMqttDestroy()销毁客户端
 *
 * @param client
 * @param clientConfig
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig)
{

    RyanMqttError_e result = RyanMqttSuccessError;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->clientId, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->host, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->taskName, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(13 < clientConfig->recvBufferSize && (RyanMqttMaxPayloadLen + 5) >= clientConfig->recvBufferSize, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(13 < clientConfig->sendBufferSize && (RyanMqttMaxPayloadLen + 5) >= clientConfig->sendBufferSize, RyanMqttParamInvalidError, rlog_d);

    result = setConfigValue(&client->config.clientId, clientConfig->clientId);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    if (NULL == clientConfig->userName)
    {
        client->config.userName = NULL;
    }
    else
    {
        result = setConfigValue(&client->config.userName, clientConfig->userName);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });
    }

    if (NULL == clientConfig->password)
    {
        client->config.password = NULL;
    }
    else
    {
        result = setConfigValue(&client->config.password, clientConfig->password);
        RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });
    }

    result = setConfigValue(&client->config.host, clientConfig->host);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config.taskName, clientConfig->taskName);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

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

    client->config.recvBufferSize = clientConfig->recvBufferSize;
    client->config.sendBufferSize = clientConfig->sendBufferSize;
    client->config.recvBuffer = clientConfig->recvBuffer;
    client->config.sendBuffer = clientConfig->sendBuffer;

    return RyanMqttSuccessError;

__exit:
    RyanMqttDestroy(client);
    return RyanMqttFailedError;
}

/**
 * @brief 设置遗嘱的配置信息
 * 此函数必须在发送connect报文前调用，因为遗嘱消息包含在connect报文中
 * 例如 RyanMqttStart前 / RyanMqttEventReconnectBefore事件中
 *
 * @param client
 * @param topicName
 * @param qos
 * @param retain
 * @param payload
 * @return RyanMqttError_e
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
    client->lwtFlag = RyanMqttTrue;
    client->lwtOptions.qos = qos;
    client->lwtOptions.retain = retain;
    client->lwtOptions.payloadLen = payloadLen;

    result = RyanMqttStringCopy(&client->lwtOptions.payload, payload, payloadLen);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    result = RyanMqttStringCopy(&client->lwtOptions.topic, topicName, strlen(topicName));
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(client->lwtOptions.payload); });

    return RyanMqttSuccessError;
}

/**
 * @brief 丢弃指定ack
 *
 * @param client
 * @param packetId
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDiscardAckHandler(RyanMqttClient_t *client, enum msgTypes packetType, uint16_t packetId)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttAckHandler_t *ackHandler = NULL;
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(CONNECT <= packetType && DISCONNECT >= packetType, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(0 < packetId, RyanMqttParamInvalidError, rlog_d);

    // 删除pubrel记录
    result = RyanMqttAckListNodeFind(client, packetType, packetId, &ackHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, RyanMqttNoRescourceError, rlog_d);

    RyanMqttEventMachine(client, RyanMqttEventAckHandlerdiscard, (void *)ackHandler); // 回调函数

    RyanMqttAckListRemoveToAckList(client, ackHandler);
    RyanMqttAckHandlerDestroy(client, ackHandler);
    return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttRegisterEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    client->eventFlag |= eventId;
    return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttCancelEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId)
{
    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    client->eventFlag &= ~eventId;
    return RyanMqttSuccessError;
}
