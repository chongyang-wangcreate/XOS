
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
#include "xos_page.h"
#include "xos_zone.h"
#include "printk.h"


xos_pg_cache_t page_cache_block;

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
    xos_unspinlock(&zone_normal.slock);
    return PHY_TO_VIRT((XOS_PAGE_TO_PHY(page)));
}

void xos_free_page(void *addr)
{
    /*
        通过addr 获取order
    */
    xos_page_t *page = addr;
    
    xos_free_pages(&zone_normal,page,page->order);
}
void *xos_get_phy(int mode,int order)
{
    char *phy_addr;
    xos_page_t *page;
    xos_spinlock(&zone_normal.slock);
    page = xos_get_page(&zone_normal,order);
    phy_addr = (void*)(XOS_PAGE_TO_PHY(page));
    xos_unspinlock(&zone_normal.slock);

    return phy_addr;
}


void *xos_get_user_phy(int mode,int order)
{
    char *phy_addr;
    xos_page_t *page;
    xos_spinlock(&zone_user.slock);
    page = xos_get_page(&zone_user,order);
    phy_addr = (void*)(XOS_USER_PAGE_TO_phy(page));
    xos_unspinlock(&zone_user.slock);

    return phy_addr;
}

int xos_page_cache_init()
{
    list_init(&page_cache_block.pg_list);
    xos_spinlock_init(&page_cache_block.pg_lock);
    return 0;
}

