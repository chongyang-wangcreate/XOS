#include "types.h"
#include "list.h"
#include "string.h"
#include "error.h"
#include "spinlock.h"
#include "xos_block_dev.h"
#include "xos_partition.h"
#include "printk.h"


/*
    2025.7.22 嘉兴平湖乍浦海边，看到一排排排放整齐的船突然想起我一直待开发的块设备，给了我很大灵感，每艘船就像一个个磁盘块
    船只都停在泊位线内，就像数据写入也必须遵循块对齐，每艘船都有固定的停泊位置，类似磁盘块分配器，排放整齐类似于对齐操作；船舶的进港出港，需要有序的管理船只
    防止竞争和冲突，防止使用同一个泊位，需要有效的协调管理，非常像磁盘的IO调度，船停好后，必须系缆绳，防止船飘走，否则不稳定，让我想到数据写到cache ，
    但是如果不刷新数据就不会稳定保存(类似于系缆绳)，乍浦让我受益匪浅

*/
static dlist_t xos_blockdev_list;
static xos_spinlock_t xos_blockdev_list_lock;
static int xos_blockdev_ready;

static int xos_blockdev_valid(const xos_blkdev_t *bdev)
{
    if (bdev == NULL || bdev->name[0] == '\0' ||
        bdev->ops == NULL || bdev->ops->read_sectors == NULL ||
        bdev->sector_size == 0 || bdev->sector_count == 0) {
        return 0;
    }
    return 1;
}

static int xos_blockdev_range_valid(const xos_blkdev_t *bdev,
                                    uint64 sector,
                                    uint32 sector_count)
{
    if (bdev == NULL || sector_count == 0) {
        return 0;
    }
    if (sector >= bdev->sector_count) {
        return 0;
    }
    if ((uint64)sector_count > bdev->sector_count - sector) {
        return 0;
    }
    return 1;
}

int xos_blockdev_init(void)
{
    if (xos_blockdev_ready) {
        return 0;
    }

    list_init(&xos_blockdev_list);
    xos_spinlock_init(&xos_blockdev_list_lock);
    xos_blockdev_ready = 1;
    return 0;
}

