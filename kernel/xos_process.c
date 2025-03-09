
/********************************************************
    
    development started: 2024
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

/*
    task.c backup 

    As the project grows larger, it will be discovered that many areas were previously unreasonable and require modification 
    So create a file backup to modify it

*/
#include "xos_pid.h"

#define TASK_NAME_LEN  128
#define USER_BIN_PATH 256
#define MAX_LOWEST_PRIO   10

struct proc_attr {
    char name[TASK_NAME_LEN];
    struct task_struct* parent;
    int	prio;
    int slice;
    void *user_entry;
    void *data;
    char * const *argv;
    char * const *envp;
};

/*
    kargv
    kenvp 
    信息拷贝存入用户栈

*/
static int init_proc_desc(struct task_struct* tsk,struct proc_attr *param)
{
    char *kstack;
    int  cpuid;
    tsk->sched_flags = 0;
    tsk->cpu_context.pc = (unsigned long)ret_from_fork;
    kstack = (thread_union_t *)xos_get_free_page(0,1);
    tsk->kstack = kstack;
    struct pt_regs * ptr = get_task_pt_regs_new((char*)kstack);
    tsk->cpu_context.sp = (unsigned long)ptr;  //栈顶
    tsk->default_prio = PRIO_ONE;
    tsk->prio = tsk->default_prio+param->prio;
    tsk->timeslice = CONFIG_RR_INTERVAL;
    tsk->timerslice_count = param->slice;
    tsk->user_entry = param->user_entry;
    tsk->pid = alloc_pid();

    tsk->sched_policy = SCHED_RR;
    tsk->state = TSTATE_READY;
    list_init(&tsk->g_list);
    list_init(&tsk->cpu_list);
    list_init(&tsk->sem_list);
    list_init(&tsk->delay_list);
    xos_init_timer(&tsk->timer, 0, NULL, NULL);

//  init_kern_context(tsk, kstack, (unsigned long)param->argv, (unsigned long)param->envp);
    init_kern_context(tsk, kstack);
    init_user_context(tsk, kstack);
    creat_user_mm(tsk);

    
    /*
        如果当前创建任务是ready 状态，加入runqueu run_list 队列
        置位相应核优先级的对应的bitmap,如果多个任务可能存在相同优先级，相同优先级加入同一队列
    */
    space_idx++;
    dup_list_add_after(&tsk->g_list, &task_global_list);

    return 0;
}

static inline struct task_struct *create_task_desc(void)
{
    struct task_struct *tsk;
    tsk = xos_kmalloc(sizeof(struct task_struct));
    if (tsk == NULL)
        return NULL;

    init_proc_desc(tsk);
    return tsk;
}

void init_kern_context(struct task_struct * tsk, unsigned long stack)
{

    memset(&tsk->cpu_context, 0x0, sizeof(struct cpu_context));
    tsk->cpu_context.fp = 0;

    tsk->cpu_context.sp = get_task_pt_regs_new(char* stack);
    tsk->cpu_context.pc = (unsigned long)ret_from_fork;
    tsk->cpu_context.x19 = 0;  /*very important user mode*/
    tsk->cpu_context.x22 = 5097;
    tsk->cpu_context.x23 = 5098;
    tsk->cpu_context.x24 = 5099;
    tsk->cpu_context.x25 = 5100;
    tsk->cpu_context.x26 = 5101;
    tsk->cpu_context.x27 = 5102;
}

void create_user_process_vma(struct task_struct * tsk)
{
    /*
        to do
        elf loadmsg
        dynamic:
        code section
        data section
        bss section
    */

    /*
        fixed stack segment
        fixed heap segment
        fixed envp segment
    */
}


void init_user_context(struct task_struct * tsk, unsigned long stack)
{
    struct pt_regs * pt_regs = get_task_pt_regs_new((char*)stack);
    tsk->kstack = pt_regs;
    pt_regs = tsk->kstack;
    pt_regs->pc = (u64)tsk->user_entry;
    pt_regs->pstate = PSR_MODE_EL0t;
}

int creat_user_mm(struct task_struct * tsk)
{
    tsk->task_pgd = (pgd_t*)xos_get_free_page(0, 1);
    if(!tsk->task_pgd){
        return -EINVAL;
    }

    tsk->mm = (struct mm_struct*)xos_get_free_page(0, 0);
    if(!tsk->mm){
        return -EINVAL;
    }
    create_user_process_vma(tsk);
    
}
static inline int init_proc_context(struct task_struct * tsk, 
    unsigned long argv, unsigned long envp, unsigned long stack)
{
    init_kern_context(tsk, stack);
    init_user_context(tsk, stack);
    return 0;
}

struct task_struct *process_init(struct proc_attr *param)
{
    int cpuid;
    int ret;
    unsigned long flags;
    struct task_struct *tsk;
    union thread_union *stack = NULL;
    cpuid = cur_cpuid();
    
    if (param->prio < 0 || param->prio >= MAX_LOWEST_PRIO)
        goto out;

    tsk = xos_kmalloc(sizeof(struct task_struct));
    if (tsk == NULL)
        goto  kalloc_fail;
    ret = init_proc_desc(tsk,param);
    if(ret < 0){
        goto init_fail;
    }

	/* 利用 param 对 task 赋值 */
    tsk->prio = param->prio;
    strncpy(tsk->t_name, param->name, TASK_NAME_LEN);
    strncpy(tsk->user_path, param->name, USER_BIN_PATH);

    tsk->state = TSTATE_READY;
    
    arch_local_irq_disable();
    add_to_cpu_runqueue(cpuid,tsk);
    arch_local_irq_disable();
    
    return tsk;

init_fail:
    xos_kfree(tsk);
kalloc_fail:
    
	return NULL;
}

/*
    原方案是内核进程创建和用户进程创建使用一个接口
    现在使用两个接口实现,do_sys_execv,调用本接口

    elf_load 加载  获取user_entry 

    do_sys_exec 调用本接口，不需要创建新的task_t 对象
*/
struct task_struct *user_process_create(struct	proc_attr *proc_param)
{
    struct task_struct *ret = NULL;
    /*
        在进程初始化之前需要根据参数信息
    */

    ret = process_init(proc_param);

    return ret;
}

/*
    
    用户进程创建调用本接口，未完成待完善,后续用于替代
    process_create()
    如果我拥有子进程的一切用信息，就可以通过本接口创建子进程
    这个接口其实跟do_fork 存在区别, 本接口是创建加载新进程，而不是
    fork 拷贝
    
*/
int xos_create_process(int (*user_entry)(void *data),
        void *data,
        char *name,
        int prio,
        char * const *kargv,
        char * const *kenvp)
{
    int ret = 0;
    int name_strlen = 0;
    struct task_struct* ret_tsk;
    struct	proc_attr proc_param;
    
    if (!name) {
        ret = -1;
        return ret;
    }
    if (prio < 0 || prio >= MAX_LOWEST_PRIO) {
        ret = -1;
        return ret;
    }
    name_strlen = strlen(name);
    if (name_strlen >= sizeof(proc_param.name)){
        name_strlen = sizeof(proc_param.name) - 1;
    }
    strncpy(proc_param.name, name, name_strlen);
    
    proc_param.name[name_strlen+1] = '\0';
    proc_param.parent = current_task;
    proc_param.prio  = prio;
    proc_param.user_entry = user_entry;
    proc_param.data = data;
    proc_param.argv = kargv;
    proc_param.envp = kenvp;
    
    ret_tsk = user_process_create(&proc_param NULL, NULL);
    if(!ret_tsk){
        return -1;
    }
}

