/********************************************************
    
    development started:2024
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 ~ 2028 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/

#include "types.h"
#include "list.h"
#include "mem_layout.h"
#include "phy_mem.h"
#include "mmu.h"
#include "xos_page.h"
#include "setup_map.h"
#include "xos_zone.h"
#include "printk.h"

extern uint64 l1_kernel_pgt[];

static uint64 kmap_slot_pa;
static void *const kmap_slot_va = (void *)XOS_FIXMAP_ADDR(0);
/*
    2026.08.29 16:39
    There are minor issues with the kmap implementation,
    specifically the lack of shared resource protection. 
    This will be optimized in a future update
*/
static void xos_kmap_flush_tlb(void)
{
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("tlbi vmalle1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static void *xos_kmap_slot_map(uint64 pa)
{
    pte_t *pte;


    pte = copy_create_3level_page((pgd_t *)l1_kernel_pgt, kmap_slot_va);
    if (pte == NULL) {
        return NULL;
    }

    *pte = (pa & PG_4k_ADDR_MASK) | ACCESS_FLAG | SH_IN_SH | PG_RW_EL1 |
           NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE |
           PT_ENTRY_VALID;
    kmap_slot_pa = pa;
    xos_kmap_flush_tlb();
    return (void *)kmap_slot_va;
}

static void xos_kmap_slot_unmap(void)
{
    pte_t *pte;

    if (kmap_slot_pa == 0 ) {
        return;
    }

    pte = copy_create_3level_page((pgd_t *)l1_kernel_pgt, kmap_slot_va);
    if (pte == NULL) {
        return;
    }

    *pte = 0;
    kmap_slot_pa = 0;
    xos_kmap_flush_tlb();
}

xos_pg_cache_t page_cache_block;

static xos_zone_t *xos_get_zone_by_pa(uint64 pa, xos_page_t **page_out)
{
    xos_zone_t *zones[ZONE_MAX] = {
        &zone_normal,
        &zone_user,
        &zone_dma,
    };
    uint64 pfn = pa >> PAGE_SHIFT;
    int i;

    for (i = 0; i < ZONE_MAX; i++) {
        xos_zone_t *zone = zones[i];

        if (zone->z_vmempage == NULL || zone->z_pfn_cnt == 0) {
            continue;
        }
        if (pfn >= zone->start_pfn && pfn <= zone->end_pfn) {
            if (page_out != NULL) {
                *page_out = zone->z_vmempage + (pfn - zone->start_pfn);
            }
            return zone;
        }
    }

    return NULL;
}

void *xos_alloc_page(zone_desc_t *zone)
{
    void *page; 
    page = alloc_buddy(zone,0);
    return page;
}


void xos_alloc_pages(zone_desc_t *zone,uint32_t order)
{
//    page = alloc_buddy(zone,order);
}

void *xos_get_kern_page()
{
    return xos_alloc_page(&kern_zone);
}
/*
    1. mode 选同不同的zone 区域 GFP_KEREL  GFP_USER
    2. count 指定页个数
*/
void * xos_get_free_page(int mode,int order)
{
    xos_page_t *page;

    xos_spinlock(&zone_normal.slock);
    page = xos_get_page(&zone_normal,order);
    if(page == NULL){
        printk(PT_ERROR,"%s:%d,zone_normal out of pages,order=%d\n\r",__FUNCTION__,__LINE__,order);
        xos_unspinlock(&zone_normal.slock);
        return NULL;
    }
    xos_unspinlock(&zone_normal.slock);
    return PHY_TO_VIRT((XOS_PAGE_TO_PHY(page)));
}

void xos_free_page(void *addr)
{
    uint64 pa;
    xos_zone_t *zone;
    xos_page_t *page;

    if (addr == NULL) {
        return;
    }

    pa = V2P(addr);
    zone = xos_get_zone_by_pa(pa, &page);
    if (zone == NULL || page == NULL) {
        printk(PT_ERROR, "%s:%d invalid addr=%lx pa=%lx\n\r",
               __FUNCTION__, __LINE__, (uint64)addr, pa);
        return;
    }

    xos_spinlock(&zone->slock);
    xos_free_pages(zone, page, page->order);
    xos_unspinlock(&zone->slock);
}
void *xos_get_phy(int mode,int order)
{
    char *phy_addr;
    xos_page_t *page;
    xos_spinlock(&zone_normal.slock);
    page = xos_get_page(&zone_normal,order);
    xos_unspinlock(&zone_normal.slock);
    if(page == NULL){
        printk(PT_ERROR,"%s:%d,zone_normal out of pages,order=%d\n\r",__FUNCTION__,__LINE__,order);
        return NULL;
    }
    phy_addr = (void*)(XOS_PAGE_TO_PHY(page));
    
    return phy_addr;
}


void *xos_get_user_phy(int mode,int order)
{
    char *phy_addr;
    xos_page_t *page;
    xos_spinlock(&zone_user.slock);
    page = xos_get_page(&zone_user,order);
    xos_unspinlock(&zone_user.slock);
   
    if(page == NULL){
        printk(PT_ERROR,"%s:%d,zone_normal out of pages,order=%d\n\r",__FUNCTION__,__LINE__,order);
        return NULL;
    }
    phy_addr = (void*)(XOS_USER_PAGE_TO_phy(page));

    return phy_addr;
}

uint64 xos_alloc_user_page_pa(void)
{
    return (uint64)xos_get_user_phy(0, 0);
}

void *xos_kmap_page_pa(uint64 pa)
{
    xos_zone_t *zone;
    xos_page_t *page;

    zone = xos_get_zone_by_pa(pa, &page);
    if (zone == NULL) {
        return NULL;
    }

    if (zone == &zone_user) {
        if (kmap_slot_pa != pa) {
            if (kmap_slot_pa != 0) {
                xos_kmap_slot_unmap();
            }
            if (xos_kmap_slot_map(pa) == NULL) {
                return NULL;
            }
        }
        return kmap_slot_va;
    }

    return P2V(pa);
}

void xos_kunmap_page(void *kva)
{
    if (kva != kmap_slot_va) {
        return;
    }
    xos_kmap_slot_unmap();
}

int xos_page_get(uint64 pa)
{
    xos_zone_t *zone;
    xos_page_t *page;

    zone = xos_get_zone_by_pa(pa, &page);
    if (zone == NULL) {
        return 0;
    }

    xos_spinlock(&zone->slock);
    if (page->ref_cnt <= 0) {
        xos_unspinlock(&zone->slock);
        return -1;
    }
    page->ref_cnt++;
    xos_unspinlock(&zone->slock);
    return 0;
}

int xos_page_put(uint64 pa)
{
    xos_zone_t *zone;
    xos_page_t *page;

    zone = xos_get_zone_by_pa(pa, &page);
    if (zone == NULL) {
        return 0;
    }

    xos_spinlock(&zone->slock);
    if (page->ref_cnt <= 0) {
        xos_unspinlock(&zone->slock);
        return -1;
    }

    {
        int order = page->order;

        if (order < 0) {
            xos_unspinlock(&zone->slock);
            return -1;
        }
        page->ref_cnt--;
        if (page->ref_cnt == 0) {
            xos_free_pages(zone, page, order);
        }
    }
    xos_unspinlock(&zone->slock);
    return 0;
}

int xos_page_cache_init()
{
    list_init(&page_cache_block.pg_list);
    xos_spinlock_init(&page_cache_block.pg_lock);
    kmap_slot_pa = 0;
    return 0;
}


