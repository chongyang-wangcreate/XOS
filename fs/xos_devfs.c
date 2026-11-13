/********************************************************
    
    development started: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

   Copyright (c) 2024 - 2027 wangchongyang

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   "When using, modifying, or distributing this code, 
   appropriate credit must be given to the original author. 
   You must include an acknowledgment of the original author in all copies or substantial portions of the software.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
********************************************************/


#include "types.h"
#include "list.h"
#include "error.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "xos_cache.h"
#include "xos_vfs.h"
#include "fs.h"
#include "xos_char_dev.h"
#include "xos_devfs.h"
#include "com_fs.h"
#include "xnode.h"
#include "path.h"
#include "mount.h"
#include "tick_timer.h"
#include "task.h"


xos_spinlock_t  devfs_lock;
/* /dev 尚未挂载时暂存注册信息 */
typedef struct devfs_pending_device {
    dlist_t list;
    char name[256];
    int mode;
    devno_t devno;
    const void *owner;
    int syncing;
} devfs_pending_device_t;

static dlist_t devfs_pending_devices;
static char devfs_auto_directory;

xinode* alloc_devfs_node_init(xsuperblock * super,int mode,devno_t dev);
static int devfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
                           int mode, devno_t dev, const void *owner);

/* 与 open/注销共用 inode_lock；检查并引用必须原子完成。 */
static int devfs_parent_get(xdentry *parent)
{
    xinode *node;
    int ret = 0;

    xos_spinlock(&inode_lock);
    node = parent->file_node;
    if (node == NULL) {
        ret = -ENOENT;
    } else if (!S_ISDIR(node->node_mode)) {
        ret = -ENOTDIR;
    } else {
        node->ref_count++;
    }
    xos_unspinlock(&inode_lock);
    return ret;
}

/* 减少目录引用计数 */
static void devfs_parent_put(xdentry *parent)
{
    xos_spinlock(&inode_lock);
    parent->file_node->ref_count--;
    xos_unspinlock(&inode_lock);
}


static int devfs_device_name(const char *name, path_name_t *part)
{
    unsigned int len = 0;
    unsigned int component = 0;
    const char *p;
    /* 设备名可含目录分隔符，不能是空路径 */
    if (name == NULL || name[0] == '\0') {
        return -EINVAL;
    }
    for (p = name; ; ++p) {
        if (*p != '\0' && *p != '/') {
            if (++component >= XOS_DENTRY_NAME_MAX || ++len >= 256) {
                return -ENAMETOOLONG;
            }
            continue;
        }
        if (component == 0 ||
            (component == 1 && p[-1] == '.') ||
            (component == 2 && p[-2] == '.' && p[-1] == '.')) {
            return -EINVAL;
        }
        if (*p == '\0') {
            break;
        }
        component = 0;
        if (++len >= 256) {
            return -ENAMETOOLONG;
        }
    }
    part->path_name = (unsigned char *)name;
    part->len = len;
    return 0;
}

/* 逐级遍历目录；仅 add 创建缺失的自动目录，remove 不得创建。 */
static int devfs_device_parent(xdentry *root, const path_name_t *name,
                                int create, xdentry **parent, path_name_t *last)
{
    const unsigned char *p = name->path_name;
    const unsigned char *end = p + name->len;
    xdentry *dir = root;
    xdentry *child;
    xinode *child_node;
    path_name_t part;
    int ret;

    ret = devfs_parent_get(dir);
    if (ret < 0) {
        return ret;
    }
    while (p < end) {
        const unsigned char *begin = p;
        while (p < end && *p != '/') {
            ++p;
        }
        part.path_name = (unsigned char *)begin;
        part.len = p - begin;
        if (p == end) {
            *parent = dir;
            *last = part;
            return 0;
        }
        child = create ? xdentry_get_or_create(dir, &part) :
                         find_in_pdentry(dir, &part);
        if (child == NULL) {
            ret = create ? -ENOMEM : -ENOENT;
            goto out_put;
        }
        xos_spinlock(&inode_lock);
        child_node = child->file_node;
        xos_unspinlock(&inode_lock);
        if (child_node == NULL && create) {
            ret = devfs_new_fnode(dir->file_node, child,
                                  S_IFDIR | 0755, 0, &devfs_auto_directory);
            if (ret < 0 && ret != -EEXIST) {
                goto out_put;
            }
        }
        ret = devfs_parent_get(child);
        if (ret < 0) {
            goto out_put;
        }
        devfs_parent_put(dir);
        dir = child;
        ++p;
    }
    ret = -EINVAL;
out_put:
    devfs_parent_put(dir);
    return ret;
}

