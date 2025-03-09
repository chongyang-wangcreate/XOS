#ifndef __PRINTK_H__
#define __PRINTK_H__


enum{
    PT_ERROR,
    PT_WARRING,
    PT_DEBUG,
    PT_RUN,


};
extern void _k_puts (char *s);
extern void _putint (char *prefix, uint val, char* suffix);

extern uint32 printk(int level,const char* format, ...);

#endif
