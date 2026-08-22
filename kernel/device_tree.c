#include "types.h"
#include "string.h"
#include "bit_map.h"
#include "list.h"
#include "printk.h"
#include "mem_layout.h"
#include "device_tree.h"
#include "spinlock.h"
#include "xos_mutex.h"
//#include "xos_kern_def.h"

static int handle_begin_node(xos_dtb_ctx_t *ctx);
static void handle_end_node(xos_dtb_ctx_t *ctx);
static int handle_prop(xos_dtb_ctx_t *ctx);
static int add_dtb_node(xos_dtb_ctx_t *ctx,const char *node_name);
static void classify_level1_node(xos_dtb_ctx_t *ctx,const char *node_name);
static int parse_common_prop(xos_dtb_ctx_t *ctx,const char *prop_name,
                           const uint32 *data,uint32 len);
static int parse_current_node_prop(xos_dtb_ctx_t *ctx ,const char *prop_name,
                                 const uint32 *data,uint32 len);         
static void parse_root_prop(xos_dtb_desc_t *info,const char *name,const uint32 *data,uint32 len);                   

static void parse_chosen_prop(xos_dtb_desc_t *info,const char *name,const uint32 *data,uint32 len);
static void parse_memory_reg(xos_dtb_desc_t *info,const uint32 *data,uint32 len);
static int str_eq(const char *a,const char *b);
static void copy_fdt_string(char *dst,uint32 dst_len,const uint32 *data,uint32 len);
static void copy_fdt_bytes(char *dst,uint32 dst_len,const uint32 *data,uint32 len,uint32 *out_len);
static uint32 get_node_interrupt_cells(uint32 phandle,uint32 fallback);
static void parse_node_prop(xos_dtb_node_t *node,const char *name,
                            const uint32 *data, uint32 len,
                            uint32 parent_addr_cells,uint32 parent_size_cells);

static void parse_node_interrupts(xos_dtb_node_t *node,
                                const uint32 *data ,uint32 len,
                                uint32 interrupt_cells);  

static void parse_node_interrupts_extended(xos_dtb_node_t *node,
                                const uint32 *data ,uint32 len);  
enum{
    NODE_OTHER = 0,
    NODE_ROOT,
    NODE_CHOSEN,
    NODE_MEMORY,
};

static uint64 g_boot_dtb_phys;
static xos_dtb_desc_t   g_dtb_info;
static xos_dtb_node_t   g_dtb_nodes[XOS_DTB_MAX_NODES];
static int g_dtb_node_count;

static uint32 fdt32_to_cpu(uint32 v)
{
    return ((v & 0x000000ffU) << 24)|
           ((v & 0x0000ff00U) << 8) |
           ((v & 0x00ff0000U) >> 8) |
           ((v & 0xff000000U) >> 24);
}
static uint64 fdt64_to_cpu(const uint32 *v)
{
    return ((uint64)fdt32_to_cpu(v[0]) << 32) | fdt32_to_cpu(v[1]);
}

static uint32 align4(uint32 v)
{
    return (v + 3U) & ~3U;
}

int fdt_header_magic_ok(uint64 phys)
{
    const fdt_header_t *hdr = (const fdt_header_t*)LINEAR_P2V(phys);
    if(phys < PHYS_MEM_START || phys > PHYS_MEM_END){
        return 0;
    }
    return fdt32_to_cpu(hdr->magic) == FDT_MAGIC;
}

static void init_parse_ctx(xos_dtb_ctx_t *ctx)
{
    int i;
    memset(ctx,0,sizeof(*ctx));
    ctx->level = -1;
    ctx->node_type = NODE_OTHER;
    for(i = 0;i < XOS_DTB_MAX_DEPTH;i++){
        ctx->node_stack[i] = -1;
        ctx->addr_cells_stack[i] = FDT_DEFAULT_ADDR_CELLS;
        ctx->size_cells_stack[i] = FDT_DEFAULT_SIZE_CELLS;
        ctx->irq_cells_stack[i] =  FDT_DEFAULT_IRQ_CELLS;
        ctx->irq_parent_stack[i] = 0;
    }
}

