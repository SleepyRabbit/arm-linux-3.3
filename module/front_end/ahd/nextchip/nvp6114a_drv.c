/**
 * @file nvp6114a_drv.c
 * NextChip NVP6114A 4CH AHD2.0 RX and 9CH Audio Codec Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.3 $
 * $Date: 2015/09/04 08:22:51 $
 *
 * ChangeLog:
 *  $Log: nvp6114a_drv.c,v $
 *  Revision 1.3  2015/09/04 08:22:51  shawn_hu
 *  Add "AFHD 1080P CIF" + "AHD 720P" hybrid 2-CH output mode support:
 *  VPORTA: 1080P30*2, VPORTB: 720P30*2  --> vmode=17
 *  VPORTA: 720P30*2,  VPORTB: 1080P30*2 --> vmode=18
 *  VPORTA: 1080P25*2, VPORTB: 720P25*2  --> vmode=19
 *  VPORTA: 720P25*2,  VPORTB: 1080P25*2 --> vmode=20
 *
 *  Revision 1.2  2015/08/27 07:52:26  shawn_hu
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
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "platform.h"
#include "nextchip/nvp6114a.h"                      ///< from module/include/front_end/ahd

#define NVP6114A_CLKIN                 27000000     ///< Master source clock of device
#define CH_IDENTIFY(id, vin, vport)    (((id)&0xf)|(((vin)&0xf)<<4)|(((vport)&0xf)<<8))
#define CH_IDENTIFY_ID(x)              ((x)&0xf)
#define CH_IDENTIFY_VIN(x)             (((x)>>4)&0xf)
#define CH_IDENTIFY_VPORT(x)           (((x)>>8)&0xf)

/* device number */
static int dev_num = 1;
module_param(dev_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dev_num, "Device Number: 1~4");

/* device i2c bus */
static int ibus = 0;
module_param(ibus, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ibus, "Device I2C Bus: 0~4");

/* device i2c address */
static ushort iaddr[NVP6114A_DEV_MAX] = {0x60, 0x62, 0x64, 0x66};
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
static int clk_freq = NVP6114A_CLKIN;   ///< 27MHz
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

static int vmode[NVP6114A_DEV_MAX] = {[0 ... (NVP6114A_DEV_MAX - 1)] = NVP6114A_VMODE_NTSC_720P_2CH};
module_param_array(vmode, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vmode, "Video Output Mode: 0:1CH_BYPASS, 1:NTSC_960H_2CH, 2:NTSC_960H_4CH, 3:NTSC_720P_2CH,\
4:PAL_960H_2CH, 5:PAL_960H_4CH, 6:PAL_720P_2CH, 7:NTSC_960H_720P_2CH, 8:NTSC_720P_960H_2CH, 9:PAL_960H_720P_2CH,\
10:PAL_720P_960H_2CH, 11:NTSC_CIF_AHD_2CH, 12:NTSC_CIF_AHD_4CH, 13:PAL_CIF_AHD_2CH, 14:PAL_CIF_AHD_4CH,\
15:NTSC_CIF_AFHD_2CH, 16:PAL_CIF_AFHD_2CH, 17:NTSC_CIF_AHD_720P_2CH, 18:NTSC_720P_CIF_AHD_2CH,\
19:PAL_CIF_AHD_720P_2CH, 20:PAL_720P_CIF_AHD_2CH");

/* device channel mapping */
static int ch_map = 0;
module_param(ch_map, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_map, "channel mapping");

