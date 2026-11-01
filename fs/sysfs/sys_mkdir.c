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
#include "uio.h"
//#include "fcntl.h"

#define AT_FDCWD  -100
#define XOS_MKDIR_PATH_MAX 4096U
/*
    给大家找了一篇不错的文章帮助理解
    https://blog.csdn.net/lkqboy2599/article/details/9978453
*/


/*
    p_dir 最后一个路径分量的父亲
*/
int lookup_dentry_in_pdentry(path_name_t *name, xdentry *p_dir,
                            lookup_path_t *look, xdentry **result)
{
    /*
        1. 先从缓存目录中查找问题
        2. 存在返回对应文件dentry
        3. 确认文件系统中是否存在，存在返回
        4. 不存在创建
    */
    xdentry *ret_dentry = NULL;
    lookup_path_t local_lookup = {0};
    if (result == NULL) {
        return -EINVAL;
    }
    *result = NULL;
    if (name == NULL || name->path_name == NULL || name->len == 0 ||
        p_dir == NULL) {
        return -EINVAL;
    }
    if (name->len >= XOS_DENTRY_NAME_MAX) {
        return -ENAMETOOLONG;
    }
    if (p_dir->file_node == NULL) {
        return -ENOENT;
    }
    if (!S_ISDIR(p_dir->file_node->node_mode)) {
        return -ENOTDIR;
    }
//    xos_spinlock(&dentry_lock);
    ret_dentry = find_in_pdentry_unlock(p_dir, name);  //find_in_dcache
    if(!ret_dentry){
        if (p_dir->file_node->inode_ops == NULL) {
            return -EOPNOTSUPP;
        }
        if(!p_dir->file_node->inode_ops->lookup){
            if (IS_RDONLY(p_dir->file_node)) {
                return -EROFS;
            }
            ret_dentry = xdentry_cache_alloc_init(p_dir,name);
        }else{  /*lookup*/
            printk(PT_RUN,"%s:%d,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,p_dir->file_name.path_name);
            if (look == NULL) {
                local_lookup.cur_path = *name;
                local_lookup.look_dentry = p_dir;
                look = &local_lookup;
            }
            ret_dentry = p_dir->file_node->inode_ops->lookup(p_dir->file_node, p_dir, look);
            if(!ret_dentry){
                if (IS_RDONLY(p_dir->file_node)) {
                    return -EROFS;
                }
                /*
                    1.ret_dentry = alloc_new_dentry();
                    2. init ret_dentry
                    3. bind parent_dentry
                    这样的话看着就比较散乱，不美观，还容易忘记初始化
                    所以分配新ret_dentry 的时候直接初始化，将分配和初始化放在一块
                */
                ret_dentry = xdentry_cache_alloc_init(p_dir,name);
                if (ret_dentry) {
                    printk(PT_DEBUG,"%s:%d,ret_dentry->name=%s,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,ret_dentry->file_name.path_name,p_dir->file_name.path_name);
                }
            }

        }

    }
//    xos_unspinlock(&dentry_lock);
    if (ret_dentry == NULL) {
        return -ENOMEM;
    }
    *result = ret_dentry;
    return 0;
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
    /* pathname 已由内核调用方提供，或由 sys_mkdir 复制到内核缓冲区。 */
    ret = resolve_path((char*)pathname, LOOKUP_PARENT, &look_path);
    if (ret < 0) {
        return ret; /*父目录不存在*/
    }
    /* mkdir 的目标若是根目录、. 或 ..，对应目录本身已经存在。 */
    if (look_path.path_type == PATH_ROOT) {
        return -EEXIST;
    }
    if (look_path.path_type != PATH_NORMAL){
        /* PATH_DOT、PATH_DOTDOT 也不是可创建的新目录名。 */
        return -EEXIST;
    }
    
    printk(PT_RUN,"%s:%d,look_path->look_dentry.path_name=%s ,look_path->look_path.last_path=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name,look_path.last_path.path_name);

    parent_node = look_path.look_dentry->file_node;
    if (!parent_node) {
        return -ENOENT;
    }
    if (!S_ISDIR(parent_node->node_mode)) {
        return -ENOTDIR;
    }
    if (IS_RDONLY(parent_node)) {
        return -EROFS;
    }
    if (!parent_node->inode_ops || !parent_node->inode_ops->mkdir) {
        return -EPERM;
    }
    /*
        find file in parent dir
    */
    printk(PT_RUN,"%s:%d,p_dir->name=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name);
    ret = lookup_dentry_in_pdentry(&look_path.last_path,
                                   look_path.look_dentry, NULL,
                                   &son_file_dentry);
    if (ret < 0) {
        return ret;
    }
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    ret = check_create_fdir_node(son_file_dentry,parent_node,mode);
    /* 缓存目录项由父目录持有；这里未取得额外引用，不能 dput。 */
    return ret;
}

/*
    专门为内核态封装的mkdir
*/
int xos_mkdir(const char *pathname, int mode)
{
    return do_mkdirat(AT_FDCWD, pathname, mode);
}

/*
    适配系统调用的mkdir ,需要copy_from_user
*/
int sys_mkdir(const char *pathname, int mode)
{
    char *kernel_path;
    uint32 length;
    int ret = -ENAMETOOLONG;

    if (pathname == NULL) {
        return -EFAULT;
    }
    kernel_path = xos_kmalloc(XOS_MKDIR_PATH_MAX + 1U);
    if (kernel_path == NULL) {
        return -ENOMEM;
    }
    /* 按字节拷贝，避免一次复制跨越未映射页或越过字符串结尾。 */
    for (length = 0; length <= XOS_MKDIR_PATH_MAX; length++) {
        if (copy_from_user(&kernel_path[length], pathname + length, 1) != 0) {
            ret = -EFAULT;
            break;
        }

        if (kernel_path[length] == '\0') {
            if (length == XOS_MKDIR_PATH_MAX) {
                ret = -ENAMETOOLONG;
            } else {
                ret = xos_mkdir(kernel_path, mode);
            }
            break;
        }
    }
    xos_kfree(kernel_path);
    return ret;
}

