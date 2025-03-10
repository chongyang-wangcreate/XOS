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
/*
    当前mem_cache 基本问题是，cache 无可用空间时，只能分配一个页进行拆分
    我的mem_node 获取是基于page 页首地址，这样释放addr 4K 对齐时就可以
    找到对应mem_node 结构体

*/
static void* __mem_cache_alloc(cache_block_t *cache_block)
{
    mem_obj_t  *mem_node;
    dlist_t *list_node;
    char *p_start;
    int tmp_alloc_size;
    int bitmap_take_size;
    int block_index = 0;
    void *alloc_addr;
    arch_local_irq_disable();

    if(list_is_empty(&cache_block->free_list)){
        if(list_is_empty(&cache_block->partial_list)){
            /*
                本cache 真没有可用空间
                需要再向buddy 内存分配器申请，申请固定4K 页
            */
            p_start = (char*)xos_get_free_page(0, 0);
            tmp_alloc_size = (1 << PAGE_SHIFT);
            /*
                初始化mem_node
            */
            mem_node = (mem_obj_t*)p_start;
            printk(PT_DEBUG,"%s:%d,tmp_alloc_size=%d\n\r",__FUNCTION__,__LINE__,tmp_alloc_size);
            mem_node->use_count = 0;
            mem_node->block_size = cache_block->obj_block_size;
            /*
                ((tmp_alloc_size - sizeof(mem_obj_t) -x_size)/cache_block->obj_block_size/8 = x_size
                使用上述公式就能解出x_size 大小
                 ((tmp_alloc_size - sizeof(mem_obj_t)) = x_size*8*cache_block->obj_block_size+x_size
                 x_size = ((tmp_alloc_size - sizeof(mem_obj_t))/(8*cache_block->obj_block_size+1)
            */ 
            bitmap_take_size = ((tmp_alloc_size - sizeof(mem_obj_t))/(8*cache_block->obj_block_size+1));
            
            mem_node->start_addr = (char*)ALIGN_UP(((uint64_t)(p_start + (sizeof(mem_obj_t)) +bitmap_take_size)),8);
            mem_node->mem_map.btmp_bytes_len = (tmp_alloc_size - sizeof(mem_obj_t) -bitmap_take_size)/8;// 表示(1024位)
            mem_node->mem_map.bit_start = (uint8*)(mem_node->start_addr + (sizeof(mem_obj_t)));
            mem_node->free_count = bitmap_take_size*8;
            printk(PT_DEBUG,"%s:%d,mem_node->mem_map.btmp_bytes_len=%d\n\r",__FUNCTION__,__LINE__,mem_node->mem_map.btmp_bytes_len);
            printk(PT_DEBUG,"%s:%d,bitmap_take_size=%d,cache_block->obj_block_size=%d\n\r",__FUNCTION__,__LINE__,bitmap_take_size,cache_block->obj_block_size);
            bitmap_init(&mem_node->mem_map);
            /*
                切一块空间，然后加入到partial_list 链表，加入链表之后
                再修改bitmap
            */
            block_index = find_free_bit((bitmap_t *)&mem_node->mem_map);
            
            alloc_addr = mem_node->start_addr+ block_index *cache_block->obj_block_size;
            set_bit((uint8_t*)mem_node->mem_map.bit_start, block_index);
            /*
                将当前mem_node 加入到当前cache_block->partial_list 中
            */
            list_add_back(&mem_node->list, &cache_block->partial_list);
            mem_node->use_count++;
            mem_node->free_count--;
        }else{
            /*
                从partial_list 链表中获取节点,分配完毕之后判断本节点管理的内存块是否分配完毕
                如果分配完毕则先脱链，然后加入到full_list 链表
            */
            printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
            list_node = cache_block->partial_list.next;
            mem_node = list_entry(list_node, mem_obj_t, list);
            block_index = find_free_bit(&mem_node->mem_map);
            alloc_addr = mem_node->start_addr+ block_index *cache_block->obj_block_size;
            set_bit((uint8_t*)mem_node->mem_map.bit_start, block_index);
            /*
                将当前mem_node 加入到当前cache_block->partial_list 中
            */
            mem_node->use_count++;
            mem_node->free_count--;
            if(mem_node->free_count == 0){

                list_add_back(&mem_node->list,&cache_block->full_list);
            }
        }
    }else{

        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        list_node =  cache_block->free_list.next;
        mem_node = list_entry(list_node,mem_obj_t,list);
        block_index = find_free_bit((bitmap_t *)&mem_node->mem_map);
        alloc_addr = mem_node->start_addr+ block_index *cache_block->obj_block_size;
        set_bit((uint8_t*)mem_node->mem_map.bit_start, block_index);
        list_add_back(&mem_node->list ,&cache_block->partial_list);
        mem_node->use_count++;
        mem_node->free_count--;

    }
    arch_local_irq_save();
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
    int index = 0;
    index = get_base2_index(size >> DEFAULT_ORDER);
    return index+1;
}

void *mem_cache_alloc(int size)
{
    void *mem_ptr;
    cache_block_t *local_cache_block;

    if(size > mem_size_set[6].cache_bsize){

        mem_ptr = xos_get_free_page(0, 0);
        return mem_ptr;
    }

       
    local_cache_block = GET_MEM_BLOCK(get_size_index(size));

    local_cache_block->obj_block_size = GET_BLOCK_SIZE(get_size_index(size));
    printk(PT_RUN,"%s:%d,size=%d,local_cache_block->obj_block_size=%d\n\r",__FUNCTION__,__LINE__,size,local_cache_block->obj_block_size);


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
    int bindex;
    unsigned long align_addr = ALIGN_DOWN(addr, PAGE_SIZE);
    /*
        每个也都是4K 对齐
        获取mem_node 结构的地址
    */
    mem_node = (mem_obj_t*)align_addr;
    bindex = ((char*)addr - mem_node->start_addr)/mem_node->block_size;
    /*
        设置Bitmap,清除对应的bit 位
    */
    clear_bit((uint8_t*)mem_node->mem_map.bit_start, bindex);
    mem_node->free_count++;
    if(mem_node->use_count != 0)
    mem_node->use_count--;
    
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


