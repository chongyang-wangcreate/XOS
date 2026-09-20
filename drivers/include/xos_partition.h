#ifndef __XOS_PARTITION_H__
#define __XOS_PARTITION_H__

#include "types.h"
#include "xos_block_dev.h"

#define XOS_PARTITION_MAX 16
#define XOS_MBR_PRIMARY_PARTITIONS 4

int xos_partition_init(void);
int xos_mbr_scan(xos_block_device_t *parent);
int xos_gpt_scan(xos_block_device_t *parent);
int xos_partition_scan(xos_block_device_t *parent);
int xos_partition_count(void);
int xos_partition_remove(xos_block_device_t *parent);
void xos_partition_dump(xos_block_device_t *parent);

#endif