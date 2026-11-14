/********************************************************
    development start:2023.12
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

   Copyright (c) 2023 - 2027 wangchongyang

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
#include "string.h"
#include "spinlock.h"
#include "setup_map.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "interrupt.h"
#include "tick_timer.h"
#include "arch64_timer.h"
#include "xos_mutex.h"
#include "fs.h"
#include "task.h"
#include "schedule.h"
#include "cpu_desc.h"
#include "spinlock.h"
#include "device_tree.h"
#include "arch64_irq.h"
#include "barriers.h"
#include "error.h"

cpu_desc_t cpu_array[CPU_NR];
/* Early secondary entry reads this table before it has a per-CPU stack. */
uint64 xos_cpu_boot_mpidrs[CPU_NR];
int g_cpu_possible_count = 1;
int g_cpu_ready;

#define XOS_RESCHEDULE_IPI 1

extern void xos_set_vector_entry(void);

static void xos_reschedule_ipi(void *desc)
{
    struct task_struct *task = get_current_task();
    int cpuid = (int)cur_cpuid();
    int need_reschedule = 0;

    (void)desc;
    if(cpuid >= 0 && cpuid < CPU_NR){
        if(cpu_array[cpuid].smp_call_pending){
            xos_smp_call_func_t func;
            void *arg;

            xos_spinlock(&cpu_array[cpuid].smp_call_lock);
            func = cpu_array[cpuid].smp_call_func;
            arg = cpu_array[cpuid].smp_call_arg;
            cpu_array[cpuid].smp_call_pending = 0;
            xos_unspinlock(&cpu_array[cpuid].smp_call_lock);
            if(func != NULL){
                func(arg);
            }
        }
        if(cpu_array[cpuid].smp_reschedule_pending){
            cpu_array[cpuid].smp_reschedule_pending = 0;
            need_reschedule = 1;
        }
    }
    if(need_reschedule && task != NULL){
        task->need_switch = 1;
    }
}

void xos_smp_init(void)
{
    request_irq(XOS_RESCHEDULE_IPI, xos_reschedule_ipi, 0,
                "reschedule-ipi", NULL);
}

void xos_smp_send_reschedule(int cpuid)
{
    if(cpuid < 0 || cpuid >= g_cpu_possible_count){
        return;
    }
    if(!cpu_array[cpuid].cpu_online || cpuid == (int)cur_cpuid()){
        return;
    }
    cpu_array[cpuid].smp_reschedule_pending = 1;
    dmb(ish);
    gicv3_send_sgi(cpuid, XOS_RESCHEDULE_IPI);
}

int xos_smp_call_function_on_cpu(int cpuid,
                                 xos_smp_call_func_t func,
                                 void *arg)
{
    unsigned long flags;

    if(func == NULL || cpuid < 0 || cpuid >= g_cpu_possible_count ||
       !cpu_array[cpuid].cpu_online){
        return -EINVAL;
    }
    if(cpuid == (int)cur_cpuid()){
        func(arg);
        return 0;
    }

    flags = arch_local_irq_save();
    xos_spinlock(&cpu_array[cpuid].smp_call_lock);
    if(cpu_array[cpuid].smp_call_pending){
        xos_unspinlock(&cpu_array[cpuid].smp_call_lock);
        arch_local_irq_restore(flags);
        return -EBUSY;
    }
    cpu_array[cpuid].smp_call_func = func;
    cpu_array[cpuid].smp_call_arg = arg;
    dmb(ish);
    cpu_array[cpuid].smp_call_pending = 1;
    xos_unspinlock(&cpu_array[cpuid].smp_call_lock);
    arch_local_irq_restore(flags);

    if(gicv3_send_sgi(cpuid, XOS_RESCHEDULE_IPI) != 0){
        cpu_array[cpuid].smp_call_pending = 0;
        return -EIO;
    }
    return 0;
}

/*
    0 比较特殊
*/
char prio_table[128] = {

 0,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 5,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 6,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 5,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,


};

