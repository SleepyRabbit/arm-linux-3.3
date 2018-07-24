/**
 * @file tp2822_lib.c
 * TechPoint TP2822 4CH HD-TVI Video Decoder Library
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "techpoint/tp2822.h"   ///< from module/include/front_end/hdtvi

#define DEFAULT_2CH_FREERUN_VFMT TP2822_VFMT_720P50 ///< Frame rate of free-run output should be doubled during 2-CH dual edge mode

static int is_v2_cam[TP2822_DEV_MAX*TP2822_DEV_CH_MAX] = {[0 ... (TP2822_DEV_MAX*TP2822_DEV_CH_MAX - 1)] = 0}; ///< Check if video input is TVI v2 camera
static int pre_fmt[TP2822_DEV_MAX*TP2822_DEV_CH_MAX] = {[0 ... (TP2822_DEV_MAX*TP2822_DEV_CH_MAX - 1)] = -1}; ///< Previous video input format, -1: Unknown
static int vin_stable[TP2822_DEV_MAX*TP2822_DEV_CH_MAX] = {[0 ... (TP2822_DEV_MAX*TP2822_DEV_CH_MAX - 1)] = -1}; ///< Video input status , 0: unstable, 1: stable, -1: debounce design for v2 camera

static struct tp2822_video_fmt_t tp2822_video_fmt_info[TP2822_VFMT_MAX] = {
    /* Idx                        Width   Height  Prog    FPS */
    {  TP2822_VFMT_720P60,         1280,   720,    1,      60  },  ///< 720P @60
    {  TP2822_VFMT_720P50,         1280,   720,    1,      50  },  ///< 720P @50
    {  TP2822_VFMT_1080P30,        1920,   1080,   1,      30  },  ///< 1080P@30
    {  TP2822_VFMT_1080P25,        1920,   1080,   1,      25  },  ///< 1080P@25
    {  TP2822_VFMT_720P30,         1280,   720,    1,      30  },  ///< 720P @30
    {  TP2822_VFMT_720P25,         1280,   720,    1,      25  },  ///< 720P @25
    {  TP2822_VFMT_HALF_1080P30,    960,   1080,   1,      30  },  ///< Half-1080P@30
    {  TP2822_VFMT_HALF_1080P25,    960,   1080,   1,      25  },  ///< Half-1080P@25
    {  TP2822_VFMT_HALF_720P30,     640,   720,    1,      30  },  ///< Half-720P @30
    {  TP2822_VFMT_HALF_720P25,     640,   720,    1,      25  },  ///< Half-720P @25
};

const unsigned char SYS_AND[4] = {0xfe, 0xfd, 0xfb, 0xf7};
const unsigned char SYS_OR[4]  = {0x01, 0x02, 0x04, 0x08};

static const unsigned char TP2822_720P60[] = {
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x06,
    0x1d, 0x72
};

static const unsigned char TP2822_720P50[] = {
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x07,
    0x1d, 0xbc
};

static const unsigned char TP2822_720P30[] = {
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0c,
    0x1d, 0xe4
};

static const unsigned char TP2822_720P25[] = {
    0x15, 0x13,
    0x16, 0x16,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0f,
    0x1d, 0x78
};

static const unsigned char TP2822_1080P30[] = {
    0x15, 0x03,
    0x16, 0xd3,
    0x17, 0x80,
    0x18, 0x29,
    0x19, 0x38,
    0x1a, 0x47,
    0x1b, 0x00,
    0x1c, 0x08,
    0x1d, 0x98
};

static const unsigned char TP2822_1080P25[] = {
    0x15, 0x03,
    0x16, 0xd3,
    0x17, 0x80,
    0x18, 0x29,
    0x19, 0x38,
    0x1a, 0x47,
    0x1b, 0x00,
    0x1c, 0x0a,
    0x1d, 0x50
};

