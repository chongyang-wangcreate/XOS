#ifndef __XOS_BUS_H__
#define __XOS_BUS_H__

extern struct xos_bus_desc *get_platform_bus();

extern void platform_bus_init(struct xos_bus_desc *platform_bus);

extern int xos_bus_register(struct xos_bus_desc * bus);
extern int xos_bus_insert_driver(xdriver_t *drv);

extern void xos_bus_init();


#endif
