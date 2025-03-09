/********************************************************
    
    development start:2024.3
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
#include "setup_map.h"
#include "cpu_desc.h"
#include "printk.h"
#include "arch_irq.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "xdentry.h"
#include "mount.h"
#include "tick_timer.h"
#include "task.h"



uint64 kernel_ticks;


/*
2024.04.04 PM:23：32
    通过cpuid 找到相应的cpuid 上任务等待队列delay_list
    一个tick 时间到达更新等待队列delay_tick 值，只更新头节点后面节点即可


2024.04.5 21:11    handler_delay_task 任务休眠再唤醒的时候同优先级任务出现问题，多次休眠唤醒之后
    delay_ticks 出现问题，还是相对延时引入的，思来想去决定还是否决掉相对延时使用
    绝对延时，直接比较当前是否是否到期，插入时间链表时也方便

*/
void handle_delay_task(int cpuid)
{
    /*
        查表法是最快查找方式，用空间换时间，如果要查表，某个优先级任务delay
        时，需要将当前优先级记录到bitmap 表中
        结构中已经定义了bitmap，当前暂时偷懒使用轮询方式，就是效率比较低
 
    */
    int i;
    struct task_struct *d_cur_task;
    dlist_t *tmp_head;
    for(i = 0 ; i < PRIO_MAX;i++){
        /*
            队列是否非空，如果非空当前优先级delay队列有休眠任务
            如果队列为空，则当前优先级delay 队列没有休眠任务
        */
        if(list_is_empty(&cpu_array[cpuid].wait_que[i].delay_list) != 1){
            /*
                非空
            */
            printk(PT_RUN,"%s:%d, prio=%d\n\r",__func__,__LINE__,i);
            tmp_head = cpu_array[cur_cpuid()].wait_que[i].delay_list.next;
            if(tmp_head == NULL){
                printk(PT_RUN,"%s:%d, prio=%d\n\r",__func__,__LINE__,i);
                continue;
            }
            
            d_cur_task = list_entry(tmp_head,struct task_struct,delay_list);
            printk(PT_RUN,"%s:%d, prio=%d,d_cur_task->delay_ticks=%d\n\r",__func__,__LINE__,i,d_cur_task->delay_ticks);
            if(d_cur_task->delay_ticks >= 1){
                d_cur_task->delay_ticks--;
            }else if(d_cur_task->delay_ticks == 0){
                /*
                    休眠已经到期，可以加入到相应的ruqeue run_list 队列
                    1.更改任务状态由sleep 变为ready 态
                    2.加入对应优先级run_queue run_list 队列
                */
                printk(PT_RUN,"%s:%d, prio=%d\n\r",__func__,__LINE__,i);
                d_cur_task->state = TSTATE_READY;
                /*
                    20240405 AM:8:20 任务休眠超时，将任务加入到runqueu 对应的优先级运行队列
                    但是当前节点并未从cpu_array[cpuid].wait_que[i].delay_list 链表节点中删除

                    导致等待链表一直处于非空状态
                */
                list_del(&d_cur_task->delay_list);
                list_add_front(&d_cur_task->cpu_list, &cpu_array[cur_cpuid()].runqueue[d_cur_task->prio].run_list);
                /*
                    2024.04.5 AM:7:45 任务休眠之后无法再被调度原因:所在优先级的bitmap 没有被置位
                    find_prio_bitmap 找不到对应优先级
                */
                cpu_array[cur_cpuid()].run_count[d_cur_task->prio]++;/*2024.405 am:9:23*/
                set_bit((uint8_t*)(cpu_array[cpuid].run_bitmap.bit_start), d_cur_task->prio);
                
            }
        }
        
    }
    
}

/*
    以前的handle_delay_task 还是保留，记录自己操作系统改变历程

    当前应该使用一系列封装函数实现定时功能，既方便又简洁：
    add_timer ,mode_timer handle_timer

    系统工程代码。尤其是模块开发的关键就是模块封装，结构体抽象
    这两者做好之后代码就是工作量的问题
*/

void  xos_mod_timer(xos_timer_t *timer,uint64 timer_out){
    timer->time_out = kernel_ticks + timer_out;
    printk(PT_RUN,"%s:%d,timer->time_out = %lx\n\r",__FUNCTION__,__LINE__,timer->time_out);
}


