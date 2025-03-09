#include <unistd.h>

#if 0
void* usr_memset(void *dst, int v, int n)
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


char* usr_strcpy(char* dst_, const char* src_) {
   char* r = dst_;		       // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}

#endif

int getpid(){
    return  SYS_CALL_DEF0(13);
}

int getppid(){
    return  SYS_CALL_DEF0(14);
}

int gettgid(){
    return  SYS_CALL_DEF0(15);
}



