/********************************************************
    
    development create timer: 2025.1.1
    All rights reserved
    author :wangchongyang

    Copyright (c) 2025 - 2028 wangchongyang

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
#include "string.h"
#include "error.h"
#include "mem_layout.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "kernel_types.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "cpu_desc.h"
#include "xos_sleep.h"
#include "tick_timer.h"
#include "xos_cache.h"
#include "xos_page.h"
#include "xdentry.h"
#include "mount.h"
#include "task.h"
#include "schedule.h"
#include "spinlock.h"
#include "xos_zone.h"
#include "user_map.h"
#include "syscall.h"
#include "xos_dev.h"
#include "xos_mutex.h"
#include "uart.h"
#include "xos_page.h"
#include "xos_vmm.h"
#include "fork.h"

/*
    2026.0830 09：46 
    - Add copy‑on‑write (COW) handling
    - Implement MAP_SHARED support
    - Add page‑table validity judgement
    - Only copy valid page‑table entries

*/
int get_free_pid()
{
    static int next_pid = 1;

    return next_pid++;
}

static inline void fork_flush_tlb(void)
{
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("tlbi vmalle1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static inline int fork_entry_is_table(uint64 entry)
{
    return ((entry & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) ==
            (PT_ENTRY_TABLE | PT_ENTRY_VALID));
}

static inline uint64 fork_vma_prot(struct vm_area_struct *vma, int cow)
{
    uint64 prot;

    if (cow) {
        prot = PG_RO_EL1_EL0 | PTE_COW;
    } else if (vma->vm_flags & VM_WRITE) {
        prot = PG_RW_EL1_EL0;
    } else {
        prot = PG_RO_EL1_EL0;
    }

    if ((vma->vm_flags & VM_EXEC) == 0) {
        prot |= ATTR_UXN;
    }

    return prot | ATTR_PXN;
}

static int fork_copy_present_pages(struct task_struct *child,
                                   struct task_struct *parent,
                                   struct vm_area_struct *vma,
                                   uint64 prot,
                                   int update_parent)
{
    uint64 va = ALIGN_DOWN(vma->vm_start, PAGE_SIZE);
    uint64 end = ALIGN_UP(vma->vm_end, PAGE_SIZE);
    while (va < end) {
        pgd_t *pgd = &parent->task_pgd[PGD_IDX(va)];
        pmd_t *pmd_base;
        pmd_t *pmd;
        pte_t *pte_base;
        pte_t *pte;
        uint64 pmd_end;

        if (!fork_entry_is_table(*pgd)) {
            va = ALIGN_UP(va + 1, PGD_ENTRY_SIZE);
            continue;
        }

        pmd_base = (pmd_t *)P2V((*pgd) & PG_4k_ADDR_MASK);
        pmd = &pmd_base[PMD_IDX(va)];
        if (!fork_entry_is_table(*pmd)) {
            va = ALIGN_UP(va + 1, PMD_ENTRY_SIZE);
            continue;
        }

        pte_base = (pte_t *)P2V((*pmd) & PG_4k_ADDR_MASK);
        pmd_end = ALIGN_UP(va + 1, PMD_ENTRY_SIZE);
        if (pmd_end > end) {
            pmd_end = end;
        }
        while (va < pmd_end) {
            pte = &pte_base[PTE_IDX(va)];
            if (*pte & PT_ENTRY_VALID) {
                uint64 phy_addr = *pte & PG_4k_ADDR_MASK;

                if (update_parent) {
                    xos_3level_one_pagemap(parent->task_pgd,
                                           (void *)va, phy_addr, prot);
                }
                xos_3level_one_pagemap(child->task_pgd,
                                       (void *)va, phy_addr, prot);
                xos_page_get(phy_addr);
            }
            va += PAGE_SIZE;
        }
    }

    return 0;
}

int copy_task_struct(struct task_struct *child,  struct task_struct *parent)
{
    void *child_kstack;
    pgd_t *child_pgd;
    struct mm_struct *child_mm;
    struct cpu_context child_context;

    child_kstack = child->kstack;
    child_pgd = child->task_pgd;
    child_mm = child->mm;
    memcpy(&child_context, &child->cpu_context, sizeof(child_context));

    xos_spinlock(&parent->mm->mm_lock);
    memcpy(child,parent,sizeof(struct task_struct));
    child->kstack = child_kstack;
    child->task_pgd = child_pgd;
    child->mm = child_mm;
    memcpy(&child->cpu_context, &child_context, sizeof(child->cpu_context));
    child->pid = get_free_pid();
    child->tgid = parent->pid;
    child->ppid = parent->pid;
    child->tid = child->pid;
    child->state = TSTATE_READY;
    list_init(&child->sem_list);
    list_init(&child->cpu_list);
    list_init(&child->g_list);
    list_init(&child->child_list);
    list_init(&child->children_list);
    list_init(&child->delay_list);
    list_init(&child->wait_list);
    list_init(&child->mutex_list);
    xos_unspinlock(&parent->mm->mm_lock);
    return 0;
}

int alloc_mm_init(struct task_struct *child)
{
    child->mm = xos_kmalloc(sizeof(struct mm_struct));
    if(!child->mm){
        return -1;
    }
    memset(child->mm, 0, sizeof(*child->mm));
    xos_spinlock_init(&child->mm->mm_lock);
    child->mm->start_brk = 0;
    child->mm->end_brk = 0;
    child->mm->start_code = 0;
    child->mm->end_code = 0;
    child->mm->start_data = 0;
    child->mm->end_code = 0;
    return 0;
}

int alloc_pgd_init(struct task_struct *child)
{
    if(!child){
        return -1;
    }
    child->task_pgd = xos_get_free_page(0, 1);
    if(!child->task_pgd){

        return -1;
    }
    memset(child->task_pgd, 0, PAGE_SIZE);
    child->mm->mm_pgd = child->task_pgd;
    return 0;
}

int copy_vma_dec(struct task_struct *child,struct task_struct * parent)
{
    /*
        text share no need copy
    */
    xos_spinlock(&parent->mm->mm_lock);
    child->mm->start_brk   = parent->mm->start_brk;
    child->mm->end_brk     = parent->mm->end_brk;
    child->mm->start_data  = parent->mm->start_data;
    child->mm->start_stack = parent->mm->start_stack;
    xos_unspinlock(&parent->mm->mm_lock);
    return 0;
}

/*
    parent and child process use a physical address

    no copy behaivor  no need for lock
*/
int copy_vma_page(struct task_struct *child,struct task_struct * parent)
{
    int ret;
    struct vm_area_struct *next_vma;
    struct vm_area_struct *child_vma;
    struct vm_area_struct *vma;
    uint64 prot;
    int parent_pte_changed = 0;
//    pte_t *parent_pte;
//    pte_t *child_pte;
    vma = parent->mm->mmap;
    for(;vma != NULL;vma = vma->vm_next){
        if (vma->vm_flags & VM_SHARED){
            /*
                no need chang prot ,pte some map
                虚拟地址相同，物理地址相同，pgd  ,pmd 页表位置不同，pte 页表相同
                所以最后一级页表需要根据情况单独做映射
                如果中间某次vma 申请失败如何处理，以前的申请的vma 全部遍历释放
                共享映射，不修改VMA权限
            */
            ret = create_vma(child, vma->vm_start, vma->vm_end, vma->pma_saddr,vma->vm_flags);
            if(ret < 0){
                goto fail_create_vma;
            }
            prot = fork_vma_prot(vma, 0);
            ret = fork_copy_present_pages(child, parent, vma, prot, 0);
            if(ret < 0){
                goto fail_find_pte;
            }

            
        }else{
            /*
                child parent need chang prot readonly
                pte some map
            */
            prot = fork_vma_prot(vma, 1);
            ret = create_vma(child, vma->vm_start, vma->vm_end, vma->pma_saddr,vma->vm_flags);
            if(ret < 0){
                goto fail_create_vma;
            }
            ret = fork_copy_present_pages(child, parent, vma, prot, 1);
            if(ret < 0){
                goto fail_find_pte;
            }
            parent_pte_changed = 1;
        }
    }

    if (parent_pte_changed) {
        fork_flush_tlb();
    }
    return 0;

fail_find_pte:

    child_vma = child->mm->mmap;
    for(;child_vma != NULL;child_vma = next_vma){
        next_vma = child_vma->vm_next;
        xos_kfree(child_vma);
    }
    child->mm->mmap = NULL;

fail_create_vma:
    return ret;
}

int copy_vma(struct task_struct *child,struct task_struct *parent)
{
    int ret;
    if((!child)||(!child->mm)||(!parent)||(!parent->mm)){

        return -1;
    }
    ret = copy_vma_dec(child,parent);
    if(ret < 0){
        
    }
    ret = copy_vma_page(child,parent);
    return ret;
}


int  copy_mm(struct task_struct *child,  struct task_struct *parent)
{
    int ret;
    /*
       1. kmalloc  mm_struct  for child
       2. alloc page dir for child
       3. copy_space from parent to child
    */
    ret = alloc_mm_init(child);
    if(ret < 0){
        goto  fail_alloc_mm;
    }
    ret = alloc_pgd_init(child);
    if(ret < 0){
        goto fail_alloc_pgd;
    }
    ret = copy_vma(child,parent);
    if(ret < 0){
        goto fail_copy_vma;
    }
    return 0;

fail_copy_vma:
    xos_free_page(child->task_pgd);
    child->task_pgd = NULL;

fail_alloc_pgd:
    xos_kfree(child->mm);
    child->mm = NULL;
fail_alloc_mm:

    return ret;
}

void copy_files(struct task_struct *child,  struct task_struct *parent)
{
    memcpy(&child->fs_context,&parent->fs_context,sizeof(parent->fs_context));
    memcpy(&child->files_set,&parent->files_set,sizeof(parent->files_set));
    xos_spinlock_init(&child->fs_context.lock);
    xos_spinlock_init(&child->files_set.file_lock);
    
}

void copy_singal()
{
    
}

static int copy_process( struct task_struct *child,  struct task_struct *parent)
{
    int ret = -1;
    thread_union_t *child_stack;
    if(!parent->mm){
        return ret;
    }
    child_stack = (thread_union_t *)child->kstack;
    xos_spinlock(&parent->mm->mm_lock);
    memcpy(child->kstack, parent->kstack, 2*PAGE_SIZE);
    memcpy(&child->cpu_context,&parent->cpu_context,sizeof(parent->cpu_context));
    xos_unspinlock(&parent->mm->mm_lock);
    child_stack->thread_val.p_task = child;
    copy_task_struct(child,parent);
    ret = copy_mm(child,parent);
    if(ret < 0){
        return ret;
    }
    copy_files(child,parent);
    return 0;
}





void clone_mnt()
{


}
int do_clone(int clone_flags)
{
    #define FOR_FAILE -1
    int ret;
    int cpuid;
    unsigned long flags;
    thread_union_t *kstack;
    struct task_struct *cur = current_task;
    struct task_struct *child;

    flags = arch_local_irq_save();

    child = xos_get_free_page(0, 2);
    if(!child){
        goto fail_alloc_task;
    }
    kstack = (thread_union_t *)xos_get_free_page(0,1);
    if(!kstack){
        goto fail_kstack;
    }
    struct pt_regs * ptr = get_task_pt_regs_new((char*)kstack);
    memset(ptr,0, sizeof(struct pt_regs));
    memset(&child->cpu_context, 0,sizeof(struct cpu_context));
    child->kstack = kstack;
    ret = copy_process(child, cur);
    if(ret < 0){
        goto fail_copy_process;
    }
    kstack->thread_val.p_task = child;
    child->sched_policy = SCHED_RR;
    task_refresh_sched_class(child);
    child->state = TSTATE_READY;
    list_init(&child->g_list);
    list_init(&child->cpu_list);
    list_init(&child->sem_list);
    list_init(&child->delay_list);
    xos_init_timer(&child->timer, 0, NULL, NULL);

    child->cpu_context.pc = (unsigned long)ret_from_fork;
    child->cpu_context.sp = (unsigned long)ptr;  //栈顶
    child->cpu_context.x19 = 0;

    ((struct pt_regs *)(child->cpu_context.sp))->regs[0] = 0;

    cpuid = cur_cpuid();
    add_to_g_list(child);
    add_to_cpu_runqueue(cpuid, child);
//    regs->regs[0] = child->pid;
//    return regs->regs[0];
    arch_local_irq_restore(flags);
    return child->pid;

fail_copy_process:
    xos_free_page(kstack);

fail_kstack:
    xos_free_page(child);

fail_alloc_task:

//     regs->regs[0] = FOR_FAILE;

     arch_local_irq_restore(flags);
     return -1;
}
