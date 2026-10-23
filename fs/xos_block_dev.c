#include "types.h"
#include "list.h"
#include "string.h"
#include "error.h"
#include "spinlock.h"
#include "xos_block_dev.h"
#include "xos_disk.h"
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
static int xos_blockdev_is_ready;

void xos_blockdev_init_device(xos_block_device_t *bdev,
                              const char *name,
                              devno_t devno,
                              uint32 sector_size,
                              uint64 sector_count,
                              uint32 flags,
                              const xos_block_device_ops_t *ops,
                              void *private_data)
{
    char name_copy[XOS_BLOCKDEV_NAME_MAX];
    uint32 name_length;

    if (bdev == NULL) {
        return;
    }

    name_length = 0;
    if (name != NULL) {
        name_length = strlen(name);
        if (name_length >= XOS_BLOCKDEV_NAME_MAX) {
            name_length = XOS_BLOCKDEV_NAME_MAX - 1;
        }
        memcpy(name_copy, name, name_length);
    }
    name_copy[name_length] = '\0';
    memset(bdev, 0, sizeof(*bdev));
    memcpy(bdev->name, name_copy, name_length + 1);
    bdev->devno = devno;
    bdev->sector_size = sector_size;
    bdev->sector_count = sector_count;
    bdev->flags = flags;
    bdev->private_data = private_data;
    bdev->ops = ops;
}

static int xos_blockdev_is_valid(const xos_block_device_t *bdev)
{
    if (bdev == NULL || bdev->name[0] == '\0' ||
        bdev->ops == NULL || bdev->ops->read_sectors == NULL ||
        bdev->sector_size == 0 || bdev->sector_count == 0) {
        return 0;
    }
    return 1;
}

static int xos_blockdev_range_is_valid(const xos_block_device_t *bdev,
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
    if (xos_blockdev_is_ready) {
        return 0;
    }

    list_init(&xos_blockdev_list);
    xos_spinlock_init(&xos_blockdev_list_lock);
    xos_blockdev_is_ready = 1;
    return 0;
}

