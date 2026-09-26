#ifndef __XOS_BLOCK_IO_H__
#define __XOS_BLOCK_IO_H__

#include "types.h"
#include "list.h"
#include "spinlock.h"

struct xos_blkdev;

#define XOS_BLOCK_IO_DEFAULT_DEADLINE_TICKS 100U

typedef enum {
    XOS_BLOCK_IO_READ = 0,
    XOS_BLOCK_IO_WRITE,
    XOS_BLOCK_IO_FLUSH,
} xos_block_io_op_t;

struct xos_block_io_request;
typedef void (*xos_block_io_complete_fn)(
    struct xos_block_io_request *request, void *data);

typedef struct xos_block_io_request {
    dlist_t list;
    struct xos_block_io_request *merge_next;
    struct xos_blkdev *bdev;
    xos_block_io_op_t op;
    uint64 sector;
    uint32 sector_count;
    void *buffer;
    int status;
    uint64 sequence;
    uint64 deadline_tick;
    xos_block_io_complete_fn complete;
    void *complete_data;
} xos_block_io_request_t;

typedef struct xos_block_io_queue {
    dlist_t request_list;
    xos_spinlock_t lock;
    struct xos_blkdev *bdev;
    uint64 current_sector;
    uint64 next_sequence;
    uint32 pending_count;
    int direction;
    int initialized;
    uint32 deadline_ticks;
    uint64 merged_count;
    uint64 completed_count;
} xos_block_io_queue_t;

void xos_block_io_queue_init(xos_block_io_queue_t *queue,
                             struct xos_blkdev *bdev);
int xos_block_io_enqueue(xos_block_io_queue_t *queue,
                         xos_block_io_request_t *request);
int xos_block_io_submit_async(xos_block_io_queue_t *queue,
                              xos_block_io_request_t *request);
int xos_block_io_dispatch_locked(xos_block_io_queue_t *queue);
int xos_block_io_dispatch(xos_block_io_queue_t *queue);
uint32 xos_block_io_pending(const xos_block_io_queue_t *queue);

#endif
