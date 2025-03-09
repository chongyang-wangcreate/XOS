/********************************************************
    
    development timer:2024
    All rights reserved
    author :wangchongyang

   Copyright (c) 2024 - 2027 wangchongyang

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

********************************************************/

#include "types.h"
#include "mmu.h"
#include "list.h"
#include "tick_timer.h"
#include "bit_map.h"
#include "phy_mem.h"
#include "printk.h"
#include "common.h"
#include "mem_layout.h"
#include "setup_map.h"
#include "xos_page.h"

/*
    20240427：
    物理内存规划管理：
    物理内存管理部分自己设计的两种内存管理
    1. 第一阶段由于急切的想看到内核态任务启动，所以任务管理设计的非常简单，就是将内存块简单的分割成4K 块，也不存在内存释放，
    没有内存块分割 合并，
    2.第二阶段内核态任务运行之后，完善的内存管理成了，操作系统开发的最大障碍，所以我自己设计的buddy 管理，自己设计的这个
    buddy 管理确实可用也经过验证，但是在内存管理也存在很大弊端， 内存分割一来比较繁琐，而来非常麻烦，涉及到邻居链表
    拆分合并非常麻烦，步骤非常多

   3.第三阶段：第二阶段设计的内存管理已经不适合当前xos ，也存在一些问题，所以经过一段时间研究linux ,以及一些技术文章，学习其他牛人的
   设计思想，完善实现自己新的内存管理系统
    
*/
#define SIZE_4K 0x1000
zone_desc_t user_zone;  //第二阶段定义
zone_desc_t kern_zone;

//zone_t user_zone;   //第三阶段定义
//zone_t kern_zone;


buddy_pool_t buddy_pool;
buddy_pool_t user_buddy_pool;
buddy_pool_t kern_buddy_pool;
buddy_sec_t  buddy_area; //从哪儿申请,需要一块从bss 段，2k 空间应该就可以 

/*
    直接放到这里头混乱了，以前的结构没有删除吗，又添加新的结构，所以还是
*/
int kern_max_pages;  //pages 数量当前系统page 按照4k 来计算
int user_max_pages;


#define BUDDY_POOL_SIZE sizeof(buddy_pool_t)



/*
    显然我buddy 实现是存在问题的，buddy_pool 没有实现
    管理用户堆和管理内核堆空间分开

    那么用户申请堆空间，内核申请堆空间那就区分不开了
    所以应该分成user_buddy_pool     和 kern_buddy_pool
*/
buddy_sec_t *alloc_buddy_space()
{
    int free_block_idx;
    buddy_sec_t *buddy_ptr;
    buddy_pool.pool_bitmap.bit_start[0] = 0xff;
//     _putint (NULL, (long)buddy_pool.pool_bitmap.btmp_bytes_len, NULL);
    free_block_idx = find_free_bit(&buddy_pool.pool_bitmap);
    if(free_block_idx < 0){
        printk(PT_RUN,"find free bit failed\n\r");
        return NULL;
    }
    set_bit(buddy_pool.pool_bitmap.bit_start,free_block_idx);
    buddy_ptr =  (buddy_sec_t*)(buddy_pool.virt_start + free_block_idx * BUDDY_POOL_SIZE);
    return buddy_ptr;
}
int insert_buddy(zone_desc_t * zone)
{
    buddy_sec_t  *buddy_area;
    int order = 10;
//    zone->free_pages  //4k 页数量
//    zone->phy_start 
    
    //从头开始切页，先切大块，然后切小块 1024*4k
    //1024pages，512pages  ，128pages,64pages,32pages,16pages
    for(;order > 0;order--){
        
        while(zone->free_pages >= (1 << order)){
//             _puts("insert_buddy\n\r");
            
            buddy_area = alloc_buddy_space();
            list_init(&buddy_area->list);
            list_init(&buddy_area->using_list);
            printk(PT_RUN,"buddy_area addr =%lx,order=%d\n\r",(long)buddy_area,order);
            buddy_area->phy_start = zone->phy_start;
            printk(PT_RUN,"buddy_area->phy_start =%lx,order=%d\n\r",buddy_area->phy_start,order);
            buddy_area->phy_end = zone->phy_start+(1 << order)*SIZE_4K;
            buddy_area->order = order;      
            list_add_back(&buddy_area->list,&zone->section[order].free_list);
            zone->free_pages -= (1 << order);
            zone->phy_start = buddy_area->phy_end;
        }
    }
//    _puts("insert_buddy end\n\r");
    return 0;
}

