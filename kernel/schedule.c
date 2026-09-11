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
#include "barriers.h"


xos_spinlock_t  g_wakeup_lock = {0};


int nesting = 0;



/*
    自己的第一版程序设计，代码编写不会复杂，而是越简单越好，
    操作系统的工作开发量非常大，当前就是先实现功能，系统可以跑
    不会耗费经历在复杂的算法上

    2026.8.28 22:04: Add scheduling classes, and first implement the basic framework
*/

extern dlist_t task_global_list;



extern struct task_struct *load_task(struct task_struct *tsk);
extern struct task_struct *cpu_switch_to(struct task_struct *prev, struct task_struct *next);

static void sched_queue_enqueue(bitmap_t *bitmap, runque_t runqueue[], int run_count[], struct task_struct *task)
{
    uint32_t prio = task->prio;

    run_count[prio]++;
    if(run_count[prio] <= 1){
        set_bit((uint8_t*)(bitmap->bit_start), prio);
    }
    list_add_back(&task->cpu_list,&runqueue[prio].run_list);
}

static void sched_queue_dequeue(bitmap_t *bitmap, int run_count[], struct task_struct *task)
{
    uint32_t prio = task->prio;

    if(run_count[prio] <= 0){
        return;
    }

    list_del(&task->cpu_list);
    run_count[prio]--;
    if(run_count[prio] == 0){
        clear_bit((uint8_t*)(bitmap->bit_start), prio);
    }
}

static struct task_struct *sched_queue_pick_next(bitmap_t *bitmap, runque_t runqueue[])
{
    int prio_max_num;
    dlist_t *node;

    prio_max_num = find_left_set_bit(bitmap);
    if(prio_max_num < 0 || prio_max_num >= PRIO_MAX){
        return NULL;
    }

    node = list_get_first_node(&runqueue[prio_max_num].run_list);
    if(node == NULL){
        return NULL;
    }

    return list_entry(node,struct task_struct,cpu_list);
}

static void sched_queue_rr_tick(runque_t runqueue[], struct task_struct *curr)
{
    if(curr == NULL){
        return;
    }

    if(curr->sched_policy != SCHED_RR){
        return;
    }

    if(curr->timerslice_count > 0){
        curr->timerslice_count--;
    }

    if(curr->timerslice_count == 0){
        curr->timerslice_count = curr->timeslice;
        list_del(&curr->cpu_list);
        list_add_front(&curr->cpu_list, &runqueue[curr->prio].run_list);
        curr->need_switch = 1;
    }
}

static void rt_enqueue_task(cpu_desc_t *rq, struct task_struct *task)
{
    sched_queue_enqueue(&rq->rt_run_bitmap, rq->rt_runqueue, rq->rt_run_count, task);
}

static void rt_dequeue_task(cpu_desc_t *rq, struct task_struct *task)
{
    sched_queue_dequeue(&rq->rt_run_bitmap, rq->rt_run_count, task);
}

static struct task_struct *rt_pick_next_task(cpu_desc_t *rq)
{
    if(rq == NULL){
        return NULL;
    }
    return sched_queue_pick_next(&rq->rt_run_bitmap, rq->rt_runqueue);
}

static void rt_task_tick(cpu_desc_t *rq, struct task_struct *curr)
{

}

static void normal_enqueue_task(cpu_desc_t *rq, struct task_struct *task)
{
    sched_queue_enqueue(&rq->run_bitmap, rq->runqueue, rq->run_count, task);
}

static void normal_dequeue_task(cpu_desc_t *rq, struct task_struct *task)
{
    sched_queue_dequeue(&rq->run_bitmap, rq->run_count, task);
}

static struct task_struct *normal_pick_next_task(cpu_desc_t *rq)
{
    if(rq == NULL){
        return NULL;
    }
    return sched_queue_pick_next(&rq->run_bitmap, rq->runqueue);
}

