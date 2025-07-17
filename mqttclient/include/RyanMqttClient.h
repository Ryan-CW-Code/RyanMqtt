#ifndef __RyanMqttClient__
#define __RyanMqttClient__

#ifdef __cplusplus
extern "C" {
#endif

#include "platformSystem.h"
#include "platformNetwork.h"
#include "RyanMqttLog.h"
#include "RyanList.h"
#include "RyanMqttPublic.h"

// 接收到订阅消息回调函数类型，eventData用户不要进行修改否则mqtt客户端可能崩溃
typedef void (*RyanMqttEventHandle)(void *client, RyanMqttEventId_e event, const void *eventData);

// 定义枚举类型

// 定义结构体类型
typedef struct
{
	RyanMqttBool_e retained: 1; // retained 标志位
	RyanMqttBool_e dup: 1;      // 重发标志
	uint16_t packetId;          // packetId 系统生成
	RyanMqttQos_e qos;          // QOS等级
	uint32_t payloadLen;        // 数据长度
	uint32_t topicLen;          // topic长度
	char *topic;                // 主题信息
	char *payload;              // 数据内容
} RyanMqttMsgData_t;

typedef struct
{
	uint16_t packetId; // 关联的packetId
	uint16_t topicLen; // 主题长度
	RyanMqttQos_e qos; // qos等级
	RyanList_t list;   // 链表节点，用户勿动
	char *topic;       // 主题
} RyanMqttMsgHandler_t;

typedef struct
{
	uint8_t packetType;                  // 期望接收到的ack报文类型
	uint16_t repeatCount;                // 当前ack超时重发次数
	uint16_t packetId;                   // 报文标识符 系统生成，用户勿动
	RyanMqttBool_e isPreallocatedPacket; // 是否是预分配的内存
	uint32_t packetLen;                  // 报文长度
	RyanList_t list;                     // 链表节点，用户勿动
	RyanMqttTimer_t timer;               // ack超时定时器，用户勿动
	RyanMqttMsgHandler_t *msgHandler;    // msg信息
	uint8_t *packet;                     // 没有收到期望ack，重新发送的原始报文
} RyanMqttAckHandler_t;

typedef struct
{
	RyanMqttBool_e lwtFlag; // 遗嘱标志位
	uint8_t retain;         // 遗嘱保留标志位
	RyanMqttQos_e qos;      // 遗嘱qos等级
	uint32_t payloadLen;    // 消息长度
	char *topic;            // 遗嘱主题
	char *payload;          // 遗嘱消息
} lwtOptions_t;

typedef struct
{
	char *topic;
	uint16_t topicLen;
	RyanMqttQos_e qos;
} RyanMqttSubscribeData_t;

typedef struct
{
	char *topic;
	uint16_t topicLen;
} RyanMqttUnSubscribeData_t;

typedef struct
{
	char *clientId;                   // 客户端ID
	char *userName;                   // 用户名
	char *password;                   // 密码
	char *host;                       // mqtt服务器地址
	char *taskName;                   // 线程名字
	RyanMqttBool_e autoReconnectFlag; // 自动重连标志位
	RyanMqttBool_e cleanSessionFlag;  // 清除会话标志位
	uint8_t mqttVersion;              // mqtt版本 3.1.1是4, 3.1是3
	uint16_t port;                    // mqtt服务器端口

	// ack重发超过这个数值后触发事件回调,根据实际硬件选择。典型值为 *
	// ackTimeout ~= 300秒
	uint16_t ackHandlerRepeatCountWarning;
	uint16_t taskPrio;  // mqtt线程优先级
	uint16_t taskStack; // 线程栈大小

	// mqtt等待接收命令超时时间, 根据实际硬件选择。推荐 > ackTimeout && <= (keepaliveTimeoutS / 2)
	uint16_t recvTimeout;
	uint16_t sendTimeout;       // mqtt发送给命令超时时间, 根据实际硬件选择。
	uint16_t ackTimeout;        // mqtack等待命令超时时间, 典型值为5 - 30
	uint16_t keepaliveTimeoutS; // mqtt心跳时间间隔秒

	// 等待ack的警告数.每次添加ack，ack总数大于或等于该值将触发事件回调,根据实际硬件选择。典型值是32
	uint16_t ackHandlerCountWarning;

	uint16_t reconnectTimeout;           // mqtt重连间隔时间
	RyanMqttEventHandle mqttEventHandle; // mqtt事件回调函数
	void *userData;                      // 用户自定义数据,用户需要保证指针指向内容的持久性
} RyanMqttClientConfig_t;

typedef struct
{
	RyanMqttBool_e destroyFlag;  // 销毁标志位
	uint16_t ackHandlerCount;    // 等待ack的记录个数
	uint16_t packetId;           // mqtt报文标识符,控制报文必须包含一个非零的 16 位报文标识符
	uint32_t eventFlag;          // 事件标志位
	RyanMqttState_e clientState; // mqtt客户端的状态

	// 维护消息处理列表，这是mqtt协议必须实现的内容，所有来自服务器的publish报文都会被处理（前提是订阅了对应的消息，或者设置了拦截器）
	RyanList_t msgHandlerList;
	RyanList_t ackHandlerList;              // 维护ack链表
	RyanList_t userAckHandlerList;          // 用户接口的ack链表,会由mqtt线程移动到ack链表
	RyanMqttTimer_t ackScanThrottleTimer;   // ack链表检查节流定时器
	RyanMqttTimer_t keepaliveTimer;         // 保活定时器
	RyanMqttTimer_t keepaliveThrottleTimer; // 保活检查节流定时器
	platformNetwork_t network;              // 网络组件
	RyanMqttClientConfig_t config;          // mqtt config
	platformThread_t mqttThread;            // mqtt线程
	platformMutex_t sendLock;               // 写缓冲区锁
	platformMutex_t msgHandleLock;          // msg链表锁
	platformMutex_t ackHandleLock;          // ack链表锁
	platformMutex_t userSessionLock;        // 用户接口的锁
	platformCritical_t criticalLock;        // 临界区锁
	lwtOptions_t *lwtOptions;               // 遗嘱相关配置
} RyanMqttClient_t;

/* extern variables-----------------------------------------------------------*/

extern RyanMqttError_e RyanMqttInit(RyanMqttClient_t **pClient);
extern RyanMqttError_e RyanMqttDestroy(RyanMqttClient_t *client);
extern RyanMqttError_e RyanMqttStart(RyanMqttClient_t *client);
extern RyanMqttError_e RyanMqttDisconnect(RyanMqttClient_t *client, RyanMqttBool_e sendDiscFlag);
extern RyanMqttError_e RyanMqttReconnect(RyanMqttClient_t *client);

extern RyanMqttError_e RyanMqttPublish(RyanMqttClient_t *client, char *topic, char *payload, uint32_t payloadLen,
				       RyanMqttQos_e qos, RyanMqttBool_e retain);

extern RyanMqttError_e RyanMqttSubscribeMany(RyanMqttClient_t *client, int32_t count,
					     RyanMqttSubscribeData_t subscribeManyData[]);
extern RyanMqttError_e RyanMqttSubscribe(RyanMqttClient_t *client, char *topic, RyanMqttQos_e qos);
extern RyanMqttError_e RyanMqttUnSubscribeMany(RyanMqttClient_t *client, int32_t count,
					       RyanMqttUnSubscribeData_t unSubscribeManyData[]);
extern RyanMqttError_e RyanMqttUnSubscribe(RyanMqttClient_t *client, char *topic);

extern RyanMqttState_e RyanMqttGetState(RyanMqttClient_t *client);
extern RyanMqttError_e RyanMqttGetKeepAliveRemain(RyanMqttClient_t *client, uint32_t *keepAliveRemain);
extern RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig);
extern RyanMqttError_e RyanMqttSetLwt(RyanMqttClient_t *client, char *topicName, char *payload, uint32_t payloadLen,
				      RyanMqttQos_e qos, RyanMqttBool_e retain);

extern RyanMqttError_e RyanMqttGetSubscribeTotalCount(RyanMqttClient_t *client, int32_t *subscribeTotalCount);
// !此函数是非线程安全的，已不推荐. 请使用 RyanMqttGetSubscribeSafe 代替
extern RyanMqttError_e RyanMqttGetSubscribe(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandles,
					    int32_t msgHandleSize, int32_t *subscribeNum);
extern RyanMqttError_e RyanMqttSafeFreeSubscribeResources(RyanMqttMsgHandler_t *msgHandles, int32_t subscribeNum);
extern RyanMqttError_e RyanMqttGetSubscribeSafe(RyanMqttClient_t *client, RyanMqttMsgHandler_t **msgHandles,
						int32_t *subscribeNum);

extern RyanMqttError_e RyanMqttDiscardAckHandler(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId);
extern RyanMqttError_e RyanMqttRegisterEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId);
extern RyanMqttError_e RyanMqttCancelEventId(RyanMqttClient_t *client, RyanMqttEventId_e eventId);

#ifdef __cplusplus
}
#endif

#endif
