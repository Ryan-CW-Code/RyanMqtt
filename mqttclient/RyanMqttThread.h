
#ifndef __mqttClientTask__
#define __mqttClientTask__

#include "RyanMqttClient.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern void RyanMqttThread(void *argument);
    extern void RyanMqttEventMachine(RyanMqttClient_t *client, RyanMqttEventId_e eventId, void *eventData);

#ifdef __cplusplus
}
#endif

#endif
