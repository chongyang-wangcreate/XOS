/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
    
   Copyright (c) 2024 - 2028 wangchongyang

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   "When using, modifying, or distributing this code, 
   appropriate credit must be given to the original author. 
   You must include an acknowledgment of the original author in all copies or substantial portions of the software.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Thank you for csdn pwl999 sharing the good article
********************************************************/


#include "types.h"
#include "list.h"
#include "error.h"
#include "string.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "printk.h"
#include "xos_zone.h"
#include "xos_page.h"
#include "xos_cache.h"
#include "xdentry.h"
#include "xdentry.h"
#include "xos_path.h"
#include "tick_timer.h"
#include "xnode.h"
#include "fs.h"
#include "mount.h"
#include "task.h"
#include "xos_vfs.h"
#include "xos_block_dev.h"
#include "path.h"

xmount_t *mount_cache_ptr = NULL;

#define MAX_HASH_NR 1024
#define MOUNT_CACHE_NR  256

dlist_t mount_hash_pool[MAX_HASH_NR];

xos_spinlock_t mnt_lock;
xos_spinlock_t inode_lock;
xos_spinlock_t dentry_lock;
xos_spinlock_t g_file_lock;

xmount_t  mount_cache_pool[MOUNT_CACHE_NR] = {0};

/*
    设计有点Low 暂时先这样
    mem_cache 管理存在Bug 暂时先使用数据段
*/
void  init_mount_cache(void)
{
    int i;
    for(i = 0;i < MOUNT_CACHE_NR; i++)
    {
        mount_cache_pool[i].alloc_count = 0;
        xos_spinlock_init(&(mount_cache_pool[i].mnt_lock));
    }
    for(i = 0;i < MAX_HASH_NR; i++)
    {
        list_init(&mount_hash_pool[i]);
    }
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
}


xmount_t *xmount_cache_alloc()
{
    int i;
    xos_spinlock(&mnt_lock);
    for(i = 0;i < MOUNT_CACHE_NR;i++)
    {
        if(mount_cache_pool[i].alloc_count == 0){
            mount_cache_pool[i].alloc_count = 1;
             xos_unspinlock(&mnt_lock);
            return &mount_cache_pool[i];
        }
    }
    xos_unspinlock(&mnt_lock);
    return NULL;
}

void xmount_cache_free(xmount_t *mnt_node)
{
    xos_spinlock(&mnt_lock);
    if(mnt_node->alloc_count == 1){
        mnt_node->alloc_count = 0;
    }
    xos_unspinlock(&mnt_lock);

}


