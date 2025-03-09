/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2028 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "list.h"
#include "string.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "xos_cache.h"
#include "xos_kern_def.h"
#include "setup_map.h"
#include "mem_layout.h"
#include "xdentry.h"
#include "mount.h"
#include "mmu.h"
#include "tick_timer.h"
#include "task.h"
#include "schedule.h"
#include "printk.h"
#include "xos_file.h"
#include "dirent.h"
#include "syscall.h"


typedef int(*sys_call_fun)(struct pt_regs *regs);


enum{
    SYS_FORK,
    SYS_READ,
    SYS_CALL_MAX,
};



/*
    fork 系统调用与其它系统调用返回值存在一些冲突
    需要做相应更改，如果更改了，当前这个regs 父进程regs
*/

int do_syscall_fork(struct pt_regs *regs){

    int clone_flags = 0;
//    return do_clone(clone_flags,regs);
    return do_clone(clone_flags);
}
int do_syscall_open(struct pt_regs *regs)
{
    /*
        待实现copy_from_user
        暂时用memcpy     strcpy代替
    */
    int flags;
    int mode;
    char path_name[1204];
    strcpy(path_name,(char*)regs->regs[0]);
    flags = regs->regs[1];
    mode = regs->regs[2];
    return do_sys_open(path_name,flags, mode);
}
int do_syscall_read(struct pt_regs *regs){

    ssize_t ret;
    int fd = regs->regs[0];
    void *buf = (void*)regs->regs[1];    
    ssize_t count = regs->regs[2];
    printk(PT_RUN,"%s:%d,fd=%d\n\r",__FUNCTION__,__LINE__,fd);
    ret = do_sys_read(fd,buf,count);
    return ret;
}

int do_syscall_write(struct pt_regs *regs){

    ssize_t ret;
    int fd = regs->regs[0];
    void *buf = (void*)regs->regs[1];    
    ssize_t count = regs->regs[2];
    printk(PT_DEBUG,"%s:%d,fd=%d\n\r",__FUNCTION__,__LINE__,fd);
    ret = do_sys_write(fd,buf,count);
    return ret;
}


int do_syscall_sleep(struct pt_regs *regs){

    unsigned int seconds = regs->regs[1];
    
    sys_nano_sleep(seconds);
    return 0;
}


int do_syscall_getcwd(struct pt_regs *regs){

    char *buf = (char*)regs->regs[0];
    int buf_size = regs->regs[1];
    do_sys_getcwd(buf,buf_size);
    return 0;
}

int do_syscall_chdir(struct pt_regs *regs){

    char *buf = (char*)regs->regs[0];
    
    printk(PT_RUN,"%s:%d  buf=%s\n\r",__FUNCTION__,__LINE__,buf);
    do_sys_chdir(buf);
    return 0;
}

int do_syscall_dup(struct pt_regs *regs){

    int old_fd = (int)regs->regs[0];
    return  do_sys_dup(old_fd);

}

int do_syscall_mkdir(struct pt_regs *regs){

    const char* path_name = (char*)regs->regs[0];
    int mode = (int)regs->regs[1];
    printk(PT_RUN,"%s:%d,path_name=%s,mode=%d\n\r",__FUNCTION__,__LINE__,path_name,mode);
    return  sys_mkdir(path_name, mode);

}

int do_syscall_getstat(struct pt_regs *regs){

    const char* path_name = (char*)regs->regs[0];
    struct kstat *stat = (struct kstat *)regs->regs[1];
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return sys_user_getstat(path_name, stat);
}

int do_syscall_readdir(struct pt_regs *regs){

    int fd = (int)regs->regs[0];
    struct xos_dirent64 *dirent = (void*)regs->regs[1];
    int buf_size = (int)regs->regs[2];
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return  sys_readdir(fd,dirent, buf_size);

}

int do_syscall_llseek(struct pt_regs *regs)
{
    int fd = (int)regs->regs[0];
    loff_t offset = (loff_t)regs->regs[1];
    int whence = (int)regs->regs[2];
    return do_sys_llseek(fd,offset,whence);
}

int do_syscall_chmod(struct pt_regs *regs)
{
    const char* path_name = (char*)regs->regs[0];
    int mode = (int)regs->regs[1];
    return do_sys_chmod(path_name,mode);
}

int do_syscall_getpid(struct pt_regs *regs)
{
    return do_sys_getpid();
}

int do_syscall_getppid(struct pt_regs *regs)
{
    return do_sys_getppid();
}


sys_call_fun syscall_tabs[128] = {
        do_syscall_fork,
        do_syscall_open,
        do_syscall_read,
        do_syscall_write,
        do_syscall_sleep,
        do_syscall_getcwd,
        do_syscall_chdir,
        do_syscall_dup,
        do_syscall_mkdir,
        do_syscall_getstat,
        do_syscall_readdir,
        do_syscall_llseek,
        do_syscall_chmod,
        do_syscall_getpid,
};



int sys_call_entry(int sys_call_no,struct pt_regs *regs)
{
    int ret;
    printk(PT_RUN,"%s:%d,sys_call_no=%d\n\r",__FUNCTION__,__LINE__,sys_call_no);
    if(sys_call_no > NR_MAX){
        return -1;
    }
    ret = syscall_tabs[sys_call_no](regs);
    return ret;
}


