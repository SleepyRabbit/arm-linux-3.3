/**
 * @file sen_sc2035.c
 * sc2035 sensor driver
 *
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 * 20160421 author RobertChuang
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_sc2035"
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

static uint inv_clk = 1;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");
//=============================================================================
// global parameters
//=============================================================================
#define SENSOR_NAME         "SC2035"
#define SENSOR_MAX_WIDTH        1920
#define SENSOR_MAX_HEIGHT       1080
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        4
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF

//mipi setting
#define  DEFAULT_1080P_DVP_MODE     MODE_DVP_1080P
#define  DEFAULT_1080P_DVP_VTS                1125
#define  DEFAULT_1080P_DVP_HTS                1000
#define  DEFAULT_1080P_DVP_WIDTH              1920
#define  DEFAULT_1080P_DVP_HEIGHT             1080
#define  DEFAULT_1080P_DVP_MIRROR                0
#define  DEFAULT_1080P_DVP_FLIP                  0
#define  DEFAULT_1080P_DVP_TROW                296
#define  DEFAULT_1080P_DVP_PCLK           67500000
#define  DEFAULT_1080P_DVP_MAXFPS               30
#define  DEFAULT_1080P_DVP_LANE                  2 //don't care
#define  DEFAULT_1080P_DVP_RAWBIT            RAW12
#define  DEFAULT_1080P_DVP_BAYER          BAYER_BG
#define  DEFAULT_1080P_DVP_INTERFACE   IF_PARALLEL

#define Gain2X                  128
#define Gain4X                  256
#define Gain8X                  512
#define Gain16X                1024

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


static sensor_info_t g_sSC2035info[]=
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
//20160317 setting
    {0x3105, 0x02},//start up timing begin
    {0x0103, 0x01},// reset all registers
    {0x3105, 0x02},
    {0x0100, 0x00},//start up timing end
    {0x301E, 0xB0},// mode select
    {0x320c, 0x03},// hts=2000
    {0x320d, 0xe8},
    {0x3231, 0x24},// half hts  to 2000
    {0x320E, 0x04},
    {0x320F, 0x65},
    {0x3211, 0x08},//x start 20160113 20160120
    {0x3213, 0x10},//y start
    {0x3e03, 0x03},//AEC AGC 03 : close aec/agc 00: open aec/agc
    {0x3e01, 0x46},//exp time
    {0x3e08, 0x00},//gain 1x
    {0x3e09, 0x10},//10-1f,16step->1/16
    {0x3518, 0x03},
    {0x5025, 0x09},
    {0x3908, 0xc0},//BLC RNCincrease blc target for rnc
    {0x3907, 0x01},//12.14
    {0x3928, 0x01},//20160315
    {0x3416, 0x12},//20160113
    {0x3401, 0x1e},//12.11
    {0x3402, 0x0c},//12.11
    {0x3403, 0x70},//12.11
    {0x3e0f, 0x90},
    {0x3638, 0x84},//RAMP config  20160113B
    {0x3637, 0xbc},//1018 20160113 20160120
    {0x3639, 0x98},
    {0x3035, 0x01},//count clk
    {0x3034, 0xc2},//1111   20160120
    {0x3300, 0x12},//eq  20160315
    {0x3301, 0x08},//cmprst  20160120 20160307
    {0x3308, 0x30},// tx 1111 20160307
    {0x3306, 0x3a},// count down 1111 20160120
    {0x330a, 0x00},
    {0x330b, 0x90},// count up
    {0x3303, 0x30},//ramp gap 20160307
    {0x3309, 0x30},//cnt up gap 20160307
    {0x331E, 0x2c},//integ 1st pos point 20160307
    {0x331F, 0x2c},//integ 2nd pos point 20160307
    {0x3320, 0x2e},//ofs fine 1st pos point 20160307
    {0x3321, 0x2e},//ofs fine 2nd pos point 20160307
    {0x3322, 0x2e},//20160307
    {0x3323, 0x2e},//20160307
    {0x3626, 0x03},//memory readout delay 0613 0926
    {0x3621, 0x28},//counter clock div [3] column fpn 0926
    {0x3F08, 0x04},//WRITE TIME
    {0x3F09, 0x44},//WRITE/READ TIME GAP
    {0x4500, 0x25},//data delay 0926
    {0x3c09, 0x08},// Sram start position
    {0x335D, 0x20},//prechg tx auto ctrl [5] 
    {0x3368, 0x02},//EXP1
    {0x3369, 0x00},
    {0x336A, 0x04},//EXP2
    {0x336b, 0x65},
    {0x330E, 0x50},// start value
    {0x3367, 0x08},// end value  12.14
    {0x3f00, 0x06},
    {0x3f04, 0x01},// sram write
    {0x3f05, 0xdf},// 1111  20160113 20160120 20160307
    {0x3905, 0x1c},
    {0x5780, 0x7f},//DPC
    {0x5781, 0x0a},//12.17 20160307
    {0x5782, 0x0a},//12.17 20160307
    {0x5783, 0x08},//12.17  20160307  20160317
    {0x5784, 0x08},//12.17  20160307  20160317
    {0x5785, 0x18},//12.11 ; 20160112 20160307
    {0x5786, 0x18},//12.11 ; 20160112 20160307
    {0x5787, 0x18},//12.11 20160307 20160317
    {0x5788, 0x18},// 20160307  20160317
    {0x5789, 0x01},//12.11 
    {0x578a, 0x0f},//12.11 
    {0x5000, 0x06},          
    {0x3632, 0x44},//bypass NVDD analog config  20160113
    {0x3622, 0x0e},//enable sa1/ecl blksun  
    {0x3627, 0x08},//0921 20160307
    {0x3630, 0xb4},//94 1111  20160113  20160120
    {0x3633, 0x97},//vrhlp voltage 14~24 could be choosed 20160113
    {0x3620, 0x62},//comp and bitline current 20160307
    {0x363a, 0x0c},//sa1 common voltage
    {0x3333, 0x10},// 20160307
    {0x3334, 0x20},//column fpn 20160307
    {0x3312, 0x06},//20160307
    {0x3340, 0x03},//20160307
    {0x3341, 0xb0},//20160307
    {0x3342, 0x02},//20160307
    {0x3343, 0x20},//20160307
    {0x303f, 0x81},//format
    {0x501f, 0x00},
    {0x3b00, 0xf8},
    {0x3b01, 0x40},
    {0x3c01, 0x14},
    {0x4000, 0x00},
    {0x3d08, 0x00},//data output DVP CLK INV
    {0x3640, 0x00},// pad driver
    {0x0100, 0x01},
    {0x303a, 0x07},// PLL  67.5M pclk
    {0x3039, 0x8e},
    {0x303f, 0x82},
    {0x3636, 0x88},//lpDVDD
    {0x3631, 0x80},//0820  20160113  20160120
    {0x3635, 0x66},//1018  20160113  20160120
    {0x3105, 0x04},//1018
    {0x3105, 0x04},//1018                   
};
#define NUM_OF_1080P_DVP_INIT_REGS (sizeof(sensor_1080p_dvp_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    unsigned char DCG; 
    //unsigned char analog;
    int gain_x;
} gain_set_t;

static gain_set_t gain_table[] =
{    
    {0x10, 64 },
    {0x11, 68 },
    {0x12, 72 },
    {0x13, 76 },
    {0x14, 80 },
    {0x15, 84 },
    {0x16, 88 },
    {0x17, 92 },
    {0x18, 96 },
    {0x19, 100},
    {0x1A, 104},
    {0x1B, 108},
    {0x1C, 112},
    {0x1D, 116},
    {0x1E, 120},
    {0x1F, 124},
    {0x20, 128},
    {0x21, 132},
    {0x22, 136},
    {0x23, 140},
    {0x24, 144},
    {0x25, 148},
    {0x26, 152},
    {0x27, 156},
    {0x28, 160},
    {0x29, 164},
    {0x2A, 168},
    {0x2B, 172},
    {0x2C, 176},
    {0x2D, 180},
    {0x2E, 184},
    {0x2F, 188},
    {0x30, 192},
    {0x31, 196},
    {0x32, 200},
    {0x33, 204},
    {0x34, 208},
    {0x35, 212},
    {0x36, 216},
    {0x37, 220},
    {0x38, 224},
    {0x39, 228},
    {0x3A, 232},
    {0x3B, 236},
    {0x3C, 240},
    {0x3D, 244},
    {0x3E, 248},
    {0x3F, 252},
    {0x40, 256},
    {0x41, 260},
    {0x42, 264},
    {0x43, 268},
    {0x44, 272},
    {0x45, 276},
    {0x46, 280},
    {0x47, 284},
    {0x48, 288},
    {0x49, 292},
    {0x4A, 296},
    {0x4B, 300},
    {0x4C, 304},
    {0x4D, 308},
    {0x4E, 312},
    {0x4F, 316},
    {0x50, 320},
    {0x51, 324},
    {0x52, 328},
    {0x53, 332},
    {0x54, 336},
    {0x55, 340},
    {0x56, 344},
    {0x57, 348},
    {0x58, 352},
    {0x59, 356},
    {0x5A, 360},
    {0x5B, 364},
    {0x5C, 368},
    {0x5D, 372},
    {0x5E, 376},
    {0x5F, 380},
    {0x60, 384},
    {0x61, 388},
    {0x62, 392},
    {0x63, 396},
    {0x64, 400},
    {0x65, 404},
    {0x66, 408},
    {0x67, 412},
    {0x68, 416},
    {0x69, 420},
    {0x6A, 424},
    {0x6B, 428},
    {0x6C, 432},
    {0x6D, 436},
    {0x6E, 440},
    {0x6F, 444},
    {0x70, 448},
    {0x71, 452},
    {0x72, 456},
    {0x73, 460},
    {0x74, 464},
    {0x75, 468},
    {0x76, 472},
    {0x77, 476},
    {0x78, 480},
    {0x79, 484},
    {0x7A, 488},
    {0x7B, 492},
    {0x7C, 496},
    {0x7D, 500},
    {0x7E, 504},
    {0x7F, 508},
    {0x80, 512},
    {0x81, 516},
    {0x82, 520},
    {0x83, 524},
    {0x84, 528},
    {0x85, 532},
    {0x86, 536},
    {0x87, 540},
    {0x88, 544},
    {0x89, 548},
    {0x8A, 552},
    {0x8B, 556},
    {0x8C, 560},
    {0x8D, 564},
    {0x8E, 568},
    {0x8F, 572},
    {0x90, 576},
    {0x91, 580},
    {0x92, 584},
    {0x93, 588},
    {0x94, 592},
    {0x95, 596},
    {0x96, 600},
    {0x97, 604},
    {0x98, 608},
    {0x99, 612},
    {0x9A, 616},
    {0x9B, 620},
    {0x9C, 624},
    {0x9D, 628},
    {0x9E, 632},
    {0x9F, 636},
    {0xA0, 640},
    {0xA1, 644},
    {0xA2, 648},
    {0xA3, 652},
    {0xA4, 656},
    {0xA5, 660},
    {0xA6, 664},
    {0xA7, 668},
    {0xA8, 672},
    {0xA9, 676},
    {0xAA, 680},
    {0xAB, 684},
    {0xAC, 688},
    {0xAD, 692},
    {0xAE, 696},
    {0xAF, 700},
    {0xB0, 704},
    {0xB1, 708},
    {0xB2, 712},
    {0xB3, 716},
    {0xB4, 720},
    {0xB5, 724},
    {0xB6, 728},
    {0xB7, 732},
    {0xB8, 736},
    {0xB9, 740},
    {0xBA, 744},
    {0xBB, 748},
    {0xBC, 752},
    {0xBD, 756},
    {0xBE, 760},
    {0xBF, 764},
    {0xC0, 768},
    {0xC1, 772},
    {0xC2, 776},
    {0xC3, 780},
    {0xC4, 784},
    {0xC5, 788},
    {0xC6, 792},
    {0xC7, 796},
    {0xC8, 800},
    {0xC9, 804},
    {0xCA, 808},
    {0xCB, 812},
    {0xCC, 816},
    {0xCD, 820},
    {0xCE, 824},
    {0xCF, 828},
    {0xD0, 832},
    {0xD1, 836},
    {0xD2, 840},
    {0xD3, 844},
    {0xD4, 848},
    {0xD5, 852},
    {0xD6, 856},
    {0xD7, 860},
    {0xD8, 864},
    {0xD9, 868},
    {0xDA, 872},
    {0xDB, 876},
    {0xDC, 880},
    {0xDD, 884},
    {0xDE, 888},
    {0xDF, 892},
    {0xE0, 896},
    {0xE1, 900},
    {0xE2, 904},
    {0xE3, 908},
    {0xE4, 912},
    {0xE5, 916},
    {0xE6, 920},
    {0xE7, 924},
    {0xE8, 928},
    {0xE9, 932},
    {0xEA, 936},
    {0xEB, 940},
    {0xEC, 944},
    {0xED, 948},
    {0xEE, 952},
    {0xEF, 956},
    {0xF0, 960},
    {0xF1, 964},
    {0xF2, 968},
    {0xF3, 972},
    {0xF4, 976},
    {0xF5, 980},
    {0xF6, 984},
    {0xF7, 988},
    {0xF8, 992},   
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))


//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x60 >> 1
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
    u8   tmp[4];

    tmp[0]     = (addr >> 8) & 0xFF;
    tmp[1]     = addr & 0xFF;
    tmp[2]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 3;
    msgs.buf   = tmp;

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk= g_sSC2035info[pinfo->currentmode].pclk;
	
    //isp_info("sc2035 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sSC2035info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC2035info[pinfo->currentmode].t_row;

    //isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 exp_line,exp_line_h,exp_line_m,exp_line_l;
	
    if (!g_psensor->curr_exp) {
        read_reg(0x3E00, &exp_line_h);	
        read_reg(0x3E01, &exp_line_m);
        read_reg(0x3E02, &exp_line_l);		
        exp_line=((exp_line_h & 0x0F)<<12)|((exp_line_m & 0xFF)<<4)|((exp_line_l & 0xF0)>>4);             
        g_psensor->curr_exp = (exp_line * (pinfo->t_row) + 500) / 1000;
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line,tmp_reg,tmp_reg1;

    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1>exp_line)
        exp_line = 1;

    if (exp_line>(SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE))
        exp_line=SENSOR_MAX_VTS-SENSOR_NON_EXP_LINE;

    if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) {
        
        tmp_reg=(exp_line+SENSOR_NON_EXP_LINE)-0x265;
        tmp_reg1=(pinfo->hts)-0x100;    	
        
        write_reg(0x320E, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
        write_reg(0x320F, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
        write_reg(0x336A, (exp_line+SENSOR_NON_EXP_LINE) >> 8);
        write_reg(0x336B, (exp_line+SENSOR_NON_EXP_LINE) & 0xff);
        write_reg(0x3368, (tmp_reg) >> 8);
        write_reg(0x3369, (tmp_reg) & 0xff);
        write_reg(0x3340, (tmp_reg1) >> 8);
        write_reg(0x3341, (tmp_reg1) & 0xff);            
        pinfo->long_exp = 1;
    } else {
        if (pinfo->long_exp) {
        	
            tmp_reg=(pinfo->vts)-0x265;
            tmp_reg1=(pinfo->hts)-0x100;

            write_reg(0x320E, (pinfo->vts >> 8) & 0xFF);        
            write_reg(0x320F, (pinfo->vts & 0xFF));
            write_reg(0x336A, (pinfo->vts) >> 8);
            write_reg(0x336B, (pinfo->vts) & 0xff);
            write_reg(0x3368, (tmp_reg) >> 8);
            write_reg(0x3369, (tmp_reg) & 0xff);
            write_reg(0x3340, (tmp_reg1) >> 8);
            write_reg(0x3341, (tmp_reg1) & 0xff);
            pinfo->long_exp = 0;
        }
    }
    write_reg(0x3E00, (exp_line >> 12) & 0x0F);    
    write_reg(0x3E01, (exp_line >> 4) & 0xFF);
    write_reg(0x3E02, (exp_line & 0x0F) <<4 );
    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;

}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps,tmp_reg,tmp_reg1;

    max_fps = g_sSC2035info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sSC2035info[pinfo->currentmode].vts) * max_fps / fps;

    if ((pinfo->vts) > SENSOR_MAX_VTS)
        pinfo->vts=	SENSOR_MAX_VTS;

    tmp_reg=(pinfo->vts)-0x265;
    tmp_reg1=(pinfo->hts)-0x100;    		

    write_reg(0x320E, (pinfo->vts) >> 8);
    write_reg(0x320F, (pinfo->vts) & 0xff);
    write_reg(0x336A, (pinfo->vts) >> 8);
    write_reg(0x336B, (pinfo->vts) & 0xff);
    write_reg(0x3368, (tmp_reg) >> 8);
    write_reg(0x3369, (tmp_reg) & 0xff);
    write_reg(0x3340, (tmp_reg1) >> 8);
    write_reg(0x3341, (tmp_reg1) & 0xff);
           
    get_exp();
    set_exp(g_psensor->curr_exp);
	
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    u32 fine_gain;

    if (!g_psensor->curr_gain) {
        read_reg(0x3e09, &fine_gain);
        g_psensor->curr_gain = (fine_gain / 16) * 64;        
    }    
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    int ret = 0;
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
    
    write_reg(0x3e09, gain_table[i-1].DCG);	           

    if (gain_table[i-1].gain_x < Gain2X) {	  	  	  	  
        write_reg(0x3630, 0xB4);
        write_reg(0x3635, 0x6C);	      	      	      	      
    } else {	  
        write_reg(0x3630, 0x94);
        if (gain_table[i-1].gain_x <= Gain4X) { 
            write_reg(0x3635, 0x6A);
        } else if (gain_table[i-1].gain_x <= Gain8X) {
            write_reg(0x3635, 0x68);	      
        } else if (gain_table[i-1].gain_x <= Gain16X) {
            write_reg(0x3635, 0x66);
        } else {
            write_reg(0x3635, 0x64);	      
        }
    } 
	  
    g_psensor->curr_gain = gain_table[i-1].gain_x;

    return ret;

}

static int get_mirror(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    return pinfo->mirror;

}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    isp_api_wait_frame_end();
    if (enable) {
        if (pinfo->flip) {	
            write_reg(0x3213, 0x09);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x00);       
            write_reg(0x4500, 0x25);      
            write_reg(0x5044, 0x00);
        } else {
            write_reg(0x3213, 0x08);        
            write_reg(0x3220, 0x00);       
            write_reg(0x3221, 0x00);       
            write_reg(0x4500, 0x25);      
            write_reg(0x5044, 0x00);
        }
    } else {
        if (pinfo->flip) {	
            write_reg(0x3213, 0x09);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x06);       
            write_reg(0x4500, 0x21);      
            write_reg(0x5044, 0x20); 
        } else {
            write_reg(0x3213, 0x08);
            write_reg(0x3220, 0x00);        
            write_reg(0x3221, 0x06);       
            write_reg(0x4500, 0x21);          
            write_reg(0x5044, 0x20);  
        }
    }
    
    pinfo->mirror = enable;
	
    return 0;

}

static int get_flip(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    return pinfo->flip;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    isp_api_wait_frame_end();
    if (enable) {
        if (pinfo->mirror) {
            write_reg(0x3213, 0x09);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x00);       
            write_reg(0x4500, 0x25);      
            write_reg(0x5044, 0x00);
        } else {		
            write_reg(0x3213, 0x09);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x06);       
            write_reg(0x4500, 0x21);      
            write_reg(0x5044, 0x20); 
        }
    } else {
        if (pinfo->mirror) {
            write_reg(0x3213, 0x08);        
            write_reg(0x3220, 0x00);       
            write_reg(0x3221, 0x00);       
            write_reg(0x4500, 0x25);      
            write_reg(0x5044, 0x00);
        } else {		
            write_reg(0x3213, 0x08);
            write_reg(0x3220, 0x00);        
            write_reg(0x3221, 0x06);       
            write_reg(0x4500, 0x21);          
            write_reg(0x5044, 0x20); 
        }	
    }
    
    pinfo->flip = enable;

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
		
    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);
    calculate_t_row();
    adjust_blk(g_psensor->fps);

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sSC2035info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sSC2035info[pinfo->currentmode].img_h;	

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
static void sc2035_deconstruct(void)
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

static int sc2035_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->interface = g_sSC2035info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sSC2035info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sSC2035info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sSC2035info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
    g_psensor->inv_clk = inv_clk;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;    
    g_psensor->min_exp = 1;    //100us
    g_psensor->max_exp = 5000; // 0.5 sec ,500ms
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
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
    sc2035_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc2035_init(void)
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

    if ((ret = sc2035_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc2035_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc2035_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc2035_init);
module_exit(sc2035_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor sc2035");
MODULE_LICENSE("GPL");
