#ifndef __MMU_H__
#define __MMU_H__

#include "mmu_regs.h"
/*

    aarch64 支持四种格式的entry
每个entry是 64 位，低两位确定entry的类型。
请注意，某些table entry仅在特定级别有效。table的最大级别数是四级，这就是为什么没有level3（或第四级）表的表描述符的原因。同样地，第0级也没有块描述符或页描述符。因为第0级 entry覆盖了很大的虚拟地址空间区域，因此在level0允许块是没有意义的。
转换粒度是可以描述的最小的内存块。AArch64 支持三种不同的粒度大小：4KB、16KB 和 64KB。

处理器支持的粒度是自定义的并由 ID_AA64MMFR0_EL1 保存。所有Arm Cortex-A处理器都支持4KB和64KB。选择的粒度是最高level table中描述的大小。

一个处理器所支持的粒度大小是ID_AA64MMFR0_EL1定义的，所有Arm Cortex-A处理器都支持4KB和64KB。选择的颗粒是可以在最新级别表中描述的最小块。也可以描述更大的块。此表显示了基于所选粒度的每个级别表的不同块的尺寸：


*/
#define PT_ENTRY_VALID         0x01
#define PT_ENTRY_NEXT_BLOCK    0x00
#define PT_ENTRY_TABLE         0x02      //属于不同的Level,(block desc) (level0 leve1 level2),中间层level
#define PT_ENTRY_PAGE          0x02      //属于不同的level,最后一个Level page descriptors(leve3)
#define PT_ENTRY_NEXT_TABLE    0x03
#define PT_ENTRY_IGNOR	       0x00


#define PGD_TABLE (1UL << 1)  // 一级页表项标志，指向下一级页表
#define PGD_VALID (1UL << 0)  // 一级页表项有效
#define PGD_ADDR_MASK (~((1UL << 12) - 1))  // 用于提取页表的物理地址，去除低12位（对应页对齐）

#define PMD_TABLE (1UL << 1)  // 二级页表项标志，指向下一级页表
#define PMD_VALID (1UL << 0)  // 二级页表项有效
#define PMD_ADDR_MASK (~((1UL << 21) - 1))  // 用于提取页表的物理地址，去除低21位（对应页对齐）

#define PTE_VALID (1UL << 0)  // 三级页表项有效
//#define PTE_ADDR_MASK (~((1UL << 12) - 1))  // 用于提取物理地址，去除低12位（对应页对齐


/*

    attrindex[2:0]  bits[4:2]
    Memory attributes index field, for the MAIR_ELx,
    attrIndex只是一个索引，AArch64下有8组Attr，用户可以自己定义和选择组合，并通过MAIR_ELx来配置

    NORMAL MEMORY
    DEVICE MEMORY

     ldr x5, =MAIR(0x00, MT_DEVICE_nGnRnE) | \
    MAIR(0x04, MT_DEVICE_nGnRE) | \
    MAIR(0x0c, MT_DEVICE_GRE) | \
    MAIR(0x44, MT_NORMAL_NC) | \
    MAIR(0xff, MT_NORMAL)
    msr mair_el1, x5

页表中的memory attribute的信息并非直接体现在descriptor中的bit中，而是通过了一个间接的手段。描述符中的AttrIndx[2:0]是一个index，可以定位到8个条目，而这些条目就是保存在MAIR_EL1（Memory Attribute Indirection Register (EL1)）中。对于ARM64处理器，linux kernel定义了下面的index：

#define MT_DEVICE_nGnRnE 0
#define MT_DEVICE_nGnRE 1
#define MT_DEVICE_GRE 2
#define MT_NORMAL_NC 3
#define MT_NORMAL 4

*   n = AttrIndx[2:0]
*          n   MAIR
*   DEVICE_nGnRnE  000 00000000
*   DEVICE_nGnRE   001 00000100
*   DEVICE_GRE     010 00001100
*   NORMAL_NC      011 01000100
*   NORMAL         100 11111111

NC是no cache，也就是说MT_NORMAL_NC的memory是normal memory，但是对于这种类型的memory的访问不需要通过cache系统。这些index用于页表中的描述符中关于memory attribute的设定，对于初始化阶段的页表都是被设定成MT_NORMAL。
    Linux预定义的6种内存属性
第一个是MT_DEVICE_nGnRnE 代表是设备内存，必须保证严格按照代码中的访问顺序来，不允许合并对内存的访问，不允许对指令重排，写操作的ack必须来自最终的目的地而不是中间的write buffer。

第二个是MT_DEVICE_nGnRE 必须保证严格按照代码中的访问顺序来，不允许合并对内存的访问，不允许对指令重排，但是写操作的ack不必须来自最终的目的地而不是中间的write buffer。

第三个 MT_DEVICE_nGRE是不能对内存访问合并，但是指令可以重排，ack不必须来自最终的目的地。

第四个MT_DEVICE_GRE 是对内存访问可以合并，指令可以重排，ack不必须来自最终的目的地。

第五个 MT_NORMAL_NC是不带cache的内存。

第六个MT_NORMAL 是正常的内存。

第七个MT_NORMAL_WT 是带 write Through的。

#define MEM_ATTR_IDX_0	(0 << 2)
#define MEM_ATTR_IDX_1	(1 << 2)
#define MEM_ATTR_IDX_2	(2 << 2)
#define MEM_ATTR_IDX_3	(3 << 2)
#define MEM_ATTR_IDX_4	(4 << 2)
#define MEM_ATTR_IDX_5	(5 << 2)
#define MEM_ATTR_IDX_6	(6 << 2)
#define MEM_ATTR_IDX_7	(7 << 2)
*/


