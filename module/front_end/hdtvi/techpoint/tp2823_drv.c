/**
 * @file tp2823_drv.c
 * TechPoint TP2823 4CH HD-TVI Video Decoder Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.8 $
 * $Date: 2015/10/02 05:13:32 $
 *
 * ChangeLog:
 *  $Log: tp2823_drv.c,v $
 *  Revision 1.8  2015/10/02 05:13:32  shawn_hu
 *  Fix a bug which might come across the below symptom:
 *  1. When TP2823 runs in 2-CH dual edge output, and a NTSC 960H & a PAL 960H video inputs arrive at the same BT.656 output port.
 *     If the PAL 960H input is connected first and then the NTSC 960H after, the video output of NTSC 960H will be flickering.
 *  2. Wrong setting on the register@0xBB, it will lead to abnormal rainbow color on TVI V1 1080P25 video output.
 *
 *  Revision 1.7  2015/09/17 01:53:47  shawn_hu
 *  Change the black image output during video loss to 30FPS in 2-CH dual edge mode, e.g. 1920*480@30FPS or 640*720@30FPS.
 *  This is because 25FPS setting will cause the next format detection to be abnormal.
 *
 *  Revision 1.6  2015/08/27 03:16:55  arvin_hsu
 *  1. add audio volume ioct.
 *  2. Update TP2823 audio driver to v35.
 *
 *  Revision 1.5  2015/08/19 02:10:35  shawn_hu
 *  1. Also add a debounce design to video loss/present (VDLOSS) detection.
 *  2. Update Techpoint TP2823 driver to v35, it prevents from the "video on" false alarm when no camera is connected.
 *
 *  Revision 1.4  2015/07/09 03:51:06  arvin_hsu
 *  Add Techpoint TP2823 Audio driver support
 *
 *  Revision 1.3  2015/07/03 02:55:29  shawn_hu
 *  1. Add simulated annealing like algorithm on debounce design for better video format detection.
 *  2. Fix the wrong loop index setting on proc node show function.
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
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "platform.h"
#include "techpoint/tp2823.h"                       ///< from module/include/front_end/hdtvi

#define TP2823_CLKIN                   27000000     ///< Master source clock of device
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
static ushort iaddr[TP2823_DEV_MAX] = {0x88, 0x8a, 0x8c, 0x8e};
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
static int clk_freq = TP2823_CLKIN;   ///< 27MHz
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
static int vout_format[TP2823_DEV_MAX] = {[0 ... (TP2823_DEV_MAX - 1)] = TP2823_VOUT_FORMAT_BT656};
module_param_array(vout_format, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vout_format, "Video Output Format: 0:BT656, 1:BT1120");

/* device video port output mode */
static int vout_mode[TP2823_DEV_MAX] = {[0 ... (TP2823_DEV_MAX - 1)] = TP2823_VOUT_MODE_1CH_BYPASS};
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

/* audio sample rate */
static int sample_rate = AUDIO_SAMPLERATE_8K;
module_param(sample_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_rate, "audio sample rate: 0:8k  1:16k");

/* audio sample size */
static int sample_size = AUDIO_BITS_16B;
module_param(sample_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sample_size, "audio sample size: 0:16bit  1:8bit");

