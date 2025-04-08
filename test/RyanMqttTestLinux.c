
#define RyanMqttClientId ("RyanMqttTest888") // 填写mqtt客户端id，要求唯一
#define RyanMqttHost ("127.0.0.1")           // 填写你的mqtt服务器ip
#define RyanMqttPort (1883)                  // mqtt服务器端口
#define RyanMqttUserName ("test")            // 填写你的用户名,没有填NULL
#define RyanMqttPassword ("test")            // 填写你的密码,没有填NULL

#define rlogEnable               // 是否使能日志
#define rlogColorEnable          // 是否使能日志颜色
#define rlogLevel (rlogLvlDebug) // 日志打印等级
#define rlogTag "RyanMqttTest"   // 日志tag

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>

#include "RyanMqttLog.h"
#include "RyanMqttClient.h"

#define delay(ms) usleep(ms * 1000)

#define checkMemory                                                          \
    do                                                                       \
    {                                                                        \
        int area = 0, use = 0;                                               \
        v_mcheck(&area, &use);                                               \
        if (area != 0 || use != 0)                                           \
        {                                                                    \
            rlog_e("内存泄漏");                                              \
            while (1)                                                        \
            {                                                                \
                int area = 0, use = 0;                                       \
                v_mcheck(&area, &use);                                       \
                rlog_w("|||----------->>> area = %d, size = %d", area, use); \
                delay(3000);                                                 \
            }                                                                \
        }                                                                    \
    } while (0)

static uint32_t mqttTest[10] = {0};
#define dataEventCount (0)      // 接收到数据次数统计
#define PublishedEventCount (1) // qos1和qos2发布成功的次数统计

static void printfArrStr(char *buf, uint32_t len, char *userData)
{
    rlog_raw("%s", userData);
    for (uint32_t i = 0; i < len; i++)
        rlog_raw("%x", buf[i]);

    rlog_raw("\r\n");
}

/**
 * @brief mqtt事件回调处理函数
 * 事件的详细定义可以查看枚举定义
 *
 * @param pclient
 * @param event
 * @param eventData 查看事件枚举，后面有说明eventData是什么类型
 */
static void mqttEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
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
        mqttTest[PublishedEventCount]++;
        break;
    }

    case RyanMqttEventData:
    {
        RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
        rlog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d",
               msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen);

        rlog_i("%.*s", msgData->payloadLen, msgData->payload);
        mqttTest[dataEventCount]++;
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
        RyanMqttDiscardAckHandler(client, ackHandler->packetType, ackHandler->packetId);
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
        free(client->config.sendBuffer);
        free(client->config.recvBuffer);
        if (client->config.userData)
            sem_post((sem_t *)client->config.userData);
        break;

    default:
        break;
    }
}

