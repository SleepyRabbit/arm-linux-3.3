/**
 * @file sen_bg0703.c
 * brigates BG070X sensor driver
 *
 * Copyright (C) 2013 GM Corp. (http://www.grain-media.com)
 *
 
 #define u32  int 
 #define u16  int 
 #define u8  int 
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

#define PFX "sen_bg0703k"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1280
#define DEFAULT_IMAGE_HEIGHT    720
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

static uint ch_num = 4;
module_param(ch_num, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_num, "channel number");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "frame per second");

static uint interface = IF_PARALLEL; // IF_LVDS; // IF_PARALLEL;
module_param(interface, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "BG0703"
#define SENSOR_MAX_WIDTH    1280 //1932
#define SENSOR_MAX_HEIGHT   720  // 1092

#define VREFH_LIMIT_H 0x7F
#define VREFH_LIMIT_L 0x0C
//Minimum Gain for Full Swing in small FD (8bits fix point)
#define MIN_DGAIN (0x133)
//FD Gain BG070X (10bits fix point)
#define FD_Gain_0701 (275)
#define FD_Gain_0703 (217)

//FD Switch Threshold (When AGAIN > ?)
#define FD_TH  342
/***  FD_gain=3.39  again=5.33 dgain var  ***/
//#define SWITCH_TH1    1069
#define SWITCH_TH1    1388
/***  FD_gain=3.39  again=var dgain =1.2  ***/
#define SWITCH_TH2    409
/***  FD_gain=1  again=5.33 dgain =var  ***/
#define SWITCH_TH3    260
/***  FD_gain=1  again=var  dgain =1.2  ***/

//Limit AGain to 1-12X
#define VREFH_LIMIT_H 0x7F
#define VREFH_LIMIT_L 0x0C

// Typical BLCC Target
#define BLCC_TARGET_TYPICAL (24)

#define blcc_target_coef_0701b (48)
#define blcc_target_base_0701b (11)

#define blcc_target_coef_0701d (48)
#define blcc_target_base_0701d (11)

#define blcc_target_coef_0701e (58)
#define blcc_target_base_0701e (11)

#define blcc_target_coef_0703b (91)
#define blcc_target_base_0703b (6)

#define blcc_target_coef_0703d (91)
#define blcc_target_base_0703d (6)

#define BG0701B_ID    (0x070107)
#define BG0701D_ID    (0x070109)
#define BG0701E_ID    (0x07010A)
#define BG0703B_ID    (0x070703)
#define BG0703D_ID    (0x070707)

static sensor_dev_t *g_psensor = NULL;

static u32  FD_Gain=0;
static u32  sensor_id=0;

typedef struct sensor_info {
    bool is_init;
    int row_bin;
    int col_bin;
    int mirror;
    int flip;
    u32 t_row;          // unit is 1/10 us
    int HMax;
    int VMax;
    int long_exp;
    u32 min_a_gain;
    u32 max_a_gain;
    u32 min_d_gain;
    u32 max_d_gain;
    u32 curr_a_gain;
    u32 curr_d_gain;
} sensor_info_t;

typedef struct sensor_reg {
    u16 addr;
    u16 val;
} sensor_reg_t;
#define DELAY_CODE      0x00

