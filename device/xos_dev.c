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

extern int console_init();


int dev_insert(xdevice_t * dev)
{
//    struct xos_bus_desc  *bus = dev->bus;
    

    
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


