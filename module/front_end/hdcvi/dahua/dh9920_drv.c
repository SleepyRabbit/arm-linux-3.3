/**
 * @file dh9920_drv.c
 * DaHua 4CH HDCVI Receiver Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2016/01/25 12:40:02 $
 *
 * ChangeLog:
 *  $Log: dh9920_drv.c,v $
 *  Revision 1.1  2016/01/25 12:40:02  shawn_hu
 *  1. DH9920 Driver setting is adapted from Dahua DH9920 SDK v20151215_1022.
 *  2. Add the half-1080P & half-720P output during 2-CH dual edge mode.
 *  3. The PTZ function for camera remote control was not verified in this revision.
 *
 *  Note:
 *  As the description in DH9920 datasheet, half-1080P & half-720P can't be mixed in the same BT.656 output.
 *
 *  Known issue:
 *  When DH9920 runs in 2-CH half-720P mode & a 1080P camera is connected, it will cause abnormal black image output.
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
#include "plat_cpucomm.h"
#include "dh9920_lib.h"
#include "dahua/dh9920.h"              ///< from module/include/front_end/hdcvi

#define DH9920_CLKIN                   27000000
#define CH_IDENTIFY(id, cvi, vout)     (((id)&0xf)|(((cvi)&0xf)<<4)|(((vout)&0xf)<<8))
#define CH_IDENTIFY_ID(x)              ((x)&0xf)
#define CH_IDENTIFY_CVI(x)             (((x)>>4)&0xf)
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
static ushort iaddr[DH9920_DEV_MAX] = {0x60, 0x62, 0x64, 0x66};
module_param_array(iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(iaddr, "Device I2C Address");

/* clock port used */
static int clk_used = 0;
module_param(clk_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_used, "External Clock Port: BIT0:EXT_CLK0, BIT1:EXT_CLK1");

/* clock source */
static int clk_src = PLAT_EXTCLK_SRC_PLL3;
module_param(clk_src, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_src, "External Clock Source: 0:PLL1OUT1 1:PLL1OUT1/2 2:PLL4OUT2 3:PLL4OUT1/2 4:PLL3");

/* clock frequency */
static int clk_freq = DH9920_CLKIN;   ///< 27MHz
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
static int vout_format[DH9920_DEV_MAX] = {[0 ... (DH9920_DEV_MAX - 1)] = DH9920_VOUT_FORMAT_BT656_DUAL_HEADER};
module_param_array(vout_format, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vout_format, "Video Output Format: 0:BT656_DUAL_HEADER, 1:BT1120, 2:BT656");

static int vout_mode[DH9920_DEV_MAX] = {[0 ... (DH9920_DEV_MAX - 1)] = DH9920_VOUT_MODE_1CH_BYPASS};
module_param_array(vout_mode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vout_mode, "Video Output Mode: 0:1CH_BYPASS, 1:2CH_DUAL_EDGE, 2:2CH_DUAL_EDGE_HALF_1080P, 3:2CH_DUAL_EDGE_HALF_720P");

/* device channel mapping */
static int ch_map = 0;
module_param(ch_map, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_map, "channel mapping");

/* audio sample rate */
static int sample_rate = DHC_AUDIO_SAMPLERATE_8K;
module_param(sample_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_rate, "audio sample rate: 0:8k  1:16k");

/* audio sample size */
static int sample_size = DH9920_AUDIO_SAMPLESIZE_16BITS;
module_param(sample_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_size, "audio sample size: 0:8bit  1:16bit");

/* notify */
static int notify = 1;
module_param(notify, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Device Notify => 0:Disable 1:Enable");

/******************************************************************************
 GM8287/GM8286 EVB Channel and CVI Mapping Table (1CH Bypass Mode)

 VCH#0 ------> CVI#0.0 ---------> VOUT#0.0 -------> X_CAP#0
 VCH#1 ------> CVI#0.1 ---------> VOUT#0.1 -------> X_CAP#1
 VCH#2 ------> CVI#0.2 ---------> VOUT#0.2 -------> X_CAP#2
 VCH#3 ------> CVI#0.3 ---------> VOUT#0.3 -------> X_CAP#3
 -------------------------------------------------------------------------------
 GM8283/GM8287 EVB Channel and CVI Mapping Table (2CH Dual Edge Mode)

 VCH#0 ------> CVI#0.0 ---------> VOUT#0.0 -------> X_CAP#0 (VCH#0/VCH#1)
 VCH#1 ------> CVI#0.1 ----|
 VCH#2 ------> CVI#0.2 ---------> VOUT#0.1 -------> X_CAP#1 (VCH#2/VCH#3)
 VCH#3 ------> CVI#0.3 ----|
*******************************************************************************/

struct dh9920_dev_t {
    int                     index;                          ///< device index
    char                    name[16];                       ///< device name
    struct semaphore        lock;                           ///< device locker
    struct miscdevice       miscdev;                        ///< device node for ioctl access

    int                     pre_plugin[DH9920_DEV_CH_MAX];  ///< device channel previous cable plugin status
    int                     cur_plugin[DH9920_DEV_CH_MAX];  ///< device channel current  cable plugin status

    struct dh9920_ptz_t     ptz[DH9920_DEV_CH_MAX];         ///< device channel PTZ config

    int                     (*vfmt_notify)(int id, int ch, struct dh9920_video_fmt_t *vfmt);
    int                     (*vlos_notify)(int id, int ch, int vlos);
};

static struct dh9920_dev_t    dh9920_dev[DH9920_DEV_MAX];
static struct proc_dir_entry *dh9920_proc_root[DH9920_DEV_MAX]        = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_status[DH9920_DEV_MAX]      = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_video_fmt[DH9920_DEV_MAX]   = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_video_color[DH9920_DEV_MAX] = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_video_pos[DH9920_DEV_MAX]   = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_cable_type[DH9920_DEV_MAX]  = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_clear_eq[DH9920_DEV_MAX]    = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_audio_vol[DH9920_DEV_MAX]   = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_ptz_root[DH9920_DEV_MAX]    = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_ptz_cfg[DH9920_DEV_MAX]     = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *dh9920_proc_ptz_ctrl[DH9920_DEV_MAX]    = {[0 ... (DH9920_DEV_MAX - 1)] = NULL};

static audio_funs_t        dh9920_audio_funs;
static int                 dh9920_api_inited  = 0;
static struct i2c_client  *dh9920_i2c_client  = NULL;
static struct task_struct *dh9920_wd          = NULL;
static struct task_struct *dh9920_comm        = NULL;
static DEFINE_SEMAPHORE(dh9920_comm_lock);
static u32 DH9920_VCH_MAP[DH9920_DEV_MAX*DH9920_DEV_CH_MAX];

static struct dh9920_video_fmt_t dh9920_video_fmt_info[DH9920_VFMT_MAX] = {
    /* Idx                       Width   Height  Prog    FPS */
    {  DH9920_VFMT_720P25,       1280,   720,    1,      25  },  ///< 720P       @25
    {  DH9920_VFMT_720P30,       1280,   720,    1,      30  },  ///< 720P       @30
    {  DH9920_VFMT_720P50,       1280,   720,    1,      50  },  ///< 720P       @50
    {  DH9920_VFMT_720P60,       1280,   720,    1,      60  },  ///< 720P       @60
    {  DH9920_VFMT_1080P25,      1920,  1080,    1,      25  },  ///< 1080P      @25
    {  DH9920_VFMT_1080P30,      1920,  1080,    1,      30  },  ///< 1080P      @30
    {  DH9920_VFMT_960H25,        960,   576,    0,      25  },  ///< 960H       @25
    {  DH9920_VFMT_960H30,        960,   480,    0,      30  },  ///< 960H       @30
    {  DH9920_VFMT_HALF_720P25,   640,   720,    1,      25  },  ///< Half-720P  @25
    {  DH9920_VFMT_HALF_720P30,   640,   720,    1,      30  },  ///< Half-720P  @30
    {  DH9920_VFMT_HALF_1080P25,  960,  1080,    1,      25  },  ///< Half-1080P @25
    {  DH9920_VFMT_HALF_1080P30,  960,  1080,    1,      25  },  ///< Half-1080P @30
};

static int dh9920_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    dh9920_i2c_client = client;
    return 0;
}

static int dh9920_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id dh9920_i2c_id[] = {
    { "dh9920_i2c", 0 },
    { }
};

static struct i2c_driver dh9920_i2c_driver = {
    .driver = {
        .name  = "DH9920_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = dh9920_i2c_probe,
    .remove   = dh9920_i2c_remove,
    .id_table = dh9920_i2c_id
};

/* Video format mapping for u8VideoFormat from DHC_DH9920_SDK_GetVideoStatus() */
static int dh9920_vfmt_map(unsigned char vfmt)
{
    int ret;

    switch(vfmt) {
        case 0xB: ///< DHC_SD_PAL_BGHID
        case 0xA: ///< DHC_SD_PAL_CN
        case 0x9: ///< DHC_SD_PAL_60
        case 0x8: ///< DHC_SD_PAL_M
            ret = DH9920_VFMT_960H25;
            break;
        case 0x7: ///< DHC_SD_NTSC_443
        case 0x6: ///< DHC_SD_NTSC_JM
            ret = DH9920_VFMT_960H30;
            break;
        case 0x5: ///< DHC_HD_1080P_30HZ
            ret = DH9920_VFMT_1080P30;
            break;
        case 0x4: ///< DHC_HD_1080P_25HZ
            ret = DH9920_VFMT_1080P25;
            break;
        case 0x3: ///< DHC_HD_720P_60HZ
            ret = DH9920_VFMT_720P60;
            break;
        case 0x2: ///< DHC_HD_720P_50HZ
            ret = DH9920_VFMT_720P50;
            break;
        case 0x1: ///< DHC_HD_720P_30HZ
            ret = DH9920_VFMT_720P30;
            break;
        case 0x0: ///< DHC_HD_720P_25HZ
        default:
            ret = DH9920_VFMT_720P25;
            break;
    }
    return ret;
}

static int dh9920_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&dh9920_i2c_driver);
    if(ret < 0) {
        printk("DH9920 can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = iaddr[0]>>1;
    strlcpy(info.type, "dh9920_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("DH9920 can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("DH9920 can't add i2c device!\n");
        goto err_driver;
    }

    return 0;

err_driver:
   i2c_del_driver(&dh9920_i2c_driver);
   return -ENODEV;
}

static void dh9920_i2c_unregister(void)
{
    i2c_unregister_device(dh9920_i2c_client);
    i2c_del_driver(&dh9920_i2c_driver);
    dh9920_i2c_client = NULL;
}

static int dh9920_create_ch_map(int map_id)
{
    int i;

    switch(map_id) {
        case 1: ///< for GM8210/GM8287/GM8283, 2CH Dual Edge Mode
            for(i=0; i<dev_num; i++) {
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, DH9920_DEV_VOUT_0);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, DH9920_DEV_VOUT_0);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, DH9920_DEV_VOUT_1);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, DH9920_DEV_VOUT_1);
            }
            break;
        case 0: ///< for GM8210/GM8287/GM8286, 1CH Bypass Mode
        default:
            for(i=0; i<dev_num; i++) {
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, DH9920_DEV_VOUT_0);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, DH9920_DEV_VOUT_1);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, DH9920_DEV_VOUT_2);
                DH9920_VCH_MAP[(i*DH9920_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, DH9920_DEV_VOUT_3);
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
int dh9920_get_vch_id_ext(int id, DH9920_DEV_VOUT_T vout, int seq_id)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vout >= DH9920_DEV_VOUT_MAX || seq_id >= 2)
        return 0;

    if((vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE) || (vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) || (vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P))
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*DH9920_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(DH9920_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VOUT(DH9920_VCH_MAP[i]) == vout) &&
           ((CH_IDENTIFY_CVI(DH9920_VCH_MAP[i])%vport_chnum) == seq_id)) {
            return i;
        }
    }

    return 0;
}
EXPORT_SYMBOL(dh9920_get_vch_id_ext);