static void normal_task_tick(cpu_desc_t *rq, struct task_struct *curr)
{
    if(rq == NULL){
        return;
    }
    sched_queue_rr_tick(rq->runqueue, curr);
}

static void idle_enqueue_task(cpu_desc_t *rq, struct task_struct *task)
{

}

static void idle_dequeue_task(cpu_desc_t *rq, struct task_struct *task)
{

}

static struct task_struct *idle_pick_next_task(cpu_desc_t *rq)
{
    if(rq == NULL){
        return NULL;
    }
    return rq->idle_task;
}

static void idle_task_tick(cpu_desc_t *rq, struct task_struct *curr)
{

}

static const sched_class_ops_t rt_sched_ops = {
    .enqueue_task = rt_enqueue_task,
    .dequeue_task = rt_dequeue_task,
    .pick_next_task = rt_pick_next_task,
    .task_tick = rt_task_tick,
};

static const sched_class_ops_t normal_sched_ops = {
    .enqueue_task = normal_enqueue_task,
    .dequeue_task = normal_dequeue_task,
    .pick_next_task = normal_pick_next_task,
    .task_tick = normal_task_tick,
};

const sched_class_t xos_normal_sched_class = {
    .name = "normal",
    .ops = &normal_sched_ops,
    .next = &xos_idle_sched_class,
};

const sched_class_t xos_rt_sched_class = {
    .name = "rt",
    .ops = &rt_sched_ops,
    .next = &xos_normal_sched_class,
};

static const sched_class_ops_t rr_sched_ops = {
    .enqueue_task = normal_enqueue_task,
    .dequeue_task = normal_dequeue_task,
    .pick_next_task = normal_pick_next_task,
    .task_tick = normal_task_tick,
};

const sched_class_t xos_rr_sched_class = {
    .name = "rr",
    .ops = &rr_sched_ops,
    .next = NULL,
};

static const sched_class_ops_t idle_sched_ops = {
    .enqueue_task = idle_enqueue_task,
    .dequeue_task = idle_dequeue_task,
    .pick_next_task = idle_pick_next_task,
    .task_tick = idle_task_tick,
};

const sched_class_t xos_idle_sched_class = {
    .name = "idle",
    .ops = &idle_sched_ops,
    .next = NULL,
};

static const sched_class_t *xos_sched_class_root = &xos_rt_sched_class;

static cpu_desc_t *sched_cpu_rq(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR || !cpu_array[cpuid].possible){
        return NULL;
    }
    return &cpu_array[cpuid];
}

static int sched_rq_load(cpu_desc_t *rq)
{
    int load = 0;
    int prio;

    if(rq == NULL){
        return 0;
    }

    for(prio = 0; prio < PRIO_MAX; prio++){
        load += rq->rt_run_count[prio];
        load += rq->run_count[prio];
    }
    return load;
}

static int sched_balance_pair(int src_cpuid, int dst_cpuid);

static uint64 sched_online_cpu_mask(void)
{
    uint64 mask = 0;
    int cpuid;

    for(cpuid = 0; cpuid < xos_cpu_possible_count(); cpuid++){
        if(cpu_array[cpuid].possible && cpu_array[cpuid].cpu_online){
            mask |= 1ULL << cpuid;
        }
    }
    return mask;
}

static int sched_task_cpu_allowed(const struct task_struct *task, int cpuid)
{
    uint64 allowed;

    if(task == NULL || cpuid < 0 || cpuid >= CPU_NR){
        return 0;
    }
    allowed = task->cpus_allowed;
    if(allowed == 0){
        allowed = sched_online_cpu_mask();
    }
    return (allowed & (1ULL << cpuid)) != 0;
}

