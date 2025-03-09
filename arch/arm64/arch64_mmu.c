/********************************************************
      
    development start:20240127
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
  
    Copyright (c) 2024 - 2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
     
    体系相关MMU 寄存器配置
  
  ********************************************************/

#include "types.h"
#include "list.h"
#include "../../../device/dev_addr_def.h"
#include "mmu.h"
#include "mem_layout.h"
#include "setup_map.h"
#include "boot_mem.h"

extern void * exce_vectors;

extern void boot_puts (char *s);
extern void _putint (char *prefix, uint val, char* suffix);
extern void config_mmu_regs (uint64* kern_pgtbl, uint64* user_pgtbl);
void build_pgd_item(uint64 *kernel_pgd,uint64 *kernel_pmd);
void config_enable_mmu(uint64 *kernel_pgd,uint64 *user_pgd);



uint64 *kernel_pgtbl_tmp = NULL;
uint64 *user_pgtbl_tmp = NULL;
uint64 *kernel_l2_pmd = NULL;
uint64 *user_l2_pmd = NULL;
#define MAIR(attr, mt)  ((attr) << ((mt) * 8))
  
  
                 // MAIR(0xffUL,MT_NORMAL)
  
#define MM_ATTR_VALUE	(MAIR(0x00, MT_DEVICE_nGnRnE) |	\
                  MAIR(0x04,MT_DEVICE_nGnRE)   |  \
                  MAIR(0x0c,MT_DEVICE_GRE)     |  \
                  MAIR(0x44,MT_NORMAL_NC)      |  \
                  MAIR(0xffUL,MT_NORMAL))
  
  



static inline void set_flush_invalid_icache()
{
    asm volatile("ic ialluis": : :);
}

static inline void set_flush_invalid_tlb()
{
    asm volatile("tlbi vmalle1" : : :);
}
  
static inline void set_mair_reg()
{
    uint64  val64;
    boot_puts("Setting Memory Attribute Indirection Register (MAIR_EL1)\n");
    val64 = MM_ATTR_VALUE;
    asm volatile("MSR MAIR_EL1, %[v]": :[v]"r" (val64):);
    asm volatile("ISB": : :"memory");
}
  
static inline void set_tcr_el1_reg()
{
    uint64  val64;
    val64 = (TCR_T0SZ(39)|TCR_T1SZ(39)|TCR_TTBR0_SH(3)|TCR_IRGN0(2)|TCR_IPS(2));
    asm volatile("MSR TCR_EL1, %[v]": :[v]"r" (val64):);
    asm volatile("ISB": : :);
}

  
static inline void set_ttbr1_el1(uint64 k_pgd)
{
    uint64  val64;
    boot_puts("Setting Translation Table Base Register 1 (TTBR1_EL1)\n");
    val64 = k_pgd;
    asm volatile("MSR TTBR1_EL1, %[v]": :[v]"r" (val64):);
    asm volatile("ISB": : :);
}
  
