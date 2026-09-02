/********************************************************
    
    development start:2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
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
#include "setup_map.h"
#include "string.h"
#include "tick_timer.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "fs.h"
#include "xos_cache.h"
#include "xdentry.h"
#include "mount.h"
#include "task.h"
#include "schedule.h"
#include "fork.h"
#include "printk.h"
#include "cpu_desc.h"
#include "arch_irq.h"
#include "xos_page.h"



#define CONFIG_DEFAULT_TASK_PRIORITY  10
#define CONFIG_RR_INTERVAL 5

#define NO_REAL_PRIO_MAX 32

int load_proc_flags = 0;
dlist_t task_global_list;
dlist_t pend_global_list;


char task_space[3][4096] = {0};


static cpu_desc_t *task_cpu_rq(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR){
        return NULL;
    }
    return &cpu_array[cpuid];
}

static int task_prio_valid(struct task_struct *task)
{
    return task != NULL && task->prio < PRIO_MAX;
}

static void rq_update_preempt(struct task_struct *task)
{
    struct task_struct *curr = current_task;
    if(get_load_flags() != 0 && curr != NULL && task->prio < curr->prio){
        curr->need_switch = 1;
    }
}

void init_fs_context(struct task_struct *parent, struct task_struct *child)
{
    if(!parent){
        memset(&child->fs_context, 0 , sizeof(child->fs_context));
        xos_spinlock_init(&child->fs_context.lock);
    }else{
        memcpy(&child->fs_context,&parent->fs_context,sizeof(parent->fs_context));
        xos_spinlock_init(&child->fs_context.lock);
    }
}


void add_to_g_list(struct task_struct *task)
{
    list_add_back(&task->g_list, &task_global_list);
}

void add_to_cpu_runqueue(int cpuid,struct task_struct *task)
{
    struct task_struct *curr = NULL;
    const sched_class_t *class = task_get_sched_class(task);
    cpu_desc_t *rq = task_cpu_rq(cpuid);

    if(rq == NULL || class == NULL || class->ops == NULL || class->ops->enqueue_task == NULL || !task_prio_valid(task)){
        return;
    }

    task->cpuid = cpuid;
    task->task_cpunum = cpuid;
    class->ops->enqueue_task(rq,task);
    rq_update_preempt(task);
    return;

    runque_t *cpu_runque;
    cpu_runque = &cpu_array[cpuid].runqueue[task->prio];
    if(task->prio < PRIO_MAX){
        cpu_array[cpuid].run_count[task->prio]++;
        //printk(PT_RUN,"%s:%d  task->prio=%d\n\r",__FUNCTION__,__LINE__,task->prio);
        if(cpu_array[cpuid].run_count[task->prio] <= 1){
           // printk(PT_RUN,"%s:%d task->prio=%d\n\r",__FUNCTION__,__LINE__,task->prio);
            set_bit((uint8_t*)(cpu_array[cpuid].run_bitmap.bit_start), task->prio); /*2024.0404 20.12还需要增加同优先级优先级统计计数*/
        }
        if(get_load_flags() != 0){
            curr = current_task;
            if(curr != NULL && task->prio < curr->prio){
                /*
                    设置调度标志
                */
               // printk(PT_RUN,"%s:%d,task->prio=%d,current_task->prio=%d\n\r",__FUNCTION__,__LINE__,task->prio,current_task->prio);
                curr->need_switch = 1;
            }
        }
        list_add_back(&task->cpu_list,&cpu_runque->run_list);
    }
    
}

void del_from_cpu_runqueue(int cpuid,struct task_struct *task)
{
    const sched_class_t *class = task_get_sched_class(task);
    cpu_desc_t *rq = task_cpu_rq(cpuid);
    unsigned long flags;

    if(rq == NULL || class == NULL || class->ops == NULL || class->ops->dequeue_task == NULL || !task_prio_valid(task)){
        return;
    }

    flags = arch_local_irq_save();
    printk(PT_RUN,"%s:%d,cur_task->prio=%d,run_cnt=%d\n\r",__func__,__LINE__,task->prio,rq->run_count[task->prio]);
    class->ops->dequeue_task(rq,task);
    arch_local_irq_restore(flags);
    return;

    int run_cnt;
    arch_local_irq_disable();
    list_del(&task->cpu_list);
    run_cnt = cpu_array[cur_cpuid()].run_count[task->prio];
    printk(PT_RUN,"%s:%d,cur_task->prio=%d,run_cnt=%d\n\r",__func__,__LINE__,task->prio,run_cnt);
    cpu_array[cur_cpuid()].run_count[task->prio]--;
    if(cpu_array[cur_cpuid()].run_count[task->prio] == 0){
        clear_bit((uint8_t*)(cpu_array[cur_cpuid()].run_bitmap.bit_start), task->prio);
    }
    arch_local_irq_enable();
    
}