static sensor_reg_t sensor_720P_init_regs[] = {
    //30 fps
    {0xf0, 0x00},  //select page0
    //{0x1c, 0x01},  //soft reset
    {0x89, 0x21},  //internal vddpix off
    {0xb9, 0x21},  //Manual Gain
    
    {0x06, 0x05}, 
    {0x07, 0x08},
    {0x08, 0x02}, 
    {0x09, 0xD8},
    
    {0x0e, 0x04}, 
    {0x0f, 0xB0},  //row time = 0x408/Fmclk = 1032/24MHz = 43 us
    
    {0x14, 0x03},  //TXB ON *
    {0x1E, 0x0f},  //VTH 3.8V please check the voltage
    //{0x20, 0x02},  //mirror
    {0x20, 0x01},  //mirror
    
    {0x21, 0x00},  
    {0x22, 0x0E},
    
    {0x28, 0x00},  //RAMP1 ONLY
    {0x29, 0x18},  //RSTB =1us
    {0x2a, 0x18},  //TXB = 1us
    //{0x2d, 0x01},  
    //{0x2e, 0xB0},  //ibias_cnten_gap=17u
    {0x2d, 0x00},  
    {0x2e, 0x01},  //ibias_cnten_gap
    {0x30, 0x18},  //rstb_cmprst_gap=1u
    {0x34, 0x20},  //tx_ramp2=32 CLKIN cycle*
    
    {0x38, 0x03}, 
    {0x39, 0xfd}, 
    {0x3a, 0x03}, 
    {0x3b, 0xfa}, 
    
    {0x50, 0x00}, 
    {0x53, 0x6C}, 
    {0x54, 0x03}, 
    {0x52, 0xDE}, // PCLK OUTPUT 37.125MHz
    {0x60, 0x00},  //row refresh mode
    {0x6d, 0x01},  //pll=288M pclk=72M  (when clkin=24M)
    {0x64, 0x02},  
    {0x65, 0x00},  //RAMP1 length=200
    {0x67, 0x05}, 
    {0x68, 0xff},  //RAMP1 length=5ff
    {0x87, 0xaf},  // votlgate of vbg-i
    {0x1d, 0x01},  //restart
    
    {0xf0, 0x01}, 
    {0xc8, 0x04}, 
    {0xc7, 0x55},  // FD Gain = 1X 
    {0xe0, 0x01}, 
    {0xe1, 0x04}, 
    {0xe2, 0x03}, 
    {0xe3, 0x02}, 
    {0xe4, 0x01}, 
    {0xe5, 0x01},  //vcm_comp =2.56V
    {0xb4, 0x01},  //row noise remove on*
    {0x20, 0x00}, //blcc off
    {0x31, 0x00}, //blcc target upper high
    {0x32, 0x38}, 
    {0x33, 0x00}, //blcc target upper low
    {0x34, 0x35}, 
    {0x35, 0x00}, //blcc target lower high
    {0x36, 0x33}, 
    {0x37, 0x00}, //blcc target lower low
    {0x38, 0x30}, 
    {0x39, 0x04}, //frame count to ave
    
    {0x3E, 0x07},
    {0x3F, 0xff}, // Upper Limit
    
    {0x40, 0xff},
    {0x41, 0xC0}, // Lower Limit
    
    {0x20, 0x00},  //blcc on
    
    {0x4e, 0x00},  
    {0x4f, 0x00},  //digital offset
    
    {0xf1, 0x07},  //dpc on
    
    {0xf0, 0x00}, 
    {0x7f, 0x00},  //cmp current
    {0x81, 0x09},  //dot_en=1,vrst=vth,vtx=vth
    {0x82, 0x11},  //bandgap current & ramp current
    {0x83, 0x01},  //pixel current
    {0x84, 0x07},  //check rst voltage 
    {0x88, 0x05},  //pclk phase
    {0x8a, 0x01},  //pclk drv
    {0x8c, 0x01},  //data drv
    {0xb0, 0x01}, 
    {0xb1, 0x7f}, 
    {0xb2, 0x01}, 
    {0xb3, 0x7f},  //analog gain=1X
    {0xb4, 0x11}, 
    {0xb5, 0x11}, 
    {0xb6, 0x11}, 
    {0xb7, 0x01}, 
    {0xb8, 0x00},  //digital gain=1X
    {0xbf, 0x0c}, 
    {0x8e, 0x00},  //OEN
    {0x8d, 0x00},  //OEN
    {0x1d, 0x02}, 
    {0xFF, 0xFF}, //END
    {0xFF, 0xFF},
};
#define NUM_OF_720P_INIT_REGS (sizeof(sensor_720P_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    u16 a_gain;
    u16 d_gain;
    u16 fd_gain;
    u16 gain_x;     // UFIX7.6
} gain_set_t;

