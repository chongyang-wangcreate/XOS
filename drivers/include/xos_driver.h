#ifndef __DRIVER_H__
#define __DRIVER_H__

/*
    设备基类
    字符设备描述和块设备描述不相同
*/

#if 0
typedef struct driver_desc
{
    char driver_name[128];
    unsigned int flags;                /*driver 标志*/
    unsigned int status;               /*driver 状态*/
    dlist_t list;                      
    dlist_t device_list;                /*关联的设备列表*/
    xos_spinlock_t device_lock;             /* 设备锁 */
    struct xos_bus_desc		*bus;         /*挂在那个总线下*/
} driver_t;

#endif
extern int driver_register(struct  driver_desc *drv);

extern int driver_unregister(struct         driver_desc *drv);


#endif
