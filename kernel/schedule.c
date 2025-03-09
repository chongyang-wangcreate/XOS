/*
    development started: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
    
    Copyright (c) 2024 - 2028 wangchongyang

    2024.03.24 AM:1:45
    自己的第一版程序设计，代码编写不会复杂，而是越简单越好，
    操作系统的工作开发量非常大，当前就是先实现功能，系统可以跑
    不会耗费经历在复杂的算法上

*/

#include "types.h"
#include "setup_map.h"
#include "printk.h"
#include "bit_map.h"
#include "list.h"
#include "tick_timer.h"
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
#include "cpu_desc.h"
#include "arch_irq.h"
#include "xos_preempt.h"
#include "mmu.h"
#include "mem_layout.h"
#include "arch64_irq.h"


xos_spinlock_t  g_wakeup_lock = {0};


int nesting = 0;



/*
    自己的第一版程序设计，代码编写不会复杂，而是越简单越好，
    操作系统的工作开发量非常大，当前就是先实现功能，系统可以跑
    不会耗费经历在复杂的算法上
*/

extern dlist_t task_global_list;



extern struct task_struct *load_task(struct task_struct *tsk);
extern struct task_struct *cpu_switch_to(struct task_struct *prev, struct task_struct *next);

/*
    这个问题在公园里思考了两个多小时，
    当前所有核只有一个tick 中断(核心点)
    1.哪些任务的时间片需要更新。 如果一个任务状态不是running 状态时间片显然不需要更新
    2.如果是多核系统，是否其它核上的任务也需要更新， 单核与多核的区别在于单核心不需要遍历，多核需要遍历
     单核每个核上的运行队列都需要遍历

     关于多核多任务，tick 产生之后多核是否都会被中断，这个还需我详细查查

    每个核都有自己独立的tick 中断

    摘自内核：scheduler_tick 函数，每个cpu 都有一个时钟中断，都会被周期的调度到scheduler_tick 函数
    scheduler_tick 主要完成的任务如下：
    1. 更新WALT 统计  2. 更新系统时钟   3.
*/
void handle_task_timerslice(int cpuid)
{
    struct task_struct *cur_tcb = cpu_array[cur_cpuid()].cur_task;
    if(cur_tcb == NULL){
        return ;
    }
    printk(PT_RUN,"%s:%d,cur_tcb_addr=%llx\n\r",__func__,__LINE__,(unsigned long)cur_tcb);
    xos_spinlock(&cpu_array[cur_cpuid()].lock);
    if(cur_tcb->sched_policy == SCHED_RR){
        if(cur_tcb->timerslice_count == 0){
            /*
                补充时间片进行时间片轮转
                优先级不更改，放到当前队列尾部
                 更改任务运行状态由runnning 态转换为ready 态，放到队列尾部
                 中断退出时，重新选取下一个任务进行调度
            */
            printk(PT_RUN,"%s:%d\n\r",__func__,__LINE__);
            cur_tcb->timerslice_count = cur_tcb->timeslice;
            /*
                操作当前cpu 运行队列，将当前任务节点放到运行队列run_list 
                尾部
            */
            list_del(&cur_tcb->cpu_list);
            /*
                加入队列尾部
            */
            list_add_front(&cur_tcb->cpu_list,&cpu_array[cur_cpuid()].runqueue[cur_tcb->prio].run_list);
            /*
                后续增加可抢占标识,在哪些时机设置可抢占标识？
            */

        }else{
            /*
                更改时间片计数
            */
            printk(PT_DEBUG,"%s:%d\n\r",__func__,__LINE__);
            cur_tcb->timerslice_count--;
        }

    }
    xos_unspinlock(&cpu_array[cur_cpuid()].lock);
    
}



void check_exec_preempt()
{

}

void check_exec_signal()
{

}

void start_schedule_tail(struct task_struct *prev)
{

    /*
        开中断，开启抢占，可以被中断，中断之后进入el1_irq
        保存现场寄存器，执行中断处理函数，恢复现场
    */
    arch_local_irq_enable();
    preempt_enable();

    printk(PT_RUN,"%s TAIL TAIL enter \n",__func__);
}

