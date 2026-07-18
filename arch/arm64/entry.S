/* 直接在entry.S中定义所有常量，避免头文件问题 */


.equ VA_KERNEL_START,       0xFFFFFF8000000000
.equ PHYS_MEM_START,        0x40000000
.equ DEVICE_MEM_START,      0x08000000      /* 注意：应该是0x08000000，不是0x09000000 */
.equ DEVICE_MEM_END,        0x3FFFFFFF
.equ UART0_PHYS,            0x09000000
.equ PAGESIZE,              4096

/* 页表项标志位 */
.equ PTE_VALID,             0x0000000000000001
.equ PTE_TABLE,             0x0000000000000002
.equ PTE_AF,                0x0000000000000400
.equ PTE_UXN,               0x0040000000000000
.equ PTE_PXN,               0x0020000000000000
.equ PTE_INDX_DEVICE,       0x0000000000000000
.equ PTE_INDX_NORMAL,       0x0000000000000004


.equ PTE_KERNEL_FLAGS,      (PTE_AF | PTE_INDX_NORMAL | PTE_VALID)  /* 0x405 */
.equ PTE_DEVICE_FLAGS,      (PTE_AF | PTE_INDX_DEVICE | PTE_VALID)  /* 0x401 */
.equ PTE_TABLE_FLAGS,       (PTE_TABLE | PTE_VALID)                 /* 0x3 */


.equ TCR_T0SZ,              (64 - 39)           /* 25 */
.equ TCR_T1SZ,              ((64 - 39) << 16)   /* 25 << 16 = 0x01900000 */
.equ TCR_TG0_4KB,           (0 << 14)           /* 0 */
.equ TCR_TG1_4KB,           (2 << 30)           /* 0x80000000 */
.equ TCR_IPS,               (0 << 32)           /* 40位物理地址 */
.equ TCR_VALUE,             (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4KB | TCR_TG1_4KB | TCR_IPS)

/* MAIR 设置 */
.equ MAIR_DEVICE_nGnRnE,    0x00
.equ MAIR_NORMAL_NC,        0x44
.equ MAIR_VALUE,            ((MAIR_NORMAL_NC << 8) | MAIR_DEVICE_nGnRnE)  /* 0x4400 */
.equ CPACR_EL1_FPEN,        (3 << 20)

.section ".text.boot"

.global _start

_start:
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cbz x1, kernel_start
    b secondary_entry

kernel_start:
    ldr x0, =CPACR_EL1_FPEN
    msr cpacr_el1,x0
    isb

    ldr x1, =__bss_start_phys
    ldr w2, =__bss_size
1:  cbz w2, 2f
    str xzr, [x1], #8
    sub w2, w2, #1
    b 1b
2:

    /* ========== 用户页表：恒等映射 ========== */
    adrp x0, l2_user_pgt
    
    /* 映射物理地址 0x40000000 到 0x40000000（恒等映射） */
    ldr x1, =PHYS_MEM_START       /* 0x40000000 */
    ldr x2, =kernel_end_phys
    sub x2, x2, #1
    
    /* 39位地址空间：L2索引是位[29:21]，右移21位 */
    lsr x3, x1, #21          /* 正确的L2索引计算：位[29:21] */
    and x3, x3, #0x1ff
    lsr x4, x2, #21
    and x4, x4, #0x1ff
    
    /* 设置页表项 */
    ldr x5, =PTE_KERNEL_FLAGS
    orr x6, x1, x5
    
