/*
 * @file tp2822_nvp6114_hybrid_drv.c
 *  TechPoint TP2822 4CH HD-TVI & Nextchip NVP6114 4CH AHD Hybrid Receiver Driver (2-CH dual edge output mode)
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2015/05/04 12:50:08 $
 *
 * ChangeLog:
 *  $Log: tp2822_nvp6114_hybrid_drv.c,v $
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
#include <mach/ftpmu010.h>

#include "platform.h"
#include "techpoint/tp2822.h"           ///< from module/include/front_end/hdtvi/techpoint
#include "nextchip/nvp6114.h"           ///< from module/include/front_end/ahd
#include "tp2822_nvp6114_hybrid_drv.h"
#include "tp2822_dev.h"
#include "nvp6114_dev.h"

/*************************************************************************************
 *  Module Parameters
 *************************************************************************************/
/* device number */
int tp2822_dev_num = 1;
module_param(tp2822_dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2822_dev_num, "TP2822 Device Number: 1~4");

int nvp6114_dev_num = 1;
module_param(nvp6114_dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp6114_dev_num, "NVP6114 Device Number: 1~4");

/* device i2c bus */
static int ibus = 0;
module_param(ibus, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ibus, "Device I2C Bus: 0~4");

/* device i2c address */
ushort tp2822_iaddr[TP2822_DEV_MAX] = {0x88, 0x8a, 0x8c, 0x8e};
module_param_array(tp2822_iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2822_iaddr, "TP2822 Device I2C Address");

ushort nvp6114_iaddr[NVP6114_DEV_MAX] = {0x60, 0x62, 0x64, 0x66};
module_param_array(nvp6114_iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp6114_iaddr, "NVP6114 Device I2C Address");

/* device video mode */
int tp2822_vout_format[TP2822_DEV_MAX] = {[0 ... (TP2822_DEV_MAX - 1)] = TP2822_VOUT_FORMAT_BT656};
module_param_array(tp2822_vout_format, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2822_vout_format, "Video Output Format: 0:BT656, 1:BT1120");

/* device video port output mode */
int tp2822_vout_mode[TP2822_DEV_MAX] = {[0 ... (TP2822_DEV_MAX - 1)] = TP2822_VOUT_MODE_2CH_DUAL_EDGE};
module_param_array(tp2822_vout_mode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2822_vout_mode, "Video Output Mode: 0:1CH_BYPASS, 1:2CH_DUAL_EDGE");

int nvp6114_vmode[NVP6114_DEV_MAX] = {[0 ... (NVP6114_DEV_MAX - 1)] = NVP6114_VMODE_NTSC_720P_2CH};
module_param_array(nvp6114_vmode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp6114_vmode, "Video Mode");

/* audio sample rate */
int nvp6114_sample_rate = AUDIO_SAMPLERATE_8K;
module_param(nvp6114_sample_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp6114_sample_rate, "audio sample rate: 0:8k  1:16k  2:32k 3:44.1k 4:48k");

/* audio sample size */
int nvp6114_sample_size = AUDIO_BITS_16B;
module_param(nvp6114_sample_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp6114_sample_size, "audio sample size: 0:16bit  1:8bit");

