#include "user_map.h"
#include <mmu.h>
#include <stdarg.h>

#include "printf.h"

/*
    参考自dup_printk.c,  CSDN以及求助大模型
    重新进行了简化开发

*/

#define UART_BASE       0x09000000
#define UART_UARTDR	(UART_BASE + 0x00)
#define UART_UARTIBRD	(UART_BASE + 0x24)
#define UART_UARTFBRD	(UART_BASE + 0x28)
#define UART_UARTLCR_H	(UART_BASE + 0x2C)
#define UART_UARTCR	(UART_BASE + 0x30)
#define UART_UARTIFLS	(UART_BASE + 0x34)
#define UART_UARTIMSC	(UART_BASE + 0x38)
#define UART_UARTRIS	(UART_BASE + 0x3C)
#define UART_UARTMIS	(UART_BASE + 0x40)
#define UART_UARTICR	(UART_BASE + 0x44)
#define UART_UARTPeriphID2	(UART_BASE + 0xFE8)

#define VIRT_GIC_DIST_BASE 0x08000000
#define VIRT_GIC_CPU_BASE 0x08010000


int print_char(char c)
{
     *(unsigned int *)_TO_UVA_(UART_UARTDR) = c;
 
     return 1;
}

static inline int print_string(const char *p)
{
	int i = 0;

	if (p == NULL) {
		return 0;
	}

	while (*(p + i) != '\0') {
		print_char(*(p + i));
		i++;
	}

	return i;
}

static inline int print_ulong_t(uint64_t d, int base, int width, char pad)
{
    static const char digits[] = "0123456789ABCDEF";  // 定义为const，因为digits不会改变

    int num_printed = 0;
    int num_digit = 0;
    int num_pad;

    // 处理数字并存入缓冲区
    char buf[64];  // 缓冲区存储转换后的数字字符
    do {
        buf[num_digit++] = digits[d % base];
        d /= base;
    } while (d != 0);

    // 计算需要填充的字符数
    num_pad = width - num_digit;

    // 填充字符
    for (int j = 0; j < num_pad; ++j) {
        print_char(pad);
        num_printed++;
    }

    // 打印已存储的数字字符（反向打印）
    for (int i = num_digit - 1; i >= 0; --i) {
        print_char(buf[i]);
        num_printed++;
    }

    return num_printed;
}



static inline int print_ptr(const void *p)
{
    int num_printed = 0;
    int base = 16;
    int width;
    char pad = '0';

    if (sizeof(void*) == 8) {
        // 64 位系统，指针宽度为 16 个字符（8 字节，16 个十六进制字符）
        width = 16;
    } else if (sizeof(void*) == 4) {
        // 32 位系统，指针宽度为 8 个字符（4 字节，8 个十六进制字符）
        width = 8;
    } else {
        // 默认宽度处理（假设不太可能发生，针对其他架构）
        width = 8; 
    }

    num_printed += print_ulong_t((uintptr_t)p, base, width, pad);

    return num_printed;
}

static inline int print_int32_t(int32_t d)
{
	int num_printed = 0;
	int base = 10;
	int width = -1;
	char pad = -1;

	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_ulong_t(d, base, width, pad);

	return num_printed;
}

static inline int print_hex(int64_t d)
{
	int num_printed = 0;
	int base = 16;
	int width = -1;
	char pad = -1;

	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_ulong_t(d, base, width, pad);

	return num_printed;
}



static inline int print_unsigned_int(unsigned int d, int base, int width, char pad)
{
    if (base < 2 || base > 16) {
        return -1; 
    }

    static char digits[] = "0123456789ABCDEF";
    int quotient, rem;
    int num_printed = 0;
    char buf[32]; // 假设数字不会超过 32 位
    int num_digit = 0;
    int i, j;
    int num_zero;

    do {
        quotient = d / base;
        rem = d % base;
        buf[num_digit++] = digits[rem];
        d = quotient;
    } while (d != 0);

    num_zero = width - num_digit;
    
    for (j = 0; j < num_zero; j++) {
        print_char(pad);
        num_printed += 1;
    }

    for (i = num_digit - 1; i >= 0; i--) {
        print_char(buf[i]);
        num_printed += 1;
    }

    return num_printed;
}

static inline int print_int(int d, int base, int width, char pad)
{
	int num_printed = 0;


	if (d < 0) {
		num_printed += print_char('-');
		d = -d;
	}

	num_printed += print_unsigned_int(d, base, width, pad);

	return num_printed;
}

int extract_width(const char **fmt) {
    int width = 0;
    while (**fmt >= '0' && **fmt <= '9') {
        width = width * 10 + (**fmt - '0');
        (*fmt)++;
    }
    if (width == 0) {
        return -1;
    }
    return width;
}

int printf(const char *fmt, ...)
{
    int len;

    va_list ap;
    
    int d;
    void *p;
    char c, *s;
    int width;
    char pad;

    va_start(ap, fmt); 
	for (;*fmt != '\0'; fmt++) { 
		if (*fmt != '%') { 
			print_char(*fmt); 
			len += 1; 
			continue; 
		} 
		fmt += 1; 
 
		if (*fmt == '\0') { 
			break; 
		} 
 
		pad = ' '; 
		if (*fmt == '0') { 
			pad = '0'; 
			fmt += 1; 
			if (*fmt == '\0') { 
				break; 
			} 
		} 
        /*提取宽度*/
        width = extract_width(&fmt);
        if (*fmt == '\0') {
            // 格式字符串结束，退出循环
            break;
        }
		switch (*fmt) { 
		case 's': /* string */ 
			s = va_arg(ap, char *); 
			len += print_string(s); 
			break; 
		case 'd': /* signed decimal */ 
			d = va_arg(ap, int); 
			len += print_int(d, 10, width, pad); 
			break; 
		case 'u': 
			d = va_arg(ap, int); 
			len += print_unsigned_int(d, 10, width, pad); 
			break; 
		case 'x': /* hex int */ 
			d = va_arg(ap, int); 
			len += print_unsigned_int(d, 16, width, pad); 
			break; 
		case 'o': /* octal int */ 
			d = va_arg(ap, int); 
			len += print_unsigned_int(d, 8, width, pad); 
			break; 
		case 'p': /* pointer */ 
			p = va_arg(ap, void *); 
			len += print_ptr(p); 
			break; 
		case 'c': /* char */ 
            c = va_arg(ap, int); 
			len += print_char(c); 
			break; 
		default: 
			break; 
		} 
	} 
	va_end(ap);

	return len;
}

