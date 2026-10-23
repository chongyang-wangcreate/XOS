#ifndef __XOS_EXT4_H__
#define __XOS_EXT4_H__

#include "types.h"

#define XOS_EXT4_SUPERBLOCK_OFFSET 1024U
#define XOS_EXT4_SUPER_MAGIC       0xef53U

struct xos_block_device;

/* The block layer owns all sector translation used by ext4. */
int xos_ext4_read_superblock(struct xos_block_device *bdev,
                             void *buffer,
                             uint32 buffer_size);

#endif
