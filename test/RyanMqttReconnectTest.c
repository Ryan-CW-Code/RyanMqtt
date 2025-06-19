#include "RyanMqttTest.h"

// todo 增加 RyanMqttReconnect 测试
static RyanMqttError_e reconnectTest(uint32_t count, uint32_t delayms)
{
	RyanMqttClient_t *client;
	RyanMqttInitSync(&client, RyanMqttTrue, RyanMqttTrue, 120, NULL);
	for (uint32_t i = 0; i < count; i++)
	{
		RyanMqttDisconnect(client, i % 2 == 0);

		// 测试手动连接
		// if (0 == i % 5)
		// {
		//     RyanMqttReconnect();
		// }

		while (RyanMqttConnectState != RyanMqttGetState(client))
		{
			delay(1);
		}

		if (delayms)
		{
			delay(delayms);
		}
	}

	RyanMqttDestorySync(client);
	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttReconnectTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	result = reconnectTest(3, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
