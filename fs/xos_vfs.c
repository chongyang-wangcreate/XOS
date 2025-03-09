
/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 - 2028 wangchongyang

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
#include "xos_cache.h"
#include "mount.h"
#include "xos_vfs.h"
#include "fs.h"
#include "xos_devfs.h"
#include "syscall.h"
#include "xdentry.h"
//#include "xos_fs_types.h"

extern void fs_list_init();


/*
    20240706 今天是开发操作系统的第9个月，如果算上实质开发准备时间到今天马上11个月了，时间太宝贵了，我几乎把空余时间全部利用起来了，
    虽然系统还是问题多多，遗留了很多问题，后续还需慢慢解决，但是今天值得记录一下，一个节点也是新的起点
    开启新征程文件系统
    这次要从0实现文件系统对自己来说会是极大的挑战，开发过程中会遇到很多问题，时间应该会很长，问题会很多，当然对我来说也没什么可怕的，最艰难时光都已经经历过了
    希望这次文件系统开发将自己技术水平提升一个小小的台阶,文件系统开发对我来说过程可能比较漫长，边学边思考边开发,多看csdn,知乎,linux 多向优秀的人学习，
    在开发文件系统期间，也可以完善xos 其它部分
    
    开始的数据结构定义是最重要也是最难部分

    2024.11.30. 多向优秀的人学习
    
*/
struct file *file_cache_pool = NULL;

void init_fs_lock()
{
    xos_spinlock_init(&mnt_lock);
    xos_spinlock_init(&inode_lock);
    xos_spinlock_init(&dentry_lock);
    xos_spinlock_init(&g_file_lock);
}
void init_file_cache()
{
    file_cache_pool = xos_kmalloc(sizeof(struct file)*512);
}

void  init_file_sys()
{
    init_ramfs();  /*初始化根文件系统到系统*/
    init_tmpfs();
    init_devfs();
}

int xos_vfs_init()
{
    init_fs_lock();
    init_file_cache();
    init_inode_cache();
    init_dentry_cache();
    init_mount_cache();
    fs_list_init();

    init_file_sys();

    mount_rootfs("rootfs",0,"/",NULL,NULL); /*挂载第一个文件系统*/
   

    return 0;
}

void xos_mount_fs()
{

    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    sys_mkdir("/dev", 0);
    sys_mkdir("/home", 0);
    sys_mkdir("/tmp", 0);
    sys_mkdir("/etc", 0);
    sys_mkdir("/home/wcy", 0);  /*wcy 最下层节点可能创建的存在问题*/
    sys_mkdir("/home/test", 0);
    sys_mkdir("/home/wcy1", 0);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);


    /*
        2024.12.28 17:03 mount 实现存在问题，正在找原因
        有可能锁的原因
    */
    do_mount("devfs",0, "/dev","/dev",NULL);
    do_mount("tmpfs",0,"/tmp","/tmp",NULL); /*挂载*/

    /*
        open 设备节点的时候，通过名字找到对应的设备号
    */
    sys_mknod("/dev/wcy",S_IFCHR,10);

}



