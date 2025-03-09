/********************************************************
    
    development Started:2024
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
#include "xos_fcntl.h"
#include "printk.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "setup_map.h"
#include "fs.h"
#include "mount.h"
#include "tick_timer.h"
#include "task.h"
#include "path.h"
#include "xdentry.h"
#include "xos_file.h"

/*
    Starting from simplicity
*/

enum{
    NO_CREATE,
    CREAT_FILE,

};
/*
    参考linux may_open
*/
static int may_open(lookup_path_t *look_path, int acc_mode,int flag)
{
    xinode *file_node = look_path->look_dentry->file_node;
    xmount_t *mnt = look_path->look_mnt;
    int ret = 0;


	if (!file_node)
		return -ENOENT;

	if (S_ISLNK(file_node->node_mode))
		return -ELOOP;
	
	if (S_ISDIR(file_node->node_mode)&& (flag & FMODE_WRITE))
		return -EISDIR;

	if (S_ISFIFO(file_node->node_mode) || S_ISSOCK(file_node->node_mode)) {
		flag &= ~O_TRUNC;
	} else if (S_ISBLK(file_node->node_mode) || S_ISCHR(file_node->node_mode)) {
		if (mnt->mnt_flags & MNT_NODEV)
			return -EACCES;
		flag &= ~O_TRUNC;
	} else if (IS_RDONLY(file_node) && (flag & FMODE_WRITE))
		return -EROFS;

	if (IS_APPEND(file_node)) {
		if  ((flag & FMODE_WRITE) && !(flag & O_APPEND))
			return -EPERM;
		if (flag & O_TRUNC)
			return -EPERM;
	}
    /*
	if (flag & O_NOATIME)
		if (current->fsuid != file_node->uid)
			return -EPERM;
    */
    return ret;
}

/*
    to do lock
*/
struct file *do_open_file(lookup_path_t *look, struct open_flags *op)
{
    xdentry *f_dentry_cache = look->look_dentry;
    xinode *fnode;
    struct file *new_file_ret;
    int error;
    printk(PT_RUN,"%s:%d,look->look_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,look->look_dentry->file_name.path_name);
    error = -ENFILE;
    /*
        alloc file
    */
    new_file_ret = xos_kmalloc(sizeof(struct file));
    if(!new_file_ret){
        goto  alloc_fail;
    }
    /*
        init file 
    */
    xos_spinlock(&inode_lock);
    new_file_ret->f_flags = op->open_flag;
    new_file_ret->f_mode = op->acc_mode;
    printk(PT_RUN,"WWWWWWWWWW %s:%d,op->acc_mode=%x\n\r",__FUNCTION__,__LINE__,op->acc_mode);
    fnode = f_dentry_cache->file_node;
    fnode->ref_count++;
    new_file_ret->f_dentry = f_dentry_cache;
    new_file_ret->f_ops = fnode->file_ops;
    xos_unspinlock(&inode_lock);
    if(!new_file_ret->f_ops){
        printk(PT_RUN,"%s:WWWWWWWWWW %d\n\r",__FUNCTION__,__LINE__);
    }
    /*
        call devfs open crash 
        Because the devfs_open function is undefined
    */
    if (new_file_ret->f_ops && new_file_ret->f_ops->open) {
            error = new_file_ret->f_ops->open(fnode,new_file_ret);
            if (error){
                goto cleanup;
            }
    }
    new_file_ret->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);

    return new_file_ret;

cleanup:
    
    new_file_ret->f_flags = 0;
    xos_kfree(new_file_ret);

alloc_fail:
    look->ref--;
    
	return NULL;
}

int  do_filp_open(lookup_path_t *path,struct open_flags *op)
{
    int fd = -1;
    int ret = -1;
    struct file *ret_file = NULL;
    ret_file = do_open_file(path, op);
    if(!ret_file){
        return fd;
    }
    fd = alloc_unused_fd(current_task);
    printk(PT_RUN,"%s:%d,fd=%d\n\r",__FUNCTION__,__LINE__,fd);
    if(fd < 0){
        return -1;
    }
    ret = file_fd_bind(ret_file,fd);
    if(!ret){
        return fd;
    }
    return fd;
}

