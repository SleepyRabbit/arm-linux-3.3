/**
 * @file nvp6114a.c
 * VCAP300 NVP6114A Input Device Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2015/08/27 07:52:27 $
 *
 * ChangeLog:
 *  $Log: nvp6114a.c,v $
 *  Revision 1.2  2015/08/27 07:52:27  shawn_hu
 *  1. It seems a chip issue that the output sequence would be reversed when NVP6114A runs in 1-CH bypass mode, e.g. VIN1->VDO2, VIN2->VDO1.
 *     But this setting will cause a mismatched channel mapping, so we do a workaround in frontend driver to reverse the order back.
 *  2. Also fix the "show" function of "norm" & "timeout" proc node in input module for 1-CH bypass mode.
 *
 *  Revision 1.1  2015/06/05 01:35:35  shawn_hu
 *  Add Nextchip NVP6114A front_end driver support.
 *
 *  1. In current design, "2CH 720P + 960H" & "2CH Half-720P60/50" & "4CH Half-720P30/25 AHD-CIF" mode are not supported.
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include "vcap_input.h"
#include "vcap_dbg.h"
#include "nextchip/nvp6114a.h"  ///< from module/include/front_end/ahd

#define DEV_MAX                 NVP6114A_DEV_MAX
#define DEV_VPORT_MAX           NVP6114A_DEV_VPORT_MAX                   ///< VPORT_A, VPORT_B
#define DEFAULT_TYPE            VCAP_INPUT_TYPE_AHD
#define DEFAULT_NORM            VCAP_INPUT_NORM_1080P30
#define DEFAULT_NORM_WIDTH      1920
#define DEFAULT_NORM_HEIGHT     1080
#define DEFAULT_FRAME_RATE      30
#define DEFAULT_VFMT            NVP6114A_VFMT_1080P30
#define DEFAULT_MODE            VCAP_INPUT_MODE_BYPASS
#define DEFAULT_SPEED           VCAP_INPUT_SPEED_P
#define DEFAULT_INTF            VCAP_INPUT_INTF_BT656_PROGRESSIVE
#define DEFAULT_ORDER           VCAP_INPUT_ORDER_ANYONE
#define DEFAULT_DATA_RANGE      VCAP_INPUT_DATA_RANGE_256
#define DEFAULT_CH_ID_MODE      VCAP_INPUT_CH_ID_EAV_SAV
#define DEFAULT_TIMEOUT         ((1000/DEFAULT_FRAME_RATE)*2)

static ushort vport[DEV_MAX] = {0x4321, 0x8765, 0, 0};
module_param_array(vport, short, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vport, "VPort Link => VPortA[0:3], VPortB[4:7], VPortC[8:11], VPortD[12:15], 0:None, 1~8:X_CAP#0~7, 9:X_CAPCAS");

static int mode[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0};
module_param_array(mode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mode, "Video Output Mode=> 0:1CH, 1:2CH, 2:4CH");

static int yc_swap[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0x0000};
module_param_array(yc_swap, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(yc_swap, "0: None, 1:YC Swap, 2:CbCr Swap, 3:YC and CbCr Swap");

static int inv_clk[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0x0000};
module_param_array(inv_clk, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "Invert Clock, 0:Disable, 1: Enable");

struct nvp6114a_dev_t {
    int                     index;                        ///< device index
    struct semaphore        lock;                         ///< device locker
    int                     vfmt[DEV_VPORT_MAX];          ///< record current video format index
    int                     vlos[DEV_VPORT_MAX];          ///< record current video loss status
    int                     frame_rate[DEV_VPORT_MAX];    ///< record current video frame rate

    struct vcap_input_dev_t *port[DEV_VPORT_MAX];
};

static struct nvp6114a_dev_t  nvp6114a_dev[DEV_MAX];
static struct proc_dir_entry *nvp6114a_proc_root[DEV_MAX]    = {[0 ... (DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_norm[DEV_MAX]    = {[0 ... (DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_timeout[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = NULL};

static int nvp6114a_module_get(void)
{
    return try_module_get(THIS_MODULE);
}

static void nvp6114a_module_put(void)
{
    module_put(THIS_MODULE);
}

static int nvp6114a_device_init(int id, int norm)
{
    int i, ret = 0;
    struct vcap_input_dev_t *pinput;

    if((id >= DEV_MAX) || (id >= nvp6114a_get_device_num()))
        return -1;

    /* Update device norm */
    for(i=0; i<DEV_VPORT_MAX; i++) {
        pinput = nvp6114a_dev[id].port[i];
        if(pinput && (pinput->norm.mode != norm)) {
            pinput->norm.mode   = norm;
            pinput->norm.width  = DEFAULT_NORM_WIDTH;
            pinput->norm.height = DEFAULT_NORM_HEIGHT;
            pinput->frame_rate  = pinput->max_frame_rate = DEFAULT_FRAME_RATE;
            pinput->timeout_ms  = DEFAULT_TIMEOUT;
            pinput->speed       = DEFAULT_SPEED;
            pinput->interface   = DEFAULT_INTF;
            pinput->field_order = DEFAULT_ORDER;

            nvp6114a_dev[id].vfmt[i] = DEFAULT_VFMT;
        }
    }

    return ret;
}