void dup_list_add_after(dlist_t *new_node,dlist_t *head)
{
    new_node->prev = head;
    new_node->next = head->prev;
    new_node->prev->next = new_node;
    new_node->next->prev = new_node;
}

void dup_list_init(dlist_t *head)
{
    head->next = head;
    head->prev = head;
}

void xos_kernel_exit()
{
    /*
        to do
    */
}

void xos_kernel_entry(void *arg)
{
    struct task_struct *task = arg;
    task->tsk_entry(arg);
    /*
        执行完毕后，进行善后工作
    */
    xos_kernel_exit();
}

int space_idx = -0;
/*
    20240519 PM 17:21 发现自己的任务存在问题，每次还会从ret_from_fork 开始运行
    即使是多次运行后也存在这样问题。

    如果不使用thread_info 坏处就是task_struct 占用的空间非常大，确实非常不好

|-----------|statck_high
|-----------|        想画一个清晰的视图帮助大家理解，但是太不好画了，后续再完善图示
|___________|
|--------   |
|--------   |
|task struct|
|--------   |
|___________|stack_low

|-----------|stack_high        
|    free   |
|           |
|           |
|           |
|           |
|———————————|
|           |
|thread_info|
|———————————| stack_low
*/


/*
    20240608：PM:19:24
    stack  和task_struct 地址空间分离

    不管是内存态线程，还是用户态线程，在堆栈区域的最高地址(我先不用栈顶，栈底来描述，以免有的人混乱)
    都要开辟一块sizeof(pt_regs) 这么大区域，
*/
int xos_thread_create(unsigned int prio, unsigned long fn, unsigned long arg)
{

    int cpuid;
    uint64 flags;
    thread_union_t *stack;
    cpuid = cur_cpuid();
//    struct task_struct *child = (struct task_struct *)alloc_page();
    
    struct task_struct *child = (struct task_struct *)xos_get_free_page(0,2);
    if(child == NULL){
        printk(PT_ERROR,"%s:%d, child alloc failed\n\r",__func__,__LINE__);
    }
    stack = (thread_union_t *)xos_get_free_page(0,1);
    if(child == NULL || stack == NULL){
        goto alloc_stack_faild;
    }
    memset(child,0,sizeof(*child));
    memset(stack,0,sizeof(*stack));
    stack->thread_val.p_task = child;
#ifdef old
    struct task_struct *child = xos_get_kern_page();
    struct task_struct *child = (struct task_struct *)task_space[space_idx];
    struct pt_regs * ptr = get_task_pt_regs(child);  // 栈顶sp 指向位置
    child->cpu_context.sp = (unsigned long)((char*)child->kstack+4096);  //栈顶
    child->kstack = xos_get_kern_page();
#endif
    struct pt_regs * ptr = get_task_pt_regs_new((char*)stack);
    memset(ptr,0, sizeof(struct pt_regs));
    memset(&child->cpu_context, 0,sizeof(struct cpu_context));
    child->kstack = stack;

    /*
        寄存器的初始化配置需要遵循规范
    */
    child->tsk_entry = (task_fun)fn;
    child->cpu_context.x19 = (unsigned long)xos_kernel_entry;
    child->cpu_context.x20 = (unsigned long)child;
    //test
    child->cpu_context.x21 = (unsigned long)child;
    child->cpu_context.x22 = 4097;
    child->cpu_context.x23 = 4098;
    child->cpu_context.x24 = 4099;
    child->cpu_context.x25 = 4100;
    child->cpu_context.x26 = 4101;
    child->cpu_context.x27 = 4102;
   // printk(PT_RUN,"%s:%d,fn=%llx\n\r",__FUNCTION__,__LINE__,fn);
   // printk(PT_RUN,"%s:%d,x21=%llx\n\r",__FUNCTION__,__LINE__,child->cpu_context.x21);
   
    child->sched_flags = 0;
    child->cpu_context.pc = (unsigned long)ret_from_fork;
    child->cpu_context.sp = (unsigned long)ptr;  //栈顶
    child->default_prio = PRIO_ONE;
    child->prio = child->default_prio+prio;
    child->timeslice = CONFIG_RR_INTERVAL;
    child->timerslice_count = child->timeslice;

    child->sched_policy = SCHED_RR;
    task_refresh_sched_class(child);
    child->state = TSTATE_READY;
    list_init(&child->g_list);
    list_init(&child->cpu_list);
    list_init(&child->sem_list);
    list_init(&child->delay_list);
    list_init(&child->wait_list);
    list_init(&child->mutex_list);
    init_fs_context(NULL, child);
    memset(&child->files_set.fd_set,0,sizeof(child->files_set.fd_set));
    child->files_set.fd_map.bit_start = (uint8_t*)child->files_set.fd_set;
    child->files_set.fd_map.btmp_bytes_len = sizeof(child->files_set.fd_set);
    xos_spinlock_init(&child->files_set.file_lock);
    
    xos_init_timer(&child->timer, 0, NULL, NULL);
    
    /*
        如果当前创建任务是ready 状态，加入runqueu run_list 队列

        置位相应核优先级的对应的bitmap,如果多个任务可能存在相同优先级，相同优先级加入同一队列
    */
    space_idx++;
    dup_list_add_after(&child->g_list, &task_global_list);

    flags = arch_local_irq_save();
    add_to_cpu_runqueue(cpuid,child);
    arch_local_irq_restore(flags);

//	add_to_g_list(child);
alloc_stack_faild:

    return 0;
}

