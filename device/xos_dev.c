/********************************************************
    
    development started:2024
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
#include "printk.h"
#include "xos_mutex.h"

#include "xos_cache.h"
#include "xos_kobj.h"
#include "xos_kern_def.h"
#include "xos_char_dev.h"
#include "xos_dev.h"
#include "xos_bus.h"

extern int console_init();
static dlist_t xos_device_register;
static int xos_device_register_ready;

static void xos_device_reigster_init(void)
{
    if(!xos_device_register_ready){
        list_init(&xos_device_register);
        xos_device_register_ready = 1;
    }
}

static int xos_device_add_to_list(xdevice_t *dev)
{
    if(!dev || !dev->device_name){
        return -1;
    }
    xos_device_reigster_init();
    list_init(&dev->register_list);
    list_add_front(&dev->register_list,&xos_device_register);
    return 0;
}

xdevice_t *dev_find_by_name(const char *name)
{
    dlist_t *head;
    dlist_t *cur;
    xdevice_t *dev;
    if(!name){
        return NULL;
    }
    xos_device_reigster_init();
    head = &xos_device_register;
    list_for_each(cur,head){
        dev = list_entry(cur,xdevice_t,register_list);
        if(dev->device_name && strcmp(dev->device_name,name) == 0){
            return dev;
        } 
    }
    return NULL;

}

int dev_insert(xdevice_t * dev)
{

    int ret;
    if(!dev){
        return -1;
    }
    ret = xos_device_add_to_list(dev);
    if(ret < 0){
        return ret;
    }
    if(!dev->bus){
        return 0;
    }
    return xos_bus_insert_device(dev);

    
    return 0;
}

int dev_unregister(xdevice_t *dev)
{
    if(!dev){
        return -1;
    }
    list_del(&dev->register_list);
    if(dev->bus){
        list_del(&dev->list);
    }
    return 0;
}
void dev_del()
{

}

void init_char_dev()
{
    chrdev_init();
    console_init();
}

void init_block_dev()
{

}

int xos_init_deivices()
{
    init_char_dev();
    init_block_dev();
    return 0;
}


