/**
 * @file tp2824_lib.c
 * TechPoint TP2824 4CH HD-TVI Video Decoder Library
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "techpoint/tp2824.h"   ///< from module/include/front_end/hdtvi

extern int audio_chnum;

#define DEFAULT_2CH_FREERUN_VFMT TP2824_VFMT_720P50 ///< Frame rate of free-run output should be doubled during 2-CH dual edge mode

static int is_v2_cam[TP2824_DEV_MAX*TP2824_DEV_CH_MAX] = {[0 ... (TP2824_DEV_MAX*TP2824_DEV_CH_MAX - 1)] = 0}; ///< Check if video input is TVI v2 camera
static int pre_fmt[TP2824_DEV_MAX*TP2824_DEV_CH_MAX] = {[0 ... (TP2824_DEV_MAX*TP2824_DEV_CH_MAX - 1)] = -1}; ///< Previous video input format, -1: Unknown
static int vin_stable[TP2824_DEV_MAX*TP2824_DEV_CH_MAX] = {[0 ... (TP2824_DEV_MAX*TP2824_DEV_CH_MAX - 1)] = -1}; ///< Video input status , 0: unstable, 1: stable, -1: debounce design for v2 camera

static struct tp2824_video_fmt_t tp2824_video_fmt_info[TP2824_VFMT_MAX] = {
    /* Idx                        Width   Height  Prog    FPS */
    {  TP2824_VFMT_720P60,         1280,   720,    1,      60  },  ///< 720P @60
    {  TP2824_VFMT_720P50,         1280,   720,    1,      50  },  ///< 720P @50
    {  TP2824_VFMT_1080P30,        1920,   1080,   1,      30  },  ///< 1080P@30
    {  TP2824_VFMT_1080P25,        1920,   1080,   1,      25  },  ///< 1080P@25
    {  TP2824_VFMT_720P30,         1280,   720,    1,      30  },  ///< 720P @30
    {  TP2824_VFMT_720P25,         1280,   720,    1,      25  },  ///< 720P @25
    {  TP2824_VFMT_960H30,         1920,   480,    0,      30  },  ///< SD 960H NTSC
    {  TP2824_VFMT_960H25,         1920,   576,    0,      25  },  ///< SD 960H PAL
    {  TP2824_VFMT_HALF_1080P30,    960,   1080,   1,      30  },  ///< Half-1080P@30
    {  TP2824_VFMT_HALF_1080P25,    960,   1080,   1,      25  },  ///< Half-1080P@25
    {  TP2824_VFMT_HALF_720P30,     640,   720,    1,      30  },  ///< Half-720P @30
    {  TP2824_VFMT_HALF_720P25,     640,   720,    1,      25  },  ///< Half-720P @25
};

const unsigned char SYS_AND[5] =  {0xfe, 0xfd, 0xfb, 0xf7, 0xf0};
const unsigned char SYS_OR[5]  =  {0x01, 0x02, 0x04, 0x08, 0x0f};

