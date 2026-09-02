/********************************************************
    
    development started: 2024
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024-2027 wangchongyang

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
#include "arch_irq.h"
#include "spinlock.h"
#include "printk.h"
#include "xos_zone.h"
#include "xos_page.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "setup_map.h"
#include "mmu.h"
extern xos_zone_t zone_normal;

#define CACHE_SLAB_MAGIC  0x58434d53U
#define CACHE_LARGE_MAGIC 0x58434d4cU

typedef struct cache_alloc_hdr {
    uint32_t magic;
    mem_obj_t *mem_node;
} cache_alloc_hdr_t;

/*
    20240406 PM:15:14
    cache 管理的内存从buddy获取,
    kmalloc  zalloc  首先是遍历对应的size 长度的cache 链表，如果没有空闲节点
    则get_free_page 从buddy 申请，然后将申请的pages 也拆分成对应cache_size 大小块
    将各个块加入到对应的cache_size 链表中


    
20240518 AM:10:59

    画图是最好的思路解析器， sourceinsight 不方便画图

                ----|----|----|----|
                ----|----|----|----|
从buddy 子系统申请page,然后拆分page
cache->size 设置成哪些大小，cache->size 集合，设置成2的次幂比较好{32，64，128，256，512，1024，2048}
为何2的次幂比较好，这样我可以使用相与的方式，异或都比较方便，提高效率

还是举例子方式 如果一个进程向内核申请32字节空间大小，
首先内存管理系统会判断申请空间大小，如果大于4K page 则直接从buddy 内存管理器申请
如果申请空间小于4K,则从cache 内存管理子系统申请，
buddy 内存管理子系统，管理整页内存
cache 内存管理子系统，管理小块内存
两个内存管理子系统的管理在于cache 内存管理子系统从buddy 子系统申请空间然后释放到cache 子系统中

如何来定义struct xos_cache 结构
第一我申请32字节，这时分配器会选择cache 分配器，cache 分配器会检测释放有空闲匹配的空间，
如果没有则向高级查找，有点类似buddy 子系统，如果找到最高价都是空，则从buddy 子系统申请



20240609 当前cache 只实现了kmalloc 功能，还没有实现kfree，kmalloc 也一直没有做过压力测试

当前设计的cache 管理还是非常简陋，并且block 块尺寸过大的情况下，存在空间浪费严重问题
需要进一步更改设计


2024.11.29：
    当前的 2024 年4月份设计cache 管理有必要优化进行优化,也存在相应的问题,最大的问题是cache 管理头和分配的
    数据块相连，后续cache 需要单独内存管理空间，必须和分配的内存块分离。

2024.12.21 AM:11:06 cache 管理还是存在Bug,写的也很烂，后续重写内存管理xos_slab.c，当前内存管理实现比较烂
    总比没有强
*/


xos_cache_t  cache_val; 



#define GET_INDEX(size,order) ((size >> order) -1) 
#define DEFAULT_ORDER 5
xos_cache_t  mem_size_set[7]={
    {
        .cache_bsize = 32,
    },
    {
        .cache_bsize = 64,
    },
    {
        .cache_bsize = 128,
    },
    {
        .cache_bsize = 256,
    },
    {
        .cache_bsize = 512,
    },
    {
        .cache_bsize = 1024,
    },
    {
        .cache_bsize = 2048,
    },
};

