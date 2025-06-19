#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "RyanMqttPublic.h"
#include "platformSystem.h"

typedef struct
{
	int socket;
} platformNetwork_t;

extern RyanMqttError_e platformNetworkInit(void *userData, platformNetwork_t *platformNetwork);
extern RyanMqttError_e platformNetworkDestroy(void *userData, platformNetwork_t *platformNetwork);
extern RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host,
					      uint16_t port);
extern int32_t platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf,
					size_t recvLen, int32_t timeout);
extern int32_t platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf,
					size_t sendLen, int32_t timeout);
extern RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork);

#ifdef __cplusplus
}
#endif

#endif
