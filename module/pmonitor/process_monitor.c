#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <mach/platform/platform_io.h>
#include <mach/ftpmu010.h>


/* This file is a process monitor program, used to monitor the status inside the IC
 */

/* local variables 
 */
static void __iomem  *base_addr = NULL;
static struct proc_dir_entry *pmonitor_proc_root = NULL;
static struct proc_dir_entry *counter_info = NULL;
static unsigned int apb_clk;

/* platform extern functions
 */
extern void platform_init(void);
extern void platform_exit(void);

/* 
 * Module Parameter
 */
static ushort period_ms = 1000;    /* ms */
module_param(period_ms, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(period_ms, "monitor period in ms");

#define BLOCK_CNT   16
#define COUNTER_CNT 3

static u32 block_val[BLOCK_CNT][COUNTER_CNT];

static irqreturn_t pmonitor_hander(int irq, void *dev_id)
{
    u32 value, timer_value, blk_idx, cnt_idx, offset = 0x0C;
    
    if (dev_id) {}
    
    /* clear interrupt */
    value = ioread32(base_addr);
    value = (value & ~0x7) | 0x2;    //PM_INT_CLR
    iowrite32(value, base_addr);
    
    /* PM_START */
    value |= ((0x1 << 2) | 0x1);
            
    /* start to read value */
    for (blk_idx = 0; blk_idx < BLOCK_CNT; blk_idx ++) {
        for (cnt_idx = 0; cnt_idx < COUNTER_CNT; cnt_idx ++) {
            block_val[blk_idx][cnt_idx] = ioread32(base_addr + offset);
            offset += 0x4;
        }
    }
    
    /* set the period */
    timer_value = period_ms * (apb_clk / 1000);
    iowrite32(timer_value, base_addr + 0x4);
    
    //start
    iowrite32(value, base_addr);

    return IRQ_HANDLED;
}

/* PROC
 */
static int proc_read_counter_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int block, counter, len = 0;
        
    for (block = 0; block < BLOCK_CNT; block ++ ) {
        len += sprintf(page + len, "Block idx: %d \n", block);        
        for (counter = 0; counter < COUNTER_CNT; counter ++)
            len += sprintf(page + len, "    --- Counter idx: %d, value: 0x%x \n", counter, block_val[block][counter]);        
    }
    return len;
}

int proc_init(void)
{
    int ret = 0;
    struct proc_dir_entry *p;
    
    p = create_proc_entry("pmonitor", S_IFDIR | S_IRUGO | S_IXUGO, NULL);
    if (p == NULL) {
        ret = -ENOMEM;
        goto end;
    }
    
    pmonitor_proc_root = p;
    
    counter_info = create_proc_entry("counter", S_IRUGO, pmonitor_proc_root);
    if (counter_info == NULL)
        panic("Fail to create proc counter_info!\n");

    counter_info->read_proc = (read_proc_t *) proc_read_counter_info;
    counter_info->write_proc = NULL;
    
end:
    return ret;    
}

void proc_exit(void)
{
    if (counter_info != NULL)
        remove_proc_entry(counter_info->name, pmonitor_proc_root);
        
    if (pmonitor_proc_root != NULL)
        remove_proc_entry(pmonitor_proc_root->name, NULL);        
}

static int __init pmonitor_init(void)
{
    int ret = 0;
    u32 timer_value, value;
    
    apb_clk = APB0_CLK_IN;
        
    platform_init();
    proc_init();
    
    base_addr = ioremap_nocache(PMONITOR_PA_BASE, PMONITOR_PA_SIZE);
    if (base_addr == NULL) {
        printk("%s, error in ioremap! \n", __func__);
        return -1;
    }
    
    /* set the period */
    timer_value = period_ms * (apb_clk / 1000);
    iowrite32(timer_value, base_addr + 0x4);
    printk("pMonitor: monitor period is %d ms, apb_clk = %d \n", period_ms, apb_clk);
           
    ret = request_irq(PMONITOR_IRQ, pmonitor_hander, 0, "pMonitor", NULL);
    if (ret < 0) {
        printk("%s, request_irq failed: %d\n", __func__, ret);
        return -1;
    }
    
    /* start the pmonitor now */
    value = 0x1 | (0x1 << 1) | (0x1 << 2);  /* PM_INT_EN, PM_INT_CLR, PM_START */
    iowrite32(value, base_addr);
        
    return ret;
}

static void __exit pmonitor_exit(void)
{    
    __iounmap(base_addr);
    base_addr = NULL;
    
    free_irq(PMONITOR_IRQ, NULL);
    
    proc_exit();
    platform_exit();
    
    return;
}

module_init(pmonitor_init);
module_exit(pmonitor_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Process Monitor");
MODULE_LICENSE("GPL");