int dh9920_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= DH9920_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*DH9920_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(DH9920_VCH_MAP[i]) == id) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(dh9920_vin_to_vch);

int dh9920_get_vout_id(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= DH9920_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*DH9920_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(DH9920_VCH_MAP[i]) == id) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[i]) == vin_idx))
            return CH_IDENTIFY_VOUT(DH9920_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(dh9920_get_vout_id);

int dh9920_get_vout_seq_id(int id, int vin_idx)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vin_idx >= DH9920_DEV_CH_MAX)
        return 0;

    if((vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE) || (vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) || (vout_mode[id] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P))
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*DH9920_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(DH9920_VCH_MAP[i]) == id) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[i]) == vin_idx)) {
            return (vin_idx%vport_chnum);
        }
    }

    return 0;
}
EXPORT_SYMBOL(dh9920_get_vout_seq_id);

int dh9920_get_vout_format(int id)
{
    if(id >= dev_num)
        return DH9920_VOUT_FORMAT_BT656_DUAL_HEADER;
    else
        return vout_format[id];
}
EXPORT_SYMBOL(dh9920_get_vout_format);

int dh9920_get_vout_mode(int id)
{
    if(id >= dev_num)
        return DH9920_VOUT_MODE_1CH_BYPASS;
    else
        return vout_mode[id];
}
EXPORT_SYMBOL(dh9920_get_vout_mode);

int dh9920_get_device_num(void)
{
    return dev_num;
}
EXPORT_SYMBOL(dh9920_get_device_num);

int dh9920_notify_vfmt_register(int id, int (*nt_func)(int, int, struct dh9920_video_fmt_t *))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&dh9920_dev[id].lock);

    dh9920_dev[id].vfmt_notify = nt_func;

    up(&dh9920_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(dh9920_notify_vfmt_register);

void dh9920_notify_vfmt_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&dh9920_dev[id].lock);

    dh9920_dev[id].vfmt_notify = NULL;

    up(&dh9920_dev[id].lock);
}
EXPORT_SYMBOL(dh9920_notify_vfmt_deregister);

int dh9920_notify_vlos_register(int id, int (*nt_func)(int, int, int))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&dh9920_dev[id].lock);

    dh9920_dev[id].vlos_notify = nt_func;

    up(&dh9920_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(dh9920_notify_vlos_register);

void dh9920_notify_vlos_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&dh9920_dev[id].lock);

    dh9920_dev[id].vlos_notify = NULL;

    up(&dh9920_dev[id].lock);
}
EXPORT_SYMBOL(dh9920_notify_vlos_deregister);

static DHC_9920ErrID_E dh9920_writebyte(u8 addr, u8 reg, u8 data)
{
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(!dh9920_i2c_client) {
        printk("DH9920 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(dh9920_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = addr>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("DH9920(0x%02x) i2c write failed!!\n", addr);
        return -1;
    }

    return 0;
}

static u8 dh9920_readbyte(u8 addr, u8 reg)
{
    u8 reg_data = 0;
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(!dh9920_i2c_client) {
        printk("DH9920 i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(dh9920_i2c_client->dev.parent);

    msgs[0].addr  = addr>>1;
    msgs[0].flags = 0; /* write */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = (addr + 1)>>1;
    msgs[1].flags = 1; /* read */
    msgs[1].len   = 1;
    msgs[1].buf   = &reg_data;

    if(i2c_transfer(adapter, msgs, 2) != 2)
        printk("DH9920(0x%02x) i2c read failed!!\n", addr);

//printk("Read DH9920@0x%02x Reg@0x%02x\n", addr, reg);

    return reg_data;
}

static void dh9920_msleep(u8 msdly)
{
    msleep(msdly);
}

static void dh9920_printf(char *str)
{
    printk("%s\n", str);
}

/********************************************
  DH9920 COMM Message Buffer
  |-----------------| -
  |                 | |
  |     Header      | |
  |                 | |
  |-----------------| DH9920_COMM_MSG_BUF_MAX
  |                 | |
  |     Data        | |
  |                 | |
  |-----------------| -

 *********************************************/
static int dh9920_comm_cmd_send(int id, int ch, int cmd_id, void *cmd_data, u32 data_len, int wait_ack, int timeout)
{
    int ret = -1;
    struct plat_cpucomm_msg  tx_msg, rx_msg;
    struct dh9920_comm_header_t *p_header;
    unsigned char *p_data;
    unsigned char *msg_buf = NULL;

    if((id >= dev_num) || (ch >= DH9920_DEV_CH_MAX))
        return -1;

    if((data_len + sizeof(struct dh9920_comm_header_t)) > DH9920_COMM_MSG_BUF_MAX)
        return -1;

    down(&dh9920_comm_lock);

    /* allocate dh9920 comm message buffer */
    msg_buf = kzalloc(DH9920_COMM_MSG_BUF_MAX, GFP_KERNEL);
    if(!msg_buf)
        goto exit;

    p_header = (struct dh9920_comm_header_t *)msg_buf;
    p_data   = ((unsigned char *)p_header) + sizeof(struct dh9920_comm_header_t);

    /* process tx command */
    switch(cmd_id) {
        case DH9920_COMM_CMD_ACK_OK:
        case DH9920_COMM_CMD_ACK_FAIL:
        case DH9920_COMM_CMD_CLEAR_EQ:
            {
                p_header->magic    = DH9920_COMM_MSG_MAGIC;
                p_header->cmd_id   = cmd_id;
                p_header->data_len = 0;
                p_header->dev_id   = id;
                p_header->dev_ch   = ch;
                if(cmd_id <= DH9920_COMM_CMD_ACK_FAIL)
                    wait_ack = 0;
            }
            break;
        case DH9920_COMM_CMD_GET_VIDEO_STATUS:
        case DH9920_COMM_CMD_SET_COLOR:
        case DH9920_COMM_CMD_GET_COLOR:
        case DH9920_COMM_CMD_SEND_RS485:
        case DH9920_COMM_CMD_SET_VIDEO_POS:
        case DH9920_COMM_CMD_GET_VIDEO_POS:
        case DH9920_COMM_CMD_SET_AUDIO_VOL:
        case DH9920_COMM_CMD_SET_CABLE_TYPE:
        case DH9920_COMM_CMD_GET_PTZ_CFG:
            if(cmd_data) {
                p_header->magic    = DH9920_COMM_MSG_MAGIC;
                p_header->cmd_id   = cmd_id;
                p_header->data_len = data_len;
                p_header->dev_id   = id;
                p_header->dev_ch   = ch;
                memcpy(p_data, cmd_data, data_len);
            }
            else {
                ret = -1;
                goto exit;
            }
            break;
        default:
            ret = -1;
            goto exit;
    }

    /* prepare tx data */
    tx_msg.length   = sizeof(struct dh9920_comm_header_t) + p_header->data_len;
    tx_msg.msg_data = msg_buf;

    /* trigger data transmit */
    ret = plat_cpucomm_tx(&tx_msg, timeout);
    if(ret != 0)
        goto exit;

    /* wait to receive data or ack */
    if(wait_ack) {
        rx_msg.length   = DH9920_COMM_MSG_BUF_MAX;
        rx_msg.msg_data = msg_buf;
        ret = plat_cpucomm_rx(&rx_msg, timeout);
        if((ret != 0) || (rx_msg.length < sizeof(struct dh9920_comm_header_t)))
            goto exit;

        p_header = (struct dh9920_comm_header_t *)rx_msg.msg_data;
        p_data   = ((unsigned char *)p_header) + sizeof(struct dh9920_comm_header_t);

        if((p_header->magic == DH9920_COMM_MSG_MAGIC)) {
            switch(cmd_id) {
                /* ack with command data */
                case DH9920_COMM_CMD_GET_VIDEO_STATUS:
                case DH9920_COMM_CMD_GET_COLOR:
                case DH9920_COMM_CMD_GET_VIDEO_POS:
                case DH9920_COMM_CMD_GET_PTZ_CFG:
                    if((p_header->cmd_id != cmd_id)     ||
                       (p_header->data_len != data_len) ||
                       (p_header->dev_id != id)         ||
                       (p_header->dev_ch != ch)) {
                        ret = -1;
                        goto exit;
                    }
                    memcpy(cmd_data, p_data, p_header->data_len);
                    break;

                /* only ack */
                case DH9920_COMM_CMD_CLEAR_EQ:
                case DH9920_COMM_CMD_SET_COLOR:
                case DH9920_COMM_CMD_SEND_RS485:
                case DH9920_COMM_CMD_SET_VIDEO_POS:
                case DH9920_COMM_CMD_SET_AUDIO_VOL:
                case DH9920_COMM_CMD_SET_CABLE_TYPE:
                    if((p_header->cmd_id != DH9920_COMM_CMD_ACK_OK) ||
                       (p_header->dev_id != id)                     ||
                       (p_header->dev_ch != ch)) {
                        ret = -1;
                        goto exit;
                    }
                    break;
                default:
                    ret = -1;
                    goto exit;
            }
        }
        else {
            ret = -1;
            goto exit;
        }
    }

exit:
    if(msg_buf)
        kfree(msg_buf);

    up(&dh9920_comm_lock);

    return ret;
}

static int dh9920_proc_status_show(struct seq_file *sfile, void *v)
{
    int i, j, ret;
    int vlos;
    DHC_VideoStatus_S cvi_sts;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    seq_printf(sfile, "[DH9920#%d]\n", pdev->index);
    seq_printf(sfile, "------------------\n");
    seq_printf(sfile, "CVI    VCH    LOS \n");
    seq_printf(sfile, "==================\n");
    for(i=0; i<DH9920_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*DH9920_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(DH9920_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[j]) == i)) {
                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoStatus(pdev->index, i, &cvi_sts);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, i, DH9920_COMM_CMD_GET_VIDEO_STATUS, &cvi_sts, sizeof(cvi_sts), 1, 1000);
                }
                vlos = (ret == 0) ? cvi_sts.u8VideoLost : 0;
                seq_printf(sfile, "%-6d %-6d %-7s\n", i, j, ((vlos == 0) ? "NO" : "YES"));
            }
        }
    }



    return 0;
}

