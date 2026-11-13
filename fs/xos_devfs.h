#ifndef __XOS_DEVFS_H__
#define __XOS_DEVFS_H__

#include "xos_dev.h"


extern void init_devfs();

/* 设备注册表发布/撤销节点,owner 必须是注册设备对象本身。 */
int devfs_device_add(const char *name, int mode, devno_t devno, const void *owner);
int devfs_device_remove(const char *name, int mode, devno_t devno, const void *owner);
/* /dev 挂载后发布挂载前已注册的设备节点。 */
extern int devfs_check_pending_device_add(void);


#endif