/* notify */
static int notify = 1;
module_param(notify, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(notify, "Device Notify => 0:Disable 1:Enable");

/* audio sample rate */
static int sample_rate = AUDIO_SAMPLERATE_8K;
module_param(sample_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_rate, "audio sample rate: 0:8k  1:16k");

/* audio sample size */
static int sample_size = AUDIO_BITS_16B;
module_param(sample_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_size, "audio sample size: 0:16bit  1:8bit");

/* audio channel number */
int audio_chnum = 8;
module_param(audio_chnum, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(audio_chnum, "audio channel number");

struct nvp6114a_dev_t {
    int                 index;      ///< device index
    char                name[16];   ///< device name
    struct semaphore    lock;       ///< device locker
    struct miscdevice   miscdev;    ///< device node for ioctl access

    int                 pre_input_fmt[NVP6114A_DEV_CH_MAX];  ///< device channel previous input format
    int                 cur_input_fmt[NVP6114A_DEV_CH_MAX];  ///< device channel current  input format

    int                 (*vlos_notify)(int id, int vin, int vlos);
    int                 (*vfmt_notify)(int id, int ch, struct nvp6114a_video_fmt_t *vfmt);
};

static struct nvp6114a_dev_t  nvp6114a_dev[NVP6114A_DEV_MAX];
static struct proc_dir_entry *nvp6114a_proc_root[NVP6114A_DEV_MAX]      = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_vmode[NVP6114A_DEV_MAX]     = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_status[NVP6114A_DEV_MAX]    = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_video_fmt[NVP6114A_DEV_MAX] = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_bri[NVP6114A_DEV_MAX]       = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_con[NVP6114A_DEV_MAX]       = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_sat[NVP6114A_DEV_MAX]       = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_hue[NVP6114A_DEV_MAX]       = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_sharp[NVP6114A_DEV_MAX]     = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_volume[NVP6114A_DEV_MAX]    = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp6114a_proc_output_ch[NVP6114A_DEV_MAX] = {[0 ... (NVP6114A_DEV_MAX - 1)] = NULL};

static struct task_struct *nvp6114a_wd         = NULL;
static struct i2c_client  *nvp6114a_i2c_client = NULL;
static u32 NVP6114A_VCH_MAP[NVP6114A_DEV_MAX*NVP6114A_DEV_CH_MAX];
static u32 NVP6114A_ACH_MAP[NVP6114A_DEV_MAX*NVP6114A_DEV_AUDIO_CH_MAX];

static int nvp6114a_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    nvp6114a_i2c_client = client;
    return 0;
}

static int nvp6114a_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id nvp6114a_i2c_id[] = {
    { "nvp6114a_i2c", 0 },
    { }
};

static struct i2c_driver nvp6114a_i2c_driver = {
    .driver = {
        .name  = "NVP6114A_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = nvp6114a_i2c_probe,
    .remove   = nvp6114a_i2c_remove,
    .id_table = nvp6114a_i2c_id
};

static int nvp6114a_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&nvp6114a_i2c_driver);
    if(ret < 0) {
        printk("NVP6114A can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = iaddr[0]>>1;
    strlcpy(info.type, "nvp6114a_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("NVP6114A can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("NVP6114A can't add i2c device!\n");
        goto err_driver;
    }

    return 0;

err_driver:
   i2c_del_driver(&nvp6114a_i2c_driver);
   return -ENODEV;
}

static void nvp6114a_i2c_unregister(void)
{
    i2c_unregister_device(nvp6114a_i2c_client);
    i2c_del_driver(&nvp6114a_i2c_driver);
    nvp6114a_i2c_client = NULL;
}

/******************************************************************************
 GM8286 8CH EVB Channel and VIN Mapping Table (System Board)

 VCH#0 ------> VIN#0.0 ---------> VPORT#A -------> X_CAP#3
 VCH#1 ------> VIN#0.1 -----------^
 VCH#2 ------> VIN#0.2 ---------> VPORT#B -------> X_CAP#2
 VCH#3 ------> VIN#0.3 -----------^
 VCH#4 ------> VIN#1.0 ---------> VPORT#A -------> X_CAP#1
 VCH#5 ------> VIN#1.1 -----------^
 VCH#6 ------> VIN#1.2 ---------> VPORT#B -------> X_CAP#0
 VCH#7 ------> VIN#1.3 -----------^
==============================================================================
 GM8283 4CH EVB Channel and VIN Mapping Table (System Board)

 VCH#0 ------> VIN#0.0 ---------> VPORT#A -------> X_CAP#1
 VCH#1 ------> VIN#0.1 -----------^
 VCH#2 ------> VIN#0.2 ---------> VPORT#B -------> X_CAP#0
 VCH#3 ------> VIN#0.3 -----------^
==============================================================================
 GM8136 2CH EVB Channel and VIN Mapping Table (System Board)

 VCH#0 ------> VIN#0.0 ---------> VPORT#A -------> X_CAP#0
 VCH#1 ------> VIN#0.1 ---------> VPORT#B -------> X_CAP#1
 VCH#2 ------> VIN#0.2 ---------X
 VCH#3 ------> VIN#0.3 ---------X
*******************************************************************************/

static int nvp6114a_create_ch_map(int map_id)
{
    int i;

    switch(map_id) {
        case 3: ///< for Generic 1CH
            for(i=0; i<dev_num; i++) {
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_VPORT_MAX)+1] = CH_IDENTIFY(i, 0, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_VPORT_MAX)+0] = CH_IDENTIFY(i, 1, NVP6114A_DEV_VPORT_B);
                ///< This reverse order is required for NVP6114A register setting at 0xC2~0xC7 during 1-CH bypass mode
            }
            break;
        case 2: ///< for Generic 4CH on VPORT_B
            for(i=0; i<dev_num; i++) {
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, NVP6114A_DEV_VPORT_B);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, NVP6114A_DEV_VPORT_B);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, NVP6114A_DEV_VPORT_B);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, NVP6114A_DEV_VPORT_B);
            }
            break; 
        case 1: ///< for Generic 4CH on VPORT_A
            for(i=0; i<dev_num; i++) {
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, NVP6114A_DEV_VPORT_A);
            }
            break; 
        case 0: ///< for Generic 2CH
        default:
            for(i=0; i<dev_num; i++) {
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, NVP6114A_DEV_VPORT_A);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, NVP6114A_DEV_VPORT_B);
                NVP6114A_VCH_MAP[(i*NVP6114A_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, NVP6114A_DEV_VPORT_B);
            }
            break;
    }
	
	//Audio channel map
	for (i=0; i<dev_num; i++) {		
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+0] = CH_IDENTIFY(dev_num-i-1, 0, NVP6114A_DEV_VPORT_A);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+1] = CH_IDENTIFY(dev_num-i-1, 1, NVP6114A_DEV_VPORT_A);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+2] = CH_IDENTIFY(dev_num-i-1, 2, NVP6114A_DEV_VPORT_A);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+3] = CH_IDENTIFY(dev_num-i-1, 3, NVP6114A_DEV_VPORT_A);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+4] = CH_IDENTIFY(dev_num-i-1, 4, NVP6114A_DEV_VPORT_B);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+5] = CH_IDENTIFY(dev_num-i-1, 5, NVP6114A_DEV_VPORT_B);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+6] = CH_IDENTIFY(dev_num-i-1, 6, NVP6114A_DEV_VPORT_B);
		NVP6114A_ACH_MAP[(i*NVP6114A_DEV_AUDIO_CH_MAX)+7] = CH_IDENTIFY(dev_num-i-1, 7, NVP6114A_DEV_VPORT_B);
	}
    return 0;
}

int nvp6114a_vin_to_ch(int id, int vin_idx)
{
    int i;

    if(id >= NVP6114A_DEV_MAX || vin_idx >= NVP6114A_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP6114A_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[i]) == vin_idx))
            return (i%NVP6114A_DEV_CH_MAX);
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_vin_to_ch);

int nvp6114a_ain_to_ch(int id, int vin_idx)
{
    int i;

    if(id >= NVP6114A_DEV_MAX || vin_idx >= NVP6114A_DEV_AUDIO_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP6114A_DEV_AUDIO_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_ACH_MAP[i]) == id) && (CH_IDENTIFY_VIN(NVP6114A_ACH_MAP[i]) == vin_idx))
            return (i%NVP6114A_DEV_AUDIO_CH_MAX);
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_ain_to_ch);

int nvp6114a_ch_to_ain(int id, int ch_idx)
{
    int i;

    if(id >= NVP6114A_DEV_MAX || ch_idx >= NVP6114A_DEV_AUDIO_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP6114A_DEV_AUDIO_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_ACH_MAP[i]) == id) && ((i%NVP6114A_DEV_AUDIO_CH_MAX) == ch_idx))
            return CH_IDENTIFY_VIN(NVP6114A_ACH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_ch_to_ain);


int nvp6114a_ch_to_vin(int id, int ch_idx)
{
    int i;

    if(id >= NVP6114A_DEV_MAX || ch_idx >= NVP6114A_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP6114A_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[i]) == id) && ((i%NVP6114A_DEV_CH_MAX) == ch_idx))
            return CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_ch_to_vin);

