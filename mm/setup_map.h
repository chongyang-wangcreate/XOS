#ifndef __SETUP_PAGE_H__
#define __SETUP_PAGE_H__

/*
    paging map data
*/
#define PAGE_SHIFT  12
#define PAGE_SIZE (1<< 12)

#define KERNEL_EXEC_ADDR         0xFFFFFFFF40030000
#define KERNEL_VRIT KERNEL_EXEC_ADDR
#define KERNEL_PHY  0x0000000040030000
/*
      pgd     pud      pmd      pte     phy_offset
    ------ |------- |------- |--------|------------|
     9b    |  9b    |   9b   |   9b   |   12b      |
*/
#define PGD_ITEMS 512
#define PUD_ITEMS 512
#define PMD_ITEMS 512
#define PTE_ITEMS 512

typedef uint64	pte_t;
typedef uint64 pud_t;
typedef uint64  pmd_t;
typedef uint64  pgd_t;

/*
typedef struct struct_pgd
{
    unsigned long entry[PGD_ITEMS];
} pgd_t;


typedef struct struct_pud
{
    unsigned long entry[PUD_ITEMS];
} pud_t;

typedef struct struct_pmd
{
    unsigned long entry[PMD_ITEMS];
} pmd_t;

typedef struct struct_pte
{
    unsigned long entry[PTE_ITEMS];
} pte_t;


extern int pgd_map(pgd_t *pgd_p,u64 vaddr,u64 paddr,u64 size,u64 attr);
extern int pud_map(pgd_t *pgd_p, u64 vaddr, u64 end, u64 paddr, u64 attr);
extern int pmd_map(pud_t *pud_p, u64 vaddr, u64 end, u64 paddr, u64 attr);
extern int ptd_map(pmd_t *pmd_p, u64 vaddr, u64 end, u64 paddr, u64 attr);

*/
extern void xos_linear_maps(uint64 phy_start, uint64 phy_end);
extern int xos_pages_map(pgd_t *pgdir, void *va, uint64 size, unsigned long pa, uint64 prot);
extern int map_page(pgd_t *pgdir, void *va, uint64 size, unsigned long pa, uint64 prot);
extern int xos_3level_one_pagemap (pgd_t *pgdir, void *va, unsigned long pa, uint64 prot);
extern pte_t * copy_create_3level_page(pgd_t *pgdir, void *va);


extern int find_pte_phy(pgd_t *pgdir, void *va,uint64 *phy_addr);

#define ALIGN_UP(x, align)    (uint64)(((uint64)(x) +  (align - 1)) & ~(align - 1))

extern void xos_2level_maps(uint64 *pgd,uint64 virt, uint64 phy, uint len, uint64 mm_attr);
extern void all_phys_linear_map();


#endif
