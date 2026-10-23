/********************************************************
    development timer:2026.9
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2026-2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Partition table parser for block devices.

    Supports MBR (primary partitions) and GPT (GUID Partition Table).
    xos_partition_scan() tries GPT first, falls back to MBR if no
    valid GPT header is found.

    Each partition is registered as a child block device whose I/O
    operations delegate to the parent device with a sector offset.

********************************************************/

#include "types.h"
#include "string.h"
#include "error.h"
#include "spinlock.h"
#include "xos_block_dev.h"
#include "xos_disk.h"
#include "xos_partition.h"
#include "printk.h"

#define XOS_MBR_SIZE 512U
#define XOS_MBR_SECTOR_SIZE 512U
#define XOS_MBR_PARTITION_OFFSET 446U
#define XOS_MBR_PARTITION_SIZE 16U
#define XOS_MBR_SIGNATURE_OFFSET 510U
#define XOS_MBR_SIGNATURE_0 0x55U
#define XOS_MBR_SIGNATURE_1 0xaaU
/*定义设备主设备掩码和次设备号掩码*/
#define XOS_DEV_MINOR_MASK 0xfffffU
#define XOS_DEV_MAJOR_MASK (~XOS_DEV_MINOR_MASK)

#define XOS_GPT_HEADER_SIZE 92U
#define XOS_GPT_HEADER_LBA 1U
#define XOS_GPT_ENTRY_SIZE 128U

static xos_partition_t xos_partitions[XOS_PARTITION_MAX];
static xos_spinlock_t xos_partition_lock;
static int xos_partition_is_ready;
static int xos_partition_registered;

typedef struct {
    uint64 start_sector;
    uint64 sector_count;
    uint8 type;
    uint8 index;
} xos_partition_desc_t;

static uint32 xos_get_le32(const uint8 *value)
{
    return (uint32)value[0] |
           ((uint32)value[1] << 8) |
           ((uint32)value[2] << 16) |
           ((uint32)value[3] << 24);
}

static uint64 xos_get_le64(const uint8 *value)
{
    return (uint64)xos_get_le32(value) |
           ((uint64)xos_get_le32(value + 4) << 32);
}

static int xos_partition_is_extended(uint8 type)
{
    return type == 0x05 || type == 0x0f || type == 0x85;
}

/* MBR partition entries use 512 bytes logical sectors */
static int xos_mbr_to_device_range(const xos_block_device_t *parent,
                                   uint64 mbr_start,
                                   uint64 mbr_count,
                                   uint64 *start_sector,
                                   uint64 *sector_count)
{
    uint64 scale;

    if (parent == NULL || start_sector == NULL || sector_count == NULL ||
        parent->sector_size < XOS_MBR_SECTOR_SIZE ||
        parent->sector_size % XOS_MBR_SECTOR_SIZE != 0 ||
        mbr_count == 0) {
        return -EINVAL;
    }

    scale = parent->sector_size / XOS_MBR_SECTOR_SIZE;
    if (mbr_start % scale != 0 || mbr_count % scale != 0) {
        return -EINVAL;
    }

    *start_sector = mbr_start / scale;
    *sector_count = mbr_count / scale;
    if (*start_sector == 0 || *sector_count == 0 ||
        *start_sector >= parent->sector_count ||
        *sector_count > parent->sector_count - *start_sector) {
        return -EINVAL;
    }
    return 0;
}

static int xos_partition_range_is_valid(const xos_block_device_t *parent,
                                     uint64 start_sector,
                                     uint64 sector_count)
{
    return parent != NULL && sector_count != 0 &&
           start_sector < parent->sector_count &&
           sector_count <= parent->sector_count - start_sector;
}

static int xos_partition_byte_range_valid(const xos_block_device_t *parent,
                                          uint64 offset,
                                          uint32 length)
{
    uint64 capacity;
    int ret = 0;

    if (parent == NULL || length == 0 ||
        parent->sector_size == 0 ||
        parent->sector_count > (~(uint64)0) / parent->sector_size) {
        return 0;
    }

    capacity = parent->sector_count * (uint64)parent->sector_size;
    if(offset <= capacity && (uint64)length <= capacity - offset){
        return 1;
    }
    return ret;
}