int nvp6114a_get_vch_id(int id, NVP6114A_DEV_VPORT_T vport, int vport_seq)
{
    int i;
    int vport_chnum;

    if(id >= NVP6114A_DEV_MAX || vport >= NVP6114A_DEV_VPORT_MAX || vport_seq >= 4)
        return 0;

    /* get vport max channel number */
    switch(vmode[id]) {
        case NVP6114A_VMODE_1CH_BYPASS:
            vport_chnum = 1;
            break;
        case NVP6114A_VMODE_NTSC_960H_2CH:
        case NVP6114A_VMODE_NTSC_720P_2CH:
        case NVP6114A_VMODE_PAL_960H_2CH:
        case NVP6114A_VMODE_PAL_720P_2CH:
        case NVP6114A_VMODE_NTSC_960H_720P_2CH:
        case NVP6114A_VMODE_NTSC_720P_960H_2CH:
        case NVP6114A_VMODE_PAL_960H_720P_2CH:
        case NVP6114A_VMODE_PAL_720P_960H_2CH:
        case NVP6114A_VMODE_NTSC_CIF_AHD_2CH:
        case NVP6114A_VMODE_PAL_CIF_AHD_2CH:
        case NVP6114A_VMODE_NTSC_CIF_AFHD_2CH:
        case NVP6114A_VMODE_PAL_CIF_AFHD_2CH:
        case NVP6114A_VMODE_NTSC_CIF_AFHD_720P_2CH:
        case NVP6114A_VMODE_NTSC_720P_CIF_AFHD_2CH:
        case NVP6114A_VMODE_PAL_CIF_AFHD_720P_2CH:
        case NVP6114A_VMODE_PAL_720P_CIF_AFHD_2CH:
            vport_chnum = 2;
            break;
        case NVP6114A_VMODE_NTSC_960H_4CH:
        case NVP6114A_VMODE_PAL_960H_4CH:
        case NVP6114A_VMODE_NTSC_CIF_AHD_4CH:
        case NVP6114A_VMODE_PAL_CIF_AHD_4CH:
        default:
            vport_chnum = 4;
            break;
    }

    for(i=0; i<(dev_num*NVP6114A_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VPORT(NVP6114A_VCH_MAP[i]) == vport) &&
           ((CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[i])%vport_chnum) == vport_seq)) {
            return i;
        }
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_get_vch_id);

int nvp6114a_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= NVP6114A_DEV_MAX || vin_idx >= NVP6114A_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP6114A_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_vin_to_vch);

int nvp6114a_notify_vlos_register(int id, int (*nt_func)(int, int, int))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&nvp6114a_dev[id].lock);

    nvp6114a_dev[id].vlos_notify = nt_func;

    up(&nvp6114a_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(nvp6114a_notify_vlos_register);

void nvp6114a_notify_vlos_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&nvp6114a_dev[id].lock);

    nvp6114a_dev[id].vlos_notify = NULL;

    up(&nvp6114a_dev[id].lock);
}
EXPORT_SYMBOL(nvp6114a_notify_vlos_deregister);

int nvp6114a_notify_vfmt_register(int id, int (*nt_func)(int, int, struct nvp6114a_video_fmt_t *))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&nvp6114a_dev[id].lock);

    nvp6114a_dev[id].vfmt_notify = nt_func;

    up(&nvp6114a_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(nvp6114a_notify_vfmt_register);

void nvp6114a_notify_vfmt_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&nvp6114a_dev[id].lock);

    nvp6114a_dev[id].vfmt_notify = NULL;

    up(&nvp6114a_dev[id].lock);
}
EXPORT_SYMBOL(nvp6114a_notify_vfmt_deregister);

