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

#define NORMAL_PAGE_SIZE (1 << 12)



/*

    后续xos_get_page()第一个入参改成mode ,选择不同的mode ,会选择不同的zone 区域

2024.11.22 20:21
   zone.c 描述不应该存在budy 相关的大量定义，排版太乱需要重新做整理
    
*/
xos_zone_t zone_normal;
xos_zone_t zone_dma;
xos_zone_t zone_user;

/*
    后续可以扩展成多个不同的zone 区域
*/
xos_zone_t zone_areas[ZONE_MAX];
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
    printk(PT_DEBUG,"%s:%d,zone->z_pfn_cnt=%d\n\r",__FUNCTION__,__LINE__,zone->z_pfn_cnt);
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
    
    /*
    ptr = xos_get_page(&zone_dma,4);
    printk("%s:%d,zone_dma  ptr_addr=%llx\n\r",__FUNCTION__,__LINE__,(long long)(ptr));
    printk("zone_dma  ptr->order=%d\n\r",ptr->order);
    */
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

void zone_early_init()
{

    /*

        规划保留一段物理内容，提供给mem_page_buf 来使用
        这段内存足够大，mem_page_buf[],可以描述当前系统的所有页

        init_normal_zone  zone_mappage
        init_dma_zone     dma_mappage
        int_usr_zone      usr_mappage

        我是这样规划的，暂时先使用共一个normal_zone

        后续扩充多个zone_area

        我当前规划的只有一个zone_normal 所以规划的这块区域mem 配置都归为zone_nomal

        开始写代码吧let's go
        
    */


    /*
        计算zone_normal 管理的内存区域pfn 的个数
        确定号start_pfn (是否需要2的次幂)，start_pfn 可以是奇数吗？
        start_pfn 是否是2的次幂以及start_pfn 是奇数或者偶数并并比影响开发
        本区域的pfn 号还是村0 开始，还是满足2次幂要求

        当前页4K 不存在大页
        计算normal_maxpfn


        free_pages = NORMAL_ZONE_MEM_SIZE/sizeof(xos_page_t)

        两个区域需要做比较，page 表示的区域，和真实可用物理空间区域做比较
        去较小值


    */
    int i = 0;
    zone_normal.z_pfn_cnt = (NORMAL_ZONE_MEM_SIZE >> PAGE_SHIFT);
    zone_normal.z_vmempage = (xos_page_t*)PHY_TO_VIRT(PHY_MEM_PAGE_START);
    zone_normal.free_pages = zone_normal.z_pfn_cnt;  
    zone_normal.start_pfn = NORMAL_ZONE_PHY_START >> PAGE_SHIFT;
    zone_normal.end_pfn = NORMAL_ZONE_PHY_END >> PAGE_SHIFT;
    for(i = 0;i < zone_normal.z_pfn_cnt;i++){
        zone_normal.z_vmempage[i].idle_flags = 0; /*设置当前page 为空闲状态*/
        zone_normal.z_vmempage[i].order = -1; /*当前order值设置成0阶*/
    }
    
    zone_user.z_pfn_cnt = (USER_ZONE_MEM_SIZE >> PAGE_SHIFT);
    zone_user.z_vmempage =  zone_normal.z_vmempage+zone_normal.z_pfn_cnt;
    zone_user.free_pages = zone_user.z_pfn_cnt;  
    zone_user.start_pfn = USER_ZONE_PHY_START >> PAGE_SHIFT;
    zone_user.end_pfn = USER_ZONE_PHY_END >> PAGE_SHIFT;
    for(i = 0;i < zone_user.z_pfn_cnt;i++){
        zone_user.z_vmempage[i].idle_flags = 0; /*设置当前page 为空闲状态*/
        zone_user.z_vmempage[i].order = -1; /*当前order值设置成0阶*/
    }
    #if 0
    zone_dma.z_pfn_cnt = (DMA_ZONE_PHY_START >> PAGE_SHIFT);
    zone_dma.z_vmempage =  zone_user.z_vmempage+zone_user.z_pfn_cnt;
    zone_dma.free_pages = zone_dma.z_pfn_cnt;  
    zone_dma.start_pfn = DMA_ZONE_PHY_START >> PAGE_SHIFT;
    zone_dma.end_pfn =   DMA_ZONE_PHY_END >> PAGE_SHIFT;
    for(i = 0;i < zone_dma.z_pfn_cnt;i++){
        zone_dma.z_vmempage[i].idle_flags = 0; /*设置当前page 为空闲状态*/
        zone_dma.z_vmempage[i].order = -1; /*当前order值设置成0阶*/
    }
    #endif
}



