#include "RyanMqttTest.h"

static RyanMqttQos_e exportQos = RyanMqttQos0;
static char *subscribeArr[] = {
    "testlinux/pub1",
    "testlinux/pub2",
    "testlinux/pub3",
    "testlinux/pub4",
    "testlinux/pub5",
    "testlinux/pub6",
    "testlinux/pub7",
    "testlinux/pub8",
    "testlinux/pub9",
    "testlinux/pub10",
};

static RyanMqttBool_e topicIsSubscribeArr(char *topic)
{
    RyanMqttBool_e isFindflag = RyanMqttFalse;
    for (int32_t j = 0; j < getArraySize(subscribeArr); j++)
    {
        if (0 == strcmp(topic, subscribeArr[j]))
        {
            isFindflag = RyanMqttTrue;
            break;
        }
    }

    return isFindflag;
}

static void RyanMqttSubEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{

    switch (event)
    {

    case RyanMqttEventSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_i("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        if (exportQos != msgHandler->qos)
            rlog_e("mqtt 订阅主题降级 topic: %s, exportQos: %d, qos: %d", msgHandler->topic, exportQos, msgHandler->qos);

        break;
    }

    case RyanMqttEventSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_i("mqtt订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    case RyanMqttEventUnSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_i("mqtt取消订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        if (exportQos != msgHandler->qos || RyanMqttFalse == topicIsSubscribeArr(msgHandler->topic))
        {
            rlog_e("mqtt 取消订阅主题信息不对 topic: %s, exportQos: %d, qos: %d", msgHandler->topic, exportQos, msgHandler->qos);
            while (1)
            {
                delay(100);
            }
        }

        break;
    }

    case RyanMqttEventUnSubscribedFaile:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_w("mqtt取消订阅失败回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        break;
    }

    default:
        mqttEventBaseHandle(pclient, event, eventData);
        break;
    }
}

static RyanMqttError_e RyanMqttSubscribeTest(RyanMqttQos_e qos)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClient_t *client;
    RyanMqttMsgHandler_t msgHandles[20] = {0};
    int32_t subscribeNum = 0;

    exportQos = qos;
    RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttSubEventHandle);

    // 订阅5个主题
    for (uint32_t i = 0; i < getArraySize(subscribeArr); i++)
    {
        RyanMqttSubscribe(client, subscribeArr[i], qos);
    }

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
        {
            result = RyanMqttFailedError;
            goto __exit;
        }

        delay(100);
    }

    // 检查订阅主题是否正确
    for (int32_t i = 0; i < subscribeNum; i++)
    {
        RyanMqttBool_e isFindflag = topicIsSubscribeArr(msgHandles[i].topic);
        if (RyanMqttTrue != isFindflag)
        {
            rlog_e("主题不匹配或者qos不对, topic: %s, qos: %d", msgHandles[i].topic, msgHandles[i].qos);
            result = RyanMqttFailedError;
            goto __exit;
        }
    }

    // 取消所有订阅消息
    for (int32_t i = 0; i < getArraySize(subscribeArr); i++)
        RyanMqttUnSubscribe(client, subscribeArr[i]);

    delay(100);
    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, getArraySize(msgHandles), &subscribeNum);
        if (result == RyanMqttNoRescourceError)
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", getArraySize(msgHandles));

        if (0 == subscribeNum)
            break;

        if (i > 500)
        {
            result = RyanMqttFailedError;
            goto __exit;
        }

        delay(100);
    }

    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == checkAckList(client), RyanMqttFailedError, rlog_e, { goto __exit; });

__exit:
    rlog_i("mqtt 订阅测试，销毁mqtt客户端");
    RyanMqttDestorySync(client);
    return result;
}

RyanMqttError_e RyanMqttSubTest()
{
    RyanMqttError_e result = RyanMqttSuccessError;
    result = RyanMqttSubscribeTest(RyanMqttQos0);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    result = RyanMqttSubscribeTest(RyanMqttQos1);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    result = RyanMqttSubscribeTest(RyanMqttQos2);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    return RyanMqttSuccessError;

__exit:
    return RyanMqttFailedError;
}