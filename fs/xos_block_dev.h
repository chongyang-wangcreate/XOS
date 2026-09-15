#ifndef __XOS_BLOCK_DEV_H__
#define __XOS_BLOCK_DEV_H__

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_dev.h"

#define XOS_BLOCKDEV_NAME_MAX 32
#define XOS_BLOCKDEV_DEFAULT_SECTOR_SIZE 512U

#define XOS_BLOCKDEV_READ_ONLY (1U << 0)
#define XOS_BLOCKDEV_REMOVABLE (1U << 1)

struct xos_block_device;

typedef int (*xos_blockdev_read_fn)(struct xos_block_device *bdev,
                                    uint64  sector,
                                    void    *buffer,
                                    uint32  sector_count);
typedef int (*xos_blockdev_write_fn)(struct xos_block_device *bdev,
                                    const   void  *buffer,
                                    uint32 sector_count);
typedef int (*xos_blockdev_flush_fn)(struct xos_block_device *bdev);

typedef int (*xos_blockdev_flush_fn)(struct xos_block_device *bdev);

typedef struct xos_block_device_ops {
    xos_blockdev_read_fn read_sectors;
    xos_blockdev_write_fn write_sectors;
    xos_blockdev_flush_fn flush;
} xos_block_device_ops_t;

typedef struct xos_block_device {
    dlist_t list;
    xos_spinlock_t lock;
    xos_spinlock_t io_lock;
    char name[XOS_BLOCKDEV_NAME_MAX];
    devno_t devno;
    uint32 sector_size;
    uint64 sector_count;
    uint32 flags;
    uint32 ref_count;
    int registered;
    void *private_data;
    const xos_block_device_ops_t *ops;
} xos_block_device_t;

int xos_blockdev_init(void);
int xos_blockdev_register(xos_block_device_t *bdev);
int xos_blockdev_unregister(xos_block_device_t *bdev);

xos_block_device_t *xos_blockdev_get_by_name(const char *name);
xos_block_device_t *xos_blockdev_get_by_devno(devno_t devno);
void xos_blockdev_get(xos_block_device_t *bdev);
void xos_blockdev_put(xos_block_device_t *bdev);

int xos_blockdev_read_sectors(xos_block_device_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count);

int xos_blockdev_write_sectors(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count);

int xos_blockdev_flush(xos_block_device_t *bdev);

static inline uint64 xos_blockdev_capacity_bytes(const xos_block_device_t *bdev)
{
    if (bdev == NULL) {
        return 0;
    }
    return bdev->sector_count * (uint64)bdev->sector_size;
}


#endif