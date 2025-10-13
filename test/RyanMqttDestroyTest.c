#include "RyanMqttTest.h"

static RyanMqttError_e RyanMqttConnectDestroy(uint32_t count)
{
	RyanMqttClientConfig_t baseMqttConfig = {.clientId = RyanMqttClientId,
						 .userName = RyanMqttUserName,
						 .password = RyanMqttPassword,
						 .host = RyanMqttHost,
						 .port = RyanMqttPort,
						 .taskName = "mqttThread",
						 .taskPrio = 16,
						 .taskStack = 4096,
						 .mqttVersion = 4,
						 .ackHandlerRepeatCountWarning = 6,
						 .ackHandlerCountWarning = 60000,
						 .autoReconnectFlag = RyanMqttTrue,
						 .cleanSessionFlag = RyanMqttTrue,
						 .reconnectTimeout = 3000,
						 .recvTimeout = 2000,
						 .sendTimeout = 1800,
						 .ackTimeout = 10000,
						 .keepaliveTimeoutS = 120,
						 .mqttEventHandle = NULL,
						 .userData = NULL};

	for (uint32_t i = 0; i < count; i++)
	{
		RyanMqttClient_t *client = NULL;

		RyanMqttTestInit(&client, (i == count - 1) ? RyanMqttTrue : RyanMqttFalse, RyanMqttTrue, 120, NULL,
				 NULL);

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

		// 偶尔等待订阅主题全部订阅成功
		if (i % 7 == 0)
		{
			delay(2);
		}

		RyanMqttTestDestroyClient(client);

		// 测试只初始化不启动的销毁
		RyanMqttInit(&client);
		if (i % 2)
		{
			RyanMqttSetConfig(client, &baseMqttConfig);
		}
		RyanMqttDestroy(client);
	}

	return RyanMqttSuccessError;
}

RyanMqttError_e RyanMqttDestroyTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	result = RyanMqttConnectDestroy(100);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
