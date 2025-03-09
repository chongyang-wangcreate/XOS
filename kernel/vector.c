/********************************************************
    
    development started:2024
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
#include "setup_map.h"
#include "mem_layout.h"
#include "phy_mem.h"
#include "xos_page.h"
#include "xos_zone.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "printk.h"
#include "error.h"
#include "vector.h"
#include "tick_timer.h"
#include "fs.h"
#include "setup_map.h"
#include "mount.h"
#include "task.h"
#include "schedule.h"
#include "syscall.h"
#include "user_map.h"
#include "cpu_desc.h"

typedef int(*sys_call_fun)(struct pt_regs *regs);
extern void show_pt_regs(const struct pt_regs *regs);
extern void user_page_mapping(uint64_t *pg_dir, void *virt_addr,
                                    uint64_t phy_addr, unsigned long size,
                                    int prot);
/*
    The Exception Class (EC) is part of the ESR, which indicates the type of exception that occurred. The EC is a 6-bit field within the ESR register and is located in bits [31:26] of the ESR.

    Here are the exception classes (EC values) that the ARMv8 architecture defines:

    0x00: Reset (Exception due to reset)
    0x01: Unknown (Synchronous) (This is used for synchronous exceptions that do not fall into any other category)
    0x02: IRQ (Interrupt Request) (Normal interrupt)
    0x03: FIQ (Fast Interrupt Request) (Fast interrupt)
    0x04: SError (System Error) (Indicates a system error exception)
    0x06: IABT (Instruction Abort) (Abort exception caused by a bad instruction fetch)
    0x07: DABT (Data Abort) (Abort exception caused by a bad data access)
    0x08: IRQ (Interrupt Request) (Interrupt service routine handling)
    0x09: FIQ (Fast Interrupt Request) (Fast interrupt service routine handling)
    0x0F: Breakpoint Exception (Indicates a breakpoint exception)

*/
static inline uint64_t get_exce_from_esr(uint64_t esr)
{
    return ((esr >> 26) & 0x3F);
}

static int is_svc_st(uint64_t esr)
{
    return (get_exce_from_esr(esr) == 0x15);
}

static int is_user_data_abort(uint64_t esr)
{
    return (get_exce_from_esr(esr) == 0x24);
}
static int is_kernel_data_abort(uint64_t esr)
{
    return (get_exce_from_esr(esr) == 0x25);
}


static inline uint64 get_syscall_no(struct pt_regs *regs)
{
    /*read AAPCS*/
    return regs->regs[9];
}

void page_fault(struct task_struct *tsk,uint64_t vaddr)
{
    uint64_t phy_addr = (uint64_t)xos_get_phy(0, 0);
    if(tsk == NULL){
        printk(PT_RUN,"%s:%d,vaddr=%lx phyaddr=%x\n\r",__FUNCTION__,__LINE__,vaddr,phy_addr);
    }
    user_page_mapping(tsk->task_pgd, (void *)vaddr,
    phy_addr,
    PAGE_SIZE,
    0);
    printk(PT_RUN,"%s:%d,vaddr=%lx phyaddr=%x\n\r",__FUNCTION__,__LINE__,vaddr,phy_addr);
}

void abnormal_scene_display(int err_type,uint64_t esr,uint64_t exce_address){
    printk(PT_DEBUG,"err_type=%d, ESR= 0x%lx\r\n", err_type, esr);
    printk(PT_DEBUG,"exception class = 0x%lx\r\n", get_exce_from_esr(esr));
    printk(PT_DEBUG,"ELR=0x%lx\r\n",exce_address);
}

