
#include "include/mmu_regs.h"

#include "../../include/mem_layout.h"

tmp     .req    x15

/*
	Add some annotations to explain:
	
	Maybe I should introduce section segments, just like other systems do.
	.section ".entry.text"
	.section ".entry.data"
	.section ".entry.rodata"

	example  lds:
	#define TEXT_START          0x40000000
	#define KERNEL_VA_BASE      0xffffff8000000000
	#define KERNEL_VA_START     (KERNEL_VA_BASE + TEXT_START)
	
	ENTRY(_start)
	
	SECTIONS {

		. = KERNEL_VA_START;   //设置内核起始位置

		.entry.text : {                   //入口代码段
			kernel_text_start = .;
			KEEP(*(.entry.text))
			. = ALIGN(4096);
			kernel_text_end = .;
		} AT(LOADADDR(.entry.text) - KERNEL_VA_BASE)
		
		   .text : {                       //主代码段
			   __text_start = .;
			   *(.text) *(.text.*)		 //所有文本段 
			   . = ALIGN(4096);
			   __text_end = .;
		   }							 // 自动继承偏移 
		   
		   //只读数据
		   .rodata : {
			   __rodata_start = .;
			   *(.rodata) *(.rodata.*)
			   . = ALIGN(4096);
			   __rodata_end = .;
		   }
		   
		   //数据段
		   .data : {
			   __data_start = .;
			   *(.data) *(.data.*)
			   . = ALIGN(4096);
			   __data_end = .;
		   }
		   
		   //BSS段
		   .bss : {
			   __bss_start = .;
			   *(.bss) *(.bss.*) *(COMMON)
			   . = ALIGN(4096);
			   __bss_end = .;
		   }
		   
		   // 内核结束
		   . = ALIGN(4096);
		   __kernel_end = .;
		   
		   __entry_text_size = __entry_text_end - __entry_text_start;
		   __text_size = __text_end - __text_start;
		
	}

*/

.text
.align 16
.global _start

_start:

	mrs x0, mpidr_el1  //mpidr_el1 寄存器中的低 8 位用来表示处理器的核号或 ID
	tst x0, #0xFF
	b.ne    slave_cpu
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
slave_cpu:
	mrs x1, mpidr_el1   // 读取MPIDR_EL1寄存器到x1寄存器
    and x1, x1, #0xFF   // 屏蔽掉不需要的位，只保留最低的8位

	mov     x0, #1     // select SP_EL1
	msr     spsel, x0  // select SP_EL1
	isb
	mrs 	tmp, CurrentEL
	and 	tmp, tmp, #0b1100

	cmp 	tmp, #(1 << 2)
	// 跳转到EL1处理	   EL1: 01
	beq 	slave_el1_entry



slave_el1_entry:
	mrs 	tmp, sctlr_el1
	bic 	tmp, tmp, #SCTLR_ELx_I
	bic 	tmp, tmp, #SCTLR_ELx_C
	bic 	tmp, tmp, #SCTLR_ELx_M
	msr 	sctlr_el1, tmp


	ldr x0, =slave_boot_stacks
	mov x2, #PAGE_SIZE
	add x1, x1, #1 
	mul x1, x1, x2
	add x0, x0, x1

	mov 	sp, x0	//设置从核堆栈SP， 或定义一个二位数组不同的核占用不同空间

	BL      other_boot_main