static inline void set_sctlr_el1()
{
    uint32  val32;
    boot_puts("Setting System Control Register (SCTLR_EL1)\n");
    asm volatile("MRS %[r], SCTLR_EL1":[r]"=r" (val32): :); //0x0000000000c50838
    val32 = val32 | 0x01;
    asm volatile("MSR SCTLR_EL1, %[v]": :[v]"r" (val32):);
    asm volatile("ISB": : :);
}

  /*
      20240405:增加注释方便他人理解
      config_mmu_regs 调试成功耗费了大量时间，差点让我放弃，还好不断鼓励自己，不断查找资料，参考linux，求助chatgpt csdn  ,baidu.com
      不断尝试，虚拟物理地址映射终于成功了，热泪盈眶。所以要对MMU 寄存器资料做一些描述
      ARM64启动汇编和内存初始化(中) 这篇文章写的非常好
  汇编
      MRS x0, TTBR0_EL1 // 把 TTBR0_EL1 的值保存到x0
      MSR TTBR0_EL1, x0 // 把x0 的值写入TTBR0_EL1寄存器
  
      翻译控制寄存器（TCR_ELx）
      翻译控制寄存器是第1阶段翻译状态的控制寄存器，包括TCR_EL1、 TCR_EL2和 TCR_EL3。
      AArch64系统寄存器TCR_EL1位[31:0]在架构上映射到AArch32系统寄存器TTBCR[31:0]。AArch64系统寄存器TCR_EL1位[63:32]在架构上映射到AArch32系统寄存器TTBCR2[31:0]。AArch64系统寄存器TCR_EL2位[31:0]在架构上映射到AArch32系统寄存器HTCR[31:0]。
      34:32   IPS 中间物理地址大小。000：32位,4GB；001：36位,64GB；010：40位,1TB;011：42位,4TB；100：44位，16TB；101：48位，256TB；110：52位，4PB
      31:30   TG1 TTBR1_EL1的颗粒大小。01：16KB；10：4KB；11：64Kb
      21:16   T1SZ    TTBR1_EL1寻址的内存区域大小偏移量。区域大小为2的(64-T1SZ)次方字节。T1SZ的最大值和最小值取决于翻译表的级别和内存翻译粒度大小。
      15:14   TG0 TTBR0_EL1的颗粒大小。01：16KB；10：4KB；11：64Kb
    IRGN0,bits[9:8]
    Inner cacheability attribute for memory associated with translation tablewalks using TTBRO_EL1.
    0b00
    0b01
    Normal memory, Inner Non-cacheable.
    Normal memory, Inner Write-Back Read-Allocate Write-Allocate Caclheable
    0b10
    Normal memory, Inner Write-Through Read-Allocate No Write-Allocate Cacheable
    Normal memory, Inner Write-Back Read-Allocate No Write-Alloecate Cacheable
    0b11
    This field resets to an architecturally UNKNOWN value.
      5:0 T0SZ    TTBR0_EL1寻址的内存区域大小偏移量。区域大小为2的(64-T0SZ)次方字节。T0SZ的最大值和最小值取决于翻译表的级别和内存翻译粒度大小。
      原文链接：https://blog.csdn.net/heshuangzong/article/details/127695422
  
      mair寄存器在哪里配置的，在启动汇编中，有2个情况，次核和主核
   
      arch/arm64/mm/proc.S:195:ENTRY(__cpu_setup)
   
      
          __cpu_setup
      
          Initialise the processor for turning the MMU on.  Return in x0 the
          value of the SCTLR_EL1 register.
      
  ENTRY(__cpu_setup)
      ic  iallu               // I+BTB cache invalidate
      tlbi    vmalle1is           // invalidate I + D TLBs
      dsb ish
   
      mov x0, #3 << 20
      msr cpacr_el1, x0           // Enable FP/ASIMD
      mov x0, #1 << 12            // Reset mdscr_el1 and disable
      msr mdscr_el1, x0           // access to the DCC from EL0
      reset_pmuserenr_el0 x0          // Disable PMU access from EL0
  
      
    #define MAIR(attr, mt)  ((attr) << ((mt) * 8))
        Memory region attributes for LPAE:
       
          n = AttrIndx[2:0]
                  n   MAIR
          DEVICE_nGnRnE   000 00000000
          DEVICE_nGnRE    001 00000100
          DEVICE_GRE      010 00001100
          NORMAL_NC       011 01000100
          NORMAL      100 11111111
  
	    #define MT_DEVICE_nGnRnE	0
        #define MT_DEVICE_nGnRE		1
        #define MT_DEVICE_GRE		2
        #define MT_NORMAL_NC		3
        #define MT_NORMAL		4
        #define MT_NORMAL_WT		5
      
      ldr x5, =MAIR(0x00, MT_DEVICE_nGnRnE) | \
               MAIR(0x04, MT_DEVICE_nGnRE) | \
               MAIR(0x0c, MT_DEVICE_GRE) | \
               MAIR(0x44, MT_NORMAL_NC) | \
               MAIR(0xff, MT_NORMAL)             //#define MAIR(attr, mt) ((attr) << ((mt) * 8))
               计算结果：0xFF440C0400
      msr mair_el1, x5
      
        Prepare SCTLR
       
      adr x5, crval
      ldp w5, w6, [x5]
      mrs x0, sctlr_el1
      bic x0, x0, x5          // clear bits
      orr x0, x0, x6          // set bits
      
        Set/prepare TCR and TTBR. We use 512GB (39-bit) address range for
        both user and kernel.
       
      ldr x10, =TCR_TxSZ(VA_BITS) | TCR_CACHE_FLAGS | TCR_SMP_FLAGS | \
              TCR_TG_FLAGS | TCR_ASID16 | TCR_TBI0
      tcr_set_idmap_t0sz  x10, x9
   
      
        Read the PARange bits from ID_AA64MMFR0_EL1 and set the IPS bits in
        TCR_EL1.
       
      mrs x9, ID_AA64MMFR0_EL1
      bfi x10, x9, #32, #3
      msr tcr_el1, x10
      ret                 // return to head.S
  ENDPROC(__cpu_setup)
  
      内嵌汇编不要求精通但要熟练
      asm volatile(
      Assembler Template;
      :outputOperands
      :inputOperands
      [:Clobbers])
  
      asm [volatile] ( AssemblerTemplate
                 : OutputOperands  // 可选的输出操作数
                 : InputOperands   // 可选的输入操作数
                 : Clobbers         // 可选的破坏描述
                 );
          AssemblerTemplate 是包含实际汇编指令的字符串。
          OutputOperands 指定输出操作数（如果有的话），以 = 开头。
          InputOperands 指定输入操作数（如果有的话）。
          Clobbers 列出了被内联汇编代码所改变的寄存器。
      )
  
      asm volatile(
      "MSR VBAR_EL1, %[v]"  // 将 val64 的值写入 VBAR_EL1 寄存器
      :                       // 无输出
      : [v] "r" (val64)       // 输入：val64 被作为寄存器传递
      : );  

      DSB SY 是一个全系统范围（Full System）的内存屏障。

      它确保在 DSB SY 之前的所有内存访问操作（如加载和存储）都完成后，才会执行 DSB SY 之后的指令。

      作用范围包括整个系统（所有处理器核心和设备。

      dsb ish 是一个内部共享域（Inner Shareable Domain）的内存屏障。

      它确保在 dsb ish 之前的所有内存访问操作都完成后，才会执行 dsb ish 之后的指令。

      作用范围仅限于当前处理器核心和其他共享同一内部共享域的处理器核心。

  */
  
