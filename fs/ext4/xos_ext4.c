#include "types.h"
#include "list.h"
#include "error.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "bit_map.h"
#include "xos_cache.h"
#include "xos_vfs.h"
#include "fs.h"
#include "xos_char_dev.h"
#include "tick_timer.h"
#include "task.h"
#include "stat.h"
#include "path.h"
#include "xos_file.h"
#include "xos_fcntl.h"
#include "xnode.h"
#include "xos_block_dev.h"
#include "xos_ext4.h"

/*
    部分代码参考ext2
*/

typedef struct xos_ext4_inode_private {
    xos_ext4_sb_info_t *sbi;
    xos_ext4_inode_t disk_inode;
} xos_ext4_inode_private_t;

static xinode_ops_t xos_ext4_dir_inode_ops;
static xinode_ops_t xos_ext4_symlink_inode_ops;
static xfile_ops_t xos_ext4_file_ops;
static xfile_ops_t xos_ext4_dir_ops;
/*
 * ext4 filesystem type registration.
*/

static uint16 xos_ext4_get_le16(const uint8 *value)
{
    return (uint16)value[0] | ((uint16)value[1] << 8);
}

static uint32 xos_ext4_get_le32(const uint8 *value)
{
    return (uint32)value[0] |
           ((uint32)value[1] << 8) |
           ((uint32)value[2] << 16) |
           ((uint32)value[3] << 24);
}

