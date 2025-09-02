#include "RyanMqttTest.h"

static RyanMqttError_e RyanMqttConnectDestroy(uint32_t count, uint32_t delayms)
{
	for (uint32_t i = 0; i < count; i++)
	{
		RyanMqttClient_t *client;

		RyanMqttTestInit(&client, i == count - 1 ? RyanMqttTrue : RyanMqttFalse, RyanMqttTrue, 120, NULL, NULL);

		// 增加一些测试量
		RyanMqttSubscribe(client, "testlinux/pub3", RyanMqttQos2);
		RyanMqttSubscribe(client, "testlinux/pub2", RyanMqttQos1);
		RyanMqttSubscribe(client, "testlinux/pub1", RyanMqttQos0);
		RyanMqttPublish(client, "testlinux/pub3", "helloworld", RyanMqttStrlen("helloworld"), RyanMqttQos2,
				RyanMqttFalse);
		RyanMqttPublish(client, "testlinux/pub2", "helloworld", RyanMqttStrlen("helloworld"), RyanMqttQos1,
				RyanMqttFalse);
		RyanMqttPublish(client, "testlinux/pub1", "helloworld", RyanMqttStrlen("helloworld"), RyanMqttQos0,
				RyanMqttFalse);

		if (delayms)
		{
			delay(delayms);
		}

		RyanMqttTestDestroyClient(client);

		if (i == count - 1) // 最后一次同步释放
		{
			delay(100);
		}
	}

	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttDestroyTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	result = RyanMqttConnectDestroy(100, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
