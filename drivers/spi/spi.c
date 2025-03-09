#define to_spi_driver(d)	list_entry(d, struct spi_driver, driver)
#define to_spi_device(d)	list_entry(d, struct spi_device, dev)

struct_cpu_desc platform_driver *plat_drv = to_platform_driver(dev->driver);
struct platform_device *plat_dev = to_platform_device(dev);


static int spi_match_device(struct device_desc *dev, struct driver_desc *drv)
{
    const struct spi_device	*spi = to_spi_device(dev);
    const struct spi_driver	*sdrv = to_spi_driver(drv);

    return strcmp(spi->modalias, drv->name) == 0;
}


struct xos_bus_desc spi_bus_type = {
    .bus_name   = "spi",
    .bus_match  = spi_match_device,
};

static int spi_drv_probe(struct device_desc *dev)
{
    const struct spi_driver		*sdrv = to_spi_driver(dev->driver);
    struct spi_device		*spi = to_spi_device(dev);
    int ret;

    ret = sdrv->probe(spi);

    return ret;
}


static int spi_drv_remove(struct device_desc *dev)
{
    const struct spi_driver		*sdrv = to_spi_driver(dev->driver);
    struct spi_device		*spi = to_spi_device(dev);
    int ret;

    ret = sdrv->remove(spi);

    return ret;
}

int spi_register_driver(struct spi_driver *sdrv)
{
    sdrv->driver.bus = &spi_bus_type;
    if (sdrv->probe)
        sdrv->driver.drv_probe = spi_drv_probe;
    if (sdrv->remove)
    sdrv->driver.drv_move = spi_drv_remove;
    return driver_register(&sdrv->driver);
}




