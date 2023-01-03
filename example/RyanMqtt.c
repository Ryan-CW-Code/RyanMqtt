#include "rtconfig.h"

#define RyanMqttHostName ("broker.emqx.io") // 填写你的mqtt服务器ip
#define RyanMqttPort ("1883")               // mqtt服务器端口
#define RyanMqttUserName ("")               // 为空时填写""
#define RyanMqttPassword ("")               // 为空时填写""

#define DBG_ENABLE
#define DBG_SECTION_NAME "mqttTest"
#define DBG_LEVEL LOG_LVL_DBG
#define DBG_COLOR

#ifdef PKG_USING_RYANMQTT_EXAMPLE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <board.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <rtdbg.h>

#include "RyanMqttClient.h"

#define delay(ms) rt_thread_mdelay(ms)

static RyanMqttClient_t *client = NULL;

static char mqttRecvBuffer[2048];
static char mqttSendBuffer[2048];

// 具体数值计算可以查看事件回调函数
static uint32_t mqttTest[10] = {0};
#define dataEventCount (0)      // 接收到几次数据
#define PublishedEventCount (1) // 发布成功的次数

void printfArrStr(char *buf, uint32_t len, char *userData)
{
    rt_kprintf("%s", userData);
    for (uint32_t i = 0; i < len; i++)
        rt_kprintf("%x", buf[i]);

    rt_kprintf("\r\n");
}

/**
 * @brief mqtt事件回调处理函数
 * 事件的详细定义可以查看枚举定义
 *
 * @param pclient
 * @param event
 * @param eventData
 */
