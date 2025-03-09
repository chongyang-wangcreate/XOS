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
#include "xnode.h"
#include "syscall.h"
#include "xnode.h"
/*
    user buf 
    copy to user

    20250109 实现存在问题，只考虑了同一个文件系统的路径展示
    出现mount 情况未考虑,每个文件系统都有自己的根

    20250110,实现了triple可用的getcwd ，调试过程中发现几个问题
    1. mem_cache 存在一些bug,造成getcwd 出现问题， 后续会重构 mem_cache 
    2. 文件系统切换存在问题
    本功能还需要继续完备，缺少特殊字符支持 ，缺少 路径检查功能
*/
int sys_getcwd_org(char *user_buf,int buf_size)
{
    int i = 0;
    char path_name[512] = {0};
    char path_buf[12][128] = {0};
    struct xos_dentry  *dentry_node;
//    xmount_t  *dentry_mnt;
    struct task_struct *cur_task = get_current_task();
    dentry_node = cur_task->fs_context.curr_dentry;
//  dentry_mnt = cur_task->fs_context.curr_dentry_mount;
    printk(PT_DEBUG,"do_sys_getcwd,cur_path = %s\n\r",dentry_node->file_name.path_name);
    printk(PT_DEBUG,"mnt_cur_count = %d\n\r",dentry_node->cur_mnt_count);
    while(dentry_node->parent_dentry != dentry_node){
        
        printk(PT_RUN,"do_sys_getcwd,cur_path = %s\n\r",dentry_node->file_name.path_name);
        strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
        dentry_node = dentry_node->parent_dentry;
        i++;
    }
    strcpy(path_buf[i],(const char*)("/"));
    for(;i >=0 ;i--){
        strcat(path_name, path_buf[i]);
        if(strcmp(path_buf[i],"/")){
            strcat(path_name, "/");
        }
    }
    
    printk(PT_DEBUG,"path_name = %s\n\r",path_name);


    return 0;
}


