#include "RyanMqttTest.h"

static RyanMqttError_e keepAliveTest(void)
{
	RyanMqttClient_t *client;
	RyanMqttError_e result = RyanMqttSuccessError;

	RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 10, NULL, NULL);

	while (RyanMqttConnectState != RyanMqttGetState(client))
	{
		delay(100);
	}

	uint32_t keepAliveRemain = 0;
	uint32_t minKeepAliveRemain = 0;
	for (uint32_t i = 0; i < 60; i++)
	{
		if (RyanMqttConnectState != RyanMqttGetState(client))
		{
			result = RyanMqttFailedError;
			break;
		}

		RyanMqttGetKeepAliveRemain(client, &keepAliveRemain);
		if (keepAliveRemain < minKeepAliveRemain)
		{
			minKeepAliveRemain = keepAliveRemain;
		}

		RyanMqttLog_w("心跳倒计时: %d", keepAliveRemain);
		RyanMqttCheckCodeNoReturn(0 != keepAliveRemain, RyanMqttFailedError, RyanMqttLog_e, { break; });

		// 超时判断：如果剩余心跳时间小于 3 秒，视为超时/异常
		if (keepAliveRemain < 3000)
		{
			RyanMqttLog_e("心跳剩余时间过短: %d 秒，心跳包发送周期不对", keepAliveRemain);
			result = RyanMqttFailedError;
			break;
		}

		delay(500);
	}

	RyanMqttTestDestroyClient(client);

	if (minKeepAliveRemain > 6 * 1000)
	{
		RyanMqttLog_e("心跳剩余时间过短: %d 秒，可能频繁的发送心跳包", minKeepAliveRemain);
		result = RyanMqttFailedError;
	}

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