void  xos_init_timer(xos_timer_t *timer,u64 out_ticks,void *arg,timer_fun timer_call)
{
    timer->timer_val = out_ticks;
    timer->time_out = kernel_ticks + out_ticks;
    timer->arg = arg;
    timer->timer_fun = timer_call;
    list_init(&timer->t_list);

}
/*
    2024.4.5:20:08
    将初始化好的timer 时间注册到相应链表中
    链表头肯定要对外隐藏，不要暴漏

    add_timer 实现和xos_sleep 实现非常相似，后续xos_sleep 可以封装调用xos_add_timer

    当然不必每个优先级都需要一个队列头，凡是休眠的任务都加入一个等待队列
    xos_sleep 每个优先级都有一个timer_delay 队列不但浪费空间，繁琐，而且容易
    引起问题，当前xos_add_timer 对上述问题做了改进

    超时唤醒之后再根据任务优先级加入到相应优先级的runqueu 运行队列

    2024.05.31通过调试发现本来是在执行期间被timer_isr 中断打断，造成数据冲突
    所以需要关中断保护，不仅仅hi自旋锁
*/
int xos_add_timer(xos_timer_t *timer){

    xos_timer_t *tmp;
    dlist_t *tmp_list;
    arch_local_irq_disable();
    xos_spinlock(&cpu_array[cur_cpuid()].lock);
    dlist_t *l_timer_list_head = &cpu_array[cur_cpuid()].timer_list_head;
    if (list_is_empty(l_timer_list_head)) { 

        list_add_back(&timer->t_list, l_timer_list_head);
        printk(PT_RUN,"%s:%d  MAGIC A\n\r",__FUNCTION__,__LINE__);
    } else {

        xos_timer_t *first_node = list_entry(l_timer_list_head->next, xos_timer_t, t_list);
        if (timer->time_out < first_node->time_out) {
            list_add_back(&timer->t_list, l_timer_list_head); //加入到头节点后面
            printk(PT_RUN,"%s:%d MAGIC B\n\r",__FUNCTION__,__LINE__);
        } else {

            list_for_each(tmp_list,l_timer_list_head){
                tmp = list_entry(tmp_list,xos_timer_t,t_list);
                if(timer->time_out <= tmp->time_out){
                    printk(PT_RUN,"%s:%d MAGIC B\n\r",__FUNCTION__,__LINE__);
                    list_add_front(&timer->t_list, &tmp->t_list);
                    break;
                }
                
              }
               if(tmp_list == l_timer_list_head){
                    list_add_front(&timer->t_list, l_timer_list_head);
                }
                
           }
            
        }
    xos_unspinlock(&cpu_array[cur_cpuid()].lock);
    arch_local_irq_enable();
    return 0;
}        



int xos_del_timer(){
    
    return 0;
}

void xos_timer_out_dispatch(xos_timer_t *timer)
{
    printk(PT_RUN,"%s:%d timer->time_out=%ld\n\r",__FUNCTION__,__LINE__,timer->time_out);
    timer->timer_fun(timer,timer->arg);
}


void xos_timer_out_reg(xos_timer_t *timer,timer_fun  t_fun,void *arg)
{
    timer->timer_fun = t_fun;
    timer->arg = arg;
}
/*
    20240405：PM:22:20
    写完这个有点后悔，感觉应该再下封装实现的更完美一些，不过那样耗费精力时间有点多，怕打消自己积极性
*/
void xos_check_timer()
{
    xos_timer_t *tmp;
    dlist_t *tmp_list;
    xos_spinlock(&cpu_array[cur_cpuid()].lock);
    dlist_t *l_timer_list_head = &cpu_array[cur_cpuid()].timer_list_head;
    list_for_each(tmp_list,l_timer_list_head){
        tmp = list_entry(tmp_list,xos_timer_t,t_list);
        if(tmp->time_out > kernel_ticks){
            printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;  //当前节点及后序节点都不会超时，后序节点time_out 会越来越大，可以停止遍历
        }else{
            
            printk(PT_RUN,"%s:%d,dispatch timer\n\r",__FUNCTION__,__LINE__);
            xos_timer_out_dispatch(tmp);
        }
        /*
            刚才想打印task 对应的pid ，突然发现没有实现pid，后续要加上pid
            ppid 又是一大项工程
        */
    }
    xos_unspinlock(&cpu_array[cur_cpuid()].lock);
}
