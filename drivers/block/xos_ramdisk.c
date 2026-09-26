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

static uint8 xos_ramdisk_storage[XOS_RAMDISK_SIZE_BYTES];
static xos_blkdev_t xos_ramdisk_dev;
static int xos_ramdisk_ready;

static int xos_ramdisk_read_sectors(xos_blkdev_t *bdev,
                                     uint64 sector,
                                     void *buffer,
                                     uint32 sector_count)
{
    uint64 offset;
    uint64 length;

    if (bdev == NULL || buffer == NULL || bdev->private_data == NULL) {
        return -EINVAL;
    }

    offset = sector * (uint64)bdev->sector_size;
    length = (uint64)sector_count * (uint64)bdev->sector_size;

    if (offset + length > XOS_RAMDISK_SIZE_BYTES) {
        return -EINVAL;
    }

    memcpy(buffer, (uint8 *)bdev->private_data + offset, length);
    return 0;
}

static int xos_ramdisk_write_sectors(xos_blkdev_t *bdev,
                                      uint64 sector,
                                      const void *buffer,
                                      uint32 sector_count)
{
    uint64 offset;
    uint64 length;

    if (bdev == NULL || buffer == NULL || bdev->private_data == NULL) {
        return -EINVAL;
    }

    offset = sector * (uint64)bdev->sector_size;
    length = (uint64)sector_count * (uint64)bdev->sector_size;

    if (offset + length > XOS_RAMDISK_SIZE_BYTES) {
        return -EINVAL;
    }

    memcpy((uint8 *)bdev->private_data + offset, buffer, length);
    return 0;
}

static int xos_ramdisk_flush(xos_blkdev_t *bdev)
{
    if (bdev == NULL) {
        return -EINVAL;
    }
    return 0;
}

static const xos_blkdev_ops_t xos_ramdisk_ops = {
    .read_sectors = xos_ramdisk_read_sectors,
    .write_sectors = xos_ramdisk_write_sectors,
    .flush = xos_ramdisk_flush,
};

int xos_ramdisk_init(void)
{
    int ret;

    if (xos_ramdisk_ready) {
        return 0;
    }

    memset(xos_ramdisk_storage, 0, sizeof(xos_ramdisk_storage));
    memset(&xos_ramdisk_dev, 0, sizeof(xos_ramdisk_dev));

    strcpy(xos_ramdisk_dev.name, "ram0");
    xos_ramdisk_dev.devno = XOS_RAMDISK_DEV_MAJOR;
    xos_ramdisk_dev.sector_size = XOS_RAMDISK_SECTOR_SIZE;
    xos_ramdisk_dev.sector_count = XOS_RAMDISK_SECTOR_COUNT;
    xos_ramdisk_dev.flags = 0;
    xos_ramdisk_dev.private_data = xos_ramdisk_storage;
    xos_ramdisk_dev.ops = &xos_ramdisk_ops;

    ret = xos_blockdev_add_disk(&xos_ramdisk_dev);
    if (ret < 0) {
        printk(PT_ERROR, "ramdisk: add_disk failed (ret=%d)\n", ret);
        return ret;
    }

    xos_ramdisk_ready = 1;
    printk(PT_DEBUG,
           "ramdisk: registered ram0, %u sectors, %u bytes/sector\n",
           XOS_RAMDISK_SECTOR_COUNT, XOS_RAMDISK_SECTOR_SIZE);
    return 0;
}

void xos_ramdisk_exit(void)
{
    if (!xos_ramdisk_ready) {
        return;
    }

    xos_blockdev_del_disk(&xos_ramdisk_dev);
    memset(xos_ramdisk_storage, 0, sizeof(xos_ramdisk_storage));
    memset(&xos_ramdisk_dev, 0, sizeof(xos_ramdisk_dev));
    xos_ramdisk_ready = 0;
}
