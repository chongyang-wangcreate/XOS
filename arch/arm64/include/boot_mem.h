#ifndef __BOOT_MEM_H__
#define __BOOT_MEM_H__


extern void *boot_alloc_l1_pgd();
extern void *boot_alloc_l2_pmd();
extern void boot_mem_init();


#endif