static const unsigned char TP2824_720P60[] = {
    0x0d, 0x10,
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x06,
    0x1d, 0x72,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_720P50[] = {
    0x0d, 0x10,
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x07,
    0x1d, 0xbc,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_720P30[] = {
    0x0d, 0x10,
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0c,
    0x1d, 0xe4,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_720P25[] = {
    0x0d, 0x10,
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0f,
    0x1d, 0x78,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_1080P30[] = {
    0x0d, 0x10,
    0x15, 0x03,
    0x16, 0xd3,
    0x17, 0x80,
    0x18, 0x29,
    0x19, 0x38,
    0x1a, 0x47,
    0x1b, 0x00,
    0x1c, 0x08,
    0x1d, 0x98,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_1080P25[] = {
    0x0d, 0x10,
    0x15, 0x03,
    0x16, 0xd3,
    0x17, 0x80,
    0x18, 0x29,
    0x19, 0x38,
    0x1a, 0x47,
    0x1b, 0x00,
    0x1c, 0x0a,
    0x1d, 0x50,
    0x2d, 0x30,
    0x2e, 0x70,
    0x30, 0x48,
    0x31, 0xbb,
    0x32, 0x2e,
    0x33, 0x90,
    0x39, 0x30
};

static const unsigned char TP2824_960H30[] = {
    0x02, 0xce,
    0x0d, 0x10,
    0x15, 0x13,
    0x16, 0x4e,
    0x17, 0xbc,
    0x18, 0x15,
    0x19, 0xf0,
    0x1a, 0x07,
    0x1c, 0x09,
    0x1d, 0x38,
    0x20, 0xa0,
    0x26, 0x12,
    0x2d, 0x68,
    0x2e, 0x5e,
    0x30, 0x62,
    0x31, 0xbb,
    0x32, 0x96,
    0x33, 0xc0,
    0x35, 0x25,
    0x39, 0x84
};

static const unsigned char TP2824_960H25[] = {
    0x02, 0xce,
    0x0d, 0x11,
    0x15, 0x13,
    0x16, 0x5f,
    0x17, 0xbc,
    0x18, 0x17,
    0x19, 0x20,
    0x1a, 0x17,
    0x1c, 0x09,
    0x1d, 0x48,
    0x20, 0xb0,
    0x26, 0x02,
    0x2d, 0x60,
    0x2e, 0x5e,
    0x30, 0x7a,
    0x31, 0x4a,
    0x32, 0x4d,
    0x33, 0xf0,
    0x35, 0x25,
    0x39, 0x84
};

static int tp2824_comm_init(int id, TP2824_VOUT_MODE_T vout_mode)
{
    int ret;

    if(id >= TP2824_DEV_MAX || vout_mode >= TP2824_VOUT_MODE_MAX)
        return -1;

    if(vout_mode == TP2824_VOUT_MODE_2CH_DUAL_EDGE) {
        ret = tp2824_i2c_write(id, 0xF5, 0x0F); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF6, 0x10); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF7, 0x32); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF8, 0x10); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF9, 0x32); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xFA, 0x22); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xFB, 0x22); if(ret < 0) goto exit;
        printk("TP2824#%d enters 2-CH dual edge mode!\n", id);
    }
    else {
        ret = tp2824_i2c_write(id, 0xF5, 0x00); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF6, 0x00); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF7, 0x11); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF8, 0x22); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xF9, 0x33); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xFA, 0x88); if(ret < 0) goto exit;
        ret = tp2824_i2c_write(id, 0xFB, 0x88); if(ret < 0) goto exit;
        printk("TP2824#%d enters 1-CH bypass mode!\n", id);
    }

    ret = tp2824_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x02, 0xCA); if(ret < 0) goto exit; ///< Set MD656 to 1 -> BT.656 format output; GMEN to 1 -> C first
    ret = tp2824_i2c_write(id, 0x07, 0xC0); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x0B, 0xC0); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x3A, 0x70); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x26, 0x01); if(ret < 0) goto exit;

    ret = tp2824_i2c_write(id, 0x30, 0x48); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x31, 0xBB); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x32, 0x2E); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x33, 0x90);	if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x4D, 0x0F);	if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x4E, 0x0F);	if(ret < 0) goto exit;
    ///< Channel ID
    ret = tp2824_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x34, 0x10); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x40, 0x01); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x34, 0x11); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x40, 0x02); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x34, 0x10); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x40, 0x03); if(ret < 0) goto exit;
    ret = tp2824_i2c_write(id, 0x34, 0x11); if(ret < 0) goto exit;

exit:
    return ret;
}

int tp2824_get_dev_id(int id)
{
    if(id >= TP2824_DEV_MAX)
        return 0;

    return tp2824_i2c_read(id, 0xff);
}
EXPORT_SYMBOL(tp2824_get_dev_id);

