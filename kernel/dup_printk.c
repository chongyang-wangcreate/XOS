/*****************************************************

 * Copyright (C) 1991, 1992  Linus Torvalds
 * License terms: GNU General Public License (GPL) version 2
 *
 ************************************************************/

#include <stdarg.h>
#include "types.h"
#include "list.h"
#include "string.h"
#include "mem_layout.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "uart.h"
#include "printk.h"

#define true  1
#define false 0
#define INT_MAX		((int32_t)(~0U>>1))
static char format_buf[1024];
#define default_message_loglevel 4

typedef char int8_t;
typedef int bool;
typedef unsigned long size_t;

/*
#define va_start(ap, v) ap = (va_list)&v  // 把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))	  // ap指向下一个参数并返回其值
#define va_end(ap) ap = NULL		  // 清除ap
*/


/*
 * NOTE! This ctype does not handle EOF like the standard C
 * library is required to.
 */

#define _U	0x01	/* upper */
#define _L	0x02	/* lower */
#define _D	0x04	/* digit */
#define _C	0x08	/* cntrl */
#define _P	0x10	/* punct */
#define _S	0x20	/* white space (space/lf/tab) */
#define _X	0x40	/* hex digit */
#define _SP	0x80	/* hard space (0x20) */

const unsigned char _ctype[] = {
/* 0-7 */
_C, _C, _C, _C, _C, _C, _C, _C,
/* 8-15 */
_C, _C|_S, _C|_S, _C|_S, _C|_S, _C|_S, _C, _C,
/* 16-23 */
_C, _C, _C, _C, _C, _C, _C, _C,
/* 24-31 */
_C, _C, _C, _C, _C, _C, _C, _C,
/* 32-39 */
_S|_SP, _P, _P, _P, _P, _P, _P, _P,
/* 40-47 */
_P, _P, _P, _P, _P, _P, _P, _P,
/* 48-55 */
_D, _D, _D, _D, _D, _D, _D, _D,
/* 56-63 */
_D, _D, _P, _P, _P, _P, _P, _P,
/* 64-71 */
_P, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U,
/* 72-79 */
_U, _U, _U, _U, _U, _U, _U, _U,
/* 80-87 */
_U, _U, _U, _U, _U, _U, _U, _U,
/* 88-95 */
_U, _U, _U, _P, _P, _P, _P, _P,
/* 96-103 */
_P, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L,
/* 104-111 */
_L, _L, _L, _L, _L, _L, _L, _L,
/* 112-119 */
_L, _L, _L, _L, _L, _L, _L, _L,
/* 120-127 */
_L, _L, _L, _P, _P, _P, _P, _C,
/* 128-143 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 144-159 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 160-175 */
_S|_SP, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,
/* 176-191 */
_P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,
/* 192-207 */
_U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U,
/* 208-223 */
_U, _U, _U, _U, _U, _U, _U, _P, _U, _U, _U, _U, _U, _U, _U, _L,
/* 224-239 */
_L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L,
/* 240-255 */
_L, _L, _L, _L, _L, _L, _L, _P, _L, _L, _L, _L, _L, _L,  _L,  _L};

#define __ismask(x) (_ctype[(int32_t)(unsigned char)(x)])

#define isalnum(c)	((__ismask(c)&(_U|_L|_D)) != 0)
#define isalpha(c)	((__ismask(c)&(_U|_L)) != 0)
#define iscntrl(c)	((__ismask(c)&(_C)) != 0)
#define isdigit(c)	((__ismask(c)&(_D)) != 0)
#define isgraph(c)	((__ismask(c)&(_P|_U|_L|_D)) != 0)
#define islower(c)	((__ismask(c)&(_L)) != 0)
#define isprint(c)	((__ismask(c)&(_P|_U|_L|_D|_SP)) != 0)
#define ispunct(c)	((__ismask(c)&(_P)) != 0)

#define isupper(c)	((__ismask(c)&(_U)) != 0)
#define isxdigit(c)	((__ismask(c)&(_D|_X)) != 0)

#define isascii(c) (((unsigned char)(c)) <= 0x7f)
#define toascii(c) (((unsigned char)(c))&0x7f)

static inline unsigned char __usr_tolower(unsigned char c)
{
	if (isupper(c)) {
		c -= 'A'-'a';
	}
	return c;
}

static inline unsigned char __usr_toupper(unsigned char c)
{
	if (islower(c)) {
		c -= 'a'-'A';
	}
	return c;
}

#define tolower(c) __usr_tolower(c)
#define toupper(c) __usr_toupper(c)

/*
 * Fast implementation of tolower() for internal usage. Do not use in your
 * code.
 */
static inline char _tolower(const char c)
{
	return c | 0x20;
}

/* Fast check for octal digit */
static inline int32_t isodigit(const char c)
{
	return c >= '0' && c <= '7';
}

/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

static uint32_t simple_guess_base(const char *cp)
{
	if (cp[0] == '0') {
		if (TOLOWER(cp[1]) == 'x' && isxdigit(cp[2])) {
			return 16;
		}
		else {
			return 8;
		}
	} else {
		return 10;
	}
}
size_t strnlen(const char *s, size_t count)
{
	const char *sc;
	size_t count_t = count;

	for (sc = s; count_t-- && *sc != '\0'; ++sc)
	{
		/* nothing */;
	}
	return sc - s;
}