/* 只回收由 devfs 自动创建且已无活跃子节点的目录 inode；保留父 dentry。 */
static void devfs_cleanup_empty_auto_create_dirs(xdentry *dir, xdentry *root)
{
    while (dir != root) {
        dlist_t *item;
        dlist_t *head = &dir->children_list_head;
        xinode *node;
        xdentry *parent;
        int occupied = 0;

        xos_spinlock(&inode_lock);
        xos_spinlock(&dentry_lock);
        node = dir->file_node;
        if (node != NULL && node->private_data == &devfs_auto_directory) {
            list_for_each(item, head) {
                xdentry *child = list_entry(item, xdentry, ch_at_parent_list);
                if (child->file_node != NULL) {
                    occupied = 1;
                    break;
                }
            }
        }
        if (node == NULL || node->private_data != &devfs_auto_directory ||
            occupied || node->ref_count != 0 ||
            dir->cur_mnt_count != 0) {
            xos_unspinlock(&dentry_lock);
            xos_unspinlock(&inode_lock);
            return;
        }
        parent = dir->parent_dentry;
        dir->file_node = NULL;
        xos_unspinlock(&dentry_lock);
        free_cache_xinode(node);
        xos_unspinlock(&inode_lock);
        dir = parent;
    }
}

static int devfs_device_root(xdentry **root)
{
    lookup_path_t look = {0};
    int ret;

    if (current_task == NULL) {
        return -ENODEV;
    }
    ret = resolve_path("/dev", LOOKUP_DIRECTORY, &look);
    if (ret < 0 || look.look_dentry == NULL || look.look_dentry->xsb == NULL ||
        look.look_dentry->xsb->maigc != DEVFS_MAGIC) {
        return -ENODEV;
    }
    *root = look.look_dentry;
    return 0;
}

/*
    暂存挂载前的设备注册请求
    devfs_device_add() 发现 devfs 根目录尚不可用时，会调用它；
    待 /dev 挂载完成，再由 devfs_check_pending_device_add() 检查发布这些节点
*/
static int devfs_device_defer(const char *name, int mode, devno_t devno,
                              const void *owner)
{
    devfs_pending_device_t *pending;
    devfs_pending_device_t *existing;
    dlist_t *entry;
    dlist_t *head = &devfs_pending_devices;

    pending = xos_kmalloc(sizeof(*pending));
    if (pending == NULL) {
        return -ENOMEM;
    }
    memcpy(pending->name, name, strlen(name) + 1);
    pending->mode = mode;
    pending->devno = devno;
    pending->owner = owner;
    pending->syncing = 0;

    xos_spinlock(&devfs_lock);
    list_for_each(entry, head) {
        existing = list_entry(entry, devfs_pending_device_t, list);
        if (strcmp(existing->name, name) == 0) {
            int ret = existing->owner == owner && existing->mode == mode &&
                      existing->devno == devno ? 0 : -EEXIST;
            xos_unspinlock(&devfs_lock);
            xos_kfree(pending);
            return ret;
        }
    }
    list_add_back(&pending->list, &devfs_pending_devices);
    xos_unspinlock(&devfs_lock);
    return 0;
}

