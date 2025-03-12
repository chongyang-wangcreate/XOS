/********************************************************
    
    development start: 2024
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
#include "string.h"
#include "setup_map.h"
#include "arch_irq.h"
#include "spinlock.h"
#include "printk.h"
#include "xos_page.h"
#include "xos_zone.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "tick_timer.h"
#include "mmu.h"
#include "xos_mutex.h"

#include "task.h"
#include "xos_kern_def.h"
#include "schedule.h"
#include "xos_vmm.h"
#include "mmu.h"
#include "mem_layout.h"
#include "cpu_desc.h"
#include "xos_sleep.h"
#include "fork.h"
#include "user_map.h"
#include "error.h"

/*
    2024.10.20
    内存管理部分存在一些小问题，大空间的申请暂时使用数据段空间替代

    2024.12.09
    加载第一个用户态程序，从写代码到调试成功用了好久好久，卡在调试一直不成功
    最后竟然是用户态程序加载成功，但是异常处理部分没对用户异常的判断和处理
    更不要提page_fault 了，好在终于过了这个大门槛。
*/

extern void xos_task_entry();
extern void add_to_cpu_runqueue(int cpuid,struct task_struct *task);

#define USER_STACK_SIZE 0x800000

#define USER_STACK_TOP    ((1UL << VA_BITS) - USER_STACK_SIZE -1)

#define USER_HEAP_START 0x10000000
#define USER_HEAD_END   0x20000000
#define USER_HEAP_SIZE (USER_HEAD_END - USER_HEAP_START)

#define INV_PHY_ADDR (-1)

#define GET_PHY_OFF(vstart ,vbase) ((uint64)(vstart) - (uint64)(vbase))
#define GET_PHY_START(pstart,vstart,vbase) (pstart + GET_PHY_OFF(vstart,vbase))

extern uint64 _user_lma[];
extern uint64 _user_vma[];



#ifndef CONFIG_PLAN2
/*
    plan1
*/
extern uint64 _user_text_saddr[];
extern uint64 _user_text_eaddr[];
extern uint64 _user_rodata_saddr[];
extern uint64 _user_rodata_eaddr[];
extern uint64 _user_data_saddr[];
extern uint64 _user_data_eaddr[];
extern uint64 _user_bss_saddr[];
extern uint64 _user_bss_eaddr[];

#define USER_TEXT_SADDR   _user_text_saddr
#define USER_TEXT_EADDR   _user_text_eaddr
#define USER_RODATA_SADDR _user_rodata_saddr
#define USER_RODATA_EADDR _user_rodata_eaddr
#define USER_DATA_SADDR   _user_data_saddr
#define USER_DATA_EADDR   _user_data_eaddr
#define USER_BSS_SADDR    _user_bss_saddr
#define USER_BSS_EADDR    _user_bss_eaddr



#else
/*
    plan2
*/
extern uint64 _user_text_saddr;
extern uint64 _user_text_eaddr;
extern uint64 _user_rodata_saddr;
extern uint64 _user_rodata_eaddr;
extern uint64 _user_data_saddr;
extern uint64 _user_data_eaddr;
extern uint64 _user_bss_saddr;
extern uint64 _user_bss_eaddr;

uint64 *_user_text_kmap_start = &_user_text_saddr;
uint64 *_user_text_kmap_end = &_user_text_eaddr;
uint64 *_user_rodata_kmap_start = &_user_rodata_saddr;
uint64 *_user_rodata_kmap_end = &_user_rodata_eaddr;
uint64 *_user_data_kmap_start = &_user_data_saddr;
uint64 *_user_data_kmap_end = &_user_data_eaddr;
uint64 *_user_bss_kmap_start = &_user_bss_saddr;
uint64 *_user_bss_kmap_end = &_user_bss_eaddr;


#define USER_TEXT_SADDR   _user_text_kmap_start
#define USER_TEXT_EADDR   _user_text_kmap_end
#define USER_RODATA_SADDR _user_rodata_kmap_start
#define USER_RODATA_EADDR _user_rodata_kmap_end
#define USER_DATA_SADDR   _user_data_kmap_start
#define USER_DATA_EADDR   _user_data_kmap_end
#define USER_BSS_SADDR    _user_bss_kmap_start
#define USER_BSS_EADDR    _user_bss_kmap_end