static int nvp6114a_proc_norm_show(struct seq_file *sfile, void *v)
{
    int i, j, vport_chnum;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    switch(mode[pdev->index]) {
        case 2:
            vport_chnum = 4;
            break;
        case 1:
            vport_chnum = 2;
            break;
        case 0:
        default:
            vport_chnum = 1;
            break;
    }

    seq_printf(sfile, "[NVP6114A.%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN   VCH   Width   Height\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_VPORT_MAX; i++) {
        if(pdev->port[i]) {
            for(j=0; j<vport_chnum; j++) {
                seq_printf(sfile, "%-5d %-5d %-7d %-7d\n",
                           (i*vport_chnum+j),
                           nvp6114a_get_vch_id(pdev->index, i, j),
                           (pdev->port[i])->norm.width,
                           (pdev->port[i])->norm.height);
            }
        }
        else
            seq_printf(sfile, "%-5s %-5s %-7s %-7s\n", "-", "-", "-", "-");
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_timeout_show(struct seq_file *sfile, void *v)
{
    int i, j, vport_chnum;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    switch(mode[pdev->index]) {
        case 2:
            vport_chnum = 4;
            break;
        case 1:
            vport_chnum = 2;
            break;
        case 0:
        default:
            vport_chnum = 1;
            break;
    }

    seq_printf(sfile, "[NVP6114A.%d]\n", pdev->index);
    seq_printf(sfile, "-------------------\n");
    seq_printf(sfile, "VIN   VCH   Timeout\n");
    seq_printf(sfile, "-------------------\n");
    for(i=0; i<NVP6114A_DEV_VPORT_MAX; i++) {
        if(pdev->port[i]) {
            for(j=0; j<vport_chnum; j++) {
                seq_printf(sfile, "%-5d %-5d %-7d\n",
                           (i*vport_chnum+j),
                           nvp6114a_get_vch_id(pdev->index, i, j),
                           (pdev->port[i])->timeout_ms);
            }
        }
        else
            seq_printf(sfile, "%-5s %-5s %-7s\n", "-", "-", "-");
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_norm_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_norm_show, PDE(inode)->data);
}

static int nvp6114a_proc_timeout_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_timeout_show, PDE(inode)->data);
}

static struct file_operations nvp6114a_proc_norm_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_norm_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_timeout_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_timeout_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void nvp6114a_proc_remove(int id)
{
    if(nvp6114a_proc_root[id]) {
        if(nvp6114a_proc_norm[id])
            vcap_input_proc_remove_entry(nvp6114a_proc_root[id], nvp6114a_proc_norm[id]);

        if(nvp6114a_proc_timeout[id])
            vcap_input_proc_remove_entry(nvp6114a_proc_root[id], nvp6114a_proc_timeout[id]);

        vcap_input_proc_remove_entry(NULL, nvp6114a_proc_root[id]);
    }
}