int devfs_device_add(const char *name, int mode, devno_t devno, const void *owner)
{
    xdentry *root;
    xdentry *parent;
    xdentry *entry;
    path_name_t part;
    int ret;

    if (owner == NULL || (!S_ISCHR(mode) && !S_ISBLK(mode))) {
        return -EINVAL;
    }
    ret = devfs_device_name(name, &part);
    if (ret < 0) {
        return ret;
    }
    ret = devfs_device_root(&root);
    if (ret < 0) {
        /* 挂载前注册不能丢失；挂载完成后由 devfs_check_pending_device_add 检查发布。 */
        return ret == -ENODEV ? devfs_device_defer(name, mode, devno, owner) : ret;
    }
    ret = devfs_device_parent(root, &part, 1, &parent, &part);
    if (ret < 0) {
        return ret;
    }
    entry = xdentry_get_or_create(parent, &part);
    if (entry == NULL) {
        devfs_parent_put(parent);
        return -ENOMEM;
    }
    xos_spinlock(&inode_lock);
    if (entry->file_node != NULL) {
        xinode *node = entry->file_node;
        ret = node->node_mode == mode && node->devno == devno &&
              node->private_data == owner ? 0 : -EEXIST;
        xos_unspinlock(&inode_lock);
        devfs_parent_put(parent);
        return ret;
    }
    xos_unspinlock(&inode_lock);
    ret = devfs_new_fnode(parent->file_node, entry, mode, devno, owner);
    devfs_parent_put(parent);
    return ret;
}

int devfs_device_remove(const char *name, int mode, devno_t devno, const void *owner)
{
    xdentry *root;
    xdentry *parent;
    xdentry *entry;
    xinode *node;
    path_name_t part;
    devfs_pending_device_t *pending;
    dlist_t *entry_iter;
    dlist_t *head = &devfs_pending_devices;
    int ret;

    if (owner == NULL || (!S_ISCHR(mode) && !S_ISBLK(mode))) {
        return -EINVAL;
    }
    ret = devfs_device_name(name, &part);
    if (ret < 0) {
        return ret;
    }
    /* 注销尚未发布的设备：删除挂载前记录，避免后续产生幽灵节点。 */
    xos_spinlock(&devfs_lock);
    list_for_each(entry_iter, head) {
        pending = list_entry(entry_iter, devfs_pending_device_t, list);
        if (strcmp(pending->name, name) == 0) {
            if (pending->mode != mode || pending->devno != devno ||
                pending->owner != owner) {
                xos_unspinlock(&devfs_lock);
                return -EEXIST;
            }
            /* 同步期间保留登记项；注销不能释放正在使用的 owner。 */
            if (pending->syncing) {
                xos_unspinlock(&devfs_lock);
                return -EBUSY;
            }
            list_del(&pending->list);
            xos_unspinlock(&devfs_lock);
            xos_kfree(pending);
            return 0;
        }
    }
    xos_unspinlock(&devfs_lock);
    ret = devfs_device_root(&root);
    if (ret < 0) {
        return ret;
    }
    ret = devfs_device_parent(root, &part, 0, &parent, &part);
    if (ret < 0) {
        return ret;
    }
    entry = find_in_pdentry(parent, &part);
    if (entry == NULL) {
        devfs_parent_put(parent);
        return -ENOENT;
    }
    /* 与 do_open_file 的 inode 引用建立采用同一把 inode_lock；校验和摘除
       都在锁内完成，不能先读出 inode 再在锁外等候导致访问已回收对象。 */
    xos_spinlock(&inode_lock);
    node = entry->file_node;
    if (node == NULL) {
        xos_unspinlock(&inode_lock);
        devfs_parent_put(parent);
        return -ENOENT;
    }
    /* 同名手工节点或其他设备的节点不能被当前注册对象删除。 */
    if (node->node_mode != mode || node->devno != devno ||
        node->private_data != owner) {
        xos_unspinlock(&inode_lock);
        devfs_parent_put(parent);
        return -EEXIST;
    }
    /* 仍被打开或挂载的节点不得回收，避免使 file->f_dentry 悬空。
       dentry 引用计数由 entry->lock 保护，与 dget/dput 保持一致。 */
    xos_spinlock(&entry->lock);
    if (node->ref_count != 0 || entry->cur_mnt_count != 0 ||
        entry->ref_count != 1) {
        xos_unspinlock(&entry->lock);
        xos_unspinlock(&inode_lock);
        devfs_parent_put(parent);
        return -EBUSY;
    }
    entry->file_node = NULL;
    xos_unspinlock(&entry->lock);
    free_cache_xinode(node);
    xos_unspinlock(&inode_lock);
    devfs_parent_put(parent);
    /* 留下空目录项供下次注册复用，避免误释放其他查找者持有的缓存。 */
    devfs_cleanup_empty_auto_create_dirs(parent, root);
    return 0;
}

