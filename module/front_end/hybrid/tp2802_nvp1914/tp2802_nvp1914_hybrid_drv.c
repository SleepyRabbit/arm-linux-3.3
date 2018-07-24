/*
 * @file tp2802_nvp1914_hybrid_drv.c
 * TechPoint TP2802 4CH HD-TVI & Nextchip NVP1914 4-CH SD Hybrid Receiver Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2015/01/16 06:26:05 $
 *
 * ChangeLog:
 *  $Log: tp2802_nvp1914_hybrid_drv.c,v $
 *  Revision 1.1  2015/01/16 06:26:05  shawn_hu
 *  Add HD-TVI TP2802 + D1 NVP1914 hybrid frontend driver design for TVT product.
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
#include "techpoint/tp2802.h"          ///< from module/include/front_end/hdtvi/techpoint
#include "nextchip/nvp1914.h"          ///< from module/include/front_end/decoder
#include "tp2802_nvp1914_hybrid_drv.h"
#include "tp2802_dev.h"
#include "nvp1914_dev.h"

/*************************************************************************************
 *  Module Parameters
 *************************************************************************************/
/* device number */
int tp2802_dev_num = TP2802_DEV_MAX;
module_param(tp2802_dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2802_dev_num, "TP2802 Device Number: 1~4");

int nvp1914_dev_num = NVP1914_DEV_MAX;
module_param(nvp1914_dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp1914_dev_num, "NVP1914 Device Number: 1~4");

/* device i2c bus */
static int ibus = 0;
module_param(ibus, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ibus, "Device I2C Bus: 0~4");

/* device i2c address */
ushort tp2802_iaddr[TP2802_DEV_MAX] = {0x88, 0x8a, 0x8c, 0x8e};
module_param_array(tp2802_iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2802_iaddr, "TP2802 Device I2C Address");

ushort nvp1914_iaddr[NVP1914_DEV_MAX] = {0x60, 0x62, 0x64, 0x66};
module_param_array(nvp1914_iaddr, ushort, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp1914_iaddr, "NVP1914 Device I2C Address");

/* device video mode */
int tp2802_vout_format[TP2802_DEV_MAX] = {[0 ... (TP2802_DEV_MAX - 1)] = TP2802_VOUT_FORMAT_BT656};
module_param_array(tp2802_vout_format, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tp2802_vout_format, "Video Output Format: 0:BT656, 1:BT1120");

int nvp1914_vmode[NVP1914_DEV_MAX] = {[0 ... (NVP1914_DEV_MAX - 1)] = NVP1914_VMODE_NTSC_720H_1CH};
module_param_array(nvp1914_vmode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp1914_vmode, "Video Mode");

/* audio sample rate */
int nvp1914_sample_rate = AUDIO_SAMPLERATE_8K;
module_param(nvp1914_sample_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp1914_sample_rate, "audio sample rate: 0:8k  1:16k  2:32k 3:44.1k 4:48k");

/* audio sample size */
int nvp1914_sample_size = AUDIO_BITS_16B;
module_param(nvp1914_sample_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nvp1914_sample_size, "audio sample size: 0:16bit  1:8bit");

