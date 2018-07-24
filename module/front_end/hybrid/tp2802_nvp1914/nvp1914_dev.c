/**
 * @file nvp1914_dev.c
 * Nextchip NVP1914 4-CH 960H/D1 Video Decoders and Audio Codecs Driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2015/01/16 06:26:05 $
 *
 * ChangeLog:
 *  $Log: nvp1914_dev.c,v $
 *  Revision 1.1  2015/01/16 06:26:05  shawn_hu
 *  Add HD-TVI TP2802 + D1 NVP1914 hybrid frontend driver design for TVT product.
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <mach/ftpmu010.h>

#include "platform.h"
#include "techpoint/tp2802.h"          ///< from module/include/front_end/hdtvi/techpoint
#include "nextchip/nvp1914.h"          ///< from module/include/front_end/decoder
#include "nvp1914_dev.h"
#include "tp2802_nvp1914_hybrid_drv.h"

/*************************************************************************************
 *  Initial values of the following structures come from tp2802_nvp1914_hybrid.c
 *************************************************************************************/
static int dev_num;
static ushort iaddr[NVP1914_DEV_MAX];
static int vmode[NVP1914_DEV_MAX];
static int sample_rate, sample_size;
static struct nvp1914_dev_t* nvp1914_dev;
static int init;
static u32 TP2802_NVP1914_VCH_MAP[TP2802_DEV_MAX*TP2802_DEV_CH_MAX];

/*************************************************************************************
 *  Original structure
 *************************************************************************************/
static struct proc_dir_entry *nvp1914_proc_root[NVP1914_DEV_MAX]      = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_vmode[NVP1914_DEV_MAX]     = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_status[NVP1914_DEV_MAX]    = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_bri[NVP1914_DEV_MAX]       = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_con[NVP1914_DEV_MAX]       = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_sat[NVP1914_DEV_MAX]       = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_hue[NVP1914_DEV_MAX]       = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_sharp[NVP1914_DEV_MAX]     = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_volume[NVP1914_DEV_MAX]    = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};
static struct proc_dir_entry *nvp1914_proc_output_ch[NVP1914_DEV_MAX] = {[0 ... (NVP1914_DEV_MAX - 1)] = NULL};

/*************************************************************************************
 *  Revised for hybrid design
 *************************************************************************************/
int nvp1914_i2c_write(u8 id, u8 reg, u8 data)
{
    return tp2802_nvp1914_i2c_write(iaddr[id], reg, data);
}
EXPORT_SYMBOL(nvp1914_i2c_write);

u8 nvp1914_i2c_read(u8 id, u8 reg)
{
    return tp2802_nvp1914_i2c_read(iaddr[id], reg);
}
EXPORT_SYMBOL(nvp1914_i2c_read);

int nvp1914_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt)
{
#define NVP1914_I2C_ARRAY_WRITE_MAX     256
    int i, num = 0;
    u8  buf[NVP1914_I2C_ARRAY_WRITE_MAX+1];

    if(id >= dev_num || !parray || array_cnt > NVP1914_I2C_ARRAY_WRITE_MAX)
        return -1;

    buf[num++] = addr;
    for(i=0; i<array_cnt; i++) {
        buf[num] = parray[i];
        tp2802_nvp1914_i2c_write(iaddr[id], buf[0], buf[num]);
        num++;
        buf[0]++;
    }

    return 0;
}
EXPORT_SYMBOL(nvp1914_i2c_array_write);

void nvp1914_set_params(nvp1914_params_p params_p)
{
    int i = 0;

    dev_num = params_p->nvp1914_dev_num;
    for(i=0; i<dev_num; i++) {
        iaddr[i] = *(params_p->nvp1914_iaddr + i);
    }
    for(i=0; i<NVP1914_DEV_MAX; i++) {
        vmode[i] = *(params_p->nvp1914_vmode + i);
    }
    sample_rate = params_p->nvp1914_sample_rate;
    sample_size = params_p->nvp1914_sample_size;
    nvp1914_dev = params_p->nvp1914_dev;
    init = params_p->init;
    for(i=0; i<TP2802_DEV_MAX*TP2802_DEV_CH_MAX; i++) {
        TP2802_NVP1914_VCH_MAP[i] = *(params_p->TP2802_NVP1914_VCH_MAP + i);
    }
}