/* NVP6114 audio channel number */
int audio_chnum = 4;
module_param(audio_chnum, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(audio_chnum, "audio channel number");

/* clock port used */
static int clk_used = 0x3;
module_param(clk_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_used, "External Clock Port: BIT0:EXT_CLK0, BIT1:EXT_CLK1");

/* clock source */
static int clk_src = PLAT_EXTCLK_SRC_PLL3;
module_param(clk_src, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_src, "External Clock Source: 0:PLL1OUT1 1:PLL1OUT1/2 2:PLL4OUT2 3:PLL4OUT1/2 4:PLL3");

/* clock frequency */
static int clk_freq = CLKIN;   ///< 27MHz
module_param(clk_freq, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(clk_freq, "External Clock Frequency(Hz)");

/* device use gpio as rstb pin */
static int rstb_used = 1;
module_param(rstb_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rstb_used, "Use GPIO as RSTB Pin: 0:None, 1:X_CAP_RST");

/* device channel mapping and vch start number of NVP6114 */
int ch_map[2] = {0, 0};
module_param_array(ch_map, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_map, "channel mapping: [0]:map_id# [1]:VCH start index of NVP6114");

/* device init */
int init = 1;
module_param(init, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(init, "device init: 0:no-init  1:init");

/* notify */
static int notify = 1;
module_param(notify, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Device Notify => 0:Disable 1:Enable");

/* Hybrid VPORT Output mode */
static ushort hybrid_vport_out = 0x000C; // DEV_MAX = 4
module_param(hybrid_vport_out, short, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Hybrid VPORT Output Mode (bitmap) => 0:AHD (NVP6114) 1:TVI (TP2822)");
/************************************************************************************** 
  e.g. hybrid_vport_out = 0x000C = 16b'0000000000001100, that means:
  TP2822_Dev#0 TVI_OUT[0] -> output VIN3&VIN4 (2-CH TVI bypass output) to XCAP#2
  NVP6114_Dev#0 AHD_OUT[1] -> output VIN1&VIN2 (2-CH TVI bypass output) to XCAP#0
  if hybrid_vport_out = 0x0003 = 16b'0000000000000011, that means:
  TP2822_Dev#0 TVI_OUT[0] -> output VIN1&VIN2 (2-CH TVI bypass output) to XCAP#2
  NVP6114_Dev#0 AHD_OUT[1] -> output VIN3&VIN4 (2-CH TVI bypass output) to XCAP#0
  = 1 * 2-CH TVI dual edge output + 1 * 2-CH AHD dual edge output
**************************************************************************************/

/*************************************************************************************
 *  Original structure
 *************************************************************************************/
struct tp2822_dev_t tp2822_dev[TP2822_DEV_MAX];
struct nvp6114_dev_t nvp6114_dev[NVP6114_DEV_MAX];

/*************************************************************************************
 *  Data structure or APIs for hybrid design
 *************************************************************************************/
static struct i2c_client  *tp2822_nvp6114_i2c_client = NULL;
static struct task_struct *tp2822_nvp6114_wd  = NULL;
static struct proc_dir_entry *proc_hybrid_vport_out = NULL;
static struct proc_dir_entry *proc_hybrid_dbg_mode = NULL;
static int hybrid_dbg_mode = 0; // Flag for debug mode enable/disable

u32 TP2822_NVP6114_VCH_MAP[TP2822_DEV_MAX*TP2822_DEV_CH_MAX];

struct tp2822_video_fmt_t tp2822_nvp6114_video_fmt_info[TP2822_VFMT_MAX] = {
    /* Idx                         Width   Height  Prog    FPS */
    {  TP2822_VFMT_720P60,         1280,   720,    1,      60  },  ///< 720P @60
    {  TP2822_VFMT_720P50,         1280,   720,    1,      50  },  ///< 720P @50
    {  TP2822_VFMT_1080P30,        1920,   1080,   1,      30  },  ///< 1080P@30
    {  TP2822_VFMT_1080P25,        1920,   1080,   1,      25  },  ///< 1080P@25
    {  TP2822_VFMT_720P30,         1280,   720,    1,      30  },  ///< 720P @30
    {  TP2822_VFMT_720P25,         1280,   720,    1,      25  },  ///< 720P @25
    {  TP2822_VFMT_HALF_1080P30,    960,  1080,    1,      30  },  ///< Half-1080P@30
    {  TP2822_VFMT_HALF_1080P25,    960,  1080,    1,      25  },  ///< Half-1080P@25
    {  TP2822_VFMT_HALF_720P30,     640,   720,    1,      30  },  ///< Half-720P @30
    {  TP2822_VFMT_HALF_720P25,     640,   720,    1,      25  },  ///< Half-720P @25
    {  TP2822_VFMT_960H30,          960,   480,    0,      60  },  ///< 960H @30, for AHD D1 output in hybrid design
    {  TP2822_VFMT_960H25,          960,   576,    0,      50  },  ///< 960H @25, for AHD D1 output in hybrid design
};

int tp2822_get_device_num(void)
{
    ///< Let vcap300_tp2822 create two 2-CH dual edge video out path
    return nvp6114_dev_num + tp2822_dev_num;
}
EXPORT_SYMBOL(tp2822_get_device_num);

static int tp2822_nvp6114_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    tp2822_nvp6114_i2c_client = client;
    return 0;
}

static int tp2822_nvp6114_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tp2822_nvp6114_i2c_id[] = {
    { "tp2822_nvp6114_i2c", 0 },
    { }
};

static struct i2c_driver tp2822_nvp6114_i2c_driver = {
    .driver = {
        .name  = "TP2822_NVP6114_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = tp2822_nvp6114_i2c_probe,
    .remove   = tp2822_nvp6114_i2c_remove,
    .id_table = tp2822_nvp6114_i2c_id
};

static int tp2822_nvp6114_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&tp2822_nvp6114_i2c_driver);
    if(ret < 0) {
        printk("TP2822 can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = tp2822_iaddr[0]>>1;
    strlcpy(info.type, "tp2822_nvp6114_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("TP2822&NVP6114 can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("TP2822&NVP6114 can't add i2c device!\n");
        goto err_driver;
    }
    
    return 0;

err_driver:
   i2c_del_driver(&tp2822_nvp6114_i2c_driver);
   return -ENODEV;
}

static void tp2822_nvp6114_i2c_unregister(void)
{
    i2c_unregister_device(tp2822_nvp6114_i2c_client);
    i2c_del_driver(&tp2822_nvp6114_i2c_driver);
    tp2822_nvp6114_i2c_client = NULL;
}

int tp2822_nvp6114_i2c_write(u8 addr, u8 reg, u8 data)
{
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(!tp2822_nvp6114_i2c_client) {
        printk("TP2822&NVP6114 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2822_nvp6114_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = addr>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        if(hybrid_dbg_mode)
            printk("TP2822 or NVP6114 @ 0x%02x i2c write failed!!\n", addr);
        return -1;
    }

    return 0;
}

u8 tp2822_nvp6114_i2c_read(u8 addr, u8 reg)
{
    u8 reg_data = 0;
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(!tp2822_nvp6114_i2c_client) {
        printk("TP2822&NVP6114 i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(tp2822_nvp6114_i2c_client->dev.parent);

    msgs[0].addr  = addr>>1;
    msgs[0].flags = 0; /* write */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = (addr + 1)>>1;
    msgs[1].flags = 1; /* read */
    msgs[1].len   = 1;
    msgs[1].buf   = &reg_data;

    if(i2c_transfer(adapter, msgs, 2) != 2) {
        if(hybrid_dbg_mode)
            printk("TP2822 or NVP6114 @ 0x%02x i2c read failed!!\n", addr);
    }

    return reg_data;
}

int tp2822_nvp6114_i2c_array_write(u8 addr, u8 offset, u8 *parray, u32 array_cnt)
{
#define TP2822_NVP6114_I2C_ARRAY_WRITE_MAX     256
    int i, num = 0;
    u8  buf[TP2822_NVP6114_I2C_ARRAY_WRITE_MAX+1];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(!parray || array_cnt > TP2822_NVP6114_I2C_ARRAY_WRITE_MAX)
        return -1;

    if(!tp2822_nvp6114_i2c_client) {
        printk("TP2822&NVP6114 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2822_nvp6114_i2c_client->dev.parent);

    buf[num++] = offset;
    for(i=0; i<array_cnt; i++) {
        buf[num++] = parray[i];
    }

    msgs.addr  = addr>>1;
    msgs.flags = 0;
    msgs.len   = num;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("TP2822 or NVP6114 @ 0x%02x i2c array write failed!!\n", addr);
        return -1;
    }

    return 0;
}

static int tp2822_nvp6114_create_ch_map(int map_id)
{
    int i;

/******************************************************************************
 Customer's GM8286 EVB Channel Mapping Table (2-CH dual edge output mode)

 VCH#0 ------> NVP6114#VIN#0.0 -------> ---v
 VCH#1 ------> NVP6114#VIN#0.1 -------> NVP6114#VDO2 (2-CH) -------> X_CAP#0
 VCH#2 ------> NVP6114#VIN#0.2 -------> ---^
 VCH#3 ------> NVP6114#VIN#0.3 -------> ---^

 VCH#3 ------> TP2822#TVI#0.0 -------> TP2822#VD1 (2-CH) -------> X_CAP#2
 VCH#2 ------> TP2822#TVI#0.1 -------> ---^
 VCH#1 ------> TP2822#TVI#0.2 -------> ---^
 VCH#0 ------> TP2822#TVI#0.3 -------> ---^

*******************************************************************************/

    switch(map_id) {
        case 0: ///< for customer's board - 2-CH dual edge mode
        default:
            for(i=0; i<tp2822_dev_num+nvp6114_dev_num; i++) {
                TP2822_NVP6114_VCH_MAP[(i*TP2822_DEV_CH_MAX)+0] = CH_IDENTIFY(i, (i==0?0:3), (i==0?TP2822_DEV_VOUT1:TP2822_DEV_VOUT0));
                TP2822_NVP6114_VCH_MAP[(i*TP2822_DEV_CH_MAX)+1] = CH_IDENTIFY(i, (i==0?1:2), (i==0?TP2822_DEV_VOUT1:TP2822_DEV_VOUT0));
                TP2822_NVP6114_VCH_MAP[(i*TP2822_DEV_CH_MAX)+2] = CH_IDENTIFY(i, (i==0?2:1), (i==0?TP2822_DEV_VOUT1:TP2822_DEV_VOUT0));
                TP2822_NVP6114_VCH_MAP[(i*TP2822_DEV_CH_MAX)+3] = CH_IDENTIFY(i, (i==0?3:0), (i==0?TP2822_DEV_VOUT1:TP2822_DEV_VOUT0));
            }
            break;
    }

    return 0;
}

static void tp2822_nvp6114_hw_reset(void)
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

static void tp2822_nvp6114_params_passing(void)
{
    struct tp2822_params_t tp2822_params;
    struct nvp6114_params_t nvp6114_params;

    tp2822_params.tp2822_dev_num = tp2822_dev_num;
    tp2822_params.tp2822_iaddr = &tp2822_iaddr[0];
    tp2822_params.tp2822_vout_format = &tp2822_vout_format[0];
    tp2822_params.tp2822_vout_mode = &tp2822_vout_mode[0];
    tp2822_params.tp2822_dev = &tp2822_dev[0];
    tp2822_params.init = init;
    tp2822_params.TP2822_NVP6114_VCH_MAP = &TP2822_NVP6114_VCH_MAP[0];
    tp2822_set_params(&tp2822_params);

    nvp6114_params.nvp6114_dev_num = nvp6114_dev_num;
    nvp6114_params.nvp6114_iaddr = &nvp6114_iaddr[0];
    nvp6114_params.nvp6114_vmode = &nvp6114_vmode[0];
    nvp6114_params.nvp6114_sample_rate = nvp6114_sample_rate;
    nvp6114_params.nvp6114_sample_size = nvp6114_sample_size;
    nvp6114_params.nvp6114_dev = &nvp6114_dev[0];
    nvp6114_params.nvp6114_ch_map = &ch_map[0];
    nvp6114_params.init = init;
    nvp6114_params.TP2822_NVP6114_VCH_MAP = &TP2822_NVP6114_VCH_MAP[0];
    nvp6114_set_params(&nvp6114_params);
}

static int tp2822_nvp6114_watchdog_thread(void *data)
{
    int ret, dev_id, ch, vmode;
    int nvp6114_novid, tp2822_vdloss, is_tvi_2th_ch, is_ahd_2th_ch;
    u8 tp2822_vd1sel_val, nvp6114_vport2_seq_val;
    u8 last_tp2822_vd1sel_val = 0xFF, last_nvp6114_vport2_seq_val = 0xFF, reversed_val; ///< To keep record of the last setting

    struct tp2822_dev_t *tp2822_pdev;
    struct nvp6114_dev_t *nvp6114_pdev;
    struct tp2822_video_fmt_t vin_fmt, vout_fmt;
    
    do {
        ///< Check NVP6114 & TP2822 video present/lost status to determine hybrid_vport_out value
        tp2822_pdev = &tp2822_dev[tp2822_dev_num-1];
        nvp6114_pdev = &nvp6114_dev[nvp6114_dev_num-1];

        down(&tp2822_pdev->lock);
        down(&nvp6114_pdev->lock);

        tp2822_vdloss = 0;
        nvp6114_novid = nvp6114_status_get_novid(nvp6114_pdev->index);
        for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) { ///< Note the VIN sequence of TP2822 was inversed
            tp2822_vdloss |= tp2822_status_get_video_loss(tp2822_pdev->index, (TP2822_DEV_CH_MAX-ch-1))<<ch;
        }

        switch(nvp6114_novid) {
            case 0x0:
            case 0x4:
            case 0x8:
            case 0xC:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0xC;
                break;
            case 0x1:
            case 0x9:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0x9;
                break;
            case 0x2:
            case 0xA:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0xA;
                break;
            case 0x3:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0x3;
                break;
            case 0x5:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0x5;
                break;
            case 0x6:
                hybrid_vport_out &= ~(0xF);
                hybrid_vport_out |= 0x6;
                break;
            case 0x7:
                hybrid_vport_out = 0x7;
                for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) {
                    if((tp2822_vdloss&(0x1<<ch)) && (ch != 3)) {
                        hybrid_vport_out &= ~(0x1<<ch);
                        break;
                    }
                }
                if(hybrid_vport_out == 0x7) ///< 3 TVI input detected!
                    hybrid_vport_out = 0x3;
                break;
            case 0xB:
                hybrid_vport_out = 0xB;
                for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) {
                    if((tp2822_vdloss&(0x1<<ch)) && (ch != 2)) {
                        hybrid_vport_out &= ~(0x1<<ch);
                        break;
                    }
                }
                if(hybrid_vport_out == 0xB) ///< 3 TVI input detected!
                    hybrid_vport_out = 0x3;
                break;
            case 0xD:
                hybrid_vport_out = 0xD;
                for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) {
                    if((tp2822_vdloss&(0x1<<ch)) && (ch != 1)) {
                        hybrid_vport_out &= ~(0x1<<ch);
                        break;
                    }
                }
                if(hybrid_vport_out == 0xD) ///< 3 TVI input detected!
                    hybrid_vport_out = 0xC;
                break;
            case 0xE:
                hybrid_vport_out = 0xE;
                for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) {
                    if((tp2822_vdloss&(0x1<<ch)) && (ch != 0)) {
                        hybrid_vport_out &= ~(0x1<<ch);
                        break;
                    }
                }
                if(hybrid_vport_out == 0xE) ///< 3 TVI input detected!
                    hybrid_vport_out = 0xC;
                break;
            case 0xF: ///< All AHD channel are video loss
                switch(tp2822_vdloss) { ///< Check TVI input status
                    case 0x0:
                    case 0x4:
                    case 0x8:
                    case 0xC:
                    case 0xD:
                    case 0xE:
                    case 0xF:
                        hybrid_vport_out = 0xFFF3;
                        break;
                    case 0x1:
                    case 0x9:
                        hybrid_vport_out = 0xFFF6;
                        break;
                    case 0x2:
                    case 0xA:
                        hybrid_vport_out = 0xFFF5;
                        break;
                    case 0x3:
                    case 0x7:
                    case 0xB:
                        hybrid_vport_out = 0xFFFC;
                        break;
                    case 0x5:
                        hybrid_vport_out = 0xFFFA;
                        break;
                    case 0x6:
                        hybrid_vport_out = 0xFFF9;
                        break;
                }
                break;
            default:
                break;
        }
        
        is_tvi_2th_ch = 0;
        is_ahd_2th_ch = 0;
        for(ch=0; ch < TP2822_DEV_CH_MAX; ch++) { ///< Update video present/loss status
            dev_id = ch / TP2822_DEV_CH_MAX;
            if(hybrid_vport_out&(0x1<<ch)) { ///< TVI enabled
                if((tp2822_vdloss&(0x1<<ch)) == 0) {   ///< cable is connected
                    /* notify current video present */
                    if(notify && tp2822_pdev->vlos_notify)
                        tp2822_pdev->vlos_notify(dev_id+1, (is_tvi_2th_ch==0)?0:1, 0);

                    /* get input camera video fromat */
                    ret = tp2822_status_get_video_input_format(dev_id, (TP2822_DEV_CH_MAX-ch-1), &vin_fmt);
                    
                    /* switch video port output format */
                    if((ret == 0) && (vout_fmt.fmt_idx != vin_fmt.fmt_idx)) {
                        tp2822_video_set_video_output_format(dev_id, (TP2822_DEV_CH_MAX-ch-1), vin_fmt.fmt_idx, tp2822_vout_format[dev_id]);
                    }
                    
                    /* get video port output format */
                    ret = tp2822_video_get_video_output_format(dev_id, (TP2822_DEV_CH_MAX-ch-1), &vout_fmt);
                    if(ret < 0)
                        continue;
                    
                    /* notify current video format */
                    if(notify && tp2822_pdev->vfmt_notify) 
                        tp2822_pdev->vfmt_notify(dev_id+1, (is_tvi_2th_ch==0)?0:1, &vout_fmt);               
                }
                else {
                    /* notify current video loss */
                    if(notify && tp2822_pdev->vlos_notify)
                        tp2822_pdev->vlos_notify(dev_id+1, (is_tvi_2th_ch==0)?0:1, 1);
                    /* If HD cable is plugged-out, force black image output to 720P25(1CH Bypass)/Half-720P15(2CH Dual Edge) */
                    if(tp2822_vout_mode[tp2822_pdev->index] == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                        ret = tp2822_video_set_video_output_format(tp2822_pdev->index, (TP2822_DEV_CH_MAX-ch-1), TP2822_VFMT_720P30, tp2822_vout_format[tp2822_pdev->index]);
                    else
                        ret = tp2822_video_set_video_output_format(tp2822_pdev->index, (TP2822_DEV_CH_MAX-ch-1), TP2822_VFMT_720P25, tp2822_vout_format[tp2822_pdev->index]);
                    if(ret < 0)
                        continue;

                    /* notify current video format */
                    if(notify && tp2822_pdev->vfmt_notify) {
                        /* get video port output format */
                        ret = tp2822_video_get_video_output_format(tp2822_pdev->index, (TP2822_DEV_CH_MAX-ch-1), &vout_fmt);
                        if(ret < 0)
                            continue;

                        if(tp2822_vout_mode[tp2822_pdev->index] == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                            vout_fmt.frame_rate /= 2;
                        tp2822_pdev->vfmt_notify(dev_id+1, (is_tvi_2th_ch==0)?0:1, &vout_fmt);
                    }
                }
                if(is_tvi_2th_ch == 0)
                    is_tvi_2th_ch = 1;
            }
            else { ///< AHD enabled
                /* video signal check */
                if(notify && tp2822_pdev->vlos_notify) {
                    if(nvp6114_novid&(0x1<<ch)) {
                        tp2822_pdev->vlos_notify(dev_id, (is_ahd_2th_ch==0)?0:1, 1);
                    }
                    else {
                        tp2822_pdev->vlos_notify(dev_id, (is_ahd_2th_ch==0)?0:1, 0);
                    }
                }
                if(notify && tp2822_pdev->vfmt_notify) {
                    vmode = nvp6114_video_get_mode(nvp6114_pdev->index);
                    switch(vmode) { // Set AHD video format
                        case NVP6114_VMODE_PAL_960H_2CH:
                            vout_fmt.fmt_idx    = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H25].fmt_idx;
                            vout_fmt.width      = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H25].width;
                            vout_fmt.height     = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H25].height;
                            vout_fmt.prog       = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H25].prog;
                            vout_fmt.frame_rate = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H25].frame_rate;
                            break;
                        case NVP6114_VMODE_NTSC_960H_2CH:
                            vout_fmt.fmt_idx    = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H30].fmt_idx;
                            vout_fmt.width      = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H30].width;
                            vout_fmt.height     = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H30].height;
                            vout_fmt.prog       = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H30].prog;
                            vout_fmt.frame_rate = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_960H30].frame_rate;
                            break;
                        case NVP6114_VMODE_PAL_720P_2CH:
                            vout_fmt.fmt_idx    = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P25].fmt_idx;
                            vout_fmt.width      = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P25].width;
                            vout_fmt.height     = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P25].height;
                            vout_fmt.prog       = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P25].prog;
                            vout_fmt.frame_rate = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P25].frame_rate;
                            break;
                        case NVP6114_VMODE_NTSC_720P_2CH:
                        default:
                            vout_fmt.fmt_idx    = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P30].fmt_idx;
                            vout_fmt.width      = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P30].width;
                            vout_fmt.height     = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P30].height;
                            vout_fmt.prog       = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P30].prog;
                            vout_fmt.frame_rate = tp2822_nvp6114_video_fmt_info[TP2822_VFMT_720P30].frame_rate;
                            break;
                    }
                    tp2822_pdev->vfmt_notify(dev_id, (is_ahd_2th_ch==0)?0:1, &vout_fmt);
               }
               if(is_ahd_2th_ch == 0)
                   is_ahd_2th_ch = 1;
            }
        }

        switch(hybrid_vport_out&0xF) { ///< Reorder the channel sequence
            case 0xC: ///< 4'b1100 -> VIN3&4: TVI output, VIN1&2 AHD output
                nvp6114_vport2_seq_val = 0x10;
                tp2822_vd1sel_val = 0x01; ///< VIN3&4 = TP2822_VIN#1&0
                break;
            case 0xA: ///< 4'b1010 -> VIN2&4: TVI output, VIN1&3 AHD output
                nvp6114_vport2_seq_val = 0x20;
                tp2822_vd1sel_val = 0x02; ///< VIN2&4 = TP2822_VIN#2&0
                break;
            case 0x9: ///< 4'b1001 -> VIN1&4: TVI output, VIN2&3 AHD output
                nvp6114_vport2_seq_val = 0x21;
                tp2822_vd1sel_val = 0x03; ///< VIN1&4 = TP2822_VIN#3&0
                break;
            case 0x6: ///< 4'b0110 -> VIN2&3: TVI output, VIN1&4 AHD output
                nvp6114_vport2_seq_val = 0x30;
                tp2822_vd1sel_val = 0x12; ///< VIN2&3 = TP2822_VIN#3&0
                break;
            case 0x5: ///< 4'b0101 -> VIN1&3: TVI output, VIN2&4 AHD output
                nvp6114_vport2_seq_val = 0x31;
                tp2822_vd1sel_val = 0x13; ///< VIN1&3 = TP2822_VIN#3&1
                break;
            case 0x3: ///< 4'b0011 -> VIN1&2: TVI output, VIN3&4 AHD output
                nvp6114_vport2_seq_val = 0x32;
                tp2822_vd1sel_val = 0x23; ///< VIN1&2 = TP2822_VIN#3&2
                break;
            default:
                if(hybrid_dbg_mode)
                    printk("Can't update TP2822 VD11SEL/VD12SEL at Dev#%d!\n", dev_id);
                continue;
        }

        ///< Keep from wrong channel changing

        reversed_val = ((tp2822_vd1sel_val&0x0F)<<4) | ((tp2822_vd1sel_val&0xF0)>>4);
        if((((last_tp2822_vd1sel_val^reversed_val)&0x0F) == 0) || (((last_tp2822_vd1sel_val^reversed_val)&0xF0) == 0))
            tp2822_vd1sel_val = reversed_val;

        reversed_val = ((nvp6114_vport2_seq_val&0x0F)<<4) | ((nvp6114_vport2_seq_val&0xF0)>>4);
        if((((last_nvp6114_vport2_seq_val^reversed_val)&0x0F) == 0) || (((last_nvp6114_vport2_seq_val^reversed_val)&0xF0) == 0))
            nvp6114_vport2_seq_val = reversed_val;

        last_tp2822_vd1sel_val = tp2822_vd1sel_val;
        last_nvp6114_vport2_seq_val = nvp6114_vport2_seq_val;

        /* Update TP2822 VD11SEL/VD12SEL & NVP6114 VPORT2_SEQ */
        ret = nvp6114_i2c_write(dev_id, 0xFF, 0x1);
        if(ret < 0){
            if(hybrid_dbg_mode)
                printk("Can't update NVP6114#%d VPORT2_SEQ!\n", dev_id);
            continue;
        }
        ret = nvp6114_i2c_write(dev_id, 0xC2, nvp6114_vport2_seq_val);
        if(ret < 0){
            if(hybrid_dbg_mode)
                printk("Can't update NVP6114#%d VPORT2_SEQ!\n", dev_id);
            continue;
        }
        ret = tp2822_i2c_write(dev_id, 0xF6, tp2822_vd1sel_val);
        if(ret != 0){
            if(hybrid_dbg_mode)
                printk("Can't update TP2822#%d VD11SEL/VD12SEL!\n", dev_id);
            continue;
        }

        up(&nvp6114_pdev->lock);
        up(&tp2822_pdev->lock);
        /* sleep 0.5 second */
        schedule_timeout_interruptible(msecs_to_jiffies(500));
    } while (!kthread_should_stop());

    return 0;
}

