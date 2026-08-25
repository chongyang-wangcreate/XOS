/* 直接在entry.S中定义所有常量，避免头文件问题 */


.equ VA_KERNEL_START,       0xFFFFFF8000000000
.equ PHYS_MEM_START,        0x40000000
.equ BOOR_EARLY_MAP_END,     0x60000000
.equ DEVICE_MEM_START,      0x08000000      /* 注意：应该是0x08000000，不是0x09000000 */
.equ DEVICE_MEM_END,        0x3FFFFFFF
.equ UART0_PHYS,            0x09000000
.equ PAGESIZE,              4096

/* 页表项标志位 */
.equ L1_SHIFT,               30
.equ L2_SHIFT,               21
.equ PT_INDEX_MASK,          0x1ff
.equ BLOCK_SIZE,             0x200000
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

/* MAIR set */
.equ MAIR_DEVICE_nGnRnE,    0x00
.equ MAIR_NORMAL_NC,        0x44
.equ MAIR_VALUE,            ((MAIR_NORMAL_NC << 8) | MAIR_DEVICE_nGnRnE)  /* 0x4400 */
.equ CPACR_EL1_FPEN,        (3 << 20)

.macro enable_fp
    ldr x0, =CPACR_EL1_FPEN
    msr cpacr_el1,x0
    isb
.endm

.macro clear_bss
    ldr x1, =__bss_start_phys
    ldr w2, =__bss_size
1:
    cbz w2,2f
    str xzr,[x1],#8
    sub w2,w2,#1
    b 1b
2:
.endm

.macro map_l2_blocks table,phys_start,phys_end,virt_base,flags
    adrp x0,\table
    ldr x1,=\phys_start
    ldr x2,=\phys_end
    ldr x3,=\virt_base
    ldr x4,=\flags

    add x5,x1,x3
    add x6,x2,x3
    lsr x7,x5,#L2_SHIFT
    and x7,x7,#PT_INDEX_MASK
    lsr x8,x6,#L2_SHIFT
    and x8,x8,#PT_INDEX_MASK
    orr x9,x1,x4
1:
    str x9,[x0,x7,lsl #3]
    add x7,x7,#1
    add x9,x9,#BLOCK_SIZE
    cmp x7,x8
    b.ls 1b
.endm
.macro install_l1_table l1_table,l2_table,virt
    adrp x0,\l1_table
    ldr x1,=\virt
    lsr x2,x1,#L1_SHIFT
    and x2,x2, #PT_INDEX_MASK
    adrp x3,\l2_table
    ldr x4,=PTE_TABLE_FLAGS
    orr x3,x3,x4
    str x3,[x0,x2,lsl #3]
.endm

.macro install_boot_page_tables
    map_l2_blocks l2_user_pgt,PHYS_MEM_START,(BOOR_EARLY_MAP_END -1),0,PTE_KERNEL_FLAGS
    install_l1_table l1_user_pgt,l2_user_pgt,PHYS_MEM_START

    map_l2_blocks l2_kernel_pgt0,DEVICE_MEM_START,DEVICE_MEM_END,VA_KERNEL_START,PTE_DEVICE_FLAGS
    map_l2_blocks l2_kernel_pgt1,PHYS_MEM_START,(BOOR_EARLY_MAP_END -1),VA_KERNEL_START,PTE_KERNEL_FLAGS

    install_l1_table l1_kernel_pgt,l2_kernel_pgt0,(VA_KERNEL_START + DEVICE_MEM_START)
    install_l1_table l1_kernel_pgt,l2_kernel_pgt1,(VA_KERNEL_START + PHYS_MEM_START)
.endm

.macro set_mmu_regs
    adrp x0,l1_user_pgt
    msr ttbr0_el1,x0
    adrp x0,l1_kernel_pgt
    msr ttbr1_el1,x0

    ldr x0,=TCR_VALUE
    msr tcr_el1,x0
    ldr x0,=MAIR_VALUE
    msr mair_el1,x0 
.endm

.macro enable_mmu
    isb
    tlbi vmalle1
    dsb sy
    isb
    mrs x0,sctlr_el1
    orr x0,x0,#1
    msr sctlr_el1,x0
    isb
.endm
.section ".text.boot"

.global _start

_start:
    mov x20, x0
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cbz x1, primary_start
    b secondary_entry

primary_start:
    enable_fp
    clear_bss
    install_boot_page_tables
    set_mmu_regs
    enable_mmu
    
    ldr x1, =p_entry_virt
    br x1

secondary_entry:
    set_mmu_regs
    enable_mmu

    ldr x1, =p_entry_virt
    br x1

.section ".text"
.global p_entry_virt
p_entry_virt:
    enable_fp
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    add x0, x0, #1
    mov x1, #PAGESIZE
    mul x0, x0, x1
    ldr x2, =main_stack_top
    sub x2, x2, x0
    mov sp, x2
    mov x0, x20
    bl boot_main


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