/*************************************************************************************
 *  Keep as original design (only XXXX_VCH_MAP was updated)
 *************************************************************************************/
int nvp1914_vin_to_ch(int id, int vin_idx)
{
    int i;

    if(id >= NVP1914_DEV_MAX || vin_idx >= NVP1914_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP1914_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[i]) == vin_idx))
            return (i%NVP1914_DEV_CH_MAX);
    }

    return 0;
}
EXPORT_SYMBOL(nvp1914_vin_to_ch);

int nvp1914_ch_to_vin(int id, int ch_idx)
{
    int i;

    if(id >= NVP1914_DEV_MAX || ch_idx >= NVP1914_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP1914_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[i]) == id) && ((i%NVP1914_DEV_CH_MAX) == ch_idx))
            return CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[i]);
    }

    return 0;
}
EXPORT_SYMBOL(nvp1914_ch_to_vin);

int nvp1914_get_vch_id(int id, NVP1914_DEV_VPORT_T vport, int vport_seq)
{
    int i;

    if(id >= NVP1914_DEV_MAX || vport >= NVP1914_DEV_VPORT_MAX || vport_seq >= 4)
        return 0;

    for(i=0; i<(dev_num*NVP1914_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[i]) == id) &&
           (CH_IDENTIFY_VOUT(TP2802_NVP1914_VCH_MAP[i]) == vport) &&
           ((CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[i])%4) == vport_seq)) {
            return i;
        }
    }

    return 0;
}
EXPORT_SYMBOL(nvp1914_get_vch_id);

int nvp1914_vin_to_vch(int id, int vin_idx)
{
    int i;

    if(id >= NVP1914_DEV_MAX || vin_idx >= NVP1914_DEV_CH_MAX)
        return 0;

    for(i=0; i<(dev_num*NVP1914_DEV_CH_MAX); i++) {
        if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[i]) == id) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[i]) == vin_idx))
            return i;
    }

    return 0;
}
EXPORT_SYMBOL(nvp1914_vin_to_vch);

/* return value, 0: success, others: failed */
int nvp1914_sound_switch(int ssp_idx, int chan_num, bool is_on)
{
    const u32 single_decoder_chan_cnt  = 8;
    const u32 cascade_decoder_chan_cnt = 16;
    const u32 max_audio_chan_cnt   = 32;
    const u32 chip_id = ftpmu010_get_attr(ATTR_TYPE_CHIPVER) >> 16;
    u32 temp, vin_idx, decoder_idx = 0;

    switch (chip_id) {
        case 0x8287:
            decoder_idx = 0;
            break;
        default:
            decoder_idx = 0;
            break;
    }

    if (chan_num >= max_audio_chan_cnt) {
        printk(KERN_WARNING "%s: Not implement yet\n", __func__);
        return -EACCES;
    }

    temp = chan_num;
    if (chan_num >= cascade_decoder_chan_cnt && chan_num < max_audio_chan_cnt) {
        chan_num -= cascade_decoder_chan_cnt;
        decoder_idx = 2;
    }
    if (chan_num >= single_decoder_chan_cnt)
        chan_num -= single_decoder_chan_cnt;
    vin_idx = nvp1914_ch_to_vin(decoder_idx, chan_num);

    if (temp >= single_decoder_chan_cnt)
        vin_idx += single_decoder_chan_cnt;
    is_on ? nvp1914_audio_set_output_ch(decoder_idx, vin_idx) : nvp1914_audio_set_output_ch(decoder_idx, 0x10);

    return 0;
}

