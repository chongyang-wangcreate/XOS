#ifndef __XOS_CACHE_H__
#define __XOS_CACHE_H__

/*
    对于每个cache 对象的描述
*/
typedef struct cache_obj{
    dlist_t free_list;
    dlist_t use_list;
    int size;

}cache_obj_t;

/*
    对于整个cache 分配器的描述
20240518 PM:22:12
    包含了cache_obj_t 包含了cache 对象
    当前cache_obj 对象大小( 内存块大小)，有cach-32  ,cache-64,cache-128,所以需要指明（描述）当前cache-obj的cache-size 大小
    cache 被引用次数统计
    多cache 互联链表节点
20240518 PM:23:16:
    cache 作用就是内存池的作用，cahe-32 cache 管理的都是32 字节的内存块
    这些内存块分别挂到3个链表中
    free_list 链节点上挂载的cache-32 块没有人使用
    partial_list 链节点上挂载的cache-32 块都是部分被使用了
    full_list 链节点上挂载的cache-32 块都被使用了
20240519 AM:7:46
    如何给4K page 切片呢，
    4K page 空间如果没有头的话。平均切成4k/32 = N块
    分析分析折就成了数组首地址+偏移 = 具体的内存块首地址
    需要有头，
    typedef struct cache_block_struct
    {
        char *block_addr; 数组首地址
        int block_len;  数组长度
        int block_nfree;空闲块数量
        bitmap;         是否需要加入位图表
    }cache_block_t;


20240525 AM:14:49
	想使用固定block 内存管理方式 首地址+index*block  获取想要的地址
	使用Bitmap 来标志当前块释放被占用，通过bitmap 方式找到空闲的内存块
	也通过Bitmap 释放内存块
	
	所以每个xxx_list 链表需要挂载关联另外一个结构，起名脚mem_obj
	这里是真正的内存块，当前mem_obj_t 管理的内存块不要过大，block 太多需要使用bitmap 表示的空间就更大
	分配内存空间是查找空闲内存块需要的变量次数更多,变量空闲块也就更慢
	
	感觉自己太喜欢用Bitmap了，某些情况下确实好用
	
	不用bitmap 可以吗，不太好，bitmap 表示占用空间并不是很大1024 块128 字节就够了
	typdef struct mem_obj_bitmap{
		int bit_count;    // 大小如何计算
		char *bit_start;  //从哪里开辟空间，
	}mem_bitmap_t;
	typedef struct mem_obj{
		struct list_head list; //链表节点与free_list  partial_list 或者full_list 相关联
		char *start_addr; //起始地址
		int index;// index 内存块标识
		int free_count;
		int use_count;
		mem_bitmap_t mem_map;
		
	}mem_obj_t
	
*/


/*
    cache_block_t 结构和mem_obj_t 结构通过free_list , partial_list

    full_list 三个链表相关联
    引入三个链表模仿了linux ,
    当前结构对于mem_obj_t 来说是个总的管理块(或者管理头，需要描述mem_obj_t 重要信息)
    mem_obj_t 的初始化数据来自mem_block_t 头数据

    为何这么定义呢
    我定义的mem_obj_t 挂在那个链表上
    我定义的mem_obj_t 管理的内存块每块多大首先肯定是mem_block_t 要指定所以必须定义obj_block_size 来表示
    我定义的mem_obj_t 管理的内存块到底有多少块mem_block_t 结构中肯定要描述

    mem_block_t 是来管理mem_obj_t 

    引入链表多重作用，一个链表可以挂多个节点，每个节点都是一个内存管理模块
    这样我可以将一个大的内存块拆分成多个节点，每个节点内存数量减少
    遍历整个内存块区域会很快
*/
typedef struct cache_block_st{
	struct list_head free_list;
	struct list_head partial_list;
	struct list_head full_list;
	int obj_block_size;  //描述管理的内存块大小
	int obj_block_count;
    xos_spinlock_t lock;
	
}cache_block_t;    //bisect_mem_t

typedef struct mem_pool{
    int cache_bsize; 
    cache_block_t cache_block;
}xos_cache_t;

/*
    真正操作内存块的结构,内存等分对象描述符
    类似磁盘块的管理，本结构管理的内存块不易过多，管理过多的内存块不管bitmap 的空间占比
    还是轮询时间占比都会极大提升，效率下降。
*/
typedef struct mem_obj{
    struct list_head list; //链表节点与free_list  partial_list 或者full_list 相关联
    char *start_addr; //起始地址
    int index;// index 内存块标识
    int free_count;
    int use_count;
    int block_size;
    cache_block_t *cache_block; //确定自己属于那个caceh_block;
    bitmap_t mem_map;
    
}mem_obj_t;

extern void *xos_kmalloc(int size);
extern void mem_cache_init();
extern void xos_kfree(void *addr);


#endif
