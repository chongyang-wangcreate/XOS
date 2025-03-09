#ifndef __USTRING_H__
#define __USTRING_H__



static inline char* usr_strcpy(char* dst_, const char* src_) {
   char* r = dst_;  // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}

static inline int usr_strlen(const char* str) {
   const char* p = str;
   while(*p++);
   return (p - str - 1);
}

static inline int usr_strcmp(const char *p, const char *q)
{
    while(*p && *p == *q) {
        p++, q++;
    }
    if((*p == *q)&&(*p == '\0')){
        return 0;
    }
    return (uchar)*p - (uchar)*q;
}

static inline void* user_memset(void *dst, int v, int n)
{
    uint8	*p;
    uint8	c;
    uint32	val;
    uint32	*p4;

    p   = dst;
    c   = v & 0xff;
    val = (c << 24) | (c << 16) | (c << 8) | c;

    // set bytes before whole uint32
    for (; (n > 0) && ((uint64)p % 4); n--, p++){
        *p = c;
    }

    // set memory 4 bytes a time
    p4 = (uint*)p;

    for (; n >= 4; n -= 4, p4++) {
        *p4 = val;
    }

    // set leftover one byte a time
    p = (uint8*)p4;

    for (; n > 0; n--, p++) {
        *p = c;
    }

    return dst;
}











#endif
