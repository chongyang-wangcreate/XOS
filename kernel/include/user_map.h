#ifndef __LOAD_USER_H__
#define __LOAD_USER_H__

#include "types.h"

#define VA_BITS 39

#define USER_SPACE_DEVMAP      0x110000000
#define USER_STACK_SIZE        0x800000
#define USER_STACK_TOP    (1UL << VA_BITS)
#define USER_STACK_START  (USER_STACK_TOP - USER_STACK_SIZE)
#define USER_STACK_INIT_SP (USER_STACK_TOP & ~0xfUL)

#define __va(x) ((void *)((unsigned long)(x) + VA_KERNEL_START))
#define __pa(x) ((void *)((unsigned long)(x) - VA_KERNEL_START))
#define  V_TO_POINTER(virt) ((void *)((unsigned long)(virt) - VA_KERNEL_START))
#define  V_TO_P(virt) (((unsigned long)(virt) - VA_KERNEL_START))

#define _TO_UVA_(x) ((void *)((unsigned long)(x) + USER_SPACE_DEVMAP))

#define UART_BASE       0x09000000

void user_page_mapping(uint64_t *pg_dir, void *virt_addr,
            uint64_t phy_addr, unsigned long size,
            int prot);

extern void user_load_task(char *array);
extern void process_create();


#endif
