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
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#define IODEV_READ 0x301
#define IODEV_WRITE 0x302

#define STATUS_REG		0x00
#define CMD_REG			0x04
#define LBA_REG			0x08
#define BUF_REG			0x0c

#define READ_CMD		0
#define WRITE_CMD		1

#define DEV_BUSY		1
#define DEV_READY		0

#define WAIT_FOR_COMPLETION	1

static wait_queue_head_t wait_q;
static int condition = 0;
static spinlock_t wait_q_lock;
static uint32_t kbuf[PAGE_SIZE/sizeof(uint32_t)];

struct goldfish_iodev_data {
	void __iomem *reg_base;
};

static struct goldfish_iodev_data *data;

#define GOLDFISH_IODEV_READ(data, addr) \
	(readl(data->reg_base + addr))
#define GOLDFISH_IODEV_WRITE(data, addr, x) \
	(writel(x, data->reg_base + addr))


static int iodev_mmap(struct file *file, struct vm_area_struct *vma){
	if(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		printk(KERN_INFO "iodev MMAP fa\n");
		return -EAGAIN;
	}
	return 0;
}


static irqreturn_t goldfish_iodev_handler(int irq, void *dev_id)
{
	printk(KERN_ERR "(Interrupt) irq: %d\n", irq);
	spin_lock(&wait_q_lock);
	condition = 1;
	wake_up(&wait_q);
	spin_unlock(&wait_q_lock);
	return IRQ_HANDLED;
}

#define PAGE_SHIFT	12
#define PAGE_REMAIN	0xfff
/*
static inline int devm_request_irq(struct device* dev, unsigned int irg, irq_handler_t handler, unsigned long irqflags, const char *devname, void* dev_id){
	// week5
	
}
*/
static ssize_t iodev_read (struct file *file, char __user *buf, size_t size, loff_t *loff) {
	int i;
	uint32_t lpn;

	if (*loff & (PAGE_SIZE - 1) || size != PAGE_SIZE )
		return -EINVAL;

	lpn = (uint32_t)(*loff >> PAGE_SHIFT);

	/*(iodev_read)
		1. Read status register (wait if busy)
		1-2. Write LBA
		2. Write command register
		3. Read buf (check loff whether it aligned to 4K)
		4. Wait for completion (interrupt)
		5. copy_to_user()
	 */
	
	while (readl(data->reg_base+STATUS_REG) != DEV_READY);
	
	spin_lock_irq(&wait_q_lock);
	condition = 0;

	GOLDFISH_IODEV_WRITE(data,BUF_REG,lpn); // write LBA
	// 2 write CMD REG	
	//GOLDFISH_IODEV_WRITE(data,CMD_REG,READ_CMD);	
	writel(READ_CMD,data->reg_base+CMD_REG);
	for (i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++ ) {
		//3
		// ?? check loff whether is aligned to 4k ??
		kbuf[i] = GOLDFISH_IODEV_READ(data,BUF_REG);	
	}
	// interrupt
	wait_event_lock_irq(wait_q,condition,wait_q_lock);
	spin_unlock_irq(&wait_q_lock);
	//5
	copy_to_user(buf,kbuf,sizeof(kbuf));
	return size;
}

static ssize_t iodev_write (struct file *file, const char __user *buf, size_t size, loff_t *loff) {
	int i;
	uint32_t lpn;

	if (*loff & (PAGE_SIZE - 1) || size != PAGE_SIZE )
		return -EINVAL;

	lpn = (uint32_t)(*loff >> PAGE_SHIFT);
	copy_from_user((char*)kbuf, buf, size);

	/*(iodev_write)
		1. Read status register (until not DEV_READY)
		2. Write lpn to LBA_REG
		3. Write kbuf repeatedly (size of 4 bytes)f to BUF_REG
		4. Write WRITE_CMD to CMD_REG
		5. Wait for condition variable (condition)
		6. Increase offset
		7. Return size
	 */
	while (readl(data->reg_base + STATUS_REG) != DEV_READY);	//1

	spin_lock_irq(&wait_q_lock);
	condition = 0;

	//2
	GOLDFISH_IODEV_WRITE(data,LBA_REG,lpn);		

	for (i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++ ) {
		//3
		GOLDFISH_IODEV_WRITE(data,BUF_REG,kbuf[i]);	
	}

	//4
	GOLDFISH_IODEV_WRITE(data,CMD_REG,WRITE_CMD);	

	wait_event_lock_irq(wait_q, condition, wait_q_lock);	//5
	spin_unlock_irq(&wait_q_lock);

	(*loff) += size;	//6

	return size;	//7
}

static int goldfish_iodev_remove(struct platform_device *pdev)
{
	return 0;
}

static int iodev_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "iodev file is opened\n");
	return 0;
}

static int iodev_release(struct inode *inode, struct file *file) {
	printk(KERN_INFO "iodev file is close\n");
	return 0;
}

static const struct file_operations iodev_fops = {
	.owner = THIS_MODULE,
	.read = iodev_read,
	.write = iodev_write,
	.open = iodev_open,
	.release = iodev_release,
	.mmap = iodev_mmap,
};

static struct miscdevice iodev_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "iodev",
	.fops = &iodev_fops
};

static int goldfish_iodev_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
	int error;

	printk(KERN_ERR "iodev probe started\n");
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "platform_get_resource failed\n");
		return -ENODEV;
	}

	if(request_mem_region(r->start, resource_size(r), "7iodev")==NULL){
		printk(KERN_INFO "register 7iodev fail\n");
		return -EBUSY;
	}

	misc_register(&iodev_dev);

	data->reg_base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (data->reg_base == NULL) {
		dev_err(&pdev->dev, "unable to remap MMIO\n");
		return -ENOMEM;
	}
	// handler : IO Finish
	error = devm_request_irq(&pdev->dev, platform_get_irq(pdev, 0), goldfish_iodev_handler, 0, "goldfish_iodev", NULL);
	init_waitqueue_head(&wait_q);
	spin_lock_init(&wait_q_lock);
	condition = 0;
	printk(KERN_ERR "iodev probe finished\n");


	return 0;
}


static const struct of_device_id goldfish_iodev_of_match[] = {
	{ .compatible = "generic,goldfish_iodev", },
	{},
};
MODULE_DEVICE_TABLE(of, goldfish_iodev_of_match);

static const struct acpi_device_id goldfish_iodev_acpi_match[] = {
	{ "GFSH0001", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, goldfish_iodev_acpi_match);


static struct platform_driver goldfish_iodev_device = {
	.probe		= goldfish_iodev_probe,
	.remove		= goldfish_iodev_remove,
	.driver = {
		.name = "goldfish_iodev",
		.of_match_table = goldfish_iodev_of_match,
		.acpi_match_table = ACPI_PTR(goldfish_iodev_acpi_match),
	}
};
module_platform_driver(goldfish_iodev_device);


MODULE_AUTHOR("Jinkyu Jeong wowzerjk@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("7 iodev driver for the Goldfish emulator");
