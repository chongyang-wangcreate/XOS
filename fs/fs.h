#ifndef __FS_H__
#define __FS_H__

#include "stat.h"
#include "arch64_timer.h"

typedef unsigned long size_t;


#define MAY_EXEC		0x00000001
#define MAY_WRITE		0x00000002
#define MAY_READ		0x00000004
#define MAY_APPEND		0x00000008
#define MAY_ACCESS		0x00000010
#define MAY_OPEN		0x00000020
#define MAY_CHDIR		0x00000040
/* called from RCU mode, don't block */
#define MAY_NOT_BLOCK		0x00000080


/*tmp */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14


#define S_SYNC		1	/* Writes are synced at once */
#define S_NOATIME	2	/* Do not update access times */
#define S_APPEND	4	/* Append-only file */
#define S_IMMUTABLE	8	/* Immutable file */
#define S_DEAD		16	/* removed, but still open directory */
#define S_NOQUOTA	32	/* Inode is not counted to quota */
#define S_DIRSYNC	64	/* Directory modifications are synchronous */
#define S_NOCMTIME	128	/* Do not update file c/mtime */
#define S_SWAPFILE	256	/* Do not truncate: swapon got its bmaps */



#define SB_RDONLY	 1	/* Mount read-only */
#define SB_NOSUID	 2	/* Ignore suid and sgid bits */
#define SB_NODEV	 4	/* Disallow access to device special files */
#define SB_NOEXEC	 8	/* Disallow program execution */
#define SB_SYNCHRONOUS	16	/* Writes are synced at once */
#define SB_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define SB_DIRSYNC	128	/* Directory modifications are synchronous */
#define SB_NOATIME	1024	/* Do not update access times. */
#define SB_NODIRATIME	2048	/* Do not update directory access times */
#define SB_SILENT	32768
#define SB_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define SB_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define SB_I_VERSION	(1<<23) /* Update inode I_version field */
#define SB_LAZYTIME	(1<<25) /* Update the on-disk [acm]times lazily */

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)


/* file is open for reading */
#define FMODE_READ		(0x1)
/* file is open for writing */
#define FMODE_WRITE		(0x2)
/* file is seekable */
#define FMODE_LSEEK		(0x4)
/* file can be accessed using pread */
#define FMODE_PREAD		(0x8)
/* file can be accessed using pwrite */
#define FMODE_PWRITE		(0x10)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC		(0x20)
/* File is opened with O_NDELAY (only set for block devices) */
#define FMODE_NDELAY		(0x40)
/* File is opened with O_EXCL (only set for block devices) */
#define FMODE_EXCL		(0x80)
/* File is opened using open(.., 3, ..) and is writeable only for ioctls
   (specialy hack for floppy.c) */
#define FMODE_WRITE_IOCTL	(0x100)
/* 32bit hashes as llseek() offset (for directories) */
#define FMODE_32BITHASH         (0x200)
/* 64bit hashes as llseek() offset (for directories) */
#define FMODE_64BITHASH         (0x400)


/*from linux */
#define ATTR_MODE	(1 << 0)
#define ATTR_UID	(1 << 1)
#define ATTR_GID	(1 << 2)
#define ATTR_SIZE	(1 << 3)
#define ATTR_ATIME	(1 << 4)
#define ATTR_MTIME	(1 << 5)
#define ATTR_CTIME	(1 << 6)
#define ATTR_ATIME_SET	(1 << 7)
#define ATTR_MTIME_SET	(1 << 8)
#define ATTR_FORCE	(1 << 9) /* Not a change, but a change it */
#define ATTR_KILL_SUID	(1 << 11)
#define ATTR_KILL_SGID	(1 << 12)
#define ATTR_FILE	(1 << 13)
#define ATTR_KILL_PRIV	(1 << 14)
#define ATTR_OPEN	(1 << 15) /* Truncating from open(O_TRUNC) */
#define ATTR_TIMES_SET	(1 << 16)
#define ATTR_TOUCH	(1 << 17)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define IS_RDONLY(file_node) (((file_node)->p_super) && ((file_node)->p_super->mount_behavior & SB_RDONLY))

#define IS_APPEND(file_node) ((file_node)->flags & S_APPEND)
#define IS_IMMUTABLE(file_node) ((file_node)->flags & S_IMMUTABLE)



//#define ACC_MODE(x) ("\004\002\006\006"[(x)&O_ACCMODE])

#define ACC_MODE(x) (((x) + 1) & O_ACCMODE)

