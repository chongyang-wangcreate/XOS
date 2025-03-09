#ifndef __XOS_SLAB_H__
#define __XOS_SLAB_H__

#define NODE_NR  1;
#define CPU_NR  1
struct kmem_cache_node{
    unsigned long nr_partial;
    struct list_head partial;
    xos_spinlock_t cache_node_lock;

};

struct kmem_cache_cpu {
    void **free_list;
    struct page_desc *page;

};


typedef struct slab_desc{

    unsigned int size;  
    unsigned int object_size;
    unsigned int offset;    /*free pointer*/
    struct kmem_cache_node  *mem_node[NODE_NR];
    struct kmem_cache_cpu *cache_cpu[1];

}slab_desc_t;

#endif
