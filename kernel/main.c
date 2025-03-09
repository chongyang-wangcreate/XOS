/********************************************************
    development timer:2023.11
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2023-2027 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    

    Working day and night, after a year and a half, 
    the operating system has finally been released.

    My personal abilities are limited and time is limited. 
    I am aware that there are many problems with the current system 
    and look forward to the participation of talented individuals.

********************************************************/
#include "types.h"
#include "string.h"
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
#include "xos_zone.h"
#include "user_map.h"
#include "syscall.h"
#include "xos_dev.h"
#include "xos_mutex.h"
#include "uart.h"
#include "xos_page.h"
#include "xos_bus.h"
#include "workqueue.h"
#include "arch64_irq.h"

/********************************************************************************************

    2024：3.3    当前版本只考虑支持单核，多核心需要考虑的问题比较多，实现基本功能之后，再扩展支持多核
    2024.3.27    做了一下改进引入了CPUID，，多核还设计到tick 中断，时钟同步，方便以后可以扩展支持多核，但是当前还是单核系统，多核实现还需要等待
    最兴奋的就是一直存在问题的mmu 配置终于成功了，对后续的开发太关键了。
    2024.4.4 11:11 .ticks实现已经加入，需要对任务切换做进一步处理，同优先级之间可以进行RR 调度了，注册任务时
    给每个任务一个初始时间片，每次产生一个tick 中断就将时间片减少1，如果时间片为0，则将任务放到队列尾部，并补充时间片
    task->init_timer_slice 初始固定值 ，  task->timer_slice_count 动态变化值，最小值0

    2024.0602：开发操作系统必须有毅力，有激情，并且是在对系统有充分理解的情况下，
    写之前有很好的基础非常必要，我为此准备了好几年，现在终于有一点点成果了,非常小的成果
************************************************************************************************/

extern void * exce_vectors;
extern void * k_bss_start;
extern void * k_bss_end;


#define wfi()       asm volatile("wfi" : : : "memory")

char idle_stack[1024];




void clear_kbss (void)
{
    printk(PT_DEBUG,"clear_bss%s:%d\n\r",__FUNCTION__,__LINE__);
    memset(&k_bss_start, 0x00, (unsigned long)&k_bss_end-(unsigned long)&k_bss_start);
}



void kernel_idle(char *array)
{
    while(1){
        printk(PT_RUN,"idle idle idle%s:%d\n\r",__FUNCTION__,__LINE__);
        /*
            to do
        */
    }
}


void kernel_thread1(char *array)
{
    while(1){
        printk(PT_RUN,"%s enter,XXXXXXXXXXXXXXXX,%d\n\r",__func__,__LINE__);
        xos_sleep_ticks(15);
    }

}

void kernel_thread2(char *array)
{
    printk(PT_DEBUG,"%s:%d kernel_thread2 enter\n\r",__func__,__LINE__);
    while(1)
        {
        printk(PT_RUN,"%s:%d run \n\r",__func__,__LINE__);
        xos_sleep_ticks(10);
    }
}


void start_init(char *array)
{

#ifndef CONFIG_VFS
    xos_vfs_init();
    xos_mount_fs();
    xos_page_cache_init();
#endif
    xos_workqueue_init();
#ifndef CONFIG_DIRVER
    xos_bus_init();
#endif
    
#ifndef CONFIG_DEV
     xos_init_deivices(); 
#endif

    process_create();
    while(1)
    {
        xos_sleep_ticks(10);
    }
}


void xos_set_vector_entry()
{
    uint64  val64;
    val64 = (uint64)&exce_vectors;
    asm("msr vbar_el1, %[v]": :[v]"r" (val64):);
}
void kernel_init (void)
{
    clear_kbss();
    xos_uart_init();

    xos_zone_init();
    mem_cache_init();
    /*
        to do 
        printk_debug(){level = PT_DEBUG};
        printk_run(){level = PT_RUN};
    */
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    all_phys_linear_map();
    
    test_buddy();
    
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    local_irq_disable();
    xos_irq_init();
    xos_uart_irq_init();
    xos_set_vector_entry();

    xos_timer_init();

    list_init(&task_global_list);
    list_init(&pend_global_list);
    cpu_desc_init();
    /*
        已引入128优先级
        后续实现多种调度策略，当前是实现越简单越好
    */
    xos_thread_create(1, (unsigned long)&kernel_thread2, 7);
    xos_thread_create(5, (unsigned long)&kernel_idle, 6);
    xos_thread_create(2, (unsigned long)&start_init, 6);

    xos_cli();
    load_proc_flags = 1;
    load_first_task();

    while(1){
        wfi();
    }
}
