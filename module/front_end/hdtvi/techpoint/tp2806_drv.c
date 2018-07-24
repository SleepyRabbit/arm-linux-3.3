/**
 * @file tp2806_drv.c
 * TechPoint TP2806 4CH HD-TVI Video Decoder Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2015/03/03 13:44:14 $
 *
 * ChangeLog:
 *  $Log: tp2806_drv.c,v $
 *  Revision 1.1  2015/03/03 13:44:14  shawn_hu
 *  Add Techpoint HD-TVI TP2806 driver support (1-CH bypass & 2-CH dual edge).
 *  1. TP2806 doesn't support TVI 1080P cameras, our driver will force that channel to output blue screen (chip limitation of TP2806)
 *     if 1080P camera is connected.
 *  2. TP2806 doesn't support TVI v1 cameras (148.5 MHz video output) during 2-CH dual edge mode, our driver will force that channel to
 *     output blue screen (chip limitation of TP2806) if v1 camera is connected. Only v2 cameras (74.25 MHz video output) are supported.
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
#include "techpoint/tp2806.h"                       ///< from module/include/front_end/hdtvi

#define TP2806_CLKIN                   27000000     ///< Master source clock of device
#define CH_IDENTIFY(id, vin, vout)     (((id)&0xf)|(((vin)&0xf)<<4)|(((vout)&0xf)<<8))
#define CH_IDENTIFY_ID(x)              ((x)&0xf)
#define CH_IDENTIFY_VIN(x)             (((x)>>4)&0xf)
#define CH_IDENTIFY_VOUT(x)            (((x)>>8)&0xf)

/* device number */
static int dev_num = 1;
module_param(dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dev_num, "Device Number: 1~4");

/* device i2c bus */
static int ibus = 0;
module_param(ibus, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ibus, "Device I2C Bus: 0~4");

