/********************************************************
    
    development started: 2024
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

#include "types.h"
#include "string.h"
#include "mem_layout.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "kernel_types.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "xos_mutex.h"
#include "spinlock.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "cpu_desc.h"
#include "xos_sleep.h"
#include "tick_timer.h"
#include "task.h"
#include "schedule.h"
#include "uart.h"

mutext_t   g_mutex;


void mutext_init(mutext_t *mutex)
{
    mutex->wait_count = 0;
    mutex->source = 1;
    xos_spinlock_init(&mutex->sp_lock);
    list_init(&mutex->wait_list_head);
    
}



void mutext_lock(mutext_t *mutex)
{
    xos_spinlock(&mutex->sp_lock);
    while(1){
        if(mutex->source == 1){
            mutex->source--;
            printk(PT_RUN," LOCK SUCCESS %s,%d\n",__FUNCTION__,__LINE__);
            break;
        }else{
            current_task->state = TSTATE_PENDING;
            printk(PT_RUN," LOCK fail %s,%d\n",__FUNCTION__,__LINE__);
            list_add_back(&current_task->mutex_list,&mutex->wait_list_head);
            mutex->wait_count++;
            xos_unspinlock(&mutex->sp_lock);
            schedule();
            xos_spinlock(&mutex->sp_lock);
        }
    }
    xos_unspinlock(&mutex->sp_lock);

}


void mutext_unlock(mutext_t *mutex)
{
    dlist_t *list_cur;
    struct task_struct *cur_t;
    dlist_t *wait_head = &mutex->wait_list_head;
    printk(PT_RUN," unlock start %s,%d\n",__FUNCTION__,__LINE__);
    if(mutex->wait_count > 0){
        list_for_each(list_cur, wait_head){
            cur_t = list_entry(list_cur, struct task_struct, mutex_list);
            printk(PT_RUN,"%s,%d,cur_addr=%lx",__FUNCTION__,__LINE__,(unsigned long)(cur_t));
            mutex->wait_count--;
            wake_up_proc(cur_t);
        }
    }else{
        mutex->wait_count = 0;
        mutex->source = 1;
    }

}