int nvp6114a_i2c_write(u8 id, u8 reg, u8 data)
{
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return -1;

    if(!nvp6114a_i2c_client) {
        printk("NVP6114A i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(nvp6114a_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("NVP6114A#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_i2c_write);

u8 nvp6114a_i2c_read(u8 id, u8 reg)
{
    u8 reg_data = 0;
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return 0;

    if(!nvp6114a_i2c_client) {
        printk("NVP6114A i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(nvp6114a_i2c_client->dev.parent);

    msgs[0].addr  = iaddr[id]>>1;
    msgs[0].flags = 0; /* write */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = (iaddr[id] + 1)>>1;
    msgs[1].flags = 1; /* read */
    msgs[1].len   = 1;
    msgs[1].buf   = &reg_data;

    if(i2c_transfer(adapter, msgs, 2) != 2)
        printk("NVP6114A#%d i2c read failed!!\n", id);

    return reg_data;
}
EXPORT_SYMBOL(nvp6114a_i2c_read);

int nvp6114a_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt)
{
#define NVP6114A_I2C_ARRAY_WRITE_MAX     256
    int i, num = 0;
    u8  buf[NVP6114A_I2C_ARRAY_WRITE_MAX+1];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num || !parray || array_cnt > NVP6114A_I2C_ARRAY_WRITE_MAX)
        return -1;

    if(!nvp6114a_i2c_client) {
        printk("NVP6114A i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(nvp6114a_i2c_client->dev.parent);

    buf[num++] = addr;
    for(i=0; i<array_cnt; i++) {
        buf[num++] = parray[i];
    }

    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = num;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("NVP6114A#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(nvp6114a_i2c_array_write);

int nvp6114a_get_device_num(void)
{
    return dev_num;
}
EXPORT_SYMBOL(nvp6114a_get_device_num);

static int nvp6114a_proc_vmode_show(struct seq_file *sfile, void *v)
{
    int i, cur_vmode;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;
    char *vmode_str[] = {"1CH_BYPASS",
                         "NTSC_960H_2CH", "NTSC_960H_4CH", "NTSC_720P_2CH",
                         "PAL_960H_2CH" , "PAL_960H_4CH",  "PAL_720P_2CH",
                         "NTSC_960H_720P_2CH", "NTSC_720P_960H_2CH",
                         "PAL_960H_720P_2CH",  "PAL_720P_960H_2CH",
                         "NTSC_CIF_AHD_2CH", "NTSC_CIF_AHD_4CH",
                         "PAL_CIF_AHD_2CH",  "PAL_CIF_AHD_4CH", 
                         "NTSC_CIF_AFHD_2CH", "PAL_CIF_AFHD_2CH",
                         "NTSC_CIF_AHD_720P_2CH", "NTSC_720P_CIF_AHD_2CH", 
                         "PAL_CIF_AHD_720P_2CH", "PAL_720P_CIF_AHD_2CH"
                         };

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    for(i=0; i<NVP6114A_VMODE_MAX; i++)
        seq_printf(sfile, "%02d: %s\n", i, vmode_str[i]);
    seq_printf(sfile, "----------------------------------\n");

    cur_vmode = vmode[pdev->index];
    seq_printf(sfile, "Current==> %s\n\n",(cur_vmode >= 0 && cur_vmode < NVP6114A_VMODE_MAX) ? vmode_str[cur_vmode] : "Unknown");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_status_show(struct seq_file *sfile, void *v)
{
    int i, j, novid, vfmt;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;
    char *vfmt_str[] = {"Unknown", "SD_PAL", "SD_NTSC", "AHD_25P", "AHD_30P", "AHD_50P", "AHD_60P", "AFHD_25P", "AFHD_30P"};

    down(&pdev->lock);

    novid = nvp6114a_status_get_novid(pdev->index);
    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "----------------------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    NOVID        INPUT_VFMT \n");
    seq_printf(sfile, "----------------------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                vfmt = nvp6114a_status_get_video_input_format(pdev->index, i);
                seq_printf(sfile, "%-7d %-7d %-12s %s\n", i,
                           nvp6114a_vin_to_vch(pdev->index, i),
                           (novid & (0x1<<i)) ? "Video_Loss" : "Video_On",
                           vfmt_str[vfmt]);
            }
        }
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_video_fmt_show(struct seq_file *sfile, void *v)
{
    int i, j, ret;
    struct nvp6114a_video_fmt_t vfmt;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[NVP6114A#%d] Incoming Video Fromat\n", pdev->index);
    seq_printf(sfile, "====================================================\n");
    seq_printf(sfile, "VIN   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "====================================================\n");

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                ret = nvp6114a_video_get_output_format(pdev->index, i, &vfmt, vmode[pdev->index]);
                if((ret == 0) && ((vfmt.fmt_idx >= 0) && (vfmt.fmt_idx < NVP6114A_VFMT_MAX))) {
                    seq_printf(sfile, "%-5d %-5d %-7d %-8d %-12s %-11d\n",
                               i,
                               j,
                               vfmt.width,
                               vfmt.height,
                               ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                               vfmt.frame_rate);
                }
                else {
                    seq_printf(sfile, "%-5d %-5d %-7s %-8s %-12s %-11s\n", i, j, "-", "-", "-", "-");
                }
            }
        }
    }

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_bri_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    BRIGHTNESS\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, nvp6114a_vin_to_vch(pdev->index, i), nvp6114a_video_get_brightness(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nBrightness[-128 ~ +127] ==> 0x01=+1, 0x7f=+127, 0x80=-128, 0xff=-1\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_con_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    CONTRAST  \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, nvp6114a_vin_to_vch(pdev->index, i), nvp6114a_video_get_contrast(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nContrast[0x00 ~ 0xff] ==> 0x00=x0, 0x40=x0.5, 0x80=x1, 0xff=x2\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_sat_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    SATURATION\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, nvp6114a_vin_to_vch(pdev->index, i), nvp6114a_video_get_saturation(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nSaturation[0x00 ~ 0xff] ==> 0x00=x0, 0x80=x1, 0xc0=x1.5, 0xff=x2\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_hue_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    HUE       \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, nvp6114a_vin_to_vch(pdev->index, i), nvp6114a_video_get_hue(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nHue[0x00 ~ 0xff] ==> 0x00=0, 0x40=90, 0x80=180, 0xff=360\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_sharp_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    SHARPNESS \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*((vmode[pdev->index]==NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)); j++) {
            if((CH_IDENTIFY_ID(NVP6114A_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(NVP6114A_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, nvp6114a_vin_to_vch(pdev->index, i), nvp6114a_video_get_sharpness(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nH_Sharpness[0x0 ~ 0xf] - Bit[7:4] ==> 0x0:x0, 0x4:x0.5, 0x8:x1, 0xf:x2");
    seq_printf(sfile, "\nV_Sharpness[0x0 ~ 0xf] - Bit[3:0] ==> 0x0:x1, 0x4:x2,   0x8:x3, 0xf:x4\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_volume_show(struct seq_file *sfile, void *v)
{
    int aogain;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);
    aogain = nvp6114a_audio_get_mute(pdev->index);
    seq_printf(sfile, "Volume[0x0~0xf] = %d\n", aogain);

    up(&pdev->lock);

    return 0;
}

static int nvp6114a_proc_output_ch_show(struct seq_file *sfile, void *v)
{
    int ch, vin_idx;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;
    static char *output_ch_str[] = {"CH 1", "CH 2", "CH 3", "CH 4", "CH 5", "CH 6",
                                    "CH 7", "CH 8", "CH 9", "CH 10", "CH 11", "CH 12",
                                    "CH 13", "CH 14", "CH 15", "CH 16", "FIRST PLAYBACK AUDIO",
                                    "SECOND PLAYBACK AUDIO", "THIRD PLAYBACK AUDIO",
                                    "FOURTH PLAYBACK AUDIO", "MIC input 1", "MIC input 2",
                                    "MIC input 3", "MIC input 4", "Mixed audio", "no audio output"};

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP6114A#%d]\n", pdev->index);

    vin_idx = nvp6114a_audio_get_output_ch(pdev->index);

    if (vin_idx >= 0 && vin_idx <= 7)
        ch = nvp6114a_ain_to_ch(pdev->index, vin_idx);
    else if (vin_idx >= 8 && vin_idx <= 15) {
        vin_idx -= 8;
        ch = nvp6114a_ain_to_ch(pdev->index, vin_idx);
        ch += 8;
    }
    else
        ch = vin_idx;

    seq_printf(sfile, "Current[0x0~0x18]==> %s\n\n", output_ch_str[ch]);

    up(&pdev->lock);

    return 0;
}

static ssize_t nvp6114a_proc_bri_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  i, vin;
    u32  bri;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &bri);

    down(&pdev->lock);

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP6114A_DEV_CH_MAX) {
            nvp6114a_video_set_brightness(pdev->index, i, (u8)bri);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_con_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 con;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &con);

    down(&pdev->lock);

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP6114A_DEV_CH_MAX) {
            nvp6114a_video_set_contrast(pdev->index, i, (u8)con);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_sat_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 sat;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &sat);

    down(&pdev->lock);

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP6114A_DEV_CH_MAX) {
            nvp6114a_video_set_saturation(pdev->index, i, (u8)sat);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_hue_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 hue;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &hue);

    down(&pdev->lock);

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP6114A_DEV_CH_MAX) {
            nvp6114a_video_set_hue(pdev->index, i, (u8)hue);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_sharp_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 sharp;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &sharp);

    down(&pdev->lock);

    for(i=0; i<NVP6114A_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP6114A_DEV_CH_MAX) {
            nvp6114a_video_set_sharpness(pdev->index, i, (u8)sharp);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_volume_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 volume;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &volume);

    down(&pdev->lock);

    nvp6114a_audio_set_volume(pdev->index, volume);

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_output_ch_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 ch, vin_idx, temp;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &ch);

    temp = ch;
    if (ch >= 16)
        vin_idx = ch;

    if (ch < 16) {
        if (ch >= 8)
            ch -= 8;
        vin_idx = nvp6114a_ch_to_ain(pdev->index, ch);
        if (temp >= 8)
            vin_idx += 8;
    }

    down(&pdev->lock);

    nvp6114a_audio_set_output_ch(pdev->index, vin_idx);

    up(&pdev->lock);

    return count;
}

static ssize_t nvp6114a_proc_vmode_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int cur_vmode;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &cur_vmode);

    down(&pdev->lock);

    if(cur_vmode != vmode[pdev->index]) {
        if((cur_vmode < NVP6114A_VMODE_1CH_BYPASS) || (cur_vmode >= NVP6114A_VMODE_MAX)) {
            printk("Unknown vmode!\n");  ///< check if user input is legal
            goto exit;
        }
        switch(vmode[pdev->index]) {
            case NVP6114A_VMODE_1CH_BYPASS:
                printk("Can't dynamically change vmode from 1CH to 2CH/4CH mode!\n");
                goto exit;
                break;
            case NVP6114A_VMODE_NTSC_960H_2CH:
            case NVP6114A_VMODE_NTSC_720P_2CH:
            case NVP6114A_VMODE_PAL_960H_2CH:
            case NVP6114A_VMODE_PAL_720P_2CH:
            case NVP6114A_VMODE_NTSC_960H_720P_2CH:
            case NVP6114A_VMODE_NTSC_720P_960H_2CH:
            case NVP6114A_VMODE_PAL_960H_720P_2CH:
            case NVP6114A_VMODE_PAL_720P_960H_2CH:
            case NVP6114A_VMODE_NTSC_CIF_AHD_2CH:
            case NVP6114A_VMODE_PAL_CIF_AHD_2CH:
            case NVP6114A_VMODE_NTSC_CIF_AFHD_2CH:
            case NVP6114A_VMODE_PAL_CIF_AFHD_2CH:
            case NVP6114A_VMODE_NTSC_CIF_AFHD_720P_2CH:
            case NVP6114A_VMODE_NTSC_720P_CIF_AFHD_2CH:
            case NVP6114A_VMODE_PAL_CIF_AFHD_720P_2CH:
            case NVP6114A_VMODE_PAL_720P_CIF_AFHD_2CH:
                if((cur_vmode == NVP6114A_VMODE_1CH_BYPASS) || (cur_vmode == NVP6114A_VMODE_NTSC_960H_4CH) 
                   || (cur_vmode == NVP6114A_VMODE_PAL_960H_4CH) || (cur_vmode == NVP6114A_VMODE_NTSC_CIF_AHD_4CH) 
                   || (cur_vmode == NVP6114A_VMODE_PAL_CIF_AHD_4CH)) {
                    printk("Can't dynamically change vmode from 2CH to 1CH/4CH mode!\n");
                    goto exit;
                }
                else
                    ; ///< exit switch case & re-init NVP6114A for current vmode
                break;
            case NVP6114A_VMODE_NTSC_960H_4CH:
            case NVP6114A_VMODE_PAL_960H_4CH:
            case NVP6114A_VMODE_NTSC_CIF_AHD_4CH:
            case NVP6114A_VMODE_PAL_CIF_AHD_4CH:
                if((cur_vmode == NVP6114A_VMODE_NTSC_960H_4CH) || (cur_vmode == NVP6114A_VMODE_PAL_960H_4CH)
                   || (cur_vmode == NVP6114A_VMODE_NTSC_CIF_AHD_4CH) || (cur_vmode == NVP6114A_VMODE_PAL_CIF_AHD_4CH))
                    ; ///< exit switch case & re-init NVP6114A for current vmode
                else {
                    printk("Can't dynamically change vmode from 4CH to 1CH/2CH mode!\n");
                    goto exit;
                }
                break;
        }
        nvp6114a_video_init(pdev->index, cur_vmode);
        vmode[pdev->index] = cur_vmode;  ///< update vmode for watchdog thread
    }
exit:
    up(&pdev->lock);

    return count;
}

static int nvp6114a_proc_vmode_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_vmode_show, PDE(inode)->data);
}

static int nvp6114a_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_status_show, PDE(inode)->data);
}

static int nvp6114a_proc_video_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_video_fmt_show, PDE(inode)->data);
}

static int nvp6114a_proc_bri_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_bri_show, PDE(inode)->data);
}

