#ifndef __CONSOLE_H__
#define __CONSOLE_H__


extern void ioq_putchar(con_uart_que* ioq, char byte);
extern char ioq_getchar(con_uart_que* ioq);
extern int  sys_mknod(const char *pathname,mode_t mode,devno_t dev);



#endif