void* alloc_buddy(zone_desc_t *zone,uint32_t order)
{
    int cur_order;
    int max_order = 11;
    buddy_sec_t  *buddy_area;
    dlist_t *available_node;
//	spin_lock(&zone->lock);
	for(cur_order = order; cur_order < max_order;cur_order++){
        if(list_is_empty(&zone->section[cur_order].free_list)){
//            printk("%s:%d,cur_order=%d\n\r",__FUNCTION__,__LINE__,cur_order);
            continue;
        }    

        available_node = zone->section[cur_order].free_list.next;
        
        buddy_area = list_entry(available_node, buddy_sec_t, list);
        list_del(&buddy_area->list);
        zone->section[cur_order].free_cnt--;
        zone->free_pages -= (1UL << order);
//      xos_split_order(zone,order,cur_order);
        printk(PT_DEBUG,"%s:%d buddy_area->phy_start=%lx\n\r",__FUNCTION__,__LINE__,P2V(buddy_area->phy_start));
        return (void*)P2V(buddy_area->phy_start);
        //list_add_back(&buddy_area->list, &zone->section[cur_order].using_list);
 
    }

    /*
        后续还应该将大段进行split 拆分
    */
    return NULL;
}
void init_user_pmm(uint64 phy_start,uint64 len)
{
    //用户可见到内存
//    struct phy_zone  user_zone;

//将内容分层不同的Block  ,分别挂入不同的order 链表

    int i;
    printk(PT_DEBUG,"%s:%d,phy_start=%lx\n\r",__FUNCTION__,__LINE__,phy_start);
    user_zone.phy_start = phy_start;
    user_zone.phy_end = phy_start + len;
    user_zone.free_pages = len/SIZE_4K;
    
	for (i = 0 ;i< MAX_ORDER; i++){
        list_init(&user_zone.section[i].free_list);
        list_init(&user_zone.section[i].using_list);
    }
        
		
    insert_buddy(&user_zone);
    
}


void init_kernel_pmm(uint64 phy_start,uint64 len)
{

    int i;
    printk(PT_DEBUG,"%s:%d,phy_start=%lx\n\r",__FUNCTION__,__LINE__,phy_start);
    kern_zone.phy_start = phy_start;
    kern_zone.phy_end = phy_start + len;
    kern_zone.free_pages = len/SIZE_4K;
    
	for (i = 0 ;i< MAX_ORDER; i++)
        list_init(&kern_zone.section[i].free_list);
        list_init(&kern_zone.section[i].using_list);
		
    insert_buddy(&kern_zone);

}


void init_pmm(uint64 phy_start, uint64 phy_end)
{
    long phy_len = phy_end - phy_start;
    long free_len = ALIGN_DOWN(phy_len, SIZE_4K);
    long kernel_free_len = free_len/2;
    long user_free_len = free_len - kernel_free_len;
    /*
        20240413 显然这里搞错了
    */
    init_kernel_pmm(phy_start,kernel_free_len);
    init_user_pmm(phy_start+kernel_free_len,user_free_len);
   
}
/*
    20240113 
*/
void init_user_buddy(uint64 virt_start, uint64 virt_end)
{
    int buddy_block_num;
    int buddy_block_byte;
    uint64 align_v_start;
    user_buddy_pool.virt_start = virt_start;
    user_buddy_pool.virt_end = virt_end;
    buddy_block_num = (virt_end - virt_start)/BUDDY_POOL_SIZE;
    buddy_block_byte = buddy_block_num/8;
    user_buddy_pool.pool_bitmap.btmp_bytes_len = buddy_block_byte;  // 代表buddy_pool 有多少section 块
    
    align_v_start = ALIGN_UP(virt_start,SIZE_4K);
    user_buddy_pool.pool_bitmap.bit_start = (uint8*)align_v_start;   //应该是虚拟地址
    bitmap_init(&user_buddy_pool.pool_bitmap);
}

void init_kern_buddy(uint64 virt_start, uint64 virt_end)
{
    int buddy_block_num;
    int buddy_block_byte;
    uint64 align_v_start;
    kern_buddy_pool.virt_start = virt_start;
    kern_buddy_pool.virt_end = virt_end;
    buddy_block_num = (virt_end - virt_start)/BUDDY_POOL_SIZE;
    buddy_block_byte = buddy_block_num/8;
    kern_buddy_pool.pool_bitmap.btmp_bytes_len = buddy_block_byte;  // 代表buddy_pool 有多少section 块
    
    align_v_start = ALIGN_UP(virt_start,SIZE_4K);
    kern_buddy_pool.pool_bitmap.bit_start = (uint8*)align_v_start;   //应该是虚拟地址
    bitmap_init(&kern_buddy_pool.pool_bitmap);
}

void init_buddy(uint64 virt_start, uint64 virt_end) // 全部改成虚拟地址
{
    
    int buddy_block_num;
    int buddy_block_byte;
    uint64 align_v_start;
    
    buddy_pool.virt_start = virt_start;
    buddy_pool.virt_end = virt_end;
    buddy_block_num = (virt_end - virt_start)/BUDDY_POOL_SIZE;
    buddy_block_byte = buddy_block_num/8;
    buddy_pool.pool_bitmap.btmp_bytes_len = buddy_block_byte;  // 代表buddy_pool 有多少section 块
    
    align_v_start = ALIGN_UP(virt_start,SIZE_4K);
    buddy_pool.pool_bitmap.bit_start = (uint8*)align_v_start;   //应该是虚拟地址
    bitmap_init(&buddy_pool.pool_bitmap);
    
   
    uint64 align_virt_start = ALIGN_UP(virt_start,8);
    uint64 align_virt_end = ALIGN_DOWN(virt_end,8);
    uint64 total_len = align_virt_end - align_virt_start;
    uint64 user_len = total_len/2;
    uint64 user_virt_start = align_virt_start;
    uint64 user_virt_end = align_virt_start+user_len;

    uint64 kern_virt_start = ALIGN_UP(align_virt_start+user_len,8);
    uint64 kern_virt_end = align_virt_end;
    init_user_buddy(user_virt_start,user_virt_end);
    init_kern_buddy(kern_virt_start,kern_virt_end);
   
    
}


