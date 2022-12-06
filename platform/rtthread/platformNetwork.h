

#ifndef __platformNetSocket__
#define __platformNetSocket__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <rtthread.h>
#include "RyanMqttPublic.h"
#include "platformTimer.h"

#ifdef RT_USING_SAL
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include "sal_netdb.h"

#else
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include <lwip/sockets.h>
#include "lwip/netdb.h"
#endif

    // 定义枚举类型

    // 定义结构体类型
    typedef struct
    {
        int socket;
    } platformNetwork_t;

    /* extern variables-----------------------------------------------------------*/

    extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, const char *port);
    extern RyanMqttError_e platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, int recvLen, int timeout);
    extern RyanMqttError_e platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, int sendLen, int timeout);
    extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
