#include "RyanMqttTest.h"

static RyanMqttError_e keepAliveTest(void)
{
	RyanMqttClient_t *client;
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttTrue, 20, NULL);

	while (RyanMqttConnectState != RyanMqttGetState(client))
	{
		delay(100);
	}

	uint32_t keepAliveRemain = 0;
	for (uint32_t i = 0; i < 90; i++)
	{
		if (RyanMqttConnectState != RyanMqttGetState(client))
		{
			RyanMqttLog_e("mqtt断连了");
			return RyanMqttFailedError;
		}

		RyanMqttGetKeepAliveRemain(client, &keepAliveRemain);
		RyanMqttLog_w("心跳倒计时: %d", keepAliveRemain);
		RyanMqttCheckCodeNoReturn(0 != keepAliveRemain, RyanMqttFailedError, RyanMqttLog_e, { break; });
		delay(1000);
	}

	RyanMqttDestorySync(client);

	return result;
}

RyanMqttError_e RyanMqttKeepAliveTest(void)
{
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == keepAliveTest(), RyanMqttFailedError, RyanMqttLog_e,
				  { goto __exit; });

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
