#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__


enum  
{
    INITIALIZING,
    READY,
    SLEEP, 
    STOPPED, 
    MAX_STATUS
};

#define KERNEL_STACK_SIZE 8192




#define switch_to_next(prev, next, last)					\
	do {								\
		((last) = __switch_to_next((prev), (next)));			\
	} while (0)


extern struct  pt_regs * get_task_pt_regs(struct task_struct * task);

static inline struct task_struct *current_task_info(void)
{
	unsigned long sp;
	struct task_struct *ti;

	asm volatile (
		      "mov %0, SP\n\t"
		      : "=r"(sp)
		     );

	ti = (struct task_struct *)(sp & ~(KERNEL_STACK_SIZE - 1));

	return ti;
}



extern void show_pt_regs(const struct pt_regs *regs);
extern void schedule(void);
extern void int_schedule(void);
extern void load_first_task();
extern void handle_task_timerslice(int cpuid);
extern struct task_struct *get_current_task(void);
extern struct pt_regs * get_task_pt_regs_new(char* stack);
extern int wake_up_proc(struct task_struct *tsk);


#endif
