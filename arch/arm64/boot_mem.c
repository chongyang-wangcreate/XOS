/*
    Development Started: 2023-11
    Copyright (C) 2023-2027 wangchongyang
    Email: rockywang599@gmail.com
    
    This program is licensed under the GPL v2 License. See LICENSE for more details.
*/

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "uart.h"
#include "mmu.h"
#include "mem_layout.h"
#include "common.h"
#include "string.h"
#include "printk.h"
#include "gic-v3.h"
#include "mmu.h"
#include "uart.h"

/*
    boot 阶段的内存管理主要为早期页表分配内存
    boot 内存关于与架构和资源强相关
    当前实现的比较简陋

*/

#define BOOT_MEM_LEN  0x10000

extern uint64 bss_end[];


extern uint64 _boot_mem_begin;

uint64 *p_boot_mem_start  =&_boot_mem_begin;


uint64 boot_mem_start = 0;
uint64 boot_mem_end  = 0;


extern void boot_puts (char *s);




void boot_mem_init()
{
    boot_mem_start = (uint64)p_boot_mem_start; /*引用*/
    boot_mem_end = (uint64)((uint64)boot_mem_start + BOOT_MEM_LEN);

}



uint64 boot_mem_alloc(size_t size) {
    uint64 ret_addr = (uint64)boot_mem_start;
    boot_mem_start += ALIGN_UP(size, sizeof(uint64));

    return ret_addr;
}


void *boot_alloc_l1_pgd()
{
    boot_puts("boot_alloc_l1_pgd..\n\r");
    return (void*)boot_mem_alloc(0x1000);
}



void *boot_alloc_l2_pmd()
{
    boot_puts("boot_alloc_l2_pmd..\n\r");
    return (void*)boot_mem_alloc(0x4000);
}






