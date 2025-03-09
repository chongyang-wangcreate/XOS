/********************************************************
    
    development started: 2024
    author :wangchongyang
    email:rockywang599@gmail.com
    
    Copyright (c) 2024 - 2026 wangchongyang

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
#include "error.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "xos_vfs.h"
#include "fs.h"
#include "xos_char_dev.h"
//#include "xos_ramfs.h"
#include "tick_timer.h"
#include "task.h"
#include "stat.h"
#include "path.h"
#include "xos_file.h"
#include "xnode.h"

#define ROUND_UP64(x) (((x)+sizeof(u64)-1) & ~(sizeof(u64)-1))

/*
    说明：
    文件系统以及缓存部分做得比较简陋，很多地方存在问题，很多地方没有实现，尤其资源释放部分，为什么会这样呢?
    主要是因为时间段且琐碎、任务繁重。为了确保 XOS 能顺利完成，并避免真的成为X，因此在开发过程中做了很多妥协。
    毕竟从零开始开发是一个巨大的挑战，我还要不断的学习提高，因此各位观众看到 XOS 中的一些不足之处，也是开发过程中的必然结果，后续会持续迭代完善。

    The file system and cache parts are relatively rudimentary, with many issues and lack of implementation, especially in the resource release part. Why is this happening? 
    Mainly due to the time period being tedious and the task being heavy. In order to ensure the smooth completion of XOS and avoid becoming X, many compromises were made during the development process. 
    After all, developing from scratch is a huge challenge, and I need to constantly learn and improve. Therefore, the shortcomings in XOS that you have seen are also an inevitable result of the development process, and will continue to iterate and improve in the future.


*/

/*********************************
 csdn  知乎上有许多优秀的文章，也建议大家多看

**********************************/

int ramfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
	int mode, devno_t dev);

xsuperblock *init_file_ramfs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data);
void release_ramfs(xsuperblock *super);
int init_file_ramfs_old(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data,xdentry **root_dentry);

int ramfs_mkdir(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode)
{
    int retval = ramfs_new_fnode(parent_node, inode_dentry, mode | S_IFDIR, 0);

    printk(PT_RUN,"%s:%d,parent_node_addr=%lx,inode_dentry->name=%s\n\r",__FUNCTION__,__LINE__,(unsigned long)parent_node,inode_dentry->file_name.path_name);
    if (!retval){
        parent_node->link_cnt++;
    }

    return retval;
}

int ramfs_create(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode, struct fs_path_lookup *lookup)
{
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    ramfs_new_fnode(parent_node, inode_dentry, mode | S_IFREG, 0);
    return 0;
}

static struct xos_dentry * ramfs_lookup (struct xos_inode *parent_node,struct xos_dentry *parent_dentry, struct fs_path_lookup *lookup_path)
{


    return NULL;
}
int generic_dir_open(struct xos_inode *xnode, struct file *filp)
{
    path_name_t *root_name = xos_kmalloc(sizeof(path_name_t));
    root_name->path_name = (unsigned char*)"/";
    root_name->len = 1;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
//    filp->private_data = xdentry_cache_alloc_init(filp->f_dentry, root_name);

    filp->private_data = filp->f_dentry;
    return filp->private_data ? 0 : -ENOMEM;
}