static int nvp1914_proc_vmode_show(struct seq_file *sfile, void *v)
{
    int i, vmode;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;
    char *vmode_str[] = {"NTSC_720H_1CH", "NTSC_720H_2CH", "NTSC_720H_4CH",
                         "NTSC_960H_1CH", "NTSC_960H_2CH", "NTSC_960H_4CH",
                         "PAL_720H_1CH" , "PAL_720H_2CH" , "PAL_720H_4CH",
                         "PAL_960H_1CH" , "PAL_960H_2CH" , "PAL_960H_4CH"};

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    for(i=0; i<NVP1914_VMODE_MAX; i++) {
        if(nvp1914_video_mode_support_check(pdev->index, i))
            seq_printf(sfile, "%02d: %s\n", i, vmode_str[i]);
    }
    seq_printf(sfile, "----------------------------------\n");

    vmode = nvp1914_video_get_mode(pdev->index);
    seq_printf(sfile, "Current==> %s\n\n",(vmode >= 0 && vmode < NVP1914_VMODE_MAX) ? vmode_str[vmode] : "Unknown");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_status_show(struct seq_file *sfile, void *v)
{
    int i, j, novid;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    novid = nvp1914_status_get_novid(pdev->index);
    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    NOVID     \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d %s\n", i, j, (novid & (0x1<<(NVP1914_DEV_CH_MAX-i-1))) ? "Video_Loss" : "Video_On");
            }
        }
    }
    seq_printf(sfile, "\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_bri_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    BRIGHTNESS\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, j, nvp1914_video_get_brightness(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nBrightness[-128 ~ +127] ==> 0x01=+1, 0x7f=+127, 0x80=-128, 0xff=-1\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_con_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    CONTRAST  \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, j, nvp1914_video_get_contrast(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nContrast[0x00 ~ 0xff] ==> 0x00=x0, 0x40=x0.5, 0x80=x1, 0xff=x2\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_sat_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    SATURATION\n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, j, nvp1914_video_get_saturation(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nSaturation[0x00 ~ 0xff] ==> 0x00=x0, 0x80=x1, 0xc0=x1.5, 0xff=x2\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_hue_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    HUE       \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, j, nvp1914_video_get_hue(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nHue[0x00 ~ 0xff] ==> 0x00=0, 0x40=90, 0x80=180, 0xff=360\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_sharp_show(struct seq_file *sfile, void *v)
{
    int i, j;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    seq_printf(sfile, "--------------------------\n");
    seq_printf(sfile, "VIN#    VCH#    SHARPNESS \n");
    seq_printf(sfile, "--------------------------\n");
    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        for(j=0; j<(dev_num*NVP1914_DEV_CH_MAX); j++) {
            if((CH_IDENTIFY_ID(TP2802_NVP1914_VCH_MAP[j]) == pdev->index) && (CH_IDENTIFY_VIN(TP2802_NVP1914_VCH_MAP[j]) == i)) {
                seq_printf(sfile, "%-7d %-7d 0x%02x\n", i, j, nvp1914_video_get_sharpness(pdev->index, i));
            }
        }
    }
    seq_printf(sfile, "\nH_Sharpness[0x0 ~ 0xf] - Bit[7:4] ==> 0x0:x0, 0x4:x0.5, 0x8:x1, 0xf:x2");
    seq_printf(sfile, "\nV_Sharpness[0x0 ~ 0xf] - Bit[3:0] ==> 0x0:x1, 0x4:x2,   0x8:x3, 0xf:x4\n\n");

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_volume_show(struct seq_file *sfile, void *v)
{
    int aogain;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);
    aogain = nvp1914_audio_get_mute(pdev->index);
    seq_printf(sfile, "Volume[0x0~0xf] = %d\n", aogain);

    up(&pdev->lock);

    return 0;
}

