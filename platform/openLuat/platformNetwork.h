

#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include "RyanMqttPublic.h"
#include "platformTimer.h"

#include "common_api.h"
#include "lwip/ip4_addr.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

    typedef struct
    {
        int socket;
    } platformNetwork_t;

    extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, const char *port);
    extern RyanMqttError_e platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, int recvLen, int timeout);
    extern RyanMqttError_e platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, int sendLen, int timeout);
    extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