#endif

char task_buf[8192] = {0};
char task_pgd_buf[10240] = {0};
char task_mm[4096] = {0};
char task_kstack[8192] = {0};

/**********************************
    cur_task = (struct task_struct *)task_buf;
    if (cur_task == NULL) {
        printk("%s: !!!!!!!!!!!! task is null\n", __FUNCTION__);
    }
    cur_task->task_pgd = xos_get_free_page(0,1);
    cur_task->task_pgd = (pgd_t*)task_pgd_buf;
    cur_task->mm = xos_kmalloc(sizeof(struct mm_struct));
****************************************/



void vma_space_mapping(uint64_t *pg_dir, void *virt_addr,
                uint64_t phy_addr, uint64_t size,
                int wr_flags)
{

    char *addr;
    int i;
    if (pg_dir == NULL) {
        printk(PT_RUN,"%s: pg_dir is null\n", __FUNCTION__);
        return;
    }
    if (phy_addr == (0xffffffffffffffff)) {
        addr = (char*) align_down(virt_addr, PTE_ENTRY_SIZE);
        phy_addr = (uint64_t)xos_get_phy(0, 0);
        for(i = 0; i < size ;i += PAGE_SIZE){
            if(wr_flags){
                map_page(pg_dir, addr, size, (uint64)phy_addr,PG_RO_EL1_EL0);
            }else{
                map_page(pg_dir, addr, size, (uint64)phy_addr,PG_RW_EL1_EL0);
            }
            addr += PAGE_SIZE;
            phy_addr = (uint64_t)xos_get_phy(0, 0);
            printk(PT_RUN,"%s: phy_addr=%lx\n", __FUNCTION__,(uint64)phy_addr);
        
        }
        printk(PT_RUN,"%s: phy_addr=%lx\n", __FUNCTION__,(uint64)phy_addr);
    }else{
            addr = (char*) align_down(virt_addr, PTE_ENTRY_SIZE);
            for(i = 0; i < size ;i += PAGE_SIZE){
                if(!wr_flags){
                    map_page(pg_dir, addr, size, (uint64)phy_addr,PG_RW_EL1_EL0);
                }else{
                    map_page(pg_dir, addr, size, (uint64)phy_addr,PG_RO_EL1_EL0);
                }
                
                addr += PAGE_SIZE;
                phy_addr += PAGE_SIZE;
            }
        
    }

}


void user_page_mapping(uint64_t *pg_dir, void *virt_addr,
                uint64_t phy_addr, unsigned long size,
                int prot)
{

    char *addr;
    int i;
    if (pg_dir == NULL) {
        printk(PT_RUN,"%s: pg_dir is null\n", __FUNCTION__);
        return;
    }
    if (phy_addr == (0xffffffffffffffff)) {
        printk(PT_DEBUG,"%s: pg_dir is null\n", __FUNCTION__);
        addr = (char*) align_down(virt_addr, PTE_ENTRY_SIZE);
        phy_addr = (uint64_t)xos_get_phy(0, 0);
        for(i = 0; i < size ;i += PAGE_SIZE){
            map_page(pg_dir, virt_addr, size, (uint64)phy_addr,PG_RW_EL1_EL0);
            addr += PAGE_SIZE;
            phy_addr = (uint64_t)xos_get_phy(0, 0);
        }
        printk(PT_RUN,"%s: phy_addr=%lx\n", __FUNCTION__,(uint64)phy_addr);
    }else{
            addr = (char*) align_down(virt_addr, PTE_ENTRY_SIZE);
            for(i = 0; i < size ;i += PAGE_SIZE){
                map_page(pg_dir, virt_addr, size, (uint64)phy_addr,PG_RW_EL1_EL0);
                addr += PAGE_SIZE;
                phy_addr += PAGE_SIZE;
            }
    }

}


