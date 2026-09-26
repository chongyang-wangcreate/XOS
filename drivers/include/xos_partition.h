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

#ifndef DISK_MAX_PARTS
#define DISK_MAX_PARTS			256
#endif
#ifndef DISK_NAME_LEN
#define DISK_NAME_LEN			32
#endif
#define PARTATION_NAME_LEN			32

int xos_partition_init(void);
int xos_mbr_scan(xos_blkdev_t *parent);
int xos_gpt_scan(xos_blkdev_t *parent);
int xos_partition_scan(xos_blkdev_t *parent);
int xos_partition_count(void);
int xos_partition_remove(xos_blkdev_t *parent);
void xos_partition_dump(xos_blkdev_t *parent);





#endif