#ifndef __PHY_MEM_H__
#define __PHY_MEM_H__

#include "list.h"
#include "spinlock.h"
#include "bit_map.h"

#define MAX_ORDER 11
extern void init_pmm(uint64 phy_start, uint64 phy_end);
extern void init_buddy(uint64 virt_start, uint64 virt_end);



typedef struct free_node {
	dlist_t 	free_list;
    dlist_t     using_list;
	uint64 		free_cnt;
}free_node_t;

typedef struct struct_zone{
    uint64  phy_start;
    uint64  phy_end;
    xos_spinlock_t zone_lock;
    uint64 	free_pages;
    int order;
    free_node_t section[MAX_ORDER];
    dlist_t usr_list;
}zone_desc_t;

/*
    20240427 重新定义zone 结构
    这次实现要实现锁，锁的实现不能退后了

    仿照linux 内核引入xos_page ，是时候引入page 结构了
    原理定义的buddy_area 结构体名称并不是很好，一个好的名称对于项目的开发也是非常重要的
    
*/
/*
typedef struct zone_struct{
    uint32_t 	free_pages;
//	spinlock_t 	lock;
	xos_page_t* 	zone_page_start; //zone 区域物理起始页帧地址指针
	uint32_t 	size;  //zone 大小
	free_node_t section[MAX_ORDER];
}zone_t;
*/
typedef struct buddy_block{
    uint64 phy_start;
    uint64 phy_end;
    uint64 page_count;
    dlist_t list;
    dlist_t neig_list;   //左右邻居指针
    dlist_t using_list;
    int order;

}buddy_sec_t;



/*
    管理buddy_sec_t 块
*/
typedef struct buddy_pool{
    bitmap_t pool_bitmap;
    uint64 virt_start;
    uint64 virt_end;
    //lock

}buddy_pool_t;
    
extern zone_desc_t user_zone;
extern zone_desc_t kern_zone;

extern void* alloc_buddy(zone_desc_t *zone,uint32_t order);

#endif
