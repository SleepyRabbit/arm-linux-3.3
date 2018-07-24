/**
 * @file tp2806_lib.c
 * TechPoint TP2806 4CH HD-TVI Video Decoder Library
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "techpoint/tp2806.h"   ///< from module/include/front_end/hdtvi

#define DEFAULT_2CH_FREERUN_VFMT TP2806_VFMT_720P60 ///< Framerate of free-run output should be doubled during 2-CH dual edge mode

static int is_v2_cam[TP2806_DEV_MAX*TP2806_DEV_CH_MAX] = {[0 ... (TP2806_DEV_MAX*TP2806_DEV_CH_MAX - 1)] = 0}; ///< Check if video input is TVI v2 camera
static int pre_fmt[TP2806_DEV_MAX*TP2806_DEV_CH_MAX] = {[0 ... (TP2806_DEV_MAX*TP2806_DEV_CH_MAX - 1)] = -1}; ///< Previous video input format, -1: Unknown
static int force_freerun[TP2806_DEV_MAX*TP2806_DEV_CH_MAX] = {[0 ... (TP2806_DEV_MAX*TP2806_DEV_CH_MAX - 1)] = -1}; ///< Force free-run output, 0: normal input video data, 1: force free-run, -1: debounce design

static struct tp2806_video_fmt_t tp2806_video_fmt_info[TP2806_VFMT_MAX] = {
    /* Idx                    Width   Height  Prog    FPS */
    {  TP2806_VFMT_720P60,    1280,   720,    1,      60  },  ///< 720P @60
    {  TP2806_VFMT_720P50,    1280,   720,    1,      50  },  ///< 720P @50
    {  TP2806_VFMT_1080P30,   1920,   1080,   1,      30  },  ///< 1080P@30 -> Unsupported
    {  TP2806_VFMT_1080P25,   1920,   1080,   1,      25  },  ///< 1080P@25 -> Unsupported
    {  TP2806_VFMT_720P30,    1280,   720,    1,      30  },  ///< 720P @30
    {  TP2806_VFMT_720P25,    1280,   720,    1,      25  },  ///< 720P @25
};

static const unsigned char TP2806_720P60[] = {
    0x15, 0x13,
    0x16, 0x15,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x06,
    0x1d, 0x72
};

static const unsigned char TP2806_720P50[] = {
    0x15, 0x13,
    0x16, 0x15,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x07,
    0x1d, 0xbc
};

static const unsigned char TP2806_720P30[] = {
    0x15, 0x13,
    0x16, 0x15,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0c,
    0x1d, 0xe4
};

static const unsigned char TP2806_720P25[] = {
    0x15, 0x13,
    0x16, 0x15,
    0x17, 0x00,
    0x18, 0x19,
    0x19, 0xd0,
    0x1a, 0x25,
    0x1b, 0x00,
    0x1c, 0x0f,
    0x1d, 0x78
};