static int32_t RyanMqttInitSync(RyanMqttClient_t **client, RyanMqttBool_e syncFlag)
{

    char aaa[64];

    // 手动避免count的资源竞争了
    static uint32_t count = 0;
    snprintf(aaa, sizeof(aaa), "%s%d", RyanMqttClientId, count);
    count++;

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
        .recvBufferSize = 1024,
        .sendBufferSize = 1024,
        .recvBuffer = malloc(1024),
        .sendBuffer = malloc(1024),
        .mqttVersion = 4,
        .ackHandlerRepeatCountWarning = 6,
        .ackHandlerCountWarning = 20,
        .autoReconnectFlag = RyanMqttTrue,
        .cleanSessionFlag = RyanMqttTrue,
        .reconnectTimeout = 3000,
        .recvTimeout = 5000,
        .sendTimeout = 2000,
        .ackTimeout = 10000,
        .keepaliveTimeoutS = 120,
        .mqttEventHandle = mqttEventHandle,
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

    // 设置遗嘱消息
    result = RyanMqttSetLwt(*client, "pub/test", "this is will", strlen("this is will"), RyanMqttQos0, 0);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 启动mqtt客户端线程
    result = RyanMqttStart(*client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    while (RyanMqttConnectState != RyanMqttGetState(*client))
    {
        delay(100);
    }

    return 0;
}

static void RyanMqttDestorySync(RyanMqttClient_t *client)
{
    sem_t *sem = (sem_t *)client->config.userData;
    // 启动mqtt客户端线程
    RyanMqttDestroy(client);

    sem_wait(sem);
    sem_destroy(sem);
    free(sem);
    delay(3);
}

static RyanMqttError_e RyanMqttSubscribeTest(RyanMqttQos_e qos)
{

#define getArraySize(arr) (sizeof(arr) / sizeof((arr)[0]))

    RyanMqttClient_t *client;
    RyanMqttInitSync(&client, RyanMqttTrue);

    char *subscribeArr[] = {
        "testlinux/pub",
        "testlinux/pub2",
        "testlinux/pub3",
        "testlinux/pub4",
        "testlinux/pub5",
    };

    for (uint8_t i = 0; i < getArraySize(subscribeArr); i++)
        RyanMqttSubscribe(client, subscribeArr[i], qos);

    RyanMqttMsgHandler_t msgHandles[10] = {0};
    int32_t subscribeNum = 0;
    int32_t result = RyanMqttSuccessError;

    delay(100);
    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, getArraySize(msgHandles), &subscribeNum);
        if (result == RyanMqttNoRescourceError)
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", getArraySize(msgHandles));

        if (subscribeNum == getArraySize(subscribeArr))
            break;

        rlog_i("mqtt客户端已订阅的主题数: %d, 应该订阅主题数: %d", subscribeNum, getArraySize(subscribeArr));
        for (int32_t i = 0; i < subscribeNum; i++)
            rlog_i("已经订阅主题: %d, topic: %s, QOS: %d", i, msgHandles[i].topic, msgHandles[i].qos);

        if (i > 500)
            return RyanMqttFailedError;

        delay(100);
    }

    for (int32_t i = 0; i < subscribeNum; i++)
    {
        uint8_t flag = 0;
        for (uint8_t j = 0; j < getArraySize(subscribeArr); j++)
        {
            if (0 == strcmp(msgHandles[i].topic, subscribeArr[j]))
                flag = 1;
        }

        if (flag != 1)
        {
            rlog_i("主题不匹配: %d", msgHandles[i].topic);
            return RyanMqttFailedError;
        }
    }

    for (uint8_t i = 0; i < getArraySize(subscribeArr); i++)
        RyanMqttUnSubscribe(client, subscribeArr[i]);

    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, getArraySize(msgHandles), &subscribeNum);
        if (result == RyanMqttNoRescourceError)
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", getArraySize(msgHandles));

        if (0 == subscribeNum)
            break;

        if (i > 500)
            return RyanMqttFailedError;

        delay(100);
    }

    RyanMqttDestorySync(client);

    return RyanMqttSuccessError;
}

static RyanMqttError_e RyanMqttUnSubscribeTest(RyanMqttQos_e qos)
{

    int count = 2;

#define getArraySize(arr) (sizeof(arr) / sizeof((arr)[0]))

    RyanMqttClient_t *client;
    RyanMqttInitSync(&client, RyanMqttTrue);

    char *subscribeArr[] = {
        "testlinux/pub",
        "testlinux/pub2",
        "testlinux/pub3",
        "testlinux/pub4",
        "testlinux/pub5",
    };

    for (uint8_t i = 0; i < getArraySize(subscribeArr); i++)
        RyanMqttSubscribe(client, subscribeArr[i], qos);

    RyanMqttMsgHandler_t msgHandles[10] = {0};
    int32_t subscribeNum = 0;
    int32_t result = RyanMqttSuccessError;

    delay(100);
    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, getArraySize(msgHandles), &subscribeNum);
        if (result == RyanMqttNoRescourceError)
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", getArraySize(msgHandles));

        if (subscribeNum == getArraySize(subscribeArr))
            break;

        rlog_i("mqtt客户端已订阅的主题数: %d, 应该订阅主题数: %d", subscribeNum, getArraySize(subscribeArr));

        if (i > 500)
            return RyanMqttFailedError;

        delay(100);
    }

    // 取消订阅指定的数值
    for (uint8_t i = 0; i < getArraySize(subscribeArr) - count - 1; i++)
        RyanMqttUnSubscribe(client, subscribeArr[i]);

    delay(100);
    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, getArraySize(msgHandles), &subscribeNum);
        if (result == RyanMqttNoRescourceError)
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", getArraySize(msgHandles));

        if (subscribeNum == getArraySize(subscribeArr) - count)
            break;

        rlog_i("mqtt客户端已订阅的主题数: %d, 应该订阅主题数: %d", subscribeNum, getArraySize(subscribeArr) - count);

        if (i > 500)
            return RyanMqttFailedError;

        delay(100);
    }

    for (int32_t i = 0; i < subscribeNum; i++)
    {
        uint8_t flag = 0;
        for (uint8_t j = count - 1; j < getArraySize(subscribeArr); j++)
        {
            if (0 == strcmp(msgHandles[i].topic, subscribeArr[j]))
                flag = 1;
        }

        if (flag != 1)
        {
            rlog_i("主题不匹配: %d", msgHandles[i].topic);
            return RyanMqttFailedError;
        }
    }

    RyanMqttDestorySync(client);

    return RyanMqttSuccessError;
}

