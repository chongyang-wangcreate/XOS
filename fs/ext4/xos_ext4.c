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


/*
    ext4 filesystem type registration.
    Actual ext4 mount/read/write not yet implemented.
    init_file_ext4fs currently just allocates a superblock stub.
*/

xsuperblock *init_file_ext4fs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data)
{

    return NULL;
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
    .init_file_system = init_file_ext4fs,
    .init_file_system_old = NULL,
    .release_file_system = release_ext4fs,

};

void init_ext4fs(void)
{
    xfs_type_register(&ext4_fs_type);
}