static int xos_ext4_is_power_of_two(uint32 value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

int xos_ext4_read_superblock(struct xos_block_device *bdev,
                             void *buffer,
                             uint32 buffer_size)
{
    if (bdev == NULL || buffer == NULL ||
        buffer_size < 1024U) {
        return -EINVAL;
    }
    return xos_blockdev_read(bdev, XOS_EXT4_SUPERBLOCK_OFFSET,
                             buffer, buffer_size);
}

int xos_ext4_parse_superblock(struct xos_block_device *bdev,
                              const void *buffer,
                              uint32 buffer_size,
                              xos_ext4_sb_info_t *sbi)
{
    const uint8 *sb = buffer;
    uint64 capacity;
    uint64 group_blocks;
    uint32 log_block_size;
    uint32 block_count_lo;
    uint32 block_count_hi;
    uint32 free_blocks_lo;
    uint32 free_blocks_hi;

    if (bdev == NULL || sb == NULL || sbi == NULL ||
        buffer_size < XOS_EXT4_SUPERBLOCK_SIZE) {
        return -EINVAL;
    }
    if (xos_ext4_get_le16(sb + 56) != XOS_EXT4_SUPER_MAGIC) {
        return -EINVAL;
    }

    if (xos_ext4_get_le16(sb + 58) != 1U){
         return -EOPNOTSUPP;
    }
    memset(sbi, 0, sizeof(*sbi));
    sbi->bdev = bdev;
    sbi->inode_count = xos_ext4_get_le32(sb + 0);
    block_count_lo = xos_ext4_get_le32(sb + 4);
    free_blocks_lo = xos_ext4_get_le32(sb + 12);
    sbi->first_data_block = xos_ext4_get_le32(sb + 20);
    log_block_size = xos_ext4_get_le32(sb + 24);
    sbi->blocks_per_group = xos_ext4_get_le32(sb + 32);
    sbi->inodes_per_group = xos_ext4_get_le32(sb + 40);
    sbi->feature_compat = xos_ext4_get_le32(sb + 92);
    sbi->feature_incompat = xos_ext4_get_le32(sb + 96);
    sbi->feature_ro_compat = xos_ext4_get_le32(sb + 100);
    sbi->first_inode = xos_ext4_get_le32(sb + 84);
    sbi->inode_size = xos_ext4_get_le16(sb + 88);
    sbi->desc_size = xos_ext4_get_le16(sb + 254);

    if (log_block_size > 20) {
        return -EINVAL;
    }
    sbi->block_size = 1024U << log_block_size;
    if (sbi->block_size < 1024U || sbi->block_size > 4096U ||
        !xos_ext4_is_power_of_two(sbi->block_size) ||
        sbi->blocks_per_group == 0 || sbi->inodes_per_group == 0 ||
        sbi->blocks_per_group > sbi->block_size * 8U ||
        sbi->inodes_per_group > sbi->block_size * 8U ||
        sbi->inode_count == 0) {
        return -EINVAL;
    }
    if (sbi->first_data_block != (sbi->block_size == 1024U ? 1U : 0U)){
        return -EINVAL;
    }
    
    sbi->block_bits = 0;
    while ((1U << sbi->block_bits) < sbi->block_size) {
        sbi->block_bits++;
    }
    if (sbi->inode_size < 128 ||
        sbi->inode_size > sbi->block_size ||
        (sbi->inode_size & 3U) != 0) {
        return -EINVAL;
    }
    if (sbi->desc_size == 0) {
        sbi->desc_size = 32;
    }
    if (sbi->desc_size < 32 || sbi->desc_size > sbi->block_size ||
        /* 64-byte descriptors need the unsupported 64bit layout. */
        sbi->desc_size != 32U ||
        (sbi->desc_size & 7U) != 0 ||
        sbi->block_size % sbi->desc_size != 0) {
        return -EINVAL;
    }

    /* flex_bg only changes where group metadata lives; descriptors carry
     * absolute inode-table block numbers. */
    if (sbi->feature_incompat & ~(XOS_EXT4_FEATURE_INCOMPAT_FILETYPE |
                                  XOS_EXT4_FEATURE_INCOMPAT_EXTENTS |
                                  XOS_EXT4_FEATURE_INCOMPAT_FLEX_BG)) {
        return -EOPNOTSUPP;
    }
    if (sbi->feature_ro_compat & ~XOS_EXT4_RO_COMPAT_SUPPORTED){
        return -EOPNOTSUPP;
    }
    
    block_count_hi = xos_ext4_get_le32(sb + 336);
    free_blocks_hi = xos_ext4_get_le32(sb + 344);
    /* The 64bit incompat feature is not supported by this reader. */
    if (block_count_hi != 0 || free_blocks_hi != 0) return -EINVAL;
    sbi->block_count = (uint64)block_count_lo |
                       ((uint64)block_count_hi << 32);
    sbi->free_block_count = (uint64)free_blocks_lo |
                            ((uint64)free_blocks_hi << 32);
    if (sbi->block_count <= sbi->first_data_block) {
        return -EINVAL;
    }
    if ((uint64)sbi->first_data_block + 1 >= sbi->block_count) {
        return -EINVAL;
    }
    sbi->group_desc_block = (uint64)sbi->first_data_block + 1;

    group_blocks = sbi->block_count - sbi->first_data_block;
    if (group_blocks / sbi->blocks_per_group +
        (group_blocks % sbi->blocks_per_group != 0) > ~(uint32)0)
        return -EOVERFLOW;
    sbi->group_count = (uint32)(group_blocks / sbi->blocks_per_group +
                       (group_blocks % sbi->blocks_per_group != 0));
    if ((uint64)sbi->group_count * sbi->inodes_per_group < sbi->inode_count){
        return -EINVAL;
    }
    
    if (sbi->group_count == 0 ||
        sbi->group_count > (~(uint32)0) / sbi->desc_size) {
        return -EOVERFLOW;
    }
    if (sbi->block_count > (~(uint64)0) / sbi->block_size) {
        return -EOVERFLOW;
    }
    capacity = sbi->block_count * (uint64)sbi->block_size;
    if (capacity > xos_blockdev_capacity_bytes(bdev)) {
        return -EINVAL;
    }
    return 0;
}

int xos_ext4_read_block(const xos_ext4_sb_info_t *sbi,
                        uint64 block_no,
                        void *buffer)
{
    uint64 offset;

    if (sbi == NULL || sbi->bdev == NULL || buffer == NULL ||
        sbi->block_size == 0 || block_no >= sbi->block_count ||
        block_no > (~(uint64)0) / sbi->block_size) {
        return -EINVAL;
    }
    offset = block_no * (uint64)sbi->block_size;
    return xos_blockdev_read(sbi->bdev, offset, buffer, sbi->block_size);
}

int xos_ext4_read_group_desc(const xos_ext4_sb_info_t *sbi,
                             uint32 group,
                             xos_ext4_group_desc_t *desc)
{
    uint8 raw[64];
    uint64 offset;
    uint64 inode_table_blocks;
    int ret;

    if (sbi == NULL || desc == NULL || group >= sbi->group_count ||
        sbi->desc_size > sizeof(raw)) {
        return -EINVAL;
    }
    if (sbi->group_desc_block > (~(uint64)0) / sbi->block_size) {
        return -EOVERFLOW;
    }
    offset = sbi->group_desc_block * (uint64)sbi->block_size;
    if ((uint64)group > (~(uint64)0 - offset) / sbi->desc_size) {
        return -EOVERFLOW;
    }
    offset += (uint64)group * sbi->desc_size;
    ret = xos_blockdev_read(sbi->bdev, offset, raw, sbi->desc_size);
    if (ret < 0) {
        return ret;
    }

    memset(desc, 0, sizeof(*desc));
    desc->block_bitmap = xos_ext4_get_le32(raw + 0);
    desc->inode_bitmap = xos_ext4_get_le32(raw + 4);
    desc->inode_table = xos_ext4_get_le32(raw + 8);
    desc->free_blocks = xos_ext4_get_le16(raw + 12);
    desc->free_inodes = xos_ext4_get_le16(raw + 14);
    desc->used_dirs = xos_ext4_get_le16(raw + 16);
    inode_table_blocks = ((uint64)sbi->inodes_per_group * sbi->inode_size +
                          sbi->block_size - 1) / sbi->block_size;
    if (desc->block_bitmap >= sbi->block_count ||
        desc->inode_bitmap >= sbi->block_count ||
        desc->inode_table >= sbi->block_count ||
        inode_table_blocks > sbi->block_count - desc->inode_table) {
        return -EINVAL;
    }
    return 0;
}

int xos_ext4_read_inode(const xos_ext4_sb_info_t *sbi,
                        uint32 inode_no,
                        xos_ext4_inode_t *inode)
{
    uint8 *raw;
    xos_ext4_group_desc_t desc;
    uint64 group;
    uint64 index;
    uint64 table_offset;
    uint64 inode_offset;
    uint32 inode_size;
    int i;
    int ret;

    if (sbi == NULL || inode == NULL || inode_no == 0 ||
        inode_no > sbi->inode_count || sbi->inode_size > 4096U) {
        return -EINVAL;
    }
    group = ((uint64)inode_no - 1) / sbi->inodes_per_group;
    index = ((uint64)inode_no - 1) % sbi->inodes_per_group;
    if (group >= sbi->group_count) {
        return -EINVAL;
    }

    ret = xos_ext4_read_group_desc(sbi, (uint32)group, &desc);
    if (ret < 0) {
        return ret;
    }
    inode_size = sbi->inode_size;
    if ((uint64)desc.inode_table > (~(uint64)0) / sbi->block_size) {
        return -EOVERFLOW;
    }
    table_offset = (uint64)desc.inode_table * sbi->block_size;
    if (index > (~(uint64)0 - table_offset) / inode_size) {
        return -EOVERFLOW;
    }
    inode_offset = table_offset + index * inode_size;
    raw = xos_kmalloc(inode_size);
    if (raw == NULL){
        return -ENOMEM;
    } 
    ret = xos_blockdev_read(sbi->bdev, inode_offset, raw, inode_size);
    if (ret < 0) {
        xos_kfree(raw);
        return ret;
    }

    memset(inode, 0, sizeof(*inode));
    inode->mode = xos_ext4_get_le16(raw + 0);
    inode->uid = xos_ext4_get_le16(raw + 2);
    inode->size = xos_ext4_get_le32(raw + 4);
    inode->atime = xos_ext4_get_le32(raw + 8);
    inode->ctime = xos_ext4_get_le32(raw + 12);
    inode->mtime = xos_ext4_get_le32(raw + 16);
    inode->dtime = xos_ext4_get_le32(raw + 20);
    inode->gid = xos_ext4_get_le16(raw + 24);
    inode->links_count = xos_ext4_get_le16(raw + 26);
    inode->blocks = xos_ext4_get_le32(raw + 28);
    inode->flags = xos_ext4_get_le32(raw + 32);
    inode->generation = xos_ext4_get_le32(raw + 100);
    inode->file_acl = xos_ext4_get_le32(raw + 104);
    inode->size_high = xos_ext4_get_le32(raw + 108);
    for (i = 0; i < 15; i++) {
        inode->block[i] = xos_ext4_get_le32(raw + 40 +
                                            i * sizeof(uint32));
    }
    xos_kfree(raw);
    return 0;
}

static int xos_ext4_extent_root_header(const xos_ext4_inode_t *inode,
                                       uint16 *entries,
                                       uint16 *max_entries,
                                       uint16 *depth)
{
    uint32 header;
    uint32 limits;

    if (inode == NULL || entries == NULL || max_entries == NULL ||
        depth == NULL) {
        return -EINVAL;
    }
    header = inode->block[0];
    limits = inode->block[1];
    if ((header & 0xffffU) != XOS_EXT4_EXTENT_MAGIC) {
        return -EINVAL;
    }
    *entries = (uint16)(header >> 16);
    *max_entries = (uint16)(limits & 0xffffU);
    *depth = (uint16)(limits >> 16);
    if (*entries > *max_entries || *max_entries > 4 || *depth > 5) {
        return -EINVAL;
    }
    return 0;
}

static uint32 xos_ext4_extent_length(uint16 encoded)
{
    /* 0x8000 is initialized; only values above it describe unwritten blocks. */
    return encoded > 0x8000U ? encoded - 0x8000U : encoded;
}

static int xos_ext4_extent_map_node(const xos_ext4_sb_info_t *sbi,
                                    uint64 block_no,
                                    uint16 depth,
                                    uint32 file_block,
                                    uint64 *disk_block)
{
    uint8 *raw;
    uint16 entries;
    uint16 max_entries;
    uint16 node_depth;
    uint64 selected_block = 0;
    uint32 selected_leaf_lo = 0;
    uint16 selected_leaf_hi = 0;
    int i;
    int ret;

    raw = xos_kmalloc(sbi->block_size);
    if (raw == NULL) return -ENOMEM;
    ret = xos_ext4_read_block(sbi, block_no, raw);
    if (ret < 0) goto out;
    if (xos_ext4_get_le16(raw) != XOS_EXT4_EXTENT_MAGIC) {
        ret = -EINVAL;
        goto out;
    }
    entries = xos_ext4_get_le16(raw + 2);
    max_entries = xos_ext4_get_le16(raw + 4);
    node_depth = xos_ext4_get_le16(raw + 6);
    if (node_depth != depth || entries > max_entries ||
        max_entries > (sbi->block_size - 12U) / 12U) {
        ret = -EINVAL;
        goto out;
    }

    if (depth == 0) {
        for (i = 0; i < entries; i++) {
            const uint8 *extent = raw + 12 + i * 12;
            uint32 first = xos_ext4_get_le32(extent);
            uint16 length = xos_ext4_get_le16(extent + 4);
            uint64 start;

            if (file_block < first) {
                break;
            }
            if (length == 0) {
                ret = -EINVAL;
                goto out;
            }
            if ((uint64)file_block < (uint64)first +
                                      xos_ext4_extent_length(length)) {
                if (length > 0x8000U) {
                    ret = -ENOENT;
                    goto out;
                }
                start = (uint64)xos_ext4_get_le16(extent + 6) << 32;
                start |= xos_ext4_get_le32(extent + 8);
                if (start >= sbi->block_count ||
                    (uint64)(file_block - first) >
                        sbi->block_count - start - 1) {
                    ret = -EINVAL;
                    goto out;
                }
                *disk_block = start + (file_block - first);
                ret = 0;
                goto out;
            }
        }
        ret = -ENOENT;
        goto out;
    }

    for (i = 0; i < entries; i++) {
        const uint8 *index = raw + 12 + i * 12;
        uint32 logical = xos_ext4_get_le32(index);

        if (logical > file_block) {
            break;
        }
        selected_leaf_lo = xos_ext4_get_le32(index + 4);
        selected_leaf_hi = xos_ext4_get_le16(index + 8);
    }
    if (i == 0) {
        ret = -ENOENT;
        goto out;
    }
    selected_block = selected_leaf_lo |
                     ((uint64)selected_leaf_hi << 32);
    xos_kfree(raw);
    return xos_ext4_extent_map_node(sbi, selected_block, depth - 1,
                                    file_block, disk_block);
out:
    xos_kfree(raw);
    return ret;
}

static int xos_ext4_map_indirect(const xos_ext4_sb_info_t *sbi,
                                uint32 block_no, uint32 index,
                                unsigned int depth, uint64 *disk_block)
{
    uint8 *raw;
    uint32 pointers = sbi->block_size / sizeof(uint32);
    uint32 divisor = 1;
    uint32 next;
    unsigned int i;
    int ret;

    for (i = 1; i < depth; i++) divisor *= pointers;
    raw = xos_kmalloc(sbi->block_size);
    if (raw == NULL) {
        return -ENOMEM;
    }
    while (depth != 0) {
        if (block_no == 0) { ret = -ENOENT; goto out; }
        if (block_no >= sbi->block_count) { ret = -EINVAL; goto out; }
        ret = xos_ext4_read_block(sbi, block_no, raw);
        if (ret < 0) goto out;
        next = xos_ext4_get_le32(raw + (index / divisor) * 4U);
        index %= divisor;
        block_no = next;
        depth--;
        if (depth != 0) divisor /= pointers;
    }
    if (block_no == 0) {
        ret = -ENOENT;
    }else if (block_no >= sbi->block_count){
        ret = -EINVAL;
    }else {
        *disk_block = block_no;
        ret = 0;
    }
out:
    xos_kfree(raw);
    return ret;
}

int xos_ext4_map_file_block(const xos_ext4_sb_info_t *sbi,
                            const xos_ext4_inode_t *inode,
                            uint32 file_block,
                            uint64 *disk_block)
{
    uint16 entries;
    uint16 max_entries;
    uint16 depth;
    int i;
    int ret;

    if (sbi == NULL || inode == NULL || disk_block == NULL ||
        sbi->block_size < 1024U || sbi->block_size > 4096U){
            return -EINVAL;
        }
       
    if (!(inode->flags & XOS_EXT4_INODE_FLAG_EXTENTS)) {
        uint64 index = file_block;
        uint64 pointers = sbi->block_size / sizeof(uint32);
        uint64 span = pointers;
        if (index < 12U) {
            uint32 physical = inode->block[index];
            if (physical == 0) return -ENOENT;
            if (physical >= sbi->block_count) return -EINVAL;
            *disk_block = physical;
            return 0;
        }
        index -= 12U;
        for (i = 1; i <= 3; i++) {
            if (index < span){
                return xos_ext4_map_indirect(sbi, inode->block[11 + i],
                                             (uint32)index, i, disk_block);
            }               
            index -= span;
            span *= pointers;
        }
        return -EFBIG;
    }
    ret = xos_ext4_extent_root_header(inode, &entries, &max_entries,
                                      &depth);
    if (ret < 0) {
        return ret;
    }
    if (depth == 0) {
        for (i = 0; i < entries; i++) {
            uint32 first = inode->block[3 + i * 3];
            uint32 limits = inode->block[4 + i * 3];
            uint16 length = (uint16)(limits & 0xffffU);
            uint64 start;

            if (file_block < first) {
                break;
            }
            if (length == 0) {
                return -EINVAL;
            }
            if ((uint64)file_block < (uint64)first +
                                      xos_ext4_extent_length(length)) {
                if (length > 0x8000U) {
                    return -ENOENT;
                }
                start = (uint64)(limits >> 16) << 32;
                start |= inode->block[5 + i * 3];
                if (start >= sbi->block_count ||
                    (uint64)(file_block - first) >
                        sbi->block_count - start - 1) {
                    return -EINVAL;
                }
                *disk_block = start + (file_block - first);
                return 0;
            }
        }
        return -ENOENT;
    }

    for (i = 0; i < entries; i++) {
        uint32 logical = inode->block[3 + i * 3];

        if (logical > file_block) {
            break;
        }
    }
    if (i == 0) {
        return -ENOENT;
    }
    return xos_ext4_extent_map_node(sbi,
                                    inode->block[4 + (i - 1) * 3] |((uint64)(inode->block[5 +(i - 1) * 3] & 0xffffU) << 32),
                                    depth - 1, file_block, disk_block);
}



static uint32 xos_ext4_inode_size(const xos_ext4_inode_t *inode)
{
    /* 64-bit inode sizes are not representable by the current XOS inode. */
    return inode->size;
}

int xos_ext4_read_inode_data(const xos_ext4_sb_info_t *sbi,
                             const xos_ext4_inode_t *inode,
                             uint64 offset,
                             void *buffer,
                             uint32 length)
{
    uint8 *block;
    uint8 *dst = buffer;
    uint32 file_size;
    uint32 done = 0;
    int ret;

    if (sbi == NULL || inode == NULL || buffer == NULL ||
        sbi->block_size == 0 || sbi->block_size > 4096U) {
        return -EINVAL;
    }
    file_size = xos_ext4_inode_size(inode);
    if (offset > file_size || (uint64)length > file_size - offset) {
        return -EINVAL;
    }
    if (length == 0){
        return 0;
    } 
    block = xos_kmalloc(sbi->block_size);
    if (block == NULL) return -ENOMEM;
    while (done < length) {
        uint64 absolute = offset + done;
        uint32 file_block = (uint32)(absolute / sbi->block_size);
        uint32 block_offset = (uint32)(absolute % sbi->block_size);
        uint32 chunk = sbi->block_size - block_offset;
        uint64 disk_block;

        if (chunk > length - done) {
            chunk = length - done;
        }
        ret = xos_ext4_map_file_block(sbi, inode, file_block, &disk_block);
        if (ret == -ENOENT) {
            memset(dst + done, 0, chunk);
        } else if (ret < 0) {
            goto out;
        } else {
            ret = xos_ext4_read_block(sbi, disk_block, block);
            if (ret < 0) {
                goto out;
            }
            memcpy(dst + done, block + block_offset, chunk);
        }
        done += chunk;
    }
    ret = 0;
out:
    xos_kfree(block);
    return ret;
}

static int xos_ext4_write_inode_data_partial(xos_ext4_sb_info_t *sbi,
                                             const xos_ext4_inode_t *inode,
                                             uint64 offset, const void *buffer,
                                             uint32 length, uint32 *written)
{
    const uint8 *src = buffer;
    uint8 *block;
    uint32 done = 0;
    uint64 disk_block;
    int ret;

    *written = 0;
    if (!sbi || !inode || !buffer || !S_ISREG(inode->mode))
        return -EINVAL;
    if (!sbi->writable || !sbi->bdev ||
        (sbi->bdev->flags & XOS_BLOCKDEV_READ_ONLY)) return -EROFS;
    if (sbi->block_size < 1024U || sbi->block_size > 4096U) return -EINVAL;
    if (offset > inode->size || (uint64)length > inode->size - offset)
        return -EFBIG;
    if (!length) return 0;

    mutext_lock(&sbi->write_lock);
    /* Validate the entire range before modifying any block. Holes and
     * uninitialized extents require allocation/conversion, not supported. */
    for (uint64 pos = offset; pos < offset + length; ) {
        uint32 chunk = sbi->block_size - (uint32)(pos % sbi->block_size);
        if (chunk > offset + length - pos) chunk = offset + length - pos;
        ret = xos_ext4_map_file_block(sbi, inode,
                                      (uint32)(pos / sbi->block_size),
                                      &disk_block);
        if (ret < 0) {
            if (ret == -ENOENT) ret = -EOPNOTSUPP;
            goto unlock;
        }
        pos += chunk;
    }
    block = xos_kmalloc(sbi->block_size);
    if (!block) { ret = -ENOMEM; goto unlock; }
    while (done < length) {
        uint64 pos = offset + done;
        uint32 skip = (uint32)(pos % sbi->block_size);
        uint32 chunk = sbi->block_size - skip;
        if (chunk > length - done) chunk = length - done;
        ret = xos_ext4_map_file_block(sbi, inode,
                                      (uint32)(pos / sbi->block_size),
                                      &disk_block);
        if (ret < 0) goto partial_failure;
        if (skip || chunk != sbi->block_size) {
            ret = xos_ext4_read_block(sbi, disk_block, block);
            if (ret < 0) goto partial_failure;
            memcpy(block + skip, src + done, chunk);
        } else {
            memcpy(block, src + done, chunk);
        }
        ret = xos_blockdev_write(sbi->bdev,
                                 disk_block * (uint64)sbi->block_size,
                                 block, sbi->block_size);
        if (ret < 0) goto partial_failure;
        done += chunk;
    }
    ret = xos_blockdev_flush(sbi->bdev);
    goto free_block;
partial_failure:
    /* Keep the completed prefix visible to the caller only after flushing. */
    if (done != 0) {
        if (xos_blockdev_flush(sbi->bdev) < 0)
            ret = -EIO;
        else
            *written = done;
    }
free_block:
    if (ret == 0)
        *written = done;
    xos_kfree(block);
unlock:
    mutext_unlock(&sbi->write_lock);
    return ret;
}

int xos_ext4_write_inode_data(xos_ext4_sb_info_t *sbi,
                              const xos_ext4_inode_t *inode,
                              uint64 offset, const void *buffer, uint32 length)
{
    uint32 written;

    return xos_ext4_write_inode_data_partial(sbi, inode, offset, buffer,
                                              length, &written);
}

int xos_ext4_read_symlink(const xos_ext4_sb_info_t *sbi,
                          const xos_ext4_inode_t *inode,
                          char *buffer, uint32 capacity)
{
    int ret;

    if (sbi == NULL || inode == NULL || buffer == NULL ||
        !S_ISLNK(inode->mode) || inode->size == 0 ||
        inode->size >= capacity || inode->size > 4096U){
             return -EINVAL;
    }
       
    if (inode->size <= sizeof(inode->block) && inode->blocks == 0) {
        /* i_block contains inline bytes, not disk block numbers. */
        memcpy(buffer, inode->block, inode->size);
    } else {
        ret = xos_ext4_read_inode_data(sbi, inode, 0, buffer, inode->size);
        if (ret < 0){
            return ret;
        } 
       
    }
    buffer[inode->size] = '\0';
    return (int)inode->size;
}

static int xos_ext4_dirent_valid(const uint8 *raw, uint32 offset,
                                 uint32 block_size, uint32 remaining)
{
    uint16 rec_len;
    uint8 name_len;

    if (remaining < 8 || offset > block_size - 8) {
        return 0;
    }
    rec_len = xos_ext4_get_le16(raw + offset + 4);
    name_len = raw[offset + 6];
    return rec_len >= 8 && (rec_len & 3U) == 0 &&
           rec_len <= remaining && name_len <= rec_len - 8;
}

static uint32 xos_ext4_dirent_type(uint8 type, uint16 mode)
{
    switch (type) {
    case XOS_EXT4_FT_DIR: return DT_DIR;
    case XOS_EXT4_FT_REG_FILE: return DT_REG;
    case XOS_EXT4_FT_SYMLINK: return DT_LNK;
    case XOS_EXT4_FT_CHRDEV: return DT_CHR;
    case XOS_EXT4_FT_BLKDEV: return DT_BLK;
    case XOS_EXT4_FT_FIFO: return DT_FIFO;
    case XOS_EXT4_FT_SOCK: return DT_SOCK;
    default:
        if (S_ISDIR(mode)) return DT_DIR;
        if (S_ISREG(mode)) return DT_REG;
        if (S_ISLNK(mode)) return DT_LNK;
        if (S_ISCHR(mode)) return DT_CHR;
        if (S_ISBLK(mode)) return DT_BLK;
        if (S_ISFIFO(mode)) return DT_FIFO;
        return DT_UNKNOWN;
    }
}

static int xos_ext4_alloc_inode(xsuperblock *super, uint32 inode_no,
                                const xos_ext4_inode_t *disk,
                                xinode **out)
{
    xinode *node;
    xos_ext4_inode_private_t *private_data;

    if (S_ISREG(disk->mode) && disk->size_high != 0)
        return -EFBIG;
    node = alloc_cache_xinode();
    if (node == NULL) return -ENOMEM;
    memset(node, 0, sizeof(*node));
    private_data = xos_kmalloc(sizeof(*private_data));
    if (private_data == NULL) {
        free_cache_xinode(node);
        return -ENOMEM;
    }
    private_data->sbi = super->private_data;
    private_data->disk_inode = *disk;
    node->p_super = super;
    node->node_num = inode_no;
    node->node_mode = disk->mode;
    node->file_size = disk->size;
    node->block_size = super->block_size;
    node->block_count = disk->blocks;
    node->uid = disk->uid;
    node->gid = disk->gid;
    node->link_cnt = disk->links_count;
    node->private_data = private_data;
    node->ref_count = 1;
    node->file_ops = S_ISDIR(disk->mode) ? &xos_ext4_dir_ops : &xos_ext4_file_ops;
    node->inode_ops = S_ISDIR(disk->mode) ? &xos_ext4_dir_inode_ops :
                      S_ISLNK(disk->mode) ? &xos_ext4_symlink_inode_ops : NULL;
    *out = node;
    return 0;
}

static int xos_ext4_lookup_inode(xinode *parent_node,
                                 const char *name, uint32 name_len,
                                 uint32 *inode_no)
{
    xos_ext4_inode_private_t *private_data = parent_node->private_data;
    uint8 *block;
    uint32 size = parent_node->file_size;
    uint32 offset = 0;
    int ret;

    if (private_data == NULL || name == NULL || name_len == 0 ||
        name_len >= XOS_DENTRY_NAME_MAX ||
        !S_ISDIR(parent_node->node_mode)) return -EINVAL;
    block = xos_kmalloc(private_data->sbi->block_size);
    if (block == NULL) return -ENOMEM;
    while (offset < size) {
        uint32 block_offset = offset % private_data->sbi->block_size;
        uint32 block_left = private_data->sbi->block_size - block_offset;
        uint32 read_len = block_left;
        uint32 pos = 0;
        if (read_len > size - offset) read_len = size - offset;
        memset(block, 0, private_data->sbi->block_size);
        ret = xos_ext4_read_inode_data(private_data->sbi,
                                       &private_data->disk_inode,
                                       offset - block_offset, block,
                                       read_len);
        if (ret < 0) goto out;
        while (pos + 8 <= read_len) {
            uint8 *entry = block + pos;
            uint16 rec_len;
            uint8 entry_name_len;
            uint32 entry_inode;
            if (!xos_ext4_dirent_valid(block, pos,
                                       private_data->sbi->block_size,
                                       read_len - pos)) {
                ret = -EIO;
                goto out;
            }
            rec_len = xos_ext4_get_le16(entry + 4);
            entry_name_len = entry[6];
            entry_inode = xos_ext4_get_le32(entry);
            if (entry_inode != 0 && entry_name_len == name_len &&
                memcmp(entry + 8, name, name_len) == 0) {
                if (entry_inode > private_data->sbi->inode_count) {
                    ret = -EIO;
                    goto out;
                }
                *inode_no = entry_inode;
                ret = 0;
                goto out;
            }
            pos += rec_len;
        }
        if (pos != read_len && read_len != private_data->sbi->block_size) {
            ret = -EIO;
            goto out;
        }
        offset += read_len;
    }
    ret = -ENOENT;
out:
    xos_kfree(block);
    return ret;
}

static xdentry *xos_ext4_lookup(xinode *parent_node, xdentry *parent_dentry,
                                struct fs_path_lookup *lookup_path)
{
    path_name_t *name;
    uint32 inode_no;
    xos_ext4_inode_t disk_inode;
    xinode *child_inode;
    xdentry *child_dentry;
    int ret;

    if (parent_node == NULL || parent_dentry == NULL || lookup_path == NULL ||
        lookup_path->cur_path.path_name == NULL) return NULL;
    name = &lookup_path->cur_path;
    ret = xos_ext4_lookup_inode(parent_node,
                                (const char *)name->path_name, name->len,
                                &inode_no);
    if (ret < 0) return NULL;
    ret = xos_ext4_read_inode(((xos_ext4_inode_private_t *)parent_node->private_data)->sbi,
                              inode_no, &disk_inode);
    if (ret < 0) return NULL;
    child_dentry = find_in_pdentry(parent_dentry, name);
    if (child_dentry != NULL) return child_dentry;
    ret = xos_ext4_alloc_inode(parent_node->p_super, inode_no, &disk_inode,
                               &child_inode);
    if (ret < 0) return NULL;
    child_dentry = xdentry_cache_alloc_init(parent_dentry, name);
    if (child_dentry == NULL) {
        xos_kfree(child_inode->private_data);
        free_cache_xinode(child_inode);
        return NULL;
    }
    child_dentry->file_node = child_inode;
    child_dentry->xsb = parent_node->p_super;
    return child_dentry;
}

static int xos_ext4_open(xinode *node, struct file *filp)
{
    if (node == NULL || filp == NULL) return -EINVAL;
    if ((filp->f_flags & O_ACCMODE) != O_RDONLY &&
        IS_RDONLY(node)) return -EROFS;
    if (filp->f_flags & O_TRUNC) return -EOPNOTSUPP;
    if ((filp->f_flags & O_ACCMODE) != O_RDONLY &&
        !S_ISREG(node->node_mode)) return -EISDIR;
    filp->private_data = node;
    return 0;
}

static int xos_ext4_release(xinode *node, struct file *filp)
{
    (void)node;
    if (filp != NULL) filp->private_data = NULL;
    return 0;
}

static long xos_ext4_read(struct file *filp, char *buffer, size_t count,
                          long *ppos)
{
    xinode *node;
    xos_ext4_inode_private_t *private_data;
    uint32 available;
    int ret;

    if (filp == NULL || filp->f_dentry == NULL || buffer == NULL ||
        ppos == NULL || *ppos < 0) return -EINVAL;
    node = filp->f_dentry->file_node;
    if (node == NULL || !S_ISREG(node->node_mode)) return -EISDIR;
    if ((uint64)*ppos >= node->file_size) return 0;
    available = node->file_size - (uint32)*ppos;
    if (count > available) count = available;
    private_data = node->private_data;
    if (private_data == NULL) return -EIO;
    ret = xos_ext4_read_inode_data(private_data->sbi,
                                   &private_data->disk_inode,
                                   (uint64)*ppos, buffer, (uint32)count);
    if (ret < 0) return ret;
    *ppos += count;
    filp->f_pos = *ppos;
    return (long)count;
}

static long xos_ext4_write(struct file *filp, const char *buffer, size_t count,
                            long *ppos)
{
    xinode *node;
    xos_ext4_inode_private_t *private_data;
    uint32 written;
    int ret;

    if (!filp || !filp->f_dentry || !buffer || !ppos || *ppos < 0)
        return -EINVAL;
    node = filp->f_dentry->file_node;
    if (!node || !S_ISREG(node->node_mode)) return -EISDIR;
    if (IS_RDONLY(node)) return -EROFS;
    if (count > node->file_size || (uint64)*ppos > node->file_size ||
        count > node->file_size - (uint64)*ppos) return -EFBIG;
    private_data = node->private_data;
    if (!private_data) return -EIO;
    ret = xos_ext4_write_inode_data_partial(private_data->sbi,
                                             &private_data->disk_inode,
                                             (uint64)*ppos, buffer,
                                             (uint32)count, &written);
    if (ret < 0 && written == 0) return ret;
    *ppos += written;
    filp->f_pos = *ppos;
    return (long)written;
}

static loff_t xos_ext4_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t position;
    xinode *node;

    if (filp == NULL || filp->f_dentry == NULL ||
        filp->f_dentry->file_node == NULL) return -EINVAL;
    node = filp->f_dentry->file_node;
    if (whence == SEEK_SET) position = offset;
    else if (whence == SEEK_CUR) position = filp->f_pos + offset;
    else if (whence == SEEK_END) position = (loff_t)node->file_size + offset;
    else return -EINVAL;
    if (position < 0 || position > 0xffffffffLL) return -EINVAL;
    filp->f_pos = position;
    return position;
}