static int RyanMqttPublishTest(RyanMqttQos_e qos, uint32_t count, uint32_t delayms)
{
    RyanMqttClient_t *client;
    RyanMqttInitSync(&client, RyanMqttTrue);

    RyanMqttSubscribe(client, "testlinux/pub", qos);
    mqttTest[PublishedEventCount] = 0;
    mqttTest[dataEventCount] = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        RyanMqttError_e result = RyanMqttPublish(client, "testlinux/pub", "helloworld", strlen("helloworld"), qos, RyanMqttFalse);
        if (RyanMqttSuccessError != result)
        {
            rlog_e("QOS发布错误 Qos: %d, result: %d", qos, result);
            return -1;
        }

        if (delayms)
            delay(delayms);
    }

    for (uint32_t i = 0; i < 60; i++)
    {
        delay(1000);

        uint8_t result = 0;
        if (RyanMqttQos0 == qos)
        {
            if (count == mqttTest[dataEventCount])
                result = 1;
        }
        else if (mqttTest[PublishedEventCount] == count && mqttTest[PublishedEventCount] == mqttTest[dataEventCount])
            result = 1;

        if (!result)
        {
            rlog_e("QOS测试失败 Qos: %d, PublishedEventCount: %d, dataEventCount: %d", qos, mqttTest[PublishedEventCount], mqttTest[dataEventCount]);
            return -1;
        }
        else
        {
            rlog_i("QOS测试成功 Qos: %d", qos);
            break;
        }
    }

    RyanMqttUnSubscribe(client, "testlinux/pub");

    RyanMqttDestorySync(client);
    return 0;
}

static void RyanMqttConnectDestory(uint32_t count, uint32_t delayms)
{
    for (uint32_t i = 0; i < count; i++)
    {

        RyanMqttClient_t *client;

        RyanMqttInitSync(&client, i == count - 1 ? RyanMqttTrue : RyanMqttFalse);

        RyanMqttPublish(client, "testlinux/pub", "helloworld", strlen("helloworld"), RyanMqttQos0, RyanMqttFalse);

        if (delayms)
            delay(delayms);

        if (i == count - 1) // 最后一次同步释放
        {
            RyanMqttDestorySync(client);
            delay(1000);
        }
        else
            RyanMqttDestroy(client);
    }
}

static void RyanMqttReconnectTest(uint32_t count, uint32_t delayms)
{
    RyanMqttClient_t *client;
    RyanMqttInitSync(&client, RyanMqttTrue);
    for (uint32_t i = 0; i < count; i++)
    {
        RyanMqttDisconnect(client, i % 2 == 0);
        while (RyanMqttConnectState != RyanMqttGetState(client))
        {
            delay(1);
        }

        if (delayms)
            delay(delayms);
    }

    RyanMqttDestorySync(client);
}

