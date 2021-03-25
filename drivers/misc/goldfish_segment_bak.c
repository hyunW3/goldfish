#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/acpi.h>
#include <linux/kernel.h>

struct goldfish_segment_data {
	void __iomem *reg_base;
};

#define GOLDFISH_SEGMENT_READ(data, addr) \
	(readl(data->reg_base + addr))
#define GOLDFISH_SEGMENT_WRITE(data, addr, x) \
	(writel(x, data->reg_base + addr))

enum  {
	SEGMENT0    = 0x000,
	SEGMENT1    = 0x004,
	SEGMENT2    = 0x008,
	SEGMENT3    = 0x00C,
	SEGMENT4    = 0x010,
	SEGMENT5    = 0x014,
	SEGMENT6    = 0x018,
};

struct goldfish_segment_data *segment_data;
asmlinkage long sys_segment(void){
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT6, 0x24);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT5, 0x5d);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT4, 0x6d);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT3, 0x2e);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT2, 0x6b);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT1, 0x7a);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT0, 0x27);

	return 0;
}

static int goldfish_segment_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
//	struct goldfish_segment_data *data;

	printk(KERN_ERR "segment probe started\n");
	segment_data = devm_kzalloc(&pdev->dev, sizeof(*segment_data), GFP_KERNEL);
	if (segment_data == NULL)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "platform_get_resource failed\n");
		return -ENODEV;
	}

	segment_data->reg_base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (segment_data->reg_base == NULL) {
		dev_err(&pdev->dev, "unable to remap MMIO\n");
		return -ENOMEM;
	}

	printk(KERN_ERR "segment probe finished\n");

	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT0, 0x24);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT1, 0x5d);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT2, 0x6d);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT3, 0x2e);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT4, 0x6b);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT5, 0x7a);
	GOLDFISH_SEGMENT_WRITE(segment_data, SEGMENT6, 0x27);

	return 0;
}

static int goldfish_segment_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id goldfish_segment_of_match[] = {
	{ .compatible = "generic,goldfish_segment", },
	{},
};
MODULE_DEVICE_TABLE(of, goldfish_segment_of_match);

static const struct acpi_device_id goldfish_segment_acpi_match[] = {
	{ "GFSH0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, goldfish_segment_acpi_match);


static struct platform_driver goldfish_segment_device = {
	.probe		= goldfish_segment_probe,
	.remove		= goldfish_segment_remove,
	.driver = {
		.name = "goldfish_segment",
		.of_match_table = goldfish_segment_of_match,
		.acpi_match_table = ACPI_PTR(goldfish_segment_acpi_match),
	}
};
module_platform_driver(goldfish_segment_device);


MODULE_AUTHOR("Jinkyu Jeong wowzerjk@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("7 segment driver for the Goldfish emulator");