int tp2824_video_get_video_output_format(int id, int ch, struct tp2824_video_fmt_t *vfmt)
{
    int ret = 0;
    int fmt_idx;
    int npxl_value;

    if(id >= TP2824_DEV_MAX || ch >= TP2824_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2824_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    npxl_value = (tp2824_i2c_read(id, 0x1c)<<8) | tp2824_i2c_read(id, 0x1d);

    switch(npxl_value) {
        case 0x0672:
            if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2824_VFMT_720P30;
            else {
                if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                    fmt_idx = TP2824_VFMT_HALF_720P30;
                else
                    fmt_idx = TP2824_VFMT_720P60;
            }
            break;
        case 0x07BC:
            if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2824_VFMT_720P25;
            else {
                if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                    fmt_idx = TP2824_VFMT_HALF_720P25;
                else
                    fmt_idx = TP2824_VFMT_720P50;
            }
            break;
        case 0x0CE4:
            if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2824_VFMT_HALF_720P30;
            else
                fmt_idx = TP2824_VFMT_720P30;
            break;
        case 0x0F78:
            if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2824_VFMT_HALF_720P25;
            else
                fmt_idx = TP2824_VFMT_720P25;
            break;
        case 0x0898:
            if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2824_VFMT_HALF_1080P30;
            else
                fmt_idx = TP2824_VFMT_1080P30;
            break;
        case 0x0A50:
            if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2824_VFMT_HALF_1080P25;
            else
                fmt_idx = TP2824_VFMT_1080P25;
            break;
        case 0x0938:
            fmt_idx = TP2824_VFMT_960H30;
            break;
        case 0x0948:
            fmt_idx = TP2824_VFMT_960H25;
            break;
        default:
            ret = -1;
            goto exit;
    }

    vfmt->fmt_idx    = tp2824_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2824_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2824_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2824_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2824_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_video_get_video_output_format);

int tp2824_video_set_video_output_format(int id, int ch, TP2824_VFMT_T vfmt, TP2824_VOUT_FORMAT_T vout_fmt)
{
    int i, ret = 0;
    u8 tmp;

    if(id >= TP2824_DEV_MAX || ch > TP2824_DEV_CH_MAX || vfmt >= TP2824_VFMT_MAX || vout_fmt >= TP2824_VOUT_FORMAT_MAX)
        return -1;

    ret = tp2824_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page, 4 for all channel
    if(ret < 0)
        goto exit;

    if((is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1) && (vfmt != TP2824_VFMT_960H25) && (vfmt != TP2824_VFMT_960H30)) ///< TVI v2 camera video output setting
        vfmt = (vfmt==TP2824_VFMT_720P50)?TP2824_VFMT_720P50:TP2824_VFMT_720P60;

    if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_1CH_BYPASS) { ///< Update per port VCLK setting only during 1-CH bypass mode 
        if((is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1) || (vfmt == TP2824_VFMT_960H30) || (vfmt == TP2824_VFMT_960H25)) { ///< TVI v2 camera or 960H clock setting
            tmp = tp2824_i2c_read(id, 0xF5);
            tmp |= SYS_OR[ch];
            ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
            tmp = tp2824_i2c_read(id, (ch/2)?0xFB:0xFA);
            tmp &= ch%2?0x8F:0xF8;
            tmp |= ch%2?0x10:0x01;
            ret = tp2824_i2c_write(id, (ch/2)?0xFB:0xFA, tmp); if(ret < 0) goto exit;
        }
        else { ///< TVI v1 camera clock setting
            tmp = tp2824_i2c_read(id, 0xF5);
            tmp &= SYS_AND[ch];
            ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
            tmp = tp2824_i2c_read(id, 0x35);
            tmp &= 0xDF; ///< Set FSL to 0, status reflects 148.5 MHz system clock
            ret = tp2824_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            tmp = tp2824_i2c_read(id, (ch/2)?0xFB:0xFA);
            tmp &= ch%2?0x8F:0xF8;
            tmp &= ~(ch%2?0x10:0x01);
            ret = tp2824_i2c_write(id, (ch/2)?0xFB:0xFA, tmp); if(ret < 0) goto exit;
        }
        ret = tp2824_i2c_write(id, 0x26, 0x02); if(ret < 0) goto exit; ///< If Reg@0x26 set to 0x01, the video will be flickering
    }
    else { ///< TP2824_VOUT_MODE_2CH_DUAL_EDGE
        if((is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1) || (vfmt == TP2824_VFMT_960H30) || (vfmt == TP2824_VFMT_960H25)) {
            tmp = tp2824_i2c_read(id, 0xF5);
            tmp |= SYS_OR[ch];
            ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
            if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1) {
                tmp = tp2824_i2c_read(id, 0x35);
                tmp &= 0xBF; ///< Set DS2 to 0, disable half output mode for v2 cameras
                ret = tp2824_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
                ///< Data Set for v2 cameras
                ret = tp2824_i2c_write(id, 0x20, 0x60); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x26, 0x02); if(ret < 0) goto exit;
                if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] != 1) ///< Color setting for SD mode
                    ret = tp2824_i2c_write(id, 0x2B, 0x70); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x2D, 0x30); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x2E, 0x70); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x30, 0x48); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x31, 0xBB); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x32, 0x2E); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x33, 0x90); if(ret < 0) goto exit;
                ret = tp2824_i2c_write(id, 0x39, 0x88); if(ret < 0) goto exit;
            }
        }
        else {
            if(tp2824_status_get_video_loss(id, ch) == 0) { ///< video present of v1 camera input
                tmp = tp2824_i2c_read(id, 0xF5);
                tmp &= SYS_AND[ch];
                ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;

                tmp = tp2824_i2c_read(id, 0x35);
                tmp |= 0x40; ///< Set DS2 to 1, enable half output mode for v1 cameras
                ret = tp2824_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            }
            else { ///< video lost
                tmp = tp2824_i2c_read(id, 0xF5);
                tmp |= SYS_OR[ch];
                ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;

                tmp = tp2824_i2c_read(id, 0x35);
                tmp &= 0xBF; ///< Set DS2 to 0, disable half output mode for v2 cameras
                ret = tp2824_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            }
            ///< Data Set for v1 cameras
            ret = tp2824_i2c_write(id, 0x20, 0x60); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x26, 0x02); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x2B, 0x4A); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x2D, 0x30); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x2E, 0x70); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x30, 0x48); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x31, 0xBB); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x32, 0x2E); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x33, 0x90); if(ret < 0) goto exit;
            ret = tp2824_i2c_write(id, 0x39, 0x8C); if(ret < 0) goto exit;
        }
    }

    if(pre_fmt[id*TP2824_DEV_CH_MAX+ch] != vfmt) { ///< Update output setting only when video format is changed

        tmp = tp2824_i2c_read(id, 0x02);

        ret = tp2824_i2c_write(id, 0x2A, 0x30); if(ret < 0) goto exit;

        switch(vfmt) {
            case TP2824_VFMT_1080P25:
                for(i=0; i<ARRAY_SIZE(TP2824_1080P25); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_1080P25[i], TP2824_1080P25[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_1080P30:
                for(i=0; i<ARRAY_SIZE(TP2824_1080P30); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_1080P30[i], TP2824_1080P30[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_720P25:
                for(i=0; i<ARRAY_SIZE(TP2824_720P25); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_720P25[i], TP2824_720P25[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                tmp |= 0x02;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_720P30:
                for(i=0; i<ARRAY_SIZE(TP2824_720P30); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_720P30[i], TP2824_720P30[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                tmp |= 0x02;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_720P50:
                for(i=0; i<ARRAY_SIZE(TP2824_720P50); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_720P50[i], TP2824_720P50[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                tmp |= 0x02;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_720P60:
                for(i=0; i<ARRAY_SIZE(TP2824_720P60); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_720P60[i], TP2824_720P60[i+1]); if(ret < 0) goto exit;
                }
                tmp &= 0xF8;
                tmp |= 0x02;
                ret = tp2824_i2c_write(id, 0x02, tmp); if(ret < 0) goto exit;
                break;
            case TP2824_VFMT_960H30:
                for(i=0; i<ARRAY_SIZE(TP2824_960H30); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_960H30[i], TP2824_960H30[i+1]); if(ret < 0) goto exit;
                }
                break;
            case TP2824_VFMT_960H25:
                for(i=0; i<ARRAY_SIZE(TP2824_960H25); i+=2) {
                    ret = tp2824_i2c_write(id, TP2824_960H25[i], TP2824_960H25[i+1]); if(ret < 0) goto exit;
                }
                break;
            default:
                ret = -1;
                break;
        }
        pre_fmt[id*TP2824_DEV_CH_MAX+ch] = vfmt;
    }

    if(tp2824_status_get_video_loss(id, ch) == 0) { ///< Set video input as stable only when video present
        if(vin_stable[id*TP2824_DEV_CH_MAX+ch] == -1)
            vin_stable[id*TP2824_DEV_CH_MAX+ch] = 0;
        else if(vin_stable[id*TP2824_DEV_CH_MAX+ch] == 0)
            vin_stable[id*TP2824_DEV_CH_MAX+ch] = 1;
        if(vfmt == TP2824_VFMT_960H30)
           ret = tp2824_i2c_write(id, 0x26, 0x12); if(ret < 0) goto exit;
        else
           ret = tp2824_i2c_write(id, 0x26, 0x02); if(ret < 0) goto exit;
    } else { ///< Keep the VDLOSS false alarm away when current video input isn't present
        ret = tp2824_i2c_write(id, 0x26, 0x01); if(ret < 0) goto exit;
    }

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_video_set_video_output_format);

int tp2824_video_set_pattern_output(int id, int ch, TP2824_VFMT_T vfmt, TP2824_VOUT_FORMAT_T vout_fmt, int enable)
{
    int ret = 0;
    u8 tmp;

    ret = tp2824_i2c_write(id, 0x40, ch); if(ret < 0) goto exit;

    if(enable == 0) { ///< Normal video output
        if((vfmt != TP2824_VFMT_960H25) && (vfmt != TP2824_VFMT_960H30)) {
            tmp = tp2824_i2c_read(id, 0x35) & 0xDF; ///< Set FSL to 0
            ret = tp2824_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
        }
        tp2824_video_set_video_output_format(id, ch, vfmt, vout_fmt);
        ret = tp2824_i2c_write(id, 0x2A, 0x30); if(ret < 0) goto exit;
    }
    else { ///< Blue screen pattern output
        tp2824_video_set_video_output_format(id, ch, TP2824_VFMT_960H25, vout_fmt);
        ret = tp2824_i2c_write(id, 0x2A, 0x3C); if(ret < 0) goto exit;
    }

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_video_set_pattern_output);

int tp2824_status_get_video_loss(int id, int ch)
{
    int ret;
    int vlos = 1;
    u8 status, tmp;

    if(id >= TP2824_DEV_MAX || ch >= TP2824_DEV_CH_MAX)
        return 1;

    ret = tp2824_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    status = tp2824_i2c_read(id, 0x01);

    if(tp2824_i2c_read(id, 0x2f) == 0x08) {
        tmp = tp2824_i2c_read(id, 0x04); 
        if(tmp >= 0x30)
            status |= 0x80;
    }

    if(!(status & 0x80)) ///< Video Present
        vlos = 0;
    else
        vlos = 1;

    if(vlos) {
        is_v2_cam[id*TP2824_DEV_CH_MAX+ch] = 0; ///< Clear the flag of V2 camera when video input was lost
        vin_stable[id*TP2824_DEV_CH_MAX+ch] = -1; ///< Clear the flag of video input status
    }
exit:
    return vlos;
}
EXPORT_SYMBOL(tp2824_status_get_video_loss);

int tp2824_status_get_video_input_format(int id, int ch, struct tp2824_video_fmt_t *vfmt)
{
    int ret = 0;
    u8  cvstd, sywd, tmp;
    int fmt_idx;

    if(id >= TP2824_DEV_MAX || ch >= TP2824_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2824_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    /* Set SYSCLK to 148.5 MHz for v1/v2 camera identification */
    if((tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE) && (vin_stable[id*TP2824_DEV_CH_MAX+ch] <= 0)) {
        tmp = tp2824_i2c_read(id, 0xF5);
        tmp &= SYS_AND[ch];
        ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
    }

    cvstd = tp2824_i2c_read(id, 0x03);
    sywd = (cvstd & 0x08)?1:0;
    cvstd &= 0x07;

    if(cvstd < TP2824_VFMT_MAX) {
        if(sywd && (TP2824_VFMT_720P25 == cvstd || TP2824_VFMT_720P30 == cvstd)) { ///< See if v2 camera was connected
            if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 0) {
                is_v2_cam[id*TP2824_DEV_CH_MAX+ch] = sywd;
                if(is_v2_cam[id*TP2824_DEV_CH_MAX+ch] == 1)
                    printk("TP2824#%d-CH#%d: TVI V2 camera detected!\n", id, ch);
            }
        }

        fmt_idx = tp2824_i2c_read(id, 0x03) & 0x07;
        if(fmt_idx >= TP2824_VFMT_MAX) {
            ret = -1;
            goto exit;
        }else if(fmt_idx == TP2824_VFMT_960H30) { ///< SD input
            if(tp2824_i2c_read(id, 0x01)&0x04)
                fmt_idx = TP2824_VFMT_960H25;
            else
                fmt_idx = TP2824_VFMT_960H30;
        }
    }
    else { ///< Unknown video input format
        ret = -1;
        goto exit;
    }

    /* Set SYSCLK to 74.25 MHz for 2-CH dual edge output */
    if(tp2824_get_vout_mode(id) == TP2824_VOUT_MODE_2CH_DUAL_EDGE && (vin_stable[id*TP2824_DEV_CH_MAX+ch] <= 0)) {
        tmp = tp2824_i2c_read(id, 0xF5);
        tmp |= SYS_OR[ch];
        ret = tp2824_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
    }

    vfmt->fmt_idx    = tp2824_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2824_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2824_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2824_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2824_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_status_get_video_input_format);

int tp2824_video_init(int id, TP2824_VOUT_FORMAT_T vout_fmt, TP2824_VOUT_MODE_T vout_mode)
{
    u8  tmp;
    int ret;

    if(id >= TP2824_DEV_MAX || vout_fmt >= TP2824_VOUT_FORMAT_MAX || vout_mode >= TP2824_VOUT_MODE_MAX)
        return -1;

    /* get and check chip device id */
    if(tp2824_get_dev_id(id) != TP2824_DEV_ID) {
        printk("TP2824#%d: unknown device ID!\n", id);
        return -1;
    }

    ret = tp2824_i2c_write(id, 0x40, 0x04);   ///< Switch to write all page
    if(ret < 0)
        goto exit;

    /* Video Common */
    if(vout_fmt == TP2824_VOUT_FORMAT_BT1120) {
        ret = -1;
        printk("TP2824#%d driver doesn't support BT1120 vout format!\n", id);
        goto exit;
    }
    else {
        ret = tp2824_comm_init(id, vout_mode);
        if(ret < 0)
            goto exit;
    }

    /* Video Standard */
    if(vout_mode == TP2824_VOUT_MODE_2CH_DUAL_EDGE)
        ret = tp2824_video_set_video_output_format(id, 4, DEFAULT_2CH_FREERUN_VFMT, vout_fmt); ///< ch=4 for config all channel
    else
        ret = tp2824_video_set_video_output_format(id, 4, TP2824_VFMT_720P25, vout_fmt); ///< ch=4 for config all channel
    if(ret < 0)
        goto exit;

	/* PLL2 software reset */
	tmp = tp2824_i2c_read(id, 0x44);
	ret = tp2824_i2c_write(id, 0x44, (tmp | 0x40)); if(ret < 0) goto exit;
	msleep(10);
	ret = tp2824_i2c_write(id, 0x44, tmp); if(ret < 0) goto exit;

	/* ADC reset */
	ret = tp2824_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
	ret = tp2824_i2c_write(id, 0x3B, 0x33); if(ret < 0) goto exit;
	ret = tp2824_i2c_write(id, 0x3B, 0x03); if(ret < 0) goto exit;

	/* device software reset */
	tmp = tp2824_i2c_read(id, 0x06);
	ret = tp2824_i2c_write(id, 0x06, (tmp | 0x80)); if(ret < 0) goto exit;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_video_init);

static int tp2824_audio_init(int id, int ch_num)
{
    int ret = 0;
    int cascade  = 0;
    unsigned int tmp;
    
    if(ch_num < 0)
        return -1;

    if (ch_num > 4)
        cascade = 1;
            
    if(id == 0)
    {   
	    tmp = tp2824_i2c_read(id, 0x40);
	    tmp |= 0x40;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
	    
        if(ch_num == 2)
        {
          ret = tp2824_i2c_write(id, 0x00, 0x01); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x08, 0x02); if(ret < 0) goto exit;
        }
			
        if(ch_num == 4)
        {
          ret = tp2824_i2c_write(id, 0x00, 0x02); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x01, 0x01); if(ret < 0) goto exit;	
          ret = tp2824_i2c_write(id, 0x08, 0x04); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x09, 0x03); if(ret < 0) goto exit;
        }
			
        if(ch_num == 8)
        {
          ret = tp2824_i2c_write(id, 0x00, 0x04); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x01, 0x03); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x02, 0x02); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x03, 0x01); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x08, 0x08); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x09, 0x07); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0A, 0x06); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0B, 0x05); if(ret < 0) goto exit;
        }
			
        if(ch_num == 16)
        {
          ret = tp2824_i2c_write(id, 0x07, 0x01); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x06, 0x02); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x05, 0x03); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x04, 0x04); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x03, 0x05); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x02, 0x06); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x01, 0x07); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x00, 0x08); if(ret < 0) goto exit;
	    
          ret = tp2824_i2c_write(id, 0x0F, 0x09); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0E, 0x0A); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0D, 0x0B); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0C, 0x0C); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0B, 0x0D); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x0A, 0x0E); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x09, 0x0F); if(ret < 0) goto exit;
          ret = tp2824_i2c_write(id, 0x08, 0x10); if(ret < 0) goto exit;
        }
	    
	    ret = tp2824_i2c_write(id, 0x17, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x18, 0x20); if(ret < 0) goto exit;
	    if(cascade)
	    {
            ret = tp2824_i2c_write(id, 0x19, 0x1F); if(ret < 0) goto exit;
	    }
	    else
	    {
            ret = tp2824_i2c_write(id, 0x19, 0x0F); if(ret < 0) goto exit;
	    }	      
	    ret = tp2824_i2c_write(id, 0x1A, 0x15); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1B, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x08); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x00); if(ret < 0) goto exit;
	    
	    ret = tp2824_i2c_write(id, 0x37, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x38, 0x38); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3E, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3D, 0x01); if(ret < 0) goto exit;
	    tmp &= 0xBF;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
    }
    
    if((id == 1) && cascade)
    {   
	    tmp = tp2824_i2c_read(id, 0x40);
	    tmp |= 0x40;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x00, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x01, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x02, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x03, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x04, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x05, 0x02); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x06, 0x03); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x07, 0x04); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x08, 0x05); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x09, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0A, 0x07); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0B, 0x08); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0C, 0x09); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0D, 0x0A); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0E, 0x0B); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0F, 0x0C); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x17, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x18, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x19, 0x1F); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1A, 0x15); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1B, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x08); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x00); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x37, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x38, 0x38); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3E, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3D, 0x01); if(ret < 0) goto exit;
	    tmp &= 0xBF;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
    }
    
    if((id == 2) && cascade)
    {   
	    tmp = tp2824_i2c_read(id, 0x40);
	    tmp |= 0x40;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x00, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x01, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x02, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x03, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x04, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x05, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x06, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x07, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x08, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x09, 0x02); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0A, 0x03); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0B, 0x04); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0C, 0x05); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0D, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0E, 0x07); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0F, 0x08); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x17, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x18, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x19, 0x1F); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1A, 0x15); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1B, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x08); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x00); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x37, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x38, 0x38); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3E, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3D, 0x01); if(ret < 0) goto exit;
	    tmp &= 0xBF;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
    }
    
    if((id == 3) && cascade)
    {   
	    tmp = tp2824_i2c_read(id, 0x40);
	    tmp |= 0x40;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x00, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x01, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x02, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x03, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x04, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x05, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x06, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x07, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x08, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x09, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0A, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0B, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0C, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0D, 0x02); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0E, 0x03); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x0F, 0x04); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x17, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x18, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x19, 0x1F); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1A, 0x15); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1B, 0x01); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3C, 0x00); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x08); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x1D, 0x00); if(ret < 0) goto exit;
	
	    ret = tp2824_i2c_write(id, 0x37, 0x20); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x38, 0x38); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3E, 0x06); if(ret < 0) goto exit;
	    ret = tp2824_i2c_write(id, 0x3D, 0x01); if(ret < 0) goto exit;
	    tmp &= 0xBF;
	    ret = tp2824_i2c_write(id, 0x40, tmp); if(ret < 0) goto exit;
    }
   
