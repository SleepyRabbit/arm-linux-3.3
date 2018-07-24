/**
 * @file sen_imx290.c
 * imx290 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 * 20160406 author RobertChuang
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_imx290"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1920 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT    1080 //ISP Output size
#define DEFAULT_XCLK        37125000
#define DEFAULT_INTERFACE    IF_MIPI

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

static ushort interface = DEFAULT_INTERFACE;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");

//=============================================================================
// global parameters
//=============================================================================
#define SENSOR_NAME          "IMX290"
#define SENSOR_MAX_WIDTH         1920
#define SENSOR_MAX_HEIGHT        1080
#define SENSOR_MAX_VMAX        212643
#define SENSOR_NON_EXP_LINE         2
#define SENSOR_ISINIT               0
#define SENSOR_LONG_EXP             0
#define DELAY_CODE             0xFFFF

//mipi setting
#define  DEFAULT_1080P_MIPI_MODE    MODE_MIPI_1080P
#define  DEFAULT_1080P_MIPI_VMAX               1125
#define  DEFAULT_1080P_MIPI_HMAX               4400
#define  DEFAULT_1080P_MIPI_WIDTH              1920
#define  DEFAULT_1080P_MIPI_HEIGHT             1080
#define  DEFAULT_1080P_MIPI_MIRROR                0
#define  DEFAULT_1080P_MIPI_FLIP                  0
#define  DEFAULT_1080P_MIPI_TROW                296
#define  DEFAULT_1080P_MIPI_PCLK           74250000
#define  DEFAULT_1080P_MIPI_MAXFPS               30
#define  DEFAULT_1080P_MIPI_LANE                  2
#define  DEFAULT_1080P_MIPI_RAWBIT            RAW12
#define  DEFAULT_1080P_MIPI_BAYER          BAYER_RG
#define  DEFAULT_1080P_MIPI_INTERFACE       IF_MIPI

//mipi setting
#define  DEFAULT_720P_MIPI_MODE      MODE_MIPI_720P
#define  DEFAULT_720P_MIPI_VMAX                 750
#define  DEFAULT_720P_MIPI_HMAX                3300
#define  DEFAULT_720P_MIPI_WIDTH               1280
#define  DEFAULT_720P_MIPI_HEIGHT               720
#define  DEFAULT_720P_MIPI_MIRROR                 0
#define  DEFAULT_720P_MIPI_FLIP                   0
#define  DEFAULT_720P_MIPI_TROW                 222
#define  DEFAULT_720P_MIPI_PCLK            74250000
#define  DEFAULT_720P_MIPI_MAXFPS                60
#define  DEFAULT_720P_MIPI_LANE                   2
#define  DEFAULT_720P_MIPI_RAWBIT             RAW12
#define  DEFAULT_720P_MIPI_BAYER           BAYER_RG
#define  DEFAULT_720P_MIPI_INTERFACE        IF_MIPI

//lvds setting
#define  DEFAULT_1080P_LVDS_MODE    MODE_LVDS_1080P
#define  DEFAULT_1080P_LVDS_VMAX               1125
#define  DEFAULT_1080P_LVDS_HMAX               2200
#define  DEFAULT_1080P_LVDS_WIDTH              1932//lvds need crop 12 ob line,so need add 12 line
#define  DEFAULT_1080P_LVDS_HEIGHT             1100//lvds need crop 20 ob line,so need add 20 line
#define  DEFAULT_1080P_LVDS_MIRROR                0
#define  DEFAULT_1080P_LVDS_FLIP                  0
#define  DEFAULT_1080P_LVDS_TROW                296
#define  DEFAULT_1080P_LVDS_PCLK           74250000
#define  DEFAULT_1080P_LVDS_MAXFPS               30
#define  DEFAULT_1080P_LVDS_LANE                  2
#define  DEFAULT_1080P_LVDS_RAWBIT            RAW12
#define  DEFAULT_1080P_LVDS_BAYER          BAYER_GB
#define  DEFAULT_1080P_LVDS_INTERFACE       IF_LVDS

//lvds setting
#define  DEFAULT_720P_LVDS_MODE      MODE_LVDS_720P
#define  DEFAULT_720P_LVDS_VMAX                 750
#define  DEFAULT_720P_LVDS_HMAX                3300
#define  DEFAULT_720P_LVDS_WIDTH               1292//lvds need crop 12 ob line,so need add 12 line
#define  DEFAULT_720P_LVDS_HEIGHT               730//lvds need crop 10 ob line,so need add 10 line
#define  DEFAULT_720P_LVDS_MIRROR                 0
#define  DEFAULT_720P_LVDS_FLIP                   0
#define  DEFAULT_720P_LVDS_TROW                 222
#define  DEFAULT_720P_LVDS_PCLK            74250000
#define  DEFAULT_720P_LVDS_MAXFPS                60
#define  DEFAULT_720P_LVDS_LANE                   2
#define  DEFAULT_720P_LVDS_RAWBIT             RAW12
#define  DEFAULT_720P_LVDS_BAYER           BAYER_GB
#define  DEFAULT_720P_LVDS_INTERFACE        IF_LVDS

#define  Gain32X                               2048

static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_MIPI_1080P = 0,
    MODE_MIPI_720P,		
    MODE_LVDS_1080P,
    MODE_LVDS_720P,    
    MODE_MAX
};

typedef struct sensor_info {
    bool is_init;
    enum SENSOR_MODE currentmode; 
    u32 vmax;	
    u32 hmax;
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
    enum SENSOR_IF interface;	
} sensor_info_t;


static sensor_info_t g_sIMX290info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_1080P_MIPI_MODE,        
    DEFAULT_1080P_MIPI_VMAX,
    DEFAULT_1080P_MIPI_HMAX,
    DEFAULT_1080P_MIPI_WIDTH,
    DEFAULT_1080P_MIPI_HEIGHT,
    DEFAULT_1080P_MIPI_MIRROR,
    DEFAULT_1080P_MIPI_FLIP,
    DEFAULT_1080P_MIPI_TROW,
    DEFAULT_1080P_MIPI_PCLK,
    DEFAULT_1080P_MIPI_MAXFPS,
    DEFAULT_1080P_MIPI_LANE,
    DEFAULT_1080P_MIPI_RAWBIT,
    DEFAULT_1080P_MIPI_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_1080P_MIPI_INTERFACE,    
    },
    {
    SENSOR_ISINIT,
    DEFAULT_720P_MIPI_MODE,        
    DEFAULT_720P_MIPI_VMAX,
    DEFAULT_720P_MIPI_HMAX,
    DEFAULT_720P_MIPI_WIDTH,
    DEFAULT_720P_MIPI_HEIGHT,
    DEFAULT_720P_MIPI_MIRROR,
    DEFAULT_720P_MIPI_FLIP,
    DEFAULT_720P_MIPI_TROW,
    DEFAULT_720P_MIPI_PCLK,
    DEFAULT_720P_MIPI_MAXFPS,
    DEFAULT_720P_MIPI_LANE,
    DEFAULT_720P_MIPI_RAWBIT,
    DEFAULT_720P_MIPI_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_720P_MIPI_INTERFACE,    
    },    
    {
    SENSOR_ISINIT,
    DEFAULT_1080P_LVDS_MODE,        
    DEFAULT_1080P_LVDS_VMAX,
    DEFAULT_1080P_LVDS_HMAX,
    DEFAULT_1080P_LVDS_WIDTH,
    DEFAULT_1080P_LVDS_HEIGHT,
    DEFAULT_1080P_LVDS_MIRROR,
    DEFAULT_1080P_LVDS_FLIP,
    DEFAULT_1080P_LVDS_TROW,
    DEFAULT_1080P_LVDS_PCLK,
    DEFAULT_1080P_LVDS_MAXFPS,
    DEFAULT_1080P_LVDS_LANE,
    DEFAULT_1080P_LVDS_RAWBIT,
    DEFAULT_1080P_LVDS_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_1080P_LVDS_INTERFACE,    
    },
    {
    SENSOR_ISINIT,
    DEFAULT_720P_LVDS_MODE,        
    DEFAULT_720P_LVDS_VMAX,
    DEFAULT_720P_LVDS_HMAX,
    DEFAULT_720P_LVDS_WIDTH,
    DEFAULT_720P_LVDS_HEIGHT,
    DEFAULT_720P_LVDS_MIRROR,
    DEFAULT_720P_LVDS_FLIP,
    DEFAULT_720P_LVDS_TROW,
    DEFAULT_720P_LVDS_PCLK,
    DEFAULT_720P_LVDS_MAXFPS,
    DEFAULT_720P_LVDS_LANE,
    DEFAULT_720P_LVDS_RAWBIT,
    DEFAULT_720P_LVDS_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_720P_LVDS_INTERFACE,    
    },    
};

typedef struct sensor_reg {
    u16 addr;  
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_1080p_mipi_init_regs[] = {
    {0x3000, 0x01},
    {0x3002, 0x01},
    {DELAY_CODE, 50},
    {0x3005, 0x01},
    {0x3007, 0x00},
    {0x3009, 0x02},
    {0x300A, 0xF0},
    {0x300F, 0x00},
    {0x3010, 0x21},
    {0x3012, 0x64},
    {0x3016, 0x09},
    {0x3018, 0x65},
    {0x3019, 0x04},
    {0x301C, 0x30},
    {0x301D, 0x11},
    {0x3046, 0x01},
    {0x304B, 0x0A},
    {0x3055, 0x00}, // disable i2c timeout   
    {0x305C, 0x18},
    {0x305D, 0x03},
    {0x305E, 0x20},
    {0x305F, 0x01},
    {0x3070, 0x02},
    {0x3071, 0x11},
    {0x309B, 0x10},
    {0x309C, 0x22},
    {0x30A2, 0x02},
    {0x30A6, 0x20},
    {0x30A8, 0x20},
    {0x30AA, 0x20},
    {0x30AC, 0x20},
    {0x30B0, 0x43},
    {0x3119, 0x9E},
    {0x311C, 0x1E},
    {0x311E, 0x08},
    {0x3128, 0x05},
    {0x3129, 0x00},
    {0x313D, 0x83},
    {0x3150, 0x03},
    {0x315E, 0x1A},
    {0x3164, 0x1A},	
    {0x317C, 0x00},
    {0x317E, 0x00},
    {0x31EC, 0x00},
    {0x32B8, 0x50},
    {0x32B9, 0x10},
    {0x32BA, 0x00},
    {0x32BB, 0x04},
    {0x32C8, 0x50},
    {0x32C9, 0x10},
    {0x32CA, 0x00},
    {0x32CB, 0x04},
    {0x332C, 0xD3},
    {0x332D, 0x10},
    {0x332E, 0x0D},
    {0x3358, 0x06},
    {0x3359, 0xE1},
    {0x335A, 0x11},
    {0x3360, 0x1E},
    {0x3361, 0x61},
    {0x3362, 0x10},
    {0x33B0, 0x50},
    {0x33B2, 0x1A},
    {0x33B3, 0x04},
    {0x3405, 0x10},
    {0x3407, 0x01},
    {0x3414, 0x0A},
    {0x3418, 0x49},
    {0x3419, 0x04},
    {0x3441, 0x0C},
    {0x3442, 0x0C},    
    {0x3443, 0x01},
    {0x3444, 0x20},
    {0x3445, 0x25},
    {0x3446, 0x57},    
    {0x3447, 0x00},
    {0x3448, 0x37},
    {0x3449, 0x00},  
    {0x344A, 0x1F},    
    {0x344B, 0x00},
    {0x344C, 0x1F},
    {0x344D, 0x00},    
    {0x344E, 0x1F},
    {0x344F, 0x00},
    {0x3450, 0x77},
    {0x3451, 0x00},
    {0x3452, 0x1F},
    {0x3453, 0x00},    
    {0x3454, 0x17},
    {0x3455, 0x00},
    {0x3472, 0x9C},
    {0x3473, 0x07},    
    {0x3480, 0x49},
    {0x3000, 0x00},
    {DELAY_CODE, 50},
    {0x3002, 0x00},
    {DELAY_CODE, 50}, 
};
#define NUM_OF_1080P_MIPI_INIT_REGS (sizeof(sensor_1080p_mipi_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_720p_mipi_init_regs[] = {
    {0x3000, 0x01},
    {0x3002, 0x01},
    {DELAY_CODE, 50},
    {0x3005, 0x01},
    {0x3007, 0x10},
    {0x3009, 0x01},
    {0x300A, 0xF0},
    {0x300F, 0x00},
    {0x3010, 0x21},
    {0x3012, 0x64},
    {0x3016, 0x09},
    {0x3018, 0xEE},
    {0x3019, 0x02},
    {0x301C, 0xE4},
    {0x301D, 0x0C},
    {0x3046, 0x01},
    {0x304B, 0x0A},
    {0x3055, 0x00}, // disable i2c timeout       
    {0x305C, 0x20},
    {0x305D, 0x00},
    {0x305E, 0x20},
    {0x305F, 0x01},
    {0x3070, 0x02},
    {0x3071, 0x11},
    {0x309B, 0x10},
    {0x309C, 0x22},
    {0x30A2, 0x02},
    {0x30A6, 0x20},
    {0x30A8, 0x20},
    {0x30AA, 0x20},
    {0x30AC, 0x20},
    {0x30B0, 0x43},
    {0x3119, 0x9E},
    {0x311C, 0x1E},
    {0x311E, 0x08},
    {0x3128, 0x05},
    {0x3129, 0x00},
    {0x313D, 0x83},
    {0x3150, 0x03},
    {0x315E, 0x1A},
    {0x3164, 0x1A},	
    {0x317C, 0x00},
    {0x317E, 0x00},
    {0x31EC, 0x00},
    {0x32B8, 0x50},
    {0x32B9, 0x10},
    {0x32BA, 0x00},
    {0x32BB, 0x04},
    {0x32C8, 0x50},
    {0x32C9, 0x10},
    {0x32CA, 0x00},
    {0x32CB, 0x04},
    {0x332C, 0xD3},
    {0x332D, 0x10},
    {0x332E, 0x0D},
    {0x3358, 0x06},
    {0x3359, 0xE1},
    {0x335A, 0x11},
    {0x3360, 0x1E},
    {0x3361, 0x61},
    {0x3362, 0x10},
    {0x33B0, 0x50},
    {0x33B2, 0x1A},
    {0x33B3, 0x04},
    {0x3405, 0x00},
    {0x3407, 0x01},
    {0x3414, 0x04},
    {0x3418, 0xD9},
    {0x3419, 0x02},
    {0x3441, 0x0C},
    {0x3442, 0x0C},
    {0x3443, 0x01},
    {0x3444, 0x20},
    {0x3445, 0x25},
    {0x3446, 0x67},
    {0x3447, 0x00},    
    {0x3448, 0x57},
    {0x3449, 0x00},    
    {0x344A, 0x2F},
    {0x344B, 0x00},    
    {0x344C, 0x27},
    {0x344D, 0x00},    
    {0x344E, 0x2F},
    {0x344F, 0x00},    
    {0x3450, 0xBF},
    {0x3451, 0x00},    
    {0x3452, 0x2F},
    {0x3453, 0x00},    
    {0x3454, 0x27},
    {0x3455, 0x00},    
    {0x3472, 0x1C},
    {0x3473, 0x05},
    {0x3480, 0x49},
    {0x3000, 0x00},
    {DELAY_CODE, 50},
    {0x3002, 0x00},
    {DELAY_CODE, 50},
};
#define NUM_OF_720P_MIPI_INIT_REGS (sizeof(sensor_720p_mipi_init_regs) / sizeof(sensor_reg_t))


static sensor_reg_t sensor_1080p_lvds_init_regs[] = {
    {0x3000, 0x01},
    {0x3002, 0x01},
    {DELAY_CODE, 50},
    {0x3005, 0x01},
    {0x3007, 0x00},
    {0x3009, 0x02},
    {0x300A, 0xF0},
    {0x300F, 0x00},
    {0x3010, 0x21},
    {0x3012, 0x64},
    {0x3016, 0x09},
    {0x3018, 0x65},
    {0x3019, 0x04},
    {0x301C, 0x30},
    {0x301D, 0x11},
    {0x3046, 0xD1},
    {0x304B, 0x0A},
    {0x3055, 0x00}, // disable i2c timeout       
    {0x305C, 0x18},
    {0x305D, 0x00},
    {0x305E, 0x20},
    {0x305F, 0x01},
    {0x3070, 0x02},
    {0x3071, 0x11},
    {0x309B, 0x10},
    {0x309C, 0x22},
    {0x30A2, 0x02},
    {0x30A6, 0x20},
    {0x30A8, 0x20},
    {0x30AA, 0x20},
    {0x30AC, 0x20},
    {0x30B0, 0x43},
    {0x3119, 0x9E},
    {0x311C, 0x1E},
    {0x311E, 0x08},
    {0x3128, 0x05},
    {0x3129, 0x00},
    {0x313D, 0x83},
    {0x3150, 0x03},
    {0x315E, 0x1A},
    {0x3164, 0x1A},	
    {0x317C, 0x00},
    {0x317E, 0x00},
    {0x31EC, 0x00},
    {0x32B8, 0x50},
    {0x32B9, 0x10},
    {0x32BA, 0x00},
    {0x32BB, 0x04},
    {0x32C8, 0x50},
    {0x32C9, 0x10},
    {0x32CA, 0x00},
    {0x32CB, 0x04},
    {0x332C, 0xD3},
    {0x332D, 0x10},
    {0x332E, 0x0D},
    {0x3358, 0x06},
    {0x3359, 0xE1},
    {0x335A, 0x11},
    {0x3360, 0x1E},
    {0x3361, 0x61},
    {0x3362, 0x10},
    {0x33B0, 0x50},
    {0x33B2, 0x1A},
    {0x33B3, 0x04},
    {0x3418, 0x49},
    {0x3419, 0x04},
    {0x3480, 0x49},
    {0x3000, 0x00},
    {DELAY_CODE, 50},
    {0x3002, 0x00},
    {DELAY_CODE, 50},
};
#define NUM_OF_1080P_LVDS_INIT_REGS (sizeof(sensor_1080p_lvds_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_720p_lvds_init_regs[] = {
    {0x3000, 0x01},
    {0x3002, 0x01},
    {DELAY_CODE, 50},
    {0x3005, 0x01},
    {0x3007, 0x10},
    {0x3009, 0x01},
    {0x300A, 0xF0},
    {0x300F, 0x00},
    {0x3010, 0x21},
    {0x3012, 0x64},
    {0x3016, 0x09},
    {0x3018, 0xEE},
    {0x3019, 0x02},
    {0x301C, 0xE4},
    {0x301D, 0x0C},
    {0x3046, 0xD1},
    {0x304B, 0x0A},
    {0x3055, 0x00}, // disable i2c timeout       
    {0x305C, 0x20},
    {0x305D, 0x00},
    {0x305E, 0x20},
    {0x305F, 0x01},
    {0x3070, 0x02},
    {0x3071, 0x11},
    {0x309B, 0x10},
    {0x309C, 0x22},
    {0x30A2, 0x02},
    {0x30A6, 0x20},
    {0x30A8, 0x20},
    {0x30AA, 0x20},
    {0x30AC, 0x20},
    {0x30B0, 0x43},
    {0x3119, 0x9E},
    {0x311C, 0x1E},
    {0x311E, 0x08},
    {0x3128, 0x05},
    {0x3129, 0x00},
    {0x313D, 0x83},
    {0x3150, 0x03},
    {0x315E, 0x1A},
    {0x3164, 0x1A},	
    {0x317C, 0x00},
    {0x317E, 0x00},
    {0x31EC, 0x00},
    {0x32B8, 0x50},
    {0x32B9, 0x10},
    {0x32BA, 0x00},
    {0x32BB, 0x04},
    {0x32C8, 0x50},
    {0x32C9, 0x10},
    {0x32CA, 0x00},
    {0x32CB, 0x04},
    {0x332C, 0xD3},
    {0x332D, 0x10},
    {0x332E, 0x0D},
    {0x3358, 0x06},
    {0x3359, 0xE1},
    {0x335A, 0x11},
    {0x3360, 0x1E},
    {0x3361, 0x61},
    {0x3362, 0x10},
    {0x33B0, 0x50},
    {0x33B2, 0x1A},
    {0x33B3, 0x04},
    {0x3418, 0xD9},
    {0x3419, 0x02},
    {0x3480, 0x49},
    {0x3000, 0x00},
    {DELAY_CODE, 50},
    {0x3002, 0x00},
    {DELAY_CODE, 50},
};
#define NUM_OF_720P_LVDS_INIT_REGS (sizeof(sensor_720p_lvds_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  a_gain;
    u8  cg_gain;
    u32 total_gain;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00,0x00,    64}, {0x04,0x00,    73}, {0x07,0x00,    82}, {0x09,0x00,    87},
    {0x0C,0x00,    97}, {0x0E,0x00,   104}, {0x10,0x00,   111}, {0x12,0x00,   119},
    {0x14,0x00,   128}, {0x18,0x00,   147}, {0x1B,0x00,   163}, {0x1D,0x00,   174},
    {0x20,0x00,   193}, {0x22,0x00,   207}, {0x24,0x00,   222}, {0x26,0x00,   238},	
    {0x28,0x00,   255}, {0x2C,0x00,   293}, {0x2F,0x00,   324}, {0x31,0x00,   348},
    {0x34,0x00,   386}, {0x36,0x00,   413}, {0x38,0x00,   443}, {0x3A,0x00,   474},	
    {0x3C,0x00,   508}, {0x40,0x00,   584}, {0x43,0x00,   647}, {0x46,0x00,   718},
    {0x48,0x00,   769}, {0x4A,0x00,   824}, {0x4C,0x00,   883}, {0x4F,0x00,   980},	
    {0x50,0x00,  1014}, {0x54,0x00,  1165}, {0x57,0x00,  1292}, {0x5A,0x00,  1433},
    {0x5C,0x00,  1535}, {0x5E,0x00,  1645}, {0x60,0x00,  1763}, {0x63,0x00,  1955},	
    {0x64,0x00,  2024}, {0x54,0x10,  2329}, {0x57,0x10,  2584}, {0x5A,0x10,  2866},	
    {0x5C,0x10,  3071}, {0x5E,0x10,  3290}, {0x60,0x10,  3525}, {0x63,0x10,  3910},	    
    {0x64,0x10,  4048}, {0x67,0x10,  4490}, {0x6A,0x10,  4980}, {0x6D,0x10,  5523},
    {0x70,0x10,  6126}, {0x72,0x10,  6565}, {0x74,0x10,  7034}, {0x76,0x10,  7537},
    {0x78,0x10,  8076}, {0x7B,0x10,  8958}, {0x7E,0x10,  9936}, {0x81,0x10, 11021},
    {0x84,0x10, 12224}, {0x86,0x10, 13098}, {0x88,0x10, 14035}, {0x8A,0x10, 15039},
    {0x8C,0x10, 16114}, {0x8F,0x10, 17874}, {0x92,0x10, 19825}, {0x95,0x10, 21989},  
    {0x98,0x10, 24390}, {0x9A,0x10, 26134}, {0x9C,0x10, 28003}, {0x9E,0x10, 30006},
    {0xA0,0x10, 32152}, {0xA3,0x10, 35662}, {0xA7,0x10, 40946}, {0xA9,0x10, 43874},
    {0xAC,0x10, 48664}, {0xAE,0x10, 52145}, {0xB0,0x10, 55874}, {0xB2,0x10, 59870}, 	
    {0xB4,0x10, 64152}, {0xB8,0x10, 73656}, {0xBB,0x10, 81698}, {0xBD,0x10, 87541},
    {0xC0,0x10, 97097}, {0xC2,0x10,104042}, {0xC4,0x10,111483}, {0xC6,0x10,119457},
    {0xC8,0x10,128000}, {0xCC,0x10,146964}, {0xCF,0x10,163008}, {0xD1,0x10,174667}, 	
    {0xD4,0x10,193736}, {0xD6,0x10,207592}, {0xD8,0x10,222439}, {0xDA,0x10,238347},
    {0xDC,0x10,255394}, {0xE0,0x10,293231}, {0xE3,0x10,325245}, {0xE5,0x10,345406},
    {0xE8,0x10,386554}, {0xEA,0x10,414200}, {0xEC,0x10,443823}, {0xEE,0x10,475565}, 
    {0xF0,0x10,509577},    
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

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
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk= g_sIMX290info[pinfo->currentmode].pclk;
	
    //isp_info("imx290 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hmax=g_sIMX290info[pinfo->currentmode].hmax;
    pinfo->t_row=g_sIMX290info[pinfo->currentmode].t_row;

    //isp_info("h_total=%d, t_row=%d\n", pinfo->hmax, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 shs1_h,shs1_m,shs1_l,shs1_total;
    u32 vmax_h,vmax_m,vmax_l,vmax_total;
    u32 lines;
	
    if (!g_psensor->curr_exp) {
        read_reg(0x301A, &vmax_h);
        read_reg(0x3019, &vmax_m);
        read_reg(0x3018, &vmax_l);
		
        read_reg(0x3022, &shs1_h);
        read_reg(0x3021, &shs1_m);
        read_reg(0x3020, &shs1_l);

        vmax_total = (vmax_l&0xFF)|((vmax_m&0xFF)<<8)|((vmax_h&0x03)<<16);	
        shs1_total = (shs1_l&0xFF)|((shs1_m&0xFF)<<8)|((shs1_h&0x03)<<16);	
		
        lines = vmax_total-shs1_total-1;
        g_psensor->curr_exp = (lines * (pinfo->t_row) + 500) / 1000;
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line,shs1;
		
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1>exp_line)
        exp_line = 1;

    if (exp_line>(SENSOR_MAX_VMAX-SENSOR_NON_EXP_LINE))
        exp_line=SENSOR_MAX_VMAX-SENSOR_NON_EXP_LINE;
	
    if (exp_line > ((pinfo->vmax)-SENSOR_NON_EXP_LINE)) {
        write_reg(0x301A, (((exp_line+SENSOR_NON_EXP_LINE) >> 16) & 0x03));    
        write_reg(0x3019, (((exp_line+SENSOR_NON_EXP_LINE)>>8) & 0xFF));
        write_reg(0x3018, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
        shs1=1;
        pinfo->long_exp = 1;
    } else {
        if (pinfo->long_exp) {			
            write_reg(0x301A, (((pinfo->vmax)>> 16) & 0x03));    
            write_reg(0x3019, (((pinfo->vmax)>>8) & 0xFF));
            write_reg(0x3018, ((pinfo->vmax) & 0xFF));
            pinfo->long_exp = 0;
        }		
            shs1=(pinfo->vmax)-exp_line-1;		
    }
    
    write_reg(0x3022, (shs1 >> 16) & 0x03);
    write_reg(0x3021, (shs1 >> 8) & 0xFF);
    write_reg(0x3020, (shs1 & 0xFF));
    
    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;

}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;

    max_fps = g_sIMX290info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vmax = (g_sIMX290info[pinfo->currentmode].vmax) * max_fps / fps;

    if ((pinfo->vmax) > SENSOR_MAX_VMAX)
        pinfo->vmax = SENSOR_MAX_VMAX;
		
    //isp_info("adjust_blk fps=%d, vmax=%d\n", fps, pinfo->vmax);

    write_reg(0x301A, ((pinfo->vmax)>>16)&0x3);
    write_reg(0x3019, ((pinfo->vmax)>>8) & 0xFF);
    write_reg(0x3018, (pinfo->vmax) & 0xFF);

    get_exp();
    set_exp(g_psensor->curr_exp);
	
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    u32 a_gain,cg_gain;
    int i=0;

    if (!g_psensor->curr_gain) {
        read_reg(0x3014, &a_gain);
        read_reg(0x3009, &cg_gain);			
        if (a_gain > gain_table[NUM_OF_GAINSET - 1].a_gain)
            a_gain = gain_table[NUM_OF_GAINSET - 1].a_gain;
        else if(a_gain < gain_table[0].a_gain)
            a_gain = gain_table[0].a_gain;

        cg_gain=((cg_gain>>4) & 0x01);
        if (0!=cg_gain) {
            // search most suitable gain into gain table
            for (i=41; i<NUM_OF_GAINSET; i++) {
                if (gain_table[i].a_gain > a_gain)
                    break;
            }
        } else {
            for (i=0; i<NUM_OF_GAINSET; i++) {
                if (gain_table[i].a_gain > a_gain)
                    break;            
            }
        }	
        g_psensor->curr_gain = (gain_table[i-1].total_gain)*(cg_gain+1);			
    }	
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    u8 a_gain=0;
    u32 cg_gain;
    static u32 pre_cg_gain=0;
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
	
    read_reg(0x3009, &cg_gain);

    if (gain_table[i-1].total_gain >= Gain32X)
        cg_gain=cg_gain|BIT4;		
    else
        cg_gain=cg_gain&~BIT4;
	
    a_gain = gain_table[i-1].a_gain;
    write_reg(0x3014, a_gain);	
    if (pre_cg_gain!=cg_gain) {
        isp_api_wait_frame_start();	
        write_reg(0x3009, cg_gain);		
        pre_cg_gain=cg_gain;
    }
	
    g_psensor->curr_gain = gain_table[i-1].total_gain;

    return 0;
}

static int get_mirror(void)
{
    u32 reg;
    read_reg(0x3007, &reg);
    return (reg & BIT1) ? 1 : 0;    
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    read_reg(0x3007, &reg);

    if (pinfo->flip)
        reg |=BIT0;
    else
        reg &=~BIT0;
    
    if (enable)
        reg |= BIT1;
    else
        reg &= ~BIT1;
    write_reg(0x3007, reg);

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x3007, &reg);
    return (reg & BIT0) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;
    
    pinfo->flip= enable;

    read_reg(0x3007, &reg);

    if (pinfo->mirror)
        reg |= BIT1;
    else
        reg &= ~BIT1;
    if (enable)
        reg |= BIT0;
    else
        reg &= ~BIT0;
    write_reg(0x3007, reg);

    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    int ret = 0;
    
    //isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }

    if ((width)!=g_sIMX290info[pinfo->currentmode].img_w) {
        sensor_reg_t* pInitTable;
        u32 InitTable_Num;
        int i=0;	

        if (IF_MIPI==g_sIMX290info[pinfo->currentmode].interface) {
		        if (1280>=width) {
                pinfo->currentmode=MODE_MIPI_720P;				
                pInitTable=sensor_720p_mipi_init_regs;
                InitTable_Num=NUM_OF_720P_MIPI_INIT_REGS;			
            } else {
                pinfo->currentmode=MODE_MIPI_1080P;	    	    
                pInitTable=sensor_1080p_mipi_init_regs;
                InitTable_Num=NUM_OF_1080P_MIPI_INIT_REGS;    	    
            }
        } else {
		        if(1280>=width) {	
                pinfo->currentmode=MODE_LVDS_720P;					
                pInitTable=sensor_720p_lvds_init_regs;
                InitTable_Num=NUM_OF_720P_LVDS_INIT_REGS;			
            } else {
                pinfo->currentmode=MODE_LVDS_1080P;    	    
                pInitTable=sensor_1080p_lvds_init_regs;
                InitTable_Num=NUM_OF_1080P_LVDS_INIT_REGS;            
            }
        }
		
        g_psensor->pclk = get_pclk(g_psensor->xclk);			
        // set initial registers
        for (i=0; i<InitTable_Num; i++) {
            if (pInitTable[i].addr == DELAY_CODE) {
                mdelay(pInitTable[i].val);
            } else if (write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
                isp_err("%s init fail!!", SENSOR_NAME);
                return -EINVAL;
            }
        }
		
        set_mirror(pinfo->mirror);
        set_flip(pinfo->flip);
        calculate_t_row();
        adjust_blk(g_psensor->fps);
    }

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sIMX290info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sIMX290info[pinfo->currentmode].img_h;	
    
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;

    if (MODE_LVDS_1080P==pinfo->currentmode) {
        g_psensor->img_win.x = 12;//lvds need isp to crop ob line 	
        g_psensor->img_win.y = 20;//lvds need isp to crop ob line        
    } else if (MODE_LVDS_720P==pinfo->currentmode) {
        g_psensor->img_win.x = 12;//lvds need isp to crop ob line 		    
        g_psensor->img_win.y = 10;//lvds need isp to crop ob line  
    } else {
        g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;		
        g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;
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
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;
    int i,ret = 0;
    
    if (pinfo->is_init)
        return 0;

    pinfo->mirror = mirror;
    pinfo->flip = flip;

    //only one sensor mode 1080p
    if (MODE_MIPI_1080P==pinfo->currentmode) {
        pInitTable=sensor_1080p_mipi_init_regs;
        InitTable_Num=NUM_OF_1080P_MIPI_INIT_REGS;
    } else if(MODE_MIPI_720P==pinfo->currentmode) {
        pInitTable=sensor_720p_mipi_init_regs;
        InitTable_Num=NUM_OF_720P_MIPI_INIT_REGS;
    } else if(MODE_LVDS_1080P==pinfo->currentmode) {
        pInitTable=sensor_1080p_lvds_init_regs;
        InitTable_Num=NUM_OF_1080P_LVDS_INIT_REGS;
    } else if(MODE_LVDS_720P==pinfo->currentmode) {
        pInitTable=sensor_720p_lvds_init_regs;
        InitTable_Num=NUM_OF_720P_LVDS_INIT_REGS;
    } else {
        isp_err("%s init fail!!", SENSOR_NAME);
        return -EINVAL;    
    }

    //isp_info("start initial...\n");

    // set initial registers
    for (i=0; i<InitTable_Num; i++) {
        if (pInitTable[i].addr == DELAY_CODE) {
            mdelay(pInitTable[i].val);
        } else if (write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
            isp_err("%s init fail!!", SENSOR_NAME);
            return -EINVAL;
        }
    }

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);

    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);

    calculate_t_row();
    adjust_blk(g_psensor->fps);

    if (ret == 0)
        pinfo->is_init = true;
    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current exposure and gain setting
    get_exp();
    get_gain();

    //isp_info("initial end ...\n"); 	
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void imx290_deconstruct(void)
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

static int imx290_construct(u32 xclk, u16 width, u16 height)
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

    if(IF_MIPI==interface) {
        if (1280>=width)
            pinfo->currentmode=MODE_MIPI_720P;
        else
            pinfo->currentmode=MODE_MIPI_1080P;
    } else if (IF_LVDS==interface) {
        if (1280>=width)
            pinfo->currentmode=MODE_LVDS_720P;
		else
            pinfo->currentmode=MODE_LVDS_1080P;
    }	
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sIMX290info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sIMX290info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sIMX290info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sIMX290info[pinfo->currentmode].bayer;
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

    return 0;

construct_err:
    imx290_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init imx290_init(void)
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

    if ((ret = imx290_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit imx290_exit(void)
{
    isp_unregister_sensor(g_psensor);
    imx290_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(imx290_init);
module_exit(imx290_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor imx290");
MODULE_LICENSE("GPL");