enum {

    DIR_NODE ,
    LINK_NODE,

};

enum{

    FS_ACTIVE,
    FS_MAX,
};

#define MAX_FILE_NR   1024
#define FS_ACTIVE_STATUS	(1UL << FS_ACTIVE)

#define LOOKUP_DIRECTORY	0x0002
#define AT_FDCWD  -100


struct xos_file_ops;
struct file;
struct xos_inode;
struct xos_inode_ops;
struct xos_file_ops;
struct fs_path_lookup;
struct xos_dentry;
struct xos_superblock;

#include "xos_char_dev.h"
#include "xos_path.h"

struct iattr {
    unsigned int	ia_valid;
    umode_t		ia_mode;
    uid_t		ia_uid;
    gid_t		ia_gid;
    loff_t		ia_size;
    struct timespec64 ia_atime;
    struct timespec64 ia_mtime;
    struct timespec64 ia_ctime;
    struct file *ia_file;
};


/*
    参考内核
*/
typedef struct super_block_operation{
    
    struct xos_inode *(*alloc_inode)(struct xos_superblock *sb);
    void (*destroy_inode)(struct xos_inode *);
}super_block_ops;

typedef struct xos_superblock{
    u32 maigc;        /*识别区分具体的文件系统*/
    u32 block_size;   /*当前文件系统数据块大小*/
    dev_t s_dev;      /*文件系统对应的设备号*/
    u32 ref_count;   /*被引用计数*/
    dlist_t super_list; /*可以链入文件系统集合，不止一种文件系统*/
    struct xos_dentry *root_dentry;/*当前文件系统的根dentry*/
    super_block_ops *super_ops;/*操作函数集*/
    void *real_fs_super;/*similar void *private */
    xos_spinlock_t  lock;  /*防止多进程竞争冲突问题*/
    dlist_t inode_list; /*和inode 节点关联*/
    int mount_behavior;
    unsigned long		s_flags;
}xsuperblock;


typedef struct xos_inode_ops{
    int (*create) (struct xos_inode *,struct xos_dentry *,int, struct fs_path_lookup *);
    int (*mkdir) (struct xos_inode *,struct xos_dentry *,int);
    int (*rmdir) (struct xos_inode *,struct xos_dentry *);
    int (*mknod) (struct xos_inode *,struct xos_dentry *,int,devno_t);
    struct xos_dentry * (*lookup) (struct xos_inode *,struct xos_dentry *, struct fs_path_lookup *);
    int (*follow_link) (struct xos_dentry *, struct fs_path_lookup *);
    int (*setattr)(struct xos_dentry *, struct iattr *);

}xinode_ops_t;


typedef int (*filldir_t)(void *, const char *, int, long, uint64_t, unsigned int);


typedef struct xos_file_ops{
    int (*open) (struct xos_inode *, struct file *);
    long (*read) (struct file *, char  *, size_t, long *);
    long (*write) (struct file *, const char  *, size_t, long *);
    int (*readdir) (struct file *, void *, filldir_t);
    loff_t (*llseek)(struct file *, loff_t , int);
}xfile_ops_t;

/*

    20250106:
    我在inode 节点中定义了和cache_page 关联的结构
    unsigned long   nr_pages
    dlist_t     *pg_list;
    xspinlock_t pg_lock;
   

    看了Linux 内核之后，将
    dlist_t     *pg_list;
    xspinlock_t pg_lock;
    封装成一个结构address_space

*/
struct address_space {
    unsigned long alloc_flags;

    struct xos_inode  *node_owner;		/* belong who similar to mutex lock*/

    dlist_t page_list_head;	

    xos_spinlock_t pg_lock;	/* and spinlock protecting it */

    unsigned int i_mmap_writable;/* count VM_SHARED mappings */

    unsigned long   nr_pages;	/* number of total pages */
    /**
     * from linux 
     */
    unsigned long   writeback_index;/* writeback starts here */
    /**
     * from linux 
     */
    struct address_space_ops *ops;	/* methods */

    unsigned long		flags;		/* error bits/paf mask */

};


