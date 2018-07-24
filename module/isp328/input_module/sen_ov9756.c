/**
 * @file sen_ov9756.c
 * OV9756 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 * 20160120 author RobertChuang
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_ov9756"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"
//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1280 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT     960 //ISP Output size
#define DEFAULT_XCLK        12000000//30FPS  24000000:60FPS

static u16 sensor_w = DEFAULT_IMAGE_WIDTH;
module_param(sensor_w, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_w, "sensor image width");

static u16 sensor_h = DEFAULT_IMAGE_HEIGHT;
module_param(sensor_h, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_h, "sensor image height");

static u32 sensor_xclk = DEFAULT_XCLK;
module_param(sensor_xclk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_xclk, "sensor external clock frequency");

static int mirror = 0;
module_param(mirror, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mirror, "sensor mirror function");

static int flip = 0;
module_param(flip, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flip, "sensor flip function");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

//=============================================================================
// global parameters
//=============================================================================
#define SENSOR_NAME         "OV9756"
#define SENSOR_MAX_WIDTH        1280
#define SENSOR_MAX_HEIGHT        960
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        4
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF

//normal setting
#define  DEFAULT_960P_30FPS_MODE    MODE_960P_30FPS
#define  DEFAULT_960P_30FPS_VTS                 988
#define  DEFAULT_960P_30FPS_HTS                 810
#define  DEFAULT_960P_30FPS_WIDTH              1280
#define  DEFAULT_960P_30FPS_HEIGHT              960
#define  DEFAULT_960P_30FPS_MIRROR                0
#define  DEFAULT_960P_30FPS_FLIP                  0
#define  DEFAULT_960P_30FPS_TROW                337
#define  DEFAULT_960P_30FPS_PCLK            4800000
#define  DEFAULT_960P_30FPS_MAXFPS               30
#define  DEFAULT_960P_30FPS_LANE                  2 //parallel ignore
#define  DEFAULT_960P_30FPS_RAWBIT            RAW10
#define  DEFAULT_960P_30FPS_BAYER          BAYER_BG

#define  DEFAULT_960P_60FPS_MODE    MODE_960P_60FPS
#define  DEFAULT_960P_60FPS_VTS                 988
#define  DEFAULT_960P_60FPS_HTS                 810
#define  DEFAULT_960P_60FPS_WIDTH              1280
#define  DEFAULT_960P_60FPS_HEIGHT              960
#define  DEFAULT_960P_60FPS_MIRROR                0
#define  DEFAULT_960P_60FPS_FLIP                  0
#define  DEFAULT_960P_60FPS_TROW                168
#define  DEFAULT_960P_60FPS_PCLK            9600000
#define  DEFAULT_960P_60FPS_MAXFPS               60
#define  DEFAULT_960P_60FPS_LANE                  2 //parallel ignore
#define  DEFAULT_960P_60FPS_RAWBIT            RAW10
#define  DEFAULT_960P_60FPS_BAYER          BAYER_BG


static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_960P_30FPS = 0,
	MODE_960P_60FPS,
    MODE_MAX
};

typedef struct sensor_info {
    bool is_init;
    enum SENSOR_MODE currentmode; 
    u32 vts;	
    u32 hts;
    int img_w;
    int img_h;
    int mirror;
    int flip;
    u32 t_row;          // unit is 1/10 us
    u32 pclk;
    int fps;
    int lane;
    enum SRC_FMT rawbit;	
    enum BAYER_TYPE bayer;
    int long_exp;
} sensor_info_t;


static sensor_info_t g_sOV9756info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_960P_30FPS_MODE,        
    DEFAULT_960P_30FPS_VTS,
    DEFAULT_960P_30FPS_HTS,
    DEFAULT_960P_30FPS_WIDTH,
    DEFAULT_960P_30FPS_HEIGHT,
    DEFAULT_960P_30FPS_MIRROR,
    DEFAULT_960P_30FPS_FLIP,
    DEFAULT_960P_30FPS_TROW,
    DEFAULT_960P_30FPS_PCLK,
    DEFAULT_960P_30FPS_MAXFPS,
    DEFAULT_960P_30FPS_LANE,
    DEFAULT_960P_30FPS_RAWBIT,
    DEFAULT_960P_30FPS_BAYER,
    SENSOR_LONG_EXP,
    },
    {
    SENSOR_ISINIT,
    DEFAULT_960P_60FPS_MODE,        
    DEFAULT_960P_60FPS_VTS,
    DEFAULT_960P_60FPS_HTS,
    DEFAULT_960P_60FPS_WIDTH,
    DEFAULT_960P_60FPS_HEIGHT,
    DEFAULT_960P_60FPS_MIRROR,
    DEFAULT_960P_60FPS_FLIP,
    DEFAULT_960P_60FPS_TROW,
    DEFAULT_960P_60FPS_PCLK,
    DEFAULT_960P_60FPS_MAXFPS,
    DEFAULT_960P_60FPS_LANE,
    DEFAULT_960P_60FPS_RAWBIT,
    DEFAULT_960P_60FPS_BAYER,
    SENSOR_LONG_EXP,
    },    
};

typedef struct sensor_reg {
    u16 addr;  
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_960P_init_regs[] = {   
    {0x0103, 0x01}, 
    {0x0100, 0x00}, 
    {0x0300, 0x02}, 
    {0x0302, 0x50}, 
    {0x0303, 0x01}, 
    {0x0304, 0x01}, 
    {0x0305, 0x01}, 
    {0x0306, 0x01}, 
    {0x030a, 0x00}, 
    {0x030b, 0x00}, 
    {0x030d, 0x1e}, 
    {0x030e, 0x01}, 
    {0x030f, 0x04}, 
    {0x0312, 0x01}, 
    {0x031e, 0x04}, 
    {0x3000, 0x0f}, 
    {0x3001, 0xff}, 
    {0x3002, 0xe1}, 
    {0x3005, 0xf0}, 
    {0x3009, 0x00}, 
    {0x300f, 0x03}, 
    {0x3011, 0x20}, 
    {0x3016, 0x00}, 
    {0x3018, 0x32}, 
    {0x301a, 0xf0}, 
    {0x301b, 0xf0}, 
    {0x301c, 0xf0}, 
    {0x301d, 0xf0}, 
    {0x301e, 0xf0}, 
    {0x3022, 0x21}, 
    {0x3031, 0x0a}, 
    {0x3032, 0x80}, 
    {0x303c, 0xff}, 
    {0x303e, 0xff}, 
    {0x3040, 0xf0}, 
    {0x3041, 0x00}, 
    {0x3042, 0xf0}, 
    {0x3104, 0x01}, 
    {0x3106, 0x15}, 
    {0x3107, 0x01}, 
    {0x3500, 0x00}, 
    {0x3501, 0x3d}, 
    {0x3502, 0x00}, 
    {0x3503, 0x08}, 
    {0x3504, 0x03}, 
    {0x3505, 0x83}, 
    {0x3508, 0x00}, 
    {0x3509, 0x80}, 
    {0x3600, 0x65}, 
    {0x3601, 0x60}, 
    {0x3602, 0x22}, 
    {0x3610, 0xe8}, 
    {0x3611, 0x56}, 
    {0x3612, 0x48}, 
    {0x3613, 0x5a}, 
    {0x3614, 0x96}, 
    {0x3615, 0x79}, 
    {0x3617, 0x07}, 
    {0x3620, 0x84}, 
    {0x3621, 0x90}, 
    {0x3622, 0x00}, 
    {0x3623, 0x00}, 
    {0x3625, 0x07}, 
    {0x3633, 0x10}, 
    {0x3634, 0x10}, 
    {0x3635, 0x14}, 
    {0x3636, 0x13}, 
    {0x3650, 0x00}, 
    {0x3652, 0xff}, 
    {0x3654, 0x20}, 
    {0x3653, 0x34}, 
    {0x3655, 0x20}, 
    {0x3656, 0xff}, 
    {0x3657, 0xc4}, 
    {0x365a, 0xff}, 
    {0x365b, 0xff}, 
    {0x365e, 0xff}, 
    {0x365f, 0x00}, 
    {0x3668, 0x00}, 
    {0x366a, 0x07}, 
    {0x366d, 0x00}, 
    {0x366e, 0x10}, 
    {0x3700, 0x14}, 
    {0x3701, 0x08}, 
    {0x3702, 0x1d}, 
    {0x3703, 0x10}, 
    {0x3704, 0x14}, 
    {0x3705, 0x00}, 
    {0x3706, 0x27}, 
    {0x3707, 0x04}, 
    {0x3708, 0x12}, 
    {0x3709, 0x24}, 
    {0x370a, 0x00}, 
    {0x370b, 0x7d}, 
    {0x370c, 0x04}, 
    {0x370d, 0x00}, 
    {0x370e, 0x00}, 
    {0x370f, 0x05}, 
    {0x3710, 0x18}, 
    {0x3711, 0x0e}, 
    {0x3714, 0x24}, 
    {0x3718, 0x13}, 
    {0x371a, 0x5e}, 
    {0x3720, 0x05}, 
    {0x3721, 0x05}, 
    {0x3726, 0x06}, 
    {0x3728, 0x05}, 
    {0x372a, 0x03}, 
    {0x372b, 0x53}, 
    {0x372c, 0x53}, 
    {0x372d, 0x53}, 
    {0x372e, 0x06}, 
    {0x372f, 0x10}, 
    {0x3730, 0x82}, 
    {0x3731, 0x06}, 
    {0x3732, 0x14}, 
    {0x3733, 0x10}, 
    {0x3736, 0x30}, 
    {0x373a, 0x02}, 
    {0x373b, 0x03}, 
    {0x373c, 0x05}, 
    {0x373e, 0x18}, 
    {0x3755, 0x00}, 
    {0x3758, 0x00}, 
    {0x375a, 0x06}, 
    {0x375b, 0x13}, 
    {0x375f, 0x14}, 
    {0x3772, 0x23}, 
    {0x3773, 0x05}, 
    {0x3774, 0x16}, 
    {0x3775, 0x12}, 
    {0x3776, 0x08}, 
    {0x377a, 0x06}, 
    {0x3788, 0x00}, 
    {0x3789, 0x04}, 
    {0x378a, 0x01}, 
    {0x378b, 0x60}, 
    {0x3799, 0x27}, 
    {0x37a0, 0x44}, 
    {0x37a1, 0x2d}, 
    {0x37a7, 0x44}, 
    {0x37a8, 0x38}, 
    {0x37aa, 0x44}, 
    {0x37ab, 0x24}, 
    {0x37b3, 0x33}, 
    {0x37b5, 0x36}, 
    {0x37c0, 0x42}, 
    {0x37c1, 0x42}, 
    {0x37c2, 0x04}, 
    {0x37c5, 0x00}, 
    {0x37c7, 0x30}, 
    {0x37c8, 0x00}, 
    {0x37d1, 0x13}, 
    {0x3800, 0x00}, 
    {0x3801, 0x00}, 
    {0x3802, 0x00}, 
    {0x3803, 0x04}, 
    {0x3804, 0x05}, 
    {0x3805, 0x0f}, 
    {0x3806, 0x03}, 
    {0x3807, 0xcb}, 
    {0x3808, 0x05}, 
    {0x3809, 0x00}, 
    {0x380a, 0x03}, 
    {0x380b, 0xc0}, 
    {0x380c, 0x03}, 
    {0x380d, 0x2a}, 
    {0x380e, 0x03},
    {0x380f, 0xdc},
    {0x3810, 0x00}, 
    {0x3811, 0x08}, 
    {0x3812, 0x00}, 
    {0x3813, 0x04}, 
    {0x3814, 0x01}, 
    {0x3815, 0x01}, 
    {0x3816, 0x00}, 
    {0x3817, 0x00}, 
    {0x3818, 0x00}, 
    {0x3819, 0x00}, 
    {0x3820, 0x80}, 
    {0x3821, 0x40}, 
    {0x3826, 0x00}, 
    {0x3827, 0x08}, 
    {0x382a, 0x01}, 
    {0x382b, 0x01}, 
    {0x3836, 0x02}, 
    {0x3838, 0x10}, 
    {0x3861, 0x00}, 
    {0x3862, 0x00}, 
    {0x3863, 0x02}, 
    {0x3b00, 0x00}, 
    {0x3c00, 0x89}, 
    {0x3c01, 0xab}, 
    {0x3c02, 0x01}, 
    {0x3c03, 0x00}, 
    {0x3c04, 0x00}, 
    {0x3c05, 0x03}, 
    {0x3c06, 0x00}, 
    {0x3c07, 0x05}, 
    {0x3c0c, 0x00}, 
    {0x3c0d, 0x00}, 
    {0x3c0e, 0x00}, 
    {0x3c0f, 0x00}, 
    {0x3c40, 0x00}, 
    {0x3c41, 0xa3}, 
    {0x3c43, 0x7d}, 
    {0x3c56, 0x80}, 
    {0x3c80, 0x08}, 
    {0x3c82, 0x01}, 
    {0x3c83, 0x61}, 
    {0x3d85, 0x17}, 
    {0x3f08, 0x08}, 
    {0x3f0a, 0x00}, 
    {0x3f0b, 0x30}, 
    {0x4000, 0xcd}, 
    {0x4003, 0x40}, 
    {0x4009, 0x0d}, 
    {0x4010, 0xf0}, 
    {0x4011, 0x70}, 
    {0x4017, 0x10}, 
    {0x4040, 0x00}, 
    {0x4041, 0x00}, 
    {0x4303, 0x00}, 
    {0x4307, 0x30}, 
    {0x4500, 0x30}, 
    {0x4502, 0x40}, 
    {0x4503, 0x06}, 
    {0x4508, 0xaa}, 
    {0x450b, 0x00}, 
    {0x450c, 0x00}, 
    {0x4600, 0x00}, 
    {0x4601, 0x80}, 
    {0x4700, 0x04}, 
    {0x4704, 0x00}, 
    {0x4705, 0x04}, 
    {0x481f, 0x26}, 
    {0x4837, 0x14}, 
    {0x484a, 0x3f}, 
    {0x5000, 0x10}, 
    {0x5001, 0x01}, 
    {0x5002, 0x28}, 
    {0x5004, 0x0c}, 
    {0x5006, 0x0c}, 
    {0x5007, 0xe0}, 
    {0x5008, 0x01}, 
    {0x5009, 0xb0}, 
    {0x502a, 0x18}, 
    {0x5901, 0x00}, 
    {0x5a01, 0x00}, 
    {0x5a03, 0x00}, 
    {0x5a04, 0x0c}, 
    {0x5a05, 0xe0}, 
    {0x5a06, 0x09}, 
    {0x5a07, 0xb0}, 
    {0x5a08, 0x06}, 
    {0x5e00, 0x00}, 
    {0x5e10, 0xfc}, 
    {0x0100, 0x01}, 
};

#define NUM_OF_960P_INIT_REGS (sizeof(sensor_960P_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  real_gain;
    u8  real_gain_fraction;
    u16 dgain;
    u16 total_gain;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00, 0x80,  0x400,  64}, {0x00, 0x90,  0x400,  72}, {0x00, 0xA0,  0x400,  80}, {0x00, 0xB0,  0x400,  88},
    {0x00, 0xC0,  0x400,  96}, {0x00, 0xD0,  0x400, 104}, {0x00, 0x60,  0x400, 112}, {0x00, 0xF0,  0x400, 120},
    {0x01, 0x00,  0x400, 128}, {0x01, 0x20,  0x400, 144}, {0x01, 0x40,  0x400, 160}, {0x01, 0x60,  0x400, 176},
    {0x01, 0x80,  0x400, 192}, {0x01, 0xA0,  0x400, 208}, {0x01, 0xC0,  0x400, 224}, {0x01, 0xE0,  0x400, 240},	
    {0x02, 0x00,  0x400, 256}, {0x02, 0x40,  0x400, 288}, {0x02, 0x80,  0x400, 320}, {0x02, 0xC0,  0x400, 352},
    {0x03, 0x00,  0x400, 384}, {0x03, 0x40,  0x400, 416}, {0x03, 0x80,  0x400, 448}, {0x03, 0xC0,  0x400, 480},	
    {0x04, 0x00,  0x400, 512}, {0x04, 0x80,  0x400, 576}, {0x05, 0x00,  0x400, 640}, {0x05, 0x80,  0x400, 704},
    {0x06, 0x00,  0x400, 768}, {0x06, 0x80,  0x400, 832}, {0x07, 0x00,  0x400, 896}, {0x07, 0x80,  0x400, 960},
    {0x07, 0xC0,  0x421,1024}, {0x07, 0xC0,  0x4A5,1152}, {0x07, 0xC0,  0x529,1280}, {0x07, 0xC0,  0x5AD,1408},
    {0x07, 0xC0,  0x631,1536}, {0x07, 0xC0,  0x6B5,1664}, {0x07, 0xC0,  0x739,1792}, {0x07, 0xC0,  0x7BD,1920},	
    {0x07, 0xC0,  0x842,2048}, {0x07, 0xC0,  0x94A,2304}, {0x07, 0xC0,  0xA52,2560}, {0x07, 0xC0,  0xB5A,2816},
    {0x07, 0xC0,  0xC63,3072}, {0x07, 0xC0,  0xD6B,3328}, {0x07, 0xC0,  0xE73,3584}, {0x07, 0xC0,  0XF7B,3840},
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))
//=============================================================================
// I2C ID
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x6C >> 1
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    u8 buf[2],buf2[2];
       
    buf[0]        = (addr >> 8) & 0xFF;
    buf[1]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;
    msgs[0].len   = 2;
    msgs[0].buf   = buf;

    buf2[0]       = 0;
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1;
    msgs[1].len   = 1;
    msgs[1].buf   = buf2;

    if (sensor_i2c_transfer(msgs, 2))
        return -1;

    *pval = buf2[0];
    return 0;
}

static int write_reg(u32 addr, u32 val)
{
    struct i2c_msg  msgs;
    u8   buf[3];

    buf[0]     = (addr >> 8) & 0xFF;
    buf[1]     = addr & 0xFF;
    buf[2]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 3;
    msgs.buf   = buf;

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	

    pinfo->pclk= g_sOV9756info[pinfo->currentmode].pclk;
    isp_info("OV9756 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sOV9756info[pinfo->currentmode].hts;
    pinfo->t_row=g_sOV9756info[pinfo->currentmode].t_row;
    isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, exp_line_l,exp_line_m,exp_line_h;

    if (0==g_psensor->curr_exp)
    {
        read_reg(0x3500, &exp_line_h);
        read_reg(0x3501, &exp_line_m);		
        read_reg(0x3502, &exp_line_l);
        lines = (((exp_line_h & 0x0F) << 12)|((exp_line_m & 0xFF)<<4)|((exp_line_l >>4)& 0x0F));
        g_psensor->curr_exp =(lines * (pinfo->t_row) + 500) / 1000;

    }

        //isp_info("g_psensor->curr_exp=%d\n", g_psensor->curr_exp);
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line=0;

    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;
  
    if (1>exp_line)
        exp_line = 1;

    if(exp_line>(SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE))
        exp_line=SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE;

    if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) 
    {
        write_reg(0x380E, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
        write_reg(0x380F, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
        pinfo->long_exp = 1;
    } 
    else 
    {
        if (pinfo->long_exp)
        {
            write_reg(0x380E, (pinfo->vts >> 8) & 0xFF);        
            write_reg(0x380F, (pinfo->vts & 0xFF));
            pinfo->long_exp = 0;
        }
    }

    write_reg(0x3500, (exp_line >>12) & 0x0F);
    write_reg(0x3501, (exp_line >> 4) & 0xFF);
    write_reg(0x3502, (((exp_line &0x0F) << 4)&0xf0));	

    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;

}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;

    max_fps = g_sOV9756info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sOV9756info[pinfo->currentmode].vts) * max_fps / fps;

    if((pinfo->vts) > SENSOR_MAX_VTS)
	    pinfo->vts=	SENSOR_MAX_VTS;
		
    //isp_info("adjust_blk fps=%d, vts=%d\n", fps, pinfo->vts);

    write_reg(0x380E, (pinfo->vts) >> 8);
    write_reg(0x380F, (pinfo->vts) & 0xff);
    g_psensor->fps = fps;

    get_exp();
    set_exp(g_psensor->curr_exp);	
}

static u32 get_gain(void)
{
    u32 real_gain=0;
    u32 real_gain_fraction=0;
   
    if (g_psensor->curr_gain == 0) 
    {
        read_reg(0x3508, &real_gain);
        read_reg(0x3509, &real_gain_fraction);	  
        g_psensor->curr_gain=64*(100*(((real_gain&0x1F)<<1)|(real_gain_fraction>>7))+(real_gain_fraction&0x7F)*100/128)/100;   		
    }
	    //isp_info("g_psensor->curr_gain=%d\n", g_psensor->curr_gain);
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    u8 real_gain = 0;
    u8 real_gain_fraction=0; 
    u8 dgain_h=0,dgain_l=0;
    int i;

    if (gain > gain_table[NUM_OF_GAINSET - 1].total_gain)
        gain = gain_table[NUM_OF_GAINSET - 1].total_gain;
    else if(gain < gain_table[0].total_gain)
        gain = gain_table[0].total_gain;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].total_gain > gain)
            break;
    }

    real_gain = gain_table[i-1].real_gain;
    real_gain_fraction = gain_table[i-1].real_gain_fraction;
    dgain_h= (gain_table[i-1].dgain >>8) &0x0F;	
    dgain_l= gain_table[i-1].dgain & 0xFF;

    write_reg(0x3508, (real_gain & 0x1F) );
    write_reg(0x3509, real_gain_fraction);	

    write_reg(0x5032, dgain_h);
    write_reg(0x5033, dgain_l);
    write_reg(0x5034, dgain_h);
    write_reg(0x5035, dgain_l);	
    write_reg(0x5036, dgain_h);
    write_reg(0x5037, dgain_l);		
	
    g_psensor->curr_gain = gain_table[i-1].total_gain;

    return 0;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->flip){
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_GB;
        else
            g_psensor->bayer_type = BAYER_BG;
    }
    else{
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_GB;
        else
            g_psensor->bayer_type = BAYER_BG;
    }
}

static int get_mirror(void)
{
    u32 reg;
	
    read_reg(0x3821, &reg);
    return (reg & BIT1) ? 1 : 0;    
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    read_reg(0x3821, &reg);

    if (enable)
        reg |= (BIT1|BIT2);
    else
        reg &= ~(BIT1|BIT2);
    write_reg(0x3821, reg);

    update_bayer_type();

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x3820, &reg);
    return (reg & BIT1) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;
  
    pinfo->flip= enable;

    read_reg(0x3820, &reg);

    if (enable)
        reg |= (BIT1|BIT2);
    else
        reg &= ~(BIT1|BIT2);
    write_reg(0x3820, reg);

    update_bayer_type();

    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;
    int i, ret = 0;
    
    //isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT))
    {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }

    pinfo->img_w=width;
    pinfo->img_h=height;

    if(24000000==sensor_xclk)
        pinfo->currentmode=MODE_960P_60FPS;
    else
        pinfo->currentmode=MODE_960P_30FPS;
    pInitTable=sensor_960P_init_regs;
    InitTable_Num=NUM_OF_960P_INIT_REGS;

    isp_info("start initial...\n");

    // set initial registers
    for (i=0; i<InitTable_Num; i++)
    {
        if (pInitTable[i].addr == DELAY_CODE) 
	    {
	        mdelay(pInitTable[i].val);
        }
        else if(write_reg(pInitTable[i].addr, pInitTable[i].val) < 0)
	    {
            isp_err("%s init fail!!", SENSOR_NAME);
            return -EINVAL;
        }
    }

    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);
	
    calculate_t_row();
    adjust_blk(g_psensor->fps);


    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sOV9756info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sOV9756info[pinfo->currentmode].img_h;	

    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;
    g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;
    //isp_info("initial end ...\n");
    return ret;
}


static int get_property(enum SENSOR_CMD cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd) {
        case ID_MIRROR:
            *(int*)arg = get_mirror();
            break;
        case ID_FLIP:
            *(int*)arg = get_flip();
            break;
        case ID_FPS:
            *(int*)arg = g_psensor->fps;
            break;       
        default:
            ret = -EFAULT;
            break;
    }

    return ret;
}


static int set_property(enum SENSOR_CMD cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd) {
        case ID_MIRROR:
            set_mirror((int)arg);
            break;
        case ID_FLIP:
            set_flip((int)arg);
            break;
        case ID_FPS:
            adjust_blk((int)arg);
            break;
        default:
            ret = -EFAULT;
            break;
    }

    return ret;
}

static int init(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    
    if (pinfo->is_init)
        return 0;

    pinfo->mirror = mirror;
    pinfo->flip = flip;

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);

    if (ret == 0)
        pinfo->is_init = true;
    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current exposure and gain setting
    get_exp();
    get_gain();
 	
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void ov9756_deconstruct(void)
{
    if ((g_psensor) && (g_psensor->private)) {
        kfree(g_psensor->private);
    }

    sensor_remove_i2c_driver();
    // turn off EXT_CLK
    isp_api_extclk_onoff(0);
    // release CAP_RST
    isp_api_cap_rst_exit();
}

static int ov9756_construct(u32 xclk, u16 width, u16 height)
{
    sensor_info_t *pinfo = NULL;
    int ret = 0;

    // initial CAP_RST
    ret = isp_api_cap_rst_init();
    if (ret < 0)
        return ret;

    // clear CAP_RST
    isp_api_cap_rst_set_value(0);

    // set EXT_CLK frequency and turn on it.
    ret = isp_api_extclk_set_freq(xclk);
    if (ret < 0)
        return ret;
    ret = isp_api_extclk_onoff(1);
    if (ret < 0)
        return ret;
    mdelay(50);

    // set CAP_RST
    isp_api_cap_rst_set_value(1);
    mdelay(50);

    pinfo = kzalloc(sizeof(sensor_info_t), GFP_KERNEL);
    if (!pinfo) {
        isp_err("failed to allocate private data!");
        return -ENOMEM;
    }

    g_psensor->private = pinfo;
    snprintf(g_psensor->name, DEV_MAX_NAME_SIZE, SENSOR_NAME);

    if(24000000==sensor_xclk)
        pinfo->currentmode=MODE_960P_60FPS;
    else
        pinfo->currentmode=MODE_960P_30FPS;		
    
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = IF_PARALLEL;
    g_psensor->num_of_lane = g_sOV9756info[pinfo->currentmode].lane;//parallel ignore
    g_psensor->fmt = g_sOV9756info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sOV9756info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->inv_clk = 0;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;    
    g_psensor->min_exp = 1;    //100us
    g_psensor->max_exp = 5000; // 0.5 sec ,500ms
    g_psensor->min_gain = gain_table[0].total_gain;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].total_gain;
    g_psensor->exp_latency = 0;
    g_psensor->gain_latency = 0;
    g_psensor->fps = fps;
    g_psensor->fn_init = init;
    g_psensor->fn_read_reg = read_reg;
    g_psensor->fn_write_reg = write_reg;
    g_psensor->fn_set_size = set_size;
    g_psensor->fn_get_exp = get_exp;
    g_psensor->fn_set_exp = set_exp;
    g_psensor->fn_get_gain = get_gain;
    g_psensor->fn_set_gain = set_gain;
    g_psensor->fn_get_property = get_property;
    g_psensor->fn_set_property = set_property;
	
    if ((ret = sensor_init_i2c_driver()) < 0)
        goto construct_err;

    if ((ret = init()) < 0)
        goto construct_err;

    return 0;

construct_err:
    ov9756_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init ov9756_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp32X.ko(%x)!",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = ov9756_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
        goto init_err2;

    // register sensor device to ISP driver
    if ((ret = isp_register_sensor(g_psensor)) < 0)
        goto init_err2;

    return ret;

init_err2:
    kfree(g_psensor);

init_err1:
    return ret;
}

static void __exit ov9756_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ov9756_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(ov9756_init);
module_exit(ov9756_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor OV9756");
MODULE_LICENSE("GPL");
