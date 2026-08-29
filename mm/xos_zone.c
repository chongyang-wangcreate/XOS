/********************************************************
    
    development start:2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    Copyright (c) 2024 - 2025 wangchongyang

********************************************************/

/*

    后续xos_get_page()第一个入参改成mode ,选择不同的mode ,会选择不同的zone 区域

2024.11.22 20:21
   zone.c 描述不应该存在budy 相关的大量定义，排版太乱需要重新做整理
    
*/


/*
    后续可以扩展成多个不同的zone 区域
*/



#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "bit_map.h"
#include "xos_mutex.h"
#include "mount.h"
#include "fs.h"
#include "xos_cache.h"
#include "xos_kern_def.h"
#include "mem_layout.h"
#include "xos_page.h"
#include "printk.h"
#include "xos_zone.h"
#include "spinlock.h"
#include "uart.h"
#include "memblock.h"

#define NORMAL_PAGE_SIZE (1 << 12)
#define BUDDY_ALIGN  (1 <(PAGE_SHIFT + MAX_ORDER -1))
extern uint8_t _kernel_page_array_start[];
extern uint8_t _kernel_page_array_end[];
extern uint8_t _user_page_array_start[];
extern uint8_t _user_page_array_end[];
extern uint8_t _dma_page_array_start[];
extern uint8_t _dma_page_array_end[];

/* Zone 全局变量 */
xos_zone_t zone_normal = {0};
xos_zone_t zone_user = {0};
xos_zone_t zone_dma = {0};
xos_zone_t zone_areas[ZONE_MAX];
static uint64 g_zone_linear_map_start;
static uint64 g_zone_linear_map_end;
int g_zone_layout_set_done;

/* 声明外部符号（来自lds） */
extern uint8_t _zone_metadata_start[];
extern uint8_t _kernel_page_array_start[];
extern uint8_t _user_page_array_start[];
extern uint8_t _dma_page_array_start[];
extern void put_hex(uint64_t val);


static xos_zone_layout_t  zone_layouts[ZONE_MAX] = {

    {
        .zone_id = ZONE_KERNEL,
        .name = "kernel",
        .size = ZONE_KERNEL_SIZE,
        .page_array = (xos_page_t*)_kernel_page_array_start,
        .page_array_end = (xos_page_t*)_kernel_page_array_end,

    },
    {
        .zone_id = ZONE_USER,
        .name = "user",
        .size = ZONE_USER_SIZE,
        .page_array = (xos_page_t*)_user_page_array_start,
        .page_array_end = (xos_page_t*)_dma_page_array_end,

    },
    {
        .zone_id = ZONE_DMA,
        .name = "dma",
        .size = ZONE_DMA_SIZE,
        .page_array = (xos_page_t*)_dma_page_array_start,
        .page_array_end = (xos_page_t*)_kernel_page_array_end,

    },
};

uint64 xos_get_zone_page_counts(const xos_zone_layout_t *layout)
{
    return ((uint64)layout->page_array_end - (uint64)layout->page_array) / sizeof(xos_page_t);

}

/*static void xos_build_zone_layout(void)
{
    uint64 start = ZONE_KERNEL_START;
    int i;
    for(i = 0; i < ZONE_MAX ;i++){
        zone_layouts[i].start = start;
        zone_layouts[i].end = start + zone_layouts[i].size - 1;
        start = zone_layouts[i].end + 1;

    }

}*/

const xos_zone_layout_t *xos_get_zone_layouts(int *count)
{
    if(count != NULL){
        *count = ZONE_MAX;
    }
    return zone_layouts;

}

static void xos_zone_reset_pages(xos_zone_t *zone,int zone_id)
{
    uint32_t i;
    for(i = 0; i < zone->z_pfn_cnt ; i++){
        zone->z_vmempage[i].idle_flags = 0;
        zone->z_vmempage[i].order = -1;
        zone->z_vmempage[i].zone_type = zone_id;
        zone->z_vmempage[i].ref_cnt = 0;
        list_init(&zone->z_vmempage[i].list);
    }

}

static void xos_zone_bind(xos_zone_t *zone,const xos_zone_layout_t *layout)
{
    zone->z_pfn_cnt = layout->size >> PAGE_SHIFT;
    zone->z_vm_cnt = zone->z_pfn_cnt;
    zone->z_vmempage = layout->page_array;
    zone->free_pages = zone->z_pfn_cnt;
    zone->start_pfn = layout->start >> PAGE_SHIFT;
    zone->end_pfn = layout->end >> PAGE_SHIFT;
    xos_spinlock_init(&zone->slock);
    xos_zone_reset_pages(zone,layout->zone_id);
}
/*
    判断当前page 的伙伴是否空闲，如果空闲可以合并
    1. 找到当前Page 的伙伴
    2. 判断伙伴是否空闲
*/
static int xos_page_is_buddy_free(xos_page_t *page, uint8_t order)
{
    if((page->order == order)&&(page->idle_flags == 0)) /*当前阶次是否相同，页状态是否空闲*/
        return 1;
    return 0;
}
/*
    释放page 到buddy 子系统
    notice  
*/