/* device i2c address */
static ushort iaddr[TP2806_DEV_MAX] = {0x88, 0x8a, 0x8c, 0x8e};
module_param_array(iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(iaddr, "Device I2C Address");

/* clock port used */
static int clk_used = 0x3;
module_param(clk_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_used, "External Clock Port: BIT0:EXT_CLK0, BIT1:EXT_CLK1");

/* clock source */
static int clk_src = PLAT_EXTCLK_SRC_PLL1OUT1_DIV2;
module_param(clk_src, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_src, "External Clock Source: 0:PLL1OUT1 1:PLL1OUT1/2 2:PLL4OUT2 3:PLL4OUT1/2 4:PLL3");

/* clock frequency */
static int clk_freq = TP2806_CLKIN;   ///< 27MHz
module_param(clk_freq, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_freq, "External Clock Frequency(Hz)");

/* device init */
static int init = 1;
module_param(init, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(init, "device init: 0:no-init  1:init");

/* device use gpio as rstb pin */
static int rstb_used = 1;
module_param(rstb_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rstb_used, "Use GPIO as RSTB Pin: 0:None, 1:X_CAP_RST");

/* device video port output format */
static int vout_format[TP2806_DEV_MAX] = {[0 ... (TP2806_DEV_MAX - 1)] = TP2806_VOUT_FORMAT_BT656};
module_param_array(vout_format, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vout_format, "Video Output Format: 0:BT656, 1:BT1120");

/* device video port output mode */
static int vout_mode[TP2806_DEV_MAX] = {[0 ... (TP2806_DEV_MAX - 1)] = TP2806_VOUT_MODE_1CH_BYPASS};
module_param_array(vout_mode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vout_mode, "Video Output Mode: 0:1CH_BYPASS, 1:2CH_DUAL_EDGE");

/* device channel mapping */
static int ch_map = 0;
module_param(ch_map, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_map, "channel mapping");

/* notify */
static int notify = 1;
module_param(notify, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Device Notify => 0:Disable 1:Enable");

/******************************************************************************
 GM8210 EVB Channel and TP2806 Mapping Table (1CH Bypass)

 VCH#0 ------> TP2806#1.0(VIN0) ---------> VOUT#0.0 -------> X_CAP#3
 VCH#1 ------> TP2806#1.1(VIN1) ---------> VOUT#0.1 -------> X_CAP#2
 VCH#2 ------> TP2806#1.2(VIN2) ---------> VOUT#0.2 -------> X_CAP#1
 VCH#3 ------> TP2806#1.3(VIN3) ---------> VOUT#0.3 -------> X_CAP#0
 VCH#4 ------> TP2806#0.0(VIN0) ---------> VOUT#1.0 -------> X_CAP#7
 VCH#5 ------> TP2806#0.1(VIN1) ---------> VOUT#1.1 -------> X_CAP#6
 VCH#6 ------> TP2806#0.2(VIN2) ---------> VOUT#1.2 -------> X_CAP#5
 VCH#7 ------> TP2806#0.3(VIN3) ---------> VOUT#1.3 -------> X_CAP#4

 ==============================================================================
 GM8287 EVB Channel and TP2806 Mapping Table (1CH Bypass)

 VCH#0 ------> TP2806#0.0(VIN0) ---------> VOUT#0.0 -------> X_CAP#3
 VCH#1 ------> TP2806#0.1(VIN1) ---------> VOUT#0.1 -------> X_CAP#2
 VCH#2 ------> TP2806#0.2(VIN2) ---------> VOUT#0.2 -------> X_CAP#1
 VCH#3 ------> TP2806#0.3(VIN3) ---------> VOUT#0.3 -------> X_CAP#0

 ==============================================================================
 GM8287 EVB Channel and TP2806 Mapping Table (2CH Dual Edge)

 VCH#0 ------> TP2806#0.0(VIN0) ---------> VOUT#0.0 -------> X_CAP#1
 VCH#1 ------> TP2806#0.1(VIN1) ----|
 VCH#2 ------> TP2806#0.2(VIN2) ---------> VOUT#0.1 -------> X_CAP#0
 VCH#3 ------> TP2806#0.3(VIN3) ----|

*******************************************************************************/

struct tp2806_dev_t {
    int               index;                          ///< device index
    char              name[16];                       ///< device name
    struct semaphore  lock;                           ///< device locker
    struct miscdevice miscdev;                        ///< device node for ioctl access

    int               pre_plugin[TP2806_DEV_CH_MAX];  ///< device channel previous cable plugin status
    int               cur_plugin[TP2806_DEV_CH_MAX];  ///< device channel current  cable plugin status

    int               (*vfmt_notify)(int id, int ch, struct tp2806_video_fmt_t *vfmt);
    int               (*vlos_notify)(int id, int ch, int vlos);
};

static struct tp2806_dev_t    tp2806_dev[TP2806_DEV_MAX];
static struct proc_dir_entry *tp2806_proc_root[TP2806_DEV_MAX]      = {[0 ... (TP2806_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2806_proc_status[TP2806_DEV_MAX]    = {[0 ... (TP2806_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2806_proc_video_fmt[TP2806_DEV_MAX] = {[0 ... (TP2806_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2806_proc_vout_fmt[TP2806_DEV_MAX]  = {[0 ... (TP2806_DEV_MAX - 1)] = NULL};

static struct i2c_client  *tp2806_i2c_client = NULL;
static struct task_struct *tp2806_wd         = NULL;
static u32 TP2806_VCH_MAP[TP2806_DEV_MAX*TP2806_DEV_CH_MAX];

static int tp2806_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    tp2806_i2c_client = client;
    return 0;
}

static int tp2806_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tp2806_i2c_id[] = {
    { "tp2806_i2c", 0 },
    { }
};

static struct i2c_driver tp2806_i2c_driver = {
    .driver = {
        .name  = "TP2806_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = tp2806_i2c_probe,
    .remove   = tp2806_i2c_remove,
    .id_table = tp2806_i2c_id
};

static int tp2806_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&tp2806_i2c_driver);
    if(ret < 0) {
        printk("TP2806 can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = iaddr[0]>>1;
    strlcpy(info.type, "tp2806_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("TP2806 can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("TP2806 can't add i2c device!\n");
        goto err_driver;
    }

    return 0;

err_driver:
   i2c_del_driver(&tp2806_i2c_driver);
   return -ENODEV;
}

static void tp2806_i2c_unregister(void)
{
    i2c_unregister_device(tp2806_i2c_client);
    i2c_del_driver(&tp2806_i2c_driver);
    tp2806_i2c_client = NULL;
}

static int tp2806_create_ch_map(int map_id)
{
    int i;

    switch(map_id) {
        case 1: ///< for GM8210/GM8287 Socket Board
            for(i=0; i<dev_num; i++) {
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+0] = CH_IDENTIFY(dev_num-i-1, 0, TP2806_DEV_VOUT0);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+1] = CH_IDENTIFY(dev_num-i-1, 1, TP2806_DEV_VOUT1);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+2] = CH_IDENTIFY(dev_num-i-1, 2, TP2806_DEV_VOUT2);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+3] = CH_IDENTIFY(dev_num-i-1, 3, TP2806_DEV_VOUT3);
            }
            break;

        case 2: ///< for GM8210/GM828x, 2CH Dual Edge Mode
            for(i=0; i<dev_num; i++) {
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, TP2806_DEV_VOUT0);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, TP2806_DEV_VOUT0);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, TP2806_DEV_VOUT1);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, TP2806_DEV_VOUT1);
            }
            break;

        case 3: ///< for GM8210/GM828x, 2CH Dual Edge Mode
            for(i=0; i<dev_num; i++) {
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, TP2806_DEV_VOUT2);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, TP2806_DEV_VOUT2);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, TP2806_DEV_VOUT3);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, TP2806_DEV_VOUT3);
            }
            break;

        case 0: ///< for GM8210/GM8287 System Board
        default:
            for(i=0; i<dev_num; i++) {
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, TP2806_DEV_VOUT0);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, TP2806_DEV_VOUT1);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, TP2806_DEV_VOUT2);
                TP2806_VCH_MAP[(i*TP2806_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, TP2806_DEV_VOUT3);
            }
            break;
    }

    return 0;
}