static int tp2822_comm_init(int id, TP2822_VOUT_MODE_T vout_mode)
{
    int ret;

    if(id >= TP2822_DEV_MAX || vout_mode >= TP2822_VOUT_MODE_MAX)
        return -1;

    if(vout_mode == TP2822_VOUT_MODE_2CH_DUAL_EDGE) {
        ret = tp2822_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0x02, 0xCA); if(ret < 0) goto exit; ///< Set MD656 to 1 -> BT.656 format output; GMEN to 1 -> C first

        ret = tp2822_i2c_write(id, 0xF5, 0x0F); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xF6, 0x10); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xF7, 0x32); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xFA, 0x99); if(ret < 0) goto exit; ///< Changed from 0xAA to 0x99 for GM8210 socket board line lack issue
        printk("TP2822#%d enters 2-CH dual edge mode!\n", id);
    }
    else {
        ret = tp2822_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0x02, 0x8A); if(ret < 0) goto exit; ///< Set MD656 to 1 -> BT.656 format output; GMEN to 0 -> Y first

        ret = tp2822_i2c_write(id, 0xF5, 0x00); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xF6, 0x00); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xF7, 0x11); if(ret < 0) goto exit;
        ret = tp2822_i2c_write(id, 0xFA, 0x88); if(ret < 0) goto exit;
        printk("TP2822#%d enters 1-CH bypass mode!\n", id);
    }

    ret = tp2822_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x07, 0xC0); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x0B, 0xC0); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x22, 0x38); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x2E, 0x60); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x30, 0x48); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x31, 0xBB); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x32, 0x2E); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x33, 0x90);	if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x39, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x3A, 0x01); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x3D, 0x08); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x4D, 0x0F); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x4E, 0xFF); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x34, 0x10); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x40, 0x01); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x34, 0x11); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x40, 0x02); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x34, 0x10); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x40, 0x03); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x34, 0x11); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x40, 0x10); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x50, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x51, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x52, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x7F, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x80, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x81, 0x00); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x50, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x51, 0x0B); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x52, 0x0C); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x53, 0x19); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x54, 0x78); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x55, 0x21); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x7E, 0x0F); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x7F, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x80, 0x07); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x81, 0x08); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x82, 0x04); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x83, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x84, 0x00); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x85, 0x60); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x86, 0x10); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x87, 0x06); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x88, 0xBE); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x89, 0x39); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x8A, 0x27); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0xB9, 0x0F); if(ret < 0) goto exit;

    ret = tp2822_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
    ret = tp2822_i2c_write(id, 0x3D, 0x00); if(ret < 0) goto exit;

exit:
    return ret;
}

int tp2822_get_dev_id(int id)
{
    if(id >= TP2822_DEV_MAX)
        return 0;

    return tp2822_i2c_read(id, 0xff);
}
EXPORT_SYMBOL(tp2822_get_dev_id);

