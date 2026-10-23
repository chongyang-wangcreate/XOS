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

#define XOS_BLOCKDEV_NAME_MAX 32
#define XOS_BLOCKDEV_DEFAULT_SECTOR_SIZE 512U
#define XOS_BLOCKDEV_SCRATCH_SIZE 4096U

#define XOS_BLOCKDEV_READ_ONLY (1U << 0)
#define XOS_BLOCKDEV_REMOVABLE (1U << 1)

struct xos_block_device;
struct xos_disk_device;
struct xos_partition;

typedef struct xos_block_device xos_block_device_t;

typedef int (*xos_blockdev_read_fn)(xos_block_device_t *bdev,
                                    uint64 sector,
                                    void *buffer,
                                    uint32 sector_count);
typedef int (*xos_blockdev_write_fn)(xos_block_device_t *bdev,
                                     uint64 sector,
                                     const void *buffer,
                                     uint32 sector_count);
typedef int (*xos_blockdev_flush_fn)(xos_block_device_t *bdev);
typedef int (*xos_blockdev_open_fn)(xos_block_device_t *bdev);
typedef int (*xos_blockdev_release_fn)(xos_block_device_t *bdev);


typedef struct xos_block_device_ops {
    xos_blockdev_open_fn open;
    xos_blockdev_release_fn release;
    xos_blockdev_read_fn read_sectors;
    xos_blockdev_write_fn write_sectors;
    xos_blockdev_flush_fn flush;
} xos_block_device_ops_t;


typedef xos_block_device_t xos_blkdev_t;
typedef xos_block_device_ops_t xos_blkdev_ops_t;


struct xos_block_device {
    dlist_t list;
    dlist_t inodes;
    xos_spinlock_t lock;
    xos_spinlock_t io_lock;
    xsem_t sem;

    char name[XOS_BLOCKDEV_NAME_MAX];
    devno_t devno;
    uint32 sector_size;
    uint64 sector_count;
    uint32 flags;

    uint32 ref_count;
    int registered;
    int hold_count;
    int open_count;
    int open_partitions;

    void *owner;
    struct xos_inode *block_node;
    struct xos_block_device *parent;
    struct xos_disk_device *disk;
    struct xos_partition *partition;

    void *private_data;
    const xos_block_device_ops_t *ops;
    xos_blk_req_que_t request_queue;
};


void xos_blockdev_init_device(xos_block_device_t *bdev,
                              const char *name,
                              devno_t devno,
                              uint32 sector_size,
                              uint64 sector_count,
                              uint32 flags,
                              const xos_block_device_ops_t *ops,
                              void *private_data);

int xos_blockdev_init(void);
int xos_blockdev_register(xos_block_device_t *bdev);
int xos_blockdev_unregister(xos_block_device_t *bdev);

xos_block_device_t *xos_blockdev_get_by_name(const char *name);
xos_block_device_t *xos_blockdev_get_by_devno(devno_t devno);
void xos_blockdev_get(xos_block_device_t *bdev);
void xos_blockdev_put(xos_block_device_t *bdev);
int xos_blockdev_open(xos_block_device_t *bdev);
int xos_blockdev_release(xos_block_device_t *bdev);

int xos_blockdev_read_sectors(xos_block_device_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count);
int xos_blockdev_write_sectors(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count);
int xos_blockdev_flush(xos_block_device_t *bdev);

int xos_blockdev_read(xos_block_device_t *bdev,
                      uint64 offset,
                      void *buffer,
                      uint32 length);
int xos_blockdev_write(xos_block_device_t *bdev,
                       uint64 offset,
                       const void *buffer,
                       uint32 length);


int xos_blockdev_read_block(xos_block_device_t *bdev,
                            uint64 block,
                            uint32 block_size,
                            void *buffer);
int xos_blockdev_write_block(xos_block_device_t *bdev,
                             uint64 block,
                             uint32 block_size,
                             const void *buffer);

static inline uint64
xos_blockdev_capacity_bytes(const xos_block_device_t *bdev)
{
    if (bdev == NULL) {
        return 0;
    }
    return bdev->sector_count * (uint64)bdev->sector_size;
}

int xos_blockdev_submit_io(xos_block_device_t *bdev, xos_blk_req_t *req);
int xos_blockdev_dispatch_io(xos_block_device_t *bdev);

int xos_blockdev_add_disk(struct xos_disk_device *disk);
int xos_blockdev_del_disk(struct xos_disk_device *disk);

extern xfile_ops_t xos_blockdev_file_ops;

#endif
