#ifndef __MEM_LAYOUT_H__
#define __MEM_LAYOUT_H__


/********************************************************************
0x40000000 +-----------------+
           | 内核镜像         | ← 已分配，不可被zone管理
           | (代码/数据/BSS)  |
0x4xxxxxxx +-----------------+
           | Boot内存分配器   | ← 已分配
           | 页表/栈         |
0x4xxxxxxx +-----------------+
           | Zone页描述符数组 | ← 为zone元数据预留
           | (kernel zone)   |
           | (user zone)     |
           | (DMA zone)      |
0x4xxxxxxx +-----------------+
           | Kernel Zone     | ← 空闲内存，被zone_normal管理
           | (64MB)          |
0x4xxxxxxx +-----------------+
           | User Zone       | ← 空闲内存，被zone_user管理
           | (128MB)         |
0x4xxxxxxx +-----------------+
           | DMA Zone        | ← 空闲内存，被zone_dma管理
           | (32MB)          |
0x80000000 +-----------------+

TTBR1_EL1 → L1内核页表 (l1_kernel_pgt) -> L2内核页表0 (l2_kernel_pgt0)
                                           ↓
                                        L2内核页表1 (l2kpgt1)
                                        
TTBR0_EL1 → L1用户页表 (l1upgt) -> L2用户页表 (l2upgt)

********************************************************/
/* 内核虚拟地址起始 */
#define VA_KERNEL_START      0xFFFFFF8000000000

/* 物理内存布局（基于QEMU virt 1GB内存） */
#define PHYS_MEM_START       0x40000000UL  /* 1GB内存起始 */
#define PHYS_MEM_SIZE        (1UL * 1024 * 1024 * 1024)  /* 1GB */
#define PHYS_MEM_END         (PHYS_MEM_START + PHYS_MEM_SIZE - 1)

/* ========== 已分配区域（不能被zone管理）========== */

/* 1. 内核镜像区域 */
#define KERNEL_LOAD_START    0x40010000
#define KERNEL_LOAD_END      (KERNEL_LOAD_START + 0x200000)  /* 假设内核最大2MB */


//extern uint8_t kernel_end[];   // 原第48行的错误来源

// 如果还有其他 extern 或函数声明，也放这里
// extern uint8_t _boot_mem_begin[];
// extern uint8_t _boot_mem_end[];


/* 计算已使用的物理内存 */
extern uint8_t kernel_end[]; 
extern uint8_t _user_image_end_lma_virt[];
#define RESERVED_MEM_START   PHYS_MEM_START
#define RESERVED_MEM_END     ((uintptr_t)_user_image_end_lma_virt - VA_KERNEL_START - 1)


/* ========== Zone 空闲内存区域 ========== */
/* Zone从已使用内存之后开始 */

#define ZONE_KERNEL_START    ((RESERVED_MEM_END + 0xFFFFF) & ~0xFFFFF)  /* 对齐到1MB */
#define ZONE_KERNEL_SIZE     (128 * 1024 * 1024)  /* 64MB */
#define ZONE_KERNEL_END      (ZONE_KERNEL_START + ZONE_KERNEL_SIZE - 1)

#define ZONE_USER_START      (ZONE_KERNEL_END + 1)
#define ZONE_USER_SIZE       (128 * 1024 * 1024) /* 128MB */
#define ZONE_USER_END        (ZONE_USER_START + ZONE_USER_SIZE - 1)

#define ZONE_DMA_START       (ZONE_USER_END + 1)
#define ZONE_DMA_SIZE        (32 * 1024 * 1024)  /* 32MB */
#define ZONE_DMA_END         (ZONE_DMA_START + ZONE_DMA_SIZE - 1)



/* 计算每个zone的页数 */
#define ZONE_KERNEL_PAGES    (ZONE_KERNEL_SIZE >> PAGE_SHIFT)
#define ZONE_USER_PAGES      (ZONE_USER_SIZE >> PAGE_SHIFT)
#define ZONE_DMA_PAGES       (ZONE_DMA_SIZE >> PAGE_SHIFT)