static int xos_free_page_to_buddy(xos_zone_t *zone,xos_page_t *page,int order)
{
//    free_area_t *free_area;
    xos_page_t *part_page;
    unsigned long part_index;
    uint32_t page_index = page - zone->z_vmempage;
    /*
        更新free_pages 个数，如果当前order = x,那么free_pages += (1 << order)
        如果后续存在合并那么还会free_pages 还需要进行更改吗，不需要，free_pages 我统计的是空闲页的数量，
        合并之后free_pages 值并不会变化
    */
    zone->free_pages += 1 << order; 

/*
    假如开始order 阶是0
    page_index = 0 , 或是是 page_index = 1 ，假如page_index = 0 h和page_index = 1
    都处于空闲状态，那么两个页可以合并成一个order = 1的页
    下一个要循环查看的Page_index 就是两个伙伴相与的值，这时规律 
    也就是order = 0 需要order++， 这时order = 1;

    当前order = 1; 0-1 作为一个的的page,page->order = 1;
    那么它的伙伴就是0 ^(1 << 1) = 2 它的伙伴就是2--3这一个页
    为了简单起见，就是order = 1 阶次没有空闲的伙伴来合并，
    while 循环会退出，将当前page 加入到order = 1 ,free_area[order].list
    链表中
    
*/
    while (order < MAX_ORDER - 1){

//        free_area = zone->free_area + order;
        part_index = page_index ^(1 << order);  //使用异或算法找伙伴,异或交换律 0 1是伙伴 2 3 也是伙伴
        part_page = zone->z_vmempage + part_index;// 找到伙伴page
        /*
            判断part_page 是否空闲，伙伴是否，如果空闲可以向高阶合并,
            当前page order 是否是当前order
        */
        if(!xos_page_is_buddy_free(part_page,order)){
            /*
                伙伴并不空闲，不能向更高阶合并，退出循环
            */
            break;
        }
        /*
            伙伴空闲，可以向上合并成更高阶，当前阶次链表空闲节点数量减少1
        */
        zone->free_area[order].node_nfree--;
        /*
            将当前节点从当前阶次链表中删除
            这个分析有误，当时卡了几个消息，从链表中删除的应该是伙伴节点
            当前节点并不在链表中如果删除出现错误。
        */
        list_del(&part_page->list); 
        part_page->order = -1;
        page_index &= part_index; //伙伴相与，确定下一个page_index位置

        order++; //阶次增加1
        
        
    }
   
    page = zone->z_vmempage + page_index;
//    printk("%s:%d,zone->z_pfn_cnt=%d,page_index=%d,order=%d\n\r",__FUNCTION__,__LINE__,zone->z_pfn_cnt,page_index,order);
    /*
        我举得例子page_index 还是0，但是page0 page1 进行了合并
        所以page order 值变成高阶
    */
    page->order = order;
    page->idle_flags = 0;
    list_add_back(&page->list,&zone->free_area[order].free_list);
    zone->free_area[order].node_nfree++;  /*当前阶次空闲块数量加1 ，主要这里的node_free 统计的可不是页的数量*/

    return 0;
}

