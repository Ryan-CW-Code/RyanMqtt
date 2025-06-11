#include "RyanMqttTest.h"

static RyanMqttError_e RyanMqttConnectDestory(uint32_t count, uint32_t delayms)
{
    for (uint32_t i = 0; i < count; i++)
    {

        RyanMqttClient_t *client;

        RyanMqttInitSync(&client, i == count - 1 ? RyanMqttTrue : RyanMqttFalse, NULL);

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

    return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttDestoryTest()
{
    RyanMqttError_e result = RyanMqttSuccessError;
    result = RyanMqttConnectDestory(100, 0);
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
    checkMemory;

    return RyanMqttSuccessError;

__exit:
    return RyanMqttFailedError;
}