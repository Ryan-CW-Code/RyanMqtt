#include "rtconfig.h"

#define RyanMqttClientId ("RyanMqttTessdfwrt") // 填写mqtt客户端id，要求唯一
#define RyanMqttHost ("broker.emqx.io")        // 填写你的mqtt服务器ip
#define RyanMqttPort ("1883")                  // mqtt服务器端口
#define RyanMqttUserName ("")                  // 为空时填写""
#define RyanMqttPassword ("")                  // 为空时填写""

#ifdef PKG_USING_RYANMQTT_EXAMPLE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <board.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <rtdbg.h>

#define rlogEnable 1             // 是否使能日志
#define rlogColorEnable 1        // 是否使能日志颜色
#define rlogLevel (rlogLvlDebug) // 日志打印等级
#define rlogTag "RyanMqttTest"   // 日志tag
#include "RyanMqttLog.h"
#include "RyanMqttClient.h"

#define delay(ms) rt_thread_mdelay(ms)

static RyanMqttClient_t *client = NULL;

static char mqttRecvBuffer[1024];
static char mqttSendBuffer[1024];

// 具体数值计算可以查看事件回调函数
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
static void mqttEventHandle(void *pclient, RyanMqttEventId_e event, const void const *eventData)
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
        rlog_i("接收到mqtt消息事件回调 topic: %s, packetId: %d, payload len: %d",
               msgData->topic, msgData->packetId, msgData->payloadLen);

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
        break;

    default:
        break;
    }
}

/**
 * @brief mqtt msh命令
 *
 */
struct RyanMqttCmdDes
{
    const char *cmd;
    const char *explain;
    int (*fun)(int argc, char *argv[]);
};

static int MqttHelp(int argc, char *argv[]);

/**
 * @brief 获取mqtt状态
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttState(int argc, char *argv[])
{
    char *str = NULL;
    RyanMqttState_e clientState = RyanMqttGetState(client);
    switch (clientState)
    {
    case RyanMqttInvalidState:
        str = "无效状态";
        break;

    case RyanMqttInitState:
        str = "初始化状态";
        break;

    case RyanMqttStartState:
        str = "mqtt开始状态";
        break;

    case RyanMqttConnectState:
        str = "连接状态";
        break;

    case RyanMqttDisconnectState:
        str = "断开连接状态";
        break;

    case RyanMqttReconnectState:
        str = "重新连接状态";
        break;

    default:
        RyanMqttCheck(NULL, RyanMqttFailedError, rlog_d);
        break;
    }

    rlog_i("client state: %s", str);

    return 0;
}

/**
 * @brief mqtt 连接服务器
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttConnect(int argc, char *argv[])
{
    if (RyanMqttConnectState == RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端已经连接,请不要重复连接");
        return 0;
    }

    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClientConfig_t mqttConfig = {
        .clientId = RyanMqttClientId,
        .userName = RyanMqttUserName,
        .password = RyanMqttPassword,
        .host = RyanMqttHost,
        .port = RyanMqttPort,
        .taskName = "mqttThread",
        .taskPrio = 16,
        .taskStack = 2048,
        .recvBufferSize = sizeof(mqttRecvBuffer),
        .sendBufferSize = sizeof(mqttSendBuffer),
        .recvBuffer = mqttRecvBuffer,
        .sendBuffer = mqttSendBuffer,
        .mqttVersion = 4,
        .ackHandlerRepeatCountWarning = 6,
        .ackHandlerCountWarning = 20,
        .autoReconnectFlag = RyanMqttTrue,
        .cleanSessionFlag = RyanMqttFalse,
        .reconnectTimeout = 3000,
        .recvTimeout = 5000,
        .sendTimeout = 2000,
        .ackTimeout = 10000,
        .keepaliveTimeoutS = 120,
        .mqttEventHandle = mqttEventHandle,
        .userData = NULL};

    // 初始化mqtt客户端
    result = RyanMqttInit(&client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 注册需要的事件回调
    result = RyanMqttRegisterEventId(client, RyanMqttEventAnyId);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 设置mqtt客户端config
    result = RyanMqttSetConfig(client, &mqttConfig);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 设置遗嘱消息
    result = RyanMqttSetLwt(client, "pub/test", "this is will", strlen("this is will"), RyanMqttQos0, 0);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);

    // 启动mqtt客户端线程
    result = RyanMqttStart(client);
    RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_d);
    return 0;
}

/**
 * @brief mqtt 重连函数，注意事项请看函数简介
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttReconnect(int argc, char *argv[])
{
    RyanMqttReconnect(client);
    return 0;
}

/**
 * @brief mqtt销毁客户端
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttDestroy(int argc, char *argv[])
{
    RyanMqttDestroy(client);
    client = NULL;
    return 0;
}

/**
 * @brief 断开mqtt客户端连接
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttDisconnect(int argc, char *argv[])
{
    if (RyanMqttConnectState != RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端没有连接");
        return 0;
    }
    RyanMqttDisconnect(client, RyanMqttTrue);
    return 0;
}

/**
 * @brief mqtt发布消息
 *
 * @param argc
 * @param argv
 * @return int
 */