static int dh9920_proc_video_fmt_show(struct seq_file *sfile, void *v)
{
    int i, j, ret;
    DHC_VideoStatus_S cvi_sts;
    struct dh9920_video_fmt_t *p_vfmt;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    seq_printf(sfile, "[DH9920#%d] Incoming Video Fromat\n", pdev->index);
    seq_printf(sfile, "====================================================\n");
    seq_printf(sfile, "CVI   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "====================================================\n");

    for(i=0; i<DH9920_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*DH9920_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(DH9920_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[j]) == i)) {
                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoStatus(pdev->index, i, &cvi_sts);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, i, DH9920_COMM_CMD_GET_VIDEO_STATUS, &cvi_sts, sizeof(cvi_sts), 1, 1000);
                }

                if((ret == 0) && ((dh9920_vfmt_map(cvi_sts.u8VideoFormat) >= DH9920_VFMT_720P25) && ((dh9920_vfmt_map(cvi_sts.u8VideoFormat) < DH9920_VFMT_MAX)))) {
                    p_vfmt = &dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)];
                    seq_printf(sfile, "%-5d %-5d %-7d %-8d %-12s %-11d\n",
                               i,
                               j,
                               p_vfmt->width,
                               p_vfmt->height,
                               ((p_vfmt->prog == 1) ? "Progressive" : "Interlace"),
                               p_vfmt->frame_rate);
                }
                else {
                    seq_printf(sfile, "%-5d %-5d %-7s %-8s %-12s %-11s\n", i, j, "-", "-", "-", "-");
                }
            }
        }
    }

    return 0;
}

static int dh9920_proc_video_color_show(struct seq_file *sfile, void *v)
{
    int i, j, ret;
    DHC_VideoColor_S v_color;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    seq_printf(sfile, "[DH9920#%d] Video Color\n", pdev->index);
    seq_printf(sfile, "=======================================================\n");
    seq_printf(sfile, "CVI   VCH   BRI   CON   SAT   HUE   GAIN   W/B   SHARP \n");
    seq_printf(sfile, "=======================================================\n");

    for(i=0; i<DH9920_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*DH9920_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(DH9920_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[j]) == i)) {
                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetColor(pdev->index, i, &v_color, DHC_SET_MODE_USER);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, i, DH9920_COMM_CMD_GET_COLOR, &v_color, sizeof(v_color), 1, 1000);
                }

                if(ret == 0) {
                    seq_printf(sfile, "%-5d %-5d %-5d %-5d %-5d %-5d %-6d %-5d %-5d\n",
                               i,
                               j,
                               v_color.u8Brightness,
                               v_color.u8Contrast,
                               v_color.u8Saturation,
                               v_color.u8Hue,
                               v_color.u8Gain,
                               v_color.u8WhiteBalance,
                               v_color.u8Sharpness);
                }
                else {
                    seq_printf(sfile, "%-5d %-5d %-5d %-5d %-5d %-5d %-6d %-5d %-5d\n", i, j, 0, 0, 0, 0, 0, 0, 0);
                }
            }
        }
    }
    seq_printf(sfile, "=======================================================\n");
    seq_printf(sfile, "[0]Brightness  : 0 ~ 100\n");
    seq_printf(sfile, "[1]Contrast    : 0 ~ 100\n");
    seq_printf(sfile, "[2]Saturation  : 0 ~ 100\n");
    seq_printf(sfile, "[3]Hue         : 0 ~ 100\n");
    seq_printf(sfile, "[4]Gain        : Not support\n");
    seq_printf(sfile, "[5]WhiteBalance: Not support\n");
    seq_printf(sfile, "[6]Sharpness   : 0 ~ 15\n");
    seq_printf(sfile, "=======================================================\n");
    seq_printf(sfile, "echo [CVI#] [PARAM#] [VALUE] for parameter setup\n\n");

    return 0;
}

static int dh9920_proc_video_pos_show(struct seq_file *sfile, void *v)
{
    int i, j, ret;
    DHC_VideoOffset_S vpos;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    seq_printf(sfile, "[DH9920#%d] Video Position\n", pdev->index);
    seq_printf(sfile, "----------------------------------\n");
    seq_printf(sfile, "CVI    VCH    H_Offset    V_Offset\n");
    seq_printf(sfile, "==================================\n");
    for(i=0; i<DH9920_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*DH9920_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(DH9920_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[j]) == i)) {
                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoPos(pdev->index, i, &vpos);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, i, DH9920_COMM_CMD_GET_VIDEO_POS, &vpos, sizeof(vpos), 1, 1000);
                }

                if(ret == 0)
                    seq_printf(sfile, "%-6d %-6d %-11d %-11d\n", i, j, vpos.u8HOffset, vpos.u8VOffset);
                else
                    seq_printf(sfile, "%-6d %-6d %-11d %-11d\n", i, j, 0, 0);
            }
        }
    }
    seq_printf(sfile, "==================================\n");
    seq_printf(sfile, "echo [CVI#] [H_Offset] [V_Offset] for video position setup\n\n");

    return 0;
}

static int dh9920_proc_cable_type_show(struct seq_file *sfile, void *v)
{
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[DH9920#%d] Cable Type\n", pdev->index);
    seq_printf(sfile, "========================================\n");
    seq_printf(sfile, "[0]COAXIAL\n");
    seq_printf(sfile, "[1]UTP_10OHM\n");
    seq_printf(sfile, "[2]UTP_17OHM\n");
    seq_printf(sfile, "[3]UTP_25OHM\n");
    seq_printf(sfile, "[4]UTP_35OHM\n");
    seq_printf(sfile, "========================================\n");
    seq_printf(sfile, "echo [CVI#] [TYPE#] for cable type setup\n\n");

    up(&pdev->lock);

    return 0;
}

static int dh9920_proc_clear_eq_show(struct seq_file *sfile, void *v)
{
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[DH9920#%d] Clear EQ\n", pdev->index);
    seq_printf(sfile, "========================================\n");
    seq_printf(sfile, "echo [CVI#] for trigger channel EQ clear\n\n");

    up(&pdev->lock);

    return 0;
}

static int dh9920_proc_audio_vol_show(struct seq_file *sfile, void *v)
{
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;
    unsigned char reg_0xAA, reg_0xAB, reg_0xAC;

    down(&pdev->lock);

    seq_printf(sfile, "\n[DH9920#%d]\nAudio Record Volume\n", pdev->index);
    seq_printf(sfile, "=======================\n");
    seq_printf(sfile, "VCH    Volume\n");
    seq_printf(sfile, "=======================\n");

    reg_0xAA = dh9920_readbyte(iaddr[pdev->index],0xaa);
    reg_0xAB = dh9920_readbyte(iaddr[pdev->index],0xab);
    reg_0xAC = dh9920_readbyte(iaddr[pdev->index],0xac);
    
    seq_printf(sfile, "%-6d 0x%X\n", 0+((pdev->index)*5), (reg_0xAA&0xF0)>>4);
    seq_printf(sfile, "%-6d 0x%X\n", 1+((pdev->index)*5), (reg_0xAB&0xF0)>>4);
    seq_printf(sfile, "%-6d 0x%X\n", 2+((pdev->index)*5), (reg_0xAB&0x0F));
    seq_printf(sfile, "%-6d 0x%X\n", 3+((pdev->index)*5), (reg_0xAC&0xF0)>>4);
    seq_printf(sfile, "%-6d 0x%X\n\n", 4+((pdev->index)*5), (reg_0xAC&0x0F));

    seq_printf(sfile, "Audio Playback Volume\n");
    seq_printf(sfile, "=======================\n");
    seq_printf(sfile, "VCH    Volume\n");
    seq_printf(sfile, "=======================\n");
    seq_printf(sfile, "%-6d 0x%X\n\n", 0, (reg_0xAA&0x0f));

    up(&pdev->lock);

    return 0;
}

static int dh9920_proc_ptz_cfg_show(struct seq_file *sfile, void *v)
{
    int i, j, ret = 0;
    struct dh9920_ptz_t ptz_cfg;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;
    char *ptz_prot_str[] = {"DH_SD1"};
    char *ptz_baud_str[] = {"1200", "2400", "4800", "9600", "19200", "38400"};

    seq_printf(sfile, "[DH9920#%d] PTZ Configuration\n", pdev->index);
    seq_printf(sfile, "---------------------------------------------\n");
    seq_printf(sfile, "CVI   VCH   PROTOCOL   BAUD_RATE   PARITY_CHK\n");
    seq_printf(sfile, "=============================================\n");
    for(i=0; i<DH9920_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*DH9920_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(DH9920_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_CVI(DH9920_VCH_MAP[j]) == i)) {
                if(init) {
                    down(&pdev->lock);

                    memcpy(&ptz_cfg, &pdev->ptz[i], sizeof(struct dh9920_ptz_t));

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, i, DH9920_COMM_CMD_GET_PTZ_CFG, &ptz_cfg, sizeof(struct dh9920_ptz_t), 1, 1000);
                }
                if(ret == 0) {
                    seq_printf(sfile, "%-5d %-5d %-10s %-11s %-10s\n", i, j,
                               (ptz_cfg.protocol < DH9920_PTZ_PTOTOCOL_MAX) ? ptz_prot_str[ptz_cfg.protocol] : "Unknown",
                               (ptz_cfg.baud_rate < DH9920_PTZ_BAUD_MAX) ? ptz_baud_str[ptz_cfg.baud_rate] : "Unknown",
                               (ptz_cfg.parity_chk == 0) ? "No" : "Yes");
                }
                else {
                    seq_printf(sfile, "%-5d %-5d %-10s %-11s %-10s\n", i, j, "Unknown", "Unknown", "No");
                }
            }
        }
    }

    return 0;
}

static int dh9920_proc_ptz_ctrl_show(struct seq_file *sfile, void *v)
{
    int i;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[DH9920#%d]\n", pdev->index);

    for(i=0; i<DH9920_PTZ_PTOTOCOL_MAX; i++) {
        switch(i) {
            case DH9920_PTZ_PTOTOCOL_DHSD1:
                seq_printf(sfile, "<< DH_SD1 Command List >>\n");
                seq_printf(sfile, "=========================\n");
                seq_printf(sfile, "[0]Menu_CLOSE\n");
                seq_printf(sfile, "[1]Menu_OPEN\n");
                seq_printf(sfile, "[2]Menu_BACK\n");
                seq_printf(sfile, "[2]Menu_NEXT\n");
                seq_printf(sfile, "[4]Menu_UP\n");
                seq_printf(sfile, "[5]Menu_DOWN\n");
                seq_printf(sfile, "[6]Menu_LEFT\n");
                seq_printf(sfile, "[7]Menu_RIGHT\n");
                seq_printf(sfile, "[8]Menu_ENTER\n");
                seq_printf(sfile, "=========================\n");
                seq_printf(sfile, "echo [CVI#] [CAMERA#(0~255)] [CMD#] for remoet camrea control\n\n");
                break;
            default:
                break;
        }
    }

    up(&pdev->lock);

    return 0;
}

static int dh9920_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_status_show, PDE(inode)->data);
}

