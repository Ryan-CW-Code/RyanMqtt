#include "RyanMqttTest.h"

static RyanMqttError_e keepAliveTest()
{
    RyanMqttClient_t *client;
    RyanMqttError_e result = RyanMqttSuccessError;

    RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttTrue, 20, NULL);

    while (RyanMqttConnectState != RyanMqttGetState(client))
    {
        delay(100);
    }

    for (uint32_t i = 0; i < 90; i++)
    {
        if (RyanMqttConnectState != RyanMqttGetState(client))
        {
            rlog_e("mqtt断连了");
            return RyanMqttFailedError;
        }

        rlog_w("心跳倒计时: %d", RyanMqttTimerRemain(&client->keepaliveTimer));
        if (0 == RyanMqttTimerRemain(&client->keepaliveTimer))
        {
            result = RyanMqttFailedError;
            break;
        }
        delay(1000);
    }

    RyanMqttDestorySync(client);

    return result;
}

RyanMqttError_e RyanMqttKeepAliveTest()
{
    RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == keepAliveTest(), RyanMqttFailedError, rlog_e, { goto __exit; });

    return RyanMqttSuccessError;

__exit:
    return RyanMqttFailedError;
}