void vma_space_maps(struct task_struct *t)
{
    struct vm_area_struct *vma;
    int wr_flags;
    if (t == NULL) {
        printk(PT_RUN,"%s: task is null\n", __FUNCTION__);
        return;
    }

    for (vma = t->mm->mmap; vma != NULL; vma = vma->vm_next) {
        wr_flags = !(vma->vm_flags & VM_WRITE);
        vma_space_mapping(t->task_pgd, (void *)vma->vm_start,
        vma->pma_saddr,
        (vma->vm_end - vma->vm_start),
        wr_flags);
    }
}




void init_mm(struct mm_struct* mm)
{
    mm->mmap_count = 0;
    mm->start_code = (unsigned long)USER_TEXT_SADDR;
    mm->end_code = (unsigned long)USER_TEXT_EADDR;
    
    mm->start_data = (unsigned long)USER_DATA_SADDR;
    mm->end_data = (unsigned long)USER_DATA_EADDR;
    
    xos_spinlock_init(&mm->mm_lock);

}

uint64_t get_user_entry()
{
    uint64_t user_entry_addr;
    asm volatile (
        "ldr %0, = shell_main\n\t"
        : "=r" (user_entry_addr)
    );
    return user_entry_addr;
}
void init_ucontext(struct pt_regs * cur_regs, void *pc, void *sp) {
    cur_regs->pc = (uint64)pc;
    cur_regs->sp = (uint64)sp;
    cur_regs->pstate = PSR_MODE_EL0t;
}

int create_data_vma(struct task_struct *cur_task,uint64 usr_phy_start)
{
    int ret = 0;
    uint64 *user_data_start;
    uint64 *user_data_end;
    user_data_start = V_TO_POINTER(USER_DATA_SADDR);
    user_data_end = V_TO_POINTER(USER_DATA_EADDR);
    ret = create_vma(cur_task, (uint64)user_data_start,
     (uint64)user_data_end,
     (uint64)(usr_phy_start), VM_READ | VM_WRITE );
    if (ret < 0) {
        printk(PT_RUN,"create_vma for user rodata failed\n");
        ret = -EDATA_VMA;
    }
    return ret;
}


int create_rodata_vma(struct task_struct *cur_task,uint64 usr_phy_start)
{
    int ret = 0;
    int *user_rodata_start;
    int *user_rodata_end;
    user_rodata_start = V_TO_POINTER(USER_RODATA_SADDR);
    user_rodata_end = V_TO_POINTER(USER_RODATA_EADDR);
    ret = create_vma(cur_task, (uint64)user_rodata_start,
     (uint64)user_rodata_end,
     (uint64)(usr_phy_start), VM_READ | VM_SHARED);
    if (ret < 0) {
        printk(PT_RUN,"create_vma for user rodata failed\n");
        ret = -ERODATA_VMA; 
    }
     return ret;
}
int create_text_vma(struct task_struct *cur_task,uint64 usr_phy_start)
{
    int ret = 0;
    uint64 *user_text_start;
    uint64 *user_text_end;
    user_text_start = V_TO_POINTER((uint64)USER_TEXT_SADDR);
    user_text_end = V_TO_POINTER((uint64)USER_TEXT_EADDR);
    ret = create_vma(cur_task, (uint64)user_text_start,
     (uint64)user_text_end,
     (uint64)usr_phy_start, VM_READ | VM_EXEC | VM_SHARED);
    if (ret < 0) {
        printk(PT_RUN,"create_vma for user text failed\n");
        ret = -ETEXT_VMA;
    }
    return ret;
}
int create_bss_vma(struct task_struct *cur_task,uint64 usr_phy_start)
{
    int ret = 0;
    uint64 *user_bss_start;
    uint64 *user_bss_end;
    user_bss_start = V_TO_POINTER((uint64)USER_BSS_SADDR);
    user_bss_end = V_TO_POINTER((uint64)USER_BSS_EADDR);
    ret = create_vma(cur_task, (uint64)user_bss_start,
        (uint64)user_bss_end,
        (uint64)(usr_phy_start), VM_READ | VM_WRITE);
    if (ret < 0) {
        printk(PT_RUN,"setup_vma for user bss failed\n");
        ret = -EBSS_VMA;
    }
    return ret;
}


