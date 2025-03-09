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
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "printk.h"
#include "xos_kern_def.h"
#include "xos_cache.h"
#include "xos_mutex.h"

#define MAX_MAJOR  256
#define DEVICE_NR  32
dlist_t   chrdev_list_head;
mutext_t  g_char_mutex;

/*
    cdev 结构代表的是字符设备的抽象，它提供了字符设备操作（如 open、read、write）的接口和操作函数。
    而 struct device 主要用于表示硬件设备的抽象，包含设备的物理属性、资源和驱动信息。
    两者分别处理设备的不同层面，因此不直接嵌套。

    struct device 关注设备的硬件资源、驱动程序以及设备的生命周期管理。
    而 cdev 只是描述字符设备与用户空间交互的接口，它通常和 struct file_operations 配合使用，
    更多的是软件层面的抽象。


    cdev 是字符设备模型的核心结构，它与硬件设备无关，可以作为独立的字符设备抽象存在。
    而 struct device 则是更广泛的设备管理框架的一部分，它不仅适用于字符设备，
    也可以用于块设备、网络设备等。因此，cdev 更加简洁，便于扩展和定制。
*/
//xos_spinlock_t xos_cdev_lock;

xcdev_t* xcdev_loopup(devno_t      devno, int *index);

/*
    如何关联次设备号呢？如何设计呢？
    主设备标识的 那类备
    次设备号，那个设备

    本系统暂无可固定的设备号

    次设备号动态设备，一次最少申请4个
*/

typedef struct char_dev_desc{
    dlist_t   *chrdev_list_head[256]; // 同一个主设备号使用链表关联起来
    int       ref_count[256];           //当前支持次设备号数量
    char      chr_dev_name[256];
    xos_spinlock_t chrdev_lock;
 }char_dev_desc_t;


char_dev_desc_t  g_chrdev_val;


/*
    字符设备的通用open 接口，驱动设备的open ,read,write 并没有和文件
    file 绑定，当前comm_chardev_open 主要工作就是绑定字符设备和文件

    file  attach  dentry  
    dentry attach xinode 
    xinode attach chr_dev_driver

    开发并不完善，还有许多工作要做
*/

int comm_chardev_open(struct xos_inode *file_node, struct file *filp)
{
    int ret = 0;
    int count;
    xcdev_t  *cdev = file_node->chardev;
    if(!cdev){
        /*
            设备文件未绑定
        */
        printk(PT_RUN,"%s:%d,file_node->devno=%d\n\r",__FUNCTION__,__LINE__,file_node->devno);
        file_node->chardev = xcdev_loopup(file_node->devno, &count);
        cdev = file_node->chardev;
        if(!cdev){
            ret = -ENXIO;
            return ret;
        }else{

            list_add_back(&file_node->device_list, &cdev->char_dev_list);
        }
    }

    filp->f_ops = cdev->char_ops;
    if (!filp->f_ops) {
        
        /*
            释放字符设备统计计数
        */
        if(cdev->ref_count == 0){
            /*
                暂时不做操作
            */
        }
        cdev->ref_count--;
        return -ENXIO;
    }

    /*
        调用驱动open 操作函数
        写的比较简陋，后续慢慢完善,现在还是一颗树苗

    */
    if(filp->f_ops->open){
        mutext_lock(&g_char_mutex);
        ret = filp->f_ops->open(file_node, filp);
        mutext_unlock(&g_char_mutex);
    }
    return ret;
}


xfile_ops_t comm_chrdev_ops ={
    .open = comm_chardev_open,

};


/*
    第一个分配完毕之后，新增设备超过最大设备数量，还需要重新分配

    后续再处理这个情况
*/

int alloc_chrdev_region(devno_t *dev, unsigned baseminor, unsigned count,
			const char *name)
{
   
    int i;
    int j = 0;
    xos_spinlock(&g_chrdev_val.chrdev_lock);
    for(i = 0;i < MAX_MAJOR;i++){
        if(!g_chrdev_val.chrdev_list_head[i]){
            break;
        }
    }
    if(i != MAX_MAJOR){
        if(count < DEVICE_NR){
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            g_chrdev_val.chrdev_list_head[i] = xos_kmalloc(DEVICE_NR*sizeof(dlist_t));
            for(j =0; j < DEVICE_NR;j++){
                 list_init(&g_chrdev_val.chrdev_list_head[i][j]);
            }
           
            g_chrdev_val.ref_count[i] = DEVICE_NR;
        }else{
             g_chrdev_val.chrdev_list_head[i] = xos_kmalloc(count*sizeof(dlist_t));
             g_chrdev_val.ref_count[i] = count;
        }
        
    }else{
         xos_unspinlock(&g_chrdev_val.chrdev_lock);
        return -1;
    }
    *dev = i << 20 | baseminor;
    xos_unspinlock(&g_chrdev_val.chrdev_lock);
   return 0;
}