void zone_init(xos_zone_t * zone_area, int zone_id)
{

    xos_spinlock_init(&zone_area->slock);
    xos_insert_to_buddy(zone_area);

}

void mem_init()
{

    zone_init(&zone_areas[ZONE_KERNEL], 0);
    zone_init(&zone_areas[ZONE_USER], 0);
    zone_init(&zone_areas[ZONE_DMA], 0);

}


int  single_zone_init(uint32 phy_page_start,uint32 phy_page_end, uint32_t zone_phy_start,uint32_t zone_phy_end,int zone_id)
{

    /*
        zone_phy_start 向上4K 对齐
        zone_phy_end   向下4K 对齐

        z_vmempage 这个当前不能改变，都是提前规划好的，后续
        z_vmmempage 赋值设置成从函数获取

    */
    int i;
    xos_zone_t *cur_zone = NULL;
    uint32_t zone_phy_start_align = align_up(zone_phy_start, PAGE_SIZE);
    uint32_t zone_phy_end_align =   align_down(zone_phy_end, PAGE_SIZE);
    uint32_t zone_size = zone_phy_end_align - zone_phy_start_align;
    if(zone_id > ZONE_MAX){
        return -1; //非法参数
    }
    cur_zone = &zone_areas[zone_id];

    cur_zone->z_vm_cnt = ((phy_page_end - phy_page_start) >> PAGE_SHIFT);
    cur_zone->z_pfn_cnt = (zone_size >> PAGE_SHIFT);
    cur_zone->z_vmempage = (xos_page_t*)PHY_TO_VIRT(phy_page_start);
    cur_zone->free_pages = cur_zone->z_pfn_cnt;
    cur_zone->start_pfn = zone_phy_start_align >> PAGE_SHIFT;
    cur_zone->end_pfn = zone_phy_end_align >> PAGE_SHIFT;
    for(i = 0;i < cur_zone->z_pfn_cnt;i++){
        cur_zone->z_vmempage[i].idle_flags = 0; /*设置当前page 为空闲状态*/
        cur_zone->z_vmempage[i].order = -1; /*当前order值设置成0阶*/
    }

    
    for (i = 0 ;i< MAX_ORDER; i++){
        
        list_init(&cur_zone->free_area[i].free_list);
        cur_zone->free_area[i].order = i;
    }
    
    return 0;
}

void org_zone_area_init()
{
    /*
        实现多zone_area 规划并初始化

        zone_normal :  a --b
        zone_dma    :  b --d
        zone_user   :  d---f

        single_zone_init( kern_zone_phy_start,kern_zone_phy_end);
        single_zone_init( dma_zone_phy_start,dma_zone_phy_end);
        single_zone_init( usr_zone_phy_start,usr_zone_phy_end);
    */
    
    single_zone_init(KERNEL_MEM_PAGE_START,KERNEL_MEM_PAGE_END,kern_zone_phy_start,kern_zone_phy_end,ZONE_KERNEL);
    single_zone_init(USER_MEM_PAGE_START,USER_MEM_PAGE_END,usr_zone_phy_start,usr_zone_phy_end,ZONE_USER);
    single_zone_init(DMA_MEM_PAGE_START,DMA_MEM_PAGE_END,dma_zone_phy_start,dma_zone_phy_end,ZONE_DMA);
    

}


void xos_zone_init()
{
    zone_early_init();

    zone_init(&zone_normal, 0);
        
    zone_init(&zone_user, 0);

    //  zone_init(&zone_dma, 0); /*当前zone_dma 存在问题原因映射区域太小只有558M*/
    
}

