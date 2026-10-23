#ifndef __XOS_BLOCK_IO_H__
#define __XOS_BLOCK_IO_H__

#include "types.h"
#include "list.h"
#include "spinlock.h"

struct xos_block_device;
struct xos_blk_req_que;

#define XOS_BLOCK_IO_DEFAULT_DEADLINE_TICKS 100U

typedef enum {
    XOS_BLOCK_IO_READ = 0,
    XOS_BLOCK_IO_WRITE,
    XOS_BLOCK_IO_FLUSH,
} xos_block_io_op_t;

typedef struct xos_blk_req xos_blk_req_t;
typedef struct xos_blk_req_que xos_blk_req_que_t;

typedef void (*xos_block_io_complete_fn)(xos_blk_req_t *request, void *data);

struct xos_blk_req {
    dlist_t list;
//  struct xos_blk_req *merge_next;
//  struct xos_blk_req *merge_tail;
    xos_blk_req_t *merge_next;
    /* 引入merget_tail 指向尾部最后一个节点，避免遍历 */
    xos_blk_req_t *merge_tail;
    struct xos_block_device *bdev;
    xos_block_io_op_t op;
    uint64 sector;
    uint32 sector_count;
    void *buffer;
    int status;
    uint64 sequence;
    uint64 deadline_tick;
    xos_block_io_complete_fn complete;
    void *complete_data;
};

/* 
    Refer to Linux
    Called with queue->lock held,must not sleep or perform I/O. 
*/
typedef struct xos_blk_req_que_ops {
    
    int (*merge_requests)(xos_blk_req_que_t *queue,
                          xos_blk_req_t *first,
                          xos_blk_req_t *second);
    int (*submit_request)(xos_blk_req_que_t *queue,
                          xos_blk_req_t *request);
    int (*dispatch_requests)(xos_blk_req_que_t *queue);
} xos_blk_req_que_ops_t;

struct xos_blk_req_que {
    dlist_t request_list;
    xos_spinlock_t lock;
    struct xos_block_device *bdev;
    const xos_blk_req_que_ops_t *ops;
    uint64 current_sector;
    uint64 next_sequence;
    uint32 pending_count;
    int direction;
    int initialized;
    uint32 deadline_ticks;
    uint64 merged_count;
    uint64 completed_count;
};

void xos_block_io_queue_init(xos_blk_req_que_t *queue,
                             struct xos_block_device *bdev);
int xos_block_io_enqueue(xos_blk_req_que_t *queue, xos_blk_req_t *request);
int xos_block_io_submit_async(xos_blk_req_que_t *queue,
                              xos_blk_req_t *request);
int xos_block_io_dispatch_locked(xos_blk_req_que_t *queue);
int xos_block_io_dispatch(xos_blk_req_que_t *queue);
uint32 xos_block_io_pending(const xos_blk_req_que_t *queue);

/* 本来使用的是xos_block_req_t 总觉缺少了IO不太好，重新起别名也能跟文件名对应 */
typedef xos_blk_req_t xos_block_io_request_t;
typedef xos_blk_req_que_t xos_block_io_queue_t;

#endif