/*
    调试加载运行第一个user shell 一直不成功，卡了一个月，非常困惑但是明明切换到用户态了，以为卡死了 
    后续不断思考调试验证，发现是异常部分做的不好，尤其是没有加调试打印信息，增加打印之后，发现一直在进入异常
    并且异常地址是用户态地址，说明用户程序运行了，只是一直在进入异常，无打印时看着以为是卡死
    
    为什么一直进入异常呢？理解这个非常关键，核心原因就是虚拟地址没有做映射
    做映射，所以在不断的进入异常，如果每次产生缺页异常后，给对应的虚拟地址做映射
    你会发现运行一段，当所有地址全部映射完毕之后，data_abort 不在产生

*/
int first = 0;
void do_el0_sync(struct pt_regs *regs,uint64_t addr, uint64_t esr)
{
    uint64 syscallno;
    int ret;

    struct task_struct *cur_task = get_current_task();
    
    printk(PT_RUN,"%s:%d，vaddr=%lx ,esr=%lx\n\r",__FUNCTION__,__LINE__,addr,esr);
    show_pt_regs(regs);
    if (is_svc_st(esr)){
        syscallno = get_syscall_no(regs);

        ret = sys_call_entry(syscallno,regs);
        regs->regs[0] = ret;
    }else if(is_user_data_abort(esr)){
        page_fault(cur_task,addr);

    }else if(is_kernel_data_abort(esr)){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    }

    /*
2024.06.01；PM:20:59
最近几天一直看抢占,网上文章都写的非常好，本来我就想搞个简单系统，先不引入抢占，但是看了抢占，就想引入抢占，然后就这么胡乱写了一通，也是释放压力一种方式
        我自己的想法 如果不使用b ret_to_user
        那么在系统调度处理函数，中断处理函数，异常处理函数退出时(不出现panic)
        如果能执行到这块
        1.检测是否需要抢占
        2.后续愿景，就是判断是否存有pending 信号存在

        如果我使用C 代码 ，
        仿照linux 内核思想，实现系统调用退出工作
        1.check_preempt_schedule(); 处理抢占，抢占成功，本A用户进程被调度出cpu，B用户进程被调度进CPU,A用户进程再次被调度才能处理信号
        效仿内核，如果本A 进程再次被调度进CPU ，是否还要检测抢占标识，如果还需要抢占的话再次将A进程调度出CPU(linux内核这块没有完全搞明白)
        2.check_sig_pendig   set_signal();  处理信号
        。。。。
        那么不管是系统调用，还是异常中断返回都需要调用着两个函数

        关于是否需要抢占，我自己的task_struct 已经定义了taskA->need_switch 标识变量，内核的抢占标识设定是不同调度时机设定的
        我如何来实现自己的抢占呢，设置抢占标志位时机
对于我自己的系统我觉得抢占标识设置有这么几个时刻：
        1. 任务加入cpu_runqueue 队列后，如果当前任务状态ready ,并且优先级比当前进程优先级高，那么设置need_switch = 1
        2. 系统支持RR 时间片调度，时间片等于0，之后，需要设置need_switch = 1
        3. 休眠任务被唤醒(中断产生后中断处理函数wakeup)，并且当前被唤醒的新任务优先级比当前cur_task 优先级高，设置need_switch= 1
检测抢占标志位的时机有哪些：
        1. 中断退出时需要检测need_switch 标志位，如果置位则抢占执行int_schedule。
        2. 系统调度返回时需要检测need_switch 标志，如果置位则抢占调用执行schedule。
        3. 内核还支持软中断，但是现在我根本没有软中断(先画个大饼吧)
        4. sleep 睡眠超时被唤醒timer_isr

    
        linux  内核如何做：
        ret_to_user是系统调用、中断、异常触发处理完成后都会调用的函数，调用路径(arm64)：
        ret_to_user -> work_pending -> do_notify_resume(arch/arm64/kernel/signal.c) -> do_signal -> get_signal、handle_signal -> setup_rt_frame
        linux 内核是这样做的

        系统调用返回用户空间和中断返回用户空间都是通过ret_to_user函数，所以判断执行抢占的部分是相同的。

        文件:arch/arm64/kernel/entry.S
          函数调用：
          	ret_to_user
          	  work_pending
                    do_notify_resume
                      schedule() 

        arm64中syscall通过同步异常el0_sync->el0_svc->el0_svc_handler处理完成后，ret_to_user返回用户空间。
        参考中断返回用户空间。
    */
}