/**
 * simple_strtoull - convert a string to an unsigned long long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long long
simple_strtoull(const char *cp, char **endp, uint32_t base)
{
	unsigned long long result = 0;

	if (!base) {
		base = simple_guess_base(cp);
	}

	if (base == 16 && cp[0] == '0' && TOLOWER(cp[1]) == 'x') {
		cp += 2;
	}

	while (isxdigit(*cp)) {
		uint32_t value;

		value = isdigit(*cp) ? *cp - '0' : TOLOWER(*cp) - 'a' + 10;
		if (value >= base) {
			break;
		}
		result = result * base + value;
		cp++;
	}
	if (endp) {
		*endp = (char *)cp;
	}

	return result;
}
/* EXPORT_SYMBOL(simple_strtoull); */

/**
 * simple_strtoul - convert a string to an unsigned long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long simple_strtoul(const char *cp, char **endp, uint32_t base)
{
	return simple_strtoull(cp, endp, base);
}
/* EXPORT_SYMBOL(simple_strtoul); */

/**
 * simple_strtol - convert a string to a signed long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
long simple_strtol(const char *cp, char **endp, uint32_t base)
{
	if (*cp == '-') {
		return -simple_strtoul(cp + 1, endp, base);
	}

	return simple_strtoul(cp, endp, base);
}
/* EXPORT_SYMBOL(simple_strtol); */

/**
 * simple_strtoll - convert a string to a signed long long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
long long simple_strtoll(const char *cp, char **endp, uint32_t base)
{
	if (*cp == '-') {
		return -simple_strtoull(cp + 1, endp, base);
	}

	return simple_strtoull(cp, endp, base);
}
/* EXPORT_SYMBOL(simple_strtoll); */

static
int32_t skip_atoi(const char **s)
{
	int32_t i = 0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';

	return i;
}

/* Decimal conversion is by far the most typical, and is used
 * for /proc and /sys data. This directly impacts e.g. top performance
 * with many processes running. We optimize it for speed
 * using code from
 * http://www.cs.uiowa.edu/~jones/bcd/decimal.html
 * (with permission from the author, Douglas W. Jones). */

/* Formats correctly any integer in [0,99999].
 * Outputs from one to five digits depending on input.
 * On i386 gcc 4.1.2 -O2: ~250 bytes of code. */
static
char *put_dec_trunc(char *buf, unsigned q)
{
	unsigned d3, d2, d1, d0;

	d1 = (q>>4) & 0xf;
	d2 = (q>>8) & 0xf;
	d3 = (q>>12);

	d0 = 6*(d3 + d2 + d1) + (q & 0xf);
	q = (d0 * 0xcd) >> 11;
	d0 = d0 - 10*q;
	*buf++ = d0 + '0'; /* least significant digit */
	d1 = q + 9*d3 + 5*d2 + d1;
	if (d1 != 0) {
		q = (d1 * 0xcd) >> 11;
		d1 = d1 - 10*q;
		*buf++ = d1 + '0'; /* next digit */

		d2 = q + 2*d2;
		if ((d2 != 0) || (d3 != 0)) {
			q = (d2 * 0xd) >> 7;
			d2 = d2 - 10*q;
			*buf++ = d2 + '0'; /* next digit */

			d3 = q + 4*d3;
			if (d3 != 0) {
				q = (d3 * 0xcd) >> 11;
				d3 = d3 - 10*q;
				*buf++ = d3 + '0';  /* next digit */
				if (q != 0) {
					*buf++ = q + '0';
				}
					/* most sign. digit */
			}
		}
	}

	return buf;
}
/* Same with if's removed. Always emits five digits */
static
char *put_dec_full(char *buf, unsigned q)
{
	/* BTW, if q is in [0,9999], 8-bit ints will be enough, */
	/* but anyway, gcc produces better code with full-sized ints */
	unsigned d3, d2, d1, d0;

	d1 = (q>>4) & 0xf;
	d2 = (q>>8) & 0xf;
	d3 = (q>>12);

	/*
	 * Possible ways to approx. divide by 10
	 * gcc -O2 replaces multiply with shifts and adds
	 * (x * 0xcd) >> 11:	11001101 - shorter code than * 0x67 (on i386)
	 * (x * 0x67) >> 10:	1100111
	 * (x * 0x34) >> 9:	110100 - same
	 * (x * 0x1a) >> 8:	11010 - same
	 * (x * 0x0d) >> 7:	1101 - same, shortest code (on i386)
	 */
	d0 = 6*(d3 + d2 + d1) + (q & 0xf);
	q = (d0 * 0xcd) >> 11;
	d0 = d0 - 10*q;
	*buf++ = d0 + '0';
	d1 = q + 9*d3 + 5*d2 + d1;
		q = (d1 * 0xcd) >> 11;
		d1 = d1 - 10*q;
		*buf++ = d1 + '0';

		d2 = q + 2*d2;
			q = (d2 * 0xd) >> 7;
			d2 = d2 - 10*q;
			*buf++ = d2 + '0';

			d3 = q + 4*d3;
				q = (d3 * 0xcd) >> 11; /* - shorter code */
				/* q = (d3 * 0x67) >> 10; - would also work */
				d3 = d3 - 10*q;
				*buf++ = d3 + '0';
					*buf++ = q + '0';

	return buf;
}

# define vsprintf_do_div(n,base) ({				\
	uint32_t __rem;						\
	__rem = ((uint32_t)n) % ((uint32_t)(base));		\
	n = ((uint32_t)n) / ((uint32_t)(base));			\
	__rem;							\
 })

