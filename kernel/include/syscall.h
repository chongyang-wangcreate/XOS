#ifndef __SYSCALL_H__
#define __SYSCALL_H__
#include "dirent.h"


enum{

    NR_FORK,
    NR_OPEN,
    NR_READ,
    NR_WRITE,
    NR_SLEEP,
    NR_PWD,
    NR_CD,
    NR_DUP,
    NR_MKDIR,
    NR_STAT,
    NR_READDIR,
    NR_LSEEK,
    NR_CHMOD,
    NR_GETPID,
    NR_MAX

};


typedef int devno_t;

int sys_call_entry(int sys_call_no,struct pt_regs *regs);

extern int sys_nano_sleep(int seconds);
extern int sys_mkdir(const char  *pathname, int mode);
extern int  sys_mknod(const char *pathname,mode_t mode,devno_t dev);
extern int do_sys_open(char *path_name,int flags, int mode);
extern ssize_t do_sys_read(int fd,void *buf,ssize_t count);
extern ssize_t do_sys_write(int fd,void *buf,ssize_t count);
extern int do_sys_getcwd(char *user_buf,int buf_size);
extern int do_sys_chdir(char *user_pathname);
extern int do_sys_dup(int old_fd);
extern int do_clone(int clone_flags);
extern int sys_user_getstat(const char * filename, void *stat);
extern int sys_readdir(unsigned int fd, struct xos_dirent64 *dirent, unsigned int count);
extern int do_sys_llseek(int fd,loff_t offset,int whence);
extern int do_sys_chmod(const char *path_name,int mode);
extern int do_sys_getpid();
extern int do_sys_getppid();
extern int do_sys_gettgid();





#endif