static int nvp1914_proc_output_ch_show(struct seq_file *sfile, void *v)
{
    int ch, vin_idx;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;
    static char *output_ch_str[] = {"CH 1", "CH 2", "CH 3", "CH 4", "CH 5", "CH 6",
                                    "CH 7", "CH 8", "CH 9", "CH 10", "CH 11", "CH 12",
                                    "CH 13", "CH 14", "CH 15", "CH 16", "FIRST PLAYBACK AUDIO",
                                    "SECOND PLAYBACK AUDIO", "THIRD PLAYBACK AUDIO",
                                    "FOURTH PLAYBACK AUDIO", "MIC input 1", "MIC input 2",
                                    "MIC input 3", "MIC input 4", "Mixed audio", "no audio output"};

    down(&pdev->lock);

    seq_printf(sfile, "\n[NVP1914#%d]\n", pdev->index);

    vin_idx = nvp1914_audio_get_output_ch(pdev->index);

    if (vin_idx >= 0 && vin_idx <= 7)
        ch = nvp1914_vin_to_ch(pdev->index, vin_idx);
    else if (vin_idx >= 8 && vin_idx <= 15) {
        vin_idx -= 8;
        ch = nvp1914_vin_to_ch(pdev->index, vin_idx);
        ch += 8;
    }
    else
        ch = vin_idx;

    seq_printf(sfile, "Current[0x0~0x18]==> %s\n\n", output_ch_str[ch]);

    up(&pdev->lock);

    return 0;
}

static ssize_t nvp1914_proc_vmode_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int vmode;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &vmode);

    down(&pdev->lock);

    if(vmode != nvp1914_video_get_mode(pdev->index))
        nvp1914_video_set_mode(pdev->index, vmode);

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_bri_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  i, vin;
    u32  bri;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &bri);

    down(&pdev->lock);

    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP1914_DEV_CH_MAX) {
            nvp1914_video_set_brightness(pdev->index, i, (u8)bri);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_con_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 con;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &con);

    down(&pdev->lock);

    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP1914_DEV_CH_MAX) {
            nvp1914_video_set_contrast(pdev->index, i, (u8)con);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_sat_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 sat;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &sat);

    down(&pdev->lock);

    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP1914_DEV_CH_MAX) {
            nvp1914_video_set_saturation(pdev->index, i, (u8)sat);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_hue_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 hue;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &hue);

    down(&pdev->lock);

    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP1914_DEV_CH_MAX) {
            nvp1914_video_set_hue(pdev->index, i, (u8)hue);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_sharp_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i, vin;
    u32 sharp;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %x\n", &vin, &sharp);

    down(&pdev->lock);

    for(i=0; i<NVP1914_DEV_CH_MAX; i++) {
        if(i == vin || vin >= NVP1914_DEV_CH_MAX) {
            nvp1914_video_set_sharpness(pdev->index, i, (u8)sharp);
        }
    }

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_volume_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 volume;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

    if(copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &volume);

    down(&pdev->lock);

    nvp1914_audio_set_volume(pdev->index, volume);

    up(&pdev->lock);

    return count;
}

static ssize_t nvp1914_proc_output_ch_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 ch, vin_idx, temp;
    char value_str[32] = {'\0'};
    struct seq_file *sfile = (struct seq_file *)file->private_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)sfile->private;

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
        vin_idx = nvp1914_ch_to_vin(pdev->index, ch);
        if (temp >= 8)
            vin_idx += 8;
    }

    down(&pdev->lock);

    nvp1914_audio_set_output_ch(pdev->index, vin_idx);

    up(&pdev->lock);

    return count;
}

static int nvp1914_proc_vmode_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_vmode_show, PDE(inode)->data);
}

static int nvp1914_proc_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_status_show, PDE(inode)->data);
}

static int nvp1914_proc_bri_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_bri_show, PDE(inode)->data);
}

static int nvp1914_proc_con_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_con_show, PDE(inode)->data);
}

static int nvp1914_proc_sat_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_sat_show, PDE(inode)->data);
}

static int nvp1914_proc_hue_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_hue_show, PDE(inode)->data);
}

static int nvp1914_proc_sharp_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_sharp_show, PDE(inode)->data);
}

static int nvp1914_proc_volume_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_volume_show, PDE(inode)->data);
}

