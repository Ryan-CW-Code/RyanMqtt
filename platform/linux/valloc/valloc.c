#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "valloc.h"

#define HEADER_SIZE sizeof(int)

static pthread_mutex_t mutex;
static int count = 0;
static int use = 0;

void *v_malloc(size_t size)
{
	if (size == 0)
	{
		return NULL;
	}

	void *p = malloc(size + HEADER_SIZE);
	if (!p)
	{
		return NULL;
	}

	*(int *)p = (int)size;

	pthread_mutex_lock(&mutex);
	count++;
	use += (int)size;
	pthread_mutex_unlock(&mutex);

	return (char *)p + HEADER_SIZE;
}

void *v_calloc(size_t num, size_t size)
{
	size_t total = num * size;
	void *p = v_malloc(total);
	if (p)
	{
		memset(p, 0, total);
	}
	return p;
}

void v_free(void *block)
{
	if (!block)
	{
		return;
	}

	void *p = (char *)block - HEADER_SIZE;
	int size = *(int *)p;

	pthread_mutex_lock(&mutex);
	count--;
	use -= size;
	pthread_mutex_unlock(&mutex);

	free(p);
}

void *v_realloc(void *block, size_t size)
{
	if (size == 0)
	{
		v_free(block);
		return NULL;
	}

	int old_size = 0;
	void *raw = NULL;

	if (block)
	{
		raw = (char *)block - HEADER_SIZE;
		old_size = *(int *)raw;
	}

	void *p = realloc(raw, size + HEADER_SIZE);
	if (!p)
	{
		return NULL;
	}

	*(int *)p = (int)size;

	pthread_mutex_lock(&mutex);
	if (!block)
	{
		count++;
	}
	use += (int)(size - old_size);
	pthread_mutex_unlock(&mutex);

	return (char *)p + HEADER_SIZE;
}

int v_mcheck(int *dstCount, int *dstUse)
{
	pthread_mutex_lock(&mutex);
	if (dstCount)
	{
		*dstCount = count;
	}
	if (dstUse)
	{
		*dstUse = use;
	}
	pthread_mutex_unlock(&mutex);
	return 0;
}

void displayMem(void)
{
	int32_t area2 = 0, use2 = 0;
	v_mcheck(&area2, &use2);
	printf("|||----------->>> area = %d, size = %d\r\n", area2, use2);
}

void vallocInit(void)
{
	pthread_mutex_init(&mutex, NULL);
}
