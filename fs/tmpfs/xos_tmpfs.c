/********************************************************
    
    development started: 2025
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
    
    Copyright (c) 2025 - 2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
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
#include "stat.h"
#include "path.h"
#include "xos_page.h"
#include "xos_zone.h"
#include "tick_timer.h"
#include "task.h"
#include "uio.h"
#include "xos_file.h"
#include "xnode.h"

enum{
    IO_READ,
    IO_WRITE,

};
/*
    2024.12.29 简单实现tmpfs,由于是内存文件系统
    不用考虑 磁盘数据同步，休眠，IO commit提交，从简单开始实现
*/
 xfile_ops_t tmpfs_file_ops;
 xinode_ops_t tmpfs_dir_fnode_ops;
typedef struct uio_msg
{
    void  *buf_base;
    int len;
    int left_size; /*剩余字节数*/
    int read_size; /*读取成功的函数*/
}uio_msg_t;

typedef struct kio {
    struct file		*kfilp;
    long			ki_pos;
    void			*private;
    int			ki_flags;
    u16			ki_hint;
    u16			ki_ioprio; /* See linux/ioprio.h */
    int         nr_segs;
    uio_msg_t   uio_seg;

}kio_t;


#define MAX_PAGE_NR  246
#define BLOCK_SIZE   4096
xos_page_t tmpfs_block[5][BLOCK_SIZE];

/*
    xos  定义了 dentry_cache   ,xnode_cache,等，
    新加入page_cache 

*/


xinode* alloc_tmpfs_node_init(xsuperblock * super,int mode,devno_t dev)
{
    xinode *inode = NULL;
    inode = alloc_cache_xinode();
    if(!inode){
         return NULL;
    }
    printk(PT_DEBUG,"%s:%d,inode=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)inode);
    inode->devno = dev;
    inode->node_mode = mode;
    inode->block_count = 0;
    inode->p_super = super;
//  inode->ref_count++;
    list_init(&inode->bind_xsuper_list);
     /*
        mode 用来区分是那种文件类型
     */
     switch(mode & S_IFMT){
        case S_IFREG:

 //         inode->inode_ops = &tmpfs_file_fnode_ops;
            inode->file_ops = &tmpfs_file_ops;
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;

        case S_IFDIR:
            
            inode->inode_ops = &tmpfs_dir_fnode_ops;
            if(inode->inode_ops->create){
                printk(PT_DEBUG,"%s:%d,inode->inode_ops_addr=%lx  XXXXXXXXXXXcreate exist\n\r",__FUNCTION__,__LINE__,(unsigned long)inode->inode_ops);
            }
//          inode->file_ops = &simple_dir_ops;
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        
        case S_IFLNK:
            break;
        default:
            special_xnode_init(inode,mode,dev);
            break;
     }
     return inode;
     
}

int tmpfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
    int mode, devno_t dev)
{
    xinode *fnode;
    int error = -ENOSPC;
    /*
        防并发操作
    */
    printk(PT_DEBUG,"%s:%d,dentry_cache->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name);
    fnode = alloc_tmpfs_node_init(parent_node->p_super, mode, dev);
    if (fnode) {
        if (parent_node->node_mode & S_ISGID) {
                fnode->gid = parent_node->gid;
                if (S_ISDIR(mode))
                fnode->node_mode |= S_ISGID;
        }
        dentry_cache->file_node = fnode;
        dentry_cache->ref_count++;  /*增加目录引用计数*/
        printk(PT_DEBUG,"%s:%d,fnode_addr=%lx,inode_dentry=%s,parent_node_ops_add=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)fnode,dentry_cache->file_name.path_name,(unsigned long)dentry_cache->file_node->inode_ops);
        
        printk(PT_DEBUG,"%s:%d,parent_node=%s,parent_node_addr=%lx\n\r",__FUNCTION__,__LINE__,dentry_cache->file_name.path_name,(unsigned long)parent_node);
        error = 0;
    }

    return error;
}


int tmpfs_open(struct xos_inode *fs_node, struct file * filp)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    filp->private_data = fs_node;
    return 0;
}
int tmpfs_create(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode, struct fs_path_lookup *lookup)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    tmpfs_new_fnode(parent_node, inode_dentry, mode | S_IFREG, 0);
    return 0;
}