int sched_select_cpu(int preferred_cpuid)
{
    int best_cpu = -1;
    int best_load = 0x7fffffff;
    int possible = xos_cpu_possible_count();
    int cpuid;

    if(preferred_cpuid >= 0 && preferred_cpuid < possible &&
       cpu_array[preferred_cpuid].possible &&
       cpu_array[preferred_cpuid].cpu_online){
        best_cpu = preferred_cpuid;
    }

    for(cpuid = 0; cpuid < possible; cpuid++){
        cpu_desc_t *rq;
        int load;

        if(!cpu_array[cpuid].possible || !cpu_array[cpuid].cpu_online){
            continue;
        }

        rq = &cpu_array[cpuid];
        xos_spinlock(&rq->lock);
        load = sched_rq_load(rq);
        xos_unspinlock(&rq->lock);

        if(load < best_load ||
           (load == best_load && cpuid == preferred_cpuid)){
            best_cpu = cpuid;
            best_load = load;
        }
    }
    return best_cpu;
}

static int sched_cpu_load(int cpuid)
{
    cpu_desc_t *rq;
    int load;

    rq = sched_cpu_rq(cpuid);
    if(rq == NULL || !rq->cpu_online){
        return -1;
    }

    xos_spinlock(&rq->lock);
    load = sched_rq_load(rq);
    xos_unspinlock(&rq->lock);
    return load;
}

int sched_periodic_balance(void)
{
    int busiest = -1;
    int idlest = -1;
    int busiest_load = -1;
    int idlest_load = 0x7fffffff;
    int cpuid;
    int possible = xos_cpu_possible_count();

    for(cpuid = 0; cpuid < possible; cpuid++){
        int load;

        load = sched_cpu_load(cpuid);
        if(load < 0){
            continue;
        }
        if(load > busiest_load){
            busiest_load = load;
            busiest = cpuid;
        }
        if(load < idlest_load){
            idlest_load = load;
            idlest = cpuid;
        }
    }

    if(busiest < 0 || idlest < 0 || busiest == idlest ||
       busiest_load <= idlest_load + 1){
        return 0;
    }

    return sched_balance_pair(busiest, idlest);
}

int sched_select_task_cpu(const struct task_struct *task,
                          int preferred_cpuid)
{
    int best_cpu = -1;
    int best_load = 0x7fffffff;
    int possible = xos_cpu_possible_count();
    int cpuid;

    for(cpuid = 0; cpuid < possible; cpuid++){
        cpu_desc_t *rq;
        int load;

        if(!cpu_array[cpuid].possible || !cpu_array[cpuid].cpu_online ||
           !sched_task_cpu_allowed(task, cpuid)){
            continue;
        }

        rq = &cpu_array[cpuid];
        xos_spinlock(&rq->lock);
        load = sched_rq_load(rq);
        xos_unspinlock(&rq->lock);

        if(load < best_load ||
           (load == best_load && cpuid == preferred_cpuid)){
            best_cpu = cpuid;
            best_load = load;
        }
    }
    return best_cpu;
}

static struct task_struct *sched_find_migratable(cpu_desc_t *rq,
                                                 int dst_cpuid)
{
    dlist_t *head;
    dlist_t *node;
    int prio;

    for(prio = 0; prio < PRIO_MAX; prio++){
        head = &rq->runqueue[prio].run_list;
        list_for_each(node, head){
            struct task_struct *task =
                list_entry(node, struct task_struct, cpu_list);
            if(task != rq->cur_task && task->state == TSTATE_READY &&
               !task->on_cpu && !task->migration_disabled &&
               !(task->sched_flags & TASK_SCHED_PINNED) &&
               sched_task_cpu_allowed(task, dst_cpuid)){
                return task;
            }
        }
    }
    return NULL;
}

