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
#include "xos_partition.h"
#include "printk.h"

#define XOS_MBR_SIZE 512U
#define XOS_MBR_PARTITION_OFFSET 446U
#define XOS_MBR_PARTITION_SIZE 16U
#define XOS_MBR_SIGNATURE_OFFSET 510U
#define XOS_MBR_SIGNATURE_0 0x55U
#define XOS_MBR_SIGNATURE_1 0xaaU
#define XOS_DEV_MINOR_MASK 0xfffffU
#define XOS_DEV_MAJOR_MASK (~XOS_DEV_MINOR_MASK)

#define XOS_GPT_SIGNATURE_0 0x454c4946U  /* "ELFI" reversed = "EFIE" */
#define XOS_GPT_HEADER_SIZE 92U
#define XOS_GPT_HEADER_LBA 1U
#define XOS_GPT_ENTRY_SIZE 128U

typedef struct xos_partition {
    xos_block_device_t bdev;
    xos_block_device_t *parent;
    uint64 start_sector;
    uint8 type;
    uint8 index;
    int in_use;
} xos_partition_t;

static xos_partition_t xos_partitions[XOS_PARTITION_MAX];
static xos_spinlock_t xos_partition_lock;
static int xos_partition_ready;
static int xos_partition_registered;

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

static int xos_partition_name(char *name,
                              uint32 name_size,
                              const char *parent_name,
                              uint8 index)
{
    uint32 length;

    if (name == NULL || parent_name == NULL || index == 0 || index > 9) {
        return -EINVAL;
    }

    length = strlen(parent_name);
    if (length + 3 > name_size) {
        return -ENAMETOOLONG;
    }

    memcpy(name, parent_name, length);
    name[length] = 'p';
    name[length + 1] = (char)('0' + index);
    name[length + 2] = '\0';
    return 0;
}

static int xos_partition_read(xos_block_device_t *bdev,
                              uint64 sector,
                              void *buffer,
                              uint32 sector_count)
{
    xos_partition_t *partition;

    if (bdev == NULL || bdev->private_data == NULL) {
        return -EINVAL;
    }

    partition = bdev->private_data;
    return xos_blockdev_read_sectors(partition->parent,
                                     partition->start_sector + sector,
                                     buffer, sector_count);
}

static int xos_partition_write(xos_block_device_t *bdev,
                               uint64 sector,
                               const void *buffer,
                               uint32 sector_count)
{
    xos_partition_t *partition;

    if (bdev == NULL || bdev->private_data == NULL) {
        return -EINVAL;
    }

    partition = bdev->private_data;
    return xos_blockdev_write_sectors(partition->parent,
                                      partition->start_sector + sector,
                                      buffer, sector_count);
}

static int xos_partition_flush(xos_block_device_t *bdev)
{
    xos_partition_t *partition;

    if (bdev == NULL || bdev->private_data == NULL) {
        return -EINVAL;
    }

    partition = bdev->private_data;
    return xos_blockdev_flush(partition->parent);
}

static const xos_block_device_ops_t xos_partition_ops = {
    .read_sectors = xos_partition_read,
    .write_sectors = xos_partition_write,
    .flush = xos_partition_flush,
};