/* NVP1914 audio channel number */
int audio_chnum = 4;
module_param(audio_chnum, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(audio_chnum, "audio channel number");

/* clock port used */
static int clk_used = 0;
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
static int rstb_used = 0;
module_param(rstb_used, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rstb_used, "Use GPIO as RSTB Pin: 0:None, 1:X_CAP_RST");

/* device channel mapping */
static int ch_map = 0;
module_param(ch_map, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_map, "channel mapping");

/* device init */
int init = 1;
module_param(init, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(init, "device init: 0:no-init  1:init");

/* notify */
static int notify = 1;
module_param(notify, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Device Notify => 0:Disable 1:Enable");

/* Hybrid VPORT Output mode */
static ushort hybrid_vport_out = 0xFFFF; // DEV_MAX = 4
module_param(hybrid_vport_out, short, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Hybrid VPORT Output Mode (bitmap) => 0:SD (NVP1914) 1:HD (TP2802)");
/************************************************************************************** 
  e.g. hybrid_vport_out = 0x000F = 16b'0000000000001111, that means:
  TP2802_Dev#0 HD_OUT[0~3] -> all enabled = 4 * 1-CH HD bypass output
  if hybrid_vport_out = 0x00F3 = 16b'0000000011110011, that means:
  TP2802_Dev#0 HD_OUT[0~1] -> enabled, NVP1914_Dev#0_SD_OUT[2~3] -> enabled
  TP2802_Dev#1 HD_OUT[0~3] -> all enabled = 4 * 1-CH HD bypass output 
  = 2 * 1-CH HD bypass output + 2 * 1-CH SD bypass output + 4 * 1-CH HD bypass output
**************************************************************************************/

/* HD/SD automatic switch */
static int auto_switch = 0; // if enabled, notify should be also set to 1
module_param(auto_switch, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(auto_switch, "Auto HD/SD switching: 0:Disable  1:Enable");
/******************************************************************************
  If this feature is enabled, it will automatically switch to HD or SD mode
  according to the state of video present/lost on that capture port. And if 
  both HD/SD video are lost, just keep at current setting.
******************************************************************************/

/******************************************************************************
 Customer's GM8210 EVB Channel and CVI Mapping Table

 VCH#0 ------> TP2802#TVI#0.0 -------> TP2802#VOUT#0.0 -------> X_CAP#3
 VCH#1 ------> TP2802#TVI#0.1 -------> TP2802#VOUT#0.1 -------> X_CAP#2
 VCH#2 ------> TP2802#TVI#0.2 -------> TP2802#VOUT#0.2 -------> X_CAP#1
 VCH#3 ------> TP2802#TVI#0.3 -------> TP2802#VOUT#0.3 -------> X_CAP#0
 VCH#4 ------> TP2802#TVI#1.0 -------> TP2802#VOUT#1.0 -------> X_CAP#8
 VCH#5 ------> TP2802#TVI#1.1 -------> TP2802#VOUT#1.1 -------> X_CAP#7
 VCH#6 ------> TP2802#TVI#1.2 -------> TP2802#VOUT#1.2 -------> X_CAP#6
 VCH#7 ------> TP2802#TVI#1.3 -------> TP2802#VOUT#1.3 -------> X_CAP#5

 -Attention! The connection between VCH to NVP1914 VIN is in inversed order -

 VCH#0 ------> NVP1914#VIN#0.3 -------> NVP1914#VOUT#0.0 -------> X_CAP#3
 VCH#1 ------> NVP1914#VIN#0.2 -------> NVP1914#VOUT#0.1 -------> X_CAP#2
 VCH#2 ------> NVP1914#VIN#0.1 -------> NVP1914#VOUT#0.2 -------> X_CAP#1
 VCH#3 ------> NVP1914#VIN#0.0 -------> NVP1914#VOUT#0.3 -------> X_CAP#0
 VCH#4 ------> NVP1914#VIN#1.3 -------> NVP1914#VOUT#1.0 -------> X_CAP#8
 VCH#5 ------> NVP1914#VIN#1.2 -------> NVP1914#VOUT#1.1 -------> X_CAP#7
 VCH#6 ------> NVP1914#VIN#1.1 -------> NVP1914#VOUT#1.2 -------> X_CAP#6
 VCH#7 ------> NVP1914#VIN#1.0 -------> NVP1914#VOUT#1.3 -------> X_CAP#5

*******************************************************************************/

/*************************************************************************************
 *  Original structure
 *************************************************************************************/
struct tp2802_dev_t tp2802_dev[TP2802_DEV_MAX];
struct nvp1914_dev_t nvp1914_dev[NVP1914_DEV_MAX];
static audio_funs_t nvp1914_audio_funs;

/*************************************************************************************
 *  Data structure or APIs for hybrid design
 *************************************************************************************/
static struct i2c_client  *tp2802_nvp1914_i2c_client = NULL;
static struct task_struct *tp2802_nvp1914_wd  = NULL;
static ushort last_hybrid_vport_out;
static struct proc_dir_entry *proc_hybrid_vport_out = NULL;
static struct proc_dir_entry *proc_hybrid_dbg_mode = NULL;
static int hybrid_dbg_mode = 0; // Flag for debug mode enable/disable

u32 TP2802_NVP1914_VCH_MAP[TP2802_DEV_MAX*TP2802_DEV_CH_MAX];

enum {
    NVP1914_VFMT_960H25 = TP2802_VFMT_MAX,
    NVP1914_VFMT_960H30,
    NVP1914_VFMT_720H25,
    NVP1914_VFMT_720H30,
    TP2802_NVP1914_VFMT_MAX
};

struct tp2802_video_fmt_t tp2802_nvp1914_video_fmt_info[TP2802_NVP1914_VFMT_MAX] = {
    /* Idx                    Width   Height  Prog    FPS */
    {  TP2802_VFMT_720P60,     1280,   720,    1,      60  },  ///< 720P @60
    {  TP2802_VFMT_720P50,     1280,   720,    1,      50  },  ///< 720P @50
    {  TP2802_VFMT_1080P30,    1920,   1080,   1,      30  },  ///< 1080P@30
    {  TP2802_VFMT_1080P25,    1920,   1080,   1,      25  },  ///< 1080P@25
    {  TP2802_VFMT_720P30,     1280,   720,    1,      30  },  ///< 720P @30
    {  TP2802_VFMT_720P25,     1280,   720,    1,      25  },  ///< 720P @25
    {  NVP1914_VFMT_960H25,     960,   576,    0,      50  },  ///< 960H @25
    {  NVP1914_VFMT_960H30,     960,   480,    0,      60  },  ///< 960H @30
    {  NVP1914_VFMT_720H25,     720,   576,    0,      50  },  ///< 720H @25
    {  NVP1914_VFMT_720H30,     720,   480,    0,      60  },  ///< 720H @30
};

int tp2802_get_device_num(void)
{
    // Let vcap300_tp2802 create maximum bypass video out path
    return (nvp1914_dev_num >= tp2802_dev_num) ? nvp1914_dev_num : tp2802_dev_num;
}
EXPORT_SYMBOL(tp2802_get_device_num);

static int tp2802_nvp1914_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    tp2802_nvp1914_i2c_client = client;
    return 0;
}

static int tp2802_nvp1914_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tp2802_nvp1914_i2c_id[] = {
    { "tp2802_nvp1914_i2c", 0 },
    { }
};

static struct i2c_driver tp2802_nvp1914_i2c_driver = {
    .driver = {
        .name  = "TP2802_NVP1914_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = tp2802_nvp1914_i2c_probe,
    .remove   = tp2802_nvp1914_i2c_remove,
    .id_table = tp2802_nvp1914_i2c_id
};

static int tp2802_nvp1914_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&tp2802_nvp1914_i2c_driver);
    if(ret < 0) {
        printk("TP2802 can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = tp2802_iaddr[0]>>1;
    strlcpy(info.type, "tp2802_nvp1914_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("TP2802&NVP1914 can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("TP2802&NVP1914 can't add i2c device!\n");
        goto err_driver;
    }
    
    return 0;

err_driver:
   i2c_del_driver(&tp2802_nvp1914_i2c_driver);
   return -ENODEV;
}

static void tp2802_nvp1914_i2c_unregister(void)
{
    i2c_unregister_device(tp2802_nvp1914_i2c_client);
    i2c_del_driver(&tp2802_nvp1914_i2c_driver);
    tp2802_nvp1914_i2c_client = NULL;
}

extern void gpio_i2c_write(u8 addr, u8 reg, u8 data);

int tp2802_nvp1914_i2c_write(u8 addr, u8 reg, u8 data)
{
#if 1
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(!tp2802_nvp1914_i2c_client) {
        printk("TP2802&NVP1914 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2802_nvp1914_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = addr>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        if(hybrid_dbg_mode)
            printk("TP2802 or NVP1914 @ 0x%02x i2c write failed!!\n", addr);
        return -1;
    }
#else
    gpio_i2c_write(addr, reg, data);
#endif
    return 0;
}

extern u8 gpio_i2c_read(u8 addr, u8 reg);

u8 tp2802_nvp1914_i2c_read(u8 addr, u8 reg)
{
    u8 reg_data = 0;
#if 1
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(!tp2802_nvp1914_i2c_client) {
        printk("TP2802&NVP1914 i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(tp2802_nvp1914_i2c_client->dev.parent);

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
            printk("TP2802 or NVP1914 @ 0x%02x i2c read failed!!\n", addr);
    }
#else
    reg_data = gpio_i2c_read(addr, reg);
#endif
    return reg_data;
}

/* Due to TP2802 I2C is not stable, add some error checking on I2C read */
int tp2802_nvp1914_i2c_read_ext(u8 addr, u8 reg)
{
    u8 reg_data = 0;
#if 1
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(!tp2802_nvp1914_i2c_client) {
        printk("TP2802&NVP1914 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2802_nvp1914_i2c_client->dev.parent);

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
            printk("TP2802 or NVP1914 @ 0x%02x i2c read failed!!\n", addr);
        return -1;
    }
#else
    reg_data = gpio_i2c_read(addr, reg);
#endif
    return (int)reg_data;
}

static int tp2802_nvp1914_create_ch_map(int map_id)
{
    int i;

    switch(map_id) {
        case 0: ///< for customer's GM8210 EVB
        default:
            for(i=0; i<tp2802_get_device_num(); i++) {
                TP2802_NVP1914_VCH_MAP[(i*TP2802_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, TP2802_DEV_VOUT0);
                TP2802_NVP1914_VCH_MAP[(i*TP2802_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, TP2802_DEV_VOUT1);
                TP2802_NVP1914_VCH_MAP[(i*TP2802_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, TP2802_DEV_VOUT2);
                TP2802_NVP1914_VCH_MAP[(i*TP2802_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, TP2802_DEV_VOUT3);
            }
            break;
    }

    return 0;
}

static void tp2802_nvp1914_hw_reset(void)
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

static void tp2802_nvp1914_params_passing(void)
{
    struct tp2802_params_t tp2802_params;
    struct nvp1914_params_t nvp1914_params;

    tp2802_params.tp2802_dev_num = tp2802_dev_num;
    tp2802_params.tp2802_iaddr = &tp2802_iaddr[0];
    tp2802_params.tp2802_vout_format = &tp2802_vout_format[0];
    tp2802_params.tp2802_dev = &tp2802_dev[0];
    tp2802_params.init = init;
    tp2802_params.TP2802_NVP1914_VCH_MAP = &TP2802_NVP1914_VCH_MAP[0];
    tp2802_set_params(&tp2802_params);

    nvp1914_params.nvp1914_dev_num = nvp1914_dev_num;
    nvp1914_params.nvp1914_iaddr = &nvp1914_iaddr[0];
    nvp1914_params.nvp1914_vmode = &nvp1914_vmode[0];
    nvp1914_params.nvp1914_sample_rate = nvp1914_sample_rate;
    nvp1914_params.nvp1914_sample_size = nvp1914_sample_size;
    nvp1914_params.nvp1914_dev = &nvp1914_dev[0];
    nvp1914_params.init = init;
    nvp1914_params.TP2802_NVP1914_VCH_MAP = &TP2802_NVP1914_VCH_MAP[0];
    nvp1914_set_params(&nvp1914_params);
}

#define RETRY_INTERVAL 200 // 200ms
#define NUMBER_OF_RETRY 3 // retry 3 times when I2C access got failed

static int tp2802_nvp1914_watchdog_thread(void *data)
{
    int ret, i, ch;
    int dev_id, novid, vmode, total_ch_num, vlos, debounce = 0, retry;
    ushort hybrid_vport_out_mask, hybrid_vport_out_xor;
    u8 val;

    struct tp2802_dev_t *tp2802_pdev;
    struct nvp1914_dev_t *nvp1914_pdev;
    struct tp2802_video_fmt_t vin_fmt, vout_fmt;

    total_ch_num = tp2802_get_device_num() * TP2802_DEV_CH_MAX;
    switch((tp2802_dev_num >= nvp1914_dev_num) ? nvp1914_dev_num : tp2802_dev_num){
        case 4:
            hybrid_vport_out_mask = 0xFFFF;
            break;
        case 3:
            hybrid_vport_out_mask = 0x0FFF;
            break;
        case 2:
            hybrid_vport_out_mask = 0x00FF;
            break;
        case 1:
            hybrid_vport_out_mask = 0x000F;
            break;
        default:
            hybrid_vport_out_mask = 0x0000;
            break;
    }

    do {
        hybrid_vport_out_xor = hybrid_vport_out ^ last_hybrid_vport_out;
        if(hybrid_vport_out_xor & hybrid_vport_out_mask){ // check hybrid vport output mode switching
            for(i=0; i < total_ch_num; i++) {
                if(hybrid_vport_out_xor & (0x1<<i)) {
                    dev_id = i / TP2802_DEV_CH_MAX;
                    ch = i % TP2802_DEV_CH_MAX;
                    tp2802_pdev = &tp2802_dev[dev_id];
                    nvp1914_pdev = &nvp1914_dev[dev_id];
                    down(&tp2802_pdev->lock);
                    down(&nvp1914_pdev->lock);
                    /* HD/SD switching */
                    if(hybrid_vport_out & (0x1<<i)) { // HD enabled
                        /* Disable SD output from NVP1914 */
                        ret = nvp1914_i2c_write(dev_id, 0xFF, 0x1); // switch to bank#1
                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't switch bank at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xFF, 0x1);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val = nvp1914_i2c_read(dev_id, 0xCA);
                        ret = nvp1914_i2c_write(dev_id, 0xCA, (val & ~(0x1<<ch)));
                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't disable SD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xCA, (val & ~(0x1<<ch)));
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val = nvp1914_i2c_read(dev_id, 0xD4);
                        ret = nvp1914_i2c_write(dev_id, 0xD4, (val & ~(0x1<<ch)));
                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't disable SD video clock at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xD4, (val & ~(0x1<<ch)));
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        /* Enable HD output from TP2802 */
                        val = (hybrid_vport_out & hybrid_vport_out_mask)>>(dev_id*TP2802_DEV_CH_MAX); // Due to TP2802 I2C isn't stable, keep the setting by ourself
                        ret = tp2802_i2c_write(dev_id, 0x4D, val & 0x0F);
                        if(ret != 0){
                            if(hybrid_dbg_mode)
                                printk("Can't enable HD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = tp2802_i2c_write(dev_id, 0x4D, val & 0x0F);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val &= 0x0F;
                        if(val & 0x8) // Check if CH#3 is in HD mode, bit[3] = 1
                            val |= 0x80; // Force CLKO4 output driver to 20mA
                        ret = tp2802_i2c_write(dev_id, 0x4E, val);
                        if(ret != 0){
                            if(hybrid_dbg_mode)
                                printk("Can't enable HD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = tp2802_i2c_write(dev_id, 0x4E, val);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                    }
                    else { // SD enabled
                        /* Disable HD output from TP2802 */
                        val = (hybrid_vport_out & hybrid_vport_out_mask)>>(dev_id*TP2802_DEV_CH_MAX); // Due to TP2802 I2C isn't stable, keep the setting by ourself
                        ret = tp2802_i2c_write(dev_id, 0x4D, val & 0x0F);
                        if(ret != 0){
                            if(hybrid_dbg_mode)
                                printk("Can't disable HD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = tp2802_i2c_write(dev_id, 0x4D, val & 0x0F);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val &= 0x0F;
                        if(val & 0x8) // Check if CH#3 is in HD mode, bit[3] = 1
                            val |= 0x80; // Force CLKO4 output driver to 20mA
                        ret = tp2802_i2c_write(dev_id, 0x4E, val);
                        if(ret != 0){
                            if(hybrid_dbg_mode)
                                printk("Can't disable HD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = tp2802_i2c_write(dev_id, 0x4E, val);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        /* Enable SD output from NVP1914 */
                        ret = nvp1914_i2c_write(dev_id, 0xFF, 0x1); // switch to bank#1
                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't switch bank at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xFF, 0x1);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val = nvp1914_i2c_read(dev_id, 0xCA);
                        ret = nvp1914_i2c_write(dev_id, 0xCA, (val | (0x1<<ch)));
                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't enable SD video output at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xCA, (val | (0x1<<ch)));
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                        val = nvp1914_i2c_read(dev_id, 0xD4);
                        ret = nvp1914_i2c_write(dev_id, 0xD4, (val | (0x1<<ch)));

                        if(ret < 0){
                            if(hybrid_dbg_mode)
                                printk("Can't enable SD video clock at Dev#%d!\n", dev_id);
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = nvp1914_i2c_write(dev_id, 0xD4, (val | (0x1<<ch)));
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            continue;
                        }
                    }
                    if(hybrid_dbg_mode)
                        printk("Issue a notification for HD/SD switching on Dev#%d-CH%d!\n", dev_id, ch);
                    up(&nvp1914_pdev->lock);
                    up(&tp2802_pdev->lock);
                }
            }
        last_hybrid_vport_out = hybrid_vport_out;
        }
        else { // check channel status
            for(i=0; i < total_ch_num; i++) {
                dev_id = i / TP2802_DEV_CH_MAX;
                ch = i % TP2802_DEV_CH_MAX;
                tp2802_pdev = &tp2802_dev[dev_id];
                nvp1914_pdev = &nvp1914_dev[dev_id];
                down(&tp2802_pdev->lock);
                down(&nvp1914_pdev->lock);

                if((hybrid_vport_out & hybrid_vport_out_mask) & (0x1<<i)) { // HD enabled, check tp2802 channel status
                    for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C read failed
                        ret = tp2802_status_get_video_loss(dev_id, ch);
                        if(ret >= 0)
                            break;
                        msleep(RETRY_INTERVAL);
                    }
                    tp2802_pdev->cur_plugin[ch] = (ret == 0) ? 1 : 0;
            
                    if((tp2802_pdev->cur_plugin[ch] == 1) && (tp2802_pdev->pre_plugin[ch] == 0)) {        ///< cable plugged-in
                        goto sts_update;
                    }
                    else if((tp2802_pdev->cur_plugin[ch] == 1) && (tp2802_pdev->pre_plugin[ch] == 1)) {   ///< cable is connected
                        novid = nvp1914_status_get_novid(nvp1914_pdev->index);
                        if(!(novid & (0x1<<(NVP1914_DEV_CH_MAX-ch-1)))) { // SD input is present, switch to SD mode
                            if(auto_switch) {
                                hybrid_vport_out &= ~(0x1<<(ch+dev_id*TP2802_DEV_CH_MAX));
                            }
                        }
                        else { // HD input is present
                            /* notify current video present */
                            if(notify && tp2802_pdev->vlos_notify)
                                tp2802_pdev->vlos_notify(dev_id, ch, 0);

                            /* get video port output format */
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C read failed
                                ret = tp2802_video_get_video_output_format(dev_id, ch, &vout_fmt);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                            if(ret < 0)
                                goto sts_update;

                            /* get input camera video fromat */
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C read failed
                                ret = tp2802_status_get_video_input_format(dev_id, ch, &vin_fmt);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }

                            /* switch video port output format */
                            if((ret == 0) && (vout_fmt.fmt_idx != vin_fmt.fmt_idx)) {
                                for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                    ret = tp2802_video_set_video_output_format(dev_id, ch, vin_fmt.fmt_idx, tp2802_vout_format[dev_id]);
                                    if(ret == 0)
                                        break;
                                    msleep(RETRY_INTERVAL);
                                }
                            }

                            /* notify current video format */
                            if(notify && tp2802_pdev->vfmt_notify) { 
                                if(ret == 0) // Check if "tp2802_video_set_video_output_format()" succeeds, yes -> new format is applicable
                                    tp2802_pdev->vfmt_notify(dev_id, ch, &vin_fmt);
                                else // keep original video output format
                                    tp2802_pdev->vfmt_notify(dev_id, ch, &vout_fmt);
                            }
                        }
                    }
                    else {  ///< cable is plugged-out
                        /* notify current video loss */
                        if(notify && tp2802_pdev->vlos_notify)
                            tp2802_pdev->vlos_notify(dev_id, ch, 1);

                        /* If HD cable is plugged-out, force black image output to 720P30 */
                        if((tp2802_pdev->cur_plugin[ch] == 0) && (tp2802_pdev->pre_plugin[ch] == 1)) { // notify once when cable was plugged-out
                            for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C write failed
                                ret = tp2802_video_set_video_output_format(dev_id, ch, TP2802_VFMT_720P30, tp2802_vout_format[dev_id]);
                                if(ret == 0)
                                    break;
                                msleep(RETRY_INTERVAL);
                            }
                        
                            if(notify && tp2802_pdev->vfmt_notify && (ret == 0)) {
                                vout_fmt.fmt_idx    = tp2802_nvp1914_video_fmt_info[TP2802_VFMT_720P30].fmt_idx;
                                vout_fmt.width      = tp2802_nvp1914_video_fmt_info[TP2802_VFMT_720P30].width;
                                vout_fmt.height     = tp2802_nvp1914_video_fmt_info[TP2802_VFMT_720P30].height;
                                vout_fmt.prog       = tp2802_nvp1914_video_fmt_info[TP2802_VFMT_720P30].prog;
                                vout_fmt.frame_rate = tp2802_nvp1914_video_fmt_info[TP2802_VFMT_720P30].frame_rate;

                                tp2802_pdev->vfmt_notify(dev_id, ch, &vout_fmt);
                            }
                        }
                    }
sts_update:     
                    tp2802_pdev->pre_plugin[ch] = tp2802_pdev->cur_plugin[ch];
                }                
                else { // SD enabled, check nvp1914 video status
                    /* video signal check */
                    if(notify && tp2802_pdev->vlos_notify) {
                        novid = nvp1914_status_get_novid(nvp1914_pdev->index);
                        
                        if(novid & (0x1<<(NVP1914_DEV_CH_MAX-ch-1))) {
                            tp2802_pdev->vlos_notify(dev_id, ch, 1);
                            if(auto_switch) {
                                for(retry = 0; retry < NUMBER_OF_RETRY; retry++) { // retry when I2C read failed
                                    ret = tp2802_status_get_video_loss(dev_id, ch);
                                    if(ret >= 0)
                                        break;
                                    msleep(RETRY_INTERVAL);
                                }
                                vlos = ret;
                                if(vlos == 0) { // HD video present
                                    debounce++;
                                    if(debounce == 2) {
                                        hybrid_vport_out |= 0x1<<(ch+dev_id*TP2802_DEV_CH_MAX);
                                        debounce = 0;
                                    }
                                }
                            }
                        }
                        else
                            tp2802_pdev->vlos_notify(dev_id, ch, 0);
                    }
                    /* video format check */
                    if(notify && tp2802_pdev->vfmt_notify) {
                        vmode = nvp1914_video_get_mode(nvp1914_pdev->index);
                        switch(vmode) { // Set SD video format
                            case NVP1914_VMODE_PAL_960H_1CH:
                                vout_fmt.fmt_idx    = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H25].fmt_idx;
                                vout_fmt.width      = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H25].width;
                                vout_fmt.height     = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H25].height;
                                vout_fmt.prog       = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H25].prog;
                                vout_fmt.frame_rate = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H25].frame_rate;
                                break;
                            case NVP1914_VMODE_NTSC_960H_1CH:
                                vout_fmt.fmt_idx    = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H30].fmt_idx;
                                vout_fmt.width      = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H30].width;
                                vout_fmt.height     = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H30].height;
                                vout_fmt.prog       = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H30].prog;
                                vout_fmt.frame_rate = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_960H30].frame_rate;
                                break;
                            case NVP1914_VMODE_PAL_720H_1CH:
                                vout_fmt.fmt_idx    = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H25].fmt_idx;
                                vout_fmt.width      = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H25].width;
                                vout_fmt.height     = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H25].height;
                                vout_fmt.prog       = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H25].prog;
                                vout_fmt.frame_rate = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H25].frame_rate;
                                break;
                            case NVP1914_VMODE_NTSC_720H_1CH:
                            default:
                                vout_fmt.fmt_idx    = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H30].fmt_idx;
                                vout_fmt.width      = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H30].width;
                                vout_fmt.height     = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H30].height;
                                vout_fmt.prog       = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H30].prog;
                                vout_fmt.frame_rate = tp2802_nvp1914_video_fmt_info[NVP1914_VFMT_720H30].frame_rate;
                                break;
                        }
                        tp2802_pdev->vfmt_notify(dev_id, ch, &vout_fmt);
                    }
                }
                up(&nvp1914_pdev->lock);
                up(&tp2802_pdev->lock);
            }
        }
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