/* No inlining helps gcc to use registers better */
static
char *put_dec(char *buf, unsigned long long num)
{
	while (1) {
		unsigned rem;

		if (num < 100000) {
			return put_dec_trunc(buf, num);
		}
		rem = vsprintf_do_div(num, 100000);
		buf = put_dec_full(buf, rem);
	}
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SMALL	32		/* use lowercase in hex (must be 32 == 0x20) */
#define SPECIAL	64		/* prefix hex with "0x", octal with "0" */

enum format_type {
	FORMAT_TYPE_NONE, /* Just a string part */
	FORMAT_TYPE_WIDTH,
	FORMAT_TYPE_PRECISION,
	FORMAT_TYPE_CHAR,
	FORMAT_TYPE_STR,
	FORMAT_TYPE_PTR,
	FORMAT_TYPE_PERCENT_CHAR,
	FORMAT_TYPE_INVALID,
	FORMAT_TYPE_LONG_LONG,
	FORMAT_TYPE_ULONG,
	FORMAT_TYPE_LONG,
	FORMAT_TYPE_UBYTE,
	FORMAT_TYPE_BYTE,
	FORMAT_TYPE_USHORT,
	FORMAT_TYPE_SHORT,
	FORMAT_TYPE_UINT,
	FORMAT_TYPE_INT,
	FORMAT_TYPE_NRCHARS,
	FORMAT_TYPE_SIZE_T,
	FORMAT_TYPE_PTRDIFF
};

struct printf_spec {
	uint8_t	type;		/* format_type enum */
	uint8_t	flags;		/* flags to number() */
	uint8_t	base;		/* number base, 8, 10 or 16 only */
	uint8_t	qualifier;	/* number qualifier, one of 'hHlLtzZ' */
	int16_t	field_width;	/* width of output field */
	int16_t	precision;	/* # of digits/chars */
};

static
char *number(char *buf, char *end, unsigned long long num,
		struct printf_spec spec)
{
	/* we are called with base 8, 10 or 16, only,
	thus don't need "G..."  */
	static const char digits[16] = "0123456789ABCDEF";
	/* "GHIJKLMNOPQRSTUVWXYZ"; */

	char tmp[66];
	char sign;
	char locase;
	int32_t need_pfx = ((spec.flags & SPECIAL) && spec.base != 10);
	int32_t i;

	/* locase = 0 or 0x20. ORing digits or letters with 'locase'
	 * produces same digits or (maybe lowercased) letters */
	locase = (spec.flags & SMALL);
	if (spec.flags & LEFT) {
		spec.flags &= ~ZEROPAD;
	}
	sign = 0;
	if (spec.flags & SIGN) {
		if ((signed long long)num < 0) {
			sign = '-';
			num = -(signed long long)num;
			spec.field_width--;
		} else if (spec.flags & PLUS) {
			sign = '+';
			spec.field_width--;
		} else if (spec.flags & SPACE) {
			sign = ' ';
			spec.field_width--;
		}
	}
	if (need_pfx) {
		spec.field_width--;
		if (spec.base == 16) {
			spec.field_width--;
		}
	}

	/* generate full string in tmp[], in reverse order */
	i = 0;
	if (num == 0) {
		tmp[i++] = '0';
	}
	/* Generic code, for any base:
	else do {
		tmp[i++] = (digits[vsprintf_do_div(num,base)] | locase);
	} while (num != 0);
	*/
	else if (spec.base != 10) { /* 8 or 16 */
		int32_t mask = spec.base - 1;
		int32_t shift = 3;

		if (spec.base == 16) {
			shift = 4;
		}
		do {
			tmp[i++] = (digits[((unsigned char)num) & mask]
					| locase);
			num >>= shift;
		} while (num);
	} else { /* base 10 */
		i = put_dec(tmp, num) - tmp;
	}

	/* printing 100 using %2d gives "100", not "00" */
	if (i > spec.precision) {
		spec.precision = i;
	}
	/* leading space padding */
	spec.field_width -= spec.precision;
	if (!(spec.flags & (ZEROPAD+LEFT))) {
		while (--spec.field_width >= 0) {
			if (buf < end) {
				*buf = ' ';
			}
			++buf;
		}
	}
	/* sign */
	if (sign) {
		if (buf < end) {
			*buf = sign;
		}
		++buf;
	}
	/* "0x" / "0" prefix */
	if (need_pfx) {
		if (buf < end) {
			*buf = '0';
		}
		++buf;
		if (spec.base == 16) {
			if (buf < end) {
				*buf = ('X' | locase);
			}
			++buf;
		}
	}
	/* zero or space padding */
	if (!(spec.flags & LEFT)) {
		char c = (spec.flags & ZEROPAD) ? '0' : ' ';

		while (--spec.field_width >= 0) {
			if (buf < end) {
				*buf = c;
			}
			++buf;
		}
	}
	/* hmm even more zero padding? */
	while (i <= --spec.precision) {
		if (buf < end) {
			*buf = '0';
		}
		++buf;
	}
	/* actual digits of result */
	while (--i >= 0) {
		if (buf < end) {
			*buf = tmp[i];
		}
		++buf;
	}
	/* trailing space padding */
	while (--spec.field_width >= 0) {
		if (buf < end) {
			*buf = ' ';
		}
		++buf;
	}

	return buf;
}

static
char *string(char *buf, char *end, const char *s, struct printf_spec spec)
{
	int32_t len, i;

	/* if ((unsigned long)s < PAGE_SIZE) */
	/* s = "(null)"; */

	len = strnlen(s, spec.precision);

	if (!(spec.flags & LEFT)) {
		while (len < spec.field_width--) {
			if (buf < end) {
				*buf = ' ';
			}
			++buf;
		}
	}
	for (i = 0; i < len; ++i) {
		if (buf < end) {
			*buf = *s;
		}
		++buf; ++s;
	}
	while (len < spec.field_width--) {
		if (buf < end) {
			*buf = ' ';
		}
		++buf;
	}

	return buf;
}

#define MODULE_NAME_LEN (64 - sizeof(unsigned long))
#define KSYM_NAME_LEN 128
#define KSYM_SYMBOL_LEN (sizeof("%s+%#lx/%#lx [%s]") + (KSYM_NAME_LEN - 1) +	\
			2*(BITS_PER_LONG*3/10) + (MODULE_NAME_LEN - 1) + 1)
