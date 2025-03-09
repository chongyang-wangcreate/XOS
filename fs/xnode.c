/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2030 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "error.h"
#include "list.h"
#include "string.h"
#include "printk.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "fs.h"
#include "mount.h"
#include "tick_timer.h"
#include "task.h"
#include "path.h"
#include "mount.h"
#include "xnode.h"
/*
    贴了两篇不错的文章
    https://blog.csdn.net/weixin_47763623/article/details/144204069
    https://blog.csdn.net/bullbat/article/details/7242566
*/
#define  CACHE_NODE_NR  256
xinode  *inode_cache_pool = NULL;

cache_node_desc node_pool;

/*
    Known but unresolved issues
    1. xnode->refcount
    2. dentry->refcount
*/

xinode *alloc_xinode()
{
    xinode *node;
    node = xos_kmalloc(sizeof(xinode));
    return node;
}


static inline int get_cache_node_free_cnt()
{
    return node_pool.cache_pool_free_cnt;
}

static inline int get_cache_node_using_cnt()
{
    return node_pool.cache_pool_using_cnt;
}

xinode *alloc_cache_xinode()
{
    int i;
    xinode  *xcache_node;
    dlist_t *head_node;
    
    xos_spinlock(&g_file_lock);
    if(get_cache_node_free_cnt() > 0){
        for(i = 0; i < CACHE_NODE_NR; i++){
        if(node_pool.icache_pool[i].alloc_flags == 0){
                node_pool.icache_pool[i].alloc_flags = 1;
                node_pool.icache_pool[i].in_cache_pool = 1;
                node_pool.icache_pool[i].inode_cache.node_num ++;
                node_pool.cache_pool_using_cnt++;
                node_pool.cache_pool_free_cnt--;
                xos_unspinlock(&g_file_lock);
                return &node_pool.icache_pool[i].inode_cache;
            }
        }
    }
    xos_unspinlock(&g_file_lock);
    if(list_is_empty(&node_pool.cache_free_list)){
        xcache_node = xos_kmalloc(sizeof(xinode));
        if(!xcache_node){
            return NULL;
        }
        xcache_node->in_cache_pool = 0;
        list_add_back(&xcache_node->cache_list, &node_pool.cache_using_list);
    }else{
        head_node = &node_pool.cache_free_list;
        xcache_node = list_entry(head_node->next,xinode,cache_list);
    }

    return xcache_node;
}



void free_cache_xinode(xinode  * new_node){

    node_man_t *inode_man = (node_man_t*)new_node;
    if(inode_man->in_cache_pool == 1){
        inode_man->alloc_flags = 0;
        node_pool.cache_pool_free_cnt++;
        node_pool.cache_pool_using_cnt--;
    }else{
        list_del(&new_node->cache_list);
        list_add_back(&new_node->cache_list, &node_pool.cache_free_list);
    }
}


void add_xinode_ref(xinode *cur_node)
{
    xos_spinlock(&cur_node->lock);
    cur_node->ref_count++;
    xos_unspinlock(&cur_node->lock);
}

void dec_xinode_ref(xinode *cur_node)
{
    xos_spinlock(&cur_node->lock);
    if(cur_node->ref_count == 0){
        /*
            回收
        */
       cur_node->alloc_flags = 0;
       xos_unspinlock(&cur_node->lock);
       return;
    }
    cur_node->ref_count--;
    xos_unspinlock(&cur_node->lock);
}

int init_node_cache_space(struct address_space  *node_space,xinode *x_node)
{
    node_space->node_owner = x_node;
    node_space->alloc_flags = 0; /*occupied sign*/
    node_space->nr_pages = CACHE_NODE_NR;  /*tmp set*/
    list_init(&node_space->page_list_head); /*attach page*/
    xos_spinlock_init(&node_space->pg_lock);
    
    return 0;
}
void  init_inode_cache_old(void)
{
    int i;
    inode_cache_pool = xos_kmalloc(sizeof(xinode)*CACHE_NODE_NR);
    for(i = 0;i < CACHE_NODE_NR; i++)
    {
        xos_spinlock_init(&(inode_cache_pool[i].lock));
        init_node_cache_space(&inode_cache_pool[i].cache_space,&inode_cache_pool[i]);
    }
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
}