static int xos_partition_read_bytes(xos_block_device_t *parent,
                                    uint64 sector,
                                    uint32 sector_offset,
                                    void *buffer,
                                    uint32 length)
{
    uint64 offset;

    if (parent == NULL || parent->sector_size == 0 ||
        sector > (~(uint64)0) / parent->sector_size ||
        sector_offset > parent->sector_size) {
        return -EINVAL;
    }

    offset = sector * (uint64)parent->sector_size + sector_offset;
    if (offset < sector * (uint64)parent->sector_size ||
        !xos_partition_byte_range_valid(parent, offset, length)) {
        return -EINVAL;
    }
    return xos_blockdev_read(parent, offset, buffer, length);
}

static int xos_get_partition_name(char *name,
                              uint32 name_size,
                              const char *parent_name,
                              uint8 index)
{
    uint32 length;

    if (name == NULL || parent_name == NULL || index == 0 || index > 99) {
        return -EINVAL;
    }

    length = strlen(parent_name);
    /*判断需要增加的字节数*/
    if (length + (index > 9 ? 4 : 3) > name_size) {
        return -ENAMETOOLONG;
    }
    /*拼接分区名称*/
    memcpy(name, parent_name, length);
    name[length] = 'p';
    if (index > 9) {
        name[length + 1] = (char)('0' + index / 10);
        name[length + 2] = (char)('0' + index % 10);
        name[length + 3] = '\0';
    } else {
        name[length + 1] = (char)('0' + index);
        name[length + 2] = '\0';
    }
    return 0;
}

static int xos_partition_read(xos_block_device_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count)
{
    xos_partition_t *partition;
    int ret;

    if (bdev == NULL || bdev->partition == NULL) {
        return -EINVAL;
    }

    partition = bdev->partition;
    if (!xos_partition_range_is_valid(bdev, sector, sector_count) ||
        partition->start_sector > (~(uint64)0) - sector) {
        return -EINVAL;
    }
    ret = xos_blockdev_read_sectors(partition->parent,
                                    partition->start_sector + sector,
                                    buffer, sector_count);
    if (ret == 0) {
        partition->read_times++;
        partition->read_sectors += sector_count;
    }
    return ret;
}


static int xos_partition_open(xos_block_device_t *bdev)
{
    xos_partition_t *partition;
    xos_block_device_t *parent;
    int ret;

    if (bdev == NULL || bdev->partition == NULL) {
        return -EINVAL;
    }

    partition = bdev->partition;
    parent = partition->parent;
    ret = xos_blockdev_open(parent);
    if (ret < 0) {
        return ret;
    }

    xos_spinlock(&parent->lock);
    parent->open_partitions++;
    xos_unspinlock(&parent->lock);
    return 0;
}

static int xos_partition_release(xos_block_device_t *bdev)
{
    xos_partition_t *partition;
    xos_block_device_t *parent;
    int ret;

    if (bdev == NULL || bdev->partition == NULL) {
        return -EINVAL;
    }

    partition = bdev->partition;
    parent = partition->parent;
    xos_spinlock(&parent->lock);
    if (parent->open_partitions > 0) {
        parent->open_partitions--;
    }
    xos_unspinlock(&parent->lock);

    ret = xos_blockdev_release(parent);
    return ret;
}

static int xos_partition_write(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count)
{
    xos_partition_t *partition;
    int ret;

    if (bdev == NULL || bdev->partition == NULL) {
        return -EINVAL;
    }

    partition = bdev->partition;
    if (!xos_partition_range_is_valid(bdev, sector, sector_count) ||
        partition->start_sector > (~(uint64)0) - sector) {
        return -EINVAL;
    }
    ret = xos_blockdev_write_sectors(partition->parent,
                                     partition->start_sector + sector,
                                     buffer, sector_count);
    if (ret == 0) {
        partition->write_times++;
        partition->write_sectors += sector_count;
    }
    return ret;
}

static int xos_partition_flush(xos_block_device_t *bdev)
{
    xos_partition_t *partition;

    if (bdev == NULL || bdev->partition == NULL) {
        return -EINVAL;
    }

    partition = bdev->partition;
    return xos_blockdev_flush(partition->parent);
}

static const xos_block_device_ops_t xos_partition_ops = {
    .open = xos_partition_open,
    .release = xos_partition_release,
    .read_sectors = xos_partition_read,
    .write_sectors = xos_partition_write,
    .flush = xos_partition_flush,
};