/******************
    具体文件描述符号
    当前描述
******************/
typedef struct xos_inode{
    xos_spinlock_t  lock;  /*防止多进程竞争冲突问题*/
    u32 in_cache_pool;
    u32 file_types; 
    u32 file_size; /*文件大小*/
    u32 file_size_limit; /*文件最大限制*/
    u32 block_size;/*文件块大小*/
    u32 block_count;
    u32 pos; /*文件偏移位置*/
    u32 ref_count;/*xinode文件操作引用计数*/
    u32 uid;
    u32 gid;
    u32 link_cnt;
    u32 devno; /*如果是设备文件的话需要设备号*/
    xcdev_t *chardev;/*如果inode 属于字符设备文件，还需要定义一个字符设备描述符关联,设备文件相关联*/
    /*如果inode 描述块设备还需定义块设备描述符想关联*/
    dlist_t device_list; /*设备链表*/
    dlist_t bind_xsuper_list;
    dlist_t cache_list;
    xsuperblock *p_super; /*属于那个超级快*/
    xfile_ops_t *file_ops;/*文件操作函数集合*/
    xinode_ops_t *inode_ops;/*节点操作函数集合*/
    void *private_data;
    int node_mode;   /*确定是什么文件 普通文件 ，目录文件  ，设备文件 链接文件  ，权限*/
    long node_num; 
    int flags;  /*权限*/
    int alloc_flags;
    struct timespec64	i_atime;
    struct timespec64	i_mtime;
    struct timespec64	i_ctime;
    struct address_space cache_space;  
}xinode;


struct address_space_ops{


};


/*
    file 描述符号
*/

typedef struct file {
    xos_spinlock_t  f_lock;
    struct xos_dentry  *f_dentry;	/* cached value */
    struct xos_file_ops  *f_ops;
    unsigned long  ref_count;
    long f_count;
    unsigned int f_flags;
    int f_mode;
    long f_pos;
    void *private_data;
    char *f_space;
}xos_file_t;

typedef struct files_set{
    xos_spinlock_t  file_lock;
    bitmap_t  fd_map;
    /*
        使用指针目的减小结构体大小
    */
    xos_file_t *file_set[MAX_FILE_NR];
    char fd_set[MAX_FILE_NR/8];

}files_set_t;


typedef struct dentry_name {
	/**
	 * 文件名及其长度
	 */
	unsigned char *name;
	unsigned int len;

}xdentry_name;


/*
    描述文件名称，关联文件节点
*/
typedef struct xos_dentry{
    xos_spinlock_t  lock;  /*防止多进程竞争冲突问题*/
//    char file_name[128]; /**/
    xsuperblock *xsb;
    path_name_t file_name;
    struct xos_inode *file_node;
    struct xos_dentry *parent_dentry;/*关联父目录*/
    struct list_head children_list_head; /*子文件节点链表头*/
    struct list_head ch_at_parent_list; /*使用本节点挂载父节点链表头，挂到父children_list_head*/
    dlist_t dcache_list;
    int ref_count;
    int cur_mnt_count;
    int is_mount;  /* 0 no mount , 1, mount*/
    struct xos_dentry *next;
}xdentry;

extern xos_spinlock_t mnt_lock;
extern xos_spinlock_t inode_lock;
extern xos_spinlock_t dentry_lock;
extern xos_spinlock_t g_file_lock;

static inline long parent_xinode(xdentry *xdentry_node)
{
    long res;

    xos_spinlock(&dentry_lock);
    res = xdentry_node->parent_dentry->file_node->node_num;
    xos_unspinlock(&dentry_lock);
    return res;
}




extern int real_lookup_path(char *mount_path, lookup_path_t *look_path);

extern xinode *alloc_cache_xinode();

extern int resolve_path(char *mount_path, int flags,  lookup_path_t *look_path);
extern xdentry *find_in_pdentry(xdentry *parent, path_name_t *name);
extern xdentry *xdentry_cache_alloc_init(xdentry *parent,const path_name_t *name);

//extern void  init_inode_cache(void);
extern void  init_inode_cache(void);

extern void init_ramfs();
extern void init_tmpfs();


extern void special_xnode_init(xinode *inode,int mode,devno_t dev);
extern void free_dentry_cache(xdentry *dentry_node);
extern int is_alloc_dentry_cache(xdentry *dentry_node);
extern xdentry *find_in_pdentry_unlock(xdentry *parent, path_name_t *name);

extern xdentry *alloc_dentry_cache();
extern xdentry *lookup_dentry_in_pdentry(path_name_t *name, xdentry * p_dir,lookup_path_t *look);



/*
    comm
*/

extern xdentry *lookup_dentry_in_pdentry(path_name_t *name, xdentry * p_dir,lookup_path_t *look);
extern int resolve_path(char *mount_path, int flags,  lookup_path_t *look_path);

#endif





