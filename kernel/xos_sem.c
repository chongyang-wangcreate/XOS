/*
    假期唯一想做的事情就是写代码 20240609：PM 14:49
    The only thing I want to do during the holiday is write code 20240609: PM 14:49
*/

/********************************************************
    
    development started:2024
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024-2038 wangchongyang

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
#include "kernel_types.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "cpu_desc.h"
#include "xos_sleep.h"
#include "tick_timer.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "task.h"
#include "schedule.h"
#include "xos_sem.h"

/*
    ref_count 根据初始化时设置不同的值，
    可以成为二进制信号量，计数信号量
    所以我封装的sem_init(sem_t *sem)还应当添加一个count 
    参数，由用户来决定是什么类型的信号量
    sem_init(sem_t *sem) 改成 sem_init(sem_t *sem,int count)
*/
void sem_init(xsem_t *sem,int count,int type)
{
    xos_spinlock_init(&sem->lock);
    sem->sem_types = type;
    sem->sem_count = count;
    list_init(&sem->wait_list_head);
}



void sem_pend(xsem_t *sem)
{
    /*
        获取信号量
        判断sem_count 是否大于0.如果不满足条件则休眠
        改变当前进程状态，将当前进程加入信号量 链表wait_list_head

        写着写着代码，突然发现如果获取信号量的时产生中断怎么办
        写着写着就有思路了
    */
    struct task_struct *curr = get_current_task();
    xos_spinlock(&sem->lock);
    if(sem->sem_count > 0){
        sem->sem_count--;
        xos_unspinlock(&sem->lock);
        return ;
    }else{
        list_add_back(&curr->sem_list, &sem->wait_list_head);
        /*
            改变curr 状态。将curr 从运行链表或者ready ,链表脱链
        */
        curr->state = TSTATE_PENDING;
        list_del(&curr->cpu_list);  //从对应cpu    runqueue 中删除

        xos_unspinlock(&sem->lock);
        schedule();
    } 
    xos_unspinlock(&sem->lock);
}


void sem_post(xsem_t *sem)
{
    dlist_t *cur_node;
    dlist_t *list_head;
    struct task_struct *curr;
    /*
        释放信号量
        检查wait_list_head 是否为空，如果为空ref_count 值增加
        否则唤醒当前信号量等待的进程
    */
    list_head = &sem->wait_list_head;
    xos_spinlock(&sem->lock);
    if(list_is_empty(list_head)){
        sem->sem_count++;
    }else{
        /*
            存在休眠进程，需要唤醒休眠进程,
            变量唤醒
        */
        list_head = &sem->wait_list_head;
        list_for_each(cur_node,list_head){
            curr = list_entry(cur_node, struct task_struct, sem_list);
            list_del(&curr->sem_list); //从等待链表中删除
            /*
                wakeup
            */
            curr->state = TSTATE_READY;  //被调度时任务状态由READY 转为RUNNING
            list_add_back(&curr->cpu_list, &cpu_array[cur_cpuid()].runqueue[curr->prio].run_list);
        }

    }
    xos_unspinlock(&sem->lock);
}