int tp2822_video_get_video_output_format(int id, int ch, struct tp2822_video_fmt_t *vfmt)
{
    int ret = 0;
    int fmt_idx;
    int npxl_value;

    if(id >= TP2822_DEV_MAX || ch >= TP2822_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2822_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    npxl_value = (tp2822_i2c_read(id, 0x1c)<<8) | tp2822_i2c_read(id, 0x1d);

    switch(npxl_value) {
        case 0x0672:
            if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2822_VFMT_720P30;
            else {
                if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                    fmt_idx = TP2822_VFMT_HALF_720P30;
                else
                    fmt_idx = TP2822_VFMT_720P60;
            }
            break;
        case 0x07BC:
            if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2822_VFMT_720P25;
            else {
                if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                    fmt_idx = TP2822_VFMT_HALF_720P25;
                else
                    fmt_idx = TP2822_VFMT_720P50;
            }
            break;
        case 0x0CE4:
            if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2822_VFMT_HALF_720P30;
            else
                fmt_idx = TP2822_VFMT_720P30;
            break;
        case 0x0F78:
            if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2822_VFMT_HALF_720P25;
            else
                fmt_idx = TP2822_VFMT_720P25;
            break;
        case 0x0898:
            if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2822_VFMT_HALF_1080P30;
            else
                fmt_idx = TP2822_VFMT_1080P30;
            break;
        case 0x0A50:
            if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
                fmt_idx = TP2822_VFMT_HALF_1080P25;
            else
                fmt_idx = TP2822_VFMT_1080P25;
            break;
        default:
            ret = -1;
            goto exit;
    }

    vfmt->fmt_idx    = tp2822_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2822_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2822_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2822_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2822_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2822_video_get_video_output_format);

int tp2822_video_set_video_output_format(int id, int ch, TP2822_VFMT_T vfmt, TP2822_VOUT_FORMAT_T vout_fmt)
{
    int i, ret = 0;
    u8 tmp;

    if(id >= TP2822_DEV_MAX || ch > TP2822_DEV_CH_MAX || vfmt >= TP2822_VFMT_MAX || vout_fmt >= TP2822_VOUT_FORMAT_MAX)
        return -1;

    ret = tp2822_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page, 4 for all channel
    if(ret < 0)
        goto exit;

    if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1) ///< TVI v2 camera video output setting
        vfmt = (vfmt==TP2822_VFMT_720P50)?TP2822_VFMT_720P50:TP2822_VFMT_720P60;

    if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_1CH_BYPASS) { ///< Update per port VCLK setting only during 1-CH bypass mode 
        if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1) { ///< TVI v2 camera clock setting
            tmp = tp2822_i2c_read(id, 0xF5);
            tmp |= SYS_OR[ch];
            ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
            tmp = tp2822_i2c_read(id, 0xFA);
            tmp &= ch%2?0x8F:0xF8;
            tmp |= ch%2?0x10:0x01;
            ret = tp2822_i2c_write(id, 0xFA, tmp); if(ret < 0) goto exit;
        }
        else { ///< TVI v1 camera clock setting
            tmp = tp2822_i2c_read(id, 0xF5);
            tmp &= SYS_AND[ch];
            ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
            tmp = tp2822_i2c_read(id, 0x35);
            tmp &= 0xDF; ///< Set FSL to 0, status reflects 148.5 MHz system clock
            ret = tp2822_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            tmp = tp2822_i2c_read(id, 0xFA);
            tmp &= ch%2?0x8F:0xF8;
            tmp &= ~(ch%2?0x10:0x01);
            ret = tp2822_i2c_write(id, 0xFA, tmp); if(ret < 0) goto exit;
        }
    }
    else { ///< TP2822_VOUT_MODE_2CH_DUAL_EDGE
        if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1) {
            tmp = tp2822_i2c_read(id, 0x35);
            tmp &= 0xBF; ///< Set DS2 to 0, disable half output mode for v2 cameras
            ret = tp2822_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
        }
        else {
            if(tp2822_status_get_video_loss(id, ch) == 0) { ///< video present of v1 camera input
                tmp = tp2822_i2c_read(id, 0xF5);
                tmp &= SYS_AND[ch];
                ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;

                tmp = tp2822_i2c_read(id, 0x35);
                tmp |= 0x40; ///< Set DS2 to 1, enable half output mode for v1 cameras
                ret = tp2822_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            }
            else { ///< video lost
                tmp = tp2822_i2c_read(id, 0xF5);
                tmp |= SYS_OR[ch];
                ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;

                tmp = tp2822_i2c_read(id, 0x35);
                tmp &= 0xBF; ///< Set DS2 to 0, disable half output mode for v2 cameras
                ret = tp2822_i2c_write(id, 0x35, tmp); if(ret < 0) goto exit;
            }
        }
    }

    if(pre_fmt[id*TP2822_DEV_CH_MAX+ch] != vfmt) { ///< Update output setting only when video format is changed

        tmp = tp2822_i2c_read(id, 0x02);

        switch(vfmt) {
            case TP2822_VFMT_1080P25:
                for(i=0; i<ARRAY_SIZE(TP2822_1080P25); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_1080P25[i], TP2822_1080P25[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp &= 0xFD;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            case TP2822_VFMT_1080P30:
                for(i=0; i<ARRAY_SIZE(TP2822_1080P30); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_1080P30[i], TP2822_1080P30[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp &= 0xFD;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            case TP2822_VFMT_720P25:
                for(i=0; i<ARRAY_SIZE(TP2822_720P25); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_720P25[i], TP2822_720P25[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp |= 0x02;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            case TP2822_VFMT_720P30:
                for(i=0; i<ARRAY_SIZE(TP2822_720P30); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_720P30[i], TP2822_720P30[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp |= 0x02;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            case TP2822_VFMT_720P50:
                for(i=0; i<ARRAY_SIZE(TP2822_720P50); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_720P50[i], TP2822_720P50[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp |= 0x02;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            case TP2822_VFMT_720P60:
                for(i=0; i<ARRAY_SIZE(TP2822_720P60); i+=2) {
                    ret = tp2822_i2c_write(id, TP2822_720P60[i], TP2822_720P60[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                tmp |= 0x02;
                ret = tp2822_i2c_write(id, 0x02, tmp);
                if(ret < 0)
                    goto exit;
                break;
            default:
                ret = -1;
                break;
        }
        pre_fmt[id*TP2822_DEV_CH_MAX+ch] = vfmt;
    }

    if(tp2822_status_get_video_loss(id, ch) == 0) { ///< Set video input as stable only when video present
        if(vin_stable[id*TP2822_DEV_CH_MAX+ch] == -1)
            vin_stable[id*TP2822_DEV_CH_MAX+ch] = 0;
        else if(vin_stable[id*TP2822_DEV_CH_MAX+ch] == 0)
            vin_stable[id*TP2822_DEV_CH_MAX+ch] = 1;
    }

exit:
    return ret;
}
EXPORT_SYMBOL(tp2822_video_set_video_output_format);

int tp2822_status_get_video_loss(int id, int ch)
{
    int ret;
    int vlos = 1;
    u8 status, tmp;

    if(id >= TP2822_DEV_MAX || ch >= TP2822_DEV_CH_MAX)
        return 1;

    ret = tp2822_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    status = tp2822_i2c_read(id, 0x01);

    if(tp2822_i2c_read(id, 0x2f) == 0x08) {
        tmp = tp2822_i2c_read(id, 0x04); 
        if(tmp >= 0x30)
            status |= 0x80;
    }

    if(!(status & 0x80)) ///< Video Present
        vlos = 0;
    else
        vlos = 1;

    if(vlos) {
        is_v2_cam[id*TP2822_DEV_CH_MAX+ch] = 0; ///< Clear the flag of V2 camera when video input was lost
        vin_stable[id*TP2822_DEV_CH_MAX+ch] = -1; ///< Clear the flag of video input status
    }
exit:
    return vlos;
}
EXPORT_SYMBOL(tp2822_status_get_video_loss);

int tp2822_status_get_video_input_format(int id, int ch, struct tp2822_video_fmt_t *vfmt)
{
    int ret = 0;
    u8  cvstd, sywd, tmp;
    int fmt_idx;

    if(id >= TP2822_DEV_MAX || ch >= TP2822_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2822_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    /* Set SYSCLK to 148.5 MHz for v1/v2 camera identification */
    if((tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE) && (vin_stable[id*TP2822_DEV_CH_MAX+ch] <= 0)) {
        tmp = tp2822_i2c_read(id, 0xF5);
        tmp &= SYS_AND[ch];
        ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
    }

    cvstd = tp2822_i2c_read(id, 0x03);
    sywd = (cvstd & 0x08)?1:0;
    cvstd &= 0x07;

    if(cvstd < TP2822_VFMT_MAX) {
        if(sywd && (TP2822_VFMT_720P25 == cvstd || TP2822_VFMT_720P30 == cvstd)) { ///< See if v2 camera was connected
            if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 0) {
                is_v2_cam[id*TP2822_DEV_CH_MAX+ch] = sywd;
                if(is_v2_cam[id*TP2822_DEV_CH_MAX+ch] == 1)
                    printk("TP2822#%d-CH#%d: TVI V2 camera detected!\n", id, ch);
            }
        }

        fmt_idx = tp2822_i2c_read(id, 0x03) & 0x07;
        if(fmt_idx >= TP2822_VFMT_MAX) {
            ret = -1;
            goto exit;
        }
    }
    else { ///< Unknown video input format
        ret = -1;
        goto exit;
    }

    /* Set SYSCLK to 74.25 MHz for 2-CH dual edge output */
    if(tp2822_get_vout_mode(id) == TP2822_VOUT_MODE_2CH_DUAL_EDGE && (vin_stable[id*TP2822_DEV_CH_MAX+ch] <= 0)) {
        tmp = tp2822_i2c_read(id, 0xF5);
        tmp |= SYS_OR[ch];
        ret = tp2822_i2c_write(id, 0xF5, tmp); if(ret < 0) goto exit;
    }

    vfmt->fmt_idx    = tp2822_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2822_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2822_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2822_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2822_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2822_status_get_video_input_format);

int tp2822_video_init(int id, TP2822_VOUT_FORMAT_T vout_fmt, TP2822_VOUT_MODE_T vout_mode)
{
    u8  tmp;
    int ret;

    if(id >= TP2822_DEV_MAX || vout_fmt >= TP2822_VOUT_FORMAT_MAX || vout_mode >= TP2822_VOUT_MODE_MAX)
        return -1;

    /* get and check chip device id */
    if(tp2822_get_dev_id(id) != TP2822_DEV_ID) {
        printk("TP2822#%d: unknown device ID!\n", id);
        return -1;
    }

    ret = tp2822_i2c_write(id, 0x40, 0x04);   ///< Switch to write all page
    if(ret < 0)
        goto exit;

    /* Video Common */
    if(vout_fmt == TP2822_VOUT_FORMAT_BT1120) {
        ret = -1;
        printk("TP2822#%d driver doesn't support BT1120 vout format!\n", id);
        goto exit;
    }
    else {
        ret = tp2822_comm_init(id, vout_mode);
        if(ret < 0)
            goto exit;
    }

    /* Video Standard */
    if(vout_mode == TP2822_VOUT_MODE_2CH_DUAL_EDGE)
        ret = tp2822_video_set_video_output_format(id, 4, DEFAULT_2CH_FREERUN_VFMT, vout_fmt); ///< ch=4 for config all channel
    else
        ret = tp2822_video_set_video_output_format(id, 4, TP2822_VFMT_720P25, vout_fmt); ///< ch=4 for config all channel
    if(ret < 0)
        goto exit;

	/* PLL2 software reset */
	tmp = tp2822_i2c_read(id, 0x44);
	ret = tp2822_i2c_write(id, 0x44, (tmp | 0x40)); if(ret < 0) goto exit;
	msleep(10);
	ret = tp2822_i2c_write(id, 0x44, tmp); if(ret < 0) goto exit;

	/* ADC reset */
	ret = tp2822_i2c_write(id, 0x40, 0x04); if(ret < 0) goto exit;
	ret = tp2822_i2c_write(id, 0x3B, 0x33); if(ret < 0) goto exit;
	ret = tp2822_i2c_write(id, 0x3B, 0x03); if(ret < 0) goto exit;

	/* device software reset */
	tmp = tp2822_i2c_read(id, 0x06);
	ret = tp2822_i2c_write(id, 0x06, (tmp | 0x80)); if(ret < 0) goto exit;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2822_video_init);
