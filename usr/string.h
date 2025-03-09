#ifndef __U_STRING_H__
#define __U_STRING_H__

typedef unsigned int	uint;
typedef unsigned short	ushort;
typedef unsigned char	uchar;
typedef unsigned long	uint64;
typedef unsigned int	uint32;
typedef unsigned int u32;
typedef unsigned int U32;


extern void* memset(void *dst, int v, int n);
extern int memcmp(const void *v1, const void *v2, uint n);
extern void* memmove(void *dst, const void *src, uint n);
extern void* memcpy(void *dst, const void *src, uint n);
extern int strcmp(const char *p, const char *q);
extern int strncmp(const char *p, const char *q, uint n);

extern char* strncpy(char *s, const char *t, int n);
extern char* strcpy(char* dst_, const char* src_);
extern int strlen(const char* str);
extern char *itoa16_align(char * str, int num);
extern char*  safestrcpy(char*, const char*, int);
#endif

