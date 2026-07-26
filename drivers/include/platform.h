#ifndef __PLATFORM_H__
#define __PLATFORM_H__

struct platform_device {
    const char  *name;
    int id;
//    bool    id_auto;
    struct device_desc	dev;
    u32    num_resources;
    struct resource	*resource;
    int irq;
};

#define RESOURCE_MEM    1
#define RESOURCE_IO     2
#define RESOURCE_IRQ    3

struct of_device_id {
    char	name[32];
    char	type[32];
    char	compatible[128];
    const void *data;
};


struct platform_driver{
    const char  *name;
    struct driver_desc driver;
    int (*platform_probe)(struct platform_device *device);
    void (*platform_remove)(struct platform_device *device);
    void (*platform_shutdown)(struct platform_device *device);
};

struct resource {
    unsigned long  start;
    unsigned long  end;
    unsigned long  type;
};

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))


#endif
