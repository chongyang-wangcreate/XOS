/********************************************************
    
    development started:2024.1
    author :wangchongyang
    email:rockywang599@gmail.com
    Copyright (c) 2024  wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    https://blog.csdn.net/h_b__k/article/details/145766239

********************************************************/

#include "types.h"
#include "list.h"
#include "sysreg.h"
#include "setup_map.h"
#include "arch64_timer.h"
#include "interrupt.h"
#include "printk.h"
#include "tick_timer.h"
#include "cpu_desc.h"


/*
    cntp_tval_el0 是计时器的值寄存器，它存储了定时器的初始值。
    定时器会从这个值开始递减，并且当计时器到达零时触发中断。该寄存器的值通常是由程序设定的，
    用来决定定时器的周期。
*/

#ifndef CONF_EL1
    
#define CNTP_CTL_EL     cntp_ctl_el0
#define CNTFRQ_EL       cntfrq_el0
#define CNTP_TVAL_EL    cntp_tval_el0
    
#else

    
#endif



#define READ_REG(reg) ({                         \
    long _val;                                   \
    asm volatile("mrs %0, " __stringify(reg) : "=r" (_val));  \
    _val;                                        \
})


#define WRITE_REG(v, name)                                                          \
    do {                                                                                 \
        uint64_t _r = v;                                                                 \
        asm volatile("msr "__stringify(name) ", %0" : : "r"(_r));                        \
    } while (0)



int arch64_timer_stop()
{
    uint64_t ctrl = READ_REG(CNTP_CTL_EL);
    if((ctrl & (1 << 0)))
    {
        ctrl &= ~(1 << 0);
        WRITE_REG(ctrl, CNTP_CTL_EL);
    }
    return 0;
}


void arch64_timer_start(void)
{
    uint64_t ctrl = READ_REG(CNTP_CTL_EL);
    if(!(ctrl & (1 << 0)))
    {
        ctrl |= (1 << 0);
        WRITE_REG(ctrl, CNTP_CTL_EL);
    }

}


void xos_timer_init()
{
    arch64_timer_init(TICK_TIMER_COUNT);
    arch64_timer_start();
}
static inline uint64_t arch_timer_frequecy(void)
{
    uint64_t rate = READ_REG(CNTFRQ_EL);
    return (rate != 0) ? rate : 1000000;
}

/*
    将kernel_ticks 操作方法timer_isr 具体的中断处理函数中来处理

    每次tick 中断产生之后，进行遍历当前的等待队列

    但是当前等待队列比较单一，delay_timer   , event 相关的都放到一个队列中

    唤醒的时候非常不方便，所以wait_queue 需要将上述情况分开来处理

*/
int g_period;
void timer_isr(void *desc)
{
    uint64_t period;
    int cpuid = cur_cpuid();


    if(cpuid == 0){ /* 核0 是主核ticks 操作只能主核操作*/
        kernel_tick_inc();
    }
/*
    同优先级管理RR暂时先屏蔽掉
    handle_task_timerslice(cpuid);
    handle_delay_task(cpuid);
*/
    xos_check_timer();
    /*
        对休眠任务进行处理
    */
    period = arch_timer_frequecy() / 100;
    WRITE_REG(period,CNTP_TVAL_EL);
}

int  arch64_timer_init(uint64 freq_value)
{

    request_irq(30, timer_isr, 0, "arch_timer", NULL);
    g_period = freq_value;
    WRITE_REG(freq_value,CNTP_TVAL_EL);
    return 0;

}

uint64 second_to_ticks(int seconds)
{
    return seconds*TICK_TIMER_COUNT;
}

uint64 ticks_to_second(uint64 ticks)
{
//  int delta_ticks;
    int seconds = 0;
    if(ticks > TICK_TIMER_COUNT)
    while(ticks - TICK_TIMER_COUNT){
    
        ticks -= TICK_TIMER_COUNT;
        seconds++;
    }
//  delta_ticks = ticks;
    return seconds;
}