/*
    20241217:PM:22:41
    end_schedule_tail  用户处理收尾工作
    1.是否会被抢占
    2.是否存在pending 信号等待处理
    只有返回用户态态时才会做这个工作
*/

void end_schedule_tail(struct pt_regs *regs)
{
    show_pt_regs(regs);
    printk(PT_RUN,"%s end_schedule_tail \n",__func__);

    check_exec_preempt();
    check_exec_signal();
}

struct task_struct *__switch_to_next(struct task_struct *prev,
        struct task_struct *next)
{
    struct task_struct *last;
    last = cpu_switch_to(prev, next);

    return last;
}


/*********************
    global list

    to do:
    优先级调度策略
    快速选出最高优先级方法空间换时间,查表
    
*********************/
struct task_struct * get_next_task(dlist_t *g_task_list){

    dlist_t *cur_list;
    dlist_t *cur = g_task_list->next;
    struct task_struct *cur_task;
    list_for_each(cur_list, g_task_list){
         cur_task = list_entry(cur,struct task_struct,g_list);
    }

    if(cur != g_task_list){
        
        cur_task = list_entry(cur,struct task_struct,g_list);
        list_del(cur);
        printk(PT_RUN,"%s:%d,cur_task->context->x22 =%llx\n\r",__FUNCTION__,__LINE__,cur_task->cpu_context.x22);
        return cur_task;
    }
   return NULL;

   
}


struct task_struct * get_next_task_from_cpu(int cpuid)
{
    /*
       
        优先级调度，查找最高优先级任务
        同优先级会支持RR 调度，轮转，time_slice 如果为0 则放到同优先级的队列队尾

        后期非实时任务会引入动态优先级调整，实时任务优先级不会进行调整
        
        别忘的共享资源一定要加锁

        当前的调度首先从查找就绪的最高优先级任务，然后从当前优先级当中找到某个就绪任务

        需要优先级Bitmap 表，0 表示当前优先级没有任务就绪，1 表示当前优先级任务就绪

        当前只引入32个优先级，所以4字节即可全部覆盖，当前的优先级查找还是比较简单，并且优先级数字越小
        优先级越高
    */

      int prio_max_num;
      dlist_t *node;
      struct task_struct *cur_task;
   
    /*
        有没有考虑过，如果当前找不到合适的优先级，那么怎么办
        所以必须加入idle 任务，用于待命处于ready态，就是优先级总是被别人抢占
    */
    prio_max_num =  find_left_set_bit(&cpu_array[cpuid].run_bitmap);
    printk(PT_RUN,"%s:%d prio_max_num=%d\n\r",__FUNCTION__,__LINE__,prio_max_num);
      /*
        从run_list 获取ready 任务更改状态为running
      */

      node = list_get_first_node(&cpu_array[cpuid].runqueue[prio_max_num].run_list);
      if(node != NULL){
            cur_task = list_entry(node,struct task_struct,cpu_list);
            printk(PT_RUN,"%s:%d,cur_task->prio=%d\n\r",__FUNCTION__,__LINE__,cur_task->prio);
            return cur_task;
      }
     