static
char *symbol_string(char *buf, char *end, void *ptr,
			struct printf_spec spec, char ext)
{
	unsigned long value = (unsigned long) ptr;
#ifdef CONFIG_KALLSYMS
	char sym[KSYM_SYMBOL_LEN];

	if (ext != 'f' && ext != 's')
		sprint_symbol(sym, value);
	else
		kallsyms_lookup(value, NULL, NULL, NULL, sym);

	return string(buf, end, sym, spec);
#else

	spec.field_width = 2 * sizeof(void *);
	spec.flags |= SPECIAL | SMALL | ZEROPAD;
	spec.base = 16;

	return number(buf, end, value, spec);
#endif

}

/*
 * Show a '%p' thing.  A kernel extension is that the '%p' is followed
 * by an extra set of alphanumeric characters that are extended format
 * specifiers.
 *
 * Right now we handle:
 *
 * - 'F' For symbolic function descriptor pointers with offset
 * - 'f' For simple symbolic function names without offset
 * - 'S' For symbolic direct pointers with offset
 * - 's' For symbolic direct pointers without offset
 * - 'R' For decoded struct resource, e.g., [mem 0x0-0x1f 64bit pref]
 * - 'r' For raw struct resource, e.g., [mem 0x0-0x1f flags 0x201]
 * - 'M' For a 6-byte MAC address, it prints the address in the
 *       usual colon-separated hex notation
 * - 'm' For a 6-byte MAC address, it prints the hex address without colons
 * - 'MF' For a 6-byte MAC FDDI address, it prints the address
 *       with a dash-separated hex notation
 * - 'I' [46] for IPv4/IPv6 addresses printed in the usual way
 *       IPv4 uses dot-separated decimal without leading 0's (1.2.3.4)
 *       IPv6 uses colon separated network-order 16 bit hex with leading 0's
 * - 'i' [46] for 'raw' IPv4/IPv6 addresses
 *       IPv6 omits the colons (01020304...0f)
 *       IPv4 uses dot-separated decimal with leading 0's (010.123.045.006)
 * - '[Ii]4[hnbl]' IPv4 addresses in host, network, big or little endian order
 * - 'I6c' for IPv6 addresses printed as specified by
 *       http://tools.ietf.org/html/draft-ietf-6man-text-addr-representation-00
 * - 'U' For a 16 byte UUID/GUID, it prints the UUID/GUID in the form
 *       "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 *       Options for %pU are:
 *         b big endian lower case hex (default)
 *         B big endian UPPER case hex
 *         l little endian lower case hex
 *         L little endian UPPER case hex
 *           big endian output byte order is:
 *             [0][1][2][3]-[4][5]-[6][7]-[8][9]-[10][11][12][13][14][15]
 *           little endian output byte order is:
 *             [3][2][1][0]-[5][4]-[7][6]-[8][9]-[10][11][12][13][14][15]
 * - 'V' For a struct va_format which contains a format string * and va_list *,
 *       call vsnprintf(->format, *->va_list).
 *       Implements a "recursive vsnprintf".
 *       Do not use this feature without some mechanism to verify the
 *       correctness of the format string and va_list arguments.
 *
 * Note: The difference between 'S' and 'F' is that on ia64 and ppc64
 * function pointers are really function descriptors, which contain a
 * pointer to the real address.
 */
#ifndef dereference_function_descriptor
#define dereference_function_descriptor(p) (p)
#endif

static
char *pointer(const char *fmt, char *buf, char *end, void *ptr,
		struct printf_spec spec)
{
	if (!ptr) {
		return string(buf, end, "(null)", spec);
	}

	switch (*fmt) {
	case 'F':
	case 'f':
		ptr = dereference_function_descriptor(ptr);
		/* Fallthrough */
	case 'S':
	case 's':
		return symbol_string(buf, end, ptr, spec, *fmt);
	case 'R':
	case 'r':
		return "";/* resource_string(buf, end, ptr, spec, fmt); */
	case 'M':
	/* Colon separated: 00:01:02:03:04:05 */
	case 'm':			/* Contiguous: 000102030405 */
					/* [mM]F (FDDI, bit reversed) */
		return "";/* mac_address_string(buf, end, ptr, spec, fmt); */
	case 'I':			/* Formatted IP supported
					 * 4:	1.2.3.4
					 * 6:	0001:0203:...:0708
					 * 6c:	1::708 or 1::1.2.3.4
					 */
	case 'i':			/* Contiguous:
					 * 4:	001.002.003.004
					 * 6:   000102...0f
					 */
		switch (fmt[1]) {
		case '6':
			return "";
			/* ip6_addr_string(buf, end, ptr, spec, fmt); */
		case '4':
			return "";
			/* ip4_addr_string(buf, end, ptr, spec, fmt); */
		}
		break;
	case 'U':
		return "";/* uuid_string(buf, end, ptr, spec, fmt); */
	case 'V':
		return "";
	}
	spec.flags |= SMALL;
	if (spec.field_width == -1) {
		spec.field_width = 2*sizeof(void *);
		spec.flags |= ZEROPAD;
	}
	spec.base = 16;

	return number(buf, end, (unsigned long) ptr, spec);
}