static int xos_ext4_readdir(struct file *filp, void *buf, filldir_t filldir)
{
    xinode *node;
    xos_ext4_inode_private_t *private_data;
    uint8 *block;
    uint64 disk_offset;
    uint32 size;
    int ret;

    if (filp == NULL || filp->f_dentry == NULL || filldir == NULL ||
        filp->f_dentry->file_node == NULL) return -EINVAL;
    if (filp->f_pos < 0) return -EINVAL;
    node = filp->f_dentry->file_node;
    if (!S_ISDIR(node->node_mode)) return -ENOTDIR;
    private_data = node->private_data;
    if (private_data == NULL || private_data->sbi->block_size > 4096U)
        return -EIO;
    if (filp->f_pos == 0) {
        if (filldir(buf, ".", 1, 0, node->node_num, DT_DIR) < 0) return 0;
        filp->f_pos = 1;
    }
    if (filp->f_pos == 1) {
        if (filldir(buf, "..", 2, 1, parent_xinode(filp->f_dentry), DT_DIR) < 0)
            return 0;
        filp->f_pos = 2;
    }
    block = xos_kmalloc(private_data->sbi->block_size);
    if (block == NULL) return -ENOMEM;
    disk_offset = (uint64)filp->f_pos - 2;
    while (disk_offset < node->file_size) {
        uint32 block_offset = disk_offset % private_data->sbi->block_size;
        uint32 block_base = (uint32)(disk_offset - block_offset);
        uint32 pos = block_offset;
        uint32 limit = private_data->sbi->block_size;
        if (limit > node->file_size - block_base) limit = node->file_size - block_base;
        memset(block, 0, private_data->sbi->block_size);
        size = private_data->sbi->block_size;
        if (size > node->file_size - block_base) size = node->file_size - block_base;
        ret = xos_ext4_read_inode_data(private_data->sbi,
                                       &private_data->disk_inode,
                                       block_base, block, size);
        if (ret < 0){
            goto out;
        }

        while (pos + 8 <= limit) {
            uint16 rec_len;
            uint8 name_len;
            uint32 inode_no;
            uint32 type;
            xos_ext4_inode_t entry_inode;
            if (!xos_ext4_dirent_valid(block, pos,
                                       private_data->sbi->block_size,
                                       limit - pos)) {
                ret = -EIO;
                goto out;
            }
            rec_len = xos_ext4_get_le16(block + pos + 4);
            name_len = block[pos + 6];
            inode_no = xos_ext4_get_le32(block + pos);
            if (inode_no > private_data->sbi->inode_count) {
                ret = -EIO;
                goto out;
            }
            type = DT_UNKNOWN;
            if (inode_no != 0) {
                if (private_data->sbi->feature_incompat &
                    XOS_EXT4_FEATURE_INCOMPAT_FILETYPE)
                    type = xos_ext4_dirent_type(block[pos + 7], 0);
                if (type == DT_UNKNOWN) {
                    ret = xos_ext4_read_inode(private_data->sbi, inode_no,
                                               &entry_inode);
                    if (ret < 0) goto out;
                    type = xos_ext4_dirent_type(0, entry_inode.mode);
                }
            }
            disk_offset = block_base + pos + rec_len;
            if (inode_no != 0 &&
                !(name_len == 1 && block[pos + 8] == '.') &&
                !(name_len == 2 && block[pos + 8] == '.' &&
                  block[pos + 9] == '.') &&
                filldir(buf, (const char *)(block + pos + 8), name_len,
                        (long)(disk_offset + 2), inode_no, type) < 0) {
                ret = 0;
                goto out;
            }
            filp->f_pos = (long)(disk_offset + 2);
            pos += rec_len;
        }
        if (pos != limit && limit != private_data->sbi->block_size) {
            ret = -EIO;
            goto out;
        }
        if (block_base + limit <= block_base) {
            ret = -EIO;
            goto out;
        }
        disk_offset = block_base + limit;
        filp->f_pos = (long)(disk_offset + 2);
    }
    ret = 0;
out:
    xos_kfree(block);
    return ret;
}

