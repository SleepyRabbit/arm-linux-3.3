#include <linux/version.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "ft3dnr200.h"

static struct proc_dir_entry *ctrlproc;
static struct proc_dir_entry *ctrl_paramproc, *ctrl_ctrlproc, *ctrl_ctrl_spatialenproc, *ctrl_ctrl_temporalenproc, *ctrl_ctrl_eeenproc;

static int ctrl_proc_param_show(struct seq_file *sfile, void *v)
{
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;
    ft3dnr_ctrl_t ctrl;
    ft3dnr_ycbcr_t ycbcr;

    tmp = *(volatile unsigned int *)(base + 0x00);
    ctrl.spatial_en = tmp & 0x1;
    ctrl.temporal_en = (tmp >> 1) & 0x1;
    ctrl.tnr_learn_en = (tmp >> 3) & 0x1;
    ctrl.ee_en = (tmp >> 10) & 0x1;
    ctrl.tnr_rlt_w = (tmp >> 12) & 0x1;
    ycbcr.src_yc_swap = (tmp >> 4) & 0x1;;
    ycbcr.src_cbcr_swap = (tmp >> 5) & 0x1;
    ycbcr.dst_yc_swap = (tmp >> 6) & 0x1;
    ycbcr.dst_cbcr_swap = (tmp >> 7) & 0x1;
    ycbcr.ref_yc_swap = (tmp >> 8) & 0x1;
    ycbcr.ref_cbcr_swap = (tmp >> 9) & 0x1;

    seq_printf(sfile, "spatial_en = %d\n", ctrl.spatial_en);
    seq_printf(sfile, "temporal_en = %d\n", ctrl.temporal_en);
    seq_printf(sfile, "tnr_learn_en = %d\n", ctrl.tnr_learn_en);
    seq_printf(sfile, "ee_en = %d\n", ctrl.ee_en);
    seq_printf(sfile, "tnr_rlt_w = %d\n", ctrl.tnr_rlt_w);
    seq_printf(sfile, "src_yc_swap = %d\n", ycbcr.src_yc_swap);
    seq_printf(sfile, "src_cbcr_swap = %d\n", ycbcr.src_cbcr_swap);
    seq_printf(sfile, "des_yc_swap = %d\n", ycbcr.dst_yc_swap);
    seq_printf(sfile, "des_cbcr_swap = %d\n", ycbcr.dst_cbcr_swap);
    seq_printf(sfile, "ref_yc_swap = %d\n", ycbcr.ref_yc_swap);
    seq_printf(sfile, "ref_cbcr_swap = %d\n", ycbcr.ref_cbcr_swap);

    return 0;
}

static int ctrl_proc_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, ctrl_proc_param_show, PDE(inode)->data);
}

static int ctrl_proc_ctrl_spatialen_show(struct seq_file *sfile, void *v)
{
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;
    ft3dnr_ctrl_t ctrl;

    tmp = *(volatile unsigned int *)(base + 0x00);
    ctrl.spatial_en = tmp & 0x1;

    seq_printf(sfile, "spatial_en = %d\n", ctrl.spatial_en);

    return 0;
}

static int ctrl_proc_ctrl_spatialen_open(struct inode *inode, struct file *file)
{
    return single_open(file, ctrl_proc_ctrl_spatialen_show, PDE(inode)->data);
}

static ssize_t ctrl_proc_ctrl_spatialen_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &value);
    value = (value == 0 ? 0 : 1);

    priv.default_param.ctrl.spatial_en = value;

    return count;
}

static int ctrl_proc_ctrl_temporalen_show(struct seq_file *sfile, void *v)
{
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;
    ft3dnr_ctrl_t ctrl;

    tmp = *(volatile unsigned int *)(base + 0x00);
    ctrl.temporal_en = (tmp >> 1) & 0x1;
    //ctrl.tnr_learn_en = (tmp >> 3) & 0x1;

    seq_printf(sfile, "temporal_en = %d\n", ctrl.temporal_en);
    //seq_printf(sfile, "tnr_learn_en = %d\n", ctrl.tnr_learn_en);

    return 0;
}

static int ctrl_proc_ctrl_temporalen_open(struct inode *inode, struct file *file)
{
    return single_open(file, ctrl_proc_ctrl_temporalen_show, PDE(inode)->data);
}

static ssize_t ctrl_proc_ctrl_temporalen_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &value);
    value = (value == 0 ? 0 : 1);

    priv.default_param.ctrl.temporal_en = value;
    priv.default_param.ctrl.tnr_learn_en = value;

    return count;
}

static int ctrl_proc_ctrl_eeen_show(struct seq_file *sfile, void *v)
{
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;
    ft3dnr_ctrl_t ctrl;

    tmp = *(volatile unsigned int *)(base + 0x00);
    ctrl.ee_en = (tmp >> 10) & 0x1;

    seq_printf(sfile, "ee_en = %d\n", ctrl.ee_en);

    return 0;
}