static RyanMqttError_e RyanMqttKeepAliveTest()
{
    RyanMqttClient_t *client;
    RyanMqttError_e result = RyanMqttSuccessError;

    sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
    sem_init(sem, 0, 0);
    RyanMqttClientConfig_t mqttConfig = {
        .clientId = "dfawerwdfgaeruyfku",
        .userName = RyanMqttUserName,
        .password = RyanMqttPassword,
        .host = RyanMqttHost,
        .port = RyanMqttPort,
        .taskName = "mqttThread",
        .taskPrio = 16,
        .taskStack = 4096,
        .recvBufferSize = 1024,
        .sendBufferSize = 1024,
        .recvBuffer = malloc(1024),
        .sendBuffer = malloc(1024),
        .mqttVersion = 4,
        .ackHandlerRepeatCountWarning = 6,
        .ackHandlerCountWarning = 20,
        .autoReconnectFlag = RyanMqttTrue,
        .cleanSessionFlag = RyanMqttTrue,
        .reconnectTimeout = 3000,
        .recvTimeout = 5000,
        .sendTimeout = 2000,
        .ackTimeout = 10000,
        .keepaliveTimeoutS = 30,
        .mqttEventHandle = mqttEventHandle,
        .userData = sem};

    // 初始化mqtt客户端
    result = RyanMqttInit(&client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 注册需要的事件回调
    result = RyanMqttRegisterEventId(client, RyanMqttEventAnyId);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 设置mqtt客户端config
    result = RyanMqttSetConfig(client, &mqttConfig);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    // 启动mqtt客户端线程
    result = RyanMqttStart(client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

    while (RyanMqttConnectState != RyanMqttGetState(client))
    {
        delay(100);
    }

    // recvTimeout = 5000,每过 5000 ms检查一次心跳周期，如果超过 3 / 4 时间就会进行心跳保活
    for (uint32_t i = 0; i < 90; i++)
    {
        if (RyanMqttConnectState != RyanMqttGetState(client))
        {
            rlog_e("mqtt断连了");
            return RyanMqttFailedError;
        }

        rlog_w("心跳倒计时: %d", platformTimerRemain(&client->keepaliveTimer));
        delay(1000);
    }

    RyanMqttDestorySync(client);

    return RyanMqttSuccessError;
}

// !当测试程序出错时，并不会回收内存。交由父进程进行回收
int main()
{
    vallocInit();
    int result = 0;

    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttSubscribeTest(RyanMqttQos0), RyanMqttFailedError, rlog_e, { goto __exit; });
    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttSubscribeTest(RyanMqttQos1), RyanMqttFailedError, rlog_e, { goto __exit; });
    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttSubscribeTest(RyanMqttQos2), RyanMqttFailedError, rlog_e, { goto __exit; });

    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttUnSubscribeTest(RyanMqttQos0), RyanMqttFailedError, rlog_e, { goto __exit; });
    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttUnSubscribeTest(RyanMqttQos1), RyanMqttFailedError, rlog_e, { goto __exit; });
    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttUnSubscribeTest(RyanMqttQos2), RyanMqttFailedError, rlog_e, { goto __exit; });

    // 发布 & 订阅 qos 测试
    result = RyanMqttPublishTest(RyanMqttQos0, 1000, 0);
    if (result != 0)
        goto __exit;
    checkMemory;

    result = RyanMqttPublishTest(RyanMqttQos1, 1000, 1);
    if (result != 0)
        goto __exit;
    checkMemory;

    result = RyanMqttPublishTest(RyanMqttQos2, 1000, 1);
    if (result != 0)
        goto __exit;
    checkMemory;

    RyanMqttConnectDestory(100, 0);
    checkMemory;

    RyanMqttReconnectTest(3, 0);
    checkMemory;

    RyanMqttCheckCode(RyanMqttSuccessError == RyanMqttKeepAliveTest(), RyanMqttFailedError, rlog_e, { goto __exit; });

__exit:
    while (1)
    {
        displayMem();
        delay(10 * 1000);
    }

    return 0;
}