exit:
    return ret;
}

static int tp2824_audio_set_sample_rate(int id, TP2824_SAMPLERATE_T samplerate)
{
    int pagesave, data, umask;
    int ret;

    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;          ///< select apage 1
    
    data = tp2824_i2c_read(id, 0x18);
    umask = (unsigned char)(AUDIO_SAMPLERATE_16K);
    data &= ~umask;
    data |= (samplerate);
    ret = tp2824_i2c_write(id, 0x18, data); if(ret < 0) goto exit;
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore apage setting

exit:
    return ret;
}

static int tp2824_audio_set_sample_size(int id, TP2824_SAMPLESIZE_T samplesize)
{
    int pagesave, data, umask;
    int ret;

    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;          ///< select apage 1
    
    data = tp2824_i2c_read(id, 0x17);
    umask = (unsigned char)(AUDIO_BITS_8B << 2);
    data &= ~umask;
    data |= (samplesize << 2);
    ret = tp2824_i2c_write(id, 0x17, data); if(ret < 0) goto exit;

    data = tp2824_i2c_read(id, 0x1b);
    umask = (unsigned char)(AUDIO_BITS_8B << 6);
    data &= ~umask;
    data |= (samplesize << 6);
    ret = tp2824_i2c_write(id, 0x1b, data); if(ret < 0) goto exit;
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting

exit:
    return ret;
}

