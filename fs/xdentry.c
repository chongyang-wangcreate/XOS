/********************************************************

     Development Started: 2024
     Copyright (C) 2024-2025 wangchongyang
     Author: Wang Chongyang
     Email: rockywang599@gmail.com

    This program is licensed under the GPL v2 License. See LICENSE for more details.
********************************************************/

#include "types.h"
#include "list.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_cache.h"
#include "fs.h"
#include "xdentry.h"

/*
    文件系统的缓存部分做得比较简陋，主要是因为时间紧迫、任务繁重。为了确保 XOS 能顺利完成，
    并避免真的成为X，因此在开发过程中做了很多妥协。毕竟从零开始开发是一个巨大的挑战，
    因此各位观众看到 XOS 中的一些不足之处，也是开发过程中的必然结果，错误之处也欢迎指正，系统存在的问题也会不断处理
    持续的迭代完善系统。

    The caching part of the file system is relatively rudimentary, 
    mainly due to time constraints and heavy workload. To ensure the smooth completion of XOS, 
    And to avoid truly becoming X, many compromises were made during the development process.
    After all, developing from scratch is a huge challenge, 
    Therefore, the shortcomings that viewers have seen in XOS are also an inevitable result of the development process, 
    and will continue to be iterated and improved in the future.

*/
#define MAX_DENTRY 256
enum{
    FALSE,
    TRUE

};

xdentry *dentry_cache_pool = NULL;

cache_dentry_desc  dentry_pool;



xdentry dentry_cache_pool_buf[MAX_DENTRY] = {0};

static inline int get_cache_xdentry_free_cnt()
{
    return dentry_pool.dentry_pool_free_cnt;
}

static inline int get_cache_xdentry_using_cnt()
{
    return dentry_pool.dentry_pool_using_cnt;
}




void  init_dentry_cache(void)
{
    int i;
    dentry_pool.dentry_cache_man = xos_kmalloc(sizeof(cache_pool_man_t)*MAX_DENTRY);
    dentry_pool.dentry_pool_max_cnt = MAX_DENTRY;
    dentry_pool.dentry_pool_free_cnt = MAX_DENTRY;
    dentry_pool.dentry_pool_using_cnt = 0;
    for(i = 0;i < MAX_DENTRY; i++)
    {
        xos_spinlock_init(&(dentry_pool.dentry_cache_man[i].dentry_cache.lock));
        memset(&dentry_pool.dentry_cache_man[i],0,sizeof(cache_pool_man_t));

    }
    list_init(&dentry_pool.dentry_free_list);
    list_init(&dentry_pool.dentry_using_list);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
}



xdentry *alloc_cache_xdentry()
{
    int i;
    xdentry  *cache_xdentry = NULL;
    cache_pool_man_t *cache_man;
    dlist_t *head_node = NULL;
    xos_spinlock(&dentry_lock);
    if(get_cache_xdentry_free_cnt() > 0){
        for(i = 0; i < MAX_DENTRY; i++){
        if(dentry_pool.dentry_cache_man[i].alloc_flags == 0){
                dentry_pool.dentry_cache_man[i].in_cache_pool = 1;
                dentry_pool.dentry_cache_man[i].alloc_flags = 1;
                dentry_pool.dentry_pool_free_cnt--;
                dentry_pool.dentry_pool_using_cnt++;
                xos_unspinlock(&dentry_lock);
                printk(PT_DEBUG,"%s:%d,MAGIC:lock alloc_index =%d\n\r",__FUNCTION__,__LINE__,i);
                return &dentry_pool.dentry_cache_man[i].dentry_cache;
            }
        }
    }
    xos_spinlock(&dentry_lock);
    if(list_is_empty(&dentry_pool.dentry_free_list)){
        cache_man = xos_kmalloc(sizeof(cache_pool_man_t));
        if(!cache_man){
            return NULL;
        }
        cache_man->in_cache_pool = 0;
        list_add_back(&cache_xdentry->dcache_list, &dentry_pool.dentry_using_list);
    }else{
        head_node = &dentry_pool.dentry_free_list;

        cache_xdentry = list_entry(head_node->next,xdentry,dcache_list);

    }
    xos_unspinlock(&dentry_lock);
    return cache_xdentry;
}