int tmpfs_mkdir(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode)
{
    int retval = tmpfs_new_fnode(parent_node, inode_dentry, mode | S_IFDIR, 0);

    printk(PT_DEBUG,"%s:%d,parent_node_addr=%lx,inode_dentry->name=%s\n\r",__FUNCTION__,__LINE__,(unsigned long)parent_node,inode_dentry->file_name.path_name);
    if (!retval){
        parent_node->link_cnt++;
    }

    return retval;
}


static struct xos_dentry * tmpfs_lookup (struct xos_inode *parent_node,struct xos_dentry *parent_dentry, struct fs_path_lookup *lookup_path)
{


    return NULL;
}

/*
    linux kernel:
    index = *ppos >> PAGE_SHIFT;
    prev_index = ra->prev_pos >> PAGE_SHIFT;
    prev_offset = ra->prev_pos & (PAGE_SIZE-1);
    last_index = (*ppos + iter->count + PAGE_SIZE-1) >> PAGE_SHIFT;
    offset = *ppos & ~PAGE_MASK;
    for (;;){
        struct page *page;
        pgoff_t end_index;
        loff_t isize;
        unsigned long nr, ret;
        if (!page) {
            if (iocb->ki_flags & IOCB_NOWAIT)
				goto would_block;
			page_cache_sync_readahead(mapping,
					ra, filp,
					index, last_index - index);
			page = find_get_page(mapping, index);
			if (unlikely(page == NULL))
				goto no_cached_page;
		}

        ret = copy_page_to_iter(page, offset, nr, iter);
		offset += ret;
		index += offset >> PAGE_SHIFT;
		offset &= ~PAGE_MASK;
		prev_offset = offset;

		isize = i_size_read(inode);
		end_index = (isize - 1) >> PAGE_SHIFT;
		if (unlikely(!isize || index > end_index)) {
			put_page(page);
			goto out;
		}
		
    }
*/
struct page_desc *get_cache_page(xinode *x_node,unsigned long index, void *data,int mode)
{
    /*
    if(index > MAX_PAGE_NR){
        return NULL;
    }
    return tmpfs_block[index];
    */
    dlist_t *pg_list_head;
    xos_spinlock_t *pg_lock;
    dlist_t *pg_list_cur;
    xos_page_t *pg_cur;
    pg_list_head = &x_node->cache_space.page_list_head;
    pg_lock = &x_node->cache_space.pg_lock;
    if(mode == IO_READ){
        if(!is_empty_page_cache(pg_list_head)){
        xos_spinlock(pg_lock);
        list_for_each(pg_list_cur, pg_list_head){
            pg_cur = list_entry(pg_list_cur, xos_page_t, attach_cache_list);
            if(pg_cur->page_num == index){
                xos_unspinlock(pg_lock);
                return pg_cur;
            }
        }
        xos_unspinlock(pg_lock);
        return NULL;
    }else if(mode == IO_WRITE){

                if(!is_empty_page_cache(pg_list_head)){
                xos_spinlock(pg_lock);
                list_for_each(pg_list_cur, pg_list_head){
                    pg_cur = list_entry(pg_list_cur, xos_page_t, attach_cache_list);
                    if((!pg_cur->idle_flags)&&(pg_cur->page_num == index)){
                        xos_unspinlock(pg_lock);
                        return pg_cur;
                    }
                }
                xos_unspinlock(pg_lock);
        }else{
                pg_cur = xos_get_free_page(0, 0);
                /*
                    page_init
                */
                pg_cur->order = 0;
                pg_cur->idle_flags = 1;
                pg_cur->page_num =  index;  /*当前page 在address space 编号*/
                xos_spinlock(pg_lock);
                list_add_back(&pg_cur->attach_cache_list, pg_list_head);
                x_node->cache_space.nr_pages++;
                xos_unspinlock(pg_lock);
                return pg_cur;

            }

        }
    }

    return NULL;
}


struct page_desc *add_cache_page(unsigned long index, void *data)
{


    return NULL;
}


struct page_desc *del_cache_page(unsigned long index, void *data)
{


    return NULL;
}

static void *get_virt_addr_by_page(struct page_desc *page)
{
    void *kaddr;
    kaddr =  PHY_TO_VIRT((XOS_PAGE_TO_PHY(page)));
    return kaddr;
}