static int sched_balance_pair(int src_cpuid, int dst_cpuid)
{
    cpu_desc_t *first;
    cpu_desc_t *second;
    cpu_desc_t *src;
    cpu_desc_t *dst;
    struct task_struct *task;
    const sched_class_t *class;
    unsigned long flags;

    if(src_cpuid == dst_cpuid){
        return 0;
    }

    src = sched_cpu_rq(src_cpuid);
    dst = sched_cpu_rq(dst_cpuid);
    if(src == NULL || dst == NULL || !src->cpu_online || !dst->cpu_online){
        return 0;
    }

    first = src_cpuid < dst_cpuid ? src : dst;
    second = src_cpuid < dst_cpuid ? dst : src;
    flags = arch_local_irq_save();
    xos_spinlock(&first->lock);
    xos_spinlock(&second->lock);

    if(sched_rq_load(src) <= sched_rq_load(dst) + 1){
        xos_unspinlock(&second->lock);
        xos_unspinlock(&first->lock);
        arch_local_irq_restore(flags);
        return 0;
    }

    task = sched_find_migratable(src, dst_cpuid);
    if(task == NULL){
        xos_unspinlock(&second->lock);
        xos_unspinlock(&first->lock);
        arch_local_irq_restore(flags);
        return 0;
    }

    class = task_get_sched_class(task);
    class->ops->dequeue_task(src, task);
    task->cpuid = dst_cpuid;
    task->task_cpunum = dst_cpuid;
    class->ops->enqueue_task(dst, task);

    xos_unspinlock(&second->lock);
    xos_unspinlock(&first->lock);
    arch_local_irq_restore(flags);
    xos_smp_send_reschedule(dst_cpuid);
    return 1;
}

int sched_balance_idle_cpu(int cpuid)
{
    int possible = xos_cpu_possible_count();
    int src_cpuid;

    for(src_cpuid = 0; src_cpuid < possible; src_cpuid++){
        if(sched_balance_pair(src_cpuid, cpuid)){
            return 1;
        }
    }
    return 0;
}

int sched_migrate_ready_task(struct task_struct *task, int dst_cpuid)
{
    cpu_desc_t *first;
    cpu_desc_t *second;
    cpu_desc_t *src;
    cpu_desc_t *dst;
    const sched_class_t *class;
    unsigned long flags;
    int src_cpuid;
    int ret = -1;

    if(task == NULL || dst_cpuid < 0 ||
       dst_cpuid >= xos_cpu_possible_count() ||
       !cpu_array[dst_cpuid].possible || !cpu_array[dst_cpuid].cpu_online ||
       !sched_task_cpu_allowed(task, dst_cpuid)){
        return -1;
    }

    src_cpuid = task->cpuid;
    if(src_cpuid == dst_cpuid){
        return 0;
    }
    if(src_cpuid < 0 || src_cpuid >= xos_cpu_possible_count()){
        return -1;
    }

    src = &cpu_array[src_cpuid];
    dst = &cpu_array[dst_cpuid];
    first = src_cpuid < dst_cpuid ? src : dst;
    second = src_cpuid < dst_cpuid ? dst : src;
    flags = arch_local_irq_save();
    xos_spinlock(&first->lock);
    xos_spinlock(&second->lock);

    if(task->state != TSTATE_READY || task->on_cpu ||
       task->migration_disabled ||
       (task->sched_flags & TASK_SCHED_PINNED) ||
       src->cur_task == task){
        goto out;
    }

    class = task_get_sched_class(task);
    if(class == NULL || class->ops == NULL ||
       class->ops->dequeue_task == NULL ||
       class->ops->enqueue_task == NULL){
        goto out;
    }

    task->migration_pending = 1;
    class->ops->dequeue_task(src, task);
    task->cpuid = dst_cpuid;
    task->task_cpunum = dst_cpuid;
    class->ops->enqueue_task(dst, task);
    task->migration_pending = 0;
    ret = 0;
out:
    xos_unspinlock(&second->lock);
    xos_unspinlock(&first->lock);
    arch_local_irq_restore(flags);

    if(ret == 0){
        xos_smp_send_reschedule(dst_cpuid);
    }
    return ret;
}

