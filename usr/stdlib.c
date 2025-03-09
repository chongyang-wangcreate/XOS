#include "stdlib.h"
#include <stdio.h>
//#include <unistd.h>
//#include "../mm/mm.c"

//static struct mem_pool malloc_pool;
#if 0
void init_malloc_free(void)
{
	init_mm(&malloc_pool, sbrk, NULL, NULL, NULL, NULL);
	printf("get_pool_malloc_total_length=%d\n",
	       get_pool_malloc_total_length(&malloc_pool));
}

void *malloc(size_t size)
{
	return alloc_mem(&malloc_pool, size);
}

void free(void *ptr)
{
	free_mem(&malloc_pool, ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total_size = nmemb * size;
	char *p;
	int i;

	p = malloc(total_size);
	if (p == NULL) {
		return NULL;
	}

	for (i = 0; i < total_size; i++) {
		p[i] = 0;
	}

	return p;
}
#endif
