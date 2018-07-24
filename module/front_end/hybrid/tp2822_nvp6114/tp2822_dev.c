/**
 * @file tp2822_dev.c
 * TechPoint TP2822 4CH HD-TVI Video Decoder Driver (for hybrid design)
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2015/05/04 12:50:08 $
 *
 * ChangeLog:
 *  $Log: tp2822_dev.c,v $
 *  Revision 1.1  2015/05/04 12:50:08  shawn_hu
 *  Add 4-CH TVI TP2822 + AHD NVP6114 (2-CH dual edge mode) hybrid frontend driver support for HY GM8286 design.
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "platform.h"
#include "techpoint/tp2822.h"                       ///< from module/include/front_end/hdtvi
#include "tp2822_dev.h"
#include "tp2822_nvp6114_hybrid_drv.h"

/*************************************************************************************
 *  Initial values of the following structures come from tp2822_nvp6114_hybrid_drv.c
 *************************************************************************************/
static int dev_num;
static ushort iaddr[TP2822_DEV_MAX];
static int vout_format[TP2822_DEV_MAX];
static int vout_mode[TP2822_DEV_MAX];
static struct tp2822_dev_t* tp2822_dev;
static int init;
static u32 TP2822_NVP6114_VCH_MAP[TP2822_DEV_MAX*TP2822_DEV_CH_MAX];

/*************************************************************************************
 *  Original structure
 *************************************************************************************/