static int ctrl_proc_ctrl_eeen_open(struct inode *inode, struct file *file)
{
    return single_open(file, ctrl_proc_ctrl_eeen_show, PDE(inode)->data);
}

static ssize_t ctrl_proc_ctrl_eeen_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &value);
    value = (value == 0 ? 0 : 1);

    priv.default_param.ctrl.ee_en = value;

    return count;
}

static struct file_operations ctrl_proc_param_ops = {
    .owner  = THIS_MODULE,
    .open   = ctrl_proc_param_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations ctrl_proc_ctrl_spatialen_ops = {
    .owner  = THIS_MODULE,
    .open   = ctrl_proc_ctrl_spatialen_open,
    .write  = ctrl_proc_ctrl_spatialen_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations ctrl_proc_ctrl_temporalen_ops = {
    .owner  = THIS_MODULE,
    .open   = ctrl_proc_ctrl_temporalen_open,
    .write  = ctrl_proc_ctrl_temporalen_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations ctrl_proc_ctrl_eeen_ops = {
    .owner  = THIS_MODULE,
    .open   = ctrl_proc_ctrl_eeen_open,
    .write  = ctrl_proc_ctrl_eeen_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

int ft3dnr_ctrl_proc_init(struct proc_dir_entry *entity_proc)
{
    int ret = 0;

    ctrlproc = create_proc_entry("ctrl", S_IFDIR|S_IRUGO|S_IXUGO, entity_proc);
    if (ctrlproc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrlproc->owner = THIS_MODULE;
#endif

    /* the ctrl parameter content of controller register */
    ctrl_paramproc = create_proc_entry("param", S_IRUGO|S_IXUGO, ctrlproc);
    if (ctrl_paramproc == NULL) {
        printk("error to create %s/ctrl/param proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ctrl_paramproc->proc_fops = &ctrl_proc_param_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrl_paramproc->owner = THIS_MODULE;
#endif

    /* provide the control interface for users */
    ctrl_ctrlproc = create_proc_entry("control", S_IFDIR|S_IRUGO|S_IXUGO, ctrlproc);
    if (ctrl_ctrlproc == NULL) {
        printk("error to create %s/ctrl/control proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrl_ctrlproc->owner = THIS_MODULE;
#endif

    /* the control interface of spatial_en parameter */
    ctrl_ctrl_spatialenproc = create_proc_entry("spatial_en", S_IRUGO|S_IXUGO, ctrl_ctrlproc);
    if (ctrl_ctrl_spatialenproc == NULL) {
        printk("error to create %s/ctrl/control/spatial_en proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ctrl_ctrl_spatialenproc->proc_fops = &ctrl_proc_ctrl_spatialen_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrl_ctrl_spatialenproc->owner = THIS_MODULE;
#endif

    /* the control interface of temporal_en parameter */
    ctrl_ctrl_temporalenproc = create_proc_entry("temporal_en", S_IRUGO|S_IXUGO, ctrl_ctrlproc);
    if (ctrl_ctrl_temporalenproc == NULL) {
        printk("error to create %s/ctrl/control/temporal_en proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ctrl_ctrl_temporalenproc->proc_fops = &ctrl_proc_ctrl_temporalen_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrl_ctrl_temporalenproc->owner = THIS_MODULE;
#endif

    /* the control interface of ee_en parameter */
    ctrl_ctrl_eeenproc = create_proc_entry("ee_en", S_IRUGO|S_IXUGO, ctrl_ctrlproc);
    if (ctrl_ctrl_eeenproc == NULL) {
        printk("error to create %s/ctrl/control/ee_en proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ctrl_ctrl_eeenproc->proc_fops = &ctrl_proc_ctrl_eeen_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ctrl_ctrl_eeenproc->owner = THIS_MODULE;
#endif

    return 0;
}

void ft3dnr_ctrl_proc_remove(struct proc_dir_entry *entity_proc)
{
    if (ctrl_ctrl_eeenproc)
        remove_proc_entry(ctrl_ctrl_eeenproc->name, ctrl_ctrlproc);
    if (ctrl_ctrl_temporalenproc)
        remove_proc_entry(ctrl_ctrl_temporalenproc->name, ctrl_ctrlproc);
    if (ctrl_ctrl_spatialenproc)
        remove_proc_entry(ctrl_ctrl_spatialenproc->name, ctrl_ctrlproc);
    if (ctrl_ctrlproc)
        remove_proc_entry(ctrl_ctrlproc->name, ctrlproc);
    if (ctrl_paramproc)
        remove_proc_entry(ctrl_paramproc->name, ctrlproc);

    if (ctrlproc)
        remove_proc_entry(ctrlproc->name, ctrlproc->parent);
}
