#ifndef __XOS_FILE_H__
#define __XOS_FILE_H__


struct open_flags {
	int open_flag;
	int mode;
	int acc_mode;
	int intent;
	int lookup_flags;
};

extern ssize_t sys_read(int fd,void *buf,ssize_t count);


extern int alloc_unused_fd(struct task_struct *tsk);
extern struct file *get_free_file(int fd);

extern int file_fd_bind(struct file* new_file,int fd);
extern struct file * get_file_by_fd(int fd);

extern loff_t generic_ram_file_llseek(struct file *, loff_t , int);

#endif
