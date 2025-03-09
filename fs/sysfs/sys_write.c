/********************************************************
    
    development start: 2025
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
    
   Copyright (c) 2025 - 2028 wangchongyang

   This program is licensed under the GPL v2 License. See LICENSE for more details.

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


ssize_t do_sys_write(int fd,void *buf,ssize_t count){

    int ret = -1;
    struct file *new_file;
    new_file = get_file_by_fd(fd);
    
    if (new_file->f_flags & O_APPEND)
        new_file->f_pos = new_file->f_dentry->file_node->file_size;
    if(new_file->f_mode & FMODE_WRITE){
        if(new_file->f_ops->write){
            ret =new_file->f_ops->write(new_file,buf,count,&new_file->f_pos);
        }
    }
    

    return ret;
}

