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


int get_free_pid()
{

    return 0;
}

int copy_task_struct(struct task_struct *child,  struct task_struct *parent)
{
    xos_spinlock(&parent->mm->mm_lock);
    memcpy(child,parent,sizeof(struct task_struct));
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
    xos_unspinlock(&parent->mm->mm_lock);
    return 0;
}

int alloc_mm_init(struct task_struct *child)
{
    child->mm = xos_kmalloc(sizeof(struct mm_struct));
    if(!child->mm){
        return -1;
    }
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
    struct page_desc *page;
    unsigned long phy_addr;
    unsigned long vm_addr;
    unsigned long pfn;
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
            ret = create_vma(child, vma->vm_start, vma->vm_end, vma->pma_saddr,VM_SHARED);
            if(ret < 0){
                goto fail_create_vma;
            }
            if(vma->vm_flags &VM_WRITE){
                prot = PG_RW_EL1_EL0;
            }else{
                
                prot = PG_RO_EL1;
            }
            for(vm_addr =vma->vm_start;vm_addr < vma->vm_end;vm_addr += PAGE_SIZE){
                ret = find_pte_phy(parent->task_pgd,(void*)vm_addr,&phy_addr);
                if(ret){
                    goto fail_find_pte;
                }
                
//                xos_3level_one_pagemap(parent->task_pgd, vm_addr,phy_addr, prot);
                xos_3level_one_pagemap(child->task_pgd, (void*)vm_addr,phy_addr, prot);

            }

            
        }else{
            /*
                child parent need chang prot readonly
                pte some map
            */
            prot = PG_RO_EL1;
            ret = create_vma(child, vma->vm_start, vma->vm_end, vma->pma_saddr,vma->vm_flags);
            if(ret < 0){
                goto fail_create_vma;
            }
            for(vm_addr =vma->vm_start;vm_addr < vma->vm_end;vm_addr+= PAGE_SIZE){
                ret = find_pte_phy(parent->task_pgd,(void*)vm_addr,&phy_addr);
                if(ret){
                    goto  fail_find_pte;
                }
              /*
                    write trigger  page fault
                    XOS_PFN_TO_PAGE
               */  
               /*
                all set readonly
                parent_pte = copy_create_3level_page(parent->task_pgd, (void*)vm_addr);
                child_pte = copy_create_3level_page(child->task_pgd, (void*)vm_addr);
                *parent_pte = phy_addr | ACCESS_FLAG | SH_IN_SH | prot | NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE | PT_ENTRY_VALID;
                *child_pte = phy_addr | ACCESS_FLAG | SH_IN_SH | prot | NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE | PT_ENTRY_VALID;
                */
                xos_3level_one_pagemap(parent->task_pgd, (void*)vm_addr,phy_addr, prot);
                xos_3level_one_pagemap(child->task_pgd, (void*)vm_addr,phy_addr, prot);
                pfn = XOS_PHY_TO_PFN(phy_addr);
                page = XOS_PFN_TO_PAGE(pfn);
                /*
                    需要添加原子操作，暂未实现
                 */
                page->ref_cnt++;
            }
        }
    }


fail_find_pte:

    child_vma = child->mm->mmap;
    for(;child_vma != NULL;child_vma = next_vma){
        next_vma = child_vma->vm_next;
        xos_kfree(child_vma);
    }

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

fail_copy_vma:
    xos_free_page(child->task_pgd);

fail_alloc_pgd:
    xos_kfree(child->mm);
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
    if(!parent->mm){
        return ret;
    }
    xos_spinlock(&parent->mm->mm_lock);
    memcpy(child->kstack, parent->kstack, 2*PAGE_SIZE);
    memcpy(&child->cpu_context,&parent->cpu_context,sizeof(parent->cpu_context));
    xos_unspinlock(&parent->mm->mm_lock);
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
    thread_union_t *kstack;
    struct task_struct *cur = current_task;
    struct task_struct *child;
    child = xos_get_free_page(0, 0);
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
    child->sched_policy = SCHED_RR;
    child->state = TSTATE_READY;
    list_init(&child->g_list);
    list_init(&child->cpu_list);
    list_init(&child->sem_list);
    list_init(&child->delay_list);
    xos_init_timer(&child->timer, 0, NULL, NULL);

    child->cpu_context.pc = (unsigned long)ret_from_fork;
    child->cpu_context.sp = (unsigned long)ptr;  //栈顶

    ((struct pt_regs *)(child->cpu_context.sp))->regs[0] = 0;

//    regs->regs[0] = child->pid;
//    return regs->regs[0];
    return child->pid;

fail_copy_process:


fail_kstack:
    xos_free_page(child);

fail_alloc_task:

//     regs->regs[0] = FOR_FAILE;

     return -1;
}


