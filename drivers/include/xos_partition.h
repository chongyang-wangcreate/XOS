#ifndef __XOS_PARTITION_H__
#define __XOS_PARTITION_H__

#include "types.h"
#include "xos_block_dev.h"

#ifndef XOS_PARTITION_MAX
#define XOS_PARTITION_MAX 16
#endif

#ifndef XOS_MBR_PRIMARY_PARTITIONS
#define XOS_MBR_PRIMARY_PARTITIONS 4
#endif

#ifndef PARTITION_NAME_LEN
#define PARTITION_NAME_LEN XOS_BLOCKDEV_NAME_MAX
#endif

/*
 * Logical block-device object backed by a sector range in its parent disk.
 * Statistics and partition-table metadata belong here, not in the base class.
 */
typedef struct xos_partition {
    xos_block_device_t block_device;
    xos_block_device_t *parent;
    dlist_t disk_list;

    uint64 start_sector;
    uint64 read_times;
    uint64 read_sectors;
    uint64 write_times;
    uint64 write_sectors;

    unsigned long state;
    uint8 type;
    uint8 index;
    int in_use;
} xos_partition_t;

static inline xos_block_device_t *
xos_partition_to_block(xos_partition_t *partition)
{
    return partition == NULL ? NULL : &partition->block_device;
}

int xos_partition_init(void);
int xos_mbr_scan(xos_block_device_t *parent);
int xos_gpt_scan(xos_block_device_t *parent);
int xos_partition_scan(xos_block_device_t *parent);
int xos_partition_count(void);
int xos_partition_remove(xos_block_device_t *parent);
void xos_partition_dump_show(xos_block_device_t *parent);

#endif
