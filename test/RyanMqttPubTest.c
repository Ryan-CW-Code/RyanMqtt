#include "RyanMqttTest.h"

static int32_t pubTestPublishedEventCount = 0;
static int32_t pubTestDataEventCount = 0;
static char *pubStr = NULL;
static int32_t pubStrLen = 0;

static void RyanMqttPublishEventHandle(void *pclient, RyanMqttEventId_e event, const void *eventData)
{
    switch (event)
    {
    case RyanMqttEventPublished:
    {
        RyanMqttMsgHandler_t *msgHandler = ((RyanMqttAckHandler_t *)eventData)->msgHandler;
        rlog_w("qos1 / qos2发送成功事件回调 topic: %s, qos: %d", msgHandler->topic, msgHandler->qos);
        pubTestPublishedEventCount++;
        break;
    }

    case RyanMqttEventData:
    {
        RyanMqttMsgData_t *msgData = (RyanMqttMsgData_t *)eventData;
        // rlog_i("接收到mqtt消息事件回调 topic: %.*s, packetId: %d, payload len: %d, qos: %d",
        //        msgData->topicLen, msgData->topic, msgData->packetId, msgData->payloadLen, msgData->qos);

        if (0 == strncmp(msgData->payload, pubStr, pubStrLen))
            pubTestDataEventCount++;
        else
            rlog_e("pub测试收到数据不一致 %.*s", msgData->payloadLen, msgData->payload);

        break;
    }

    default:
        mqttEventBaseHandle(pclient, event, eventData);
        break;
    }
}

static RyanMqttError_e RyanMqttPublishTest(RyanMqttQos_e qos, int32_t count, uint32_t delayms)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    RyanMqttClient_t *client;
    time_t timeStampNow = 0;

    RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttPublishEventHandle);

    RyanMqttSubscribe(client, "testlinux/pub", qos);
    delay(100);

    time(&timeStampNow);

    pubStr = (char *)malloc(2048);
    memset(pubStr, 0, 2048);

    srand(timeStampNow);
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    for (int32_t i = 0; i < rand() % 250 + 1 + 100; i++)
    {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        snprintf(pubStr + 4 * i, 2048 - 4 * i, "%04d", rand());
    }
    pubStrLen = (int32_t)strlen(pubStr);

    pubTestPublishedEventCount = 0;
    pubTestDataEventCount = 0;
    for (int32_t i = 0; i < count; i++)
    {
        RyanMqttError_e pubResult = RyanMqttPublish(client, "testlinux/pub", pubStr, pubStrLen, qos, RyanMqttFalse);
        if (RyanMqttSuccessError != pubResult)
        {
            rlog_e("QOS发布错误 Qos: %d, result: %d", qos, pubResult);
            result = RyanMqttFailedError;
            goto __exit;
        }

        if (delayms)
            delay(delayms);
    }

    for (int32_t i = 0;; i++)
    {
        if (RyanMqttQos0 == qos)
        {
            if (count == pubTestDataEventCount)
                break;
        }
        else if (pubTestPublishedEventCount == count && pubTestPublishedEventCount == pubTestDataEventCount)
            break;

        if (i > 300)
        {
            rlog_e("QOS测试失败 Qos: %d, PublishedEventCount: %d, dataEventCount: %d", qos, pubTestPublishedEventCount, pubTestDataEventCount);
            result = RyanMqttFailedError;
            goto __exit;
        }

        delay(100);
    }

    RyanMqttUnSubscribe(client, "testlinux/pub");

    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == checkAckList(client), RyanMqttFailedError, rlog_e, { goto __exit; });

__exit:
    free(pubStr);
    pubStr = NULL;
    rlog_i("mqtt 发布测试，销毁mqtt客户端");
    RyanMqttDestorySync(client);
    return result;
}

RyanMqttError_e RyanMqttPubTest()
{
    RyanMqttError_e result = RyanMqttSuccessError;

    // 发布 & 订阅 qos 测试
    result = RyanMqttPublishTest(RyanMqttQos0, 1000, 0);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    result = RyanMqttPublishTest(RyanMqttQos1, 1000, 2);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    result = RyanMqttPublishTest(RyanMqttQos2, 1000, 5);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    return RyanMqttSuccessError;

__exit:
    return RyanMqttFailedError;
}