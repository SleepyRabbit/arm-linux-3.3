#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "ft3dnr200.h"

static struct proc_dir_entry *dmaproc, *paramproc, *bufAddrProcNode;

static void dma_get_param(rw_wait_value_t *param)
{
    u32 base = 0;
    u32 regVal;

    base = ft3dnr_get_ip_base();

    regVal = *(volatile u32 *)(base + 0x0204);
    param->rcmd_wait = (regVal >> 16) & 0xffff;
    param->wcmd_wait = regVal & 0xffff;
}

static void dma_set_param(RW_CMD_WAIT_VAL_ID_T index, u32 value)
{
    u32 base = 0;
    u32 regVal = 0;

    base = ft3dnr_get_ip_base();

    switch(index) {
        case WC_WAIT_VALUE:
            regVal = *(volatile u32 *)(base + 0x0204);
            regVal &= 0xffff0000;
            regVal |= value;
            *(volatile u32 *)(base + 0x0204) = regVal;
            priv.engine.wc_wait_value = value;
            break;
        case RC_WAIT_VALUE:
            regVal = *(volatile u32 *)(base + 0x0204);
            regVal &= 0xffff;
            regVal |= (value << 16);
            *(volatile u32 *)(base + 0x0204) = regVal;
            priv.engine.rc_wait_value = value;
            break;
        default:
            break;
    }
}

static ssize_t dma_proc_param_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int index = 0;
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &index, &value);

    dma_set_param(index, value);

    return count;
}

static int dma_proc_param_show(struct seq_file *sfile, void *v)
{
    rw_wait_value_t param;

    memset(&param, 0x0, sizeof(rw_wait_value_t));

    dma_get_param(&param);

    seq_printf(sfile, "\n=== DMA Parameter ===\n");
    seq_printf(sfile, "[00]WC_WAIT_VALUE (0x0~0xffff) : 0x%x\n", param.wcmd_wait);
    seq_printf(sfile, "[01]RC_WAIT_VALUE (0x0~0xffff) : 0x%x\n\n", param.rcmd_wait);

    return 0;
}

static int dma_proc_buf_addr_show(struct seq_file *sfile, void *v)
{
    dma_buf_addr_t dmaAddr;
    u32 base=0;

    base = ft3dnr_get_ip_base();

    memset(&dmaAddr,0x0,sizeof(dma_buf_addr_t));

    dmaAddr.src_addr = *(volatile u32 *)(base+THDNR_DMA_SRC_ADDR);
    dmaAddr.des_addr = *(volatile u32 *)(base+THDNR_DMA_DES_ADDR);
    dmaAddr.ee_addr = *(volatile u32 *)(base+THDNR_DMA_EE_ADDR);
    dmaAddr.ref_r_addr = *(volatile u32 *)(base+THDNR_DMA_REF_R_ADDR);
    dmaAddr.var_addr = *(volatile u32 *)(base+THDNR_DMA_VAR_ADDR);
    dmaAddr.mot_addr = *(volatile u32 *)(base+THDNR_DMA_MOT_ADDR);
    dmaAddr.ref_w_addr = *(volatile u32 *)(base+THDNR_DMA_REF_W_ADDR);

    seq_printf(sfile, "\n=== DMA Buffer Address ===\n");
    seq_printf(sfile, "SRC Buffer Address   : 0x%08X\n",dmaAddr.src_addr);
    seq_printf(sfile, "DES Buffer Address   : 0x%08X\n",dmaAddr.des_addr);
    seq_printf(sfile, "EE Buffer Address    : 0x%08X\n",dmaAddr.ee_addr);
    seq_printf(sfile, "MOT Buffer Address   : 0x%08X\n",dmaAddr.mot_addr);
    seq_printf(sfile, "VAR Buffer Address   : 0x%08X\n",dmaAddr.var_addr);
    seq_printf(sfile, "REF_R Buffer Address : 0x%08X\n",dmaAddr.ref_r_addr);
    seq_printf(sfile, "REF_W Buffer Address : 0x%08X\n\n",dmaAddr.ref_w_addr);

    return 0;
}

static int dma_proc_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, dma_proc_param_show, PDE(inode)->data);
}

static int dma_proc_buf_addr_open(struct inode *inode, struct file *file)
{
    return single_open(file, dma_proc_buf_addr_show, PDE(inode)->data);
}

static struct file_operations dma_proc_param_ops = {
    .owner  = THIS_MODULE,
    .open   = dma_proc_param_open,
    .write  = dma_proc_param_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dma_proc_addr_ops = {
    .owner  = THIS_MODULE,
    .open   = dma_proc_buf_addr_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

int ft3dnr_dma_proc_init(struct proc_dir_entry *entity_proc)
{
    int ret = 0;

    dmaproc = create_proc_entry("dma", S_IFDIR|S_IRUGO|S_IXUGO, entity_proc);
    if (dmaproc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dmaproc->owner = THIS_MODULE;
#endif

    /* DMA wc/rc wait interval */
    paramproc = create_proc_entry("param", S_IRUGO|S_IXUGO, dmaproc);
    if (paramproc == NULL) {
        printk("error to create %s/dma/param proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    paramproc->proc_fops  = &dma_proc_param_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    paramproc->owner      = THIS_MODULE;
#endif

    /* DMA buffer address */
    bufAddrProcNode = create_proc_entry("buf_addr", S_IRUGO|S_IXUGO, dmaproc);
    if (bufAddrProcNode == NULL) {
        printk("error to create %s/dma/buf_addr proc node\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    bufAddrProcNode->proc_fops  = &dma_proc_addr_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    bufAddrProcNode->owner      = THIS_MODULE;
#endif

    return 0;
}

void ft3dnr_dma_proc_remove(struct proc_dir_entry *entity_proc)
{
    if (bufAddrProcNode)
        remove_proc_entry(bufAddrProcNode->name, dmaproc);
    if (paramproc != 0)
        remove_proc_entry(paramproc->name, dmaproc);
    if (dmaproc != 0)
        remove_proc_entry(dmaproc->name, dmaproc->parent);
}