l2_upgt_loop:
    str x6, [x0, x3, lsl #3]
    add x3, x3, #1
    add x6, x6, #0x200000
    cmp x3, x4
    b.ls l2_upgt_loop
    
    /* L1 用户页表 - 39位地址空间：L1索引是位[38:30]，右移30位 */
    adrp x0, l1_user_pgt
    ldr x1, =PHYS_MEM_START
    lsr x3, x1, #30          /* 正确的L1索引计算：位[38:30] */
    and x3, x3, #0x1ff
    ldr x4, =PTE_TABLE_FLAGS
    adrp x5, l2_user_pgt
    orr x6, x5, x4
    str x6, [x0, x3, lsl #3]
    
    /* ========== 内核页表 ========== */
    /* 设备内存 0x08000000 - 0x3FFFFFFF */
    adrp x0, l2_kernel_pgt0
    
    ldr x2, =DEVICE_MEM_START
    ldr x3, =DEVICE_MEM_END
    ldr x4, =VA_KERNEL_START
    
    /* 计算虚拟地址 */
    add x5, x2, x4           /* 虚拟地址起始: 0xFFFFFF8008000000 */
    add x6, x3, x4           /* 虚拟地址结束: 0xFFFFFF803FFFFFFF */
    
    /* 39位地址空间：L2索引是位[29:21]，右移21位 */
    lsr x7, x5, #21
    and x7, x7, #0x1ff
    lsr x8, x6, #21
    and x8, x8, #0x1ff
    
    /* 设置页表项 */
    ldr x10, =PTE_DEVICE_FLAGS
    orr x9, x2, x10
    
    /* 修正：计算正确的物理地址 */
mov x11, x7                  /* x11 = L2起始索引 (0x40) */
lsl x11, x11, #21            /* x11 = 0x40 << 21 = 0x08000000 */
orr x9, x11, x10             /* x9 = 0x08000000 | 0x401 = 0x08000401 */

l2pgt_loop0:
    str x9, [x0, x7, lsl #3]
    add x7, x7, #1
    add x9, x9, #0x200000
    cmp x7, x8
    b.ls l2pgt_loop0
    
    /* 内核代码 0x40000000 - kend */
    adrp x1, l2_kernel_pgt1
    ldr x2, =PHYS_MEM_START
    ldr x3, =kernel_end_phys
    sub x3, x3, #1
    
    add x5, x2, x4
    add x6, x3, x4
    
    /* 39位地址空间：L2索引是位[29:21]，右移21位 */
    lsr x7, x5, #21
    and x7, x7, #0x1ff
    lsr x8, x6, #21
    and x8, x8, #0x1ff
    
    ldr x10, =PTE_KERNEL_FLAGS
    orr x9, x2, x10
    
l2pgt_loop1:
    str x9, [x1, x7, lsl #3]
    add x7, x7, #1
    add x9, x9, #0x200000
    cmp x7, x8
    b.ls l2pgt_loop1
    
    /* L1 内核页表 - 39位地址空间：L1索引是位[38:30]，右移30位 */
    adrp x0, l1_kernel_pgt
    
    /* 第一项: 设备内存 (0xFFFFFF8008000000) */
    ldr x1, =DEVICE_MEM_START
    ldr x2, =VA_KERNEL_START
    add x3, x1, x2           /* 虚拟地址: 0xFFFFFF8008000000 */
    lsr x5, x3, #30          /* 正确的L1索引计算：位[38:30] */
    and x5, x5, #0x1ff
    
    ldr x6, =PTE_TABLE_FLAGS
    adrp x7, l2_kernel_pgt0
    orr x7, x7, x6
    str x7, [x0, x5, lsl #3]
    
    /* 第二项: 内核代码/数据 (0xFFFFFF8040000000) */
    ldr x1, =PHYS_MEM_START
    add x3, x1, x2           /* 虚拟地址: 0xFFFFFF8040000000 */
    lsr x5, x3, #30          /* 正确的L1索引计算：位[38:30] */
    and x5, x5, #0x1ff
    
    adrp x8, l2_kernel_pgt1
    orr x8, x8, x6
    str x8, [x0, x5, lsl #3]
    
    /* 验证UART映射计算 */
    /* UART地址: 0x09000000 -> 虚拟地址: 0xFFFFFF8009000000 */
    ldr x1, =UART0_PHYS
    add x3, x1, x2           /* 0xFFFFFF8009000000 */
    
    /* 计算L1索引：位[38:30] */
    lsr x4, x3, #30
    and x4, x4, #0x1ff       /* 应该是0x100 (256) */
    
    /* 计算L2索引：位[29:21] */
    lsr x5, x3, #21
    and x5, x5, #0x1ff       /* 应该是0x80 (128) */
    
    /* 验证：UART应该在l2kpgt0表中，L1索引=0x100，L2索引=0x80 */
    
    /* ========== 配置 MMU ========== */
    adrp x0, l1_user_pgt
    msr ttbr0_el1, x0
    adrp x0, l1_kernel_pgt
    msr ttbr1_el1, x0
    
    ldr x0, =TCR_VALUE
    msr tcr_el1, x0
    
    ldr x1, =MAIR_VALUE
    msr mair_el1, x1
    
    isb
    tlbi vmalle1
    dsb sy
    isb

    mrs x0, sctlr_el1
    orr x0, x0, #1
    msr sctlr_el1, x0
    isb
    

    ldr x1, =p_entry_virt
    br x1

.section ".text"
.global p_entry_virt
p_entry_virt:
    ldr x0, =CPACR_EL1_FPEN
    msr cpacr_el1,x0
    isb

    mrs x0, mpidr_el1
    and x0, x0, #0xff
    add x0, x0, #1
    mov x1, #PAGESIZE
    mul x0, x0, x1
    ldr x2, =main_stack_top
    add x2, x2, x0
    mov sp, x2
    bl boot_main

secondary_entry:
    adrp x0, l1_user_pgt
    msr ttbr0_el1, x0
    adrp x0, l1_kernel_pgt
    msr ttbr1_el1, x0
    ldr x0, =TCR_VALUE
    msr tcr_el1, x0
    ldr x1, =MAIR_VALUE
    msr mair_el1, x1
    
    ldr x1, =p_entry_virt
    mrs x0, sctlr_el1
    orr x0, x0, #1
    msr sctlr_el1, x0
    br x1

.global set_virt_stack
set_virt_stack:
    mov x0, sp
    ldr x1, =VA_KERNEL_START
    add sp, x1, x0
    ret

.global hang
hang:
    wfe
    b hang