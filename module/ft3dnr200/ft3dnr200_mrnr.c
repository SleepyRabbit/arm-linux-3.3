#include <linux/version.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "ft3dnr200.h"
#include "ft3dnr200_util.h"

static struct proc_dir_entry *mrnrproc;
static struct proc_dir_entry *mrnr_paramproc, *mrnr_nrstrproc;

static int nr_str = DEF_MRNR_NR_STR;

static int mrnr_proc_param_show(struct seq_file *sfile, void *v)
{
    int i, j;
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;
    mrnr_param_t mrnr;

    seq_printf(sfile, "\n");

    tmp = *(volatile unsigned int *)(base + 0x10);
    // luma layer edg threshold
    for (i = 0; i < 4; i++) {
        tmp = *(volatile unsigned int *)(base + 0x10 + (0x10 * i) + 0x0);
        mrnr.Y_L_edg_th[i][0] = tmp & 0x3ff;
        mrnr.Y_L_edg_th[i][1] = (tmp >> 16) & 0x3ff;
        tmp = *(volatile unsigned int *)(base + 0x10 + (0x10 * i) + 0x4);
        mrnr.Y_L_edg_th[i][2] = tmp & 0x3ff;
        mrnr.Y_L_edg_th[i][3] = (tmp >> 16) & 0x3ff;
        tmp = *(volatile unsigned int *)(base + 0x10 + (0x10 * i) + 0x8);
        mrnr.Y_L_edg_th[i][4] = tmp & 0x3ff;
        mrnr.Y_L_edg_th[i][5] = (tmp >> 16) & 0x3ff;
        tmp = *(volatile unsigned int *)(base + 0x10 + (0x10 * i) + 0xc);
        mrnr.Y_L_edg_th[i][6] = tmp & 0x3ff;
        mrnr.Y_L_edg_th[i][7] = (tmp >> 16) & 0x3ff;
    }
    // cb layer edg threshold
    tmp = *(volatile unsigned int *)(base + 0x50);
    mrnr.Cb_L_edg_th[0] = tmp & 0x3ff;
    mrnr.Cb_L_edg_th[1] = (tmp >> 16) & 0x3ff;
    tmp = *(volatile unsigned int *)(base + 0x54);
    mrnr.Cb_L_edg_th[2] = tmp & 0x3ff;
    mrnr.Cb_L_edg_th[3] = (tmp >> 16) & 0x3ff;
    // cr layer edg threshold
    tmp = *(volatile unsigned int *)(base + 0x58);
    mrnr.Cr_L_edg_th[0] = tmp & 0x3ff;
    mrnr.Cr_L_edg_th[1] = (tmp >> 16) & 0x3ff;
    tmp = *(volatile unsigned int *)(base + 0x5c);
    mrnr.Cr_L_edg_th[2] = tmp & 0x3ff;
    mrnr.Cr_L_edg_th[3] = (tmp >> 16) & 0x3ff;
    // luma layer smooth threshold
    for (i = 0; i < 4; i++) {
        tmp = *(volatile unsigned int *)(base + 0x60 + (0x8 * i) + 0x0);
        mrnr.Y_L_smo_th[i][0] = tmp & 0xff;
        mrnr.Y_L_smo_th[i][1] = (tmp >> 8) & 0xff;
        mrnr.Y_L_smo_th[i][2] = (tmp >> 16) & 0xff;
        mrnr.Y_L_smo_th[i][3] = (tmp >> 24) & 0xff;
        tmp = *(volatile unsigned int *)(base + 0x60 + (0x8 * i) + 0x4);
        mrnr.Y_L_smo_th[i][4] = tmp & 0xff;
        mrnr.Y_L_smo_th[i][5] = (tmp >> 8) & 0xff;
        mrnr.Y_L_smo_th[i][6] = (tmp >> 16) & 0xff;
        mrnr.Y_L_smo_th[i][7] = (tmp >> 24) & 0xff;
    }
    // cb layer smooth threshold
    tmp = *(volatile unsigned int *)(base + 0x80);
    mrnr.Cb_L_smo_th[0] = tmp & 0xff;
    mrnr.Cb_L_smo_th[1] = (tmp >> 8) & 0xff;
    mrnr.Cb_L_smo_th[2] = (tmp >> 16) & 0xff;
    mrnr.Cb_L_smo_th[3] = (tmp >> 24) & 0xff;
    tmp = *(volatile unsigned int *)(base + 0x84);
    mrnr.Cr_L_smo_th[0] = tmp & 0xff;
    mrnr.Cr_L_smo_th[1] = (tmp >> 8) & 0xff;
    mrnr.Cr_L_smo_th[2] = (tmp >> 16) & 0xff;
    mrnr.Cr_L_smo_th[3] = (tmp >> 24) & 0xff;
    // cr layer smooth threshold
    tmp = *(volatile unsigned int *)(base + 0x88);
    mrnr.Y_L_nr_str[0] = tmp & 0xf;
    mrnr.Y_L_nr_str[1] = (tmp >> 4) & 0xf;
    mrnr.Y_L_nr_str[2] = (tmp >> 8) & 0xf;
    mrnr.Y_L_nr_str[3] = (tmp >> 12) & 0xf;
    mrnr.C_L_nr_str[0] = (tmp >> 16) & 0xf;
    mrnr.C_L_nr_str[1] = (tmp >> 20) & 0xf;
    mrnr.C_L_nr_str[2] = (tmp >> 24) & 0xf;
    mrnr.C_L_nr_str[3] = (tmp >> 28) & 0xf;

     for (i = 0; i < 4; i++) {
        for (j = 0; j < 8; j++)
            seq_printf(sfile, "Y_L%d_edg_th%d = %d\n", i, j, mrnr.Y_L_edg_th[i][j]);
    }
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "Cb_L%d_edg_th = %d\n", i, mrnr.Cb_L_edg_th[i]);
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "Cr_L%d_edg_th = %d\n", i, mrnr.Cr_L_edg_th[i]);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 8; j++)
            seq_printf(sfile, "Y_L%d_smo_th%d = %d\n", i, j, mrnr.Y_L_smo_th[i][j]);
    }
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "Cb_L%d_smo_th = %d\n", i, mrnr.Cb_L_smo_th[i]);
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "Cr_L%d_smo_th = %d\n", i, mrnr.Cr_L_smo_th[i]);
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "Y_L%d_nr_th = %d\n", i, mrnr.Y_L_nr_str[i]);
    for (i = 0; i < 4; i++)
        seq_printf(sfile, "C_L%d_nr_th = %d\n", i, mrnr.C_L_nr_str[i]);

    seq_printf(sfile, "\n");

    return 0;
}