static void cpu_desc_init_queues(cpu_desc_t *cpu)
{
    int prio_num;
    for (prio_num = 0; prio_num < PRIO_MAX; prio_num++) {
        list_init(&cpu->runqueue[prio_num].run_list);
        list_init(&cpu->rt_runqueue[prio_num].run_list);
        list_init(&cpu->normal_runqueue[prio_num].run_list);
        list_init(&cpu->wait_que[prio_num].delay_list);
        list_init(&cpu->wait_que[prio_num].event_list);
        list_init(&cpu->wait_que[prio_num].wait_list);
    }
    list_init(&cpu->timer_list_head);
    xos_spinlock_init(&cpu->lock);
    xos_spinlock_init(&cpu->smp_call_lock);
    cpu->run_bitmap.bit_start = (uint8 *)&cpu->bitmap_runque_start;
    cpu->run_bitmap.btmp_bytes_len = 32; //32个bit
    cpu->wait_bitmap.bit_start = (uint8 *)&cpu->bitmap_waitque_start;
    cpu->wait_bitmap.btmp_bytes_len = 32;
    cpu->rt_run_bitmap.bit_start = (uint8 *)&cpu->bitmap_rt_runque_start;
    cpu->rt_run_bitmap.btmp_bytes_len = 32;
    cpu->normal_run_bitmap.bit_start = (uint8 *)&cpu->bitmap_normal_runque_start;
    cpu->normal_run_bitmap.btmp_bytes_len = 32;
}

static void cpu_desc_set_topology(cpu_desc_t *cpu, int cpuid,
                                  const xos_dtb_cpu_t *dt_cpu, uint64 mpidr)
{
    cpu->cpuid = cpuid;
    cpu->possible = 1;
    cpu->boot_state = XOS_CPU_OFFLINE;
    cpu->mpidr = mpidr;
    if (dt_cpu != NULL) {
        cpu->release_addr = dt_cpu->release_addr;
        strncpy(cpu->enable_method, dt_cpu->enable_method,
                sizeof(cpu->enable_method) - 1);
    }
}

void cpu_desc_init(void)
{
    const xos_dtb_desc_t *dtb = xos_dtb_get_info();
    uint64 boot_mpidr = read_mpidr_el1();
    const xos_dtb_cpu_t *boot_dt_cpu = NULL;
    int count = 1;
    int i;
    int j;
    uint64 mpidr;

    g_cpu_ready = 0;
    memset(cpu_array, 0, sizeof(cpu_array));
    memset(xos_cpu_boot_mpidrs, 0, sizeof(xos_cpu_boot_mpidrs));

    if (dtb != NULL && dtb->valid) {
        for (i = 0; i < dtb->cpu_count && i < XOS_DTB_MAX_CPUS; i++) {
            if (dtb->cpus[i].enable &&(dtb->cpus[i].mpidr & MPIDR_HWID_MASK) == boot_mpidr) {
                boot_dt_cpu = &dtb->cpus[i];
                break;
            }
        }
        if (dtb->cpu_count && boot_dt_cpu == NULL) {
            printk(PT_WARRING,
                   "boot MPIDR 0x%lx absent from DT; only boot CPU enabled\n",
                   boot_mpidr);
        }
    }

    cpu_desc_set_topology(&cpu_array[0], 0, boot_dt_cpu, boot_mpidr);
    cpu_array[0].boot_cpu = 1;
    if (boot_dt_cpu != NULL) {
        for (i = 0; i < dtb->cpu_count && i < XOS_DTB_MAX_CPUS &&count < CPU_NR; i++) {
           
            if (!dtb->cpus[i].enable) {
                continue;
            }
            mpidr = dtb->cpus[i].mpidr & MPIDR_HWID_MASK;
            if (mpidr == boot_mpidr) {
                continue;
            }
            for (j = 1; j < count; j++) {
                if (cpu_array[j].mpidr == mpidr) {
                    break;
                }
            }
            if (j != count) {
                printk(PT_WARRING, "duplicate CPU MPIDR 0x%lx in DT\n", mpidr);
                continue;
            }
            cpu_desc_set_topology(&cpu_array[count], count, &dtb->cpus[i], mpidr);
            count++;
        }
    }

    for (i = 0; i < count; i++) {
        cpu_desc_init_queues(&cpu_array[i]);
        xos_cpu_boot_mpidrs[i] = cpu_array[i].mpidr;
    }
    g_cpu_possible_count = count;
    dmb(ish);
    g_cpu_ready = 1;
    xos_cpu_mark_online(0);
}

void xos_cpu_mark_starting(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR || !cpu_array[cpuid].possible){
        return;
    }
    cpu_array[cpuid].boot_state = XOS_CPU_STARTING;
    dmb(ish);
}

void xos_cpu_mark_online(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR || !cpu_array[cpuid].possible){
        return;
    }
    cpu_array[cpuid].cpu_online = 1;
    dmb(ish);
    cpu_array[cpuid].boot_state = XOS_CPU_ONLINE;
    sev();
}

