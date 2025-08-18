#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t mutex;
static int count = 0;
static int use = 0;

void *v_malloc(size_t size)
{
	void *p;
	p = malloc(size ? size + sizeof(int) : 0);
	if (!p)
	{
		return NULL;
	}

	pthread_mutex_lock(&mutex); // 互斥锁上锁
	count++;
	*(int *)p = size;
	use += size;
	pthread_mutex_unlock(&mutex); // 互斥锁解锁
	return (void *)((char *)p + sizeof(int));
}

void *v_calloc(size_t num, size_t size)
{
	void *p;
	p = v_malloc(num * size);
	if (!p)
	{
		return NULL;
	}
	memset(p, 0, num * size);
	return p;
}

void v_free(void *block)
{
	void *p;
	if (!block)
	{
		return;
	}
	p = (void *)((char *)block - sizeof(int));

	pthread_mutex_lock(&mutex); // 互斥锁上锁
	use -= *(int *)p;
	count--;
	pthread_mutex_unlock(&mutex); // 互斥锁解锁

	free(p);
}

void *v_realloc(void *block, size_t size)
{
	void *p;
	int s = 0;
	if (block)
	{
		block = (void *)((char *)block - sizeof(int));
		s = *(int *)block;
	}
	p = realloc(block, size ? size + sizeof(int) : 0);
	if (!p)
	{
		return NULL;
	}

	pthread_mutex_lock(&mutex); // 互斥锁上锁
	if (!block)
	{
		count++;
	}
	*(int *)p = size;
	use += (size - s);
	pthread_mutex_unlock(&mutex); // 互斥锁解锁

	return (void *)((char *)p + sizeof(int));
}

int v_mcheck(int *_count, int *_use)
{
	pthread_mutex_lock(&mutex); // 互斥锁上锁
	if (_count)
	{
		*_count = count;
	}
	if (_use)
	{
		*_use = use;
	}
	pthread_mutex_unlock(&mutex); // 互斥锁解锁

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
	/* 初始化互斥锁 */
	pthread_mutex_init(&mutex, NULL);
}
