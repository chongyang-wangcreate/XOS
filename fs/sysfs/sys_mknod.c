/********************************************************
    
    development started: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2028 wangchongyang

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
#include "fs.h"
#include "xos_cache.h"
#include "mount.h"
#include "tick_timer.h"
#include "task.h"
#include "path.h"
#include "xdentry.h"



//#define AT_FDCWD  -100 /*参考自内核*/

/*
    user interface:
    int mknod(const char *pathname, mode_t mode, dev_t dev);

    user_path_parent(dfd, filename, &nd, &tmp);//查找路径中的最后一个项的父目录项
    dentry = lookup_create(&nd, 0);//创建需要创建的最后一项的目录项
    error = may_mknod(mode);//进行模式检查

    case S_IFCHR: case S_IFBLK:
			printk("S_IFCHR: case S_IFBLK.\n");
			error = vfs_mknod(nd.path.dentry->d_inode,dentry,mode,
			new_decode_dev(dev));
	error = dir->i_op->mknod(dir, dentry, mode, dev);//调用具体的mknod函数

	就调用相应文件系统的mknod函数了，前面我们知道/dev目录属于tmpfs文件系统，我们看在挂载tmpfs的时候，会调用它的get_sb函数 

	创建/dev目录时，这里对应 

	 case S_IFDIR:
			inc_nlink(inode);
			inode->i_size = 2 * BOGO_DIRENT_SIZE;
			inode->i_op = &shmem_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;
			break;


sys_mknod()和sys_mkdir()的代码，几乎相同。只不过sys_mkdir调用的是vfs_mkdir,而sys_mknod会调用vfs_create或者vfs_mknod.其实ramfs_mkdir也会调用ramfs_mknod，只不过参数不同而已。
另外创建目录”/dev”和创建设备文件”/dev/console”还有一个不同点，就是创建”/dev/console”时，因为”/dev”已经存在，所以user_path_parent()->do_path_lookup()->path_walk()->link_path_walk()->__link_path_walk()中的for循环会循环两次，
而之前创建”/dev”时，只执行了一次，就退出循环了。


执行的命令：mknod /dev/test_char c 128 11
mknod("/dev/test_char", S_IFCHR|0666, makedev(128, 11)) = 0
mknod对应的内核函数为sys_mknod

*/
typedef uint32_t mode_t;

int check_mknod(xdentry *son_node_dentry,xinode *parent_node,int mode,devno_t dev)
{
    int ret;
    if (son_node_dentry->file_node){
        
        ret = -EEXIST;
    }
    else if (!parent_node->inode_ops || !parent_node->inode_ops->mkdir){
        
        ret = -EPERM;
    }
    else  
    {   
        printk(PT_RUN,"%s:%d,!!!son_file_dentry=%s,len=%d\n\r",__FUNCTION__,__LINE__,son_node_dentry->file_name.path_name,son_node_dentry->file_name.len);
        ret = parent_node->inode_ops->mknod(parent_node, son_node_dentry, mode,dev);
    }

    return ret;
}

int do_mknod(int dir_fd, const char  *pathname, int mode,devno_t dev)
{
    int ret;
    lookup_path_t look_path;
    xinode *parent_node;
    xdentry *son_file_dentry;
    int flags = LOOKUP_PARENT;
    ret =  resolve_path((char*)pathname, flags,&look_path);
    
    printk(PT_RUN,"%s:%d,look_path->look_dentry.path_name=%s ,look_path->look_path.last_path=%s\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name,look_path.last_path.path_name);
    
    if (ret || (look_path.path_type != PATH_NORMAL)){
        
        goto resolve_path_fail;
    }
    
    /*
        find file in parent dir
    */
    look_path.flags &= ~LOOKUP_PARENT;
    son_file_dentry = lookup_dentry_in_pdentry(&look_path.last_path, look_path.look_dentry, NULL);
    if(!son_file_dentry){
        ret = -ENOMEM;
        goto lookup_dentry_faile;
    }
    parent_node = look_path.look_dentry->file_node;
    
    printk(PT_DEBUG,"%s:%d, look_path.look_dentry.name=%s parent_node_addr=%lx\n\r",__FUNCTION__,__LINE__,look_path.look_dentry->file_name.path_name,(unsigned long)parent_node);
    /*
        dentry cache 已经管理inode 节点
    */
    ret = check_mknod(son_file_dentry,parent_node,mode,dev);
    dput(son_file_dentry);
lookup_dentry_faile:

resolve_path_fail:

    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    dlist_t *cur_list;
    dlist_t *list_head;
    xdentry *cur_dentry;
    list_head = &look_path.look_dentry->children_list_head;
    list_for_each(cur_list, list_head){
            cur_dentry = list_entry(cur_list,xdentry,ch_at_parent_list);
            
            printk(PT_DEBUG,"%s:%d,do_mkdirat->dir=%s\n\r",__FUNCTION__,__LINE__,cur_dentry->file_name.path_name);
      }
	return ret;

}

int  sys_mknod(const char *pathname,mode_t mode,devno_t dev)
{
    /*
        resolve path, find  the node parent  dentry node
        user the parent fun make node and init node
        bind parent

        similar to sys_mkdir
    */
    #if 0
    if(dev == 0){
        /*
            系统申请空闲dev 号
        */
         alloc_chrdev_region(&dev, 0, 1,pathname);
    }
    #endif
    return do_mknod(AT_FDCWD, pathname, mode,dev);
}