int sched_set_task_affinity(struct task_struct *task, uint64 cpu_mask)
{
    cpu_desc_t *rq;
    unsigned long flags;
    uint64 possible_mask;
    int need_reschedule = 0;
    int migrate_now = 0;
    int dst_cpuid;

    if(task == NULL || cpu_mask == 0){
        return -1;
    }

    possible_mask = (1ULL << xos_cpu_possible_count()) - 1;
    cpu_mask &= possible_mask;
    if(cpu_mask == 0){
        return -1;
    }

    rq = sched_cpu_rq(task->cpuid);
    if(rq == NULL){
        return -1;
    }

    flags = arch_local_irq_save();
    xos_spinlock(&rq->lock);
    task->cpus_allowed = cpu_mask;
    task->sched_flags &= ~TASK_SCHED_PINNED;
    if(sched_task_cpu_allowed(task, task->cpuid)){
        task->migration_pending = 0;
        xos_unspinlock(&rq->lock);
        arch_local_irq_restore(flags);
        return 0;
    }

    if(task->state == TSTATE_READY && !task->on_cpu){
        migrate_now = 1;
    }else{
        task->migration_pending = 1;
        if(task->on_cpu){
            task->need_switch = 1;
            need_reschedule = 1;
        }
    }
    xos_unspinlock(&rq->lock);
    arch_local_irq_restore(flags);

    dst_cpuid = sched_select_task_cpu(task, task->cpuid);
    if(migrate_now){
        return sched_migrate_ready_task(task, dst_cpuid);
    }
    if(need_reschedule && dst_cpuid >= 0){
        xos_smp_send_reschedule(task->cpuid);
    }
    return 0;
}

void sched_migrate_disable(void)
{
    if(current_task != NULL){
        current_task->migration_disabled++;
    }
}

void sched_migrate_enable(void)
{
    if(current_task != NULL && current_task->migration_disabled > 0){
        current_task->migration_disabled--;
        if(current_task->migration_disabled == 0 &&
           current_task->migration_pending){
            current_task->need_switch = 1;
        }
    }
}

static struct task_struct *sched_pick_next_task(cpu_desc_t *rq)
{
    const sched_class_t *class;
    struct task_struct *task;

    if(rq == NULL){
        return NULL;
    }

    xos_spinlock(&rq->lock);
    for(class = xos_sched_class_root; class != NULL; class = class->next){
        if(class->ops == NULL || class->ops->pick_next_task == NULL){
            continue;
        }
        task = class->ops->pick_next_task(rq);
        if(task != NULL){
            task->state = TSTATE_RUNNING;
            task->on_cpu = 1;
            rq->cur_task = task;
            xos_unspinlock(&rq->lock);
            return task;
        }
    }
    xos_unspinlock(&rq->lock);

    return NULL;
}

static void finish_task_switch(struct task_struct *prev)
{
    cpu_desc_t *rq;
    int migration_pending;
    int dst_cpuid;

    if(prev == NULL || prev == current_task){
        return;
    }

    rq = sched_cpu_rq(prev->cpuid);
    if(rq == NULL){
        return;
    }

    xos_spinlock(&rq->lock);
    prev->on_cpu = 0;
    if(prev->state == TSTATE_RUNNING){
        prev->state = TSTATE_READY;
    }
    migration_pending = prev->migration_pending;
    xos_unspinlock(&rq->lock);

    if(migration_pending && prev->state == TSTATE_READY){
        dst_cpuid = sched_select_task_cpu(prev, prev->cpuid);
        if(dst_cpuid >= 0 && dst_cpuid != prev->cpuid){
            sched_migrate_ready_task(prev, dst_cpuid);
        }
    }
}

