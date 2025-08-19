#ifndef __mqttClientPublic__
#define __mqttClientPublic__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

// 允许的mqtt packetId最大值，协议标准为1-65534的非零16位数
#define RyanMqttMaxPacketId   (UINT16_MAX - 1U)
// 允许的mqtt可变报头和有效载荷最长长度。默认值为协议标准
#define RyanMqttMaxPayloadLen (268435455UL)

#define RyanMqttMsgInvalidPacketId (UINT16_MAX)

// 定义枚举类型
typedef enum
{
	// RyanMqttBit31 = 0x80000000,
	RyanMqttBit30 = 0x40000000,
	RyanMqttBit29 = 0x20000000,
	RyanMqttBit28 = 0x10000000,
	RyanMqttBit27 = 0x08000000,
	RyanMqttBit26 = 0x04000000,
	RyanMqttBit25 = 0x02000000,
	RyanMqttBit24 = 0x01000000,
	RyanMqttBit23 = 0x00800000,
	RyanMqttBit22 = 0x00400000,
	RyanMqttBit21 = 0x00200000,
	RyanMqttBit20 = 0x00100000,
	RyanMqttBit19 = 0x00080000,
	RyanMqttBit18 = 0x00040000,
	RyanMqttBit17 = 0x00020000,
	RyanMqttBit16 = 0x00010000,
	RyanMqttBit15 = 0x00008000,
	RyanMqttBit14 = 0x00004000,
	RyanMqttBit13 = 0x00002000,
	RyanMqttBit12 = 0x00001000,
	RyanMqttBit11 = 0x00000800,
	RyanMqttBit10 = 0x00000400,
	RyanMqttBit9 = 0x00000200,
	RyanMqttBit8 = 0x00000100,
	RyanMqttBit7 = 0x00000080,
	RyanMqttBit6 = 0x00000040,
	RyanMqttBit5 = 0x00000020,
	RyanMqttBit4 = 0x00000010,
	RyanMqttBit3 = 0x00000008,
	RyanMqttBit2 = 0x00000004,
	RyanMqttBit1 = 0x00000002,
	RyanMqttBit0 = 0x00000001,
} RyanMqttBit_e;

typedef enum
{
	RyanMqttFalse = 0,
	RyanMqttTrue = 1
} RyanMqttBool_e;

typedef enum
{
	RyanMqttQos0 = 0x00,
	RyanMqttQos1 = 0x01,
	RyanMqttQos2 = 0x02,
	RyanMqttSubFail = 0x80
} RyanMqttQos_e;

typedef enum
{
	RyanMqttInvalidState = -1, // 无效状态
	RyanMqttInitState = 0,     // 初始化状态
	RyanMqttStartState,        // 开始状态
	RyanMqttConnectState,      // 连接状态
	RyanMqttDisconnectState,   // 断开连接状态
	RyanMqttReconnectState,    // 重新连接状态
} RyanMqttState_e;

