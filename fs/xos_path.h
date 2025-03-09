#ifndef __XOS_PATH_H__
#define __XOS_PATH_H__

struct xos_mount_desc;

typedef struct path_name{
    unsigned char *path_name;
    unsigned int len;
}path_name_t;
/*
    目录查找描述符
*/
typedef struct fs_path_lookup{
    xos_spinlock_t lock;
    path_name_t cur_path; /*当前路径或分路径*/
    path_name_t last_path;
    struct xos_mount_desc *look_mnt; /*安装的文件系统对象描述指针(xmount_t)*/
    struct xos_dentry *look_dentry;  /*cur_path parent_dentry*/
    struct xos_dentry *file_dentry;

    /*
        这两个变量造成超多混乱
    */
    struct xos_mount_desc *f_tmp_mnt;   /*这个变量废弃，用局部临时变量取代,容易和look_mnt混乱*/
    struct xos_dentry *f_tmp_dentry;    /*这个变量废弃 ，用局部临时变量取代*/
    int ref;
    int path_type; /*路径分量类型*/
    int flags;  /*标识路径*/
    int mode;
}lookup_path_t;



typedef struct fs_path_lookup_old{
    xos_spinlock_t lock;
    struct xos_dentry *look_dentry;  /*cur_path parent_dentry*/
    path_name_t cur_path; /*当前路径或分路径*/
    path_name_t last_path;
    struct xos_mount_desc *mnt; /*安装的文件系统对象描述指针(xmount_t)*/
    struct xos_dentry *file_dentry;
    struct xos_mount_desc *file_tmp_mnt;   /*这个变量废弃，用局部临时变量取代*/
    struct xos_dentry *file_tmp_dentry;    /*这个变量废弃 ，用局部临时变量取代*/
    int ref;
    int path_type; /*路径分量类型*/
    int flags;  /*标识路径*/
    int mode;
}lookup_path_old_t;


struct path {
    struct xos_dentry *dentry;
    struct xos_mount_desc  *mnt;
};

#endif