static int copy_page_data_to_user(kio_t *desc, struct page_desc *page,
            unsigned long offset, unsigned long size)
{
    unsigned long left_size, count = desc->uio_seg.left_size;
    unsigned long un_copy_size;
    void *kaddr;
    if (size > count)
        size = count;


    kaddr = get_virt_addr_by_page(page);
    /**
     * 复制页中的数据到用户态空间.
     */
    un_copy_size = copy_to_user(desc->uio_seg.buf_base, kaddr + offset, size);
    /*
        返回值未成功复制到用户态字节数
    */

    if (un_copy_size) {
        size -= un_copy_size;
    }
    /**
        * uio的字段.
    */
    desc->uio_seg.left_size = count - size;
    desc->uio_seg.read_size += size;
    desc->uio_seg.buf_base += size;
    left_size = size;
    
    return left_size;
}

static int copy_user_data_to_kpage(kio_t *desc, struct page_desc *page,
        unsigned long offset, unsigned long size)
{
    unsigned long left, count = desc->uio_seg.left_size;
    char *kaddr;

    if (size > count)
        size = count;

    /**
    * page 申请函数get_free_page
    */
    kaddr = get_virt_addr_by_page(page);
    /**
    * 复制用户态空间数据到页地址空间
    */
    left = copy_from_user(kaddr + offset,desc->uio_seg.buf_base, size);


    if (left) {
        size -= left;
    }
    /**
    * uio的字段.
    */
    desc->uio_seg.left_size = count - size;
    desc->uio_seg.read_size += size;
    desc->uio_seg.buf_base += size;

    return size;
}

/*
    1. 通过ppos 获取当前页编号和页内偏移
    2. 找到对应页address_space_cache
*/
long tmpfs_read(struct file *filp, char  *buf, size_t read_size, long *ppos)
{
    int ret = 0;
    u32 file_size = 0;
    /*
    uio_msg_t uio;
    uio.base = buf;
    uio.len = count;
    */
    kio_t  new_kio;
    unsigned long offset;
    unsigned long pg_index;
    struct page_desc *page;
//    int start_page_idx = *ppos/BLOCK_SIZE;
    int end_page_idx;
    int left_size;
    unsigned long read_left_bytes;
    file_size = filp->f_dentry->file_node->file_size;
    end_page_idx = file_size/BLOCK_SIZE;

    if ((filp->f_pos + read_size) > file_size){
        left_size = file_size - filp->f_pos;
        if(left_size == 0){
            return -1; //已经到文件末尾
        }
        read_size = left_size;
    }
    
    pg_index = *ppos >> PAGE_SHIFT;
    offset = *ppos & ~(PAGE_SIZE -1);
    /*
        loop  from pg_index to end_page_index
        find 
    */    
    new_kio.uio_seg.buf_base = buf;  /*user buf*/
    new_kio.uio_seg.len = read_size; /*the number of bytes to be read*/
    new_kio.uio_seg.left_size = read_size; 
    do{

        read_left_bytes = PAGE_SIZE;
        if(pg_index > end_page_idx){
            goto end_loop;
         }
        read_left_bytes = read_left_bytes - offset;  /*offset 页内偏移*/
        page = get_cache_page(filp->f_dentry->file_node,pg_index, filp,IO_READ); /*get page by page_index*/

        /*
            file 和page  attach
            copy_page_data_to_user
        */
        ret = copy_page_data_to_user(&new_kio, page, offset, read_left_bytes);
        offset += ret;
        pg_index += offset >> PAGE_SHIFT; /*超过4K 跳到下一页*/
        offset &= ~(PAGE_SIZE -1);
    }while (ret == read_left_bytes && new_kio.uio_seg.left_size);
end_loop:
        /**
         * 更新文件位置
         */
        *ppos = ((long) pg_index << PAGE_SHIFT) + offset;

    return ret;
}



