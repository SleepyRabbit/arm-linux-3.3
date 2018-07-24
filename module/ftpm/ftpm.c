/**
 * Copyright (C) 2016 Faraday Corp. (http://www.faraday-tech.com)
 *
 * Modified by Justin
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <linux/random.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/synclink.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <mach/fmem.h>
#include <linux/proc_fs.h>

#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <mach/platform/platform_io.h>
#include <mach/ftpmu010.h>
#include <linux/gpio.h>


static int pm_open(struct inode *inode, struct file *filp);
static long pm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int pm_mmap(struct file *filp, struct vm_area_struct *vma);
static int pm_release(struct inode *inode, struct file *filp);

int pm_probe(struct platform_device *pdev);
static int __devexit pm_remove(struct platform_device *pdev);
extern int gm_pm_enter(u32 sram_base, u32 ddr_base, u32 gpio_base, int pin);

unsigned int sram_base, ddr_base, gpio_base;

static int GPIO_PORT = 0;
module_param(GPIO_PORT, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(GPIO_PORT, "GPIO port");

static int GPIO_PIN = 0;
module_param(GPIO_PIN, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(GPIO_PIN, "GPIO pin");

#define ftpm_proc_name 	"ftpm"
#define ftpm_enter_name	"enter_pm"
static struct proc_dir_entry *ftpm_proc = NULL;


struct file_operations pm_fops = {
  owner:THIS_MODULE,
  unlocked_ioctl:pm_ioctl,
  mmap:pm_mmap,
  open:pm_open,
  release:pm_release,
};

struct miscdevice pm_device = {
  minor:MISC_DYNAMIC_MINOR,
  name:ftpm_proc_name,
  fops:&pm_fops,
};

static struct platform_driver pm_driver = {
    .driver = {
               .owner = THIS_MODULE,
               .name = ftpm_proc_name,

               },
    .probe  = pm_probe,
    .remove = __devexit_p(pm_remove),
};


int pm_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret = 0;

    return ret;
}

static int pm_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int pm_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int __devexit pm_remove(struct platform_device *pdev)
{
    return 0;
}

int pm_probe(struct platform_device *pdev)
{
    struct resource *mem;
    int ret = 0;

    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mem) {
        printk("no mem resource?\n");
        ret = -ENODEV;
        goto err_final;
    }

    return 0;

err_final:
    return ret;
}

static long pm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static ssize_t ftpm_write(struct file *file, const char *buffer, size_t count, loff_t *offset)
{
    gm_pm_enter(sram_base, ddr_base, gpio_base, GPIO_PIN);
    return count;
}

static int dummy_proc_show(struct seq_file *sfile, void *v)
{
    return 0;
}

static int dummy_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, dummy_proc_show, NULL);
}

static const struct file_operations ftpm_set_proc = {
	.owner	= THIS_MODULE,
	.open	= dummy_proc_open,
    .write  = ftpm_write,
    .release = single_release,
};

/*
 * The driver boot-time initialization code!
 */
static int __init pm_init(void)
{
	char str[6];
    int ret = 0;

    /* Register the driver */
    if ((ret = platform_driver_register(&pm_driver)) < 0) {
        printk("Failed to register Security driver\n");
        goto out3;
    }

    /* register misc device */
    if ((ret = misc_register(&pm_device)) < 0) {
        printk("can't get major number");
        goto out3;
    }

	sprintf(str, "gpio%d", GPIO_PORT);
    if ((ret = gpio_request(GPIO_PIN, str)) != 0) {
        printk("gpio request fail\n");
        return ret;
    }
    gpio_direction_output(GPIO_PIN, 0);

    printk("PM set %s, pin %d\n", str, GPIO_PIN);

	sram_base = ioremap_nocache(MCP_FTMCP100_PA_BASE, MCP_FTMCP100_PA_SIZE);
	ddr_base = ioremap_nocache(DDRC_FTDDRC010_PA_BASE, DDRC_FTDDRC010_PA_SIZE);

	if(GPIO_PORT)
		gpio_base = ioremap_nocache(GPIO_FTGPIO010_1_PA_BASE, GPIO_FTGPIO010_1_PA_SIZE);
	else
		gpio_base = ioremap_nocache(GPIO_FTGPIO010_0_PA_BASE, GPIO_FTGPIO010_0_PA_SIZE);

	//printk("pm = 0x%x, GPIO = 0x%x\n", ddr_base, gpio_base);

    ftpm_proc = proc_mkdir(ftpm_proc_name, NULL);
    if (ftpm_proc == NULL) {
        printk(KERN_ERR "Error to create driver ftpm proc\n");
        goto out3;
    }

    if (!proc_create(ftpm_enter_name, 0, ftpm_proc, &ftpm_set_proc))
        panic("Fail to create proc for ftpm!\n");

out3:
    return ret;
}

static void __exit pm_exit(void)
{
    remove_proc_entry(ftpm_enter_name, ftpm_proc);
    remove_proc_entry(ftpm_proc_name, NULL);

	iounmap((void __iomem *)sram_base);
	iounmap((void __iomem *)ddr_base);
	iounmap((void __iomem *)gpio_base);

	gpio_free(GPIO_PIN);

    misc_deregister(&pm_device);

    platform_driver_unregister(&pm_driver);
}

module_init(pm_init);
module_exit(pm_exit);

MODULE_DESCRIPTION("Grain Media PM Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
