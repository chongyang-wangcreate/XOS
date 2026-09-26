/********************************************************
    development timer:2026.9
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2026-2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Block I/O elevator scheduler.

    Implements a bidirectional SCAN (elevator) algorithm with deadline
    dispatch and adjacent-request merging.  Each block device owns one
    xos_block_io_queue_t, initialized in xos_blockdev_register().

    Request lifecycle:
      1. xos_block_io_enqueue()      - validate, set sequence/deadline
      2. merge attempt              - try to merge with existing request
      3. xos_block_io_dispatch()     - pick best request, execute, complete

    Dispatch selection order:
      a. Expired-deadline requests (oldest first)
      b. Nearest sector in current scan direction
      c. Reverse direction and retry if nothing found
      d. Pending flush requests (oldest first)

********************************************************/

#include "types.h"
#include "list.h"
#include "error.h"
#include "spinlock.h"
#include "tick_timer.h"
#include "xos_block_io.h"
#include "xos_block_dev.h"

static int xos_block_io_request_valid(const xos_block_io_request_t *request)
{
    if (request == NULL || request->bdev == NULL) {
        return 0;
    }
    if (request->op == XOS_BLOCK_IO_FLUSH) {
        return request->buffer == NULL && request->sector_count == 0;
    }
    return request->sector_count != 0 && request->buffer != NULL;
}

void xos_block_io_queue_init(xos_block_io_queue_t *queue,
                             struct xos_blkdev *bdev)
{
    if (queue == NULL) {
        return;
    }
    list_init(&queue->request_list);
    xos_spinlock_init(&queue->lock);
    queue->bdev = bdev;
    queue->current_sector = 0;
    queue->next_sequence = 0;
    queue->pending_count = 0;
    queue->direction = 1;
    queue->initialized = 1;
    queue->deadline_ticks = XOS_BLOCK_IO_DEFAULT_DEADLINE_TICKS;
    queue->merged_count = 0;
    queue->completed_count = 0;
}

static int xos_block_io_can_merge(const xos_block_io_request_t *first,
                                  const xos_block_io_request_t *second)
{
    uint64 first_end;
    uint64 first_buffer_end;

    if (first->op == XOS_BLOCK_IO_FLUSH ||
        second->op != first->op ||
        first->bdev != second->bdev ||
        first->sector_count > 0xffffffffU - second->sector_count) {
        return 0;
    }
    first_end = first->sector + first->sector_count;
    first_buffer_end = (uint64)(uintptr_t)first->buffer +
                       (uint64)first->sector_count *
                       first->bdev->sector_size;
    return first_end == second->sector &&
           first_buffer_end == (uint64)(uintptr_t)second->buffer;
}

static void xos_block_io_merge_locked(xos_block_io_queue_t *queue,
                                      xos_block_io_request_t *first,
                                      xos_block_io_request_t *second)
{
    second->status = -EINPROGRESS;
    second->merge_next = NULL;
    if (first->merge_next == NULL) {
        first->merge_next = second;
    } else {
        xos_block_io_request_t *tail = first->merge_next;
        while (tail->merge_next != NULL) {
            tail = tail->merge_next;
        }
        tail->merge_next = second;
    }
    first->sector_count += second->sector_count;
    queue->merged_count++;
}

int xos_block_io_enqueue(xos_block_io_queue_t *queue,
                         xos_block_io_request_t *request)
{
    if (queue == NULL || !queue->initialized ||
        !xos_block_io_request_valid(request) ||
        request->bdev != queue->bdev) {
        return -EINVAL;
    }
    if (request->op != XOS_BLOCK_IO_FLUSH &&
        (request->sector >= request->bdev->sector_count ||
         (uint64)request->sector_count >
             request->bdev->sector_count - request->sector)) {
        return -EINVAL;
    }
    if (request->op == XOS_BLOCK_IO_WRITE &&
        (request->bdev->flags & XOS_BLOCKDEV_READ_ONLY)) {
        return -EROFS;
    }
    if (request->op != XOS_BLOCK_IO_READ &&
        request->op != XOS_BLOCK_IO_WRITE &&
        request->op != XOS_BLOCK_IO_FLUSH) {
        return -EINVAL;
    }

    list_init(&request->list);
    request->merge_next = NULL;
    request->status = -EINPROGRESS;
    xos_spinlock(&queue->lock);
    request->sequence = queue->next_sequence++;
    request->deadline_tick = kernel_ticks + queue->deadline_ticks;
    {
        dlist_t *node;
        list_for_each(node, (&queue->request_list)) {
            xos_block_io_request_t *queued =
                list_entry(node, xos_block_io_request_t, list);
            if (xos_block_io_can_merge(queued, request)) {
                xos_block_io_merge_locked(queue, queued, request);
                xos_unspinlock(&queue->lock);
                return 0;
            }
        }
    }
    list_add_back(&request->list, &queue->request_list);
    queue->pending_count++;
    xos_unspinlock(&queue->lock);
    return 0;
}