/*
    后续去掉讨厌的zone 参数 20240608
*/
int  xos_free_pages(xos_zone_t *zone,xos_page_t *page,int order)
{
    /*
        内存分配器属于共享资源，可能存在竞争冲突，所以使用前应当加锁
        使用后要解锁，锁实现完内存管理之后再实现自旋锁
        xos_spinlock
        xos_free_page_to_buddy();
        xos_spinunlock
    */
    return xos_free_page_to_buddy(zone,page,order);
}
/*
    在思维不清晰的时候画图是个非常好的方法
    page 描述图order = 3
    我现在要从page->order = 3 拆分成page->order = 0 并且返还给调用者，其它的加入合适的Order 链表
    ----|-----|----|-----|-----|----|----|-----|

    ----|-----|----|-----|-----|----|----|-----|
    
 page->order = 2            page->order= 2
    ----|-----|----|-----|  ---|----|----|-----|

    ----|-----|----|-----|  ---|----|----|-----|
    一部分加入order 链表一部分继续拆分
                                page->order = 1
                           ----|----|  ---|----|
                                        page->order = 0

                                       ---|  --|
     
*/
xos_page_t *xos_split_page(xos_zone_t *zone,xos_page_t *page,int cur_order,int high_order)
{
    /*
        每次拆分order 下降1阶次
        需要加锁
    */
    xos_page_t *tmp_page;
    while(cur_order < high_order){
       
        high_order--;
        page->order--;
        tmp_page = page+(1<<high_order); //后半部分
        tmp_page->order = high_order;
        tmp_page->idle_flags = 0;
        tmp_page->ref_cnt = 0;
        list_add_back(&tmp_page->list, &zone->free_area[high_order].free_list);
        zone->free_area[high_order].node_nfree++;
    }
    page->ref_cnt++;
    return page;
}
xos_page_t *xos_get_page(xos_zone_t *zone,int order)
{
    /*
        先查找本order ，从free_list 查看链表是否为空，为空证明本order 没有空闲，无法从本order 分配内存
        需要从更改级别order 查找，拆分
    */
    int org_order = order;
    int high_order;
    dlist_t *tmp_list;
    xos_page_t *tmp_page;
    while(zone->free_area[order].free_list.next == &zone->free_area[order].free_list){
        /*
            遍历Order 先找到最接近order
        */
        if(order == MAX_ORDER - 1){
            return NULL;
        }
        order++;

    }
    high_order = order;
    tmp_list = zone->free_area[high_order].free_list.next; //获取第一个节点
    tmp_page = list_entry(tmp_list, xos_page_t, list);
    list_del(tmp_list);//脱链
    zone->free_area[high_order].node_nfree--;
    /*
        拆分当前page,并且将拆分后的Page加入匹配的free_area[order]链表
    */
    return xos_split_page(zone ,tmp_page,org_order,high_order); 
}

int xos_insert_to_buddy(xos_zone_t *zone)
{
    int32_t i = 0;
//  int max_pfn_cnt;   

    xos_page_t* v_page_buf = zone->z_vmempage;

    for (i = 0 ;i< MAX_ORDER; i++)
        list_init(&zone->free_area[i].free_list);

    /*
        遍历每一个页帧，如果当前页帧空闲，将页帧加入buddy 子系统
    */
//  max_pfn_cnt = (zone->z_pfn_cnt < zone->z_vm_cnt)?zone->z_pfn_cnt:zone->z_vm_cnt;
//    printk(PT_DEBUG,"zone->z_pfn_cnt=%d\n\r",zone->z_pfn_cnt);
    for(i = 0; i < zone->z_pfn_cnt;i++){
        /*
            判断页是否空闲，如果页空闲则加入buddy 子系统
            当前按照单个page 加入到Buddy子系统也就是加入到
            zone->free_area[0].free_list 链表头中

            还应当考虑当前释放的块是否有伙伴
        */
        if(v_page_buf[i].idle_flags == 0 || v_page_buf[i].order == -1){
            xos_free_pages(zone,&v_page_buf[i],0);
        }

    }
    return 0;

}



static uint64 get_min(uint64 a,uint64 b)
{
    return a < b ? a:b;
}
static int xos_set_zone_layouts(void)
{
    xos_memblock_info_t *memblock = xos_memblock_get_info();
    uint64 usable_start;
    uint64 usable_end;
    uint64 remaining_size;
    uint64 start;
    uint64 default_size[ZONE_MAX];
    uint64 total_desired_size;
    uint64 tail_keep;
    uint64 size;
    int i;
    if(memblock == NULL || !memblock->vaild || memblock->usable.size == 0){
        return -1;
    }
    usable_start = ALIGN_UP(memblock->usable.base,BUDDY_ALIGN);
    usable_end = ALIGN_DOWN(memblock->usable.base + memblock->usable.size,PAGE_SIZE);
    if(usable_end <= usable_start){
        return -1;
    }
    remaining_size = usable_end - usable_start;
    start = usable_start;
    default_size[ZONE_KERNEL] = 256UL * 1024UL *1024UL;
    default_size[ZONE_USER]   = 128UL * 1024UL *1024UL;
    default_size[ZONE_DMA]    = 128UL * 1024UL *1024UL;
    total_desired_size = default_size[ZONE_KERNEL] + default_size[ZONE_USER] + default_size[ZONE_DMA];

    for(i = 0; i < ZONE_MAX ;i++){
        uint64 max_pages = xos_get_zone_page_counts(&zone_layouts[i]);
        size = get_min(default_size[i],remaining_size);
        if(remaining_size < total_desired_size){
            if(i == ZONE_KERNEL){
                size = remaining_size / 2;
            }else if(i == ZONE_USER){
                size = remaining_size - (remaining_size / 2);
            }else{
                size = 0;
            }
        }
        if(i + 1 < ZONE_MAX){
            tail_keep = (ZONE_MAX -i -1)*PAGE_SIZE;
            if(remaining_size > tail_keep && size > (remaining_size - tail_keep)){
                size = remaining_size - tail_keep;
            }
        }
        size = ALIGN_DOWN(size,PAGE_SIZE);
        if(size > (max_pages << PAGE_SHIFT)){
            size = max_pages << PAGE_SHIFT;
        }
        zone_layouts[i].start = start;
        zone_layouts[i].size = size;
        if(size != 0){
            zone_layouts[i].end = start + size -1;
            start += size;
            remaining_size -= size;
        }else{
            zone_layouts[i].end = start ?(start - 1):0;
        }
    }
    g_zone_linear_map_start = zone_layouts[ZONE_KERNEL].start;
    
    for(i = ZONE_MAX -1 ;i >= 0; i--){
        if(zone_layouts[i].size != 0){
            g_zone_linear_map_end = zone_layouts[i].end;
        }
    }
    g_zone_layout_set_done = 1;
    return 0;

}

