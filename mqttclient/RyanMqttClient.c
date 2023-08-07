#define rlogEnable 1               // 是否使能日志
#define rlogColorEnable 1          // 是否使能日志颜色
#define rlogLevel (rlogLvlWarning) // 日志打印等级
#define rlogTag "RyanMqttClient"   // 日志tag

#include "RyanMqttLog.h"
#include "MQTTPacket.h"
#include "RyanMqttClient.h"
#include "RyanMqttUtile.h"
#include "RyanMqttThread.h"

/**
 * @brief 获取报文标识符，报文标识符不可为0
 *
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
    client->network = (platformNetwork_t *)platformMemoryMalloc(sizeof(platformNetwork_t));
    RyanMqttCheckCode(NULL != client->network, RyanMqttNotEnoughMemError, rlog_d, { RyanMqttDestroy(client); });
    memset(client->network, 0, sizeof(platformNetwork_t));
    client->network->socket = -1;

    client->config = (RyanMqttClientConfig_t *)platformMemoryMalloc(sizeof(RyanMqttClientConfig_t));
    RyanMqttCheckCode(NULL != client->config, RyanMqttNotEnoughMemError, rlog_d, { RyanMqttDestroy(client); });
    memset(client->config, 0, sizeof(RyanMqttClientConfig_t));

    client->mqttThread = platformMemoryMalloc(sizeof(platformThread_t));
    RyanMqttCheckCode(NULL != client->mqttThread, RyanMqttNotEnoughMemError, rlog_d, { RyanMqttDestroy(client); });
    memset(client->mqttThread, 0, sizeof(platformThread_t));

    client->sendBufLock = platformMemoryMalloc(sizeof(platformMutex_t));
    RyanMqttCheckCode(NULL != client->sendBufLock, RyanMqttNotEnoughMemError, rlog_d, { RyanMqttDestroy(client); });
    memset(client->sendBufLock, 0, sizeof(platformMutex_t));

    client->packetId = 1; // 控制报文必须包含一个非零的 16 位报文标识符
    client->clientState = 0;
    client->eventFlag = 0;
    client->keepaliveTimeoutCount = 0;
    client->ackHandlerCount = 0;
    client->lwtFlag = RyanMqttFalse;
    client->lwtOptions = NULL;

    platformMutexInit(client->config->userData, client->sendBufLock); // 初始化发送缓冲区互斥锁

    RyanListInit(&client->msgHandlerList);
    RyanListInit(&client->ackHandlerList);
    platformTimerInit(&client->keepaliveTimer);

    RyanMqttSetClientState(client, RyanMqttInitState);

    *pClient = client;
    return RyanMqttSuccessError;
}

/**
 * @brief 销毁mqtt客户端
 *
 * @param client
 * @return RyanMqttError_e
 */
RyanMqttError_e RyanMqttDestroy(RyanMqttClient_t *client)
{

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);

    RyanMqttEventMachine(client, RyanMqttEventDestoryBefore, (void *)NULL);

    // 先清除掉线程
    if (NULL != client->mqttThread)
    {
        platformThreadDestroy(client->config->userData, client->mqttThread);
        platformMemoryFree(client->mqttThread);
        client->mqttThread = NULL;
    }

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
        if (RyanMqttTrue != client->config->recvBufferStaticFlag && NULL != client->config->recvBuffer)
            platformMemoryFree(client->config->recvBuffer);

        if (RyanMqttTrue != client->config->sendBufferStaticFlag && NULL != client->config->sendBuffer)
            platformMemoryFree(client->config->sendBuffer);

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

    platformMemoryFree(client);
    client = NULL;

    return RyanMqttSuccessError;
}

