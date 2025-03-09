/*****************
    development Started:2025
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 - 2028 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*************************************************/

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



ssize_t do_sys_read(int fd,void *buf,ssize_t count)
{
    struct file *cur_file;
    cur_file = get_file_by_fd(fd);
    if(!cur_file){
        return -EBADF;
    }
    if (fd < 0){
        return -EBADF;
    }
    /*
        Check file permissions  very important
    */
    printk(PT_RUN,"%s:%d,cur_file->f_mode=%x\n\r",__FUNCTION__,__LINE__,cur_file->f_mode);
    if (cur_file->f_mode & FMODE_READ){
        if(cur_file->f_ops->read){
            return cur_file->f_ops->read(cur_file, buf, count, &cur_file->f_pos);
        }
    }

    return 0;
}