static int nvp6114a_proc_con_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_con_show, PDE(inode)->data);
}

static int nvp6114a_proc_sat_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_sat_show, PDE(inode)->data);
}

static int nvp6114a_proc_hue_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_hue_show, PDE(inode)->data);
}

static int nvp6114a_proc_sharp_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_sharp_show, PDE(inode)->data);
}

static int nvp6114a_proc_volume_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_volume_show, PDE(inode)->data);
}

static int nvp6114a_proc_output_ch_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp6114a_proc_output_ch_show, PDE(inode)->data);
}

static struct file_operations nvp6114a_proc_vmode_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_vmode_open,
    .write  = nvp6114a_proc_vmode_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_video_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_video_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_bri_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_bri_open,
    .write  = nvp6114a_proc_bri_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_con_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_con_open,
    .write  = nvp6114a_proc_con_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_sat_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_sat_open,
    .write  = nvp6114a_proc_sat_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_hue_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_hue_open,
    .write  = nvp6114a_proc_hue_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_sharp_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_sharp_open,
    .write  = nvp6114a_proc_sharp_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_volume_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_volume_open,
    .write  = nvp6114a_proc_volume_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp6114a_proc_output_ch_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp6114a_proc_output_ch_open,
    .write  = nvp6114a_proc_output_ch_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void nvp6114a_proc_remove(int id)
{
    if(id >= NVP6114A_DEV_MAX)
        return;

    if(nvp6114a_proc_root[id]) {
        if(nvp6114a_proc_vmode[id]) {
            remove_proc_entry(nvp6114a_proc_vmode[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_vmode[id] = NULL;
        }

        if(nvp6114a_proc_status[id]) {
            remove_proc_entry(nvp6114a_proc_status[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_status[id] = NULL;
        }

        if(nvp6114a_proc_video_fmt[id]) {
            remove_proc_entry(nvp6114a_proc_video_fmt[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_video_fmt[id] = NULL;
        }

        if(nvp6114a_proc_bri[id]) {
            remove_proc_entry(nvp6114a_proc_bri[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_bri[id] = NULL;
        }

        if(nvp6114a_proc_con[id]) {
            remove_proc_entry(nvp6114a_proc_con[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_con[id] = NULL;
        }

        if(nvp6114a_proc_sat[id]) {
            remove_proc_entry(nvp6114a_proc_sat[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_sat[id] = NULL;
        }

        if(nvp6114a_proc_hue[id]) {
            remove_proc_entry(nvp6114a_proc_hue[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_hue[id] = NULL;
        }

        if(nvp6114a_proc_sharp[id]) {
            remove_proc_entry(nvp6114a_proc_sharp[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_sharp[id] = NULL;
        }

        if(nvp6114a_proc_volume[id]) {
            remove_proc_entry(nvp6114a_proc_volume[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_volume[id] = NULL;
        }

        if(nvp6114a_proc_output_ch[id]) {
            remove_proc_entry(nvp6114a_proc_output_ch[id]->name, nvp6114a_proc_root[id]);
            nvp6114a_proc_output_ch[id] = NULL;
        }

        remove_proc_entry(nvp6114a_proc_root[id]->name, NULL);
        nvp6114a_proc_root[id] = NULL;
    }
}

static int nvp6114a_proc_init(int id)
{
    int ret = 0;

    if(id >= NVP6114A_DEV_MAX)
        return -1;

    /* root */
    nvp6114a_proc_root[id] = create_proc_entry(nvp6114a_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!nvp6114a_proc_root[id]) {
        printk("create proc node '%s' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_root[id]->owner = THIS_MODULE;
#endif

    /* vmode */
    nvp6114a_proc_vmode[id] = create_proc_entry("vmode", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_vmode[id]) {
        printk("create proc node '%s/vmode' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_vmode[id]->proc_fops = &nvp6114a_proc_vmode_ops;
    nvp6114a_proc_vmode[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_vmode[id]->owner     = THIS_MODULE;
#endif

    /* status */
    nvp6114a_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_status[id]->proc_fops = &nvp6114a_proc_status_ops;
    nvp6114a_proc_status[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* frame format */
    nvp6114a_proc_video_fmt[id] = create_proc_entry("video_format", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_video_fmt[id]) {
        printk("create proc node '%s/video_format' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_video_fmt[id]->proc_fops = &nvp6114a_proc_video_fmt_ops;
    nvp6114a_proc_video_fmt[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_video_fmt[id]->owner     = THIS_MODULE;
#endif

    /* brightness */
    nvp6114a_proc_bri[id] = create_proc_entry("brightness", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_bri[id]) {
        printk("create proc node '%s/brightness' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_bri[id]->proc_fops = &nvp6114a_proc_bri_ops;
    nvp6114a_proc_bri[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_bri[id]->owner     = THIS_MODULE;
#endif

    /* contrast */
    nvp6114a_proc_con[id] = create_proc_entry("contrast", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_con[id]) {
        printk("create proc node '%s/contrast' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_con[id]->proc_fops = &nvp6114a_proc_con_ops;
    nvp6114a_proc_con[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_con[id]->owner     = THIS_MODULE;
#endif

    /* saturation */
    nvp6114a_proc_sat[id] = create_proc_entry("saturation", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_sat[id]) {
        printk("create proc node '%s/saturation' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_sat[id]->proc_fops = &nvp6114a_proc_sat_ops;
    nvp6114a_proc_sat[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_sat[id]->owner     = THIS_MODULE;
#endif

    /* hue */
    nvp6114a_proc_hue[id] = create_proc_entry("hue", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_hue[id]) {
        printk("create proc node '%s/hue' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_hue[id]->proc_fops = &nvp6114a_proc_hue_ops;
    nvp6114a_proc_hue[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_hue[id]->owner     = THIS_MODULE;
#endif

    /* sharpness */
    nvp6114a_proc_sharp[id] = create_proc_entry("sharpness", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_sharp[id]) {
        printk("create proc node '%s/sharpness' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_sharp[id]->proc_fops = &nvp6114a_proc_sharp_ops;
    nvp6114a_proc_sharp[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_sharp[id]->owner     = THIS_MODULE;
#endif

    /* volume */
    nvp6114a_proc_volume[id] = create_proc_entry("volume", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_volume[id]) {
        printk("create proc node '%s/volume' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_volume[id]->proc_fops = &nvp6114a_proc_volume_ops;
    nvp6114a_proc_volume[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_volume[id]->owner     = THIS_MODULE;
#endif

    /* output channel */
    nvp6114a_proc_output_ch[id] = create_proc_entry("output_ch", S_IRUGO|S_IXUGO, nvp6114a_proc_root[id]);
    if(!nvp6114a_proc_output_ch[id]) {
        printk("create proc node '%s/output_ch' failed!\n", nvp6114a_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp6114a_proc_output_ch[id]->proc_fops = &nvp6114a_proc_output_ch_ops;
    nvp6114a_proc_output_ch[id]->data      = (void *)&nvp6114a_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp6114a_proc_output_ch[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    nvp6114a_proc_remove(id);
    return ret;
}

static int nvp6114a_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct nvp6114a_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(nvp6114a_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &nvp6114a_dev[i];
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

static int nvp6114a_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long nvp6114a_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int tmp, vin_idx, ch, ret = 0;
    struct nvp6114a_ioc_data_t ioc_data;
    struct nvp6114a_dev_t *pdev = (struct nvp6114a_dev_t *)file->private_data;

    down(&pdev->lock);

    if(_IOC_TYPE(cmd) != NVP6114A_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case NVP6114A_GET_NOVID:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            if(ioc_data.ch >= NVP6114A_DEV_CH_MAX) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_status_get_novid(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = (tmp>>ioc_data.ch) & 0x1;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_GET_CONTRAST:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_video_get_contrast(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_SET_CONTRAST:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp6114a_video_set_contrast(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP6114A_GET_BRIGHTNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_video_get_brightness(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_SET_BRIGHTNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp6114a_video_set_brightness(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP6114A_GET_SATURATION:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_video_get_saturation(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_SET_SATURATION:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp6114a_video_set_saturation(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP6114A_GET_HUE:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_video_get_hue(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_SET_HUE:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp6114a_video_set_hue(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP6114A_GET_SHARPNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_video_get_sharpness(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP6114A_SET_SHARPNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp6114a_video_set_sharpness(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP6114A_GET_INPUT_VFMT:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            if(ioc_data.ch >= NVP6114A_DEV_CH_MAX) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp6114a_status_get_video_input_format(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;
			
		case NVP6114A_GET_VOL:
			tmp = nvp6114a_audio_get_mute(pdev->index);
            if(tmp < 0) {                
				ret = -EFAULT;                
				goto exit;            
			}
			
            ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
			break;
			
		case NVP6114A_SET_VOL:
			if(copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
				ret = -EFAULT;
				goto exit;
			}
			
			ret = nvp6114a_audio_set_volume(pdev->index, tmp);
			if(tmp < 0) {
			    ret = -EFAULT;
				goto exit;
			}
			break;
				
		case NVP6114A_GET_OUT_CH:
			tmp = nvp6114a_audio_get_output_ch(pdev->index);
			if(tmp < 0) {
				ret = -EFAULT;
				goto exit;
			}
			vin_idx = tmp;
			if (vin_idx >= 8 && vin_idx <= 15) {
				vin_idx -= 8;
				ch = nvp6114a_ain_to_ch(pdev->index, vin_idx);
				ch += 8;
			}
			else if (vin_idx >= 0 && vin_idx <= 7)
				ch = nvp6114a_ain_to_ch(pdev->index, vin_idx);
			else
				ch = vin_idx;
		
			tmp = ch;
		
			ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
			break;
		
		case NVP6114A_SET_OUT_CH:
			if(copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
				ret = -EFAULT;
				goto exit;
			}
			ch = tmp;
			if (tmp >= 16)
				vin_idx = tmp;
		
			if (tmp < 16) {
				if (tmp >= 8)
					tmp -= 8;
				vin_idx = nvp6114a_ch_to_ain(pdev->index, tmp);
				if (ch >= 8)
					vin_idx += 8;
			}
			tmp = vin_idx;
		
			ret = nvp6114a_audio_set_output_ch(pdev->index, tmp);
			if(tmp < 0) {
				ret = -EFAULT;
				goto exit;
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

static struct file_operations nvp6114a_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = nvp6114a_miscdev_open,
    .release        = nvp6114a_miscdev_release,
    .unlocked_ioctl = nvp6114a_miscdev_ioctl,
};

static int nvp6114a_miscdev_init(int id)
{
    int ret;

    if(id >= NVP6114A_DEV_MAX)
        return -1;

    /* clear */
    memset(&nvp6114a_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    nvp6114a_dev[id].miscdev.name  = nvp6114a_dev[id].name;
    nvp6114a_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    nvp6114a_dev[id].miscdev.fops  = (struct file_operations *)&nvp6114a_miscdev_fops;
    ret = misc_register(&nvp6114a_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", nvp6114a_dev[id].name);
        nvp6114a_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

static void nvp6114a_hw_reset(void)
{
    if(!rstb_used || !init)
        return;

    /* Active Low */
    plat_gpio_set_value(rstb_used, 0);

    udelay(10);

    /* Active High and release reset */
    plat_gpio_set_value(rstb_used, 1);
}

static int nvp6114a_device_init(int id)
{
    int ret;

    if (!init)
        return 0;

    if(id >= NVP6114A_DEV_MAX)
        return -1;

    /*====================== video init ========================= */
    ret = nvp6114a_video_init(id, vmode[id]);
    if(ret < 0)
        goto exit;

    /*====================== audio init ========================= */
    ret = nvp6114a_audio_set_mode(id, vmode[(id>>1)<<1], sample_size, sample_rate);
    if(ret < 0)
        goto exit;

exit:
    return ret;
}

static int nvp6114a_watchdog_thread(void *data)
{
    int i, ch, ret, novid;
    int debounce[NVP6114A_DEV_MAX*NVP6114A_DEV_CH_MAX] = {[0 ... (NVP6114A_DEV_MAX*NVP6114A_DEV_CH_MAX - 1)] = 0}; ///< debounce design to fix the wrong format detection if camera was plug on&off quickly
    int pre_vmode[dev_num];
    struct nvp6114a_dev_t *pdev;
    struct nvp6114a_video_fmt_t vout_fmt;

    for(i=0; i<dev_num; i++) {
        pre_vmode[i] = vmode[i]; ///< keep record of previous vmode for vmode change
    }

    do {
        /* check nvp6114a channel status */
        for(i=0; i<dev_num; i++) {
            pdev = &nvp6114a_dev[i];

            down(&pdev->lock);

            novid = nvp6114a_status_get_novid(i);

            for(ch=0; ch<((vmode[i] == NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX); ch++) {

                if((vmode[i] == NVP6114A_VMODE_1CH_BYPASS) || (pre_vmode[i] != vmode[i])) { ///< dynamically update EQ & output format only during 1-CH bypass mode or vmode changed
                    pdev->cur_input_fmt[ch] = nvp6114a_status_get_video_input_format(i, ch);
                    pre_vmode[i] = vmode[i];
                }

                if((pdev->cur_input_fmt[ch] != pdev->pre_input_fmt[ch]) && (pdev->cur_input_fmt[ch] != NVP6114A_VFMT_UNKNOWN)) {   ///< video input format was changed
                    if(debounce[i*((vmode[i] == NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)+ch] < 2) 
                        debounce[i*((vmode[i] == NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)+ch]++;
                    else {
                        /* Set equalizer for current input format */
                        ret = nvp6114a_video_set_equalizer(i, ch, pdev->cur_input_fmt[ch]);
                        if(ret < 0)
                            continue;

                        /* sleep 0.1 second for EQ ready */
                        schedule_timeout_interruptible(msecs_to_jiffies(100));

                        /* switch video port output format */
                        ret = nvp6114a_video_set_output_format(i, ch, pdev->cur_input_fmt[ch], vmode[i]);
                        if(ret < 0)
                            continue;

                        pdev->pre_input_fmt[ch] = pdev->cur_input_fmt[ch];
                    }
                }
                else {  ///< video input format didn't changed
                    /* notify current video present/loss status */
                    if(notify && pdev->vlos_notify) {
                        if(novid & (0x1<<ch)) {
                            pdev->vlos_notify(i, ch, 1);
                            /* Fix the abnormal freerun output on 720H NTSC */
                            ret = nvp6114a_i2c_write(i, 0xFF, 0x05+ch);
                            ret = nvp6114a_i2c_write(i, 0x2C, 0x04);
                        }
                        else
                            pdev->vlos_notify(i, ch, 0);
                        debounce[i*((vmode[i] == NVP6114A_VMODE_1CH_BYPASS)?NVP6114A_DEV_VPORT_MAX:NVP6114A_DEV_CH_MAX)+ch] = 0;
                    }

                    /* notify current video format */
                    if(notify && pdev->vfmt_notify) {
                        /* get video port output format */
                        ret = nvp6114a_video_get_output_format(i, ch, &vout_fmt, vmode[i]);
                        if(ret < 0)
                            continue;
                        else {
                            switch(vmode[i]){
                                case NVP6114A_VMODE_1CH_BYPASS:
                                    pdev->vfmt_notify(i, ch, &vout_fmt);
                                    break;
                                case NVP6114A_VMODE_NTSC_960H_2CH:
                                case NVP6114A_VMODE_NTSC_720P_2CH:
                                case NVP6114A_VMODE_PAL_960H_2CH:
                                case NVP6114A_VMODE_PAL_720P_2CH:
                                case NVP6114A_VMODE_NTSC_960H_720P_2CH:
                                case NVP6114A_VMODE_NTSC_720P_960H_2CH:
                                case NVP6114A_VMODE_PAL_960H_720P_2CH:
                                case NVP6114A_VMODE_PAL_720P_960H_2CH:
                                case NVP6114A_VMODE_NTSC_CIF_AHD_2CH:
                                case NVP6114A_VMODE_PAL_CIF_AHD_2CH:
                                case NVP6114A_VMODE_NTSC_CIF_AFHD_2CH:
                                case NVP6114A_VMODE_PAL_CIF_AFHD_2CH:
                                default:
                                    pdev->vfmt_notify(i, ch/2, &vout_fmt);
                                    break;
                                case NVP6114A_VMODE_NTSC_960H_4CH:
                                case NVP6114A_VMODE_PAL_960H_4CH:
                                case NVP6114A_VMODE_NTSC_CIF_AHD_4CH:
                                case NVP6114A_VMODE_PAL_CIF_AHD_4CH:
                                    pdev->vfmt_notify(i, ch/4, &vout_fmt);
                                    break;
                            }
                        }
                    }
                }
            }

            up(&pdev->lock);

        }

        /* sleep 0.5 second */
        schedule_timeout_interruptible(msecs_to_jiffies(500));

    } while (!kthread_should_stop());

    return 0;
}

static int __init nvp6114a_init(void)
{
    int i, ret;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(dev_num > NVP6114A_DEV_MAX) {
        printk("NVP6114A dev_num=%d invalid!!(Max=%d)\n", dev_num, NVP6114A_DEV_MAX);
        return -1;
    }

    /* register i2c client for contol nvp6114a */
    ret = nvp6114a_i2c_register();
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
    ret = nvp6114a_create_ch_map(ch_map);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    nvp6114a_hw_reset();

    /* NVP6114A init */
    for(i=0; i<dev_num; i++) {
        nvp6114a_dev[i].index = i;

        sprintf(nvp6114a_dev[i].name, "nvp6114a.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&nvp6114a_dev[i].lock, 1);
#else
        init_MUTEX(&nvp6114a_dev[i].lock);
#endif
        ret = nvp6114a_proc_init(i);
        if(ret < 0)
            goto err;

        ret = nvp6114a_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = nvp6114a_device_init(i);
        if(ret < 0)
            goto err;
    }

    if(init && notify) {
        /* init nvp6114a watchdog thread for monitor video status */
        nvp6114a_wd = kthread_create(nvp6114a_watchdog_thread, NULL, "nvp6114a_wd");
        if(!IS_ERR(nvp6114a_wd))
            wake_up_process(nvp6114a_wd);
        else {
            printk("create nvp6114a watchdog thread failed!!\n");
            nvp6114a_wd = 0;
            ret = -EFAULT;
            goto err;
        }
    }

    return 0;

err:
    if(nvp6114a_wd)
        kthread_stop(nvp6114a_wd);

    nvp6114a_i2c_unregister();
    for(i=0; i<NVP6114A_DEV_MAX; i++) {
        if(nvp6114a_dev[i].miscdev.name)
            misc_deregister(&nvp6114a_dev[i].miscdev);

        nvp6114a_proc_remove(i);
    }

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit nvp6114a_exit(void)
{
    int i;

    /* stop nvp6114a watchdog thread */
    if(nvp6114a_wd)
        kthread_stop(nvp6114a_wd);

    nvp6114a_i2c_unregister();
    for(i=0; i<NVP6114A_DEV_MAX; i++) {
        /* remove device node */
        if(nvp6114a_dev[i].miscdev.name)
            misc_deregister(&nvp6114a_dev[i].miscdev);

        /* remove proc node */
        nvp6114a_proc_remove(i);
    }

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);
}

module_init(nvp6114a_init);
module_exit(nvp6114a_exit);

MODULE_DESCRIPTION("Grain Media NVP6114A Video Decoders and Audio Codecs Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
