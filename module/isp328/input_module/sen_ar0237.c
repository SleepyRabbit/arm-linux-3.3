/**
 * @file sen_ar0237.c
 * ar0237 sensor driver
 *
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 * 20160408 author RobertChuang
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_ar0237"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH      1920 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT     1080 //ISP Output size
#define DEFAULT_XCLK         27000000
#define DEFAULT_INTERFACE IF_PARALLEL

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
#define SENSOR_NAME         "AR0237"
#define SENSOR_MAX_WIDTH        1920
#define SENSOR_MAX_HEIGHT       1080
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        2
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF

//mipi setting
#define  DEFAULT_1080P_DVP_MODE     MODE_DVP_1080P
#define  DEFAULT_1080P_DVP_VTS                1125
#define  DEFAULT_1080P_DVP_HTS                1100
#define  DEFAULT_1080P_DVP_WIDTH              1920
#define  DEFAULT_1080P_DVP_HEIGHT             1080
#define  DEFAULT_1080P_DVP_MIRROR                0
#define  DEFAULT_1080P_DVP_FLIP                  0
#define  DEFAULT_1080P_DVP_TROW                296
#define  DEFAULT_1080P_DVP_PCLK           74250000
#define  DEFAULT_1080P_DVP_MAXFPS               30
#define  DEFAULT_1080P_DVP_LANE                  2 //don't care
#define  DEFAULT_1080P_DVP_RAWBIT            RAW12
#define  DEFAULT_1080P_DVP_BAYER          BAYER_GR
#define  DEFAULT_1080P_DVP_INTERFACE   IF_PARALLEL

static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_DVP_1080P = 0,  
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
    enum SENSOR_IF interface;	
} sensor_info_t;


static sensor_info_t g_sAR0237info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_1080P_DVP_MODE,        
    DEFAULT_1080P_DVP_VTS,
    DEFAULT_1080P_DVP_HTS,
    DEFAULT_1080P_DVP_WIDTH,
    DEFAULT_1080P_DVP_HEIGHT,
    DEFAULT_1080P_DVP_MIRROR,
    DEFAULT_1080P_DVP_FLIP,
    DEFAULT_1080P_DVP_TROW,
    DEFAULT_1080P_DVP_PCLK,
    DEFAULT_1080P_DVP_MAXFPS,
    DEFAULT_1080P_DVP_LANE,
    DEFAULT_1080P_DVP_RAWBIT,
    DEFAULT_1080P_DVP_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_1080P_DVP_INTERFACE,    
    },  
};

typedef struct sensor_reg {
    u16 addr;  
    u16 val;
} sensor_reg_t;

static sensor_reg_t sensor_1080p_dvp_init_regs[] = {
    {0x301A, 0x0001},
    {DELAY_CODE, 200},
    {0x3088, 0x8000},
    {0x3086, 0x4558},
    {0x3086, 0x72A6},
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
    {0x3086, 0xA403},
    {0x3086, 0x430D},
    {0x3086, 0x2D46},
    {0x3086, 0x4316},
    {0x3086, 0x2A90},
    {0x3086, 0x3E06},
    {0x3086, 0x2A98},
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
    {0x3086, 0x902B},
    {0x3086, 0x803E},
    {0x3086, 0x062A}, 
    {0x3086, 0xF23F},
    {0x3086, 0x103E},
    {0x3086, 0x0160}, 
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
    {0x3086, 0x3F36},
    {0x3086, 0x2AF2},
    {0x3086, 0x3F10},
    {0x3086, 0x3E01},
    {0x3086, 0x6029},
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
    {0x3086, 0x76A7},
    {0x3086, 0x77A7},
    {0x3086, 0x4644},
    {0x3086, 0x1616},
    {0x3086, 0xA57A},
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
    {0x3086, 0x4BB9},
    {0x3086, 0x2C2C},
    {0x3086, 0x2C2C},
    {0x3064, 0x1802},
    {0x3EEE, 0xA0AA},
    {0x30BA, 0x762C},
    {0x3F4A, 0x0F70},
    {0x309E, 0x016C},
    {0x3092, 0x006F},
    {0x3EE4, 0x9937},
    {0x3EE6, 0x3863},
    {0x3EEC, 0x3B0C},
    {0x3EEA, 0x2839},
    {0x3ECC, 0x4E2D},
    {0x3ED2, 0xFEA6},
    {0x3ED6, 0x2CB3},
    {0x3EEA, 0x2819},
    {0x31AE, 0x0301},
    {0x31C6, 0x0006},
    {0x306E, 0x9018},//0x2418 for PCLK and Data Voltage
    {0x301A, 0x10D8},
    {0x30B0, 0x0118},
    {0x31AC, 0x0C0C},
    {0x318E, 0x0000},
    {0x3082, 0x0009},
    {0x30BA, 0x762C},
    {0x31D0, 0x0000},
    {0x30B4, 0x0091},
    //PLL-3
    {0x302A, 0x0008},
    {0x302C, 0x0001},
    {0x302E, 0x0002},    
    {0x3030, 0x002C},
    {0x3036, 0x000C},
    {0x3038, 0x0001},
    {0x3004, 0x000C},
    {0x3008, 0x078B},
    {0x3002, 0x0004},
    {0x3006, 0x043B},
    {0x30A2, 0x0001},
    {0x30A6, 0x0001},    
    {0x3040, 0x0000},
    {0x300A, 0x0465},
    {0x300C, 0x044C},  
    {0x301A, 0x10DC},                       
};
#define NUM_OF_1080P_DVP_INIT_REGS (sizeof(sensor_1080p_dvp_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u16  a_gain;
    u16  a_gain_offset;
    u16  d_gain;
    u16  total_gain;
} gain_set_t;

static gain_set_t gain_table[] =
{
    {0x000B, 0x0000, 0x0085,   64},{0x000D, 0x0000, 0x0085,   71},{0x000F, 0x0000, 0x0085,   79},{0x000F, 0x0000, 0x0094,   88},
    {0x000F, 0x0000, 0x00A1,   96},{0x000F, 0x0000, 0x00AF,  104},{0x0000, 0x0004, 0x0085,  114},{0x0002, 0x0004, 0x0085,  121},
    {0x0004, 0x0004, 0x0085,  130},{0x0007, 0x0004, 0x0085,  146},{0x0009, 0x0004, 0x0085,  158},{0x000B, 0x0004, 0x0085,  173},
    {0x000D, 0x0004, 0x0085,  191},{0x000F, 0x0004, 0x0085,  214},{0x0010, 0x0004, 0x0085,  227},{0x0012, 0x0004, 0x0085,  243},
    {0x0014, 0x0004, 0x0085,  260},{0x0017, 0x0004, 0x0085,  291},{0x0019, 0x0004, 0x0085,  316},{0x001B, 0x0004, 0x0085,  346},
    {0x001D, 0x0004, 0x0085,  382},{0x001E, 0x0004, 0x0085,  405},{0x0020, 0x0004, 0x0085,  455},{0x0022, 0x0004, 0x0085,  485},
    {0x0024, 0x0004, 0x0085,  520},{0x0027, 0x0004, 0x0085,  582},{0x0029, 0x0004, 0x0085,  633},{0x002B, 0x0004, 0x0085,  691},
    {0x002E, 0x0004, 0x0085,  809},{0x0030, 0x0004, 0x0085,  909},{0x0032, 0x0004, 0x0085,  970},{0x0034, 0x0004, 0x0085, 1039},
    {0x0037, 0x0004, 0x0085, 1164},{0x0039, 0x0004, 0x0085, 1265},{0x003B, 0x0004, 0x0085, 1382},{0x003D, 0x0004, 0x0085, 1528},
    {0x003E, 0x0004, 0x0085, 1619},{0x003F, 0x0004, 0x008B, 1792},{0x0040, 0x0004, 0x008C, 1920},{0x0040, 0x0004, 0x0096, 2048},  
    {0x0040, 0x0004, 0x00A8, 2304},{0x0040, 0x0004, 0x00BB, 2560},{0x0040, 0x0004, 0x00CE, 2816},{0x0040, 0x0004, 0x00E1, 3072},                
    {0x0040, 0x0004, 0x00F3, 3328},{0x0040, 0x0004, 0x0106, 3584},{0x0040, 0x0004, 0x0119, 3840},{0x0040, 0x0004, 0x012C, 4096},           
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x20 >> 1
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    u8 tmp[2],tmp2[2];
       
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
    u8   tmp[4];

    tmp[0]     = (addr >> 8) & 0xFF;
    tmp[1]     = addr & 0xFF;
    tmp[2]     = (val >> 8) & 0xFF;
    tmp[3]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 4;
    msgs.buf   = tmp;

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk= g_sAR0237info[pinfo->currentmode].pclk;
	
    //isp_info("ar0237 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sAR0237info[pinfo->currentmode].hts;
    pinfo->t_row=g_sAR0237info[pinfo->currentmode].t_row;

    //isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 exp_line;
	
    if (!g_psensor->curr_exp) {
        read_reg(0x3012, &exp_line);
        g_psensor->curr_exp = (exp_line * (pinfo->t_row) + 500) / 1000;
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line;

    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1>exp_line)
        exp_line = 1;

    if (exp_line>(SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE))
        exp_line=SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE;

    if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) {
        write_reg(0x300A, (exp_line+SENSOR_NON_EXP_LINE) & 0xFFFF);    
        pinfo->long_exp = 1;
    } else {
        if (pinfo->long_exp) {
            write_reg(0x300A, (pinfo->vts) & 0xFFFF);        
            pinfo->long_exp = 0;
        }
    }
    
    write_reg(0x3012, (exp_line & 0xFFFF));
    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;

}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;

    max_fps = g_sAR0237info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sAR0237info[pinfo->currentmode].vts) * max_fps / fps;

    if ((pinfo->vts) > SENSOR_MAX_VTS)
        pinfo->vts=	SENSOR_MAX_VTS;
		
    //isp_info("adjust_blk fps=%d, vts=%d\n", fps, pinfo->vts);

    write_reg(0x300A, (pinfo->vts) & 0xFFFF);

    get_exp();
    set_exp(g_psensor->curr_exp);
	
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    u32 a_gain,a_gain_offset,d_gain;
    int i;
    if (!g_psensor->curr_gain) {
        read_reg(0x3060, &a_gain);
        read_reg(0x3100, &a_gain_offset);
        read_reg(0x305E, &d_gain); 		
        a_gain_offset=(a_gain_offset>>2) & 0x1;

		if(1!=a_gain_offset) {
            for (i=0; i<NUM_OF_GAINSET; i++) {
                if (gain_table[i].a_gain > a_gain)
                break;
            }
		} else {
            for (i=7; i<NUM_OF_GAINSET; i++) {
                if ((gain_table[i].a_gain > a_gain)||(gain_table[i].d_gain > d_gain))
                break;
            }		    
        }
        g_psensor->curr_gain = gain_table[i-1].total_gain;
        //isp_info("g_psensor->curr_gain=%d\n", g_psensor->curr_gain); 
    }
	return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    u16 a_gain,a_gain_offset,d_gain;
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

    a_gain = gain_table[i-1].a_gain;
    a_gain_offset = gain_table[i-1].a_gain_offset;
    d_gain = gain_table[i-1].d_gain;
    write_reg(0x3060, a_gain);
    write_reg(0x3100, a_gain_offset);    
    write_reg(0x305E, d_gain);	
    g_psensor->curr_gain = gain_table[i-1].total_gain;
    //isp_info("g_psensor->curr_gain=%d\n", g_psensor->curr_gain);

    return 0;
}

static int get_mirror(void)
{
    u32 reg;
    read_reg(0x3040, &reg);
    return (reg & BIT14) ? 1 : 0;    
}

static int set_mirror(int enable)
{

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    read_reg(0x3040, &reg);

    if(pinfo->flip)
        reg |=BIT15;
    else
        reg &=~BIT15;
    
    if (enable)
        reg |= BIT14;
    else
        reg &= ~BIT14;
    write_reg(0x3040, reg);

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x3040, &reg);
    return (reg & BIT15) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;
    
    pinfo->flip= enable;

    read_reg(0x3040, &reg);

    if (pinfo->mirror)
        reg |= BIT14;
    else
        reg &= ~BIT14;
    if (enable)
        reg |= BIT15;
    else
        reg &= ~BIT15;
    write_reg(0x3040, reg);

    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
	
    //isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT))
    {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }
		
    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sAR0237info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sAR0237info[pinfo->currentmode].img_h;	

    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;	
    g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;
	
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
    pInitTable=sensor_1080p_dvp_init_regs;
    InitTable_Num=NUM_OF_1080P_DVP_INIT_REGS;

    //isp_info("start initial...\n");
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
static void ar0237_deconstruct(void)
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

static int ar0237_construct(u32 xclk, u16 width, u16 height)
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
    pinfo->currentmode=MODE_DVP_1080P;    
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sAR0237info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sAR0237info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sAR0237info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sAR0237info[pinfo->currentmode].bayer;
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
    ar0237_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init ar0237_init(void)
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

    if ((ret = ar0237_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit ar0237_exit(void)
{
    isp_unregister_sensor(g_psensor);
    ar0237_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(ar0237_init);
module_exit(ar0237_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor ar0237");
MODULE_LICENSE("GPL");
