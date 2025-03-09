/********************************************************
    
    development started: 2025.1
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 - 2030 wangchongyang

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
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_dev.h"
#include "xos_bus.h"

struct xos_bus_desc  arch64_bus;
struct xos_bus_desc  platform_bus;
struct xos_bus_desc  i2c_bus;

int xos_bus_register(struct xos_bus_desc * bus){
    list_init(&bus->device_list);
    list_init(&bus->driver_list);

    return 0;
}
void xos_bus_genrial_probe()
{

}




int device_driver_match(xdevice_t *dev,xdriver_t *driver)
{
    int ret;
//    strcmp(dev->device_name, driver->driver_name)
    if(!driver->bus->bus_match){
        return -1;
    }
    ret = driver->bus->bus_match(dev,driver);
    if(ret < 0){
        return -1;
    }
    /*
        调用probe 函数初始化设备
        字符设备可能还包括cdve 相应操作
    */
    dev->driver = driver;   /*dev  driver  attach*/
    if(dev->bus->bus_probe){
       ret = dev->bus->bus_probe(dev); /*入参dev 初始化设备*/
       if(ret){
            return -1;
       }
        
    }else if(driver->drv_probe){
       ret =  driver->drv_probe(dev);
       if(ret){
            return -1;
       }
    }
    return 0;
}


/*
    return 0 not find
    return 1 find
*/
int xos_device_match(xdevice_t *dev)
{
    int ret;
    struct xos_bus_desc * bus = dev->bus;
    dlist_t *cur_node;
    dlist_t *driver_list = &bus->driver_list;
    xdriver_t *driver_node;

    if(!bus->bus_match){
        return -1;
    }
    list_for_each(cur_node, driver_list){
        driver_node = list_entry(cur_node, xdriver_t, list);
        ret = device_driver_match(dev,driver_node);
        return ret;
    }
    return 0;
}

int xos_driver_match(xdriver_t *drv){
    int ret;
    struct xos_bus_desc * bus = drv->bus;
    dlist_t *cur_node;
    dlist_t *device_list = &bus->driver_list;
    xdevice_t *driver_node;
    if(!bus->bus_match){
        return -1;
    }
    list_for_each(cur_node, device_list){
        driver_node = list_entry(cur_node, xdevice_t, list);
        ret = device_driver_match(driver_node,drv);
        return ret;
    }
    return 0;
}


int xos_bus_insert_device(xdevice_t *dev)
{
    int ret;
    if(!dev || !dev->bus){
        return -1;
    }
    list_add_front(&dev->list, &dev->bus->device_list);
    ret = xos_device_match(dev);
    return ret;
}


int xos_bus_insert_driver(xdriver_t *drv)
{
   /*
        确定当前驱动和那个总线绑定，找到对应的总线，然后
        将驱动节点插入总线驱动链表
    */
    int ret;
    if(!drv || !drv->bus){
        return -1;
    }
    list_add_front(&drv->list,  &drv->bus->driver_list);
    ret = xos_driver_match(drv);
    return ret;
}

int xos_bus_match(struct device_desc *dev, struct driver_desc *drv)
{

    return 0;
}


void xos_bus_init()
{
    struct xos_bus_desc *bus = &arch64_bus;
    list_init(&bus->device_list);
    list_init(&bus->driver_list);
    platform_bus_init(get_platform_bus());
}

