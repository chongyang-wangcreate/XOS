#ifndef __TASK_H__
#define __TASK_H__

//#include "types.h"
//#include "list.h"
//#include "xos_vfs.h"
//#include "mount.h"
#include "xos_kern_def.h"

#define CONFIG_TASK_NAME_SIZE 63
#define TASK_TYPE_USER      0x00
#define TASK_TYPE_KERNEL    0xff
#define SCHED_RR            0x00
#define SCHED_FIFO          0x01
#define STACK_SIZE  8192
#define PROCESS_MAX_FILE_NR 1024

#define THREAD_SIZE    STACK_SIZE


typedef unsigned long pid_t;
typedef void (*task_fun)(void*);



typedef enum task_state
{
    TSTATE_INVALID    = 0, 
    TSTATE_INACTIVE,       
    TSTATE_READY,    
    TSTATE_RUNNING,       
    TSTATE_SLEEPING,       
    TSTATE_PENDING,        /* READY_TO_RUN - But blocked */
    TSTATE_SUSPEND,        /* READY_TO_RUN - But suspend util resume */
    TSTATE_STOP,        
    NUM_TASK_STATES             /* Must be last */
} task_state_e;


struct cpu_context {
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
};


enum {
    VM_READ = 1 << 0,
    VM_WRITE  = 1 << 1,
    VM_EXEC  = 1 << 2,
    VM_SHARED  = 1 << 3,
    VM_FILE  = 1 << 4,
    VM_IPC  = 1 << 5
};



struct vm_area_struct
{
    struct mm_struct* vm_mm;
    
    struct vm_area_struct *vm_next, *vm_prev;
    unsigned long vm_start;
    unsigned long vm_end;
    unsigned long pma_saddr;
    unsigned long vm_page_prot;
    unsigned long vm_paoff;
    struct vma_ops* ops;
    uint32_t vm_flags;
};

struct vma_ops {
    void (*open)(struct vm_area_struct * area);
    void (*close)(struct vm_area_struct * area);
};


struct mm_struct
{
	struct vm_area_struct* mmap;
    pgd_t* mm_pgd;
	unsigned long free_area_cache;

    unsigned long start_code;
    unsigned long end_code;

    unsigned long start_data;
    unsigned long end_data;

    unsigned long start_brk;
    unsigned long end_brk;

    unsigned long start_stack;
    xos_spinlock_t mm_lock;
    int mmap_count;

};

/*
    结构体会不断的更新补充日趋完善  2024.0406 AM:11:00

    20240606 PM:22:18 今天抽时间看了内核要引入thread_info 结构的原因，task_struct 结构体占用空间太大了
    堆栈的大部分空间都被描述符分走了，这样可不好，所以引入thread_info
    peempt_count , need_switch 抢占相关的变量都放在thread_info 结构体中

*/
struct task_struct
{
    struct cpu_context   cpu_context;
    int32_t sched_flags;
    void*  kstack;
    struct task_struct *parent;
    struct mm_struct* mm;
    pgd_t* task_pgd;
    task_fun tsk_entry;
    task_fun  user_entry;
    xos_timer_t timer;
    pid_t pid;			
    pid_t ppid;
    pid_t tgid;
    pid_t tid;
    int32_t  task_cpunum;
    int32_t  task_status;
    int32_t  task_type;

    uint32_t timeslice;
    uint32_t timerslice_count;
    uint32_t delay_ticks;
	uint32_t prio;
	uint32_t default_prio;
    uint32_t sched_policy;
    u8   preempt_count; /*抢占计数*/
    u8  soft_nesting;  /*soft irq*/
    u8  int_nesting; /*int irq nesting*/
    char  need_switch; /*抢占标识变量，通过变量判断是否需要抢占*/
    int cpuid;
    task_state_e state;
    long   counter;  //timer slice
//    wait_queue_t wait_childexit;
    struct list_head children_list;/*通过本节点挂到父进程的链表*/
    struct list_head child_list;  /*当前进程子进程挂载的链表节点*/
    struct list_head g_list;  //全局链表
    struct list_head cpu_list; //per cpu 核自己链表
    struct list_head sem_list; //信号量等待链表
    struct list_head mutex_list;
    struct list_head wait_list;
    struct list_head delay_list;
//    int32_t preempt_count; // 持有的自旋锁的数量
    char t_name[128];
    char user_path[128]; //bin 路径
    x_task_fs fs_context;
    files_set_t files_set;

};

/*
    引入这个有多重意义
    主要时节省内核栈空间，开始并没有引入thread_info 这个结构，随着系统开发的不断深入
    发现thread_info 引入非常巧妙，加入到系统是非常好的
*/
typedef struct thread_info_st{
    int need_switch_flags;
    int preempt_count;  //抢占计数
    struct task_struct *p_task;
    int bind_cpu;

}thread_info;

/*
    开发本系统的初衷，
    一是：梦想，
    二是：让想学习操作系统的人，学习路线没那么陡峭
    
    不管sp 怎么变动，堆栈sp 对于8k align_down 向下对齐都是thread_info 首地址
*/
static inline thread_info *current_task_info_new(void)
{
	unsigned long sp;
	thread_info *ti;

	asm volatile (
		      "mov %0, SP\n\t"
		      : "=r"(sp)
		     );

	ti = (thread_info*)(sp & ~(KERNEL_STACK_SIZE - 1));

	return ti;
}


/*
    参考自内核
*/
typedef union thread_union {
	thread_info thread_val;
	unsigned long stack[8192/sizeof(long)];
}thread_union_t;

extern dlist_t pend_global_list;

extern int xos_thread_create(unsigned int prio, unsigned long fn, unsigned long arg);
extern struct task_struct *get_current_task(void);

extern void add_to_g_list(struct task_struct *task);
extern void init_fs_context(struct task_struct *parent, struct task_struct *child);
extern void add_to_cpu_runqueue(int cpuid,struct task_struct *task);
extern void del_from_cpu_runqueue(int cpuid,struct task_struct *task);
extern dlist_t task_global_list;
extern struct task_struct * get_next_task_from_cpu(int cpuid);


#define current_task get_current_task()

extern int load_proc_flags;

static inline int get_load_flags()
{
    return load_proc_flags;
}
#endif
