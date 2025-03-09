/********************************************************
    
    development timer:2025
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 ~ 2027 wangchongyang

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   "When using, modifying, or distributing this code, 
   appropriate credit must be given to the original author. 
   You must include an acknowledgment of the original author in all copies or substantial portions of the software.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

********************************************************/

#include "types.h"
#include "string.h"
#include "mem_layout.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "kernel_types.h"
#include "setup_map.h"
#include "interrupt.h"
#include "arch64_timer.h"
#include "cpu_desc.h"
#include "xos_bus.h"
#include "schedule.h"
#include "schedule.h"



int driver_register(struct       driver_desc *drv)
{
    list_init(&drv->list);

    xos_bus_insert_driver(drv);

    return 0;
}


int driver_unregister(struct       driver_desc *drv)
{
    list_del(&drv->list);
    xos_bus_insert_driver(drv);
    return 0;
}

