#ifndef __XOS_DEV_H__
#define __XOS_DEV_H__

#include "types.h"
typedef int devno_t;
struct device_desc;
struct driver_desc;


struct attr_group{
    
};
/*
    抽象出一个公共的设备基类
    struct xos_bus_desc  总线基类
    struct device_desc   设备基类描述符
    struct 
*/

 struct xos_bus_desc{
    int             bus_status;
    xos_spinlock_t  bus_lock;
    struct attr_group  *bus_group;
    const char      *dev_name;
    char            *bus_name;
    dlist_t         device_list;
    dlist_t         driver_list;
    int (*bus_match)(struct device_desc *dev, struct driver_desc *drv);
    int (*bus_probe)(struct device_desc *dev);
    int (*bus_remove)(struct device_desc *dev);

};


typedef struct device_desc
{
    char *device_name;
    dlist_t list;                        /* 设备在驱动中的链表 */
    struct driver_desc *driver;      /* 设备所在的驱动 */
    uint16_t mtime;                     /* 设备修改时的时间 */
    uint16_t mdate;                     /* 设备修改时的日期 */
    mutext_t mutex_lock;            /* 设备自旋锁 */
    struct xos_bus_desc *bus;   /*挂在那个总线下*/
}xdevice_t;


typedef struct driver_desc
{
    char *driver_name;
    unsigned int flags;                
    dlist_t list;                      
    dlist_t device_list;                /*设备列表*/
    mutext_t mutex_lock;             /* 设备锁 */
    struct xos_bus_desc *bus;   /*挂在那个总线下*/
    int	(*drv_probe)(struct device_desc * dev);
    int	(*drv_move)(struct device_desc * dev);
    const struct of_device_id	*of_match_table;
} xdriver_t;





extern int xos_init_deivices();

#endif
