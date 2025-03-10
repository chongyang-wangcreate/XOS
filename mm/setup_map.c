
/********************************************************
    
    development start: 2023
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2023 - 2028 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "mmu.h"
#include "string.h"
#include "printk.h"
#include "spinlock.h"
#include "setup_map.h"
#include "mem_layout.h"
#include "xos_page.h"
#include "mmu.h"
#include "error.h"
/*
      pgd     pud      pmd      pte     phy_offset
    ------ |------- |------- |--------|------------|
     9b    |  9b    |   9b   |   9b   |   12b      |
*/

#define PGDIR_OFFSET    39
#define PGDIR_SIZE      (1UL << PGDIR_OFFSET)
#define PGDIR_MASK      (~(PGDIR_SIZE-1))
#define PUD_OFFSET      30
#define PUDIR_SIZE     (1<<PUD_OFFSET)
#define PUDIR_MASK      (~(PUDIR_SIZE-1))
#define PMD_OFFSET      21
#define PMD_SIZE        (1<<PMD_OFFSET)
#define PMDIR_MASK      (~(PMD_SIZE-1))
#define PAGE_SIZE       (1<< 12)
#define PTE_ADDR_MASK (~((1UL << PAGE_SHIFT) - 1))


extern uint64 *kernel_pgtbl_tmp;


/*
48 位有效地址情况
    映射时需要继续深入搞清几个问题
    每一个pgd 目录项寻址范围时512G
    每一个pud 目录项能寻址 1G
    每一个pmd 目录项能寻址 2M
    每一个pte 目录项能寻址 4K

    48bit 虚拟地址
39 位有效地址情况：
    每一个pgd 目录项寻址范围时1G ,总共512 条目
    每一个pmd 目录项能寻址 2M，总共512 条目
    每一个pte 目录项能寻址 4K  总共512 条目
*/
extern void boot_puts (char *s);
extern int xos_3level_maps (pgd_t *pgdir, void *va, uint64 size, uint64 pa, uint64 ap);


int pte_map(pmd_t *pmd_p, u64 vaddr, u64 end, u64 paddr, u64 attr)
{
    pmd_t *pmd = pmd_p;
    pte_t *pte;
    u64 idx;
    u64 virt_pte_next;
    if(!*pmd){
        
        //地址不存在需要申请
        
    }else{
            
         pte = (pte_t *)((*pmd) & PTE_ADDR_MASK);  //4k align   pte 物理地址，未使能MMU 之前可以这么用
         
    }
    //get pte index
    idx = PTE_IDX(vaddr);
    pte += idx;

    for (virt_pte_next = vaddr; virt_pte_next != end; pte++) 
    {
        virt_pte_next = (virt_pte_next + PAGE_SIZE) & PTE_ADDR_MASK;  //步长4K
        virt_pte_next = virt_pte_next < end ? virt_pte_next : end;
        
        *pte = ((paddr >> PAGE_SHIFT) << PAGE_SHIFT) |PT_ENTRY_TABLE | PT_ENTRY_VALID | attr ;
        paddr += virt_pte_next - vaddr;
        vaddr = virt_pte_next;
    }
    return 0;
}

int pmd_map(pud_t *pud_p, u64 vaddr, u64 end, u64 paddr, u64 attr)
{
    pud_t *pud = pud_p;
    pmd_t *pmd;
    u64 idx;
    u64 virt_pmd_next;
    if(!*pud)
    {
        //地址不存在需要申请
    }else{
         pmd = (pmd_t *)((*pud) & PTE_ADDR_MASK);  //4k align
    }
//  idx = (vaddr >> PMD_OFFSET)&PMDIR_MASK;
    idx = PMD_IDX(vaddr);
    pmd += idx;

    for(virt_pmd_next = vaddr;virt_pmd_next < vaddr;pmd++)
    {
        
        virt_pmd_next = (virt_pmd_next + PMD_SIZE) & PMDIR_MASK; //步长2M
        virt_pmd_next = virt_pmd_next < end ? virt_pmd_next : end;
        pte_map(pmd, vaddr, virt_pmd_next, paddr, attr);

        paddr += virt_pmd_next- vaddr;
        vaddr = virt_pmd_next;
            
    }
    return 0;
}


