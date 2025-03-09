#ifndef __XNODE_H__
#define __XNODE_H__

#include "xos_dev.h"
#include "xos_char_dev.h"
#include "xdentry.h"

enum loop_ret_val{
    RET_ROOT,
    RET_NORMAL_COMPONET,
    RET_LASTER_SLASH,

};
typedef unsigned long size_t;

typedef struct cache_node_man{
    
    xinode   inode_cache;
    char     in_cache_pool;
    char     alloc_flags;

}node_man_t;


typedef struct cache_mang_node_desc{
    xinode *inode_cache_pool; //一次性分配
    node_man_t *icache_pool;
    dlist_t cache_free_list;  //单次分配
    dlist_t cache_using_list;
    int   cache_pool_max_cnt;
    int   cache_pool_using_cnt;
    int   cache_pool_free_cnt;

}cache_node_desc;


#if 0

enum {
    PATHTYPE_NOTHING,
    PATHTYPE_ROOT,
    PATHTYPE_NORMAL,
    PATHTYPE_DOT,
    PATHTYPE_DOTDOT,
};


/*
    操作函数集合参考了内核，主要想定义的函数接口和内核一致
*/
struct xos_inode;
struct xos_inode_ops;
struct xos_file_ops;
struct fs_path_lookup;


typedef struct xos_inode_ops{
    int (*create) (struct xos_inode *,struct xos_dentry *,int, struct fs_path_lookup *);
    int (*mkdir) (struct xos_inode *,struct xos_dentry *,int);
    int (*rmdir) (struct xos_inode *,struct xos_dentry *);
    int (*mknod) (struct xos_inode *,struct xos_dentry *,int,devno_t);
    xdentry * (*lookup) (struct xos_inode *,struct xos_dentry *, struct fs_path_lookup *);

}xinode_ops_t;





/******************
    描述具体文件
    当前描述
******************/
typedef struct xos_inode{
    xos_spinlock_t  lock;  /*防止多进程竞争冲突问题*/
    u32 file_types; /*普通文件 目录文件 设备文件*/
    u32 file_size; /*文件大小*/
    u32 block_size;/*文件块大小*/
    u32 block_count;
    u32 pos; /*文件偏移位置*/
    u32 ref_count;/*文件操作引用计数*/
    u32 devno; /*如果是设备文件的话需要设备号*/
    u32 uid;
    u32 gid;
    xcdev_t *chardev;/*如果inode 属于字符设备文件，还需要定义一个字符设备描述符关联*/
    /*如果inode 描述块设备还需定义块设备描述符想关联*/
    dlist_t device_list; /*设备链表*/
    dlist_t bind_xsuper_list;
    void *p_super; /*属于那个超级快*/
    xfile_ops_t *file_ops;/*文件操作函数集合*/
    xinode_ops_t *inode_ops;/*节点操作函数集合*/
    int node_mode;  /*权限*/
    int node_type;  /*确定是什么文件 普通文件 ，目录文件  ，设备文件 链接文件*/
    int alloc_flags;
}xinode;


extern int real_lookup_path(char *mount_path, lookup_path_t *look_path);

#endif
//extern void switch_to_parent_dir(xmount_t **mnt, xdentry **dentry_cache);
extern void travel_parent_dir(xmount_t **mnt, xdentry **dentry_cache);
extern void free_cache_xinode(xinode  * new_node);
extern void switch_to_parent_dir(lookup_path_t *look_path);



#endif