/*
 * seq_id: channel sequence number in video output port,
 *         for 1CH_BYPASS    mode vout_seq = 0
 *         for 2CH_DUAL_EDGE mode vout_seq = 0 or 1
*/
int tp2806_get_vch_id(int id, TP2806_DEV_VOUT_T vout, int seq_id)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vout >= TP2806_DEV_VOUT_MAX || seq_id >= 2)
        return 0;

    if(vout_mode[id] == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*TP2806_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2806_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VOUT(TP2806_VCH_MAP[i]) == vout) &&
           ((CH_IDENTIFY_VIN(TP2806_VCH_MAP[i])%vport_chnum) == seq_id)) {
            return i;
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_get_vch_id);

int tp2806_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= TP2806_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*TP2806_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2806_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2806_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_vin_to_vch);

int tp2806_get_vout_id(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= TP2806_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*TP2806_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2806_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2806_VCH_MAP[i]) == vin_idx))
            return CH_IDENTIFY_VOUT(TP2806_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_get_vout_id);

int tp2806_get_vout_seq_id(int id, int vin_idx)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vin_idx >= TP2806_DEV_CH_MAX)
        return 0;

    if(vout_mode[id] == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*TP2806_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2806_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2806_VCH_MAP[i]) == vin_idx)) {
            return (vin_idx%vport_chnum);
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_get_vout_seq_id);

int tp2806_get_vout_format(int id)
{
    if(id >= dev_num)
        return TP2806_VOUT_FORMAT_BT656;
    else
        return vout_format[id];
}
EXPORT_SYMBOL(tp2806_get_vout_format);

int tp2806_get_vout_mode(int id)
{
    if(id >= dev_num)
        return TP2806_VOUT_MODE_1CH_BYPASS;
    else
        return vout_mode[id];
}
EXPORT_SYMBOL(tp2806_get_vout_mode);

int tp2806_get_device_num(void)
{
    return dev_num;
}
EXPORT_SYMBOL(tp2806_get_device_num);

int tp2806_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2806_video_fmt_t *))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2806_dev[id].lock);

    tp2806_dev[id].vfmt_notify = nt_func;

    up(&tp2806_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2806_notify_vfmt_register);

void tp2806_notify_vfmt_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2806_dev[id].lock);

    tp2806_dev[id].vfmt_notify = NULL;

    up(&tp2806_dev[id].lock);
}
EXPORT_SYMBOL(tp2806_notify_vfmt_deregister);

int tp2806_notify_vlos_register(int id, int (*nt_func)(int, int, int))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2806_dev[id].lock);

    tp2806_dev[id].vlos_notify = nt_func;

    up(&tp2806_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2806_notify_vlos_register);