/*
    Need to add locks to prevent concurrency

    static inline ino_t parent_ino(struct dentry *dentry)
    {
        ino_t res;
        spin_lock(&dentry->d_lock);
        res = dentry->d_parent->d_inode->i_ino;
        spin_unlock(&dentry->d_lock);
        return res;
    }

    static inline bool dir_emit_dot(struct file *file, struct dir_context *ctx)
    {
        return ctx->actor(ctx, ".", 1, ctx->pos,
                file->f_path.dentry->d_inode->i_ino, DT_DIR) == 0;
    }
    static inline bool dir_emit_dotdot(struct file *file, struct dir_context *ctx)
    {
        return ctx->actor(ctx, "..", 2, ctx->pos,
            parent_ino(file->f_path.dentry), DT_DIR) == 0;
    }
    dcache_readdir(struct file *file, struct dir_context *ctx)

    static inline bool dir_emit_dots(struct file *file, struct dir_context *ctx)
    {
        if (ctx->pos == 0) {
            if (!dir_emit_dot(file, ctx))
            return false;
            ctx->pos = 1;
        }
        if (ctx->pos == 1) {
            if (!dir_emit_dotdot(file, ctx))
                return false;
            ctx->pos = 2;
        }
        return true;
    }
*/
int generic_dir_readdir (struct file *filp, void *buf, filldir_t filldir)
{
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    xdentry *file_dentry = filp->f_dentry;
    dlist_t *cur_node = NULL;
    long node_num;
    int pos = filp->f_pos;
    unsigned char dt_type;
    int fill_ret;
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    if(filp->f_pos == 0){
        node_num = file_dentry->file_node->node_num;
        if (filldir(buf, ".", 1, pos, node_num, DT_DIR) < 0){
            return -1;
        }
        filp->f_pos++;
    }if(filp->f_pos == 1){
        node_num = parent_xinode(file_dentry);
        if (filldir(buf, "..", 2, pos, node_num, DT_DIR) < 0){
            return -1;
        }
        filp->f_pos++;
    }else{

        cur_node = file_dentry->children_list_head.next;
        for (;cur_node != &file_dentry->children_list_head;
            cur_node = cur_node->next){
            xdentry *next_node;

            next_node = list_entry(cur_node, xdentry, ch_at_parent_list);
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

            /**
            * 将当前节点信息返回给上层
            */
            dt_type = (next_node->file_node->node_mode >> 12) & 15;
            fill_ret = filldir(buf, (const char*)next_node->file_name.path_name,
            next_node->file_name.len, filp->f_pos,
            next_node->file_node->node_num, dt_type);
            if (fill_ret < 0)
                return 0;

            filp->f_pos++;
        }
    }
    return 0;
}





int ramfs_open(struct xos_inode *fs_node, struct file * filp)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    filp->private_data = fs_node;
    return 0;
}

/*
static super_block_ops ramfs_super_ops = {

};
*/

xfile_ops_t ramfs_file_ops = {
    .open = ramfs_open,
     .llseek     = generic_ram_file_llseek,
/*
    .read		= generic_file_read,
    .write		= generic_file_write,
*/

};

xinode_ops_t ramfs_dir_fnode_ops = {
    .create     = ramfs_create,
    .mkdir		= ramfs_mkdir,
    .lookup		= ramfs_lookup,

};

xfile_ops_t simple_dir_ops = {

    .open		= generic_dir_open,
    .readdir	= generic_dir_readdir,
};


xvfs_file_t  ramfs={
    .fs_type = "rootfs",
    .init_file_system = init_file_ramfs,
    .init_file_system_old = init_file_ramfs_old,
    .release_file_system = release_ramfs,

};
    

/*
    设备节点初始化
*/
void special_xnode_init(xinode *inode,int mode,devno_t dev)
{
    switch(mode & S_IFMT)
    {
        case S_IFCHR:
            inode->file_ops = &comm_chrdev_ops;
            inode->devno = dev;
            printk(PT_DEBUG,"%s:%d,devno=%d\n\r",__FUNCTION__,__LINE__,dev);
            break;
        case S_IFBLK:
            break;
        case S_IFIFO:
            break;
    
    }
}