/*
 * Helper function to decode printk style format.
 * Each call decode a token from the format and return the
 * number of characters read (or likely the delta where it wants
 * to go on the next call).
 * The decoded token is returned through the parameters
 *
 * 'h', 'l', or 'L' for integer fields
 * 'z' support added 23/7/1999 S.H.
 * 'z' changed to 'Z' --davidm 1/25/99
 * 't' added for ptrdiff_t
 *
 * @fmt: the format string
 * @type of the token returned
 * @flags: various flags such as +, -, # tokens..
 * @field_width: overwritten width
 * @base: base of the number (octal, hex, ...)
 * @precision: precision of a number
 * @qualifier: qualifier of a number (long, size_t, ...)
 */
static
int32_t format_decode(const char *fmt, struct printf_spec *spec)
{
	const char *start = fmt;

	/* we finished early by reading the field width */
	if (spec->type == FORMAT_TYPE_WIDTH) {
		if (spec->field_width < 0) {
			spec->field_width = -spec->field_width;
			spec->flags |= LEFT;
		}
		spec->type = FORMAT_TYPE_NONE;
		goto precision;
	}

	/* we finished early by reading the precision */
	if (spec->type == FORMAT_TYPE_PRECISION) {
		if (spec->precision < 0) {
			spec->precision = 0;
		}

		spec->type = FORMAT_TYPE_NONE;
		goto qualifier;
	}

	/* By default */
	spec->type = FORMAT_TYPE_NONE;

	for (; *fmt ; ++fmt) {
		if (*fmt == '%') {
			break;
		}
	}

	/* Return the current non-format string */
	if (fmt != start || !*fmt) {
		return fmt - start;
	}

	/* Process flags */
	spec->flags = 0;

	while (1) { /* this also skips first '%' */
		bool found = true;

		++fmt;

		switch (*fmt) {
		case '-':
			spec->flags |= LEFT;
			break;
		case '+':
			spec->flags |= PLUS;
			break;
		case ' ':
			spec->flags |= SPACE;
			break;
		case '#':
			spec->flags |= SPECIAL;
			break;
		case '0':
			spec->flags |= ZEROPAD;
			break;
		default:
			found = false;

		}

		if (!found) {
			break;
		}
	}

	/* get field width */
	spec->field_width = -1;

	if (isdigit(*fmt)) {
		spec->field_width = skip_atoi(&fmt);
	}
	else if (*fmt == '*') {
		/* it's the next argument */
		spec->type = FORMAT_TYPE_WIDTH;
		return ++fmt - start;
	}

precision:
	/* get the precision */
	spec->precision = -1;
	if (*fmt == '.') {
		++fmt;
		if (isdigit(*fmt)) {
			spec->precision = skip_atoi(&fmt);
			if (spec->precision < 0) {
				spec->precision = 0;
			}
		} else if (*fmt == '*') {
			/* it's the next argument */
			spec->type = FORMAT_TYPE_PRECISION;
			return ++fmt - start;
		}
	}

qualifier:
	/* get the conversion qualifier */
	spec->qualifier = -1;
	if (*fmt == 'h' || TOLOWER(*fmt) == 'l' ||
		TOLOWER(*fmt) == 'z' || *fmt == 't') {
		spec->qualifier = *fmt++;
		if ((spec->qualifier == *fmt)) {
			if (spec->qualifier == 'l') {
				spec->qualifier = 'L';
				++fmt;
			} else if (spec->qualifier == 'h') {
				spec->qualifier = 'H';
				++fmt;
			}
		}
	}

	/* default base */
	spec->base = 10;
	switch (*fmt) {
	case 'c':
		spec->type = FORMAT_TYPE_CHAR;
		return ++fmt - start;

	case 's':
		spec->type = FORMAT_TYPE_STR;
		return ++fmt - start;

	case 'p':
		spec->type = FORMAT_TYPE_PTR;
		return fmt - start;
		/* skip alnum */

	case 'n':
		spec->type = FORMAT_TYPE_NRCHARS;
		return ++fmt - start;

	case '%':
		spec->type = FORMAT_TYPE_PERCENT_CHAR;
		return ++fmt - start;

	/* integer number formats - set up the flags and "break" */
	case 'o':
		spec->base = 8;
		break;

	case 'x':
		spec->flags |= SMALL;

	case 'X':
		spec->base = 16;
		break;

	case 'd':
	case 'i':
		spec->flags |= SIGN;
	case 'u':
		break;

	default:
		spec->type = FORMAT_TYPE_INVALID;
		return fmt - start;
	}

	if (spec->qualifier == 'L') {
		spec->type = FORMAT_TYPE_LONG_LONG;
	}
	else if (spec->qualifier == 'l') {
		if (spec->flags & SIGN) {
			spec->type = FORMAT_TYPE_LONG;
		}
		else {
			spec->type = FORMAT_TYPE_ULONG;
		}
	} else if (TOLOWER(spec->qualifier) == 'z') {
		spec->type = FORMAT_TYPE_SIZE_T;
	} else if (spec->qualifier == 't') {
		spec->type = FORMAT_TYPE_PTRDIFF;
	} else if (spec->qualifier == 'H') {
		if (spec->flags & SIGN) {
			spec->type = FORMAT_TYPE_BYTE;
		}
		else {
			spec->type = FORMAT_TYPE_UBYTE;
		}
	} else if (spec->qualifier == 'h') {
		if (spec->flags & SIGN) {
			spec->type = FORMAT_TYPE_SHORT;
		}
		else {
			spec->type = FORMAT_TYPE_USHORT;
		}
	} else {
		if (spec->flags & SIGN) {
			spec->type = FORMAT_TYPE_INT;
		}
		else {
			spec->type = FORMAT_TYPE_UINT;
		}
	}

	return ++fmt - start;
}

