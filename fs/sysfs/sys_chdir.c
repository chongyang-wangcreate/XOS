/********************************************************
    
    development started: 2025
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
#include "task.h"

/*
    to do copy_from_user
    tmp using user_pathname or memcpy
*/
int do_sys_chdir(char *user_pathname)
{
    int ret = 0;
    int flags = LOOKUP_FOLLOW|LOOKUP_DIRECTORY;  /*last dash is directory，路径必须存在否则返回错误*/
    lookup_path_t name_path;
    char ken_pathname[512] = {0};
    /*
        it need to be changed to copy from user
    */
    memcpy(ken_pathname,user_pathname,strlen(user_pathname));
    struct task_struct *cur_task = get_current_task();
    
    ret = resolve_path(ken_pathname, flags, &name_path);
    
    printk(PT_RUN,"ret code = %d",ret);
    if(ret < 0){
        return ret;
    }
    printk(PT_RUN,"name_path.mnt->root_dentry->file_name.path_name = %s\n\r",name_path.look_mnt->root_dentry->file_name.path_name);
    set_pwd(&cur_task->fs_context, name_path.look_mnt, name_path.look_dentry);
    printk(PT_RUN,"%s:%d\n",__FUNCTION__,__LINE__);
    printk(PT_DEBUG,"cur_path = %s\n",cur_task->fs_context.curr_dentry->file_name);
    return 0;
}


