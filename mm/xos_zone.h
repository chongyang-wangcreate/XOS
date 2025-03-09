#ifndef __XOS_ZONE_H__
#define __XOS_ZONE_H__

#define	MAX_ORDER	11

#include "mem_layout.h"
#include "spinlock.h"
#include "xos_page.h"

enum {
    ZONE_KERNEL,
    ZONE_USER,
    ZONE_DMA,
    ZONE_MAX
};


typedef struct free_area_struct {
	struct list_head 	free_list;
    uint32_t        order;
	uint32_t 		node_nfree;
}free_area_t;


typedef struct zone_struct {
    uint32_t 	free_pages;  //统计当前空闲页数量
    uint32_t    z_vm_cnt;
    uint32_t    z_pfn_cnt;
    uint32_t    start_pfn;
    uint32_t    end_pfn;
    xos_page_t* 	z_vmempage;
    free_area_t	free_area[MAX_ORDER];
    xos_spinlock_t slock;
}xos_zone_t;

#define MEM_PAGE_SIZE      (60*1024*1024)

#define PHY_MEM_PAGE_START (MEM_PHY_START + FIR_MAP_SIZE )
#define PHY_MEM_PAGE_END   (PHY_MEM_PAGE_START + MEM_PAGE_SIZE -1)

//#define PH_MEM_PAGE_USER_START 


#define NORMAL_ZONE_MEM_SIZE  500*1024*1024
#define USER_ZONE_MEM_SIZE    300*1024*1024
#define DMA_ZONE_MEM_SIZE     100*1024*1024


#define NORMAL_ZONE_PHY_START (PHY_MEM_PAGE_END +1)
#define NORMAL_ZONE_PHY_END   ((unsigned long)NORMAL_ZONE_PHY_START + (unsigned long)NORMAL_ZONE_MEM_SIZE -1)

#define USER_ZONE_PHY_START (NORMAL_ZONE_PHY_END  +1)
#define USER_ZONE_PHY_END   ((unsigned long)USER_ZONE_PHY_START + (unsigned long)USER_ZONE_MEM_SIZE -1)

#define DMA_ZONE_PHY_START  (USER_ZONE_PHY_END  +1)
#define DMA_ZONE_PHY_END   ((unsigned long)DMA_ZONE_PHY_START + (unsigned long)DMA_ZONE_MEM_SIZE -1)


#define KERNEL_MEM_PAGE_SIZE   5*1024*1024
#define KERNEL_MEM_PAGE_START  PHY_MEM_PAGE_START
#define KERNEL_MEM_PAGE_END    (KERNEL_MEM_PAGE_START + KERNEL_MEM_PAGE_SIZE -1)

#define USER_MEM_PAGE_SIZE    5*1024*1024
#define USER_MEM_PAGE_START  (KERNEL_MEM_PAGE_END +1)
#define USER_MEM_PAGE_END    (USER_MEM_PAGE_START + USER_MEM_PAGE_SIZE - 1)

#define DMA_MEM_PAGE_SIZE    2*1024*1024
#define DMA_MEM_PAGE_START   (USER_MEM_PAGE_END +1)
#define DMA_MEM_PAGE_END    (DMA_MEM_PAGE_START + DMA_MEM_PAGE_SIZE -1)


#define MEM_PAGE_SECTCION_SIZE  13*1024*1024
#define MEM_PAGE_SEC_CNT  (MEM_PAGE_SECTCION_SIZE/sizeof(xos_page_t))
#define MAX_PFN_NUM   (NORMAL_ZONE_MEM_SIZE >> PAGE_SHIFT)



#define kern_zone_phy_start   (DMA_MEM_PAGE_END+1)
#define kern_zone_phy_end    (kern_zone_phy_start +1*1024*1024 -1)
#define usr_zone_phy_start   (kern_zone_phy_end +1) 
#define usr_zone_phy_end     (usr_zone_phy_start+1*1024*1024 -1)
#define dma_zone_phy_start   (usr_zone_phy_end+1)
#define dma_zone_phy_end     ((unsigned long)dma_zone_phy_start +(unsigned long)1*1024*1024-1)


extern xos_zone_t zone_normal;
extern xos_zone_t zone_user;
extern xos_zone_t zone_dma;
#define XOS_PAGE_TO_PFN(page) (page - zone_normal.z_vmempage + zone_normal.start_pfn)
#define XOS_PFN_TO_PAGE(pfn)  (zone_normal.z_vmempage +pfn -zone_normal.start_pfn)

#define	XOS_PHY_TO_PFN(paddr)	((unsigned long)((paddr) >> PAGE_SHIFT))

#define XOS_PAGE_TO_PHY(page) (XOS_PAGE_TO_PFN(page)<<PAGE_SHIFT)
//#define XOS_PFN_TO_PAGE(pfn)  (zone_normal.z_vmempage + pfn)
#define PHY_TO_VIRT(phy) (void*)(VA_KERNEL_START + phy)

#define XOS_USER_PAGE_TO_PFN(page) (page - zone_user.z_vmempage + zone_user.start_pfn)
#define XOS_USER_PAGE_TO_phy(page) (XOS_USER_PAGE_TO_PFN(page)<<PAGE_SHIFT)
#define XOS_PFN_TO_USER_PAGE(pfn)  (zone_user.z_vmempage + pfn)

//extern void xos_zone_init(uint64 virt_start, uint64 virt_end,uint32_t phy_mm_start,uint32_t phy_mm_end);
extern xos_page_t *xos_get_page(xos_zone_t *zone,int order);
extern int  xos_free_pages(xos_zone_t *zone,xos_page_t *page,int order);
extern void zone_early_init();
extern void buddy_init(xos_zone_t * zone_area, int zone_id);
extern void zone_area_init();
extern void mem_init();
extern void xos_zone_init();

extern void test_buddy();

#endif