static int xos_parse_blob(xos_dtb_ctx_t *ctx)
{
    const fdt_header_t *hdr;
    uint32 off_dt_struct;
    uint32 size_dt_struct;
    hdr  = (const fdt_header_t*)LINEAR_P2V(g_boot_dtb_phys);
    if(fdt32_to_cpu(hdr->magic) != FDT_MAGIC){
        return -1;
    }
    g_dtb_info.total_size = fdt32_to_cpu(hdr->totalsize);
    if(g_dtb_info.total_size < sizeof(fdt_header_t)||
       g_dtb_info.total_size > (16*1024*1024UL)){
       return -1;
    }
    off_dt_struct = fdt32_to_cpu(hdr->off_dt_struct);
    size_dt_struct = fdt32_to_cpu(hdr->size_dt_struct);
    ctx->fdt_header = hdr;
    g_dtb_info.version = fdt32_to_cpu(ctx->fdt_header->version);
    g_dtb_info.last_comp_version = fdt32_to_cpu(ctx->fdt_header->last_comp_version);
    g_dtb_info.address_cells = FDT_DEFAULT_ADDR_CELLS;
    g_dtb_info.size_cells = FDT_DEFAULT_SIZE_CELLS;
    
    ctx->blob_end = ((const char*)hdr + g_dtb_info.total_size);
    ctx->cur = (uint32*)((const char*)hdr + off_dt_struct);
    ctx->struct_end = (const uint32*)((const char *)ctx->cur + size_dt_struct);
    return 0;
}
static int init_dtb_related_bufs(void)
{
    memset(&g_dtb_info,0,sizeof(g_dtb_info));
    memset(g_dtb_nodes,0,sizeof(g_dtb_nodes));
    g_dtb_node_count = 0;
    g_dtb_info.load_phys = g_boot_dtb_phys;
    g_dtb_info.address_cells = FDT_DEFAULT_ADDR_CELLS;
    g_dtb_info.size_cells    = FDT_DEFAULT_SIZE_CELLS;
    return 0;
}

 xos_dtb_desc_t *xos_dtb_get_info()
{
    return &g_dtb_info;
}
int xos_parse_dtb(void)
{
    xos_dtb_ctx_t dtb_ctx;
    init_dtb_related_bufs();
    init_parse_ctx(&dtb_ctx);
    if(xos_parse_blob(&dtb_ctx) < 0){
        return -1;
    }
    while(dtb_ctx.cur < dtb_ctx.struct_end){
        uint32 token;
        if((char*)dtb_ctx.cur + sizeof(uint32) > dtb_ctx.blob_end){
            return -1;
        }
        token = fdt32_to_cpu(*dtb_ctx.cur++);
        if(token == FDT_NOP){
            continue;
        }
        if(token == FDT_END){
            break;
        }
        if(token ==  FDT_BEGIN_NODE){
            if(handle_begin_node(&dtb_ctx) < 0){
                return -1;
            }
            continue;
        }
        if(token == FDT_END_NODE){
            handle_end_node(&dtb_ctx);
            continue;
        }
        if(token == FDT_PROP){
            printk(PT_ERROR,"%s:%d\n\r",__FUNCTION__,__LINE__);
            if(handle_prop(&dtb_ctx) < 0){
                return -1;
            }
            continue;
        }
    }
    g_dtb_info.valid = 1;
    return 0;
}
static int compatible_is_match(const char *list,uint32 list_len,const char *compatible)
{
    uint32 offset = 0;
    uint32 i;
    if(list == NULL || compatible == NULL){
        return 0;
    }

    while(offset < list_len && list[offset] != '\0'){
        i = 0;
        while((offset + i) < list_len &&
             list[offset + i] != '\0' &&
             list[offset + i] != ';'&&
             compatible[i] != '\0'&&
             list[offset + i] == compatible[i]){
                i++;
             }
        if((offset + i) < list_len && (list[offset + i] == '\0') && compatible[i] == '\0'){
            return 1;
        }
        while(offset < list_len && list[offset] != '\0'){
            offset++;
        }
        offset++;

    }
    return 0;
}