void config_mmu_regs (uint64* kern_pgtbl, uint64* user_pgtbl)
{
    uint32  val32 = 0;


    set_flush_invalid_icache();

    set_flush_invalid_tlb();

    asm volatile("dsb sy" : : :);

  /* no trapping on FP/SIMD instructions
      linux operation 
      mov x0, #3 << 20
      msr cpacr_el1, x0           // Enable FP/ASIMD
      mov x0, #1 << 12            // Reset mdscr_el1 and disable
      msr mdscr_el1, x0           // access to the DCC from EL0
  */
    val32 = 0x03 << 20;
    asm volatile("msr cpacr_el1, %[v]": :[v]"r" (val32):);

    val32 = 0x01 << 12;
    asm volatile("msr mdscr_el1, %[v]": :[v]"r" (val32):);

    set_mair_reg();

    boot_puts("set TCR_EL1...\n");
    set_tcr_el1_reg();

    set_ttbr1_el1((uint64)kern_pgtbl);
    boot_puts("set (TTBR0_EL1)...\n");
    set_ttbr0_el1((uint64)user_pgtbl);

    boot_puts("set sctlr_el1...\n");
    set_sctlr_el1();
    boot_puts("mmu completed...\n");

}
  
void kernel_early_mem_map(uint64 *kern_pgd)
{
    /*
        map text section
        map rodata data section
        map bss section
        map devmem section
    */
    xos_2level_maps(kern_pgd,(uint64)VA_KERNEL_START + (uint64)MEM_PHY_START, (uint64)MEM_PHY_START, FIR_MAP_SIZE, ATTR_UXN|PT_ATTRINDX(MT_NORMAL));
    xos_2level_maps(kern_pgd,(uint64)MEM_PHY_START, (uint64)MEM_PHY_START, FIR_MAP_SIZE, ATTR_UXN|PT_ATTRINDX(MT_NORMAL));
    xos_2level_maps(kern_pgd,(uint64)VA_KERNEL_START + (uint64)PHY_KERNMAP_START, (uint64)PHY_KERNMAP_START, PHY_TMP_MAP_END-PHY_KERNMAP_START, ATTR_UXN|PT_ATTRINDX(MT_NORMAL));
     //为了串口能够继续使用原地址，对设备暂时进行了一一映射
    xos_2level_maps(kern_pgd,(uint64)VA_KERNEL_START+(uint64)DEVMEM_RANGE1_BASE, (uint64)DEVMEM_RANGE1_BASE, 0x2000000, PT_ATTRINDX(MT_DEVICE_nGnRnE));
    xos_2level_maps(kern_pgd,(uint64)DEVMEM_RANGE1_BASE, (uint64)DEVMEM_RANGE1_BASE, DEVMEM_MAP_SIZE, PT_ATTRINDX(MT_DEVICE_nGnRnE)); 
}


  /*
      set 2 level page
  
      PGD   (Page Global Directory)                    bit[39:30]      level1
  
      PMD   (Page Middle Directory)                    bit[30:21]     level2
  
      页表配置并不复杂，而且可以说模型非常简单，但是难点在于需要把MMU 寄存器配置搞清除
      寄存器种类非常多，并且每个寄存器不同字段都有不同含义
  
      MMU 配置需要多查资料，不断尝试
  
  
      2024.3.12: 注意：
      初始页目录地址可以不在链接脚本中指定，
      可以在程序里面指定物理地址存放页表，这个地址，需要根据实际情况进行规划
      在链接脚本中指定就是为了方便，这个页表就是临时页表，为了开启mmu之后
      不至于崩溃
      也可以说我就是为了偷懒才没实现boot_mem 管理

      boot_mem 需要在lds 指定起始管理地址boot_mem_start  和 结束地址 boot_mem_end
      或者在boot 节点实现boot_mem 管理(简单)，第一级页表，第二级页表都使用
      boot_mem_alloc 来实现
  
      2024.11.23:为了看到本操作的系统的人进一步了解页表，再次做详细说明
      
      MMU在转换虚拟地址的时候遵循以下步骤(基于以上配置)：
  
      1,如果虚拟地址bit[63:40]都为1，则使用TTBR1作为第一级页目录表基地址，当bit[63:40]都为0时，使用TTBR0作为第一级页目录表基地址；
  
      2,PGD包含512个64位PMD表，从虚拟地址中获取VA[39:31]进行索引，找到对应条目为PGD+index[39:31])；
  
      4,MMU检查PGD目录项的有效性(bit[1:0])，以及其属性标志判断是否允许请求的内存访问。假设它有效，且允许访问内存；
  
      5,MMU从PGD目录表项中获取bit[39:12]，作为PMD页表的物理基址(table descriptor)。
  
      6,PMD包含512个64位PTE表，从虚拟地址中获取VA[30:21]进行索引，PMD+(index[30:21]8)，MMU从PMD表项中读取PTE表的基地址；
  
      7,MMU检查PMD目录项的有效性(bit[1:0])，以及其属性标志判断是否允许请求的内存访问。假设它有效，且允许访问内存；
  
      8,pmd目录表项中获取bit[39:12]，作为pte页表的物理基基址,
  
      9,pte指向一个4k的页(page descriptor)，mmu获取pte的bit[39:12]作为最终物理地址的pa[39:12]；
  
      10,取出va[11:0]作为pa[11:0]，然后返回完整的PA[39:0]，以及来自页表项的附加信息。
  
  */

