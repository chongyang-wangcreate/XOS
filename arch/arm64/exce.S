#include "./include/vector.h"


#define S_PT_REGS_SIZE 	288 /* sizeof(struct pt_regs)	// */
#define S_LR 			240 /* offsetof(struct pt_regs, regs[30])	// */
#define S_SP 			248 /* offsetof(struct pt_regs, sp)	// */
#define S_PC 			256 /* offsetof(struct pt_regs, pc)	// */
#define S_PSTATE 		264
#define S_X30 			240
#define THREAD_FLAG 	0 

#define S_X0 			0 /* offsetof(struct pt_regs, regs) */
#define S_X8 			64 /* offsetof(struct pt_regs, regs[8]) */

#define THREAD_CPU_CONTEXT 0

.globl err_loop
err_loop: b err_loop


.macro inv_entry el, err_type
save_cur_regs \el
mov	x0, #\err_type
mrs	x1, esr_el1
mrs	x2, elr_el1
bl	abnormal_scene_display
b	err_loop
.endm


#define ENTRY(name) \
.globl name; \
name:

#define END(name) \
.size name, .-name
/*
 * Enable and disable interrupts.
 *
*/

.macro	xos_disable_irq
msr daifset, #2
.endm

.macro	xos_enable_irq
msr daifclr, #2
.endm

