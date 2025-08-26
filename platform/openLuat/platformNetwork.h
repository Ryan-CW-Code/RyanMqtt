#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

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

#ifdef __cplusplus
}
#endif

#endif