#define MEM_SET(index) (mem_size_set[index])
#define GET_MEM_BLOCK(index) (&mem_size_set[index].cache_block)
#define GET_BLOCK_SIZE(index) (mem_size_set[index].cache_bsize)
/*
    所以说开始free_list 可以挂载多个mem_obj 节点
    关于申请的块数量，并不需要都相同
    每个mem_obj 管理对应宽带的block ,上限1024块
    开始mem_pool 没有可管理的内存块，申请内存是首先向系统申请一块大内存，然后将内存块格式化加入到mem_obj
    32 字节  32*1024 = 32k+(mem_obj头+bitmap(空间))
    64 字节  64*1024 = 64k+
    128 字节 128*512 = 64k(mem_obj 头+ bitmap(空间))
    256      256*256
    512      512*128  //重点扩充一下 512*512
    1024     1024*64
    除了32字节块申请1024块其它块
    默认申请64k大小 给一个mem_obj 管理 实际申请的空间是malloc(64k+mem_obj+bitmap(空间)) 设置成申请64+4k
    这样可以计算出块数，也初始化了Bitmap

*/
#define OBJ_ALLOC_SIZE 64*1024
void osp_mem_block_init(xos_cache_t *cache,int size)
{
	/*
        要做哪些工作，通过，现在定义了6个不同尺寸的pool
        初始化要做哪些工作呢 初始化的主要工作就是初始化
        mem_block_t 结构成员
    */
    cache->cache_block.obj_block_size = cache->cache_bsize;
    /*
        32字节块特殊处理，总空间只分配32k+1k
        32 字节  32*1024 = 32k+(mem_obj头+bitmap(空间)) 
    */
    
    if(size == 32){
       cache->cache_block.obj_block_count = 1024;
    }else if(size == 512){
        cache->cache_block.obj_block_count = 512;
    }else{
        cache->cache_block.obj_block_count = OBJ_ALLOC_SIZE/cache->cache_block.obj_block_size;
    }
    list_init(&cache->cache_block.free_list);
    list_init(&cache->cache_block.partial_list);
    list_init(&cache->cache_block.full_list);
//  pthread_mutext_init(&cache->mem_block.lock);
}
void mem_cache_init()
{
    int i;
    for(i = 0; i < sizeof(mem_size_set)/sizeof(xos_cache_t);i++){
        osp_mem_block_init(&mem_size_set[i],mem_size_set[i].cache_bsize);
    }
}

static void cache_list_move(dlist_t *node, dlist_t *head)
{
    list_del(node);
    list_add_back(node, head);
}

static void cache_slab_build_free_list(mem_obj_t *mem_node)
{
    char *block;
    int i;

    mem_node->free_head = NULL;
    for(i = mem_node->total_count - 1; i >= 0; i--){
        block = mem_node->start_addr + i * mem_node->block_size;
        *(void **)block = mem_node->free_head;
        mem_node->free_head = block;
    }
}

static mem_obj_t *cache_new_slab(cache_block_t *cache_block)
{
    mem_obj_t *mem_node;
    char *p_start;
    uint64_t start_addr;
    uint64_t alloc_size;
    int order;

    order = 0;
    alloc_size = PAGE_SIZE;
    while(alloc_size < (uint64_t)OBJ_ALLOC_SIZE &&
          alloc_size < ((uint64_t)cache_block->obj_block_size *
                        cache_block->obj_block_count + sizeof(mem_obj_t))){
        order++;
        alloc_size = PAGE_SIZE << order;
    }

    p_start = (char*)xos_get_free_page(0, order);
    if(p_start == NULL){
        return NULL;
    }

    mem_node = (mem_obj_t*)p_start;
    mem_node->magic = CACHE_SLAB_MAGIC;
    mem_node->use_count = 0;
    mem_node->block_size = cache_block->obj_block_size;
    mem_node->page_order = order;
    mem_node->cache_block = cache_block;
    list_init(&mem_node->list);

    start_addr = ALIGN_UP((uint64_t)(p_start + sizeof(mem_obj_t)), 8);
    mem_node->start_addr = (char *)start_addr;
    mem_node->total_count = (int)((p_start + alloc_size - mem_node->start_addr) /
                                  mem_node->block_size);
    mem_node->free_count = mem_node->total_count;
    if(mem_node->total_count <= 0){
        mem_node->magic = 0;
        xos_free_page(p_start);
        return NULL;
    }

    cache_slab_build_free_list(mem_node);
    list_add_back(&mem_node->list, &cache_block->free_list);
    return mem_node;
}