/**
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * This function follows C99 vsnprintf, but has some extensions:
 * %pS output the name of a text symbol with offset
 * %ps output the name of a text symbol without offset
 * %pF output the name of a function pointer with its offset
 * %pf output the name of a function pointer without its offset
 * %pR output the address range in a struct resource with decoded flags
 * %pr output the address range in a struct resource with raw flags
 * %pM output a 6-byte MAC address with colons
 * %pm output a 6-byte MAC address without colons
 * %pI4 print an IPv4 address without leading zeros
 * %pi4 print an IPv4 address with leading zeros
 * %pI6 print an IPv6 address with colons
 * %pi6 print an IPv6 address without colons
 * %pI6c print an IPv6 address as specified by
 *   http://tools.ietf.org/html/draft-ietf-6man-text-addr-representation-00
 * %pU[bBlL] print a UUID/GUID in big or little endian using lower or upper
 *   case.
 * %n is ignored
 *
 * The return value is the number of characters which would
 * be generated for the given input, excluding the trailing
 * '\0', as per ISO C99. If you want to have the exact
 * number of characters written into @buf as return value
 * (not including the trailing '\0'), use vscnprintf(). If the
 * return is greater than or equal to @size, the resulting
 * string is truncated.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want snprintf() instead.
 */
int32_t vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	unsigned long long num;
	char *str, *end;
	struct printf_spec spec = {0};

	/* Reject out-of-range values early.  Large positive sizes are
	   used for unknown buffer sizes. */

	str = buf;
	end = buf + size;

	/* Make sure end is always >= buf */
	if (end < buf) {
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt) {
		const char *old_fmt = fmt;
		int32_t read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE: {
			int32_t copy = read;

			if (str < end) {
				if (copy > end - str) {
					copy = end - str;
				}
				memcpy(str, old_fmt, copy);
			}
			str += read;
			break;
		}

		case FORMAT_TYPE_WIDTH:
			spec.field_width = va_arg(args, int32_t);
			break;

		case FORMAT_TYPE_PRECISION:
			spec.precision = va_arg(args, int32_t);
			break;

		case FORMAT_TYPE_CHAR: {
			char c;

			if (!(spec.flags & LEFT)) {
				while (--spec.field_width > 0) {
					if (str < end) {
						*str = ' ';
					}
					++str;

				}
			}
			c = (unsigned char) va_arg(args, int32_t);
			if (str < end) {
				*str = c;
			}
			++str;
			while (--spec.field_width > 0) {
				if (str < end) {
					*str = ' ';
				}
				++str;
			}
			break;
		}

		case FORMAT_TYPE_STR:
			str = string(str, end, va_arg(args, char *), spec);
			break;

		case FORMAT_TYPE_PTR:
			str = pointer(fmt+1, str, end, va_arg(args, void *),
					spec);
			while (isalnum(*fmt))
				fmt++;
			break;

		case FORMAT_TYPE_PERCENT_CHAR:
			if (str < end) {
				*str = '%';
			}
			++str;
			break;

		case FORMAT_TYPE_INVALID:
			if (str < end) {
				*str = '%';
			}
			++str;
			break;

		case FORMAT_TYPE_NRCHARS: {
			uint8_t qualifier = spec.qualifier;

			if (qualifier == 'l') {
				long *ip = va_arg(args, long *);
				*ip = (str - buf);
			} else if (TOLOWER(qualifier) == 'z') {
				size_t *ip = va_arg(args, size_t *);
				*ip = (str - buf);
			} else {
				int32_t *ip = va_arg(args, int32_t *);
				*ip = (str - buf);
			}
			break;
		}

		default:
			switch (spec.type) {
			case FORMAT_TYPE_LONG_LONG:
				num = va_arg(args, long long);
				break;
			case FORMAT_TYPE_ULONG:
				num = va_arg(args, unsigned long);
				break;
			case FORMAT_TYPE_LONG:
				num = va_arg(args, long);
				break;
			case FORMAT_TYPE_SIZE_T:
				num = va_arg(args, size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				num = va_arg(args, int32_t);
				break;
			case FORMAT_TYPE_UBYTE:
				num = (unsigned char) va_arg(args, int32_t);
				break;
			case FORMAT_TYPE_BYTE:
				num = (int8_t) va_arg(args, int32_t);
				break;
			case FORMAT_TYPE_USHORT:
				num = (unsigned short) va_arg(args, int32_t);
				break;
			case FORMAT_TYPE_SHORT:
				num = (short) va_arg(args, int32_t);
				break;
			case FORMAT_TYPE_INT:
				num = (int32_t) va_arg(args, int32_t);
				break;
			default:
				num = va_arg(args, uint32_t);
			}

			str = number(str, end, num, spec);
		}
	}

	if (size > 0) {
		if (str < end) {
			if (*(str-1) == '\n')
				*(str++) = '\r';
			*str = '\0';
		}
		else {
			end[-1] = '\0';
		}
	}

	/* the trailing null byte doesn't count towards the total */
	return str-buf;

}
/* EXPORT_SYMBOL(vsnprintf); */

/**
 * vscnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which have been written into
 * the @buf not including the trailing '\0'. If @size is <= 0 the function
 * returns 0.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want scnprintf() instead.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int32_t vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int32_t i;

	i = vsnprintf(buf, size, fmt, args);

	return (i >= size) ? (size - 1) : i;
}
/* EXPORT_SYMBOL(vscnprintf); */

/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters which would be
 * generated for the given input, excluding the trailing null,
 * as per ISO C99.  If the return is greater than or equal to
 * @size, the resulting string is truncated.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int32_t snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int32_t i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}
