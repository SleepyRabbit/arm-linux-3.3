/**
 * @file sen_ov9732.c
 * OmniVision OV9732 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
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
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1280
#define DEFAULT_IMAGE_HEIGHT    720
#define DEFAULT_XCLK            27000000
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

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static ushort interface = DEFAULT_INTERFACE;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");


//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "OV9732"
#define SENSOR_MAX_WIDTH    1280
#define SENSOR_MAX_HEIGHT   720
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
    int long_exp;
} sensor_info_t;

typedef struct sensor_reg {
    u16 addr;
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_mipi_init_regs[] = {
#if 0	
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3007, 0x00},
    {0x3009, 0x02},
    {0x3010, 0x00},
    {0x3011, 0x08},
    {0x3014, 0x22},
    {0x301e, 0x15},
    {0x3030, 0x19},
    {0x3080, 0x02},
    {0x3081, 0x35},
    {0x3082, 0x04},
    {0x3083, 0x00},
    {0x3084, 0x02},
    {0x3085, 0x01},
    {0x3086, 0x01},
    {0x3089, 0x01},
    {0x308a, 0x00},
    {0x3103, 0x01},
    {0x3600, 0xf6},
    {0x3601, 0x72},
    {0x3610, 0x0c},
    {0x3611, 0xf0},
    {0x3612, 0x35},
    {0x3654, 0x10},
    {0x3655, 0x77},
    {0x3656, 0x77},
    {0x3657, 0x07},
    {0x3658, 0x22},
    {0x3659, 0x22},
    {0x365a, 0x02},
    {0x3700, 0x1f},
    {0x3701, 0x10},
    {0x3702, 0x0c},
    {0x3703, 0x0b},//AVDD 2.8V
    {0x3704, 0x3c},
    {0x3705, 0x51},//AVDD 2.8V
    {0x370d, 0x10},
    {0x3710, 0x0c},
    {0x3783, 0x08},
    {0x3784, 0x05},
    {0x3785, 0x55},
    {0x37c0, 0x07},
    {0x3800, 0x00},
    {0x3801, 0x04},
    {0x3802, 0x00},
    {0x3803, 0x04},
    {0x3804, 0x05},
    {0x3805, 0x0b},
    {0x3806, 0x02},
    {0x3807, 0xdb},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x02},
    {0x380b, 0xd0},
    {0x380c, 0x05},
    {0x380d, 0xc6},
    {0x380e, 0x03},
    {0x380f, 0x22},
    {0x3810, 0x00},
    {0x3811, 0x04},
    {0x3812, 0x00},
    {0x3813, 0x04},
    {0x3816, 0x00},
    {0x3817, 0x00},
    {0x3818, 0x00},
    {0x3819, 0x04},
    {0x3820, 0x10},
    {0x3821, 0x00},
    {0x382c, 0x06},
    {0x3500, 0x00},
    {0x3501, 0x31},
    {0x3502, 0x00},
    {0x3503, 0x03},
    {0x3504, 0x00},
    {0x3505, 0x00},
    {0x3509, 0x10},
    {0x350a, 0x00},
    {0x350b, 0x40},
    {0x3d00, 0x00},
    {0x3d01, 0x00},
    {0x3d02, 0x00},
    {0x3d03, 0x00},
    {0x3d04, 0x00},
    {0x3d05, 0x00},
    {0x3d06, 0x00},
    {0x3d07, 0x00},
    {0x3d08, 0x00},
    {0x3d09, 0x00},
    {0x3d0a, 0x00},
    {0x3d0b, 0x00},
    {0x3d0c, 0x00},
    {0x3d0d, 0x00},
    {0x3d0e, 0x00},
    {0x3d0f, 0x00},
    {0x3d80, 0x00},
    {0x3d81, 0x00},
    {0x3d82, 0x38},
    {0x3d83, 0xa4},
    {0x3d84, 0x00},
    {0x3d85, 0x00},
    {0x3d86, 0x1f},
    {0x3d87, 0x03},
    {0x3d8b, 0x00},
    {0x3d8f, 0x00},
    {0x4001, 0xe0},
    {0x4004, 0x00},
    {0x4005, 0x02},
    {0x4006, 0x01},
    {0x4007, 0x40},
    {0x4009, 0x0b},
    {0x4300, 0x03},
    {0x4301, 0xff},
    {0x4304, 0x00},
    {0x4305, 0x00},
    {0x4309, 0x00},
    {0x4600, 0x00},
    {0x4601, 0x04},
    {0x4800, 0x00},
    {0x4805, 0x00},
    {0x4821, 0x50},
    {0x4823, 0x50},
    {0x4837, 0x2d},
    {0x4a00, 0x00},
    {0x4f00, 0x80},
    {0x4f01, 0x10},
    {0x4f02, 0x00},
    {0x4f03, 0x00},
    {0x4f04, 0x00},
    {0x4f05, 0x00},
    {0x4f06, 0x00},
    {0x4f07, 0x00},
    {0x4f08, 0x00},
    {0x4f09, 0x00},
    {0x5000, 0x0f},//Disable AWB GAIN bit4
    {0x500c, 0x00},
    {0x500d, 0x00},
    {0x500e, 0x00},
    {0x500f, 0x00},
    {0x5010, 0x00},
    {0x5011, 0x00},
    {0x5012, 0x00},
    {0x5013, 0x00},
    {0x5014, 0x00},
    {0x5015, 0x00},
    {0x5016, 0x00},
    {0x5017, 0x00},
    {0x5080, 0x00},
    {0x5180, 0x01},
    {0x5181, 0x00},
    {0x5182, 0x01},
    {0x5183, 0x00},
    {0x5184, 0x01},
    {0x5185, 0x00},
    {0x5708, 0x06},
    {0x5781, 0x00},
    {0x5783, 0x0f},
    {0x0100, 0x01},   
#endif    
    
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3007, 0x1f},
    {0x3008, 0xff},
    {0x3009, 0x02},
    {0x3010, 0x00},
    {0x3011, 0x08},
    {0x3014, 0x22},
    {0x301e, 0x15},
    {0x3030, 0x19},
    {0x3080, 0x02},
    {0x3081, 0x35}, //3// c(35 for MCLK=27MHz, 3c for MCLK=24MHz, 20150708 tom modify)
    {0x3082, 0x04},
    {0x3083, 0x00},
    {0x3084, 0x02},
    {0x3085, 0x01},
    {0x3086, 0x01},
    {0x3089, 0x01},
    {0x308a, 0x00},
    {0x3103, 0x01},
    {0x3600, 0xf6},
    {0x3601, 0x72},
    {0x3610, 0x0c},
    {0x3611, 0xf0},
    {0x3612, 0x35},
    {0x3654, 0x10},
    {0x3655, 0x77},
    {0x3656, 0x77},
    {0x3657, 0x07},
    {0x3658, 0x22},
    {0x3659, 0x22},
    {0x365a, 0x02},
    {0x3700, 0x1f},
    {0x3701, 0x10},
    {0x3702, 0x0c},
    {0x3703, 0x07},
    {0x3704, 0x3c},
    {0x3705, 0x81},
    {0x370d, 0x20},
    {0x3710, 0x0c},
    {0x3782, 0x58},
    {0x3783, 0x60},
    {0x3784, 0x05},
    {0x3785, 0x55},
    {0x37c0, 0x07},
    {0x3800, 0x00},
    {0x3801, 0x04},
    {0x3802, 0x00},
    {0x3803, 0x04},
    {0x3804, 0x05},
    {0x3805, 0x0b},
    {0x3806, 0x02},
    {0x3807, 0xdb},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x02},
    {0x380b, 0xd0},
    {0x380c, 0x05},
    {0x380d, 0xc6},
    {0x380e, 0x03},
    {0x380f, 0x22},
    {0x3810, 0x00},
    {0x3811, 0x04},
    {0x3812, 0x00},
    {0x3813, 0x04},
    {0x3816, 0x00},
    {0x3817, 0x00},
    {0x3818, 0x00},
    {0x3819, 0x04},
    {0x3820, 0x10},
    {0x3821, 0x00},
    {0x382c, 0x06},
    {0x3500, 0x00},
    {0x3501, 0x31},
    {0x3502, 0x00},
    {0x3503, 0x03},
    {0x3504, 0x00},
    {0x3505, 0x00},
    {0x3509, 0x10},
    {0x350a, 0x00},
    {0x350b, 0x40},
    {0x3d00, 0x00},
    {0x3d01, 0x00},
    {0x3d02, 0x00},
    {0x3d03, 0x00},
    {0x3d04, 0x00},
    {0x3d05, 0x00},
    {0x3d06, 0x00},
    {0x3d07, 0x00},
    {0x3d08, 0x00},
    {0x3d09, 0x00},
    {0x3d0a, 0x00},
    {0x3d0b, 0x00},
    {0x3d0c, 0x00},
    {0x3d0d, 0x00},
    {0x3d0e, 0x00},
    {0x3d0f, 0x00},
    {0x3d80, 0x00},
    {0x3d81, 0x00},
    {0x3d82, 0x38},
    {0x3d83, 0xa4},
    {0x3d84, 0x00},
    {0x3d85, 0x00},
    {0x3d86, 0x1f},
    {0x3d87, 0x03},
    {0x3d8b, 0x00},
    {0x3d8f, 0x00},
    {0x4001, 0xe0},
    {0x4004, 0x00},
    {0x4005, 0x02},
    {0x4006, 0x01},
    {0x4007, 0x40},
    {0x4009, 0x0b},
    {0x4300, 0x03},
    {0x4301, 0xff},
    {0x4304, 0x00},
    {0x4305, 0x00},
    {0x4309, 0x00},
    {0x4600, 0x00},
    {0x4601, 0x04},
    {0x4800, 0x00},
    {0x4805, 0x00},
    {0x4821, 0x50},
    {0x4823, 0x50},
    {0x4837, 0x2d},
    {0x4a00, 0x00},
    {0x4f00, 0x80},
    {0x4f01, 0x10},
    {0x4f02, 0x00},
    {0x4f03, 0x00},
    {0x4f04, 0x00},
    {0x4f05, 0x00},
    {0x4f06, 0x00},
    {0x4f07, 0x00},
    {0x4f08, 0x00},
    {0x4f09, 0x00},
    {0x5000, 0x03}, //Disable AWB GAIN bit4;Disable WC/BC correction
    {0x500c, 0x00},
    {0x500d, 0x00},
    {0x500e, 0x00},
    {0x500f, 0x00},
    {0x5010, 0x00},
    {0x5011, 0x00},
    {0x5012, 0x00},
    {0x5013, 0x00},
    {0x5014, 0x00},
    {0x5015, 0x00},
    {0x5016, 0x00},
    {0x5017, 0x00},
    {0x5080, 0x00},
    {0x5180, 0x01},
    {0x5181, 0x00},
    {0x5182, 0x01},
    {0x5183, 0x00},
    {0x5184, 0x01},
    {0x5185, 0x00},
    {0x5708, 0x06},
    {0x5781, 0x00},
    {0x5783, 0x0f},
    {0x0100, 0x01},
    {0x3703, 0x0b},
    {0x3705, 0x51}    
    
};
#define NUM_OF_MIPI_INIT_REGS (sizeof(sensor_mipi_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_dvp_init_regs[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x3001, 0x3f},
    {0x3002, 0xff},
    {0x3007, 0x00},
    {0x3009, 0x03},
    {0x3010, 0x00},
    {0x3011, 0x00},
    {0x3014, 0x36},
    {0x301e, 0x15},
    {0x3030, 0x09},
    {0x3080, 0x02},
    {0x3081, 0x35},
    {0x3082, 0x04},
    {0x3083, 0x00},
    {0x3084, 0x02},
    {0x3085, 0x01},
    {0x3086, 0x01},
    {0x3089, 0x01},
    {0x308a, 0x00},
    {0x3103, 0x01},
    {0x3600, 0xf6},
    {0x3601, 0x72},
    {0x3610, 0x0c},
    {0x3611, 0xf0},
    {0x3612, 0x35},
    {0x3654, 0x10},
    {0x3655, 0x77},
    {0x3656, 0x77},
    {0x3657, 0x07},
    {0x3658, 0x22},
    {0x3659, 0x22},
    {0x365a, 0x02},
    {0x3700, 0x1f},
    {0x3701, 0x10},
    {0x3702, 0x0c},
    {0x3703, 0x0b},//AVDD 2.8V
    {0x3704, 0x3c},
    {0x3705, 0x51},//AVDD 2.8V
    {0x370d, 0x10},
    {0x3710, 0x0c},
    {0x3783, 0x08},
    {0x3784, 0x05},
    {0x3785, 0x55},
    {0x37c0, 0x07},
    {0x3800, 0x00},
    {0x3801, 0x04},
    {0x3802, 0x00},
    {0x3803, 0x04},
    {0x3804, 0x05},
    {0x3805, 0x0b},
    {0x3806, 0x02},
    {0x3807, 0xdb},
    {0x3808, 0x05},
    {0x3809, 0x00},
    {0x380a, 0x02},
    {0x380b, 0xd0},
    {0x380c, 0x05},
    {0x380d, 0xc6},
    {0x380e, 0x03},
    {0x380f, 0x22},
    {0x3810, 0x00},
    {0x3811, 0x04},
    {0x3812, 0x00},
    {0x3813, 0x04},
    {0x3816, 0x00},
    {0x3817, 0x00},
    {0x3818, 0x00},
    {0x3819, 0x04},
    {0x3820, 0x10},
    {0x3821, 0x00},
    {0x382c, 0x06},
    {0x3500, 0x00},
    {0x3501, 0x31},
    {0x3502, 0x00},
    {0x3503, 0x03},
    {0x3504, 0x00},
    {0x3505, 0x00},
    {0x3509, 0x10},
    {0x350a, 0x00},
    {0x350b, 0x40},
    {0x3d00, 0x00},
    {0x3d01, 0x00},
    {0x3d02, 0x00},
    {0x3d03, 0x00},
    {0x3d04, 0x00},
    {0x3d05, 0x00},
    {0x3d06, 0x00},
    {0x3d07, 0x00},
    {0x3d08, 0x00},
    {0x3d09, 0x00},
    {0x3d0a, 0x00},
    {0x3d0b, 0x00},
    {0x3d0c, 0x00},
    {0x3d0d, 0x00},
    {0x3d0e, 0x00},
    {0x3d0f, 0x00},
    {0x3d80, 0x00},
    {0x3d81, 0x00},
    {0x3d82, 0x38},
    {0x3d83, 0xa4},
    {0x3d84, 0x00},
    {0x3d85, 0x00},
    {0x3d86, 0x1f},
    {0x3d87, 0x03},
    {0x3d8b, 0x00},
    {0x3d8f, 0x00},
    {0x4001, 0xe0},
    {0x4004, 0x00},
    {0x4005, 0x02},
    {0x4006, 0x01},
    {0x4007, 0x40},
    {0x4009, 0x0b},
    {0x4300, 0x03},
    {0x4301, 0xff},
    {0x4304, 0x00},
    {0x4305, 0x00},
    {0x4309, 0x00},
    {0x4600, 0x00},
    {0x4601, 0x04},
    {0x4800, 0x04},
    {0x4805, 0x00},
    {0x4821, 0x3c},
    {0x4823, 0x3c},
    {0x4837, 0x2d},
    {0x4a00, 0x00},
    {0x4f00, 0x80},
    {0x4f01, 0x10},
    {0x4f02, 0x00},
    {0x4f03, 0x00},
    {0x4f04, 0x00},
    {0x4f05, 0x00},
    {0x4f06, 0x00},
    {0x4f07, 0x00},
    {0x4f08, 0x00},
    {0x4f09, 0x00},
    {0x5000, 0x03},//Disable AWB GAIN bit4,Disable WC/BC correction
    {0x500c, 0x00},
    {0x500d, 0x00},
    {0x500e, 0x00},
    {0x500f, 0x00},
    {0x5010, 0x00},
    {0x5011, 0x00},
    {0x5012, 0x00},
    {0x5013, 0x00},
    {0x5014, 0x00},
    {0x5015, 0x00},
    {0x5016, 0x00},
    {0x5017, 0x00},
    {0x5080, 0x00},
    {0x5180, 0x01},
    {0x5181, 0x00},
    {0x5182, 0x01},
    {0x5183, 0x00},
    {0x5184, 0x01},
    {0x5185, 0x00},
    {0x5708, 0x06},
    {0x5781, 0x00},
    {0x5783, 0x0f},
    {0x0100, 0x01},
};
#define NUM_OF_DVP_INIT_REGS (sizeof(sensor_dvp_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u32 a_gain;
    u32 gain_x;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {

    { 16,  64},{ 17,  68},{ 18,  72},{ 19,  76},
    { 20,  80},{ 21,  84},{ 22,  88},{ 23,  92},
    { 24,  96},{ 25, 100},{ 26, 104},{ 27, 108},
    { 28, 112},{ 29, 116},{ 30, 120},{ 31, 124},
    { 32, 128},{ 34, 136},{ 36, 144},{ 38, 152},
    { 40, 160},{ 42, 168},{ 44, 176},{ 46, 184},
    { 48, 192},{ 50, 200},{ 52, 208},{ 54, 216},
    { 56, 224},{ 58, 232},{ 60, 240},{ 62, 248},
    { 64, 256},{ 68, 272},{ 72, 288},{ 76, 304},
    { 80, 320},{ 84, 336},{ 88, 352},{ 92, 368},
    { 96, 384},{100, 400},{104, 416},{108, 432},
    {112, 448},{116, 464},{120, 480},{124, 496},
    {128, 512},{136, 544},{144, 576},{152, 608},
    {160, 640},{168, 672},{176, 704},{184, 736},
    {192, 768},{200, 800},{208, 832},{216, 864},
    {224, 896},{232, 928},{240, 960},{248, 992},
    {255,1084},

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

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 prediv, mul, sysdiv, pix_div;
    u32 prediv_tbl[16]={2,3,4,5,6,7,8,9,10,11,12,13,14,16,18,18};
    u32 pclk;

    read_reg(0x3080, &prediv);
    read_reg(0x3081, &mul);
    read_reg(0x3082, &sysdiv);
    read_reg(0x3083, &pix_div);

    prediv = prediv_tbl[prediv & 0x07]; //x2
    mul &= 0xff;
    sysdiv = (sysdiv & 0x0f) + 1;
    pix_div = ((pix_div & 0x01)+ 1) << 1;
    //printk("%d, %d, %d, %d, %d\n", prediv,mul,sysdiv,pix_div,sys_clk_div);
    pclk = xclk / prediv * mul / sysdiv / pix_div / 2;
    pclk *= 2;  //sclk = pll sclk / 2;
    pinfo->pclk = pclk;
    isp_info("xclk=%d, pclk=%d\n", xclk, pclk);    
    
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
    pinfo->t_row = (hts * 10000) / (pinfo->pclk / 1000);
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;

    read_reg(0x380E, &reg_h);
    read_reg(0x380F, &reg_l);
    pinfo->sw_lines_max = (((reg_h & 0x7F) << 8) | (reg_l & 0xFF)) - 4; // 1185

    read_reg(0x3502, &reg_l);
    read_reg(0x3501, &reg_h);
    read_reg(0x3500, &reg_msb);
    pinfo->prev_sw_reg = ((reg_msb & 0xFF) << 16) | ((reg_h & 0xFF) << 8) | (reg_l & 0xF);

    isp_info("hts=%d, t_row=%d, sw_lines_max=%d\n", hts, pinfo->t_row, pinfo->sw_lines_max);
}

static void adjust_blk(int fps)
{
    u32 vmax, max_fps;

    max_fps = 30;
    if (fps > max_fps)
        fps = max_fps;
    vmax = 802 * max_fps / fps;

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
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 lines, tmp;
    
    lines = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

	// printk("line %d,exp %d,t_row %d\n",lines,exp,pinfo->t_row);
    //lines = (exp * 1000) / pinfo->t_row;
    if (lines == 0)
        lines = 1;
   
    if (lines > pinfo->sw_lines_max) {
        write_reg(0x380f, (lines & 0xFF));
        write_reg(0x380e, (lines >> 8) & 0xFF);
        pinfo->long_exp = 1;
    } else {
        if (pinfo->long_exp){
            write_reg(0x380f, (pinfo->sw_lines_max & 0xFF));
            write_reg(0x380e, (pinfo->sw_lines_max >> 8) & 0xFF);
            pinfo->long_exp = 0;
        }
    }

    tmp = lines << 4;//lines << 4;

    if (tmp != pinfo->prev_sw_reg) {
        write_reg(0x3502, (tmp & 0xFF));
        write_reg(0x3501, (tmp >> 8) & 0xFF);
        write_reg(0x3500, (tmp >> 16) & 0xFF);
        pinfo->prev_sw_reg = tmp;
    }

    g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;

//    printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;
}

static u32 get_gain(void)
{
    u32 i, gain_x, a_gain, reg;
   
    if (g_psensor->curr_gain == 0) {
        read_reg(0x350B, &reg);
        a_gain = reg & 0xFF;
        
        gain_x = (a_gain / 16) * 64;
        
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
    int ret = 0, i, a_gain;

    // search most suitable gain into gain table
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if (gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    a_gain = gain_table[i-1].a_gain;

    g_psensor->curr_gain = gain_table[i-1].gain_x;
   
	write_reg(0x350B, a_gain & 0x00ff);

//    isp_info("set_gain %d, %d\n", gain, gain_table[i-1].gain_x);
    
    return ret;
}

static int get_mirror(void)
{
    u32 reg;

    read_reg(0x3820, &reg);
    return (reg & BIT3) ? 1 : 0;
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    read_reg(0x3820, &reg);

    if (enable) {
        write_reg(0x3820, reg | BIT3);
    } else {        
        write_reg(0x3820, reg &~BIT3);
    }
    pinfo->mirror = enable;
        
    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x3820, &reg);
    return (reg & BIT2) ? 1 : 0;
}

static int set_flip(int enable)
{  
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    read_reg(0x3820, &reg);

    if (enable) {
        write_reg(0x3820, reg | BIT2);
    } else {        
        write_reg(0x3820, reg &~BIT2);
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
    int ret = 0, i;

    if (pinfo->is_init)
        return 0;

    if (g_psensor->interface != IF_MIPI){
        isp_info("Sensor Interface : PARALLEL\n");
       // write initial register value
        for (i=0; i<NUM_OF_DVP_INIT_REGS; i++) {
            if (write_reg(sensor_dvp_init_regs[i].addr, sensor_dvp_init_regs[i].val) < 0) {
                isp_err("failed to initial %s!\n", SENSOR_NAME);
                return -EINVAL;
            }            
        }
    }
    else{
        isp_info("Sensor Interface : MIPI\n");
        // write initial register value
        for (i=0; i<NUM_OF_MIPI_INIT_REGS; i++) {
            if (write_reg(sensor_mipi_init_regs[i].addr, sensor_mipi_init_regs[i].val) < 0) {
                isp_err("failed to initial %s!\n", SENSOR_NAME);
                return -EINVAL;
            }
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
static void ov9732_deconstruct(void)
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

static int ov9732_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->num_of_lane = 1;
    g_psensor->protocol = 0; 
    g_psensor->fmt = RAW10;
    g_psensor->bayer_type = BAYER_BG;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
	g_psensor->inv_clk = inv_clk;
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

  	if ((ret = init()) < 0)
       	goto construct_err;

    return 0;

construct_err:
    ov9732_deconstruct();
    return ret;
}


//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init ov9732_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp328.ko(%x)!\n",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = ov9732_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit ov9732_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ov9732_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(ov9732_init);
module_exit(ov9732_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor OV9732");
MODULE_LICENSE("GPL");