static gain_set_t gain_table[] =
{
    {127,  307, 0,   77},{125,  307, 0,   78},{124,  307, 0,   79},{122,  307, 0,   80},
    {120,  307, 0,   81},{118,  307, 0,   83},{116,  307, 0,   84},{114,  307, 0,   86},
    {112,  307, 0,   87},{110,  307, 0,   89},{108,  307, 0,   90},{106,  307, 0,   92},
    {104,  307, 0,   94},{102,  307, 0,   96},{100,  307, 0,   98},{ 98,  307, 0,  100},
    { 96,  307, 0,  102},{ 94,  307, 0,  104},{ 92,  307, 0,  106},{ 90,  307, 0,  109},
    { 88,  307, 0,  111},{ 86,  307, 0,  114},{ 84,  307, 0,  116},{ 82,  307, 0,  119},
    { 80,  307, 0,  122},{ 78,  307, 0,  125},{ 76,  307, 0,  129},{ 74,  307, 0,  132},
    { 72,  307, 0,  136},{ 70,  307, 0,  140},{ 68,  307, 0,  144},{ 66,  307, 0,  148},
    { 64,  307, 0,  153},{ 60,  307, 0,  163},{ 56,  307, 0,  175},{ 52,  307, 0,  188},
    { 48,  307, 0,  204},{ 44,  307, 0,  223},{ 40,  307, 0,  245},{ 36,  307, 0,  272},
    { 32,  307, 0,  307},{ 28,  307, 0,  350},{ 24,  307, 0,  409},{ 80,  307, 1,  416},
    { 76,  307, 1,  438},{ 72,  307, 1,  462},{ 68,  307, 1,  489},{ 64,  307, 1,  520},
    { 60,  307, 1,  555},{ 56,  307, 1,  594},{ 52,  307, 1,  640},{ 48,  307, 1,  693},
    { 44,  307, 1,  756},{ 40,  307, 1,  832},{ 36,  307, 1,  925},{ 32,  307, 1, 1040},
    { 28,  307, 1, 1189},{ 24,  307, 1, 1387},{ 24,  350, 1, 1582},{ 24,  400, 1, 1808},
    { 24,  450, 1, 2034},{ 24,  500, 1, 2260},{ 24,  550, 1, 2486},{ 24,  600, 1, 2712},
    { 24,  650, 1, 2938},{ 24,  700, 1, 3164},{ 24,  750, 1, 3390},{ 24,  800, 1, 3616},
    { 24,  850, 1, 3842},{ 24,  900, 1, 4068},{ 24,  950, 1, 4294},{ 24, 1000, 1, 4520},
    { 24, 1050, 1, 4746},{ 24, 1100, 1, 4972},{ 24, 1150, 1, 5198},{ 24, 1200, 1, 5424},
    { 24, 1250, 1, 5650},{ 24, 1300, 1, 5876},{ 24, 1350, 1, 6102},{ 24, 1400, 1, 6328},
    { 24, 1450, 1, 6554},{ 24, 1500, 1, 6780},{ 24, 1550, 1, 7006},{ 24, 1600, 1, 7232},
    { 24, 1650, 1, 7458},{ 24, 1700, 1, 7684},{ 24, 1750, 1, 7910},{ 24, 1800, 1, 8136},
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x64 >> 1)
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[2], tmp2[2];


    tmp[0]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
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


    buf[0]     = addr & 0xFF;
    buf[1]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    //printk("write_reg 0x%02x 0x%02x\n", buf[0], buf[1]);

    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    u32 pclk;
	
	pclk = 37125000;
    isp_info("pclk(%d) XCLK(%d)\n", pclk, xclk);

    return pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    g_psensor->pclk = get_pclk(g_psensor->xclk);

    // trow unit is 1/10 us
    pinfo->t_row = (pinfo->HMax * 10000) / (g_psensor->pclk / 1000);
    isp_info("HMAX=%d, VMAX=%d, t_row=%d\n", pinfo->HMax, pinfo->VMax, pinfo->t_row);
}

static void adjust_blanking(int fps)
{
    u32 vmax=0, vsize, reg, max_fps;

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    vmax = 750;   //30 fps

    max_fps = 30;    
    if (fps > max_fps)
        fps = max_fps;
    pinfo->VMax = vmax * max_fps / fps;
    read_reg(0x08, &reg);
    read_reg(0x09, &vsize);
    vsize += reg << 8;
    reg = pinfo->VMax - 8 - vsize;
   	write_reg(0x21, reg >> 8);
   	write_reg(0x22, reg & 0xFF);
   	write_reg(0x1d, 0x02);   
    isp_info("adjust_blanking, fps=%d\n", fps);
    
    pinfo->HMax = 1650;   //30 fps
    calculate_t_row();
    g_psensor->fps = fps;
}

