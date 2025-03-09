#ifndef __MMU_REGS_H__
#define __MMU_REGS_H__

/*
     SCTLR_EL1 寄存器各 bit 定义
     位位置   名称  描述
     M (bit[0]) - 异常模型位
     
         0: 使用基于特权级别的异常模型
     
         1: 使用基于虚拟化的异常模型
     
     A (bit[1]) - 地址对齐检查
     
         0: 禁用地址对齐检查
     
         1: 启用地址对齐检查
     
     C (bit[2]) - 数据缓存启用
     
         0: 禁用数据缓存
     
         1: 启用数据缓存
     
     SA (bit[3]) - 短对齐检查
     
         0: 禁用短对齐检查
     
         1: 启用短对齐检查（针对小于字长的访问）
     
     CP15BEN (bit[5]) - CP15屏障指令的启用
     
         0: 禁用CP15屏障指令
     
         1: 启用CP15屏障指令
     
     ITD (bit[7]) - IT指令禁用位
     
         0: 允许IT指令（ARM指令集的Thumb状态下的条件执行）
     
         1: 禁止IT指令
     
     SED (bit[8]) - 小端模式启用
     
         0: 大端模式
     
         1: 小端模式
     
     UMA (bit[9]) - 未对齐内存访问的控制
     
         0: 未对齐内存访问产生异常
     
         1: 未对齐内存访问不产生异常，由硬件自动处理（需要MMU的支持）
     
     EO (bit[10]) - 异常顺序控制
     
         0: 按照正常顺序处理异常
     
         1: 按照相反顺序处理异常（主要用于调试）
     
     FI (bit[11]) - Fast Interrupt Requests 控制位
     
         0: 标准中断请求处理
     
         1: 快速中断请求处理（提高中断响应速度）
     
     U (bit[22]) - 对齐检查控制位（针对未对齐访问）
     
         0: 未对齐访问产生异常（如果SA=0）
     
         1: 未对齐访问不产生异常（如果UMA=1且MMU支持）
     
     WXN (bit[19]) - 可执行内存区域写保护控制位
     
         0: 可执行内存区域可以被写入（如果MMU支持）
     
         1: 可执行内存区域写保护，禁止写入（如果MMU支持）

 */
 
#define SCTLR_ELx_WXN		(1 << 19)
#define SCTLR_EL1_DZE		(1 << 14)
#define SCTLR_ELx_I		    (1 << 12)
#define SCTLR_EL1_FI        (1 << 11)
#define SCTLR_EL1_UMA		(1 << 9)
#define SCTLR_EL1_SED		(1 << 8)
#define SCTLR_EL1_ITD		(1 << 7)
#define SCTLR_EL1_THEE		(1 << 6)
#define SCTLR_EL1_CP15BEN	(1 << 5)
#define SCTLR_EL1_SA0		(1 << 4)
#define SCTLR_ELx_SA		(1 << 3)
#define SCTLR_ELx_C		    (1 << 2)
#define SCTLR_ELx_A		    (1 << 1)
#define SCTLR_ELx_M		    (1 << 0)


/*
 * SPSR_EL3/SPSR_EL2 bits definitions
 */
#define SPSR_EL_LITTLE_ENDIAN		(0 << 9)  // Exception Little-endian
#define SPSR_EL_DEBUG_EXCEPTION	    (1 << 9)  // Debug Exception Masked
#define SPSR_EL_ASYNC_ABORT_MASK	(1 << 8)  // Asynchronous Data Abort Masked
#define SPSR_EL_SYSTEM_ERROR_MASK	(1 << 8)  // System Error Masked
#define SPSR_EL_IRQ_MASK			(1 << 7)  // IRQ Masked
#define SPSR_EL_FIQ_MASK			(1 << 6)  // FIQ Masked
#define SPSR_EL_ARM_A32_MODE		(0 << 5)  // AArch32 Instruction Set A32
#define SPSR_EL_MODE_AARCH64		(0 << 4)  // Exception Taken from AArch64
#define SPSR_EL_MODE_AARCH32		(1 << 4)  // Exception Taken from AArch32
#define SPSR_EL_MODE_SVC			(0x3)     // Exception Taken from SVC Mode
#define SPSR_EL_MODE_HYP			(0xa)     // Exception Taken from HYP Mode
#define SPSR_EL_MODE_EL1H			(5)       // Exception Taken from EL1h Mode
#define SPSR_EL_MODE_EL2H			(9)       // Exception Taken from EL2h Mode

/*
 * HCR_EL2 bits definitions
 */
#define HCR_EL2_RW_AARCH64_MODE	(1 << 31) // EL1 is AArch64 Mode
#define HCR_EL2_DISABLE_HYPERCALL	(1 << 29) // Hypervisor Call Disabled

#endif
