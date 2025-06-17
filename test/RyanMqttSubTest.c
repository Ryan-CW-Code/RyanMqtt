#include "RyanMqttTest.h"

static RyanMqttSubscribeData_t *subscribeManyData = NULL;
static uint32_t subTestCount = 0;

static RyanMqttSubscribeData_t *topicIsSubscribeArr(char *topic)
{
    for (uint32_t i = 0; i < subTestCount; i++)
    {
        if (0 == strcmp(topic, subscribeManyData[i].topic))
            return &subscribeManyData[i];
    }

    return NULL;
}

static void RyanMqttSubEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
    RyanMqttClient_t *client = (RyanMqttClient_t *)pclient;
    switch (event)
    {

    case RyanMqttEventSubscribed:
    {
        RyanMqttMsgHandler_t *msgHandler = (RyanMqttMsgHandler_t *)eventData;
        rlog_i("mqtt订阅成功回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        RyanMqttSubscribeData_t *subscribeData = topicIsSubscribeArr(msgHandler->topic);
        if (NULL == subscribeData)
        {
            rlog_e("mqtt 订阅主题非法 topic: %s", msgHandler->topic);
            RyanMqttDestroy(client);
        }

        if (subscribeData->qos != msgHandler->qos)
        {
            rlog_e("mqtt 订阅主题降级 topic: %s, exportQos: %d, qos: %d", msgHandler->topic, subscribeData->qos, msgHandler->qos);
            RyanMqttDestroy(client);
        }

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
        RyanMqttSubscribeData_t *subscribeData = topicIsSubscribeArr(msgHandler->topic);
        if (NULL == subscribeData)
        {
            rlog_e("mqtt 订阅主题非法 topic: %s", msgHandler->topic);
        }
        if (subscribeData->qos != msgHandler->qos)
        {
            rlog_e("mqtt 取消订阅主题信息不对 topic: %s, exportQos: %d, qos: %d", msgHandler->topic, subscribeData->qos, msgHandler->qos);
            RyanMqttDestroy(client);
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

static RyanMqttError_e RyanMqttSubscribeCheckMsgHandle(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles, uint32_t count)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    uint32_t subscribeNum = 0;

    delay(100);
    for (uint32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, count, (int32_t *)&subscribeNum);
        if (RyanMqttNoRescourceError == result)
        {
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", count);
        }
        else
        {
            rlog_i("mqtt客户端已订阅的主题数: %d, 应该订阅主题数: %d", subscribeNum, count);
            // for (int32_t i = 0; i < subscribeNum; i++)
            //     rlog_i("已经订阅主题: %d, topic: %s, QOS: %d", i, msgHandles[i].topic, msgHandles[i].qos);
            uint32_t subscribeTotalCount = 0;
            RyanMqttGetSubscribeTotalCount(client, (int32_t *)&subscribeTotalCount);

            if (subscribeNum == count && subscribeTotalCount == count)
                break;
        }

        if (i > 500)
        {
            result = RyanMqttFailedError;
            goto __exit;
        }

        delay(100);
    }

    // 检查订阅主题是否正确
    for (uint32_t i = 0; i < subscribeNum; i++)
    {
        if (NULL == topicIsSubscribeArr(msgHandles[i].topic))
        {
            rlog_e("主题不匹配或者qos不对, topic: %s, qos: %d", msgHandles[i].topic, msgHandles[i].qos);
            result = RyanMqttFailedError;
            goto __exit;
        }
    }

__exit:
    return result;
}

static RyanMqttError_e RyanMqttSubscribeTest(uint32_t count)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClient_t *client;
    int32_t subscribeNum = 0;
    RyanMqttUnSubscribeData_t *unSubscribeManyData = NULL;
    RyanMqttMsgHandler_t *msgHandles = (RyanMqttMsgHandler_t *)malloc(sizeof(RyanMqttMsgHandler_t) * count);
    subTestCount = count;

    RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttTrue, 120, RyanMqttSubEventHandle);

    subscribeManyData = (RyanMqttSubscribeData_t *)malloc(sizeof(RyanMqttSubscribeData_t) * count);
    for (uint32_t i = 0; i < count; i++)
    {
        subscribeManyData[i].qos = i % 3;
        char *topic = (char *)malloc(64);
        snprintf(topic, 64, "test/subscribe/%d", i);
        subscribeManyData[i].topic = topic;
        subscribeManyData[i].topicLen = strlen(topic);
    }

    // 订阅全部主题
    RyanMqttSubscribeMany(client, count - 1, subscribeManyData);
    RyanMqttSubscribe(client, subscribeManyData[count - 1].topic, subscribeManyData[count - 1].qos);
    result = RyanMqttSubscribeCheckMsgHandle(client, msgHandles, count);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    // 测试重复订阅，不修改qos等级
    RyanMqttSubscribeMany(client, count / 2, subscribeManyData);
    result = RyanMqttSubscribeCheckMsgHandle(client, msgHandles, count);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    // 测试重复订阅并且修改qos等级
    for (uint32_t i = count; i > 0; i--)
    {
        subscribeManyData[count - i].qos = i % 3;
    }
    RyanMqttSubscribeMany(client, count, subscribeManyData);
    result = RyanMqttSubscribeCheckMsgHandle(client, msgHandles, count);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

    // 测试取消所有订阅消息
    unSubscribeManyData = malloc(sizeof(RyanMqttUnSubscribeData_t) * count);
    for (uint32_t i = 0; i < count; i++)
    {
        char *topic = (char *)malloc(64);
        snprintf(topic, 64, "test/subscribe/%d", i);
        unSubscribeManyData[i].topic = topic;
        unSubscribeManyData[i].topicLen = strlen(topic);
    }
    RyanMqttUnSubscribeMany(client, count - 1, unSubscribeManyData);
    RyanMqttUnSubscribe(client, unSubscribeManyData[count - 1].topic);

    // 重复取消订阅主题
    RyanMqttUnSubscribeMany(client, count / 2, unSubscribeManyData);

    delay(100);
    for (int32_t i = 0; i < 600; i++)
    {
        result = RyanMqttGetSubscribe(client, msgHandles, count, &subscribeNum);
        if (RyanMqttNoRescourceError == result)
        {
            rlog_w("订阅主题数超过缓冲区%d个，已截断，请修改msgHandles缓冲区", count);
        }

        if (0 == subscribeNum)
            break;

        if (i > 500)
        {
            result = RyanMqttFailedError;
            goto __exit;
        }

        delay(100);
    }

    result = checkAckList(client);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });

__exit:
    if (NULL != msgHandles)
        free(msgHandles);

    // 删除
    for (uint32_t i = 0; i < count; i++)
    {
        if (NULL != subscribeManyData)
            free(subscribeManyData[i].topic);
        if (NULL != unSubscribeManyData)
            free(unSubscribeManyData[i].topic);
    }
    if (NULL != subscribeManyData)
        free(subscribeManyData);
    if (NULL != unSubscribeManyData)
        free(unSubscribeManyData);

    rlog_i("mqtt 订阅测试，销毁mqtt客户端");
    RyanMqttDestorySync(client);
    return result;
}

RyanMqttError_e RyanMqttSubTest()
{
    RyanMqttError_e result = RyanMqttSuccessError;
    result = RyanMqttSubscribeTest(1000);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    return RyanMqttSuccessError;

__exit:
    return RyanMqttFailedError;
}