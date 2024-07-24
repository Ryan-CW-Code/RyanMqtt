#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include "RyanMqttPublic.h"
#include "platformTimer.h"

#include <rtthread.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include "sal_netdb.h"
#include <netdb.h>
    // #include <sys/select.h> // 使用select时打开

    typedef struct
    {
        int socket;
    } platformNetwork_t;

    extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, uint16_t port);
    extern RyanMqttError_e platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, int recvLen, int timeout);
    extern RyanMqttError_e platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, int sendLen, int timeout);
    extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