/* EXPORT_SYMBOL(snprintf); */

/**
 * scnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is <= 0 the function returns 0.
 */

int32_t scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int32_t i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return (i >= size) ? (size - 1) : i;
}
/* EXPORT_SYMBOL(scnprintf); */

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use vsnprintf() or vscnprintf() in order to avoid
 * buffer overflows.
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want sprintf() instead.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int32_t vsprintf(char *buf, const char *fmt, va_list args)
{
	return vsnprintf(buf, INT_MAX, fmt, args);
}
/* EXPORT_SYMBOL(vsprintf); */

/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of characters written
 * into @buf. Use snprintf() or scnprintf() in order to avoid
 * buffer overflows.
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int32_t sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int32_t i;

	va_start(args, fmt);
	i = vsnprintf(buf, INT_MAX, fmt, args);
	va_end(args);

	return i;
}
/* EXPORT_SYMBOL(sprintf); */

#ifdef CONFIG_BINARY_PRINTF
/*
 * bprintf service:
 * vbin_printf() - VA arguments to binary data
 * bstr_printf() - Binary data to text string
 */

/**
 * vbin_printf - Parse a format string and place args' binary value in a buffer
 * @bin_buf: The buffer to place args' binary value
 * @size: The size of the buffer(by words(32bits), not characters)
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The format follows C99 vsnprintf, except %n is ignored, and its argument
 * is skiped.
 *
 * The return value is the number of words(32bits) which would be generated for
 * the given input.
 *
 * NOTE:
 * If the return value is greater than @size, the resulting bin_buf is NOT
 * valid for bstr_printf().
 */
int32_t vbin_printf(uint32_t *bin_buf, size_t size, const char *fmt, va_list args)
{
	struct printf_spec spec = {0};
	char *str, *end;

	str = (char *)bin_buf;
	end = (char *)(bin_buf + size);

#define save_arg(type)							\
do {									\
	if (sizeof(type) == 8) {					\
		unsigned long long value;				\
		str = PTR_ALIGN(str, sizeof(uint32_t));			\
		value = va_arg(args, unsigned long long);		\
		if (str + sizeof(type) <= end) {			\
			*(uint32_t *)str = *(uint32_t *)&value;		\
			*(uint32_t *)(str + 4) = *((uint32_t *)&value + 1);	\
		}							\
	} else {							\
		unsigned long value;					\
		str = PTR_ALIGN(str, sizeof(type));			\
		value = va_arg(args, int32_t);				\
		if (str + sizeof(type) <= end)				\
			*(typeof(type) *)str = (type)value;		\
	}								\
	str += sizeof(type);						\
} while (0)

	while (*fmt) {
		int32_t read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE:
		case FORMAT_TYPE_INVALID:
		case FORMAT_TYPE_PERCENT_CHAR:
			break;

		case FORMAT_TYPE_WIDTH:
		case FORMAT_TYPE_PRECISION:
			save_arg(int32_t);
			break;

		case FORMAT_TYPE_CHAR:
			save_arg(char);
			break;

		case FORMAT_TYPE_STR: {
			const char *save_str = va_arg(args, char *);
			size_t len;

			if ((unsigned long)save_str > (unsigned long)-PAGE_SIZE
					|| (unsigned long)save_str < PAGE_SIZE)
				save_str = "(null)";
			len = strlen(save_str) + 1;
			if (str + len < end) {
				memcpy(str, save_str, len);
			}
			str += len;
			break;
		}

		case FORMAT_TYPE_PTR:
			save_arg(void *);
			/* skip all alphanumeric pointer suffixes */
			while (isalnum(*fmt))
				fmt++;
			break;

		case FORMAT_TYPE_NRCHARS: {
			/* skip %n 's argument */
			uint8_t qualifier = spec.qualifier;
			void *skip_arg;

			if (qualifier == 'l') {
				skip_arg = va_arg(args, long *);
			} else if (TOLOWER(qualifier) == 'z') {
				skip_arg = va_arg(args, size_t *);
			} else {
				skip_arg = va_arg(args, int32_t *);
			}
			break;
		}

		default:
			switch (spec.type) {

			case FORMAT_TYPE_LONG_LONG:
				save_arg(long long);
				break;
			case FORMAT_TYPE_ULONG:
			case FORMAT_TYPE_LONG:
				save_arg(unsigned long);
				break;
			case FORMAT_TYPE_SIZE_T:
				save_arg(size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				save_arg(ptrdiff_t);
				break;
			case FORMAT_TYPE_UBYTE:
			case FORMAT_TYPE_BYTE:
				save_arg(char);
				break;
			case FORMAT_TYPE_USHORT:
			case FORMAT_TYPE_SHORT:
				save_arg(short);
				break;
			default:
				save_arg(int32_t);
			}
		}
	}

	return (uint32_t *)(PTR_ALIGN(str, sizeof(uint32_t))) - bin_buf;
#undef save_arg
}
/* EXPORT_SYMBOL_GPL(vbin_printf); */

/**
 * bstr_printf - Format a string from binary arguments and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @bin_buf: Binary arguments for the format string
 *
 * This function like C99 vsnprintf, but the difference is that vsnprintf gets
 * arguments from stack, and bstr_printf gets arguments from @bin_buf which is
 * a binary buffer that generated by vbin_printf.
 *
 * The format follows C99 vsnprintf, but has some extensions:
 *  see vsnprintf comment for details.
 *
 * The return value is the number of characters which would
 * be generated for the given input, excluding the trailing
 * '\0', as per ISO C99. If you want to have the exact
 * number of characters written into @buf as return value
 * (not including the trailing '\0'), use vscnprintf(). If the
 * return is greater than or equal to @size, the resulting
 * string is truncated.
 */