static void update_bayer_type(void);
static int set_mirror(int enable);
static int set_flip(int enable);

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int i, ret = 0;

	isp_info("w=%d, h=%d\n", width, height);

    // set initial registers
    for (i=0; i<NUM_OF_720P_INIT_REGS; i++) {

         if (sensor_720P_init_regs[i].addr == DELAY_CODE) {
            mdelay(sensor_720P_init_regs[i].val);
         }
         else if(write_reg(sensor_720P_init_regs[i].addr, sensor_720P_init_regs[i].val) < 0) {
                isp_err("%s init fail!!", SENSOR_NAME);
                return -EINVAL;
         }
     }

    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);
    update_bayer_type();

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;

    adjust_blanking(g_psensor->fps);

    g_psensor->out_win.width = 1280;
    g_psensor->out_win.height = 720;
    g_psensor->img_win.x = (1280 - width) / 2;
    g_psensor->img_win.y = (720 - height) / 2;
	
    return ret;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;
    u32 num_of_line;

    if (!g_psensor->curr_exp) {
        read_reg(0x0d, &reg_l);
        read_reg(0x0c, &reg_h);
        num_of_line = (((reg_h & 0xFF) << 8) | (reg_l & 0xFF));
        g_psensor->curr_exp = (num_of_line * pinfo->t_row + 500) / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
	sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	int ret = 0;
	u32 lines, tmp, vsize, vblank;

	lines = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;
	// printk("line %d,exp %d,t_row %d\n",lines,exp,pinfo->t_row);

	if (lines<=1)
		lines = 1;
    read_reg(0x08, &tmp);
    read_reg(0x09, &vsize);
    vsize += tmp << 8;
    if (lines <= pinfo->VMax) {
        vblank = pinfo->VMax - 8 - vsize;
    	write_reg(0xf0,0x00);
    	write_reg(0x0c, ((lines >> 8) & 0xFF) );
    	write_reg(0x0d, (lines & 0xFF) );
        if (pinfo->long_exp) {
            write_reg(0x21, ((vblank >> 8) & 0xFF));
            write_reg(0x22, (vblank & 0xFF));
            pinfo->long_exp = 0;
        }
    	write_reg(0x1d,0x02);
    }
    else {
        vblank = lines - 8 - vsize;
    	write_reg(0xf0,0x00);
    	write_reg(0x0c, ((lines >> 8) & 0xFF) );
    	write_reg(0x0d, (lines & 0xFF) );
        write_reg(0x21, ((vblank >> 8) & 0xFF));
        write_reg(0x22, (vblank & 0xFF));
    	write_reg(0x1d,0x02);
        pinfo->long_exp = 1;
    }
	
	g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;
	// printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;
}

static u32 get_gain(void)
{

    u32 reg;
    u32 a_gain, d_gain, fd_gain;
    if (!g_psensor->curr_gain) {
        write_reg(0xF0, 0x01); // Select Page 1
        read_reg(0xC7, &fd_gain); // FD_Gain
        write_reg(0xF0, 0x00); // Select Page 0
        read_reg(0xB1, &a_gain); // AGain
        read_reg(0xB7, &reg); // DGain
        read_reg(0xB8, &d_gain);
        fd_gain = fd_gain == 0xAA ? 339 : 100;
        d_gain = (reg << 8)| (d_gain & 0xff);
		g_psensor->curr_gain = (100 * 128 / a_gain * d_gain / 256 * fd_gain) * 64 / 10000;
    }

    return g_psensor->curr_gain;
}

