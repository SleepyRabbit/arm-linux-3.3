
/**
 * @file sen_ps5230.c
 * OmniVision ps5230 sensor driver
 *
 * Copyright (C) 2010 GM Corp. (http://www.grain-media.com)
 *
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
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1920
#define DEFAULT_IMAGE_HEIGHT    1080
#define DEFAULT_XCLK            27000000

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

//=============================================================================
// global
//============================================================================
#define SENSOR_NAME         "ps5230"
#define SENSOR_MAX_WIDTH    1920
#define SENSOR_MAX_HEIGHT   1080

static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    int mirror;
    int flip;
    u32 total_line;    
    u32 t_row;          // unit is 1/10 us
    int htp;
} sensor_info_t;

typedef struct sensor_reg {
    u8 addr;
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_init_regs[] = { 
#if 0    
    {0xEF, 0x00},                                               
    {0x06, 0x02},                                           
    {0x0B, 0x00},                                           
    {0x0C, 0xA0},                                           
    {0x10, 0x01},                                           
    {0x12, 0x80},                                           
    {0x13, 0x00},                                           
    {0x14, 0xFF},                                           
    {0x15, 0x03},                                           
    {0x16, 0xFF},                                           
    {0x17, 0xFF},                                           
    {0x18, 0xFF},                                           
    {0x19, 0x64},                                           
    {0x1A, 0x64},                                           
    {0x1B, 0x64},                                           
    {0x1C, 0x64},                                           
    {0x1D, 0x64},                                           
    {0x1E, 0x64},                                           
    {0x1F, 0x64},                                           
    {0x20, 0x64},                                           
    {0x21, 0x00},                                           
    {0x22, 0x00},                                           
    {0x23, 0x00},                                           
    {0x24, 0x00},                                           
    {0x25, 0x00},                                           
    {0x26, 0x00},                                           
    {0x27, 0x00},                                           
    {0x28, 0x00},                                           
    {0x29, 0x64},                                           
    {0x2A, 0x64},                                           
    {0x2B, 0x64},                                           
    {0x2C, 0x64},                                           
    {0x2D, 0x64},                                           
    {0x2E, 0x64},                                           
    {0x2F, 0x64},                                           
    {0x30, 0x64},                                           
    {0x31, 0x0F},                                           
    {0x32, 0x00},                                           
    {0x33, 0x64},                                           
    {0x34, 0x64},                                           
    {0x89, 0x10},                                           
    {0x8B, 0x00},                                           
    {0x8C, 0x00},                                           
    {0x8D, 0x00},                                           
    {0x8E, 0x00},                                           
    {0x8F, 0x00},                                           
    {0x90, 0x02},                                           
    {0x91, 0x00},                                           
    {0x92, 0x11},                                           
    {0x93, 0x10},                                           
    {0x94, 0x00},                                           
    {0x95, 0x00},                                           
    {0x96, 0x00},                                           
    {0x97, 0x00},                                           
    {0x99, 0x00},                                           
    {0x9A, 0x00},                                           
    {0x9B, 0x09},                                           
    {0x9C, 0x00},                                           
    {0x9D, 0x00},                                           
    {0x9E, 0x40},                                           
    {0x9F, 0x01},                                           
    {0xA0, 0x2C},                                           
    {0xA1, 0x01},                                           
    {0xA2, 0x2C},                                           
    {0xA3, 0x07},                                           
    {0xA4, 0xFF},                                           
    {0xA5, 0x03},                                           
    {0xA6, 0xFF},                                           
    {0xA7, 0x00},                                           
    {0xA8, 0x00},                                           
    {0xA9, 0x11},                                           
    {0xAA, 0x23},                                           
    {0xAB, 0x23},                                           
    {0xAD, 0x00},                                           
    {0xAE, 0x00},                                           
    {0xAF, 0x00},                                           
    {0xB0, 0x00},                                           
    {0xB1, 0x00},                                           
    {0xBE, 0x35},// V and H sync active high                
    {0xBF, 0x00},                                           
    {0xC0, 0x10},                                           
    {0xC7, 0x10},                                           
    {0xC8, 0x01},                                           
    {0xC9, 0x00},                                           
    {0xCA, 0x55},                                           
    {0xCB, 0x06},                                           
    {0xCC, 0x09},                                           
    {0xCD, 0x00},                                           
    {0xCE, 0xA2},                                           
    {0xCF, 0x00},                                           
    {0xD0, 0x02},                                           
    {0xD1, 0x10},                                           
    {0xD2, 0x1E},                                           
    {0xD3, 0x19},                                           
    {0xD4, 0x04},                                           
    {0xD5, 0x18},                                           
    {0xD6, 0xC8},                                           
    {0xF0, 0x00},                                           
    {0xF1, 0x00},                                           
    {0xF2, 0x00},                                           
    {0xF3, 0x00},                                           
    {0xF4, 0x00},                                           
    {0xF5, 0x40},                                           
    {0xF6, 0x00},                                           
    {0xF7, 0x00},                                           
    {0xF8, 0x00},                                           
    {0xED, 0x01},                                           
    {0xEF, 0x01},                                           
    {0x02, 0xFF},                                           
    {0x03, 0x01},                                           
    {0x04, 0x45},                                           
    {0x05, 0x03},                                           
    {0x06, 0xFF},                                           
    {0x07, 0x00},                                           
    {0x08, 0x00},                                           
    {0x09, 0x00},                                           
    {0x0A, 0x04},                                           
    {0x0B, 0x64},                                           
    {0x0C, 0x00},                                           
    {0x0D, 0x02},                                           
    {0x0E, 0x00},                                           
    {0x0F, 0x92},                                           
    {0x10, 0x00},                                           
    {0x11, 0x01},                                           
    {0x12, 0x00},                                           
    {0x13, 0x92},                                           
    {0x14, 0x01},                                           
    {0x15, 0x00},                                           
    {0x16, 0x00},                                           
    {0x17, 0x00},                                           
    {0x1A, 0x00},                                           
    {0x1B, 0x07},                                           
    {0x1C, 0x90},                                           
    {0x1D, 0x04},                                           
    {0x1E, 0x4A},                                           
    {0x1F, 0x00},                                           
    {0x20, 0x00},                                           
    {0x21, 0x00},                                           
    {0x22, 0xD4},                                           
    {0x23, 0x10},                                           
    {0x24, 0xA0},                                           
    {0x25, 0x00},                                           
    {0x26, 0x08},                                           
    {0x27, 0x08},                                           
    {0x28, 0x98},                                           
    {0x29, 0x0F},                                           
    {0x2A, 0x08},                                           
    {0x2B, 0x97},                                           
    {0x2C, 0x10},                                           
    {0x2D, 0x06},                                           
    {0x2E, 0x3C},                                           
    {0x2F, 0x10},                                           
    {0x30, 0x05},                                           
    {0x31, 0x3E},                                           
    {0x32, 0x10},                                           
    {0x33, 0x05},                                           
    {0x34, 0x00},                                           
    {0x35, 0x00},                                           
    {0x36, 0x00},                                           
    {0x37, 0xFE},                                           
    {0x38, 0x55},                                           
    {0x39, 0x00},                                           
    {0x3A, 0x18},                                           
    {0x3B, 0x16},                                           
    {0x3C, 0x02},                                           
    {0x3D, 0x02},                                           
    {0x3E, 0x11},                                           
    {0x3F, 0x26},                                           
    {0x40, 0x04},                                           
    {0x41, 0x15},                                           
    {0x42, 0x40},                                           
    {0x43, 0x14},                                           
    {0x44, 0x05},                                           
    {0x45, 0x15},                                           
    {0x46, 0x14},                                           
    {0x47, 0x05},                                           
    {0x48, 0x1F},                                           
    {0x49, 0x0A},                                           
    {0x4A, 0x12},                                           
    {0x4B, 0x08},                                           
    {0x4C, 0x26},                                           
    {0x4D, 0x1E},                                           
    {0x4E, 0x01},                                           
    {0x4F, 0x0E},                                           
    {0x50, 0x04},                                           
    {0x51, 0x05},                                           
    {0x52, 0x23},                                           
    {0x53, 0x04},                                           
    {0x54, 0x00},                                           
    {0x55, 0x00},                                           
    {0x56, 0x8D},                                           
    {0x57, 0x00},                                           
    {0x58, 0x92},                                           
    {0x59, 0x00},                                           
    {0x5A, 0xD6},                                           
    {0x5B, 0x00},                                           
    {0x5C, 0x2E},                                           
    {0x5D, 0x01},                                           
    {0x5E, 0x9E},                                           
    {0x5F, 0x00},                                           
    {0x60, 0x3C},                                           
    {0x61, 0x08},                                           
    {0x62, 0x4C},                                           
    {0x63, 0x62},                                           
    {0x64, 0x02},                                           
    {0x65, 0x00},                                           
    {0x66, 0x00},                                           
    {0x67, 0x00},                                           
    {0x68, 0x00},                                           
    {0x69, 0x00},                                           
    {0x6A, 0x00},                                           
    {0x6B, 0x92},                                           
    {0x6C, 0x00},                                           
    {0x6D, 0x64},                                           
    {0x6E, 0x01},                                           
    {0x6F, 0xEE},                                           
    {0x70, 0x00},                                           
    {0x71, 0x64},                                           
    {0x72, 0x08},                                           
    {0x73, 0x98},                                           
    {0x74, 0x00},                                           
    {0x75, 0x00},                                           
    {0x76, 0x00},                                           
    {0x77, 0x00},                                           
    {0x78, 0x0A},                                           
    {0x79, 0xAB},                                           
    {0x7A, 0x50},                                           
    {0x7B, 0x40},                                           
    {0x7C, 0x40},                                           
    {0x7D, 0x00},                                           
    {0x7E, 0x01},                                           
    {0x7F, 0x00},                                           
    {0x80, 0x00},                                           
    {0x87, 0x00},                                           
    {0x88, 0x08},                                           
    {0x89, 0x01},                                           
    {0x8A, 0x04},                                           
    {0x8B, 0x4A},                                           
    {0x8C, 0x00},                                           
    {0x8D, 0x01},                                           
    {0x8E, 0x00},                                           
    {0x90, 0x00},                                           
    {0x91, 0x01},                                           
    {0x92, 0x80},                                           
    {0x93, 0x00},                                           
    {0x94, 0xFF},                                           
    {0x95, 0x00},                                           
    {0x96, 0x00},                                           
    {0x97, 0x01},                                           
    {0x98, 0x02},                                           
    {0x99, 0x09},                                           
    {0x9A, 0x60},                                           
    {0x9B, 0x02},                                           
    {0x9C, 0x60},                                           
    {0x9D, 0x00},                                           
    {0x9E, 0x00},                                           
    {0x9F, 0x00},                                           
    {0xA0, 0x00},                                           
    {0xA1, 0x00},                                           
    {0xA2, 0x00},                                           
    {0xA3, 0x00},                                           
    {0xA4, 0x14},                                           
    {0xA5, 0x04},                                           
    {0xA6, 0x48},//workaround image_h need >= 0x48, ori=0x38
    {0xA7, 0x00},                                           
    {0xA8, 0x08},                                           
    {0xA9, 0x07},                                           
    {0xAA, 0x80},                                           
    {0xAB, 0x01},                                           
    {0xAD, 0x00},                                           
    {0xAE, 0x00},                                           
    {0xAF, 0x00},                                           
    {0xB0, 0x50},                                           
    {0xB1, 0x00},                                           
    {0xB2, 0x00},                                           
    {0xB3, 0x00},                                           
    {0xB4, 0x50},                                           
    {0xB5, 0x07},                                           
    {0xB6, 0x80},                                           
    {0xB7, 0x82},                                           
    {0xB8, 0x0A},                                           
    {0xCD, 0x95},                                           
    {0xCE, 0xF6},                                           
    {0xCF, 0x01},                                           
    {0xD0, 0x42},                                           
    {0xD1, 0x30},                                           
    {0xD2, 0x00},                                           
    {0xD3, 0x1C},                                           
    {0xD4, 0x00},                                           
    {0xD5, 0x01},                                           
    {0xD6, 0x00},                                           
    {0xD7, 0x07},                                           
    {0xD8, 0x84},                                           
    {0xD9, 0x54},                                           
    {0xDA, 0x70},                                           
    {0xDB, 0x70},                                           
    {0xDC, 0x10},                                           
    {0xDD, 0x60},                                           
    {0xDE, 0x50},                                           
    {0xDF, 0x43},                                           
    {0xE0, 0x40},                                           
    {0xE1, 0x11},                                           
    {0xE2, 0x3D},                                           
    {0xE3, 0x21},                                           
    {0xE4, 0x66},                                           
    {0xE6, 0x00},                                           
    {0xE7, 0x00},                                           
    {0xF0, 0x00},                                           
    {0xF4, 0x02},                                           
    {0xF2, 0x1F},                                           
    {0xF1, 0x16},                                           
    {0xF5, 0x12},                                           
    {0xF6, 0xAF},                                           
    {0xF7, 0x07},                                           
    {0x09, 0x01},                                           
    {0xEF, 0x02},                                           
    //{0x00, 0x4A},                                           
    {0x00, 0x48},// Turn off high temperature control                                       
    {0x01, 0xA0},                                           
    {0x02, 0x03},                                           
    {0x03, 0x00},                                           
    {0x04, 0x00},                                           
    {0x05, 0x30},                                           
    {0x06, 0x04},                                           
    {0x07, 0x01},                                           
    {0x08, 0x03},                                           
    {0x09, 0x00},                                           
    {0x0A, 0x30},                                           
    {0x0B, 0x00},                                           
    {0x0C, 0x2B},                                           
    {0x0D, 0x16},                                           
    {0x0E, 0x00},                                           
    {0x0F, 0x00},                                           
    {0x10, 0x01},                                           
    {0x11, 0xD1},                                           
    {0x12, 0x74},                                           
    {0x13, 0x36},                                           
    {0x14, 0x21},                                           
    {0x15, 0x1F},                                           
    {0x16, 0xA5},                                           
    {0x17, 0xB0},                                           
    {0x18, 0x77},                                           
    {0x19, 0x40},                                           
    {0x1A, 0x00},                                           
    {0x30, 0xFF},                                           
    {0x31, 0x06},                                           
    {0x32, 0x07},                                           
    {0x33, 0x85},                                           
    {0x34, 0x00},                                           
    {0x35, 0x00},                                           
    {0x36, 0x01},                                           
    {0x37, 0x00},                                           
    {0x38, 0x00},                                           
    {0x39, 0x00},                                           
    {0x3A, 0xCE},                                           
    {0x3B, 0x17},                                           
    {0x3C, 0x64},                                           
    {0x3D, 0x04},                                           
    {0x3E, 0x00},                                           
    {0x3F, 0x0A},                                           
    {0x40, 0x04},                                           
    {0x41, 0x05},                                           
    {0x42, 0x04},                                           
    {0x43, 0x05},                                           
    {0x45, 0x00},                                           
    {0x46, 0x00},                                           
    {0x47, 0x00},                                           
    {0x48, 0x00},                                           
    {0x49, 0x00},                                           
    {0x4A, 0x00},                                           
    {0x4B, 0x00},                                           
    {0x4C, 0x00},                                           
    {0x4D, 0x00},                                           
    {0x88, 0x07},                                           
    {0x89, 0x22},                                           
    {0x8A, 0x00},                                           
    {0x8B, 0x14},                                           
    {0x8C, 0x00},                                           
    {0x8D, 0x00},                                           
    {0x8E, 0x52},                                           
    {0x8F, 0x60},                                           
    {0x90, 0x20},                                           
    {0x91, 0x00},                                           
    {0x92, 0x01},                                           
    {0x9E, 0x00},                                           
    {0x9F, 0x40},                                           
    {0xA0, 0x30},                                           
    {0xA1, 0x00},                                           
    {0xA2, 0x00},                                           
    {0xA3, 0x03},                                           
    {0xA4, 0xC0},                                           
    {0xA5, 0x00},                                           
    {0xA6, 0x00},                                           
    {0xA7, 0x00},                                           
    {0xA8, 0x00},                                           
    {0xA9, 0x00},                                           
    {0xAA, 0x00},                                           
    {0xAB, 0x00},                                           
    {0xAC, 0x00},                                           
    {0xB7, 0x00},                                           
    {0xB8, 0x00},                                           
    {0xB9, 0x00},                                           
    {0xBA, 0x00},                                           
    {0xBB, 0x00},                                           
    {0xBC, 0x00},                                           
    {0xBD, 0x00},                                           
    {0xBE, 0x00},                                           
    {0xBF, 0x00},                                           
    {0xC0, 0x00},                                           
    {0xC1, 0x00},                                           
    {0xC2, 0x00},                                           
    {0xC3, 0x00},                                           
    {0xC4, 0x00},                                           
    {0xC5, 0x00},                                           
    {0xC6, 0x00},                                           
    {0xC7, 0x00},                                           
    {0xC8, 0x00},                                           
    {0xC9, 0x00},                                           
    {0xCA, 0x00},                                           
    {0xED, 0x01},                                           
    {0xEF, 0x05},                                           
    {0x03, 0x10},                                           
    {0x04, 0xE0},                                           
    {0x05, 0x01},                                           
    {0x06, 0x00},                                           
    {0x07, 0x80},                                           
    {0x08, 0x02},                                           
    {0x09, 0x09},                                           
    {0x0A, 0x04},                                           
    {0x0B, 0x06},                                           
    {0x0C, 0x0C},                                           
    {0x0D, 0xA1},                                           
    {0x0E, 0x00},                                           
    {0x0F, 0x00},                                           
    {0x10, 0x01},                                           
    {0x11, 0x00},                                           
    {0x12, 0x00},                                           
    {0x13, 0x00},                                           
    {0x14, 0xB8},                                           
    {0x15, 0x07},                                           
    {0x16, 0x06},                                           
    {0x17, 0x06},                                           
    {0x18, 0x03},                                           
    {0x19, 0x04},                                           
    {0x1A, 0x06},                                           
    {0x1B, 0x06},                                           
    {0x1C, 0x07},                                           
    {0x1D, 0x08},                                           
    {0x1E, 0x1A},                                           
    {0x1F, 0x00},                                           
    {0x20, 0x00},                                           
    {0x21, 0x1E},                                           
    {0x22, 0x1E},                                           
    {0x23, 0x01},                                           
    {0x24, 0x04},                                           
    {0x27, 0x00},                                           
    {0x28, 0x00},                                           
    {0x2A, 0x08},                                           
    {0x2B, 0x02},                                           
    {0x2C, 0xA4},                                           
    {0x2D, 0x06},                                           
    {0x2E, 0x00},                                           
    {0x2F, 0x05},                                           
    {0x30, 0xE0},                                           
    {0x31, 0x01},                                           
    {0x32, 0x00},                                           
    {0x33, 0x00},                                           
    {0x34, 0x00},                                           
    {0x35, 0x00},                                           
    {0x36, 0x00},                                           
    {0x37, 0x00},                                           
    {0x38, 0x0E},                                           
    {0x39, 0x01},                                           
    {0x3A, 0x02},                                           
    {0x3B, 0x01},                                           
    {0x3C, 0x00},                                           
    {0x3D, 0x00},                                           
    {0x3E, 0x00},                                           
    {0x3F, 0x00},                                           
    {0x40, 0x16},                                           
    {0x41, 0x26},                                           
    {0x42, 0x00},                                           
    {0x47, 0x05},                                           
    {0x48, 0x07},                                           
    {0x49, 0x01},                                           
    {0x4D, 0x02},                                           
    {0x4F, 0x00},                                           
    {0x54, 0x05},                                           
    {0x55, 0x01},                                           
    {0x56, 0x05},                                           
    {0x57, 0x01},                                           
    {0x58, 0x02},                                           
    {0x59, 0x01},                                           
    {0x5B, 0x00},                                           
    {0x5C, 0x03},                                           
    {0x5D, 0x00},                                           
    {0x5E, 0x07},                                           
    {0x5F, 0x08},                                           
    {0x60, 0x00},                                           
    {0x61, 0x00},                                           
    {0x62, 0x00},                                           
    {0x63, 0x28},                                           
    {0x64, 0x30},                                           
    {0x65, 0x9E},                                           
    {0x66, 0xB9},                                           
    {0x67, 0x52},                                           
    {0x68, 0x70},                                           
    {0x69, 0x4E},                                           
    {0x70, 0x00},                                           
    {0x71, 0x00},                                           
    {0x72, 0x00},                                           
    {0x90, 0x04},                                           
    {0x91, 0x01},                                           
    {0x92, 0x00},                                           
    {0x93, 0x00},                                           
    {0x94, 0x03},                                           
    {0x96, 0x00},                                           
    {0x97, 0x01},                                           
    {0x98, 0x01},                                           
    {0xA0, 0x00},                                           
    {0xA1, 0x01},                                           
    {0xA2, 0x00},                                           
    {0xA3, 0x01},                                           
    {0xA4, 0x00},                                           
    {0xA5, 0x01},                                           
    {0xA6, 0x00},                                           
    {0xA7, 0x00},                                           
    {0xAA, 0x00},                                           
    {0xAB, 0x0F},                                           
    {0xAC, 0x08},                                           
    {0xAD, 0x09},                                           
    {0xAE, 0x0A},                                           
    {0xAF, 0x0B},                                           
    {0xB0, 0x00},                                           
    {0xB1, 0x00},                                           
    {0xB2, 0x01},                                           
    {0xB3, 0x00},                                           
    {0xB4, 0x00},                                           
    {0xB5, 0x0A},                                           
    {0xB6, 0x0A},                                           
    {0xB7, 0x0A},                                           
    {0xB8, 0x0A},                                           
    {0xB9, 0x00},                                           
    {0xBA, 0x00},                                           
    {0xBB, 0x00},                                           
    {0xBC, 0x00},                                           
    {0xBD, 0x00},                                           
    {0xBE, 0x00},                                           
    {0xBF, 0x00},                                           
    {0xC0, 0x00},                                           
    {0xC1, 0x00},                                           
    {0xC2, 0x00},                                           
    {0xC3, 0x00},                                           
    {0xC4, 0x00},                                           
    {0xC5, 0x00},                                           
    {0xC6, 0x00},                                           
    {0xC7, 0x00},                                           
    {0xC8, 0x00},                                           
    {0xD3, 0x80},                                           
    {0xD4, 0x00},                                           
    {0xD5, 0x00},                                           
    {0xD6, 0x03},                                           
    {0xD7, 0x77},                                           
    {0xD8, 0x00},                                           
    {0xED, 0x01},                                           
    {0xEF, 0x00},                                           
    {0x11, 0x00},                                           
    {0xEF, 0x01},                                           
    {0x02, 0xFB},                                           
    {0x05, 0x01},                                           
    {0x09, 0x01},      
#endif 
    //20160426 Better Random Noise and Thermal noise
    {0xEF, 0x00},
    {0x06, 0x02},
    {0x0B, 0x00},
    {0x0C, 0xA0},
    {0x10, 0x01},
    {0x12, 0x80},
    {0x13, 0x00},
    {0x14, 0xFF},
    {0x15, 0x03},
    {0x16, 0xFF},
    {0x17, 0xFF},
    {0x18, 0xFF},
    {0x19, 0x64},
    {0x1A, 0x64},
    {0x1B, 0x64},
    {0x1C, 0x64},
    {0x1D, 0x64},
    {0x1E, 0x64},
    {0x1F, 0x64},
    {0x20, 0x64},
    {0x21, 0x00},
    {0x22, 0x00},
    {0x23, 0x00},
    {0x24, 0x00},
    {0x25, 0x00},
    {0x26, 0x00},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x29, 0x64},
    {0x2A, 0x64},
    {0x2B, 0x64},
    {0x2C, 0x64},
    {0x2D, 0x64},
    {0x2E, 0x64},
    {0x2F, 0x64},
    {0x30, 0x64},
    {0x31, 0x0F},
    {0x32, 0x00},
    {0x33, 0x64},
    {0x34, 0x64},
    {0x89, 0x10},
    {0x8B, 0x00},
    {0x8C, 0x00},
    {0x8D, 0x00},
    {0x8E, 0x00},
    {0x8F, 0x00},
    {0x90, 0x02},
    {0x91, 0x00},
    {0x92, 0x11},
    {0x93, 0x10},
    {0x94, 0x00},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0x00},
    {0x99, 0x00},
    {0x9A, 0x00},
    {0x9B, 0x09},
    {0x9C, 0x00},
    {0x9D, 0x00},
    {0x9E, 0x40},
    {0x9F, 0x01},
    {0xA0, 0x2C},
    {0xA1, 0x01},
    {0xA2, 0x2C},
    {0xA3, 0x07},
    {0xA4, 0xFF},
    {0xA5, 0x03},
    {0xA6, 0xFF},
    {0xA7, 0x00},
    {0xA8, 0x00},
    {0xA9, 0x11},
    {0xAA, 0x23},
    {0xAB, 0x23},
    {0xAD, 0x00},
    {0xAE, 0x00},
    {0xAF, 0x00},
    {0xB0, 0x00},
    {0xB1, 0x00},
    {0xBE, 0x35},// V and H sync active high
    {0xBF, 0x00},
    {0xC0, 0x10},
    {0xC7, 0x10},
    {0xC8, 0x01},
    {0xC9, 0x00},
    {0xCA, 0x55},
    {0xCB, 0x06},
    {0xCC, 0x09},
    {0xCD, 0x00},
    {0xCE, 0xA2},
    {0xCF, 0x00},
    {0xD0, 0x02},
    {0xD1, 0x10},
    {0xD2, 0x1E},
    {0xD3, 0x19},
    {0xD4, 0x04},
    {0xD5, 0x18},
    {0xD6, 0xC8},
    {0xF0, 0x00},
    {0xF1, 0x00},
    {0xF2, 0x00},
    {0xF3, 0x00},
    {0xF4, 0x00},
    {0xF5, 0x40},
    {0xF6, 0x00},
    {0xF7, 0x00},
    {0xF8, 0x00},
    {0xED, 0x01},
    {0xEF, 0x01},
    {0x02, 0xFF},
    {0x03, 0x01},
    {0x04, 0x45},
    {0x05, 0x03},
    {0x06, 0xFF},
    {0x07, 0x00},
    {0x08, 0x00},
    {0x09, 0x00},
    {0x0A, 0x04},
    {0x0B, 0x64},
    {0x0C, 0x00},
    {0x0D, 0x02},
    {0x0E, 0x00},
    {0x0F, 0x92},
    {0x10, 0x00},
    {0x11, 0x01},
    {0x12, 0x00},
    {0x13, 0x92},
    {0x14, 0x01},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x1A, 0x00},
    {0x1B, 0x07},
    {0x1C, 0x90},
    {0x1D, 0x04},
    {0x1E, 0x4A},
    {0x1F, 0x00},
    {0x20, 0x00},
    {0x21, 0x00},
    {0x22, 0xD4},
    {0x23, 0x10},
    {0x24, 0xA0},
    {0x25, 0x00},
    {0x26, 0x08},
    {0x27, 0x08},
    {0x28, 0x98},
    {0x29, 0x0F},
    {0x2A, 0x08},
    {0x2B, 0x97},
    {0x2C, 0x10},
    {0x2D, 0x06},
    {0x2E, 0x3C},
    {0x2F, 0x10},
    {0x30, 0x05},
    {0x31, 0x3E},
    {0x32, 0x10},
    {0x33, 0x05},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0x00},
    {0x37, 0xFE},
    {0x38, 0x55},
    {0x39, 0x00},
    {0x3A, 0x18},
    {0x3B, 0x16},
    {0x3C, 0x02},
    {0x3D, 0x02},
    {0x3E, 0x11},
    {0x3F, 0x26},
    {0x40, 0x04},
    {0x41, 0x15},
    {0x42, 0x40},
    {0x43, 0x14},
    {0x44, 0x05},
    {0x45, 0x15},
    {0x46, 0x14},
    {0x47, 0x05},
    {0x48, 0x1F},
    {0x49, 0x0A},
    {0x4A, 0x12},
    {0x4B, 0x08},
    {0x4C, 0x26},
    {0x4D, 0x1E},
    {0x4E, 0x01},
    {0x4F, 0x0E},
    {0x50, 0x04},
    {0x51, 0x05},
    {0x52, 0x23},
    {0x53, 0x04},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x56, 0x78},
    {0x57, 0x00},
    {0x58, 0x92},
    {0x59, 0x00},
    {0x5A, 0xD6},
    {0x5B, 0x00},
    {0x5C, 0x2E},
    {0x5D, 0x01},
    {0x5E, 0x9E},
    {0x5F, 0x00},
    {0x60, 0x3C},
    {0x61, 0x08},
    {0x62, 0x4C},
    {0x63, 0x62},
    {0x64, 0x02},
    {0x65, 0x00},
    {0x66, 0x00},
    {0x67, 0x00},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6A, 0x00},
    {0x6B, 0x92},
    {0x6C, 0x00},
    {0x6D, 0x64},
    {0x6E, 0x01},
    {0x6F, 0xEE},
    {0x70, 0x00},
    {0x71, 0x64},
    {0x72, 0x08},
    {0x73, 0x98},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x0A},
    {0x79, 0xAB},
    {0x7A, 0x50},
    {0x7B, 0x40},
    {0x7C, 0x40},
    {0x7D, 0x00},
    {0x7E, 0x01},
    {0x7F, 0x00},
    {0x80, 0x00},
    {0x87, 0x00},
    {0x88, 0x08},
    {0x89, 0x01},
    {0x8A, 0x04},
    {0x8B, 0x4A},
    {0x8C, 0x00},
    {0x8D, 0x01},
    {0x8E, 0x00},
    {0x90, 0x00},
    {0x91, 0x01},
    {0x92, 0x80},
    {0x93, 0x00},
    {0x94, 0xFF},
    {0x95, 0x00},
    {0x96, 0x00},
    {0x97, 0x01},
    {0x98, 0x02},
    {0x99, 0x09},
    {0x9A, 0x60},
    {0x9B, 0x02},
    {0x9C, 0x60},
    {0x9D, 0x00},
    {0x9E, 0x00},
    {0x9F, 0x00},
    {0xA0, 0x00},
    {0xA1, 0x00},
    {0xA2, 0x00},
    {0xA3, 0x00},
    {0xA4, 0x14},
    {0xA5, 0x04},
    {0xA6, 0x48},//workaround image_h need >= 0x48, ori=0x38
    {0xA7, 0x00},
    {0xA8, 0x08},
    {0xA9, 0x07},
    {0xAA, 0x80},
    {0xAB, 0x01},
    {0xAD, 0x00},
    {0xAE, 0x00},
    {0xAF, 0x00},
    {0xB0, 0x50},
    {0xB1, 0x00},
    {0xB2, 0x00},
    {0xB3, 0x00},
    {0xB4, 0x50},
    {0xB5, 0x07},
    {0xB6, 0x80},
    {0xB7, 0x82},
    {0xB8, 0x0A},
    {0xCD, 0x95},
    {0xCE, 0xF6},
    {0xCF, 0x01},
    {0xD0, 0x42},
    {0xD1, 0x3C},
    {0xD2, 0x00},
    {0xD3, 0x1C},
    {0xD4, 0x00},
    {0xD5, 0x01},
    {0xD6, 0x00},
    {0xD7, 0x07},
    {0xD8, 0x84},
    {0xD9, 0x54},
    {0xDA, 0x70},
    {0xDB, 0x70},
    {0xDC, 0x10},
    {0xDD, 0x60},
    {0xDE, 0x50},
    {0xDF, 0x43},
    {0xE0, 0x50},
    {0xE1, 0x01},
    {0xE2, 0x35},
    {0xE3, 0x21},
    {0xE4, 0x66},
    {0xE6, 0x00},
    {0xE7, 0x00},
    {0xF5, 0x02},
    {0xF6, 0xA8},
    {0xF7, 0x03},
    {0xF0, 0x00},
    {0xF4, 0x02},
    {0xF2, 0x1F},
    {0xF1, 0x16},
    {0xF5, 0x12},
    {0x09, 0x01},
    {0xEF, 0x02},
    //{0x00, 0x4A},
    {0x00, 0x48},//Turn off high temperature control   
    {0x01, 0xA0},
    {0x02, 0x03},
    {0x03, 0x00},
    {0x04, 0x00},
    {0x05, 0x30},
    {0x06, 0x04},
    {0x07, 0x01},
    {0x08, 0x03},
    {0x09, 0x00},
    {0x0A, 0x30},
    {0x0B, 0x00},
    {0x0C, 0x00},
    {0x0D, 0x08},
    {0x0E, 0x20},
    {0x0F, 0x32},
    {0x10, 0x42},
    {0x11, 0x75},
    {0x12, 0x13},
    {0x13, 0x57},
    {0x14, 0x25},
    {0x15, 0x45},
    {0x16, 0xB9},
    {0x17, 0x31},
    {0x18, 0x43},
    {0x19, 0x34},
    {0x1A, 0x20},
    {0x30, 0xFF},
    {0x31, 0x06},
    {0x32, 0x07},
    {0x33, 0x85},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0x00},
    {0x37, 0x01},
    {0x38, 0x00},
    {0x39, 0x00},
    {0x3A, 0xCE},
    {0x3B, 0x17},
    {0x3C, 0x64},
    {0x3D, 0x04},
    {0x3E, 0x00},
    {0x3F, 0x0A},
    {0x40, 0x04},
    {0x41, 0x05},
    {0x42, 0x04},
    {0x43, 0x05},
    {0x45, 0x00},
    {0x46, 0x00},
    {0x47, 0x00},
    {0x48, 0x00},
    {0x49, 0x00},
    {0x4A, 0x00},
    {0x4B, 0x00},
    {0x4C, 0x00},
    {0x4D, 0x00},
    {0x88, 0x07},
    {0x89, 0x22},
    {0x8A, 0x00},
    {0x8B, 0x14},
    {0x8C, 0x00},
    {0x8D, 0x00},
    {0x8E, 0x52},
    {0x8F, 0x60},
    {0x90, 0x20},
    {0x91, 0x00},
    {0x92, 0x01},
    {0x9E, 0x00},
    {0x9F, 0x40},
    {0xA0, 0x30},
    {0xA1, 0x00},
    {0xA2, 0x00},
    {0xA3, 0x03},
    {0xA4, 0xC0},
    {0xA5, 0x00},
    {0xA6, 0x00},
    {0xA7, 0x00},
    {0xA8, 0x00},
    {0xA9, 0x00},
    {0xAA, 0x00},
    {0xAB, 0x00},
    {0xAC, 0x00},
    {0xB7, 0x00},
    {0xB8, 0x00},
    {0xB9, 0x00},
    {0xBA, 0x00},
    {0xBB, 0x00},
    {0xBC, 0x00},
    {0xBD, 0x00},
    {0xBE, 0x00},
    {0xBF, 0x00},
    {0xC0, 0x00},
    {0xC1, 0x00},
    {0xC2, 0x00},
    {0xC3, 0x00},
    {0xC4, 0x00},
    {0xC5, 0x00},
    {0xC6, 0x00},
    {0xC7, 0x00},
    {0xC8, 0x00},
    {0xC9, 0x00},
    {0xCA, 0x00},
    {0xED, 0x01},
    {0xEF, 0x05},
    {0x03, 0x10},
    {0x04, 0xE0},
    {0x05, 0x01},
    {0x06, 0x00},
    {0x07, 0x80},
    {0x08, 0x02},
    {0x09, 0x09},
    {0x0A, 0x04},
    {0x0B, 0x06},
    {0x0C, 0x0C},
    {0x0D, 0xA1},
    {0x0E, 0x00},
    {0x0F, 0x00},
    {0x10, 0x01},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0xB8},
    {0x15, 0x07},
    {0x16, 0x06},
    {0x17, 0x06},
    {0x18, 0x03},
    {0x19, 0x04},
    {0x1A, 0x06},
    {0x1B, 0x06},
    {0x1C, 0x07},
    {0x1D, 0x08},
    {0x1E, 0x1A},
    {0x1F, 0x00},
    {0x20, 0x00},
    {0x21, 0x1E},
    {0x22, 0x1E},
    {0x23, 0x01},
    {0x24, 0x04},
    {0x27, 0x00},
    {0x28, 0x00},
    {0x2A, 0x08},
    {0x2B, 0x02},
    {0x2C, 0xA4},
    {0x2D, 0x06},
    {0x2E, 0x00},
    {0x2F, 0x05},
    {0x30, 0xE0},
    {0x31, 0x01},
    {0x32, 0x00},
    {0x33, 0x00},
    {0x34, 0x00},
    {0x35, 0x00},
    {0x36, 0x00},
    {0x37, 0x00},
    {0x38, 0x0E},
    {0x39, 0x01},
    {0x3A, 0x02},
    {0x3B, 0x01},
    {0x3C, 0x00},
    {0x3D, 0x00},
    {0x3E, 0x00},
    {0x3F, 0x00},
    {0x40, 0x16},
    {0x41, 0x26},
    {0x42, 0x00},
    {0x47, 0x05},
    {0x48, 0x07},
    {0x49, 0x01},
    {0x4D, 0x02},
    {0x4F, 0x00},
    {0x54, 0x05},
    {0x55, 0x01},
    {0x56, 0x05},
    {0x57, 0x01},
    {0x58, 0x02},
    {0x59, 0x01},
    {0x5B, 0x00},
    {0x5C, 0x03},
    {0x5D, 0x00},
    {0x5E, 0x07},
    {0x5F, 0x08},
    {0x60, 0x00},
    {0x61, 0x00},
    {0x62, 0x00},
    {0x63, 0x28},
    {0x64, 0x30},
    {0x65, 0x9E},
    {0x66, 0xB9},
    {0x67, 0x52},
    {0x68, 0x70},
    {0x69, 0x4E},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x90, 0x04},
    {0x91, 0x01},
    {0x92, 0x00},
    {0x93, 0x00},
    {0x94, 0x03},
    {0x96, 0x00},
    {0x97, 0x01},
    {0x98, 0x01},
    {0xA0, 0x00},
    {0xA1, 0x01},
    {0xA2, 0x00},
    {0xA3, 0x01},
    {0xA4, 0x00},
    {0xA5, 0x01},
    {0xA6, 0x00},
    {0xA7, 0x00},
    {0xAA, 0x00},
    {0xAB, 0x0F},
    {0xAC, 0x08},
    {0xAD, 0x09},
    {0xAE, 0x0A},
    {0xAF, 0x0B},
    {0xB0, 0x00},
    {0xB1, 0x00},
    {0xB2, 0x01},
    {0xB3, 0x00},
    {0xB4, 0x00},
    {0xB5, 0x0A},
    {0xB6, 0x0A},
    {0xB7, 0x0A},
    {0xB8, 0x0A},
    {0xB9, 0x00},
    {0xBA, 0x00},
    {0xBB, 0x00},
    {0xBC, 0x00},
    {0xBD, 0x00},
    {0xBE, 0x00},
    {0xBF, 0x00},
    {0xC0, 0x00},
    {0xC1, 0x00},
    {0xC2, 0x00},
    {0xC3, 0x00},
    {0xC4, 0x00},
    {0xC5, 0x00},
    {0xC6, 0x00},
    {0xC7, 0x00},
    {0xC8, 0x00},
    {0xD3, 0x80},
    {0xD4, 0x00},
    {0xD5, 0x00},
    {0xD6, 0x03},
    {0xD7, 0x77},
    {0xD8, 0x00},
    {0xED, 0x01},
    {0xEF, 0x00},
    {0x11, 0x00},
    {0xEF, 0x01},
    {0x02, 0xFB},
    {0x05, 0x31},
    {0x09, 0x01},                                                                                      
};
#define NUM_OF_INIT_REGS (sizeof(sensor_init_regs) / sizeof(sensor_reg_t))


typedef struct gain_set {
    u16 a_gain;
    u32 gain_x;
} gain_set_t;

static const gain_set_t gain_table[] = {
    {2731, 64  }, {2621, 67  }, {2521, 69  }, {2427, 72  }, 
    {2341, 75  }, {2260, 77  }, {2185, 80  }, {2114, 83  }, 
    {2048, 85  }, {1928, 91  }, {1820, 96  }, {1725, 101 }, 
    {1638, 107 }, {1560, 112 }, {1489, 117 }, {1425, 123 }, 
    {1365, 128 }, {1311, 133 }, {1260, 139 }, {1214, 144 }, 
    {1170, 149 }, {1130, 155 }, {1092, 160 }, {1057, 165 }, 
    {1024, 171 }, {964 , 181 }, {910 , 192 }, {862 , 203 }, 
    {819 , 213 }, {780 , 224 }, {745 , 235 }, {712 , 245 }, 
    {683 , 256 }, {655 , 267 }, {630 , 277 }, {607 , 288 }, 
    {585 , 299 }, {565 , 309 }, {546 , 320 }, {529 , 331 }, 
    {512 , 341 }, {482 , 363 }, {455 , 384 }, {431 , 405 }, 
    {410 , 427 }, {390 , 448 }, {372 , 469 }, {356 , 491 }, 
    {341 , 512 }, {328 , 533 }, {315 , 555 }, {303 , 576 }, 
    {293 , 597 }, {282 , 619 }, {273 , 640 }, {264 , 661 }, 
    {256 , 683 }, {241 , 725 }, {228 , 768 }, {216 , 811 }, 
    {205 , 853 }, {195 , 896 }, {186 , 939 }, {178 , 981 }, 
    {171 , 1024}, {164 , 1067}, {158 , 1109}, {152 , 1152}, 
    {146 , 1195}, {141 , 1237}, {137 , 1280}, {132 , 1323}, 
    {128 , 1365}, //From 1.5X start
};

#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x90 >> 1)

#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[1], tmp2[2];

    tmp[0]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0; // write
    msgs[0].len   = 1;
    msgs[0].buf   = tmp;

    tmp2[0]       = 0; // clear
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1; // read
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

    buf[0]     = addr & 0xFF;
    buf[1]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    return sensor_i2c_transfer(&msgs, 1);
}

static void adjust_vblk(int fps)
{    
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;  
    
    if ((fps > 60) || (fps < 5))
        return;                                                                            
    
    if ((fps <= 60) && (fps >= 5)) {    	
        pinfo->total_line = g_psensor->pclk / fps / pinfo->htp;
        reg_h = (pinfo->total_line >> 8) & 0xFF;
        reg_l = pinfo->total_line & 0xFF;
        write_reg(0xEF, 0x01);//bank1
        write_reg(0x0A, reg_h);
        write_reg(0x0B, reg_l);      
        write_reg(0x09, 0x01);//update flag           
        isp_dbg("pinfo->total_line=%d\n",pinfo->total_line);
    }        
                
    //Update fps status
    g_psensor->fps = fps;  
    isp_dbg("g_psensor->fps=%d\n", g_psensor->fps);                          
    
}

static u32 get_pclk(u32 xclk)
{
    u32 pixclk;

    pixclk = 74250000;           
          
    printk("pixel clock %d\n", pixclk);
    return pixclk;
}


static int set_size(u32 width, u32 height)
{
    int ret = 0;    

    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("%dx%d exceeds maximum resolution %dx%d!\n",
            width, height, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT);
        return -1;
    }

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = SENSOR_MAX_WIDTH;
    g_psensor->out_win.height = SENSOR_MAX_HEIGHT; 
    g_psensor->img_win.x = ((SENSOR_MAX_WIDTH - width) >> 1) &~BIT0;
    g_psensor->img_win.y = ((SENSOR_MAX_HEIGHT - height) >> 1) &~BIT0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
        
    isp_dbg("x=%d, y=%d, img_win.width=%d, img_win.height=%d\n", g_psensor->img_win.x, g_psensor->img_win.y, g_psensor->img_win.width, g_psensor->img_win.height);        

    return ret;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, sw_u, sw_l;

    if (!g_psensor->curr_exp) {
        write_reg(0xEF, 0x01);//bank1
        read_reg(0x0A, &sw_u);//high byte
        read_reg(0x0B, &sw_l);//low  byte        
        lines = (sw_u << 8) | sw_l;
        g_psensor->curr_exp = (lines * pinfo->t_row + 50) / 100;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines;

    lines = ((exp * 1000)+ pinfo->t_row /2) / pinfo->t_row;
    
    //printk("exp=%d,pinfo->t_row=%d\n", exp, pinfo->t_row);

    if (lines == 0)
        lines = 1;  
           
    write_reg(0xEF, 0x01);//bank1

    if (lines > pinfo->total_line) {// Long Exposure for slow shutter
        write_reg(0x0A, ((lines + 2) >> 8) & 0xFF);//Cmd_Lpf[15:8] 
        write_reg(0x0B, ((lines + 2) & 0xFF));//Cmd_Lpf[7:0]     
        write_reg(0x0C, 0x00);//Cmd_OffNy1[15:8] 
        write_reg(0x0D, 0x02);//Cmd_OffNy1[7:0]     // Min. Ny=2    
        //printk("lines = %d\n",lines);        
    } else {//// Short Exposure for max. frame rate
        write_reg(0x0A, (pinfo->total_line >> 8) & 0xFF);
        write_reg(0x0B, (pinfo->total_line & 0xFF));         
        write_reg(0x0C, ((pinfo->total_line - lines)>>8) & 0xFF );//Cmd_OffNy1[15:8] 
        write_reg(0x0D, ((pinfo->total_line - lines) & 0xFF ));//Cmd_OffNy1[7:0]    
    }         

    write_reg(0x09, 0x01);//update flag    
    
    g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;
     
    //printk("g_psensor->curr_exp=%d, lines=%d\n", g_psensor->curr_exp, lines);

    return 0;
}

static u32 get_gain(void)
{
    u32 i, gain_x, reg, GDAC_h, GDAC_l, GDAC;
   
    if (g_psensor->curr_gain == 0) {
        write_reg(0xEF, 0x01);//bank1
        read_reg(0x78, &reg);//Cmd_GDAC[12:8]
        GDAC_h = reg << 8;
        read_reg(0x79, &reg);//Cmd_GDAC[7:0]
        GDAC_l = reg & 0xFF;                       
        GDAC = GDAC_h | GDAC_l;        
        gain_x = 2048 / GDAC;
        
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
    int ret = 0, i;
    u32 a_gain;
	
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    a_gain = gain_table[i-1].a_gain;     
 
    write_reg(0xEF, 0x01);//bank1
    write_reg(0x78, (a_gain >> 8) & 0x1F);//Cmd_GDAC[12:8]
    write_reg(0x79, (a_gain & 0xFF));//Cmd_GDAC[7:0]         
    write_reg(0x09, 0x01);//update flag   
    
    g_psensor->curr_gain = gain_table[i-1].gain_x;  
	
    //printk("g_psensor->curr_gain=%d, a_gain=%d\n",g_psensor->curr_gain, a_gain);

    return ret;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->flip) {
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_RG;
        else
            g_psensor->bayer_type = BAYER_GR;
    }
    else {
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_RG;
        else
            g_psensor->bayer_type = BAYER_GR;
    }
}

static int get_mirror(void)
{
    u32 reg;
	
    write_reg(0xEF, 0x01);//bank1
    read_reg(0x1B, &reg);
    
    return (reg & BIT7) ? 1 : 0;    
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    write_reg(0xEF, 0x01);//bank1
    read_reg(0x1B, &reg);
    
    if (enable)
        reg |= BIT7;
    else
        reg &= ~BIT7;
    write_reg(0x1B, reg);
    write_reg(0x09, 0x01);//update flag    
    update_bayer_type();

    return 0;
}

static int get_flip(void)
{
    u32 reg;
	
    write_reg(0xEF, 0x01);//bank1
    read_reg(0x1D, &reg);
    
    return (reg & BIT7) ? 1 : 0;    
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    write_reg(0xEF, 0x01);//bank1
    read_reg(0x1D, &reg);
    
    if (enable)
        reg |= BIT7;
    else
        reg &= ~BIT7;
    write_reg(0x1D, reg);
    write_reg(0x09, 0x01);//update flag    
    update_bayer_type();

    return 0;
}

static int check_sensor_reg_status(void)
{
    int i;
    u32 gain,a_gain;
    u32 control_enh,control_sel;
    u32 temp_reg,temp_reg2,temp_reg_total;


    gain = get_gain();
	
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    a_gain = gain_table[i-1].a_gain;  
	
    write_reg(0xEF, 0x02);//bank2
    read_reg(0x1B, &temp_reg);//Normalized_DigDac1[10:8](Measurement MSB value of sensor dark current)
    read_reg(0x1C, &temp_reg2);//Normalized_DigDac1[7:0](Measurement LSB value of sensor dark current)
    write_reg(0xEF, 0x01);//bank1
    read_reg(0xE1, &control_enh);//control_enh (Sensor analog control signal)
    read_reg(0xE2, &control_sel);//control_sel (Sensor analog control selection)
    temp_reg_total = ((temp_reg & 0x7) << 8) | (temp_reg2 & 0xFF);

    //High Temperature Control
	if ((0x001C < temp_reg_total) || (0x0400 > a_gain)) {
	    write_reg(0xEF, 0x01);//bank1
	    if((control_enh & 0x10) != 0x10)//check the value of Bank1[0xE1] bit4
	        write_reg(0xE2, (control_sel & 0xF0) | 0x09);
	    else
	        write_reg(0xE2, (control_sel & 0xF0) | 0x0D);  
        write_reg(0xE1, control_enh | 0x10);        		        			
	} else if ((0x0016 > temp_reg_total) && (0x0555 < a_gain)) {
        write_reg(0xEF, 0x01);//bank1
        if((control_sel & 0x05) == 0x5){
	        write_reg(0xE2, (control_sel & 0xF0) | 0x09);
	        write_reg(0xE1, control_enh | 0x10);
	    }    
	    else{	                
            write_reg(0xE2, control_sel & 0xF7);
            write_reg(0xE1, control_enh & 0xEF);			        	
        }    
	}
	write_reg(0x09, 0x01);// Update Flag	
    
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

    case ID_CHECK_SENSOR_STATUS:
        *(int*)arg = check_sensor_reg_status();
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
        adjust_vblk((int)arg);
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
    u32 reg_h, reg_l;
    int i;    

    if (pinfo->is_init)
        return 0;        
        
    isp_info("Sensor Interface : PARALLEL\n");
    isp_info("Sensor Name : ps5230\n");
    // write initial register value
    for (i=0; i<NUM_OF_INIT_REGS; i++) {
        if (write_reg(sensor_init_regs[i].addr, sensor_init_regs[i].val) < 0) {
            isp_err("failed to initial %s!\n", SENSOR_NAME);
            return -EINVAL;
        }            
    }     
    
    write_reg(0xEF, 0x01);//bank1
    read_reg(0x0A, &reg_h);//VTS Cmd_Lpf[15:8]
    read_reg(0x0B, &reg_l);//Cmd_Lpf[7:0] 
    pinfo->total_line = (((reg_h & 0xFF) << 8) | reg_l);         
    g_psensor->pclk = get_pclk(g_psensor->xclk); 
        
    read_reg(0x27, &reg_h);//Cmd_LineTime[12:8]  
    read_reg(0x28, &reg_l);//Cmd_LineTime[7:0] 
    pinfo->htp = (((reg_h & 0x1F) << 8) | reg_l); 
    pinfo->t_row = (pinfo->htp * 10000) / (g_psensor->pclk / 1000);    
        
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;             
    
    //printk("pinfo->total_line=%d, pinfo->htp=%d, pinfo->t_row=%d, g_psensor->min_exp=%d\n", pinfo->total_line, pinfo->htp, pinfo->t_row, g_psensor->min_exp);    

    // set image size
    if ((ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height)) < 0)
        return ret;
        
    adjust_vblk(g_psensor->fps);

    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current shutter_width and gain setting
    g_psensor->curr_exp = get_exp();
    g_psensor->curr_gain = get_gain();   
    
    pinfo->is_init = true;
    
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void ps5230_deconstruct(void)
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

static int ps5230_construct(u32 xclk, u16 width, u16 height)
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

    pinfo = kzalloc(sizeof(sensor_info_t), GFP_KERNEL);
    if (!pinfo) {
        isp_err("failed to allocate private data!\n");
        return -ENOMEM;
    }
    g_psensor->private = pinfo;
    snprintf(g_psensor->name, DEV_MAX_NAME_SIZE, SENSOR_NAME);
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->interface = IF_PARALLEL;
    g_psensor->fmt = RAW10;
    g_psensor->bayer_type = BAYER_GR;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH; 
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
    ps5230_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init ps5230_init(void)
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

    if ((ret = ps5230_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit ps5230_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ps5230_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(ps5230_init);
module_exit(ps5230_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor ps5230");
MODULE_LICENSE("GPL");