/*
    walk path only
*/

int check_open_file( const char *pathname,lookup_path_t  *look_path,struct open_flags *op)
{
    int ret;
    int fd;
    printk(PT_RUN,"check open start %s:%d\n\r",__FUNCTION__,__LINE__);

    op->lookup_flags |= LOOKUP_FOLLOW;
    ret = resolve_path((char*)pathname, op->lookup_flags, look_path);
    if(ret < 0){
        printk(PT_DEBUG,"RETRETRETRET%s:%d\n\r",__FUNCTION__,__LINE__);
        return ret;
    }
    /*
        20250301 22:23  check add
    */
    if(look_path->path_type != PATH_NORMAL) //非正常文件
    {
        ret = -EISDIR;
        return ret ;
    }
    /*
        Attempt to open the file
    */
    ret = may_open(look_path, op->acc_mode,op->open_flag);
    
    if(!ret){

        fd = do_filp_open(look_path,op);
        return fd;

    }
    printk(PT_RUN,"may open fail %s:%d,ret=%d\n\r",__FUNCTION__,__LINE__,ret);
    return ret;
}

int create_open_file( const char *pathname,lookup_path_t  *look_path,struct open_flags *op)
{
    int fd;
    int ret;
    int mode = 0;
    xinode *parent_node;
    xdentry *son_file_dentry;
    /*
        find the parent directory first
        walk path create file node

        发现致命bug, 创建文件也是首先要想检查文件的父目录是否存在，你不确定输入的是否是
        有效目录
        所以它：
        op->lookup_flags |= LOOKUP_FOLLOW;
        ret = resolve_path((char*)pathname, op->lookup_flags, &look_path);
        改成：

        op->lookup_flags |= LOOKUP_PARENT;
        ret = resolve_path((char*)pathname, op->lookup_flags, &look_path);

    */
    op->lookup_flags |= LOOKUP_PARENT;
    ret = resolve_path((char*)pathname, op->lookup_flags, look_path);
    if(ret < 0){
        return ret;
    }
    printk(PT_DEBUG,"%s:%d,last_parent_name=%s\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);
    printk(PT_DEBUG,"%s:%d,inode_ops_addr=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)look_path->look_dentry->file_node->inode_ops);
    /*最后一个分量父目录路径类型*/
    if (look_path->path_type != PATH_NORMAL){
        
        ret = -EISDIR;
        goto reslove_path_fail;
    }

    parent_node = look_path->look_dentry->file_node;
    printk(PT_DEBUG,"%s:%d,parent_node_dentry=%s\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);

    /*
        获取最后一个路径分量对应的dentry
        /home/test/wcy
        获取的就是wcy对应的 dentry 描述符

        look_path->flags  需要做进一步判断
    */
    look_path->flags &= ~LOOKUP_PARENT;
    son_file_dentry = lookup_dentry_in_pdentry(&look_path->last_path, look_path->look_dentry, NULL);
    if(!son_file_dentry){
        ret = -ENOMEM;
        goto lookup_fail;
    }
    look_path->look_dentry = son_file_dentry;
    if (!son_file_dentry->file_node)
    {
        printk(PT_DEBUG,"%s:%d,parent_node->inode_ops_addr=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)parent_node->inode_ops);
        if(!parent_node->inode_ops->create){
            printk(PT_DEBUG,"%s:%d,SSSSSSSSSSSSSSSSS parent_name=%s\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);
        }
        if(parent_node->inode_ops->create){

            printk(PT_DEBUG,"%s:%d,parent_name=%s\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);
            printk(PT_DEBUG,"%s:%d,parent_node->inode_ops=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)parent_node->inode_ops);
            ret = parent_node->inode_ops->create(parent_node, son_file_dentry, mode, look_path);
            if(ret){
                goto  create_fail;
            }
            
            op->open_flag &= ~O_TRUNC;
            ret = may_open(look_path, op->acc_mode,op->open_flag);
            if(!ret){
            
                do_filp_open(look_path,op);
                
            }else{
                 goto  may_open_fail;
            }
        }

    }else{
        /*
            file already exist
        */
        printk(PT_DEBUG,"%s:%d,node->name=%s,file inode alread exist\n\r",__FUNCTION__,__LINE__,look_path->look_dentry->file_name.path_name);
        fd = do_filp_open(look_path,op);
        if(fd < 0){
            goto do_filp_open_fail;
        }
    }
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    dput(son_file_dentry);
    
do_filp_open_fail:

may_open_fail:
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

create_fail:

lookup_fail:
reslove_path_fail:
    return fd;
}

/*
    2024.12.26 21:37
    后续注释全部使用英文
*/
/*
    2024.12.27 21:48
    int fd = build_open_flags(flags, mode, &op);
    tmp = getname(filename);
    fd = get_unused_fd_flags(flags);
    struct file *f = vfs_open(dfd, tmp, &op);
    fd_install(fd, f);

    1.如果没有定义O_CREAT标志.只要查找文件系统中结点是否存在就可以了
    
    2.如果定义了O_CREAT标志.则先查找父结点,判断查找是否成功
      到父目录中查找是否有该结点.如果没有该结点就会创建相应的dentry但dentry->d_inode为空
      dentry->d_inode为空.说明这个结点是新建的,创建新节点，增加节点引用计数


    3.结点原本就存在的情况，则返回节点已存在错误

    4.如果是挂载目录.则跳转到挂载文件系统的根目录
    if (d_mountpoint(dentry)) {
        error = -ELOOP;

        if (flag & O_NOFOLLOW)

        goto exit_dput;

        while (__follow_down(&nd->mnt,&dentry) && d_mountpoint(dentry));

        }
   5.
        如果结点是一个符号链接

        if (dentry->d_inode->i_op && dentry->d_inode->i_op->follow_link)

        goto do_link;

        dput(nd->dentry);

        nd->dentry = dentry;

        error = -EISDIR;

   6.如果结点是一个目录，出错退出
    if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode))

    goto exit;


    Negative dentry, just create the file 
*/

int vfs_open(int dfd, const char *pathname, struct open_flags *op)
{
    int fd = 0;

    lookup_path_t  look_path;

    if(!(op->open_flag & O_CREAT)){
        /*
            walk path only
        */
        fd = check_open_file(pathname,&look_path,op);
        if(fd){
            return fd;
        }

    }else{
         look_path.mode = 0;
         fd = create_open_file(pathname, &look_path,op);
         return fd;
    }
    return fd;
}


int  xos_vfs_open(char *path_name,int flags, int mode)
{
    int fd = 0;
    int dfd = 0;;
    int acc_mode = 0;
    int lookup_flags = 0;
    struct open_flags op;
    acc_mode = ACC_MODE(flags);
    printk(PT_RUN,"%s:%d,acc_mode=%x,\n\r",__FUNCTION__,__LINE__,acc_mode);
    if (flags & O_DIRECTORY)
        lookup_flags |= LOOKUP_DIRECTORY;
    if (!(flags & O_NOFOLLOW))
        lookup_flags |= LOOKUP_FOLLOW;
    
    op.lookup_flags = lookup_flags;
    op.open_flag = flags;
    op.mode = mode;
    op.acc_mode = acc_mode;
	fd = vfs_open(dfd, path_name, &op);
    return fd;

}
/*
    确定一下行为动作：
    1.文件存在时，创建文件如何操作，截断文件。
    2.文件不存在只打开文件，则打开失败。

    最关键的一部先walk path

    2024.12:27  21:36
    写着写着就偏离自己规划的实现和格式，开发过程中遇到问题,后续需要反复思考
    后续慢慢完善，慢慢解决问题，先简单实现open 操作
    
*/

int do_sys_open(char *path_name,int flags, int mode)
{
    int fd;
    printk(PT_RUN,"%s:%d,path_name=%s,flags=%d\n",__FUNCTION__,__LINE__,path_name,flags);

    fd = xos_vfs_open(path_name, flags,mode);
    if(fd < 0){
        return fd;
    }

    printk(PT_RUN,"%s:%d,path_name=%s,flags=%d\n",__FUNCTION__,__LINE__,path_name,flags);
    return fd;
}


int do_sys_dup(int old_fd)
{
    int new_fd = -1;
    struct file  *file_node  = get_file_by_fd(old_fd);
    new_fd = alloc_unused_fd(current_task);
    if(!file_node){
        return new_fd;
    }
    file_fd_bind(file_node, new_fd);
    return new_fd;
}


int sys_fchmodat(int dfd, const char  *filename, unsigned short mode)
{

    return 0;
}

/*
    int can_read_file(struct inode *inode) {
    return (inode->i_mode & S_IRUGO) || capable(CAP_DAC_READ_SEARCH);
}
 
// 检查当前进程是否可以写入文件
int can_write_file(struct inode *inode) {
    return (inode->i_mode & S_IWUGO) || capable(CAP_DAC_OVERRIDE);
}
 
// 检查当前进程是否可以执行文件
int can_execute_file(struct inode *inode) {
    return (inode->i_mode & S_IXUGO) || capable(CAP_DAC_READ_SEARCH);
}
*/


int check_perm(xinode * inode, struct file * file, int type){

    int error = 0;
    if (file->f_mode & FMODE_WRITE){
        if (!(inode->node_mode & S_IWUGO))
            goto Eaccess;
    }

    if (file->f_mode & FMODE_READ){
        if (!(inode->node_mode & S_IRUGO))
            goto Eaccess;
    }
    goto done;
Eaccess:
    
    error = -EINVAL;
    goto done;
done:

    return error;

}

void get_pg_index_page(struct address_space *file_space, unsigned long pg_index)
{
    

}


void pagecache_file_size_extended(xinode *inode, loff_t from, loff_t to){

    unsigned long pg_index;
    struct address_space *file_space = &inode->cache_space;
    pg_index = from >> PAGE_SHIFT;
    get_pg_index_page(file_space,pg_index);

}

void pagecache_truncate(xinode *inode, loff_t newsize){

        
}


int simple_setattr(xdentry *new_dentry, struct iattr *attr_block)
{
    xinode *cur_node = new_dentry->file_node;
    if (attr_block->ia_valid & ATTR_SIZE){

        if(attr_block->ia_size > cur_node->file_size){
            pagecache_file_size_extended(cur_node, cur_node->file_size, attr_block->ia_size);
        }
        pagecache_truncate(cur_node, cur_node->file_size);
    }
    if (attr_block->ia_valid & ATTR_MODE)
        cur_node->node_mode = attr_block->ia_mode;

    return 0;
}
int  set_file_attr(int fd,int mode)
{
    int error;
    struct iattr newattrs;
    struct file *filp = get_file_by_fd(fd);
    xdentry *new_dentry = filp->f_dentry;
    xinode *cur_inode = filp->f_dentry->file_node;

    newattrs.ia_mode = (mode & S_IALLUGO) || (filp->f_dentry->file_node->node_mode & ~S_IALLUGO);
    newattrs.ia_valid = ATTR_MODE| ATTR_CTIME;
    
    if (cur_inode->inode_ops->setattr)
        error = cur_inode->inode_ops->setattr(new_dentry, &newattrs);
    else{
        
        error = simple_setattr(new_dentry, &newattrs);
    }

    return error;
}

int do_sys_chmod(const char *path_name,int mode)
{
    int fd;
    fd = xos_vfs_open((char*)path_name, O_RDONLY,mode);
    return set_file_attr(fd,mode);
}