int create_stack_vma(struct task_struct *cur_task,uint64 stack_start,uint64 stack_end,uint64 usr_phy_start)
{
    int ret = 0;
    ret = create_vma(cur_task, (uint64)stack_start,
        (uint64)stack_end,
        (uint64)(usr_phy_start), VM_READ | VM_WRITE);
    if (ret < 0) {
        printk(PT_RUN,"setup_vma for user stack failed\n");
        ret = -EBSS_VMA;
    }
    return ret;
}

int create_heap_vma(struct task_struct *cur_task,uint64 heap_start,uint64 heap_end,uint64 usr_phy_start)
{
    int ret = 0;
    ret = create_vma(cur_task, (uint64)heap_start,
        (uint64)heap_end,
        (uint64)(usr_phy_start), VM_READ | VM_WRITE);
    if (ret < 0) {
        printk(PT_DEBUG,"setup_vma for user heap failed\n");
        ret = -EHEAP_VMA;
    }
    return ret;
}


int create_dev_vma(struct task_struct *cur_task,uint64 dev_start,uint64 dev_end,uint64 usr_phy_start)
{
    int ret = 0;

    ret = create_vma(cur_task, (uint64)dev_start,
        (uint64)dev_end,
        (uint64)(usr_phy_start), VM_READ | VM_WRITE);
    if (ret < 0) {
        printk(PT_DEBUG,"setup_vma for user dev failed\n");
        ret = -EDEV_VMA;
    }
    return ret;
}



int create_process_vma(struct task_struct *cur_task)
{
    int ret = 0;
    uint64 user_pma_start;
    uint64 user_vma;
//    uint64 vma_access;
    
    uint64 user_text_start;
    uint64 user_rodata_start;
    uint64 user_data_start;
    uint64 user_bss_start;
    uint64 user_bss_end;

    user_pma_start = V_TO_P(_user_lma);
    user_vma = V_TO_P(_user_vma);
    printk(PT_RUN,"lma_user=%p\n", user_pma_start);
    printk(PT_RUN,"vma_user=%p\n", user_vma);
    uint64 user_stack_start = (USER_STACK_TOP - USER_STACK_SIZE +1);
    uint64 user_stack_end = (USER_STACK_TOP);


    user_text_start = V_TO_P((uint64)USER_TEXT_SADDR);
    user_rodata_start = V_TO_P((uint64)USER_RODATA_SADDR);
    user_data_start = V_TO_P((uint64)USER_DATA_SADDR);
    user_bss_start = V_TO_P((uint64)USER_BSS_SADDR);
    user_bss_end = V_TO_P((uint64)USER_BSS_EADDR);

    ret = create_text_vma(cur_task,user_pma_start);
    if(ret < 0){
         printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret;
    }
    ret = create_rodata_vma(cur_task, GET_PHY_START(user_pma_start,user_rodata_start,user_text_start));
    if(ret < 0){
         printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret;
    }
    ret = create_data_vma(cur_task,GET_PHY_START(user_pma_start,user_data_start,user_text_start));
    if(ret < 0){
         printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret;
    }
    ret = create_bss_vma(cur_task,GET_PHY_START(user_pma_start,user_bss_start,user_text_start));
    if(ret < 0){
         printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret;
    }
    ret = create_stack_vma(cur_task,user_stack_start,user_stack_end,(uint64)INV_PHY_ADDR);
    if(ret < 0){
         printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret;
    }

    cur_task->mm->start_brk = user_bss_end;
    cur_task->mm->end_brk = cur_task->mm->start_brk + USER_HEAP_SIZE;
    ret = create_heap_vma(cur_task,(uint64)cur_task->mm->start_brk,(uint64)cur_task->mm->end_brk ,(uint64)INV_PHY_ADDR);
    if(ret < 0){
        return ret; 
    }
    
    ret = create_dev_vma(cur_task,(uint64)_TO_UVA_(UART_BASE),((uint64)_TO_UVA_(UART_BASE) + PAGE_SIZE),(uint64)UART_BASE);
    if(ret < 0){
        printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
        return ret; 
    }



    return ret;

}