int devfs_check_pending_device_add(void)
{
    devfs_pending_device_t *pending;
    xdentry *root;
    dlist_t *first;
    int ret;

    ret = devfs_device_root(&root);
    if (ret < 0) {
        return ret;
    }
    /* 保留登记项直到发布完成；分配 inode 和解析路径时不持自旋锁。 */
    for (;;) {
        xos_spinlock(&devfs_lock);
        first = list_get_first_node(&devfs_pending_devices);
        if (first == NULL) {
            xos_unspinlock(&devfs_lock);
            return 0;
        }
        pending = list_entry(first, devfs_pending_device_t, list);
        if (pending->syncing) {
            xos_unspinlock(&devfs_lock);
            return -EBUSY;
        }
        pending->syncing = 1;
        xos_unspinlock(&devfs_lock);
        ret = devfs_device_add(pending->name, pending->mode,
                               pending->devno, pending->owner);
        xos_spinlock(&devfs_lock);
        if (ret < 0) {
            pending->syncing = 0;
            xos_unspinlock(&devfs_lock);
            return ret;
        }
        list_del(&pending->list);
        xos_unspinlock(&devfs_lock);
        xos_kfree(pending);
    }
}

static int devfs_new_fnode(xinode *parent_node, xdentry *dentry_cache,
                            int mode, devno_t dev, const void *owner)
{
    xinode *fnode;
    if (parent_node == NULL || dentry_cache == NULL ||
        !S_ISDIR(parent_node->node_mode)) {
        return -EINVAL;
    }
    if (IS_RDONLY(parent_node)) {
        return -EROFS;
    }
    if (!S_ISDIR(mode) && !S_ISCHR(mode) && !S_ISBLK(mode)) {
        return -EOPNOTSUPP;
    }

    /* 分配 inode 不需要持有 inode_lock；绑定与 open/注销共用 inode_lock。 */
    fnode = alloc_devfs_node_init(parent_node->p_super, mode, dev);
    if (fnode == NULL) {
        return -ENOMEM;
    }
    if (parent_node->node_mode & S_ISGID) {
        fnode->gid = parent_node->gid;
        if (S_ISDIR(mode)) {
            fnode->node_mode |= S_ISGID;
        }
    }
    fnode->private_data = (void *)owner;
    xos_spinlock(&inode_lock);
    if (dentry_cache->file_node != NULL) {
        xinode *existing = dentry_cache->file_node;
        /* 设备注册在绑定点判定同名：并发注册同一设备幂等，异主拒绝。
           手工 mknod 和自动目录仍按原来的 EEXIST 语义处理。 */
        int ret = owner != NULL && (S_ISCHR(mode) || S_ISBLK(mode)) &&
                  existing->node_mode == mode && existing->devno == dev &&
                  existing->private_data == owner ? 0 : -EEXIST;
        xos_unspinlock(&inode_lock);
        free_cache_xinode(fnode);
        return ret;
    }
    dentry_cache->file_node = fnode;
    /* dentry 已由父目录缓存持有，绑定 inode 不增加 dentry 引用。 */
    xos_unspinlock(&inode_lock);
    return 0;
}

static struct xos_dentry *
devfs_lookup(struct xos_inode *xnode, struct xos_dentry *xdentry_node,
    struct fs_path_lookup *look_path)
{
    if (xnode == NULL || xdentry_node == NULL || look_path == NULL ||
        !S_ISDIR(xnode->node_mode)) {
        return NULL;
    }
    /* 目录项是 devfs 的节点索引，不需要再访问 ramfs 或磁盘。 */
    return find_in_pdentry(xdentry_node, &look_path->cur_path);
}

