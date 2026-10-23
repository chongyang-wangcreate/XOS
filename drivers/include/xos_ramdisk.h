#ifndef __XOS_RAMDISK_H__
#define __XOS_RAMDISK_H__

#include "types.h"
#include "xos_block_dev.h"


#define XOS_RAMDISK_SECTOR_SIZE  4096U


#define XOS_RAMDISK_SIZE_BYTES   (4U * 1024U * 1024U)
#define XOS_RAMDISK_SECTOR_COUNT (XOS_RAMDISK_SIZE_BYTES / XOS_RAMDISK_SECTOR_SIZE)


#define XOS_RAMDISK_DEV_MAJOR   254
#define XOS_RAMDISK_DEV_MINOR   0

int xos_ramdisk_init(void);
void xos_ramdisk_exit(void);

#endif