#define MT_DEVICE_nGnRnE 0
#define MT_DEVICE_nGnRE 1
#define MT_DEVICE_GRE 2
#define MT_NORMAL_NC 3
#define MT_NORMAL 4

#define NON_SECURE_PA (1 << 5)
#define PTE_INDX(idx) (((idx) & 7) << 2)


#define PT_ATTRINDX(t)		((t) << 2)
/*0x200193219*/
#define TCR_T0SZ_OFFSET  0   

#define TCR_T1SZ_OFFSET 16

#define TCR_TTBR0_OFFSET 12
#define TCR_IRGN0_OFFSET 8

#define TCR_IPS_OFFSET   32

#define TCR_T0SZ(x)  (((64) - (x)) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ(x)  (((64) - (x)) << TCR_T1SZ_OFFSET)
#define TCR_TTBR0_SH(x) (((x)) << TCR_TTBR0_OFFSET) 
#define TCR_IRGN0(x)      ((x)<<TCR_IRGN0_OFFSET)

#define TCR_IPS(x)      ((unsigned long)(x) << TCR_IPS_OFFSET)



#define PTE_DEVICE  PTE_INDX(MT_DEVICE_nGnRnE)

/*
    AP[2:1],bits[7:6]  Data access Permissions bits

    Data access permissions   数据访问权限 
         EL1/EL2              EL0(USER)
    00   read/write           none
    01   read/write           read/write
    10   read-only            none
    11   read-only            read-only


    XN (Execute Never)：控制是否允许执行内存区域的代码。

    XN = 0：允许执行该区域的代码。
    XN = 1：禁止执行该区域的代码。这个选项用于防止代码注入攻击。
    AP (Access Permissions)：访问权限位，控制对内存区域的读写权限。

    ARMv8 页表项的 AP 字段通常是 2 位或 3 位（取决于具体的页表层级和实现），其具体设置如下：

    AP[1:0]：这两个位用于控制对内存的访问权限，通常定义为：

    00: 无访问权限（No access），即禁止访问该内存。
    01: 只读访问（Read-only），即只能读取数据，不能写入。
    10: 读写访问（Read/write），即可以读取和写入数据。
    11: 特权访问（Privileged access），通常表示只有操作系统等特权代码可以访问，用户空间代码无法访问。


    example

#define PAGE_SIZE 4096 // 4KB 页大小
#define PAGE_TABLE_ENTRY_SIZE 8 // 64 位页表项

    // 设置页表项,权限设定位AP
    void set_page_table_entry(uint64_t *entry, uint64_t base_addr, uint8_t ap, uint8_t xn) {
        *entry = base_addr & ~((1 << 12) - 1); // 设置页表项的地址，清除最低的 12 位
        *entry |= (ap << 6);  // 设置访问权限（AP[1:0]）
        *entry |= (xn << 53); // 设置 XN（执行禁止）
        *entry |= (1 << 0);   // 设置有效位 (V)
    }

    // 使用上述函数初始化一个页表项
    void setup_page_table() {
        uint64_t page_table[L2_TABLE_SIZE];
        uint64_t entry;

        // 设置地址 0x10000 的页表项为读写权限，禁止执行
        set_page_table_entry(&entry, 0x10000, 0b10, 1);
        page_table[0] = entry;

        // 启用 MMU
        enable_mmu();
}
*/