int pud_map(pgd_t *pgd_p, u64 vaddr, u64 end, u64 paddr, u64 attr)
{
    pgd_t *pgd = pgd_p;
    pud_t *pud;
    u64 idx;
    u64 virt_pud_next;
//    virt_pud_end = ALIGN_UP(vaddr + size, PAGE_SIZE);
    if(!*pgd){
        /*
            pud is not exist
        */
        //     u64 pud_paddr = alloc(PAGE_SIZE);
    }else{
         pud = (pud_t *)((*pgd) & PTE_ADDR_MASK);
         }
    
    idx = PUD_IDX(vaddr); //PUD_IDX
    pud += idx;
    
    for(virt_pud_next = vaddr;virt_pud_next < end;pud++)
    {
       
       virt_pud_next = (virt_pud_next + PUDIR_SIZE) & PUDIR_MASK; //步长1G
       virt_pud_next = virt_pud_next < end ? virt_pud_next : end;
       pmd_map(pud, vaddr, virt_pud_next, paddr,attr);
       
        paddr += virt_pud_next - vaddr; //下一次pgd 映射起始物理地址
        vaddr = virt_pud_next; //下一次pgd 映射起始虚拟地址
         
    }
    return 0;
}

int pgd_map(pgd_t *pgd_p,u64 vaddr,u64 paddr,u64 size,u64 attr)
{
    u64 idx;
    u64 virt_next, virt_end;
    pgd_t *pgd = pgd_p;
    
    idx = PGD_IDX(vaddr);

    /*
        根据偏移获取具体的页目录项
    */
    pgd += idx;

    virt_end = ALIGN_UP(vaddr + size, PAGE_SIZE);
    /*
        如果单次映射小于512G则循环一次
        如果大于512G 则循环多次
    */
    for (virt_next = vaddr; virt_next != virt_end; pgd++)
    {
        virt_next = (virt_next + PGDIR_SIZE) & PGDIR_MASK;
        virt_next = virt_next < virt_end ? virt_next : virt_end;
        /*
            virt_next 地址512 对齐，低39bit 清0
        */
        pud_map(pgd, vaddr, virt_next, paddr, attr);

        paddr += virt_next - vaddr; //下一次pgd 映射起始物理地址
        vaddr = virt_next; //下一次pgd 映射起始虚拟地址
    }
    return 0;
}



static inline void flush_tlb (void)
{
    asm("tlbi vmalle1" : : :);
}


int xos_pages_map(pgd_t *pgdir, void *va, uint64 size, unsigned long pa, uint64 prot)
{
    xos_3level_maps(pgdir, va,size, pa,  prot);
    return 0;
}

int map_page(pgd_t *pgdir, void *va, uint64 size, unsigned long pa, uint64 prot)
{
    //增加以4k位步长的循环
    xos_3level_one_pagemap(pgdir, va, pa,  prot);

    return 0;
}


/*
    后续增加flags 判断是user 还是kernel
    normal mem maps
*/

void xos_linear_maps(uint64 phy_start, uint64 phy_end)
{
    uint64 map_size = phy_end - phy_start;
    pgd_t *pgdir = P2V(kernel_pgtbl_tmp);
    void *map_vaddr_star = P2V(phy_start);
    uint64  prot = PG_RW_EL1_EL0;
    
    xos_3level_maps(pgdir, map_vaddr_star,map_size, phy_start,prot);
    flush_tlb ();
}


void all_phys_linear_map()
{
   xos_linear_maps(PHY_KERNMAP_START, PHY_KERNMAP_END);
}