static struct proc_dir_entry *tp2822_proc_root[TP2822_DEV_MAX]      = {[0 ... (TP2822_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2822_proc_status[TP2822_DEV_MAX]    = {[0 ... (TP2822_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2822_proc_video_fmt[TP2822_DEV_MAX] = {[0 ... (TP2822_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2822_proc_vout_fmt[TP2822_DEV_MAX]  = {[0 ... (TP2822_DEV_MAX - 1)] = NULL};

/*************************************************************************************
 *  Revised for hybrid design
 *************************************************************************************/
int tp2822_i2c_write(u8 id, u8 reg, u8 data)
{
    return tp2822_nvp6114_i2c_write(iaddr[id], reg, data);
}
EXPORT_SYMBOL(tp2822_i2c_write);

u8 tp2822_i2c_read(u8 id, u8 reg)
{
    return tp2822_nvp6114_i2c_read(iaddr[id], reg);
}
EXPORT_SYMBOL(tp2822_i2c_read);

void tp2822_set_params(tp2822_params_p params_p)
{
    int i = 0;

    dev_num = params_p->tp2822_dev_num;
    for(i=0; i<dev_num; i++) {
        iaddr[i] = *(params_p->tp2822_iaddr + i);
    }
    for(i=0; i<TP2822_DEV_MAX; i++) {
        vout_format[i] = *(params_p->tp2822_vout_format + i);
    }
    for(i=0; i<TP2822_DEV_MAX; i++) {
        vout_mode[i] = *(params_p->tp2822_vout_mode + i);
    }
    tp2822_dev = params_p->tp2822_dev;
    init = params_p->init;
    for(i=0; i<TP2822_DEV_MAX*TP2822_DEV_CH_MAX; i++) {
        TP2822_NVP6114_VCH_MAP[i] = *(params_p->TP2822_NVP6114_VCH_MAP + i);
    }
}   

/*
 * seq_id: channel sequence number in video output port,
 *         for 1CH_BYPASS    mode vout_seq = 0
 *         for 2CH_DUAL_EDGE mode vout_seq = 0 or 1
*/
int tp2822_get_vch_id(int id, TP2822_DEV_VOUT_T vout, int seq_id)
{
    int i;
    int vport_chnum;

    if(id > dev_num || vout >= TP2822_DEV_VOUT_MAX || seq_id >= 2) ///< For hybrid design
        return 0;

    if(vout_mode[id] == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(tp2822_get_device_num()*TP2822_DEV_CH_MAX); i++) { ///< For hybrid design
        if((CH_IDENTIFY_ID(TP2822_NVP6114_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VOUT(TP2822_NVP6114_VCH_MAP[i]) == vout) &&
           ((CH_IDENTIFY_VIN(TP2822_NVP6114_VCH_MAP[i])%vport_chnum) == seq_id)) {
            return CH_IDENTIFY_VIN(TP2822_NVP6114_VCH_MAP[i]); ///< For hybrid design
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2822_get_vch_id);

int tp2822_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= TP2822_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*TP2822_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2822_NVP6114_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2822_NVP6114_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(tp2822_vin_to_vch);

/*************************************************************************************
 *  Keep as original design (only XXXX_VCH_MAP was updated)
 *************************************************************************************/

int tp2822_get_vout_id(int id, int vin_idx)
{
    int i;

    if(id > dev_num || vin_idx >= TP2822_DEV_CH_MAX) ///< For hybrid design
        return 0;

    for(i=0; i<(tp2822_get_device_num()*TP2822_DEV_CH_MAX); i++) { ///< For hybrid design
        if((CH_IDENTIFY_ID(TP2822_NVP6114_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2822_NVP6114_VCH_MAP[i]) == vin_idx))
            return CH_IDENTIFY_VOUT(TP2822_NVP6114_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(tp2822_get_vout_id);

int tp2822_get_vout_seq_id(int id, int vin_idx)
{
    int i;
    int vport_chnum;

    if(id > dev_num || vin_idx >= TP2822_DEV_CH_MAX)
        return 0;

    if(vout_mode[id] == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(tp2822_get_device_num()*TP2822_DEV_CH_MAX); i++) { ///< For hybrid design
        if((CH_IDENTIFY_ID(TP2822_NVP6114_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2822_NVP6114_VCH_MAP[i]) == vin_idx)) {
            return (vin_idx%vport_chnum);
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2822_get_vout_seq_id);

int tp2822_get_vout_format(int id)
{
    if(id >= dev_num)
        return TP2822_VOUT_FORMAT_BT656;
    else
        return vout_format[id];
}
EXPORT_SYMBOL(tp2822_get_vout_format);

int tp2822_get_vout_mode(int id)
{
    if(id > dev_num) ///< For hybrid design 
        return TP2822_VOUT_MODE_1CH_BYPASS;
    else {
        if(id == dev_num)
            return vout_mode[id-1];
        else
            return vout_mode[id];
    }
}
EXPORT_SYMBOL(tp2822_get_vout_mode);

int tp2822_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2822_video_fmt_t *))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2822_dev[id].lock);

    tp2822_dev[id].vfmt_notify = nt_func;

    up(&tp2822_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2822_notify_vfmt_register);

void tp2822_notify_vfmt_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2822_dev[id].lock);

    tp2822_dev[id].vfmt_notify = NULL;

    up(&tp2822_dev[id].lock);
}
EXPORT_SYMBOL(tp2822_notify_vfmt_deregister);

int tp2822_notify_vlos_register(int id, int (*nt_func)(int, int, int))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2822_dev[id].lock);

    tp2822_dev[id].vlos_notify = nt_func;

    up(&tp2822_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2822_notify_vlos_register);

void tp2822_notify_vlos_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2822_dev[id].lock);

    tp2822_dev[id].vlos_notify = NULL;

    up(&tp2822_dev[id].lock);
}
EXPORT_SYMBOL(tp2822_notify_vlos_deregister);

static int tp2822_proc_status_show(struct seq_file *sfile, void *v)
{
    int i;
    int vlos;
    struct tp2822_dev_t *pdev = (struct tp2822_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2822#%d]\n", pdev->index);
    seq_printf(sfile, "------------------\n");
    seq_printf(sfile, "VIN    VCH    LOS \n");
    seq_printf(sfile, "==================\n");
    for(i=0; i<TP2822_DEV_CH_MAX; i++) {
        vlos = tp2822_status_get_video_loss(pdev->index, i);
        seq_printf(sfile, "%-6d %-6d %-7s\n", i, tp2822_vin_to_vch(pdev->index, i), ((vlos == 0) ? "NO" : "YES"));
    }
    up(&pdev->lock);

    return 0;
}

static int tp2822_proc_video_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret, vlos;
    struct tp2822_video_fmt_t vfmt;
    struct tp2822_dev_t *pdev = (struct tp2822_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2822#%d] Incoming Video Fromat\n", pdev->index);
    seq_printf(sfile, "====================================================\n");
    seq_printf(sfile, "VIN   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "====================================================\n");

    for(i=0; i<TP2822_DEV_CH_MAX; i++) {
        vlos = tp2822_status_get_video_loss(pdev->index, i);
        if(vlos == 0)
            ret = tp2822_status_get_video_input_format(pdev->index, i, &vfmt);
        else
            ret = -1; ///< Unknown video input format when video loss
        if((ret == 0) && ((vfmt.fmt_idx >= TP2822_VFMT_720P60) && (vfmt.fmt_idx < TP2822_VFMT_MAX))) {
            seq_printf(sfile, "%-5d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2822_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-5d %-5d %-7s %-8s %-12s %-11s\n", i, tp2822_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2822_proc_vout_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret;
    struct tp2822_video_fmt_t vfmt;
    struct tp2822_dev_t *pdev = (struct tp2822_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2822#%d] Video Output Fromat\n", pdev->index);
    seq_printf(sfile, "=====================================================\n");
    seq_printf(sfile, "VOUT   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "=====================================================\n");

    for(i=0; i<TP2822_DEV_CH_MAX; i++) {
        ret = tp2822_video_get_video_output_format(pdev->index, i, &vfmt);
        if((ret == 0) && ((vfmt.fmt_idx >= TP2822_VFMT_720P60) && (vfmt.fmt_idx < TP2822_VFMT_MAX))) {
            seq_printf(sfile, "%-6d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2822_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-6d %-5d %-7s %-8s %-12s %-11s\n", i, tp2822_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2822_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2822_proc_status_show, PDE(inode)->data);
}

static int tp2822_proc_video_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2822_proc_video_fmt_show, PDE(inode)->data);
}

static int tp2822_proc_vout_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2822_proc_vout_fmt_show, PDE(inode)->data);
}

static struct file_operations tp2822_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2822_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2822_proc_video_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2822_proc_video_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2822_proc_vout_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2822_proc_vout_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

void tp2822_proc_remove(int id)
{
    if(id >= TP2822_DEV_MAX)
        return;

    if(tp2822_proc_root[id]) {
        if(tp2822_proc_status[id]) {
            remove_proc_entry(tp2822_proc_status[id]->name, tp2822_proc_root[id]);
            tp2822_proc_status[id] = NULL;
        }

        if(tp2822_proc_video_fmt[id]) {
            remove_proc_entry(tp2822_proc_video_fmt[id]->name, tp2822_proc_root[id]);
            tp2822_proc_video_fmt[id] = NULL;
        }

        if(tp2822_proc_vout_fmt[id]) {
            remove_proc_entry(tp2822_proc_vout_fmt[id]->name, tp2822_proc_root[id]);
            tp2822_proc_vout_fmt[id] = NULL;
        }

        remove_proc_entry(tp2822_proc_root[id]->name, NULL);
        tp2822_proc_root[id] = NULL;
    }
}

int tp2822_proc_init(int id)
{
    int ret = 0;

    if(id >= TP2822_DEV_MAX)
        return -1;

    /* root */
    tp2822_proc_root[id] = create_proc_entry(tp2822_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!tp2822_proc_root[id]) {
        printk("create proc node '%s' failed!\n", tp2822_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2822_proc_root[id]->owner = THIS_MODULE;
#endif

    /* status */
    tp2822_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, tp2822_proc_root[id]);
    if(!tp2822_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", tp2822_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2822_proc_status[id]->proc_fops = &tp2822_proc_status_ops;
    tp2822_proc_status[id]->data      = (void *)&tp2822_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2822_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* frame format */
    tp2822_proc_video_fmt[id] = create_proc_entry("video_format", S_IRUGO|S_IXUGO, tp2822_proc_root[id]);
    if(!tp2822_proc_video_fmt[id]) {
        printk("create proc node '%s/video_format' failed!\n", tp2822_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2822_proc_video_fmt[id]->proc_fops = &tp2822_proc_video_fmt_ops;
    tp2822_proc_video_fmt[id]->data      = (void *)&tp2822_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2822_proc_video_fmt[id]->owner     = THIS_MODULE;
#endif

    /* video output format */
    tp2822_proc_vout_fmt[id] = create_proc_entry("vout_format", S_IRUGO|S_IXUGO, tp2822_proc_root[id]);
    if(!tp2822_proc_vout_fmt[id]) {
        printk("create proc node '%s/vout_format' failed!\n", tp2822_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2822_proc_vout_fmt[id]->proc_fops = &tp2822_proc_vout_fmt_ops;
    tp2822_proc_vout_fmt[id]->data      = (void *)&tp2822_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2822_proc_vout_fmt[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    tp2822_proc_remove(id);
    return ret;
}

static int tp2822_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct tp2822_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(tp2822_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &tp2822_dev[i];
            break;
        }
    }

    if(!pdev) {
        ret = -EINVAL;
        goto exit;
    }

    file->private_data = (void *)pdev;

exit:
    return ret;
}

static int tp2822_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long tp2822_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct tp2822_dev_t *pdev = (struct tp2822_dev_t *)file->private_data;

    down(&pdev->lock);

    if(_IOC_TYPE(cmd) != TP2822_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case TP2822_GET_NOVID:
            {
                struct tp2822_ioc_data_t ioc_data;

                if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_data.vin_ch >= TP2822_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_data.data = tp2822_status_get_video_loss(pdev->index, ioc_data.vin_ch);

                ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            }
            break;

        case TP2822_GET_VIDEO_FMT:
            {
                struct tp2822_ioc_vfmt_t ioc_vfmt;
                struct tp2822_video_fmt_t vfmt;

                if(copy_from_user(&ioc_vfmt, (void __user *)arg, sizeof(ioc_vfmt))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vfmt.vin_ch >= TP2822_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ret = tp2822_status_get_video_input_format(pdev->index, ioc_vfmt.vin_ch, &vfmt);
                if(ret < 0) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_vfmt.width      = vfmt.width;
                ioc_vfmt.height     = vfmt.height;
                ioc_vfmt.prog       = vfmt.prog;
                ioc_vfmt.frame_rate = vfmt.frame_rate;

                ret = (copy_to_user((void __user *)arg, &ioc_vfmt, sizeof(ioc_vfmt))) ? (-EFAULT) : 0;
            }
            break;

        default:
            ret = -ENOIOCTLCMD;
            break;
    }

exit:
    up(&pdev->lock);
    return ret;
}

static struct file_operations tp2822_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = tp2822_miscdev_open,
    .release        = tp2822_miscdev_release,
    .unlocked_ioctl = tp2822_miscdev_ioctl,
};

int tp2822_miscdev_init(int id)
{
    int ret;

    if(id >= TP2822_DEV_MAX)
        return -1;

    /* clear */
    memset(&tp2822_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    tp2822_dev[id].miscdev.name  = tp2822_dev[id].name;
    tp2822_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    tp2822_dev[id].miscdev.fops  = (struct file_operations *)&tp2822_miscdev_fops;
    ret = misc_register(&tp2822_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", tp2822_dev[id].name);
        tp2822_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

int tp2822_device_init(int id)
{
    int ret = 0;

    if(id >= TP2822_DEV_MAX)
        return -1;

    if(!init)
        goto exit;

    /* video init */
    ret = tp2822_video_init(id, vout_format[id], vout_mode[id]);
    if(ret < 0)
        goto exit;

    /* audio init */
    /* ToDo */

exit:
    printk("TP2822#%d Init...%s!\n", id, ((ret == 0) ? "OK" : "Fail"));

    return ret;
}

