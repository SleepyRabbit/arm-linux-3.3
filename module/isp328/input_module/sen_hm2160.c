/**
 * @file sen_hm2160.c
 * OmniVision hm2160 sensor driver
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

static uint mirror = 1;
module_param(mirror, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mirror, "sensor mirror function");

static uint flip = 1;
module_param(flip, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(flip, "sensor flip function");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static ushort interface = IF_MIPI;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");
//=============================================================================
// global
//============================================================================
#define SENSOR_NAME         "hm2160"
#define SENSOR_MAX_WIDTH    1936
#define SENSOR_MAX_HEIGHT   1096

static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    int mirror;
    int flip;
    u32 total_line;    
    u32 reg04;
    u32 t_row;          // unit is 1/10 us
    int htp;
    int pclk;
} sensor_info_t;

typedef struct sensor_reg {
    u16 addr;
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_mipi_init_regs[] = {
    //------------------------------------------------------------------------
    // Initial HM2160
    //------------------------------------------------------------------------
    {0x0103, 0x00},// software reset
    //added for static bpc part1
    {0x3519, 0x00},// PWE interval high byte
    {0x351A, 0x05},// PWE interval low byte
    {0x351B, 0x1E},// PWE interval high byte
    {0x351C, 0x90},// PWE interval low byte
    {0x351E, 0x15},// PRD high pulse byte
    {0x351D, 0x15},// PRD interval low byte
    {0xBA93, 0x01},// Read Header
    
    // start of BIST
    {0xBAA2, 0xC0}, 
    {0xBAA2, 0xC0}, 
    {0xBAA2, 0xC0}, 
    {0xBAA2, 0x40}, 
    // end of BIST
    {0xBA93, 0x03},// Read Header
    {0xBA93, 0x02},// Read Header
    {0xBA90, 0x01},// BPS_OTP_EN0
    //end of added for static bpc part 1
    {0x0100, 0x02},// power up
    {0x4024, 0x00},// disabled MIPI
    {0x4B30, 0x00},// reset MIPI 
    {0x4B30, 0x0F},// release reset MIPI
    //------------------------------------------------------------------------
    // PLL
    //------------------------------------------------------------------------
    //{0x4001, 0x01},// ck_cfg - choose mclk_pll
    // PCLK (RAW)
    {0x0303, 0x02},// CLK_DIV, 1: divide by 1, 2: divide by 2
    {0x0305, 0x0C},// PLL N,   N = 12, while mclk 24mhz
    {0x0307, 0x3F},// PLL M,   M = 63, pclk_raw = (mclk*2*(M/N))/(4*CLK_DIV) 
    // PKTCLK (MIPI)
    {0x0309, 0x01},// CLK_DIV, 1:divide by 1, 2:divide by 2
    {0x030D, 0x0C},// PLL N,   N = 12, while mclk 24mhz
    {0x030F, 0x2B},// PLL M,   M = 48, pkt_clk=(mclk*2*(M/N))/(4*CLK_DIV)
    //------------------------------------------------------------------------
    // Analog
    //------------------------------------------------------------------------ 
    {0x4131, 0x01},  //reorder disable, BLI enable
    {0x4144, 0x03},  
    {0x4145, 0x31},  
    {0x4146, 0x51},  
    {0x4147, 0x03},  
    {0x4148, 0xFF},  
    {0x4270, 0x06},  
    {0x4271, 0xFF},  
    {0x4272, 0x00},  
    {0x4380, 0xAA},  
    {0x4381, 0x70},  
    {0x4384, 0x20},  
    {0x4385, 0xA4},  
    {0x4386, 0x52},  
    {0x4387, 0x07},  
    {0x438A, 0x80},  
    {0x438B, 0x00},  
    {0x438C, 0x33},  
    {0x438D, 0xA0},  
    {0x438F, 0x00},  
    {0x44D0, 0x24},  
    {0x44D1, 0x24},  
    {0x44D2, 0x24},  
    {0x44D3, 0x24},  
    {0x44D4, 0x24},  
    {0x44D5, 0x24},  
    {0x4C00, 0x00},  
    {0x45FC, 0x10},  
    //end of Analog//---------------------------------------------------------
    //------------------------------------------------------------------------
    // Digital
    //------------------------------------------------------------------------
    {0x4149, 0x5C},
    {0x414A, 0x02},
    {0x400D, 0x04},
    //------------------------------------------------------------------------
    // Frame Timing
    //------------------------------------------------------------------------
    {0x0340, 0x04},// smia Frame length Hb 
    {0x0341, 0x86},// smia Frame length Lb (VSYNC : 1158)
    {0x0342, 0x03},// smia line  length Hb 
    {0x0343, 0xE8},// smia line  length Lb (HSYNC : 1000)
    //------------------------------------------------------------------------
    // Image Size
    //------------------------------------------------------------------------
    {0x0344, 0x00},// start of x Hb       
    {0x0345, 0x00},// start of x Lb (   0)
    {0x0348, 0x07},// end   of x Hb       
    {0x0349, 0x8F},// end   of x Lb (1935)                                       
    {0x0346, 0x00},// start of y Hb       
    {0x0347, 0x00},// start of y Lb (   0)
    {0x034A, 0x04},// end   of y Hb       
    {0x034B, 0x47},// end   of y Lb (1095)
    //------------------------------------------------------------------------
    // Output Size / Digiatl Window
    //------------------------------------------------------------------------
    {0x034C, 0x07},// output size of x Hb
    {0x034D, 0x90},// output size of x Lb (1936)
    {0x034E, 0x04},// output size of y Hb
    {0x034F, 0x48},// output size of y Lb (1096)
    //------------------------------------------------------------------------
    // Binning / Subsample
    //------------------------------------------------------------------------
    {0x0381, 0x01},// X_EVEN_INC
    {0x0383, 0x01},// X_ODD_INC, 01:Full, 03:Sub2
    {0x0385, 0x01},// Y_EVEN_INC
    {0x0387, 0x01},// Y_ODD_INC, 01:Full, 03:Sub2
    {0x0390, 0x00},// [1:0], 0:No binning, 1:VBinning, 2:HBinning, 3:H+VBinning//
    //------------------------------------------------------------------------
    // Embedded Function
    //------------------------------------------------------------------------
    {0x4B0C, 0x09},// Embedded Data Length Hb
    {0x4B0D, 0x74},// Embedded Data Length Lb
    {0x4B48, 0x01},// Embedded Data CFG
    {0x0102, 0x00},// Embedded Enable, 0:Disable, 1:Enable
    //------------------------------------------------------------------------
    // MIPI      
    //------------------------------------------------------------------------
    {0x0111, 0x01},// 0:1lane, 1:2lane 
    {0x0112, 0x0A},// DataFormat : 0x08 : RAW8, 0x0A : RAW10  
    {0x0113, 0x0A},// DataFormat : 0x08 : RAW8, 0x0A : RAW10 
    {0x4B01, 0x2B},// DataFormat : 0x2A : RAW8, 0x2B : RAW10 
    //{0x4B20, 0xDE},// CC0 : Clock lane on always.(9E) / woLSLE (8E)// GC2 : Clock lane on while frame active.  (DE) / woLSLE (CE) 
    {0x4B20, 0x9E},
    {0x4B08, 0x04},// VSIZE (1096) 
    {0x4B09, 0x48},//
    {0x4B0A, 0x07},// HSIZE (1936)
    {0x4B0B, 0x90},//
    {0x4B18, 0x0C},// [7:0] FULL_TRIGGER_TIME                 
    {0x4B02, 0x02},// TLPX
    {0x4B43, 0x03},// CLK_PREPARE
    {0x4B05, 0x0B},// CLK_ZERO
    {0x4B0E, 0x00},// CLK_FRONT_PORCH (CLK_PRE)
    {0x4B0F, 0x09},// CLK_BACK_PORCH (CLK_POST)
    {0x4B06, 0x02},// CLK_TRAIL
    {0x4B39, 0x04},// CLK_EXIT                  
    {0x4B42, 0x03},// HS_PREPARE
    {0x4B03, 0x0D},// HS_ZERO
    {0x4B04, 0x03},// HS_TRAIL
    {0x4B3A, 0x04},// HS_EXIT
    {0x4B11, 0x0F},// LP strength
    {0x4B47, 0x40},// MIPI skew                  
    {0x4B51, 0x00},// disabled MIPI Status Monitor 
    {0x4B52, 0x49},// disabled Dynamic Trigger Value Correction
    {0x4B57, 0x03},// Dynamic Range Setting for Dynamic Trigger Value Correction, 0:no correction, 1:-15~+15, 2:-31~+31, 3:-63~+63, 
    {0x4B56, 0x02},// disabled AutoReset           
    {0x4B51, 0x80},// enabled MIPI Status Monitor 
    {0x4B52, 0xC9},// enabled Dynamic Trigger Value Correction 
    {0x4024, 0x40},// Enabled MIPI
    //------------------------------------------------------------------------
    // CMU update
    //------------------------------------------------------------------------
    {0X4800, 0X00},
    {0X0104, 0X01},
    {0X0104, 0X00},
    {0X4801, 0X00},
    {0X0000, 0X00},
    //added for static bpc part 2
    {0XBA94, 0X01},
    {0XBA94, 0X00},
    {0XBA93, 0X02},
    //added for static bpc part 2
    //------------------------------------------------------------------------
    // Turn on rolling shutter
    //------------------------------------------------------------------------
    {0X0100, 0X01},// mode_select
};
#define NUM_OF_MIPI_INIT_REGS (sizeof(sensor_mipi_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  ad_reg;
    u32 gain_x;
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00,	64  }, {0x01,	68  }, {0x02,	72  }, {0x03,	76  }, {0x04,	80  }, {0x05,	84  }, {0x06,	88  }, {0x07,	92  }, 
    {0x08,	96  }, {0x09,	100 }, {0x0A,	104 }, {0x0B,	108 }, {0x0C,	112 }, {0x0D,	116 }, {0x0E,	120 }, {0x0F,	124 }, 
    {0x10,	128 }, {0x11,	132 }, {0x12,	136 }, {0x13,	140 }, {0x14,	144 }, {0x15,	148 }, {0x16,	152 }, {0x17,	156 }, 
    {0x18,	160 }, {0x19,	164 }, {0x1A,	168 }, {0x1B,	172 }, {0x1C,	176 }, {0x1D,	180 }, {0x1E,	184 }, {0x1F,	188 }, 
    {0x20,	192 }, {0x21,	196 }, {0x22,	200 }, {0x23,	204 }, {0x24,	208 }, {0x25,	212 }, {0x26,	216 }, {0x27,	220 }, 
    {0x28,	224 }, {0x29,	228 }, {0x2A,	232 }, {0x2B,	236 }, {0x2C,	240 }, {0x2D,	244 }, {0x2E,	248 }, {0x2F,	252 }, 
    {0x30,	256 }, {0x31,	260 }, {0x32,	264 }, {0x33,	268 }, {0x34,	272 }, {0x35,	276 }, {0x36,	280 }, {0x37,	284 }, 
    {0x38,	288 }, {0x39,	292 }, {0x3A,	296 }, {0x3B,	300 }, {0x3C,	304 }, {0x3D,	308 }, {0x3E,	312 }, {0x3F,	316 }, 
    {0x40,	320 }, {0x41,	324 }, {0x42,	328 }, {0x43,	332 }, {0x44,	336 }, {0x45,	340 }, {0x46,	344 }, {0x47,	348 }, 
    {0x48,	352 }, {0x49,	356 }, {0x4A,	360 }, {0x4B,	364 }, {0x4C,	368 }, {0x4D,	372 }, {0x4E,	376 }, {0x4F,	380 }, 
    {0x50,	384 }, {0x51,	388 }, {0x52,	392 }, {0x53,	396 }, {0x54,	400 }, {0x55,	404 }, {0x56,	408 }, {0x57,	412 }, 
    {0x58,	416 }, {0x59,	420 }, {0x5A,	424 }, {0x5B,	428 }, {0x5C,	432 }, {0x5D,	436 }, {0x5E,	440 }, {0x5F,	444 }, 
    {0x60,	448 }, {0x61,	452 }, {0x62,	456 }, {0x63,	460 }, {0x64,	464 }, {0x65,	468 }, {0x66,	472 }, {0x67,	476 }, 
    {0x68,	480 }, {0x69,	484 }, {0x6A,	488 }, {0x6B,	492 }, {0x6C,	496 }, {0x6D,	500 }, {0x6E,	504 }, {0x6F,	508 }, 
    {0x70,	512 }, {0x71,	516 }, {0x72,	520 }, {0x73,	524 }, {0x74,	528 }, {0x75,	532 }, {0x76,	536 }, {0x77,	540 }, 
    {0x78,	544 }, {0x79,	548 }, {0x7A,	552 }, {0x7B,	556 }, {0x7C,	560 }, {0x7D,	564 }, {0x7E,	568 }, {0x7F,	572 }, 
    {0x80,	576 }, {0x81,	580 }, {0x82,	584 }, {0x83,	588 }, {0x84,	592 }, {0x85,	596 }, {0x86,	600 }, {0x87,	604 }, 
    {0x88,	608 }, {0x89,	612 }, {0x8A,	616 }, {0x8B,	620 }, {0x8C,	624 }, {0x8D,	628 }, {0x8E,	632 }, {0x8F,	636 }, 
    {0x90,	640 }, {0x91,	644 }, {0x92,	648 }, {0x93,	652 }, {0x94,	656 }, {0x95,	660 }, {0x96,	664 }, {0x97,	668 }, 
    {0x98,	672 }, {0x99,	676 }, {0x9A,	680 }, {0x9B,	684 }, {0x9C,	688 }, {0x9D,	692 }, {0x9E,	696 }, {0x9F,	700 }, 
    {0xA0,	704 }, {0xA1,	708 }, {0xA2,	712 }, {0xA3,	716 }, {0xA4,	720 }, {0xA5,	724 }, {0xA6,	728 }, {0xA7,	732 }, 
    {0xA8,	736 }, {0xA9,	740 }, {0xAA,	744 }, {0xAB,	748 }, {0xAC,	752 }, {0xAD,	756 }, {0xAE,	760 }, {0xAF,	764 }, 
    {0xB0,	768 }, {0xB1,	772 }, {0xB2,	776 }, {0xB3,	780 }, {0xB4,	784 }, {0xB5,	788 }, {0xB6,	792 }, {0xB7,	796 }, 
    {0xB8,	800 }, {0xB9,	804 }, {0xBA,	808 }, {0xBB,	812 }, {0xBC,	816 }, {0xBD,	820 }, {0xBE,	824 }, {0xBF,	828 }, 
    {0xC0,	832 }, {0xC1,	836 }, {0xC2,	840 }, {0xC3,	844 }, {0xC4,	848 }, {0xC5,	852 }, {0xC6,	856 }, {0xC7,	860 }, 
    {0xC8,	864 }, {0xC9,	868 }, {0xCA,	872 }, {0xCB,	876 }, {0xCC,	880 }, {0xCD,	884 }, {0xCE,	888 }, {0xCF,	892 }, 
    {0xD0,	896 }, {0xD1,	900 }, {0xD2,	904 }, {0xD3,	908 }, {0xD4,	912 }, {0xD5,	916 }, {0xD6,	920 }, {0xD7,	924 }, 
    {0xD8,	928 }, {0xD9,	932 }, {0xDA,	936 }, {0xDB,	940 }, {0xDC,	944 }, {0xDD,	948 }, {0xDE,	952 }, {0xDF,	956 }, 
    {0xE0,	960 }, {0xE1,	964 }, {0xE2,	968 }, {0xE3,	972 }, {0xE4,	976 }, {0xE5,	980 }, {0xE6,	984 }, {0xE7,	988 }, 
    {0xE8,	992 }, {0xE9,	996 }, {0xEA,	1000}, {0xEB,	1004}, {0xEC,	1008}, {0xED,	1012}, {0xEE,	1016}, {0xEF,	1020}, 
    {0xF0,	1024},
};  
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x48 >> 1)
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

static void adjust_vblk(int fps)
{    
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;  
    
    if((fps > 60) || (fps < 5))
        return;                                                                            
    
    if((fps <= 60) && (fps >= 5)) {        
        pinfo->total_line = pinfo->pclk / fps / pinfo->htp;
        reg_h = (pinfo->total_line >> 8) & 0xFF;
        reg_l = pinfo->total_line & 0xFF;
        write_reg(0x0340, reg_h);
        write_reg(0x0341, reg_l);                
        isp_dbg("pinfo->total_line=%d\n",pinfo->total_line);
    }    
    write_reg(0x0104, 0x01);//CMD Update
    write_reg(0x0104, 0x00);//CMD Update                
    //Update fps status
    g_psensor->fps = fps;  
    isp_dbg("g_psensor->fps=%d\n", g_psensor->fps);                          
    
}

static u32 get_pclk(u32 xclk)
{
    u32 pixclk;

    pixclk = 35440000;//xclk=27MHz@30fps       
          
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
        
    //printk("x=%d,y=%d\n",g_psensor->img_win.x , g_psensor->img_win.y);        

    return ret;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, sw_u, sw_l;

    if (!g_psensor->curr_exp) {
        read_reg(0x0202, &sw_u);//high byte
        read_reg(0x0203, &sw_l);//low  byte        
        lines = (sw_u << 8) | sw_l;
        g_psensor->curr_exp = (lines * pinfo->t_row + 50) / 100;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, tmp, max_exp_line;

    lines = ((exp * 1000)+ pinfo->t_row /2) / pinfo->t_row;
    
    isp_dbg("exp=%d,pinfo->t_row=%d\n", exp, pinfo->t_row);

    if (lines == 0)
        lines = 1;  
          
    max_exp_line = pinfo->total_line - 2;//HW Limit
   
    if (lines > max_exp_line) {
        tmp = max_exp_line;      
    } else {
        tmp = lines;
    }    

    if (lines > max_exp_line) {
        write_reg(0x0340, (lines >> 8) & 0xFF);
        write_reg(0x0341, (lines & 0xFF));                
        //printk("lines = %d\n",lines); 
        write_reg(0x0203, (lines & 0xFF));//Coarse_Integration_L[7:0]
        write_reg(0x0202, ((lines >> 8) & 0xFF));//Coarse_Integration_H[7:0]
    }else{
        write_reg(0x0340, (pinfo->total_line >> 8) & 0xFF);
        write_reg(0x0341, (pinfo->total_line & 0xFF)); 
        write_reg(0x0203, (tmp & 0xFF));//Coarse_Integration_L[7:0]
        write_reg(0x0202, ((tmp >> 8) & 0xFF));//Coarse_Integration_H[7:0]
    }
    write_reg(0x0104, 0x01);//CMD Update
    write_reg(0x0104, 0x00);//CMD Update
   
    g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;             
    //printk("g_psensor->curr_exp=%d,max_exp_line=%d,lines=%d,tmp=%d\n", g_psensor->curr_exp, max_exp_line, lines, tmp);

    return 0;
}

static u32 get_gain(void)
{
    u32 gain, reg;

     if (g_psensor->curr_gain == 0) {       
        read_reg(0x0205, &reg);//analog gain
        gain = ((reg + 16) / 16);
        //printk("gain=%d\n", gain);
        gain = gain * 64;
        g_psensor->curr_gain = gain;
        //printk("get_curr_gain=%d\n",g_psensor->curr_gain);
    }

    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    int ret = 0, i;
    u32 ad_gain;
    
    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }
    
    ad_gain = gain_table[i-1].ad_reg;    
    
    write_reg(0x0205, ad_gain);
    
    g_psensor->curr_gain = gain_table[i-1].gain_x;
    
    write_reg(0x0104, 0x01);//CMD Update
    write_reg(0x0104, 0x00);//CMD Update
        
    //printk("g_psensor->curr_gain=%d, ad_gain=0x%x\n",g_psensor->curr_gain, ad_gain);

    return ret;
}

static int get_mirror(void)
{
    u32 reg;
    read_reg(0x0101, &reg);
    return (reg & BIT0) ? 1 : 0;
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;    

    if (enable) {
        pinfo->reg04 |= BIT0;
    } else {
        pinfo->reg04 &=~BIT0;
    }

    write_reg(0x0101, pinfo->reg04);
    write_reg(0x0104, 0x01);//CMD Update
    write_reg(0x0104, 0x00);//CMD Update
    pinfo->mirror = enable;  

    return 0;
}

static int get_flip(void)
{
    u32 reg;
    read_reg(0x0101, &reg);
    return (reg & BIT1) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (enable) {
        pinfo->reg04 |= BIT1;
    } else {
        pinfo->reg04 &= ~BIT1;
    }
    write_reg(0x0101, pinfo->reg04);
    write_reg(0x0104, 0x01);//CMD Update
    write_reg(0x0104, 0x00);//CMD Update
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
    u32 reg_h, reg_l, Chip_id_h, Chip_id_l, Chip_id;
    int i;    

    if (pinfo->is_init)
        return 0;        
        
    // write initial register value
    for (i=0; i<NUM_OF_MIPI_INIT_REGS; i++) {
        if (write_reg(sensor_mipi_init_regs[i].addr, sensor_mipi_init_regs[i].val) < 0) {
            isp_err("failed to initial %s!\n", SENSOR_NAME);
            return -EINVAL;
        }
    }    

    //Read ID
    read_reg(0x0000, &Chip_id_h);    
    read_reg(0x0001, &Chip_id_l);
    Chip_id = (Chip_id_h << 8) | Chip_id_l;
    isp_dbg("Chip_id = 0x%x\n", Chip_id);
    
    read_reg(0x0340, &reg_h);//Frame length Hb 
    read_reg(0x0341, &reg_l);//Frame length Lb (VSYNC : 1158)
    pinfo->total_line = ((reg_h << 8) | reg_l);         
    //g_psensor->pclk = get_pclk(g_psensor->xclk); 
    g_psensor->pclk = 45000000;     
    pinfo->pclk = get_pclk(g_psensor->xclk);
        
    read_reg(0x0342, &reg_h);//line  length Hb 
    read_reg(0x0343, &reg_l);//line  length Lb (HSYNC : 1000)
    pinfo->htp = ((reg_h << 8) | reg_l); 
    pinfo->t_row = (pinfo->htp * 10000) / (pinfo->pclk / 1000);    
        
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;             
    
    //printk("pinfo->total_line=%d, pinfo->htp=%d, pinfo->t_row=%d, g_psensor->min_exp=%d\n", pinfo->total_line, pinfo->htp, pinfo->t_row, g_psensor->min_exp);    

    // set image size
    if ((ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height)) < 0)
        return ret;
        
    adjust_vblk(g_psensor->fps);

    // set mirror and flip
    set_mirror(mirror);    
    set_flip(flip);
    //printk("mirror=%d,flip=%d\n",mirror ,flip);

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
static void hm2160_deconstruct(void)
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

static int hm2160_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->interface = interface;
    g_psensor->num_of_lane = 2;
    g_psensor->fmt = RAW10;
    g_psensor->bayer_type = BAYER_RG;
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
        
    if (g_psensor->interface == IF_MIPI){
	      // no frame if init this sensor after mipi inited
    	  if ((ret = init()) < 0)
            goto construct_err;
	  }        
        
    return 0;

construct_err:
    hm2160_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init hm2160_init(void)
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

    if ((ret = hm2160_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit hm2160_exit(void)
{
    isp_unregister_sensor(g_psensor);
    hm2160_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(hm2160_init);
module_exit(hm2160_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor hm2160");
MODULE_LICENSE("GPL");
