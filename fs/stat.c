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
#include "error.h"
#include "string.h"
#include "list.h"
#include "xos_fcntl.h"
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
#include "xdentry.h"
#include "xos_file.h"
#include "printk.h"
#include "uio.h"
#include "stat.h"


int get_stat(xinode * file_node,struct kstat *attr)
{
    attr->st_dev =   file_node->p_super->s_dev;
    attr->st_ino =   file_node->node_num;
    attr->st_mode =  file_node->node_mode;
    attr->st_nlink = file_node->link_cnt;
    attr->st_uid =   file_node->uid;
    attr->st_gid =   file_node->gid;
    attr->st_rdev =  file_node->devno;
    attr->st_atime = file_node->i_atime;
    attr->st_mtime = file_node->i_mtime;
    attr->st_ctime = file_node->i_ctime;
//    attr->st_size =  fnode_size(file_node);
    attr->st_blocks = file_node->block_count;
    attr->st_blksize = file_node->block_size;
    printk(PT_DEBUG,"%s:%d，file_node->node_mode=%x\n\r",__FUNCTION__,__LINE__,file_node->node_mode);
    return 0;
}


static int do_stat(int dfd, char  *name, struct kstat *stat)
{
    lookup_path_t look_path = {0};
    int lookup_flags = 0;
    int ret;

    lookup_flags |= LOOKUP_DIRECTORY;
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    ret = resolve_path( name, lookup_flags, &look_path);
    if (!ret) {
        ret = get_stat(look_path.look_dentry->file_node, stat);
    }

    return ret;
}



static inline int copy_stat_to_user(struct kstat *attr, void *stat)
{
	struct kstat tmp_stat = {0};

    tmp_stat.st_dev =   attr->st_dev;
    tmp_stat.st_ino =   attr->st_ino;
    tmp_stat.st_mode =  attr->st_mode;
    tmp_stat.st_nlink = attr->st_nlink;
    tmp_stat.st_rdev =  attr->st_rdev;
    tmp_stat.st_size =  attr->st_size;
    tmp_stat.st_atime = attr->st_atime;
    tmp_stat.st_mtime = attr->st_mtime;
    tmp_stat.st_ctime = attr->st_ctime;
    printk(PT_DEBUG,"%s:%d,tmp_stat.st_mode=%x\n\r",__FUNCTION__,__LINE__,tmp_stat.st_mode);

    return copy_to_user(stat, &tmp_stat, sizeof(tmp_stat));
}

/*
    strcpy is not used in threre 
    tmporary use it
*/
int sys_user_getstat(const char * filename, void *stat)
{
    int ret;
    struct kstat attr;
    struct task_struct *cur = current_task;
//    char *kpath_name = NULL;
    char kpath_buf[512] = {0};
    if(filename == NULL){
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        strcpy(kpath_buf,(const char*)cur->fs_context.curr_dentry->file_name.path_name);
    }else{
        strcpy(kpath_buf,filename);
    }
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);


    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    ret = do_stat(AT_FDCWD, (char *)kpath_buf, &attr);
    if (!ret)
        ret = copy_stat_to_user(&attr, stat);

    return ret;
}
