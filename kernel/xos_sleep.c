/********************************************************
    
    development started: 2024
    All rights reserved
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
#include "list.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "fs.h"
#include "setup_map.h"
#include "mount.h"
#include "cpu_desc.h"
#include "printk.h"
#include "tick_timer.h"
#include "task.h"
#include "xos_kern_def.h"
#include "schedule.h"
#include "arch_irq.h"


/*
    20240404:共享资源操作在并发环境中需要加锁，下一步需要实现锁，我需要一把锁
    20240404：19：00 定义变量名称有时候也是一件难事

    20250307:这里面暂时有自己开发过程中大量注释，容易帮助别人理解，但是看起来比较乱，所以大部分删除了
*/
int xos_sleep(uint32_t ticks)
{
    dlist_t *dlist_cur;
    arch_local_irq_disable();
    struct task_struct *l_cur_task = cpu_array[cur_cpuid()].cur_task;
    struct task_struct *cur_tmp;
    if(ticks == 0){
        return 0; //后续设置成规范的返回值
    }
    l_cur_task->delay_ticks = ticks; // 休眠ticks 数
    l_cur_task->state = TSTATE_SLEEPING;

    list_del(&l_cur_task->cpu_list);/*脱链*/
    printk(PT_RUN,"%s:%d,cur_task->prio=%d,ffffffffl_cur_task->delay_ticks=%d,\n\r",__func__,__LINE__,l_cur_task->prio,l_cur_task->delay_ticks);
    cpu_array[cur_cpuid()].run_count[l_cur_task->prio]--;

    /*
        当前clear_bit 清除相应的bitmap.bits 位是有缺陷的，无法应对同优先级任务，所以还应该添加计数
        类似与文件操作， 如果当前优先级bit 位对应的count 值为0 ，再去做clear_bit
    */
    if(cpu_array[cur_cpuid()].run_count[l_cur_task->prio] == 0){
        clear_bit((uint8_t*)(cpu_array[cur_cpuid()].run_bitmap.bit_start), l_cur_task->prio);
    }
    
    printk(PT_RUN,"%s:%d,cur_task->prio=%d\n\r",__func__,__LINE__,l_cur_task->prio);
    /*
        加入相应的等待队列
        由于时delay_list 队列，每个任务等待时间可能不同，所以
        加入wait_queue delay_list 之前需要做比较，头节点永远时等待时间
        最段的节点，所以再做节点插入时需要做遍历比较
    */

    if(list_is_empty(&cpu_array[cur_cpuid()].wait_que[l_cur_task->prio].delay_list)){
        list_add_back(&l_cur_task->delay_list ,&cpu_array[cur_cpuid()].wait_que[l_cur_task->prio].delay_list);
    }else{

        dlist_t *tmp_head = &cpu_array[cur_cpuid()].wait_que[l_cur_task->prio].delay_list;
        list_for_each(dlist_cur,tmp_head)
        {
            cur_tmp = list_entry(dlist_cur,struct task_struct,delay_list);
            if(l_cur_task->delay_ticks >= cur_tmp->delay_ticks){

                l_cur_task->delay_ticks -= cur_tmp->delay_ticks;  /*为何delay_ticks 好端端的值为何突然变成0 问题就在这块*/
                printk(PT_RUN,"%s:CCCCCCCCC%d\n\r",__func__,__LINE__);
                continue;
            }else{

                printk(PT_RUN,"%s:insert DDDDDDDDD%d\n\r",__func__,__LINE__);

                list_add_front(&l_cur_task->delay_list ,dlist_cur);
                cur_tmp = list_entry(dlist_cur,struct task_struct,delay_list);
                cur_tmp->delay_ticks -= l_cur_task->delay_ticks;/*这里出现了验证的问题，非常难查觉*/
            }
        }

        if(dlist_cur == tmp_head){
            list_add_front(&l_cur_task->delay_list ,dlist_cur);
            printk(PT_RUN,"%s:%d\n\r",__func__,__LINE__);
        }
        
    }
    arch_local_irq_enable();
    schedule();  // 调度查找下一个任务   

    return 0;
}



void xos_sleep_timerout(struct timer_struct *timer, void *arg)
{
    /*
        20240405 PM:22:12  感觉有点累，脑子不转了，开始想看手机了，不行坚持一下
        tick 中断，检测是否超时，如果超时调度sleep_out 睡觉时间到
        耗费2个小时新改写的方案，效率感觉也没有提升，只是面向对象抽象之后
        更容易写代码了，也不容易出错。
        20240405:PM:22:45  忘记置位对应优先级的bitmap 

        20240606 PM:23:12 一边熬药一边写代码，此情此景，多线程并行处理
    */
    struct task_struct *l_cur_task = (struct task_struct*)arg;

    list_del(&timer->t_list);
    l_cur_task->state = TSTATE_READY;

    cpu_array[cur_cpuid()].run_count[l_cur_task->prio]++;/*2024.405 am:9:23*/
    set_bit((uint8_t*)(cpu_array[cur_cpuid()].run_bitmap.bit_start), l_cur_task->prio);
    list_add_front(&l_cur_task->cpu_list,&cpu_array[cur_cpuid()].runqueue[l_cur_task->prio].run_list);

    if(l_cur_task->prio  < current_task->prio){
        current_task_info_new()->need_switch_flags = 1;
    }
    
}


void xos_sleep_ticks(u64 ticks)
{
    int run_cnt;
    struct task_struct *l_cur = cpu_array[cur_cpuid()].cur_task;
    list_del(&l_cur->cpu_list);
    l_cur->state = TSTATE_SLEEPING;
    arch_local_irq_disable();
    run_cnt = cpu_array[cur_cpuid()].run_count[l_cur->prio];
    printk(PT_RUN,"%s:%d,cur_task->prio=%d,run_cnt=%d\n\r",__func__,__LINE__,l_cur->prio,run_cnt);
    cpu_array[cur_cpuid()].run_count[l_cur->prio]--;
    if(cpu_array[cur_cpuid()].run_count[l_cur->prio] == 0){
        clear_bit((uint8_t*)(cpu_array[cur_cpuid()].run_bitmap.bit_start), l_cur->prio);
    }
    /*
        20240405：PM:22:05
        我还以为我新的的定时器有错误，一顿失落，难受，写操作系统就是这样
        时而失落，时而兴奋，记录是我写下去的动力
    */
    
    xos_mod_timer(&l_cur->timer, ticks); 
    xos_timer_out_reg(&l_cur->timer,xos_sleep_timerout,l_cur);
    xos_add_timer(&l_cur->timer);
    arch_local_irq_enable();

    /*
        sleep 工作暂时不用    wait_queue[prio].wait_list   ，这个后续可做它用
        20240405 pm:22:00 折腾版本说怎么不调度了，左找右找
        糊涂了，尽然把他给忽略了
        20240519 PM:17:12 schedule 需要做保护工作
    */
    schedule();  
}
