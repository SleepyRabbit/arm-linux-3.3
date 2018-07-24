/**
 * @file tp2823.c
 * VCAP300 TP2823 Input Device Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.3 $
 * $Date: 2015/07/20 11:49:27 $
 *
 * ChangeLog:
 *  $Log: tp2823.c,v $
 *  Revision 1.3  2015/07/20 11:49:27  shawn_hu
 *  Remove inappropriate VI height setting in tp2823_vfmt_notify_handler().
 *
 *  Revision 1.2  2015/07/02 06:43:57  shawn_hu
 *  During 2-CH dual edge mode:
 *  1. Let 960H output have higher priority, now 960H can output if there is only one 960H camera input on a BT656 port.
 *    a. If the format of another channel on the same BT656 port isn't 960H, force it to blue screen pattern output.
 *    b. If another channel input isn't present, force the black image output to 960H25 (1920*576).
 *  2. Fix a potential bug on `debounce` variable (it may overflow) in watchdog_thread() of tp2823_drv.c.
 *  3. Add "tp2823_video_set_pattern_output()" API in tp2823_lib.c for enabling/disabling blue screen pattern output.
 *
 *  Revision 1.1  2015/06/29 06:00:23  shawn_hu
 *  Add Techpoint TP2823 driver support (Video part).
 *  1. When TP2823 runs in 2-CH dual edge mode, the TVI V1 1080P30(half-1080P)/720P50(half-720P) can't be mixed with TVI V2 720P (not half mode)
 *     camera input into the same BT656 port. In current driver design, only TVI V2 720P can output if V1 1080P30/720P50 and V2 720P input are mixed.
 *
 *  2. The resolution of TP2823 960H video output will be (1920*480 or 1920*576), and it can be scaling down to 960H via Videograph.
 *
 *  3. In current driver design in 2-CH dual edge mode, the 960H video can output only when both 960H inputs of a BT656 port are present.
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "vcap_input.h"
#include "vcap_dbg.h"
#include "techpoint/tp2823.h"   ///< from module/include/front_end/hdtvi

#define DEV_MAX                 TP2823_DEV_MAX
#define DEV_VPORT_MAX           TP2823_DEV_VOUT_MAX    ///< VD#1, VD#2
#define DEV_CH_MAX              TP2823_DEV_CH_MAX
#define DEFAULT_TYPE            VCAP_INPUT_TYPE_TVI
#define DEFAULT_NORM_WIDTH      960
#define DEFAULT_NORM_HEIGHT     720
#define DEFAULT_FRAME_RATE      30
#define DEFAULT_SPEED           VCAP_INPUT_SPEED_P
#define DEFAULT_ORDER           VCAP_INPUT_ORDER_ANYONE
#define DEFAULT_DATA_RANGE      VCAP_INPUT_DATA_RANGE_256
#define DEFAULT_TIMEOUT         ((1000/DEFAULT_FRAME_RATE)*2)

static ushort vport[DEV_MAX] = {0x1234, 0x5678, 0, 0};
module_param_array(vport, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vport, "VPort Link => VOUT0[0:3], VOUT1[4:7], VOUT2[8:11], VOUT3[12:15], 0:None, 1~8:X_CAP#0~7, 9:X_CAPCAS");

static int yc_swap[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0x0000};
module_param_array(yc_swap, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(yc_swap, "0: None, 1:YC Swap, 2:CbCr Swap, 3:YC and CbCr Swap");

static int inv_clk[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0x0000};
module_param_array(inv_clk, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "Invert Clock, 0:Disable, 1: Enable");

static int data_swap[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = 0};
module_param_array(data_swap, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(data_swap, "0: None, 1:Lo8Bit, 2:Byte, 3:Lo8Bit+Byte");

struct tp2823_dev_t {
    int                     index;                        ///< device index
    struct semaphore        lock;                         ///< device locker
    int                     vfmt[DEV_CH_MAX];             ///< record current video format index
    int                     vlos[DEV_CH_MAX];             ///< record current video loss status
    int                     frame_rate[DEV_CH_MAX];       ///< record current video frame rate

    struct vcap_input_dev_t *port[DEV_VPORT_MAX];
};

static struct tp2823_dev_t    tp2823_dev[DEV_MAX];
static struct proc_dir_entry *tp2823_proc_root[DEV_MAX]    = {[0 ... (DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_norm[DEV_MAX]    = {[0 ... (DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_timeout[DEV_MAX] = {[0 ... (DEV_MAX - 1)] = NULL};

static int tp2823_module_get(void)
{
    return try_module_get(THIS_MODULE);
}

static void tp2823_module_put(void)
{
    module_put(THIS_MODULE);
}

static int tp2823_vfmt_notify_handler(int id, int ch, struct tp2823_video_fmt_t *vfmt)
{
    int vout, dev_ch;
    int norm_switch = 0;
    struct vcap_input_dev_t *pinput = NULL;
    struct tp2823_dev_t     *pdev   = &tp2823_dev[id];

    if((id >= DEV_MAX) || (ch >= DEV_CH_MAX) || !vfmt)
        return -1;

    down(&pdev->lock);

    vout = tp2823_get_vout_id(pdev->index, ch);
    if(vout >= DEV_VPORT_MAX)
        goto exit;

    pinput = pdev->port[vout];

    /* check video format */
    if(pdev->vfmt[ch] != vfmt->fmt_idx) {
        pdev->vfmt[ch] = vfmt->fmt_idx;
        if(pinput->mode == VCAP_INPUT_MODE_BYPASS) {
            dev_ch = 0;
            if(vfmt->prog == 0) {   ///< interlace
                switch(tp2823_get_vout_format(pdev->index)) {
                    case TP2823_VOUT_FORMAT_BT1120:
                        pinput->interface = VCAP_INPUT_INTF_BT1120_INTERLACE;
                        norm_switch++;
                        break;
                    case TP2823_VOUT_FORMAT_BT656:
                        pinput->interface = VCAP_INPUT_INTF_BT656_INTERLACE;
                        norm_switch++;
                        break;
                    default:
                        break;
                }

                if(pinput->speed != VCAP_INPUT_SPEED_I) {
                    pinput->speed       = VCAP_INPUT_SPEED_I;
                    pinput->field_order = VCAP_INPUT_ORDER_ODD_FIRST;
                }
            }
            else {
                switch(tp2823_get_vout_format(pdev->index)) {
                    case TP2823_VOUT_FORMAT_BT1120:
                        pinput->interface = VCAP_INPUT_INTF_BT1120_PROGRESSIVE;
                        norm_switch++;
                        break;
                    case TP2823_VOUT_FORMAT_BT656:
                        pinput->interface = VCAP_INPUT_INTF_BT656_PROGRESSIVE;
                        norm_switch++;
                        break;
                    default:
                        break;
                }

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
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }

            /* check frame rate */
            if((vfmt->frame_rate > 0) && (pdev->frame_rate[ch] != vfmt->frame_rate)) {
                pdev->frame_rate[ch] = pinput->frame_rate = pinput->max_frame_rate = vfmt->frame_rate;
                pinput->timeout_ms = (1000/pinput->frame_rate)*2;   ///< base on current frame rate
                if(pinput->timeout_ms < 40)
                    pinput->timeout_ms = 40;
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_FRAMERATE_CHANGE);
            }

            /* update channel parameter */
            pinput->ch_param[dev_ch].mode       = pinput->norm.mode;
            pinput->ch_param[dev_ch].width      = pinput->norm.width;
            pinput->ch_param[dev_ch].height     = pinput->norm.height;
            pinput->ch_param[dev_ch].prog       = vfmt->prog;
            pinput->ch_param[dev_ch].frame_rate = pinput->frame_rate;
            pinput->ch_param[dev_ch].timeout_ms = pinput->timeout_ms;
            pinput->ch_param[dev_ch].speed      = pinput->speed;

            /* Update VI setting for 960H input format */
            if((vfmt->fmt_idx == TP2823_VFMT_960H30) || (vfmt->fmt_idx == TP2823_VFMT_960H25)) {
                if(pinput->norm.width != 1920) {
                    pinput->norm.width = 1920;
                    pinput->norm.mode++;    ///< update mode to inform norm switched
                    vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
                }
            }
        }
        else {  ///< 2CH Dual Edge Mode
            dev_ch = tp2823_get_vout_seq_id(pdev->index, ch)*2;  ///< vout_seq_id would be 0 and 1, the input device channel use 0 and 2 for 2ch dual edge mode
            if(dev_ch >= VCAP_INPUT_DEV_CH_MAX)
                goto exit;

            if(pinput->ch_param[dev_ch].prog != vfmt->prog) {
                pinput->ch_param[dev_ch].prog = vfmt->prog;
                norm_switch++;
            }

            if((vfmt->width > 0) && (vfmt->width != pinput->ch_param[dev_ch].width)) {
                pinput->ch_param[dev_ch].width      = vfmt->width;
                norm_switch++;
            }

            if((vfmt->height > 0) && (vfmt->height != pinput->ch_param[dev_ch].height)) {
                pinput->ch_param[dev_ch].height     = vfmt->height;
                norm_switch++;
            }

            /* check speed */
            if(vfmt->prog) {
                if(vfmt->frame_rate == 50 || vfmt->frame_rate == 60)
                    pinput->ch_param[dev_ch].speed = VCAP_INPUT_SPEED_2P;
                else
                    pinput->ch_param[dev_ch].speed = VCAP_INPUT_SPEED_P;
            }
            else {
                pinput->ch_param[dev_ch].speed = VCAP_INPUT_SPEED_I;
            }

            /* check norm */
            if(norm_switch) {
                pinput->ch_param[dev_ch].mode++;    ///< update mode to inform norm switched
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }

            /* check frame rate */
            if((vfmt->frame_rate > 0) && (pdev->frame_rate[ch] != vfmt->frame_rate)) {
                pdev->frame_rate[ch] = pinput->ch_param[dev_ch].frame_rate = vfmt->frame_rate;
                pinput->ch_param[dev_ch].timeout_ms = (1000/vfmt->frame_rate)*2;   ///< base on current frame rate
                if(pinput->ch_param[dev_ch].timeout_ms < 40)
                    pinput->ch_param[dev_ch].timeout_ms = 40;
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_FRAMERATE_CHANGE);
            }

        }
    }

    if(pinput->mode != VCAP_INPUT_MODE_BYPASS) { ///< 2CH dual edge mode 
        /* Workaround for V1 1080P30/720P50/960H output */
        dev_ch = tp2823_get_vout_seq_id(pdev->index, ch)*2;
        if((vfmt->fmt_idx == TP2823_VFMT_720P30) || (vfmt->fmt_idx == TP2823_VFMT_720P25) || ((pinput->ch_param[dev_ch==0?2:0].width == 1280))) {
            if(pinput->norm.width != 1280) {
                pinput->norm.width = 1280;
                pinput->interface = VCAP_INPUT_INTF_BT656_PROGRESSIVE;
                pinput->speed       = VCAP_INPUT_SPEED_P;
                pinput->field_order = VCAP_INPUT_ORDER_ANYONE;
                pinput->norm.mode++;    ///< update mode to inform norm switched
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }
        }
        else if((vfmt->fmt_idx == TP2823_VFMT_960H30) || (vfmt->fmt_idx == TP2823_VFMT_960H25)) {
            if(pinput->norm.width != 1920) { ///< Let 960H output have higher priority
                pinput->norm.width = 1920;
                pinput->interface = VCAP_INPUT_INTF_BT656_INTERLACE;
                pinput->speed       = VCAP_INPUT_SPEED_I;
                pinput->field_order = VCAP_INPUT_ORDER_ODD_FIRST;
                pinput->norm.mode++;    ///< update mode to inform norm switched
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }
        }
        else {
            if((pinput->norm.width != DEFAULT_NORM_WIDTH) && (pinput->ch_param[dev_ch==0?2:0].width != 1920)) {
                pinput->norm.width = DEFAULT_NORM_WIDTH;
                pinput->interface = VCAP_INPUT_INTF_BT656_PROGRESSIVE;
                pinput->speed       = VCAP_INPUT_SPEED_P;
                pinput->field_order = VCAP_INPUT_ORDER_ANYONE;
                pinput->norm.mode++;    ///< update mode to inform norm switched
                vcap_input_device_notify(pinput->vi_idx, dev_ch, VCAP_INPUT_NOTIFY_NORM_CHANGE);
            }
        }
    }