      return NULL;
      
}
/*
    调度期间不能被打断，所以必须关闭中断，开始未关闭中断，出现莫名其妙错误
2024。04.05 AM:6:42
    schedule 功能不完善
    1. 没有判断 是在那个上下文调度
    2. 没有加锁
    3.只考虑的上下文切换，没有考虑地址空间切换，当前值考虑内核态任务切换 2024.0405 10:24
    需要后续慢慢完善，当前最要紧的是关闭中断

20240520 AM:12:25 要重新设计调度器，当前调度器存在问题，不再使用bitmap 
引入多链表

关中断加锁   开开中断解锁
    2025.01.24. 后续支持256优先级调度和cfs 调度，可配置

    2025.02.14：22：14
    fs_context 未做基本处理，切换新进程运行后fs_context 可能无效
    新进程需要fs_context 有效

*/
void schedule(void)
{
    /*
        必须加锁，保证对临界区的唯一操作
    */
    int cpuid;
//  unsigned long flags;
    cpuid = cur_cpuid();
    struct task_struct *prev;
    struct task_struct *next;
    /*
        如果不允许抢占直接返回
    */
    if(preempt_is_disabled()){
        return;
    }
    arch_local_irq_disable();
//  flags = arch_local_irq_save();
//  next = get_next_task(&task_global_list);
    next = get_next_task_from_cpu(cpuid);
    
    printk(PT_RUN,"%s: next->cpu_context.x19=%lx\n\r",__FUNCTION__,next->cpu_context.x19);
    printk(PT_RUN,"%s: next->cpu_context.x21=%lx\n\r",__FUNCTION__,next->cpu_context.x21);
    prev = get_current_task();
    cpu_array[cpuid].cur_task = next;
    printk(PT_RUN,"%s: next->cpu_context.pc=%lx\n\r",__FUNCTION__,prev->cpu_context.pc);
    if(next->task_pgd != NULL)
    {
        set_ttbr0_el1((u64)V2P(next->task_pgd));
    }
    switch_to_next(prev,next,prev);
    arch_local_irq_enable();
}

/*
    中断上下文调用
*/
void int_schedule(void)
{

    int cpuid;
    cpuid = cur_cpuid();
    struct task_struct *prev;
    struct task_struct *next;
    nesting = 1;
    
//  next = get_next_task(&task_global_list);
    next = get_next_task_from_cpu(cpuid);
    
    printk(PT_RUN,"%s: next->cpu_context.x19=%lx\n\r",__FUNCTION__,next->cpu_context.x19);
    printk(PT_RUN,"%s: next->cpu_context.x21=%lx\n\r",__FUNCTION__,next->cpu_context.x21);
    prev = get_current_task();
    cpu_array[cpuid].cur_task = next;
    printk(PT_RUN,"%s: next->cpu_context.pc=%lx\n\r",__FUNCTION__,prev->cpu_context.pc);
    if(next->task_pgd != NULL)
    {
        set_ttbr0_el1((u64)V2P(next->task_pgd));
    }
    if(next != prev)
    switch_to_next(prev,next,prev);
    nesting = 0;
}



cpu_desc_t *scheduler_get_cur_runqueue()
{   
    int cpuid;
    cpuid = cur_cpuid();
    if(cpuid < CPU_NR){
        return &cpu_array[cpuid];
    }
    return NULL;
}

void load_first_task()
{
    
    int cpuid;
    struct task_struct *tsk = NULL;
    cpuid = cur_cpuid();
    printk(PT_DEBUG,"%s:%d,LLLLLLLL\n\r",__func__,__LINE__);
    tsk = get_next_task(&task_global_list);
    tsk = get_next_task_from_cpu(cpuid);
    printk(PT_DEBUG,"%s:%d,tsk->prio=%d\n\r",__func__,__LINE__,tsk->prio);

    cpu_array[cpuid].cur_task = tsk;
    xos_sti();
    load_task(tsk);
}



int wake_up_proc(struct task_struct *tsk)
{
    /*
        check process status
    */
    int cpuid;
    cpuid = cur_cpuid();
    xos_spinlock(&g_wakeup_lock);
    if((tsk->state == TSTATE_PENDING)||(tsk->state == TSTATE_SUSPEND)||(tsk->state == TSTATE_STOP)){

        tsk->state = TSTATE_READY;
        add_to_cpu_runqueue(cpuid, tsk);

    }else{
        xos_unspinlock(&g_wakeup_lock);
        return -1;
    }
    xos_unspinlock(&g_wakeup_lock);
    return 0;
}


void sched_scheme_select()
{
    /*
        1. support  RT
          1.1 .Supports up to 128 priority tasks
        2. support cfs
    */

}



