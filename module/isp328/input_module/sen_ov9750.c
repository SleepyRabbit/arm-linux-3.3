/**
 * @file sen_ov9750.c
 * OmniVision ov9750 sensor driver
 *
 * Copyright (C) 2014 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.8 $
 * $Date: 2015/09/08 07:03:10 $
 *
 *  Revision 1.0  2014/10/23 18:58:03  icekirin.yuan
 *  First version, surpport parallel 960P
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

#define PFX "sen_ov9750"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1280   
#define DEFAULT_IMAGE_HEIGHT    960
#define DEFAULT_XCLK            12000000
#define DEFAULT_INTERFACE       IF_MIPI

static ushort sensor_w = DEFAULT_IMAGE_WIDTH;
module_param(sensor_w, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_w, "sensor image width");

static ushort sensor_h = DEFAULT_IMAGE_HEIGHT;
module_param(sensor_h, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_h, "sensor image height");

static uint sensor_xclk = DEFAULT_XCLK;
module_param(sensor_xclk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sensor_xclk, "sensor external clock frequency");

static uint mirror = 0;
module_param(mirror, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mirror, "sensor mirror function");

static uint flip = 0;
module_param(flip, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flip, "sensor flip function");

static uint ch_num = 2;
module_param(ch_num, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_num, "channel number");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "frame per second");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static ushort interface = DEFAULT_INTERFACE;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "ov9750"
#define SENSOR_MAX_WIDTH   1280
#define SENSOR_MAX_HEIGHT  960 
#define DELAY_CODE          0xffff


static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    u32 img_w;
    u32 img_h;
    int mirror;
    int flip;
    u32 sw_lines_max;
    u32 prev_sw_res;
    u32 prev_sw_reg;
    u32 t_row;          // unit is 1/10 us
    u32 pclk;
    u32 sensor_rev;
} sensor_info_t;

typedef struct sensor_reg {
    u16 addr;
    u8  val;
} sensor_reg_t;

static sensor_reg_t sensor_init_regs[] = {
    //;; Compile from OV9750_AB_00_07_00.ovt
    //@@ OV9750 960P 60FPS DVP Test ADC10bits
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
    {0x3508, 0x02},
    {0x3509, 0x80},
    {0x3600, 0x65},
    {0x3601, 0x60},
    {0x3602, 0x22},
    {0x3610, 0xe8},
    {0x3611, 0x5c},
    {0x3612, 0x18},
    {0x3613, 0x3a},
    {0x3614, 0x91},
    {0x3615, 0x79},
    {0x3617, 0x07},
    {0x3621, 0x90},
    {0x3622, 0x00},
    {0x3623, 0x00},
    {0x3625, 0x07},//Fixed Fix pattern by Jason
    {0x3633, 0x10},
    {0x3634, 0x10},
    {0x3635, 0x14},
    {0x3636, 0x14},
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
    {0x3702, 0x1d},
    {0x3703, 0x10},
    {0x3704, 0x14},
    {0x3705, 0x00},
    {0x3706, 0x27},
    {0x3709, 0x24},
    {0x370a, 0x00},
    {0x370b, 0x7d},
    {0x3714, 0x24},
    {0x371a, 0x5e},
    {0x3730, 0x82},
    {0x3733, 0x10},
    {0x373e, 0x18},
    {0x3755, 0x00},
    {0x3758, 0x00},
    {0x375b, 0x13},
    {0x3772, 0x23},
    {0x3773, 0x05},
    {0x3774, 0x16},
    {0x3775, 0x12},
    {0x3776, 0x08},
    {0x37a8, 0x38},
    {0x37b5, 0x36},
    {0x37c2, 0x04},
    {0x37c5, 0x00},
    {0x37c7, 0x31},
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

    {0x300f, 0x03},
    {0x3733, 0x10},
    {0x3610, 0xe8},
    {0x3611, 0x5c},
    {0x3635, 0x14},
    {0x3636, 0x14},
    {0x3620, 0x84},
    {0x3614, 0x96},
    {0x481f, 0x30},
    {0x3788, 0x00},
    {0x3789, 0x04},
    {0x378a, 0x01},
    {0x378b, 0x60},
    {0x3799, 0x27},

    {0x0100, 0x01},
};

#define NUM_OF_INIT_REGS (sizeof(sensor_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_init_mipi_regs[] = {
    //@@  OV9750 960P 60FPS MIPI 2 lane Test ADC10bits
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x0300, 0x04},
    {0x0302, 0x64},
    {0x0303, 0x00},
    {0x0304, 0x03},
    {0x0305, 0x01},
    {0x0306, 0x01},
    {0x030a, 0x00},
    {0x030b, 0x00},
    {0x030d, 0x1e},
    {0x030e, 0x01},
    {0x030f, 0x04},
    {0x0312, 0x01},
    {0x031e, 0x04},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x21},
    {0x3005, 0xf0},
    {0x3011, 0x00},
    {0x3016, 0x53},
    {0x3018, 0x32},
    {0x301a, 0xf0},
    {0x301b, 0xf0},
    {0x301c, 0xf0},
    {0x301d, 0xf0},
    {0x301e, 0xf0},
    {0x3022, 0x01},
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
    {0x3508, 0x02},
    {0x3509, 0x80},
    {0x3600, 0x65},
    {0x3601, 0x60},
    {0x3602, 0x22},
    {0x3610, 0xe8},
    {0x3611, 0x5c},
    {0x3612, 0x18},
    {0x3613, 0x3a},
    {0x3614, 0x91},
    {0x3615, 0x79},
    {0x3617, 0x57},
    {0x3621, 0x90},
    {0x3622, 0x00},
    {0x3623, 0x00},
    {0x3625, 0x07},//Fixed Fix pattern by Jason
    {0x3633, 0x10},
    {0x3634, 0x10},
    {0x3635, 0x14},
    {0x3636, 0x14},
    {0x3650, 0x00},
    {0x3652, 0xff},
    {0x3654, 0x00},
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
    {0x3702, 0x1d},
    {0x3703, 0x10},
    {0x3704, 0x14},
    {0x3705, 0x00},
    {0x3706, 0x27},
    {0x3709, 0x24},
    {0x370a, 0x00},
    {0x370b, 0x7d},
    {0x3714, 0x24},
    {0x371a, 0x5e},
    {0x3730, 0x82},
    {0x3733, 0x10},
    {0x373e, 0x18},
    {0x3755, 0x00},
    {0x3758, 0x00},
    {0x375b, 0x13},
    {0x3772, 0x23},
    {0x3773, 0x05},
    {0x3774, 0x16},
    {0x3775, 0x12},
    {0x3776, 0x08},
    {0x37a8, 0x38},
    {0x37b5, 0x36},
    {0x37c2, 0x04},
    {0x37c5, 0x00},
    {0x37c7, 0x31},
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

    {0x300f, 0x00},
    {0x3733, 0x10},
    {0x3610, 0xe8},
    {0x3611, 0x5c},
    {0x3635, 0x14},
    {0x3636, 0x14},
    {0x3620, 0x84},
    {0x3614, 0x96},
    {0x481f, 0x30},
    {0x3788, 0x00},
    {0x3789, 0x04},
    {0x378a, 0x01},
    {0x378b, 0x60},
    {0x3799, 0x27},

    {0x0100, 0x01},
};

#define NUM_OF_INIT_MIPI_REGS (sizeof(sensor_init_mipi_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u32 a_gain;
    u32 hcg;
    u32 d_gain;
    u32 gain_x;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x080, 0, 0x400,   64},{0x088, 0, 0x400,   68},{0x090, 0, 0x400,   72},{0x098, 0, 0x400,   76},
    {0x0A0, 0, 0x400,   80},{0x0A8, 0, 0x400,   84},{0x0B0, 0, 0x400,   88},{0x0B8, 0, 0x400,   92},
    {0x0C0, 0, 0x400,   96},{0x0C8, 0, 0x400,  100},{0x0D0, 0, 0x400,  104},{0x0D8, 0, 0x400,  108},
    {0x0E0, 0, 0x400,  112},{0x0E8, 0, 0x400,  116},{0x0F0, 0, 0x400,  120},{0x0F8, 0, 0x400,  124},
    {0x100, 0, 0x400,  128},{0x110, 0, 0x400,  136},{0x120, 0, 0x400,  144},{0x130, 0, 0x400,  152},
    {0x140, 0, 0x400,  160},{0x150, 0, 0x400,  168},{0x160, 0, 0x400,  176},{0x170, 0, 0x400,  184},
    {0x180, 0, 0x400,  192},{0x190, 0, 0x400,  200},{0x1A0, 0, 0x400,  208},{0x1B0, 0, 0x400,  216},
    {0x1C0, 0, 0x400,  224},{0x1D0, 0, 0x400,  232},{0x1E0, 0, 0x400,  240},{0x1F0, 0, 0x400,  248},
    {0x200, 0, 0x400,  256},{0x220, 0, 0x400,  272},{0x240, 0, 0x400,  288},{0x260, 0, 0x400,  304},
    {0x280, 0, 0x400,  320},{0x2A0, 0, 0x400,  336},{0x2C0, 0, 0x400,  352},{0x2E0, 0, 0x400,  368},
    {0x300, 0, 0x400,  384},{0x320, 0, 0x400,  400},{0x340, 0, 0x400,  416},{0x360, 0, 0x400,  432},
    {0x380, 0, 0x400,  448},{0x3A0, 0, 0x400,  464},{0x3C0, 0, 0x400,  480},{0x3E0, 0, 0x400,  496},
    {0x400, 0, 0x400,  512},{0x440, 0, 0x400,  544},{0x480, 0, 0x400,  576},{0x4C0, 0, 0x400,  608},
    {0x500, 0, 0x400,  640},{0x540, 0, 0x400,  672},{0x580, 0, 0x400,  704},{0x5C0, 0, 0x400,  736},
    {0x600, 0, 0x400,  768},{0x640, 0, 0x400,  800},{0x680, 0, 0x400,  832},{0x6C0, 0, 0x400,  864},
    {0x700, 0, 0x400,  896},{0x740, 0, 0x400,  928},{0x780, 0, 0x400,  960},{0x7C0, 0, 0x400,  992},
    {0x3A0, 1, 0x400, 1067},{0x3E0, 1, 0x400, 1141},{0x420, 1, 0x400, 1214},{0x460, 1, 0x400, 1288},
    {0x4A0, 1, 0x400, 1362},{0x4E0, 1, 0x400, 1435},{0x520, 1, 0x400, 1509},{0x560, 1, 0x400, 1582},
    {0x5A0, 1, 0x400, 1656},{0x5E0, 1, 0x400, 1730},{0x620, 1, 0x400, 1803},{0x660, 1, 0x400, 1877},
    {0x6A0, 1, 0x400, 1950},{0x6E0, 1, 0x400, 2024},{0x750, 1, 0x400, 2153},{0x7C0, 1, 0x400, 2282},
    {0x7C0, 1, 0x440, 2424},{0x7C0, 1, 0x480, 2567},{0x7C0, 1, 0x4C0, 2709},{0x7C0, 1, 0x500, 2852},
    {0x7C0, 1, 0x540, 2995},{0x7C0, 1, 0x580, 3137},{0x7C0, 1, 0x5C0, 3280},{0x7C0, 1, 0x600, 3422},
    {0x7C0, 1, 0x640, 3565},{0x7C0, 1, 0x680, 3708},{0x7C0, 1, 0x6C0, 3850},{0x7C0, 1, 0x700, 3993},
    {0x7C0, 1, 0x780, 4278},{0x7C0, 1, 0x800, 4563},{0x7C0, 1, 0x880, 4848},{0x7C0, 1, 0x900, 5134},
    {0x7C0, 1, 0x980, 5419},{0x7C0, 1, 0xA00, 5704},{0x7C0, 1, 0xA80, 5989},{0x7C0, 1, 0xB00, 6274},
    {0x7C0, 1, 0xB80, 6560},{0x7C0, 1, 0xC00, 6845},{0x7C0, 1, 0xC80, 7130},{0x7C0, 1, 0xD00, 7415},
    {0x7C0, 1, 0xD80, 7700},{0x7C0, 1, 0xE00, 7986},{0x7C0, 1, 0xE80, 8271},{0x7C0, 1, 0xF00, 8556},
    {0x7C0, 1, 0xF80, 8841},{0x7C0, 1, 0xFFF, 9124},
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x6C >> 1)
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[2], tmp2[2];

    tmp[0]        = (addr >> 8) & 0xFF;
    tmp[1]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;
    msgs[0].len   = 2;
    msgs[0].buf   = tmp;

    tmp2[0]       = 0;
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1;
    msgs[1].len   = 1;
    msgs[1].buf   = tmp2;

    if (sensor_i2c_transfer(msgs, 2))
        return -1;

    *pval = tmp2[0];
    return 0;
}

static int write_reg(u32 addr, u32 val)
{
    struct i2c_msg  msgs;
    unsigned char   buf[3];

    buf[0]     = (addr >> 8) & 0xFF;
    buf[1]     = addr & 0xFF;
    buf[2]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 3;
    msgs.buf   = buf;

    return sensor_i2c_transfer(&msgs, 1);
}


static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 prediv0, prediv, mul, pll2_sys_prediv, sysdiv, sys_prediv, sclk_pdiv;
    u32 m_div, pclk_div;
    u32 prediv_tbl[16]={2,3,4,5,6,7,8,9,10,11,12,13,14,16,18,18};
    u32 sysdiv_tbl[8]={2,3,4,5,6,7,8,10};
    u32 sys_prediv_tbl[4]={1,2,4,1};
    u32 pclk, bits;

    //from PLL2_sys_clk, 0x3032[7]=1, 0x3033[1]=0
    read_reg(0x0312, &prediv0);
    read_reg(0x030b, &prediv);
    read_reg(0x030d, &mul);
    read_reg(0x030f, &pll2_sys_prediv);
    read_reg(0x030e, &sysdiv);
    read_reg(0x3106, &sclk_pdiv);

    prediv0 = ((prediv0 >> 4) & 0x01) + 1;
    prediv = prediv_tbl[prediv & 0x0f]; //x2
    mul &= 0xff;
    pll2_sys_prediv = (pll2_sys_prediv & 0x0f) + 1;
    sysdiv = sysdiv_tbl[sysdiv & 0x07]; //x2
    sys_prediv = sys_prediv_tbl[(sclk_pdiv >> 2) & 0x03];
    sclk_pdiv = (sclk_pdiv >> 4) & 0x0f;
    if (sclk_pdiv == 0)
        sclk_pdiv = 1;
    //printk("%d, %d, %d, %d, %d, %d, %d\n", prediv0,prediv,mul,pll2_sys_prediv,sysdiv,sys_prediv,sclk_pdiv);
    pclk = xclk / prediv0 * 2 / prediv * mul / pll2_sys_prediv * 2 / sysdiv / sys_prediv / sclk_pdiv;
    pclk *= 2;  //pclk = sclk * 2;
    pinfo->pclk = pclk;
    isp_info("xclk=%d, pclk=%d\n", xclk, pclk);
    
    if (g_psensor->interface == IF_MIPI){
        //from PLL1 MIPI PHY clock
        read_reg(0x030a, &prediv0);
        read_reg(0x0300, &prediv);
        read_reg(0x0302, &mul);
        read_reg(0x0303, &m_div);
        read_reg(0x3020, &pclk_div);

        prediv0 = (prediv0 & 0x01) + 1;
        prediv = prediv_tbl[prediv & 0x0f]; //x2
        mul &= 0xff;
        m_div = (m_div & 0x0f) + 1;
        pclk_div = ((pclk_div >> 3) & 0x01) + 1;
        //printk("%d, %d, %d, %d, %d, %d\n", prediv0,prediv,mul,m_div,pclk_div);
        pclk = xclk / prediv0 * 2 / prediv * mul / m_div / pclk_div;    //MIPI data rate / lane
        pclk *= g_psensor->num_of_lane;
        bits = g_psensor->fmt == RAW10 ? 10 : 12;
        pclk /= bits;
        isp_info("s_pclk=%d\n", pclk);
    }
    
    return pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h, reg_msb;
    u32 hts;
	
    read_reg(0x380C, &reg_h);
    read_reg(0x380D, &reg_l);
    hts = ((reg_h & 0xFF) << 8) | (reg_l & 0xFF);
    pinfo->t_row = (hts * 2 * 10000) / (pinfo->pclk / 1000);
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;

    read_reg(0x380E, &reg_h);
    read_reg(0x380F, &reg_l);
    pinfo->sw_lines_max = (((reg_h & 0x7F) << 8) | (reg_l & 0xFF)); // 1185

    read_reg(0x3502, &reg_l);
    read_reg(0x3501, &reg_h);
    read_reg(0x3500, &reg_msb);
    pinfo->prev_sw_reg = ((reg_msb & 0xFF) << 16) | ((reg_h & 0xFF) << 8) | (reg_l & 0xF);

    isp_info("hts=%d, t_row=%d, sw_lines_max=%d\n", hts, pinfo->t_row, pinfo->sw_lines_max);
}

static void adjust_blk(int fps)
{
    u32 vmax, max_fps;

    if (sensor_xclk == 24000000)
        max_fps = 60;
    else
        max_fps = 30;
    if (fps > max_fps)
        fps = max_fps;
    vmax = 988 * max_fps / fps;

    isp_info("adjust_blk fps=%d, vmax=%d\n", fps, vmax);

    write_reg(0x380E, vmax >> 8);
    write_reg(0x380F, vmax & 0xff);
    calculate_t_row();
    g_psensor->fps = fps;
}

static int set_size(u32 width, u32 height)
{
    int ret = 0;
    
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("%dx%d exceeds maximum resolution %dx%d!\n",
            width, height, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT);
        return -1;
    }
    
	g_psensor->out_win.x = 0;
	g_psensor->out_win.y = 0;
	g_psensor->out_win.width = SENSOR_MAX_WIDTH;
	g_psensor->out_win.height = SENSOR_MAX_HEIGHT;

    adjust_blk(g_psensor->fps);  

    g_psensor->img_win.x = ((SENSOR_MAX_WIDTH - width) >> 1) &~BIT0;
    g_psensor->img_win.y = ((SENSOR_MAX_HEIGHT - height) >> 1) &~BIT0;

    return ret;
}

static u32 get_exp(void)
{

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, sw_t, sw_m, sw_b;

    if (!g_psensor->curr_exp) {
        read_reg(0x3500, &sw_t);
        read_reg(0x3501, &sw_m);
        read_reg(0x3502, &sw_b);
        lines = ((sw_t & 0xF) << 12)|((sw_m & 0xFF) << 4)|((sw_b & 0xFF) >> 4);
        g_psensor->curr_exp = (lines * pinfo->t_row + 500) / 1000;
        //printk("get_exp lines=%d  t_row=%d\n",lines, pinfo->t_row);
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{      
    int max_exp_line, dummy_line;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 lines, tmp;
    
    lines = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

	// printk("line %d,exp %d,t_row %d\n",lines,exp,pinfo->t_row);
    //lines = (exp * 1000) / pinfo->t_row;
    if (lines == 0)
        lines = 1;
   
    max_exp_line = pinfo->sw_lines_max;// - 4;
    if (lines > max_exp_line) {
        tmp = max_exp_line;
        dummy_line = lines - max_exp_line;
    } else {
        tmp = lines;
        dummy_line = 0;
    }

    tmp = lines << 4;//lines << 4;//

    if (tmp != pinfo->prev_sw_reg) {
        write_reg(0x3502, (tmp & 0xFF));
        write_reg(0x3501, (tmp >> 8) & 0xFF);
        write_reg(0x3500, (tmp >> 16) & 0xFF);
        pinfo->prev_sw_reg = tmp;
    }

    g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;

    // printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;
}

static u32 get_gain(void)
{
    u32 i, gain_x, a_gain, d_gain, reg;
	u32 hcg;

    if (g_psensor->curr_gain == 0) {
        read_reg(0x3508, &reg);
        a_gain = (reg & 0x1F) << 8;
        read_reg(0x3509, &reg);
        a_gain |= reg & 0xFF;
        read_reg(0x5032, &reg);
        d_gain = (reg & 0x0F) << 8;
        read_reg(0x5033, &reg);
        d_gain |= reg & 0xFF;
        read_reg(0x37c7, &reg);
        hcg = reg & 0x01 ? 10 : 23; //2.3x
        
        gain_x = (a_gain / 128) * (d_gain / 1024) * (hcg / 10) * 64;
        
        for (i=0; i<NUM_OF_GAINSET; i++) {
            if (gain_table[i].gain_x == gain_x)
                break;
        }
        if (i < NUM_OF_GAINSET)
            g_psensor->curr_gain = gain_table[i].gain_x;
    }

    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    int ret = 0, i, tmp, a_gain, d_gain;
    struct i2c_msg  msgs[9];
    unsigned char   buf[9][3];
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	u32 hcg;
    static u32 pre_hcg=0;

    // search most suitable gain into gain table
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if (gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    if (gain_table[i].gain_x < 128)
        tmp = 0;
    else if (gain_table[i].gain_x < 256)
        tmp = 1;
    else if (gain_table[i].gain_x < 512)
        tmp = 3;
    else
        tmp = 7;

    a_gain = gain_table[i-1].a_gain;
    d_gain = gain_table[i-1].d_gain;
    
    g_psensor->curr_gain = gain_table[i-1].gain_x;

    hcg = pinfo->sensor_rev ? 0x30 : 0x00;
    hcg |= (gain_table[i-1].hcg ? 0 : 1);

    i = 0;
	//write_reg(0x5032, (d_gain & 0x1f00) >> 8);
    buf[i][0] = 0x50;
    buf[i][1] = 0x32;
    buf[i++][2] = (d_gain & 0x1f00) >> 8;
	//write_reg(0x5033, (d_gain & 0x00ff));
    buf[i][0] = 0x50;
    buf[i][1] = 0x33;
    buf[i++][2] = d_gain & 0x00ff;
	//write_reg(0x5034, (d_gain & 0x1f00) >> 8);
    buf[i][0] = 0x50;
    buf[i][1] = 0x34;
    buf[i++][2] = (d_gain & 0x1f00) >> 8;
	//write_reg(0x5035, (d_gain & 0x00ff));
    buf[i][0] = 0x50;
    buf[i][1] = 0x35;
    buf[i++][2] = d_gain & 0x00ff;
	//write_reg(0x5036, (d_gain & 0x1f00) >> 8);
    buf[i][0] = 0x50;
    buf[i][1] = 0x36;
    buf[i++][2] = (d_gain & 0x1f00) >> 8;
	//write_reg(0x5037, (d_gain & 0x00ff));
    buf[i][0] = 0x50;
    buf[i][1] = 0x37;
    buf[i++][2] = d_gain & 0x00ff;
    
	//write_reg(0x3508, (a_gain & 0x1f00) >> 8);
    buf[i][0] = 0x35;
    buf[i][1] = 0x08;
    buf[i++][2] = (a_gain & 0x1f00) >> 8;
	//write_reg(0x3509, (a_gain & 0x00ff));
    buf[i][0] = 0x35;
    buf[i][1] = 0x09;
    buf[i++][2] = a_gain & 0x00ff;
	//write_reg(0x366a, tmp);
    buf[i][0] = 0x36;
    buf[i][1] = 0x6a;
    buf[i++][2] = tmp & 0xff;
    
    for (i=0; i<9; i++) {
        msgs[i].addr  = I2C_ADDR;
        msgs[i].flags = 0;
        msgs[i].len   = 3;
        msgs[i].buf   = &buf[i][0];
    }
    sensor_i2c_transfer(msgs, 9);

    if (pre_hcg != hcg){
        pre_hcg = hcg;
        isp_api_wait_frame_start();
    	write_reg(0x37c7, hcg);
    }
    
    return ret;
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

    read_reg(0x3821, &reg);

    if (enable) {
        write_reg(0x3821, (reg | (BIT1 | BIT2)));
    } else {       
        write_reg(0x3821, (reg &~(BIT1 | BIT2)));
    }
    pinfo->mirror = enable;
        
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

    read_reg(0x3820, &reg);

    if (enable) {
        write_reg(0x450b, 0x20);
        write_reg(0x3820, (reg | (BIT2 | BIT1)));
        //may top lines error, needs to restart sensor streaming
        write_reg(0x0100, 0x00);
        write_reg(0x0100, 0x01);
    } else {        
        write_reg(0x450b, 0x00);
        write_reg(0x3820, (reg &~(BIT2 | BIT1)));
    }
    pinfo->flip = enable;
    
    return 0;
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
    int ret = 0, i, reg;
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;

    if (pinfo->is_init)
        return 0;

    read_reg(0x302c, &reg);
    pinfo->sensor_rev = reg & 0x0f;
    if (pinfo->sensor_rev)
        printk("sensor revision R1C\n");
    else
        printk("sensor revision R1A/R1B\n");

    if (g_psensor->interface == IF_MIPI){
        isp_info("Interface : MIPI\n");
        pInitTable=sensor_init_mipi_regs;
        InitTable_Num=NUM_OF_INIT_MIPI_REGS;
    }
    else{
        isp_info("Interface : Parallel\n");
        pInitTable=sensor_init_regs;
        InitTable_Num=NUM_OF_INIT_REGS;
    }
    // write initial register value
    for (i=0; i<InitTable_Num; i++) 
	{
        if (pInitTable[i].addr == DELAY_CODE) 
		{
			msleep(pInitTable[i].val);
        } 
        else if (write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) 
		{
            isp_err("failed to initial %s!", SENSOR_NAME);
            return -EINVAL;
        }
    }
    
	// set to LCG
	if (pinfo->sensor_rev){ // r1c
        if (g_psensor->interface == IF_MIPI){
   	        write_reg(0x3611, 0x56);
   	        write_reg(0x3612, 0x48);
   	        write_reg(0x3613, 0x5a);
   	        write_reg(0x3636, 0x13);

   	        write_reg(0x37c7, 0x31);
        }
        else{
   	        write_reg(0x37c7, 0x31);
        }
    }
	else{   // r1a, r1b
        if (g_psensor->interface == IF_MIPI){
   	        write_reg(0x3611, 0x5c);
   	        write_reg(0x3612, 0x18);
   	        write_reg(0x3613, 0x3a);
   	        write_reg(0x3636, 0x14);

   	        write_reg(0x37c7, 0x01);
        }
        else{
   	        write_reg(0x37c7, 0x01);
        }
    }
	
    
    // get pixel clock
    g_psensor->pclk = get_pclk(g_psensor->xclk);

    // set image size
    if ((ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height)) < 0)
        return ret;

    set_mirror(mirror);
    set_flip(flip);

    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current exposure and gain setting
    get_exp();
    get_gain();

	pinfo->is_init = true;

    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void ov9750_deconstruct(void)
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

static int ov9750_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->interface = interface;
    g_psensor->num_of_lane = ch_num;
    g_psensor->protocol = 0; 
    g_psensor->fmt = RAW10;
    g_psensor->bayer_type = BAYER_BG;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
	g_psensor->inv_clk = 0;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;
    g_psensor->max_exp = 5000; // 0.5 sec
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    g_psensor->exp_latency = 1;
    g_psensor->gain_latency = 1;
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

	if (g_psensor->interface == IF_PARALLEL){
    	if ((ret = init()) < 0)
        	goto construct_err;
	}

    return 0;

construct_err:
    ov9750_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init ov9750_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp328.ko(%x)!",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = ov9750_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit ov9750_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ov9750_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(ov9750_init);
module_exit(ov9750_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor ov9750");
MODULE_LICENSE("GPL");
