#ifndef __MOUNT_H__
#define __MOUNT_H__

#include "xos_vfs.h"
#include "fs.h"

#define MNT_NOSUID	0x01
#define MNT_NODEV	0x02
#define MNT_NOEXEC	0x04
#define MNT_NOATIME	0x08
#define MNT_NODIRATIME	0x10
#define MNT_RELATIME	0x20
#define MNT_READONLY	0x40	/* does the user want this to be r/o? */




#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#define MS_NOATIME	1024	/* Do not update access times. */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
#define MS_BIND		4096
#define MS_MOVE		8192
#define MS_REC		16384
#define MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
				   MS_VERBOSE is deprecated. */
#define MS_SILENT	32768
#define MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#define MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#define MS_LAZYTIME	(1<<25) /* Update the on-disk [acm]times lazily */

/* These sb flags are internal to the kernel */
#define MS_SUBMOUNT     (1<<26)
#define MS_NOREMOTELOCK	(1<<27)
#define MS_NOSEC	(1<<28)
#define MS_BORN		(1<<29)
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

#define MNT_LOCK_ATIME		0x040000
#define MNT_LOCK_NOEXEC		0x080000
#define MNT_LOCK_NOSUID		0x100000
#define MNT_LOCK_NODEV		0x200000
#define MNT_LOCK_READONLY	0x400000
#define MNT_LOCKED		0x800000
#define MNT_DOOMED		0x1000000
#define MNT_SYNC_UMOUNT		0x2000000
#define MNT_MARKED		0x4000000
#define MNT_UMOUNT		0x8000000

enum mount_flags{
    NO_MOUNT,
    EXIST_MOUNT,

};

extern xos_spinlock_t mnt_lock;
extern xos_spinlock_t inode_lock;
extern xos_spinlock_t dentry_lock;
extern xos_spinlock_t g_file_lock;

typedef struct process_mount_space{
    /*
        进程在不同的命名空间看到不同的文件系统目录树
    */

}proc_mnt_space;
typedef struct mount_ops{

    void(*mount_relase)(void *mnt);
    void (*mount_alloc)();
}mount_ops_t;


/*
    我既是父亲也是孩子

    我作为父亲，我的孩子如何与我产生联系，每一个孩子节点加入到mount_parent_list_head 链表节点

    我作为孩子，我如何和父亲产生联系呢，加入父亲的mount_parent_list_head 链表头节点
*/

typedef struct xos_mount_desc{
    char *dev_name;
    struct xos_mount_desc *mount_parent;
    mount_ops_t mnt_ops;
    xdentry *mount_point;
    xdentry *root_dentry;/*本文件系统的根*/
    xdentry *fs_root; /*本文件系统的根,多余了*/
    dlist_t link_children_list; /*挂到当前文件系统子文件系统，可能不止一个所以不使用struct xos_mount_desc *mount_child;*/
    dlist_t link_parent_list; /*父文件系统头节点，通过本节点挂到父亲mount_hook_child_list*/
    dlist_t hash_list; /**/
    xsuperblock *x_super; /*文件系统超级块*/
    xos_spinlock_t mnt_lock;
    int alloc_count;
    int ref_count;
    int mnt_flags;
}xmount_t;


typedef struct task_fs{
    int  ref_count;

    xos_spinlock_t  lock;

    int mask;
    /*

    void *root_dentry_freelist;
    void *cur_dentry_freelist;
    */
    struct xos_dentry *root_dentry, *curr_dentry;

    xmount_t * root_dentry_mount, *curr_dentry_mount;

}x_task_fs;

extern xmount_t *xmount_cache_alloc();
extern int get_trav_start_path(char *mount_dir, lookup_path_t *lpath);
extern void  init_mount_cache(void);
extern int mount_rootfs(const char *fs_name,int mode,char *mount_path,char *dev_name,void *data);
extern int do_mount(const char *fs_name,int mode,char *mount_path,char *dev_name,void *data);
extern xmount_t *lookup_mnt(xmount_t *mnt, xdentry *filenode_cache);
extern  void set_pwd(x_task_fs *fs, xmount_t *mnt,xdentry *dentry_cache);
#endif