#define PTE_USER		(1UL << 6)
#define PTE_RDONLY		(1UL << 7)

#define PTE_AP(ap)  (((ap) & 3) << 6)
#define PTE_URW       PTE_AP(1)
#define PTE_RO        PTE_AP(2)
#define PTE_URO       PTE_AP(3)

/*
    AP[2:1],bits[7:6]  Data access Permissions bits

    Data access permissions   数据访问权限 
         EL1/EL2              EL0(USER)
    00   read/write           none
    01   read/write           read/write
    10   read-only            none
    11   read-only            read-only
*/
#define PG_RW_EL1     (0 << 6)
#define PG_RW_EL1_EL0   (1 << 6)
#define PG_RO_EL1     (2 << 6)
#define PG_RO_EL1_EL0   (3 << 6)
#define AP_MASK     (3 << 6)

#define SH_NON_SH   (0 << 8)
#define SH_UNPRED   (1 << 8)
#define SH_OUT_SH   (2 << 8)
#define SH_IN_SH    (3 << 8)

#define ACCESS_FLAG (1 << 10)

#define ATTR_UXN (1UL << 54)
#define ATTR_PXN (1UL << 53)


#define PG_4k_ADDR_MASK	0xFFFFFFFFF000	// bit 47 - bit 12


/*
48 位有效地址情况
    映射时需要继续深入搞清几个问题
    每一个pgd 目录项寻址范围时512G
    每一个pud 目录项能寻址 1G
    每一个pmd 目录项能寻址 2M
    每一个pte 目录项能寻址 4K

    48bit 虚拟地址
39 位有效地址情况：
    每一个pgd 目录项寻址范围时1G ,总共512 条目
    每一个pmd 目录项能寻址 2M，总共512 条目
    每一个pte 目录项能寻址 4K  总共512 条目
*/
#define PG_IDX(level, va) (((va) >> (39 - (level) * 9)) & 0x1ff)


#define PGD_OFFSET	            30
#define PMD_OFFSET	            21	
#define PTE_OFFSET	            12	

#define PGD_ENTRY_SIZE	        (1 << PGD_OFFSET)
#define PMD_ENTRY_SIZE          (1 << PMD_OFFSET)
#define PTE_ENTRY_SIZE	        (1 << PTE_OFFSET)

#define PGD_MASK 	            (~(PGD_ENTRY_SIZE - 1))
#define PMD_MASK	            (~(PMD_ENTRY_SIZE - 1))	
#define PTE_MASK	            (~(PTE_ENTRY_SIZE - 1))	


#define PGD_ENTRY_NUMS          (1 << 9) //9 bit
#define PMD_ENTRY_NUMS	        (1 << 9) //9 bit
#define PTE_ENTRY_NUMS	        (1 << 9) //9 bit

#define PGD_IDX(v)	            (((uint64)(v) >> PGD_OFFSET) & (PGD_ENTRY_NUMS - 1))
#define PMD_IDX(v)	            (((uint64)(v) >> PMD_OFFSET) & (PMD_ENTRY_NUMS - 1))
#define PTE_IDX(v)	            (((uint64)(v) >> PTE_OFFSET) & (PTE_ENTRY_NUMS - 1))