/*int xos_idle_thread_create(unsigned long fn, unsigned long arg)
{
    int cpuid = cur_cpuid();
    thread_union_t *stack;
    struct task_struct *idle;
    struct pt_regs *ptr;

    idle = (struct task_struct *)xos_get_free_page(0,2);
    stack = (thread_union_t *)xos_get_free_page(0,1);
    if(idle == NULL || stack == NULL){
        return -1;
    }

    memset(idle,0,sizeof(*idle));
    memset(stack,0,sizeof(*stack));
    stack->thread_val.p_task = idle;

    ptr = get_task_pt_regs_new((char*)stack);
    memset(ptr,0, sizeof(struct pt_regs));
    memset(&idle->cpu_context, 0,sizeof(struct cpu_context));

    idle->kstack = stack;
    idle->tsk_entry = (task_fun)fn;
    idle->cpu_context.x19 = (unsigned long)xos_kernel_entry;
    idle->cpu_context.x20 = (unsigned long)idle;
    idle->cpu_context.x21 = (unsigned long)idle;
    idle->cpu_context.pc = (unsigned long)ret_from_fork;
    idle->cpu_context.sp = (unsigned long)ptr;
    idle->default_prio = PRIO_MAX - 1;
    idle->prio = PRIO_MAX - 1;
    idle->timeslice = 0;
    idle->timerslice_count = 0;
    idle->sched_policy = SCHED_IDLE;
    task_refresh_sched_class(idle);
    idle->state = TSTATE_READY;
    idle->cpuid = cpuid;
    idle->task_cpunum = cpuid;

    list_init(&idle->g_list);
    list_init(&idle->cpu_list);
    list_init(&idle->sem_list);
    list_init(&idle->delay_list);
    list_init(&idle->wait_list);
    list_init(&idle->mutex_list);
    init_fs_context(current_task, idle);
    memset(&idle->files_set.fd_set,0,sizeof(idle->files_set.fd_set));
    idle->files_set.fd_map.bit_start = (uint8_t*)idle->files_set.fd_set;
    idle->files_set.fd_map.btmp_bytes_len = sizeof(idle->files_set.fd_set);
    xos_spinlock_init(&idle->files_set.file_lock);
    xos_init_timer(&idle->timer, 0, NULL, NULL);

    dup_list_add_after(&idle->g_list, &task_global_list);
    cpu_array[cpuid].idle_task = idle;
    return 0;
}*/

struct task_struct *get_current_task(void)
{
    int cpuid;
    cpuid = cur_cpuid();
    if(cpuid < 0 || cpuid >= CPU_NR){
        return NULL;
    }
    return cpu_array[cpuid].cur_task;

}

int get_task_status()
{
    return 0;
}



struct pt_regs * get_task_pt_regs(struct task_struct * task)
{
    return (struct pt_regs *)((unsigned long)task + THREAD_SIZE - sizeof(struct pt_regs));
}

struct pt_regs * get_task_pt_regs_new(char* stack)
{
    return (struct pt_regs *)((unsigned long)stack + THREAD_SIZE - sizeof(struct pt_regs));
}


int do_sys_getpid()
{
    return current_task->pid;
}


int do_sys_getppid()
{
    return current_task->ppid;
}

int do_sys_gettgid()
{
    return current_task->tgid;
}
