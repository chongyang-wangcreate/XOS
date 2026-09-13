#ifndef __XOS_PROCESS_H__
#define __XOS_PROCESS_H__

#define XOS_WNOHANG 1

extern void do_sys_exit(int status) __attribute__((noreturn));
extern int do_sys_waitpid(int pid, int *status, int options);
extern void process_release_address_space(struct task_struct *task);

#endif