int tp2824_audio_set_mute(int id, int on)
{
    int pagesave;
    int aogain;
    int ret;

    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave);  if(ret < 0) goto exit;             ///< select apage 1
    
    aogain = tp2824_i2c_read(id, 0x38);
    aogain &= 0xf0;
    ret = tp2824_i2c_write(id, 0x38, on ? aogain : (aogain | 0x08)); if(ret < 0) goto exit;  ///< Audio No MIXed out - Gain 0|1 (AOGAIN=0|1)
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_set_mute);

int tp2824_audio_get_mute(int id)
{
    int pagesave, aogain;
    int ret;

    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave);  if(ret < 0) goto exit;             ///< select apage 1
    
    aogain = tp2824_i2c_read(id, 0x38);
    aogain = ((aogain & 0x0f) >> 4);
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting

    return aogain;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_get_mute);

int tp2824_audio_set_volume(int id, int volume)
{
    int pagesave;
    int ret;
    
    if(volume > 15 || volume < 0)
      return -1;
			    
    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave);  if(ret < 0) goto exit;             ///< select apage 1
    
    ret = tp2824_i2c_read(id, 0x1f);
    ret &= 0xf0;
    volume &= 0x0f;
    ret = tp2824_i2c_write(id, 0x1f, (volume | ret)); if(ret < 0) goto exit;     ///< AOGAIN setting
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_set_volume);