int boot_init_early_map()
{

    boot_puts("setup_page_table..\n\r");

    kernel_pgtbl_tmp = boot_alloc_l1_pgd();

    kernel_l2_pmd = boot_alloc_l2_pmd();

    build_pgd_item(kernel_pgtbl_tmp,kernel_l2_pmd);
    kernel_early_mem_map(kernel_pgtbl_tmp);
    config_enable_mmu(kernel_pgtbl_tmp,kernel_pgtbl_tmp);
    
    return 0;
}

void build_pgd_item(uint64 *kernel_pgd,uint64 *kernel_pmd)
{
    uint64  l2_pmdtbl;
    uint32  pgd_idx;
    l2_pmdtbl = (uint64)kernel_pmd;   
    for(pgd_idx = 0; pgd_idx < 4; pgd_idx++) 
    {
        l2_pmdtbl = l2_pmdtbl | PT_ENTRY_TABLE | PT_ENTRY_VALID;
        kernel_pgd[pgd_idx] = l2_pmdtbl;
        l2_pmdtbl += 4096; 
    }
}

void config_enable_mmu(uint64 *kernel_pgd,uint64 *user_pgd)
{
  /*
      后续增加asid 的配置
  */
    config_mmu_regs(kernel_pgd, user_pgd);
}

void test_mmu_map_fun()
{
    boot_puts("test_mmu_map_fun\n");
}
