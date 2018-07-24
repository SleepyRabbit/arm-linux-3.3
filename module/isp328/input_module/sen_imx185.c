/**
 * @file sen_imx185.c
 * SONY IMX185 sensor driver
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

#define PFX "sen_imx185"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH          1920	
#define DEFAULT_IMAGE_HEIGHT         1080
#define DEFAULT_XCLK             27000000
#define DEFAULT_IF                IF_LVDS

#define SENSOR_NAME              "IMX185"
#define SENSOR_MAX_WIDTH             1920
#define SENSOR_MAX_HEIGHT            1080
#define SENSOR_MAX_VMAX            131070
#define SENSOR_NON_EXP_VMAX             0
#define DELAY_CODE                 0xFFFF

//1080P mode
#define  DEFAULT_1080P_ISINIT           0
#define  DEFAULT_1080P_VMAX          1125
#define  DEFAULT_1080P_HMAX          2200
#define  DEFAULT_1080P_WIDTH         1936
#define  DEFAULT_1080P_HEIGHT        1103
#define  DEFAULT_1080P_MIRROR           0
#define  DEFAULT_1080P_FLIP             0
#define  DEFAULT_1080P_TROW           296
#define  DEFAULT_1080P_PCLK      74250000
#define  DEFAULT_1080P_MAXFPS          30
#define  DEFAULT_1080P_LANE             2 
#define  DEFAULT_1080P_RAWBIT       RAW12
#define  DEFAULT_1080P_BAYER     BAYER_RG
#define  DEFAULT_1080P_IF      DEFAULT_IF

//720P mode
#define  DEFAULT_720P_ISINIT           0
#define  DEFAULT_720P_VMAX           750
#define  DEFAULT_720P_HMAX          3300
#define  DEFAULT_720P_WIDTH         1296
#define  DEFAULT_720P_HEIGHT         733
#define  DEFAULT_720P_MIRROR           0
#define  DEFAULT_720P_FLIP             0
#define  DEFAULT_720P_TROW           444
#define  DEFAULT_720P_PCLK      37125000
#define  DEFAULT_720P_MAXFPS          30
#define  DEFAULT_720P_LANE             2 
#define  DEFAULT_720P_RAWBIT       RAW12
#define  DEFAULT_720P_BAYER     BAYER_RG
#define  DEFAULT_720P_IF      DEFAULT_IF

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

static int ch_num = 2;
module_param(ch_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_num, "channel number");

static int fps = 30;
module_param(fps, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "frame per second");

//=============================================================================
// global
//=============================================================================
static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    u32  VMax;	
    u32  HMax;
    int  img_w;
    int  img_h;
    int  mirror;
    int  flip;
    u16  t_row; // unit is 1/10 us
    u32  pclk;
    int  fps;
    int  lane;
    enum SRC_FMT rawbit;	
    enum BAYER_TYPE bayer;	
    enum SENSOR_IF s_interface;
} sensor_info_t;


static sensor_info_t g_sIMX185info[]=
{
{
DEFAULT_1080P_ISINIT,
DEFAULT_1080P_VMAX,
DEFAULT_1080P_HMAX,
DEFAULT_1080P_WIDTH,
DEFAULT_1080P_HEIGHT,
DEFAULT_1080P_MIRROR,
DEFAULT_1080P_FLIP,
DEFAULT_1080P_TROW,
DEFAULT_1080P_PCLK,
DEFAULT_1080P_MAXFPS,
DEFAULT_1080P_LANE,
DEFAULT_1080P_RAWBIT,
DEFAULT_1080P_BAYER,
DEFAULT_1080P_IF,
},
{
DEFAULT_720P_ISINIT,
DEFAULT_720P_VMAX,
DEFAULT_720P_HMAX,
DEFAULT_720P_WIDTH,
DEFAULT_720P_HEIGHT,
DEFAULT_720P_MIRROR,
DEFAULT_720P_FLIP,
DEFAULT_720P_TROW,
DEFAULT_720P_PCLK,
DEFAULT_720P_MAXFPS,
DEFAULT_720P_LANE,
DEFAULT_720P_RAWBIT,
DEFAULT_720P_BAYER,
DEFAULT_720P_IF,
},
};



// sensor Mode
enum SENSOR_MODE {
    MODE_1080P = 0,
    MODE_720P,    
    MODE_MAX
};

typedef struct sensor_reg {
    u16 addr;
    u8 val;
} sensor_reg_t;

typedef struct _gain_set
{
    u16 ad_gain;
    u32 gain_x;     // UFIX7.6
} gain_set_t;

static sensor_reg_t sensor_1080P_init_regs[] = { 
    {0x3005, 0x01}, 
    {0x3006, 0x00}, // Drive mode setting  00h: All-pix scan mode   22h: Vertical 1/2 subsampling readout mode
    {0x3007, 0x10}, // Window mode setting [7:4] 0:WUXGA 1:1080p 2:720P
    {0x3009, 0x02}, // Frame rate setting  0: 120 fps  1: 60 fps  2: 30fps
    {0x300A, 0xF0}, // Black Level Adjustment, 10 bits 0x3c,  12bits: 0xF0(240), 0x78(120)
    {0x3018, 0x65}, // VD_L 
    {0x3019, 0x04}, // VD_H
    {0x301B, 0x98}, // HD_L
    {0x301C, 0x08}, // HD_H
    {0x301D, 0x08}, 
    {0x301E, 0x02}, 
    {0x3044, 0xD1}, 
    {0x3048, 0x33}, 
    {0x3049, 0x0A}, 
    {0x3052, 0x00}, 
    {0x305C, 0x2C}, 
    {0x305D, 0x00}, 
    {0x305E, 0x21}, 
    {0x305F, 0x00}, 
    {0x3063, 0x54}, 
    {0x311D, 0x0A}, 
    {0x3123, 0x0F}, 
    {0x3147, 0x87}, 
    {0x31E1, 0x9E}, 
    {0x31E2, 0x01}, 
    {0x31E5, 0x05}, 
    {0x31E6, 0x05}, 
    {0x31E7, 0x3A}, 
    {0x31E8, 0x3A}, 
    {0x3203, 0xC8}, 
    {0x3207, 0x54}, 
    {0x3213, 0x16}, 
    {0x3215, 0xF6}, 
    {0x321A, 0x14}, 
    {0x321B, 0x51}, 
    {0x3229, 0xE7}, 
    {0x322A, 0xF0}, 
    {0x322B, 0x10}, 
    {0x3231, 0xE7}, 
    {0x3232, 0xF0}, 
    {0x3233, 0x10}, 
    {0x323C, 0xE8}, 
    {0x323D, 0x70}, 
    {0x3243, 0x08}, 
    {0x3244, 0xE1}, 
    {0x3245, 0x10}, 
    {0x3247, 0xE7}, 
    {0x3248, 0x60}, 
    {0x3249, 0x1E}, 
    {0x324B, 0x00}, 
    {0x324C, 0x41}, 
    {0x3250, 0x30}, 
    {0x3251, 0x0A}, 
    {0x3252, 0xFF}, 
    {0x3253, 0xFF}, 
    {0x3254, 0xFF},
    {0x3255, 0x02},
    {0x3257, 0xF0}, 
    {0x325A, 0xA6}, 
    {0x325D, 0x14}, 
    {0x325E, 0x51}, 
    {0x3261, 0x61}, 
    {0x3266, 0x30}, 
    {0x3275, 0xE7}, 
    {0x3281, 0xEA}, 
    {0x3282, 0x70}, 
    {0x3285, 0xFF}, 
    {0x328A, 0xF0}, 
    {0x328D, 0xB6}, 
    {0x328E, 0x40}, 
    {0x3290, 0x42}, 
    {0x3291, 0x51}, 
    {0x3292, 0x1E}, 
    {0x3294, 0xC4}, 
    {0x3295, 0x20}, 
    {0x3297, 0x50}, 
    {0x3298, 0x31}, 
    {0x3299, 0x1F}, 
    {0x329B, 0xC0}, 
    {0x329C, 0x60}, 
    {0x329E, 0x4C}, 
    {0x329F, 0x71}, 
    {0x32A0, 0x1F}, 
    {0x32A2, 0xB6}, 
    {0x32A3, 0xC0}, 
    {0x32A4, 0x0B}, 
    {0x32A9, 0x24}, 
    {0x32AA, 0x41}, 
    {0x32B0, 0x25}, 
    {0x32B1, 0x51}, 
    {0x32B7, 0x1C}, 
    {0x32B8, 0xC1}, 
    {0x32B9, 0x12}, 
    {0x32BE, 0x1D}, 
    {0x32BF, 0xD1}, 
    {0x32C0, 0x12}, 
    {0x32C2, 0xA8}, 
    {0x32C3, 0xC0}, 
    {0x32C4, 0x0A}, 
    {0x32C5, 0x1E}, 
    {0x32C6, 0x21}, 
    {0x32C9, 0xB0}, 
    {0x32CA, 0x40}, 
    {0x32CC, 0x26}, 
    {0x32CD, 0xA1}, 
    {0x32D0, 0xB6}, 
    {0x32D1, 0xC0}, 
    {0x32D2, 0x0B}, 
    {0x32D4, 0xE2}, 
    {0x32D5, 0x40}, 
    {0x32D8, 0x4E}, 
    {0x32D9, 0xA1}, 
    {0x32EC, 0xF0}, 
};
#define NUM_OF_1080P_INIT_REGS (sizeof(sensor_1080P_init_regs) / sizeof(sensor_reg_t))


static sensor_reg_t sensor_720P_init_regs[] = { 
    {0x3005, 0x01},                                                                                                                   
    {0x3006, 0x00},// Drive mode setting  00h: All-pix scan mode   22h: Vertical 1/2 subsampling readout mode                         
    {0x3007, 0x20},// Window mode setting [7:4] 0:WUXGA 1:1080p 2:720P                                                                
    {0x3009, 0x02},// Frame rate setting  0: 120 fps  1: 60 fps  2: 30fps                                                             
    {0x300A, 0xF0},// Black Level Adjustment, 10 bits 0x3c,  12bits: 0xF0(240), 0x78(120)                                             
    {0x3018, 0xEE},// VD_L                                                                                                            
    {0x3019, 0x02},// VD_H                                                                                                            
    {0x301B, 0xE4},// HD_L                                                                                                            
    {0x301C, 0x0C},// HD_H                                                                                                            
    {0x301D, 0x08},                                                                                                                   
    {0x301E, 0x02},                                                                                                                   
    {0x3044, 0xD1},                                                                                                                   
    {0x3048, 0x33},                                                                                                                   
    {0x3049, 0x0A},                                                                                                                   
    {0x305C, 0x2C},                                                                                                                   
    {0x305D, 0x00},                                                                                                                   
    {0x305E, 0x21},                                                                                                                   
    {0x305F, 0x00},                                                                                                                   
    {0x3063, 0x54},                                                                                                                   
    {0x311D, 0x0A},                                                                                                                   
    {0x3123, 0x0F},                                                                                                                   
    {0x3147, 0x87},                                                                                                                   
    {0x31E1, 0x9E},                                                                                                                   
    {0x31E2, 0x01},                                                                                                                   
    {0x31E5, 0x05},                                                                                                                   
    {0x31E6, 0x05},                                                                                                                   
    {0x31E7, 0x3A},                                                                                                                   
    {0x31E8, 0x3A},                                                                                                                   
    {0x3203, 0xC8},                                                                                                                   
    {0x3207, 0x54},                                                                                                                   
    {0x3213, 0x16},                                                                                                                   
    {0x3215, 0xF6},                                                                                                                   
    {0x321A, 0x14},                                                                                                                   
    {0x321B, 0x51},                                                                                                                   
    {0x3229, 0xE7},                                                                                                                   
    {0x322A, 0xF0},                                                                                                                   
    {0x322B, 0x10},                                                                                                                   
    {0x3231, 0xE7},                                                                                                                   
    {0x3232, 0xF0},                                                                                                                   
    {0x3233, 0x10},                                                                                                                   
    {0x323C, 0xE8},                                                                                                                   
    {0x323D, 0x70},                                                                                                                   
    {0x3243, 0x08},                                                                                                                   
    {0x3244, 0xE1},                                                                                                                   
    {0x3245, 0x10},                                                                                                                   
    {0x3247, 0xE7},                                                                                                                   
    {0x3248, 0x60},                                                                                                                   
    {0x3249, 0x1E},                                                                                                                   
    {0x324B, 0x00},                                                                                                                   
    {0x324C, 0x41},                                                                                                                   
    {0x3250, 0x30},                                                                                                                   
    {0x3251, 0x0A},                                                                                                                   
    {0x3252, 0xFF},                                                                                                                   
    {0x3253, 0xFF},                                                                                                                   
    {0x3254, 0xFF},                                                                                                                   
    {0x3255, 0x02},                                                                                                                   
    {0x3257, 0xF0},                                                                                                                   
    {0x325A, 0xA6},                                                                                                                   
    {0x325D, 0x14},                                                                                                                   
    {0x325E, 0x51},                                                                                                                   
    {0x3261, 0x61},                                                                                                                   
    {0x3266, 0x30},                                                                                                                   
    {0x3275, 0xE7},                                                                                                                   
    {0x3281, 0xEA},                                                                                                                   
    {0x3282, 0x70},                                                                                                                   
    {0x3285, 0xFF},                                                                                                                   
    {0x328A, 0xF0},                                                                                                                   
    {0x328D, 0xB6},                                                                                                                   
    {0x328E, 0x40},                                                                                                                   
    {0x3290, 0x42},                                                                                                                   
    {0x3291, 0x51},                                                                                                                   
    {0x3292, 0x1E},                                                                                                                   
    {0x3294, 0xC4},                                                                                                                   
    {0x3295, 0x20},                                                                                                                   
    {0x3297, 0x50},                                                                                                                   
    {0x3298, 0x31},                                                                                                                   
    {0x3299, 0x1F},                                                                                                                   
    {0x329B, 0xC0},                                                                                                                   
    {0x329C, 0x60},                                                                                                                   
    {0x329E, 0x4C},                                                                                                                   
    {0x329F, 0x71},                                                                                                                   
    {0x32A0, 0x1F},                                                                                                                   
    {0x32A2, 0xB6},                                                                                                                   
    {0x32A3, 0xC0},                                                                                                                   
    {0x32A4, 0x0B},                                                                                                                   
    {0x32A9, 0x24},                                                                                                                   
    {0x32AA, 0x41},                                                                                                                   
    {0x32B0, 0x25},                                                                                                                   
    {0x32B1, 0x51},                                                                                                                   
    {0x32B7, 0x1C},                                                                                                                   
    {0x32B8, 0xC1},                                                                                                                   
    {0x32B9, 0x12},                                                                                                                   
    {0x32BE, 0x1D},                                                                                                                   
    {0x32BF, 0xD1},                                                                                                                   
    {0x32C0, 0x12},                                                                                                                   
    {0x32C2, 0xA8},                                                                                                                   
    {0x32C3, 0xC0},                                                                                                                   
    {0x32C4, 0x0A},                                                                                                                   
    {0x32C5, 0x1E},                                                                                                                   
    {0x32C6, 0x21},                                                                                                                   
    {0x32C9, 0xB0},                                                                                                                   
    {0x32CA, 0x40},                                                                                                                   
    {0x32CC, 0x26},                                                                                                                   
    {0x32CD, 0xA1},                                                                                                                   
    {0x32D0, 0xB6},                                                                                                                   
    {0x32D1, 0xC0},                                                                                                                   
    {0x32D2, 0x0B},                                                                                                                   
    {0x32D4, 0xE2},                                                                                                                   
    {0x32D5, 0x40},                                                                                                                   
    {0x32D8, 0x4E},                                                                                                                   
    {0x32D9, 0xA1},                                                                                                                   
    {0x32EC, 0xF0},                                                                                                                   
};
#define NUM_OF_720P_INIT_REGS (sizeof(sensor_720P_init_regs) / sizeof(sensor_reg_t))

static gain_set_t gain_table[] =
{
    {0x00, 64}, {0x01, 66}, {0x02, 69}, {0x03, 71}, {0x04, 73},
    {0x05, 76}, {0x06, 79}, {0x07, 82}, {0x08, 84}, {0x09, 87},
    {0x0A,	 90}, {0x0B, 94}, {0x0C, 97}, {0x0D, 100}, {0x0E, 104},
    {0x0F,	 107}, {0x10,	111}, {0x11, 115}, {0x12, 119}, {0x13, 123},
    {0x14,	 128}, {0x15,	132}, {0x16, 137}, {0x17, 142}, {0x18, 147},
    {0x19,	 152}, {0x1A,	157}, {0x1B, 163}, {0x1C, 168}, {0x1D, 174},
    {0x1E,	 180}, {0x1F,	187}, {0x20, 193}, {0x21, 200}, {0x22, 207},
    {0x23,	 214}, {0x24,	222}, {0x25, 230}, {0x26, 238}, {0x27, 246},
    {0x28,	 255}, {0x29,	264}, {0x2A, 273}, {0x2B, 283}, {0x2C, 293},
    {0x2D,	 303}, {0x2E,	313}, {0x2F, 324}, {0x30, 336}, {0x31, 348},
    {0x32,	 360}, {0x33,	373}, {0x34, 386}, {0x35, 399}, {0x36, 413},
    {0x37,	 428}, {0x38,	443}, {0x39, 458}, {0x3A, 474}, {0x3B, 491},
    {0x3C,	 508}, {0x3D,	526}, {0x3E, 545}, {0x3F, 564}, {0x40, 584},
    {0x41,	 604}, {0x42,	625}, {0x43, 647}, {0x44, 670}, {0x45, 694},
    {0x46,	 718}, {0x47,	743}, {0x48, 769}, {0x49, 796}, {0x4A, 824},
    {0x4B,	 853}, {0x4C,	883}, {0x4D, 914}, {0x4E, 947}, {0x4F, 980},
    {0x50, 1014}, {0x51, 1050}, {0x52, 1087}, {0x53, 1125}, {0x54, 1165},
    {0x55, 1206}, {0x56, 1248}, {0x57, 1292}, {0x58, 1337}, {0x59, 1384},
    {0x5A, 1433}, {0x5B, 1483}, {0x5C, 1535}, {0x5D, 1589}, {0x5E, 1645},
    {0x5F, 1703}, {0x60, 1763}, {0x61, 1825}, {0x62, 1889}, {0x63, 1955},
    {0x64, 2024}, {0x65, 2095}, {0x66, 2169}, {0x67, 2245}, {0x68, 2324},
    {0x69, 2405}, {0x6A, 2490}, {0x6B, 2577}, {0x6C, 2668}, {0x6D, 2762},
    {0x6E, 2859}, {0x6F, 2959}, {0x70, 3063}, {0x71, 3171}, {0x72, 3282},
    {0x73, 3398}, {0x74, 3517}, {0x75, 3641}, {0x76, 3769}, {0x77, 3901},
    {0x78, 4038}, {0x79, 4180}, {0x7A, 4327}, {0x7B, 4479}, {0x7C, 4636},
    {0x7D, 4799}, {0x7E, 4968}, {0x7F, 5143}, {0x80, 5323}, {0x81, 5510},
    {0x82, 5704}, {0x83, 5904}, {0x84, 6112}, {0x85, 6327}, {0x86, 6549},
    {0x87, 6779}, {0x88, 7017}, {0x89, 7264}, {0x8A, 7519}, {0x8B, 7784},
    {0x8C, 8057}, {0x8D, 8340}, {0x8E, 8633}, {0x8F, 8937}, {0x90, 9251},
    {0x91, 9576}, {0x92, 9912}, {0x93, 10261}, {0x94, 10621}, {0x95, 10995}, 
    {0x96,11381}, {0x97, 11781}, {0x98, 12195}, {0x99, 12624}, {0x9A, 13067}, 
    {0x9B,13526}, {0x9C, 14002}, {0x9D, 14494}, {0x9E, 15003}, {0x9F, 15530}, 
    {0xA0,16076}
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

static u32 g_udCurrentExp=0;
static u32 g_udCurrentGain=64; //default 1x        
static int   g_dMirror=0;
static int   g_dFilp=0;
static u8   g_ucCurrentMode=0;
static int   g_dCurrentFPS=0;
static int   g_dHFPS=0;
static  int  g_dLFPS=0;
static u32 g_udCurrentVMax=0;
static u16 g_uwCurrentT_row=0;

static int read_reg(u32 addr, u32 *pval);
static int write_reg(u32 addr, u32 val);
static u32 get_pclk(u32 xclk);
static void calculate_t_row(void);
static int set_mirror(int enable);
static int get_mirror(void);
static int set_flip(int enable);
static int get_flip(void);
static int set_exp(u32 exp);
static u32 get_exp(void);
static int set_gain(u32 gain);
static u32 get_gain(void);
static int set_fps(int fps);
static int get_fps(void);
static int calc_framerate_reg(u32 a_udEXP, u8 *a_pucVmaxReg1, u8 *a_pucVmaxReg2, u8 *a_pucVmaxReg3);
static int calc_exp_reg(u32 a_udEXP, u32 *a_pudActualTime, u8 *a_pucSHS1Reg1, u8 *a_pucSHS1Reg2, u8 *a_pucSHS1Reg3);
static int set_framerate_limit(int lfps, int hfps);
static int set_framerate(int fps);
static void set_sensor_streaming(bool b_flag);
static int set_size(u32 width, u32 height);
static int set_property(enum SENSOR_CMD cmd, unsigned long arg);
static int get_property(enum SENSOR_CMD cmd, unsigned long arg);
static int init(void);
//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x34 >> 1
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
    u32 pclk;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pclk=g_sIMX185info[g_ucCurrentMode].pclk;
    pinfo->pclk= pclk;
	
    isp_info("IMX185 Sensor: pclk(%d) XCLK(%d)\n", pclk, xclk);

    return pclk;
}

static void calculate_t_row(void)
{
   sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
   pinfo->HMax=g_sIMX185info[g_ucCurrentMode].HMax;
   pinfo->t_row=g_sIMX185info[g_ucCurrentMode].t_row;

    isp_info("h_total=%d, t_row=%d\n", pinfo->HMax, pinfo->t_row);
}

static int set_exp(u32 exp)
{
    int ret = 0;
    u8 ucVmaxReg1,ucVmaxReg2,ucVmaxReg3;
    u8 ucSHSReg1,ucSHSReg2,ucSHSReg3;
    u32 udActualTime;

    calc_framerate_reg(exp, &ucVmaxReg1, &ucVmaxReg2, &ucVmaxReg3);
    calc_exp_reg(exp, &udActualTime, &ucSHSReg1, &ucSHSReg2, &ucSHSReg3);

    write_reg(0x3018, ucVmaxReg3);
    write_reg(0x3019, ucVmaxReg2);
    write_reg(0x301a, ucVmaxReg1);

    write_reg(0x3020, ucSHSReg3);
    write_reg(0x3021, ucSHSReg2);
    write_reg(0x3022, ucSHSReg1);

    return ret;
}

static u32 get_exp(void)
{
    return g_udCurrentExp;
}

static int set_gain(u32 gain)
{
  //  sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;
    int ret = 0, i;
	
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++)
    {
        if (gain_table[i].gain_x > gain)
            break;
    }

    reg= gain_table[i-1].ad_gain & 0xFF;
    write_reg(0x3014, reg);
    
    g_udCurrentGain=gain_table[i-1].gain_x;

    g_psensor->curr_gain = g_udCurrentGain;
	
    return ret;
}

static u32 get_gain(void)
{
    return g_udCurrentGain;
}

static int set_fps(int fps)
{
    set_framerate(fps);
	
    return 0;
}

static int get_fps(void)
{
    return g_dCurrentFPS;
}

static int set_framerate(int fps)
{

    int ret=0;
    ret=set_framerate_limit(2,fps);

    return ret;
}

static int set_framerate_limit(int lfps, int hfps)
{

    int  defaultFPS,ret=0;
    u32 defaultVmax;
    u32 udexp;

    defaultFPS=g_sIMX185info[g_ucCurrentMode].fps;
    defaultVmax=g_sIMX185info[g_ucCurrentMode].VMax;

    if(hfps>defaultFPS)
        hfps=defaultFPS;

    if(lfps>defaultFPS)
        lfps=defaultFPS;

    if(hfps<lfps)
	hfps=lfps;

   if(lfps<1)
   	lfps=1;

   if(hfps<1)
       hfps=1;

    g_dLFPS=lfps;
    g_dHFPS=hfps;
	
    udexp= get_exp();
    set_exp(udexp);

    return ret;
}

static int calc_framerate_reg(u32 a_udEXP, u8 *a_pucVmaxReg1, u8 *a_pucVmaxReg2, u8 *a_pucVmaxReg3)
{
    int ret=0;
    u32 udMaxFPSToExp,udMinFPSToExp;
    u32 udDefaultMaxExp;
    u16 uwDrfaultT_row;
    u32 udCalVMax;

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    if((0==g_dHFPS)||(0==g_dLFPS))
    {
        g_dCurrentFPS=g_sIMX185info[g_ucCurrentMode].fps;
        g_udCurrentVMax=g_sIMX185info[g_ucCurrentMode].VMax;

        *a_pucVmaxReg1=(g_udCurrentVMax >> 16) & 0x01;
        *a_pucVmaxReg2=(g_udCurrentVMax >> 8) & 0xff;
        *a_pucVmaxReg3=g_udCurrentVMax & 0xff;	
        return ret;
    }

    udMaxFPSToExp=1000000 / g_dHFPS/100; // unit 100us
    udMinFPSToExp=1000000 / g_dLFPS/ 100;  //  unit 100us
	
    if(a_udEXP>udMinFPSToExp)
        a_udEXP = udMinFPSToExp;
    else if(a_udEXP< udMaxFPSToExp)
	a_udEXP = udMaxFPSToExp;

    uwDrfaultT_row=g_sIMX185info[g_ucCurrentMode].t_row;
    udDefaultMaxExp= (SENSOR_MAX_VMAX-SENSOR_NON_EXP_VMAX-1)*uwDrfaultT_row/10;

    if(a_udEXP<=udDefaultMaxExp)
	udCalVMax= (a_udEXP*10*100) /uwDrfaultT_row;
    else if(a_udEXP>=udDefaultMaxExp)
	udCalVMax=SENSOR_MAX_VMAX;

    g_udCurrentVMax=udCalVMax;

    g_dCurrentFPS	=1*10*1000000/(g_udCurrentVMax*uwDrfaultT_row);
    g_uwCurrentT_row=uwDrfaultT_row;

    *a_pucVmaxReg1=(g_udCurrentVMax >> 16) & 0x01;
    *a_pucVmaxReg2=(g_udCurrentVMax >> 8) & 0xff;
    *a_pucVmaxReg3=g_udCurrentVMax & 0xff;	 

     g_psensor->fps = g_dCurrentFPS;
    pinfo->fps=g_dCurrentFPS;
    pinfo->VMax=g_udCurrentVMax;
   // isp_info("VMax=%d, fps=%d \n", pinfo->VMax,pinfo->fps);

    return ret;	

}

static int calc_exp_reg(u32 a_udEXP, u32 *a_pudActualTime, u8 *a_pucSHS1Reg1, u8 *a_pucSHS1Reg2, u8 *a_pucSHS1Reg3)
{
    int ret=0;
    u32 udMaxExpLine=g_udCurrentVMax-SENSOR_NON_EXP_VMAX;
    u32 udExpLine;
    u32 udSHS1;

    udExpLine= (a_udEXP*10*100/g_uwCurrentT_row);

    if(udExpLine>(udMaxExpLine-2))
    {
        udSHS1=0;
        g_udCurrentExp=(g_udCurrentVMax-2)*g_uwCurrentT_row/1000;	
    }
    else
    {
        if(0==udExpLine)
            udSHS1=g_udCurrentVMax-2;
	else	
            udSHS1=g_udCurrentVMax-udExpLine-1;
        g_udCurrentExp=(g_udCurrentVMax-udSHS1-1)*g_uwCurrentT_row/1000;
    }
	
    *a_pucSHS1Reg1=(udSHS1 >> 16) & 0x01;
    *a_pucSHS1Reg2=(udSHS1 >> 8) & 0xff;
    *a_pucSHS1Reg3=udSHS1 & 0xff;	 

    g_psensor->curr_exp=g_udCurrentExp;
    return ret;	

}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    read_reg(0x3007, &reg);
    if (enable)
        reg |= 0x02;
    else
        reg &= ~0x02;		
    write_reg(0x3007, reg);

    g_dMirror=enable;
    pinfo->mirror= g_dMirror ;
	
    return 0;
}

static int get_mirror(void)
{
    return g_dMirror;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    read_reg(0x3007, &reg);
    if (enable)
        reg |= 0x01;
    else
        reg &= ~0x01;
    write_reg(0x3007, reg);
	
    g_dFilp=enable;
    pinfo->flip= g_dFilp ;

    return 0;
}

static int get_flip(void)
{
    return g_dFilp;
}

static void set_sensor_streaming(bool b_flag)
{
    if(true==b_flag)
    {
        write_reg(0x3000, 0x00);
        mdelay(20); 
        write_reg(0x3002, 0x00);
    }
    else if(false==b_flag)
    {
        write_reg(0x3000, 0x01); // Standby 0: Operating    1: Standby
        write_reg(0x3002, 0x01); // master mode operation 0: Start    1: Stop
    }   
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;
    int i, ret = 0;
    
    isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT))
    {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }


    pinfo->img_w=width;
    pinfo->img_h=height;

    if((1280<width)||(720<height))
    {
        g_ucCurrentMode=MODE_1080P;
        pInitTable=sensor_1080P_init_regs;
        InitTable_Num=NUM_OF_1080P_INIT_REGS;
    }
    else if((width <= 1280)||(height<= 720))
    {
        g_ucCurrentMode=MODE_720P;
        pInitTable=sensor_720P_init_regs;
        InitTable_Num=NUM_OF_720P_INIT_REGS;
        g_psensor->pclk = get_pclk(g_psensor->xclk);	    
        isp_info("start initial0...\n");
    }
    
    isp_info("start initial...\n");
	
    set_sensor_streaming(false);
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

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sIMX185info[g_ucCurrentMode].img_w;
    g_psensor->out_win.height = g_sIMX185info[g_ucCurrentMode].img_h;	


    set_fps(g_psensor->fps);
    calculate_t_row();
   
    set_sensor_streaming(true);

    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    
    if((1280<width)||(720<height))
    {
        g_psensor->img_win.x = 16;
        g_psensor->img_win.y = 23;
    }
    else if((width <= 1280)||(height<= 720))
    {
        g_psensor->img_win.x = 16;
        g_psensor->img_win.y = 13;    
    }
    
    //g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;
    //g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;
    isp_info("initial end ...\n");
	
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
            set_fps((int)arg);
            break;
        default:
            ret = -EFAULT;
            break;
    }

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
            *(int*)arg = get_fps();
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

    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void imx185_deconstruct(void)
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

static int imx185_construct(u32 xclk, u16 width, u16 height)
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

    if((1280<width)&&( 720<height))
        g_ucCurrentMode=MODE_1080P;
    else if((width <= 1280) && (height<=720))
        g_ucCurrentMode=MODE_720P;

    g_psensor->private = pinfo;
    snprintf(g_psensor->name, DEV_MAX_NAME_SIZE, SENSOR_NAME);
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sIMX185info[g_ucCurrentMode].s_interface;
    g_psensor->num_of_lane = g_sIMX185info[g_ucCurrentMode].lane;
    g_psensor->fmt =  g_sIMX185info[g_ucCurrentMode].rawbit;
    g_psensor->bayer_type = g_sIMX185info[g_ucCurrentMode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->inv_clk = 0;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;    //100us
    g_psensor->max_exp = 5000; // 0.5 sec ,500ms
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    g_psensor->curr_exp = get_exp();
    g_psensor->curr_gain= get_gain();
    g_psensor->exp_latency = 0;
    g_psensor->gain_latency = 1;
    g_psensor->fps = g_sIMX185info[g_ucCurrentMode].fps;
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
    imx185_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init imx185_init(void)
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

    if ((ret = imx185_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit imx185_exit(void)
{
    isp_unregister_sensor(g_psensor);
    imx185_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(imx185_init);
module_exit(imx185_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor IMX185");
MODULE_LICENSE("GPL");
