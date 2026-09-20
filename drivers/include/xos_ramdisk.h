#ifndef __XOS_RAMDISK_H__
#define __XOS_RAMDISK_H__

#include "types.h"
#include "xos_block_dev.h"

/*
    RAM-backed block device (ramdisk).

    Allocates a contiguous region of physical pages at init time
    and provides sector-level read/write/flush against it.
    Used to validate the block device subsystem without real hardware.

    Default: 4 MiB capacity, 512-byte sectors = 8192 sectors.
    Capacity is configurable via XOS_RAMDISK_SIZE_BYTES.
*/

#define XOS_RAMDISK_SECTOR_SIZE  512U
/* 4 MiB default */
#define XOS_RAMDISK_SIZE_BYTES   (4U * 1024U * 1024U)
#define XOS_RAMDISK_SECTOR_COUNT (XOS_RAMDISK_SIZE_BYTES / XOS_RAMDISK_SECTOR_SIZE)

/* major 254 — Linux convention for "experimental/local use" block majors */
#define XOS_RAMDISK_DEV_MAJOR   254
#define XOS_RAMDISK_DEV_MINOR   0

int xos_ramdisk_init(void);
void xos_ramdisk_exit(void);

#endif