void process_create()
{
    uint64_t user_stack_top = USER_STACK_TOP;
    uint64_t user_entry_addr;
    
    uint64_t cpuid;
    thread_union_t *kstack;
    struct task_struct *cur_task = NULL;
    cpuid = cur_cpuid();

    cur_task = (struct task_struct *)xos_get_free_page(0,1);
//  cur_task = xos_kmalloc(sizeof (struct task_struct));

    unsigned long buf_start = (unsigned long)task_buf;  // 获取 task_buf 的起始地址
    unsigned long pgd_buf_start = (unsigned long)task_pgd_buf;

    buf_start = (buf_start + 0xFFF) & ~0xFFF;  // 将地址对齐到 4KB 边界
    pgd_buf_start = (pgd_buf_start + 0xFFF) & ~0xFFF;
//  cur_task = (struct task_struct *)buf_start;
    cur_task->task_pgd = (pgd_t*)pgd_buf_start;

    cur_task->mm = (struct mm_struct*)task_mm;

    init_mm(cur_task->mm);
    create_process_vma(cur_task);

    vma_space_maps(cur_task);
    printk(PT_DEBUG,"%s:%d\n",__FUNCTION__,__LINE__);
    
    set_ttbr0_el1((uint64)(V2P(cur_task->task_pgd)));

    user_entry_addr = get_user_entry();
    printk(PT_RUN,"%s:%d,user_init_addr=%lx\n",__FUNCTION__,__LINE__,user_entry_addr);

    /*
        创建了新的task 任务，将新任务加入到相应处理器的runqueue 队列
        这我没有加，所以任务调度不成功，无法放弃处理器
    */
    kstack = (thread_union_t *)xos_get_free_page(0,1);
    if(kstack == NULL){
        goto alloc_stack_faild;
    }
    kstack->thread_val.p_task = cur_task;
    struct pt_regs * cur_regs = get_task_pt_regs_new((char*)kstack);
    memset(cur_regs,0, sizeof(struct pt_regs));
    /*
        202412.18 PM 10:48
        当前未考虑环境变量和参数，如果栈顶就是user_stack_end
        添加参数和环境变量之后栈顶还需要做更改

        当前重要工作是对zone 区域做区分，当前只有一个区域zone_normal
    */
    init_ucontext(cur_regs, (void*)user_entry_addr, (void*)user_stack_top);
    cur_task->parent = current_task;
    cur_task->default_prio = PRIO_ONE;
    cur_task->prio = cur_task->default_prio+2;
    cur_task->timeslice = 5;
    cur_task->timerslice_count = cur_task->timeslice;
    cur_task->cpu_context.pc = (unsigned long)ret_from_fork;
    cur_task->cpu_context.sp = (unsigned long)cur_regs;  //栈顶
    cur_task->kstack = cur_regs;
    cur_task->cpu_context.x19 = 0;  /*very important*/
    cur_task->cpu_context.x22 = 5097;
    cur_task->cpu_context.x23 = 5098;
    cur_task->cpu_context.x24 = 5099;
    cur_task->cpu_context.x25 = 5100;
    cur_task->cpu_context.x26 = 5101;
    cur_task->cpu_context.x27 = 5102;

    cur_task->sched_policy = SCHED_RR;
    cur_task->state = TSTATE_READY;
    list_init(&cur_task->g_list);
    list_init(&cur_task->cpu_list);
    list_init(&cur_task->sem_list);
    list_init(&cur_task->delay_list);

    init_fs_context(current_task,cur_task);
    memset(&cur_task->files_set.fd_set,0,sizeof(cur_task->files_set.fd_set));
    cur_task->files_set.fd_map.bit_start = (uint8_t*)cur_task->files_set.fd_set;
    cur_task->files_set.fd_map.btmp_bytes_len = sizeof(cur_task->files_set.fd_set);

    xos_init_timer(&cur_task->timer, 0, NULL, NULL);
    add_to_cpu_runqueue(cpuid,cur_task);
alloc_stack_faild:
    return ;

}