static void *cache_slab_alloc_one(mem_obj_t *mem_node)
{
    cache_alloc_hdr_t *hdr;
    char *block;
    void *addr;

    if(mem_node->free_head == NULL || mem_node->free_count <= 0){
        return NULL;
    }

    block = mem_node->free_head;
    mem_node->free_head = *(void **)block;
    mem_node->use_count++;
    mem_node->free_count--;

    hdr = (cache_alloc_hdr_t *)block;
    hdr->magic = CACHE_SLAB_MAGIC;
    hdr->mem_node = mem_node;
    addr = (void *)(hdr + 1);
    return addr;
}

static void cache_slab_requeue_after_alloc(mem_obj_t *mem_node)
{
    cache_block_t *cache_block = mem_node->cache_block;

    if(mem_node->free_count == 0){
        cache_list_move(&mem_node->list, &cache_block->full_list);
    }else if(mem_node->use_count == 1){
        cache_list_move(&mem_node->list, &cache_block->partial_list);
    }
}

static int cache_slab_contains_free_block(mem_obj_t *mem_node, void *addr)
{
    void *node;
    int count = 0;

    node = mem_node->free_head;
    while(node != NULL && count < mem_node->free_count){
        if(node == addr){
            return 1;
        }
        node = *(void **)node;
        count++;
    }
    return 0;
}