/*
   保存异常发生时候的上下文
   保存x0~x29，x30（lr），sp, elr, spsr保存到 栈中
 */
	.macro save_cur_regs, el
	/*
	   SP指向了栈底, S_FRAME_SIZE表示一个栈框的大小.
	   定义一个struct pt_regs来描述一个栈框,
	   用在异常发生时保存上下文.
	 */
	sub sp, sp, #S_PT_REGS_SIZE  //向低地址移动sizeof(struct pt_regs),为将要保存的寄存器现场预留空间,然后将用寄存器现场，x0---x30 sp_el0 等存入pt_regs 结构
	/*
	   保存通用寄存器x0~x29到栈框里pt_regs->x0~x29
	 */
	stp x0, x1, [sp, #16 *0]
	stp x2, x3, [sp, #16 *1]
	stp x4, x5, [sp, #16 *2]
	stp x6, x7, [sp, #16 *3]
	stp x8, x9, [sp, #16 *4]
	stp x10, x11, [sp, #16 *5]
	stp x12, x13, [sp, #16 *6]
	stp x14, x15, [sp, #16 *7]
	stp x16, x17, [sp, #16 *8]
	stp x18, x19, [sp, #16 *9]
	stp x20, x21, [sp, #16 *10]
	stp x22, x23, [sp, #16 *11]
	stp x24, x25, [sp, #16 *12]
	stp x26, x27, [sp, #16 *13]
	stp x28, x29, [sp, #16 *14]
	str x30, [sp, #S_X30]
	mrs     x22, elr_el1
	mrs     x23, spsr_el1
	.if	\el == 0
	mrs	x24, sp_el0  //如果是el0 状态下触发的中断，将sp_el0 存放在pt_regs 结构变量的 sp 变量
	.else
	add	x24, sp, #S_PT_REGS_SIZE //el1 状态下触发中断，将当前sp(由于sp 开始进行了sp - S_FRAME_SIZE),获取中断的sp 值获取需要sp = sp+S_FRAME_SIZE,来进行恢复
	.endif /* \el == 0 */
	str	x22, [sp, #S_PC]				// 存储pc  =x22 到pt_regs 结构中的pt_regs->pc 位置  
	str	x23, [sp, #S_PSTATE]            // 存储pstate   =x23 到pt_regs 结构中的pt_regs->pstate 位置  
	str	x24, [sp, #S_SP]
	.endm
/*
   恢复异常发生时保存下来的上下文
   这里的sp 指的是sp_el1 ,并不是sp_el0,这里需要回复sp_el0 已备返回用户态后使用
 */
	.macro restor_cur_regs, el
	/* 从pt_regs->pc中恢复elr_el1,
	   从pt_regs->pstate中恢复spsr_el1
	 */
	ldr     x22, [sp, #S_PSTATE]       // load SPSR
	msr     spsr_el1, x22
	ldr     x21, [sp, #S_PC]           // load PC
	msr     elr_el1, x21               // set up the return addr

	ldr     x23, [sp, #S_SP]           // load SP
	.if	\el == 0
	msr	sp_el0, x23
	.endif /* \el == 0 */
	ldp     x0, x1, [sp, #16 * 0]
	ldp     x2, x3, [sp, #16 * 1]
	ldp     x4, x5, [sp, #16 * 2]
	ldp     x6, x7, [sp, #16 * 3]
	ldp     x8, x9, [sp, #16 * 4]
	ldp     x10, x11, [sp, #16 * 5]
	ldp     x12, x13, [sp, #16 * 6]
	ldp     x14, x15, [sp, #16 * 7]
	ldp     x16, x17, [sp, #16 * 8]
	ldp     x18, x19, [sp, #16 * 9]
	ldp     x20, x21, [sp, #16 * 10]
	ldp     x22, x23, [sp, #16 * 11]
	ldp     x24, x25, [sp, #16 * 12]
	ldp     x26, x27, [sp, #16 * 13]
	ldp     x28, x29, [sp, #16 * 14]
	ldr     x30, [sp, #S_X30]           // load PC LR
	add     sp, sp, #S_PT_REGS_SIZE           // restore sp
	eret                                    // return to kernel
	.endm


.macro	vec_entry	label
.align	7
b	\label
.endm

/*
 * Exception vectors.
 * kernel_ventry 1, sync_invalid     // Synchronous EL1t  EL1t的中断向量为invalid
 * kernel_ventry 1, irq_invalid      // IRQ EL1t
 * kernel_ventry 1, fiq_invalid      // FIQ EL1t
 * kernel_ventry 1, error_invalid    // Error EL1t

 * kernel_ventry 1, sync       // Synchronous EL1h  EL1h的同步异常
 * kernel_ventry 1, irq        // IRQ EL1h  EL1h的irq
 * kernel_ventry 1, fiq_invalid      // FIQ EL1h EL1h的fiq为invalid
 * kernel_ventry 1, error      // Error EL1h EL1h的error

 * kernel_ventry 0, sync       // Synchronous 64-bit EL0  EL0的同步异常
 * kernel_ventry 0, irq        // IRQ 64-bit EL0  EL0的irq
 * kernel_ventry 0, fiq_invalid      // FIQ 64-bit EL0 EL0的fiq为invalid
 * kernel_ventry 0, error      // Error 64-bit EL0 EL0的error
 */
 
.align 11
.globl exce_vectors
exce_vectors:
		vec_entry	sync_invalid_el1t			// Synchronous EL1t
		vec_entry	irq_invalid_el1t			// IRQ EL1t
		vec_entry	fiq_invalid_el1t			// FIQ EL1t
		vec_entry	error_invalid_el1t			// Error EL1t

		vec_entry	sync_invalid_el1h			// Synchronous EL1h
		vec_entry	el1_irq 					// IRQ EL1h
		vec_entry	fiq_invalid_el1h			// FIQ EL1h
		vec_entry	error_invalid_el1h			// Error EL1h

		vec_entry	el0_sync			// Synchronous 64-bit EL0
		vec_entry	el0_irq 					// IRQ 64-bit EL0
		vec_entry	fiq_invalid_el0_64			// FIQ 64-bit EL0
		vec_entry	error_invalid_el0_64		// Error 64-bit EL0

		vec_entry	sync_invalid_el0_32 		// Synchronous 32-bit EL0
		vec_entry	irq_invalid_el0_32			// IRQ 32-bit EL0
		vec_entry	fiq_invalid_el0_32			// FIQ 32-bit EL0
		vec_entry	error_invalid_el0_32			// Error 32-bit EL0

tsk 	.req	x28 			// current thread_info

.macro get_thread_info, rd
mov 	\rd, sp
//add	  \rd, \rd, #S_PT_REGS_SIZE
and 	\rd, \rd, #~(1<<13 - 1)   // top of stack 8K 对齐操作
.endm

sync_invalid_el1t:
	inv_entry  1, BAD_SYNC_EL1t

irq_invalid_el1t:
	inv_entry  1, BAD_IRQ_EL1t

fiq_invalid_el1t:
	inv_entry  1, BAD_FIQ_EL1t

error_invalid_el1t:
	inv_entry  1, BAD_ERROR_EL1t

sync_invalid_el1h:
	inv_entry  1, BAD_SYNC_EL1h

fiq_invalid_el1h:
	inv_entry  1, BAD_FIQ_EL1h

error_invalid_el1h:
	inv_entry  1, BAD_ERROR_EL1h

sync_invalid_el0_64:
	inv_entry  0, BAD_SYNC_EL0_64

fiq_invalid_el0_64:
	inv_entry  0, BAD_FIQ_EL0_64

error_invalid_el0_64:
	inv_entry  0, BAD_ERROR_EL0_64

sync_invalid_el0_32:
	inv_entry  0, BAD_SYNC_EL0_32

irq_invalid_el0_32:
	inv_entry  0, BAD_IRQ_EL0_32

fiq_invalid_el0_32:
	inv_entry  0, BAD_FIQ_EL0_32

error_invalid_el0_32:
	inv_entry  0, BAD_ERROR_EL0_32



.align 6
el1_irq:
	save_cur_regs 1
	mov x0, sp
	bl xos_irq_el1_entry
	restor_cur_regs 1

.align 6
el0_sync:
	save_cur_regs 0
	xos_enable_irq
	mov x0, sp 
	mrs x1, far_el1
	mrs x2, esr_el1
	bl do_el0_sync   
	#返回用户态之前检测是否需要抢占，后续会添加上，现在没考虑那么多
	#b ret_to_user
	xos_disable_irq
	restor_cur_regs 0

.align 6   //cache line
el0_irq:
	save_cur_regs 0
	mov x0, sp
	bl xos_irq_el0_entry
	b ret_to_user
	restor_cur_regs 0


.align 2
.global ret_from_fork
ret_from_fork:
	bl start_schedule_tail
	cbz	x19, ret_to_user			// not a kernel thread
	mov x0, x21
	blr x19
	ret

/*
	什么情况下才会调用ret_to_user 只有在 用户态发生 异常，陷阱，系统调用，内核函数执行完毕之后退出内核态返回
	用户态时才会调用ret_to_user, 如果被ret_form_fork 第一次调用

	linux 实现方法
	get_thread_info tsk  将child->cpu_context.sp = (unsigned long)child_ptregs;   放入寄存器x28
	
	ret_to_user的第一句就是disable_daif， daif是ARM64 PSTATE代表当前处理器状态的，disable_daif则就会关闭DAIF各个位，D(debug)A(Serror)I(IRQ)F(FIQ)
	ldr    x1, [tsk, #TSK_TI_FLAGS]， 将thread_info.flags的值赋值给X1
	and    x2, x1, #_TIF_WORK_MASK， 将X1的值和_TIF_WORK_MASK的值或，_TIF_WORK_MASK是一个宏，里面包含了很多字段，比如是否需要调度字段_TIF_NEED_RESCHED等
	cbnz    x2, work_pending 当X2的值不等于0时，则跳转到work_pending做一个慢速的ret过程，在do_notify_resume中检查是否要对pending的任务做进一步的操作
	然后调用kernel_exit 0返回到用户空间

*/

ret_to_user:
	bl xos_disable_irq
	mov	x0, sp
	bl end_schedule_tail
	restor_cur_regs 0
	

.align
.global cpu_switch_to
cpu_switch_to:
	add     x8, x0, #THREAD_CPU_CONTEXT
	mov     x9, sp
	stp     x19, x20, [x8], #16
	stp     x21, x22, [x8], #16
	stp     x23, x24, [x8], #16
	stp     x25, x26, [x8], #16
	stp     x27, x28, [x8], #16
	stp     x29, x9, [x8], #16
	str     x30, [x8]

	add     x8, x1, #THREAD_CPU_CONTEXT
	ldp     x19, x20, [x8], #16
	ldp     x21, x22, [x8], #16
	ldp     x23, x24, [x8], #16
	ldp     x25, x26, [x8], #16
	ldp     x27, x28, [x8], #16
	ldp     x29, x9, [x8], #16
	ldr     x30, [x8]
	mov     sp, x9
	ret

/*
	将task_struct A 保留的数据变量 struct cpu_context 结构恢复到x19 到x30 sp 寄存器中
	X30 代表pc ,sp 代表堆栈
*/
.align
.global load_task
load_task:
	add     x8, x0, #THREAD_CPU_CONTEXT
	ldp     x19, x20, [x8], #16
	ldp     x21, x22, [x8], #16
	ldp     x23, x24, [x8], #16
	ldp     x25, x26, [x8], #16
	ldp     x27, x28, [x8], #16
	ldp     x29, x9, [x8], #16
	ldr     x30, [x8]
	mov     sp, x9
	b xos_enable_irq
	ret