xos_dtb_node_t *xos_dtb_find_node_byte_phandle(uint32 phandle)
{
    int i;
    if(phandle == 0){
        return NULL;
    }
    for(i = 0; i< g_dtb_node_count ;i++){
        if(g_dtb_nodes[i].phandle == phandle){
            return &g_dtb_nodes[i];
        }
    }
    return NULL;
}

int xos_dtb_node_is_compatible(const xos_dtb_node_t *node, const char *compatible)
{
    if(node == NULL || compatible == NULL){
        return 0;
    }
    return compatible_is_match(node->compatible,node->compatible_len,compatible);
}

xos_dtb_node_t *xos_dtb_find_compatible(const char *compatible)
{
    int i;
    if(compatible == NULL){
        return NULL;
    }
    for(i = 0; i < g_dtb_node_count ;i++){
        if(xos_dtb_node_is_compatible(&g_dtb_nodes[i],compatible)){
            return &g_dtb_nodes[i];
        }
    }
    return 0;
}

int xos_dtb_get_irq(const xos_dtb_node_t *node, uint32 index ,uint32 *irq)
{
    if(node == NULL || irq == NULL || index >= node->nr_irqs){
        return -1;
    }
    *irq = node->irqs[index].irq;
    return 0;
}

static uint64 bounded_len(const char *s ,const char *limit)
{
    const char *p_cur = s;
    while(p_cur < limit && *p_cur != '\0'){
        p_cur++;
    }
    if(p_cur >= limit){
        return (uint64)(limit - s);
    }
    return (uint64)(p_cur - s);
}

static int handle_begin_node(xos_dtb_ctx_t *ctx)
{
    const char *name = (const char*)ctx->cur;
    unsigned int name_len = bounded_len(name,ctx->blob_end);
    char node_name[64];
    unsigned int copy_len;

    if(name + name_len > ctx->blob_end){
        return -1;
    }
    copy_len = (name_len < sizeof(node_name) - 1)?
      name_len : (sizeof(node_name) - 1);
    memset(node_name,0,sizeof(node_name));
    memcpy(node_name,name,copy_len);
    node_name[copy_len] = '\0';
    printk(PT_ERROR,"node_name=%s:%d\n\r",node_name,__LINE__);
    ctx->cur = (const uint32*)(const char*)ctx->cur + align4(name_len + 1); //next
    ctx->level++;
    if(ctx->level >= XOS_DTB_MAX_DEPTH){
        return -1;
    }
    if(ctx->level == 0){
        ctx->node_type = NODE_ROOT;
        ctx->path_stack[ctx->level][0] = '/';
        ctx->path_stack[ctx->level][1] = '\0';
        return 0;
    }
    ctx->addr_cells_stack[ctx->level] = ctx->addr_cells_stack[ctx->level - 1];
    ctx->size_cells_stack[ctx->level] = ctx->size_cells_stack[ctx->level - 1];
    ctx->irq_cells_stack[ctx->level]  = ctx->irq_cells_stack[ctx->level - 1];
    if(ctx->level == 1){
        classify_level1_node(ctx,node_name);
    }else{
        ctx->node_type = NODE_OTHER;
    }
    return add_dtb_node(ctx,node_name);
}   

static void handle_end_node(xos_dtb_ctx_t *ctx)
{
    if(ctx->level == 1){
        ctx->node_type = NODE_OTHER;
    }
    if(ctx->level >= 0 && ctx->level < XOS_DTB_MAX_DEPTH){
        ctx->node_stack[ctx->level] = -1;
    }
    ctx->level--;
}

static const char *fdt_get_string(const fdt_header_t *hdr,uint32 off)
{
    return (char*)hdr + fdt32_to_cpu(hdr->off_dt_string) + off;
}

