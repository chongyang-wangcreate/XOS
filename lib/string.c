#include "types.h"


void *memset(void *s, int c, size_t n) {
    unsigned char *ptr = (unsigned char *)s;  
    unsigned int value = (unsigned char)c;    
    /*使用value 填充满4个字节，加快拷贝速率*/
    unsigned int aligned_value = value | (value << 8) | (value << 16) | (value << 24); 

    // 对齐处理
    uintptr_t start = (uintptr_t)ptr;  // 获取起始地址
    size_t offset = start % 4;  // 计算地址偏移
    ptr += offset;

    while (offset > 0 && n > 0) {
        *ptr = (unsigned char)c;
        ptr++;
        offset--;
        n--;
    }

    // 填充对齐部分，每次填充 4 字节
    unsigned int *aligned_ptr = (unsigned int *)ptr;
    while (n >= 4) {
        *aligned_ptr = aligned_value; // 4 字节拷贝
        aligned_ptr++;
        ptr = (unsigned char *)aligned_ptr;
        n -= 4;
    }

    // 填充剩余的非对齐部分（如果有的话）
    while (n > 0) {
        *ptr = (unsigned char)c;
        ptr++;
        n--;
    }

    return s;
}

/*
    后续待优化，提高比较效率
*/

int memcmp(const void *v1, const void *v2, uint n)
{
    const uchar *s1, *s2;

    s1 = v1;
    s2 = v2;

    while(n-- > 0){
        if(*s1 != *s2) {
            return *s1 - *s2;
        }

        s1++, s2++;
    }

    return 0;
}


/*
    实现待优化,后续会进行优化，非对齐部分单字节拷贝，对齐部分4或者8字节拷贝
*/

void* memcpy(void *dst, const void *src, uint n)
{
    const char *src_ptr = (const char *)src;
    char *dest_ptr = (char *)dst;
    for (; n != 0; n--){
        *dest_ptr++ = *src_ptr++;
    }
    return dst;
}

int strncmp(const char *p, const char *q, uint n)
{
    while(n > 0 && *p && *p == *q) {
        n--, p++, q++;
    }

    if(n == 0) {
        return 0;
    }

    return (uchar)*p - (uchar)*q;
}

int strcmp(const char *p, const char *q)
{
    while(*p && *p == *q) {
        p++, q++;
    }

    return (uchar)*p - (uchar)*q;
}


char* strncpy(char *s, const char *t, int n)
{
    char *pos;

    pos = s;

    while(n-- > 0 && (*s++ = *t++) != 0);

    while(n-- > 0) {
        *s++ = 0;
    }

    return pos;
}

char* safestrcpy(char *s, const char *t, int n)
{
    char *pos;

    pos = s;

    if(n <= 0) {
        return pos;
    }

    while(--n > 0 && (*s++ = *t++) != 0);

    *s = 0;
    return pos;
}


char* strcpy(char* dst_, const char* src_) {
   char* r = dst_;		       // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}

/* 返回字符串长度 */
int strlen(const char* str) {
   const char* p = str;
   while(*p++);
   return (p - str - 1);
}

char *itoa16_align(char * str, int num)
{
	char *	p = str;
	char	ch;
	int	i;
	//为0
	if(num == 0){
		*p++ = '0';
	}
	else{	//4位4位的分解出来
		for(i=28;i>=0;i-=4){		//从最高得4位开始
			ch = (num >> i) & 0xF;	//取得4位
			ch += '0';			//大于0就+'0'变成ASICA的数字
			if(ch > '9'){		//大于9就加上7变成ASICA的字母
				ch += 7;		
			}
			*p++ = ch;			//指针地址上记录下来。
		}
	}
	*p = 0;							//最后在指针地址后加个0用于字符串结束
	return str;
}


char *strchr(const char *str,char dst_char){
     while(*str != '\0'){
        if(*str == dst_char){
            return (char*)str;
        }
        str++;
     }
     return NULL;

}

char *strcat(char *dst_str,char *src_str){
    char* dst_start = dst_str;
    while(*dst_str){
        dst_str++;
    }
    while(*src_str){
        *dst_str = *src_str;
        dst_str++;
        src_str++;
    }
    return dst_start;
}
char *strtok(const char *str,char *dst_str){

    int equ_cnt = 0;
    char *cur_str = (char*)str;
    char *dst_cur = dst_str;
    if(cur_str == NULL){
        return NULL;
    }
    while(*cur_str != '\0'){
        while((*cur_str != '\0')&&(*cur_str != *dst_cur)){
            cur_str++;
        }
        char *right_start = cur_str;
        while((*right_start != '\0')&&(*right_start == *dst_cur)){
            right_start++;
            dst_cur++;
            equ_cnt++;
        }
        if(equ_cnt == strlen(dst_str)){
            return right_start;
        }else if(*right_start == '\0'){
            return 0;
        }else{
            cur_str = right_start;
            dst_cur = dst_str;
        }
    }
    return NULL;
   
}