/* 计算页描述符数组所需大小 */
#define KERNEL_PAGE_ARRAY_SIZE   (ZONE_KERNEL_PAGES * sizeof(xos_page_t))
#define USER_PAGE_ARRAY_SIZE     (ZONE_USER_PAGES * sizeof(xos_page_t))
#define DMA_PAGE_ARRAY_SIZE      (ZONE_DMA_PAGES * sizeof(xos_page_t))

/* 页描述符数组位置（放在已分配内存之后） */
#define PAGE_ARRAY_START     (RESERVED_MEM_END)
#define PAGE_ARRAY_SIZE      (KERNEL_PAGE_ARRAY_SIZE + USER_PAGE_ARRAY_SIZE + DMA_PAGE_ARRAY_SIZE)

/* 内存使用情况统计 */
#define TOTAL_PHYS_MEM       PHYS_MEM_SIZE
#define RESERVED_MEM         (RESERVED_MEM_END - RESERVED_MEM_START)
#define AVAILABLE_MEM        (ZONE_DMA_END - ZONE_KERNEL_START + 1)
#define UNUSED_MEM           (PHYS_MEM_END - ZONE_DMA_END)





/*
    空余4K
*/

/*
//#define MEM_PAGE_SIZE      (6*1024*1024 - 4096)
#define MEM_PAGE_SIZE      (6*1024*1024)

#define PHY_MEM_PAGE_START (MEM_PHY_START + FIR_MAP_SIZE )
#define PHY_MEM_PAGE_END   (PHY_MEM_PAGE_START + MEM_PAGE_SIZE)-1


//#define NORMAL_ZONE_MEM_SIZE  248*1024*1024
#define NORMAL_ZONE_MEM_SIZE  248*1024*1024
#define NORMAL_ZONE_PHY_START (PHY_MEM_PAGE_END + 1)
#define NORMAL_ZONE_PHY_END   (NORMAL_ZONE_PHY_START + NORMAL_ZONE_MEM_SIZE)
*/


#define MEM_PHY_START      0x40000000UL //0x40000000UL
#define PHY_STOP        (MEM_PHY_START + RAM_SIZE)
#define RAM_SIZE         (1UL*1024*1024*1024)

#define PHY_TMP_MAP_END         ( MEM_PHY_START + RAM_SIZE)

#define KERNEL_BOOT_IDMAP_SIZE  0x60000UL
#define KERNEL_LINEAR_MAP_START MEM_PHY_START
#define KERNEL_LINEAR_MAP_SIZE  RAM_SIZE
#define KERNEL_LINEAR_MAP_END   (KERNEL_LINEAR_MAP_START + KERNEL_LINEAR_MAP_SIZE)

#define KERNEL_LINEAR_REST_START (KERNEL_LINEAR_MAP_START + KERNEL_BOOT_IDMAP_SIZE)
#define KERNEL_LINEAR_REST_SIZE  (KERNEL_LINEAR_MAP_END - KERNEL_LINEAR_REST_START)
#define FIR_MAP_SIZE	0x600000
#define PHY_KERNMAP_START 	(MEM_PHY_START + FIR_MAP_SIZE )
#define PHY_KERNMAP_END      (MEM_PHY_START + RAM_SIZE )


#define V2P(a) (((uint64) (a)) - (uint64)VA_KERNEL_START)
#define P2V(a) ((void *) (((uint64)a) + (uint64)VA_KERNEL_START))


#define LINEAR_P2V_UL(x) ((unsigned long)(x) + (uint64)VA_KERNEL_START)
#define LINEAR_P2V(x)   ((void*)LINEAR_P2V_UL(x))

#define LINEAR_V2P_UL(x) (((unsigned long)(x)) - (unsigned long)VA_KERNEL_START)
#define LINEAR_V2P(x)    ((void*)LINEAR_V2P_UL(x))

#define XOS_FIXMAP_SLOTS   32
#define XOS_FIXMAP_TOP     0xFFFFFFFFFFE00000UL
#define XOS_FIXMAP_ADDR(idx)  (XOS_FIXMAP_TOP - (((uint64)(idx) + 1)*0x1000UL))
#endif