static void sched_switch_mm(struct task_struct *next)
{
    if(next != NULL && next->task_pgd != NULL){
        set_ttbr0_el1((u64)V2P(next->task_pgd));

        asm volatile("dsb ish" ::: "memory");
        asm volatile("tlbi vmalle1is" ::: "memory");
        asm volatile("dsb ish" ::: "memory");
        asm volatile("isb" ::: "memory");
    }
}

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
    cpu_desc_t *rq = sched_cpu_rq(cpuid);
    struct task_struct *cur_tcb;
    const sched_class_t *class;

    if(rq == NULL){
        return;
    }

    cur_tcb = rq->cur_task;
    if(cur_tcb == NULL){
        return;
    }

    class = task_get_sched_class(cur_tcb);
    if(class != NULL && class->ops != NULL && class->ops->task_tick != NULL){
        xos_spinlock(&rq->lock);
        class->ops->task_tick(rq, cur_tcb);
        xos_unspinlock(&rq->lock);
        return;
    }

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
    finish_task_switch(prev);
    if(current_task != NULL){
        current_task->sched_flags |= TASK_SCHED_STARTED;
    }
    arch_local_irq_enable();
    preempt_enable();

  //  printk(PT_RUN,"%s TAIL TAIL enter \n",__func__);
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
       // printk(PT_RUN,"%s:%d,cur_task->context->x22 =%llx\n\r",__FUNCTION__,__LINE__,cur_task->cpu_context.x22);
        return cur_task;
    }
   return NULL;

   
}


struct task_struct * get_next_task_from_cpu(int cpuid)
{
    return sched_pick_next_task(sched_cpu_rq(cpuid));

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
    //printk(PT_RUN,"%s:%d prio_max_num=%d\n\r",__FUNCTION__,__LINE__,prio_max_num);
      /*
        从run_list 获取ready 任务更改状态为running
      */

      node = list_get_first_node(&cpu_array[cpuid].runqueue[prio_max_num].run_list);
      if(node != NULL){
            cur_task = list_entry(node,struct task_struct,cpu_list);
            //printk(PT_RUN,"%s:%d,cur_task->prio=%d\n\r",__FUNCTION__,__LINE__,cur_task->prio);
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
    prev = get_current_task();
//  flags = arch_local_irq_save();
//  next = get_next_task(&task_global_list);
    next = get_next_task_from_cpu(cpuid);
    if(next == NULL){
        arch_local_irq_enable();
        return;
    }
    
    printk(PT_DEBUG,"%s: next->cpu_context.x19=%lx\n\r",__FUNCTION__,next->cpu_context.x19);
    printk(PT_DEBUG,"%s: next->cpu_context.x21=%lx\n\r",__FUNCTION__,next->cpu_context.x21);
    printk(PT_DEBUG,"%s: next->cpu_context.pc=%lx\n\r",__FUNCTION__,next->cpu_context.pc);
    sched_switch_mm(next);
    switch_to_next(prev,next,prev);
    finish_task_switch(prev);
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
    
    prev = get_current_task();
//  next = get_next_task(&task_global_list);
    next = get_next_task_from_cpu(cpuid);
    if(next == NULL){
        nesting = 0;
        return;
    }
    
    printk(PT_RUN,"%s: next->cpu_context.x19=%lx\n\r",__FUNCTION__,next->cpu_context.x19);
    printk(PT_RUN,"%s: next->cpu_context.x21=%lx\n\r",__FUNCTION__,next->cpu_context.x21);
    printk(PT_RUN,"%s: next->cpu_context.pc=%lx\n\r",__FUNCTION__,prev->cpu_context.pc);
    sched_switch_mm(next);
    if(next != prev)
        switch_to_next(prev,next,prev);
    finish_task_switch(prev);
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
    tsk = get_next_task_from_cpu(cpuid);
    if(tsk == NULL){
        return;
    }
 //   printk(PT_DEBUG,"%s:%d,tsk->prio=%d\n\r",__func__,__LINE__,tsk->prio);

    if(!cpu_array[cpuid].boot_cpu){
        dmb(ish);
        cpu_array[cpuid].cpu_online = 1;
        sev();
    }
    xos_cli();
    load_task(tsk);
}



int wake_up_proc(struct task_struct *tsk)
{
    /*
        check process status
    */
    int cpuid;
    cpuid = tsk != NULL ? sched_select_task_cpu(tsk, tsk->cpuid) : -1;
    if(tsk == NULL || cpuid < 0){
        return -1;
    }
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
