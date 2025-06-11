#include "RyanMqttTest.h"

// static RyanMqttError_e keepAliveTest()
// {
//     RyanMqttClient_t *client;
//     RyanMqttError_e result = RyanMqttSuccessError;

//     sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
//     sem_init(sem, 0, 0);
//     RyanMqttClientConfig_t mqttConfig = {
//         .clientId = "dfawerwdfgaeruyfku",
//         .userName = RyanMqttUserName,
//         .password = RyanMqttPassword,
//         .host = RyanMqttHost,
//         .port = RyanMqttPort,
//         .taskName = "mqttThread",
//         .taskPrio = 16,
//         .taskStack = 4096,
//         .mqttVersion = 4,
//         .ackHandlerRepeatCountWarning = 6,
//         .ackHandlerCountWarning = 20,
//         .autoReconnectFlag = RyanMqttTrue,
//         .cleanSessionFlag = RyanMqttTrue,
//         .reconnectTimeout = 3000,
//         .recvTimeout = 5000,
//         .sendTimeout = 2000,
//         .ackTimeout = 10000,
//         .keepaliveTimeoutS = 30,
//         .mqttEventHandle = mqttEventHandle,
//         .userData = sem};

//     // 初始化mqtt客户端
//     result = RyanMqttInit(&client);
//     RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

//     // 注册需要的事件回调
//     result = RyanMqttRegisterEventId(client, RyanMqttEventAnyId);
//     RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

//     // 设置mqtt客户端config
//     result = RyanMqttSetConfig(client, &mqttConfig);
//     RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

//     // 启动mqtt客户端线程
//     result = RyanMqttStart(client);
//     RyanMqttCheck(RyanMqttSuccessError == result, result, rlog_e);

//     while (RyanMqttConnectState != RyanMqttGetState(client))
//     {
//         delay(100);
//     }

//     // recvTimeout = 5000,每过 5000 ms检查一次心跳周期，如果超过 3 / 4 时间就会进行心跳保活
//     for (uint32_t i = 0; i < 90; i++)
//     {
//         if (RyanMqttConnectState != RyanMqttGetState(client))
//         {
//             rlog_e("mqtt断连了");
//             return RyanMqttFailedError;
//         }

//         rlog_w("心跳倒计时: %d", platformTimerRemain(&client->keepaliveTimer));
//         delay(1000);
//     }

//     RyanMqttDestorySync(client);

//     return RyanMqttSuccessError;
// }

RyanMqttError_e RyanMqttKeepAliveTest()
{
    // RyanMqttCheckCodeNoReturn(RyanMqttSuccessError == keepAliveTest(), RyanMqttFailedError, rlog_e, { goto __exit; });

    return RyanMqttSuccessError;

// __exit:
//     return RyanMqttFailedError;
}