/*
    当前未定义super_block  操作函数
*/
xinode* alloc_ramfs_node_init(xsuperblock * super,int mode,devno_t dev)
{
    xinode *inode = NULL;
    inode = alloc_cache_xinode();
    if(!inode){
         return NULL;
    }
    
     printk(PT_RUN,"%s:%d,inode=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)inode);
     inode->devno = dev;
     inode->node_mode = mode;
     inode->block_count = 1024;
     inode->p_super = super;
     list_init(&inode->bind_xsuper_list);
     /*
        mode 用来区分是那种文件类型
     */
     switch(mode & S_IFMT){
        case S_IFREG:

//          inode->inode_ops = &ramfs_file_fnode_ops;
            inode->file_ops = &ramfs_file_ops;
            printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;

        case S_IFDIR:
            
            inode->inode_ops = &ramfs_dir_fnode_ops;
            inode->file_ops = &simple_dir_ops;
            if(inode->inode_ops->create){
                printk(PT_RUN,"%s:%d,inode->inode_ops_addr=%lx  XXXXXXXXXXXcreate exist\n\r",__FUNCTION__,__LINE__,(unsigned long)inode->inode_ops);
            }
            break;
        
        case S_IFLNK:
            break;
        default:
            special_xnode_init(inode,mode,dev);
            break;
     }
     return inode;
     
}

/*
    alloc dentry from cache ,where is init cache
    mode very important
*/
int ramfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
	int mode, devno_t dev)
{
    xinode *fnode;
    int error = -ENOSPC;
    /*
        防并发操作
    */
    printk(PT_RUN,"%s:%d,dentry_cache->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    fnode = alloc_ramfs_node_init(parent_node->p_super, mode, dev);
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

    return error;
}


xdentry *alloc_ramfs_dentry_init(xdentry_name *name)
{
    xdentry *dentry = xos_kmalloc(sizeof(xdentry));
    if(dentry == NULL){
        return NULL;
    }
    dentry->file_name.path_name = name->name;
//    memcpy(dentry->file_name.path_name,name->name,name->len);
    dentry->ref_count++;
    list_init(&dentry->ch_at_parent_list);
    list_init(&dentry->children_list_head);
    dentry->cur_mnt_count = 0;
    dentry->is_mount = 0;

    return dentry;
}
void ramfs_super_init(xsuperblock * super,int flags,void *data)
{
    super->block_size = PAGE_SIZE;
    super->mount_behavior = flags;
    super->maigc = RAMFS_MAGIC;
    list_init(&super->inode_list);
//  super->super_ops = NULL;
    super->ref_count = 1;
    xos_spinlock_init(&super->lock);
    
}
/*
    主要工作申请对应文件系统的superblock inode  ,dentry初始化

    申请superblock ，初始化,superblock 真正描述一个文件系统
*/
xsuperblock *init_file_ramfs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data)
{
    xsuperblock *ram_super = xos_kmalloc(sizeof(xsuperblock));
    xinode *root_inode;
    xdentry *root_dentry;
    path_name_t *root_name = xos_kmalloc(sizeof(path_name_t));
    if(ram_super == NULL){
        goto alloc_name_fail;
    }
     printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    /*init super*/
    ramfs_super_init(ram_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_ramfs_node_init(ram_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        goto alloc_node_fail;
    }
    root_inode->ref_count++;
    root_name->path_name = (unsigned char*)"/";
    root_name->len = 1;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    if(root_dentry == NULL){
        goto xdentry_alloc_fail;
    }
    root_dentry->file_node =  root_inode;
    ram_super->root_dentry = root_dentry;
    ram_super->mount_behavior |= MS_ACTIVE;

    return ram_super;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_node_fail:
    xos_kfree(root_name);
alloc_name_fail:
    xos_kfree(ram_super);
    return NULL;
}


int init_file_ramfs_old(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data,xdentry **root_dentry)
{
    int error;
    xsuperblock *ram_super = xos_kmalloc(sizeof(xsuperblock));
    xinode *root_inode;
    path_name_t *root_name = xos_kmalloc(sizeof(path_name_t));
    if(ram_super == NULL){
        error = -ENOMEM;
        return error;
    }
     printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    /*init super*/
    ramfs_super_init(ram_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_ramfs_node_init(ram_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        xos_kfree(root_name);
        error = -ENOMEM;
        return error;
    }
    root_inode->ref_count++;
    root_name->path_name = (unsigned char*)"/";
    root_name->len = 1;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    *root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    (*root_dentry)->file_node =  root_inode;
    (*root_dentry)->xsb = ram_super;
    ram_super->root_dentry = *root_dentry;
    ram_super->mount_behavior |= MS_ACTIVE;

    return 0;
    
}


/*
    脱链dentry 挂接的所有子节点
    并将脱链的节点返回给dentry_cache 
    lock 防并发操作
*/
void free_filesystem_dentry_cache(xdentry *dentry)
{
    xdentry *cur_dnode;
    dlist_t *tmp_lnode;
    dlist_t *ch_head = &dentry->children_list_head;
    list_for_each(tmp_lnode, ch_head){
        cur_dnode = list_entry(tmp_lnode, xdentry, ch_at_parent_list);
        cur_dnode->ref_count = 0;
    }
    if(dentry->ref_count == 0){
        /*
            free
        */
       free_dentry_cache(dentry);
    }
    dentry->ref_count--;
}

void free_filesystem(xsuperblock *super)
{
    super->mount_behavior &= ~MS_ACTIVE;
}
/*
    逆序释放
*/
void release_ramfs(xsuperblock *super)
{
    if (super->root_dentry){
        free_filesystem_dentry_cache(super->root_dentry);
    }
    free_filesystem(super);
}


void init_ramfs()
{
    xfs_type_register(&ramfs);
}