static int tp2806_comm_init(int id, TP2806_VOUT_MODE_T vout_mode)
{
    int ret;

    if(id >= TP2806_DEV_MAX || vout_mode >= TP2806_VOUT_MODE_MAX)
        return -1;

    if(vout_mode == TP2806_VOUT_MODE_2CH_DUAL_EDGE) {
        ret = tp2806_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x10); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x01); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x11); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x02); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x12); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x03); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x13); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0xf5, 0x0f); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf6, 0x10); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf7, 0x32); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf8, 0x10); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf9, 0x32); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xfa, 0xaa); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xfb, 0xaa); if(ret < 0) goto exit;
        printk("TP2806#%d enters 2-CH dual edge mode!\n", id);
    }
    else {
        ret = tp2806_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x01); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x02); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x03); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x34, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0xf5, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf6, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf7, 0x11); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf8, 0x22); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xf9, 0x33); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xfa, 0x88); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xfb, 0x88); if(ret < 0) goto exit;
        printk("TP2806#%d enters 1-CH bypass mode!\n", id);
    }

    ret = tp2806_i2c_write(id, 0x40, 0x04);   ///< Switch to write all page
    if(ret < 0)
        goto exit;

    if((tp2806_get_revision(id) > 0)) {  ///< TP2806D and TP2806
	    ret = tp2806_i2c_write(id, 0x07, 0xc0); if(ret < 0) goto exit;
	    ret = tp2806_i2c_write(id, 0x0b, 0xc0); if(ret < 0) goto exit;
	    ret = tp2806_i2c_write(id, 0x22, 0x38); if(ret < 0) goto exit;
	    ret = tp2806_i2c_write(id, 0x2e, 0x60); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x30, 0x48); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x31, 0xbb); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x32, 0x2e); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x33, 0x90);	if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x39, 0x42); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x3A, 0x01); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x4d, 0x0f); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x4e, 0xff); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x01); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x50, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x51, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x52, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x7F, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x80, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x81, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x40, 0x00); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x50, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x51, 0x0b); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x52, 0x0c); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x53, 0x19); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x54, 0x78); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x55, 0x21); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x7e, 0x0f); if(ret < 0) goto exit;

        ret = tp2806_i2c_write(id, 0x7F, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x80, 0x07); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x81, 0x08); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x82, 0x04); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x83, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x84, 0x00); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x85, 0x60); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x86, 0x10); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x87, 0x06); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x88, 0xBE); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x89, 0x39); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0x8A, 0x27); if(ret < 0) goto exit;
        ret = tp2806_i2c_write(id, 0xB9, 0x0F); if(ret < 0) goto exit;
    }
    else {  ///< Not TP2806
       printk("Unknown TP280x chip revision!\n");
       ret = -1;
    }

exit:
    return ret;
}

int tp2806_get_dev_id(int id)
{
    if(id >= TP2806_DEV_MAX)
        return 0;

    return tp2806_i2c_read(id, 0xff);
}
EXPORT_SYMBOL(tp2806_get_dev_id);

int tp2806_get_revision(int id)
{
    if(id >= TP2806_DEV_MAX)
        return 0;

    if(tp2806_get_dev_id(id) == TP2806_DEV_ID)
        return (tp2806_i2c_read(id, 0x00) & 0x7);   ///< TP2806
    else
        return (tp2806_i2c_read(id, 0xfd) & 0x7);   ///< TP2806C & TP2806D
}
EXPORT_SYMBOL(tp2806_get_revision);

