#ifndef __XOS_BLOCK_DEV_H__
#define __XOS_BLOCK_DEV_H__

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_dev.h"
#include "xos_block_io.h"
#include "fs.h"
#include "xos_sem.h"
#include "xos_disk.h"

#define XOS_BLOCKDEV_NAME_MAX 32
#define XOS_BLOCKDEV_DEFAULT_SECTOR_SIZE 512U

#define XOS_BLOCKDEV_READ_ONLY (1U << 0)
#define XOS_BLOCKDEV_REMOVABLE (1U << 1)

struct xos_blkdev;

typedef int (*xos_blockdev_read_fn)(struct xos_blkdev *bdev,
                                    uint64  sector,
                                    void    *buffer,
                                    uint32  sector_count);
typedef int (*xos_blockdev_write_fn)(struct xos_blkdev *bdev,
                                     uint64 sector,
                                     const void *buffer,
                                     uint32 sector_count);
typedef int (*xos_blockdev_flush_fn)(struct xos_blkdev *bdev);



typedef struct xos_blkdev_ops {
    xos_blockdev_read_fn read_sectors;
    xos_blockdev_write_fn write_sectors;
    xos_blockdev_flush_fn flush;
} xos_blkdev_ops_t;

typedef struct xos_blkdev {
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
    const xos_blkdev_ops_t *ops;
    xos_block_io_queue_t io_queue;
} xos_blkdev_t;

int xos_blockdev_init(void);
int xos_blockdev_register(xos_blkdev_t *bdev);
int xos_blockdev_unregister(xos_blkdev_t *bdev);

xos_blkdev_t *xos_blockdev_get_by_name(const char *name);
xos_blkdev_t *xos_blockdev_get_by_devno(devno_t devno);
void xos_blockdev_get(xos_blkdev_t *bdev);
void xos_blockdev_put(xos_blkdev_t *bdev);

int xos_blockdev_read_sectors(xos_blkdev_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count);

int xos_blockdev_write_sectors(xos_blkdev_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count);

int xos_blockdev_flush(xos_blkdev_t *bdev);


int xos_blockdev_read(xos_blkdev_t *bdev,
                      uint64 offset,
                      void *buffer,
                      uint32 length);

int xos_blockdev_write(xos_blkdev_t *bdev,
                       uint64 offset,
                       const void *buffer,
                       uint32 length);

static inline uint64 xos_blockdev_capacity_bytes(const xos_blkdev_t *bdev)
{
    if (bdev == NULL) {
        return 0;
    }
    return bdev->sector_count * (uint64)bdev->sector_size;
}

/*
    I/O queue helpers: submit requests through the elevator/scheduler
    and dispatch pending I/O.  These wrap xos_block_io_enqueue/dispatch.
*/
int xos_blockdev_submit_io(xos_blkdev_t *bdev,
                           xos_block_io_request_t *req);
int xos_blockdev_dispatch_io(xos_blkdev_t *bdev);

/*
    add_disk: register device + auto-scan partitions (GPT first, MBR fallback).
    Equivalent to Linux add_disk().  Returns number of partitions found (>=0)
    or negative error code.
*/
int xos_blockdev_add_disk(xos_blkdev_t *bdev);

/*
    del_disk: remove all partitions + unregister the device.
    Equivalent to Linux del_gendisk().
*/
int xos_blockdev_del_disk(xos_blkdev_t *bdev);



/*
    VFS-level block device descriptor.
    This is the block device as seen by the VFS layer,
    distinct from the low-level driver struct xos_blkdev.
*/
struct xos_block_device {

    void *owner; /* block device descriptor current owner */

    struct xos_inode *block_node; /* block device node */

    devno_t devno; /* device number */

    dlist_t block_list; /* connect block list */

    dlist_t inodes; /* manage block device node list head */

    int hold_count; /* exclusive access counter */

    int open_count;   /* how many times device has been opened */

    int open_partitions; /* how many partitions opened */

    struct xos_block_device *container; /* if partition, points to whole disk */

    xos_disk_dev_t *disk; /* disk device descriptor */

    struct partition_desc *partition; /* if partition, points to partition desc */

    xsem_t sem;
    /*
        to do
    */

};



typedef struct xos_block_request_desc{

/*
    to do
*/

}xos_blk_req_t;

typedef struct xos_block_request_queue_desc{

    int (*merge_requests) (struct xos_block_request_queue_desc *, xos_blk_req_t *,
				 struct blk_request *);

	int (*sumit_request)(struct xos_block_request_queue_desc *queue, xos_block_io_queue_t *bio);

}xos_blk_req_que;

#endif