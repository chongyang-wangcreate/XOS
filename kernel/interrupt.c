/********************************************************
    
    development started:20240405
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2027 wangchongyang

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
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "fs.h"
#include "xos_cache.h"
#include "xdentry.h"
#include "mount.h"
#include "string.h"
#include "tick_timer.h"
#include "interrupt.h"
#include "gic-v3.h"
#include "task.h"
#include "schedule.h"
#include "printk.h"
#include "xos_preempt.h"

/*
    当前中断部分实现非常简单粗暴，定义了中断描述符，然后定义了相应的数组
    后续会持续进行改进，第一版核心要点就是在实现功能的基础上越简单越好
    当然有时候看起来写的也非常low, 这都是不是问题，慢慢改进

*/
irq_desc_t irq_desc_buf[256];

void irq_mask(uint32_t hwirq)
{
    gic_mask_irq(hwirq);
}

void irq_unmask(uint32_t hwirq)
{
    gic_unmask_irq(hwirq);
}

uint32_t irq_read_iar(void)
{
    return gic_read_iar();
}

void irq_eoi(uint32_t hwirq)
{
	gic_eoi_irq(hwirq);
}


void irq_enter()
{
    preempt_count_add(1);
}

void irq_exit()
{
    preempt_count_dec(1);

}


int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                const char *name, void *data)
{
    irq_desc_buf[irq].handle_fun = handler;
    irq_desc_buf[irq].flags = flags;
    irq_desc_buf[irq].handler_data = data;
    strcpy(irq_desc_buf[irq].name,name);
    irq_unmask(irq);
    return 0;
}

void xos_handle_irq(int irq_num)
{
    irq_desc_buf[irq_num].handle_fun(&irq_desc_buf[irq_num]);
}

/*
    20240606:23:27: 后续会引入irq_entr   和irq_exit
    schedule 函数会加入到irq_exit 中， 中断退出时 会判断是否需要抢占， 是否返回用户态，如果是怎判断signal

    或者后续，el0_irq el1_irq 中断处理函数分开各自处理各自的
*/
void xos_irq_el1_entry(struct pt_regs *regs)
{
    uint32_t irqnr;
//  struct task_struct *cur = get_current_task();
    irq_enter();
    irqnr = irq_read_iar();//查手册，查CSDN 
    /*
        2024.0308 PM:22:45
        当前中断总入口函数做的并不好
        中断注册函数做的也不好，有更好的实现方法，当前也是为了赶进度
        实现太复杂，容易打消自己的积极性造成，导致操作系统开发搁浅，
        所以要做妥协，不断迭代
    */
    xos_handle_irq(irqnr);

    irq_eoi(irqnr);
    /*
        调度期间禁止抢占
        内核在调度过程中，这时候如果发生了内核抢占, 调度会被中断, 而调度却还没有完成, 这样会丢失我们调度的信息.
        暂时未完成
    */

    irq_exit();

    if(current_task_info_new()->need_switch_flags == 1)
    {
        current_task_info_new()->need_switch_flags = 0;
        int_schedule();
    }
}


void show_pt_regs(const struct pt_regs *regs)
{

    int i;

    for (i = 0; i < 31; i++) {
        printk(PT_RUN,"X%d = 0x%p\n", i, (void*)(regs->regs[i]));
    }
    printk(PT_RUN,"sp_val:0x%p\n", (void*)(regs->sp));
    printk(PT_RUN,"pc_val :0x%p\n", (void*)(regs->pc));
    printk(PT_RUN,"pstate_val:0x%p\n", (void*)(regs->pstate));
}


void xos_irq_el0_entry(struct pt_regs *regs)
{
    uint32_t irqnr;
    int is_user_mode = 0;
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    irq_enter();
    irqnr = irq_read_iar();//查手册，查CSDN 
    /*
        2024.0308 PM:22:45
        当前中断总入口函数做的并不好
        中断注册函数做的也不好，有更好的实现方法，当前也是为了赶进度
        实现太复杂，容易打消自己的积极性造成，导致操作系统开发搁浅，
        所以要做妥协，不断迭代
    */
    is_user_mode = user_mode(regs);
    if(is_user_mode){
         /*View user site*/
         show_pt_regs(regs);
    }
    xos_handle_irq(irqnr);

    irq_eoi(irqnr);

    irq_exit();
    /*
        to do
        返回用户空间的收尾工作
        1. 查看current 是否设置的 __TIF_NEED_RESCHED，如果置为触发抢占
        2. 检查是否存在挂起的信号，如果处理信号  
        3. 处理挂起的信号linux 内核使用do_notify_resume()
    */

    if(current_task_info_new()->need_switch_flags == 1)
    {
        current_task_info_new()->need_switch_flags = 0;
        int_schedule();
    }
    /*
        2.检查是否存在挂起信号
    */

}