int tp2806_video_get_video_output_format(int id, int ch, struct tp2806_video_fmt_t *vfmt)
{
    int ret = 0;
    int fmt_idx;
    int npxl_value;

    if(id >= TP2806_DEV_MAX || ch >= TP2806_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2806_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    npxl_value = (tp2806_i2c_read(id, 0x1c)<<8) | tp2806_i2c_read(id, 0x1d);

    switch(npxl_value) {
        case 0x0672:
            if(is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2806_VFMT_720P30;
            else
                fmt_idx = TP2806_VFMT_720P60;
            break;
        case 0x07BC:
            if(is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 1)
                fmt_idx = TP2806_VFMT_720P25;
            else
                fmt_idx = TP2806_VFMT_720P50;
            break;
        case 0x0CE4:
            fmt_idx = TP2806_VFMT_720P30;
            break;
        case 0x0F78:
            fmt_idx = TP2806_VFMT_720P25;
            break;
        default:
            ret = -1;
            goto exit;
    }

    vfmt->fmt_idx    = tp2806_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2806_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2806_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2806_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2806_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2806_video_get_video_output_format);

int tp2806_video_set_video_output_format(int id, int ch, TP2806_VFMT_T vfmt, TP2806_VOUT_FORMAT_T vout_fmt)
{
    int i, ret = 0;
    u8 clk;

    if(id >= TP2806_DEV_MAX || ch > TP2806_DEV_CH_MAX || vfmt >= TP2806_VFMT_MAX || vout_fmt >= TP2806_VOUT_FORMAT_MAX)
        return -1;

    ret = tp2806_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page, 4 for all channel
    if(ret < 0)
        goto exit;

    if((ch < TP2806_DEV_CH_MAX) && (force_freerun[id*TP2806_DEV_CH_MAX+ch] == 1)) { ///< Force free-run output
        //ret = tp2806_i2c_write(id, 0x10, 0x40); if(ret < 0) goto exit; ///< Brightness setting for black screen output
        //ret = tp2806_i2c_write(id, 0x11, 0x00); if(ret < 0) goto exit; ///< Contrast setting for black screen output
        //ret = tp2806_i2c_write(id, 0x12, 0x00); if(ret < 0) goto exit; ///< UVGain setting for black screen output
        ret = tp2806_i2c_write(id, 0x2A, 0x3E); if(ret < 0) goto exit; ///< Force it to blue screen output
        vfmt = DEFAULT_2CH_FREERUN_VFMT;
    }
    else {
        //ret = tp2806_i2c_write(id, 0x10, 0x00); if(ret < 0) goto exit; ///< Brightness default setting
        //ret = tp2806_i2c_write(id, 0x11, 0x40); if(ret < 0) goto exit; ///< Contrast default setting
        //ret = tp2806_i2c_write(id, 0x12, 0x40); if(ret < 0) goto exit; ///< UVGain default setting
        ret = tp2806_i2c_write(id, 0x2A, 0x30); ///< Normal input video data
    }

    if(vout_fmt == TP2806_VOUT_FORMAT_BT1120) {
        ret = tp2806_i2c_write(id, 0x02, ((tp2806_get_vout_mode(id) == TP2806_VOUT_MODE_1CH_BYPASS) ? 0x02 : 0x0a));
    }
    else {
        ret = tp2806_i2c_write(id, 0x02, ((tp2806_get_vout_mode(id) == TP2806_VOUT_MODE_1CH_BYPASS) ? 0xc2 : 0xca));
    }

    if(tp2806_get_vout_mode(id) == TP2806_VOUT_MODE_1CH_BYPASS) { ///< Update per port VCLK setting only during 1-CH bypass mode 
        if(is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 1) { ///< TVI V2 camera support
            clk = tp2806_i2c_read(id, ch<2?0xFA:0xFB);
            clk &= ch%2?0x8F:0xF8;
            clk |= ch%2?0x10:0x01;
            ret = tp2806_i2c_write(id, ch<2?0xFA:0xFB, clk); ///< Clock Output Control
            if(ret < 0)
                goto exit;
            clk = tp2806_i2c_read(id, 0xF5);
            clk |= 0x1<<ch;
            ret = tp2806_i2c_write(id, 0xF5, clk); ///< System Clock Control
            if(ret < 0)
                goto exit;
        }
        else {
            clk = tp2806_i2c_read(id, ch<2?0xFA:0xFB);
            clk &= ch%2?0x8F:0xF8;
            clk &= ~(ch%2?0x10:0x01);
            ret = tp2806_i2c_write(id, ch<2?0xFA:0xFB, clk); ///< Clock Output Control
            if(ret < 0)
                goto exit;
            clk = tp2806_i2c_read(id, 0xF5);
            clk &= ~(0x1<<ch);
            ret = tp2806_i2c_write(id, 0xF5, clk); ///< System Clock Control
            if(ret < 0)
                goto exit;
        }
    }

    if(pre_fmt[id*TP2806_DEV_CH_MAX+ch] != vfmt) { ///< Update output setting only when video format is changed
        switch(vfmt) {
            case TP2806_VFMT_720P60:
                for(i=0; i<ARRAY_SIZE(TP2806_720P60); i+=2) {
                    ret = tp2806_i2c_write(id, TP2806_720P60[i], TP2806_720P60[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                break;
            case TP2806_VFMT_720P50:
                for(i=0; i<ARRAY_SIZE(TP2806_720P50); i+=2) {
                    ret = tp2806_i2c_write(id, TP2806_720P50[i], TP2806_720P50[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                break;
            case TP2806_VFMT_720P30:
                for(i=0; i<ARRAY_SIZE(TP2806_720P30); i+=2) {
                    ret = tp2806_i2c_write(id, TP2806_720P30[i], TP2806_720P30[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                break;
            case TP2806_VFMT_720P25:
                for(i=0; i<ARRAY_SIZE(TP2806_720P25); i+=2) {
                    ret = tp2806_i2c_write(id, TP2806_720P25[i], TP2806_720P25[i+1]);
                    if(ret < 0)
                        goto exit;
                }
                break;
            default:
                ret = -1;
                break;
        }
        pre_fmt[id*TP2806_DEV_CH_MAX+ch] = vfmt;
    }

exit:
    return ret;
}
EXPORT_SYMBOL(tp2806_video_set_video_output_format);

int tp2806_status_get_video_loss(int id, int ch)
{
    int ret;
    int vlos = 1;

    if(id >= TP2806_DEV_MAX || ch >= TP2806_DEV_CH_MAX)
        return 1;

    ret = tp2806_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    if((tp2806_i2c_read(id, 0x01) & 0x80) == 0) { ///< Video Present
        vlos = 0;
        if((tp2806_i2c_read(id, 0x2f) == 0x08) && (tp2806_i2c_read(id, 0x04) >= 0x30)) ///< Internal Status Check
            vlos = 1;
    }

    if(vlos) {
        is_v2_cam[id*TP2806_DEV_CH_MAX+ch] = 0; ///< Clear the flag of V2 camera when video input was lost
        force_freerun[id*TP2806_DEV_CH_MAX+ch] = -1; ///< Clear the flag of force free-run when video input was lost
    }
exit:
    return vlos;
}
EXPORT_SYMBOL(tp2806_status_get_video_loss);

int tp2806_status_get_video_input_format(int id, int ch, struct tp2806_video_fmt_t *vfmt)
{
    int ret = 0;
    u8  status, sys_clk;
    int fmt_idx;

    if(id >= TP2806_DEV_MAX || ch >= TP2806_DEV_CH_MAX || !vfmt)
        return -1;

    ret = tp2806_i2c_write(id, 0x40, ch);   ///< Switch to Channel Control Page
    if(ret < 0)
        goto exit;

    status = tp2806_i2c_read(id, 0x04);
    if(status < 0x30) {
        if(tp2806_get_vout_mode(id) == TP2806_VOUT_MODE_2CH_DUAL_EDGE) {
            if((is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 0) && (force_freerun[id*TP2806_DEV_CH_MAX+ch] <= 0)) {
                ret = tp2806_i2c_write(id, 0x02, 0xC2); if(ret < 0) goto exit; ///< Required setting for v1/v2 camera detection
                sys_clk = tp2806_i2c_read(id, 0xF5);
                sys_clk &= ~(0x1<<ch);
                ret = tp2806_i2c_write(id, 0xF5, sys_clk); if(ret < 0) goto exit;
            }
        }

        fmt_idx = tp2806_i2c_read(id, 0x03) & 0x7;
        if((fmt_idx == TP2806_VFMT_720P30) || (fmt_idx == TP2806_VFMT_720P25)) { ///< TP2806 will identify v2 camera as 720P30/25 when EQ locked
            if(is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 0) {
                is_v2_cam[id*TP2806_DEV_CH_MAX+ch] = (tp2806_i2c_read(id, 0x03)&0x08)?1:0; ///< See if v2 camera input when EQ locked
                if(is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 1)
                    printk("TP2806#%d-CH#%d: TVI V2 camera detected!\n", id, ch);
            }
        }

        if(tp2806_get_vout_mode(id) == TP2806_VOUT_MODE_2CH_DUAL_EDGE) {
            if((is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 0) && (force_freerun[id*TP2806_DEV_CH_MAX+ch] == 0)) {
                printk("TP2806#%d-CH#%d: doesn't support v1 cameras during 2-CH dual edge mode, force it to free-run mode!\n", id, ch);
                force_freerun[id*TP2806_DEV_CH_MAX+ch] = 1;
            }

            if((is_v2_cam[id*TP2806_DEV_CH_MAX+ch] == 0) && (force_freerun[id*TP2806_DEV_CH_MAX+ch] == -1)) ///< Debounce design for v2 camera detection
                force_freerun[id*TP2806_DEV_CH_MAX+ch] = 0;

            ret = tp2806_i2c_write(id, 0x02, 0xCA); if(ret < 0) goto exit; ///< Restore the setting for 2-CH dual edge output
            sys_clk = tp2806_i2c_read(id, 0xF5);
            sys_clk |= 0x1<<ch;
            ret = tp2806_i2c_write(id, 0xF5, sys_clk); if(ret < 0) goto exit;
        }

        if(fmt_idx >= TP2806_VFMT_MAX) {
            ret = -1;
            goto exit;
        }
    }
    else {
        ret = -1;
        goto exit;
    }

    if((fmt_idx == TP2806_VFMT_1080P30) || (fmt_idx == TP2806_VFMT_1080P25)) { ///< Check if the input format is unsupported
        if(force_freerun[id*TP2806_DEV_CH_MAX+ch] <= 0) {
            force_freerun[id*TP2806_DEV_CH_MAX+ch] = 1;
            printk("TP2806#%d CH#%d: Doesn't support 1080P camera, force it to free-run mode!\n", id, ch);
        }
    }

    vfmt->fmt_idx    = tp2806_video_fmt_info[fmt_idx].fmt_idx;
    vfmt->width      = tp2806_video_fmt_info[fmt_idx].width;
    vfmt->height     = tp2806_video_fmt_info[fmt_idx].height;
    vfmt->prog       = tp2806_video_fmt_info[fmt_idx].prog;
    vfmt->frame_rate = tp2806_video_fmt_info[fmt_idx].frame_rate;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2806_status_get_video_input_format);

int tp2806_video_init(int id, TP2806_VOUT_FORMAT_T vout_fmt, TP2806_VOUT_MODE_T vout_mode)
{
    u8  tmp;
    int ret;

    if(id >= TP2806_DEV_MAX || vout_fmt >= TP2806_VOUT_FORMAT_MAX || vout_mode >= TP2806_VOUT_MODE_MAX)
        return -1;

    /* get and check chip device id */
    if((vout_mode == TP2806_VOUT_MODE_2CH_DUAL_EDGE) && (tp2806_get_dev_id(id) != TP2806_DEV_ID)) {
        printk("TP2806#%d: unknown device ID, and it doesn't support 2ch dual edge mode!\n", id);
        return -1;
    }

    ret = tp2806_i2c_write(id, 0x40, 0x04);   ///< Switch to write all page
    if(ret < 0)
        goto exit;

    /* Video Common */
    if(vout_fmt == TP2806_VOUT_FORMAT_BT1120) {
        ret = -1;
        printk("TP2806#%d driver not support BT1120 vout format!\n", id);
        goto exit;
    }
    else {
        ret = tp2806_comm_init(id, vout_mode);
        if(ret < 0)
            goto exit;
    }

    /* Video Standard */
    if(vout_mode == TP2806_VOUT_MODE_2CH_DUAL_EDGE)
        ret = tp2806_video_set_video_output_format(id, 4, DEFAULT_2CH_FREERUN_VFMT, vout_fmt); ///< ch=4 for config all channel
    else
        ret = tp2806_video_set_video_output_format(id, 4, TP2806_VFMT_720P30, vout_fmt); ///< ch=4 for config all channel
    if(ret < 0)
        goto exit;

	/* PLL2 software reset */
	tmp = tp2806_i2c_read(id, 0x44);
	ret = tp2806_i2c_write(id, 0x44, (tmp | 0x40));
	if(ret < 0)
	    goto exit;

	msleep(10);

	ret = tp2806_i2c_write(id, 0x44, tmp);
	if(ret < 0)
	    goto exit;

	/* device software reset */
	ret = tp2806_i2c_write(id, 0x06, 0x80);
	if(ret < 0)
	    goto exit;

exit:
    return ret;
}
EXPORT_SYMBOL(tp2806_video_init);