static int Mqttpublish(int argc, char *argv[])
{
    if (argc < 7)
    {
        rlog_w("参数不完整! 请输入 topic、 qos、 payload内容、 发送条数、 间隔时间(可以为0) ");
        return 0;
    }

    if (RyanMqttConnectState != RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端没有连接");
        return 0;
    }

    char *topic = argv[2];
    RyanMqttQos_e qos = atoi(argv[3]);
    char *payload = argv[4];
    uint16_t count = atoi(argv[5]);
    uint16_t delayTime = atoi(argv[6]);

    uint16_t pubCount = 0;
    rlog_i("qos: %d, count: %d, delayTime: %d, payload: %s", qos, count, delayTime, payload);

    for (uint16_t i = 0; i < count; i++)
    {
        if (RyanMqttSuccessError == RyanMqttPublish(client, topic, payload, strlen(payload), qos, 0))
            pubCount++;
        delay(delayTime);
    }

    rlog_w("pubCount: %d", pubCount);
    return 0;
}

/**
 * @brief mqtt订阅主题，支持通配符
 *
 * @param argc
 * @param argv
 * @return int
 */
static int Mqttsubscribe(int argc, char *argv[])
{
    if (argc < 4)
    {
        rlog_w("参数不完整! 请输入 topic、 qos ");
        return 0;
    }

    if (RyanMqttConnectState != RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端没有连接");
        return 0;
    }

    RyanMqttSubscribe(client, argv[2], atoi(argv[3]));
    return 0;
}

/**
 * @brief mqtt取消订阅主题
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttUnSubscribe(int argc, char *argv[])
{
    if (argc < 3)
    {
        rlog_w("参数不完整! 请输入 取消订阅主题");
        return 0;
    }

    if (RyanMqttConnectState != RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端没有连接");
        return 0;
    }

    RyanMqttUnSubscribe(client, argv[2]);
    return 0;
}

/**
 * @brief mqtt获取已订阅主题
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttListSubscribe(int argc, char *argv[])
{
    if (RyanMqttConnectState != RyanMqttGetState(client))
    {
        rlog_w("mqtt客户端没有连接");
        return 0;
    }

    RyanMqttMsgHandler_t msgHandles[100] = {0};
    int32_t subscribeNum = 0;
    int32_t result = RyanMqttSuccessError;

    result = RyanMqttGetSubscribe(client, msgHandles, sizeof(msgHandles) / sizeof(msgHandles[0]), &subscribeNum);

    if (result == RyanMqttNoRescourceError)
        rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", sizeof(msgHandles) / sizeof(msgHandles[0]));

    rlog_i("mqtt客户端已订阅的主题数: %d", subscribeNum);

    for (int32_t i = 0; i < subscribeNum; i++)
        rlog_i("已经订阅主题: %d, topic: %s, QOS: %d", i, msgHandles[i].topic, msgHandles[i].qos);

    return 0;
}

/**
 * @brief 打印ack链表
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttListAck(int argc, char *argv[])
{
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttAckHandler_t *ackHandler = NULL;

    if (RyanListIsEmpty(&client->ackHandlerList))
    {
        rlog_w("ack链表为空，没有等待ack的消息");
        return 0;
    }

    // 遍历链表
    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        // 获取此节点的结构体
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

        // 发送qos1 / qos2消息服务器ack响应超时。需要重新发送它们。
        rlog_i(" type: %d, packetId is %d ", ackHandler->packetType, ackHandler->packetId);
        if (NULL != ackHandler->msgHandler)
            rlog_i("topic: %s, qos: %d", ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
    }
    return 0;
}

/**
 * @brief 打印msg链表
 *
 * @param argc
 * @param argv
 * @return int
 */
