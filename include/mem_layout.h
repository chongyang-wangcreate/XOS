#ifndef __MEM_LAYOUT_H__
#define __MEM_LAYOUT_H__

#define VA_KERNEL_START     0xFFFFFF8000000000	

#define TMP_MAP_SIZE    (512 * 1024 * 1024UL)

#define RAM_SIZE        (1024 * 1024 * 1024UL)

#define MEM_PHY_START       0x40000000
#define PHY_STOP        (MEM_PHY_START + RAM_SIZE)

#define PHY_TMP_MAP_END         ( MEM_PHY_START + RAM_SIZE)


#define FIR_MAP_SIZE	0x200000
#define PHY_KERNMAP_START 	(MEM_PHY_START + FIR_MAP_SIZE )
#define PHY_KERNMAP_END      (MEM_PHY_START + RAM_SIZE )




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


#define V2P(a) (((uint64) (a)) - (uint64)VA_KERNEL_START)
#define P2V(a) ((void *) (((uint64)a) + (uint64)VA_KERNEL_START))


#define LINEAR_P2V_UL(x) ((unsigned long)(x) + (uint64)VA_KERNEL_START)
#define LINEAR_P2V(x)   ((void*)LINEAR_P2V_UL(x))

#define LINEAR_V2P_UL(x) (((unsigned long)(x)) - (unsigned long)VA_KERNEL_START)
#define LINEAR_V2P(x)    ((void*)LINEAR_V2P_UL(x))


#endif