xdentry *alloc_cache_xdentry_unlock()
{
    int i;
    xdentry  *cache_xdentry = NULL;
    cache_pool_man_t *cache_man;
    dlist_t *head_node = NULL;
    
    if(get_cache_xdentry_free_cnt() > 0){
        for(i = 0; i < MAX_DENTRY; i++){
        if(dentry_pool.dentry_cache_man[i].alloc_flags == 0){
                dentry_pool.dentry_cache_man[i].in_cache_pool = 1;
                dentry_pool.dentry_cache_man[i].alloc_flags = 1;
                dentry_pool.dentry_pool_free_cnt--;
                dentry_pool.dentry_pool_using_cnt++;
                
                printk(PT_RUN,"%s:%d,MAGIC:UNLOCK alloc_index =%d\n\r",__FUNCTION__,__LINE__,i);
                return &dentry_pool.dentry_cache_man[i].dentry_cache;
            }
        }
    }
    if(list_is_empty(&dentry_pool.dentry_free_list)){
        cache_man = xos_kmalloc(sizeof(cache_pool_man_t));
        if(!cache_man){
            return NULL;
        }
        cache_man->in_cache_pool = 0;
        list_add_back(&cache_xdentry->dcache_list, &dentry_pool.dentry_using_list);
    }else{
        head_node = &dentry_pool.dentry_free_list;

        cache_xdentry = list_entry(head_node->next,xdentry,dcache_list);

    }
    return cache_xdentry;
}



void  free_dentry_cache(xdentry *dentry_node)
{
    cache_pool_man_t *cache_man = (cache_pool_man_t *)dentry_node;
    xos_spinlock(&dentry_lock);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    if(cache_man->in_cache_pool == 1){
        
        cache_man->alloc_flags = 0;
        dentry_pool.dentry_pool_free_cnt++;
        dentry_pool.dentry_pool_using_cnt--;
    }else{
        
        list_del(&dentry_node->dcache_list);
        list_add_back(&dentry_node->dcache_list, &dentry_pool.dentry_free_list);
    }

    xos_unspinlock(&dentry_lock);

}

/*
    strlen((const char*)dentry_node->file_name.path_name) == name->len
    name->path_name 字符串未做截断操作，so

*/