static int nvp1914_proc_output_ch_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvp1914_proc_output_ch_show, PDE(inode)->data);
}

static struct file_operations nvp1914_proc_vmode_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_vmode_open,
    .write   = nvp1914_proc_vmode_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_status_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_status_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_bri_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_bri_open,
    .write  = nvp1914_proc_bri_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_con_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_con_open,
    .write  = nvp1914_proc_con_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_sat_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_sat_open,
    .write  = nvp1914_proc_sat_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_hue_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_hue_open,
    .write  = nvp1914_proc_hue_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_sharp_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_sharp_open,
    .write  = nvp1914_proc_sharp_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_volume_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_volume_open,
    .write  = nvp1914_proc_volume_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations nvp1914_proc_output_ch_ops = {
    .owner  = THIS_MODULE,
    .open   = nvp1914_proc_output_ch_open,
    .write  = nvp1914_proc_output_ch_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

void nvp1914_proc_remove(int id)
{
    if(id >= NVP1914_DEV_MAX)
        return;

    if(nvp1914_proc_root[id]) {
        if(nvp1914_proc_vmode[id]) {
            remove_proc_entry(nvp1914_proc_vmode[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_vmode[id] = NULL;
        }

        if(nvp1914_proc_status[id]) {
            remove_proc_entry(nvp1914_proc_status[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_status[id] = NULL;
        }

        if(nvp1914_proc_bri[id]) {
            remove_proc_entry(nvp1914_proc_bri[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_bri[id] = NULL;
        }

        if(nvp1914_proc_con[id]) {
            remove_proc_entry(nvp1914_proc_con[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_con[id] = NULL;
        }

        if(nvp1914_proc_sat[id]) {
            remove_proc_entry(nvp1914_proc_sat[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_sat[id] = NULL;
        }

        if(nvp1914_proc_hue[id]) {
            remove_proc_entry(nvp1914_proc_hue[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_hue[id] = NULL;
        }

        if(nvp1914_proc_sharp[id]) {
            remove_proc_entry(nvp1914_proc_sharp[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_sharp[id] = NULL;
        }

        if(nvp1914_proc_volume[id]) {
            remove_proc_entry(nvp1914_proc_volume[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_volume[id] = NULL;
        }

        if(nvp1914_proc_output_ch[id]) {
            remove_proc_entry(nvp1914_proc_output_ch[id]->name, nvp1914_proc_root[id]);
            nvp1914_proc_output_ch[id] = NULL;
        }

        remove_proc_entry(nvp1914_proc_root[id]->name, NULL);
        nvp1914_proc_root[id] = NULL;
    }
}

int nvp1914_proc_init(int id)
{
    int ret = 0;

    if(id >= NVP1914_DEV_MAX)
        return -1;

    /* root */
    nvp1914_proc_root[id] = create_proc_entry(nvp1914_dev[id].name, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(!nvp1914_proc_root[id]) {
        printk("create proc node '%s' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto end;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_root[id]->owner = THIS_MODULE;
#endif

    /* vmode */
    nvp1914_proc_vmode[id] = create_proc_entry("vmode", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_vmode[id]) {
        printk("create proc node '%s/vmode' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_vmode[id]->proc_fops = &nvp1914_proc_vmode_ops;
    nvp1914_proc_vmode[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_vmode[id]->owner     = THIS_MODULE;
#endif

    /* status */
    nvp1914_proc_status[id] = create_proc_entry("status", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_status[id]) {
        printk("create proc node '%s/status' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_status[id]->proc_fops = &nvp1914_proc_status_ops;
    nvp1914_proc_status[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_status[id]->owner     = THIS_MODULE;
#endif

    /* brightness */
    nvp1914_proc_bri[id] = create_proc_entry("brightness", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_bri[id]) {
        printk("create proc node '%s/brightness' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_bri[id]->proc_fops = &nvp1914_proc_bri_ops;
    nvp1914_proc_bri[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_bri[id]->owner     = THIS_MODULE;
#endif

    /* contrast */
    nvp1914_proc_con[id] = create_proc_entry("contrast", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_con[id]) {
        printk("create proc node '%s/contrast' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_con[id]->proc_fops = &nvp1914_proc_con_ops;
    nvp1914_proc_con[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_con[id]->owner     = THIS_MODULE;
#endif

    /* saturation */
    nvp1914_proc_sat[id] = create_proc_entry("saturation", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_sat[id]) {
        printk("create proc node '%s/saturation' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_sat[id]->proc_fops = &nvp1914_proc_sat_ops;
    nvp1914_proc_sat[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_sat[id]->owner     = THIS_MODULE;
#endif

    /* hue */
    nvp1914_proc_hue[id] = create_proc_entry("hue", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_hue[id]) {
        printk("create proc node '%s/hue' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_hue[id]->proc_fops = &nvp1914_proc_hue_ops;
    nvp1914_proc_hue[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_hue[id]->owner     = THIS_MODULE;
#endif

    /* sharpness */
    nvp1914_proc_sharp[id] = create_proc_entry("sharpness", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_sharp[id]) {
        printk("create proc node '%s/sharpness' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_sharp[id]->proc_fops = &nvp1914_proc_sharp_ops;
    nvp1914_proc_sharp[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_sharp[id]->owner     = THIS_MODULE;
#endif

    /* volume */
    nvp1914_proc_volume[id] = create_proc_entry("volume", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_volume[id]) {
        printk("create proc node '%s/volume' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_volume[id]->proc_fops = &nvp1914_proc_volume_ops;
    nvp1914_proc_volume[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_volume[id]->owner     = THIS_MODULE;
#endif

    /* output channel */
    nvp1914_proc_output_ch[id] = create_proc_entry("output_ch", S_IRUGO|S_IXUGO, nvp1914_proc_root[id]);
    if(!nvp1914_proc_output_ch[id]) {
        printk("create proc node '%s/output_ch' failed!\n", nvp1914_dev[id].name);
        ret = -EINVAL;
        goto err;
    }
    nvp1914_proc_output_ch[id]->proc_fops = &nvp1914_proc_output_ch_ops;
    nvp1914_proc_output_ch[id]->data      = (void *)&nvp1914_dev[id];
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    nvp1914_proc_output_ch[id]->owner     = THIS_MODULE;
#endif

end:
    return ret;

err:
    nvp1914_proc_remove(id);
    return ret;
}

static int nvp1914_miscdev_open(struct inode *inode, struct file *file)
{
    int i, ret = 0;
    struct nvp1914_dev_t *pdev = NULL;

    /* lookup device */
    for(i=0; i<dev_num; i++) {
        if(nvp1914_dev[i].miscdev.minor == iminor(inode)) {
            pdev = &nvp1914_dev[i];
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

static int nvp1914_miscdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long nvp1914_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int tmp, vin_idx, ch, ret = 0;
    struct nvp1914_ioc_data ioc_data;
    struct nvp1914_dev_t *pdev = (struct nvp1914_dev_t *)file->private_data;

    down(&pdev->lock);

    if(_IOC_TYPE(cmd) != NVP1914_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {
        case NVP1914_GET_NOVID:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            if(ioc_data.ch >= NVP1914_DEV_CH_MAX) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_status_get_novid(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = (tmp>>ioc_data.ch) & 0x1;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_GET_MODE:
            tmp = nvp1914_video_get_mode(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }
            ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_MODE:
            if(copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_mode(pdev->index, tmp);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_CONTRAST:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_video_get_contrast(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_CONTRAST:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_contrast(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_BRIGHTNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_video_get_brightness(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_BRIGHTNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_brightness(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_SATURATION:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_video_get_saturation(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_SATURATION:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_saturation(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_HUE:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_video_get_hue(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_HUE:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_hue(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_VOL:
            tmp = nvp1914_audio_get_mute(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_VOL:
            if(copy_from_user(&tmp, (void __user *)arg, sizeof(tmp))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_audio_set_volume(pdev->index, tmp);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_OUT_CH:
            tmp = nvp1914_audio_get_output_ch(pdev->index);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }
            vin_idx = tmp;
            if (vin_idx >= 8 && vin_idx <= 15) {
                vin_idx -= 8;
                ch = nvp1914_vin_to_ch(pdev->index, vin_idx);
                ch += 8;
            }
            else if (vin_idx >= 0 && vin_idx <= 7)
                ch = nvp1914_vin_to_ch(pdev->index, vin_idx);
            else
                ch = vin_idx;

            tmp = ch;

            ret = (copy_to_user((void __user *)arg, &tmp, sizeof(tmp))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_OUT_CH:
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
                vin_idx = nvp1914_ch_to_vin(pdev->index, tmp);
                if (ch >= 8)
                    vin_idx += 8;
            }
            tmp = vin_idx;

            ret = nvp1914_audio_set_output_ch(pdev->index, tmp);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }
            break;

        case NVP1914_GET_SHARPNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            tmp = nvp1914_video_get_sharpness(pdev->index, ioc_data.ch);
            if(tmp < 0) {
                ret = -EFAULT;
                goto exit;
            }

            ioc_data.data = tmp;
            ret = (copy_to_user((void __user *)arg, &ioc_data, sizeof(ioc_data))) ? (-EFAULT) : 0;
            break;

        case NVP1914_SET_SHARPNESS:
            if(copy_from_user(&ioc_data, (void __user *)arg, sizeof(ioc_data))) {
                ret = -EFAULT;
                goto exit;
            }

            ret = nvp1914_video_set_sharpness(pdev->index, ioc_data.ch, (u8)ioc_data.data);
            if(ret < 0) {
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

static struct file_operations nvp1914_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = nvp1914_miscdev_open,
    .release        = nvp1914_miscdev_release,
    .unlocked_ioctl = nvp1914_miscdev_ioctl,
};

int nvp1914_miscdev_init(int id)
{
    int ret;

    if(id >= NVP1914_DEV_MAX)
        return -1;

    /* clear */
    memset(&nvp1914_dev[id].miscdev, 0, sizeof(struct miscdevice));

    /* create device node */
    nvp1914_dev[id].miscdev.name  = nvp1914_dev[id].name;
    nvp1914_dev[id].miscdev.minor = MISC_DYNAMIC_MINOR;
    nvp1914_dev[id].miscdev.fops  = (struct file_operations *)&nvp1914_miscdev_fops;
    ret = misc_register(&nvp1914_dev[id].miscdev);
    if(ret) {
        printk("create %s misc device node failed!\n", nvp1914_dev[id].name);
        nvp1914_dev[id].miscdev.name = 0;
        goto exit;
    }

exit:
    return ret;
}

int nvp1914_device_init(int id)
{
    int ret = 0;

    if (!init)
        return 0;

    if(id >= NVP1914_DEV_MAX)
        return -1;

    /*====================== video init ========================= */
    ret = nvp1914_video_set_mode(id, vmode[id]);
    if(ret < 0)
        goto exit;

    /*====================== audio init ========================= */
    ret = nvp1914_audio_set_mode(id, vmode[id], sample_size, sample_rate);
    if(ret < 0)
        goto exit;

    /* disable all SD video output */
    ret = nvp1914_i2c_write(id, 0xFF, 0x1); // switch to bank#1
    if(ret < 0){
        printk("Can't disable SD video output at Dev#%d!\n", id);
        goto exit;
    }
    ret = nvp1914_i2c_write(id, 0xCA, 0x0); // disable VDO
    if(ret < 0){
        printk("Can't disable SD video output at Dev#%d!\n", id);
        goto exit;
    }
    ret = nvp1914_i2c_write(id, 0xD4, 0x0); // disable MPP VCLK output
    if(ret < 0){
        printk("Can't disable SD video output at Dev#%d!\n", id);
        goto exit;
    }

exit:
    return ret;
}
