/**
 * @file sen_ar0230.c
 * Aptina AR0230 sensor driver
 *
 * Copyright (C) 2014 GM Corp. (http://www.grain-media.com)
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
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1920
#define DEFAULT_IMAGE_HEIGHT    1080
#define DEFAULT_XCLK            27000000
#define DELAY_CODE              0xFFFF

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

static uint binning = 0;
module_param(binning, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(binning, "sensor binning");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint wdr = 0;
module_param(wdr, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wdr, "WDR mode");


//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "AR0230"
#define SENSOR_MAX_WIDTH    1920
#define SENSOR_MAX_HEIGHT   1080

#define WDR_EXP_LIMIT

//#define SENSOR_LSC

static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    u32 img_w;
    u32 img_h;
    int row_bin;
    int col_bin;
    int mirror;
    int flip;
    u32 hb;
    u32 t_row;          // unit is 1/10 us
    u32 sensor_id;
} sensor_info_t;

typedef struct sensor_reg {
    u16  addr;
    u16  val;
} sensor_reg_t;

static sensor_reg_t sensor_init_regs[] = {
    //[RESET]
    //STATE= Sensor Reset, 1
    {DELAY_CODE, 200},
    //STATE= Sensor Reset, 0
    {0x301A, 0x0001},
    {DELAY_CODE, 200},
    {0x301A, 0x10D8},
    //XMCLK= 27000000
    //[==== Parallel ====]
    //[Parallel Linear 1080p30]
    //Load = Reset
    {DELAY_CODE, 200},

    //[AR0230 REV1.2 Optimized Settings]
    {0x2436, 0x000E},	  
    {0x320C, 0x0180},     				
    {0x320E, 0x0300},      					
    {0x3210, 0x0500},      					 
    {0x3204, 0x0B6D}, 
    {0x30FE, 0x0080},  					 			
    {0x3ED8, 0x7B99},
    {0x3EDC, 0x9BA8},
    {0x3EDA, 0x9B9B},
    {0x3092, 0x006F},
    {0x3EEC, 0x1C04},
    {0x30BA, 0x779C},
    {0x3EF6, 0xA70F},
    {0x3044, 0x0410}, 
    {0x3ED0, 0xFF44}, 
    {0x3ED4, 0x031F}, 
    {0x30FE, 0x0080},  
    {0x3EE2, 0x8866},  
    {0x3EE4, 0x6623}, 
    {0x3EE6, 0x2263}, 
    {0x30E0, 0x4283},
    {0x30F0, 0x1283},

    {0x30B0, 0x0000},
    {0x31AC, 0x0C0C},

    //[Linear Mode Sequencer - Rev1.3]  
    {0x301A, 0x0059},
    {DELAY_CODE, 200},
    {0x3088, 0x8242},
    {0x3086, 0x4558},
    {0x3086, 0x729B},
    {0x3086, 0x4A31},
    {0x3086, 0x4342},
    {0x3086, 0x8E03},
    {0x3086, 0x2A14},
    {0x3086, 0x4578},
    {0x3086, 0x7B3D},
    {0x3086, 0xFF3D},
    {0x3086, 0xFF3D},
    {0x3086, 0xEA2A},
    {0x3086, 0x043D},
    {0x3086, 0x102A},
    {0x3086, 0x052A},
    {0x3086, 0x1535},
    {0x3086, 0x2A05},
    {0x3086, 0x3D10},
    {0x3086, 0x4558},
    {0x3086, 0x2A04},
    {0x3086, 0x2A14},
    {0x3086, 0x3DFF},
    {0x3086, 0x3DFF},
    {0x3086, 0x3DEA},
    {0x3086, 0x2A04},
    {0x3086, 0x622A},
    {0x3086, 0x288E},
    {0x3086, 0x0036},
    {0x3086, 0x2A08},
    {0x3086, 0x3D64},
    {0x3086, 0x7A3D},
    {0x3086, 0x0444},
    {0x3086, 0x2C4B},
    {0x3086, 0x8F03},
    {0x3086, 0x430D},
    {0x3086, 0x2D46},
    {0x3086, 0x4316},
    {0x3086, 0x5F16},
    {0x3086, 0x530D},
    {0x3086, 0x1660},
    {0x3086, 0x3E4C},
    {0x3086, 0x2904},
    {0x3086, 0x2984},
    {0x3086, 0x8E03},
    {0x3086, 0x2AFC},
    {0x3086, 0x5C1D},
    {0x3086, 0x5754},
    {0x3086, 0x495F},
    {0x3086, 0x5305},
    {0x3086, 0x5307},
    {0x3086, 0x4D2B},
    {0x3086, 0xF810},
    {0x3086, 0x164C},
    {0x3086, 0x0955},
    {0x3086, 0x562B},
    {0x3086, 0xB82B},
    {0x3086, 0x984E},
    {0x3086, 0x1129},
    {0x3086, 0x9460},
    {0x3086, 0x5C19},
    {0x3086, 0x5C1B},
    {0x3086, 0x4548},
    {0x3086, 0x4508},
    {0x3086, 0x4588},
    {0x3086, 0x29B6},
    {0x3086, 0x8E01},
    {0x3086, 0x2AF8},
    {0x3086, 0x3E02},
    {0x3086, 0x2AFA},
    {0x3086, 0x3F09},
    {0x3086, 0x5C1B},
    {0x3086, 0x29B2},
    {0x3086, 0x3F0C},
    {0x3086, 0x3E03},
    {0x3086, 0x3E15},
    {0x3086, 0x5C13},
    {0x3086, 0x3F11},
    {0x3086, 0x3E0F},
    {0x3086, 0x5F2B},
    {0x3086, 0x902A},
    {0x3086, 0xF22B},
    {0x3086, 0x803E},
    {0x3086, 0x063F},
    {0x3086, 0x0660},
    {0x3086, 0x29A2},
    {0x3086, 0x29A3},
    {0x3086, 0x5F4D},
    {0x3086, 0x1C2A},
    {0x3086, 0xFA29},
    {0x3086, 0x8345},
    {0x3086, 0xA83E},
    {0x3086, 0x072A},
    {0x3086, 0xFB3E},
    {0x3086, 0x2945},
    {0x3086, 0x8824},
    {0x3086, 0x3E08},
    {0x3086, 0x2AFA},
    {0x3086, 0x5D29},
    {0x3086, 0x9288},
    {0x3086, 0x102B},
    {0x3086, 0x048B},
    {0x3086, 0x1686},
    {0x3086, 0x8D48},
    {0x3086, 0x4D4E},
    {0x3086, 0x2B80},
    {0x3086, 0x4C0B},
    {0x3086, 0x603F},
    {0x3086, 0x302A},
    {0x3086, 0xF23F},
    {0x3086, 0x1029},
    {0x3086, 0x8229},
    {0x3086, 0x8329},
    {0x3086, 0x435C},
    {0x3086, 0x155F},
    {0x3086, 0x4D1C},
    {0x3086, 0x2AFA},
    {0x3086, 0x4558},
    {0x3086, 0x8E00},
    {0x3086, 0x2A98},
    {0x3086, 0x3F0A},
    {0x3086, 0x4A0A},
    {0x3086, 0x4316},
    {0x3086, 0x0B43},
    {0x3086, 0x168E},
    {0x3086, 0x032A},
    {0x3086, 0x9C45},
    {0x3086, 0x783F},
    {0x3086, 0x072A},
    {0x3086, 0x9D3E},
    {0x3086, 0x305D},
    {0x3086, 0x2944},
    {0x3086, 0x8810},
    {0x3086, 0x2B04},
    {0x3086, 0x530D},
    {0x3086, 0x4558},
    {0x3086, 0x3E08},
    {0x3086, 0x8E01},
    {0x3086, 0x2A98},
    {0x3086, 0x8E00},
    {0x3086, 0x769C},
    {0x3086, 0x779C},
    {0x3086, 0x4644},
    {0x3086, 0x1616},
    {0x3086, 0x907A},
    {0x3086, 0x1244},
    {0x3086, 0x4B18},
    {0x3086, 0x4A04},
    {0x3086, 0x4316},
    {0x3086, 0x0643},
    {0x3086, 0x1605},
    {0x3086, 0x4316},
    {0x3086, 0x0743},
    {0x3086, 0x1658},
    {0x3086, 0x4316},
    {0x3086, 0x5A43},
    {0x3086, 0x1645},
    {0x3086, 0x588E},
    {0x3086, 0x032A},
    {0x3086, 0x9C45},
    {0x3086, 0x787B},
    {0x3086, 0x3F07},
    {0x3086, 0x2A9D},
    {0x3086, 0x530D},
    {0x3086, 0x8B16},
    {0x3086, 0x863E},
    {0x3086, 0x2345},
    {0x3086, 0x5825},
    {0x3086, 0x3E10},
    {0x3086, 0x8E01},
    {0x3086, 0x2A98},
    {0x3086, 0x8E00},
    {0x3086, 0x3E10},
    {0x3086, 0x8D60},
    {0x3086, 0x1244},
    {0x3086, 0x4B2C},
    {0x3086, 0x2C2C},

    {0x3082, 0x0009},
    {0x30BA, 0x760C},
    {0x31E0, 0x0200},

    //[ALTM Bypassed]
    {0x2400, 0x0001},
    {0x301E, 0x00A8},
    {0x2450, 0x0000},
    {0x320A, 0x0080},

    //[Motion Compensation Off]
    {0x318C, 0x0000},

    //[Linear low DCG]
    {0x3060, 0x000B},
    {0x3096, 0x0080},
    {0x3098, 0x0080},
    //ADACD low DCG	
    {0x3206, 0x0B08},
    {0x3208, 0x1E13},
    {0x3202, 0x0080},
    {0x3200, 0x0002},

    {0x3100, 0x0000},
    
    //[ADACD Disabled]
    {0x3200, 0x0000},  //ADACD_CONTROL
    //[Companding Disabled]
    {0x31D0, 0x0000}, //COMPANDING

    // Parallel Setup
    {0x31AE, 0x0301},  //HiSpi 1 lane for Parallel
    {0x306E, 0x9010},  //datapath_select
    {0x301A, 0x10D8},  //R0x301A[7] parallel enable

    //[PLL_settings - Parallel]
    {0x302A, 0x0008},
    {0x302C, 0x0001},
    {0x302E, 0x0002},
    {0x3030, 0x002C},
    {0x3036, 0x000C},
    {0x3038, 0x0001},

    {0x3004, 0x0000},  //x_addr_start
    {0x3008, 0x0787},  //x_addr_end
    {0x3002, 0x0000},  //y_addr_start
    {0x3006, 0x0437},  //y_addr_end
    {0x30A2, 0x0001},  //x_odd_inc
    {0x30A6, 0x0001},  //y_odd_inc
    {0x3040, 0x0000},  //read mode

    {0x300A, 1106},  //frame_length_line
    {0x300C, 1118},  //line_length_pck
//    {0x3012, 0x0416},  //coarse_integration_time

    {0x301A, 0x10DC},
};

#define NUM_OF_INIT_REGS (sizeof(sensor_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_wdr_init_regs[] = {
    //[RESET]
    //STATE= Sensor Reset, 1
    {DELAY_CODE, 200},
    //STATE= Sensor Reset, 0
    {0x301A, 0x0001},
    {DELAY_CODE, 200},
    {0x301A, 0x10D8},
    //XMCLK= 27000000
    //[==== Parallel ====]
    //[Parallel Linear 1080p30]
    //Load = Reset
    {DELAY_CODE, 200},

    //[AR0230 REV1.2 Optimized Settings]
    {0x2436, 0x000E},	  
    {0x320C, 0x0180},     				
    {0x320E, 0x0300},      					
    {0x3210, 0x0500},      					 
    {0x3204, 0x0B6D}, 
    {0x30FE, 0x0080},  					 			
    {0x3ED8, 0x7B99},
    {0x3EDC, 0x9BA8},
    {0x3EDA, 0x9B9B},
    {0x3092, 0x006F},
    {0x3EEC, 0x1C04},
    {0x30BA, 0x779C},
    {0x3EF6, 0xA70F},
    {0x3044, 0x0410}, 
    {0x3ED0, 0xFF44}, 
    {0x3ED4, 0x031F}, 
    {0x30FE, 0x0080},  
    {0x3EE2, 0x8866},  
    {0x3EE4, 0x6623}, 
    {0x3EE6, 0x2263}, 
    {0x30E0, 0x4283},
    {0x30F0, 0x1283},

    {0x30B0, 0x0000},
    {0x31AC, 0x0C0C},

    //[HDR Mode Sequencer - Rev1.2] 
    {0x301A, 0x0059}, 
    {DELAY_CODE, 200},
    {0x3088, 0x8000}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x729B}, 
    {0x3086, 0x4A31}, 
    {0x3086, 0x4342}, 
    {0x3086, 0x8E03}, 
    {0x3086, 0x2A14}, 
    {0x3086, 0x4578}, 
    {0x3086, 0x7B3D}, 
    {0x3086, 0xFF3D}, 
    {0x3086, 0xFF3D}, 
    {0x3086, 0xEA2A}, 
    {0x3086, 0x043D}, 
    {0x3086, 0x102A}, 
    {0x3086, 0x052A}, 
    {0x3086, 0x1535}, 
    {0x3086, 0x2A05}, 
    {0x3086, 0x3D10}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x2A04}, 
    {0x3086, 0x2A14}, 
    {0x3086, 0x3DFF}, 
    {0x3086, 0x3DFF}, 
    {0x3086, 0x3DEA}, 
    {0x3086, 0x2A04}, 
    {0x3086, 0x622A}, 
    {0x3086, 0x288E}, 
    {0x3086, 0x0036}, 
    {0x3086, 0x2A08}, 
    {0x3086, 0x3D64}, 
    {0x3086, 0x7A3D}, 
    {0x3086, 0x0444}, 
    {0x3086, 0x2C4B}, 
    {0x3086, 0x8F00}, 
    {0x3086, 0x430C}, 
    {0x3086, 0x2D63}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x8E03}, 
    {0x3086, 0x2AFC}, 
    {0x3086, 0x5C1D}, 
    {0x3086, 0x5754}, 
    {0x3086, 0x495F}, 
    {0x3086, 0x5305}, 
    {0x3086, 0x5307}, 
    {0x3086, 0x4D2B}, 
    {0x3086, 0xF810}, 
    {0x3086, 0x164C}, 
    {0x3086, 0x0855}, 
    {0x3086, 0x562B}, 
    {0x3086, 0xB82B}, 
    {0x3086, 0x984E}, 
    {0x3086, 0x1129}, 
    {0x3086, 0x0429}, 
    {0x3086, 0x8429}, 
    {0x3086, 0x9460}, 
    {0x3086, 0x5C19}, 
    {0x3086, 0x5C1B}, 
    {0x3086, 0x4548}, 
    {0x3086, 0x4508}, 
    {0x3086, 0x4588}, 
    {0x3086, 0x29B6}, 
    {0x3086, 0x8E01}, 
    {0x3086, 0x2AF8}, 
    {0x3086, 0x3E02}, 
    {0x3086, 0x2AFA}, 
    {0x3086, 0x3F09}, 
    {0x3086, 0x5C1B}, 
    {0x3086, 0x29B2}, 
    {0x3086, 0x3F0C}, 
    {0x3086, 0x3E02}, 
    {0x3086, 0x3E13}, 
    {0x3086, 0x5C13}, 
    {0x3086, 0x3F11}, 
    {0x3086, 0x3E0B}, 
    {0x3086, 0x5F2B}, 
    {0x3086, 0x902A}, 
    {0x3086, 0xF22B}, 
    {0x3086, 0x803E}, 
    {0x3086, 0x043F}, 
    {0x3086, 0x0660}, 
    {0x3086, 0x29A2}, 
    {0x3086, 0x29A3}, 
    {0x3086, 0x5F4D}, 
    {0x3086, 0x192A}, 
    {0x3086, 0xFA29}, 
    {0x3086, 0x8345}, 
    {0x3086, 0xA83E}, 
    {0x3086, 0x072A}, 
    {0x3086, 0xFB3E}, 
    {0x3086, 0x2945}, 
    {0x3086, 0x8821}, 
    {0x3086, 0x3E08}, 
    {0x3086, 0x2AFA}, 
    {0x3086, 0x5D29}, 
    {0x3086, 0x9288}, 
    {0x3086, 0x102B}, 
    {0x3086, 0x048B}, 
    {0x3086, 0x1685}, 
    {0x3086, 0x8D48}, 
    {0x3086, 0x4D4E}, 
    {0x3086, 0x2B80}, 
    {0x3086, 0x4C0B}, 
    {0x3086, 0x603F}, 
    {0x3086, 0x282A}, 
    {0x3086, 0xF23F}, 
    {0x3086, 0x0F29}, 
    {0x3086, 0x8229}, 
    {0x3086, 0x8329}, 
    {0x3086, 0x435C}, 
    {0x3086, 0x155F}, 
    {0x3086, 0x4D19}, 
    {0x3086, 0x2AFA}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x8E00}, 
    {0x3086, 0x2A98}, 
    {0x3086, 0x3F06}, 
    {0x3086, 0x1244}, 
    {0x3086, 0x4A04}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x0543}, 
    {0x3086, 0x1658}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x5A43}, 
    {0x3086, 0x1606}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x0743}, 
    {0x3086, 0x168E}, 
    {0x3086, 0x032A}, 
    {0x3086, 0x9C45}, 
    {0x3086, 0x787B}, 
    {0x3086, 0x3F07}, 
    {0x3086, 0x2A9D}, 
    {0x3086, 0x3E2E}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x253E}, 
    {0x3086, 0x068E}, 
    {0x3086, 0x012A}, 
    {0x3086, 0x988E}, 
    {0x3086, 0x0012}, 
    {0x3086, 0x444B}, 
    {0x3086, 0x0343}, 
    {0x3086, 0x2D46}, 
    {0x3086, 0x4316}, 
    {0x3086, 0xA343}, 
    {0x3086, 0x165D}, 
    {0x3086, 0x0D29}, 
    {0x3086, 0x4488}, 
    {0x3086, 0x102B}, 
    {0x3086, 0x0453}, 
    {0x3086, 0x0D8B}, 
    {0x3086, 0x1685}, 
    {0x3086, 0x448E}, 
    {0x3086, 0x032A}, 
    {0x3086, 0xFC5C}, 
    {0x3086, 0x1D8D}, 
    {0x3086, 0x6057}, 
    {0x3086, 0x5449}, 
    {0x3086, 0x5F53}, 
    {0x3086, 0x0553}, 
    {0x3086, 0x074D}, 
    {0x3086, 0x2BF8}, 
    {0x3086, 0x1016}, 
    {0x3086, 0x4C08}, 
    {0x3086, 0x5556}, 
    {0x3086, 0x2BB8}, 
    {0x3086, 0x2B98}, 
    {0x3086, 0x4E11}, 
    {0x3086, 0x2904}, 
    {0x3086, 0x2984}, 
    {0x3086, 0x2994}, 
    {0x3086, 0x605C}, 
    {0x3086, 0x195C}, 
    {0x3086, 0x1B45}, 
    {0x3086, 0x4845}, 
    {0x3086, 0x0845}, 
    {0x3086, 0x8829}, 
    {0x3086, 0xB68E}, 
    {0x3086, 0x012A}, 
    {0x3086, 0xF83E}, 
    {0x3086, 0x022A}, 
    {0x3086, 0xFA3F}, 
    {0x3086, 0x095C}, 
    {0x3086, 0x1B29}, 
    {0x3086, 0xB23F}, 
    {0x3086, 0x0C3E}, 
    {0x3086, 0x023E}, 
    {0x3086, 0x135C}, 
    {0x3086, 0x133F}, 
    {0x3086, 0x113E}, 
    {0x3086, 0x0B5F}, 
    {0x3086, 0x2B90}, 
    {0x3086, 0x2AF2}, 
    {0x3086, 0x2B80}, 
    {0x3086, 0x3E04}, 
    {0x3086, 0x3F06}, 
    {0x3086, 0x6029}, 
    {0x3086, 0xA229}, 
    {0x3086, 0xA35F}, 
    {0x3086, 0x4D1C}, 
    {0x3086, 0x2AFA}, 
    {0x3086, 0x2983}, 
    {0x3086, 0x45A8}, 
    {0x3086, 0x3E07}, 
    {0x3086, 0x2AFB}, 
    {0x3086, 0x3E29}, 
    {0x3086, 0x4588}, 
    {0x3086, 0x243E}, 
    {0x3086, 0x082A}, 
    {0x3086, 0xFA5D}, 
    {0x3086, 0x2992}, 
    {0x3086, 0x8810}, 
    {0x3086, 0x2B04}, 
    {0x3086, 0x8B16}, 
    {0x3086, 0x868D}, 
    {0x3086, 0x484D}, 
    {0x3086, 0x4E2B}, 
    {0x3086, 0x804C}, 
    {0x3086, 0x0B60}, 
    {0x3086, 0x3F28}, 
    {0x3086, 0x2AF2}, 
    {0x3086, 0x3F0F}, 
    {0x3086, 0x2982}, 
    {0x3086, 0x2983}, 
    {0x3086, 0x2943}, 
    {0x3086, 0x5C15}, 
    {0x3086, 0x5F4D}, 
    {0x3086, 0x1C2A}, 
    {0x3086, 0xFA45}, 
    {0x3086, 0x588E}, 
    {0x3086, 0x002A}, 
    {0x3086, 0x983F}, 
    {0x3086, 0x064A}, 
    {0x3086, 0x739D}, 
    {0x3086, 0x0A43}, 
    {0x3086, 0x160B}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x8E03}, 
    {0x3086, 0x2A9C}, 
    {0x3086, 0x4578}, 
    {0x3086, 0x3F07}, 
    {0x3086, 0x2A9D}, 
    {0x3086, 0x3E12}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x3F04}, 
    {0x3086, 0x8E01}, 
    {0x3086, 0x2A98}, 
    {0x3086, 0x8E00}, 
    {0x3086, 0x9176}, 
    {0x3086, 0x9C77}, 
    {0x3086, 0x9C46}, 
    {0x3086, 0x4416}, 
    {0x3086, 0x1690}, 
    {0x3086, 0x7A12}, 
    {0x3086, 0x444B}, 
    {0x3086, 0x4A00}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x6343}, 
    {0x3086, 0x1608}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x5043}, 
    {0x3086, 0x1665}, 
    {0x3086, 0x4316}, 
    {0x3086, 0x6643}, 
    {0x3086, 0x168E}, 
    {0x3086, 0x032A}, 
    {0x3086, 0x9C45}, 
    {0x3086, 0x783F}, 
    {0x3086, 0x072A}, 
    {0x3086, 0x9D5D}, 
    {0x3086, 0x0C29}, 
    {0x3086, 0x4488}, 
    {0x3086, 0x102B}, 
    {0x3086, 0x0453}, 
    {0x3086, 0x0D8B}, 
    {0x3086, 0x1686}, 
    {0x3086, 0x3E1F}, 
    {0x3086, 0x4558}, 
    {0x3086, 0x283E}, 
    {0x3086, 0x068E}, 
    {0x3086, 0x012A}, 
    {0x3086, 0x988E}, 
    {0x3086, 0x008D}, 
    {0x3086, 0x6012}, 
    {0x3086, 0x444B}, 
    {0x3086, 0x2C2C}, 
    {0x3086, 0x2C2C}, 
    //HDR 16x	
    {0x3082, 0x0008},

    {0x30BA, 0x770C},
    {0x31E0, 0x0200},

    //[ALTM Bypassed]
    {0x2400, 0x0001},
    {0x301E, 0x00A8},
    {0x2450, 0x0000},
    {0x320A, 0x0080},

    //[2DMC On]
    {0x3190, 0x0000},
    {0x318A, 0x0E74},
    {0x318C, 0xC000},
    {0x3192, 0x0400},
    {0x3198, 0x183C},

    //[HDR low DCG]
    {0x3060, 0x000B},
    {0x3096, 0x0480},
    {0x3098, 0x0480},
    //ADACD low DCG
    {0x3206, 0x0B08},
    {0x3208, 0x1E13},
    {0x3202, 0x0080},
    {0x3200, 0x0002},

    {0x3100, 0x0000},

    {0x318E, 0x0200},

    //[Companding Enabled]
    {0x31AC, 0x100C},
    {0x31D0, 0x0001}, //COMPANDING

    // Parallel Setup
    {0x31AE, 0x0301},  //HiSpi 1 lane for Parallel
    {0x306E, 0x9010},  //datapath_select
    {0x301A, 0x10D8},  //R0x301A[7] parallel enable

    //[PLL_settings - Parallel]
    {0x302A, 0x0008},
    {0x302C, 0x0001},
    {0x302E, 0x0002},
    {0x3030, 0x002C},
    {0x3036, 0x000C},
    {0x3038, 0x0001},

    {0x3004, 0x0000},  //x_addr_start
    {0x3008, 0x0787},  //x_addr_end
    {0x3002, 0x0000},  //y_addr_start
    {0x3006, 0x0437},  //y_addr_end
    {0x30A2, 0x0001},  //x_odd_inc
    {0x30A6, 0x0001},  //y_odd_inc
    {0x3040, 0x0000},  //read mode

    {0x300A, 1106},  //frame_length_line
    {0x300C, 1118},  //line_length_pck
//    {0x3012, 0x0416},  //coarse_integration_time

    {0x301A, 0x10DC},
};

#define NUM_OF_WDR_INIT_REGS (sizeof(sensor_wdr_init_regs) / sizeof(sensor_reg_t))

#ifdef SENSOR_LSC
static sensor_reg_t sensor_lsc_d65_regs[] = {
    {0x3780, 0x0000},
        
    {0x3600, 0x00f0},		
    {0x3602, 0x02aa},		
    {0x3604, 0x1350},		
    {0x3606, 0xb5cd},		
    {0x3608, 0x556e},		
    {0x360a, 0x00f0},		
    {0x360c, 0x1c29},		
    {0x360e, 0x1430},		
    {0x3610, 0xd94d},		
    {0x3612, 0x3aef},		
    {0x3614, 0x00f0},		
    {0x3616, 0x29e9},		
    {0x3618, 0x0f50},		
    {0x361a, 0xa98d},		
    {0x361c, 0x104f},		
    {0x361e, 0x00b0},		
    {0x3620, 0x2549},		
    {0x3622, 0x13d0},		
    {0x3624, 0xbbcd},		
    {0x3626, 0x12cf},		
    {0x3640, 0x1ec6},		
    {0x3642, 0xc5e9},		
    {0x3644, 0x6b0b},		
    {0x3646, 0xe40d},		
    {0x3648, 0x162f},		
    {0x364a, 0x07ea},		
    {0x364c, 0x1309},		
    {0x364e, 0x10eb},		
    {0x3650, 0xb8cd},		
    {0x3652, 0x36ef},		
    {0x3654, 0xffe9},		
    {0x3656, 0x1c8c},		
    {0x3658, 0x52cd},		
    {0x365a, 0x722b},		
    {0x365c, 0xb60f},		
    {0x365e, 0x84ea},		
    {0x3660, 0x4d6b},		
    {0x3662, 0x360d},		
    {0x3664, 0xb9cb},		
    {0x3666, 0x810f},		
    {0x3680, 0x27b0},		
    {0x3682, 0x2b6d},		
    {0x3684, 0xf4d2},		
    {0x3686, 0xa751},		
    {0x3688, 0x32b5},		
    {0x368a, 0x2770},		
    {0x368c, 0x5eec},		
    {0x368e, 0xedb2},		
    {0x3690, 0x8bd1},		
    {0x3692, 0x3bd5},		
    {0x3694, 0x2050},		
    {0x3696, 0x128e},		
    {0x3698, 0xdcf2},		
    {0x369a, 0xb531},		
    {0x369c, 0x2595},		
    {0x369e, 0x2590},		
    {0x36a0, 0x744d},		
    {0x36a2, 0xef12},		
    {0x36a4, 0xb2b1},		
    {0x36a6, 0x2e15},		
    {0x36c0, 0xd5ab},		
    {0x36c2, 0xb86f},		
    {0x36c4, 0x2792},		
    {0x36c6, 0x3dd2},		
    {0x36c8, 0xfc54},		
    {0x36ca, 0x64cb},		
    {0x36cc, 0xc8af},		
    {0x36ce, 0x0772},		
    {0x36d0, 0x2892},		
    {0x36d2, 0xeab4},		
    {0x36d4, 0x0e2c},		
    {0x36d6, 0x8470},		
    {0x36d8, 0x6051},		
    {0x36da, 0x7db2},		
    {0x36dc, 0x8195},		
    {0x36de, 0x1fc9},		
    {0x36e0, 0xa36f},		
    {0x36e2, 0x68f1},		
    {0x36e4, 0x4212},		
    {0x36e6, 0xda74},		
    {0x3700, 0x92b1},		
    {0x3702, 0xafd3},		
    {0x3704, 0x3097},		
    {0x3706, 0x6b96},		
    {0x3708, 0x833a},		
    {0x370a, 0x8991},		
    {0x370c, 0xa873},		
    {0x370e, 0x3b37},		
    {0x3710, 0x5c36},		
    {0x3712, 0x8afa},		
    {0x3714, 0x9a71},		
    {0x3716, 0xdbf3},		
    {0x3718, 0x3017},		
    {0x371a, 0x00d7},		
    {0x371c, 0x869a},		
    {0x371e, 0x9f51},		
    {0x3720, 0xbe33},		
    {0x3722, 0x2ef7},		
    {0x3724, 0x6cb6},		
    {0x3726, 0x81ba},		
    {0x3782, 0x03cc},		
    {0x3784, 0x0240},		
    {0x37C0, 0x8000},		
    {0x37C2, 0x8000},		
    {0x37C6, 0x8000},		
    {0x37C4, 0x8000},		
    {0x3780, 0x8000},
};
#define NUM_OF_LSC_D65_REGS (sizeof(sensor_lsc_d65_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_lsc_cwf_regs[] = {
    {0x3780, 0x0000},
        
    {0x3600, 0x0110},
    {0x3602, 0x14a9},
    {0x3604, 0x0dd0},
    {0x3606, 0xc14d},
    {0x3608, 0x042f},
    {0x360a, 0x00d0},
    {0x360c, 0x6907},
    {0x360e, 0x1070},
    {0x3610, 0xe74d},
    {0x3612, 0x1ccf},
    {0x3614, 0x0110},
    {0x3616, 0x19a9},
    {0x3618, 0x0a50},
    {0x361a, 0xb70d},
    {0x361c, 0x45ae},
    {0x361e, 0x00b0},
    {0x3620, 0x38e7},
    {0x3622, 0x0f10},
    {0x3624, 0xc22d},
    {0x3626, 0x2def},
    {0x3640, 0x3608},
    {0x3642, 0xe82a},
    {0x3644, 0x0f8c},
    {0x3646, 0x8c2d},
    {0x3648, 0x7aae},
    {0x364a, 0x3e8a},
    {0x364c, 0x6d88},
    {0x364e, 0xb54c},
    {0x3650, 0x8e8e},
    {0x3652, 0x34f0},
    {0x3654, 0xf647},
    {0x3656, 0x260c},
    {0x3658, 0x0aae},
    {0x365a, 0x8b0a},
    {0x365c, 0xd62f},
    {0x365e, 0xafe9},
    {0x3660, 0x286b},
    {0x3662, 0x382d},
    {0x3664, 0x9008},
    {0x3666, 0xf9ae},
    {0x3680, 0x3890},
    {0x3682, 0x466c},
    {0x3684, 0xd152},
    {0x3686, 0xd390},
    {0x3688, 0x20f5},
    {0x368a, 0x39b0},
    {0x368c, 0x84ab},
    {0x368e, 0xd852},
    {0x3690, 0x88b0},
    {0x3692, 0x2f35},
    {0x3694, 0x2db0},
    {0x3696, 0x700d},
    {0x3698, 0xefb2},
    {0x369a, 0xf410},
    {0x369c, 0x3315},
    {0x369e, 0x33d0},
    {0x36a0, 0x358d},
    {0x36a2, 0xc992},
    {0x36a4, 0x9a71},
    {0x36a6, 0x1cf5},
    {0x36c0, 0x882a},
    {0x36c2, 0xe2ae},
    {0x36c4, 0x1732},
    {0x36c6, 0x6271},
    {0x36c8, 0xdd74},
    {0x36ca, 0x9387},
    {0x36cc, 0xd98f},
    {0x36ce, 0x3d72},
    {0x36d0, 0x4452},
    {0x36d2, 0x9a15},
    {0x36d4, 0xa98a},
    {0x36d6, 0xb310},
    {0x36d8, 0x4d11},
    {0x36da, 0x19d3},
    {0x36dc, 0xde74},
    {0x36de, 0x4b6a},
    {0x36e0, 0xb82e},
    {0x36e2, 0x5031},
    {0x36e4, 0x6a51},
    {0x36e6, 0xcb74},
    {0x3700, 0xb5d2},
    {0x3702, 0x9713},
    {0x3704, 0x2797},
    {0x3706, 0x42b6},
    {0x3708, 0x807a},
    {0x370a, 0xb272},
    {0x370c, 0x8833},
    {0x370e, 0x2457},
    {0x3710, 0x30f6},
    {0x3712, 0xfed9},
    {0x3714, 0xca92},
    {0x3716, 0xcb13},
    {0x3718, 0x39b7},
    {0x371a, 0x5a96},
    {0x371c, 0x8efa},
    {0x371e, 0xbd72},
    {0x3720, 0xaef3},
    {0x3722, 0x25f7},
    {0x3724, 0x5e36},
    {0x3726, 0xfe19},
    {0x3782, 0x03d2},
    {0x3784, 0x0240},
    {0x37C0, 0x8000},
    {0x37C2, 0x8000},
    {0x37C6, 0x8000},
    {0x37C4, 0x8000},
    {0x3780, 0x8000},
};
#define NUM_OF_LSC_CWF_REGS (sizeof(sensor_lsc_cwf_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_lsc_a_regs[] = {
    {0x3780, 0x0000},
        
    {0x3600, 0x0130},
    {0x3602, 0x2889},
    {0x3604, 0x0c10},
    {0x3606, 0xbeed},
    {0x3608, 0x7f2e},
    {0x360a, 0x00f0},
    {0x360c, 0x5e86},
    {0x360e, 0x1290},
    {0x3610, 0xe20d},
    {0x3612, 0x188f},
    {0x3614, 0x00d0},
    {0x3616, 0x222a},
    {0x3618, 0x7b8f},
    {0x361a, 0xd3cd},
    {0x361c, 0x7cee},
    {0x361e, 0x00b0},
    {0x3620, 0x66a7},
    {0x3622, 0x0d10},
    {0x3624, 0xccad},
    {0x3626, 0x400f},
    {0x3640, 0x3c65},
    {0x3642, 0xb82a},
    {0x3644, 0x212c},
    {0x3646, 0xb10d},
    {0x3648, 0x7dae},
    {0x364a, 0x7069},
    {0x364c, 0x3167},
    {0x364e, 0x5789},
    {0x3650, 0xf90d},
    {0x3652, 0x3def},
    {0x3654, 0x8ae6},
    {0x3656, 0x2a4c},
    {0x3658, 0x08cd},
    {0x365a, 0x888c},
    {0x365c, 0x308c},
    {0x365e, 0xd4c9},
    {0x3660, 0x1bcb},
    {0x3662, 0x390d},
    {0x3664, 0x354a},
    {0x3666, 0x80af},
    {0x3680, 0x2ed0},
    {0x3682, 0x170c},
    {0x3684, 0x80f3},
    {0x3686, 0xcc10},
    {0x3688, 0x3c95},
    {0x368a, 0x2650},
    {0x368c, 0x02e8},
    {0x368e, 0xc7f2},
    {0x3690, 0xc730},
    {0x3692, 0x2175},
    {0x3694, 0x13d0},
    {0x3696, 0x148d},
    {0x3698, 0xdd92},
    {0x369a, 0xcf30},
    {0x369c, 0x2775},
    {0x369e, 0x24b0},
    {0x36a0, 0x02ad},
    {0x36a2, 0xef52},
    {0x36a4, 0x85b1},
    {0x36a6, 0x36f5},
    {0x36c0, 0x19ed},
    {0x36c2, 0xda0e},
    {0x36c4, 0x0d52},
    {0x36c6, 0x62b1},
    {0x36c8, 0xcd74},
    {0x36ca, 0x060e},
    {0x36cc, 0xc0af},
    {0x36ce, 0x6e31},
    {0x36d0, 0x2232},
    {0x36d2, 0xbfb4},
    {0x36d4, 0x4f0d},
    {0x36d6, 0xa990},
    {0x36d8, 0x4f52},
    {0x36da, 0x1c93},
    {0x36dc, 0x9975},
    {0x36de, 0x492d},
    {0x36e0, 0x9d8e},
    {0x36e2, 0x60d1},
    {0x36e4, 0x45d1},
    {0x36e6, 0xbed4},
    {0x3700, 0xf4f1},
    {0x3702, 0x9493},
    {0x3704, 0x39d7},
    {0x3706, 0x3db6},
    {0x3708, 0x877a},
    {0x370a, 0x90f1},
    {0x370c, 0x8fb3},
    {0x370e, 0x2077},
    {0x3710, 0x3db6},
    {0x3712, 0xf519},
    {0x3714, 0x9572},
    {0x3716, 0xc993},
    {0x3718, 0x3e37},
    {0x371a, 0x6496},
    {0x371c, 0x905a},
    {0x371e, 0xf111},
    {0x3720, 0xa5d3},
    {0x3722, 0x34b7},
    {0x3724, 0x5416},
    {0x3726, 0x881a},
    {0x3782, 0x03d4},
    {0x3784, 0x023c},
    {0x37C0, 0x8000},
    {0x37C2, 0x8000},
    {0x37C6, 0x8000},
    {0x37C4, 0x8000},
    {0x3780, 0x8000},
};
#define NUM_OF_LSC_A_REGS (sizeof(sensor_lsc_a_regs) / sizeof(sensor_reg_t))
#endif

typedef struct gain_set {
    u8  a_gain;
    u8  dcg_gain;
    u16 dig_gain;
    u16 gain_x; // UFIX 7.6, normalization by 101
} gain_set_t;

static gain_set_t gain_table[] =
{
    {0x0B, 0, 133,   64},{0x0C, 0, 133,   67},{0x0D, 0, 133,   71},{0x0E, 0, 133,   75},
    {0x0F, 0, 133,   79},{0x10, 0, 133,   84},{0x12, 0, 133,   90},{0x14, 0, 133,   96},
    {0x16, 0, 133,  104},{0x18, 0, 133,  112},{0x00, 1, 133,  114},{0x01, 1, 133,  117},
    {0x02, 1, 133,  121},{0x03, 1, 133,  125},{0x04, 1, 133,  130},{0x05, 1, 133,  135},
    {0x06, 1, 133,  140},{0x07, 1, 133,  146},{0x08, 1, 133,  152},{0x09, 1, 133,  158},
    {0x0A, 1, 133,  165},{0x0B, 1, 133,  173},{0x0C, 1, 133,  182},{0x0D, 1, 133,  192},
    {0x0E, 1, 133,  202},{0x0F, 1, 133,  214},{0x10, 1, 133,  227},{0x12, 1, 133,  243},
    {0x14, 1, 133,  260},{0x16, 1, 133,  280},{0x18, 1, 133,  303},{0x1A, 1, 133,  331},
    {0x1C, 1, 133,  364},{0x1E, 1, 133,  404},{0x20, 1, 133,  455},{0x22, 1, 133,  485},
    {0x24, 1, 133,  520},{0x26, 1, 133,  560},{0x28, 1, 133,  606},{0x2A, 1, 133,  661},
    {0x2C, 1, 133,  728},{0x2E, 1, 133,  808},{0x30, 1, 133,  909},{0x32, 1, 133,  971},
    {0x34, 1, 133, 1040},{0x36, 1, 133, 1120},{0x38, 1, 133, 1212},{0x3A, 1, 133, 1323},
    {0x3B, 1, 133, 1364},{0x3B, 1, 140, 1436},{0x3B, 1, 150, 1539},{0x3B, 1, 160, 1641},
    {0x3B, 1, 170, 1744},{0x3B, 1, 180, 1846},{0x3B, 1, 190, 1949},{0x3B, 1, 200, 2051},
    {0x3B, 1, 210, 2154},{0x3B, 1, 220, 2257},{0x3B, 1, 230, 2359},{0x3B, 1, 240, 2462},
    {0x3B, 1, 250, 2564},{0x3B, 1, 260, 2667},{0x3B, 1, 270, 2769},{0x3B, 1, 280, 2872},
    {0x3B, 1, 290, 2975},{0x3B, 1, 300, 3077},{0x3B, 1, 310, 3180},{0x3B, 1, 320, 3282},
    {0x3B, 1, 330, 3385},{0x3B, 1, 340, 3487},{0x3B, 1, 350, 3590},{0x3B, 1, 360, 3693},
    {0x3B, 1, 370, 3795},{0x3B, 1, 380, 3898},{0x3B, 1, 390, 4000},{0x3B, 1, 400, 4103},
    {0x3B, 1, 410, 4205},{0x3B, 1, 420, 4308},{0x3B, 1, 430, 4411},{0x3B, 1, 440, 4513},
    {0x3B, 1, 450, 4616},{0x3B, 1, 460, 4718},{0x3B, 1, 470, 4821},{0x3B, 1, 480, 4923},
    {0x3B, 1, 490, 5026},{0x3B, 1, 500, 5129},{0x3B, 1, 510, 5231},
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

static int init(void);
//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x20 >> 1)
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
    msgs[1].len   = 2;
    msgs[1].buf   = tmp2;

    if (sensor_i2c_transfer(msgs, 2))
        return -1;

    *pval = (tmp2[0] << 8) | tmp2[1];
    return 0;
}

static int write_reg(u32 addr, u32 val)
{
    struct i2c_msg  msgs;
    unsigned char   buf[4];

    buf[0]     = (addr >> 8) & 0xFF;
    buf[1]     = addr & 0xFF;
    buf[2]     = (val >> 8) & 0xFF;
    buf[3]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 4;
    msgs.buf   = buf;

//    printk("write_reg %x %x\n",addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    u32 vt_pix_clk_div, vt_sys_clk_div, pre_pll_clk_div, pll_multiplier, digital_test;
    u32 pclk;

    read_reg(0x302a, &vt_pix_clk_div);
    read_reg(0x302c, &vt_sys_clk_div);
    read_reg(0x302e, &pre_pll_clk_div);
    read_reg(0x3030, &pll_multiplier);
    read_reg(0x30B0, &digital_test);     //digital_test[14] , bypass all

    if(digital_test & BIT14){
        pclk = xclk;
    }else{
        pclk = xclk * pll_multiplier / vt_pix_clk_div / vt_sys_clk_div / pre_pll_clk_div;
    }

    printk("pclk(%d) XCLK(%d)\n", pclk, xclk);
    return pclk;
}

static void adjust_vblk(int fps)
{
    int Vblk; //V blanking lines, H blanking
    int img_w, img_h;
    u32 x_start, x_end, y_start, y_end;
    int line_length_pck, frame_h;;

    if (fps > 30)
        fps = 30;
    read_reg(0x3008, &x_end);
    read_reg(0x3004, &x_start);
    read_reg(0x3006, &y_end);
    read_reg(0x3002, &y_start);
    read_reg(0x300C, &line_length_pck);

    img_w = x_end - x_start + 1;
    img_h = y_end - y_start + 1;
    line_length_pck <<= 1;

    Vblk = ((g_psensor->pclk / fps)/ line_length_pck - img_h);

    if(Vblk < 16)  //min. V blanking
        Vblk = 16;

    frame_h = Vblk+img_h;

    write_reg(0x300A, (u32)frame_h);
    g_psensor->fps = fps;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 line_length_pck;

    g_psensor->pclk = get_pclk(g_psensor->xclk);

    read_reg(0X300C, &line_length_pck);
    // unit: 1/10us; 1/10 us =x, x= 10*us
    pinfo->t_row = (line_length_pck * 2) * 10000 / (g_psensor->pclk / 1000);
}

static int set_size(u32 width, u32 height)
{
    int ret = 0;
    u32 x_start=0, x_end=0, y_start=0, y_end=0;
    int sen_width, sen_height;
    u32 RegReadMode;
    // check size
    if ((width * (1 + binning) > SENSOR_MAX_WIDTH) || (height * (1 + binning) > SENSOR_MAX_HEIGHT)) {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }

    if(binning){
        sen_width = width * 2;
        sen_height = height * 2;
        x_start = (0 + (SENSOR_MAX_WIDTH - sen_width)/2) & ~BIT0;
        y_start = (2 + (SENSOR_MAX_HEIGHT - sen_height)/2) & ~BIT0;
        x_end = (sen_width - 1 + x_start + 2) | BIT0;
        y_end = (sen_height - 1 + y_start + 2) | BIT0;
        //Set skip
        write_reg(0X30A2, 0x3);
        write_reg(0X30A6, 0x3);
        //Set binning average
        read_reg(0x3040, &RegReadMode);
        RegReadMode |= (BIT12 | BIT13);
        write_reg(0X3040, RegReadMode);
    }else{
        x_start = ((SENSOR_MAX_WIDTH - width)/2) & ~BIT0;
        y_start = ((SENSOR_MAX_HEIGHT - height)/2) & ~BIT0;
        x_end = (width - 1 + x_start);
        y_end = (height- 1 + y_start);
    }
    write_reg(0X3006, y_end);
    write_reg(0x3008, x_end);
    write_reg(0X3002, y_start);
    write_reg(0X3004, x_start);

    calculate_t_row();

    // update sensor information
    g_psensor->out_win.x = x_start;
    g_psensor->out_win.y = y_start;

    g_psensor->out_win.width = (x_end - x_start + 1) / (1 + binning);
    g_psensor->out_win.height = (y_end - y_start + 1) / (1 + binning);

    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 2;
    g_psensor->img_win.width = width ;
    g_psensor->img_win.height = height;

    return ret;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 coarse_integration_time;

    if (!g_psensor->curr_exp) {
        read_reg(0x3012, &coarse_integration_time);
        g_psensor->curr_exp = coarse_integration_time * pinfo->t_row / 1000;
    }

    return g_psensor->curr_exp;

}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 coarse_integration_time;

    coarse_integration_time = MAX(1, exp * 1000 / pinfo->t_row);

    write_reg(0x3012, coarse_integration_time);

    g_psensor->curr_exp = MAX(1, coarse_integration_time * pinfo->t_row / 1000);

    return ret;

}

static u32 get_gain(void)
{
    int a_fine_gain[16]={0, 3, 7, 10, 14, 19, 23, 28, 33, 39, 46, 52, 60, 68, 78, 88};
    u32 coarse_gain, fine_gain, dig_gain, dcg_gain;
    u32 reg_ang;

    if (g_psensor->curr_gain==0) {
        read_reg(0x3100, &dcg_gain);
        read_reg(0x3060, &reg_ang);
        read_reg(0x305E, &dig_gain);

        coarse_gain = 1<<((reg_ang>>4) & 0x03);

        fine_gain = a_fine_gain[reg_ang& 0xF];

        dig_gain = ((dig_gain >> 7) & 0x0f)* 128 + (dig_gain & 0x7f);

        if (dcg_gain)
           dcg_gain = 27;
        else
           dcg_gain = 10;

        //fine_gain=16x, dig_gain=128x, dig_gain=2x
        g_psensor->curr_gain = ((coarse_gain * (1*100 + fine_gain) * dig_gain * dcg_gain) << GAIN_SHIFT) / 100 / 128 / 10;
        g_psensor->curr_gain = g_psensor->curr_gain * 64 / 101; //normalization
    }

    return g_psensor->curr_gain;
}

#ifdef SENSOR_LSC
static int set_sensor_lsc(void)
{
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;
    int i, temp_index;
    static int pre_temp_index=-1;

    // get current rb_ratio
    //isp_api_get_temperature_index();
    temp_index = 0;
    
    if (pre_temp_index != temp_index){
        if (temp_index == 0){
            pInitTable = sensor_lsc_d65_regs;
            InitTable_Num = NUM_OF_LSC_D65_REGS;
        }
        else if (temp_index == 1){
            pInitTable = sensor_lsc_cwf_regs;
            InitTable_Num = NUM_OF_LSC_CWF_REGS;
        }
        else{
            pInitTable = sensor_lsc_a_regs;
            InitTable_Num = NUM_OF_LSC_A_REGS;
        }
            
        for (i=0; i<InitTable_Num; i++) {
           if(pInitTable[i].addr == DELAY_CODE){
                mdelay(pInitTable[i].val);
            }
            else if(write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
                isp_err("%s set sensor lsc fail!!\n", SENSOR_NAME);
                return -EINVAL;
            }
        }
        pre_temp_index = temp_index;
    }
    
    return 0;
}
#endif

static int set_gain(u32 gain)
{
    int ret = 0;

    u32 a_gain, dig_gain, dcg_gain;
    u32 gain_reg=0;
    int i;

    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    read_reg(0x3100, &dcg_gain);
    a_gain   = (gain_table[i-1].a_gain)&0x7F;
    dig_gain = (gain_table[i-1].dig_gain)&0x7FF;
    dcg_gain = (dcg_gain & 0xfffb) | ((gain_table[i-1].dcg_gain & 0x01) << 2);
    gain_reg = a_gain;

    write_reg(0x3060, gain_reg);
    write_reg(0x305E, dig_gain);
    if (gain_table[i-1].dcg_gain){
        if (wdr){
            write_reg(0x3096, 0x0780);
            write_reg(0x3098, 0x0780);
            write_reg(0x3200, 0x0002);
            write_reg(0x3202, 0x00B0);
            write_reg(0x3206, 0x1C0E);
            write_reg(0x3208, 0x4E39);
        }
        else{
            write_reg(0x3096, 0x0080);
            write_reg(0x3098, 0x0080);
            write_reg(0x3200, 0x0000);
        }
        write_reg(0x3010, 0x0004);
    }
    else{
        if (wdr){
            write_reg(0x3096, 0x0480);
            write_reg(0x3098, 0x0480);
            write_reg(0x3200, 0x0002);
            write_reg(0x3202, 0x0080);
            write_reg(0x3206, 0x0B08);
            write_reg(0x3208, 0x1E13);
        }
        else{
            write_reg(0x3096, 0x0080);
            write_reg(0x3098, 0x0080);
            write_reg(0x3200, 0x0000);
        }
        write_reg(0x3010, 0x0000);
    }
    write_reg(0x3100, dcg_gain);

#ifdef SENSOR_LSC
//    set_sensor_lsc();
#endif

    g_psensor->curr_gain = gain_table[i-1].gain_x;

    return ret;
}

static int get_mirror(void)
{
    u32 reg;

    read_reg(0x301c, &reg);
    return (reg & BIT0) ? 1 : 0;
}

static int set_mirror(int enable)
{
    u32 reg;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    read_reg(0x301c, &reg);
    if (enable)
        reg |= BIT0;
    else
        reg &=~BIT0;
    write_reg(0x301c, reg);
    pinfo->mirror = enable;

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x301c, &reg);
    return (reg & BIT1) ? 1 : 0;

}

static int set_flip(int enable)
{
    u32 reg;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    read_reg(0x301c, &reg);
    if (enable)
        reg |= BIT1;
    else
        reg &=~BIT1;

    write_reg(0x301c, reg);
    pinfo->flip = enable;

    return 0;
}

static int cal_wdr_min_exp(void)
{
    u32 operation_mode_ctrl;
    int ratio, itemp=0,exp;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    read_reg(0x3082, &operation_mode_ctrl);
    //Get the T1/T2 ratio.
    itemp = (operation_mode_ctrl>>2)&(BIT0|BIT1);

    if (itemp==0)
        ratio=4;
    else if (itemp==1)
        ratio=8;
    else if (itemp==2)
        ratio=16;
    else
        ratio=32;

    exp= MAX(1, pinfo->t_row * (ratio/2) / 1000);

    return exp;
}

static int cal_wdr_max_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 frame_length_line;
    u32 operation_mode_ctrl;
    int lines, exp, ratio, itemp=0;

    read_reg(0x3082, &operation_mode_ctrl);
    read_reg(0x300A, &frame_length_line);

    //Get the T1/T2 ratio.
    itemp = (operation_mode_ctrl>>2)&(BIT0|BIT1);

    if (itemp==0)
        ratio=4;
    else if (itemp==1)
        ratio=8;
    else if (itemp==2)
        ratio=16;
    else
        ratio=32;

    lines = MIN( 70* ratio, frame_length_line - 71);
    exp = MAX(1, pinfo->t_row * lines / 1000);
    return exp;

}

static int set_wdr_en(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
//    u32 reg;
    int i;
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;
  
    if (enable){
        pInitTable=sensor_wdr_init_regs;
        InitTable_Num=NUM_OF_WDR_INIT_REGS;
#if 1
        for (i=0; i<InitTable_Num; i++) {
           if(pInitTable[i].addr == DELAY_CODE){
                mdelay(pInitTable[i].val);
            }
            else if(write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
                isp_err("%s init fail!!\n", SENSOR_NAME);
                return -EINVAL;
            }
        }
        adjust_vblk(g_psensor->fps);
#endif
#ifdef WDR_EXP_LIMIT
        g_psensor->min_exp = cal_wdr_min_exp();
#else
        g_psensor->min_exp = 2;
#endif
        g_psensor->max_exp = cal_wdr_max_exp(); // 0.5 sec

        g_psensor->fmt = RAW12_WDR16;
    }else{
        pInitTable=sensor_init_regs;
        InitTable_Num=NUM_OF_INIT_REGS;
        
        for (i=0; i<InitTable_Num; i++) {
           if(pInitTable[i].addr == DELAY_CODE){
                mdelay(pInitTable[i].val);
            }
            else if(write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
                isp_err("%s init fail!!\n", SENSOR_NAME);
                return -EINVAL;
            }
        }

        adjust_vblk(g_psensor->fps);

        g_psensor->min_exp = 1;
        g_psensor->max_exp = 5000; // 0.5 sec

        g_psensor->fmt = RAW12;

    }
    wdr = enable;
#ifdef SENSOR_LSC
    set_sensor_lsc();
#endif
    set_flip(pinfo->flip);
    set_mirror(pinfo->mirror);
    set_exp(g_psensor->curr_exp);
    set_gain(g_psensor->curr_gain);    

    return 0;
}

static int get_wdr_en(void)
{
    int enable = (g_psensor->fmt & WDR_MASK)?1:0;
    return enable;
}

static int set_reset(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    int ret = 0;

    isp_api_cap_rst_set_value(0);
    mdelay(50);
    isp_api_cap_rst_set_value(1);
    mdelay(50);

    pinfo->is_init = 0;
	
    ret = init();

    if (ret < 0)
        isp_err("sensor reset fail\n");
    else 
        isp_info("sensor reset ok\n");

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
    case ID_WDR_EN:
        *(int*)arg = get_wdr_en();
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
    case ID_WDR_EN:
        ret = set_wdr_en((int)arg);
        break;
    case ID_FPS:
        adjust_vblk((int)arg);
        break;
    case ID_RESET:
        set_reset();
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

    for (i=0; i<NUM_OF_INIT_REGS; i++) {
        if(sensor_init_regs[i].addr == DELAY_CODE){
            mdelay(sensor_init_regs[i].val);
        }
        else if(write_reg(sensor_init_regs[i].addr, sensor_init_regs[i].val) < 0) {
            isp_err("%s init fail!!\n", SENSOR_NAME);
            isp_err("%x %x\n", sensor_init_regs[i].addr, sensor_init_regs[i].val);
            return -EINVAL;
        }
    }

    g_psensor->pclk = get_pclk(g_psensor->xclk);

    // get mirror and flip status
    pinfo->mirror = get_mirror();
    pinfo->flip = get_flip();

    set_flip(flip);
    set_mirror(mirror);

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);
    if (ret == 0)
        pinfo->is_init = 1;

    //Must be after set_size()
    adjust_vblk(g_psensor->fps);

    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current shutter_width and gain setting
    get_exp();
    get_gain();

    if (wdr)
        isp_info("WDR mode on\n");
    else
        isp_info("WDR mode off\n");

    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void ar0230_deconstruct(void)
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

static int ar0230_construct(u32 xclk, u16 width, u16 height)
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
        isp_err("failed to allocate private data!\n");
        return -ENOMEM;
    }

    g_psensor->private = pinfo;
    snprintf(g_psensor->name, DEV_MAX_NAME_SIZE, SENSOR_NAME);
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->interface = IF_PARALLEL;
    g_psensor->num_of_lane = 0;
    g_psensor->protocol = 0;
    g_psensor->fmt = RAW12;
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

    //Call back function
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
    ar0230_deconstruct();
    return ret;
}


//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init AR0230_init(void)
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

    if ((ret = ar0230_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit AR0230_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ar0230_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}


module_init(AR0230_init);
module_exit(AR0230_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor AR0230");
MODULE_LICENSE("GPL");

