#ifndef __XOS_DISK_H__
#define __XOS_DISK_H__

#include "xos_block_dev.h"

#ifndef DISK_NAME_LEN
#define DISK_NAME_LEN XOS_BLOCKDEV_NAME_MAX
#endif

#ifndef DISK_MAX_PARTS
#define DISK_MAX_PARTS 256
#endif

/*
   重构结构体将原来，devno_t devno，uint32 sector_size，uint64 sector_count等成员抽象
   成一个公用的结构体xos_block_device_t block_device
 * Whole-disk object. block_device is the reusable base object while this
 * structure owns disk-specific identity and partition bookkeeping.
 */
typedef struct xos_disk_device {
    xos_block_device_t block_device;
    xdevice_t device;

    int max_partition_count;
    int device_sequence;
    int partition_count;
    uint64 request_count;
    dlist_t partition_list;

    void *private_data;
} xos_disk_dev_t;

static inline xos_block_device_t *xos_disk_to_block(xos_disk_dev_t *disk)
{
    return disk == NULL ? NULL : &disk->block_device;
}


#endif