/* audio channel number */
int audio_chnum = 4;
module_param(audio_chnum, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(audio_chnum, "audio channel number");

struct tp2823_dev_t {
    int               index;                          ///< device index
    char              name[16];                       ///< device name
    struct semaphore  lock;                           ///< device locker
    struct miscdevice miscdev;                        ///< device node for ioctl access

    int               pre_plugin[TP2823_DEV_CH_MAX];  ///< device channel previous cable plugin status
    int               cur_plugin[TP2823_DEV_CH_MAX];  ///< device channel current  cable plugin status

    int               (*vfmt_notify)(int id, int ch, struct tp2823_video_fmt_t *vfmt);
    int               (*vlos_notify)(int id, int ch, int vlos);
};

static struct tp2823_dev_t    tp2823_dev[TP2823_DEV_MAX];
static struct proc_dir_entry *tp2823_proc_root[TP2823_DEV_MAX]      = {[0 ... (TP2823_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_status[TP2823_DEV_MAX]    = {[0 ... (TP2823_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_video_fmt[TP2823_DEV_MAX] = {[0 ... (TP2823_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_vout_fmt[TP2823_DEV_MAX]  = {[0 ... (TP2823_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *tp2823_proc_volume[TP2823_DEV_MAX]   = {[0 ... (TP2823_DEV_MAX - 1)] = NULL};

static struct i2c_client  *tp2823_i2c_client = NULL;
static struct task_struct *tp2823_wd         = NULL;
static u32 TP2823_VCH_MAP[TP2823_DEV_MAX*TP2823_DEV_CH_MAX];
static u32 TP2823_ACH_MAP[TP2823_DEV_MAX*TP2823_DEV_AUDIO_CH_MAX];

static int tp2823_i2c_probe(struct i2c_client *client, const struct i2c_device_id *did)
{
    tp2823_i2c_client = client;
    return 0;
}

static int tp2823_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tp2823_i2c_id[] = {
    { "tp2823_i2c", 0 },
    { }
};

static struct i2c_driver tp2823_i2c_driver = {
    .driver = {
        .name  = "TP2823_I2C",
        .owner = THIS_MODULE,
    },
    .probe    = tp2823_i2c_probe,
    .remove   = tp2823_i2c_remove,
    .id_table = tp2823_i2c_id
};

static int tp2823_i2c_register(void)
{
    int ret;
    struct i2c_board_info info;
    struct i2c_adapter    *adapter;
    struct i2c_client     *client;

    /* add i2c driver */
    ret = i2c_add_driver(&tp2823_i2c_driver);
    if(ret < 0) {
        printk("TP2823 can't add i2c driver!\n");
        return ret;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = iaddr[0]>>1;
    strlcpy(info.type, "tp2823_i2c", I2C_NAME_SIZE);

    adapter = i2c_get_adapter(ibus);   ///< I2C BUS#
    if (!adapter) {
        printk("TP2823 can't get i2c adapter\n");
        goto err_driver;
    }

    /* add i2c device */
    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if(!client) {
        printk("TP2823 can't add i2c device!\n");
        goto err_driver;
    }

    return 0;

err_driver:
   i2c_del_driver(&tp2823_i2c_driver);
   return -ENODEV;
}

static void tp2823_i2c_unregister(void)
{
    i2c_unregister_device(tp2823_i2c_client);
    i2c_del_driver(&tp2823_i2c_driver);
    tp2823_i2c_client = NULL;
}

static int tp2823_create_ch_map(int map_id)
{
    int i;

    switch(map_id) {
        case 1: ///< 2CH Dual Edge Mode
            for(i=0; i<dev_num; i++) {
                TP2823_VCH_MAP[(i*TP2823_DEV_CH_MAX)+0] = CH_IDENTIFY(i, 0, TP2823_DEV_VOUT0);
                TP2823_VCH_MAP[(i*TP2823_DEV_CH_MAX)+1] = CH_IDENTIFY(i, 1, TP2823_DEV_VOUT0);
                TP2823_VCH_MAP[(i*TP2823_DEV_CH_MAX)+2] = CH_IDENTIFY(i, 2, TP2823_DEV_VOUT1);
                TP2823_VCH_MAP[(i*TP2823_DEV_CH_MAX)+3] = CH_IDENTIFY(i, 3, TP2823_DEV_VOUT1);
            }
            break;

        case 0: ///< 1CH Bypass Mode
        default:
            for(i=0; i<dev_num; i++) {
                TP2823_VCH_MAP[(i*TP2823_DEV_VOUT_MAX)+0] = CH_IDENTIFY(i, 0, TP2823_DEV_VOUT0);
                TP2823_VCH_MAP[(i*TP2823_DEV_VOUT_MAX)+1] = CH_IDENTIFY(i, 1, TP2823_DEV_VOUT1);
            }
            break;
    }
	
	//Audio channel map
	for (i=0; i<dev_num; i++) {		
		TP2823_ACH_MAP[(i*TP2823_DEV_AUDIO_CH_MAX)+0] = CH_IDENTIFY(dev_num-i-1, 0, TP2823_DEV_VOUT0);
		TP2823_ACH_MAP[(i*TP2823_DEV_AUDIO_CH_MAX)+1] = CH_IDENTIFY(dev_num-i-1, 1, TP2823_DEV_VOUT0);
		TP2823_ACH_MAP[(i*TP2823_DEV_AUDIO_CH_MAX)+2] = CH_IDENTIFY(dev_num-i-1, 2, TP2823_DEV_VOUT0);
		TP2823_ACH_MAP[(i*TP2823_DEV_AUDIO_CH_MAX)+3] = CH_IDENTIFY(dev_num-i-1, 3, TP2823_DEV_VOUT0);
	}
    return 0;
}

/*
 * seq_id: channel sequence number in video output port,
 *         for 1CH_BYPASS    mode vout_seq = 0
 *         for 2CH_DUAL_EDGE mode vout_seq = 0 or 1
*/
int tp2823_get_vch_id(int id, TP2823_DEV_VOUT_T vout, int seq_id)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vout >= TP2823_DEV_VOUT_MAX || seq_id >= 2)
        return 0;

    if(vout_mode[id] == TP2823_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*TP2823_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2823_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VOUT(TP2823_VCH_MAP[i]) == vout) &&
           ((CH_IDENTIFY_VIN(TP2823_VCH_MAP[i])%vport_chnum) == seq_id)) {
            return i;
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_get_vch_id);

int tp2823_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= TP2823_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*TP2823_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2823_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2823_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_vin_to_vch);

int tp2823_get_vout_id(int id, int vin_idx)
{
    int i;

    if(id >= dev_num || vin_idx >= TP2823_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*TP2823_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2823_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2823_VCH_MAP[i]) == vin_idx))
            return CH_IDENTIFY_VOUT(TP2823_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_get_vout_id);

int tp2823_get_vout_seq_id(int id, int vin_idx)
{
    int i;
    int vport_chnum;

    if(id >= dev_num || vin_idx >= TP2823_DEV_CH_MAX)
        return 0;

    if(vout_mode[id] == TP2823_VOUT_MODE_2CH_DUAL_EDGE)
        vport_chnum = 2;
    else
        vport_chnum = 1;

    for(i=0; i<(dev_num*TP2823_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2823_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2823_VCH_MAP[i]) == vin_idx)) {
            return (vin_idx%vport_chnum);
        }
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_get_vout_seq_id);

int tp2823_get_vout_format(int id)
{
    if(id >= dev_num)
        return TP2823_VOUT_FORMAT_BT656;
    else
        return vout_format[id];
}
EXPORT_SYMBOL(tp2823_get_vout_format);

int tp2823_get_vout_mode(int id)
{
    if(id >= dev_num)
        return TP2823_VOUT_MODE_1CH_BYPASS;
    else
        return vout_mode[id];
}
EXPORT_SYMBOL(tp2823_get_vout_mode);

int tp2823_get_device_num(void)
{
    return dev_num;
}
EXPORT_SYMBOL(tp2823_get_device_num);

int tp2823_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2823_video_fmt_t *))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2823_dev[id].lock);

    tp2823_dev[id].vfmt_notify = nt_func;

    up(&tp2823_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2823_notify_vfmt_register);

void tp2823_notify_vfmt_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2823_dev[id].lock);

    tp2823_dev[id].vfmt_notify = NULL;

    up(&tp2823_dev[id].lock);
}
EXPORT_SYMBOL(tp2823_notify_vfmt_deregister);

int tp2823_notify_vlos_register(int id, int (*nt_func)(int, int, int))
{
    if(id >= dev_num || !nt_func)
        return -1;

    down(&tp2823_dev[id].lock);

    tp2823_dev[id].vlos_notify = nt_func;

    up(&tp2823_dev[id].lock);

    return 0;
}
EXPORT_SYMBOL(tp2823_notify_vlos_register);

void tp2823_notify_vlos_deregister(int id)
{
    if(id >= dev_num)
        return;

    down(&tp2823_dev[id].lock);

    tp2823_dev[id].vlos_notify = NULL;

    up(&tp2823_dev[id].lock);
}
EXPORT_SYMBOL(tp2823_notify_vlos_deregister);

int tp2823_i2c_write(u8 id, u8 reg, u8 data)
{
    u8 buf[2];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return -1;

    if(!tp2823_i2c_client) {
        printk("TP2823 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2823_i2c_client->dev.parent);

    buf[0]     = reg;
    buf[1]     = data;
    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("TP2823#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_i2c_write);

u8 tp2823_i2c_read(u8 id, u8 reg)
{
    u8 reg_data = 0;
    struct i2c_msg msgs[2];
    struct i2c_adapter *adapter;

    if(id >= dev_num)
        return 0;

    if(!tp2823_i2c_client) {
        printk("TP2823 i2c_client not register!!\n");
        return 0;
    }

    adapter = to_i2c_adapter(tp2823_i2c_client->dev.parent);

    msgs[0].addr  = iaddr[id]>>1;
    msgs[0].flags = 0; /* write */
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = (iaddr[id] + 1)>>1;
    msgs[1].flags = 1; /* read */
    msgs[1].len   = 1;
    msgs[1].buf   = &reg_data;

    if(i2c_transfer(adapter, msgs, 2) != 2)
        printk("TP2823#%d i2c read failed!!\n", id);

    return reg_data;
}
EXPORT_SYMBOL(tp2823_i2c_read);

int tp2823_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt)
{
#define TP2823_I2C_ARRAY_WRITE_MAX     256
    int i, num = 0;
    u8  buf[TP2823_I2C_ARRAY_WRITE_MAX+1];
    struct i2c_msg msgs;
    struct i2c_adapter *adapter;

    if(id >= dev_num || !parray || array_cnt > TP2823_I2C_ARRAY_WRITE_MAX)
        return -1;

    if(!tp2823_i2c_client) {
        printk("TP2823 i2c_client not register!!\n");
        return -1;
    }

    adapter = to_i2c_adapter(tp2823_i2c_client->dev.parent);

    buf[num++] = addr;
    for(i=0; i<array_cnt; i++) {
        buf[num++] = parray[i];
    }

    msgs.addr  = iaddr[id]>>1;
    msgs.flags = 0;
    msgs.len   = num;
    msgs.buf   = buf;

    if(i2c_transfer(adapter, &msgs, 1) != 1) {
        printk("TP2823#%d i2c write failed!!\n", id);
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(tp2823_i2c_array_write);

static int tp2823_proc_status_show(struct seq_file *sfile, void *v)
{
    int i;
    int vlos;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2823#%d]\n", pdev->index);
    seq_printf(sfile, "------------------\n");
    seq_printf(sfile, "VIN    VCH    LOS \n");
    seq_printf(sfile, "==================\n");
    for(i=0; i<(vout_mode[pdev->index]==TP2823_VOUT_MODE_1CH_BYPASS?TP2823_DEV_CH_MAX/TP2823_DEV_VOUT_MAX:TP2823_DEV_CH_MAX); i++) {
        vlos = tp2823_status_get_video_loss(pdev->index, i);
        seq_printf(sfile, "%-6d %-6d %-7s\n", i, tp2823_vin_to_vch(pdev->index, i), ((vlos == 0) ? "NO" : "YES"));
    }
    up(&pdev->lock);

    return 0;
}

static int tp2823_proc_video_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret, vlos;
    struct tp2823_video_fmt_t vfmt;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2823#%d] Incoming Video Fromat\n", pdev->index);
    seq_printf(sfile, "====================================================\n");
    seq_printf(sfile, "VIN   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "====================================================\n");

    for(i=0; i<(vout_mode[pdev->index]==TP2823_VOUT_MODE_1CH_BYPASS?TP2823_DEV_CH_MAX/TP2823_DEV_VOUT_MAX:TP2823_DEV_CH_MAX); i++) {
        vlos = tp2823_status_get_video_loss(pdev->index, i);
        if(vlos == 0)
            ret = tp2823_status_get_video_input_format(pdev->index, i, &vfmt);
        else
            ret = -1; ///< Unknown video input format when video loss
        if((ret == 0) && ((vfmt.fmt_idx >= TP2823_VFMT_720P60) && (vfmt.fmt_idx < TP2823_VFMT_MAX))) {
            seq_printf(sfile, "%-5d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2823_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-5d %-5d %-7s %-8s %-12s %-11s\n", i, tp2823_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}

static int tp2823_proc_vout_fmt_show(struct seq_file *sfile, void *v)
{
    int i, ret;
    struct tp2823_video_fmt_t vfmt;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "[TP2823#%d] Video Output Fromat\n", pdev->index);
    seq_printf(sfile, "=====================================================\n");
    seq_printf(sfile, "VOUT   VCH   Width   Height   Prog/Inter   Frame_Rate\n");
    seq_printf(sfile, "=====================================================\n");

    for(i=0; i<(vout_mode[pdev->index]==TP2823_VOUT_MODE_1CH_BYPASS?TP2823_DEV_CH_MAX/TP2823_DEV_VOUT_MAX:TP2823_DEV_CH_MAX); i++) {
        ret = tp2823_video_get_video_output_format(pdev->index, i, &vfmt);
        if((ret == 0) && ((vfmt.fmt_idx >= TP2823_VFMT_720P60) && (vfmt.fmt_idx < TP2823_VFMT_MAX))) {
            seq_printf(sfile, "%-6d %-5d %-7d %-8d %-12s %-11d\n",
                       i,
                       tp2823_vin_to_vch(pdev->index, i),
                       vfmt.width,
                       vfmt.height,
                       ((vfmt.prog == 1) ? "Progressive" : "Interlace"),
                       vfmt.frame_rate);
        }
        else {
            seq_printf(sfile, "%-6d %-5d %-7s %-8s %-12s %-11s\n", i, tp2823_vin_to_vch(pdev->index, i), "-", "-", "-", "-");
        }
    }

    up(&pdev->lock);

    return 0;
}


static int tp2823_proc_volume_show(struct seq_file *sfile, void *v)
{
    int aogain;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[TP2823#%d]\n", pdev->index);
    aogain = tp2823_audio_get_volume(pdev->index);
    seq_printf(sfile, "Volume[0x0~0xf] = %d\n", aogain);

    up(&pdev->lock);

    return 0;
}

static ssize_t tp2823_proc_volume_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 volume;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &volume);

    down(&pdev->lock);

    tp2823_audio_set_volume(pdev->index, volume);

    up(&pdev->lock);

    return count;
}

static int tp2823_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_status_show, PDE(inode)->data);
}

static int tp2823_proc_video_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_video_fmt_show, PDE(inode)->data);
}

static int tp2823_proc_vout_fmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_vout_fmt_show, PDE(inode)->data);
}

static int tp2823_proc_volume_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp2823_proc_volume_show, PDE(inode)->data);
}

static struct file_operations tp2823_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2823_proc_video_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_video_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2823_proc_vout_fmt_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_vout_fmt_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations tp2823_proc_volume_ops = {
    .owner  = THIS_MODULE,
    .open   = tp2823_proc_volume_open,
    .write  = tp2823_proc_volume_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static void tp2823_proc_remove(int id)
{
    if(id >= TP2823_DEV_MAX)
        return;

    if(tp2823_proc_root[id]) {
        if(tp2823_proc_status[id]) {
            remove_proc_entry(tp2823_proc_status[id]->name, tp2823_proc_root[id]);
            tp2823_proc_status[id] = NULL;
        }

        if(tp2823_proc_video_fmt[id]) {
            remove_proc_entry(tp2823_proc_video_fmt[id]->name, tp2823_proc_root[id]);
            tp2823_proc_video_fmt[id] = NULL;
        }

        if(tp2823_proc_vout_fmt[id]) {
            remove_proc_entry(tp2823_proc_vout_fmt[id]->name, tp2823_proc_root[id]);
            tp2823_proc_vout_fmt[id] = NULL;
        }

        if(tp2823_proc_volume[id]) {
            remove_proc_entry(tp2823_proc_volume[id]->name, tp2823_proc_root[id]);
            tp2823_proc_volume[id] = NULL;
        }

        remove_proc_entry(tp2823_proc_root[id]->name, NULL);
        tp2823_proc_root[id] = NULL;
    }
}

static int tp2823_proc_init(int id)
{
    int ret = 0;

    if(id >= TP2823_DEV_MAX)
        return -1;

    /* root */
    tp2823_proc_root[id] = create_proc_entry(tp2823_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!tp2823_proc_root[id]) {
        printk("create proc node '%s' failed!\n", tp2823_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_root[id]->owner = THIS_MODULE;
#endif

    /* status */
    tp2823_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", tp2823_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_status[id]->proc_fops = &tp2823_proc_status_ops;
    tp2823_proc_status[id]->data      = (void *)&tp2823_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* frame format */
    tp2823_proc_video_fmt[id] = create_proc_entry("video_format", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_video_fmt[id]) {
        printk("create proc node '%s/video_format' failed!\n", tp2823_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_video_fmt[id]->proc_fops = &tp2823_proc_video_fmt_ops;
    tp2823_proc_video_fmt[id]->data      = (void *)&tp2823_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_video_fmt[id]->owner     = THIS_MODULE;
#endif

    /* video output format */
    tp2823_proc_vout_fmt[id] = create_proc_entry("vout_format", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_vout_fmt[id]) {
        printk("create proc node '%s/vout_format' failed!\n", tp2823_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_vout_fmt[id]->proc_fops = &tp2823_proc_vout_fmt_ops;
    tp2823_proc_vout_fmt[id]->data      = (void *)&tp2823_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_vout_fmt[id]->owner     = THIS_MODULE;
#endif

    /* volume */
    tp2823_proc_volume[id] = create_proc_entry("volume", S_IRUGO|S_IXUGO, tp2823_proc_root[id]);
    if(!tp2823_proc_volume[id]) {
        printk("create proc node '%s/volume' failed!\n", tp2823_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    tp2823_proc_volume[id]->proc_fops = &tp2823_proc_volume_ops;
    tp2823_proc_volume[id]->data      = (void *)&tp2823_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    tp2823_proc_volume[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    tp2823_proc_remove(id);
    return ret;
}

static int tp2823_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct tp2823_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(tp2823_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &tp2823_dev[i];
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

static int tp2823_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long tp2823_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int tmp, ret = 0;
    struct tp2823_dev_t *pdev = (struct tp2823_dev_t *)file->private_data;

    down(&pdev->lock);

    if(_IOC_TYPE(cmd) != TP2823_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case TP2823_GET_NOVID:
            {
                struct tp2823_ioc_data_t ioc_data;

                if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_data.vin_ch >= TP2823_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ioc_data.data = tp2823_status_get_video_loss(pdev->index, ioc_data.vin_ch);

                ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            }
            break;

        case TP2823_GET_VIDEO_FMT:
            {
                struct tp2823_ioc_vfmt_t ioc_vfmt;
                struct tp2823_video_fmt_t vfmt;

                if(copy_from_user(&ioc_vfmt, (void __user *)arg, sizeof(ioc_vfmt))) {
                    ret = -EFAULT;
                    goto exit;
                }

                if(ioc_vfmt.vin_ch >= TP2823_DEV_CH_MAX) {
                    ret = -EFAULT;
                    goto exit;
                }

                ret = tp2823_status_get_video_input_format(pdev->index, ioc_vfmt.vin_ch, &vfmt);
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

        case TP2823_GET_VOL:
            tmp = tp2823_audio_get_volume(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
            break;

        case TP2823_SET_VOL:
            if(copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = tp2823_audio_set_volume(pdev->index, tmp);
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

static struct file_operations tp2823_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = tp2823_miscdev_open,
    .release        = tp2823_miscdev_release,
    .unlocked_ioctl = tp2823_miscdev_ioctl,
};

static int tp2823_miscdev_init(int id)
{
    int ret;

    if(id >= TP2823_DEV_MAX)
        return -1;

    /* clear */
    memset(&tp2823_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    tp2823_dev[id].miscdev.name  = tp2823_dev[id].name;
    tp2823_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    tp2823_dev[id].miscdev.fops  = (struct file_operations *)&tp2823_miscdev_fops;
    ret = misc_register(&tp2823_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", tp2823_dev[id].name);
        tp2823_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

static void tp2823_hw_reset(void)
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

static int tp2823_device_init(int id)
{
    int ret;

    if(id >= TP2823_DEV_MAX)
        return -1;

    if(!init)
        goto exit;

    /* video init */
    ret = tp2823_video_init(id, vout_format[id], vout_mode[id]);
    if(ret < 0)
        goto exit;

    /* audio init */
    ret = tp2823_audio_set_mode(id, vout_mode[(id>>1)<<1], sample_size, sample_rate);
    if(ret < 0)
        goto exit;
		
exit:
    printk("TP2823#%d Init...%s!\n", id, ((ret == 0) ? "OK" : "Fail"));

    return ret;
}

#define VLOS_DEBOUNCE_THRESHOLD 3
#define VFMT_DEBOUNCE_THRESHOLD 20
#define VFMT_DEBOUNCE_THRESHOLD_MAX 40

static int tp2823_watchdog_thread(void *data)
{
    int i, ch, ret, ch_max;
    struct tp2823_dev_t *pdev;
    struct tp2823_video_fmt_t vin_fmt, vout_fmt;
    int pre_fmt_idx[TP2823_DEV_MAX*TP2823_DEV_CH_MAX] = {[0 ... (TP2823_DEV_MAX*TP2823_DEV_CH_MAX - 1)] = TP2823_VFMT_MAX}; ///< Previous video input format
    int vfmt_debounce[TP2823_DEV_MAX*TP2823_DEV_CH_MAX] = {[0 ... (TP2823_DEV_MAX*TP2823_DEV_CH_MAX - 1)] = 0}; ///< debounce design to fix the wrong format detection during 960H output
    int vlos_debounce[TP2823_DEV_MAX*TP2823_DEV_CH_MAX] = {[0 ... (TP2823_DEV_MAX*TP2823_DEV_CH_MAX - 1)] = 0}; ///< debounce design to fix the wrong video loss detection for customer EVB

    do {
        /* check tp2823 channel status */
        for(i=0; i<dev_num; i++) {
            ch_max = (vout_mode[i]==TP2823_VOUT_MODE_1CH_BYPASS?TP2823_DEV_CH_MAX/TP2823_DEV_VOUT_MAX:TP2823_DEV_CH_MAX);
            pdev = &tp2823_dev[i];

            down(&pdev->lock);

            for(ch=0; ch<ch_max; ch++) {
                pdev->cur_plugin[ch] = (tp2823_status_get_video_loss(i, ch) == 0) ? 1 : 0;

                if(vfmt_debounce[i*TP2823_DEV_CH_MAX+ch] > VFMT_DEBOUNCE_THRESHOLD) {
                    if(pdev->cur_plugin[ch] == pdev->pre_plugin[ch]) {
                        if(vlos_debounce[i*TP2823_DEV_CH_MAX+ch] < VLOS_DEBOUNCE_THRESHOLD)
                            vlos_debounce[i*TP2823_DEV_CH_MAX+ch]++;
                        else
                            vlos_debounce[i*TP2823_DEV_CH_MAX+ch] = VLOS_DEBOUNCE_THRESHOLD;
                    }
                    else {
                        if(vlos_debounce[i*TP2823_DEV_CH_MAX+ch] > 0) {
                            vlos_debounce[i*TP2823_DEV_CH_MAX+ch]--;
                            pdev->cur_plugin[ch] = pdev->pre_plugin[ch];
                        }
                    }
                }

                if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 0)) {        ///< cable plugged-in
                    goto sts_update;
                }
                else if((pdev->cur_plugin[ch] == 1) && (pdev->pre_plugin[ch] == 1)) {   ///< cable is connected
                    /* notify current video present */
                    if(notify && pdev->vlos_notify)
                        pdev->vlos_notify(i, ch, 0);

                    /* get input camera video fromat */
                    ret = tp2823_status_get_video_input_format(i, ch, &vin_fmt);

                    if((vfmt_debounce[i*TP2823_DEV_CH_MAX+ch] > VFMT_DEBOUNCE_THRESHOLD) && (pre_fmt_idx[i*TP2823_DEV_CH_MAX+ch] != vin_fmt.fmt_idx)) {
                        vfmt_debounce[i*TP2823_DEV_CH_MAX+ch]--;
                        goto sts_update;
                    }

                    /* switch video port output format */
                    if(ret == 0) {
                        if((vout_mode[i] == TP2823_VOUT_MODE_2CH_DUAL_EDGE) && (vin_fmt.fmt_idx!= TP2823_VFMT_960H30) && (vin_fmt.fmt_idx!= TP2823_VFMT_960H25)) {
                            if((pre_fmt_idx[i*TP2823_DEV_CH_MAX+(ch%2?ch-1:ch+1)] == TP2823_VFMT_960H30) || (pre_fmt_idx[i*TP2823_DEV_CH_MAX+(ch%2?ch-1:ch+1)] == TP2823_VFMT_960H25)) { ///< If another channel at the same vout port is 960H output, force 960H30 blue pattern output
                                ret = tp2823_video_set_pattern_output(i, ch, TP2823_VFMT_960H30, vout_format[i], 1); if(ret < 0) goto sts_update;
                                pre_fmt_idx[i*TP2823_DEV_CH_MAX+ch] = TP2823_VFMT_MAX;
                            }
                            else { ///< Normal output
                                ret = tp2823_video_set_pattern_output(i, ch, vin_fmt.fmt_idx, vout_format[i], 0); if(ret < 0) goto sts_update;
                            }
                        }
                        else { ///< 1CH Bypass mode or current video input is 960H
                            ret = tp2823_video_set_pattern_output(i, ch, vin_fmt.fmt_idx, vout_format[i], 0); if(ret < 0) goto sts_update;
                        }
                    }
                    /* get video port output format */
                    ret = tp2823_video_get_video_output_format(i, ch, &vout_fmt); if(ret < 0) goto sts_update;

                    /* notify current video format */
                    if(notify && pdev->vfmt_notify && (vout_fmt.fmt_idx >= TP2823_VFMT_720P60) && (vout_fmt.fmt_idx < TP2823_VFMT_MAX)) {
                        pdev->vfmt_notify(i, ch, &vout_fmt);
                    }

                    pre_fmt_idx[i*TP2823_DEV_CH_MAX+ch] = vin_fmt.fmt_idx;
                    if(pre_fmt_idx[i*TP2823_DEV_CH_MAX+ch] == vin_fmt.fmt_idx) {
                        if(vfmt_debounce[i*TP2823_DEV_CH_MAX+ch] < VFMT_DEBOUNCE_THRESHOLD_MAX)
                            vfmt_debounce[i*TP2823_DEV_CH_MAX+ch]++;
                        else
                            vfmt_debounce[i*TP2823_DEV_CH_MAX+ch] = VFMT_DEBOUNCE_THRESHOLD_MAX;
                    }

                }
                else {  ///< cable is plugged-out
                    /* notify current video loss */
                    if(notify && pdev->vlos_notify)
                        pdev->vlos_notify(i, ch, 1);

                    /* If HD cable is plugged-out, force black image output to 720P25(1CH Bypass)/Half-720P15(2CH Dual Edge) */
                    if(vout_mode[i] == TP2823_VOUT_MODE_2CH_DUAL_EDGE) {
                        if((pdev->cur_plugin[(ch%2?ch-1:ch+1)] == 1) && ((pre_fmt_idx[i*TP2823_DEV_CH_MAX+(ch%2?ch-1:ch+1)] == TP2823_VFMT_960H30) || (pre_fmt_idx[i*TP2823_DEV_CH_MAX+(ch%2?ch-1:ch+1)] == TP2823_VFMT_960H25))) ///< If another channel at the same vout port is 960H output, force black image output to 960H25
                            ret = tp2823_video_set_pattern_output(i, ch, TP2823_VFMT_960H30, vout_format[i], 0);
                        else
                            ret = tp2823_video_set_video_output_format(i, ch, TP2823_VFMT_720P60, vout_format[i]);
                    }
                    else
                        ret = tp2823_video_set_video_output_format(i, ch, TP2823_VFMT_720P25, vout_format[i]);

                    if(ret < 0)
                        goto sts_update;

                    /* notify current video format */
                    if(notify && pdev->vfmt_notify) {
                        /* get video port output format */
                        ret = tp2823_video_get_video_output_format(i, ch, &vout_fmt); if(ret < 0) goto sts_update;
                        pdev->vfmt_notify(i, ch, &vout_fmt);
                    }

                    /* Reset the variable for debounce design */
                    pre_fmt_idx[i*TP2823_DEV_CH_MAX+ch] = TP2823_VFMT_MAX;
                    vlos_debounce[i*TP2823_DEV_CH_MAX+ch] = 0;
                    vfmt_debounce[i*TP2823_DEV_CH_MAX+ch] = 0;
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

static int __init tp2823_init(void)
{
    int i, ret;

    /* platfrom check */
    plat_identifier_check();

    /* check device number */
    if(dev_num > TP2823_DEV_MAX) {
        printk("TP2823 dev_num=%d invalid!!(Max=%d)\n", dev_num, TP2823_DEV_MAX);
        return -1;
    }

    /* check video output format */
    for(i=0; i<dev_num; i++) {
        if((vout_format[i] < 0) || (vout_format[i] >= TP2823_VOUT_FORMAT_MAX)) {
            printk("TP2823#%d vout_format=%d not support!!\n", i, vout_format[i]);
            return -1;
        }
    }

    /* check video output mode */
    for(i=0; i<dev_num; i++) {
        if((vout_mode[i] < 0) || (vout_mode[i] >= TP2823_VOUT_MODE_MAX)) {
            printk("TP2823#%d vout_mode=%d not support!!\n", i, vout_mode[i]);
            return -1;
        }
    }

    /* register i2c client for contol tp2823 */
    ret = tp2823_i2c_register();
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
    ret = tp2823_create_ch_map(ch_map);
    if(ret < 0)
        goto err;

    /* hardware reset for all device */
    tp2823_hw_reset();

    /* TP2823 init */
    for(i=0; i<dev_num; i++) {
        tp2823_dev[i].index = i;

        sprintf(tp2823_dev[i].name, "tp2823.%d", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        sema_init(&tp2823_dev[i].lock, 1);
#else
        init_MUTEX(&tp2823_dev[i].lock);
#endif
        ret = tp2823_proc_init(i);
        if(ret < 0)
            goto err;

        ret = tp2823_miscdev_init(i);
        if(ret < 0)
            goto err;

        ret = tp2823_device_init(i);
        if(ret < 0)
            goto err;
    }

    if(init) {
        /* init tp2823 watchdog thread for check video status */
        tp2823_wd = kthread_create(tp2823_watchdog_thread, NULL, "tp2823_wd");
        if(!IS_ERR(tp2823_wd))
            wake_up_process(tp2823_wd);
        else {
            printk("create tp2823 watchdog thread failed!!\n");
            tp2823_wd = 0;
            ret = -EFAULT;
            goto err;
        }
    }

    return 0;

err:
    if(tp2823_wd)
        kthread_stop(tp2823_wd);

    tp2823_i2c_unregister();
    for(i=0; i<TP2823_DEV_MAX; i++) {
        if(tp2823_dev[i].miscdev.name)
            misc_deregister(&tp2823_dev[i].miscdev);

        tp2823_proc_remove(i);
    }

    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);

    return ret;
}

static void __exit tp2823_exit(void)
{
    int i;

    /* stop tp2823 watchdog thread */
    if(tp2823_wd)
        kthread_stop(tp2823_wd);

    tp2823_i2c_unregister();

    for(i=0; i<TP2823_DEV_MAX; i++) {
        /* remove device node */
        if(tp2823_dev[i].miscdev.name)
            misc_deregister(&tp2823_dev[i].miscdev);

        /* remove proc node */
        tp2823_proc_remove(i);
    }

    /* release gpio pin */
    if(rstb_used && init)
        plat_deregister_gpio_pin(rstb_used);
}

module_init(tp2823_init);
module_exit(tp2823_exit);

MODULE_DESCRIPTION("Grain Media TP2823 4CH HD-TVI Video Decoder Driver");
MODULE_AUTHOR("Grain Media Technology Corp.");
MODULE_LICENSE("GPL");