xdentry *find_in_pdentry(xdentry *parent, path_name_t *name)
{
    dlist_t *list_node = NULL;
    xdentry *dentry_node = NULL;
    xos_spinlock(&dentry_lock);
    dlist_t  *ch_head = &parent->children_list_head;
    list_for_each(list_node, ch_head){
        dentry_node = list_entry(list_node, xdentry, ch_at_parent_list);
        printk(PT_RUN,"%s:%d,dentry_node->path_name=%s,name=%s,name_len=%d,\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name,(const char*)name->path_name,name->len);
        if(strncmp((const char*)dentry_node->file_name.path_name,(const char*)name->path_name,name->len) == 0 &&(dentry_node->parent_dentry == parent)&&
            (strlen((const char*)dentry_node->file_name.path_name) == name->len))
        {
            xos_unspinlock(&dentry_lock);
            printk(PT_RUN,"%s:%d,dentry_node->path_name=%s,\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
            return dentry_node;
        }
       
    }
    xos_unspinlock(&dentry_lock);
    return NULL;

}


xdentry *find_in_pdentry_unlock(xdentry *parent, path_name_t *name)
{
    dlist_t *list_node = NULL;
    xdentry *dentry_node = NULL;
    dlist_t  *ch_head = &parent->children_list_head;
    list_for_each(list_node, ch_head){
        dentry_node = list_entry(list_node, xdentry, ch_at_parent_list);
        if(strncmp((const char*)dentry_node->file_name.path_name,(const char*)name->path_name,name->len) == 0 &&(dentry_node->parent_dentry == parent)&&
            (strlen((const char*)dentry_node->file_name.path_name) == name->len))
        {
            printk(PT_RUN,"%s:%d,dentry_node->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
            return dentry_node;
        }
        
    }
    return NULL;

}

/*
    2024.11.08
    这个函数开发的不完善，后续再完善
    1.没有考虑失败的情况, 如果cache 资源耗尽从cache mem 中申请新空间
    2. 没有加互斥操作

    ref_count 这个需要重点关注一下

*/

xdentry *xdentry_cache_alloc_init(xdentry *parent,const path_name_t *name)
{
    xdentry *dentry_cache = NULL;

    dentry_cache = alloc_cache_xdentry_unlock();
    memset(dentry_cache,0,sizeof(xdentry));   
    /*
        very important
        2025010, path_name 函数指针指向的是局部指针字符串，所以有问题
        dentry_cache->file_name.path_name = name->path_name;
    */
    dentry_cache->file_name.len = name->len;
    dentry_cache->file_name.path_name = name->path_name;

    dget(dentry_cache);
    
    list_init(&dentry_cache->children_list_head);
    list_init(&dentry_cache->ch_at_parent_list);
    
    if (parent) {
        dentry_cache->parent_dentry = parent;
        
        printk(PT_RUN,"%s:%d,parent.name=%s\n\r",__FUNCTION__,__LINE__,parent->file_name.path_name);
        parent->ref_count++;
        xos_spinlock(&dentry_cache->lock);
        list_add_back(&dentry_cache->ch_at_parent_list, &parent->children_list_head);
        xos_unspinlock(&dentry_cache->lock);
    } else{
        dentry_cache->parent_dentry = dentry_cache;
        
        printk(PT_RUN,"%s:%d,dentry_cache.name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    }
	return dentry_cache;
}



xdentry *xdentry_cache_alloc_init_old(xdentry *parent,const path_name_t *name)
{
    int i;
    xdentry *dentry_cache = NULL;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    
    for(i = 0;i < MAX_DENTRY;i++)
    {
        if(dentry_pool.dentry_cache_pool[i].ref_count == 0)
        {
            break;
        }
    }
    if(i == MAX_DENTRY){
        
        dentry_cache = xos_kmalloc(sizeof(xdentry));
        if(!dentry_cache){
            return NULL;
        }
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    }else{
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        dentry_cache = &dentry_pool.dentry_cache_pool[i];
        memset(dentry_cache,0,sizeof(xdentry));
    }

    memset(dentry_cache,0,sizeof(xdentry));
    /*
        very important
        2025010, path_name 函数指针指向的是局部指针字符串，所以有问题
        dentry_cache->file_name.path_name = name->path_name;
    */
    dentry_cache->file_name.len = name->len;
    dentry_cache->file_name.path_name = name->path_name;

    dget(dentry_cache);
    
    list_init(&dentry_cache->children_list_head);
    list_init(&dentry_cache->ch_at_parent_list);
    
    printk(PT_DEBUG,"%s:%d,dentry_cache.name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    if (parent) {
        printk(PT_DEBUG,"%s:%d,%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
        dentry_cache->parent_dentry = parent;
        parent->ref_count++;
        xos_spinlock(&dentry_cache->lock);
        list_add_back(&dentry_cache->ch_at_parent_list, &parent->children_list_head);
        printk(PT_DEBUG,"%s:%d,%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
        xos_unspinlock(&dentry_cache->lock);
    } else{
        dentry_cache->parent_dentry = dentry_cache;
        
        printk(PT_DEBUG,"%s:%d,dentry_cache.name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    }
	return dentry_cache;
}

