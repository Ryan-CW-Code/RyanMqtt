#include "RyanMqttTest.h"

// todo 增加在回调函数里面调用重连函数的测试，应该会失败
static RyanMqttBool_e reconnectCheckMqttConnectState(RyanMqttClient_t *client)
{
	for (uint32_t i = 0; i < 5000; i++)
	{
		if (RyanMqttConnectState == RyanMqttGetState(client))
		{
			break;
		}

		delay(1);
	}

	if (RyanMqttConnectState == RyanMqttGetState(client))
	{
		return RyanMqttTrue;
	}

	return RyanMqttFalse;
}

static RyanMqttError_e autoReconnectTest(uint32_t count, uint32_t delayms)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client = NULL;
	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttTrue, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	for (uint32_t i = 0; i < count; i++)
	{
		// 应该失败
		result = RyanMqttReconnect(client);
		RyanMqttCheckCodeNoReturn(RyanMqttConnectError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		result = RyanMqttDisconnect(client, i % 2 == 0);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		// 应该失败
		result = RyanMqttReconnect(client);
		RyanMqttCheckCodeNoReturn(RyanMqttNoRescourceError == result, RyanMqttFailedError, RyanMqttLog_e, {
			result = RyanMqttFailedError;
			goto __exit;
		});

		RyanMqttLog_i("mqtt自动重连测试，将在 %dms 后重新连接", client->config.reconnectTimeout);

		RyanMqttCheckCodeNoReturn(RyanMqttTrue == reconnectCheckMqttConnectState(client), RyanMqttFailedError,
					  RyanMqttLog_e, {
						  result = RyanMqttFailedError;
						  goto __exit;
					  });

		if (delayms)
		{
			delay(delayms);
		}
	}

	result = RyanMqttSuccessError;

__exit:
	RyanMqttLog_i("mqtt 重连，销毁mqtt客户端");
	if (client)
	{
		RyanMqttTestDestroyClient(client);
	}
	return result;
}

static RyanMqttError_e manualReconnectTest(uint32_t count, uint32_t delayms)
{
	RyanMqttError_e result = RyanMqttSuccessError;
	RyanMqttClient_t *client = NULL;
	result = RyanMqttTestInit(&client, RyanMqttTrue, RyanMqttFalse, 120, NULL, NULL);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });
	for (uint32_t i = 0; i < count; i++)
	{
		// 应该失败
		result = RyanMqttReconnect(client);
		RyanMqttCheckCodeNoReturn(RyanMqttConnectError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		result = RyanMqttDisconnect(client, i % 2 == 0);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		// todo
		// 这里可能还没有调度mqtt线程就更新状态了,目前通过延时强制等待mqtt线程调度完成
		// 这里可以使用信号量也通知应用层，但又要增加plarform移植难度和内存占用
		delay(20);

		// 应该成功
		result = RyanMqttReconnect(client);
		RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e,
					  { goto __exit; });

		RyanMqttCheckCodeNoReturn(RyanMqttTrue == reconnectCheckMqttConnectState(client), RyanMqttFailedError,
					  RyanMqttLog_e, {
						  result = RyanMqttFailedError;
						  goto __exit;
					  });

		if (delayms)
		{
			delay(delayms);
		}
	}

	result = RyanMqttSuccessError;
__exit:
	RyanMqttLog_i("mqtt 重连，销毁mqtt客户端");
	if (client)
	{
		RyanMqttTestDestroyClient(client);
	}
	return result;
}

RyanMqttError_e RyanMqttReconnectTest(void)
{
	RyanMqttError_e result = RyanMqttSuccessError;

	result = autoReconnectTest(3, 2);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	result = manualReconnectTest(10, 0);
	RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == result, RyanMqttFailedError, RyanMqttLog_e, { goto __exit; });

	checkMemory;

	return RyanMqttSuccessError;

__exit:
	return RyanMqttFailedError;
}