static xinode_ops_t xos_ext4_dir_inode_ops = {
    .lookup = xos_ext4_lookup,
};

static int xos_ext4_vfs_readlink(xinode *node, char *buffer,
                                 unsigned int capacity)
{
    xos_ext4_inode_private_t *private_data;

    if (node == NULL || !S_ISLNK(node->node_mode) ||
        (private_data = node->private_data) == NULL){
        return -EINVAL;
    }
     
    return xos_ext4_read_symlink(private_data->sbi, &private_data->disk_inode,
                                  buffer, capacity);
}

static xinode_ops_t xos_ext4_symlink_inode_ops = {
    .readlink = xos_ext4_vfs_readlink,
};

static xfile_ops_t xos_ext4_file_ops = {
    .open = xos_ext4_open,
    .release = xos_ext4_release,
    .read = xos_ext4_read,
    .write = xos_ext4_write,
    .llseek = xos_ext4_llseek,
};

static xfile_ops_t xos_ext4_dir_ops = {
    .open = xos_ext4_open,
    .release = xos_ext4_release,
    .readdir = xos_ext4_readdir,
    .llseek = xos_ext4_llseek,
};

static int xos_ext4_mount(struct xos_vfs_file_type *fs_name,
                          struct xos_block_device *bdev,
                          int flags,
                          const char *dev_name,
                          void *data,
                          xsuperblock **out_super)
{
    uint8 superblock[1024];
    xsuperblock *super;
    xinode *root_inode;
    xdentry *root_dentry;
    xos_ext4_inode_t root_disk_inode;
    xos_ext4_sb_info_t *saved_sbi;
    path_name_t root_name;
    xos_ext4_sb_info_t sbi;
    int ret;

    if (fs_name == NULL || bdev == NULL || out_super == NULL) {
        return -EINVAL;
    }
    *out_super = NULL;

    ret = xos_ext4_read_superblock(bdev, superblock, sizeof(superblock));
    if (ret < 0) {
        return ret;
    }
    ret = xos_ext4_parse_superblock(bdev, superblock, sizeof(superblock),
                                    &sbi);
    if (ret < 0) {
        return ret;
    }

    saved_sbi = xos_kmalloc(sizeof(*saved_sbi));
    if (saved_sbi == NULL) return -ENOMEM;
    *saved_sbi = sbi;
    mutext_init(&saved_sbi->write_lock);
    super = xos_kmalloc(sizeof(*super));
    if (super == NULL) {
        xos_kfree(saved_sbi);
        return -ENOMEM;
    }
    memset(super, 0, sizeof(*super));
    super->block_size = sbi.block_size;
    super->block_bits = sbi.block_bits;
    super->max_file_size = 0xffffffffULL;
    super->s_dev = bdev->devno;
    super->ref_count = 1;
    super->private_data = saved_sbi;
    super->block_device = bdev;
    super->block_device_opened = 1;
    super->mount_behavior = flags;
    /* No journal replay, metadata checksum updates, or allocation yet. */
    if ((sbi.feature_compat & XOS_EXT4_FEATURE_COMPAT_HAS_JOURNAL) ||
        (sbi.feature_ro_compat & XOS_EXT4_RO_COMPAT_GDT_CSUM) ||
        (bdev->flags & XOS_BLOCKDEV_READ_ONLY)){
        super->mount_behavior |= SB_RDONLY;
    }
        
    saved_sbi->writable = !(super->mount_behavior & SB_RDONLY);
    list_init(&super->super_list);
    list_init(&super->inode_list);
    xos_spinlock_init(&super->lock);

    ret = xos_ext4_read_inode(&sbi, XOS_EXT4_ROOT_INO, &root_disk_inode);
    if (ret < 0 || !S_ISDIR(root_disk_inode.mode)) {
        xos_kfree(saved_sbi);
        xos_kfree(super);
        return ret < 0 ? ret : -EINVAL;
    }
    ret = xos_ext4_alloc_inode(super, XOS_EXT4_ROOT_INO, &root_disk_inode,
                               &root_inode);
    if (ret < 0) {
        xos_kfree(saved_sbi);
        xos_kfree(super);
        return ret;
    }
    root_name.path_name = (unsigned char *)"/";
    root_name.len = 1;
    root_dentry = xdentry_cache_alloc_init(NULL, &root_name);
    if (root_dentry == NULL) {
        xos_kfree(root_inode->private_data);
        free_cache_xinode(root_inode);
        xos_kfree(saved_sbi);
        xos_kfree(super);
        return -ENOMEM;
    }
    root_dentry->file_node = root_inode;
    root_dentry->xsb = super;
    super->root_dentry = root_dentry;
    super->mount_behavior |= MS_ACTIVE;
    *out_super = super;
    /*防编译报错*/
    (void)dev_name;
    (void)data;
    return 0;
}


