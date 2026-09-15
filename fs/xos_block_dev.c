#include "types.h"
#include "list.h"
#include "string.h"
#include "error.h"
#include "spinlock.h"
#include "xos_block_dev.h"


/*
    2025.7.22 嘉兴平湖乍浦海边，看到一排排排放整齐的船突然想起我一直待开发的块设备，给了我很大灵感，每艘船就像一个个磁盘块
    船只都停在泊位线内，就像数据写入也必须遵循块对齐，每艘船都有固定的停泊位置，类似磁盘块分配器，排放整齐类似于对齐操作；船舶的进港出港，需要有序的管理船只
    防止竞争和冲突，防止使用同一个泊位，需要有效的协调管理，非常像磁盘的IO调度，船停好后，必须系缆绳，防止船飘走，否则不稳定，让我想到数据写到cache ，
    但是如果不刷新数据就不会稳定保存(类似于系缆绳)，乍浦让我受益匪浅

*/
static dlist_t xos_blockdev_list;
static xos_spinlock_t xos_blockdev_list_lock;
static int xos_blockdev_ready;

static int xos_blockdev_valid(const xos_block_device_t *bdev)
{
    if (bdev == NULL || bdev->name[0] == '\0' ||
        bdev->ops == NULL || bdev->ops->read_sectors == NULL ||
        bdev->sector_size == 0 || bdev->sector_count == 0) {
        return 0;
    }
    return 1;
}

static int xos_blockdev_range_valid(const xos_block_device_t *bdev,
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

int xos_blockdev_register(xos_block_device_t *bdev)
{
    dlist_t *node;
    xos_block_device_t *registered;

    if (!xos_blockdev_ready) {
        return -ENODEV;
    }
    if (!xos_blockdev_valid(bdev) ||
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
    xos_spinlock_init(&bdev->lock);
    xos_spinlock_init(&bdev->io_lock);
    bdev->ref_count = 1;
    bdev->registered = 1;
    list_add_back(&bdev->list, &xos_blockdev_list);
    xos_unspinlock(&xos_blockdev_list_lock);
    return 0;
}

int xos_blockdev_unregister(xos_block_device_t *bdev)
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

    if (!xos_blockdev_ready) {
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
    int ret;

    if (buffer == NULL || !xos_blockdev_range_valid(bdev, sector,
                                                     sector_count)) {
        return -EINVAL;
    }
    if (!bdev->registered || bdev->ops == NULL ||
        bdev->ops->read_sectors == NULL) {
        return -ENODEV;
    }

    xos_spinlock(&bdev->io_lock);
    ret = bdev->ops->read_sectors(bdev, sector, buffer, sector_count);
    xos_unspinlock(&bdev->io_lock);
    return ret;
}

int xos_blockdev_write_sectors(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count)
{
    int ret;

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

    xos_spinlock(&bdev->io_lock);
    ret = bdev->ops->write_sectors(bdev, buffer, sector_count);
    xos_unspinlock(&bdev->io_lock);
    return ret;
}

int xos_blockdev_flush(xos_block_device_t *bdev)
{
    int ret;

    if (bdev == NULL || !bdev->registered || bdev->ops == NULL) {
        return -ENODEV;
    }
    if (bdev->ops->flush == NULL) {
        return 0;
    }

    xos_spinlock(&bdev->io_lock);
    ret = bdev->ops->flush(bdev);
    xos_unspinlock(&bdev->io_lock);
    return ret;
}