int xos_blockdev_register(xos_blkdev_t *bdev)
{
    dlist_t *node;
    xos_blkdev_t *registered;

    if (!xos_blockdev_ready) {
        return -ENODEV;
    }
    if (!xos_blockdev_valid(bdev) ||
        strlen(bdev->name) >= XOS_BLOCKDEV_NAME_MAX) {
        return -EINVAL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    list_for_each(node, (&xos_blockdev_list)) {
        registered = list_entry(node, xos_blkdev_t, list);
        if (registered->devno == bdev->devno ||
            strcmp(registered->name, bdev->name) == 0) {
            xos_unspinlock(&xos_blockdev_list_lock);
            return -EEXIST;
        }
    }

    list_init(&bdev->list);
    xos_spinlock_init(&bdev->lock);
    xos_spinlock_init(&bdev->io_lock);
    bdev->ref_count = 1;
    bdev->registered = 1;
    /* Initialize the per-device I/O request queue (elevator scheduler) */
    xos_block_io_queue_init(&bdev->io_queue, bdev);
    list_add_back(&bdev->list, &xos_blockdev_list);
    xos_unspinlock(&xos_blockdev_list_lock);
    return 0;
}

int xos_blockdev_unregister(xos_blkdev_t *bdev)
{
    if (!xos_blockdev_ready || bdev == NULL) {
        return -EINVAL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    if (!bdev->registered) {
        xos_unspinlock(&xos_blockdev_list_lock);
        return -ENODEV;
    }

    xos_spinlock(&bdev->lock);
    if (bdev->ref_count != 1) {
        xos_unspinlock(&bdev->lock);
        xos_unspinlock(&xos_blockdev_list_lock);
        return -EBUSY;
    }
    bdev->registered = 0;
    bdev->ref_count = 0;
    list_del(&bdev->list);
    list_init(&bdev->list);
    xos_unspinlock(&bdev->lock);
    xos_unspinlock(&xos_blockdev_list_lock);
    return 0;
}

void xos_blockdev_get(xos_blkdev_t *bdev)
{
    if (bdev == NULL) {
        return;
    }
    xos_spinlock(&bdev->lock);
    if (bdev->registered) {
        bdev->ref_count++;
    }
    xos_unspinlock(&bdev->lock);
}

void xos_blockdev_put(xos_blkdev_t *bdev)
{
    if (bdev == NULL) {
        return;
    }
    xos_spinlock(&bdev->lock);
    if (bdev->ref_count > 0) {
        bdev->ref_count--;
    }
    xos_unspinlock(&bdev->lock);
}

static xos_blkdev_t *xos_blockdev_get_match(devno_t devno,
                                                   const char *name,
                                                   int match_name)
{
    dlist_t *node;
    xos_blkdev_t *bdev;

    if (!xos_blockdev_ready) {
        return NULL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    list_for_each(node, (&xos_blockdev_list)) {
        bdev = list_entry(node, xos_blkdev_t, list);
        if ((match_name && strcmp(bdev->name, name) == 0) ||
            (!match_name && bdev->devno == devno)) {
            xos_spinlock(&bdev->lock);
            if (bdev->registered) {
                bdev->ref_count++;
                xos_unspinlock(&bdev->lock);
                xos_unspinlock(&xos_blockdev_list_lock);
                return bdev;
            }
            xos_unspinlock(&bdev->lock);
            break;
        }
    }
    xos_unspinlock(&xos_blockdev_list_lock);
    return NULL;
}

xos_blkdev_t *xos_blockdev_get_by_name(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    return xos_blockdev_get_match(0, name, 1);
}

xos_blkdev_t *xos_blockdev_get_by_devno(devno_t devno)
{
    return xos_blockdev_get_match(devno, NULL, 0);
}

int xos_blockdev_read_sectors(xos_blkdev_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count)
{
    xos_block_io_request_t req;

    if (buffer == NULL || !xos_blockdev_range_valid(bdev, sector,
                                                     sector_count)) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->read_sectors == NULL) {
        return -ENODEV;
    }

    /*
        Build a synchronous I/O request and route it through the
        device elevator scheduler queue.  The request is enqueued
        then dispatched immediately (single-request flush).  After
        dispatch completes, req.status holds the driver return code.
    */
    req.op            = XOS_BLOCK_IO_READ;
    req.sector        = sector;
    req.buffer        = buffer;
    req.sector_count  = sector_count;
    req.complete       = NULL;
    req.complete_data  = NULL;

    if (xos_blockdev_submit_io(bdev, &req) < 0) {
        return -EIO;
    }
    xos_blockdev_dispatch_io(bdev);
    return req.status;
}

int xos_blockdev_write_sectors(xos_blkdev_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count)
{
    xos_block_io_request_t req;

    if (buffer == NULL || !xos_blockdev_range_valid(bdev, sector,
                                                     sector_count)) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->write_sectors == NULL) {
        return -ENOSYS;
    }
    if (bdev->flags & XOS_BLOCKDEV_READ_ONLY) {
        return -EROFS;
    }

    req.op            = XOS_BLOCK_IO_WRITE;
    req.sector        = sector;
    req.buffer        = (void *)buffer;
    req.sector_count  = sector_count;
    req.complete       = NULL;
    req.complete_data  = NULL;

    if (xos_blockdev_submit_io(bdev, &req) < 0) {
        return -EIO;
    }
    xos_blockdev_dispatch_io(bdev);
    return req.status;
}

int xos_blockdev_flush(xos_blkdev_t *bdev)
{
    xos_block_io_request_t req;

    if (bdev == NULL || !bdev->registered || bdev->ops == NULL) {
        return -ENODEV;
    }
    if (bdev->ops->flush == NULL) {
        return 0;
    }

    req.op            = XOS_BLOCK_IO_FLUSH;
    req.sector        = 0;
    req.buffer        = NULL;
    req.sector_count  = 0;
    req.complete       = NULL;
    req.complete_data  = NULL;

    if (xos_blockdev_submit_io(bdev, &req) < 0) {
        return -EIO;
    }
    xos_blockdev_dispatch_io(bdev);
    return req.status;
}


/*
    Byte-level read: translates arbitrary offset+length into sector reads.
    Handles partial head sector, full middle sectors, and partial tail sector.
    Uses a stack scratch buffer for partial-sector unaligned head/tail reads.
    For aligned middle sectors, reads directly into the caller buffer.
*/
#define XOS_BLOCKDEV_SCRATCH_SIZE 512

int xos_blockdev_read(xos_blkdev_t *bdev,
                      uint64 offset,
                      void *buffer,
                      uint32 length)
{
    uint8 scratch[XOS_BLOCKDEV_SCRATCH_SIZE];
    uint8 *buf = (uint8 *)buffer;
    uint64 capacity;
    uint64 first_sector;
    uint64 last_sector;
    uint32 sector_size;
    uint32 head_skip;
    uint32 tail_len;
    uint32 mid_count;
    uint64 mid_last;
    uint32 head_off;
    int ret;

    if (bdev == NULL || buffer == NULL || length == 0) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->read_sectors == NULL) {
        return -ENODEV;
    }

    sector_size = bdev->sector_size;
    if (sector_size == 0 || sector_size > XOS_BLOCKDEV_SCRATCH_SIZE) {
        return -EINVAL;
    }

    capacity = bdev->sector_count * (uint64)sector_size;
    if (offset >= capacity) {
        return -EINVAL;
    }
    if ((uint64)length > capacity - offset) {
        return -EINVAL;
    }

    first_sector = offset / sector_size;
    head_skip = (uint32)(offset % sector_size);
    last_sector = (offset + length - 1) / sector_size;

    /* --- head: partial sector at the start --- */
    if (head_skip != 0) {
        ret = xos_blockdev_read_sectors(bdev, first_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        if (first_sector == last_sector) {
            /* entire read fits in one sector */
            memcpy(buf, scratch + head_skip, length);
            return 0;
        }
        head_off = sector_size - head_skip;
        memcpy(buf, scratch + head_skip, head_off);
        buf += head_off;
        first_sector++;
    }

    /* --- determine if tail needs partial read --- */
    tail_len = 0;
    if (first_sector <= last_sector) {
        uint32 tail_off = (uint32)((offset + length - 1) % sector_size);
        if (tail_off + 1 != sector_size) {
            tail_len = tail_off + 1;
        }
    }

    /* --- middle: full sectors, read directly into caller buffer --- */
    mid_last = last_sector;
    if (tail_len != 0) {
        mid_last = last_sector - 1;
    }
    if (first_sector <= mid_last) {
        mid_count = (uint32)(mid_last - first_sector + 1);
        ret = xos_blockdev_read_sectors(bdev, first_sector, buf, mid_count);
        if (ret < 0) {
            return ret;
        }
        buf += mid_count * sector_size;
        first_sector = mid_last + 1;
    }

    /* --- tail: partial sector at end --- */
    if (tail_len != 0) {
        ret = xos_blockdev_read_sectors(bdev, first_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        memcpy(buf, scratch, tail_len);
    }

    return 0;
}

/*
    Byte-level write: translates arbitrary offset+length into sector writes.
    For partial head/tail sectors, reads the existing sector first, modifies
    the relevant bytes, then writes back. Full middle sectors are written
    directly from the caller buffer.
*/
int xos_blockdev_write(xos_blkdev_t *bdev,
                       uint64 offset,
                       const void *buffer,
                       uint32 length)
{
    uint8 scratch[XOS_BLOCKDEV_SCRATCH_SIZE];
    const uint8 *buf = (const uint8 *)buffer;
    uint64 capacity;
    uint64 first_sector;
    uint64 last_sector;
    uint32 sector_size;
    uint32 head_skip;
    uint32 tail_len;
    uint32 mid_count;
    uint64 mid_last;
    uint32 head_off;
    uint32 tail_off;
    int ret;

    if (bdev == NULL || buffer == NULL || length == 0) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->read_sectors == NULL ||
        bdev->ops->write_sectors == NULL) {
        return -ENODEV;
    }
    if (bdev->flags & XOS_BLOCKDEV_READ_ONLY) {
        return -EROFS;
    }

    sector_size = bdev->sector_size;
    if (sector_size == 0 || sector_size > XOS_BLOCKDEV_SCRATCH_SIZE) {
        return -EINVAL;
    }

    capacity = bdev->sector_count * (uint64)sector_size;
    if (offset >= capacity) {
        return -EINVAL;
    }
    if ((uint64)length > capacity - offset) {
        return -EINVAL;
    }

    first_sector = offset / sector_size;
    head_skip = (uint32)(offset % sector_size);
    last_sector = (offset + length - 1) / sector_size;
    tail_off = (uint32)((offset + length - 1) % sector_size);

    /* --- head: partial sector, read-modify-write --- */
    if (head_skip != 0) {
        ret = xos_blockdev_read_sectors(bdev, first_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        if (first_sector == last_sector) {
            memcpy(scratch + head_skip, buf, length);
            return xos_blockdev_write_sectors(bdev, first_sector,
                                             scratch, 1);
        }
        head_off = sector_size - head_skip;
        memcpy(scratch + head_skip, buf, head_off);
        ret = xos_blockdev_write_sectors(bdev, first_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        buf += head_off;
        first_sector++;
    }

    /* --- determine if tail needs RMW --- */
    tail_len = 0;
    if (first_sector <= last_sector && tail_off + 1 != sector_size) {
        tail_len = tail_off + 1;
    }

    /* --- middle: full sectors, write directly --- */
    mid_last = last_sector;
    if (tail_len != 0) {
        mid_last = last_sector - 1;
    }
    if (first_sector <= mid_last) {
        mid_count = (uint32)(mid_last - first_sector + 1);
        ret = xos_blockdev_write_sectors(bdev, first_sector, buf, mid_count);
        if (ret < 0) {
            return ret;
        }
        buf += mid_count * sector_size;
        first_sector = mid_last + 1;
    }

    /* --- tail: read-modify-write --- */
    if (tail_len != 0) {
        ret = xos_blockdev_read_sectors(bdev, last_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        memcpy(scratch, buf, tail_len);
        ret = xos_blockdev_write_sectors(bdev, last_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

/*
    Submit an I/O request through the device's elevator scheduler.
    The request is enqueued and may be merged with adjacent pending
    requests.  Call xos_blockdev_dispatch_io() to drain the queue.
*/
int xos_blockdev_submit_io(xos_blkdev_t *bdev,
                           xos_block_io_request_t *req)
{
    if (bdev == NULL || req == NULL || !bdev->registered) {
        return -EINVAL;
    }
    req->bdev = bdev;
    return xos_block_io_enqueue(&bdev->io_queue, req);
}

/*
    Dispatch all pending I/O requests on this device's queue.
    Requests are picked by the elevator algorithm (SCAN + deadline),
    executed synchronously, and completion callbacks are invoked.
*/
int xos_blockdev_dispatch_io(xos_blkdev_t *bdev)
{
    if (bdev == NULL || !bdev->registered) {
        return -EINVAL;
    }
    return xos_block_io_dispatch(&bdev->io_queue);
}

/*
    xos_blockdev_add_disk — equivalent to Linux add_disk().

    Registers the block device, then automatically triggers partition
    scanning (GPT first, MBR fallback).  Drivers should call this instead
    of calling xos_blockdev_register() + xos_partition_scan() separately.

    Returns the number of partitions found (>= 0) or a negative error code.
    If registration succeeds but partition scan fails, the device is still
    registered (partition scan is best-effort).
*/
int xos_blockdev_add_disk(xos_blkdev_t *bdev)
{
    int ret;
    int part_count = 0;

    if (bdev == NULL) {
        return -EINVAL;
    }

    ret = xos_blockdev_register(bdev);
    if (ret < 0) {
        return ret;
    }

    /*
        Automatically scan partitions.  xos_partition_scan() tries GPT
        first, falls back to MBR if no GPT header is found.

        If partition scanning fails, the device itself is still usable
        as a whole-disk device.  We log a warning but do not fail.
    */
    ret = xos_partition_scan(bdev);
    if (ret < 0) {
        printk(PT_WARRING,
               "add_disk: partition scan failed for %s (ret=%d)\n",
               bdev->name, ret);
    } else {
        part_count = ret;
    }

    printk(PT_DEBUG,
           "add_disk: %s registered, %d partitions found\n",
           bdev->name, part_count);

    return part_count;
}

/*
    xos_blockdev_del_disk — equivalent to Linux del_gendisk().

    Removes all partitions belonging to this device, then unregisters
    the device itself.  Safe to call even if partition scan was never
    done or failed.
*/
int xos_blockdev_del_disk(xos_blkdev_t *bdev)
{
    int ret;

    if (bdev == NULL) {
        return -EINVAL;
    }

    /* Remove all partitions that reference this device as parent */
    xos_partition_remove(bdev);

    ret = xos_blockdev_unregister(bdev);
    if (ret < 0) {
        printk(PT_WARRING,
               "del_disk: unregister failed for %s (ret=%d)\n",
               bdev->name, ret);
        return ret;
    }

    printk(PT_DEBUG, "del_disk: %s unregistered\n", bdev->name);
    return 0;
}


/*
    Block device file operations.
    XOS xfile_ops_t currently only supports: open, read, write, readdir, llseek.
    Linux kernel block device fops (release/aio/mmap/fsync/ioctl/readv/writev/sendfile)
    are not yet ported. Kept as reference for future expansion.
*/
#if 0
struct xos_file_ops blk_fops = {
	.open		= blkdev_open,
	.release	= blkdev_close,
	.llseek		= blkdev_llseek,
	.read		= generic_file_read,
	.write		= blkdev_file_write,
  	.aio_read	= generic_file_aio_read,
  	.aio_write	= blkdev_file_aio_write,
	.mmap		= generic_file_mmap,
	.fsync		= blkdev_fsync,
	.ioctl		= blk_ioctl,
	.readv		= generic_file_readv,
	.writev		= __generic_file_write,
	.sendfile	= generic_file_sendfile,
};
#endif
