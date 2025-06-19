#ifndef __RyanMqttUtile__
#define __RyanMqttUtile__

#ifdef __cplusplus
extern "C" {
#endif

#include "RyanMqttClient.h"
#include "core_mqtt_config_defaults.h"

#ifdef PKG_USING_RYANMQTT_IS_ENABLE_ASSERT
#define RyanMqttAssert(EX) platformAssert(EX)
#else
#define RyanMqttAssert(EX)
#endif

// 定义枚举类型

// 定义结构体类型
struct NetworkContext
{
	RyanMqttClient_t *client;
};
int32_t coreMqttTransportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv);

/* extern variables-----------------------------------------------------------*/

extern void RyanMqttSetClientState(RyanMqttClient_t *client, RyanMqttState_e state);
extern RyanMqttState_e RyanMqttGetClientState(RyanMqttClient_t *client);
extern RyanMqttError_e RyanMqttStringCopy(char **dest, char *rest, uint32_t strLen);
extern void RyanMqttCleanSession(RyanMqttClient_t *client);

extern RyanMqttError_e RyanMqttSendPacket(RyanMqttClient_t *client, uint8_t *buf, uint32_t length);
extern RyanMqttError_e RyanMqttRecvPacket(RyanMqttClient_t *client, uint8_t *buf, uint32_t length);

extern RyanMqttBool_e RyanMqttMsgIsMatch(RyanMqttMsgHandler_t *msgHandler, const char *topic, uint16_t topicLen,
					 RyanMqttBool_e topicMatchedFlag);
extern RyanMqttError_e RyanMqttMsgHandlerCreate(RyanMqttClient_t *client, const char *topic, uint16_t topicLen,
						uint16_t packetId, RyanMqttQos_e qos,
						RyanMqttMsgHandler_t **pMsgHandler);
extern void RyanMqttMsgHandlerDestory(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);
extern RyanMqttError_e RyanMqttMsgHandlerFind(RyanMqttClient_t *client, const char *topic, uint16_t topicLen,
					      RyanMqttBool_e topicMatchedFlag, RyanMqttMsgHandler_t **pMsgHandler);
extern void RyanMqttMsgHandlerFindByPackId(RyanMqttClient_t *client, const char *topic, uint16_t topicLen,
					   uint16_t packetId, RyanMqttBool_e isPacketIdEqual);
extern RyanMqttError_e RyanMqttMsgHandlerAddToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);
extern RyanMqttError_e RyanMqttMsgHandlerRemoveToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);

extern RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
						uint16_t packetLen, uint8_t *packet, RyanMqttMsgHandler_t *msgHandler,
						RyanMqttAckHandler_t **pAckHandler,
						RyanMqttBool_e isPreallocatedPacket);
extern void RyanMqttAckHandlerDestroy(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListNodeFind(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
					       RyanMqttAckHandler_t **pAckHandler);
extern RyanMqttError_e RyanMqttAckListAddToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListRemoveToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListNodeFindByUserAckList(RyanMqttClient_t *client, uint8_t packetType,
							    uint16_t packetId, RyanMqttAckHandler_t **pAckHandler);
extern RyanMqttError_e RyanMqttAckListAddToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListRemoveToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);

#ifdef __cplusplus
}
#endif

#endif
