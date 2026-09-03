#include "types.h"
#include "string.h"
#include "printk.h"
#include "mem_layout.h"
#include "device_tree.h"
#include "memblock.h"
extern uint8_t _kernel_page_array_start[];
extern uint8_t _kernel_page_array_end[];
#define XOS_MEMBLOCK_ALIGN 0x1000UL

static xos_memblock_info_t g_memblock_info;

static uint64 align_up_u64(uint64 val,uint64 align)
{
    return (val + align -1) & ~(align - 1);
}

static uint64 align_down_u64(uint64 val,uint64 align)
{
    return val & ~(align - 1);
}

xos_memblock_info_t *xos_memblock_get_info(void)
{
    return &g_memblock_info;
}

int xos_memblock_init()
{
    xos_dtb_desc_t *dtb = xos_dtb_get_info();
    uint64 mem_start;
    uint64 mem_end;
    uint64 reserved_start;
    uint64 reserved_end;
    uint64 usable_start;
    uint64 usable_end;
    memset(&g_memblock_info ,0,sizeof(g_memblock_info));
    if(dtb == NULL || !dtb->valid || dtb->mem_size == 0){
        printk(PT_ERROR,"dtb->mem_size=%d\n\r",dtb->mem_size);
        return -1;
    }
    mem_start = dtb->mem_start;
    mem_end   = dtb->mem_start + dtb->mem_size - 1;
    if(mem_start < PHYS_MEM_START){
        mem_start = PHYS_MEM_START;
    }
    if(mem_end > PHYS_MEM_END){
        mem_start = PHYS_MEM_END;
    }
    if(mem_end < mem_start){
        printk(PT_ERROR,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return -1;
    }
    g_memblock_info.memory.base = mem_start;
    g_memblock_info.memory.size = mem_end - mem_start + 1;

    reserved_start = RESERVED_MEM_START;
    reserved_end   = RESERVED_MEM_END;

    if(reserved_start < mem_start){
        reserved_start = mem_start;
    }

    if(reserved_end > mem_end){
        reserved_end = mem_end;
    }
    if(reserved_end >= reserved_start){
        g_memblock_info.reserved.base = reserved_start;
        g_memblock_info.reserved.size = reserved_end - reserved_start + 1;
    }
    usable_start = mem_start;
    if(g_memblock_info.reserved.size != 0 &&
       reserved_start <= mem_start &&
       reserved_end >= mem_start){
       usable_start = reserved_end + 1;
    }
    usable_start = align_up_u64(usable_start,XOS_MEMBLOCK_ALIGN);
    usable_end   = align_down_u64(mem_end + 1,XOS_MEMBLOCK_ALIGN);;
    if(usable_start < usable_end){
        g_memblock_info.usable.base = usable_start;
        g_memblock_info.usable.size = usable_end - usable_start;
    }
    g_memblock_info.vaild = 1;
    return 0;

}