#define PTE_ADDR(v)	            align_down (v, PTE_ENTRY_SIZE)


#if !defined VIRT_39

#define PGD_OFFSET	            30
#define PMD_OFFSET	            21	
#define PTE_OFFSET	            12	

#define PGD_ENTRY_SIZE	        (1 << PGD_OFFSET)
#define PMD_ENTRY_SIZE          (1 << PMD_OFFSET)
#define PTE_ENTRY_SIZE	        (1 << PTE_OFFSET)

#define PGD_MASK 	            (~(PGD_ENTRY_SIZE - 1))  //PGD 对齐
#define PMD_MASK	            (~(PMD_ENTRY_SIZE - 1))	 //PDM 对齐，
#define PTE_MASK	            (~(PTE_ENTRY_SIZE - 1))	 //page 对齐


#define PGD_ENTRY_NUMS          (1 << 9) //9 bit
#define PMD_ENTRY_NUMS	        (1 << 9) //9 bit
#define PTE_ENTRY_NUMS	        (1 << 9) //9 bit

#define PGD_IDX(v)	            (((uint64)(v) >> PGD_OFFSET) & (PGD_ENTRY_NUMS - 1))
#define PMD_IDX(v)	            (((uint64)(v) >> PMD_OFFSET) & (PMD_ENTRY_NUMS - 1))
#define PTE_IDX(v)	            (((uint64)(v) >> PTE_OFFSET) & (PTE_ENTRY_NUMS - 1))


#define PTE_ADDR(v)	            align_down (v, PTE_ENTRY_SIZE)


//tmp
#define PUD_OFFSET	            30					
#define PUD_ENTRY_SIZE          (1 << PUD_OFFSET)
#define PUD_MASK	            (~(PUD_ENTRY_SIZE - 1))		
#define PUD_ENTRY_NUMS	        (1 << 9)
#define PUD_IDX(v)	            (((uint64)(v) >> PUD_OFFSET) & (PUD_ENTRY_NUMS - 1))	

#endif

#if defined PG_48

#define PGD_OFFSET              39
#define PUD_OFFSET	            30					
#define PMD_OFFSET              21
#define PTE_OFFSET              12	


#define PGD_ENTRY_SIZE	        (1 << PGD_OFFSET)
#define PUD_ENTRY_SIZE          (1 << PUD_OFFSET)
#define PMD_ENTRY_SIZE          (1 << PMD_OFFSET)
#define PTE_ENTRY_SIZE          (1 << PTE_OFFSET)

#define PGD_MASK                (~(PGD_ENTRY_SIZE - 1))	
#define PUD_MASK                (~(PUD_ENTRY_SIZE - 1))		
#define PMD_MASK                (~(PMD_ENTRY_SIZE - 1))


#define PGD_ENTRY_NUMS          (1 << 9) //9 bit
#define PUD_ENTRY_NUMS	        (1 << 9)
#define PMD_ENTRY_NUMS	        (1 << 9)
#define PTE_ENTRY_NUMS          (1 << 9)


#define PGD_IDX(v)              (((uint64)(v) >> PGD_OFFSET) & (PGD_ENTRY_NUMS - 1))
#define PUD_IDX(v)              (((uint64)(v) >> PUD_OFFSET) & (PUD_ENTRY_NUMS - 1))	
#define PMD_IDX(v)              (((uint64)(v) >> PMD_OFFSET) & (PMD_ENTRY_NUMS - 1))
#define PTE_IDX(v)	            (((uint64)(v) >> PTE_OFFSET) & (PTE_ENTRY_NUMS - 1))

#define PTE_ADDR(v)             align_down (v, PTE_ENTRY_SIZE)

#endif




static inline void set_ttbr0_el1(uint64 u_pgd)
{
    uint64	val64;
    val64 = (uint64)u_pgd;
    asm volatile("MSR TTBR0_EL1, %[v]": :[v]"r" (val64):);
    asm volatile("ISB":::);

}
extern int boot_init_early_map();
extern void clear_bss (void);
extern void test_mmu_map_fun();
extern void config_enable_mmu();


#endif
