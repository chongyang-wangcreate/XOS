/*
    
    * Copyright (C) 1991, 1992  Linus Torvalds
    * License terms: GNU General Public License (GPL) version 2

    development timer:2024
    author :wangchongyang
    
    https://blog.csdn.net/zhoutaopower/article/details/117966631

    https://zhuanlan.zhihu.com/p/592306156
*/

#include "types.h"
#include "list.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "setup_map.h"
#include "mount.h"
#include "mem_layout.h"
#include "mmu.h"
#include "barriers.h"
#include "arch_irq.h"
#include "tick_timer.h"
#include "task.h"
#include "cpu_desc.h"
#include "xos_cache.h"
#include "xos_preempt.h"
#include "xos_fs.h"
#include "xdentry.h"
#include "xnode.h"


#define TICKET_SHIFT 16
static inline void arch_spin_lock(xos_spinlock_t *lock);
static inline void arch_spin_unlock(xos_spinlock_t *lock);


/*
void arch_spin_lock(xos_spinlock_t *lock) {
    uint16_t ticket;
    int result = 0;

    asm volatile(
        "mov %w0, #1\n\t"     // 将立即数1移动到w0寄存器
        ".LABLE_1:\n\t"       // 循环标签
        "ldarb %w1, [%5]\n\t" // 从锁中加载字节到w1寄存器
        "ldaxrh %w2, [%4]\n\t" // 原子加载当前票号到w2寄存器
        "cbnz %w1, .LABLE_1\n\t" // 如果锁已被持有，则继续自旋
        "cmp %w2, %w1\n\t"     // 比较当前票号与锁的状态
        "b.ne .LABLE_1\n\t"    // 如果票号不等于锁的状态，则继续自旋
        "stxrh %w3, %w0, [%5]\n\t" // 原子存储1到锁中，并将写操作结果存储到w3寄存器
        "cbnz %w3, .LABLE_1\n\t"   // 如果写操作失败，则重新尝试
        "dmb ish\n\t"             // 插入内存屏障
        : "=&r" (result),        // Output
          "=&r" (ticket)         // Output
        : "r" (1),                // Input
          "r" (lock),             // Input
          "r" (&lock->tickets.owner),    // Input
          "r" (&lock->slock),     // Input
          "r" (result)            // Temporary
        : "memory"                // Clobbered
    );
}
*/

static inline void arch_spin_lock(xos_spinlock_t *lock)
{
	unsigned int tmp;
	xos_spinlock_t lockval, newval;

	asm volatile(
	/* Atomically increment the next ticket. */
"	prfm	pstl1strm, %3\n"
"1:	ldaxr	%w0, %3\n"
"	add	%w1, %w0, %w5\n"
"	stxr	%w2, %w1, %3\n"
"	cbnz	%w2, 1b\n"
	/* Did we get the lock? */
"	eor	%w1, %w0, %w0, ror #16\n"
"	cbz	%w1, 3f\n"
	/*
	 * No: spin on the owner. Send a local event to avoid missing an
	 * unlock before the exclusive load.
	 */
"	sevl\n"
"2:	wfe\n"
"	ldaxrh	%w2, %4\n"
"	eor	%w1, %w2, %w0, lsr #16\n"
"	cbnz	%w1, 2b\n"
	/* We got the lock. Critical section starts here. */
"3:"
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp), "+Q" (*lock)
	: "Q" (lock->owner), "I" (1 << TICKET_SHIFT)
	: "memory");
}


static inline void arch_spin_unlock(xos_spinlock_t *lock)
{
	asm volatile(
"	stlrh	%w1, %0\n"
	: "=Q" (lock->owner)
	: "r" (lock->owner + 1)
	: "memory");
}


/*
void arch_spin_unlock(xos_spinlock_t *lock) {
    asm volatile(
        "dmb ish\n\t"               // 插入内存屏障
        "ldr %w0, [%1]\n\t"         // 从锁中加载当前票号
        "add %w0, %w0, #1\n\t"      // 将当前票号加1作为下一个等待获取锁的票号
        "str %w0, [%1]\n\t"         // 存储新的票号到锁中
        : "+r" (lock->tickets.owner) // Output: 使用"+r"修饰符表示此操作数为输入输出操作数
        : "r" (&lock->tickets.owner)  // Input
        : "memory"                   // Clobbered
    );
}

*/

void xos_spinlock(xos_spinlock_t *lock)
{
    
    unsigned long flags;
    preempt_disable();
    flags = arch_local_irq_save();
    arch_spin_lock(lock);
    arch_local_irq_restore(flags);
    
}


void xos_unspinlock(xos_spinlock_t *lock)
{
    
    unsigned long flags;
    flags = arch_local_irq_save();
    arch_spin_unlock(lock);
    arch_local_irq_restore(flags);
    preempt_enable();
}


void xos_spinlock_init(xos_spinlock_t *lock)
{
    lock->next = 0;
    lock->owner = 0;
}


