#ifndef __XOS_VFS_H__
#define __XOS_VFS_H__


struct xos_superblock;

#include "fs.h"
#include "spinlock.h"
//#include "xos_char_dev.h"
//#include "mount.h"

#define FILE_NAME_NAX 32
#define MAGIC_BASE 0x10240

#define RAMFS_MAGIC MAGIC_BASE
#define TMPFS_MAGIC (RAMFS_MAGIC+1)
#define DEVFS_MAGIC (RAMFS_MAGIC+2)



#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)



#if 0
typedef struct super_block_operation{
    
    struct xos_inode *(*alloc_inode)(struct xos_superblock *sb);
    void (*destroy_inode)(struct xos_inode *);
}super_block_ops;
#endif

/*
    描述整个文件系统
    这个结构需要一点点一点点完善
*/
#if 0
typedef struct xos_superblock{
    u32 maigc;        /*识别区分具体的文件系统*/
    u32 block_size;   /*当前文件系统数据块大小*/
    u32 ref_count;   /*被引用计数*/
    dlist_t super_list; /*可以链入文件系统集合，不止一种文件系统*/
    xdentry *root_dentry;
//    void *root_dentry;
    super_block_ops *super_ops;
    void *real_fs_super;
    xos_spinlock_t  lock;  /*防止多进程竞争冲突问题*/
    dlist_t inode_list; /*和inode 节点关联*/
    int mount_behavior;
    unsigned long		s_flags;
}xsuperblock;
#endif

/*
    参考Linux 内核，尤其是文件系统初始化function 函数参数
    
    文件系统初始化好几个方案，
*/
typedef int (*init_super)(xsuperblock *,void *data);
typedef struct xos_vfs_file_type{
    const char* fs_type;
    char file_name[FILE_NAME_NAX];
    dlist_t vfs_list;
    xsuperblock *(*init_file_system)(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data);
    int (*init_file_system_old)(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data,xdentry **root_dentry);
    void (*release_file_system)(xsuperblock *);
    int init_ret; /*init 执行结果*/
    int release_ret;/*release 执行结果，两个应该合并使用一个int*/

}xvfs_file_t;



int xos_vfs_init();
extern xvfs_file_t *find_match_system(const char *fs_name);

extern int xfs_type_register(xvfs_file_t *fs_type);
extern void xos_mount_fs();


#endif