pmd_t* get_pmd(pgd_t *pgdir, void *va)
{
    int pgdidx;
    uint64	*level2;
    uint64 virt = (uint64)va;

    pgdidx = PGD_IDX(virt); 
   
    
    level2 = (uint64 *)(pgdir[pgdidx] & PG_4k_ADDR_MASK);

    return level2;

}

pte_t *get_pte(pmd_t *pmd,void *va)
{
    
    int		pmdidx;
    uint64	*level3;
    uint64 virt = (uint64)va;
    
    pmdidx = PMD_IDX(virt);
    level3 = (uint64 *)(pmd[pmdidx] & PG_4k_ADDR_MASK);
    return level3;
}



/*
    set 2 level page

    PGD   (Page Global Directory)                    bit[39:30]      level1

    PMD   (Page Middle Directory)                    bit[30:21]     level2

    属性配置查阅了大量资料,为方便大家查看 mmu.c mmu.h 文件中做了大量注释

*/

void xos_2level_maps(uint64 *pgd,uint64 virt, uint64 phy, uint len, uint64 mm_attr)
{
    int idx;
    int pmdidx;
    uint64 pte;  //pte 由pteaddr,和相应的标志位组成
    pmd_t *pmd_base;
   
    for (idx = 0; idx < len; idx = idx + 0x200000) {
        pmd_base = get_pmd((pgd_t*)pgd, (void*)virt);
        pte = phy & PMD_MASK;

        pte |= ACCESS_FLAG | SH_IN_SH | PG_RW_EL1 | NON_SECURE_PA | PT_ENTRY_NEXT_BLOCK | PT_ENTRY_VALID |mm_attr;

        pmdidx = PMD_IDX(virt);
        pmd_base[pmdidx] = pte;

        virt = virt + 0x200000;
        phy = phy + 0x200000;
    }


}

/*
    通过va 找到对应的pa
*/
int find_pte_phy(pgd_t *pgdir, void *va,uint64 *phy_addr)
{
        char *start;
        pgd_t *pgd;
        pmd_t *pmd_array_base;
        pmd_t *pmd;
        pte_t *pte_array_base;
        pte_t *pte;
        
        start = (char*) ALIGN_DOWN(va, PTE_ENTRY_SIZE);

        pgd = &pgdir[PGD_IDX((uint64)start)];
        if(*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){
            pmd_array_base = (pmd_t*) P2V((*pgd) & PG_4k_ADDR_MASK);
        }else{
            return -EFAULT;
        }
        pmd = &pmd_array_base[PMD_IDX(start)]; /*根据虚拟地址找到对应的pmd 项地址*/
        if(*pmd &(PT_ENTRY_TABLE | PT_ENTRY_VALID)){
            pte_array_base = (pte_t*) P2V((*pmd) & PG_4k_ADDR_MASK);
        }else{
            return -EFAULT;
        }

        pte =  &pte_array_base[PTE_IDX(start)];
        
        if((*pte&PG_4k_ADDR_MASK) == 0){
             return -EFAULT;
        }
        *phy_addr = (*pte&PG_4k_ADDR_MASK);
        return 0;
}

/*
    20240302 我们当前需要创建三级页表，三级足够了
    
*/