static int __init tp2802_nvp1914_init(void)
{
    int i, ret;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(tp2802_dev_num > TP2802_DEV_MAX) {
        printk("TP2802 tp2802_dev_num=%d invalid!!(Max=%d)\n", tp2802_dev_num, TP2802_DEV_MAX);
        return -1;
    }
    if(nvp1914_dev_num > NVP1914_DEV_MAX) {
        printk("NVP1914 nvp1914_dev_num=%d invalid!!(Max=%d)\n", nvp1914_dev_num, NVP1914_DEV_MAX);
        return -1;
    }

    /* check video output format */
    for(i=0; i<tp2802_dev_num; i++) {
        if((tp2802_vout_format[i] < 0) || (tp2802_vout_format[i] >= TP2802_VOUT_FORMAT_MAX)) {
            printk("TP2802#%d vout_format=%d not support!!\n", i, tp2802_vout_format[i]);
            return -1;
        }
    }

    /* register i2c client for contol tp2802 & nvp1914 */
    ret = tp2802_nvp1914_i2c_register();
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
    ret = tp2802_nvp1914_create_ch_map(ch_map);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    tp2802_nvp1914_hw_reset();

    /* Passing required parameters to tp2802_dev.c & nvp1914_dev.c */
    tp2802_nvp1914_params_passing();

    /* TP2802 init */
    for(i=0; i<tp2802_dev_num; i++) {
        tp2802_dev[i].index = i;

        sprintf(tp2802_dev[i].name, "tp2802.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&tp2802_dev[i].lock, 1);
#else
        init_MUTEX(&tp2802_dev[i].lock);
#endif
        ret = tp2802_proc_init(i);
        if(ret < 0)
            goto err;

        ret = tp2802_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = tp2802_device_init(i);
        if(ret != 0)
            goto err;

    }

    /* NVP1914 init */
    for(i=0; i<nvp1914_dev_num; i++) {
        nvp1914_dev[i].index = i;

        sprintf(nvp1914_dev[i].name, "nvp1914.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&nvp1914_dev[i].lock, 1);
#else
        init_MUTEX(&nvp1914_dev[i].lock);
#endif
        ret = nvp1914_proc_init(i);
        if(ret < 0)
            goto err;

        ret = nvp1914_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = nvp1914_device_init(i);
        if(ret < 0)
            goto err;
    }

    /* create hybrid proc node */
    ret = hybrid_proc_init();
    if(ret < 0)
            goto err;

    /* set flag */
    last_hybrid_vport_out = hybrid_vport_out;

    if(init) {
        /* init tp2802_nvp1914 watchdog thread for check video status */
        tp2802_nvp1914_wd = kthread_create(tp2802_nvp1914_watchdog_thread, NULL, "tp2802_nvp1914_wd");
        if(!IS_ERR(tp2802_nvp1914_wd))
            wake_up_process(tp2802_nvp1914_wd);
        else {
            printk("create tp2802 watchdog thread failed!!\n");
            tp2802_nvp1914_wd = 0;
            ret = -EFAULT;
            goto err;
        }
    }

    /* register audio function for platform used */
    nvp1914_audio_funs.sound_switch = nvp1914_sound_switch;
    nvp1914_audio_funs.codec_name   = nvp1914_dev[0].name;
    plat_audio_register_function(&nvp1914_audio_funs);

    return 0;

err:
    if(tp2802_nvp1914_wd)
        kthread_stop(tp2802_nvp1914_wd);

    tp2802_nvp1914_i2c_unregister();
    for(i=0; i<TP2802_DEV_MAX; i++) {
        if(tp2802_dev[i].miscdev.name)
            misc_deregister(&tp2802_dev[i].miscdev);

        tp2802_proc_remove(i);
    }
    for(i=0; i<NVP1914_DEV_MAX; i++) {
        if(nvp1914_dev[i].miscdev.name)
            misc_deregister(&nvp1914_dev[i].miscdev);

        nvp1914_proc_remove(i);
    }
    
    hybrid_proc_remove();

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit tp2802_nvp1914_exit(void)
{
    int i;

    /* stop tp2802_nvp1914 watchdog thread */
    if(tp2802_nvp1914_wd)
        kthread_stop(tp2802_nvp1914_wd);

    tp2802_nvp1914_i2c_unregister();

    for(i=0; i<TP2802_DEV_MAX; i++) {
        /* remove device node */
        if(tp2802_dev[i].miscdev.name)
            misc_deregister(&tp2802_dev[i].miscdev);

        /* remove proc node */
        tp2802_proc_remove(i);
    }
    for(i=0; i<NVP1914_DEV_MAX; i++) {
        /* remove device node */
        if(nvp1914_dev[i].miscdev.name)
            misc_deregister(&nvp1914_dev[i].miscdev);

        /* remove proc node */
        nvp1914_proc_remove(i);
    }
    
    /* remove hybrid proc node */
    hybrid_proc_remove();

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    plat_audio_deregister_function();
}

module_init(tp2802_nvp1914_init);
module_exit(tp2802_nvp1914_exit);

MODULE_DESCRIPTION("Grain Media - TechPoint TP2802 4CH HDTVI & Nextchip NVP1914 4CH SD Hybrid Receiver Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