void  init_inode_cache(void)
{
    int i;
    node_pool.icache_pool = xos_kmalloc(sizeof(node_man_t)*CACHE_NODE_NR);
    node_pool.cache_pool_max_cnt = CACHE_NODE_NR;
    node_pool.cache_pool_free_cnt = CACHE_NODE_NR;
    node_pool.cache_pool_using_cnt = 0;
    for(i = 0;i < CACHE_NODE_NR; i++)
    {
        xos_spinlock_init(&(node_pool.icache_pool[i].inode_cache.lock));
        init_node_cache_space(&node_pool.icache_pool[i].inode_cache.cache_space,&node_pool.icache_pool[i].inode_cache);
    }
    list_init(&node_pool.cache_free_list);
    list_init(&node_pool.cache_using_list);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
}

void follow_mount(xmount_t **mnt,xdentry **dentry_cache)
{
    /*
        检查当前目录下是否挂载子文件系统
    */
    printk(PT_RUN,"%s:%d,mount_count=%d,dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->cur_mnt_count,(*dentry_cache)->file_name.path_name);
    xos_spinlock(&g_file_lock);
    while ((*dentry_cache)->is_mount &EXIST_MOUNT) {
        /*
            查找当前路径是否有挂载点
        */
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        
        xmount_t *child = lookup_mnt(*mnt, *dentry_cache); /*遍历节点Hash_list*/
        if (!child) /*没有挂载子文件系统*/
        {
            break;
        }
        (*mnt)->alloc_count--;/*回收mnt_cache*/
        
        *mnt = child;
        *dentry_cache = child->root_dentry;
        
        printk(PT_RUN,"%s:%d,while while *dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->file_name.path_name);
        
    }
    xos_unspinlock(&g_file_lock);
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
}






/*
    1. ../ 是否已经到达进程的根目录 ../ 是否等于 '/'
    2. 是否是所在文件系统的根，
       2.1. 不是：跳出循环
       2.2  是，切换到上级文件系统 需要更改mnt 和挂载点
    3. 找到具体目录，确认当前目录下是否挂载子文件系统
    4. 存在bug 需要处理
*/

void check_sw_parent_dentry(lookup_path_t *look_path)
{
        xmount_t **mnt = &look_path->look_mnt;
        xdentry **dentry_cache = &look_path->look_dentry;
        xmount_t *parent_mnt;
        for(;;){
        /*
             arrive cur process root break
        */
        if(*dentry_cache == current_task->fs_context.root_dentry && *mnt == current_task->fs_context.root_dentry_mount){
            break;
        }
        /*
            Not the root directory of the file system, 
            return to the parent directory
        */
        if((*mnt)->root_dentry != *dentry_cache){
            *dentry_cache = (*dentry_cache)->parent_dentry ; /*返回上一级目录*/
             break;
        }

        parent_mnt = (*mnt)->mount_parent;
        if((*mnt) == parent_mnt){
            break;
        }
        /*
            到达挂载点的根目录，(关键字挂载点)
        */
        printk(PT_DEBUG,"%s:%d,mount_count=%d,dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->cur_mnt_count,(*dentry_cache)->file_name.path_name);
        parent_mnt = (*mnt)->mount_parent;
        *dentry_cache =(*mnt)->mount_point;  /*mnt 文件系统挂载到那个目录mnt 挂载到mountpoint 之上*/
        *mnt = parent_mnt;
    }
}

#if 0
void follow_mount_new(lookup_path_t *look_path)
{
    /*
        检查当前目录下是否挂载子文件系统
    */
    
    xmount_t **mnt = &look_path->look_mnt;
    xdentry **dentry_cache = &look_path->look_dentry;
    printk(PT_RUN,"%s:%d,mount_count=%d,dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->cur_mnt_count,(*dentry_cache)->file_name.path_name);
    xos_spinlock(&g_file_lock);
    while ((*dentry_cache)->is_mount &EXIST_MOUNT) {
        /*
            查找当前路径是否有挂载点
        */
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        
        xmount_t *child = lookup_mnt(*mnt, *dentry_cache); /*遍历节点Hash_list*/
        if (!child) /*没有挂载子文件系统*/
        {
            break;
        }
        (*mnt)->alloc_count--;/*回收mnt_cache*/
        
        *mnt = child;
        *dentry_cache = child->root_dentry;
        
        printk(PT_RUN,"%s:%d,while while *dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->file_name.path_name);
        
    }
    xos_unspinlock(&g_file_lock);
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
}
#endif

/*
    需加锁防并发操作
    1. ../ 是否已经到达进程的根目录 ../ 是否等于 '/'
    2. 是否是所在文件系统的根，
       2.1. 不是：跳出循环
       2.2  是，切换到上级文件系统 需要更改mnt 和挂载点
    3. 找到具体目录，确认当前目录下是否挂载子文件系统
    4. 存在bug 需要处理
*/

void switch_to_parent_dir(lookup_path_t *look_path)
{
    xos_spinlock(&dentry_lock);
    check_sw_parent_dentry(look_path);
    xos_unspinlock(&dentry_lock);
    follow_mount(&look_path->look_mnt,&look_path->look_dentry);
}


/*
    考虑到mnt   ,dentry_cache 只作为中间变量，
    如果只能使用look结构中的变量做处理，局限性比较大，同时存储这些中间动态变量意义不大
    我调试因为这结构中的两个临时变量搞得焦头烂额
*/
void old_switch_to_parent_dir(lookup_path_t *look)
{
    xmount_t *parent_mnt;
    xmount_t **mnt = &look->look_mnt;
    xdentry **dentry_cache = &look->look_dentry;
    printk(PT_DEBUG,"333333%s:%d\n\r",__FUNCTION__,__LINE__);
//    xdentry *prev = *dentry_cache;
    while(1){
        /**
             已经到达当前进程的根目录，退出 
        **/
        if(*dentry_cache == current_task->fs_context.root_dentry && *mnt == current_task->fs_context.root_dentry_mount){
            printk(PT_DEBUG,"4444444444%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        }

        printk(PT_DEBUG,"555555555 %s,%d,(*dentry_cache)->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->file_name.path_name);
        
        printk(PT_DEBUG,"1212121212 %s,%d,(*mnt)->root_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,((*mnt)->root_dentry)->file_name.path_name);
        if(*dentry_cache != (*mnt)->root_dentry){
            *dentry_cache = (*dentry_cache)->parent_dentry ; /*返回上一级目录*/
             printk(PT_DEBUG,"666666 %s,%d,(*dentry_cache)->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->file_name.path_name);
            break;
        }

        parent_mnt = (*mnt)->mount_parent;
        if(parent_mnt == (*mnt)){
            printk(PT_DEBUG,"7777777%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        }

        printk(PT_DEBUG,"%s:%d,mount_count=%d,dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->cur_mnt_count,(*dentry_cache)->file_name.path_name);
        parent_mnt = (*mnt)->mount_parent;
        *dentry_cache =(*mnt)->mount_point;  /*mnt 文件系统挂载到那个目录mnt 挂载到mountpoint 之上*/
        *mnt = parent_mnt;
        printk(PT_DEBUG,"%s:%d,mount_count=%d,dentry_cache->path_name=%s\n\r",__FUNCTION__,__LINE__,(*dentry_cache)->cur_mnt_count,(*dentry_cache)->file_name.path_name);
    }
}


/**
 * 先在缓存中搜索，看看文件是否存在
 * 通过name 找到相应的inode 和 dentry,  如/home/wangchongyang

 *如想找到wangchongyang, 需要一级一级去找，先确定/的 mnt,和root_dentry
 * 通过获取的root_dentry 从root_dentry->children 找到当前目录下文件(目录也是文件)
 * 如home，匹配home 字段，找到之后，更改lpath->dentry 和lpath->mnt(同一个文件系统挂载mnt 不变)


  20250110: 如果我输入的路径本身就不存在，一定会找失败，那么当前创建查找失败后一定会创建
   这有时并不符合要求，所以需要flag 进行控制

     存在问题：
     if(!dentry_cache){
            dentry_cache = xdentry_cache_alloc_init(look->look_dentry, name);
         }
   20250125：更改lookup_file_in_pdentry 接口
    
    fetch_son_mnt  ,fetch_son_dentry 没有合并到looku_path_t 结构体这样展现的更清晰更易于处理,
    如果只能使用look结构中的变量做处理，局限性比较大，同时存储这些中间动态变量意义不大
    我调试因为这结构中的两个临时变量搞得焦头烂额
 */

static int lookup_file_in_pdentry(lookup_path_t *look, 
             xmount_t **fetch_son_mnt, xdentry **fetch_son_dentry)
{
    int ret = -1;
    xdentry tmp_dentry;
    xmount_t *mnt = look->look_mnt;
    printk(PT_RUN,"%s:%d,name->path_name=%s,look->look_dentry.path_name=%s\n\r",__FUNCTION__,__LINE__,look->cur_path.path_name,look->look_dentry->file_name.path_name);
    xdentry *dentry_cache = find_in_pdentry(look->look_dentry, &look->cur_path);
    if((!dentry_cache)&&(look->flags & LOOKUP_FOLLOW)){
        /*LOOKUP_FOLLOW 作用要求文件或目录必须存在*/
        printk(PT_RUN,"%s:%d,look->cur_path.path_name=%s\n\r",__FUNCTION__,__LINE__,look->cur_path.path_name);

        /*快速缓存查找如果失败，则通过lookup查找*/
        if(look->look_dentry->file_node->inode_ops->lookup){
            dentry_cache = look->look_dentry->file_node->inode_ops->lookup(look->look_dentry->file_node, &tmp_dentry, look); 
            if(!dentry_cache){
                ret = -ENOMEM;
            }
        }
        ret = - ENOENT;
        return ret;  /*文件不存在返回错误码*/
    }
    /*缓存中找不到创建，判断条件待完善*/
    else if((!dentry_cache)){
        dentry_cache = xdentry_cache_alloc_init(look->look_dentry, &look->cur_path);
        printk(PT_RUN,"%s:%d,dentry_cache=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)dentry_cache);

        if (!dentry_cache){
            ret = -ENOMEM;
            goto dentry_alloc_fail;
        }
    }
    /*
           如 /home      home 对应的mnt 和dentry_cache
    */
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    *fetch_son_mnt = mnt;
    *fetch_son_dentry = dentry_cache;
    return 0;
dentry_alloc_fail:
        
    printk(PT_DEBUG,"%s:%d,dentry_cache=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)dentry_cache);
    return ret;
}




int no_parse_store_final_component(lookup_path_t *look_path)
{
    /**
     * 用last_path 存储最后一个路径分量
     */
    memcpy(&look_path->last_path,&look_path->cur_path,sizeof(look_path->cur_path));
    /**
     * 默认路径分量类型PATH_NORMAL
     */
    printk(PT_RUN,"%s:%d,look_path->last_path=%s\n\r",__FUNCTION__,__LINE__,look_path->last_path.path_name);

    /*
        路径分量收字符不是.，当然也可以多判断
        if(look_path->last_path.path_name[0] ！= '.'|| look_path->last_path.path_name[1] ！= '.')
    */
    if (look_path->last_path.path_name[0] != '.'){
        look_path->path_type = PATH_NORMAL;
        return 0;
    }

    if ((look_path->last_path.len == 1)&&(look_path->last_path.path_name[0] == '.'))
    {
        look_path->path_type = PATH_DOT;
    }
    else if (look_path->last_path.len == 2 && look_path->last_path.path_name[0] == '.'&&
     look_path->last_path.path_name[1] == '.')
    {
        
        look_path->path_type = PATH_DOTDOT;
    }

    return 0;

}

int parse_final_component(lookup_path_t *look_path,int flags){

    int ret;
    xinode *file_node;
    xmount_t *son_file_mnt;
    xdentry  *son_file_dentry_node;
    
    printk(PT_RUN,"%s:%d,look_path->flags=%x\n\r",__FUNCTION__,__LINE__,look_path->flags);
    if ((look_path->cur_path.path_name[0] == '.') && (look_path->cur_path.len == 1)){
        return 0; /*解析完毕退出*/
    }else if(look_path->cur_path.len == 2){
        if((look_path->cur_path.path_name[0] == '.')&&(look_path->cur_path.path_name[1] == '.')){
                switch_to_parent_dir(look_path); /*切换目录后退出*/
                file_node = look_path->look_dentry->file_node;
                return 0;  /*解析完毕退出*/
        }
    }

    ret = lookup_file_in_pdentry(look_path, &son_file_mnt, &son_file_dentry_node);
    if(ret < 0){
         ret = -ENOENT;
         goto look_failed;
    }
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);

    follow_mount(&son_file_mnt, &son_file_dentry_node);
    look_path->look_dentry = son_file_dentry_node;
    look_path->look_mnt = son_file_mnt;
    
    /*
        不要删除
        follow_mount_new(look_path);
        file_node = look_path->look_dentry->file_node;
    */
    /*
        调试不同目录切换时，不同文件系统切换出现问题
        look_path->mnt = son_file_mnt;  没有设置look_path->mnt ，造成文件系统混乱，文件系统的根混乱
    */
   
    printk(PT_RUN,"%s:%d,son_file_dentry_node.path=%s\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);
     /**
     * 检查文件是否真的存在,不存在退出
     */
    file_node = look_path->look_dentry->file_node;
    if (!file_node){
        ret = -ENOENT;
        goto look_failed;
    }
    /**
     if (S_ISDIR(inode->i_mode)) {
         add_flags = DCACHE_DIRECTORY_TYPE;
         if (unlikely(!(inode->i_opflags & IOP_LOOKUP))) {
             if (unlikely(!inode->i_op->lookup))
                 add_flags = DCACHE_AUTODIR_TYPE;
             else
                 inode->i_opflags |= IOP_LOOKUP;
         }
         goto type_determined;
     }

     **/
    if (look_path->flags & LOOKUP_DIRECTORY) {
        ret = -ENOTDIR; 
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        /**
         * 没有lookup说明这不是一个目录，返回ENOTDIR错误。
         */
        if (!file_node->inode_ops || !file_node->inode_ops->lookup)
            goto look_failed;
    }
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return 0;

look_failed:
        /*
            to do free resource
        */
        printk(PT_DEBUG,"%s:%d,ret=%d\n\r",__FUNCTION__,__LINE__,ret);
        return ret;

}



int last_component_handle( lookup_path_t *look_path)
{
    int ret;
    int flags = 0;
    /*
        if (lookup_flags & LOOKUP_PARENT)
        only record the last path  example   "/home/wcy"

        only store  wcy  name and len  in last path

        Only stores name information, does not parse current directory information
    */
    printk(PT_RUN,"%s:%d,look_path->flags=%x\n\r",__FUNCTION__,__LINE__,look_path->flags);
    
    if (look_path->flags & LOOKUP_PARENT){
        ret = no_parse_store_final_component(look_path);
        return ret;
    }
    ret = parse_final_component(look_path,flags);
    return ret;
    
}


int loop_split_path_component(char *path_name, lookup_path_t *look_path,int *error_code)
{
    int ret = 0;
    xinode *file_node;
    char ch_var;
    xmount_t *son_file_mnt;
    xdentry  *son_file_dentry_node;
    while (*path_name == '/'){
     path_name++;
    }
    if (!*path_name){
        look_path->path_type = PATH_ROOT;  /*表明当前是根目录*/
        return 0;
    }
    file_node = look_path->look_dentry->file_node;
    while(1){
        ch_var = *path_name; /*有效字符非'/'和'\0',记录非常重要*/
        look_path->cur_path.path_name = (unsigned char*)path_name;
        /*
            ../home
        */
        do {
            path_name++;
            ch_var = *path_name;
            printk(PT_RUN,"%s:%d,ch_var=%c\n\r",__FUNCTION__,__LINE__,ch_var);

        } while(ch_var&&ch_var != '/'); /*loop until the condition not met*/
        look_path->cur_path.len = (unsigned char*)path_name - look_path->cur_path.path_name;
        printk(PT_RUN,"%s:%d,ch_var=%c\n\r",__FUNCTION__,__LINE__,ch_var);
        /*
            /home///test   test  may be a file or may be a dir
        */
        if(!ch_var){
            ret = RET_NORMAL_COMPONET;
            *error_code = 0;
            printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
            return ret;
        }
        do{
            path_name++;
        } while (*path_name == '/'); /*如果后续存在/ 分隔符继续跳过,mont_path 指向位置"home"中的h*/
        if (!*path_name)  /*the end*/
        {
            ret = RET_LASTER_SLASH;
            *error_code = 0;
            return ret;
        }
         if ((look_path->cur_path.path_name[0] == '.') && (look_path->cur_path.len == 1)){
            continue; 
        }else if(look_path->cur_path.len == 2){
            if((look_path->cur_path.path_name[0] == '.')&&(look_path->cur_path.path_name[1] == '.')){
                    switch_to_parent_dir(look_path);
                    file_node = look_path->look_dentry->file_node;
                    continue;  
            }
        }

       printk(PT_RUN,"%s:%d,cur_path.path_name=%s\n\r",__FUNCTION__,__LINE__,look_path->cur_path.path_name);
        
        /**
        * 在当前目录中搜索文件获取文件对应的mnt 和dentry 。
        * 并没有搜索完所以要继续搜索
        * 在这之前一直在处理 /dev  搜索根下的dev, loook_path.cur_path 父描述信息
        *  loook_path.look_dentry  
       */
        ret = lookup_file_in_pdentry(look_path,  &son_file_mnt, &son_file_dentry_node);
        if(ret == - ENOENT){
            
            *error_code = -ENOENT;
            ret = *error_code;
            return ret;
        }else if(ret == -ENOMEM){
            
            *error_code = -ENOMEM;
            ret = *error_code;
            return ret;
        }
         /**
            查看当前目录是否安装多个文件系统，如果是则取最后一个文件系统
            如果是挂载目录.则跳转到挂载文件系统的根目录
         **/
        
        follow_mount(&son_file_mnt, &son_file_dentry_node);
        printk(PT_RUN,"%s:%d,son_file_dentry_node->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,son_file_dentry_node->file_name.path_name);
        file_node = son_file_dentry_node->file_node;
        /**
        *  还需要判断目录 dentry 是否和具体的inode 关联
        */
        if (!file_node){
            *error_code = -ENOENT;
            ret = *error_code;
            printk(PT_ERROR,"no file file no file%s:%d\n\r",__FUNCTION__,__LINE__);
            return ret;
        }
        if(file_node->inode_ops->follow_link){
            
               
         }else {
                
                /*
                    开始描述"/"对应的mnt 和dentry
                    然后切换到下一个目录分量对应的dentry 和mnt, 描述"home"对应的mnt 和dentry
                    继续切换到wangchongyang 目录分量，描述"wangchongyang"对应的mnt 和dentry
                */
                look_path->look_mnt = son_file_mnt;            // 执行 home
                look_path->file_dentry = son_file_dentry_node;  //   /home/wangchongyang  file_dentry 指向home，下一次指向wangchongyang
                look_path->look_dentry = son_file_dentry_node;  
                printk(PT_RUN,"%s:%d,file_node_addr=%lx,YYYYYYYYYYlook_path->look_dentry.path_name=%s\n\r",__FUNCTION__,__LINE__,(unsigned long)file_node,look_path->look_dentry->file_name.path_name);
        }

    }
}




/*
    20240719 后续创建xpath.c 文件将相关函数加入到文件中
    参考了linux     namei.c  
    func:  path_parentat; path_lookupat; path_mountpoint;path_openat
    func:  path_init;link_path_walk;do_last;lookup_open;vfs_open
    func:  do_file_open_root;do_filp_open;path_openat


    目录解析过程 例子 mount_path = "/home/wcy"
    第一次解析路径分量 cur_path.name = "/" cur_path.name_len = 1;
    查找当前"/" 对应的dentry :lookup_file_in_pdentry, 如果找到确定当前dentry 是否挂载子文件系统

    第二次解析路径分量："home"           cur_path.name = "home" cur_path.name = 4;  如果找到确定当前dentry 是否挂载子文件系统

    第三次解析路径分量："wcy" cur_path.name = "wcy" cur_path.name = 3; 解析之后跳到 goto last_component;

    有可能存在内存泄漏
*/
int real_lookup_path(char *mount_path, lookup_path_t *look_path)
{
    int lookup_flags = 0;
    int ret = -1;
    int error_code;
    ret = loop_split_path_component(mount_path,look_path,&error_code);
    if(error_code == -ENOENT){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return ret;
    }else if(ret == RET_ROOT){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return ret; /*root  no need for parser*/
    }
    else if(ret == RET_LASTER_SLASH){ /*以'/'符号结束*/
        
        lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY; /*目录*/
        look_path->flags |= lookup_flags;
    }
    printk(PT_RUN,"%s:%d,ret=%d\n\r",__FUNCTION__,__LINE__,ret);
    ret = last_component_handle(look_path); /*通过look_path 传递并获取有用的信息*/

    if(ret == -ENOENT){
        printk(PT_DEBUG,"%s:%d, invalid path\n\r",__FUNCTION__,__LINE__);
       
    }else if(ret == -EPERM){
        printk(PT_DEBUG,"%s:%d, cat not access\n\r",__FUNCTION__,__LINE__);
    }

    return ret;

}



int resolve_path(char *path_name, int flags,  lookup_path_t *look_path)
{
    int ret = -1;
    look_path->flags = 0;
    get_trav_start_path(path_name, look_path);
    look_path->flags |= flags;
    look_path->path_type = PATH_NORMAL;
    ret = real_lookup_path(path_name,look_path);
    return ret;
}


