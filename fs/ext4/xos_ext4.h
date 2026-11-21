#ifndef __XOS_EXT4_H__
#define __XOS_EXT4_H__

#include "types.h"
#include "spinlock.h"
#include "list.h"
#include "xos_mutex.h"

#define XOS_EXT4_SUPERBLOCK_OFFSET 1024U
#define XOS_EXT4_SUPERBLOCK_SIZE   1024U
#define XOS_EXT4_SUPER_MAGIC       0xef53U

#define XOS_EXT4_FEATURE_INCOMPAT_FILETYPE 0x0002U
#define XOS_EXT4_FEATURE_INCOMPAT_EXTENTS  0x0040U
#define XOS_EXT4_FEATURE_INCOMPAT_FLEX_BG  0x0200U
#define XOS_EXT4_FEATURE_COMPAT_HAS_JOURNAL 0x0004U
#define XOS_EXT4_RO_COMPAT_GDT_CSUM       0x0010U
/* Only these read-only compatible features retain the block layout parsed here. */
#define XOS_EXT4_RO_COMPAT_SUPPORTED       0x006bU
#define XOS_EXT4_INODE_FLAG_EXTENTS        0x00080000U
#define XOS_EXT4_EXTENT_MAGIC              0xf30aU
#define XOS_EXT4_ROOT_INO                  2U

#define XOS_EXT4_FT_UNKNOWN 0
#define XOS_EXT4_FT_REG_FILE 1
#define XOS_EXT4_FT_DIR 2
#define XOS_EXT4_FT_CHRDEV 3
#define XOS_EXT4_FT_BLKDEV 4
#define XOS_EXT4_FT_FIFO 5
#define XOS_EXT4_FT_SOCK 6
#define XOS_EXT4_FT_SYMLINK 7

typedef struct xos_ext4_dir_entry {
    uint32 inode;
    uint16 rec_len;
    uint8 name_len;
    uint8 file_type;
    char name[255];
} xos_ext4_dir_entry_t;

typedef struct xos_ext4_sb_info {
    struct xos_block_device *bdev;
    mutext_t write_lock;
    uint32 writable;
    uint32 block_size;
    uint32 block_bits;
    uint32 inode_size;
    uint32 inodes_per_group;
    uint32 blocks_per_group;
    uint32 desc_size;
    uint32 first_inode;
    uint32 first_data_block;
    uint64 group_desc_block;
    uint32 group_count;
    uint64 block_count;
    uint64 free_block_count;
    uint32 inode_count;
    uint32 feature_compat;
    uint32 feature_incompat;
    uint32 feature_ro_compat;
} xos_ext4_sb_info_t;

typedef struct xos_ext4_group_desc {
    uint32 block_bitmap;
    uint32 inode_bitmap;
    uint32 inode_table;
    uint16 free_blocks;
    uint16 free_inodes;
    uint16 used_dirs;
} xos_ext4_group_desc_t;

typedef struct xos_ext4_inode {
    uint16 mode;
    uint16 uid;
    uint32 size;
    uint32 atime;
    uint32 ctime;
    uint32 mtime;
    uint32 dtime;
    uint16 gid;
    uint16 links_count;
    uint32 blocks;
    uint32 flags;
    uint32 generation;
    uint32 file_acl;
    uint32 size_high;
    uint32 block[15];
} xos_ext4_inode_t;

struct xos_block_device;

/* The block layer owns all sector translation used by ext4. */
int xos_ext4_read_superblock(struct xos_block_device *bdev,
                             void *buffer,
                             uint32 buffer_size);

int xos_ext4_parse_superblock(struct xos_block_device *bdev,
                              const void *buffer,
                              uint32 buffer_size,
                              xos_ext4_sb_info_t *sbi);

int xos_ext4_read_block(const xos_ext4_sb_info_t *sbi,
                        uint64 block_no,
                        void *buffer);

int xos_ext4_read_group_desc(const xos_ext4_sb_info_t *sbi,
                             uint32 group,
                             xos_ext4_group_desc_t *desc);

int xos_ext4_read_inode(const xos_ext4_sb_info_t *sbi,
                        uint32 inode_no,
                        xos_ext4_inode_t *inode);

int xos_ext4_map_file_block(const xos_ext4_sb_info_t *sbi,
                            const xos_ext4_inode_t *inode,
                            uint32 file_block,
                            uint64 *disk_block);

int xos_ext4_read_inode_data(const xos_ext4_sb_info_t *sbi,
                             const xos_ext4_inode_t *inode,
                             uint64 offset,
                             void *buffer,
                             uint32 length);

/* Existing allocated file blocks only; never changes inode size or metadata. */
int xos_ext4_write_inode_data(xos_ext4_sb_info_t *sbi,
                              const xos_ext4_inode_t *inode,
                              uint64 offset, const void *buffer, uint32 length);

int xos_ext4_read_symlink(const xos_ext4_sb_info_t *sbi,
                          const xos_ext4_inode_t *inode,
                          char *buffer, uint32 capacity);

#endif