static int proc_hybrid_vport_out_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\n0x%x\n", hybrid_vport_out);   
    return 0;
}

static int proc_hybrid_dbg_mode_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\n%d\n", hybrid_dbg_mode);   
    return 0;
}

static ssize_t proc_hybrid_vport_out_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int vport_out;
    char value_str[16] = {'\0'};
        
    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "0x%x\n", &vport_out);

    hybrid_vport_out = (ushort)vport_out;

    return count;
}

static ssize_t proc_hybrid_dbg_mode_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int dbg_mode;
    char value_str[16] = {'\0'};

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &dbg_mode);

    hybrid_dbg_mode = dbg_mode ? 1 : 0;

    return count;
}

static int proc_hybrid_vport_out_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_hybrid_vport_out_show, PDE(inode)->data);
}

static int proc_hybrid_dbg_mode_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_hybrid_dbg_mode_show, PDE(inode)->data);
}

static struct file_operations proc_hybrid_vport_out_ops = {
    .owner  = THIS_MODULE,
    .open   = proc_hybrid_vport_out_open,
    .write  = proc_hybrid_vport_out_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations proc_hybrid_dbg_mode_ops = {
    .owner  = THIS_MODULE,
    .open   = proc_hybrid_dbg_mode_open,
    .write  = proc_hybrid_dbg_mode_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void hybrid_proc_remove(void)
{
    if(proc_hybrid_vport_out) {
        remove_proc_entry(proc_hybrid_vport_out->name, NULL);
        proc_hybrid_vport_out = NULL;
    }
    if(proc_hybrid_dbg_mode) {
        remove_proc_entry(proc_hybrid_dbg_mode->name, NULL);
        proc_hybrid_dbg_mode = NULL;
    }
}

static int hybrid_proc_init(void)
{
    int ret = 0;
    /* hybrid vport out */
    proc_hybrid_vport_out = create_proc_entry("hybrid_vport_out", S_IRUGO|S_IXUGO, NULL);
    if(!proc_hybrid_vport_out) {
        printk("create proc node /proc/hybrid_vport_out failed!\n");
        ret = -EINVAL;
        goto err;
    }
    proc_hybrid_vport_out->proc_fops = &proc_hybrid_vport_out_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    proc_hybrid_vport_out->owner     = THIS_MODULE;
#endif
    /* hybrid dbg mode */
    proc_hybrid_dbg_mode = create_proc_entry("hybrid_dbg_mode", S_IRUGO|S_IXUGO, NULL);
    if(!proc_hybrid_dbg_mode) {
        printk("create proc node /proc/hybrid_dbg_mode failed!\n");
        ret = -EINVAL;
        goto err;
    }
    proc_hybrid_dbg_mode->proc_fops = &proc_hybrid_dbg_mode_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    proc_hybrid_dbg_mode->owner     = THIS_MODULE;
#endif

    return ret;
err:
    hybrid_proc_remove();
    return ret;
}

static int __init tp2822_nvp6114_init(void)
{
    int i, ret;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(tp2822_dev_num > TP2822_DEV_MAX) {
        printk("TP2822 tp2822_dev_num=%d invalid!!(Max=%d)\n", tp2822_dev_num, TP2822_DEV_MAX);
        return -1;
    }
    if(nvp6114_dev_num > NVP6114_DEV_MAX) {
        printk("NVP6114 nvp6114_dev_num=%d invalid!!(Max=%d)\n", nvp6114_dev_num, NVP6114_DEV_MAX);
        return -1;
    }

    /* check video output format */
    for(i=0; i<tp2822_dev_num; i++) {
        if((tp2822_vout_format[i] < 0) || (tp2822_vout_format[i] >= TP2822_VOUT_FORMAT_MAX)) {
            printk("TP2822#%d vout_format=%d not support!!\n", i, tp2822_vout_format[i]);
            return -1;
        }
    }

    /* register i2c client for contol tp2822 & nvp6114 */
    ret = tp2822_nvp6114_i2c_register();
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

    /* create channel mapping table for different board */
    ret = tp2822_nvp6114_create_ch_map(ch_map[0]);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    tp2822_nvp6114_hw_reset();

    /* Passing required parameters to tp2822_dev.c & nvp6114_dev.c */
    tp2822_nvp6114_params_passing();

    /* TP2822 init */
    for(i=0; i<tp2822_dev_num; i++) {
        tp2822_dev[i].index = i;

        sprintf(tp2822_dev[i].name, "tp2822.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&tp2822_dev[i].lock, 1);
#else
        init_MUTEX(&tp2822_dev[i].lock);
#endif
        ret = tp2822_proc_init(i);
        if(ret < 0)
            goto err;

        ret = tp2822_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = tp2822_device_init(i);
        if(ret != 0)
            goto err;

    }

    /* NVP6114 init */
    for(i=0; i<nvp6114_dev_num; i++) {
        nvp6114_dev[i].index = i;

        sprintf(nvp6114_dev[i].name, "nvp6114.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&nvp6114_dev[i].lock, 1);
#else
        init_MUTEX(&nvp6114_dev[i].lock);
#endif
        ret = nvp6114_proc_init(i);
        if(ret < 0)
            goto err;

        ret = nvp6114_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = nvp6114_device_init(i);
        if(ret < 0)
            goto err;
    }

    /* create hybrid proc node */
    ret = hybrid_proc_init();
    if(ret < 0)
            goto err;

    if(init) {
        /* init tp2822_nvp6114 watchdog thread for check video status */
        tp2822_nvp6114_wd = kthread_create(tp2822_nvp6114_watchdog_thread, NULL, "tp2822_nvp6114_wd");
        if(!IS_ERR(tp2822_nvp6114_wd))
            wake_up_process(tp2822_nvp6114_wd);
        else {
            printk("create tp2822 watchdog thread failed!!\n");
            tp2822_nvp6114_wd = 0;
            ret = -EFAULT;
            goto err;
        }
    }

    return 0;

err:
    if(tp2822_nvp6114_wd)
        kthread_stop(tp2822_nvp6114_wd);

    tp2822_nvp6114_i2c_unregister();
    for(i=0; i<TP2822_DEV_MAX; i++) {
        if(tp2822_dev[i].miscdev.name)
            misc_deregister(&tp2822_dev[i].miscdev);

        tp2822_proc_remove(i);
    }
    for(i=0; i<NVP6114_DEV_MAX; i++) {
        if(nvp6114_dev[i].miscdev.name)
            misc_deregister(&nvp6114_dev[i].miscdev);

        nvp6114_proc_remove(i);
    }
    
    hybrid_proc_remove();

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit tp2822_nvp6114_exit(void)
{
    int i;

    /* stop tp2822_nvp6114 watchdog thread */
    if(tp2822_nvp6114_wd)
        kthread_stop(tp2822_nvp6114_wd);

    tp2822_nvp6114_i2c_unregister();

    for(i=0; i<TP2822_DEV_MAX; i++) {
        /* remove device node */
        if(tp2822_dev[i].miscdev.name)
            misc_deregister(&tp2822_dev[i].miscdev);

        /* remove proc node */
        tp2822_proc_remove(i);
    }
    for(i=0; i<NVP6114_DEV_MAX; i++) {
        /* remove device node */
        if(nvp6114_dev[i].miscdev.name)
            misc_deregister(&nvp6114_dev[i].miscdev);

        /* remove proc node */
        nvp6114_proc_remove(i);
    }
    
    /* remove hybrid proc node */
    hybrid_proc_remove();

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    plat_audio_deregister_function();
}

module_init(tp2822_nvp6114_init);
module_exit(tp2822_nvp6114_exit);

MODULE_DESCRIPTION("Grain Media - TechPoint TP2822 4CH HDTVI & Nextchip NVP6114 4CH AHD Hybrid Receiver Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