static int cache_alloc_order(int size)
{
    int order = 0;
    int pages = (size + (int)sizeof(mem_obj_t) +
                 (int)sizeof(cache_alloc_hdr_t) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    while((1 << order) < pages){
        order++;
    }
    return order;
}

static void *cache_large_alloc(int size)
{
    mem_obj_t *mem_node;
    cache_alloc_hdr_t *hdr;
    char *p_start;
    int order;

    order = cache_alloc_order(size);
    p_start = (char *)xos_get_free_page(0, order);
    if(p_start == NULL){
        return NULL;
    }

    mem_node = (mem_obj_t *)p_start;
    mem_node->magic = CACHE_LARGE_MAGIC;
    mem_node->start_addr = (char *)ALIGN_UP((uint64_t)(p_start + sizeof(mem_obj_t)), 8);
    mem_node->free_head = NULL;
    mem_node->total_count = 1;
    mem_node->free_count = 0;
    mem_node->use_count = 1;
    mem_node->block_size = size;
    mem_node->page_order = order;
    mem_node->cache_block = NULL;
    list_init(&mem_node->list);

    hdr = (cache_alloc_hdr_t *)mem_node->start_addr;
    hdr->magic = CACHE_LARGE_MAGIC;
    hdr->mem_node = mem_node;
    return (void *)(hdr + 1);
}

/*
    当前mem_cache 基本问题是，cache 无可用空间时，只能分配一个页进行拆分
    我的mem_node 获取是基于page 页首地址，这样释放addr 4K 对齐时就可以
    找到对应mem_node 结构体

*/
static void* __mem_cache_alloc(cache_block_t *cache_block)
{
    mem_obj_t  *mem_node;
    dlist_t *list_node;
    void *alloc_addr = NULL;
    unsigned long flags;

    flags = arch_local_irq_save();

    if(!list_is_empty(&cache_block->partial_list)){
        list_node = cache_block->partial_list.next;
        mem_node = list_entry(list_node, mem_obj_t, list);
    }else if(!list_is_empty(&cache_block->free_list)){
        list_node = cache_block->free_list.next;
        mem_node = list_entry(list_node, mem_obj_t, list);
    }else{
        mem_node = cache_new_slab(cache_block);
        if(mem_node == NULL){
            goto out;
        }
    }

    alloc_addr = cache_slab_alloc_one(mem_node);
    if(alloc_addr != NULL){
        cache_slab_requeue_after_alloc(mem_node);
    }
out:
    arch_local_irq_restore(flags);
    return alloc_addr;
}


int get_base2_index(int num)
{
    int exponent = 0;
    int num_bak = num;
    while(num > 1){
        num /= 2;
        exponent++;
    
    }
    if(num_bak%2){
        exponent++;
    }
    return exponent;
}

int get_size_index(int size)
{
    int index;
    for(index = 0; index < (int)(sizeof(mem_size_set) / sizeof(xos_cache_t)); index++){
        if(size <= mem_size_set[index].cache_bsize){
            return index;
        }
    }
    return (int)(sizeof(mem_size_set) / sizeof(xos_cache_t)) - 1;
}

void *mem_cache_alloc(int size)
{
    cache_block_t *local_cache_block;
    int index;
    int need_size;

    need_size = size + sizeof(cache_alloc_hdr_t);
    if(need_size > mem_size_set[6].cache_bsize){
        return cache_large_alloc(size);
    }

       
    index = get_size_index(need_size);
    local_cache_block = GET_MEM_BLOCK(index);

    local_cache_block->obj_block_size = GET_BLOCK_SIZE(index);


    return __mem_cache_alloc(local_cache_block);
}

/*
    mem_cache 管理是基于buddy 管理的
    在释放addr 空间时，首先要做的就是确定addr 对应的paddr 属于那个页帧
    1. addr 4K 对齐
    2. addr 转换位paddr
    3. 通过paddr 获取页帧或4K 对齐物理地址
    4.获取地址对应的mem_node 结构
    5.通过mem_node 获取buff 管理的start_addr

*/

void mem_cache_free(void *addr)
{
    mem_obj_t  *mem_node;
    cache_alloc_hdr_t *hdr;
    char *block;
    unsigned long flags;
    if(addr == NULL){
        return;
    }

    flags = arch_local_irq_save();
    hdr = ((cache_alloc_hdr_t *)addr) - 1;
    mem_node = hdr->mem_node;

    if(mem_node == NULL || hdr->magic != mem_node->magic){
        printk(PT_ERROR,"%s:%d invalid cache addr=%lx\n\r",
               __FUNCTION__,__LINE__,(unsigned long)addr);
        arch_local_irq_restore(flags);
        return;
    }

    if(mem_node->magic == CACHE_LARGE_MAGIC){
        hdr->magic = 0;
        mem_node->magic = 0;
        arch_local_irq_restore(flags);
        xos_free_page((void *)mem_node);
        return;
    }

    if(mem_node->magic != CACHE_SLAB_MAGIC || mem_node->cache_block == NULL){
        printk(PT_ERROR,"%s:%d invalid cache addr=%lx\n\r",
               __FUNCTION__,__LINE__,(unsigned long)addr);
        arch_local_irq_restore(flags);
        return;
    }

    block = (char *)hdr;
    if(block < mem_node->start_addr ||
       block >= mem_node->start_addr +
                mem_node->total_count * mem_node->block_size ||
       (block - mem_node->start_addr) % mem_node->block_size){
        printk(PT_ERROR,"%s:%d invalid block addr=%lx\n\r",
               __FUNCTION__,__LINE__,(unsigned long)addr);
        arch_local_irq_restore(flags);
        return;
    }

    if(mem_node->free_count >= mem_node->total_count || mem_node->use_count <= 0){
        printk(PT_ERROR,"%s:%d double free addr=%lx\n\r",
               __FUNCTION__,__LINE__,(unsigned long)addr);
        arch_local_irq_restore(flags);
        return;
    }
    if(cache_slab_contains_free_block(mem_node, block)){
        printk(PT_ERROR,"%s:%d double free block addr=%lx\n\r",
               __FUNCTION__,__LINE__,(unsigned long)addr);
        arch_local_irq_restore(flags);
        return;
    }

    hdr->magic = 0;
    hdr->mem_node = NULL;
    *(void **)block = mem_node->free_head;
    mem_node->free_head = block;
    mem_node->free_count++;
    mem_node->use_count--;

    if(mem_node->use_count == 0){
        cache_list_move(&mem_node->list, &mem_node->cache_block->free_list);
    }else if(mem_node->free_count == 1){
        cache_list_move(&mem_node->list, &mem_node->cache_block->partial_list);
    }
    arch_local_irq_restore(flags);
    
}


void *xos_kmalloc(int size)
{
    return mem_cache_alloc(size);
}

void xos_kfree(void *addr)
{
    mem_cache_free(addr);
}
void xos_cache_init()
{
//    cache_val.cache_obj_buf[0].size = 32;
}
