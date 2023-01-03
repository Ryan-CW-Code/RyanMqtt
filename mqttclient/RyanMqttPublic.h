

#ifndef __mqttClientStatic__
#define __mqttClientStatic__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define DBG_ENABLE
#define DBG_SECTION_NAME RyanMqttTag
#define DBG_LEVEL LOG_LVL_WARNING
#define DBG_COLOR

#define RyanMqttMaxPacketId (0xFFFFU - 1U)  // 你允许的mqtt paketid最大值，协议标准为个非零的 16 位数
#define RyanMqttMaxPayloadLen (268435455UL) // 你允许的mqtt可变报头和有效载荷最长长度。默认值为协议标准
#define RyanMqttVersion ("0.0.1")

#define RyanMqttCheck(EX, ErrorCode) RyanMqttCheckCode(EX, ErrorCode, { NULL; })

    // 定义枚举类型
    typedef enum
    {
        RyanBit31 = 0x80000000,
        RyanBit30 = 0x40000000,
        RyanBit29 = 0x20000000,
        RyanBit28 = 0x10000000,
        RyanBit27 = 0x08000000,
        RyanBit26 = 0x04000000,
        RyanBit25 = 0x02000000,
        RyanBit24 = 0x01000000,
        RyanBit23 = 0x00800000,
        RyanBit22 = 0x00400000,
        RyanBit21 = 0x00200000,
        RyanBit20 = 0x00100000,
        RyanBit19 = 0x00080000,
        RyanBit18 = 0x00040000,
        RyanBit17 = 0x00020000,
        RyanBit16 = 0x00010000,
        RyanBit15 = 0x00008000,
        RyanBit14 = 0x00004000,
        RyanBit13 = 0x00002000,
        RyanBit12 = 0x00001000,
        RyanBit11 = 0x00000800,
        RyanBit10 = 0x00000400,
        RyanBit9 = 0x00000200,
        RyanBit8 = 0x00000100,
        RyanBit7 = 0x00000080,
        RyanBit6 = 0x00000040,
        RyanBit5 = 0x00000020,
        RyanBit4 = 0x00000010,
        RyanBit3 = 0x00000008,
        RyanBit2 = 0x00000004,
        RyanBit1 = 0x00000002,
        RyanBit0 = 0x00000001,

    } RyanBit_e;

    typedef enum
    {
        RyanFalse = 0,
        RyanTrue = 1
    } RyanBool_e;

    typedef enum
    {
        QOS0 = 0x00,
        QOS1 = 0x01,
        QOS2 = 0x02,
        subFail = 0x80
    } RyanMqttQos_e;

    typedef enum
    {
        mqttInvalidState = -1, // 无效状态
        mqttInitState = 0,     // 初始化状态
        mqttStartState,        // 开始状态
        mqttConnectState,      // 连接状态
        mqttDisconnectState,   // 断开连接状态
        mqttReconnectState,    // 重新连接状态
    } RyanMqttState_e;

    typedef enum
    {
        RyanMqttEventError = RyanBit0,                 // 保留事件
        RyanMqttEventConnected = RyanBit1,             // 连接成功 正数为RyanMqttConnectStatus_e*,负数为RyanMqttError_e*
        RyanMqttEventDisconnected = RyanBit2,          // 可能由用户触发,断开连接 正数为RyanMqttConnectStatus_e*,负数为RyanMqttError_e*
        RyanMqttEventSubscribed = RyanBit3,            // 订阅成功事件,服务端可以授予比订阅者要求的低的QoS等级。 RyanMqttMsgHandler_t*
        RyanMqttEventSubscribedFaile = RyanBit4,       // 订阅失败事件,超时 / 服务器返回订阅失败 RyanMqttMsgHandler_t*
        RyanMqttEventUnSubscribed = RyanBit5,          // 取消订阅事件 RyanMqttMsgHandler_t*
        RyanMqttEventUnSubscribedFaile = RyanBit6,     // 取消订阅失败事件，超时 RyanMqttMsgHandler_t*
        RyanMqttEventPublished = RyanBit7,             // qos1 / qos2发送成功事件。发送没有失败,只会重发或者用户手动丢弃。RyanMqttAckHandler_t*
        RyanMqttEventRepeatPublishPacket = RyanBit8,   // qos1 / qos2数据(或者ack)重发回调函数 RyanMqttAckHandler_t*
        RyanMqttEventAckRepeatCountWarning = RyanBit9, // ack重发次数超过警戒值 RyanMqttAckHandler_t*
        RyanMqttEventAckCountWarning = RyanBit10,      // ack记数值超过警戒值 uint16_t* ackHandlerCount; // 等待ack的记录个数
        RyanMqttEventAckHandlerdiscard = RyanBit11,    /*!< 用户触发,ack句柄丢弃事件,由用户手动调用RyanMqttDestroyAckHandler函数触发
                                                        * 可能是发送qos1 / qos2消息丢弃 ack丢弃，也可能是publish报文的ack丢弃 RyanMqttAckHandler_t*
                                                        */
        RyanMqttEventReconnectBefore = RyanBit12,      // 重连前事件,用户可以在此时更改connect信息 NULL
        RyanMqttEventDestoryBefore = RyanBit13,        // 用户触发，销毁客户端前回调 NULL
        RyanMqttEventData = RyanBit14,                 // 接收到订阅主题数据事件,支持通配符识别，返回的主题信息是报文主题 RyanMqttMsgData_t*
        RyanMqttEventAnyId = UINT32_MAX,
    } RyanMqttEventId_e;

    // 定义枚举类型
    typedef enum
    {
        RyanMqttParamInvalidError = -0x100, // 参数无效
        RyanMqttRecvPacketTimeOutError,     // 读取数据超时
        RyanMqttSendPacketTimeOutError,     // 发送数据超时
        RyanSocketFailedError,              // 套接字 FD 失败
        RyanMqttSocketConnectFailError,     // MQTT socket连接失败
        RyanMqttSendPacketError,            // MQTT 发送数据包错误
        RyanMqttSerializePacketError,       // 序列化报文失败
        RyanMqttDeserializePacketError,     // 解析报文失败
        RyanMqttNoRescourceError,           // 没有资源
        RyanMqttHaveRescourceError,         // 资源已存在
        RyanMqttNotConnectError,            // MQTT 没有连接
        RyanMqttConnectError,               // MQTT 已连接
        RyanMqttRecvBufToShortError,        // MQTT 缓冲区太短
        RyanMqttSendBufToShortError,        // MQTT 缓冲区太短
        RyanMqttNotEnoughMemError,          // MQTT 内存不足
        RyanMqttFailedError,                // 失败
        RyanMqttSuccessError = 0x0000       // 成功
    } RyanMqttError_e;

    typedef enum
    {
        // mqtt标准定义
        RyanMqttConnectAccepted = 0,               // 连接已被服务端接受
        RyanMqttConnectRefusedProtocolVersion = 1, // 服务端不支持客户端请求的 MQTT 协议级别
        RyanMqttConnectRefusedIdentifier = 2,      // 不合格的客户端标识符
        RyanMqttConnectRefusedServer = 3,          // 服务端不可用
        RyanMqttConnectRefusedUsernamePass = 4,    // 无效的用户名或密码
        RyanMqttConnectRefusedNotAuthorized = 5,   // 连接已拒绝，未授权

        // mqtt非标准定义
        RyanMqttConnectClientInvalid = 200, // 客户端处于无效状态
        RyanMqttConnectNetWorkFail,         // 网络错误
        RyanMqttConnectDisconnected,        // mqtt客户端断开连接
        RyanMqttKeepaliveTimeout,           // 心跳超时断开连接
        RyanMqttConnectUserDisconnected,    // 用户手动断开连接
        RyanMqttConnectTimeout              // 超时断开
    } RyanMqttConnectStatus_e;

    // 定义结构体类型

    /* extern variables-----------------------------------------------------------*/

#include "MQTTPacket.h"
#include "RyanMqttLog.h"
#include "platformNetwork.h"
#include "platformTimer.h"
#include "platformSystem.h"
#include "RyanList.h"

    extern const char *RyanStrError(RyanMqttError_e state);
#define RyanMqttCheckCode(EX, ErrorCode, code)                         \
    if (!(EX))                                                         \
    {                                                                  \
        {code};                                                        \
        LOG_D("%s:%d ErrorCode: %d, strError: %s",                     \
              __FILE__, __LINE__, ErrorCode, RyanStrError(ErrorCode)); \
        return ErrorCode;                                              \
    }

#ifdef __cplusplus
}
#endif

#endif
