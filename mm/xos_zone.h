#ifndef __XOS_ZONE_H__
#define __XOS_ZONE_H__



#include "types.h"
#include "list.h"
#include "mem_layout.h"
#include "spinlock.h"
#include "xos_page.h"

#define	MAX_ORDER	11

enum {
    ZONE_DMA,
    ZONE_KERNEL,
    ZONE_USER,
    ZONE_MAX
};


typedef struct free_area_struct {
	dlist_t	   free_list;
    uint32_t   order;
	uint32_t   node_nfree;
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

extern xos_zone_t zone_normal;
extern xos_zone_t zone_user;
extern xos_zone_t zone_dma;

#define XOS_ZONE_COUNT  ZONE_MAX

typedef struct xos_zone_layout{
    int zone_id;
    const char *name;
    uint64 start;
    uint64 end;
    uint64 size;
    xos_page_t *page_array;
    xos_page_t *page_array_end;

}xos_zone_layout_t;
//#define MEM_PAGE_SIZE      (60*1024*1024)

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
extern void mem_init();
extern void xos_zone_init();
extern const xos_zone_layout_t *xos_get_zone_layouts(int *count);
extern void test_buddy();
extern uint64 xos_zone_linear_map_start(void);
extern uint64 xos_zone_linear_map_end(void);
extern int xos_zone_set();

#endif