static int mrnr_proc_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, mrnr_proc_param_show, PDE(inode)->data);
}

static int mrnr_proc_nrstr_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "nr_str = %d\n", nr_str);

    return 0;
}

static int mrnr_proc_nrstr_open(struct inode *inode, struct file *file)
{
    return single_open(file, mrnr_proc_nrstr_show, PDE(inode)->data);
}

static ssize_t mrnr_proc_nrstr_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &value);
    nr_str = (int)clamp(value, (u32)0, (u32)255);

    DRV_SEMA_LOCK;
    ft3dnr_nr_set_strength(nr_str);
    DRV_SEMA_UNLOCK;

    return count;
}

static struct file_operations mrnr_proc_param_ops = {
    .owner  = THIS_MODULE,
    .open   = mrnr_proc_param_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations mrnr_proc_nrstr_ops = {
    .owner  = THIS_MODULE,
    .open   = mrnr_proc_nrstr_open,
    .write  = mrnr_proc_nrstr_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

int ft3dnr_mrnr_proc_init(struct proc_dir_entry *entity_proc)
{
    int ret = 0;

    mrnrproc = create_proc_entry("mrnr", S_IFDIR|S_IRUGO|S_IXUGO, entity_proc);
    if (mrnrproc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    mrnrproc->owner = THIS_MODULE;
#endif

    /* the parameter content of controller register */
    mrnr_paramproc = create_proc_entry("param", S_IRUGO|S_IXUGO, mrnrproc);
    if (mrnr_paramproc == NULL) {
        printk("error to create %s/mrnr/param proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    mrnr_paramproc->proc_fops  = &mrnr_proc_param_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    mrnr_paramproc->owner      = THIS_MODULE;
#endif

    /* the parameter interface of nr_str setting */
    mrnr_nrstrproc = create_proc_entry("nr_str", S_IRUGO|S_IXUGO, mrnrproc);
    if (mrnr_nrstrproc == NULL) {
        printk("error to create %s/mrnr/nr_str proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    mrnr_nrstrproc->proc_fops  = &mrnr_proc_nrstr_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    mrnr_nrstrproc->owner      = THIS_MODULE;
#endif

    return 0;
}

void ft3dnr_mrnr_proc_remove(struct proc_dir_entry *entity_proc)
{
    if (mrnr_nrstrproc != 0)
        remove_proc_entry(mrnr_nrstrproc->name, mrnrproc);
    if (mrnr_paramproc != 0)
        remove_proc_entry(mrnr_paramproc->name, mrnrproc);

    if (mrnrproc != 0)
        remove_proc_entry(mrnrproc->name, mrnrproc->parent);
}