typedef enum
{
	/**
	 * @brief 保留事件
	 * @eventData NULL
	 */
	RyanMqttEventError = RyanMqttBit0,

	/**
	 * @brief 连接成功
	 * @eventData 正数为RyanMqttConnectStatus_e*, 负数为RyanMqttError_e*
	 */
	RyanMqttEventConnected = RyanMqttBit1,

	/**
	 * @brief 可能由用户触发,断开连接
	 * @eventData 正数为RyanMqttConnectStatus_e*, 负数为RyanMqttError_e*
	 */
	RyanMqttEventDisconnected = RyanMqttBit2,

	/**
	 * @brief 订阅成功事件,服务端可以授予比订阅者要求的低的QoS等级
	 * @eventData RyanMqttMsgHandler_t*
	 */
	RyanMqttEventSubscribed = RyanMqttBit3,

	/**
	 * @brief 订阅失败事件,超时 / 服务器返回订阅失败
	 * @eventData RyanMqttMsgHandler_t*
	 */
	RyanMqttEventSubscribedFailed = RyanMqttBit4,
	// !弃用: 请使用 RyanMqttEventSubscribedFailed
	RyanMqttEventSubscribedFaile = RyanMqttEventSubscribedFailed,

	/**
	 * @brief 取消订阅事件
	 * @eventData RyanMqttMsgHandler_t*
	 */
	RyanMqttEventUnSubscribed = RyanMqttBit5,

	/**
	 * @brief 取消订阅失败事件，超时
	 * @eventData RyanMqttMsgHandler_t*
	 */
	RyanMqttEventUnSubscribedFailed = RyanMqttBit6,
	// !弃用: 请使用 RyanMqttEventUnSubscribedFailed
	RyanMqttEventUnSubscribedFaile = RyanMqttEventUnSubscribedFailed,

	/**
	 * @brief qos1 / qos2发送成功事件。发送没有失败,只会重发或者用户手动丢弃
	 * @eventData RyanMqttAckHandler_t*
	 */
	RyanMqttEventPublished = RyanMqttBit7,

	/**
	 * @brief qos1 / qos2数据(或者ack)重发回调函数
	 * @eventData RyanMqttAckHandler_t*
	 */
	RyanMqttEventRepeatPublishPacket = RyanMqttBit8,

	/**
	 * @brief ack重发次数超过警戒值
	 * @eventData RyanMqttAckHandler_t*
	 */
	RyanMqttEventAckRepeatCountWarning = RyanMqttBit9,

	/**
	 * @brief ack记数值超过警戒值
	 * @eventData uint16_t* ackHandlerCount;  等待ack的记录个数
	 */
	RyanMqttEventAckCountWarning = RyanMqttBit10,

	/**
	 * @brief 用户触发,ack句柄丢弃事件,由用户手动调用RyanMqttDestroyAckHandler函数触发
	 * 可能是发送qos1 / qos2消息丢弃、ack丢弃，也可能是publish报文的ack丢弃
	 *
	 * @eventData RyanMqttAckHandler_t*
	 */
	RyanMqttEventAckHandlerDiscard = RyanMqttBit11,
	// !弃用: 请使用 RyanMqttEventAckHandlerDiscard
	RyanMqttEventAckHandlerdiscard = RyanMqttEventAckHandlerDiscard,

	/**
	 * @brief 重连前事件,用户可以在此时更改connect信息
	 * @eventData NULL
	 */
	RyanMqttEventReconnectBefore = RyanMqttBit12,

	/**
	 * @brief 用户触发，销毁客户端前回调
	 * @eventData NULL
	 */
	RyanMqttEventDestroyBefore = RyanMqttBit13,
	// !弃用: 请使用 RyanMqttEventDestroyBefore
	RyanMqttEventDestoryBefore = RyanMqttEventDestroyBefore,

	/**
	 * @brief 接收到订阅主题数据事件,支持通配符识别，返回的主题信息是报文主题
	 * @eventData RyanMqttMsgData_t*
	 */
	RyanMqttEventData = RyanMqttBit14,

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
	RyanMqttSuccessError = 0x0000,      // 成功
	RyanMqttErrorForceInt32 = INT32_MAX // 强制编译器使用int32_t类型
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
	RyanMqttConnectClientInvalid = 200,         // 客户端处于无效状态
	RyanMqttConnectNetWorkFail,                 // 网络错误
	RyanMqttConnectDisconnected,                // mqtt客户端断开连接
	RyanMqttKeepaliveTimeout,                   // 心跳超时断开连接
	RyanMqttConnectUserDisconnected,            // 用户手动断开连接
	RyanMqttConnectTimeout,                     // 超时断开
	RyanMqttConnectFirstPackNotConnack,         // 发送connect后接受到的第一个报文不是connack
	RyanMqttConnectProtocolError,               // 多次收到connack
	RyanMqttConnectStatusForceInt32 = INT32_MAX // 强制编译器使用int32_t类型
} RyanMqttConnectStatus_e;

extern const char *RyanMqttStrError(int32_t state);
#define RyanMqttCheckCodeNoReturn(EX, ErrorCode, Ryanlevel, code)                                                      \
	if (!(EX))                                                                                                     \
	{                                                                                                              \
		Ryanlevel("ErrorCode: %d, strError: %s", ErrorCode, RyanMqttStrError(ErrorCode));                      \
		{code};                                                                                                \
	}

#define RyanMqttCheckCode(EX, ErrorCode, level, code)                                                                  \
	RyanMqttCheckCodeNoReturn(EX, ErrorCode, level, {                                                              \
		{code};                                                                                                \
		return ErrorCode;                                                                                      \
	});

#define RyanMqttCheckNoReturn(EX, ErrorCode, level) RyanMqttCheckCodeNoReturn(EX, ErrorCode, level, {})
#define RyanMqttCheck(EX, ErrorCode, level)         RyanMqttCheckCode(EX, ErrorCode, level, {})
#define RyanMqttCheckAssert(EX, ErrorCode, level)                                                                      \
	RyanMqttCheckCodeNoReturn(EX, ErrorCode, level, { RyanMqttAssert(NULL && "RyanMqttCheckAssert"); })

// 定义结构体类型

/* extern variables-----------------------------------------------------------*/

typedef struct
{
	uint32_t time;
	uint32_t timeOut;
} RyanMqttTimer_t;

extern void RyanMqttTimerInit(RyanMqttTimer_t *platformTimer);
extern void RyanMqttTimerCutdown(RyanMqttTimer_t *platformTimer, uint32_t timeout);
extern uint32_t RyanMqttTimerGetConfigTimeout(RyanMqttTimer_t *platformTimer);
extern uint32_t RyanMqttTimerRemain(RyanMqttTimer_t *platformTimer);

#ifdef __cplusplus
}
#endif

#endif