int devfs_mkdir(struct xos_inode *xnode,struct xos_dentry *xdentry_node,int mode)
{
    int ret = devfs_new_fnode(xnode, xdentry_node,
                              (mode & ~S_IFMT) | S_IFDIR, 0, NULL);
    if (ret == 0) {
        xnode->link_cnt++;
    }
    return ret;
}
int devfs_rmdir(struct xos_inode *xnode,struct xos_dentry *xdentry_node)
{

    return -EOPNOTSUPP;
}
int devfs_mknode(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode ,devno_t dev)
{
    /* 判断是否是设备文件 */
    if (!S_ISCHR(mode) && !S_ISBLK(mode)) {
        return -EINVAL;
    }
    return devfs_new_fnode(parent_node, inode_dentry, mode, dev, NULL);
}
int devfs_create(struct xos_inode *parent_node,struct xos_dentry *inode_dentry,int mode, struct fs_path_lookup *lookup)
{

    return -EOPNOTSUPP;
}

/*
    原方案部分模块使用函数指针数组,这种方案驱动庞大之后会显得非常混乱，
    所以摒弃掉这种方案，全部使用面向对象设计
    通过设备号major  ,minor ,或者dev_name 找到对应的driver
    driver->dispatch_function[IO_OPEN] = dev_open;
    driver->dispatch_function[IO_CLOSE] = dev_close;
    driver->dispatch_function[IO_READ] = dev_read;
    driver->dispatch_function[IO_WRITE] = dev_write;
*/
xinode_ops_t devfs_dir_fnode_ops = {
    .create = devfs_create,
    .lookup = devfs_lookup,
    .mkdir = devfs_mkdir,
    .rmdir = devfs_rmdir,
    .mknod = devfs_mknode,

};

static int devfs_dir_open(struct xos_inode *node, struct file *filp)
{
    if (node == NULL || filp == NULL || filp->f_dentry == NULL ||
        !S_ISDIR(node->node_mode)) {
        return -ENOTDIR;
    }
    filp->private_data = filp->f_dentry;
    return 0;
}

#define DEVFS_READDIR_BATCH 8

typedef struct devfs_dir_entry_snapshot {
    char name[XOS_DENTRY_NAME_MAX];
    int name_len;
    long offset;
    uint64_t inode_no;
    unsigned int type;
} devfs_dir_entry_snapshot_t;

