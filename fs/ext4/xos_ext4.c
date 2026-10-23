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
#include "xnode.h"
#include "xos_block_dev.h"
#include "xos_ext4.h"


/*
 * ext4 filesystem type registration.
*/

static uint16 xos_ext4_get_le16(const uint8 *value)
{
    return (uint16)value[0] | ((uint16)value[1] << 8);
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

static int xos_ext4_mount(struct xos_vfs_file_type *fs_name,
                          struct xos_block_device *bdev,
                          int flags,
                          const char *dev_name,
                          void *data,
                          xsuperblock **out_super)
{
    uint8 superblock[1024];

    if (fs_name == NULL || bdev == NULL || out_super == NULL) {
        return -EINVAL;
    }
    *out_super = NULL;

    if (xos_ext4_read_superblock(bdev, superblock, sizeof(superblock)) < 0) {
        return -EIO;
    }
    if (xos_ext4_get_le16(superblock + 56) != XOS_EXT4_SUPER_MAGIC) {
        return -EINVAL;
    }

    /*
     * The block device contract is now sufficient for ext4 superblock
     * parsing. Root inode and directory operations are implemented in the
     * next ext4 layer and must be added before this mount is exposed.
     */
    return -ENOSYS;
}


void release_ext4fs(xsuperblock *super)
{
    if (super == NULL){
        return;
    }
    /*
     * TODO: release ext4 filesystem resources (dentry cache, inodes, etc.)
     */
    if (super->root_dentry){
        free_dentry_cache(super->root_dentry);
    }
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