void tp2806_notify_vlos_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2806_dev[id].lock);

    tp2806_dev[id].vlos_notify = NULL;

    up(&tp2806_dev[id].lock);
}
EXPORT_SYMBOL(tp2806_notify_vlos_deregister);

int tp2806_i2c_write(u8 id, u8 reg, u8 data)
{
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return -1;

    if(!tp2806_i2c_client) {
        printk("TP2806 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2806_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("TP2806#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_i2c_write);

u8 tp2806_i2c_read(u8 id, u8 reg)
{
    u8 reg_data = 0;
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return 0;

    if(!tp2806_i2c_client) {
        printk("TP2806 i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(tp2806_i2c_client->dev.parent);

    msgs[0].addr  = iaddr[id]>>1;
    msgs[0].flags = 0; /* write */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = (iaddr[id] + 1)>>1;
    msgs[1].flags = 1; /* read */
    msgs[1].len   = 1;
    msgs[1].buf   = &reg_data;

    if(i2c_transfer(adapter, msgs, 2) != 2)
        printk("TP2806#%d i2c read failed!!\n", id);

    return reg_data;
}
EXPORT_SYMBOL(tp2806_i2c_read);

int tp2806_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt)
{
#define TP2806_I2C_ARRAY_WRITE_MAX     256
    int i, num = 0;
    u8  buf[TP2806_I2C_ARRAY_WRITE_MAX+1];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num || !parray || array_cnt > TP2806_I2C_ARRAY_WRITE_MAX)
        return -1;

    if(!tp2806_i2c_client) {
        printk("TP2806 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2806_i2c_client->dev.parent);

    buf[num++] = addr;
    for(i=0; i<array_cnt; i++) {
        buf[num++] = parray[i];
    }

    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = num;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("TP2806#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(tp2806_i2c_array_write);

static int tp2806_proc_status_show(struct seq_file *sfile, void *v)
{
    int i;
    int vlos;
    struct tp2806_dev_t *pdev = (struct tp2806_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2806#%d]\n", pdev->index);
    seq_printf(sfile, "------------------\n");
    seq_printf(sfile, "VIN    VCH    LOS \n");
    seq_printf(sfile, "==================\n");
    for(i=0; i<TP2806_DEV_CH_MAX; i++) {
        vlos = tp2806_status_get_video_loss(pdev->index, i);
        seq_printf(sfile, "%-6d %-6d %-7s\n", i, tp2806_vin_to_vch(pdev->index, i), ((vlos == 0) ? "NO" : "YES"));
    }
    up(&pdev->lock);

    return 0;
}

static int tp2806_proc_video_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret;
    struct tp2806_video_fmt_t vfmt;
    struct tp2806_dev_t *pdev = (struct tp2806_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2806#%d] Incoming Video Fromat\n", pdev->index);
    seq_printf(sfile, "====================================================\n");
    seq_printf(sfile, "VIN   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "====================================================\n");

    for(i=0; i<TP2806_DEV_CH_MAX; i++) {
        ret = tp2806_status_get_video_input_format(pdev->index, i, &vfmt);
        if((ret == 0) && ((vfmt.fmt_idx >= TP2806_VFMT_720P60) && (vfmt.fmt_idx < TP2806_VFMT_MAX))) {
            seq_printf(sfile, "%-5d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2806_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-5d %-5d %-7s %-8s %-12s %-11s\n", i, tp2806_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2806_proc_vout_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret;
    struct tp2806_video_fmt_t vfmt;
    struct tp2806_dev_t *pdev = (struct tp2806_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2806#%d] Video Output Fromat\n", pdev->index);
    seq_printf(sfile, "=====================================================\n");
    seq_printf(sfile, "VOUT   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "=====================================================\n");

    for(i=0; i<TP2806_DEV_CH_MAX; i++) {
        ret = tp2806_video_get_video_output_format(pdev->index, i, &vfmt);
        if((ret == 0) && ((vfmt.fmt_idx >= TP2806_VFMT_720P60) && (vfmt.fmt_idx < TP2806_VFMT_MAX))) {
            seq_printf(sfile, "%-6d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2806_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-6d %-5d %-7s %-8s %-12s %-11s\n", i, tp2806_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2806_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2806_proc_status_show, PDE(inode)->data);
}

static int tp2806_proc_video_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2806_proc_video_fmt_show, PDE(inode)->data);
}

static int tp2806_proc_vout_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2806_proc_vout_fmt_show, PDE(inode)->data);
}

static struct file_operations tp2806_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2806_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2806_proc_video_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2806_proc_video_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2806_proc_vout_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2806_proc_vout_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void tp2806_proc_remove(int id)
{
    if(id >= TP2806_DEV_MAX)
        return;

    if(tp2806_proc_root[id]) {
        if(tp2806_proc_status[id]) {
            remove_proc_entry(tp2806_proc_status[id]->name, tp2806_proc_root[id]);
            tp2806_proc_status[id] = NULL;
        }

        if(tp2806_proc_video_fmt[id]) {
            remove_proc_entry(tp2806_proc_video_fmt[id]->name, tp2806_proc_root[id]);
            tp2806_proc_video_fmt[id] = NULL;
        }

        if(tp2806_proc_vout_fmt[id]) {
            remove_proc_entry(tp2806_proc_vout_fmt[id]->name, tp2806_proc_root[id]);
            tp2806_proc_vout_fmt[id] = NULL;
        }

        remove_proc_entry(tp2806_proc_root[id]->name, NULL);
        tp2806_proc_root[id] = NULL;
    }
}

static int tp2806_proc_init(int id)
{
    int ret = 0;

    if(id >= TP2806_DEV_MAX)
        return -1;

    /* root */
    tp2806_proc_root[id] = create_proc_entry(tp2806_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!tp2806_proc_root[id]) {
        printk("create proc node '%s' failed!\n", tp2806_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2806_proc_root[id]->owner = THIS_MODULE;
#endif

    /* status */
    tp2806_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, tp2806_proc_root[id]);
    if(!tp2806_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", tp2806_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2806_proc_status[id]->proc_fops = &tp2806_proc_status_ops;
    tp2806_proc_status[id]->data      = (void *)&tp2806_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2806_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* frame format */
    tp2806_proc_video_fmt[id] = create_proc_entry("video_format", S_IRUGO|S_IXUGO, tp2806_proc_root[id]);
    if(!tp2806_proc_video_fmt[id]) {
        printk("create proc node '%s/video_format' failed!\n", tp2806_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2806_proc_video_fmt[id]->proc_fops = &tp2806_proc_video_fmt_ops;
    tp2806_proc_video_fmt[id]->data      = (void *)&tp2806_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2806_proc_video_fmt[id]->owner     = THIS_MODULE;
#endif

    /* video output format */
    tp2806_proc_vout_fmt[id] = create_proc_entry("vout_format", S_IRUGO|S_IXUGO, tp2806_proc_root[id]);
    if(!tp2806_proc_vout_fmt[id]) {
        printk("create proc node '%s/vout_format' failed!\n", tp2806_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2806_proc_vout_fmt[id]->proc_fops = &tp2806_proc_vout_fmt_ops;
    tp2806_proc_vout_fmt[id]->data      = (void *)&tp2806_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2806_proc_vout_fmt[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    tp2806_proc_remove(id);
    return ret;
}

static int tp2806_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct tp2806_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(tp2806_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &tp2806_dev[i];
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

static int tp2806_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long tp2806_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct tp2806_dev_t *pdev = (struct tp2806_dev_t *)file->private_data;

    down(&pdev->lock);

    if(_IOC_TYPE(cmd) != TP2806_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case TP2806_GET_NOVID:
            {
                struct tp2806_ioc_data_t ioc_data;

                if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_data.vin_ch >= TP2806_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_data.data = tp2806_status_get_video_loss(pdev->index, ioc_data.vin_ch);

                ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            }
            break;

        case TP2806_GET_VIDEO_FMT:
            {
                struct tp2806_ioc_vfmt_t ioc_vfmt;
                struct tp2806_video_fmt_t vfmt;

                if(copy_from_user(&ioc_vfmt, (void __user *)arg, sizeof(ioc_vfmt))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vfmt.vin_ch >= TP2806_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ret = tp2806_status_get_video_input_format(pdev->index, ioc_vfmt.vin_ch, &vfmt);
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

static struct file_operations tp2806_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = tp2806_miscdev_open,
    .release        = tp2806_miscdev_release,
    .unlocked_ioctl = tp2806_miscdev_ioctl,
};

static int tp2806_miscdev_init(int id)
{
    int ret;

    if(id >= TP2806_DEV_MAX)
        return -1;

    /* clear */
    memset(&tp2806_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    tp2806_dev[id].miscdev.name  = tp2806_dev[id].name;
    tp2806_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    tp2806_dev[id].miscdev.fops  = (struct file_operations *)&tp2806_miscdev_fops;
    ret = misc_register(&tp2806_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", tp2806_dev[id].name);
        tp2806_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

static void tp2806_hw_reset(void)
{
    if(!rstb_used || !init)
        return;

    /* Active Low */
    plat_gpio_set_value(rstb_used, 0);

    udelay(100);

    /* Active High and release reset */
    plat_gpio_set_value(rstb_used, 1);

    udelay(100);
}

static int tp2806_device_init(int id)
{
    int ret;

    if(id >= TP2806_DEV_MAX)
        return -1;

    if(!init)
        goto exit;

    /* video init */
    ret = tp2806_video_init(id, vout_format[id], vout_mode[id]);
    if(ret < 0)
        goto exit;

    /* audio init */
    /* ToDo */

exit:
    printk("TP2806#%d Init...%s!\n", id, ((ret == 0) ? "OK" : "Fail"));

    return ret;
}

static int tp2806_watchdog_thread(void *data)
{
    int i, ch, ret;
    struct tp2806_dev_t *pdev;
    struct tp2806_video_fmt_t vin_fmt, vout_fmt;

    do {
        /* check tp2806 channel status */
        for(i=0; i<dev_num; i++) {
            pdev = &tp2806_dev[i];

            down(&pdev->lock);

            for(ch=0; ch<TP2806_DEV_CH_MAX; ch++) {
                pdev->cur_plugin[ch] = (tp2806_status_get_video_loss(i, ch) == 0) ? 1 : 0;

                if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 0)) {        ///< cable plugged-in
                    goto sts_update;
                }
                else if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 1)) {   ///< cable is connected
                        /* notify current video present */
                        if(notify && pdev->vlos_notify)
                            pdev->vlos_notify(i, ch, 0);

                        /* get input camera video fromat */
                        ret = tp2806_status_get_video_input_format(i, ch, &vin_fmt);

                        /* switch video port output format */
                        if(ret == 0) {
                            tp2806_video_set_video_output_format(i, ch, vin_fmt.fmt_idx, vout_format[i]);
                        }

                        /* get video port output format */
                        ret = tp2806_video_get_video_output_format(i, ch, &vout_fmt);
                        if(ret < 0)
                            goto sts_update;

                        /* notify current video format */
                        if(notify && pdev->vfmt_notify && (vout_fmt.fmt_idx >= TP2806_VFMT_720P60) && (vout_fmt.fmt_idx < TP2806_VFMT_MAX)) {
                            if(vout_mode[i] == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
                                vout_fmt.frame_rate /= 2;
                            pdev->vfmt_notify(i, ch, &vout_fmt);
                        }
                }
                else {  ///< cable is plugged-out
                    /* notify current video loss */
                    if(notify && pdev->vlos_notify)
                        pdev->vlos_notify(i, ch, 1);

                    /* If HD cable is plugged-out, force black image output to 720P30 */
                    if(vout_mode[i] == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
                        ret = tp2806_video_set_video_output_format(i, ch, TP2806_VFMT_720P60, vout_format[i]);
                    else
                        ret = tp2806_video_set_video_output_format(i, ch, TP2806_VFMT_720P30, vout_format[i]);
                    if(ret < 0)
                        goto sts_update;

                    /* notify current video format */
                    if(notify && pdev->vfmt_notify) {
                        /* get video port output format */
                        ret = tp2806_video_get_video_output_format(i, ch, &vout_fmt);
                        if(ret < 0)
                            goto sts_update;

                        if(vout_mode[i] == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
                            vout_fmt.frame_rate /= 2;
                        pdev->vfmt_notify(i, ch, &vout_fmt);
                    }
                }
sts_update:
                pdev->pre_plugin[ch] = pdev->cur_plugin[ch];
            }

            up(&pdev->lock);
        }

        /* sleep 1 second */
        schedule_timeout_interruptible(msecs_to_jiffies(500));

    } while (!kthread_should_stop());

    return 0;
}

static int __init tp2806_init(void)
{
    int i, ret;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(dev_num > TP2806_DEV_MAX) {
        printk("TP2806 dev_num=%d invalid!!(Max=%d)\n", dev_num, TP2806_DEV_MAX);
        return -1;
    }

    /* check video output format */
    for(i=0; i<dev_num; i++) {
        if((vout_format[i] < 0) || (vout_format[i] >= TP2806_VOUT_FORMAT_MAX)) {
            printk("TP2806#%d vout_format=%d not support!!\n", i, vout_format[i]);
            return -1;
        }
    }

    /* check video output mode */
    for(i=0; i<dev_num; i++) {
        if((vout_mode[i] < 0) || (vout_mode[i] >= TP2806_VOUT_MODE_MAX)) {
            printk("TP2806#%d vout_mode=%d not support!!\n", i, vout_mode[i]);
            return -1;
        }
    }

    /* register i2c client for contol tp2806 */
    ret = tp2806_i2c_register();
    if(ret < 0)
        return -1;

    /* register gpio pin for device rstb pin */
    if(rstb_used && init) {
        ret = plat_register_gpio_pin(rstb_used, PLAT_GPIO_DIRECTION_OUT, 0);
        if(ret < 0)
            goto err;
    }

    /* setup platfrom external clock */
    if((init == 1) && (clk_used >= 0)) {
        if(clk_used) {
            ret = plat_extclk_set_src(clk_src);
            if(ret < 0)
                goto err;

            if(clk_used & 0x1) {
                ret = plat_extclk_set_freq(0, clk_freq);
                if(ret < 0)
                    goto err;
            }

            if(clk_used & 0x2) {
                ret = plat_extclk_set_freq(1, clk_freq);
                if(ret < 0)
                    goto err;
            }

            ret = plat_extclk_onoff(1);
            if(ret < 0)
                goto err;

        }
        else {
            /* use external oscillator. disable SoC external clock output */
            ret = plat_extclk_onoff(0);
            if(ret < 0)
                goto err;
        }
    }

    /* create channel mapping table for different EVB */
    ret = tp2806_create_ch_map(ch_map);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    tp2806_hw_reset();

    /* TP2806 init */
    for(i=0; i<dev_num; i++) {
        tp2806_dev[i].index = i;

        sprintf(tp2806_dev[i].name, "tp2806.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&tp2806_dev[i].lock, 1);
#else
        init_MUTEX(&tp2806_dev[i].lock);
#endif
        ret = tp2806_proc_init(i);
        if(ret < 0)
            goto err;

        ret = tp2806_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = tp2806_device_init(i);
        if(ret < 0)
            goto err;
    }

    if(init) {
        /* init tp2806 watchdog thread for check video status */
        tp2806_wd = kthread_create(tp2806_watchdog_thread, NULL, "tp2806_wd");
        if(!IS_ERR(tp2806_wd))
            wake_up_process(tp2806_wd);
        else {
            printk("create tp2806 watchdog thread failed!!\n");
            tp2806_wd = 0;
            ret = -EFAULT;
            goto err;
        }
    }

    return 0;

err:
    if(tp2806_wd)
        kthread_stop(tp2806_wd);

    tp2806_i2c_unregister();
    for(i=0; i<TP2806_DEV_MAX; i++) {
        if(tp2806_dev[i].miscdev.name)
            misc_deregister(&tp2806_dev[i].miscdev);

        tp2806_proc_remove(i);
    }

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit tp2806_exit(void)
{
    int i;

    /* stop tp2806 watchdog thread */
    if(tp2806_wd)
        kthread_stop(tp2806_wd);

    tp2806_i2c_unregister();

    for(i=0; i<TP2806_DEV_MAX; i++) {
        /* remove device node */
        if(tp2806_dev[i].miscdev.name)
            misc_deregister(&tp2806_dev[i].miscdev);

        /* remove proc node */
        tp2806_proc_remove(i);
    }

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);
}

module_init(tp2806_init);
module_exit(tp2806_exit);

MODULE_DESCRIPTION("Grain Media TP2806 4CH HD-TVI Video Decoder Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
