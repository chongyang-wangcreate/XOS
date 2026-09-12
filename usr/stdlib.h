#ifndef __STDLIB_H__
#define __STDLIB_H__

#define WNOHANG 1

extern unsigned int sleep(unsigned int sec);
extern unsigned int read(int fd , char *buf, int size);
extern int mkdir(const char *pathname,  int mode);
extern int execve(const char *pathname,const char *argv[],const char* envp[]);
extern void _exit(int status);
extern int waitpid(int pid, int *status, int options);
extern unsigned int dup(int fd);



#endif