static void xos_ext4_release_dentry(xdentry *dentry)
{
    dlist_t *head = &dentry->children_list_head;

    while (!list_is_empty(head)) {
        xdentry *child = list_entry(head->next, xdentry, ch_at_parent_list);

        xos_ext4_release_dentry(child);
    }
    if (dentry->parent_dentry != dentry) {
        list_del(&dentry->ch_at_parent_list);
        list_init(&dentry->ch_at_parent_list);
    }
    if (dentry->file_node != NULL) {
        xos_kfree(dentry->file_node->private_data);
        free_cache_xinode(dentry->file_node);
        dentry->file_node = NULL;
    }
    free_dentry_cache(dentry);
}

void release_ext4fs(xsuperblock *super)
{
    if (super == NULL) return;
    /* The mount owner must first ensure no open files or external dentry
     * references remain; unmount/busy checking is not implemented yet. */
    if (super->root_dentry != NULL){
        xos_ext4_release_dentry(super->root_dentry);
    }
    
    xos_kfree(super->private_data);
    xos_kfree(super);
}


xvfs_file_t  ext4_fs_type={
    .fs_type = "ext4fs",
    .fs_flags = XOS_FS_REQUIRES_BLOCK,
    .mount = xos_ext4_mount,
    .init_file_system = NULL,
    .release_file_system = release_ext4fs,

};

void init_ext4fs(void)
{
    xfs_type_register(&ext4_fs_type);
}