int tp2824_audio_get_volume(int id)
{
    int pagesave, volume;
    int ret;
			    
    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave);  if(ret < 0) goto exit;             ///< select apage 1
    
    volume = tp2824_i2c_read(id, 0x1f);
    volume &= 0x0f;
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting
    
    return volume;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_get_volume);

int tp2824_audio_set_play_ch(int id, unsigned char ch)
{
    int pagesave;
    int ret;

    pagesave = tp2824_i2c_read(id, 0x40);      ///< save page
    pagesave |= 0x40;
    ret = tp2824_i2c_write(id, 0x40, pagesave);  if(ret < 0) goto exit;             ///< select apage 1
    
    ret = tp2824_i2c_write(id, 0x1a, ch); if(ret < 0) goto exit;
    
    pagesave &= 0xBF;
    ret = tp2824_i2c_write(id, 0x40, pagesave); if(ret < 0) goto exit;      ///< restore page setting

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_set_play_ch);

int tp2824_audio_set_mode(int id, TP2824_VOUT_MODE_T mode, TP2824_SAMPLESIZE_T samplesize, TP2824_SAMPLERATE_T samplerate)
{
    int ch_num = 0;
    int ret;

    ch_num = audio_chnum;
    if(ch_num < 0)
        return -1;

    /* tp2824 audio initialize */
    ret = tp2824_audio_init(id, ch_num); if(ret < 0) goto exit;
    /* set audio sample rate */
    ret = tp2824_audio_set_sample_rate(id, samplerate); if(ret < 0) goto exit;
    /* set audio sample size */
    ret = tp2824_audio_set_sample_size(id, samplesize); if(ret < 0) goto exit;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2824_audio_set_mode);

