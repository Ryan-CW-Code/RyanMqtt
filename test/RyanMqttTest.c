#include "RyanMqttTest.h"

static pthread_spinlock_t spin;

/**
 * @brief mqtt事件回调处理函数
 * 事件的详细定义可以查看枚举定义
 *
 * @param pclient
 * @param event
 * @param eventData 查看事件枚举，后面有说明eventData是什么类型
 */
void mqttEventBaseHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
    RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;

    switch (event)
    {
    case RyanMqttEventError:
        break;

    case RyanMqttEventConnected: // 不管有没有使能clearSession，都非常推荐在连接成功回调函数中订阅主题
        rlog_i("mqtt连接成功回调 %d", *(int32_t *)eventData);

        break;

    case RyanMqttEventDisconnected:
        rlog_w("mqtt断开连接回调 %d", *(int32_t *)eventData);
        break;

    case RyanMqttEventSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_w("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_w("mqtt订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventUnSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_w("mqtt取消订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventUnSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_w("mqtt取消订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventPublished:
    {
        RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
        rlog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventData:
    {
        RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
        rlog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
               msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);

        rlog_i("%.*s", msgData->payloadLen, msgData->payload);
        break;
    }

    case RyanMqttEventRepeatPublishPacket: // qos2 / qos1重发事件回调
    {
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        rlog_w("发布消息进行重发了，packetType: %d, packetId: %d, topic: %s, qos: %d",
               ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);

        printfArrStr(ackHandler->packet, ackHandler->packetLen, "重发数据: ");
        break;
    }

    case RyanMqttEventReconnectBefore:
        // 如果每次connect都需要修改连接信息，这里是最好的选择。 否则需要注意资源互斥
        rlog_i("重连前事件回调");
        break;

    case RyanMqttEventAckCountWarning: // qos2 / qos1的ack链表超过警戒值，不进行释放会一直重发，占用额外内存
    {
        // 根据实际情况清除ack, 这里等待每个ack重发次数到达警戒值后清除。
        // 在资源有限的单片机中也不应频繁发送qos2 / qos1消息
        uint16_t ackHandlerCount = *(uint16_t *)eventData;
        rlog_i("ack记数值超过警戒值回调: %d", ackHandlerCount);
        break;
    }

    case RyanMqttEventAckRepeatCountWarning: // 重发次数到达警戒值事件
    {
        // 这里选择直接丢弃该消息
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        rlog_w("ack重发次数超过警戒值回调 packetType: %d, packetId: %d, topic: %s, qos: %d", ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
        RyanMqttDiscardAckHandler(client, ackHandler);
        break;
    }

    case RyanMqttEventAckHandlerdiscard:
    {
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        rlog_i("ack丢弃回调: packetType: %d, packetId: %d, topic: %s, qos: %d",
               ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
        break;
    }

    case RyanMqttEventDestoryBefore:
        rlog_i("销毁mqtt客户端前回调");
        if (client->config.userData)
            sem_post((sem_t *)client->config.userData);
        break;

    default:
        break;
    }
}

RyanMqttError_e RyanMqttInitSync(RyanMqttClient_t **client, RyanMqttBool_e syncFlag, RyanMqttEventHandle mqttEventCallback)
{
    // 手动避免count的资源竞争了
    static uint32_t count = 0;
    char aaa[64];

    pthread_spin_lock(&spin);
    count++;
    pthread_spin_unlock(&spin);

    snprintf(aaa, sizeof(aaa), "%s%d", RyanMqttClientId, count);

    sem_t *sem = NULL;
    if (syncFlag == RyanMqttTrue)
    {
        sem = (sem_t *)malloc(sizeof(sem_t));
        sem_init(sem, 0, 0);
    }

    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClientConfig_t mqttConfig = {
        .clientId = aaa,
        .userName = RyanMqttUserName,
        .password = RyanMqttPassword,
        .host = RyanMqttHost,
        .port = RyanMqttPort,
        .taskName = "mqttThread",
        .taskPrio = 16,
        .taskStack = 4096,
        .mqttVersion = 4,
        .ackHandlerRepeatCountWarning = 6,
        .ackHandlerCountWarning = 20,
        .autoReconnectFlag = RyanMqttTrue,
        .cleanSessionFlag = RyanMqttTrue,
        .reconnectTimeout = 3000,
        .recvTimeout = 3000,
        .sendTimeout = 2000,
        .ackTimeout = 10000,
        .keepaliveTimeoutS = 120,
        .mqttEventHandle = mqttEventCallback ? mqttEventCallback : mqttEventBaseHandle,
        .userData = sem};

    // 初始化mqtt客户端
    result = RyanMqttInit(client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 注册需要的事件回调
    result = RyanMqttRegisterEventId(*client, RyanMqttEventAnyId);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 设置mqtt客户端config
    result = RyanMqttSetConfig(*client, &mqttConfig);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // // 设置遗嘱消息
    // result = RyanMqttSetLwt(*client, "pub/test", "this is will", strlen("this is will"), RyanMqttQos0, 0);
    // RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 启动mqtt客户端线程
    result = RyanMqttStart(*client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    while (RyanMqttConnectState != RyanMqttGetState(*client))
    {
        delay(100);
    }

    return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttDestorySync(RyanMqttClient_t *client)
{
    sem_t *sem = (sem_t *)client->config.userData;
    // 启动mqtt客户端线程
    RyanMqttDestroy(client);

    sem_wait(sem);
    sem_destroy(sem);
    free(sem);
    delay(3);
    return RyanMqttSuccessError;
}

RyanMqttError_e checkAckList(RyanMqttClient_t *client)
{
    rlog_w("等待检查ack链表，等待 recvTime: %d", client->config.recvTimeout);
    delay(client->config.recvTimeout + 500);

    if (!RyanListIsEmpty(&client->ackHandlerList))
    {
        rlog_e("mqtt空间 ack链表不为空");
        return RyanMqttFailedError;
    }

    if (!RyanListIsEmpty(&client->userAckHandlerList))
    {
        rlog_e("用户空间 ack链表不为空");
        return RyanMqttFailedError;
    }

    if (!RyanListIsEmpty(&client->msgHandlerList))
    {
        rlog_e("mqtt空间 msg链表不为空");
        return RyanMqttFailedError;
    }

    return RyanMqttSuccessError;
}

void printfArrStr(uint8_t *buf, uint32_t len, char *userData)
{
    rlog_raw("%s", userData);
    for (uint32_t i = 0; i < len; i++)
        rlog_raw("%x", buf[i]);

    rlog_raw("\r\n");
}

// !当测试程序出错时，并不会回收内存。交由父进程进行回收
int main()
{
    RyanMqttError_e result = RyanMqttSuccessError;
    vallocInit();

    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

    result = RyanMqttSubTest();
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    result = RyanMqttPubTest();
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    result = RyanMqttDestoryTest();
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    result = RyanMqttReconnectTest();
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    result = RyanMqttKeepAliveTest();
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

__exit:
    pthread_spin_destroy(&spin);
    while (1)
    {
        displayMem();
        delay(10 * 1000);
    }

    return 0;
}