/**
 * @brief 启动mqtt客户端
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
    result = platformThreadInit(client->config->userData,
                                client->mqttThread,
                                client->config->taskName,
                                RyanMqttThread,
                                client,
                                client->config->taskStack,
                                client->config->taskPrio);

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

    int32_t connectState = RyanMqttConnectAccepted;
    int32_t packetLen = 0;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    if (RyanMqttTrue == sendDiscFlag)
    {
        platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
        // 序列化断开连接数据包并发送
        packetLen = MQTTSerialize_disconnect((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize);
        if (packetLen > 0)
            RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
        platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁
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
    RyanMqttCheck(NULL != client->mqttThread, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttDisconnectState != RyanMqttGetClientState(client), RyanMqttConnectError, rlog_d);

    RyanMqttEventMachine(client, RyanMqttEventReconnectBefore, NULL);
    platformThreadStart(client->config->userData, client->mqttThread);
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
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    MQTTString topicName = MQTTString_initializer;
    topicName.cstring = (char *)topic;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_subscribe((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, 0, packetId, 1, &topicName, (int *)&qos);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    result = RyanMqttMsgHandlerCreate(topic, strlen(topic), qos, &msgHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    result = RyanMqttAckHandlerCreate(client, SUBACK, packetId, packetLen, msgHandler, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        platformMemoryFree(msgHandler);
        platformMutexUnLock(client->config->userData, client->sendBufLock);
    });
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    // 确定节点是否已存在,存在就删除
    result = RyanMqttAckListNodeFind(client, SUBACK, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
        RyanMqttAckHandlerDestroy(client, ackHandler);

    result = RyanMqttAckListAdd(client, ackHandler);

    result = RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttAckHandlerDestroy(client, ackHandler); });
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
    RyanMqttMsgHandler_t *msgHandler = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;
    MQTTString topicName = MQTTString_initializer;
    topicName.cstring = topic;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    // 查找当前主题是否已经订阅,没有订阅就取消发送
    result = RyanMqttMsgHandlerFind(client, topicName.cstring, strlen(topicName.cstring), RyanMqttFalse, &msgHandler);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_unsubscribe((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, 0, packetId, 1, &topicName);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                      { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    result = RyanMqttAckHandlerCreate(client, UNSUBACK, packetId, packetLen, msgHandler, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                      { platformMutexUnLock(client->config->userData, client->sendBufLock); });
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    // 确定节点是否已存在,存在就删除
    result = RyanMqttAckListNodeFind(client, UNSUBACK, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
        RyanMqttAckHandlerDestroy(client, ackHandler);

    result = RyanMqttAckListAdd(client, ackHandler);

    result = RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttAckHandlerDestroy(client, ackHandler); });

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
    RyanMqttAckHandler_t *ackHandler = NULL;
    topicName.cstring = (char *)topic;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != topic, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttMaxPayloadLen >= payloadLen, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttQos0 <= qos && RyanMqttQos2 >= qos, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttTrue == retain || RyanMqttFalse == retain, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(RyanMqttConnectState == RyanMqttGetClientState(client), RyanMqttNotConnectError, rlog_d);

    if (payloadLen > 0 && NULL == payload) // 报文支持有效载荷长度为0
        return RyanMqttParamInvalidError;

    if (RyanMqttQos0 == qos)
    {
        platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
        packetLen = MQTTSerialize_publish((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, 0, qos, retain, packetId,
                                          topicName, (uint8_t *)payload, payloadLen);
        RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                          { platformMutexUnLock(client->config->userData, client->sendBufLock); });

        result = RyanMqttSendPacket(client, client->config->sendBuffer, packetLen);
        platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁
        return result;
    }

    // qos1 / qos2需要收到预期响应ack,否则数据将被重新发送
    platformMutexLock(client->config->userData, client->sendBufLock); // 获取互斥锁
    packetId = RyanMqttGetNextPacketId(client);

    packetLen = MQTTSerialize_publish((uint8_t *)client->config->sendBuffer, client->config->sendBufferSize, 0, qos, retain, packetId,
                                      topicName, (uint8_t *)payload, payloadLen);
    RyanMqttCheckCode(packetLen > 0, RyanMqttSerializePacketError, rlog_d,
                      { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    result = RyanMqttMsgHandlerCreate(topic, strlen(topic), qos, &msgHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d,
                      { platformMutexUnLock(client->config->userData, client->sendBufLock); });

    result = RyanMqttAckHandlerCreate(client, (RyanMqttQos1 == qos) ? PUBACK : PUBREC, packetId, packetLen, msgHandler, &ackHandler);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, {
        platformMemoryFree(msgHandler);
        platformMutexUnLock(client->config->userData, client->sendBufLock);
    });
    platformMutexUnLock(client->config->userData, client->sendBufLock); // 释放互斥锁

    // 确定节点是否已存在,存在就删除,理论上不会存在
    result = RyanMqttAckListNodeFind(client, (RyanMqttQos1 == qos) ? PUBACK : PUBREC, packetId, &ackHandler);
    if (RyanMqttSuccessError == result)
        RyanMqttAckHandlerDestroy(client, ackHandler);

    result = RyanMqttAckListAdd(client, ackHandler);

    result = RyanMqttSendPacket(client, ackHandler->packet, ackHandler->packetLen);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { RyanMqttAckHandlerDestroy(client, ackHandler); });

    // 提前设置重发标志位
    RyanMqttSetPublishDup(&ackHandler->packet[0], 1);

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

    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;

    RyanMqttCheck(NULL != client, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != msgHandles, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(0 < msgHandleSize, RyanMqttParamInvalidError, rlog_d);

    *subscribeNum = 0;

    if (RyanListIsEmpty(&client->msgHandlerList))
        return RyanMqttSuccessError;

    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);

        msgHandles[*subscribeNum].topic = msgHandler->topic;
        msgHandles[*subscribeNum].qos = msgHandler->qos;

        (*subscribeNum)++;

        if (*subscribeNum >= msgHandleSize)
            return RyanMqttNoRescourceError;
    }

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

/**
 * @brief 设置mqtt config 这是很危险的操作，需要考虑mqtt thread线程和用户线程的资源互斥
 * 推荐在 RyanMqttStart函数前 / 非用户手动触发的事件回调函数中 / mqtt thread处于挂起状态时调用
 * mqtt thread处于阻塞状态时调用此函数也是很危险的行为，因为无法确定此函数的执行时间，调用此函数的任务运行时间片有多少
 * 总之就是要保证mqtt线程和用户线程的资源互斥
 * 项目中用户也不应该频繁调用此函数
 *
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
    RyanMqttCheck(NULL != clientConfig->port, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->userName, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->password, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(NULL != clientConfig->taskName, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(2 < clientConfig->recvBufferSize && (RyanMqttMaxPayloadLen + 5) >= clientConfig->recvBufferSize, RyanMqttParamInvalidError, rlog_d);
    RyanMqttCheck(2 < clientConfig->sendBufferSize && (RyanMqttMaxPayloadLen + 5) >= clientConfig->sendBufferSize, RyanMqttParamInvalidError, rlog_d);

    RyanMqttCheckCode(NULL != client->config, RyanMqttParamInvalidError, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->clientId, clientConfig->clientId);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->userName, clientConfig->userName);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->password, clientConfig->password);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->host, clientConfig->host);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->port, clientConfig->port);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    result = setConfigValue(&client->config->taskName, clientConfig->taskName);
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { goto __exit; });

    client->config->taskPrio = clientConfig->taskPrio;
    client->config->taskStack = clientConfig->taskStack;
    client->config->mqttVersion = clientConfig->mqttVersion;
    client->config->ackHandlerRepeatCountWarning = clientConfig->ackHandlerRepeatCountWarning;
    client->config->ackHandlerCountWarning = clientConfig->ackHandlerCountWarning;
    client->config->autoReconnectFlag = clientConfig->autoReconnectFlag;
    client->config->cleanSessionFlag = clientConfig->cleanSessionFlag;
    client->config->reconnectTimeout = clientConfig->reconnectTimeout;
    client->config->recvTimeout = clientConfig->recvTimeout;
    client->config->sendTimeout = clientConfig->sendTimeout;
    client->config->ackTimeout = clientConfig->ackTimeout;
    client->config->keepaliveTimeoutS = clientConfig->keepaliveTimeoutS;
    client->config->mqttEventHandle = clientConfig->mqttEventHandle;
    client->config->userData = clientConfig->userData;

    client->config->recvBufferSize = clientConfig->recvBufferSize;
    client->config->sendBufferSize = clientConfig->sendBufferSize;
    client->config->recvBufferStaticFlag = clientConfig->recvBufferStaticFlag;
    client->config->sendBufferStaticFlag = clientConfig->sendBufferStaticFlag;
    client->config->recvBuffer = clientConfig->recvBuffer;
    client->config->sendBuffer = clientConfig->sendBuffer;

    if (RyanMqttTrue != client->config->recvBufferStaticFlag)
    {
        client->config->recvBuffer = (char *)platformMemoryMalloc(client->config->recvBufferSize);
        RyanMqttCheckCode(NULL != client->config->recvBuffer, RyanMqttFailedError, rlog_d, { goto __exit; });
    }

    if (RyanMqttTrue != client->config->sendBufferStaticFlag)
    {
        client->config->sendBuffer = (char *)platformMemoryMalloc(client->config->sendBufferSize);
        RyanMqttCheckCode(NULL != client->config->sendBuffer, RyanMqttFailedError, rlog_d, { goto __exit; });
    }
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
    RyanMqttCheck(NULL == client->lwtOptions, RyanMqttFailedError, rlog_d);

    if (payloadLen > 0 && NULL == payload) // 报文支持有效载荷长度为0
        return RyanMqttParamInvalidError;

    client->lwtOptions = (lwtOptions_t *)platformMemoryMalloc(sizeof(lwtOptions_t) + payloadLen);
    RyanMqttCheck(NULL != client->lwtOptions, RyanMqttNotEnoughMemError, rlog_d);
    memset(client->lwtOptions, 0, sizeof(lwtOptions_t) + payloadLen);

    client->lwtFlag = RyanMqttTrue;
    client->lwtOptions->qos = qos;
    client->lwtOptions->retain = retain;
    client->lwtOptions->payloadLen = payloadLen;
    client->lwtOptions->payload = (char *)client->lwtOptions + sizeof(lwtOptions_t);
    memcpy(client->lwtOptions->payload, payload, payloadLen);

    result = RyanMqttStringCopy(&client->lwtOptions->topic, topicName, strlen(topicName));
    RyanMqttCheckCode(RyanMqttSuccessError == result, result, rlog_d, { platformMemoryFree(client->lwtOptions); });

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