static int devfs_dir_readdir(struct file *filp, void *buf, filldir_t filldir)
{
    xdentry *dir;
    xdentry *child;
    dlist_t *entry;
    dlist_t *head;
    devfs_dir_entry_snapshot_t snapshot[DEVFS_READDIR_BATCH];
    long pos;
    long end_pos = 2;
    uint64_t inode_no;
    uint64_t parent_no;
    unsigned int type;
    unsigned int count;
    unsigned int i;

    if (filp == NULL || filldir == NULL || (dir = filp->f_dentry) == NULL) {
        return -ENOTDIR;
    }
    xos_spinlock(&inode_lock);
    xos_spinlock(&dentry_lock);
    if (dir->file_node == NULL || !S_ISDIR(dir->file_node->node_mode)) {
        xos_unspinlock(&dentry_lock);
        xos_unspinlock(&inode_lock);
        return -ENOTDIR;
    }
    inode_no = dir->file_node->node_num;
    parent_no = parent_xinode(dir);
    head = &dir->children_list_head;
    list_for_each(entry, head) {
        ++end_pos;
    }
    xos_unspinlock(&dentry_lock);
    xos_unspinlock(&inode_lock);
    if (filp->f_pos == 0) {
        if (filldir(buf, ".", 1, 0, inode_no, DT_DIR) < 0) {
            return 0;
        }
        filp->f_pos = 1;
    }
    if (filp->f_pos == 1) {
        if (filldir(buf, "..", 2, 1, parent_no, DT_DIR) < 0) {
            return 0;
        }
        filp->f_pos = 2;
    }
    /* 空目录项是未完成的创建，不属于 /dev 的可见设备。 */
    while (filp->f_pos < end_pos) {
        /* 与目录项插入及 inode 绑定/注销按相同顺序持锁，只复制有限项。
           filldir 可能访问用户空间，不能在自旋锁内调用。 */
        xos_spinlock(&inode_lock);
        xos_spinlock(&dentry_lock);
        entry = head->next;
        pos = 2;
        while (entry != head && pos < filp->f_pos) {
            entry = entry->next;
            ++pos;
        }
        count = 0;
        while (entry != head && pos < end_pos && count < DEVFS_READDIR_BATCH) {
            child = list_entry(entry, xdentry, ch_at_parent_list);
            if (child->file_node != NULL && child->file_name.path_name != NULL &&
                child->file_name.len < XOS_DENTRY_NAME_MAX) {
                if (S_ISDIR(child->file_node->node_mode)) {
                    type = DT_DIR;
                } else if (S_ISCHR(child->file_node->node_mode)) {
                    type = DT_CHR;
                } else if (S_ISBLK(child->file_node->node_mode)) {
                    type = DT_BLK;
                } else {
                    type = DT_UNKNOWN;
                }
                if (type != DT_UNKNOWN) {
                    snapshot[count].name_len = child->file_name.len;
                    memcpy(snapshot[count].name, child->file_name.path_name,
                           snapshot[count].name_len);
                    snapshot[count].offset = pos;
                    snapshot[count].inode_no = child->file_node->node_num;
                    snapshot[count].type = type;
                    ++count;
                }
            }
            entry = entry->next;
            ++pos;
        }
        if (entry == head && pos < end_pos) {
            pos = end_pos;
        }
        xos_unspinlock(&dentry_lock);
        xos_unspinlock(&inode_lock);
        for (i = 0; i < count; ++i) {
            filp->f_pos = snapshot[i].offset + 1;
            if (filldir(buf, snapshot[i].name, snapshot[i].name_len,
                        snapshot[i].offset, snapshot[i].inode_no,
                        snapshot[i].type) < 0) {
                filp->f_pos = snapshot[i].offset;
                return 0;
            }
        }
        filp->f_pos = pos;
    }
    return 0;
}

xfile_ops_t devfs_dir_file_ops = {
    .open = devfs_dir_open,
    .readdir = devfs_dir_readdir,
};

void devfs_super_init(xsuperblock * super,int flags,void *data)
{
    memset(super, 0, sizeof(*super));
    super->block_size = PAGE_SIZE;
    super->mount_behavior = flags;
    super->maigc = DEVFS_MAGIC;
    list_init(&super->inode_list);
    super->super_ops = NULL;
    super->ref_count = 1;
    xos_spinlock_init(&super->lock);
    
}

/*
    根据不同mode,初始化不同的操作函数
*/
xinode* alloc_devfs_node_init(xsuperblock * super,int mode,devno_t dev)
{

    /*
        首先判断super operation_ops 是否初始化了相应的alloc_inode 函数
        如果存在,则使用对应的函数分配，这个没有实现暂时屏蔽掉

    */
    xinode *inode = NULL;
    if (!S_ISDIR(mode) && !S_ISCHR(mode) && !S_ISBLK(mode)) {
        return NULL;
    }
    /*
    if(super->super_ops->alloc_inode){
        printk("%s:%d\n\r",__FUNCTION__,__LINE__);
    }else
    */
    {
        inode = alloc_cache_xinode();
        if(!inode){
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            return NULL;
        }

    }
     inode->ref_count = 0;
     inode->link_cnt = S_ISDIR(mode) ? 2 : 1;
     inode->uid = 0;
     inode->gid = 0;
     inode->file_size = 0;
     inode->file_size_limit = 0;
     inode->flags = 0;
     inode->chardev = NULL;
     inode->inode_ops = NULL;
     inode->file_ops = NULL;
     inode->private_data = NULL;
     list_init(&inode->device_list);
     inode->devno = dev;
     inode->node_mode = mode;
     inode->block_count = 0; /*tmp value*/
     inode->p_super = super;
     list_init(&inode->bind_xsuper_list);

     /*
        mode 用来区分是那种文件类型
     */
     switch(mode & S_IFMT){
        case S_IFDIR:
            inode->inode_ops = &devfs_dir_fnode_ops;
            inode->file_ops = &devfs_dir_file_ops;
            printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
            break;
        case S_IFCHR:
        case S_IFBLK:
            special_xnode_init(inode,mode,dev);
            break;
     }
     return inode;
     
}