int xos_blockdev_register(xos_block_device_t *bdev)
{
    dlist_t *node;
    xos_block_device_t *registered;

    if (!xos_blockdev_is_ready) {
        return -ENODEV;
    }
    if (!xos_blockdev_is_valid(bdev) ||
        strlen(bdev->name) >= XOS_BLOCKDEV_NAME_MAX) {
        return -EINVAL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    list_for_each(node, (&xos_blockdev_list)) {
        registered = list_entry(node, xos_block_device_t, list);
        if (registered->devno == bdev->devno ||
            strcmp(registered->name, bdev->name) == 0) {
            xos_unspinlock(&xos_blockdev_list_lock);
            return -EEXIST;
        }
    }

    list_init(&bdev->list);
    list_init(&bdev->inodes);
    xos_spinlock_init(&bdev->lock);
    xos_spinlock_init(&bdev->io_lock);
    sem_init(&bdev->sem, 1, SEM_MUTEXT);
    bdev->ref_count = 1;
    bdev->registered = 1;
    if (bdev->parent == NULL) {
        bdev->parent = bdev;
    }
    /*
        新增IO 调度
    */
    xos_block_io_queue_init(&bdev->request_queue, bdev);
    list_add_back(&bdev->list, &xos_blockdev_list);
    xos_unspinlock(&xos_blockdev_list_lock);
    return 0;
}

int xos_blockdev_open(xos_block_device_t *bdev)
{
    int ret = 0;

    if (bdev == NULL || !bdev->registered) {
        return -ENODEV;
    }

    xos_spinlock(&bdev->lock);
    if (!bdev->registered) {
        xos_unspinlock(&bdev->lock);
        return -ENODEV;
    }
    bdev->open_count++;
    bdev->ref_count++;
    xos_unspinlock(&bdev->lock);

    if (bdev->ops != NULL && bdev->ops->open != NULL) {
        ret = bdev->ops->open(bdev);
        if (ret < 0) {
            xos_spinlock(&bdev->lock);
            if (bdev->open_count > 0) {
                bdev->open_count--;
            }
            if (bdev->ref_count > 0) {
                bdev->ref_count--;
            }
            xos_unspinlock(&bdev->lock);
        }
    }
    return ret;
}

int xos_blockdev_release(xos_block_device_t *bdev)
{
    int ret = 0;

    if (bdev == NULL || !bdev->registered) {
        return -ENODEV;
    }

    xos_spinlock(&bdev->lock);
    if (bdev->open_count == 0) {
        xos_unspinlock(&bdev->lock);
        return -EINVAL;
    }
    bdev->open_count--;
    if (bdev->ref_count > 0) {
        bdev->ref_count--;
    }
    xos_unspinlock(&bdev->lock);

    if (bdev->ops != NULL && bdev->ops->release != NULL) {
        ret = bdev->ops->release(bdev);
    }
    return ret;
}

int xos_blockdev_unregister(xos_block_device_t *bdev)
{
    if (!xos_blockdev_is_ready || bdev == NULL) {
        return -EINVAL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    if (!bdev->registered) {
        xos_unspinlock(&xos_blockdev_list_lock);
        return -ENODEV;
    }

    xos_spinlock(&bdev->lock);
    if (bdev->ref_count != 1 ||
        xos_block_io_pending(&bdev->request_queue) != 0) {
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

void xos_blockdev_get(xos_block_device_t *bdev)
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

void xos_blockdev_put(xos_block_device_t *bdev)
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

static xos_block_device_t *xos_blockdev_get_match(devno_t devno,
                                                   const char *name,
                                                   int match_name)
{
    dlist_t *node;
    xos_block_device_t *bdev;

    if (!xos_blockdev_is_ready) {
        return NULL;
    }

    xos_spinlock(&xos_blockdev_list_lock);
    list_for_each(node, (&xos_blockdev_list)) {
        bdev = list_entry(node, xos_block_device_t, list);
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

xos_block_device_t *xos_blockdev_get_by_name(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    return xos_blockdev_get_match(0, name, 1);
}

xos_block_device_t *xos_blockdev_get_by_devno(devno_t devno)
{
    return xos_blockdev_get_match(devno, NULL, 0);
}

int xos_blockdev_read_sectors(xos_block_device_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count)
{
    xos_block_io_request_t req;

    if (buffer == NULL || !xos_blockdev_range_is_valid(bdev, sector,
                                                     sector_count)) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->read_sectors == NULL) {
        return -ENODEV;
    }

    /*
        初始化请求队列
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

int xos_blockdev_write_sectors(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count)
{
    xos_block_io_request_t req;

    if (buffer == NULL || !xos_blockdev_range_is_valid(bdev, sector,
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

int xos_blockdev_flush(xos_block_device_t *bdev)
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





int xos_blockdev_read(xos_block_device_t *bdev,
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
        printk(PT_DEBUG,"%s:%d,read first sector\n\r",__FUNCTION__,__LINE__);
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
    if (first_sector <= last_sector) {
        mid_count = (uint32)(last_sector - first_sector + 1) -
                    (tail_len != 0 ? 1U : 0U);
    } else {
        mid_count = 0;
    }
    if (mid_count != 0) {
        ret = xos_blockdev_read_sectors(bdev, first_sector, buf, mid_count);
        if (ret < 0) {
            return ret;
        }
        buf += (size_t)mid_count * sector_size;
        first_sector += mid_count;
    }

    /* --- tail: partial sector at end --- */
    if (tail_len != 0) {
        printk(PT_DEBUG,"%s:%d,read tail sector\n\r",__FUNCTION__,__LINE__);
        ret = xos_blockdev_read_sectors(bdev, first_sector, scratch, 1);
        if (ret < 0) {
            return ret;
        }
        memcpy(buf, scratch, tail_len);
    }

    return 0;
}


int xos_blockdev_write(xos_block_device_t *bdev,
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
    if (first_sector <= last_sector) {
        mid_count = (uint32)(last_sector - first_sector + 1) -
                    (tail_len != 0 ? 1U : 0U);
    } else {
        mid_count = 0;
    }
    if (mid_count != 0) {
        ret = xos_blockdev_write_sectors(bdev, first_sector, buf, mid_count);
        if (ret < 0) {
            return ret;
        }
        buf += (size_t)mid_count * sector_size;
        first_sector += mid_count;
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

int xos_blockdev_read_block(xos_block_device_t *bdev,
                            uint64 block,
                            uint32 block_size,
                            void *buffer)
{
    uint64 sectors_per_block;
    uint64 sector;

    if (bdev == NULL || buffer == NULL || block_size == 0 ||
        bdev->sector_size == 0 ||
        block_size % bdev->sector_size != 0) {
        return -EINVAL;
    }
    sectors_per_block = block_size / bdev->sector_size;
    if (sectors_per_block > 0xffffffffUL) {
        return -EOVERFLOW;
    }
    if (block > (~0UL) / sectors_per_block) {
        return -ERANGE;
    }
    sector = block * sectors_per_block;
    return xos_blockdev_read_sectors(bdev, sector, buffer,
                                     (uint32)sectors_per_block);
}

int xos_blockdev_write_block(xos_block_device_t *bdev,
                             uint64 block,
                             uint32 block_size,
                             const void *buffer)
{
    uint64 sectors_per_block;
    uint64 sector;

    if (bdev == NULL || buffer == NULL || block_size == 0 ||
        bdev->sector_size == 0 ||
        block_size % bdev->sector_size != 0) {
        return -EINVAL;
    }
    sectors_per_block = block_size / bdev->sector_size;
    if (sectors_per_block > 0xffffffffUL) {
        return -EOVERFLOW;
    }
    if (block > (~0UL) / sectors_per_block) {
        return -ERANGE;
    }
    sector = block * sectors_per_block;
    return xos_blockdev_write_sectors(bdev, sector, buffer,
                                      (uint32)sectors_per_block);
}

/*
    Submit an I/O request through the device's elevator scheduler.
*/
int xos_blockdev_submit_io(xos_block_device_t *bdev, xos_blk_req_t *req)
{
    if (bdev == NULL || req == NULL || !bdev->registered ||
        bdev->request_queue.ops == NULL ||
        bdev->request_queue.ops->submit_request == NULL) {
        return -EINVAL;
    }
    req->bdev = bdev;
    return bdev->request_queue.ops->submit_request(&bdev->request_queue, req);
}


int xos_blockdev_dispatch_io(xos_block_device_t *bdev)
{
    if (bdev == NULL || !bdev->registered ||
        bdev->request_queue.ops == NULL ||
        bdev->request_queue.ops->dispatch_requests == NULL) {
        return -EINVAL;
    }
    return bdev->request_queue.ops->dispatch_requests(&bdev->request_queue);
}

/*
    baidu translation:
    xos_blockdev_add_disk — equivalent to Linux add_disk().

    Registers the block device, then automatically triggers partition
    scanning GPT first, MBR fallback.  
*/
int xos_blockdev_add_disk(xos_disk_dev_t *disk)
{
    xos_block_device_t *bdev;
    int ret;
    int part_count = 0;

    if (disk == NULL) {
        return -EINVAL;
    }
    bdev = xos_disk_to_block(disk);
    bdev->disk = disk;
    bdev->parent = bdev;
    disk->partition_count = 0;
    list_init(&disk->partition_list);

    ret = xos_blockdev_register(bdev);
    if (ret < 0) {
        return ret;
    }

    /*
        baidu translation:
        Automatically scan partitions.  xos_partition_scan() tries GPT
        first, falls back to MBR if no GPT header is found.
    */
    ret = xos_partition_scan(bdev);
    if (ret < 0) {
        printk(PT_WARRING,
               "add_disk: partition scan failed for %s (ret=%d)\n",
               bdev->name, ret);
    } else {
        part_count = ret;
        disk->partition_count = ret;
    }

    printk(PT_DEBUG,
           "add_disk: %s registered, %d partitions found\n",
           bdev->name, part_count);

    return part_count;
}

/*
    xos_blockdev_del_disk — equivalent to Linux del_gendisk().
*/
int xos_blockdev_del_disk(xos_disk_dev_t *disk)
{
    xos_block_device_t *bdev;
    int ret;

    if (disk == NULL) {
        return -EINVAL;
    }
    bdev = xos_disk_to_block(disk);

    /* Remove all partitions that reference this device as parent */
    xos_partition_remove(bdev);
    if (disk->partition_count != 0) {
        return -EBUSY;
    }

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

static int xos_blockdev_file_open(struct xos_inode *inode, struct file *filp)
{
    xos_block_device_t *bdev;
    int ret;

    if (inode == NULL || filp == NULL) {
        return -EINVAL;
    }
    bdev = xos_blockdev_get_by_devno(inode->devno);
    if (bdev == NULL) {
        return -ENODEV;
    }
    if ((filp->f_mode & FMODE_WRITE) &&
        (bdev->flags & XOS_BLOCKDEV_READ_ONLY)) {
        xos_blockdev_put(bdev);
        return -EROFS;
    }
    ret = xos_blockdev_open(bdev);
    if (ret < 0) {
        xos_blockdev_put(bdev);
        return ret;
    }
    filp->private_data = bdev;
    return 0;
}

static int xos_blockdev_file_release(struct xos_inode *inode, struct file *filp)
{
    xos_block_device_t *bdev;
    int ret;

    (void)inode;
    if (filp == NULL || filp->private_data == NULL) {
        return -EINVAL;
    }
    bdev = filp->private_data;
    filp->private_data = NULL;
    ret = xos_blockdev_release(bdev);
    xos_blockdev_put(bdev);
    return ret;
}

static long xos_blockdev_file_read(struct file *filp, char *buf,
                                   size_t count, long *pos)
{
    xos_block_device_t *bdev;
    uint64 capacity;
    uint32 length;
    int ret;

    if (filp == NULL || buf == NULL || pos == NULL ||
        filp->private_data == NULL || *pos < 0) {
        return -EINVAL;
    }
    if (count == 0) {
        return 0;
    }
    bdev = filp->private_data;
    capacity = xos_blockdev_capacity_bytes(bdev);
    if ((uint64)*pos >= capacity) {
        return 0;
    }
    length = count > 0xffffffffU ? 0xffffffffU : (uint32)count;
    if ((uint64)length > capacity - (uint64)*pos) {
        length = (uint32)(capacity - (uint64)*pos);
    }
    if ((uint64)length > ((~0UL) >> 1) - (uint64)*pos) {
        length = (uint32)(((~0UL) >> 1) - (uint64)*pos);
    }
    if (length == 0) {
        return 0;
    }
    ret = xos_blockdev_read(bdev, (uint64)*pos, buf, length);
    if (ret < 0) {
        return ret;
    }
    *pos += length;
    return length;
}

static long xos_blockdev_file_write(struct file *filp, const char *buf,
                                    size_t count, long *pos)
{
    xos_block_device_t *bdev;
    uint64 capacity;
    uint32 length;
    int ret;

    if (filp == NULL || buf == NULL || pos == NULL ||
        filp->private_data == NULL || *pos < 0) {
        return -EINVAL;
    }
    if (count == 0) {
        return 0;
    }
    bdev = filp->private_data;
    if (bdev->flags & XOS_BLOCKDEV_READ_ONLY) {
        return -EROFS;
    }
    capacity = xos_blockdev_capacity_bytes(bdev);
    if ((uint64)*pos >= capacity) {
        return -ENOSPC;
    }
    length = count > 0xffffffffU ? 0xffffffffU : (uint32)count;
    if ((uint64)length > capacity - (uint64)*pos) {
        length = (uint32)(capacity - (uint64)*pos);
    }
    if ((uint64)length > ((~0UL) >> 1) - (uint64)*pos) {
        length = (uint32)(((~0UL) >> 1) - (uint64)*pos);
    }
    if (length == 0) {
        return -EOVERFLOW;
    }
    ret = xos_blockdev_write(bdev, (uint64)*pos, buf, length);
    if (ret < 0) {
        return ret;
    }
    *pos += length;
    return length;
}

static loff_t xos_blockdev_file_llseek(struct file *filp,
                                       loff_t offset, int whence)
{
    xos_block_device_t *bdev;
    uint64 capacity;
    loff_t base;
    loff_t position;

    if (filp == NULL || filp->private_data == NULL) {
        return -ENODEV;
    }
    bdev = filp->private_data;
    capacity = xos_blockdev_capacity_bytes(bdev);
    if (capacity > ((~0UL) >> 1)) {
        return -EOVERFLOW;
    }
    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = filp->f_pos;
        break;
    case SEEK_END:
        base = (loff_t)capacity;
        break;
    default:
        return -EINVAL;
    }
    if (offset < -base || offset > (loff_t)capacity - base) {
        return -EINVAL;
    }
    position = base + offset;
    filp->f_pos = (long)position;
    return position;
}

xfile_ops_t xos_blockdev_file_ops = {
    .open = xos_blockdev_file_open,
    .release = xos_blockdev_file_release,
    .read = xos_blockdev_file_read,
    .write = xos_blockdev_file_write,
    .llseek = xos_blockdev_file_llseek,
};