int xos_3level_maps (pgd_t *pgdir, void *va, uint64 size, unsigned long pa, uint64 prot)
{
    char *start, *last;
    pgd_t *pgd;
    pmd_t *pmd_array_base;
    pmd_t *pmd;
    pte_t *pte_array_base;
    pte_t *pte;
    int step_val =0;
    start = (char*) ALIGN_DOWN(va, PTE_ENTRY_SIZE);
    last = (char*) ALIGN_DOWN((uint64)va + size - 1, PTE_ENTRY_SIZE);
    /*
        for 循环每次va pa 都移动4k 步长，
    */
    for(;step_val < size ; step_val += PTE_ENTRY_SIZE){
        
        pgd = &pgdir[PGD_IDX((uint64)start)];  /*开始本来想直接使用*pgdir[PGD_IDX(uint64)va]&PG_4k_ADDR_MASK，太长了不美观*/
        
        if(*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){ /*判断一级页表页表项项是否有效*/
            pmd_array_base = (pmd_t*) P2V((*pgd) & PG_4k_ADDR_MASK);
        }else{
            /*
                无效 申请页表空间，并将页表基地址放入pmd 对应的页表项中
            */
            pmd_array_base = (pmd_t*)xos_get_free_page(0,0);

            memset(pmd_array_base, 0, PTE_ENTRY_SIZE);
            /*
                二级某个页表创建完毕，并且将页表基地址加入到一级页表(我们也成为页目录)某个目录项中

            */
            *pgd = LINEAR_V2P_UL(pmd_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;  
        }
        /*
            二级页表空间在上面已经申请完毕，下面开始初始化二级页表pmd
        */
        pmd = &pmd_array_base[PMD_IDX(start)]; /*根据虚拟地址找到对应的pmd 项地址*/
        if (*pmd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) /*判断二级页表页表项是否有效*/
        {
            pte_array_base = (pte_t*) P2V((*pmd) & PG_4k_ADDR_MASK);
        }else{
            pte_array_base = (pte_t*) xos_get_free_page(0,0);
            
            printk(PT_DEBUG,"%s:%d,pte_array_phy_base=%lx\n\r",__FUNCTION__,__LINE__,V2P(pte_array_base));
            memset(pte_array_base, 0, PTE_ENTRY_SIZE);

            *pmd = LINEAR_V2P_UL(pte_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;
        }
        
        pte =  &pte_array_base[PTE_IDX(start)];     /*pte_array_base 三级页表基地址，偏移找到具体三级页表项*/
        /*
            配置相应的三级页表项
        */
        *pte = pa | ACCESS_FLAG | SH_IN_SH | prot | NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE | PT_ENTRY_VALID;

        if (start == last) {
            break;
        }

        start += PTE_ENTRY_SIZE;
        pa += PTE_ENTRY_SIZE;
    }
   
    return 0;

}


int xos_3level_one_pagemap (pgd_t *pgdir, void *va,  unsigned long pa, uint64 prot)
{
    char *start;
//    char *last;
    pgd_t *pgd;
    pmd_t *pmd_array_base;
    pmd_t *pmd;
    pte_t *pte_array_base;
    pte_t *pte;
    start = va;
    {
        pgd = &pgdir[PGD_IDX((uint64)start)];  /*开始本来想直接使用*pgdir[PGD_IDX(uint64)va]&PG_4k_ADDR_MASK，太长了不美观*/
        
        if(*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){ /*判断一级页表页表项项是否有效*/
            pmd_array_base = (pmd_t*) P2V((*pgd) & PG_4k_ADDR_MASK);
        }else{
            /*
                无效 申请页表空间，并将页表基地址放入pmd 对应的页表项中
            */
            pmd_array_base = (pmd_t*)xos_get_free_page(0,0);
            printk(PT_RUN,"%s:%d,pmd_array_base=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)pmd_array_base);

            memset(pmd_array_base, 0, PTE_ENTRY_SIZE);
            /*
                二级某个页表创建完毕，并且将页表基地址加入到一级页表(我们也成为页目录)某个目录项中

            */
            *pgd = LINEAR_V2P_UL(pmd_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;  
        }
        /*
            二级页表空间在上面已经申请完毕，下面开始初始化二级页表pmd
        */
        pmd = &pmd_array_base[PMD_IDX(start)]; /*根据虚拟地址找到对应的pmd 项地址*/
        if (*pmd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) /*判断二级页表页表项是否有效*/
        {
            pte_array_base = (pte_t*) P2V((*pmd) & PG_4k_ADDR_MASK);
        }else{
            pte_array_base = (pte_t*) xos_get_free_page(0,0);
            printk(PT_RUN,"%s:%d,pte_array_phy_base=%lx\n\r",__FUNCTION__,__LINE__,V2P(pte_array_base));
            memset(pte_array_base, 0, PTE_ENTRY_SIZE);

            *pmd = LINEAR_V2P_UL(pte_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;
        }
        
        pte =  &pte_array_base[PTE_IDX(start)];     /*pte_array_base 三级页表基地址，偏移找到具体三级页表项*/

        /*
            配置相应的三级页表项
        */
        *pte = pa | ACCESS_FLAG | SH_IN_SH | prot | NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE | PT_ENTRY_VALID;


    }

    return 0;

}


int is_pmd_empty(pmd_t *pmd)
{
    // 只需要检查当前的 pmd 项是否有效
    if (*pmd & PT_ENTRY_VALID) {
        return 0;  // 该 pmd 项有效
    }
    return 1;  // 该 pmd 项无效
}


int xos_3level_onepage_umap(pgd_t *pgdir, void *va)
{
    char *start = va;
    pgd_t *pgd;
    pmd_t *pmd_array_base;
    pmd_t *pmd;
    pte_t *pte_array_base;
    pte_t *pte;

    // 访问一级页表
    pgd = &pgdir[PGD_IDX((uint64)start)];
    if (*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) {
        pmd_array_base = (pmd_t*)P2V(*pgd & PG_4k_ADDR_MASK);  // 获取二级页表基址
    } else {
        return -1;  // 一级页表无效，无法解除映射
    }

    // 访问二级页表
    pmd = &pmd_array_base[PMD_IDX(start)];
    if (*pmd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) {
        pte_array_base = (pte_t*)P2V(*pmd & PG_4k_ADDR_MASK);  // 获取三级页表基址
    } else {
        return -1;  // 二级页表无效，无法解除映射
    }

    // 访问三级页表
    pte = &pte_array_base[PTE_IDX(start)];
    if (*pte & PT_ENTRY_VALID) {
        *pte = 0;  // 将页表项清零或设置为无效
    } else {
        return -1;  // 三级页表项无效，无法解除映射
    }

    // 清空二级页表项
    if (is_pmd_empty(pmd)) {
        *pgd = 0;  // 清空一级页表项
        xos_free_page(pmd_array_base);  // 释放二级页表
    }

    return 0;
}



void complete_last_page_map(pte_t *pte,uint64 phys,uint64 prot)
{
    
}
pte_t * copy_create_3level_page(pgd_t *pgdir, void *va)
{

    char *start;
    pgd_t *pgd;
    pmd_t *pmd_array_base;
    pmd_t *pmd;
    pte_t *pte_array_base;
    pte_t *pte;
    start = va;
    pgd = &pgdir[PGD_IDX((uint64)start)];  
    if(*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){ /*判断一级页表页表项项是否有效*/
            pmd_array_base = (pmd_t*) P2V((*pgd) & PG_4k_ADDR_MASK);
     }else{
            /*
                无效 申请页表空间，并将页表基地址放入pmd 对应的页表项中
            */
            pmd_array_base = (pmd_t*)xos_get_free_page(0,0);
            printk(PT_DEBUG,"%s:%d,pmd_array_base=%lx\n\r",__FUNCTION__,__LINE__,(unsigned long)pmd_array_base);

            memset(pmd_array_base, 0, PTE_ENTRY_SIZE);
            /*
                二级某个页表创建完毕，并且将页表基地址加入到一级页表(我们也成为页目录)某个目录项中

            */
            *pgd = LINEAR_V2P_UL(pmd_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;  
     }
    pmd = &pmd_array_base[PMD_IDX(start)]; /*根据虚拟地址找到对应的pmd 项地址*/
    if (*pmd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) /*判断二级页表页表项是否有效*/
    {
        pte_array_base = (pte_t*) P2V((*pmd) & PG_4k_ADDR_MASK);
    }else{
        pte_array_base = (pte_t*) xos_get_free_page(0,0);
        printk(PT_DEBUG,"%s:%d,pte_array_phy_base=%lx\n\r",__FUNCTION__,__LINE__,V2P(pte_array_base));
        memset(pte_array_base, 0, PTE_ENTRY_SIZE);

        *pmd = LINEAR_V2P_UL(pte_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;
    }
    
    pte =  &pte_array_base[PTE_IDX(start)];     /*pte_array_base 三级页表基地址，偏移找到具体三级页表项*/
    return pte;
}
#if LEVEL_4
int xos_4level_maps (pgd_t *pgdir, void *va, uint size, uint pa, uint64 ap)
{
    char *a, *last;
    pgd_t *pgd;
    pud_t *pud_array_base;
    pud_t *pud;
    pmd_t *pmd_array_base;
    pmd_t *pmd;
    pte_t *pte_array_base;
    pte_t *pte;
    int step_val =0;
    
    a = (char*) align_down(va, PTE_ENTRY_SIZE);
    last = (char*) align_down((uint64)va + size - 1, PTE_ENTRY_SIZE);
    /*
        for 循环每次va pa 都移动4k 步长，
    */
    
    for(;step_val < size ; step_val += PTE_ENTRY_SIZE){
        pgd = &pgdir[PGD_IDX((uint64)a)];  /*开始本来想直接使用*pgdir[PGD_IDX(uint64)va]&PG_4k_ADDR_MASK*/
        
        if(*pgd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){ /*判断一级页表页表项项是否有效*/
            pud_array_base = (pmd_t*) P2V((*pgd) & PG_4k_ADDR_MASK);
        }else{
            /*
                申请页表空间，并将页表基地址放入pmd 对应的页表项中
            */
            pud_array_base = (pmd_t*)xos_get_free_page(0,0);
            memset(pud_array_base, 0, PTE_ENTRY_SIZE);
            /*
                二级某个页表创建完毕，并且将页表基地址加入到一级页表(我们也成为页目录)某个目录项中

            */
            *pgd = LINEAR_V2P_UL(pud_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;  
        }
        /*
            二级页表项目
        */
        pud = pud_array_base[PUD_IDX(a)];
        if(*pud & (PT_ENTRY_TABLE | PT_ENTRY_VALID)){ /*判断二级页表页表项项是否有效*/
            pmd_array_base = (pmd_t*) P2V((*pud) & PG_4k_ADDR_MASK);
        }else{
            /*
                申请页表空间，并将页表基地址放入pmd 对应的页表项中
            */
            pmd_array_base = (pmd_t*)xos_get_free_page(0,0);
            memset(pmd_array_base, 0, PTE_ENTRY_SIZE);
            /*
                二级某个页表创建完毕，并且将页表基地址加入到一级页表(我们也成为页目录)某个目录项中

            */
            *pud = LINEAR_V2P_UL(pmd_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;  
        }
        /*
            三级页表空间在上面已经申请完毕，下面开始初始化二级页表pmd
        */
        pmd = &pmd_array_base[PMD_IDX(a)]; /*根据虚拟地址找到对应的pmd 项地址*/
        if (*pmd & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) /*判断三级页表页表项是否有效*/
        {
            pte_array_base = (pte_t*) P2V((*pmd) & PG_4k_ADDR_MASK);
        }else{
            
            pte_array_base = (pmd_t*)xos_get_free_page(0,0);
            memset(pte_array_base, 0, PTE_ENTRY_SIZE);

            *pmd = LINEAR_V2P_UL(pte_array_base) | PT_ENTRY_TABLE | PT_ENTRY_VALID;
        }
        
        pte =  &pte_array_base[PTE_IDX(a)];     /*pte_array_base 三级页表基地址，偏移找到具体三级页表项*/

        /*
            配置相应的四级页表项
        */
        *pte = pa | ACCESS_FLAG | SH_IN_SH | ap | NON_SECURE_PA | PT_ATTRINDX(MT_NORMAL) | PT_ENTRY_PAGE | PT_ENTRY_VALID;

        if (a == last) {
            break;
        }

        a += PTE_ENTRY_SIZE;
        pa += PTE_ENTRY_SIZE;
    }

    return 0;

}
#endif
