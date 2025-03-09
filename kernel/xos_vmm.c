/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024-2030 wangchongyang

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
#include "arch_irq.h"
#include "spinlock.h"
#include "printk.h"
#include "xos_page.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "tick_timer.h"
#include "xos_mutex.h"
#include "task.h"
#include "xos_kern_def.h"
#include "schedule.h"


int create_vma(struct task_struct *tsk, unsigned long vm_start,
        unsigned long vm_end, unsigned long phy_addr ,
    unsigned long vm_flags)
{
    struct vm_area_struct *vma;

    if (tsk == NULL) {
        printk(PT_DEBUG,"%s: task is null\n", __FUNCTION__);
        return -1;
    }   
    vma = xos_kmalloc(sizeof (*vma));
    if (vma == NULL) {
        printk(PT_DEBUG,"%s: %d\n", __FUNCTION__,__LINE__);
        return -1;
    }

    vma->vm_start = vm_start;
    vma->vm_end = vm_end;
    vma->vm_flags = vm_flags;
    vma->pma_saddr = phy_addr;
    printk(PT_RUN,"%s: %d\n", __FUNCTION__,__LINE__);
    if (tsk->mm->mmap != NULL) {
        tsk->mm->mmap->vm_prev = vma;
    }
    printk(PT_RUN,"%s: %d\n", __FUNCTION__,__LINE__);
    vma->vm_next = tsk->mm->mmap;
    vma->vm_prev = NULL;
    tsk->mm->mmap = vma;
    printk(PT_RUN,"%s: %d\n", __FUNCTION__,__LINE__);
    return 0;
}

/*
    to do
    Functions that have not yet been implemented
    
    1. check vma access permissions
     1.1 vma read
     1.2 vma write
     1.3. vma exec
    2. check vma flags
     2.1 . MAP_SHARED
     2.2 . MAP_PRIVATE
     2.3 . MAP_ANONYMOUS
     2.4   MAP_FIXED
     2.5   MAP_LOCKED
     2.6   MAP_STACK
     2.7   MAP_FILE
     2.8   MAP_GROWSDOWN
    3. find vma
    4. merge vma
    5. insert vma
*/