static int dh9920_proc_video_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_video_fmt_show, PDE(inode)->data);
}

static int dh9920_proc_video_color_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_video_color_show, PDE(inode)->data);
}

static int dh9920_proc_video_pos_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_video_pos_show, PDE(inode)->data);
}

static int dh9920_proc_cable_type_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_cable_type_show, PDE(inode)->data);
}

static int dh9920_proc_clear_eq_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_clear_eq_show, PDE(inode)->data);
}

static int dh9920_proc_audio_vol_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_audio_vol_show, PDE(inode)->data);
}

static int dh9920_proc_ptz_cfg_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_ptz_cfg_show, PDE(inode)->data);
}

static int dh9920_proc_ptz_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, dh9920_proc_ptz_ctrl_show, PDE(inode)->data);
}

static ssize_t dh9920_proc_video_color_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int ret, cvi_ch, param_id, param_value;
    DHC_VideoColor_S video_color;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d %d\n", &cvi_ch, &param_id, &param_value);

    if((cvi_ch >= DH9920_DEV_CH_MAX) || (param_id >= 7))
        goto exit;

    if(init) {
        down(&pdev->lock);

        ret = DHC_DH9920_SDK_GetColor(pdev->index, cvi_ch, &video_color, DHC_SET_MODE_USER);

        up(&pdev->lock);
    }
    else {
        ret = dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_GET_COLOR, &video_color, sizeof(video_color), 1, 1000);
    }
    if(ret != 0)
        goto exit;

    switch(param_id) {
        case 0:
            video_color.u8Brightness = (DHC_U8)param_value;
            break;
        case 1:
            video_color.u8Contrast = (DHC_U8)param_value;
            break;
        case 2:
            video_color.u8Saturation = (DHC_U8)param_value;
            break;
        case 3:
            video_color.u8Hue = (DHC_U8)param_value;
            break;
        case 4:
            video_color.u8Gain = (DHC_U8)param_value;
            break;
        case 5:
            video_color.u8WhiteBalance = (DHC_U8)param_value;
            break;
        case 6:
            video_color.u8Sharpness = (DHC_U8)param_value;
            break;
        default:
            break;
    }

    if(init) {
        down(&pdev->lock);

        DHC_DH9920_SDK_SetColor(pdev->index, cvi_ch, &video_color, DHC_SET_MODE_USER);

        up(&pdev->lock);
    }
    else {
        dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_SET_COLOR, &video_color, sizeof(video_color), 1, 1000);
    }

exit:
    return count;
}

static ssize_t dh9920_proc_video_pos_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cvi_ch, h_offset, v_offset;
    DHC_VideoOffset_S video_pos;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d %d\n", &cvi_ch, &h_offset, &v_offset);

    if(cvi_ch >= DH9920_DEV_CH_MAX)
        goto exit;

    video_pos.u8HOffset = h_offset;
    video_pos.u8VOffset = v_offset;

    if(init) {
        down(&pdev->lock);

        DHC_DH9920_SDK_SetVideoPos(pdev->index, cvi_ch, &video_pos);

        up(&pdev->lock);
    }
    else {
        dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_SET_VIDEO_POS, &video_pos, sizeof(video_pos), 1, 1000);
    }

exit:
    return count;
}

static ssize_t dh9920_proc_cable_type_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cvi_ch, cable_type;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d\n", &cvi_ch, &cable_type);

    if(cvi_ch >= DH9920_DEV_CH_MAX)
        goto exit;

    if(init) {
        down(&pdev->lock);

        DHC_DH9920_SDK_SetCableType(pdev->index, cvi_ch, cable_type);

        up(&pdev->lock);
    }
    else {
        dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_SET_CABLE_TYPE, &cable_type, sizeof(DH9920_CABLE_TYPE_T), 1, 1000);
    }

exit:
    return count;
}

static ssize_t dh9920_proc_clear_eq_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cvi_ch;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &cvi_ch);

    if(cvi_ch >= DH9920_DEV_CH_MAX)
        goto exit;

    if(init) {
        down(&pdev->lock);

        DHC_DH9920_SDK_ClearEq(pdev->index, cvi_ch);

        up(&pdev->lock);
    }
    else {
        dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_CLEAR_EQ, NULL, 0, 1, 2000);
    }

exit:
    return count;
}

static ssize_t dh9920_proc_audio_vol_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cvi_ch, volume, in_out;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d %d\n", &in_out, &cvi_ch, &volume);

    if(cvi_ch > DH9920_DEV_CH_MAX)
        goto exit;

    if(init) {
        down(&pdev->lock);
        if (in_out == 0) {
            unsigned char val=0;

            if (cvi_ch == 0) {
                val = dh9920_readbyte(iaddr[pdev->index],0xAA);
                val &= ~(0xF0);
                val |= ((volume&0xF)<<4);
                dh9920_writebyte(iaddr[pdev->index],0xAA,val);
            } else if (cvi_ch == 1) {
                val = dh9920_readbyte(iaddr[pdev->index],0xAB);
                val &= ~(0xF0);
                val |= ((volume&0xF)<<4);
                dh9920_writebyte(iaddr[pdev->index],0xAB,val);
            } else if (cvi_ch == 2) {
                val = dh9920_readbyte(iaddr[pdev->index],0xAB);
                val &= ~(0x0F);
                val |= (volume&0xF);
                dh9920_writebyte(iaddr[pdev->index],0xAB,val);
            } else if (cvi_ch == 3) {
                val = dh9920_readbyte(iaddr[pdev->index],0xAC);
                val &= ~(0xF0);
                val |= ((volume&0xF)<<4);
                dh9920_writebyte(iaddr[pdev->index],0xAC,val);
            } else if (cvi_ch == 4) {
                val = dh9920_readbyte(iaddr[pdev->index],0xAC);
                val &= ~(0x0F);
                val |= (volume&0xF);
                dh9920_writebyte(iaddr[pdev->index],0xAC,val);
            }

            //DHC_DH9920_SDK_SetLineInVolume(pdev->index, cvi_ch, (DHC_U8)volume);
        }
        else
            DHC_DH9920_SDK_SetLineOutVolume(pdev->index, (DHC_U8)volume);
        up(&pdev->lock);
    }
    else {
        dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_SET_AUDIO_VOL, (DHC_U8 *)&volume, sizeof(DHC_U8), 1, 1000);
    }

exit:
    return count;
}

static ssize_t dh9920_proc_ptz_ctrl_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cvi_ch, cam_id, cmd, ret, protocol;
    char rs485_cmd[64];
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d %d\n", &cvi_ch, &cam_id, &cmd);

    if(cvi_ch >= DH9920_DEV_CH_MAX)
        goto exit;

    /* get ptz protocol */
    if(init) {
        protocol = pdev->ptz[cvi_ch].protocol;
    }
    else {
        struct dh9920_ptz_t ptz_cfg;

        ret = dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_GET_PTZ_CFG, &ptz_cfg, sizeof(ptz_cfg), 1, 1000);
        if(ret != 0)
            goto exit;

        protocol = ptz_cfg.protocol;
    }

    switch(protocol) {
        case DH9920_PTZ_PTOTOCOL_DHSD1:
            /*
             * Prepare DH_SD1 PTZ RS485 Command
             * [SYNC CAM_ID CMD1 DATA1 DATA2 DATA3 CRC]
             */
            rs485_cmd[0] = 0xa5;
            rs485_cmd[1] = (u8)cam_id;
            rs485_cmd[2] = 0x89;
            rs485_cmd[3] = (u8)cmd;
            rs485_cmd[4] = 0x00;
            rs485_cmd[5] = 0x00;
            rs485_cmd[6] = (rs485_cmd[0] + rs485_cmd[1] + rs485_cmd[2] + rs485_cmd[3] + rs485_cmd[4] + rs485_cmd[5])%0x100;

            if(init) {
                down(&pdev->lock);

                ret = DHC_DH9920_SDK_SendCo485Enable(pdev->index, cvi_ch, 1);

                if(ret == 0)
                    ret = DHC_DH9920_SDK_SendCo485Buf(pdev->index, cvi_ch, rs485_cmd, 7);

                DHC_DH9920_SDK_SendCo485Enable(pdev->index, cvi_ch, 0);

                up(&pdev->lock);
            }
            else {
                struct dh9920_comm_send_rs485_t comm_rs485;

                comm_rs485.buf_len = 7;
                memcpy(comm_rs485.cmd_buf, rs485_cmd, comm_rs485.buf_len);

                dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_SEND_RS485, &comm_rs485, sizeof(comm_rs485), 1, 1000);
            }
            break;
        default:
            break;
    }

exit:
    return count;
}

