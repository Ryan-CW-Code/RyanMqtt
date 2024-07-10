#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "RyanMqttPublic.h"
#include "platformTimer.h"

#include "cmsis_os2.h"
#include "ril.h"
#include "ril_util.h "
#include "ql_ps.h"
#include "ql_socket.h"
#include "ql_urc_register.h"

    typedef struct
    {
        int socket;
        osEventFlagsId_t mqttNetEventHandle;
    } platformNetwork_t;

    extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, const char *port);
    extern RyanMqttError_e platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, int recvLen, int timeout);
    extern RyanMqttError_e platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, int sendLen, int timeout);
    extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
