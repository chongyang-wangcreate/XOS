
#include "include/mmu_regs.h"

#include "../../include/mem_layout.h"

tmp     .req    x15

.text
.align 16
.global _start

_start:

	mrs x0, mpidr_el1  //mpidr_el1 寄存器中的低 8 位用来表示处理器的核号或 ID
	tst x0, #0xFF
	b.ne    other_cpu
	# initialize stack pointers for svc modes
	
	mov     x0, #1     // select SP_EL1   当 SPSel 为 0 时使用SP_EL0 当SPSel 为1时，使用SP_EL1
	msr     spsel, x0
	isb

	mrs     tmp, CurrentEL
    and     tmp, tmp, #0b1100
    
    cmp     tmp, #(1 << 2)
    // 跳转到EL1处理      EL1: 01
    beq     el1_entry
    
    cmp     tmp, #(2 << 2)
    // to do 跳转到EL2处理	EL2: 10
	

el1_entry:
	// 关闭EL1的指令和数据缓存和MMU
	mrs 	tmp, sctlr_el1
	bic 	tmp, tmp, #SCTLR_ELx_I
	bic 	tmp, tmp, #SCTLR_ELx_C
	bic 	tmp, tmp, #SCTLR_ELx_M
	msr 	sctlr_el1, tmp
	
	adrp    x0, _boot_main_stack_top  //set init stack
	mov     sp, x0

	LDR     x1, =bss_start
	LDR     x2, =bss_end
	
clear_bss:
    CMP     x1, x2              // 比较当前地址 (x1) 和结束地址 (x2)
    B.GE    end_clear_bss       // 如果 x1 >= x2，跳转到清除结束

    // 将当前地址指向的位置置为 0
    MOV     x3, #0              // 将 x3 设为 0（要写入的值）
    STR    x3, [x1]            // 将 0 存储到地址 [x1] 所指的位置

    ADD     x1, x1, #0x08          // 将 x1 增加 1，指向下一个字节
    B       clear_bss           // 继续清除下一个字节

#B.LE

#标号：小于等于(if判断)

#B.LT

#标号：小于等于(do while)

#B.GT

#标号：小于等于(while do)

#B.GE

#标号：大于等于(for)



end_clear_bss:
	BL      boot_main
	B .


.global set_virt_stack
set_virt_stack:
	MOV     x0, sp
	LDR     x1, = VA_KERNEL_START
	ADD     sp, x1, x0
	RET

/*
	如下从核代码只是示例
*/
other_cpu:
	mrs x1, mpidr_el1   // 读取MPIDR_EL1寄存器到x1寄存器
    and x1, x1, #0xFF   // 屏蔽掉不需要的位，只保留最低的8位

	mov     x0, #1     // select SP_EL1
	msr     spsel, x0  // select SP_EL1
	isb
	mrs 	tmp, CurrentEL
	and 	tmp, tmp, #0b1100

	cmp 	tmp, #(1 << 2)
	// 跳转到EL1处理	   EL1: 01
	beq 	other_el1_entry

	/*
		主从核可使用相同页表
		各核配置各自的MMU

		set vector 主核从核公用中断向量表，从核不需要再初始化

		从核主要工作就是配置自己的页表，或者主核从核公用一套页表，
		但是配置各自的ttbr0_el1 ,ttbr1_el1
		gic 应该是公用的主核配置就可以 
		配置自己的timer 定时器
		

		gic 相关资料：
		GIC 是 ARM 架构中用于管理中断的硬件组件，负责接收、分发和处理中断。它的主要功能包括：

    	接收来自外设的中断信号，将中断分发到合适的 CPU 核,提供中断优先级和屏蔽功能。
    	主核的 GIC 配置:
    	主核在系统启动时负责全局的 GIC 初始化，包括：GIC Distributor 初始化,GIC CPU Interface 初始化

    	从核的 GIC 配置:从核在启动时，通常不需要重新配置 GIC Distributor，因为这是全局的，由主核完成。但从核需要初始化自己的 GIC CPU Interface，以确保能够接收和处理中断
    	核间中断（IPI）的配置

		
		CSDN 有很多不错的文章：
		
		ARM多核启动的基本流程。在ARMv8系统中，通常主核（Primary Core，如CPU0）会首先启动，
		执行初始化代码，设置系统环境，然后负责启动从核。从核在启动时通常处于休眠状态，需要主核通过某种方式唤醒它们

	*/

other_el1_entry:
	mrs 	tmp, sctlr_el1
	bic 	tmp, tmp, #SCTLR_ELx_I
	bic 	tmp, tmp, #SCTLR_ELx_C
	bic 	tmp, tmp, #SCTLR_ELx_M
	msr 	sctlr_el1, tmp


	adrp	x0, _boot_main_stack_top  
	add x0,	x0, #8192
	mov 	sp, x0	//设置从核堆栈SP， 或定义一个二位数组不同的核占用不同空间

	BL      other_boot_main