u32 get_sensor_id(void)
{
	u32 reg_temp1=0,reg_temp2=0,reg_temp3=0;
	write_reg(0xF0, 0x00);  //select page0
	read_reg(0x00,&reg_temp1);
	read_reg(0x01,&reg_temp2);
	read_reg(0x45,&reg_temp3);
	sensor_id = (reg_temp1<<16)|(reg_temp2<<8)|(reg_temp3<<0);
	msleep(200);
	if(sensor_id==BG0701B_ID)
	{
		FD_Gain = FD_Gain_0701;
	}
	else if(sensor_id==BG0701D_ID)
	{
		FD_Gain = FD_Gain_0701;
	}
	else if(sensor_id==BG0701E_ID)
	{
		FD_Gain = FD_Gain_0701;
	}
	else if(sensor_id==BG0703B_ID)
	{
		FD_Gain = FD_Gain_0703;
	}
	else if((sensor_id&0xFFFF00)==(BG0703D_ID&0xFFFF00))
	{
		FD_Gain = FD_Gain_0703;
	}
	else
	{
		isp_err("cmos_get_ae_default : Invalid Sensor (ID = 0x%06X) !\n", sensor_id);
		return -1;
	}	
    isp_info("Sensor (ID = 0x%06X)\n", sensor_id);

    return 0;

}
static signed short cmos_blcc_update(u16 darkrow_avg, u16 reg_dgain)
{
	static signed short offset = 0;
	signed short output;
	signed short target;
	signed short target_upper_hi;
	signed short target_upper_lo;
	signed short target_lower_hi;
	signed short target_lower_lo;
	signed short diff;
	u16 blcc_coef;
	u16 blcc_base;

	if(sensor_id==BG0701B_ID)
	{
		blcc_coef = blcc_target_coef_0701b;
		blcc_base = blcc_target_base_0701b;
	}
	else if(sensor_id==BG0701D_ID)
	{
		blcc_coef = blcc_target_coef_0701d;
		blcc_base = blcc_target_base_0701d;
	}
	else if(sensor_id==BG0701E_ID)
	{
		blcc_coef = blcc_target_coef_0701e;
		blcc_base = blcc_target_base_0701e;
	}
	else if(sensor_id==BG0703B_ID)
	{
		blcc_coef = blcc_target_coef_0703b;
		blcc_base = blcc_target_base_0703b;
	}
	else if((sensor_id&0xFFFF00)==(BG0703D_ID&0xFFFF00))
	{
		blcc_coef = blcc_target_coef_0703d;
		blcc_base = blcc_target_base_0703d;
	}
	else
	{
		isp_err("cmos_blcc_update : Invalid Sensor (ID = 0x%06X) !\n", sensor_id);
		return 0;
	}

	target = ((BLCC_TARGET_TYPICAL<<8)/reg_dgain) + (((darkrow_avg*blcc_coef)>>10) + blcc_base);
	output = darkrow_avg + offset;

	target_upper_hi = target+8;
	target_upper_lo = target+4;
	target_lower_hi = target-4;
	target_lower_lo = target-8;

	if(output>target_upper_hi)
	{
		diff = -1 - ((output - target_upper_hi)>>1);
	}
	else if(output>target_upper_lo)
	{
		diff = -1;
	}
	else if(output<target_lower_lo)
	{
		diff = 1 + ((target_lower_lo - output)>>1);
	}
	else if(output<target_lower_hi)
	{
		diff = 1;
	}
	else
	{
		diff = 0;
	}

	offset += diff;

	if(offset>128) offset = 128;
	if(offset<-2048) offset = -2048;
	//printf("dgain=0x%04X dark=%d target=%d output=%d diff=%d offset=%d \n", reg_dgain, darkrow_avg, target, output, diff, offset);

	return offset;
}

#define MAX_A_GAIN_IDX  57