static int handle_prop(xos_dtb_ctx_t *ctx)
{
    uint32 len;
    uint32 nameoff;
    const char *prop_name;
    const uint32 *data;
    if((char*)ctx->cur + 2*sizeof(uint32) > ctx->blob_end){
        return -1;
    }
    len = fdt32_to_cpu(*ctx->cur++);
    nameoff = fdt32_to_cpu(*ctx->cur++);
    prop_name = fdt_get_string(ctx->fdt_header,nameoff);
    printk(PT_ERROR,"prop_name=%s\n\r",prop_name);
    data = ctx->cur;
    if((char*)data + len > ctx->blob_end){
        return -1;
    }
    parse_common_prop(ctx,prop_name,data,len);
    parse_current_node_prop(ctx,prop_name,data,len);
    ctx->cur = (const uint32*)((const char*)ctx->cur + align4(len));
    return 0;
}

static int parse_common_prop(xos_dtb_ctx_t *ctx,const char *prop_name,
                            const uint32 *data,uint32 len)
{
    if(ctx->level == 0){
        parse_root_prop(&g_dtb_info,prop_name,data,len);
        ctx->addr_cells_stack[0] = g_dtb_info.address_cells;
        ctx->size_cells_stack[0] = g_dtb_info.size_cells;
        if(str_eq(prop_name,"interrupt-parent") && len >= sizeof(uint32)){
            ctx->irq_parent_stack[0] = fdt32_to_cpu(data[0]);
        }
        return 0;
    }
    if(ctx->level == 1){
        printk(PT_ERROR,"%s:ctx->node_type=%d\n\r",__FUNCTION__,ctx->node_type);
        if(ctx->node_type == NODE_CHOSEN){
            parse_chosen_prop(&g_dtb_info,prop_name,data,len);
        }else if(ctx->node_type == NODE_MEMORY && !strcmp(prop_name,"reg")){
            printk(PT_ERROR,"%s:ctx->node_type=%d\n\r",__FUNCTION__,ctx->node_type);
            parse_memory_reg(&g_dtb_info,data,len);
        }
    }
    return 0;

}

/*//static void parse_u32_prop(xos_dtb_desc_t *info,const char *name,const uint32 *data,uint32 len)
{
    if(len < sizeof(uint32)){
        return ;
    }
    if(str_eq(name,"#address-cells")){
        info->address_cells = fdt32_to_cpu(data[0]);
    }else if (str_eq(name,"#size-cells")){
        info->size_cells = fdt32_to_cpu(data[0]);
    }
}*/
static void parse_root_prop(xos_dtb_desc_t *info,const char *name,const uint32 *data,uint32 len)
{
    if(str_eq(name,"#address-cells")){
        info->address_cells = fdt32_to_cpu(data[0]);
    }else if (str_eq(name,"#size-cells")){
        info->size_cells = fdt32_to_cpu(data[0]);
    }else if(str_eq(name,"model")){
        copy_fdt_string(info->model,sizeof(info->model),data,len);
    }
}

static void parse_chosen_prop(xos_dtb_desc_t *info,const char *name,const uint32 *data,uint32 len)
{
    if(!strcmp(name,"bootargs")){
        copy_fdt_string(info->bootrags,sizeof(info->bootrags),data,len);
    }
}

static void parse_memory_reg(xos_dtb_desc_t *info,const uint32 *data,uint32 len)
{
    uint32 addr_cells = info->address_cells ? info->address_cells:2;
    uint32 size_cells = info->size_cells ? info->size_cells :2;
    if(addr_cells == 2){
        info->mem_start = fdt64_to_cpu(data);
        data += 2;
    }else{
        info->mem_start = fdt32_to_cpu(data[0]);
        data += 1;
    }
    if(size_cells == 2){
        info->mem_size = fdt64_to_cpu(data);
    }else{
        info->mem_size = fdt32_to_cpu(data[0]);
    }
}

