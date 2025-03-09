#ifndef __XOS_PAGE_H__
#define __XOS_PAGE_H__

#include "list.h"
#define PAGE_SHIFT 12

/*
    20240427：PM:14:51
    定义page 描述符
    page 大小使用order 来表示
    page 状态释放可用 使用flags 来描述
    page 与zone 关联需要引入链表头

    后续引入cache 那么page 也需要和cache 建立关联
    当前cache 还没有定义
    随着开发的深入page 结构也会不断变化，不是一蹴而就的
*/
typedef struct page_desc{
    dlist_t list;
    int order;
    int idle_flags;
    int ref_cnt;
    void *cache;
    xos_spinlock_t cache_lock;
    dlist_t attach_cache_list;
    /*attch slab replace attach_cache_list*/
    int page_num;
    int page_ref;
    struct address_space *cache_space; 

}xos_page_t;
/*
    expand and add   pg_free_list  and pg_using_list
*/
typedef struct page_cache{
    dlist_t pg_list;
    dlist_t pg_free_list;
    dlist_t pg_using_list;
    xos_spinlock_t pg_lock;

}xos_pg_cache_t;
#if 0
struct zone {
	/* zone watermarks, access with *_wmark_pages(zone) macros */
	unsigned long _watermark[NR_WMARK];//页分配器使用的水线
	unsigned long watermark_boost;
	unsigned long nr_reserved_highatomic;
	long lowmem_reserve[MAX_NR_ZONES];//当前区域预留多少页不能给高位的区域类型 
	struct pglist_data	*zone_pgdat;
	struct per_cpu_pageset __percpu *pageset;
	/* zone_start_pfn == zone_start_paddr >> PAGE_SHIFT */
	unsigned long		zone_start_pfn;//当前区域的起始
	 /* 伙伴分配器管理的物理页数量
	 * managed_pages is present pages managed by the buddy system, which
	 * is calculated as (reserved_pages includes pages allocated by the
	 * bootmem allocator):
	 *	managed_pages = present_pages - reserved_pages;
	 */
	 atomic_long_t		managed_pages;
	unsigned long		spanned_pages;
	unsigned long		present_pages;
	const char		*name;
	int initialized; 
	/* free areas of different sizes */
	struct free_area	free_area[MAX_ORDER];//不同长度的空间区域 
	/* zone flags, see below */
	unsigned long		flags; 
	/* Primarily protects free_area */
	spinlock_t		lock;
}
#endif
extern xos_pg_cache_t page_cache_block;

/*
    20250105  22:49
    这样不对，完全错误，每个文件都有各自的Page_cache
    文件与文件直接page_cache 是不关联耦合的
    所以page_cache 需要和文件的inode 唯一绑定
    改吧,

    20250106:
    本来我在inode 节点中定义了和cache_page 关联的结构
    dlist_t     *pg_list;
    xspinlock_t pg_lock;

    看了Linux 内核之后，将
    dlist_t     *pg_list;
    xspinlock_t pg_lock;
    封装成一个结构address_space
    
*/
static inline xos_pg_cache_t *get_page_cache_head()
{
    return &page_cache_block;
}

static inline int is_empty_page_cache(dlist_t *list_head)
{
    if((list_head->next == list_head)&&(list_head->prev == list_head)){
        return 1;
    }
    return 0;
}

extern int xos_page_cache_init();
extern void *xos_get_kern_page();
extern void * xos_get_free_page(int mode,int order);
extern  void *xos_get_phy(int mode,int order);
extern  void *xos_get_user_phy(int mode,int order);
extern  void* alloc_page (void);
extern int  get_order (uint32 v);
void*           kmalloc (int order);
void            kfree (void *mem, int order);

extern void xos_free_page(void *addr);

//#define xos_free_page(addr) xos_free_pages(addr,order)
#endif
