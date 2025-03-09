#ifndef __XOS_KERN_DEF_H__
#define __XOS_KERN_DEF_H__


#include "list.h"
#include "spinlock.h"
#include "pt_frame.h"
#include "xos_dev.h"
#include "mount.h"

typedef uint64	pte_t;
typedef uint64  pud_t;
typedef uint64  pmd_t;
typedef uint64  pgd_t;

#define NORMAL_PAGE_SIZE (1 << 12)

#define KERNEL_STACK_SIZE 8192

#define align_up(x, align)    (uint64)(((uint64)(x) +  (align - 1)) & ~(align - 1))
#define align_down(x, align) ((uint64)(x) & ~((uint64)(align)-1))


#define PAGE_SIZE (1<< 12)

#endif