/*
    
    第一版本我设计的tmpfs 非常简单，没有引入page ,直接就是block_buf
    pos 偏移，index  ,segment 块，受linux 影响重新设计
    缓存page 管理
    文件权限操作暂未实现
*/
long tmpfs_write(struct file *filp, const char  *buf, size_t write_size, long *ppos)
{
    /*
        获取filp->size 
    */
    kio_t  new_kio;
    struct page_desc *page;
    new_kio.uio_seg.buf_base = (void*)buf;
    new_kio.uio_seg.len = write_size;
    new_kio.uio_seg.left_size = write_size;
    unsigned long pg_index;
    unsigned long offset;
    unsigned long write_left_bytes;
    int ret;

    /*
        pos 初始位置如何确定，当前是否存在lseek
        默认pos  ,偏移位置为0
    */
    offset = (*ppos & (PAGE_SIZE -1)); /*页内偏移*/
    pg_index = (*ppos) >> PAGE_SHIFT;     /*页标号*/

    do{
        /*
            同构index 和 offset 确定好具体位置
        */
        write_left_bytes = PAGE_SIZE;
        write_left_bytes = write_left_bytes - offset;
        if (write_left_bytes > write_size){
            write_left_bytes = write_size; /*truncate*/
        }

        /*
            获取page_cache
        */
        page = get_cache_page(filp->f_dentry->file_node,pg_index, filp,IO_WRITE);
        /*
            file 和page  attach
        */
        ret = copy_user_data_to_kpage(&new_kio, page, offset, write_left_bytes);
        if(ret > 0){
            offset += ret;
            pg_index += offset >> PAGE_SHIFT;
            offset &= ~(PAGE_SIZE -1);
            write_size -= ret;
        }else{
            break;
        }

    }while(write_size);

    /**
     * 更新文件位置和大小,当前只能向后写入，暂时不支持删除写操作
     */
    *ppos = ((long) pg_index << PAGE_SHIFT) + offset;
    filp->f_dentry->file_node->file_size = *ppos;
    return 0;
}



xfile_ops_t tmpfs_file_ops = {
    .open       = tmpfs_open,
    .read       = tmpfs_read,
    .write      = tmpfs_write,
    .llseek     = generic_ram_file_llseek,

};

xinode_ops_t tmpfs_dir_fnode_ops = {
    .create     = tmpfs_create,
    .mkdir		= tmpfs_mkdir,
    .lookup		= tmpfs_lookup,

};


void tmpfs_super_init(xsuperblock * super,int flags,void *data)
{
    super->block_size = PAGE_SIZE;
    super->mount_behavior = flags;
    super->maigc = TMPFS_MAGIC;
    list_init(&super->inode_list);
//  super->super_ops = NULL;
    super->ref_count = 1; 
    xos_spinlock_init(&super->lock);
    
}



/*
    主要工作申请对应文件系统的superblock inode  ,dentry初始化
    path_name_t root_name; 这个存在问题
*/
xsuperblock *init_file_tmpfs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data)
{
    xsuperblock *tmpfs_super = NULL;
    xinode *root_inode;
    xdentry *root_dentry;
    path_name_t *root_name = NULL;;

    tmpfs_super =  xos_kmalloc(sizeof(xsuperblock));
    if(!tmpfs_super){
        
        goto alloc_super_fail;
    }
    /*init super*/
    tmpfs_super_init(tmpfs_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_name = xos_kmalloc(sizeof(path_name_t));
    if(!root_name){
        goto alloc_name_fail;
    }
    root_name->path_name = (unsigned char*)"/tmp";
    root_name->len = strlen("/tmp");
    root_inode = alloc_tmpfs_node_init(tmpfs_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        goto alloc_node_fail;
    }

    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    if(!root_dentry){
        goto xdentry_alloc_fail;
    }
    root_dentry->file_node =  root_inode;
    tmpfs_super->root_dentry = root_dentry;
    tmpfs_super->mount_behavior |= MS_ACTIVE; /*MS_ACTIVE*/

    
    return tmpfs_super;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_node_fail:
    xos_kfree(root_name);
alloc_name_fail:
    xos_kfree(tmpfs_super);
alloc_super_fail:
    
    return NULL;

    
}

void tmpfs_free_fnode_cache(xdentry *dentry)
{
    /*
        to do
    */
}

void tmpfs_free_filesystem(xsuperblock *super)
{
    /*
        to do
    */
}


void release_tmpfs(xsuperblock *super)
{
    if (super->root_dentry){
        
        tmpfs_free_fnode_cache(super->root_dentry);
        tmpfs_free_filesystem(super);
    }
}


xvfs_file_t  tmpfs={
    .fs_type = "tmpfs",
    .init_file_system = init_file_tmpfs,
    .release_file_system = release_tmpfs,

};

void init_tmpfs()
{
    xfs_type_register(&tmpfs);
}

