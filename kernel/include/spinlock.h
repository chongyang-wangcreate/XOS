#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "types.h"



typedef struct { 

    u16 owner;
    u16 next;
} xos_spinlock_t;


extern void xos_spinlock(xos_spinlock_t *lock);

extern void xos_unspinlock(xos_spinlock_t *lock);
extern void xos_spinlock_init(xos_spinlock_t *lock);

#endif