static int set_gain(u32 gain)
{
    u32 reg_l, reg_h;
    u32 a_gain, d_gain, fd_gain;
    int ret = 0, i;
	u16 darkrow_ave;
	u16 reg_offset;

    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    a_gain = gain_table[i-1].a_gain & 0xFF;
    d_gain = gain_table[i-1].d_gain & 0xFFF;
    fd_gain = gain_table[i-1].fd_gain ? 0xAA : 0x55;

    write_reg(0xF0, 0x01); // Select Page 1
    write_reg(0xC7, fd_gain); // FD_Gain
    write_reg(0xF0, 0x00); // Select Page 0
    write_reg(0xB1, a_gain); // AGain
    write_reg(0xB7, d_gain >> 8); // DGain
    write_reg(0xB8, d_gain & 0xFF);

	read_reg(0x44,&reg_h);
	read_reg(0x45,&reg_l);	
	darkrow_ave = (reg_h<<8)|reg_l; // before DGAIN	
	if(darkrow_ave>=(1<<11))darkrow_ave = 0; // Signed 1+11 bit
	reg_offset = cmos_blcc_update(darkrow_ave, d_gain);
	
	write_reg(0x4E, 0xFF&(reg_offset>>8));
	write_reg(0x4F, 0xFF&(reg_offset));

    write_reg(0xF0, 0x00); // Select Page 0
    write_reg(0x1D, 0x02); // Updates all registers
    
    g_psensor->curr_gain = gain_table[i-1].gain_x;

	return ret;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    if (pinfo->mirror) {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GB;
        else
            g_psensor->bayer_type = BAYER_RG;
    } else {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GB;
        else
            g_psensor->bayer_type = BAYER_RG;
    }
}


static int get_mirror(void)
{
	u32 reg;

	read_reg(0x20, &reg);

	return reg & 0x2;
}

static int set_mirror(int enable)
{
    	sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	u32 reg;

	pinfo->mirror = enable;
	
	read_reg(0x20, &reg);
	
    if (pinfo->flip)
		reg |= 0x01;
	else
		reg &= ~0x01;
	if (enable)
		reg |= 0x02;
	else
		reg &= ~0x02;
	
	write_reg(0x20, reg);
	// enable
	write_reg(0x1d, 0x02);

	// Page0, 0x03 , value + 1

   	return 0;
}

static int get_flip(void)
{
	u32 reg;

	read_reg(0x20, &reg);

	return reg & 0x1;
}

static int set_flip(int enable)
{
	sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	u32 reg;

	pinfo->flip = enable;

	read_reg(0x20, &reg);
    if (pinfo->mirror)
		reg |= 0x02;
    else
		reg &= ~0x02;
    if (enable)
		reg |= 0x01;
	else
		reg &= ~0x01;
	write_reg(0x20, reg);
	// enable
	write_reg(0x1d, 0x02);
	
	// Page0, 0x05 , value + 1

   	return 0;
}

static int set_fps(int fps)
{
    	adjust_blanking(fps);
    	g_psensor->fps = fps;

    	//isp_info("pdev->fps=%d\n",pdev->fps);

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
	     update_bayer_type();
            break;
        case ID_FLIP:
            set_flip((int)arg);
	     update_bayer_type();
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

static int init(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;

    if (pinfo->is_init)
        return 0;
	
    isp_info("BG0703 init\n");
    isp_info("Interface : Parallel\n");
	get_sensor_id();

    pinfo->mirror = mirror;
    pinfo->flip = flip;

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);
    if (ret == 0)
        pinfo->is_init = 1;

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
static void bg0703_deconstruct(void)
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

static int bg0703_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->fmt = RAW12;
    g_psensor->bayer_type = BAYER_BG;// BAYER_GB;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;
    g_psensor->max_exp = 5000; // 0.5 sec
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    g_psensor->exp_latency = 0;
    g_psensor->gain_latency = 1;
    g_psensor->fps = fps;
    g_psensor->interface = IF_PARALLEL;
    g_psensor->num_of_lane = ch_num;
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
    pinfo->min_a_gain = gain_table[0].gain_x;
    pinfo->max_a_gain = gain_table[MAX_A_GAIN_IDX].gain_x;
    pinfo->curr_a_gain = 0;
    pinfo->min_d_gain = 64;
    pinfo->max_d_gain = 384;
    pinfo->curr_d_gain = 0;

    if ((ret = sensor_init_i2c_driver()) < 0)
        goto construct_err;

    if ((ret = init()) < 0)
        goto construct_err;

    return 0;

construct_err:
    bg0703_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init bg0703_init(void)
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

    if ((ret = bg0703_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit bg0703_exit(void)
{
    isp_unregister_sensor(g_psensor);
    bg0703_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(bg0703_init);
module_exit(bg0703_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor BG0703");
MODULE_LICENSE("GPL");
