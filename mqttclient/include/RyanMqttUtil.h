#ifndef __RyanMqttUtil__
#define __RyanMqttUtil__

#ifdef __cplusplus
extern "C" {
#endif

#include "RyanMqttClient.h"
#include "core_mqtt_config_defaults.h"

#ifdef PKG_USING_RYANMQTT_IS_ENABLE_ASSERT
#define RyanMqttAssert(EX) platformAssert(EX)
#else
#define RyanMqttAssert(EX) (void)(EX)
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
extern RyanMqttError_e RyanMqttStringCopy(char **dest, const char *rest, uint32_t strLen);
extern void RyanMqttPurgeSession(RyanMqttClient_t *client);
extern void RyanMqttPurgeConfig(RyanMqttClientConfig_t *clientConfig);

extern RyanMqttError_e RyanMqttSendPacket(RyanMqttClient_t *client, uint8_t *buf, uint32_t length);
extern RyanMqttError_e RyanMqttRecvPacket(RyanMqttClient_t *client, uint8_t *buf, uint32_t length);

// msg
extern RyanMqttError_e RyanMqttMsgHandlerCreate(RyanMqttClient_t *client, const char *topic, uint16_t topicLen,
						uint16_t packetId, RyanMqttQos_e qos, void *userData,
						RyanMqttMsgHandler_t **pMsgHandler);
extern void RyanMqttMsgHandlerDestroy(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);
extern RyanMqttError_e RyanMqttMsgHandlerFind(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgMatchCriteria,
					      RyanMqttBool_e isTopicMatchedFlag, RyanMqttMsgHandler_t **pMsgHandler);
extern void RyanMqttMsgHandlerFindAndDestroyByPackId(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgMatchCriteria,
						     RyanMqttBool_e skipSamePacketId);
extern RyanMqttError_e RyanMqttMsgHandlerAddToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);
extern RyanMqttError_e RyanMqttMsgHandlerRemoveToMsgList(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);

// ack
extern RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
						uint16_t packetLen, uint8_t *packet, RyanMqttMsgHandler_t *msgHandler,
						RyanMqttAckHandler_t **pAckHandler,
						RyanMqttBool_e packetAllocatedExternally);
extern void RyanMqttAckHandlerDestroy(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListNodeFind(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId,
					       RyanMqttAckHandler_t **pAckHandler, RyanMqttBool_e removeOnMatch);
extern RyanMqttError_e RyanMqttAckListAddToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListRemoveToAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListNodeFindByUserAckList(RyanMqttClient_t *client, uint8_t packetType,
							    uint16_t packetId, RyanMqttAckHandler_t **pAckHandler,
							    RyanMqttBool_e removeOnMatch);
extern RyanMqttError_e RyanMqttAckListAddToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern RyanMqttError_e RyanMqttAckListRemoveToUserAckList(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
extern void RyanMqttClearAckSession(RyanMqttClient_t *client, uint8_t packetType, uint16_t packetId);

extern uint16_t RyanMqttGetNextPacketId(RyanMqttClient_t *client);

#ifdef __cplusplus
}
#endif

#endif
