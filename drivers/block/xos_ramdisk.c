/********************************************************
    development timer:2026.9
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2026-2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    RAM-backed block device driver.

    Allocates physical pages for backing store, registers a block
    device via xos_blockdev_add_disk(), and provides sector read/write
    operations that simply memcpy to/from the RAM backing store.

    This driver is the first real consumer of the block device layer
    and exercises: register -> add_disk -> partition scan -> read/write.

********************************************************/

#include "types.h"
#include "string.h"
#include "error.h"
#include "spinlock.h"
#include "printk.h"
#include "xos_page.h"
#include "xos_cache.h"
#include "xos_block_dev.h"
#include "xos_ramdisk.h"