static int MqttListMsg(int argc, char *argv[])
{
    RyanList_t *curr = NULL,
               *next = NULL;
    RyanMqttMsgHandler_t *msgHandler = NULL;

    if (RyanListIsEmpty(&client->msgHandlerList))
    {
        rlog_w("msg链表为空，没有等待的msg消息");
        return 0;
    }

    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);
        rlog_i("topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
    }
    return 0;
}

/**
 * @brief 打印接收到的数据计数
 *
 * @param argc
 * @param argv
 * @return int
 */
static int Mqttdata(int argc, char *argv[])
{
    // mqtt data
    if (argc == 3)
    {
        uint32_t num = atoi(argv[2]);
        if (num < sizeof(mqttTest) / sizeof(mqttTest[0]) - 1)
            mqttTest[num] = 0;
        else
            rlog_e("数组越界");
    }

    rlog_i("接收到数据次数统计: %d, qos1和qos2发布成功的次数统计: %d",
           mqttTest[dataEventCount], mqttTest[PublishedEventCount]);

    return 0;
}

static const struct RyanMqttCmdDes cmdTab[] =
    {
        // {"help",        "打印帮助信息               params: null", MqttHelp},
        // {"state",       "打印mqtt客户端状态         params: null", MqttState},
        // {"connect",     "mqtt客户端连接服务器       params: null", MqttConnect},
        // {"disc",        "mqtt客户端断开连接         params: null", MqttDisconnect},
        // {"reconnect",   "mqtt断开连接时重新连接     params: null", MqttReconnect},
        // {"destory",     "mqtt销毁客户端             params: null", MqttDestroy},
        // {"pub",         "mqtt发布消息               params: topic、qos、payload内容、发送条数、间隔时间(可以为0)", Mqttpublish},
        // {"sub",         "mqtt订阅主题               params: topic、qos", Mqttsubscribe},
        // {"unsub",       "mqtt取消订阅主题           params: 取消订阅主题", MqttUnSubscribe},
        // {"listsub",     "mqtt获取已订阅主题         params: null", MqttListSubscribe},
        // {"listack",     "打印ack链表                params: null", MqttListAck},
        // {"listmsg",     "打印msg链表                params: null", MqttListMsg},
        // {"data",        "打印测试信息用户自定义的    params: null", Mqttdata},

        {"help", "打印帮助信息               params: null", MqttHelp},
        {"state", "打印mqtt客户端状态         params: null", MqttState},
        {"connect", "mqtt客户端连接服务器       params: null", MqttConnect},
        {"disc", "mqtt客户端断开连接         params: null", MqttDisconnect},
        {"reconnect", "mqtt断开连接时重新连接     params: null", MqttReconnect},
        {"destory", "mqtt销毁客户端             params: null", MqttDestroy},
        {"pub", "mqtt发布消息               params: topic、qos、payload内容、发送条数、间隔时间(可以为0)", Mqttpublish},
        {"sub", "mqtt订阅主题               params: topic、qos", Mqttsubscribe},
        {"unsub", "mqtt取消订阅主题           params: 取消订阅主题", MqttUnSubscribe},
        {"listsub", "mqtt获取已订阅主题         params: null", MqttListSubscribe},
        {"listack", "打印ack链表                params: null", MqttListAck},
        {"listmsg", "打印msg链表                params: null", MqttListMsg},
        {"data", "打印测试信息用户自定义的   params: null", Mqttdata},
};

static int MqttHelp(int argc, char *argv[])
{

    for (uint8_t i = 0; i < sizeof(cmdTab) / sizeof(cmdTab[0]); i++)
        rlog_raw("mqtt %-16s %s\r\n", cmdTab[i].cmd, cmdTab[i].explain);

    return 0;
}

static int RyanMqttMsh(int argc, char *argv[])
{
    int32_t i = 0,
            result = 0;
    const struct RyanMqttCmdDes *runCmd = NULL;

    if (argc == 1)
    {
        MqttHelp(argc, argv);
        return 0;
    }

    for (i = 0; i < sizeof(cmdTab) / sizeof(cmdTab[0]); i++)
    {
        if (rt_strcmp(cmdTab[i].cmd, argv[1]) == 0)
        {
            runCmd = &cmdTab[i];
            break;
        }
    }

    if (runCmd == NULL)
    {
        MqttHelp(argc, argv);
        return 0;
    }

    if (runCmd->fun != NULL)
        result = runCmd->fun(argc, argv);

    return result;
}

#if defined(RT_USING_MSH)
MSH_CMD_EXPORT_ALIAS(RyanMqttMsh, mqtt, RyanMqtt command);
#endif

#endif
