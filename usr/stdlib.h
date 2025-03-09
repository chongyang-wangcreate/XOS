#ifndef __STDLIB_H__
#define __STDLIB_H__

extern unsigned int sleep(unsigned int sec);
extern unsigned int read(int fd , char *buf, int size);
extern int mkdir(const char *pathname,  int mode);

extern unsigned int dup(int fd);



#endif
