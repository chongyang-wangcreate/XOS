#ifndef __XOS_MEMBLOCK_H__
#define __XOS_MEMBLOCK_H__

#include "types.h"
typedef struct xos_memblock_region{
    uint64 base;
    uint64 size;
}xos_memblock_region_t;

typedef struct xos_memblock_info{
    xos_memblock_region_t memory;
    xos_memblock_region_t reserved;
    xos_memblock_region_t usable;
    int vaild;
}xos_memblock_info_t;

extern int xos_memblock_init();
extern  xos_memblock_info_t *xos_memblock_get_info(void);
#endif