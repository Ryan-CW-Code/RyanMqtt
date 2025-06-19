#include "RyanMqttTest.h"

static RyanMqttError_e RyanMqttConnectDestory(uint32_t count, uint32_t delayms)
{
	for (uint32_t i = 0; i < count; i++)
	{

		RyanMqttClient_t *client;

		RyanMqttInitSync(&client, i == count - 1 ? RyanMqttTrue : RyanMqttFalse, RyanMqttTrue, 120, NULL);

		RyanMqttSubscribe(client, "testlinux/pub1", RyanMqttQos0);
		RyanMqttSubscribe(client, "testlinux/pub2", RyanMqttQos1);
		RyanMqttSubscribe(client, "testlinux/pub3", RyanMqttQos2);
		RyanMqttPublish(client, "testlinux/pub1", "helloworld", strlen("helloworld"), RyanMqttQos0,
				RyanMqttFalse);
		RyanMqttPublish(client, "testlinux/pub2", "helloworld", strlen("helloworld"), RyanMqttQos1,
				RyanMqttFalse);
		RyanMqttPublish(client, "testlinux/pub3", "helloworld", strlen("helloworld"), RyanMqttQos2,
				RyanMqttFalse);

		if (delayms)
		{
			delay(delayms);
		}

		if (i == count - 1) // 最后一次同步释放
		{
			RyanMqttDestorySync(client);
			delay(100);
		}
		else
		{
			RyanMqttDestroy(client);
		}
	}

	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttDestoryTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	result = RyanMqttConnectDestory(100, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, rlog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
