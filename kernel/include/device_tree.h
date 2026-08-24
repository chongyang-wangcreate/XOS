#ifndef __XOS_DEVICE_TREE_H__
#define __XOS_DEVICE_TREE_H__

#include "types.h"

#define XOS_DTB_MAX_NODES  64
#define XOS_DTB_MAX_DEPTH  16
#define XOS_DTB_MAX_REGS   4
#define XOS_DTB_MAX_IRQS   8
#define XOS_DTB_IRQ_CELLS  4

#define FDT_DEFAULT_ADDR_CELLS 2
#define FDT_DEFAULT_SIZE_CELLS 2
#define FDT_DEFAULT_IRQ_CELLS  3



#define FDT_MAGIC	0xd00dfeed	
#define FDT_TAGSIZE	sizeof(u32)

#define FDT_BEGIN_NODE	0x1	
#define FDT_END_NODE	0x2	
#define FDT_PROP	    0x3	
#define FDT_NOP		    0x4	
#define FDT_END		    0x9
typedef struct xos_dtb_reg{
    uint64 start;
    uint64 size;
}xos_dbt_reg_t;

typedef struct xos_dtb_irq{
    uint32 cells[XOS_DTB_IRQ_CELLS];
    uint32 nr_cells;
    uint32 irq;

}xos_dtb_irq_t;


typedef struct xos_dtb_desc{

    uint64 load_phys;
    uint32 total_size;
    uint32 version;
    uint32 last_comp_version;
    uint32 address_cells;
    uint32 size_cells;
    uint64 mem_start;
    uint64 mem_size;
    char   model[64];
    char   bootrags[128];
    char   source[16];
    int    valid;
}xos_dtb_desc_t;

typedef struct xos_dtb_node{
    char name[64];
    char path[128];
    char compatible[128];
    uint32 compatible_len;
    char device_type[32];
    uint32 address_cells;
    uint32 size_cells;
    uint32 interrupts_cells;
    uint32 phandle;
    uint32 interrupt_parent;
    uint32 interrupt_controller;
    xos_dbt_reg_t regs[XOS_DTB_MAX_REGS];
    uint32 nr_regs;
    xos_dtb_irq_t irqs[XOS_DTB_MAX_IRQS];
    uint32 nr_irqs;
    int    level;
    int    enabled;

}xos_dtb_node_t;

typedef struct fd_header{
    uint32 magic;
    uint32 totalsize;
    uint32 off_dt_struct;
    uint32 off_dt_string;
    uint32 off_mem_rsvmap;
    uint32 version;
    uint32 last_comp_version;
    uint32 boot_cpuid_phys;
    uint32 size_dt_string;
    uint32 size_dt_struct;

}fdt_header_t;

extern const xos_dtb_desc_t *xos_get_dtb_desc();

typedef struct xos_dtb_ctx{
    const fdt_header_t *fdt_header;
    const char *blob_end;
    const uint32 *cur;
    const uint32 *struct_end;
    int   level;
    int   node_type;
    int   node_stack[XOS_DTB_MAX_DEPTH];
    char   path_stack[XOS_DTB_MAX_DEPTH][128];
    int   addr_cells_stack[XOS_DTB_MAX_DEPTH];
    int   size_cells_stack[XOS_DTB_MAX_DEPTH];
    int   irq_cells_stack[XOS_DTB_MAX_DEPTH];
    int   irq_parent_stack[XOS_DTB_MAX_DEPTH];

}xos_dtb_ctx_t;

extern  xos_dtb_desc_t *xos_dtb_get_info();
extern void xos_dtb_set_boot_phys(uint64 phys);
extern int xos_dtb_init(void);
extern int xos_parse_dtb(void);
extern xos_dtb_node_t *xos_get_node_by_compatible(const char *compatible);
extern xos_dtb_node_t *xos_get_node_by_phandle(uint32 phandle);
int xos_dtb_node_is_compatible(const xos_dtb_node_t *node,const char *compatible);
int xos_dtb_get_irq(const xos_dtb_node_t *node,uint32 index,uint32 *irq);
uint64 xos_dtb_detect_phys(void);
#endif