int register_chrdev_region(devno_t devno,  unsigned count,const char *name)
{
   
    int major;
    int minor;
    int j;
    major = devno >> 20;
    if(major > MAX_MAJOR){
        return -1;
    }
    minor = devno &(0xfffff);
    xos_spinlock(&g_chrdev_val.chrdev_lock);
    if(count < DEVICE_NR){
        printk(PT_DEBUG,"%s:%d,minor=%d\n\r",__FUNCTION__,__LINE__,minor);
        g_chrdev_val.chrdev_list_head[major] = xos_kmalloc(DEVICE_NR*sizeof(dlist_t));
        g_chrdev_val.ref_count[major] = DEVICE_NR;
            for(j =0; j < DEVICE_NR;j++){
                 list_init(&g_chrdev_val.chrdev_list_head[major][j]);
            }
    }else{
         printk(PT_DEBUG,"%s:%d,minor=%d\n\r",__FUNCTION__,__LINE__,minor);
         g_chrdev_val.chrdev_list_head[major] = xos_kmalloc(minor*sizeof(dlist_t));
         g_chrdev_val.ref_count[major] = count;
    }

   xos_unspinlock(&g_chrdev_val.chrdev_lock);
   return 0;
}



void xcdev_init(xcdev_t *cdev, struct xos_file_ops *fops)
{
    cdev->char_ops = fops;
    list_init(&cdev->char_dev_list);
    
}

void xcdev_add(xcdev_t *cdev, devno_t devno, unsigned count)
{
    dlist_t *chrdev_list_head;
    int major,minor;
    major = devno >> 20;
    minor = devno &(0xfffff);
    cdev->devnum = devno;
    xos_spinlock(&g_chrdev_val.chrdev_lock);
    chrdev_list_head = &g_chrdev_val.chrdev_list_head[major][minor];
    printk(PT_DEBUG,"%s:%d,minor=%d,cdev->devnum=%d\n\r",__FUNCTION__,__LINE__,minor,cdev->devnum);
    list_add_back(&cdev->char_dev_list, chrdev_list_head);
    xos_unspinlock(&g_chrdev_val.chrdev_lock);
    printk(PT_DEBUG,"%s:%d,minor=%d\n\r",__FUNCTION__,__LINE__,minor);
}

/*
    通过设备号获取存储的cdev 结构
*/
xcdev_t* xcdev_loopup(devno_t      devno, int *index)
{
    int major,minor;
    xcdev_t *cdev_cur;
    dlist_t *chrdev_list_head;
    dlist_t *chrdev_cur;
    major = devno >> 20;
    minor = devno &(0xfffff);
    xos_spinlock(&g_chrdev_val.chrdev_lock);
    chrdev_list_head = &g_chrdev_val.chrdev_list_head[major][minor];
    printk(PT_RUN,"%s:%d,major=%d,minor=%d,chrdev_list_head=%lx\n\r",__FUNCTION__,__LINE__,major,minor,(unsigned long)chrdev_list_head);

    list_for_each(chrdev_cur, chrdev_list_head){
        cdev_cur = list_entry(chrdev_cur, xcdev_t, char_dev_list);
        printk(PT_RUN,"%s:%d,cdev_cur->devnum=%d\n\r",__FUNCTION__,__LINE__,cdev_cur->devnum);
        if(cdev_cur->devnum == devno){
                /*
                    found it
                */
                 printk(PT_RUN,"%s:%d,AAAAAAAAAAAAAAAAdevno=%d\n\r",__FUNCTION__,__LINE__,devno);
                index = 0;
                xos_unspinlock(&g_chrdev_val.chrdev_lock);
                return cdev_cur;
            }
    }
    xos_unspinlock(&g_chrdev_val.chrdev_lock);
    return NULL;
    
}
void xcdev_del(xcdev_t *cdev)
{
    int major;
    major = cdev->devnum >> 20;
    xos_spinlock(&g_chrdev_val.chrdev_lock);
    g_chrdev_val.ref_count[major] --;
    if( g_chrdev_val.ref_count[major] == 0){
        xos_kfree(g_chrdev_val.chrdev_list_head[major]);
    }
    list_del(&cdev->char_dev_list);
    xos_unspinlock(&g_chrdev_val.chrdev_lock);
}


/*
    总舵主哈哈
*/
void chrdev_init()
{
    int i;
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    for(i = 0; i < MAX_MAJOR;i++){
        
        g_chrdev_val.chrdev_list_head[i] = NULL;
    }
    xos_spinlock_init(&g_chrdev_val.chrdev_lock);
    mutext_init(&g_char_mutex);
}
