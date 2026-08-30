#ifndef __STDLIB_H__
#define __STDLIB_H__

extern unsigned int sleep(unsigned int sec);
extern unsigned int read(int fd , char *buf, int size);
extern int mkdir(const char *pathname,  int mode);
extern int execve(const char *pathname,const char *argv[],const char* envp[]);
extern unsigned int dup(int fd);



#endif
