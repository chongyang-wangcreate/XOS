#ifndef __XOS_DISK_H__
#define __XOS_DISK_H__

#include "xos_dev.h"
#include "list.h"
#include "spinlock.h"
#include "xos_sem.h"

/* Forward declarations */
struct xos_inode;
struct blk_request_queue;
struct block_device_operations;

#ifndef DISK_NAME_LEN
#define DISK_NAME_LEN            32
#endif

#ifndef PARTATION_NAME_LEN
#define PARTATION_NAME_LEN       32
#endif
#ifndef DISK_MAX_PARTS
#define DISK_MAX_PARTS            256
#endif

/*
    disk implement 
*/
struct partition_desc {
    xdevice_t device;

    unsigned long state;

    long start_sector;

    long sector_count;

    int read_only;

    unsigned int read_times;
    unsigned int read_sectors;
    unsigned int write_times;
    unsigned int write_sectors;

    char partition_name[PARTATION_NAME_LEN];
};


typedef struct xos_disk_device {
    xdevice_t device;

    char name[DISK_NAME_LEN];

    int flags;

    int max_partition_count;

    int dev_major;

    int read_only;

    int device_sequence;

    long sector_count;

    struct blk_request_queue *queue;

    int request_count;
    const struct block_device_operations *fops;
    void *private_data;

    int partition_count;

    struct partition_desc part[0];
}xos_disk_dev_t;





#endif
