/********************************************************
    
    development started: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2030 wangchongyang

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
//#include "fcntl.h"

#define AT_FDCWD  -100
/*
    给大家找了一篇不错的文章帮助理解
    https://blog.csdn.net/lkqboy2599/article/details/9978453
*/


/*
    p_dir 最后一个路径分量的父亲
*/
xdentry *lookup_dentry_in_pdentry(path_name_t *name, xdentry * p_dir,lookup_path_t *look)
{
    /*
        1. 先从缓存目录中查找问题
        2. 存在返回对应文件dentry
        3. 确认文件系统中是否存在，存在返回
        4. 不存在创建
    */
    xdentry *ret_dentry = NULL;
    xdentry tmp_dentry;
//    xos_spinlock(&dentry_lock);
    ret_dentry = find_in_pdentry_unlock(p_dir, name);  //find_in_dcache
    if(!ret_dentry){
        memcpy(&tmp_dentry.file_name,name,sizeof(path_name_t));
        if(!p_dir->file_node->inode_ops->lookup){
            ret_dentry = xdentry_cache_alloc_init(p_dir,name);
        }else{
            printk(PT_RUN,"%s:%d,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,p_dir->file_name.path_name);
            ret_dentry = p_dir->file_node->inode_ops->lookup(p_dir->file_node, &tmp_dentry, look);
            if(!ret_dentry){
                /*
                    1.ret_dentry = alloc_new_dentry();
                    2. init ret_dentry
                    3. bind parent_dentry
                    这样的话看着就比较散乱，不美观，还容易忘记初始化
                    所以分配新ret_dentry 的时候直接初始化，将分配和初始化放在一块
                */
                ret_dentry = xdentry_cache_alloc_init(p_dir,name);
                printk(PT_RUN,"%s:%d,ret_dentry->name=%s,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,ret_dentry->file_name.path_name,p_dir->file_name.path_name);
            }

        }

    }
//    xos_unspinlock(&dentry_lock);
    return ret_dentry;
}

int check_create_fdir_node(xdentry *son_node_dentry,xinode *parent_node,int mode)
{
    int ret = 0;
    
    if (son_node_dentry->file_node){
        ret = -EEXIST;
    }
    /**
     * 确认是否是目录
     */
    else if (!parent_node->inode_ops || !parent_node->inode_ops->mkdir){
        
        ret = -EPERM;
    }
    else  
    {   
        /*
            使用文件系统对应的操作函数
        */
        printk(PT_RUN,"%s:%d,!!!son_file_dentry=%s,len=%d\n\r",__FUNCTION__,__LINE__,son_node_dentry->file_name.path_name,son_node_dentry->file_name.len);
        ret = parent_node->inode_ops->mkdir(parent_node, son_node_dentry, mode);
    } 
    return ret;
}


/*
    mode 指示当前要创建目录

*/
int do_mkdirat(int dir_fd, const char  *pathname, int mode)
{
    int ret;
    lookup_path_t look_path = {0};
    xinode *parent_node;
    xdentry *son_file_dentry;
    int flags = LOOKUP_PARENT;
    /*
        缺少copy_from_user
    */
    ret = resolve_path((char*)pathname, flags,&look_path);
    if(ret == 0 && (look_path.path_type == PATH_ROOT)){
        goto root_finish;
    }else if(look_path.path_type != PATH_NORMAL){
        /*PATH_DOT, PATH_DOTDOT*/
        goto error_path_type;
    }
    
    printk(PT_RUN,"%s:%d,look_path->look_dentry.path_name=%s ,look_path->look_path.last_path=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name,look_path.last_path.path_name);

    if (ret == ENOENT){
        goto invalid_path; /*父目录不存在*/
    }
    /*
        find file in parent dir
    */
    look_path.flags &= ~LOOKUP_PARENT;
    
    printk(PT_RUN,"%s:%d,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name);
    son_file_dentry = lookup_dentry_in_pdentry(&look_path.last_path, look_path.look_dentry, NULL);
    if(!son_file_dentry){
        ret = -ENOMEM;
        goto no_mem; /*标签后续更改*/
    }
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    parent_node = look_path.look_dentry->file_node;
    ret = check_create_fdir_node(son_file_dentry,parent_node,mode);
    dput(son_file_dentry);

    return ret;//new add 20250301
root_finish:
    return ret;
    
no_mem:
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);

    dlist_t *cur_list;
    dlist_t *list_head;
    xdentry *cur_dentry;
    list_head = &look_path.look_dentry->children_list_head;
    list_for_each(cur_list, list_head){
            cur_dentry = list_entry(cur_list,xdentry,ch_at_parent_list);
            printk(PT_DEBUG,"%s:%d,do_mkdirat->dir=%s\n\r",__FUNCTION__,__LINE__,cur_dentry->file_name.path_name);
    }
    /*
        to do 
        release reference count 
        if(ref_count == 0){
            
        }
    */
    look_path.ref--;
    look_path.look_mnt->ref_count--;
    return ret;

invalid_path:
    /*
        to do 
        may be: mem free   ref_count sub
        
    */
error_path_type:
    return ret;
}

int sys_mkdir(const char  *pathname, int mode)
{
    /*
        1. copy_from_user
    */
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return do_mkdirat(AT_FDCWD, pathname, mode);
}