void xos_cpu_mark_failed(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR || !cpu_array[cpuid].possible){
        return;
    }
    cpu_array[cpuid].cpu_online = 0;
    dmb(ish);
    cpu_array[cpuid].boot_state = XOS_CPU_FAILED;
    sev();
}

int xos_cpu_boot_state(int cpuid)
{
    if(cpuid < 0 || cpuid >= CPU_NR || !cpu_array[cpuid].possible){
        return XOS_CPU_FAILED;
    }
    return cpu_array[cpuid].boot_state;
}

int xos_mpidr_to_cpuid(u64 mpidr)
{
    int i;

    mpidr &= MPIDR_HWID_MASK;
    if(!g_cpu_ready){
        /* Only the boot CPU executes C before topology is published. */
        return 0;
    }
    for(i = 0; i < g_cpu_possible_count; i++){
        if(cpu_array[i].possible && cpu_array[i].mpidr == mpidr){
            return i;
        }
    }
    return -1;
}

u64 xos_cpuid_to_mpidr(int cpuid)
{
    if(cpuid < 0 || cpuid >= g_cpu_possible_count || !cpu_array[cpuid].possible){
        return 0;
    }
    return cpu_array[cpuid].mpidr;
}

int xos_cpu_possible_count(void)
{
    return g_cpu_possible_count;
}

void secondary_cpu_percpu_init(void)
{
    xos_set_vector_entry();
    gicv3_init_percpu();
    xos_timer_init_percpu();
}

static void secondary_idle_task(void *arg)
{
    (void)arg;
    while(1){
        if(sched_balance_idle_cpu((int)cur_cpuid())){
            schedule();
            continue;
        }
        asm volatile("wfi" ::: "memory");
    }
}

void asm_secondary_entry(u64 mpidr)
{
    int cpuid;

    mpidr &= MPIDR_HWID_MASK;
    cpuid = xos_mpidr_to_cpuid(mpidr);
    if(cpuid >= 0 && cpuid < CPU_NR){
        printk(PT_WARRING,"cpu%d secondary init mpidr=0x%lx\n\r",cpuid,mpidr);
        secondary_cpu_percpu_init();
        if(xos_idle_thread_create((unsigned long)secondary_idle_task, 0) == 0){
            xos_cpu_mark_online(cpuid);
            printk(PT_WARRING,"cpu%d idle task ready\n\r",cpuid);
            load_first_task();
        }else{
            xos_cpu_mark_failed(cpuid);
            printk(PT_ERROR,"cpu%d idle task creation failed\n\r",cpuid);
        }
    }else{
        printk(PT_WARRING,"unknown secondary parked mpidr=0x%lx\n\r",mpidr);
    }
    while(1){
        asm volatile("wfe" ::: "memory");
    }
}


int _get_hp_task(uint64 map_val)
{
    return prio_table[map_val];
}
void find_hp_task(){
    int h_priority;
    tcb_map  prio_entity = {0};
    if(prio_entity.task_prio_map.prio_group_0 != 0){
        h_priority = _get_hp_task(prio_entity.task_prio_map.prio_group_0);
    }else if(prio_entity.task_prio_map.prio_group_1 != 0){
        h_priority = _get_hp_task(prio_entity.task_prio_map.prio_group_1);
        h_priority += 64;
    }
}

void task_set_bit(uint64 task_prio_map, uint64 bit_index) {
//  uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    task_prio_map |= (1ULL << offset);
}

void task_clear_bit(uint64 task_prio_map, uint64 bit_index) {
//    uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    task_prio_map &= ~(1ULL << offset);
}

int task_is_bit_set(uint64 task_prio_map, uint64 bit_index) {
//    uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    return (task_prio_map & (1ULL << offset))!= 0;
}


void new_add_to_cpu_runqueue(struct task_struct *task){

    uint64 *prio_bitmap;
    u64 cpuid;
    cpuid = cur_cpuid();
    if(task->prio < 64){
            prio_bitmap = &cpu_array[cpuid].map_entity.task_prio_map.prio_group_0;
            task_set_bit(*prio_bitmap,task->prio);
            cpu_array[cpuid].map_entity.task_prio_map.group0_hit = 1;
        }else if(task->prio >= 64){
             prio_bitmap = &cpu_array[cpuid].map_entity.task_prio_map.prio_group_1;
             task_set_bit(*prio_bitmap,task->prio);
             cpu_array[cpuid].map_entity.task_prio_map.group1_hit = 1;
        }
    

}

