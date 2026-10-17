#ifndef __LOAD_USER_H__
#define __LOAD_USER_H__

#include "types.h"
#include "mem_layout.h"

#define VA_BITS 39

#define USER_SPACE_DEVMAP      0x110000000
#define USER_STACK_SIZE        0x800000
#define USER_STACK_TOP    (1UL << VA_BITS)
#define USER_STACK_START  (USER_STACK_TOP - USER_STACK_SIZE)
#define USER_STACK_INIT_SP (USER_STACK_TOP & ~0xfUL)

#define __va(x) P2V(x)
#define __pa(x) ((void *)V2P(x))
#define V_TO_POINTER(virt) ((void *)V2P(virt))
#define V_TO_P(virt) V2P(virt)

#define _TO_UVA_(x) ((void *)((unsigned long)(x) + USER_SPACE_DEVMAP))

#define UART_BASE       0x09000000

void user_page_mapping(uint64_t *pg_dir, void *virt_addr,
            uint64_t phy_addr, unsigned long size,
            int prot);

extern void user_load_task(char *array);
extern int process_create();


#endif
