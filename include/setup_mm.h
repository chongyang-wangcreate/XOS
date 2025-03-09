#ifndef __SETUP_MM_H__
#define __SETUP_MM_H__


static inline void set_ttbr0_el1(uint64 u_pgd)
{
    uint64	val64;
    val64 = (uint64)u_pgd;
    asm volatile("MSR TTBR0_EL1, %[v]": :[v]"r" (val64):);
    asm volatile("ISB":::);

}
extern int boot_init_early_map();
extern void clear_bss (void);
extern void test_mmu_map_fun();
extern void config_enable_mmu();

#endif