int do_sys_getcwd(char *user_buf,int buf_size)
{
    int i = 0;
    char path_name[512] = {0};
    char path_buf[12][128] = {0};
    struct xos_dentry  *dentry_node;
    xmount_t *parent_mnt;
    xmount_t  *dentry_mnt;
    struct xos_dentry  *root_dentry;
    xmount_t  *root_mnt;
    struct task_struct *cur_task = get_current_task();
    dentry_node = cur_task->fs_context.curr_dentry;
    dentry_mnt = cur_task->fs_context.curr_dentry_mount;
    root_dentry = cur_task->fs_context.root_dentry;
    root_mnt = cur_task->fs_context.root_dentry_mount;
    printk(PT_RUN,"do_sys_getcwd,cur_path = %s\n\r",dentry_node->file_name.path_name);
    printk(PT_RUN,"mnt_cur_count = %d\n\r",dentry_node->cur_mnt_count);
    printk(PT_RUN,"dentry_mnt->root_dentry.file_name.path_name = %s\n\r",dentry_mnt->root_dentry->file_name.path_name);
    
    printk(PT_RUN,"root_dentry->file_name = %s\n\r",root_dentry->file_name.path_name);
    printk(PT_RUN,"root_mnt.mount_point = %s\n\r",root_mnt->mount_point->file_name.path_name);

    while((dentry_node != root_dentry) ||(dentry_mnt != root_mnt))
    {

    
        printk(PT_RUN,"dentry_mnt->root_dentry.path = %s,%d\n\r",dentry_mnt->root_dentry->file_name.path_name,__LINE__);
        printk(PT_RUN,"dentry_node->mount_point.path = %s:%d\n\r",dentry_node->file_name.path_name,__LINE__);

        /*
            当前是否到达本文件系统的根 ,当前目录是否等于父目录

            dentry_mnt->root_dentry   值设置存在问题，当前文件系统根的dentry
            如例子/dev/  的根就应该是 "/dev" 这和mount_point 不是一个概念，mount_point 值应该是父文件系统目录
            如/dev/  mount_point 的值就是'/'对应的dentry 值
        */
        if (dentry_node == dentry_mnt->root_dentry || (dentry_node->parent_dentry == dentry_node)){
            /*
                已经到达当前文件系统的根
                需要获取当前文件系统的父文件系统
            */
            
            printk(PT_RUN," %s:%d,dentry_node->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
            printk(PT_RUN," %s:%d,dentry_mnt->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_mnt->root_dentry->file_name.path_name);

            parent_mnt = dentry_mnt->mount_parent;
            if(parent_mnt != dentry_mnt){
                dentry_node = dentry_mnt->mount_point;
                printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
                dentry_mnt = parent_mnt;
                printk(PT_RUN,"dentry_node->mount_point.path = %s:%d\n\r",dentry_node->file_name.path_name,__LINE__);
                if(dentry_mnt->mount_point->parent_dentry){
                    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
                }
                printk(PT_RUN,"dentry_mnt.mount_point = %s\n\r",dentry_mnt->mount_point->file_name.path_name);
                //continue;
            }
            
        }
         strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
         dentry_node = dentry_node->parent_dentry;
         i++;
        printk(PT_RUN," %s:%d,dentry_node->path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
    }

    for(;i >=0 ;i--){
        strcat(path_name, path_buf[i]);
        if(strcmp(path_buf[i],"/")){
            strcat(path_name, "/");
        }
    }
    
    printk(PT_DEBUG,"path_name = %s\n\r",path_name);


    return 0;
}



#if 0
int sys_getcwd_2(char *user_buf,int buf_size)
{
    int i = 0;
    char path_name[512] = {0};
    char path_buf[12][128] = {0};
    struct xos_dentry  *dentry_node;
    xmount_t *parent_mnt;
    xmount_t  *dentry_mnt;
    struct xos_dentry  *root_dentry;
    xmount_t  *root_mnt;
    struct task_struct *cur_task = get_current_task();
    dentry_node = cur_task->fs_context.curr_dentry;
    dentry_mnt = cur_task->fs_context.curr_dentry_mount;
    root_dentry = cur_task->fs_context.root_dentry;
    root_mnt = cur_task->fs_context.root_dentry_mount;
    printk(PT_DEBUG,"do_sys_getcwd,cur_path = %s\n\r",dentry_node->file_name.path_name);
    printk(PT_DEBUG,"mnt_cur_count = %d\n\r",dentry_node->cur_mnt_count);
    
    printk(PT_DEBUG,"root_dentry->file_name = %s\n\r",root_dentry->file_name.path_name);
    printk(PT_DEBUG,"root_mnt.mount_point = %s\n\r",root_mnt->mount_point->file_name.path_name);
    xos_spinlock(&mnt_lock);
    while(1){
        /**
             已经到达当前进程的根目录，退出 
        **/
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        if(!root_mnt){
            printk(PT_DEBUG,"%s:%d,dentry_mnt->mount_point =%s \n\r",__FUNCTION__,__LINE__,dentry_mnt->mount_point->file_name.path_name);
        }
        if(dentry_node == root_dentry && dentry_mnt == root_mnt){
            printk(PT_DEBUG,"dentry_node.path_name = %s\n\r",dentry_node->file_name.path_name);
            break;
        }
        /*
            不是所在文件系统的根目录，返回上一级目录
        */
       
        if(dentry_node != dentry_mnt->root_dentry){
            dentry_node = dentry_node->parent_dentry ; 
            break;
        }
        
        printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        if(dentry_mnt){
            
            printk(PT_DEBUG,"%s:%d,dentry_mnt=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)dentry_mnt);
        }
        parent_mnt = dentry_mnt->mount_parent;
        if(!parent_mnt){
            
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        }
        if(parent_mnt == dentry_mnt){
             printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        }
         printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        /*
            到达挂载点的根目录，(关键字挂载点)
        */
        /**
            转到上一级挂载文的件系统
        **/
        
        strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
        printk(PT_DEBUG,"%s:%d，path_buf[i]=%s \n\r",__FUNCTION__,__LINE__,path_buf[i]);
        parent_mnt  = dentry_mnt->mount_parent;
        dentry_node = dentry_mnt->mount_point;  /*mnt 文件系统挂载到那个目录mnt 挂载到mountpoint 之上*/
        dentry_mnt = parent_mnt;
        printk(PT_DEBUG,"%s:%d,dentry_node->file_name=%s\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
        if(dentry_mnt){
            
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
        }
        i++;
    }
    xos_unspinlock(&mnt_lock);
    /*
    if((dentry_node->parent_dentry == root_dentry) && (dentry_mnt == root_mnt))
    {
        
        strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
        i++;
    }
    */
    strcpy(path_buf[i],(const char*)("/"));
    for(;i >=0 ;i--){
        strcat(path_name, path_buf[i]);
        if(strcmp(path_buf[i],"/")){
            strcat(path_name, "/");
        }
    }
    
    printk(PT_DEBUG,"path_name = %s\n\r",path_name);


    return 0;
}




int sys_getcwd_avaliable(char *user_buf,int buf_size)
{
    int i = 0;
    char path_name[512] = {0};
    char path_buf[12][128] = {0};
    struct xos_dentry  *dentry_node;
//    xmount_t *parent_mnt;
    xmount_t  *dentry_mnt;
    lookup_path_t  path;

    struct xos_dentry  *root_dentry;
    xmount_t  *root_mnt;
    struct task_struct *cur_task = get_current_task();
    dentry_node = cur_task->fs_context.curr_dentry;
    dentry_mnt = cur_task->fs_context.curr_dentry_mount;

    path.look_dentry = cur_task->fs_context.curr_dentry;
    path.look_mnt = cur_task->fs_context.curr_dentry_mount;
    root_dentry = cur_task->fs_context.root_dentry;
    root_mnt = cur_task->fs_context.root_dentry_mount;
    printk(PT_DEBUG,"dentry_node,cur_path = %s\n\r",dentry_node->file_name.path_name);
    printk(PT_DEBUG,"dentry_mnt->root_dentry->file_name.path_name = %s\n\r",dentry_mnt->root_dentry->file_name.path_name);
    
    printk(PT_DEBUG,"root_dentry->file_name = %s\n\r",root_dentry->file_name.path_name);
    printk(PT_DEBUG,"root_mnt.mount_point = %s\n\r",root_mnt->mount_point->file_name.path_name);
    xos_spinlock(&mnt_lock);
    strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
    strcpy(path_buf[i],(const char*)(path.f_tmp_dentry->file_name.path_name));
    i++;
    while((path.f_tmp_dentry != root_dentry) ||(path.f_tmp_mnt != root_mnt)){
        
        printk(PT_DEBUG,"%s: %d,dentry_node->root_path=%s\n\r",__FUNCTION__,__LINE__,dentry_node->file_name.path_name);
        printk(PT_DEBUG,"%s: %d,dentry_mnt->root_dentry.file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,dentry_mnt->root_dentry->file_name.path_name);
        switch_to_parent_dir(&path);
        strcpy(path_buf[i],(const char*)(dentry_node->file_name.path_name));
        i++;
    }
    xos_unspinlock(&mnt_lock);

    for(;i >=0 ;i--){
        strcat(path_name, path_buf[i]);
        if(strcmp(path_buf[i],"/")){
            strcat(path_name, "/");
        }
    }
    
    printk(PT_DEBUG,"path_name = %s\n\r",path_name);

    return 0;
}
#endif