static int nvp6114a_proc_init(struct nvp6114a_dev_t *pdev)
{
    int ret = 0;
    int id  = pdev->index;
    char name[32];

    /* root */
    sprintf(name, "nvp6114a.%d", id);
    nvp6114a_proc_root[id] = vcap_input_proc_create_entry(name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!nvp6114a_proc_root[id]) {
        vcap_err("create proc node '%s' failed!\n", name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_root[id]->owner = THIS_MODULE;
#endif

    /* norm */
    nvp6114a_proc_norm[id] = vcap_input_proc_create_entry("norm", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_norm[id]) {
        vcap_err("create proc node '%s/norm' failed!\n", name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_norm[id]->proc_fops = &nvp6114a_proc_norm_ops;
    nvp6114a_proc_norm[id]->data      = (void *)pdev;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_norm[id]->owner     = THIS_MODULE;
#endif

    /* timeout */
    nvp6114a_proc_timeout[id] = vcap_input_proc_create_entry("timeout", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_timeout[id]) {
        vcap_err("create proc node '%s/timeout' failed!\n", name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_timeout[id]->proc_fops = &nvp6114a_proc_timeout_ops;
    nvp6114a_proc_timeout[id]->data      = (void *)pdev;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_timeout[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    nvp6114a_proc_remove(id);
    return ret;
}

static int nvp6114a_vlos_notify_handler(int id, int vin, int vlos)
{
    int i, j, vch;
    struct nvp6114a_dev_t *pdev = &nvp6114a_dev[id];

    if((id >= DEV_MAX) || (vin >= NVP6114A_DEV_CH_MAX))
        return -1;

    down(&pdev->lock);

    if(pdev->vlos[vin] != vlos) {
        pdev->vlos[vin] = vlos;

        vch = nvp6114a_vin_to_vch(id, vin);
        for(i=0; i<DEV_VPORT_MAX; i++) {
            if(!pdev->port[i])
                continue;

            for(j=0; j<VCAP_INPUT_DEV_CH_MAX; j++) {
                if((pdev->port[i])->ch_id[j] == vch) {
                    (pdev->port[i])->ch_vlos[j] = vlos;
                    vcap_input_device_notify((pdev->port[i])->vi_idx, j, ((vlos == 0) ? VCAP_INPUT_NOTIFY_SIGNAL_PRESENT : VCAP_INPUT_NOTIFY_SIGNAL_LOSS));
                    goto exit;
                }
            }
        }
    }

exit:
    up(&pdev->lock);

    return 0;
}

static int nvp6114a_vfmt_notify_handler(int id, int ch, struct nvp6114a_video_fmt_t *vfmt)
{
    int fps_div, norm_switch = 0;
    struct vcap_input_dev_t *pinput;
    struct nvp6114a_dev_t *pdev = &nvp6114a_dev[id];

    if(id >= DEV_MAX || (ch >= NVP6114A_DEV_CH_MAX) || !vfmt)
        return -1;

    down(&pdev->lock);

    if(pdev->port[ch]) {
        pinput = pdev->port[ch];

        /* check video format */
        if(pdev->vfmt[ch] != vfmt->fmt_idx) {
            pdev->vfmt[ch] = vfmt->fmt_idx;
            if(vfmt->prog == 0) {   ///< interlace
                fps_div = 2;
                pinput->interface = VCAP_INPUT_INTF_BT656_INTERLACE;

                if(pinput->speed != VCAP_INPUT_SPEED_I) {
                    pinput->speed       = VCAP_INPUT_SPEED_I;
                    pinput->field_order = VCAP_INPUT_ORDER_ODD_FIRST;
                }
            }
            else {
                fps_div = 1;
                pinput->interface = VCAP_INPUT_INTF_BT656_PROGRESSIVE;

                if(vfmt->frame_rate == 50 || vfmt->frame_rate == 60) {
                    if(pinput->speed != VCAP_INPUT_SPEED_2P) {
                        pinput->speed       = VCAP_INPUT_SPEED_2P;
                        pinput->field_order = VCAP_INPUT_ORDER_ANYONE;
                    }
                }
                else {
                    if(pinput->speed != VCAP_INPUT_SPEED_P) {
                        pinput->speed       = VCAP_INPUT_SPEED_P;
                        pinput->field_order = VCAP_INPUT_ORDER_ANYONE;
                    }
                }
            }

            if((vfmt->width > 0) && (vfmt->width != pinput->norm.width)) {
                pinput->norm.width = vfmt->width;
                norm_switch++;
            }

            if((vfmt->height > 0) && (vfmt->height != pinput->norm.height)) {
                pinput->norm.height = vfmt->height;
                norm_switch++;
            }

            if(norm_switch) {
                pinput->norm.mode++;    ///< update mode to inform norm switched
                vcap_input_device_notify(pinput->vi_idx, 0, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }

            /* check frame rate */
            if(((vfmt->frame_rate/fps_div) > 0) && (pdev->frame_rate[ch] != (vfmt->frame_rate/fps_div))) {
                pdev->frame_rate[ch] = pinput->frame_rate = pinput->max_frame_rate = vfmt->frame_rate/fps_div;
                pinput->timeout_ms = (1000/pinput->frame_rate)*2;   ///< base on current frame rate
                vcap_input_device_notify(pinput->vi_idx, 0, VCAP_INPUT_NOTIFY_FRAMERATE_CHANGE);
            }
        }
    }

    up(&pdev->lock);

    return 0;
}

static int __init nvp6114a_input_init(void)
{
    int i, j;
    int dev_num;
    struct vcap_input_dev_t *pinput;
    int ret = 0;

    if(vcap_input_get_version() != VCAP_INPUT_VERSION) {
        vcap_err("Input driver version(%08x) is not compatibility with vcap300_common.ko(%08x)!\n",
                 vcap_input_get_version(), VCAP_INPUT_VERSION);
        return -EFAULT;
    }

    /* clear buffer */
    memset(nvp6114a_dev, 0, sizeof(struct nvp6114a_dev_t)*DEV_MAX);

    /* Get NVP6114A Device Number */
    dev_num = nvp6114a_get_device_num();

    for(i=0; i<DEV_MAX; i++) {
        nvp6114a_dev[i].index = i;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&nvp6114a_dev[i].lock, 1);
#else
        init_MUTEX(&nvp6114a_dev[i].lock);
#endif
        if(i >= dev_num)
            continue;

        if(vport[i]) {
            /* register input device */
            for(j=0; j<DEV_VPORT_MAX; j++) {
                if((vport[i]>>(4*j)) & 0xf) {   ///< VPort# Connect to X_CAP#
                    pinput = nvp6114a_dev[i].port[j] = kzalloc(sizeof(struct vcap_input_dev_t), GFP_KERNEL);
                    if(pinput == NULL) {
                        vcap_err("NVP6114A#%d.%d allocate vcap_input_dev_t failed\n", i, j);
                        ret = -ENOMEM;
                        goto err;
                    }

                    /* input device name */
                    snprintf(pinput->name, VCAP_INPUT_NAME_SIZE-1, "nvp6114a.%d.%d", i, j);

                    /* input device parameter setup */
                    pinput->index       = (i*DEV_VPORT_MAX) + j;
                    pinput->vi_idx      = ((vport[i]>>(4*j)) & 0xf) - 1; ///< VI#
                    pinput->vi_src      = ((vport[i]>>(4*j)) & 0xf) - 1; ///< X_CAP#
                    pinput->type        = DEFAULT_TYPE;
                    pinput->data_range  = DEFAULT_DATA_RANGE;
                    pinput->yc_swap     = 0;
                    pinput->data_swap   = 0;
                    pinput->ch_id_mode  = DEFAULT_CH_ID_MODE;
                    pinput->inv_clk     = (inv_clk[i]>>(4*j)) & 0x1;
                    pinput->init        = nvp6114a_device_init;
                    pinput->norm.mode   = -1;                            ///< init value
                    pinput->module_get  = nvp6114a_module_get;
                    pinput->module_put  = nvp6114a_module_put;

                    switch(mode[i]) {
                        case 0:  /* 74.25 or 148.5 MHz ==> 1CH */
                            pinput->mode     = VCAP_INPUT_MODE_BYPASS;
                            pinput->ch_id[0] = nvp6114a_get_vch_id(i, j, 0);
                            pinput->ch_id[1] = -1;
                            pinput->ch_id[2] = -1;
                            pinput->ch_id[3] = -1;
                            break;
                        case 1:  /* 72MHz ==> 2CH */
                            pinput->mode     = VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE;
                            pinput->ch_id[0] = nvp6114a_get_vch_id(i, j, 0);
                            pinput->ch_id[2] = nvp6114a_get_vch_id(i, j, 1);
                            break;
                        default: /* 144MHz ==> 4CH */
                            pinput->mode     = VCAP_INPUT_MODE_4CH_BYTE_INTERLEAVE;
                            pinput->ch_id[0] = nvp6114a_get_vch_id(i, j, 0);
                            pinput->ch_id[1] = nvp6114a_get_vch_id(i, j, 1);
                            pinput->ch_id[2] = nvp6114a_get_vch_id(i, j, 2);
                            pinput->ch_id[3] = nvp6114a_get_vch_id(i, j, 3);
                            break;
                    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
                    sema_init(&pinput->sema_lock, 1);
#else
                    init_MUTEX(&pinput->sema_lock);
#endif
                    ret = vcap_input_device_register(pinput);
                    if(ret < 0) {
                        vcap_err("register NVP6114A#%d.%d input device failed\n", i, j);
                        goto err;
                    }
                }
            }

            /* device proc init */
            ret = nvp6114a_proc_init(&nvp6114a_dev[i]);
            if(ret < 0)
                goto err;

            /* device init */
            ret = nvp6114a_device_init(i, DEFAULT_NORM);
            if(ret < 0)
                goto err;

            /* register nvp6114a video loss notify handler */
            nvp6114a_notify_vlos_register(i, nvp6114a_vlos_notify_handler);

            /* register nvp6114a video format notify handler */
            nvp6114a_notify_vfmt_register(i, nvp6114a_vfmt_notify_handler);

            vcap_info("Register NVP6114A#%d Input Device\n", i);
        }
    }

    return ret;

err:
    for(i=0; i<DEV_MAX; i++) {
        nvp6114a_notify_vlos_deregister(i);
        nvp6114a_proc_remove(i);
        for(j=0; j<DEV_VPORT_MAX; j++) {
            if(nvp6114a_dev[i].port[j]) {
                vcap_input_device_unregister(nvp6114a_dev[i].port[j]);
                kfree(nvp6114a_dev[i].port[j]);
            }
        }
    }
    return ret;
}

static void __exit nvp6114a_input_exit(void)
{
    int i, j;

    for(i=0; i<DEV_MAX; i++) {
        nvp6114a_notify_vlos_deregister(i);
        nvp6114a_notify_vfmt_deregister(i);
        nvp6114a_proc_remove(i);
        for(j=0; j<DEV_VPORT_MAX; j++) {
            if(nvp6114a_dev[i].port[j]) {
                vcap_input_device_unregister(nvp6114a_dev[i].port[j]);
                kfree(nvp6114a_dev[i].port[j]);
            }
        }
    }
}

module_init(nvp6114a_input_init);
module_exit(nvp6114a_input_exit);

MODULE_DESCRIPTION("Grain Media Video Capture300 NVP6114A Input Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
