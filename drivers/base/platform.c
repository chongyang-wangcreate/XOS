#include "types.h"
#include "list.h"
#include "string.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_dev.h"
#include "xos_driver.h"
#include "platform.h"
#include "xos_bus.h"
#include "uart.h"
#include "device_tree.h"

static struct xos_bus_desc platform_bus;
dlist_t early_platform_driver_list;
static int platform_device_registered = 0;


#define to_platform_driver(d)	list_entry(d, struct platform_driver, driver)
#define to_platform_device(d)	list_entry(d, struct platform_device, dev)

static struct resource uart0_resource[] = {
    {
        .start = UART0,
        .end   = UART0 + 0xfff,
        .type = RESOURCE_MEM,
    },
    {
        .start = 33,
        .end   = 33,
        .type = RESOURCE_IRQ,
    }
};

static struct platform_device  uart0_device = {
    .name = "uart0",
    .id = 0,
    .num_resources = ARRAY_SIZE(uart0_resource),
    .resource = uart0_resource,
    .irq = 33,

};

static struct platform_device *platform_driver_list[] = {
    &uart0_device,
};

static const char *get_platform_driver_name(struct platform_driver *pdrv)
{
    if(pdrv->name){
        return pdrv->name;
    }
    return pdrv->driver.driver_name;
}

static int platform_of_match(struct platform_device *pdev,struct platform_driver *pdrv)
{
    const struct of_device_id *match;
    if(pdev == NULL || pdrv == NULL || pdev->of_node == NULL || 
       pdrv->driver.of_match_table == NULL){
       return -1;
    }
    for(match = pdrv->driver.of_match_table;match->compatible[0] != '\0'; match++){
        if(xos_dtb_node_is_compatible(pdev->of_node,match->compatible)){
            return 0;
        }
    }
    return -1;
}

static void platform_device_populate_resources(struct platform_device *pdev,const char *compatible)
{
    xos_dtb_node_t *node;
    uint32 irq;
    if(pdev == NULL || compatible == NULL){
        return ;
    }
    node = xos_dtb_find_by_compatible(compatible);
    if(node == NULL){
        return ;
    }
    pdev->of_node = node;
    if(pdev->resource != NULL && pdev->num_resources > 0 && node->nr_regs > 0){
        pdev->resource[0].start = node->regs[0].start;
        pdev->resource[0].end   = node->regs[0].start + node->regs[0].size - 1;
        pdev->resource[0].type  = RESOURCE_MEM;
    }
    if(xos_dtb_get_irq(node,0,&irq) == 0){
        pdev->irq = irq;
        if(pdev->resource != NULL && pdev->num_resources > 1){
            pdev->resource[1].start = irq;
            pdev->resource[1].end   = irq;
            pdev->resource[1].type  = RESOURCE_IRQ;
        }
    }
}
int platform_device_register(struct platform_device *pdev)
{
    if(!pdev || !pdev->name){
        return -1;
    }
    pdev->dev.device_name = (char*)pdev->name;
    pdev->dev.bus = &platform_bus;
    return dev_insert(&pdev->dev);

}

void platform_device_unregister(struct platform_device *pdev)
{   
    if(!pdev || !pdev->name){
        return ;
    }
    dev_unregister(&pdev->dev);
}

struct resource *platform_get_resource(struct platform_device *pdev,
                                        unsigned long type,
                                       unsigned int num)
{
    unsigned int i;
    unsigned int match = 0;
    if(!pdev || !pdev->resource){
        return NULL;
    }
    for(i = 0; i < pdev->num_resources ;i++){
        if((pdev->resource[i].type & type) == 0){
            continue;
        }
        if(match == num){
            return &pdev->resource[i];
        }
    }
    return NULL;
}

static void platform_register_devices_list(void)
{
    unsigned int i;
    if(platform_device_registered){
        return;
    }
    platform_device_populate_resources(&uart0_device,"arm,pl011");
    for(i = 0; i < ARRAY_SIZE(platform_driver_list);i++){
        platform_device_register(platform_driver_list[i]);
    }
    platform_device_registered = 1;
}
/*
    驱动和设备都挂到bus 总线上来管理，通过某个总线让设备和设备驱动建立联系
    match maker 成功之后，驱动做自己主要工作，开始申请设备号，初始化fops
    为后续文件和设备建立关联做基本的初始化

    match make 主要工作是初始化硬件，配置寄存器

    暂时先借鉴Linux 内核bus总线思想，代码要独立开发

    cdev 是字符设备模型的核心结构，它与硬件设备无关，可以作为独立的字符设备抽象存在。
*/
int platform_match(struct device_desc *dev, struct driver_desc *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);
    const char *drv_name;
    if(!dev ||!drv){
        return -1;
    }
    drv_name = get_platform_driver_name(pdrv);
    if(!pdev->name || !drv_name){
        return -1;
    }
    /*
        暂时先使用名称匹配
    */
   if(platform_of_match(pdev,pdrv) == 0){
      return 0;
   }
    return strcmp(pdev->name,drv_name) == 0 ? 0:-1;
}

int platform_probe(struct device_desc *dev)
{
    struct platform_driver *plat_drv = to_platform_driver(dev->driver);
    struct platform_device *plat_dev = to_platform_device(dev);
    if(!plat_drv|| !plat_drv->platform_probe){
        return -1;
    }
    plat_drv->platform_probe(plat_dev);
    return 0;
}

int platform_remove(struct device_desc *dev)
{
    struct platform_driver *plat_drv = to_platform_driver(dev->driver);
    struct platform_device *plat_dev = to_platform_device(dev);

    if(!plat_drv || !plat_drv->platform_remove){
        return -1;
    }
    plat_drv->platform_remove(plat_dev);
    return 0;
}


static struct xos_bus_desc platform_bus = {
    .bus_name = "platform_bus",
    .bus_match = platform_match,
    .bus_probe = platform_probe,
    .bus_remove = platform_remove,
};

/*
    int __platform_driver_register(struct platform_driver *drv,
        struct module *owner)
    {
        drv->driver.owner = owner;
        drv->driver.bus = &platform_bus_type;
        drv->driver.probe = platform_drv_probe;
        drv->driver.remove = platform_drv_remove;
        drv->driver.shutdown = platform_drv_shutdown;

        return driver_register(&drv->driver);
    }

*/
int platform_driver_register(struct platform_driver *drv)
{
    drv->driver.bus = &platform_bus;

    if (drv->platform_probe)
        drv->driver.drv_probe = platform_probe;
    if (drv->platform_remove)
        drv->driver.drv_probe = platform_remove;

    return driver_register(&drv->driver);
}

void platform_driver_unregister(struct platform_driver *drv)
{
    driver_unregister(&drv->driver);
}

struct xos_bus_desc *get_platform_bus()
{
    return &platform_bus;
}

void platform_bus_init(struct xos_bus_desc *platform_bus)
{
    platform_bus->bus_name = "platform_bus";
    platform_bus->bus_match = platform_match;
    platform_bus->bus_probe = platform_probe;
    platform_bus->bus_remove = platform_remove;
    xos_bus_register(platform_bus);
    platform_register_devices_list();
    
}