int get_trav_start_path(char *mount_dir, lookup_path_t *lpath)
{
    /*
        本函数实现还存在问题
        进程切换时新进程需要设置new_proc->fs_context
    */
    if(mount_dir == NULL || lpath == NULL){
        return -EINVAL;
    }
    xos_spinlock(&mnt_lock);
    if(*mount_dir == '/'){
            lpath->look_mnt = current_task->fs_context.root_dentry_mount;
            lpath->look_dentry = current_task->fs_context.root_dentry;
            printk(PT_RUN,"%s:%d,lpath->look_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,lpath->look_dentry->file_name.path_name);

    }else{

        lpath->look_mnt = current_task->fs_context.curr_dentry_mount;
        lpath->ref += 1;
        lpath->look_mnt->ref_count++;
        lpath->look_dentry = current_task->fs_context.curr_dentry;
    }
    xos_unspinlock(&mnt_lock);
    return 0;
}


int get_path_by_mount_name(char *mount_dir,int flags,lookup_path_t *lookup_path)
{
    /*
        根据mount_dir 确定当前目录
        dentry ,root_dentry 或者是cur_dentry
    */
    get_trav_start_path(mount_dir,lookup_path);

    real_lookup_path(mount_dir,lookup_path);

    return 0;
}
void set_pwd(x_task_fs *fs, xmount_t *mnt,xdentry *dentry_cache)
{
    printk(PT_RUN,"%s:%d,%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)fs->curr_dentry_mount);
    xos_spinlock(&fs->lock);
    fs->curr_dentry_mount = mnt;
    fs->curr_dentry = dentry_cache;
    xos_unspinlock(&fs->lock);
}

void set_root(x_task_fs *fs, xmount_t *mnt,xdentry *dentry_cache)
{
    xos_spinlock(&fs->lock);
    fs->root_dentry_mount = mnt;
    fs->root_dentry = dentry_cache;
    xos_unspinlock(&fs->lock);
}

static int xos_mount_get_super(xvfs_file_t *fs,
                               int flags,
                               const char *dev_name,
                               void *data,
                               xsuperblock **out_super)
{
    xos_block_device_t *bdev = NULL;
    xsuperblock *super = NULL;
    int ret;

    if (fs == NULL || out_super == NULL) {
        return -EINVAL;
    }
    *out_super = NULL;

    if (fs->fs_flags & XOS_FS_REQUIRES_BLOCK) {
        if (dev_name == NULL || dev_name[0] == '\0') {
            return -EINVAL;
        }
        bdev = xos_blockdev_get_by_name(dev_name);
        if (bdev == NULL) {
            return -ENODEV;
        }
        if (bdev->flags & XOS_BLOCKDEV_READ_ONLY) {
            flags |= SB_RDONLY;
        }
    }

    if (fs->mount != NULL) {
        ret = fs->mount(fs, bdev, flags, dev_name, data, &super);
    } else if (fs->init_file_system != NULL) {
        super = fs->init_file_system(fs, flags, dev_name, data);
        ret = super == NULL ? -EIO : 0;
    } else {
        ret = -ENOSYS;
    }

    if (ret < 0 || super == NULL || super->root_dentry == NULL) {
        if (super != NULL && fs->release_file_system != NULL) {
            fs->release_file_system(super);
        }
        if (bdev != NULL) {
            xos_blockdev_put(bdev);
        }
        return ret < 0 ? ret : -EINVAL;
    }

    super->fs_type = fs;
    super->block_device = bdev;
    super->s_dev = bdev == NULL ? 0 : bdev->devno;
    super->mount_behavior |= flags;
    super->root_dentry->xsb = super;
    *out_super = super;
    return 0;
}


void xos_put_superblock(xsuperblock *super)
{
    xos_block_device_t *bdev;
    xvfs_file_t *fs;

    if (super == NULL) {
        return;
    }

    bdev = super->block_device;
    fs = super->fs_type;
    if (super->super_ops != NULL && super->super_ops->sync_fs != NULL) {
        super->super_ops->sync_fs(super);
    }
    if (super->super_ops != NULL && super->super_ops->put_super != NULL) {
        super->super_ops->put_super(super);
    } else if (fs != NULL && fs->release_file_system != NULL) {
        fs->release_file_system(super);
    }
    if (bdev != NULL) {
        xos_blockdev_put(bdev);
    }
}


void xos_destory_superblock(xsuperblock *super)
{
    xos_block_device_t *bdev;
    xvfs_file_t *fs;

    if (super == NULL) {
        return;
    }

    bdev = super->block_device;
    fs = super->fs_type;
    if (super->super_ops != NULL && super->super_ops->sync_fs != NULL) {
        super->super_ops->sync_fs(super);
    }
    if (super->super_ops != NULL && super->super_ops->put_super != NULL) {
        super->super_ops->put_super(super);
    } else if (fs != NULL && fs->release_file_system != NULL) {
        fs->release_file_system(super);
    }
    if (bdev != NULL) {
        xos_blockdev_put(bdev);
    }
}


static int xos_create_mount(xvfs_file_t *fs,
                            int fs_flags,
                            int mnt_flags,
                            const char *dev_name,
                            void *data,
                            xmount_t **out_mount)
{
    xmount_t *mnt;
    xsuperblock *xsb;
    int ret;

    if (out_mount == NULL) {
        return -EINVAL;
    }
    *out_mount = NULL;

    mnt = xmount_cache_alloc();
    if(mnt == NULL){
        return -ENOMEM;
    }
    printk(PT_RUN,"%s:%d,mnt_addr=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)mnt);
    mnt->dev_name = NULL;
    mnt->x_super = NULL;
    mnt->root_dentry = NULL;
    mnt->fs_root = NULL;
    mnt->mount_parent = NULL;
    mnt->mount_point = NULL;
    mnt->mount_point_parent = NULL;
    mnt->ref_count = 1;
    mnt->mnt_flags = mnt_flags;
    list_init(&mnt->link_children_list);
    list_init(&mnt->link_parent_list);
    list_init(&mnt->hash_list);
    xos_spinlock_init(&mnt->mnt_lock);
    if (dev_name != NULL) {
        mnt->dev_name = xos_kmalloc(strlen(dev_name) + 1);
        if (mnt->dev_name == NULL) {
            ret = -ENOMEM;
            goto fail;
        }
        strcpy(mnt->dev_name, dev_name);
    }

    ret = xos_mount_get_super(fs, fs_flags, dev_name, data, &xsb);
    if (ret < 0) {
        goto fail;
    }

    mnt->x_super = xsb;
    mnt->root_dentry = xsb->root_dentry;
    mnt->fs_root = xsb->root_dentry;
    mnt->mount_point = xsb->root_dentry;
    *out_mount = mnt;
    return 0;

fail:
    if (mnt->dev_name != NULL) {
        xos_kfree(mnt->dev_name);
        mnt->dev_name = NULL;
    }
    xmount_cache_free(mnt);
    return ret;
}

static void xos_destroy_mount(xmount_t *mnt)
{
    if (mnt == NULL) {
        return;
    }
    if (mnt->x_super != NULL) {
    //    xos_desctory_superblock(mnt->x_super);
        xos_put_superblock(mnt->x_super);
        mnt->x_super = NULL;
    }
    if (mnt->dev_name != NULL) {
        xos_kfree(mnt->dev_name);
        mnt->dev_name = NULL;
    }
    xmount_cache_free(mnt);
}


xmount_t *lookup_mnt(xmount_t *mnt, xdentry *dentry_cache)
{
    int scan_cnt = 0;
    xmount_t *ret = NULL;
//    dlist_t *dentry_head = &filenode_cache->children_list_head;
    dlist_t *hash_head;
    dlist_t *cur;
    xmount_t *tmp_mount = NULL;
    uint64_t hash;
    #if 0
	list_for_each(node, dentry_head) {
		tmp_mount = list_entry(node, xmount_t, hash_list);
		/**
		 * 该文件系统被mount到指定目录了
		 * 返回给调用者
		 */
		if (tmp_mount->mount_parent == mnt && tmp_mount->mount_point == filenode_cache) {
			tmp_mount->alloc_count++;
            ret = tmp_mount;
			break;
		}
	}
    #endif
    xos_spinlock(&mnt_lock);
    hash = ((uint64_t)mnt ^ (uint64_t)dentry_cache);
    hash <<= 5;
    printk(PT_RUN,"%s:%d，hash=%lx\n\r",__FUNCTION__,__LINE__,hash);
    hash_head = &mount_hash_pool[hash%MAX_HASH_NR];
    list_for_each(cur, hash_head) {
        if(scan_cnt++ >= MOUNT_CACHE_NR){
           printk(PT_ERROR,"%s:%d，loop exceeded the limit\n\r",__FUNCTION__,__LINE__);
           break;
        }
        tmp_mount = list_entry(cur, xmount_t, hash_list);
        printk(PT_RUN,"%s:%d,tmp_mount->root_dentry->path_name=%s\n\r",__FUNCTION__,__LINE__,tmp_mount->root_dentry->file_name.path_name);
        if (tmp_mount->mount_parent == mnt && tmp_mount->mount_point == dentry_cache) {
            tmp_mount->ref_count++;
            ret = tmp_mount;
            printk(PT_RUN,"%s:%d,ret->root_dentry->path_name=%s\n\r",__FUNCTION__,__LINE__,ret->root_dentry->file_name.path_name);
            break;
        }
    }
    xos_unspinlock(&mnt_lock);
    return ret;
}

/*
    未添加防并发操作锁
    2024,1229 21:42 存在问题
    
    hash = ((uint64_t)look_res->mnt ^ (uint64_t)look_res->look_dentry);
    hash <<= 5;
    list_add_front(&new_mnt->hash_list, &mount_hash_pool[hash%MAX_HASH_NR]);

*/
int  attach_parent_mount(xmount_t *new_mnt,lookup_path_t *look_res, int mnt_flags)
{
    int ret;
    ret = -EBUSY;
    dlist_t *cur_list;
    dlist_t *list_head;
    xdentry  *cur_dentry;
//    xmount_t *cur_mount_node;
    xmount_t *parent;
    xmount_t *l_mnt = (xmount_t*)look_res->look_mnt;
    uint64_t hash;
    xos_spinlock(&mnt_lock);
    if(l_mnt->x_super == new_mnt->x_super){

        if(l_mnt->mount_point == look_res->look_dentry){
            xos_unspinlock(&mnt_lock);
            return ret;
        }
    }
    xos_unspinlock(&mnt_lock);
    new_mnt->mnt_flags = mnt_flags;
   
    /*
        to do
        1. 检查look_res mount 挂载点是否是目录,new_mnt->root_dentry->inode->mode 是否是目录
        2. 目录是否有效
        3. 挂载点(目录有效)父子文件系统绑定
    */
    xos_spinlock(&mnt_lock);
    if (!S_ISDIR(look_res->look_dentry->file_node->node_mode)
    || !S_ISDIR(new_mnt->root_dentry->file_node->node_mode)){

        xos_unspinlock(&mnt_lock);
        return -ENOTDIR;
    }
    xos_unspinlock(&mnt_lock);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    /*
        2   挂载目录是否有效，挂载点目录是否有效
    */

    /*
        3. 挂载点(目录有效)父子文件系统绑定
    */
    xos_spinlock(&mnt_lock);
    parent = look_res->look_mnt;
    if(!parent){
        
        printk(PT_DEBUG,"%s:%d，DDDDDDD!!!!!!\n\r",__FUNCTION__,__LINE__);
    }
    new_mnt->mount_parent = parent;
    new_mnt->mount_point = look_res->look_dentry; /*当前文件系统挂载点，look_res->look_dentry 属于父文件系统目录*/
    new_mnt->mount_point_parent = look_res->look_dentry->parent_dentry;
    if(new_mnt->root_dentry){
        printk(PT_DEBUG,"@@@@@@@@@@@@%s:%d,new_mnt->root_dentry.name=%s\n\r",__FUNCTION__,__LINE__,new_mnt->root_dentry->file_name.path_name);
    }
    look_res->look_dentry->cur_mnt_count++; //更新该目录下挂载文件系统的数量
    look_res->look_dentry ->is_mount = EXIST_MOUNT;
    
    printk(PT_RUN,"%s:%d，new_mnt->mount_poin.namet=%s\n\r",__FUNCTION__,__LINE__,look_res->look_dentry->file_name.path_name);
    /*
        链表加入顺序 和 链表遍历顺序不要乱
    */
    printk(PT_RUN,"%s:%d,new_mnt->root_dentry=%s\n\r",__FUNCTION__,__LINE__,new_mnt->root_dentry->file_name.path_name);
    printk(PT_RUN,"%s:%d,look_res->look_dentry=%s\n\r",__FUNCTION__,__LINE__,look_res->look_dentry->file_name.path_name);
    printk(PT_DEBUG,"%s:%d,parent->root_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,parent->root_dentry->file_name.path_name);
    list_add_back(&new_mnt->link_parent_list, &parent->link_children_list);
    
	hash = ((uint64_t)look_res->look_mnt ^ (uint64_t)look_res->look_dentry);
	hash <<= 5;
    printk(PT_RUN,"%s:%d，hash=%lx\n\r",__FUNCTION__,__LINE__,hash);
    list_add_front(&new_mnt->hash_list, &mount_hash_pool[hash%MAX_HASH_NR]);
    
    /*
        hash_list 暂时没有使用，没有初始化
    */
//  list_add_back(&new_mnt->hash_list, &parent->hash_list);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    list_head = &parent->link_children_list;
    #ifdef old
    list_for_each(cur_list, list_head){
        cur_mount_node = list_entry(cur_list,xmount_t,link_parent_list);
        
        printk("%s:%d,cur_mount_node->root_dentry=%s\n\r",__FUNCTION__,__LINE__,cur_mount_node->root_dentry->file_name.path_name);
    }
    #endif
    list_head = &look_res->look_dentry->parent_dentry->children_list_head;
    list_for_each(cur_list, list_head){
        cur_dentry = list_entry(cur_list,xdentry,ch_at_parent_list);
        
        printk(PT_DEBUG,"%s:%d,root_dentry->dir=%s\n\r",__FUNCTION__,__LINE__,cur_dentry->file_name.path_name);
    }
    xos_unspinlock(&mnt_lock);

    /*
        暂时先使用return 0 ,后续更改成具体的return value
    */
    return 0;
}


static int xos_mount_child(xvfs_file_t *fs,
                           lookup_path_t *look_path,
                           unsigned long fs_flags,
                           unsigned long mnt_flags,
                           char *dev_name,
                           void *data)
{
    int ret;
    xmount_t *son_m_desc; 
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    ret = xos_create_mount(fs, fs_flags, mnt_flags, dev_name,
                           data, &son_m_desc);
    if (ret < 0) {
        return ret;
    }
    /******************************************
    * 将子文件系统实例与父文件系统装载点进行绑定建立联系
    ********************************************/
    ret = attach_parent_mount(son_m_desc, look_path, mnt_flags);
    if(ret < 0){
        xos_destroy_mount(son_m_desc);
        return ret;
    }

    /*
        2024.11.17
        先使用return 0,做其它重要的工作
    */
    return 0;
}


/*
    创建对应的xmount_t 初始化和对应的文件系统绑定
    还涉及到父文件系统
    如果当前文件系统挂载的不是真正的根，那么还需要根父文件系统关联起来

    这个函数写的存在问题，需要彻底重写，因为我自己引入参数mount_path
    但是根本没有解析mount_path ,没有open 

    mount_path 为/ 第一个文件系统不需要open
    如果挂载的mount_path 非/, 需要先open(mount_path)

    有必要为挂载根文件系统单独实现一个mount 函数，感觉混在一块有点混乱
*/
int do_mount_at(const char *fs_name,int flags,char *mount_path,char *dev_name,void *data)
{
    int ret = 0;
    int mnt_flags = 0;
    lookup_path_t look_path;
    xvfs_file_t *fs;
    xmount_t *mnt_desc; 

    if (fs_name == NULL || mount_path == NULL) {
        return -EINVAL;
    }
    fs = find_match_system(fs_name);
    if (fs == NULL) {
        return -ENODEV;
    }
    printk(PT_DEBUG,"%s:%d,fs_name=%s\n\r",
           __FUNCTION__,__LINE__,fs->fs_type);
   
    if(strcmp(mount_path,"/") == 0){
        ret = xos_create_mount(fs, flags, 0, dev_name, data, &mnt_desc);
        if (ret < 0) {
            return ret;
        }
        printk(PT_RUN,"%s:%d,root_mnt=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)mnt_desc);
        xos_spinlock(&mnt_lock);
        mnt_desc->mount_parent = mnt_desc;
        mnt_desc->mount_point_parent = mnt_desc->root_dentry;
        /*
            当前current_task 为空
            root mnt attach curr_task->fs_context
        */
        set_pwd(&current_task->fs_context, mnt_desc, mnt_desc->root_dentry);
        set_root(&current_task->fs_context, mnt_desc, mnt_desc->root_dentry);
        xos_unspinlock(&mnt_lock);
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return ret;
    }else{
        /*
            非根还需要跟父文件系统关联
        */
        /*
            1. 打开挂载路径文件获取文件父目录inode dentry superblock 相关信息
            2. 给当前文件分配inode dentry
            3. 父子目录关联
            4. 父子文件系统关联
        */
        
        if (flags & MS_NOSUID)
                mnt_flags |= MNT_NOSUID;
        if (flags & MS_NODEV)
                mnt_flags |= MNT_NODEV;
        if (flags & MS_NOEXEC)
               mnt_flags |= MNT_NOEXEC;
        if (flags & MS_RDONLY)
               mnt_flags |= MNT_READONLY;
        
        ret = resolve_path(mount_path, LOOKUP_FOLLOW,  &look_path);
        if (ret < 0) {
            return ret;
        }
        printk(PT_DEBUG,"%s:%d，look_path.look_dentry_name=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name);
        ret = xos_mount_child(fs, &look_path, flags, mnt_flags,
                              dev_name, data);
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return ret;
    }

}

int do_mount(const char *fs_name,int mode,char *mount_path,char *dev_name,void *data)
{
    /*
        后续增加附加处理
    */
    return do_mount_at(fs_name,mode,mount_path,dev_name,data);
}


int mount_rootfs(const char *fs_name,int mode,char *mount_path,char *dev_name,void *data)
{
    return do_mount(fs_name,mode,mount_path,dev_name,data);
}


int  deattach_parent_mount(xdentry *dentry_node)
{

    dentry_node->is_mount = 0;
    return 0;
}

/*
    path realted flags related
*/
void sys_umount(char *path_name,int flags)
{
    /*
        to do
        1. parse path
        2. get dentry, mnt
        3. check is it a mountpoint
        4. chang the dentry mnt_flags= 0
    */
}



#if 0
这个旧接口暂时把实现删除了
int do_mount_at_old(const char *fs_name,int flags,char *mount_path,char *dev_name,void *data)

#endif