static int xos_partition_overlaps(xos_block_device_t *parent,
                                  uint64 start_sector,
                                  uint64 sector_count)
{
    uint64 end_sector = start_sector + sector_count;
    uint64 existing_end;
    int i;

    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (!xos_partitions[i].in_use ||
            xos_partitions[i].parent != parent) {
            continue;
        }

        existing_end = xos_partitions[i].start_sector +
                       xos_partitions[i].bdev.sector_count;
        if (start_sector < existing_end &&
            xos_partitions[i].start_sector < end_sector) {
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

static void xos_partition_rollback(xos_partition_t **created, int count)
{
    int i;

    for (i = count - 1; i >= 0; i--) {
        xos_partition_t *partition = created[i];

        xos_blockdev_unregister(&partition->bdev);
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
    uint32 minor;
    int ret;

    partition = xos_partition_alloc_slot();
    if (partition == NULL) {
        return -ENOSPC;
    }

    memset(partition, 0, sizeof(*partition));
    partition->parent = parent;
    partition->start_sector = start_sector;
    partition->type = type;
    partition->index = index;
    partition->in_use = 1;

    ret = xos_partition_name(partition->bdev.name,
                             sizeof(partition->bdev.name),
                             parent->name, index);
    if (ret < 0) {
        memset(partition, 0, sizeof(*partition));
        return ret;
    }


    minor = ((uint32)parent->devno & XOS_DEV_MINOR_MASK) + index;
    if (minor > XOS_DEV_MINOR_MASK) {
        memset(partition, 0, sizeof(*partition));
        return -ERANGE;
    }

    partition->bdev.devno =
        ((uint32)parent->devno & XOS_DEV_MAJOR_MASK) | minor;
    partition->bdev.sector_size = parent->sector_size;
    partition->bdev.sector_count = sector_count;
    partition->bdev.flags = parent->flags;
    partition->bdev.private_data = partition;
    partition->bdev.ops = &xos_partition_ops;

    xos_blockdev_get(parent);
    ret = xos_blockdev_register(&partition->bdev);
    if (ret < 0) {
        xos_blockdev_put(parent);
        memset(partition, 0, sizeof(*partition));
        return ret;
    }

    xos_partition_registered++;
    if (out != NULL) {
        *out = partition;
    }
    return 0;
}

int xos_partition_init(void)
{
    if (xos_partition_ready) {
        return 0;
    }

    memset(xos_partitions, 0, sizeof(xos_partitions));
    xos_spinlock_init(&xos_partition_lock);
    xos_partition_registered = 0;
    xos_partition_ready = 1;
    return 0;
}

int xos_mbr_scan(xos_block_device_t *parent)
{
    xos_partition_t *created[XOS_MBR_PRIMARY_PARTITIONS];
    uint8 mbr[XOS_MBR_SIZE];
    int created_count = 0;
    int existing_count;
    int entry_index;
    int ret;

    if (!xos_partition_ready || parent == NULL ||
        parent->sector_size < XOS_MBR_SIZE || !parent->registered) {
        return -EINVAL;
    }

    ret = xos_blockdev_read(parent, 0, mbr, sizeof(mbr));
    if (ret < 0) {
        return ret;
    }
    if (mbr[XOS_MBR_SIGNATURE_OFFSET] != XOS_MBR_SIGNATURE_0 ||
        mbr[XOS_MBR_SIGNATURE_OFFSET + 1] != XOS_MBR_SIGNATURE_1) {
        return -ENOEXEC;
    }

    xos_spinlock(&xos_partition_lock);
    existing_count = xos_partition_existing_count(parent);
    if (existing_count != 0) {
        xos_unspinlock(&xos_partition_lock);
        return existing_count;
    }


    for (entry_index = 0;
         entry_index < XOS_MBR_PRIMARY_PARTITIONS;
         entry_index++) {
        const uint8 *entry = mbr + XOS_MBR_PARTITION_OFFSET +
                             entry_index * XOS_MBR_PARTITION_SIZE;
        uint8 boot_flag = entry[0];
        uint8 type = entry[4];
        uint64 start_sector = xos_get_le32(entry + 8);
        uint64 sector_count = xos_get_le32(entry + 12);

        if (type == 0 || sector_count == 0) {
            continue;
        }
        if ((boot_flag != 0 && boot_flag != 0x80) ||
            xos_partition_is_extended(type) ||
            start_sector == 0 ||
            start_sector >= parent->sector_count ||
            sector_count > parent->sector_count - start_sector ||
            xos_partition_overlaps(parent, start_sector, sector_count)) {
            continue;
        }

        ret = xos_partition_register_one(parent, start_sector,
                                         sector_count, type,
                                         entry_index + 1,
                                         &created[created_count]);
        if (ret < 0) {
            goto fail;
        }
        created_count++;
    }

    xos_unspinlock(&xos_partition_lock);
    return created_count;

fail:
    xos_partition_rollback(created, created_count);
    xos_unspinlock(&xos_partition_lock);
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
    xos_partition_t *created[XOS_PARTITION_MAX];
    uint8 header_buf[XOS_BLOCKDEV_DEFAULT_SECTOR_SIZE];
    uint8 entry_buf[XOS_BLOCKDEV_DEFAULT_SECTOR_SIZE];
    uint64 entries_start_lba;
    uint32 entry_size;
    uint32 num_entries;
    int created_count = 0;
    int existing_count;
    int i;
    int ret;

    if (!xos_partition_ready || parent == NULL ||
        parent->sector_size < XOS_GPT_HEADER_SIZE ||
        !parent->registered) {
        return -EINVAL;
    }

    /* Read GPT header from LBA 1 */
    ret = xos_blockdev_read_sectors(parent, XOS_GPT_HEADER_LBA,
                                     header_buf, 1);
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

    xos_spinlock(&xos_partition_lock);
    existing_count = xos_partition_existing_count(parent);
    if (existing_count != 0) {
        xos_unspinlock(&xos_partition_lock);
        return existing_count;
    }

    for (i = 0; i < (int)num_entries && i < XOS_PARTITION_MAX; i++) {
        uint64 entry_lba = entries_start_lba +
                           (uint64)i * entry_size / parent->sector_size;
        uint64 first_lba;
        uint64 last_lba;
        uint64 sector_count;
        int j;
        int all_zero = 1;

        ret = xos_blockdev_read_sectors(parent, entry_lba,
                                         entry_buf, 1);
        if (ret < 0) {
            goto fail;
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
            last_lba >= parent->sector_count) {
            continue;
        }
        sector_count = last_lba - first_lba + 1;

        if (xos_partition_overlaps(parent, first_lba, sector_count)) {
            continue;
        }

        ret = xos_partition_register_one(parent, first_lba,
                                         sector_count, 0,
                                         i + 1,
                                         &created[created_count]);
        if (ret < 0) {
            goto fail;
        }
        created_count++;
    }

    xos_unspinlock(&xos_partition_lock);
    return created_count;

fail:
    xos_partition_rollback(created, created_count);
    xos_unspinlock(&xos_partition_lock);
    return ret;
}

/*
    Unified partition scan: try GPT first, fall back to MBR.
    Returns the number of partitions found (>= 0) or negative error.
*/
int xos_partition_scan(xos_block_device_t *parent)
{
    int ret;

    if (!xos_partition_ready || parent == NULL || !parent->registered) {
        return -EINVAL;
    }

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

    if (!xos_partition_ready) {
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
    int i;

    if (!xos_partition_ready || parent == NULL) {
        return -EINVAL;
    }

    xos_spinlock(&xos_partition_lock);
    for (i = 0; i < XOS_PARTITION_MAX; i++) {
        if (xos_partitions[i].in_use &&
            xos_partitions[i].parent == parent) {
            xos_blockdev_unregister(&xos_partitions[i].bdev);
            xos_blockdev_put(xos_partitions[i].parent);
            memset(&xos_partitions[i], 0, sizeof(xos_partitions[i]));
            xos_partition_registered--;
            removed++;
        }
    }
    xos_unspinlock(&xos_partition_lock);
    return removed;
}

void xos_partition_dump(xos_block_device_t *parent)
{
    int i;

    if (!xos_partition_ready || parent == NULL) {
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
                   xos_partitions[i].bdev.name,
                   (unsigned long)xos_partitions[i].start_sector,
                   (unsigned long)xos_partitions[i].bdev.sector_count,
                   xos_partitions[i].type);
        }
    }
    xos_unspinlock(&xos_partition_lock);
}