static int parse_current_node_prop(xos_dtb_ctx_t *ctx ,const char *prop_name,
                                 const uint32 *data,uint32 len)
{
    xos_dtb_node_t *node;
    uint32 parent_addr_cells;
    uint32 parent_size_cells;
    uint32 parent_irq_cells;

    if(ctx->level <= 0 || ctx->level >= XOS_DTB_MAX_DEPTH||
       ctx->node_stack[ctx->level] < 0){
        return 0;
    }
    node = &g_dtb_nodes[ctx->node_stack[ctx->level]];
    parent_addr_cells = ctx->addr_cells_stack[ctx->level - 1];
    parent_size_cells = ctx->size_cells_stack[ctx->level - 1];
    parent_irq_cells  = get_node_interrupt_cells(node->interrupt_parent,
                        ctx->irq_cells_stack[ctx->level - 1]);
    parse_node_prop(node,prop_name,data,len,parent_addr_cells,parent_size_cells);
    ctx->addr_cells_stack[ctx->level] = node->address_cells;
    ctx->size_cells_stack[ctx->level] = node->size_cells;
    ctx->irq_cells_stack[ctx->level]  = node->interrupts_cells;
    ctx->irq_parent_stack[ctx->level] = node->interrupt_parent;
    if(str_eq(prop_name,"interrupts")){
        parse_node_interrupts(node,data,len,parent_irq_cells);
    }else if(str_eq(prop_name,"interrupts-extended")){
        parse_node_interrupts_extended(node,data,len);
    }
    return 0;
}          

static int str_eq(const char *a,const char *b)
{
    while(*a && *b && *a == *b){
        a++;
        b++;
    }
    return *a == *b;
}

static void copy_fdt_string(char *dst,uint32 dst_len,const uint32 *data,uint32 len)
{
    uint32 copy_len;
    if(dst == NULL || dst_len == 0){
        return ;
    }
    memset(dst ,0 ,dst_len);
    if(data == NULL || len == 0){
        return ;
    }
    copy_len = len < (dst_len - 1) ? len:(dst_len -1);
    memcpy(dst,data,copy_len);
    dst[dst_len -1] = '\0';
}

static void copy_fdt_bytes(char *dst,uint32 dst_len,const uint32 *data,uint32 len,uint32 *out_len)
{
    uint32 copy_len;
    if(out_len != 0){
        *out_len = 0;
    }
    if(dst == NULL || dst_len == 0){
        return ;
    }
    memset(dst,0,dst_len);
    if(data == NULL || len == 0){
        return ;
    }
    copy_len = len < dst_len ? len:dst_len;
    memcpy(dst,data,copy_len);
    if(out_len != 0){
        *out_len = copy_len;
    }
}


static uint32 get_node_interrupt_cells(uint32 phandle,uint32 fallback)
{
    xos_dtb_node_t *parent;
    parent = xos_dtb_find_node_byte_phandle(phandle);
    if(parent != NULL && parent->interrupts_cells != 0){
        return parent->interrupts_cells;
    }
    return fallback;
}
/*static int str_starts_value(const char *s,const char *prefix)
{
    while(*prefix){
        if(*s != *prefix){
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}*/

