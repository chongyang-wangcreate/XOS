#ifndef __XDENTRY_H__
#define __XDENTRY_H__



typedef struct cache_pool_man{
    
    xdentry  dentry_cache; //一次性分配
    char     in_cache_pool;
    char     alloc_flags;

}cache_pool_man_t;

typedef struct cache_mang_dentry_desc{
    cache_pool_man_t *dentry_cache_man;
    xdentry *dentry_cache_pool; //一次性分配
    dlist_t dentry_free_list;  //单次分配
    dlist_t dentry_using_list;
    int   dentry_pool_max_cnt;
    int   dentry_pool_using_cnt;
    int   dentry_pool_free_cnt;

}cache_dentry_desc;



extern void  init_dentry_cache(void);
extern void free_dentry_cache(xdentry *dentry_node);

static inline xdentry *dget(xdentry *dentry)
{
    if (dentry){
        
        xos_spinlock(&dentry->lock);
        dentry->ref_count++; //notice
        xos_unspinlock(&dentry->lock);
        
    }

    return dentry;
}

/*
    1. 判断dentry 引用计数如果计数0 
    2. 减少引用计数，等于0
    3. 脱链dentry cache ，以及inode 更改状态
    4. 销毁申请的相应结构
    else
    引用计数大于0
    直接返回


    处理存在问题
    
*/
static inline xdentry *dput(xdentry *dentry)
{
    if (dentry){
        
        xos_spinlock(&dentry->lock);
        if(dentry->ref_count > 1){
        
           dentry->ref_count--; 
           xos_unspinlock(&dentry->lock);
           return dentry;
        }else if(dentry->ref_count == 1){

            dentry->ref_count = 0;
            dentry->parent_dentry = NULL;
            list_del(&dentry->children_list_head);
            list_del(&dentry->ch_at_parent_list);

            /*
                确定dentry 来源
                1. 来自dentry_cache
                2. 来自xos_kmalloc
            */
            /*
            if(is_alloc_dentry_cache(dentry)){
                free_dentry_cache(dentry);
            }else{
                xos_kfree((void*)dentry);
            }
            */
            free_dentry_cache(dentry);
        }
        xos_unspinlock(&dentry->lock);
        
    }

    return dentry;
}


#endif
