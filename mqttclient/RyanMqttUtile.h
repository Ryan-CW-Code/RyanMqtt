
#ifndef __mqttGlobalFun__
#define __mqttGlobalFun__

#ifdef __cplusplus
extern "C"
{
#endif
#include "RyanMqttClient.h"
    // 定义枚举类型

    // 定义结构体类型

    /* extern variables-----------------------------------------------------------*/

    extern void RyanMqttSetClientState(RyanMqttClient_t *client, RyanMqttState_e state);
    extern RyanMqttState_e RyanMqttGetClientState(RyanMqttClient_t *client);
    extern RyanMqttError_e RyanMqttIsConnected(RyanMqttClient_t *client);

    extern RyanMqttError_e RyanMqttSendPacket(RyanMqttClient_t *client, char *buf, int32_t length);
    extern RyanMqttError_e RyanMqttRecvPacket(RyanMqttClient_t *client, char *buf, int32_t length);

    extern RyanMqttError_e RyanMqttMsgHandlerCreate(char *topic, uint16_t topicLen, RyanMqttQos_e qos, RyanMqttMsgHandler_t **pMsgHandler);
    extern void RyanMqttMsgHandlerDestory(RyanMqttMsgHandler_t *msgHandler);
    extern RyanMqttError_e RyanMqttMsgHandlerFind(RyanMqttClient_t *client, char *topic, uint16_t topicLen, RyanMqttBool_e topicMatchedFlag, RyanMqttMsgHandler_t **pMsgHandler);
    extern RyanMqttError_e RyanMqttMsgHandlerAdd(RyanMqttClient_t *client, RyanMqttMsgHandler_t *msgHandler);
    extern RyanMqttError_e RyanMqttMsgHandlerRemove(RyanMqttMsgHandler_t *msgHandler);

    extern RyanMqttError_e RyanMqttAckHandlerCreate(RyanMqttClient_t *client, enum msgTypes packetType, uint16_t packetId, uint16_t packetLen, RyanMqttMsgHandler_t *msgHandler, RyanMqttAckHandler_t **pAckHandler);
    extern void RyanMqttAckHandlerDestroy(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
    extern RyanMqttError_e RyanMqttAckListAdd(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);
    extern RyanMqttError_e RyanMqttAckListNodeFind(RyanMqttClient_t *client, enum msgTypes packetType, uint16_t packetId, RyanMqttAckHandler_t **pAckHandler);
    extern RyanMqttError_e RyanMqttAckListRemove(RyanMqttClient_t *client, RyanMqttAckHandler_t *ackHandler);

    extern RyanMqttError_e RyanMqttStringCopy(char **dest, char *rest, uint32_t strLen);
    extern RyanMqttError_e RyanMqttSetPublishDup(char *headerBuf, uint8_t dup);

    extern void RyanMqttCleanSession(RyanMqttClient_t *client);

#ifdef __cplusplus
}
#endif

#endif