int32_t bstr_printf(char *buf, size_t size, const char *fmt, const uint32_t *bin_buf)
{
	struct printf_spec spec = {0};
	char *str, *end;
	const char *args = (const char *)bin_buf;


	str = buf;
	end = buf + size;

#define get_arg(type)							\
({									\
	typeof(type) value;						\
	if (sizeof(type) == 8) {					\
		args = PTR_ALIGN(args, sizeof(uint32_t));		\
		*(uint32_t *)&value = *(uint32_t *)args;		\
		*((uint32_t *)&value + 1) = *(uint32_t *)(args + 4);	\
	} else {							\
		args = PTR_ALIGN(args, sizeof(type));			\
		value = *(typeof(type) *)args;				\
	}								\
	args += sizeof(type);						\
	value;								\
})

	/* Make sure end is always >= buf */
	if (end < buf) {
		end = ((void *)-1);
		size = end - buf;
	}

	while (*fmt) {
		const char *old_fmt = fmt;
		int32_t read = format_decode(fmt, &spec);

		fmt += read;

		switch (spec.type) {
		case FORMAT_TYPE_NONE: {
			int32_t copy = read;

			if (str < end) {
				if (copy > end - str) {
					copy = end - str;
				}
				memcpy(str, old_fmt, copy);
			}
			str += read;
			break;
		}

		case FORMAT_TYPE_WIDTH:
			spec.field_width = get_arg(int32_t);
			break;

		case FORMAT_TYPE_PRECISION:
			spec.precision = get_arg(int32_t);
			break;

		case FORMAT_TYPE_CHAR: {
			char c;

			if (!(spec.flags & LEFT)) {
				while (--spec.field_width > 0) {
					if (str < end) {
						*str = ' ';
					}
					++str;
				}
			}
			c = (unsigned char) get_arg(char);
			if (str < end) {
				*str = c;
			}
			++str;
			while (--spec.field_width > 0) {
				if (str < end) {
					*str = ' ';
				}
				++str;
			}
			break;
		}

		case FORMAT_TYPE_STR: {
			const char *str_arg = args;

			args += strlen(str_arg) + 1;
			str = string(str, end, (char *)str_arg, spec);
			break;
		}

		case FORMAT_TYPE_PTR:
			str = pointer(fmt+1, str, end, get_arg(void *), spec);
			while (isalnum(*fmt))
				fmt++;
			break;

		case FORMAT_TYPE_PERCENT_CHAR:
		case FORMAT_TYPE_INVALID:
			if (str < end) {
				*str = '%';
			}
			++str;
			break;

		case FORMAT_TYPE_NRCHARS:
			/* skip */
			break;

		default: {
			unsigned long long num;

			switch (spec.type) {

			case FORMAT_TYPE_LONG_LONG:
				num = get_arg(long long);
				break;
			case FORMAT_TYPE_ULONG:
			case FORMAT_TYPE_LONG:
				num = get_arg(unsigned long);
				break;
			case FORMAT_TYPE_SIZE_T:
				num = get_arg(size_t);
				break;
			case FORMAT_TYPE_PTRDIFF:
				num = get_arg(ptrdiff_t);
				break;
			case FORMAT_TYPE_UBYTE:
				num = get_arg(unsigned char);
				break;
			case FORMAT_TYPE_BYTE:
				num = get_arg(int8_t);
				break;
			case FORMAT_TYPE_USHORT:
				num = get_arg(unsigned short);
				break;
			case FORMAT_TYPE_SHORT:
				num = get_arg(short);
				break;
			case FORMAT_TYPE_UINT:
				num = get_arg(uint32_t);
				break;
			default:
				num = get_arg(int32_t);
			}

			str = number(str, end, num, spec);
		} /* default: */
		} /* switch(spec.type) */
	} /* while(*fmt) */

	if (size > 0) {
		if (str < end) {
			*str = '\0';
		}
		else {
			end[-1] = '\0';
		}
	}

#undef get_arg

	/* the trailing null byte doesn't count towards the total */
	return str - buf;
}
/* EXPORT_SYMBOL_GPL(bstr_printf); */

/**
 * bprintf - Parse a format string and place args' binary value in a buffer
 * @bin_buf: The buffer to place args' binary value
 * @size: The size of the buffer(by words(32bits), not characters)
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The function returns the number of words(uint32_t) written
 * into @bin_buf.
 */
int32_t bprintf(uint32_t *bin_buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int32_t ret;

	va_start(args, fmt);
	ret = vbin_printf(bin_buf, size, fmt, args);
	va_end(args);

	return ret;
}
/* EXPORT_SYMBOL_GPL(bprintf); */

#endif /* CONFIG_BINARY_PRINTF */

extern void _k_puts (char *s);

void _k_uart_putc(int c)
{
    
    volatile uint8 * uart0 = (uint8*)(UART0+VA_KERNEL_START);
    *uart0 = c;
}

void _k_puts (char *s)
{
    while (*s != '\0') {
        _k_uart_putc(*s);
        s++;
    }
}


uint32 printk(int level,const char *fmt, ...)
{
//	uintptr_t flags;
//	char *p;
	va_list args;
	int32_t len;
//	int32_t loglevel;

	va_start(args, fmt);


	memset(format_buf, 0, sizeof(format_buf));
	/**
	 * 将当前字符输出到临时缓冲区
	 */
	len = vsprintf(format_buf, fmt, args);

	/* 检查消息等级，没有则使用默认等级进行打印 */
//	p = format_buf;
    
	va_end(args);
    if(level < PT_RUN){
        _k_puts (format_buf);
    }
	return len;
}