int xos_block_io_submit_async(xos_block_io_queue_t *queue,
                              xos_block_io_request_t *request)
{
    return xos_block_io_enqueue(queue, request);
}

static xos_block_io_request_t *xos_block_io_pick_locked(
    xos_block_io_queue_t *queue)
{
    dlist_t *node;
    xos_block_io_request_t *best = NULL;
    xos_block_io_request_t *flush = NULL;
    uint64 distance;
    uint64 best_distance;
    uint64 oldest_deadline = ~0UL;
    xos_block_io_request_t *deadline_request = NULL;
    int pass;

    list_for_each(node, (&queue->request_list)) {
        xos_block_io_request_t *request =
            list_entry(node, xos_block_io_request_t, list);
        if (request->deadline_tick <= kernel_ticks &&
            request->deadline_tick < oldest_deadline) {
            oldest_deadline = request->deadline_tick;
            deadline_request = request;
        }
    }
    if (deadline_request != NULL) {
        return deadline_request;
    }

    for (pass = 0; pass < 2; pass++) {
        best = NULL;
        best_distance = ~0UL;
        list_for_each(node, (&queue->request_list)) {
            xos_block_io_request_t *request =
                list_entry(node, xos_block_io_request_t, list);

            if (request->op == XOS_BLOCK_IO_FLUSH) {
                if (flush == NULL || request->sequence < flush->sequence) {
                    flush = request;
                }
                continue;
            }
            if (queue->direction > 0) {
                if (request->sector < queue->current_sector) {
                    continue;
                }
                distance = request->sector - queue->current_sector;
            } else {
                if (request->sector > queue->current_sector) {
                    continue;
                }
                distance = queue->current_sector - request->sector;
            }
            if (best == NULL || distance < best_distance ||
                (distance == best_distance &&
                 request->sequence < best->sequence)) {
                best = request;
                best_distance = distance;
            }
        }
        if (best != NULL) {
            return best;
        }
        queue->direction = -queue->direction;
    }
    return flush;
}

static int xos_block_io_execute(xos_block_io_request_t *request)
{
    xos_blkdev_t *bdev = request->bdev;

    if (!bdev->registered || bdev->ops == NULL) {
        return -ENODEV;
    }
    if (request->op == XOS_BLOCK_IO_READ) {
        if (bdev->ops->read_sectors == NULL) {
            return -ENOSYS;
        }
        return bdev->ops->read_sectors(bdev, request->sector,
                                       request->buffer,
                                       request->sector_count);
    }
    if (request->op == XOS_BLOCK_IO_WRITE) {
        if (bdev->ops->write_sectors == NULL) {
            return -ENOSYS;
        }
        return bdev->ops->write_sectors(bdev, request->sector,
                                        request->buffer,
                                        request->sector_count);
    }
    if (bdev->ops->flush == NULL) {
        return 0;
    }
    return bdev->ops->flush(bdev);
}

static void xos_block_io_complete_chain(xos_block_io_queue_t *queue,
                                        xos_block_io_request_t *request,
                                        int status)
{
    xos_block_io_request_t *current = request;

    while (current != NULL) {
        current->status = status;
        if (current->complete != NULL) {
            current->complete(current, current->complete_data);
        }
        queue->completed_count++;
        current = current->merge_next;
    }
}

int xos_block_io_dispatch_locked(xos_block_io_queue_t *queue)
{
    xos_block_io_request_t *request;
    int dispatched = 0;

    if (queue == NULL || !queue->initialized || queue->bdev == NULL) {
        return -EINVAL;
    }
    for (;;) {
        xos_spinlock(&queue->lock);
        if (queue->pending_count == 0) {
            xos_unspinlock(&queue->lock);
            break;
        }
        request = xos_block_io_pick_locked(queue);
        list_del(&request->list);
        list_init(&request->list);
        queue->pending_count--;
        xos_unspinlock(&queue->lock);

        {
            xos_block_io_request_t *merge_next = request->merge_next;
            int status = xos_block_io_execute(request);

            xos_block_io_complete_chain(queue, request, status);
            request->merge_next = merge_next;
        }
        if (request->op != XOS_BLOCK_IO_FLUSH) {
            queue->current_sector = request->sector + request->sector_count;
        }
        dispatched++;
    }
    return dispatched;
}

int xos_block_io_dispatch(xos_block_io_queue_t *queue)
{
    xos_blkdev_t *bdev;
    int dispatched;

    if (queue == NULL || !queue->initialized || queue->bdev == NULL) {
        return -EINVAL;
    }
    bdev = queue->bdev;
    xos_spinlock(&bdev->io_lock);
    dispatched = xos_block_io_dispatch_locked(queue);
    xos_unspinlock(&bdev->io_lock);
    return dispatched;
}

uint32 xos_block_io_pending(const xos_block_io_queue_t *queue)
{
    return queue == NULL ? 0 : queue->pending_count;
}
