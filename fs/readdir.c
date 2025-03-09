/********************************************************
    
    development started: 2025
    All rights reserved
    author :wangchongyang

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
#include "kernel_types.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "cpu_desc.h"
#include "xos_sleep.h"
#include "tick_timer.h"
#include "xos_cache.h"
#include "xos_page.h"
#include "xdentry.h"
#include "task.h"
#include "schedule.h"
//#include "xos_ramfs.h"
#include "stat.h"
#include "path.h"
#include "dirent.h"
#include "uaccess.h"
#include "uio.h"
#include "xos_file.h"



/*
struct tmp_dir_context
{
    int index;
    void* dir_data_block;
    void (*read_dir)(struct dir_context* dctx,
                                   const char* name,
                                   const int len,
                                   const int dtype);
};
*/

struct dir_context {
    filldir_t actor;
    long pos;
};

typedef struct dirent64_slab{
    struct xos_dirent64  * current_dir;
    struct xos_dirent64  * prev_dir;
    struct xos_dirent64 * last_dir;

}dirent_slab_t;
struct getdents_callback64 {
    struct dir_context ctx;
    dirent_slab_t dirent_slab;
    struct xos_dirent64  * current_dir;
    struct xos_dirent64  * prev_dir;
    struct xos_dirent64 * last_dir;
    int count;
    int error;
};

#define ROUND_UP(x) (((x)+sizeof(long)-1) & ~(sizeof(long)-1))
#define ROUND_UP64(x) (((x)+sizeof(u64)-1) & ~(sizeof(u64)-1))

int copy_dir_msg_to_user(void * __buf, const char * name, int namlen, long offset,
		     long fnode_num, unsigned int d_type)
{

    struct xos_dirent64 *dirent;
    struct getdents_callback64 * buf = (struct getdents_callback64 *)__buf;
    int reclen = ROUND_UP64(offsetof(struct xos_dirent64, d_name) + namlen + 1);

    buf->error = -EINVAL;
    if (reclen > buf->count)
        return -EINVAL;

    dirent = buf->dirent_slab.prev_dir;
    if(dirent){
        memcpy(&dirent->d_off,&offset,sizeof(long));
    }

    dirent = buf->dirent_slab.current_dir;
    memcpy(&dirent->d_ino,&fnode_num,sizeof(long));
    memcpy(&dirent->d_reclen,&reclen,sizeof(short));
    memcpy(&dirent->d_type,&d_type,sizeof(int));
    strncpy(dirent->d_name,name,namlen);
    buf->dirent_slab.prev_dir = dirent;
    dirent = (void  *)dirent + reclen;
    buf->dirent_slab.current_dir = dirent;
    buf->count -= reclen;
    printk(PT_DEBUG,"%s:%d,name=%s\n\r",__FUNCTION__,__LINE__,name);
    return 0;
}




int iterate_dir(struct file *filp, void *buf)
{
    int ret = 0;
    struct getdents_callback64 *call_ptr = buf;
    xinode *fnode = filp->f_dentry->file_node;
    printk(PT_DEBUG,"%s:%d,path_name=%s\n\r",__FUNCTION__,__LINE__,filp->f_dentry->file_name.path_name);

    if (S_ISDIR(fnode->node_mode)) {
        ret = filp->f_ops->readdir(filp, buf, call_ptr->ctx.actor);
    }

    return ret;
}

void dir_buf_init( struct getdents_callback64 *buf,struct xos_dirent64 *dirent,unsigned int buf_size)
{
    buf->dirent_slab.current_dir = dirent;
    buf->dirent_slab.prev_dir = NULL;
    buf->dirent_slab.last_dir = NULL;
    buf->count = buf_size;
    buf->error = 0;
    buf->last_dir = NULL;
}
int sys_readdir(unsigned int fd, struct xos_dirent64 *dirent, unsigned int buf_size)
{
    int ret;
    struct file *filp;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    struct getdents_callback64 buf = {  
        .ctx.actor = (filldir_t)copy_dir_msg_to_user,
        .count = buf_size,
        .current_dir = dirent
    };

    ret = -EBADF;
    filp = get_file_by_fd(fd);
    if (!filp){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        goto out;
    }

    dir_buf_init(&buf,dirent,buf_size);

    ret = iterate_dir(filp, &buf);
    if (ret < 0){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        goto fail_iterate;
    }
        

    ret = buf.error;
    buf.dirent_slab.last_dir = buf.dirent_slab.prev_dir;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    if (buf.dirent_slab.last_dir) 
    {
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        if (!memcpy(&buf.dirent_slab.last_dir->d_off,&filp->f_pos,sizeof(long))) /*need chang to do*/
            ret = -EFAULT;
        else
            ret = buf_size - buf.count;
    }
fail_iterate:
    
out:
	return ret;
}

