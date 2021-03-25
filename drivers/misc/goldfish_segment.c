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
#define CMD1 0x04
#define CMD2 0x05
#define CMD3 0x06

#define val0 0x77
#define val1 0x24
#define val2 0x5d
#define val3 0x6d
#define val4 0x2e
#define val5 0x6b
#define val6 0x7a
#define val7 0x27
#define val8 0x7F
#define val9 0x2F
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
//asmlinkage long sys_segment(void){
asmlinkage long sys_segment(int number){
	int segment = 0x000;
	int next = 0x004;
	int num = number;
	int val = 0;
	int arr_val[10]={val0,val1,val2,val3,val4,val5,val6,val7,val8,val9};
	int i=0;
	printk("ESP - init number : %d\n",number);
	if(number > 9999999){	
		for(i=0; i<7; i++){
			GOLDFISH_SEGMENT_WRITE(segment_data, segment+i*next, val0);
			printk("ESP - overflow : %d %d\n",segment+i*next,val0);
		}	
	} else {
		segment = 0x018; // reverse wise , start LSB
//		while(segment>=0x000){
		for(i=0; i<7; i++){
			val = num%10;
			num = num/10;
			GOLDFISH_SEGMENT_WRITE(segment_data, segment-i*next, arr_val[val] );
			printk("ESP -%dth val :%d num: %d ,, %x %x\n",i,val,num,segment-i*next,arr_val[val]);
		}
	}	
	return 0;
}
static int segment_open(struct inode *inode, struct file *file){
    printk(KERN_INFO "segment file is open\n");
    return 0;
}
static int segment_release(struct inode *inode, struct file *file){
    printk(KERN_INFO "segment file is close\n");
    return 0;
}
static ssize_t segment_read (struct file *file, char __user *buf, size_t size, loff_t *loff){
    char kbuf[8];
	int i;
	for(i=0; i<8; i++){
		kbuf[i] = GOLDFISH_SEGMENT_READ(segment_data,4*i);
	}
    copy_to_user(buf,kbuf,sizeof(kbuf));
    return sizeof(kbuf);
}
static ssize_t segment_write (struct file *file, char __user *buf, size_t size, loff_t *loff){
    char kbuf[8];
	int i;
    copy_from_user(buf,kbuf,sizeof(kbuf));
	for(i=0; i<8; i++){
		GOLDFISH_SEGMENT_WRITE(segment_data,4*i,kbuf[i]);
	}
    return sizeof(kbuf);
}
static int segment_mmap(struct file *file, struct vm_area_struct *vma){
	if(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		printk(KERN_INFO "segment MMAP fail\n");
		return -EAGAIN;
	}
	return 0;
}
static long segment_ioctl(struct file *file, unsigned int cmd, unsigned long para){
	switch(cmd){
		case CMD1:
			printk("CMD1\n");
			break;
		case CMD2:
			printk("CMD2\n");
			break;
		case CMD3:
			printk("CMD3 %d\n",(int)para+1);
			break;
	}	
	return 0;
}
static const struct file_operations segment_fops ={
	.owner = THIS_MODULE,
	.read = segment_read,
	.write = segment_write,
	.unlocked_ioctl = segment_ioctl,
	.compat_ioctl = segment_ioctl,
	.open = segment_open,
	.release = segment_release,
	.mmap = segment_mmap,
};
static struct miscdevice segment_dev ={
	.minor = MISC_DYNAMIC_MINOR,
	.name = "segment",
	.fops = &segment_fops
};
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

	// week4 MMIO
	if(request_mem_region(r->start, resource_size(r), "7segment")==NULL){
		printk(KERN_INFO "register 7segment fail\n");
		return -EBUSY;
	}
	misc_register(&segment_dev);

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
