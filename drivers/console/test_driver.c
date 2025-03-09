#include "types.h"
#include "list.h"
#include "string.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_dev.h"
#include "xos_driver.h"
#include "platform.h"
#include "xos_bus.h"

/*
    led_probe 例子来自csdn
*/


static struct resource console_resources[] = {
    {
        .start = 0x56000050, // 假设LED设备的IO端口起始地址
        .end = 0x56000050 + 8 - 1, // IO端口结束地址
        .flags = IORESOURCE_IO, // 资源类型为IO端口
    },
    {
        .start = 5, // 假设LED设备的IRQ中断号
        .end = 5,
        .flags = IORESOURCE_IRQ, // 资源类型为IRQ中断
    },
};
 
// 定义平台设备
static struct platform_device console_device = {
    .name = "myled", // 设备名称，应与驱动中的名称匹配
    .id = -1, // 设备ID，-1表示只有一个这样的设备
    .num_resources = ARRAY_SIZE(console_resources), // 资源数量
    .resource = console_resources, // 资源数组指针
};

int test_console_probe(struct platform_device *device)
{
    
}

void test_console_remove(struct platform_device *device)
{
    
}

static const struct of_device_id test_console_match[] = {
    { .compatible = "console,test_console", },
    { },
};

static struct platform_driver test_console_driver = {
    .platform_probe = test_console_probe,
    .platform_remove = test_console_remove,
    .driver = {
        .driver_name = "test_cosole",
        .of_match_table = test_console_match,
    },
};


static int  test_console_init(void)
{
    return platform_driver_register(&test_console_driver);
}

static void  test_console_exit(void)
{
    platform_driver_unregister(&test_console_driver);
}

