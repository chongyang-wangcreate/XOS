#include "types.h"
#include "list.h"
#include "string.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "mem_layout.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "tick_timer.h"
#include "fs.h"
#include "task.h"
#include "kernel_types.h"
#include "schedule.h"
#include "setup_map.h"
#include "interrupt.h"
#include "user_map.h"



static struct vm_area_struct *find_vma_by_addr(void *vaddr)
{

    unsigned long u_vaddr = (unsigned long)vaddr;
    struct vm_area_struct *iter_mmap = current_task->mm->mmap;
    while(iter_mmap != NULL){
        if(iter_mmap->vm_start <= u_vaddr && u_vaddr < iter_mmap->vm_end){
            return iter_mmap;
        }
        iter_mmap = iter_mmap->vm_next;
    }
    return NULL;
}


int check_addr_is_legal(void *vaddr)
{
    unsigned long u_vaddr = (unsigned long)vaddr;
    struct vm_area_struct *tmp_mmap = current_task->mm->mmap;
    /*
        遍历vma 查看是否位于某个有效的vma
    */
    struct vm_area_struct *iter = tmp_mmap;
    while(iter->vm_prev){
        if (iter->vm_start <= u_vaddr && u_vaddr < iter->vm_end){
            return 1;
        }
    }
    return 0;
}

int check_addr_is_writable(void *vaddr)
{
    unsigned long u_vaddr = (unsigned long)vaddr;
    struct vm_area_struct *tmp_mmap = current_task->mm->mmap;
    /*
        遍历vma 查看是否位于某个有效的vma
    */
    struct vm_area_struct *iter = tmp_mmap;
    while(iter->vm_prev){
        if (iter->vm_start <= u_vaddr && u_vaddr < iter->vm_end){
           break;
        }
    }
    if(iter->vm_flags &VM_WRITE){
        return 1;
    }
    return 0;
}

int check_addr_is_readable(void *vaddr)
{
    unsigned long u_vaddr = (unsigned long)vaddr;
    struct vm_area_struct *tmp_mmap = current_task->mm->mmap;
    /*
        遍历vma 查看是否位于某个有效的vma
    */
    struct vm_area_struct *iter = tmp_mmap;
    while(iter->vm_prev){
        if (iter->vm_start <= u_vaddr && u_vaddr < iter->vm_end){
           break;
        }
    }
    if(iter->vm_flags &VM_READ){
        return 1;
    }
    return 0;
}

/*
    There are still many issues left. Will sort them out later. 
    Recorded on 2026.07.25
*/
static int user_range_in_vma(void *vaddr ,unsigned long size,int need_write)
{
    unsigned long start = (unsigned long)vaddr;
    unsigned long end;
    struct vm_area_struct *vma;

    if(size == 0){
        return 1;
    }
    end = start + size;
    if(end < start){
        return 0;
    }
    vma = find_vma_by_addr(vaddr);
    if(vma && start >= vma->vm_start && end <= vma->vm_end){
        if(!need_write || (vma->vm_flags & VM_WRITE)){
            return 1;
        }
    }
    if(start >= USER_STACK_START && end <= USER_STACK_TOP){
        return 1;
    }
    return 0;
}

unsigned long copy_from_user(void *to ,const void *from,unsigned long size)
{
    /*
        to do
    */
    return 0;
}

/*
    to do
    Address validity check
    Note that the current function implementation is incorrect

    to：指向用户空间的目标内存地址的指针，。
    from：指向内核空间的源数据地址的指针。
    size：要复制的字节数。
    返回值：返回未成功复制的字节数


*/

unsigned long copy_to_user(void *to ,const void *from,unsigned long size)
{
    /*
        1. to in valid vma range
        2. to   Does the VMA where to is located have write permission

        通过to 值找到对应的vma ,判断if(vma->vm_flags &VM_WRITE)
    */
    /*if(!check_addr_is_legal(to)){
        return 0;
    }
    if(!check_addr_is_readable(to)){
        return 0;
    }*/
    if(!user_range_in_vma(to,size,1)){
        return 0;
    }
    memcpy(to,from,size);
    return size;
}