xsuperblock *xos_init_devfs(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data)
{
    xsuperblock *dev_super = NULL;
    xinode *root_inode;
    xdentry *root_dentry;
    path_name_t root_name;

    dev_super = xos_kmalloc(sizeof(xsuperblock));
    if(dev_super == NULL){
        return NULL;
    }
    /*init super*/
    devfs_super_init(dev_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_devfs_node_init(dev_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){
        goto alloc_devfs_fail;
    }
    root_name.path_name = (unsigned char*)"/dev";
    root_name.len = strlen("/dev");
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    root_dentry = xdentry_cache_alloc_init(NULL,&root_name);
    if(!root_dentry){
        goto xdentry_alloc_fail;
    }
    root_dentry->file_node =  root_inode;
    dev_super->root_dentry = root_dentry;
    dev_super->mount_behavior |= MS_ACTIVE;
    printk(PT_DEBUG,"%s:%d,root_dentry->file_name.path_name=%s\n\r",__FUNCTION__,__LINE__,root_dentry->file_name.path_name);

    return dev_super;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_devfs_fail:
    xos_kfree(dev_super);
    return NULL;
}

/*
    文件系统卸载部分暂时未实现
*/
void free_dev_filesystem(xsuperblock *super)
{
    /*
        to do
    */
}

void free_dev_fnode_cache(xdentry *dentry)
{
    /*
        to do
    */
}

void xos_release_devfs(xsuperblock *super)
{
    if (super->root_dentry){
        free_dev_fnode_cache(super->root_dentry);
    }
    free_dev_filesystem(super);
}

xvfs_file_t  devfs ={
    .fs_type = "devfs",
    .fs_flags = XOS_FS_NODEV,
    .init_file_system = xos_init_devfs,
    .release_file_system = xos_release_devfs,

};

void init_devfs()
{
    xos_spinlock_init(&devfs_lock);
    list_init(&devfs_pending_devices);
    xfs_type_register(&devfs);
}

#if 0
int init_file_devfs_old(struct xos_vfs_file_type *fs_name,int flags,const char *dev_name,void *data,xdentry **root_dentry)
{
    int error = -1;
    xsuperblock *dev_super = xos_kmalloc(sizeof(xsuperblock));
    xinode *root_inode;
    path_name_t *root_name = xos_kmalloc(sizeof(path_name_t));
    if(dev_super == NULL){
        error = -ENOMEM;
        return error;
    }
     printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    /*init super*/
    devfs_super_init(dev_super,flags,(void*)data);
    /*
        给文件系统的根创建文件节点root_inode,并初始化
    */
    root_inode = alloc_devfs_node_init(dev_super,S_IFDIR | 0755,0);
    if(root_inode == NULL){

        error = -ENOMEM;
        goto alloc_devfs_fail;
    }
    root_name->path_name = (unsigned char*)"/dev";
    root_name->len = strlen("/dev");
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    *root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    if(!*root_dentry){
        goto xdentry_alloc_fail;
    }
    *root_dentry = xdentry_cache_alloc_init(NULL,root_name);
    (*root_dentry)->file_node =  root_inode;
    (*root_dentry)->xsb = dev_super;
    dev_super->root_dentry = *root_dentry;
    dev_super->mount_behavior |= MS_ACTIVE;
    xos_spinlock_init(&devfs_lock);


    return 0;
xdentry_alloc_fail:
    free_cache_xinode(root_inode);
alloc_devfs_fail:
    xos_kfree(dev_super);
    xos_kfree(root_name);
    return error;
}
#endif
