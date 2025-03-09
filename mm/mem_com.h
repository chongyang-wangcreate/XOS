#ifndef __MEM_COM_H__
#define __MEM_COM_H__

/*
    内存通用头文件
*/
#define	xos_phys_to_pgnum(paddr)	((unsigned long)((paddr) >> PAGE_SHIFT))
#define	xos_pgnum_to_phys(pfn)	((unsigned long)(pfn) << PAGE_SHIFT)


#endif
