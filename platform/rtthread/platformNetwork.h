#ifndef __platformNetwork__
#define __platformNetwork__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <rtthread.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <netdb.h>

typedef struct
{
	int socket;
} platformNetwork_t;

#ifdef __cplusplus
}
#endif

#endif