exit:
    up(&pdev->lock);

    return 0;
}

static int tp2823_vlos_notify_handler(int id, int ch, int vlos)
{
    int vout, dev_ch;
    struct tp2823_dev_t *pdev = &tp2823_dev[id];

    if((id >= DEV_MAX) || (ch >= TP2823_DEV_CH_MAX))
        return -1;

    down(&pdev->lock);

    if(pdev->vlos[ch] != vlos) {
        pdev->vlos[ch] = vlos;

        vout = tp2823_get_vout_id(id, ch);
        if((vout < DEV_VPORT_MAX) && pdev->port[vout]) {
            if((pdev->port[vout])->mode == VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID)
                dev_ch = tp2823_get_vout_seq_id(pdev->index, ch)*2;  ///< vout_seq_id would be 0 and 1, the input device channel use 0 and 2 for 2ch dual edge mode
            else
                dev_ch = tp2823_get_vout_seq_id(pdev->index, ch);

            if(dev_ch < VCAP_INPUT_DEV_CH_MAX) {
                (pdev->port[vout])->ch_vlos[dev_ch] = vlos;
                vcap_input_device_notify((pdev->port[vout])->vi_idx, dev_ch, ((vlos == 0) ? VCAP_INPUT_NOTIFY_SIGNAL_PRESENT : VCAP_INPUT_NOTIFY_SIGNAL_LOSS));
            }
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2823_device_init(int id, int norm)
{
    int i, j, ret = 0;
    struct vcap_input_dev_t *pinput;

    if((id >= DEV_MAX) || (id >= tp2823_get_device_num()))
        return -1;

    /* Update input device norm */
    for(i=0; i<DEV_VPORT_MAX; i++) {
        pinput = tp2823_dev[id].port[i];
        if(!pinput)
            continue;

        /* vi parameter */
        if(pinput->norm.mode != norm) {
            pinput->norm.mode   = norm;
            pinput->norm.width  = DEFAULT_NORM_WIDTH;
            pinput->norm.height = (pinput->mode == VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID) ? (DEFAULT_NORM_HEIGHT*2) : DEFAULT_NORM_HEIGHT;
            pinput->frame_rate  = pinput->max_frame_rate = DEFAULT_FRAME_RATE;
            pinput->timeout_ms  = DEFAULT_TIMEOUT;
        }

        /* channel parameter */
        for(j=0; j<VCAP_INPUT_DEV_CH_MAX; j++) {
            if((pinput->mode == VCAP_INPUT_MODE_BYPASS) && (j > 0))
                continue;

            if((pinput->mode == VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID) && ((j%2) != 0))    ///< 2ch dual edge, use channel 0 and 2
                continue;

            if(pinput->ch_param[j].mode != norm) {
                pinput->ch_param[j].mode       = norm;
                pinput->ch_param[j].width      = DEFAULT_NORM_WIDTH;
                pinput->ch_param[j].height     = DEFAULT_NORM_HEIGHT;
                pinput->ch_param[j].frame_rate = DEFAULT_FRAME_RATE;
                pinput->ch_param[j].timeout_ms = DEFAULT_TIMEOUT;
                pinput->ch_param[j].speed      = DEFAULT_SPEED;
                pinput->ch_param[j].prog       = (pinput->ch_param[0].speed == VCAP_INPUT_SPEED_I) ? 0 : 1;
            }
        }
    }

    return ret;
}

static int tp2823_proc_norm_show(struct seq_file *sfile, void *v)
{
    int i, vout, vout_ch, dev_ch;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2823.%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN   VCH   Width   Height\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<DEV_CH_MAX; i++) {
        vout = tp2823_get_vout_id(pdev->index, i);
        if((vout < DEV_VPORT_MAX) && pdev->port[vout]) {
            vout_ch = tp2823_get_vout_seq_id(pdev->index, i);
            if((pdev->port[vout])->mode == VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID)
                dev_ch = vout_ch*2;   ///< vout_ch_id would be 0 and 1, the input device channel use 0 and 2 for 2ch dual edge mode
            else
                dev_ch = vout_ch;

            if(dev_ch >= VCAP_INPUT_DEV_CH_MAX)
                goto unknown;

            seq_printf(sfile, "%-5d %-5d %-7d %-7d\n",
                       i,
                       tp2823_get_vch_id(pdev->index, vout, vout_ch),
                       (pdev->port[vout])->ch_param[dev_ch].width,
                       (pdev->port[vout])->ch_param[dev_ch].height);
        }
        else {
unknown:
            seq_printf(sfile, "%-5s %-5s %-7s %-7s\n", "-", "-", "-", "-");
        }
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int tp2823_proc_timeout_show(struct seq_file *sfile, void *v)
{
    int i, vout, vout_ch, dev_ch;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2823.%d]\n", pdev->index);
    seq_printf(sfile, "-------------------\n");
    seq_printf(sfile, "VIN   VCH   Timeout\n");
    seq_printf(sfile, "-------------------\n");
    for(i=0; i<DEV_CH_MAX; i++) {
        vout = tp2823_get_vout_id(pdev->index, i);
        if((vout < DEV_VPORT_MAX) && pdev->port[vout]) {
            vout_ch = tp2823_get_vout_seq_id(pdev->index, i);
            if((pdev->port[vout])->mode == VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID)
                dev_ch = vout_ch*2; ///< vout_seq_id would be 0 and 1, the input device channel use 0 and 2 for 2ch dual edge mode
            else
                dev_ch = vout_ch;

            if(dev_ch >= VCAP_INPUT_DEV_CH_MAX)
                goto unknown;

            seq_printf(sfile, "%-5d %-5d %-7d\n",
                       i,
                       tp2823_get_vch_id(pdev->index, vout, vout_ch),
                       (pdev->port[vout])->ch_param[dev_ch].timeout_ms);
        }
        else {
unknown:
            seq_printf(sfile, "%-5s %-5s %-7s\n", "-", "-", "-");
        }
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int tp2823_proc_norm_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_norm_show, PDE(inode)->data);
}

static int tp2823_proc_timeout_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_timeout_show, PDE(inode)->data);
}

static struct file_operations tp2823_proc_norm_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_norm_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2823_proc_timeout_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_timeout_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void tp2823_proc_remove(int id)
{
    if(tp2823_proc_root[id]) {
        if(tp2823_proc_norm[id])
            vcap_input_proc_remove_entry(tp2823_proc_root[id], tp2823_proc_norm[id]);

        if(tp2823_proc_timeout[id])
            vcap_input_proc_remove_entry(tp2823_proc_root[id], tp2823_proc_timeout[id]);

        vcap_input_proc_remove_entry(NULL, tp2823_proc_root[id]);
    }
}

static int tp2823_proc_init(struct tp2823_dev_t *pdev)
{
    int ret = 0;
    int id  = pdev->index;
    char name[32];

    /* root */
    sprintf(name, "tp2823.%d", id);
    tp2823_proc_root[id] = vcap_input_proc_create_entry(name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!tp2823_proc_root[id]) {
        vcap_err("create proc node '%s' failed!\n", name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_root[id]->owner = THIS_MODULE;
#endif

    /* norm */
    tp2823_proc_norm[id] = vcap_input_proc_create_entry("norm", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_norm[id]) {
        vcap_err("create proc node '%s/norm' failed!\n", name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_norm[id]->proc_fops = &tp2823_proc_norm_ops;
    tp2823_proc_norm[id]->data      = (void *)pdev;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_norm[id]->owner     = THIS_MODULE;
#endif

    /* timeout */
    tp2823_proc_timeout[id] = vcap_input_proc_create_entry("timeout", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_timeout[id]) {
        vcap_err("create proc node '%s/timeout' failed!\n", name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_timeout[id]->proc_fops = &tp2823_proc_timeout_ops;
    tp2823_proc_timeout[id]->data      = (void *)pdev;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_timeout[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    tp2823_proc_remove(id);
    return ret;
}

static int __init tp2823_input_init(void)
{
    int i, j;
    int dev_num;
    struct vcap_input_dev_t *pinput;
    int ret = 0;

    if(vcap_input_get_version() < VCAP_INPUT_VERSION) {
        vcap_err("Input driver version(%08x) is not compatibility with vcap300_common.ko(%08x)!\n",
                 vcap_input_get_version(), VCAP_INPUT_VERSION);
        return -EFAULT;
    }

    /* clear buffer */
    memset(tp2823_dev, 0, sizeof(struct tp2823_dev_t)*DEV_MAX);

    /* Get Device Number */
    dev_num = tp2823_get_device_num();

    for(i=0; i<DEV_MAX; i++) {
        tp2823_dev[i].index = i;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&tp2823_dev[i].lock, 1);
#else
        init_MUTEX(&tp2823_dev[i].lock);
#endif
        if(i >= dev_num)
            continue;

        if(vport[i]) {
            /* register input device */
            for(j=0; j<DEV_VPORT_MAX; j++) {
                if((vport[i]>>(4*j)) & 0xf) {   ///< VPort# Connect to X_CAP#
                    pinput = tp2823_dev[i].port[j] = kzalloc(sizeof(struct vcap_input_dev_t), GFP_KERNEL);
                    if(pinput == NULL) {
                        vcap_err("tp2823#%d.%d allocate vcap_input_dev_t failed\n", i, j);
                        ret = -ENOMEM;
                        goto err;
                    }

                    /* input device name */
                    snprintf(pinput->name, VCAP_INPUT_NAME_SIZE-1, "tp2823.%d.%d", i, j);

                    /* input device parameter setup */
                    pinput->index       = (i*DEV_VPORT_MAX) + j;
                    pinput->vi_idx      = ((vport[i]>>(4*j)) & 0xf) - 1; ///< VI#
                    pinput->vi_src      = ((vport[i]>>(4*j)) & 0xf) - 1; ///< X_CAP#
                    pinput->type        = DEFAULT_TYPE;
                    pinput->speed       = DEFAULT_SPEED;
                    pinput->field_order = DEFAULT_ORDER;
                    pinput->data_range  = DEFAULT_DATA_RANGE;
                    pinput->mode        = (tp2823_get_vout_mode(i) == TP2823_VOUT_MODE_2CH_DUAL_EDGE) ? VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID : VCAP_INPUT_MODE_BYPASS;
                    pinput->yc_swap     = (yc_swap[i]>>(4*j)) & 0x1;
                    pinput->data_swap   = data_swap[i];
                    pinput->ch_id_mode  = 0;
                    pinput->inv_clk     = (inv_clk[i]>>(4*j)) & 0x1;
                    pinput->init        = tp2823_device_init;
                    pinput->module_get  = tp2823_module_get;
                    pinput->module_put  = tp2823_module_put;
                    pinput->norm.mode   = -1;                            ///< init value

                    /* interface */
                    switch(tp2823_get_vout_format(i)) {
                        case TP2823_VOUT_FORMAT_BT1120:
                            pinput->interface = VCAP_INPUT_INTF_BT1120_PROGRESSIVE;
                            break;
                        case TP2823_VOUT_FORMAT_BT656:
                        default:
                            pinput->interface = VCAP_INPUT_INTF_BT656_PROGRESSIVE;
                            break;
                    }

                    /* channel mode */
                    switch(pinput->mode) {
                        case VCAP_INPUT_MODE_2CH_BYTE_INTERLEAVE_HYBRID:
                            pinput->probe_chid = VCAP_INPUT_PROBE_CHID_DISABLE;
                            pinput->ch_id[0]   = tp2823_get_vch_id(i, j, 0);    ///< vout sequence id 0
                            pinput->ch_id[2]   = tp2823_get_vch_id(i, j, 1);    ///< vout sequence id 1

                            /* channel parameter */
                            pinput->ch_param[0].mode = pinput->ch_param[2].mode = -1; ///< init value
                            break;
                        default:
                            pinput->probe_chid = VCAP_INPUT_PROBE_CHID_DISABLE;
                            pinput->ch_id[0]   = tp2823_get_vch_id(i, j, 0);    ///< vout sequence id 0

                            /* channel parameter */
                            pinput->ch_param[0].mode = -1;  ///< init value
                            break;
                    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
                    sema_init(&pinput->sema_lock, 1);
#else
                    init_MUTEX(&pinput->sema_lock);
#endif
                    ret = vcap_input_device_register(pinput);
                    if(ret < 0) {
                        vcap_err("register tp2823#%d.%d input device failed\n", i, j);
                        goto err;
                    }
                }
            }

            /* device proc init */
            ret = tp2823_proc_init(&tp2823_dev[i]);
            if(ret < 0)
                goto err;

            /* device init */
            ret = tp2823_device_init(i, 0);
            if(ret < 0)
                goto err;

            /* register tp2823 video loss notify handler */
            tp2823_notify_vlos_register(i, tp2823_vlos_notify_handler);

            /* register tp2823 video foramt notify handler */
            tp2823_notify_vfmt_register(i, tp2823_vfmt_notify_handler);

            vcap_info("Register TP2823#%d Input Device\n", i);
        }
    }

    return ret;

err:
    for(i=0; i<DEV_MAX; i++) {
        tp2823_notify_vlos_deregister(i);
        tp2823_notify_vfmt_deregister(i);
        tp2823_proc_remove(i);
        for(j=0; j<DEV_VPORT_MAX; j++) {
            if(tp2823_dev[i].port[j]) {
                vcap_input_device_unregister(tp2823_dev[i].port[j]);
                kfree(tp2823_dev[i].port[j]);
            }
        }
    }
    return ret;
}

static void __exit tp2823_input_exit(void)
{
    int i, j;

    for(i=0; i<DEV_MAX; i++) {
        tp2823_notify_vlos_deregister(i);
        tp2823_notify_vfmt_deregister(i);
        tp2823_proc_remove(i);
        for(j=0; j<DEV_VPORT_MAX; j++) {
            if(tp2823_dev[i].port[j]) {
                vcap_input_device_unregister(tp2823_dev[i].port[j]);
                kfree(tp2823_dev[i].port[j]);
            }
        }
    }
}

module_init(tp2823_input_init);
module_exit(tp2823_input_exit);

MODULE_DESCRIPTION("Grain Media Video Capture300 TP2823 Input Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
