/********************************************************
    
    development started: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

   Copyright (c) 2024 - 2027 wangchongyang

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
********************************************************/


#include "types.h"
#include "list.h"
#include "error.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "xos_cache.h"
#include "xos_vfs.h"
#include "fs.h"
#include "xos_char_dev.h"
#include "xos_devfs.h"
#include "com_fs.h"
#include "xnode.h"


xos_spinlock_t  devfs_lock;

xinode* alloc_devfs_node_init(xsuperblock * super,int mode,devno_t dev);

int devfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
	int mode, devno_t dev)
{
    xinode *fnode;
    int error = -ENOSPC;
    /*
        防并发操作,锁的范围有点大，暂时先这样
    */
    xos_spinlock(&devfs_lock);
    printk(PT_RUN,"%s:%d,dentry_cache->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    fnode = alloc_devfs_node_init(parent_node->p_super, mode, dev);
    if (fnode) {
        if (parent_node->node_mode & S_ISGID) {
                fnode->gid = parent_node->gid;
                if (S_ISDIR(mode))
                fnode->node_mode |= S_ISGID;
        }
        dentry_cache->file_node = fnode;
        dentry_cache->ref_count++;  /*增加目录引用计数*/
        printk(PT_RUN,"%s:%d,fnode_addr=%lx,inode_dentry=%s,parent_node_ops_add=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)fnode,dentry_cache->file_name.path_name,(unsigned long)dentry_cache->file_node->inode_ops);
        
        printk(PT_RUN,"%s:%d,parent_node=%s,parent_node_addr=%lx\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name,(unsigned long)parent_node);
        error = 0;
    }
    xos_unspinlock(&devfs_lock);
    return error;
}

static struct xos_dentry *
devfs_lookup(struct xos_inode *xnode, struct xos_dentry *xdentry_node,
    struct fs_path_lookup *look_path)
{   

    return NULL;
}

int devfs_mkdir(struct xos_inode *xnode,struct xos_dentry *xdentry_node,int mode)
{
    return 0;
}
int devfs_rmdir(struct xos_inode *xnode,struct xos_dentry *xdentry_node)
{
    return 0;
}
int devfs_mknode(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode ,devno_t dev)
{
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    devfs_new_fnode(parent_node, inode_dentry, mode, dev);
    return 0;
}
int devfs_create(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode, struct fs_path_lookup *lookup)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    devfs_new_fnode(parent_node, inode_dentry, mode | S_IFREG, 0);
    return 0;
}

int devfs_open(struct xos_inode *fs_node, struct file * filp)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    filp->private_data = fs_node;
    return 0;
}


xfile_ops_t devfs_file_ops = {
    .open = devfs_open,
/*
    .read		= generic_file_read,
    .write		= generic_file_write,
*/

};

/*
    原方案部分模块使用函数指针数组,这种方案驱动庞大之后会显得非常混乱，
    所以摒弃掉这种方案，全部使用面向对象设计
    通过设备号major  ,minor ,或者dev_name 找到对应的driver
    driver->dispatch_function[IO_OPEN] = dev_open;
    driver->dispatch_function[IO_CLOSE] = dev_close;
    driver->dispatch_function[IO_READ] = dev_read;
    driver->dispatch_function[IO_WRITE] = dev_write;
*/
xinode_ops_t devfs_dir_fnode_ops = {
    .create = devfs_create,
    .lookup = devfs_lookup,
    .mkdir = devfs_mkdir,
    .rmdir = devfs_rmdir,
    .mknod = devfs_mknode,

};

static super_block_ops devfs_super_ops = {

};

/*
int devfs_read_dir(struct file *filp, void *buf, filldir_t)
{

    return 0;
}
*/

xfile_ops_t devfs_dir_file_ops = {
    .open   = generic_dir_open,
//  .read = devfs_read_dir,
//  .readdir = devfs_read_dir,
};

void devfs_super_init(xsuperblock * super,int flags,void *data)
{
    super->block_size = PAGE_SIZE;
    super->mount_behavior = flags;
    super->maigc = DEVFS_MAGIC;
    super->super_ops = &devfs_super_ops;
    list_init(&super->inode_list);
    super->super_ops = NULL;
    super->ref_count = 1;
    xos_spinlock_init(&super->lock);
    
}

/*
    根据不同mode,初始化不同的操作函数
*/
xinode* alloc_devfs_node_init(xsuperblock * super,int mode,devno_t dev)
{

    /*
        首先判断super operation_ops 是否初始化了相应的alloc_inode 函数
        如果存在,则使用对应的函数分配，这个没有实现暂时屏蔽掉

    */
    xinode *inode = NULL;
    /*
    if(super->super_ops->alloc_inode){
        printk("%s:%d\n\r",__FUNCTION__,__LINE__);
    }else
    */
    {
        inode = alloc_cache_xinode();
        if(!inode){
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            return NULL;
        }

    }
     inode->devno = dev;
     inode->node_mode = mode;
     inode->block_count = 0; /*tmp value*/
     inode->p_super = super;
     list_init(&inode->bind_xsuper_list);

     /*
        mode 用来区分是那种文件类型
     */
     switch(mode & S_IFMT){
        case S_IFREG:

            inode->file_ops = &devfs_file_ops;
            break;

        case S_IFDIR:
            inode->inode_ops = &devfs_dir_fnode_ops;
            inode->file_ops = &devfs_dir_file_ops;
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        case S_IFCHR:
            special_xnode_init(inode,mode,dev);
            break;
        case S_IFLNK:
            break;
        default:
            special_xnode_init(inode,mode,dev);
            break;
     }
     return inode;
     
}



xsuperblock *xos_init_devfs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data)
{
    xsuperblock *dev_super = NULL;
    xinode *root_inode;
    xdentry *root_dentry;
    path_name_t *root_name = NULL;

    dev_super = xos_kmalloc(sizeof(xsuperblock));
    if(dev_super == NULL){
        return NULL;
    }
    root_name = xos_kmalloc(sizeof(path_name_t));
    if(root_name == NULL){
        goto alloc_name_fail;
    }
    /*init super*/
    devfs_super_init(dev_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_devfs_node_init(dev_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        
        goto alloc_devfs_fail;
    }
    root_name->path_name = (unsigned char*)"/dev";
    root_name->len = strlen("/dev");
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    if(!root_dentry){
        goto xdentry_alloc_fail;
    }
    root_dentry->file_node =  root_inode;
    dev_super->root_dentry = root_dentry;
    dev_super->mount_behavior |= MS_ACTIVE;
    printk(PT_DEBUG,"%s:%d,root_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,root_dentry->file_name.path_name);
    xos_spinlock_init(&devfs_lock);

    return dev_super;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_devfs_fail:
    xos_kfree(root_name);
alloc_name_fail:
    xos_kfree(dev_super);
    return NULL;
}


int init_file_devfs_old(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data,xdentry **root_dentry)
{
    int error = -1;
    xsuperblock *dev_super = xos_kmalloc(sizeof(xsuperblock));
    xinode *root_inode;
    path_name_t *root_name = xos_kmalloc(sizeof(path_name_t));
    if(dev_super == NULL){
        error = -ENOMEM;
        return error;
    }
     printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    /*init super*/
    devfs_super_init(dev_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_devfs_node_init(dev_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        
        error = -ENOMEM;
        goto alloc_devfs_fail;
    }
    root_name->path_name = (unsigned char*)"/dev";
    root_name->len = strlen("/dev");
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    *root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    if(!*root_dentry){
        goto xdentry_alloc_fail;
    }
    *root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    (*root_dentry)->file_node =  root_inode;
    (*root_dentry)->xsb = dev_super;
    dev_super->root_dentry = *root_dentry;
    dev_super->mount_behavior |= MS_ACTIVE;
    xos_spinlock_init(&devfs_lock);


    return 0;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_devfs_fail:
    xos_kfree(dev_super);
    xos_kfree(root_name);
    return error;
}

/*
    文件系统卸载部分暂时未实现
*/
void free_dev_filesystem(xsuperblock *super)
{
    /*
        to do
    */
}

void free_dev_fnode_cache(xdentry *dentry)
{
    /*
        to do
    */
}

void xos_release_devfs(xsuperblock *super)
{
    if (super->root_dentry){
        
        free_dev_fnode_cache(super->root_dentry);
    }
    free_dev_filesystem(super);
}

xvfs_file_t  devfs ={
    .fs_type = "devfs",
    .init_file_system = xos_init_devfs,
    .init_file_system_old = init_file_devfs_old,
    .release_file_system = xos_release_devfs,

};

void init_devfs()
{
    xfs_type_register(&devfs);
}


