/********************************************************
    
    development start:20246
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2026 - 2028 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/

#include "types.h"
#include "string.h"
#include "spinlock.h"
#include "xos_pid.h"

#define MAX_PID 65535
#define PID_SEG_SIZE 64
#define PID_MAP_SIZE ((MAX_PID + 1 + PID_SEG_SIZE - 1) / PID_SEG_SIZE)

typedef struct {
    int next_pid;
    xos_spinlock_t lock;
    uint64 used_pids[PID_MAP_SIZE];
} pid_desc_t;

static pid_desc_t pid_state;

static void pid_mark_used(int pid)
{
    pid_state.used_pids[pid / PID_SEG_SIZE] |= 1ULL << (pid % PID_SEG_SIZE);
}

static void pid_mark_free(int pid)
{
    pid_state.used_pids[pid / PID_SEG_SIZE] &= ~(1ULL << (pid % PID_SEG_SIZE));
}

/*
 * Return the first zero bit in a 64-bit word.
 *
 * The caller checks that at least one zero bit exists.  Narrowing the
 * non-zero value from 64 bits to 32, 16, 8, 4, 2 and 1 bits avoids a
 * per-bit loop while keeping this independent of compiler builtins.
 */
static int pid_find_free_bit(uint64 value)
{
    uint64 available = ~value;
    int bit = 0;

    if ((available & 0xffffffffUL) == 0) {
        available >>= 32;
        bit += 32;
    }
    if ((available & 0xffffUL) == 0) {
        available >>= 16;
        bit += 16;
    }
    if ((available & 0xffUL) == 0) {
        available >>= 8;
        bit += 8;
    }
    if ((available & 0xfUL) == 0) {
        available >>= 4;
        bit += 4;
    }
    if ((available & 0x3UL) == 0) {
        available >>= 2;
        bit += 2;
    }
    if ((available & 0x1UL) == 0) {
        bit++;
    }

    return bit;
}

static int pid_alloc_from_segment(int segment, uint64 used,
                                  int first_bit, int last_pid)
{
    uint64 available;
    int bit;
    int pid;

    if (first_bit > 0) {
        used |= (1ULL << first_bit) - 1;
    }

    available = ~used;
    if (available == 0) {
        return -1;
    }

    bit = pid_find_free_bit(used);
    pid = segment * PID_SEG_SIZE + bit;
    if (pid <= 0 || pid > last_pid) {
        return -1;
    }

    pid_mark_used(pid);
    return pid;
}

void pid_init(void)
{
    memset(&pid_state, 0, sizeof(pid_state));
    xos_spinlock_init(&pid_state.lock);
    pid_state.next_pid = 1;
    pid_mark_used(0);
}

int alloc_pid(void)
{
    unsigned long flags;
    int start_pid;
    int start_seg;
    int start_bit;
    int seg_offset;
    int pid;

    flags = xos_spin_lock_irqsave(&pid_state.lock);
    start_pid = pid_state.next_pid;
    if(start_pid <= 0 || start_pid > MAX_PID){
        start_pid = 1;
    }
    start_seg = start_pid / PID_SEG_SIZE;
    start_bit = start_pid % PID_SEG_SIZE;

    /*
     * Search one 64-bit bitmap segment .  A full segment is
     * skipped immediately; pid_find_free_bit() then narrows down the
     * first available PID in the selected segment.
     */
    for(seg_offset = 0; seg_offset < PID_MAP_SIZE; seg_offset++){
        int seg = (start_seg + seg_offset) % PID_MAP_SIZE;
        int first_bit = seg_offset == 0 ? start_bit : 0;

        pid = pid_alloc_from_segment(seg, pid_state.used_pids[seg],
                                     first_bit, MAX_PID);
        if(pid > 0){
            pid_state.next_pid = pid + 1;
            xos_spin_unlock_irqrestore(&pid_state.lock, flags);
            return pid;
        }
    }

    /*
     * The first segment was searched only from next_pid onward.  Search its
     * lower bits after the other segments to complete the circular scan.
     */
    if(start_bit != 0){
        pid = pid_alloc_from_segment(start_seg,
                                     pid_state.used_pids[start_seg],
                                     0, start_pid - 1);
        if(pid > 0){
            pid_state.next_pid = pid + 1;
            xos_spin_unlock_irqrestore(&pid_state.lock, flags);
            return pid;
        }
    }

    xos_spin_unlock_irqrestore(&pid_state.lock, flags);
    return -1;
}

void free_pid(int pid)
{
    unsigned long flags;

    if(pid <= 0 || pid > MAX_PID){
        return;
    }

    flags = xos_spin_lock_irqsave(&pid_state.lock);
    pid_mark_free(pid);
    if(pid < pid_state.next_pid){
        pid_state.next_pid = pid;
    }
    xos_spin_unlock_irqrestore(&pid_state.lock, flags);
}