uint64 xos_zone_linear_map_start(void)
{
    return g_zone_linear_map_start;
}
uint64 xos_zone_linear_map_end(void)
{
    return g_zone_linear_map_end;
}
int xos_zone_set()
{
    return xos_set_zone_layouts();
}
void zone_early_init(void)
{
    int i;
    xos_zone_t *zones[ZONE_MAX] = {

        &zone_normal,
        &zone_user,
        &zone_dma,
    };
    if(!g_zone_layout_set_done){
        return ;
    }
   // xos_build_zone_layout();
    for(i = 0; i < ZONE_MAX ; i++){
        if(zone_layouts[i].size == 0){
            continue;
        }
        xos_zone_bind(zones[i],&zone_layouts[i]);
        xos_insert_to_buddy(zones[i]);
    }
}
/*
    使用平坦模型思想，开发自己的buddy 子系统,当前实现比较简单,本阶段暂时是实现基本功能，后续
    会重写buddy 子系统，支持稀疏模型慢慢迭代
    要定义几个zone 区域比较合适呢
    zone_normal

    需要给用户态分配zone 区域吗？malloc 申请的空间从这块区域切割？

    需要给page 数组分配buf
    buf 起始位置用zone_normal.z_vmempage 来描述，或者说是指向


    伙伴系统算法的核心是 伙伴, 那什么是伙伴呢? 在Linux内核中, 把两个物理地址相邻的内存页当作成伙伴, 因为Linux是以页面号来管理内存页的, 所以就是说两个相邻页面号的页面是伙伴关系. 
    但是并不是所有相邻页面号的页面都是伙伴关系, 例如0号和1号页面是伙伴关系, 但是1号和2号就不是了. 为什么呢? 这是因为如果把1号页面和2号页面当成伙伴关系, 那么0号页面就没有伙伴从而变成孤岛了.
    其实对于内存管理这是本系统xos 第三版内存管理

    我自己引入virt_start  ,virt_end 其实没什么用，而且看起来怪怪的，指定虚拟地址空间范围
    来限制物理页的个数，非常奇怪，去掉吧
    void xos_zone_init(uint64 virt_start, uint64 virt_end,uint32_t phy_mm_start,uint32_t phy_mm_end)

    需要找一块空间存放系统成百上千个page 描述符，我当前的设计还需要进行优化

    reserve 一段空间，用来分配给page 使用
    
*/


void zone_init(xos_zone_t * zone_area, int zone_id)
{

    xos_spinlock_init(&zone_area->slock);
    xos_insert_to_buddy(zone_area);

}




void xos_zone_init()
{
    zone_early_init();
    
}

void test_buddy()
{
    xos_page_t *ptr = xos_get_page(&zone_normal,0);
    printk(PT_DEBUG,"%s:%d,xxxxxxxxxptr_addr=%llx\n\r",__FUNCTION__,__LINE__,(long long)ptr);

    printk(PT_DEBUG,"addr0 = %lx\n\r",XOS_PAGE_TO_PHY(ptr));
    
    ptr = xos_get_page(&zone_normal,0);
    printk(PT_DEBUG,"%s:%d,xxxxxxxxxptr_addr=%llx\n\r",__FUNCTION__,__LINE__,(long long)ptr);



    ptr = xos_get_page(&zone_normal,4);
    printk(PT_DEBUG,"%s:%d,xxxxxxxxxptr_addr=%llx\n\r",__FUNCTION__,__LINE__,(long long)PHY_TO_VIRT((XOS_PAGE_TO_PHY(ptr))));
    printk(PT_DEBUG,"ptr->order=%d\n\r",ptr->order);

    ptr = xos_get_page(&zone_user,4);
    printk(PT_DEBUG,"ptr->order=%d\n\r",ptr->order);
    

}