void mqttEventHandle(void *pclient, RyanMqttEventId_e event, const void const *eventData)
{
    RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;

    switch (event)
    {
    case RyanMqttEventError:
        break;

    case RyanMqttEventConnected: // 不管有没有使能clearSession，都非常推荐在连接成功回调函数中订阅主题
        LOG_I("mqtt连接成功回调");
        break;

    case RyanMqttEventDisconnected:
        LOG_W("mqtt断开连接回调 %d", *(int32_t *)eventData);
        break;

    case RyanMqttEventSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        LOG_W("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        LOG_W("mqtt订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventUnSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        LOG_W("mqtt取消订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventUnSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        LOG_W("mqtt取消订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventPublished:
    {
        RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
        LOG_W("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        mqttTest[PublishedEventCount]++;
        break;
    }

    case RyanMqttEventData:
    {
        RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
        LOG_I(" RyanMqtt topic recv callback! topic: %s, packetId: %d, payload len: %d",
              msgData->topic, msgData->packetId, msgData->payloadLen);

        rt_kprintf("%.*s\r\n", msgData->payloadLen, msgData->payload);

        mqttTest[dataEventCount]++;
        break;
    }

    case RyanMqttEventRepeatPublishPacket: // qos2 / qos1重发事件回调
    {
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        LOG_W("%s:%d %s()... packetType: %d, packetId: %d, topic: %s, qos: %d",
              __FILE__, __LINE__, __FUNCTION__,
              ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);

        printfArrStr(ackHandler->packet, ackHandler->packetLen, "重发数据: ");

        break;
    }

    case RyanMqttEventReconnectBefore:
        // 如果每次connect都需要修改连接信息，这里是最好的选择。 否则需要注意资源互斥
        LOG_I("重连前事件回调");

        RyanMqttClientConfig_t mqttConfig = {
            .clientId = "RyanMqttTest", // 这里只修改了客户端名字
            .userName = RyanMqttHostName,
            .password = RyanMqttPassword,
            .host = RyanMqttHostName,
            .port = RyanMqttPort,
            .taskName = "mqttThread",
            .taskPrio = 16,
            .taskStack = 4096,
            .recvBufferSize = sizeof(mqttRecvBuffer),
            .sendBufferSize = sizeof(mqttSendBuffer),
            .recvBuffer = mqttRecvBuffer,
            .sendBuffer = mqttSendBuffer,
            .recvBufferStaticFlag = RyanTrue,
            .sendBufferStaticFlag = RyanTrue,
            .mqttVersion = 4,
            .ackHandlerRepeatCountWarning = 6,
            .ackHandlerCountWarning = 20,
            .autoReconnectFlag = RyanTrue,
            .cleanSessionFlag = 0,
            .reconnectTimeout = 3000,
            .recvTimeout = 11000,
            .sendTimeout = 2000,
            .ackTimeout = 10000,
            .keepaliveTimeoutS = 60,
            .mqttEventHandle = mqttEventHandle,
            .userData = NULL};

        RyanMqttSetConfig(client, &mqttConfig);
        break;

    case RyanMqttEventAckCountWarning: // qos2 / qos1的ack链表超过警戒值，不进行释放会一直重发，占用额外内存
    {
        // 根据实际情况清除ack, 这里等待每个ack重发次数到达警戒值后清除。
        // 在资源有限的单片机中也不应频繁发送qos2 / qos1消息
        uint16_t ackHandlerCount = *(uint16_t *)eventData;
        LOG_I("ack记数值超过警戒值回调: %d", ackHandlerCount);
        break;
    }

    case RyanMqttEventAckRepeatCountWarning: // 重发次数到达警戒值事件
    {
        // 这里选择直接丢弃该消息
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        LOG_I("ack重发次数超过警戒值回调");
        LOG_W("%s:%d %s()... packetType: %d, packetId: %d, topic: %s, qos: %d",
              __FILE__, __LINE__, __FUNCTION__,
              ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);

        RyanMqttDiscardAckHandler(client, ackHandler->packetType, ackHandler->packetId);

        break;
    }

    case RyanMqttEventAckHandlerdiscard:
    {
        RyanMqttAckHandler_t *ackHandler = (RyanMqttAckHandler_t *)eventData;
        LOG_I("ack丢弃回调: packetType: %d, packetId: %d, topic: %s, qos: %d",
              ackHandler->packetType, ackHandler->packetId, ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
        break;
    }

    case RyanMqttEventDestoryBefore:
        LOG_I("销毁mqtt客户端前回调");
        break;

    default:
        break;
    }
}

int mqttConnectFun()
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClientConfig_t mqttConfig = {
        .clientId = "RyanMqttTessdfwrt",
        .userName = RyanMqttHostName,
        .password = RyanMqttPassword,
        .host = RyanMqttHostName,
        .port = RyanMqttPort,
        .taskName = "mqttThread",
        .taskPrio = 16,
        .taskStack = 4096,
        .recvBufferSize = sizeof(mqttRecvBuffer),
        .sendBufferSize = sizeof(mqttSendBuffer),
        .recvBuffer = mqttRecvBuffer,
        .sendBuffer = mqttSendBuffer,
        .recvBufferStaticFlag = RyanTrue,
        .sendBufferStaticFlag = RyanTrue,
        .mqttVersion = 4,
        .ackHandlerRepeatCountWarning = 6,
        .ackHandlerCountWarning = 20,
        .autoReconnectFlag = RyanTrue,
        .cleanSessionFlag = 0,
        .reconnectTimeout = 3000,
        .recvTimeout = 11000,
        .sendTimeout = 2000,
        .ackTimeout = 10000,
        .keepaliveTimeoutS = 60,
        .mqttEventHandle = mqttEventHandle,
        .userData = NULL};

    // 初始化mqtt客户端
    result = RyanMqttInit(&client);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    // 注册需要的事件回调
    result = RyanMqttRegisterEventId(client, RyanMqttEventAnyId);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    // 设置mqtt客户端config
    result = RyanMqttSetConfig(client, &mqttConfig);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    // 设置遗嘱消息
    result = RyanMqttSetLwt(client, "pub/test", "this is will", strlen("this is will"), QOS0, 0);
    RyanMqttCheck(RyanMqttSuccessError == result, result);

    // 启动mqtt客户端线程
    result = RyanMqttStart(client);
    RyanMqttCheck(RyanMqttSuccessError == result, result);
    return 0;
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
    case mqttInvalidState:
        str = "无效状态";
        break;

    case mqttInitState:
        str = "初始化状态";
        break;

    case mqttStartState:
        str = "mqtt开始状态";
        break;

    case mqttConnectState:
        str = "连接状态";
        break;

    case mqttDisconnectState:
        str = "断开连接状态";
        break;

    case mqttReconnectState:
        str = "重新连接状态";
        break;

    default:
        RyanMqttCheck(NULL, RyanMqttFailedError);
        break;
    }

    LOG_I("client state: %s", str);

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

    if (mqttConnectState == RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
        return 0;
    }
    mqttConnectFun();
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
    if (mqttConnectState != RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
        return 0;
    }
    RyanMqttDisconnect(client, RyanTrue);
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
        LOG_I("请输入 topic、 qos、 payload内容、 发送条数、 间隔时间(可以为0) ");
        return 0;
    }

    if (mqttConnectState != RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
        return 0;
    }

    char *topic = argv[2];
    RyanMqttQos_e qos = atoi(argv[3]);
    char *payload = argv[4];
    uint16_t count = atoi(argv[5]);
    uint16_t delayTime = atoi(argv[6]);

    uint16_t pubCount = 0;
    LOG_I("qos: %d, count: %d, delayTime: %d, payload: %s", qos, count, delayTime, payload);

    for (uint16_t i = 0; i < count; i++)
    {
        if (RyanMqttSuccessError == RyanMqttPublish(client, topic, payload, strlen(payload), qos, 0))
            pubCount++;
        delay(delayTime);
    }

    delay(3000);
    LOG_E("pubCount: %d", pubCount);
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
        LOG_I("请输入 topic、 qos ");
        return 0;
    }

    if (mqttConnectState != RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
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
        LOG_I("请输入 取消订阅主题");
        return 0;
    }

    if (mqttConnectState != RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
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
    if (mqttConnectState != RyanMqttGetState(client))
    {
        LOG_W("mqtt客户端没有连接");
        return 0;
    }

    RyanMqttMsgHandler_t msgHandles[10] = {0};
    int32_t subscribeNum = 0;
    int32_t result = RyanMqttSuccessError;

    result = RyanMqttGetSubscribe(client, msgHandles, sizeof(msgHandles) / sizeof(msgHandles[0]), &subscribeNum);

    if (result == RyanMqttNoRescourceError)
        LOG_W("订阅主题数超过10个，已截断");
    LOG_I("mqtt客户端已订阅的主题数: %d", subscribeNum);

    for (int32_t i = 0; i < subscribeNum; i++)
        LOG_I("订阅主题: %d, topic: %s, QOS: %d", i, msgHandles[i].topic, msgHandles[i].qos);

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
        LOG_I("ack链表为空");
        return 0;
    }

    // 遍历链表
    RyanListForEachSafe(curr, next, &client->ackHandlerList)
    {
        // 获取此节点的结构体
        ackHandler = RyanListEntry(curr, RyanMqttAckHandler_t, list);

        // 发送qos1 / qos2消息服务器ack响应超时。需要重新发送它们。
        LOG_W(" type: %d, packetId is %d ", ackHandler->packetType, ackHandler->packetId);
        if (NULL != ackHandler->msgHandler)
            LOG_W("topic: %s, qos: %d", ackHandler->msgHandler->topic, ackHandler->msgHandler->qos);
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
        LOG_I("msg链表为空");
        return 0;
    }

    RyanListForEachSafe(curr, next, &client->msgHandlerList)
    {
        msgHandler = RyanListEntry(curr, RyanMqttMsgHandler_t, list);
        LOG_W("topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
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
            LOG_E("数组越界");
    }

    LOG_I("dataEventCount: %d, publishCount:%u",
          mqttTest[dataEventCount], mqttTest[PublishedEventCount]);

    return 0;
}

static const struct RyanMqttCmdDes cmdTab[] =
    {
        {"help", "打印帮助信息", MqttHelp},
        {"state", "打印mqtt客户端状态", MqttState},
        {"connect", "mqtt客户端连接服务器", MqttConnect},
        {"disc", "mqtt客户端断开连接", MqttDisconnect},
        {"reconnect", "mqtt断开连接时,重新连接mqtt服务器", MqttReconnect},
        {"destory", "mqtt销毁客户端", MqttDestroy},
        {"pub", "mqtt发布消息", Mqttpublish},
        {"sub", "mqtt订阅主题", Mqttsubscribe},
        {"unsub", "mqtt取消订阅主题", MqttUnSubscribe},
        {"listsub", "mqtt获取已订阅主题", MqttListSubscribe},
        {"listack", "打印ack链表", MqttListAck},
        {"listmsg", "打印msg链表", MqttListMsg},
        {"data", "打印测试信息，用户自定义的", Mqttdata},
};

static int MqttHelp(int argc, char *argv[])
{

    for (uint8_t i = 0; i < sizeof(cmdTab) / sizeof(cmdTab[0]); i++)
        rt_kprintf("mqtt %-16s %s\r\n", cmdTab[i].cmd, cmdTab[i].explain);

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