static struct file_operations dh9920_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_video_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_video_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_video_color_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_video_color_open,
    .write  = dh9920_proc_video_color_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_video_pos_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_video_pos_open,
    .write  = dh9920_proc_video_pos_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_cable_type_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_cable_type_open,
    .write  = dh9920_proc_cable_type_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_clear_eq_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_clear_eq_open,
    .write  = dh9920_proc_clear_eq_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_audio_vol_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_audio_vol_open,
    .write  = dh9920_proc_audio_vol_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_ptz_cfg_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_ptz_cfg_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations dh9920_proc_ptz_ctrl_ops = {
    .owner  = THIS_MODULE,
    .open   = dh9920_proc_ptz_ctrl_open,
    .write  = dh9920_proc_ptz_ctrl_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void dh9920_proc_remove(int id)
{
    if(id >= DH9920_DEV_MAX)
        return;

    if(dh9920_proc_root[id]) {
        if(dh9920_proc_ptz_root[id]) {
            if(dh9920_proc_ptz_cfg[id]) {
                remove_proc_entry(dh9920_proc_ptz_cfg[id]->name, dh9920_proc_ptz_root[id]);
                dh9920_proc_ptz_cfg[id] = NULL;
            }

            if(dh9920_proc_ptz_ctrl[id]) {
                remove_proc_entry(dh9920_proc_ptz_ctrl[id]->name, dh9920_proc_ptz_root[id]);
                dh9920_proc_ptz_ctrl[id] = NULL;
            }

            remove_proc_entry(dh9920_proc_ptz_root[id]->name, dh9920_proc_root[id]);
            dh9920_proc_ptz_root[id] = NULL;
        }

        if(dh9920_proc_status[id]) {
            remove_proc_entry(dh9920_proc_status[id]->name, dh9920_proc_root[id]);
            dh9920_proc_status[id] = NULL;
        }

        if(dh9920_proc_video_fmt[id]) {
            remove_proc_entry(dh9920_proc_video_fmt[id]->name, dh9920_proc_root[id]);
            dh9920_proc_video_fmt[id] = NULL;
        }

        if(dh9920_proc_video_color[id]) {
            remove_proc_entry(dh9920_proc_video_color[id]->name, dh9920_proc_root[id]);
            dh9920_proc_video_color[id] = NULL;
        }

        if(dh9920_proc_video_pos[id]) {
            remove_proc_entry(dh9920_proc_video_pos[id]->name, dh9920_proc_root[id]);
            dh9920_proc_video_pos[id] = NULL;
        }

        if(dh9920_proc_cable_type[id]) {
            remove_proc_entry(dh9920_proc_cable_type[id]->name, dh9920_proc_root[id]);
            dh9920_proc_cable_type[id] = NULL;
        }

        if(dh9920_proc_clear_eq[id]) {
            remove_proc_entry(dh9920_proc_clear_eq[id]->name, dh9920_proc_root[id]);
            dh9920_proc_clear_eq[id] = NULL;
        }

        if(dh9920_proc_audio_vol[id]) {
            remove_proc_entry(dh9920_proc_audio_vol[id]->name, dh9920_proc_root[id]);
            dh9920_proc_audio_vol[id] = NULL;
        }

        remove_proc_entry(dh9920_proc_root[id]->name, NULL);
        dh9920_proc_root[id] = NULL;
    }
}

static int dh9920_proc_init(int id)
{
    int ret = 0;

    if(id >= DH9920_DEV_MAX)
        return -1;

    /* root */
    dh9920_proc_root[id] = create_proc_entry(dh9920_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!dh9920_proc_root[id]) {
        printk("create proc node '%s' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_root[id]->owner = THIS_MODULE;
#endif

    /* status */
    dh9920_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_status[id]->proc_fops = &dh9920_proc_status_ops;
    dh9920_proc_status[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* frame format */
    dh9920_proc_video_fmt[id] = create_proc_entry("video_format", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_video_fmt[id]) {
        printk("create proc node '%s/video_format' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_video_fmt[id]->proc_fops = &dh9920_proc_video_fmt_ops;
    dh9920_proc_video_fmt[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_video_fmt[id]->owner     = THIS_MODULE;
#endif

    /* video color */
    dh9920_proc_video_color[id] = create_proc_entry("video_color", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_video_color[id]) {
        printk("create proc node '%s/video_color' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_video_color[id]->proc_fops = &dh9920_proc_video_color_ops;
    dh9920_proc_video_color[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_video_color[id]->owner     = THIS_MODULE;
#endif

    /* video position */
    dh9920_proc_video_pos[id] = create_proc_entry("video_pos", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_video_pos[id]) {
        printk("create proc node '%s/video_pos' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_video_pos[id]->proc_fops = &dh9920_proc_video_pos_ops;
    dh9920_proc_video_pos[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_video_pos[id]->owner     = THIS_MODULE;
#endif

    /* cable type */
    dh9920_proc_cable_type[id] = create_proc_entry("cable_type", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_cable_type[id]) {
        printk("create proc node '%s/cable_type' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_cable_type[id]->proc_fops = &dh9920_proc_cable_type_ops;
    dh9920_proc_cable_type[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_cable_type[id]->owner     = THIS_MODULE;
#endif

    /* clear EQ */
    dh9920_proc_clear_eq[id] = create_proc_entry("clear_eq", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_clear_eq[id]) {
        printk("create proc node '%s/clear_eq' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_clear_eq[id]->proc_fops = &dh9920_proc_clear_eq_ops;
    dh9920_proc_clear_eq[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_clear_eq[id]->owner     = THIS_MODULE;
#endif

    /* audio volume */
    dh9920_proc_audio_vol[id] = create_proc_entry("audio_volume", S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_audio_vol[id]) {
        printk("create proc node '%s/audio_volume' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_audio_vol[id]->proc_fops = &dh9920_proc_audio_vol_ops;
    dh9920_proc_audio_vol[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_audio_vol[id]->owner     = THIS_MODULE;
#endif

    /* ptz root */
    dh9920_proc_ptz_root[id] = create_proc_entry("ptz", S_IFDIR|S_IRUGO|S_IXUGO, dh9920_proc_root[id]);
    if(!dh9920_proc_ptz_root[id]) {
        printk("create proc node '%s/ptz' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_ptz_root[id]->owner = THIS_MODULE;
#endif

    /* ptz config */
    dh9920_proc_ptz_cfg[id] = create_proc_entry("cfg", S_IRUGO|S_IXUGO, dh9920_proc_ptz_root[id]);
    if(!dh9920_proc_ptz_cfg[id]) {
        printk("create proc node '%s/ptz/cfg' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_ptz_cfg[id]->proc_fops = &dh9920_proc_ptz_cfg_ops;
    dh9920_proc_ptz_cfg[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_ptz_cfg[id]->owner     = THIS_MODULE;
#endif

    /* ptz control */
    dh9920_proc_ptz_ctrl[id] = create_proc_entry("control", S_IRUGO|S_IXUGO, dh9920_proc_ptz_root[id]);
    if(!dh9920_proc_ptz_ctrl[id]) {
        printk("create proc node '%s/ptz/control' failed!\n", dh9920_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    dh9920_proc_ptz_ctrl[id]->proc_fops = &dh9920_proc_ptz_ctrl_ops;
    dh9920_proc_ptz_ctrl[id]->data      = (void *)&dh9920_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dh9920_proc_ptz_ctrl[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    dh9920_proc_remove(id);
    return ret;
}

static int dh9920_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct dh9920_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(dh9920_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &dh9920_dev[i];
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

static int dh9920_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long dh9920_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct dh9920_dev_t *pdev = (struct dh9920_dev_t *)file->private_data;

    if(_IOC_TYPE(cmd) != DH9920_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case DH9920_GET_NOVID:
            {
                struct dh9920_ioc_data_t ioc_data;
                DHC_VideoStatus_S cvi_sts;

                if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_data.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoStatus(pdev->index, ioc_data.cvi_ch, &cvi_sts);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_data.cvi_ch, DH9920_COMM_CMD_GET_VIDEO_STATUS, &cvi_sts, sizeof(cvi_sts), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_data.data = cvi_sts.u8VideoLost;

                ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            }
            break;

        case DH9920_GET_VIDEO_FMT:
            {
                DHC_VideoStatus_S cvi_sts;
                struct dh9920_ioc_vfmt_t ioc_vfmt;

                if(copy_from_user(&ioc_vfmt, (void __user *)arg, sizeof(ioc_vfmt))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vfmt.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoStatus(pdev->index, ioc_vfmt.cvi_ch, &cvi_sts);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vfmt.cvi_ch, DH9920_COMM_CMD_GET_VIDEO_STATUS, &cvi_sts, sizeof(cvi_sts), 1, 1000);
                }

                if((ret != 0) || ((dh9920_vfmt_map(cvi_sts.u8VideoFormat) >= DH9920_VFMT_MAX))) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_vfmt.width      = dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)].width;
                ioc_vfmt.height     = dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)].height;
                ioc_vfmt.prog       = dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)].prog;
                ioc_vfmt.frame_rate = dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)].frame_rate;

                ret = (copy_to_user((void __user *)arg, &ioc_vfmt, sizeof(ioc_vfmt))) ? (-EFAULT) : 0;
            }
            break;

        case DH9920_GET_VIDEO_COLOR:
            {
                struct dh9920_ioc_vcolor_t ioc_vcolor;
                DHC_VideoColor_S vcol;

                if(copy_from_user(&ioc_vcolor, (void __user *)arg, sizeof(ioc_vcolor))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vcolor.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetColor(pdev->index, ioc_vcolor.cvi_ch, &vcol, DHC_SET_MODE_USER);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vcolor.cvi_ch, DH9920_COMM_CMD_GET_COLOR, &vcol, sizeof(vcol), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_vcolor.brightness    = vcol.u8Brightness;
                ioc_vcolor.contrast      = vcol.u8Contrast;
                ioc_vcolor.saturation    = vcol.u8Saturation;
                ioc_vcolor.hue           = vcol.u8Hue;
                ioc_vcolor.gain          = vcol.u8Gain;
                ioc_vcolor.white_balance = vcol.u8WhiteBalance;
                ioc_vcolor.sharpness     = vcol.u8Sharpness;

                ret = (copy_to_user((void __user *)arg, &ioc_vcolor, sizeof(ioc_vcolor))) ? (-EFAULT) : 0;
            }
            break;

        case DH9920_SET_VIDEO_COLOR:
            {
                struct dh9920_ioc_vcolor_t ioc_vcolor;
                DHC_VideoColor_S vcol;

                if(copy_from_user(&ioc_vcolor, (void __user *)arg, sizeof(ioc_vcolor))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vcolor.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                vcol.u8Brightness   = ioc_vcolor.brightness;
                vcol.u8Contrast     = ioc_vcolor.contrast;
                vcol.u8Saturation   = ioc_vcolor.saturation;
                vcol.u8Hue          = ioc_vcolor.hue;
                vcol.u8Gain         = ioc_vcolor.gain;
                vcol.u8WhiteBalance = ioc_vcolor.white_balance;
                vcol.u8Sharpness    = ioc_vcolor.sharpness;

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_SetColor(pdev->index, ioc_vcolor.cvi_ch, &vcol, DHC_SET_MODE_USER);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vcolor.cvi_ch, DH9920_COMM_CMD_SET_COLOR, &vcol, sizeof(vcol), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        case DH9920_GET_VIDEO_POS:
            {
                DHC_VideoOffset_S vpos;
                struct dh9920_ioc_vpos_t ioc_vpos;

                if(copy_from_user(&ioc_vpos, (void __user *)arg, sizeof(ioc_vpos))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vpos.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_GetVideoPos(pdev->index, ioc_vpos.cvi_ch, &vpos);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vpos.cvi_ch, DH9920_COMM_CMD_GET_VIDEO_POS, &vpos, sizeof(vpos), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_vpos.h_offset = vpos.u8HOffset;
                ioc_vpos.v_offset = vpos.u8VOffset;

                ret = (copy_to_user((void __user *)arg, &ioc_vpos, sizeof(ioc_vpos))) ? (-EFAULT) : 0;
            }
            break;

        case DH9920_SET_VIDEO_POS:
            {
                DHC_VideoOffset_S vpos;
                struct dh9920_ioc_vpos_t ioc_vpos;

                if(copy_from_user(&ioc_vpos, (void __user *)arg, sizeof(ioc_vpos))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vpos.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                vpos.u8HOffset = ioc_vpos.h_offset;
                vpos.u8VOffset = ioc_vpos.v_offset;

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_SetVideoPos(pdev->index, ioc_vpos.cvi_ch, &vpos);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vpos.cvi_ch, DH9920_COMM_CMD_SET_VIDEO_POS, &vpos, sizeof(vpos), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        case DH9920_SET_CABLE_TYPE:
            {
                struct dh9920_ioc_cable_t ioc_cable;

                if(copy_from_user(&ioc_cable, (void __user *)arg, sizeof(ioc_cable))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_cable.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_SetCableType(pdev->index, ioc_cable.cvi_ch, ioc_cable.cab_type);

                    up(&pdev->lock);
                }
                else {
                    dh9920_comm_cmd_send(pdev->index, ioc_cable.cvi_ch, DH9920_COMM_CMD_SET_CABLE_TYPE, &ioc_cable.cab_type, sizeof(DH9920_CABLE_TYPE_T), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        case DH9920_RS485_TX:
            {
                struct dh9920_ioc_rs485_tx_t ioc_rs485;

                if(copy_from_user(&ioc_rs485, (void __user *)arg, sizeof(ioc_rs485))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_rs485.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_SendCo485Enable(pdev->index, ioc_rs485.cvi_ch, 1);

                    if(ret == 0)
                        ret = DHC_DH9920_SDK_SendCo485Buf(pdev->index, ioc_rs485.cvi_ch, ioc_rs485.cmd_buf, ioc_rs485.buf_len);

                    DHC_DH9920_SDK_SendCo485Enable(pdev->index, ioc_rs485.cvi_ch, 0);

                    up(&pdev->lock);
                }
                else {
                    struct dh9920_comm_send_rs485_t comm_rs485;

                    comm_rs485.buf_len = ioc_rs485.buf_len;
                    memcpy(comm_rs485.cmd_buf, ioc_rs485.cmd_buf, comm_rs485.buf_len);

                    ret = dh9920_comm_cmd_send(pdev->index, ioc_rs485.cvi_ch, DH9920_COMM_CMD_SEND_RS485, &comm_rs485, sizeof(comm_rs485), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        case DH9920_CLEAR_EQ:
            {
                int cvi_ch;

                if(copy_from_user(&cvi_ch, (void __user *)arg, sizeof(cvi_ch))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_ClearEq(pdev->index, (DHC_U8)cvi_ch);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, cvi_ch, DH9920_COMM_CMD_CLEAR_EQ, NULL, 0, 1, 2000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        case DH9920_SET_AUDIO_VOL:
            {
                struct dh9920_ioc_audio_vol_t ioc_vol;

                if(copy_from_user(&ioc_vol, (void __user *)arg, sizeof(ioc_vol))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vol.cvi_ch >= DH9920_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(init) {
                    down(&pdev->lock);

                    ret = DHC_DH9920_SDK_SetAudioInVolume(pdev->index, ioc_vol.cvi_ch, ioc_vol.volume);

                    up(&pdev->lock);
                }
                else {
                    ret = dh9920_comm_cmd_send(pdev->index, ioc_vol.cvi_ch, DH9920_COMM_CMD_SET_AUDIO_VOL, &ioc_vol.volume, sizeof(ioc_vol.volume), 1, 1000);
                }

                if(ret != 0) {
                    ret = -EFAULT;
                    goto exit;
                }
            }
            break;

        default:
            ret = -ENOIOCTLCMD;
            break;
    }

exit:
    return ret;
}

static struct file_operations dh9920_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = dh9920_miscdev_open,
    .release        = dh9920_miscdev_release,
    .unlocked_ioctl = dh9920_miscdev_ioctl,
};

static int dh9920_miscdev_init(int id)
{
    int ret;

    if(id >= DH9920_DEV_MAX)
        return -1;

    /* clear */
    memset(&dh9920_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    dh9920_dev[id].miscdev.name  = dh9920_dev[id].name;
    dh9920_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    dh9920_dev[id].miscdev.fops  = (struct file_operations *)&dh9920_miscdev_fops;
    ret = misc_register(&dh9920_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", dh9920_dev[id].name);
        dh9920_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

static void dh9920_hw_reset(void)
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

static void dh9920_audio_set_channel_sequence(void)
{
    int idx;
    const unsigned char seq_reg_left[4] = {0xC3,0xC4,0xC5,0xC6};
    const unsigned char seq_reg_right[4] = {0xC7,0xC8,0xC9,0xCA};
    const unsigned char seq_val[8] = {0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE};

    if (dev_num == 1) {
        for (idx=0; idx<dev_num; idx++) {
            dh9920_writebyte(iaddr[0], seq_reg_left[idx], seq_val[idx]);
            dh9920_writebyte(iaddr[0], seq_reg_right[idx], seq_val[idx+1]);
        }
    } else if ((dev_num == 2) || (dev_num == 4)) {
        for (idx=0; idx<dev_num; idx++) {
            dh9920_writebyte(iaddr[0], seq_reg_left[idx], seq_val[dev_num-1-idx]);
            dh9920_writebyte(iaddr[0], seq_reg_right[idx], seq_val[(dev_num-1-idx)+dev_num]);
        }
    } else
        printk("device num %d is not support!\n", dev_num);

}

static int dh9920_device_register(void)
{
    int i, j, ret;
    DHC_Dh9920UsrCfg_S  dev_attr;
    DHC_VideoColor_S    video_color;

    if(dh9920_api_inited)
        return -1;

    /* clear attr */
    memset(&dev_attr, 0, sizeof(DHC_Dh9920UsrCfg_S));

    /* device number on board */
    dev_attr.u8AdCount = dev_num;

    for(i=0; i<dev_num; i++) {
        /* device i2c address */
        dev_attr.stDh9920Attr[i].u8ChipAddr = iaddr[i];

        /* video attr */
        if((vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE) || (vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) || (vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P)) {
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.VoTmdMode = 1;
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.VoClkEdge = DHC_VOCLK_EDGE_DUAL;    ///< 2-CH dual edge
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.HdGmMode = 1;                       ///< For 2-CH dual edge 74.25 MHz clock output
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[0] = 0x10;                   ///< VIN#0,1 -> VPORT#0
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[1] = 0x32;                   ///< VIN#2,3 -> VPORT#1
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[2] = 0xFF;
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[3] = 0xFF;
        }
        else { ///< 1-CH Bypass
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.VoTmdMode = 0;
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.VoClkEdge = DHC_VOCLK_EDGE_RISING;  ///< 1-CH bypass
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.HdGmMode = 0;                       ///< For 1-CH bypass 148.5 MHz clock output
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[0] = 0xF0;                   ///< VIN#0 -> VPORT#0
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[1] = 0xF1;                   ///< VIN#1 -> VPORT#1
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[2] = 0xF2;                   ///< VIN#2 -> VPORT#2
            dev_attr.stDh9920Attr[i].stVideoAttr.u8ChnSequence[3] = 0xF3;                   ///< VIN#3 -> VPORT#3
        }

        dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.HdBitMode = (vout_format[i] == DH9920_VOUT_FORMAT_BT1120) ? 0 : 1; ///< 0:BT1120, 1:BT656;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.Sd960HEn = 1;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVoCfg.ForceHdClk = 1;
        dev_attr.stDh9920Attr[i].stVideoAttr.u8NetraModeCh[0] = (vout_format[i] == DH9920_VOUT_FORMAT_BT656_DUAL_HEADER) ? 0 : 1;  ///< 0:Dual Header 1:Single Header;
        dev_attr.stDh9920Attr[i].stVideoAttr.u8NetraModeCh[1] = (vout_format[i] == DH9920_VOUT_FORMAT_BT656_DUAL_HEADER) ? 0 : 1;
        dev_attr.stDh9920Attr[i].stVideoAttr.u8NetraModeCh[2] = (vout_format[i] == DH9920_VOUT_FORMAT_BT656_DUAL_HEADER) ? 0 : 1;
        dev_attr.stDh9920Attr[i].stVideoAttr.u8NetraModeCh[3] = (vout_format[i] == DH9920_VOUT_FORMAT_BT656_DUAL_HEADER) ? 0 : 1;

        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[0].ChIdPos = DHC_VIDEO_OUTMODE_BOTH;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[0].ChIdVal = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[0].DataseqInv = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[0].FreerunSel = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[0].FreerunSet = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[1].ChIdPos = DHC_VIDEO_OUTMODE_BOTH;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[1].ChIdVal = 1;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[1].DataseqInv = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[1].FreerunSel = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[1].FreerunSet = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[2].ChIdPos = DHC_VIDEO_OUTMODE_BOTH;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[2].ChIdVal = 2;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[2].DataseqInv = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[2].FreerunSel = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[2].FreerunSet = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[3].ChIdPos = DHC_VIDEO_OUTMODE_BOTH;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[3].ChIdVal = 3;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[3].DataseqInv = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[3].FreerunSel = 0;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVideoOutMode[3].FreerunSet = 0;

        /* free run setting */
        dev_attr.stDh9920Attr[i].stVideoAttr.uVoFreeRun.HDorSD = DHC_VOSIZE_HD;
        dev_attr.stDh9920Attr[i].stVideoAttr.uVoFreeRun.PalOrNtsc = DHC_SDFMT_PAL;
        if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) {
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoFreeRun.Format = DHC_HD_1080P_25HZ;
        }
        else {
            dev_attr.stDh9920Attr[i].stVideoAttr.uVoFreeRun.Format = DHC_HD_720P_25HZ;
        }
        dev_attr.stDh9920Attr[i].stVideoAttr.uVoFreeRun.Color = DHC_COLORBAR_BLACK;

        /* audio attr */
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudConnectMode.CascadeNum = (dev_num - 1);
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudConnectMode.CascadeStage = i;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.SyncMode = DHC_AUDIO_SYNC_MODE_I2S;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.EncDiv = 0;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.SampleRate = sample_rate; //DHC_AUDIO_SAMPLERATE_8K;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.EncMclkEn = 1;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.EncMode = DHC_AUDIO_ENCCLK_MODE_MASTER;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.EncTest = 0;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudModeCfg.EncCs = 1;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudDecCfg.DecFreq = DHC_AUDIO_DEC_FREQ_256;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudDecCfg.DecMode = DHC_AUDIO_ENCCLK_MODE_MASTER;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudDecCfg.DecSync = DHC_AUDIO_SYNC_MODE_I2S;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudDecCfg.DecChnSel = DHC_AUDIO_DEC_I2S_CHN_LEFT;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAudDecCfg.DecCs = 1;

        dev_attr.stDh9920Attr[i].stAudioAttr.uAuEncDataSelCfg.EncDataSel0 = DHC_AUDIO_LINE_IN;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAuEncDataSelCfg.EncDataSel1 = DHC_AUDIO_LINE_IN;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAuEncDataSelCfg.EncDataSel2 = DHC_AUDIO_LINE_IN;
        dev_attr.stDh9920Attr[i].stAudioAttr.uAuEncDataSelCfg.EncDataSel3 = DHC_AUDIO_LINE_IN;

        /* rs485 attr */
        dev_attr.stDh9920Attr[i].u8ProtocolLen = 7;
        dev_attr.stDh9920Attr[i].u8ProtocolType = 0;  ///< 0: old protocol, 1: new protocol
        dev_attr.stDh9920Attr[i].bCheckChipId = DHC_TRUE; ///< Enable Chip ID checking
        dev_attr.stDh9920Attr[i].eDriverPower = DHC_DRIVER_POWER_33V; ///< 3.3V power supply
        dev_attr.stDh9920Attr[i].eEqMode = DHC_INTERNAL_CASCADE; ///< Set EQ Mode as internal cascade mode
        //dev_attr.stDh9920Attr[i].eEqMode = DHC_EXTERNAL_COUPLING; ///< Set EQ Mode as external coupling mode
    }

    /* set audio channel sequence */
    dh9920_audio_set_channel_sequence();

    /* auto detect thread attr */
    dev_attr.u8DetectPeriod = 0; ///< ms

    /* register i2c read/write function */
    dev_attr.Dh9920_WriteByte = dh9920_writebyte;
    dev_attr.Dh9920_ReadByte  = dh9920_readbyte;

    /* register misc function */
    dev_attr.Dh9920_MSleep = dh9920_msleep;
    dev_attr.Dh9920_Printf = dh9920_printf;

    ret = DHC_DH9920_SDK_Init(&dev_attr);
    if(ret != 0) {
        printk("DHC_DH9920_SDK_Init() got failed!\n");
        goto exit;
    }

    ret = DHC_DH9920_SDK_SetAudioInVolume(0, 1, 80);
    if(ret != 0) {
        printk("DHC_DH9920_SDK_SetAudioInVolume() got failed!\n");
        goto exit;
    }

    /* enable half mode */
    for(i=0; i<dev_num; i++) {
        if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) {
            for(j=0; j<DH9920_DEV_CH_MAX; j++) {
                DHC_DH9920_SDK_SetHalfMode( i, j, 1, 1);
            }
        }
        else if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P) {
            for(j=0; j<DH9920_DEV_CH_MAX; j++) {
                DHC_DH9920_SDK_SetHalfMode( i, j, 1, 0);
            }
        }
    }

    ret = DHC_DH9920_SDK_SetCableType(0, 0, DHC_CABLE_TYPE_COAXIAL);
    if(ret != 0) {
        printk("DHC_DH9920_SDK_SetCableType() got failed!\n");
        goto exit;
    }

    /* setup default color setting */
    video_color.u8Brightness   = 0x32;
    video_color.u8Contrast     = 0x32;
    video_color.u8Saturation   = 0x32;
    video_color.u8Hue          = 0x32;
    video_color.u8Gain         = 0x00;
    video_color.u8WhiteBalance = 0x00;
    video_color.u8Sharpness    = 0x00;

    for(i=0; i<dev_num; i++) {
        for(j=0; j<DH9920_DEV_CH_MAX; j++) {
            ret = DHC_DH9920_SDK_SetColor(i, j, &video_color, DHC_SET_MODE_USER);
            if(ret != 0) {
                printk("DHC_DH9920_SDK_SetColor() got failed!\n");
                goto exit;
            }
        }
    }

    /* PTZ config init */
    for(i=0; i<dev_num; i++) {
        for(j=0; j<DH9920_DEV_CH_MAX; j++) {
            dh9920_dev[i].ptz[j].protocol     = DH9920_PTZ_PTOTOCOL_DHSD1;
            dh9920_dev[i].ptz[j].baud_rate    = DH9920_PTZ_BAUD_9600;
            dh9920_dev[i].ptz[j].parity_chk   = 1;
        }
    }

    for(i=0; i<dev_num; i++) {
        if((vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE) || (vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) || (vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P)) {
            /* Disable VO_CK2,3 & VO_Data2,3 */
            dh9920_writebyte(iaddr[i], 0xFD, 0x00);
            dh9920_writebyte(iaddr[i], 0x8B, 0xF0);
            dh9920_writebyte(iaddr[i], 0x8C, 0x0C);
            /* Clock delay for 2-CH dual edge output -> fix line lack issue on GM8210 */
            dh9920_writebyte(iaddr[i], 0x17, 0x03);
            dh9920_writebyte(iaddr[i], 0x37, 0x03);
        }
    }

    /* set flag */
    dh9920_api_inited = 1;

exit:
    return ret;
}

static void dh9920_device_deregister(void)
{
    if(dh9920_api_inited) {
        DHC_DH9920_SDK_DeInit();
        dh9920_api_inited = 0;
    }
}

static int dh9920_sound_switch(int ssp_idx, int chan_num, bool is_on)
{
    /* ToDo */

    return 0;
}

static int dh9920_watchdog_thread(void *data)
{
    int ret, i, ch;
    DHC_VideoStatus_S cvi_sts;
    struct dh9920_dev_t *pdev;
    struct dh9920_video_fmt_t *p_vfmt;
    int pre_fmt_idx[DH9920_DEV_MAX*DH9920_DEV_CH_MAX] = {[0 ... (DH9920_DEV_MAX*DH9920_DEV_CH_MAX - 1)] = -1}; ///< Previous video input format
    unsigned char reg_value;

    for(i=0; i<dev_num; i++) {
        if(vout_mode[i] >= DH9920_VOUT_MODE_2CH_DUAL_EDGE) { ///< Check the 2-CH video output sequence
            pdev = &dh9920_dev[i];

            down(&pdev->lock);

            reg_value = dh9920_readbyte(iaddr[pdev->index],0x19) & 0x80; ///< Check the "vo_half_clk_flag_chX" flag of CH#0
            if(reg_value == 0)
                DHC_DH9920_SDK_SetClkPhase(i, 0, 1, 0x00);
            else
                DHC_DH9920_SDK_SetClkPhase(i, 0, 0, 0x00);
            reg_value = dh9920_readbyte(iaddr[pdev->index],0x59) & 0x80; ///< Check the "vo_half_clk_flag_chX" flag of CH#2
            if(reg_value == 0)
                DHC_DH9920_SDK_SetClkPhase(i, 2, 1, 0x00);
            else
                DHC_DH9920_SDK_SetClkPhase(i, 2, 0, 0x00);

            up(&pdev->lock);
        }
    }

    do {
            /* check dh9920 channel status */
            for(i=0; i<dev_num; i++) {
                pdev = &dh9920_dev[i];

                down(&pdev->lock);

                /* Enable auto detection on current input state */
                DHC_DH9920_SDK_DetectThr();
                for(ch=0; ch<DH9920_DEV_CH_MAX; ch++) {
                    ret = DHC_DH9920_SDK_GetVideoStatus(i, ch, &cvi_sts);
                    if(ret != DHC_ERRID_SUCCESS)
                        continue;

                    /* get video format information */
                    if(dh9920_vfmt_map(cvi_sts.u8VideoFormat) >= DH9920_VFMT_MAX)
                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_720P25];
                    else
                        p_vfmt = &dh9920_video_fmt_info[dh9920_vfmt_map(cvi_sts.u8VideoFormat)];

                    pdev->cur_plugin[ch] = (cvi_sts.u8VideoLost == 0) ? 1 : 0;

                    if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 0)) {        ///< cable plugged-in
                        goto sts_update;
                    }
                    else if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 1)) {   ///< cable is connected
                        /* notify current video present */
                        if(notify && pdev->vlos_notify)
                            pdev->vlos_notify(i, ch, 0);

                        if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE) {
                            if((p_vfmt->fmt_idx == DH9920_VFMT_1080P30) || (p_vfmt->fmt_idx == DH9920_VFMT_1080P25) || (p_vfmt->fmt_idx == DH9920_VFMT_720P60) || (p_vfmt->fmt_idx == DH9920_VFMT_720P50)) {
                                p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_720P25];
                                ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 1, DHC_COLORBAR_BLACK, DHC_HD_720P_25HZ); ///< Enable freerun mode with 720P25 format
                                if(ret != DHC_ERRID_SUCCESS)
                                    continue;
                            }
                            else {
                                switch(p_vfmt->fmt_idx) {
                                    case DH9920_VFMT_720P30:
                                        ret = DHC_HD_720P_30HZ;
                                        break;
                                    case DH9920_VFMT_960H25:
                                        ret = DHC_SD_PAL_M;
                                        break;
                                    case DH9920_VFMT_960H30:
                                        ret = DHC_SD_NTSC_JM;
                                        break;
                                    case DH9920_VFMT_720P25:
                                    default:
                                        ret = DHC_HD_720P_25HZ;
                                        break;
                                }
                                ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 0, DHC_COLORBAR_BLACK, ret); ///< Disable freerun mode
                                if(ret != DHC_ERRID_SUCCESS)
                                    continue;
                            }
                        }
                        else if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P) {
                           if((p_vfmt->fmt_idx == DH9920_VFMT_1080P30) || (p_vfmt->fmt_idx == DH9920_VFMT_1080P25) || (p_vfmt->fmt_idx == DH9920_VFMT_960H30) || (p_vfmt->fmt_idx == DH9920_VFMT_960H25)) { ///< Disable freerun mode for half-1080P & 960H output
                               switch(p_vfmt->fmt_idx) {
                                    case DH9920_VFMT_1080P30:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_1080P30];
                                        ret = DHC_HD_1080P_30HZ;
                                        break;
                                    case DH9920_VFMT_960H25:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_960H25];
                                        ret = DHC_SD_PAL_M;
                                        break;
                                    case DH9920_VFMT_960H30:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_960H30];
                                        ret = DHC_SD_NTSC_JM;
                                        break;
                                    case DH9920_VFMT_1080P25:
                                    default:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_1080P25];
                                        ret = DHC_HD_1080P_25HZ;
                                        break;
                                }
                                ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 0, DHC_COLORBAR_BLACK, ret); ///< Disable freerun mode
                                if(ret != DHC_ERRID_SUCCESS)
                                    continue;
                           }
                           else { ///< Enable freerun mode with 1080P25 format (half-mode)
                                p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_1080P25];
                                ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 1, DHC_COLORBAR_BLACK, DHC_HD_1080P_25HZ); ///< Enable freerun mode with 720P25 format
                                if(ret != DHC_ERRID_SUCCESS)
                                    continue;
                           }
                        }
                        else if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P) {
                           if((p_vfmt->fmt_idx == DH9920_VFMT_720P30) || (p_vfmt->fmt_idx == DH9920_VFMT_720P25) || (p_vfmt->fmt_idx == DH9920_VFMT_960H30) || (p_vfmt->fmt_idx == DH9920_VFMT_960H25)) { ///< Disable freerun mode for half-720P & 960H output
                               switch(p_vfmt->fmt_idx) {
                                    case DH9920_VFMT_720P30:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_720P30];
                                        ret = DHC_HD_720P_30HZ;
                                        break;
                                    case DH9920_VFMT_960H25:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_960H25];
                                        ret = DHC_HD_720P_25HZ;
                                        break;
                                    case DH9920_VFMT_960H30:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_960H30];
                                        ret = DHC_HD_720P_30HZ;
                                        break;
                                    case DH9920_VFMT_720P25:
                                    default:
                                        p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_720P25];
                                        ret = DHC_HD_720P_25HZ;
                                        break;
                                }
                                //ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 0, DHC_COLORBAR_BLACK, ret); ///< Disable freerun mode
                                //if(ret != DHC_ERRID_SUCCESS)
                                    //continue;
                           }
                           else { ///< Enable freerun mode with 720P25 format (half-mode)
                                p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_720P25];
                                //ret = DHC_DH9920_SDK_SetFreeRun(i, ch, 1, DHC_COLORBAR_BLACK, DHC_HD_720P_25HZ); ///< Enable freerun mode with 720P25 format
                                //if(ret != DHC_ERRID_SUCCESS)
                                    //continue;
                           }
                        }

                        /* notify current video format */
                        if(notify && pdev->vfmt_notify)
                            pdev->vfmt_notify(i, ch, p_vfmt);

                        pre_fmt_idx[i*DH9920_DEV_CH_MAX+ch] = p_vfmt->fmt_idx;
                    }
                    else {  ///< cable is plugged-out
                        /* notify current video loss */
                        if(notify && pdev->vlos_notify)
                            pdev->vlos_notify(i, ch, 1);

                        if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_1080P)
                            p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_1080P25];
                        else if(vout_mode[i] == DH9920_VOUT_MODE_2CH_DUAL_EDGE_HALF_720P)
                            p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_HALF_720P25];
                        else
                            p_vfmt = &dh9920_video_fmt_info[DH9920_VFMT_720P25];

                        /* notify current video format */
                        if(notify && pdev->vfmt_notify)
                            pdev->vfmt_notify(i, ch, p_vfmt);

                        pre_fmt_idx[i*DH9920_DEV_CH_MAX+ch] = DH9920_VFMT_MAX;
                    }
sts_update:
                    pdev->pre_plugin[ch] = pdev->cur_plugin[ch];
                }

                up(&pdev->lock);
            }

            /* sleep 0.2 second */
            schedule_timeout_interruptible(msecs_to_jiffies(200));

    } while(!kthread_should_stop());

    return 0;
}

static int dh9920_comm_thread(void *data)
{
    int ret, id, ch;
    struct dh9920_dev_t *pdev;
    struct plat_cpucomm_msg  rx_msg;
    struct dh9920_comm_header_t *p_header;
    unsigned char *p_data;
    unsigned char *msg_buf = NULL;

    /* allocate dh9920 comm rx message buffer */
    msg_buf = kzalloc(DH9920_COMM_MSG_BUF_MAX, GFP_KERNEL);
    if(!msg_buf) {
        printk("DH9920 COMM allocate memory failed!!\n");
        goto exit;
    }

    do {
            /* wait to receive data */
            rx_msg.length   = DH9920_COMM_MSG_BUF_MAX;
            rx_msg.msg_data = msg_buf;
            ret = plat_cpucomm_rx(&rx_msg, 100);    ///< 100ms timeout
            if(ret != 0)
                continue;

            /* process receive message data */
            if(rx_msg.length >= sizeof(struct dh9920_comm_header_t)) {
                p_header = (struct dh9920_comm_header_t *)rx_msg.msg_data;
                p_data   = ((unsigned char *)p_header) + sizeof(struct dh9920_comm_header_t);

                /* check header magic number */
                if(p_header->magic == DH9920_COMM_MSG_MAGIC) {
                    /* check device id and channel */
                    id = p_header->dev_id;
                    ch = p_header->dev_ch;
                    if(((id >= dev_num) || (ch >= DH9920_DEV_CH_MAX))) {
                        dh9920_comm_cmd_send(0, 0, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                        continue;
                    }

                    pdev = &dh9920_dev[id];

                    /* process command */
                    switch(p_header->cmd_id) {
                        case DH9920_COMM_CMD_ACK_OK:
                        case DH9920_COMM_CMD_ACK_FAIL:
                            break;

                        case DH9920_COMM_CMD_GET_VIDEO_STATUS:
                            {
                                DHC_VideoStatus_S *pcvi_sts = (DHC_VideoStatus_S *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_GetVideoStatus(id, ch, pcvi_sts);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_GET_VIDEO_STATUS, pcvi_sts, sizeof(DHC_VideoStatus_S), 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_SET_COLOR:
                            {
                                DHC_VideoColor_S *pvideo_color = (DHC_VideoColor_S *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_SetColor(id, ch, pvideo_color, DHC_SET_MODE_USER);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_GET_COLOR:
                            {
                                DHC_VideoColor_S *pvideo_color = (DHC_VideoColor_S *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_GetColor(id, ch, pvideo_color, DHC_SET_MODE_USER);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_GET_COLOR, pvideo_color, sizeof(DHC_VideoColor_S), 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_CLEAR_EQ:
                            {
                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_ClearEq(id, ch);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_SEND_RS485:
                            {
                                struct dh9920_comm_send_rs485_t *pcomm_rs485 = (struct dh9920_comm_send_rs485_t *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_SendCo485Enable(id, ch, 1);

                                if(ret == 0)
                                    ret = DHC_DH9920_SDK_SendCo485Buf(id, ch, pcomm_rs485->cmd_buf, pcomm_rs485->buf_len);

                                DHC_DH9920_SDK_SendCo485Enable(id, ch, 0);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_SET_VIDEO_POS:
                            {
                                DHC_VideoOffset_S *pvideo_pos = (DHC_VideoOffset_S *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_SetVideoPos(id, ch, pvideo_pos);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_GET_VIDEO_POS:
                            {
                                DHC_VideoOffset_S *pvideo_pos = (DHC_VideoOffset_S *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_GetVideoPos(id, ch, pvideo_pos);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_GET_VIDEO_POS, pvideo_pos, sizeof(DHC_VideoOffset_S), 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_SET_CABLE_TYPE:
                            {
                                DH9920_CABLE_TYPE_T *pcab_type = (DH9920_CABLE_TYPE_T *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_SetCableType(id, ch, *pcab_type);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_SET_AUDIO_VOL:
                            {
                                u8 *p_vol = (u8 *)p_data;

                                down(&pdev->lock);

                                ret = DHC_DH9920_SDK_SetAudioInVolume(id, ch, *p_vol);

                                up(&pdev->lock);

                                if(ret == 0)
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_OK,   NULL, 0, 0, 0);
                                else
                                    dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_ACK_FAIL, NULL, 0, 0, 0);
                            }
                            break;

                        case DH9920_COMM_CMD_GET_PTZ_CFG:
                            {
                                struct dh9920_ptz_t ptz_cfg;

                                down(&pdev->lock);

                                memcpy(&ptz_cfg, &pdev->ptz[ch], sizeof(struct dh9920_ptz_t));

                                up(&pdev->lock);

                                dh9920_comm_cmd_send(id, ch, DH9920_COMM_CMD_GET_PTZ_CFG, &ptz_cfg, sizeof(ptz_cfg), 0, 0);
                            }
                            break;

                        default:
                            printk("DH9920 COMM unkonw command id(%d)\n", p_header->cmd_id);
                            break;
                    }
                }
                else
                    printk("DH9920 COMM message magic not matched(0x%08x)\n", p_header->magic);
            }
            else
                printk("DH9920 COMM message receive length incorrect!!(%d)\n", rx_msg.length);
    } while(!kthread_should_stop());

exit:
    /* free buffer */
    if(msg_buf)
        kfree(msg_buf);

    return 0;
}

static int __init dh9920_init(void)
{
    int i, ret;
    int comm_ready = 0;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(dev_num > DH9920_DEV_MAX) {
        printk("DH9920 dev_num=%d invalid!!(Max=%d)\n", dev_num, DH9920_DEV_MAX);
        return -1;
    }

    /* check video output format and mode */
    for(i=0; i<dev_num; i++) {
        if((vout_format[i] < 0) || (vout_format[i] >= DH9920_VOUT_FORMAT_MAX)) {
            printk("DH9920#%d vout_format=%d not support!!\n", i, vout_format[i]);
            return -1;
        }

        if((vout_mode[i] < 0) || (vout_mode[i] >= DH9920_VOUT_MODE_MAX)) {
            printk("DH9920#%d vout_mode=%d not support!!\n", i, vout_mode[i]);
            return -1;
        }
    }

    /* register i2c client for contol dh9920 */
    ret = dh9920_i2c_register();
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

    /* open cpu_comm channel for dh9920 driver data communication of dual cpu */
    ret = plat_cpucomm_open();
    if(ret == 0)
        comm_ready = 1;

    /* create channel mapping table for different board */
    ret = dh9920_create_ch_map(ch_map);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    dh9920_hw_reset();

    /* DH9920 init */
    for(i=0; i<dev_num; i++) {
        dh9920_dev[i].index = i;

        sprintf(dh9920_dev[i].name, "dh9920.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&dh9920_dev[i].lock, 1);
#else
        init_MUTEX(&dh9920_dev[i].lock);
#endif
        ret = dh9920_proc_init(i);
        if(ret < 0)
            goto err;

        ret = dh9920_miscdev_init(i);
        if(ret < 0)
            goto err;
    }

    /* dh9920 device register */
    if(init) {
        ret = dh9920_device_register();
        if(ret != 0)
            goto err;
    }

    if(init) {
        /* init dh9920 watchdog thread for check video status */
        dh9920_wd = kthread_create(dh9920_watchdog_thread, NULL, "dh9920_wd");
        if(!IS_ERR(dh9920_wd))
            wake_up_process(dh9920_wd);
        else {
            printk("create dh9920 watchdog thread failed!!\n");
            dh9920_wd = 0;
            ret = -EFAULT;
            goto err;
        }

        /* init dh9920 comm thread for receive data from corresponding cpu */
        if(comm_ready) {
            dh9920_comm = kthread_create(dh9920_comm_thread, NULL, "dh9920_comm");
            if(!IS_ERR(dh9920_comm))
                wake_up_process(dh9920_comm);
            else {
                printk("create dh9920 comm thread failed!!\n");
                dh9920_comm = 0;
                ret = -EFAULT;
                goto err;
            }
        }
    }

    /* register audio function for platform used */
    dh9920_audio_funs.sound_switch = dh9920_sound_switch;
    dh9920_audio_funs.codec_name   = dh9920_dev[0].name;
    plat_audio_register_function(&dh9920_audio_funs);

    return 0;

err:
    if(dh9920_wd)
        kthread_stop(dh9920_wd);

    if(dh9920_comm)
        kthread_stop(dh9920_comm);

    dh9920_device_deregister();

    plat_cpucomm_close();

    dh9920_i2c_unregister();
    for(i=0; i<DH9920_DEV_MAX; i++) {
        if(dh9920_dev[i].miscdev.name)
            misc_deregister(&dh9920_dev[i].miscdev);

        dh9920_proc_remove(i);
    }

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit dh9920_exit(void)
{
    int i;

    /* stop dh9920 watchdog thread */
    if(dh9920_wd)
        kthread_stop(dh9920_wd);

    if(dh9920_comm)
        kthread_stop(dh9920_comm);

    dh9920_device_deregister();

    plat_cpucomm_close();

    dh9920_i2c_unregister();

    for(i=0; i<DH9920_DEV_MAX; i++) {
        /* remove device node */
        if(dh9920_dev[i].miscdev.name)
            misc_deregister(&dh9920_dev[i].miscdev);

        /* remove proc node */
        dh9920_proc_remove(i);
    }

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    plat_audio_deregister_function();
}

module_init(dh9920_init);
module_exit(dh9920_exit);

MODULE_DESCRIPTION("Grain Media DH9920 4CH HDCVI Receiver Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