static int compare_string(const char *s,const char *prefix)
{
    while(*prefix){
        if(*s != *prefix){
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

static uint64 get_fdt_cells(const uint32 *data ,uint32 cells)
{
    uint64 value = 0;
    uint32 i;
    for(i = 0; i < cells ;i++){
        value = (value << 32) | fdt32_to_cpu(data[i]);
    }
    return value;
}

static void parse_node_reg(xos_dtb_node_t *node,
                           const uint32 *data ,uint32 len,
                           uint32 parent_addr_cells,uint32 parent_size_cells)
{
    uint32 tuple_cells = parent_addr_cells + parent_size_cells;
    uint32 total_cells = len / sizeof(uint32);
    uint32 index= 0;
    if(node == 0 || tuple_cells ==0){
        return;
    }
    while(total_cells >= tuple_cells && node->nr_regs < XOS_DTB_MAX_REGS){
        node->regs[node->nr_regs].start = get_fdt_cells(data+index,parent_addr_cells);
        index += parent_addr_cells;
        node->regs[node->nr_regs].size = get_fdt_cells(data+index,parent_size_cells);
        index += parent_size_cells;
        node->nr_regs++;
        total_cells -= tuple_cells;
    }
}  

static uint32 get_irq(const uint32 *cells,uint32 nr_cells)
{
    uint32 type;
    uint32 hwirq;
    if(nr_cells < 2){
        return 0;
    }
    type = fdt32_to_cpu(cells[0]);
    hwirq = fdt32_to_cpu(cells[1]);
    if(nr_cells >= 3 && type == 0){
        return hwirq + 32;
    }
    if(nr_cells >= 3 && type ==1){
        return hwirq + 16;
    }
    return hwirq;
}
static void parse_node_interrupts(xos_dtb_node_t *node,
                                const uint32 *data ,uint32 len,
                                uint32 interrupt_cells)
{
    uint32 total_cells = len / sizeof(uint32);
    uint32 index= 0;
    uint32 raw_cells = interrupt_cells;
    uint32 saved_cells = interrupt_cells;
    uint32 i = 0;
    if(node == 0 || interrupt_cells ==0){
        return;
    }
    if(saved_cells > XOS_DTB_IRQ_CELLS){
        saved_cells = XOS_DTB_IRQ_CELLS;
    }
    while(total_cells >= raw_cells && node->nr_irqs < XOS_DTB_MAX_IRQS){
        xos_dtb_irq_t *irq = &node->irqs[node->nr_irqs];
        memset(irq,0,sizeof(*irq));
        irq->nr_cells = saved_cells;
        for(i = 0; i < saved_cells;i++){
            irq->cells[i] = fdt32_to_cpu(data[index + i]);
        }
        irq->irq = get_irq(data + index , raw_cells);
        index += raw_cells;
        total_cells -= raw_cells;
        node->nr_irqs++;
    }
}

static void parse_node_interrupts_extended(xos_dtb_node_t *node,
                                        const uint32 *data,uint32 len)
{
    uint32 total_cells = len / sizeof(uint32);
    uint32 index = 0;
    if(node == NULL){
        return;
    }
    while(total_cells > 1 && node->nr_irqs < XOS_DTB_MAX_IRQS){
        uint32 phandle = fdt32_to_cpu(data[index]);
        uint32 raw_cells = get_node_interrupt_cells(phandle,FDT_DEFAULT_IRQ_CELLS);
        uint32 saved_cells = raw_cells;
        uint32 i;
        xos_dtb_irq_t *irq;

        index++;
        total_cells--;
        if(saved_cells > XOS_DTB_IRQ_CELLS){
            saved_cells = XOS_DTB_IRQ_CELLS;
        }
        irq = &node->irqs[node->nr_irqs];
        memset(irq,0,sizeof(*irq));
        irq->nr_cells = saved_cells;
        for(i = 0;i < saved_cells ;i++){
            irq->cells[i] = fdt32_to_cpu(data[index + i]);
        }
        irq->irq = get_irq(data + index,raw_cells);
        index += raw_cells;
        total_cells -= raw_cells;
        node->nr_irqs++;
    }

}
static void make_node_path(char *dst ,int dst_len,
                                   const char *parent,const char *name)
{
    uint32 i = 0;
    uint32 j = 0;
    if(dst_len == 0){
        return ;
    }
    if(parent == NULL || parent[0] == '\0'|| str_eq(parent,"/")){
        dst[i++] = '/';

    }else{
        while(i + 1 < dst_len && parent[i] != '\0'){
            dst[i] = parent[i];
            i++;
        }
        if(i + 1 < dst_len && dst[i -1] != '/'){
            dst[i++] = '/';
        }
    }
    while(i + 1 < dst_len && name[j] != '\0'){

        dst[i++] = name[j++];
    }
    dst[i] = '\0';
}
static void parse_node_prop(xos_dtb_node_t *node,const char *name,
                            const uint32 *data ,uint32 len,
                            uint32 parent_addr_cells,uint32 parent_size_cells)
{
    if(node == 0){
        return;
    }
    if(str_eq(name,"compatible")){
        copy_fdt_bytes(node->compatible,sizeof(node->compatible),data,len,&node->compatible_len);
    }else if(str_eq(name,"device_type")){
        copy_fdt_string(node->device_type,sizeof(node->device_type),(const uint32*)data,len);
    }else if(str_eq(name,"status")){
        if(str_eq((const char*)data,"disabled")){
            node->enabled = 0;
        }
    }else if(str_eq(name,"#address-cells") && len >= sizeof(uint32)){
        node->address_cells = fdt32_to_cpu(data[0]);
        printk(PT_ERROR,"node->address_cells=%d\n\r",node->address_cells);
    }else if(str_eq(name,"#size_cells") && len >= sizeof(uint32)){
        node->size_cells = fdt32_to_cpu(data[0]);
    }else if(str_eq(name,"#interrupt-cells") && len >= sizeof(uint32)){
        node->interrupts_cells = fdt32_to_cpu(data[0]);
    }else if(str_eq(name,"reg")){
        parse_node_reg(node,data,len,parent_addr_cells,parent_size_cells);
    }else if((str_eq(name,"phandle") || str_eq(name,"phandle")) && len >= sizeof(uint32)){
        node->phandle = fdt32_to_cpu(data[0]);
    }else if(str_eq(name,"interrupt-parent") && len >= sizeof(uint32)){
        node->interrupt_parent = fdt32_to_cpu(data[0]);
    }else if(str_eq(name,"interrupt-controller")){
        node->interrupt_controller = 1;
    }
}                            

static int add_dtb_node(xos_dtb_ctx_t *ctx,const char *node_name)
{
    int parent = ctx->level - 1;
    xos_dtb_node_t *node;
    ctx->addr_cells_stack[ctx->level] = ctx->addr_cells_stack[parent];
    ctx->size_cells_stack[ctx->level] = ctx->size_cells_stack[parent];
    ctx->irq_cells_stack[ctx->level]  = ctx->irq_cells_stack[parent];
    ctx->irq_parent_stack[ctx->level]  = ctx->irq_parent_stack[parent];

   
    if(g_dtb_node_count >= XOS_DTB_MAX_NODES){
        ctx->node_stack[ctx->level] = -1;
        return 0;
    }
    make_node_path(ctx->path_stack[ctx->level],sizeof(ctx->path_stack[ctx->level]),
                                   ctx->path_stack[parent],node_name);
    node = &g_dtb_nodes[g_dtb_node_count];
    memset(node,0,sizeof(*node));
    strncpy(node->name,node_name,sizeof(node->name) -1);
    strncpy(node->path,(const char*)ctx->path_stack[ctx->level],sizeof(node->path) -1);
    printk(PT_ERROR,"node->name=%d\n\r",node->name);
    printk(PT_ERROR,"node->path=%d\n\r",node->path);
    node->address_cells = ctx->addr_cells_stack[ctx->level];
    node->size_cells = ctx->size_cells_stack[ctx->level];
    node->interrupts_cells = ctx->irq_cells_stack[ctx->level];
    node->interrupt_parent = ctx->irq_parent_stack[ctx->level];
    node->level = ctx->level;
    node->enabled = 1;
    ctx->node_stack[ctx->level] = g_dtb_node_count;
    g_dtb_node_count++;
    return 0;
}

static void classify_level1_node(xos_dtb_ctx_t *ctx,const char *node_name)
{
    if(compare_string(node_name,"chosen")){
        ctx->node_type = NODE_CHOSEN;
    }else if(compare_string(node_name,"memory")){
        ctx->node_type = NODE_MEMORY;      
    }else{
        ctx->node_type = NODE_OTHER;
    }
}

int xos_dtb_init(void)
{
    int ret = 0;
    printk(PT_DEBUG,"dtb init start\n\r");
    if(g_boot_dtb_phys < PHYS_MEM_START || g_boot_dtb_phys > PHYS_MEM_END){
        printk(PT_ERROR,"dtb invalid boot phys=0x%lx\n\r",g_boot_dtb_phys);
        return -1;
    }
    ret = xos_parse_dtb();
    if(ret < 0){
        printk(PT_ERROR,"dtb xos_parse_dtb failed\n\r");
        return -1;
    }
    printk(PT_ERROR,"dtb init done\n\r");
    return 0;

}

void xos_dtb_set_boot_phys(uint64 phys)
{
    if(phys == 0){
        phys = 0x58000000UL;
    }
    g_boot_dtb_phys = phys;
}