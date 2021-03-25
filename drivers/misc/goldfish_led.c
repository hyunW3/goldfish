#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/acpi.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#define LED_ON_xth		0x10	
#define LED_ON_ALL		0x11
#define LED_OFF_xth		0x12
#define LED_OFF_ALL		0x13
#define LED_color_xth	0x14
#define LED_color_ALL	0x15

#define LED_OFF			0x00
#define LED_WHITE		0x01
#define LED_RED			0x02
#define LED_YELLOW		0x03
#define LED_GREEN		0x04
#define LED_SKY			0x05
#define LED_PURPLE		0x06
struct goldfish_led_data {
	void __iomem *reg_base;
};

#define GOLDFISH_LED_READ(data, addr) \
	(readl(data->reg_base + addr))
#define GOLDFISH_LED_WRITE(data, addr, x) \
	(writel(x, data->reg_base + addr))

struct goldfish_led_data *led_data;

enum  {
	LED0    = 0x000,
	LED1    = 0x004,
	LED2    = 0x008,
	LED3    = 0x00C,
	LED4    = 0x010,
	LED5    = 0x014,
	LED6    = 0x018,
};
asmlinkage long sys_led(void){
	GOLDFISH_LED_WRITE(led_data, LED6, 0x0);
	GOLDFISH_LED_WRITE(led_data, LED5, 0x1);
	GOLDFISH_LED_WRITE(led_data, LED4, 0x2);
	GOLDFISH_LED_WRITE(led_data, LED3, 0x3);
	GOLDFISH_LED_WRITE(led_data, LED2, 0x4);
	GOLDFISH_LED_WRITE(led_data, LED1, 0x5);
	GOLDFISH_LED_WRITE(led_data, LED0, 0x6);

	return 0;
}
static int led_open(struct inode *inode, struct file *file){
	printk(KERN_INFO "LED file is open\n");
	return 0;
}
static int led_release(struct inode *inode, struct file *file){
	printk(KERN_INFO "LED file is close\n");
	return 0;
}
static ssize_t led_read (struct file *file, char __user *buf, size_t size, loff_t *loff){
	char kbuf[8];
	int i;
	for(i=0; i<8; i++){
		GOLDFISH_LED_READ(led_data,4*i);		
	}
	copy_to_user(buf,kbuf,sizeof(kbuf));
	return sizeof(kbuf);
}
static ssize_t led_write (struct file *file, char __user *buf, size_t size, loff_t *loff){
	char kbuf[8];
	int i;
	copy_from_user(buf,kbuf,sizeof(kbuf));
	for(i=0; i<8; i++){
		GOLDFISH_LED_WRITE(led_data,4*i,kbuf[i]);		
	}
	return sizeof(kbuf);
}
static int led_mmap(struct file *file, struct vm_area_struct *vma){
	if(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		printk(KERN_INFO "LED MMAP fail\n");
		return -EAGAIN;
	}   
	return 0;
}

static long led_ioctl(struct file *file, unsigned int cmd, unsigned long para){
	int i;
	unsigned long tmp;
    switch(cmd){
        case LED_ON_xth:
			GOLDFISH_LED_WRITE(led_data,4*para,LED_WHITE);		
           	printk("LED_ON_%dth\n",para);
            break;
        case LED_ON_ALL:
			for(i=0; i<7; i++){
				GOLDFISH_LED_WRITE(led_data,4*i,LED_WHITE);		
			}
            printk("LED_ON_ALL\n");
            break;
        case LED_OFF_xth:
			GOLDFISH_LED_WRITE(led_data,4*para,LED_OFF);		
            printk("%d\n",(int)para+1);
            break;
		case LED_OFF_ALL:
			for(i=0; i<7; i++){
				GOLDFISH_LED_WRITE(led_data,4*i,LED_OFF);		
			}
			break;
		case LED_color_xth:
			i = para>>4;
			tmp = para & 0x0F;
			printk("%xth color:%x\n",i,tmp);
			GOLDFISH_LED_WRITE(led_data,4*i,tmp);		
			//GOLDFISH_LED_WRITE(led_data,4*i,para);		
			break;
		case LED_color_ALL: // LED ON on specific color
			for(i=0; i<7; i++){
				GOLDFISH_LED_WRITE(led_data,4*i,para);		// para : color
			}
			break;
		default:
			return -1; // wrong operation
    }
	return 0;
}

static const struct file_operations led_fops ={
	.owner = THIS_MODULE,
	.read = led_read,
	.write = led_write,
	.unlocked_ioctl = led_ioctl,
	.compat_ioctl = led_ioctl,
	.open = led_open,
	.release = led_release,
	.mmap = led_mmap,
};
static struct miscdevice led_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "led",
	.fops = &led_fops
};
static int goldfish_led_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
//	struct goldfish_led_data *data;

	printk(KERN_ERR "led probe started\n");
	led_data = devm_kzalloc(&pdev->dev, sizeof(*led_data), GFP_KERNEL);
	if (led_data == NULL)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "platform_get_resource failed\n");
		return -ENODEV;
	}

	if(request_mem_region(r->start, resource_size(r), "led")== NULL){
		printk(KERN_INFO "register led fail\n");
		return -EBUSY;
	}
	misc_register(&led_dev);
	led_data->reg_base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (led_data->reg_base == NULL) {
		dev_err(&pdev->dev, "unable to remap MMIO\n");
		return -ENOMEM;
	}

	printk(KERN_ERR "led probe finished\n");

	GOLDFISH_LED_WRITE(led_data, LED0, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED1, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED2, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED3, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED4, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED5, LED_OFF);
	GOLDFISH_LED_WRITE(led_data, LED6, LED_OFF);
/*
	GOLDFISH_LED_WRITE(led_data, LED0, 0x0);
	GOLDFISH_LED_WRITE(led_data, LED1, 0x1);
	GOLDFISH_LED_WRITE(led_data, LED2, 0x2);
	GOLDFISH_LED_WRITE(led_data, LED3, 0x3);
	GOLDFISH_LED_WRITE(led_data, LED4, 0x4);
	GOLDFISH_LED_WRITE(led_data, LED5, 0x5);
	GOLDFISH_LED_WRITE(led_data, LED6, 0x6);
*/
	return 0;
}

static int goldfish_led_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id goldfish_led_of_match[] = {
	{ .compatible = "generic,goldfish_led", },
	{},
};
MODULE_DEVICE_TABLE(of, goldfish_led_of_match);

static const struct acpi_device_id goldfish_led_acpi_match[] = {
	{ "GFSH0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, goldfish_led_acpi_match);


static struct platform_driver goldfish_led_device = {
	.probe		= goldfish_led_probe,
	.remove		= goldfish_led_remove,
	.driver = {
		.name = "goldfish_led",
		.of_match_table = goldfish_led_of_match,
		.acpi_match_table = ACPI_PTR(goldfish_led_acpi_match),
	}
};
module_platform_driver(goldfish_led_device);


MODULE_AUTHOR("Jinkyu Jeong wowzerjk@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Led driver for the Goldfish emulator");
