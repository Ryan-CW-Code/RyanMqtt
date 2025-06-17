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

    extern RyanMqttError_e platformNetworkInit(void *userData, platformNetwork_t *platformNetwork);
    extern RyanMqttError_e platformNetworkDestroy(void *userData, platformNetwork_t *platformNetwork);
    extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, uint16_t port);
    extern int32_t platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, size_t recvLen, int32_t timeout);
    extern int32_t platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, size_t sendLen, int32_t timeout);
    extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
