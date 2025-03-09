#ifndef __PRINTF_H__
#define __PRINTF_H__



extern int printf(const char *fmt, ...);
extern int print_char(char c);


#define NUM_HEX_FOR_PTR ((sizeof (void *))*2)


#endif
