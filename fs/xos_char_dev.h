#ifndef __XOS_CHAR_DEV_H__
#define __XOS_CHAR_DEV_H__

#include "xos_dev.h"


typedef struct xos_char_dev{

    dlist_t char_dev_list;/*字符设备链表头*/
    xdriver_t  device;    /*设备基类*/
    struct xos_file_ops  *char_ops; /*操作函数集*/
    devno_t devnum;
    int ref_count;  //设备数量

}xcdev_t;

extern struct xos_file_ops comm_chrdev_ops;

extern void chrdev_init();
extern int register_chrdev_region(devno_t devno,  unsigned count,const char *name);
extern void xcdev_init(xcdev_t *cdev, struct xos_file_ops *fops);
extern void xcdev_add(xcdev_t *cdev, devno_t devno, unsigned count);
extern int alloc_chrdev_region(devno_t *dev, unsigned baseminor, unsigned count,
			const char *name);

#endif