static int xos_partition_overlaps(xos_block_device_t *parent,
                                  uint64 start_sector,
                                  uint64 sector_count)
{
    uint64 existing_end;
    int i;

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (!xos_partitions[i].in_use ||
            xos_partitions[i].parent != parent) {
            continue;
        }

        existing_end = xos_partitions[i].block_device.sector_count;
        if (start_sector < xos_partitions[i].start_sector + existing_end &&
            xos_partitions[i].start_sector < start_sector + sector_count) {
            return 1;
        }
    }
    return 0;
}

static xos_partition_t *xos_partition_alloc_slot(void)
{
    int i;

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (!xos_partitions[i].in_use) {
            return &xos_partitions[i];
        }
    }
    return NULL;
}

static int xos_partition_existing_count(xos_block_device_t *parent)
{
    int count = 0;
    int i;

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (xos_partitions[i].in_use &&
            xos_partitions[i].parent == parent) {
            count++;
        }
    }
    return count;
}

/*
    增加防重复注册校验,验证分区是否已被注册.
*/
static int xos_partition_matches_locked(xos_block_device_t *parent,
                                        const xos_partition_desc_t *descs,
                                        int count)
{
    uint8 matched[XOS_PARTITION_MAX];
    int i;

    if (count < 0 || count > XOS_PARTITION_MAX ||
        (count != 0 && descs == NULL)) {
        return 0;
    }
    memset(matched, 0, sizeof(matched));

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        xos_partition_t *partition = &xos_partitions[i];
        int j;

        if (!partition->in_use || partition->parent != parent) {
            continue;
        }
        for (j = 0; j < count; j++) {
            if (!matched[j] && partition->index == descs[j].index &&
                partition->start_sector == descs[j].start_sector &&
                partition->block_device.sector_count ==
                    descs[j].sector_count &&
                partition->type == descs[j].type) {
                matched[j] = 1;
                break;
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (!matched[i]) {
            return 0;
        }
    }
    return 1;
}

static int xos_partition_is_busy(xos_block_device_t *parent)
{
    int ret;

    xos_spinlock(&parent->lock);
    ret = parent->open_partitions != 0;
    xos_unspinlock(&parent->lock);
    return ret;
}

static int xos_partition_can_unregister(xos_partition_t *partition)
{
    xos_block_device_t *bdev;
    int ret;

    if (partition == NULL || !partition->in_use) {
        return 0;
    }
    bdev = &partition->block_device;
    xos_spinlock(&bdev->lock);
    ret = bdev->registered && bdev->ref_count == 1 &&
                     xos_block_io_pending(&bdev->request_queue) == 0;
    xos_unspinlock(&bdev->lock);
    return ret;
}

static void xos_partition_rollback(xos_partition_t **created, int count)
{
    int i;

    for (i = count - 1; i >= 0; i--) {
        xos_partition_t *partition = created[i];

        xos_blockdev_unregister(&partition->block_device);
        if (partition->parent->disk != NULL) {
            list_del(&partition->disk_list);
            partition->parent->disk->partition_count--;
        }
        xos_blockdev_put(partition->parent);
        memset(partition, 0, sizeof(*partition));
        xos_partition_registered--;
    }
}


static int xos_partition_register_one(xos_block_device_t *parent,
                                       uint64 start_sector,
                                       uint64 sector_count,
                                       uint8 type,
                                       uint8 index,
                                       xos_partition_t **out)
{
    xos_partition_t *partition;
    char device_name[XOS_BLOCKDEV_NAME_MAX];
    uint32 minor;
    int ret;

    partition = xos_partition_alloc_slot();
    if (partition == NULL) {
        return -ENOSPC;
    }
    if (parent->disk != NULL &&
        parent->disk->max_partition_count > 0 &&
        parent->disk->partition_count >=
            parent->disk->max_partition_count) {
        return -ENOSPC;
    }

    memset(partition, 0, sizeof(*partition));
    partition->parent = parent;
    partition->start_sector = start_sector;
    partition->type = type;
    partition->index = index;
    partition->in_use = 1;

    ret = xos_get_partition_name(partition->block_device.name,
                             sizeof(partition->block_device.name),
                             parent->name, index);
    if (ret < 0) {
        memset(partition, 0, sizeof(*partition));
        return ret;
    }
    memcpy(device_name, partition->block_device.name, sizeof(device_name));
    minor = ((uint32)parent->devno & XOS_DEV_MINOR_MASK) + index;
    if (minor > XOS_DEV_MINOR_MASK) {
        memset(partition, 0, sizeof(*partition));
        return -ERANGE;
    }

    xos_blockdev_init_device(&partition->block_device,
                             device_name,
                             ((uint32)parent->devno & XOS_DEV_MAJOR_MASK) | minor,
                             parent->sector_size,
                             sector_count,
                             parent->flags,
                             &xos_partition_ops,
                             partition);
    partition->block_device.parent = parent;
    partition->block_device.disk = parent->disk;
    partition->block_device.partition = partition;
    list_init(&partition->disk_list);

    xos_blockdev_get(parent);
    ret = xos_blockdev_register(&partition->block_device);
    if (ret < 0) {
        xos_blockdev_put(parent);
        memset(partition, 0, sizeof(*partition));
        return ret;
    }

    xos_partition_registered++;
    if (parent->disk != NULL) {
        list_add_back(&partition->disk_list,
                      &parent->disk->partition_list);
        parent->disk->partition_count++;
    }
    if (out != NULL) {
        *out = partition;
    }
    return 0;
}                                      


static int xos_partition_commit(xos_block_device_t *parent,
                                const xos_partition_desc_t *descs,
                                int count)
{
    xos_partition_t *created[XOS_PARTITION_MAX];
    int existing_count;
    int created_count = 0;
    int i;
    int ret;

    if (xos_partition_is_busy(parent)) {
        return -EBUSY;
    }

    xos_spinlock(&xos_partition_lock);
    existing_count = xos_partition_existing_count(parent);
    if (existing_count == count && existing_count != 0 &&
        xos_partition_matches_locked(parent, descs, count)) {
        xos_unspinlock(&xos_partition_lock);
        return existing_count;
    }
    xos_unspinlock(&xos_partition_lock);

    if (existing_count != 0) {
        ret = xos_partition_remove(parent);
        if (ret < 0) {
            return ret;
        }
        if (ret != existing_count) {
            return -EBUSY;
        }
    }

    if (count == 0) {
        return 0;
    }

    xos_spinlock(&xos_partition_lock);
    existing_count = xos_partition_existing_count(parent);
    if (existing_count != 0) {
        xos_unspinlock(&xos_partition_lock);
        return -EBUSY;
    }
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    for (i = 0; i < count; i++) {
        if (!xos_partition_range_is_valid(parent, descs[i].start_sector,
                                       descs[i].sector_count) ||
            xos_partition_overlaps(parent, descs[i].start_sector,
                                    descs[i].sector_count)) {
            continue;
        }

        ret = xos_partition_register_one(parent, descs[i].start_sector,
                                         descs[i].sector_count,
                                         descs[i].type, descs[i].index,
                                         &created[created_count]);
        if (ret < 0) {
            xos_partition_rollback(created, created_count);
            xos_unspinlock(&xos_partition_lock);
            return ret;
        }
        created_count++;
    }

    xos_unspinlock(&xos_partition_lock);
    return created_count;
}



int xos_partition_init(void)
{
    if (xos_partition_is_ready) {
        return 0;
    }

    memset(xos_partitions, 0, sizeof(xos_partitions));
    xos_spinlock_init(&xos_partition_lock);
    xos_partition_registered = 0;
    xos_partition_is_ready = 1;
    return 0;
}

int xos_mbr_scan(xos_block_device_t *parent)
{
    xos_partition_desc_t descs[XOS_MBR_PRIMARY_PARTITIONS];
    uint8 mbr[XOS_MBR_SIZE];
    int desc_count = 0;
    int entry_index;
    int ret;

    if (!xos_partition_is_ready || parent == NULL ||
        parent->sector_size < XOS_MBR_SIZE || !parent->registered) {
        return -EINVAL;
    }
    if (xos_partition_is_busy(parent)) {
        return -EBUSY;
    }

    ret = xos_blockdev_read(parent, 0, mbr, sizeof(mbr));
    if (ret < 0) {
        return ret;
    }
    if (mbr[XOS_MBR_SIGNATURE_OFFSET] != XOS_MBR_SIGNATURE_0 ||
        mbr[XOS_MBR_SIGNATURE_OFFSET + 1] != XOS_MBR_SIGNATURE_1) {
        return -ENOEXEC;
    }

    for (entry_index = 0;
         entry_index < XOS_MBR_PRIMARY_PARTITIONS;
         entry_index++) {
        const uint8 *entry = mbr + XOS_MBR_PARTITION_OFFSET +
                             entry_index * XOS_MBR_PARTITION_SIZE;
        uint8 boot_flag = entry[0];
        uint8 type = entry[4];
        uint64 mbr_start = xos_get_le32(entry + 8);
        uint64 mbr_count = xos_get_le32(entry + 12);
        uint64 start_sector;
        uint64 sector_count;

        if (type == 0 || mbr_count == 0) {
            continue;
        }
        if ((boot_flag != 0 && boot_flag != 0x80) ||
            xos_partition_is_extended(type) ||
            mbr_start == 0 ||
            xos_mbr_to_device_range(parent, mbr_start, mbr_count,
                                    &start_sector, &sector_count) < 0) {
            continue;
        }

        descs[desc_count].start_sector = start_sector;
        descs[desc_count].sector_count = sector_count;
        descs[desc_count].type = type;
        descs[desc_count].index = entry_index + 1;
        desc_count++;
    }
    ret = xos_partition_commit(parent, descs, desc_count);
    return ret;
}

/*
    GPT partition scan.

    GPT layout:
      LBA 0:  Protective MBR (already validated as MBR with type 0xEE)
      LBA 1:   GPT header (92 bytes)
      LBA 2+:  GPT entry array (128 bytes per entry)

    We read the GPT header from LBA 1, then iterate over partition
    entries.  Each entry has a 16-byte partition type GUID; if it is
    not all zeros, the entry is in use.
*/
int xos_gpt_scan(xos_block_device_t *parent)
{
    xos_partition_desc_t descs[XOS_PARTITION_MAX];
    uint8 header_buf[XOS_GPT_HEADER_SIZE];
    uint8 entry_buf[XOS_GPT_ENTRY_SIZE];
    uint64 entries_start_lba;
    uint64 entries_offset;
    uint64 table_size;
    uint32 entry_size;
    uint32 num_entries;
    int desc_count = 0;
    int i;
    int ret;

    if (!xos_partition_is_ready || parent == NULL ||
        parent->sector_size < XOS_GPT_HEADER_SIZE ||
        !parent->registered) {
        return -EINVAL;
    }
    if (xos_partition_is_busy(parent)) {
        return -EBUSY;
    }

    /* Read only the defined GPT header fields; sector size may exceed 512. */
    ret = xos_partition_read_bytes(parent, XOS_GPT_HEADER_LBA, 0,
                                   header_buf, sizeof(header_buf));
    if (ret < 0) {
        return ret;
    }

    /* Check "EFI PART" signature at offset 0 */
    if (header_buf[0] != 'E' || header_buf[1] != 'F' ||
        header_buf[2] != 'I' || header_buf[3] != ' ' ||
        header_buf[4] != 'P' || header_buf[5] != 'A' ||
        header_buf[6] != 'R' || header_buf[7] != 'T') {
        return -ENOEXEC;
    }

    entries_start_lba = xos_get_le64(header_buf + 72);
    num_entries = xos_get_le32(header_buf + 80);
    entry_size = xos_get_le32(header_buf + 84);

    if (num_entries == 0 || num_entries > XOS_PARTITION_MAX ||
        entry_size < XOS_GPT_ENTRY_SIZE ||
        entries_start_lba == 0 ||
        entries_start_lba >= parent->sector_count) {
        return -EINVAL;
    }

    if (num_entries > (~(uint64)0) / entry_size) {
        return -EINVAL;
    }
    table_size = (uint64)num_entries * entry_size;
    if (table_size > 0xffffffffU) {
        return -EINVAL;
    }
    if (entries_start_lba > (~(uint64)0) / parent->sector_size ||
        table_size > (~(uint64)0) -
                      entries_start_lba * (uint64)parent->sector_size ||
        !xos_partition_byte_range_valid(
            parent, entries_start_lba * (uint64)parent->sector_size,
            (uint32)table_size)) {
        return -EINVAL;
    }

    entries_offset = entries_start_lba * (uint64)parent->sector_size;
    for (i = 0; i < (int)num_entries && i < XOS_PARTITION_MAX; i++) {
        uint64 entry_offset = (uint64)i * entry_size;
        uint64 first_lba;
        uint64 last_lba;
        uint64 sector_count;
        int j;
        int all_zero = 1;

        if (entry_offset > (~(uint64)0) - entries_offset) {
            return -EINVAL;
        }
        ret = xos_blockdev_read(parent, entries_offset + entry_offset,
                                 entry_buf, sizeof(entry_buf));
        if (ret < 0) {
            return ret;
        }

        /* Check if partition type GUID is all zeros (unused entry) */
        for (j = 0; j < 16; j++) {
            if (entry_buf[j] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) {
            continue;
        }

        first_lba = xos_get_le64(entry_buf + 32);
        last_lba = xos_get_le64(entry_buf + 40);

        if (first_lba == 0 || last_lba < first_lba ||
            !xos_partition_range_is_valid(parent, first_lba,
                                       last_lba - first_lba + 1)) {
            continue;
        }
        sector_count = last_lba - first_lba + 1;

        descs[desc_count].start_sector = first_lba;
        descs[desc_count].sector_count = sector_count;
        descs[desc_count].type = 0;
        descs[desc_count].index = i + 1;
        desc_count++;
    }
    ret = xos_partition_commit(parent, descs, desc_count);
    return ret;
}

/*
    Unified partition scan: try GPT first, fall back to MBR.
    Returns the number of partitions found (>= 0) or negative error.
*/
int xos_partition_scan(xos_block_device_t *parent)
{
    int ret;

    if (!xos_partition_is_ready || parent == NULL || !parent->registered) {
        return -EINVAL;
    }
    if (xos_partition_is_busy(parent)) {
        return -EBUSY;
    }
    printk(PT_DEBUG,"%s:%d SCAN BEGIN\n",__FUNCTION__, __LINE__);
    ret = xos_gpt_scan(parent);
    if (ret >= 0) {
        return ret;
    }

    /* GPT not found or invalid, try MBR */
    ret = xos_mbr_scan(parent);
    if (ret < 0 && ret != -ENOEXEC) {
        printk(PT_WARRING,
               "partition_scan: no partition table found on %s\n",
               parent->name);
        return 0;
    }
    return ret;
}

int xos_partition_count(void)
{
    int count;

    if (!xos_partition_is_ready) {
        return 0;
    }

    xos_spinlock(&xos_partition_lock);
    count = xos_partition_registered;
    xos_unspinlock(&xos_partition_lock);
    return count;
}

int xos_partition_remove(xos_block_device_t *parent)
{
    int removed = 0;
    int ret;
    int i;

    if (!xos_partition_is_ready || parent == NULL) {
        return -EINVAL;
    }
    if (xos_partition_is_busy(parent)) {
        return -EBUSY;
    }

    xos_spinlock(&xos_partition_lock);

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (xos_partitions[i].in_use &&
            xos_partitions[i].parent == parent &&
            !xos_partition_can_unregister(&xos_partitions[i])) {
            xos_unspinlock(&xos_partition_lock);
            return -EBUSY;
        }
    }

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (xos_partitions[i].in_use &&
            xos_partitions[i].parent == parent) {
            ret = xos_blockdev_unregister(&xos_partitions[i].block_device);
            if (ret < 0) {
                xos_unspinlock(&xos_partition_lock);
                return ret;
            }
            if (xos_partitions[i].parent->disk != NULL) {
                list_del(&xos_partitions[i].disk_list);
                xos_partitions[i].parent->disk->partition_count--;
            }
            xos_blockdev_put(xos_partitions[i].parent);
            memset(&xos_partitions[i], 0, sizeof(xos_partitions[i]));
            xos_partition_registered--;
            removed++;
        }
    }
    xos_unspinlock(&xos_partition_lock);
    return removed;
}

/*
 Refer to Linux: After seeing printk_all_partitions()
 Implement a similar function for future use.

*/
void xos_partition_dump_show(xos_block_device_t *parent)
{
    int i;

    if (!xos_partition_is_ready || parent == NULL) {
        return;
    }

    printk(PT_DEBUG, "Partitions on %s (sector_size=%u, sectors=%lu):\n",
           parent->name, parent->sector_size,
           (unsigned long)parent->sector_count);

    xos_spinlock(&xos_partition_lock);
    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (xos_partitions[i].in_use &&
            xos_partitions[i].parent == parent) {
            printk(PT_DEBUG,
                   "  %s: start=%lu count=%lu type=0x%02x\n",
                   xos_partitions[i].block_device.name,
                   (unsigned long)xos_partitions[i].start_sector,
                   (unsigned long)xos_partitions[i].block_device.sector_count,
                   xos_partitions[i].type);
        }
    }
    xos_unspinlock(&xos_partition_lock);
}
