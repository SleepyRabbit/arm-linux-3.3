/**
 * @file sen_gc1024.c
 * GC1024 sensor driver
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

#define PFX "sen_gc1024"
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

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "GC1024"
#define SENSOR_MAX_WIDTH        1296
#define SENSOR_MAX_HEIGHT        742
#define SENSOR_WIN_HEIGHT        728
#define SENSOR_NON_EXP_LINE        1
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF


//normal setting
#define  DEFAULT_720P_MODE          MODE_720P
#define  DEFAULT_720P_VTS                 866//=VT(0x0d,0x0e)+VB+16(0x07,0x08)=728+16+122=866
#define  DEFAULT_720P_HTS                1039//(Hb(0x05,0x06)+Sh_dely(0x11,0x12)+win_width(0x0f,0x10)/2+4)=367+24+644+4=1039
#define  DEFAULT_720P_WIDTH              1280
#define  DEFAULT_720P_HEIGHT              720
#define  DEFAULT_720P_MIRROR                0
#define  DEFAULT_720P_FLIP                  0
#define  DEFAULT_720P_TROW                385//(Hb(0x05,0x06)+Sh_dely(0x11,0x12)+win_width(0x0f,0x10)/2+4)/(pclk/2)=(367+24+(1288/2)+4)/27=384
#define  DEFAULT_720P_PCLK           54000000//INCK=27M,PCLK=54Mhz ,INCK=24Mhz,PCLK=48Mhz
#define  DEFAULT_720P_MAXFPS               30
#define  DEFAULT_720P_LANE                  1
#define  DEFAULT_720P_RAWBIT            RAW10
#define  DEFAULT_720P_BAYER          BAYER_RG


static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_720P = 0,
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
} sensor_info_t;

static sensor_info_t g_sGC1024info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_720P_MODE,        
    DEFAULT_720P_VTS,
    DEFAULT_720P_HTS,
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
    SENSOR_LONG_EXP,
    }
};

typedef struct sensor_reg {
    u16 addr;
    u8  val;
} sensor_reg_t;

static sensor_reg_t sensor_720p_init_regs[] = {
	//////////////////////   SYS   //////////////////////
	{0xfe,0x80},//
	{0xfe,0x80},//
	{0xfe,0x80},//
	{0xf2,0x0f},//
	{0xf6,0x00},//
	{0xfc,0xc6},//
	{0xf7,0xb9},//
	{0xf8,0x03},//
	{0xf9,0x0e},//
	{0xfa,0x00},//
	{0xfe,0x00},//
	/////////   ANALOG & CISCTL   ////////////////
	{0x03,0x02},//
	{0x04,0xb5},//
	{0x05,0x01},//Hb
	{0x06,0x6f},//Hb
	{0x07,0x00},//Vb
	{0x08,0x7a},//Vb
	{0x0d,0x02},//win_height
	{0x0e,0xd8},//win_height
	{0x0f,0x05},//win_width
	{0x10,0x08},//win_width
	{0x11,0x00},//Sh_delay
	{0x12,0x18},//Sh_delay
	{0x16,0xc0},//
	{0x17,0x14},//
	{0x19,0x06},//
	{0x1b,0x4f},//
	{0x1c,0x11},//
	{0x1d,0x10},////exp<1frame 图像闪烁
	{0x1e,0xf8},////fc 左侧发紫
	{0x1f,0x38},//
	{0x20,0x81},//
	{0x21,0x2f},////6f//2f
	{0x22,0xc0},////c2 左侧发紫
	{0x23,0x82},////f2 左侧发紫
	{0x24,0x2f},//
	{0x25,0xd4},//
	{0x26,0xa8},//
	{0x29,0x3f},////54//3f
	{0x2a,0x00},//
	{0x2b,0x00},////00--powernoise  03---强光拖尾
	{0x2c,0xe0},////左右range不一致
	{0x2d,0x0a},//
	{0x2e,0x00},//
	{0x2f,0x16},////1f--横闪线
	{0x30,0x00},//
	{0x31,0x01},//
	{0x32,0x02},//
	{0x33,0x03},//
	{0x34,0x04},//
	{0x35,0x05},//
	{0x36,0x06},//
	{0x37,0x07},//
	{0x38,0x0f},//
	{0x39,0x17},//
	{0x3a,0x1f},//
	{0x3f,0x18},////关掉Vclamp 电压
	///////////////   ISP   ////////////////////// 
	{0xfe,0x00},//
	{0x8a,0x00},//
	{0x8c,0x02},//
	{0x8e,0x02},//
	{0x8f,0x15},//
	{0x90,0x01},//
	{0x94,0x02},//
	{0x95,0x02},//  Crop  720 h
	{0x96,0xd0},//  Crop  720 l
	{0x97,0x05},//  Crop 1280 h
	{0x98,0x00},//  Crop 1280 l
	/////////////////	 MIPI	/////////////////////
	{0xfe,0x03},//
	{0x01,0x00},//
	{0x02,0x00},//
	{0x03,0x00},//
	{0x06,0x00},//
	{0x10,0x00},//
	{0x15,0x00},//
	///////////////	 BLK	/////////////////////
	{0xfe,0x00},//
	{0x18,0x02},//
	{0x1a,0x11},//
	{0x40,0x23},////2b
	{0x5e,0x00},//
	{0x66,0x80},//
	/////////////// Dark SUN /////////////////////
	{0xfe,0x00},//
	{0xcc,0x25},//
	{0xce,0xf3},//
	{0x3f,0x08},//
	///////////////	 Gain	/////////////////////
	{0xfe,0x00},//
	{0xb0,0x50},//
	{0xb3,0x40},//
	{0xb4,0x40},//
	{0xb5,0x40},//
	{0xb6,0x00},//
	///////////////   pad enable   ///////////////
	{0xf2,0x0f},//
	{0xfe,0x00},//
};
#define NUM_OF_720P_INIT_REGS (sizeof(sensor_720p_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  analog_gain;
    u8  pre_gain_h;
    u8  pre_gain_l;
    u32 total_gain; // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00, 0x1, 0x00,  64 }, {0x00, 0x1, 0x08,  72 }, {0x00, 0x1, 0x10,  80 },
    {0x00, 0x1, 0x18,  88 }, {0x00, 0x1, 0x20,  96 }, {0x01, 0x1, 0x00, 105 },
    {0x00, 0x1, 0x30, 112 }, {0x02, 0x1, 0x00, 119 }, {0x02, 0x1, 0x00, 128 },
    {0x02, 0x1, 0x0D, 144 }, {0x02, 0x1, 0x15, 159 }, {0x02, 0x1, 0x1E, 176 },
    {0x02, 0x1, 0x27, 192 }, {0x03, 0x1, 0x04, 209 }, {0x04, 0x1, 0x00, 224 },
    {0x04, 0x1, 0x05, 241 }, {0x04, 0x1, 0x09, 256 }, {0x04, 0x1, 0x12, 287 },
    {0x04, 0x1, 0x1B, 318 }, {0x04, 0x1, 0x25, 353 }, {0x05, 0x1, 0x02, 384 },
    {0x05, 0x1, 0x07, 413 }, {0x06, 0x1, 0x03, 448 }, {0x06, 0x1, 0x08, 482 },
    {0x06, 0x1, 0x0D, 516 }, {0x06, 0x1, 0x16, 576 }, {0x06, 0x1, 0x20, 643 },
    {0x07, 0x1, 0x02, 706 }, {0x07, 0x1, 0x08, 770 }, {0x07, 0x1, 0x0E, 834 },
    {0x07, 0x1, 0x14, 899 }, {0x07, 0x1, 0x1A, 963 }, {0x08, 0x1, 0x01,1027 },
    {0x08, 0x1, 0x09,1153 }, {0x08, 0x1, 0x11,1280 }, {0x09, 0x1, 0x02,1412 },
    {0x09, 0x1, 0x08,1541 }, {0x09, 0x1, 0x0E,1669 }, {0x09, 0x1, 0x14,1798 },
    {0x09, 0x1, 0x1A,1926 }, {0x0A, 0x1, 0x03,2064 }, {0x0A, 0x1, 0x0B,2310 },
    {0x0A, 0x1, 0x13,2556 }, {0x0A, 0x1, 0x1C,2834 }, {0x0A, 0x1, 0x24,3080 },
    {0x0A, 0x1, 0x2C,3326 }, {0x0A, 0x1, 0x34,3573 }, {0x0A, 0x1, 0x3D,3850 },
    {0x0A, 0x2, 0x05,4096 },
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x78 >> 1)
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[1], tmp2[1];

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
    unsigned char   buf[2];

    buf[0]     = addr & 0xFF;
    buf[1]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    return sensor_i2c_transfer(&msgs, 1);
}


static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk= g_sGC1024info[pinfo->currentmode].pclk;
	
    //isp_info("GC1024 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sGC1024info[pinfo->currentmode].hts;
    pinfo->t_row=g_sGC1024info[pinfo->currentmode].t_row;
	
    g_psensor->min_exp = (pinfo->t_row + 500) / 1000;
	
	//isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);

}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 exp_lines, reg_h, reg_l;

    if (!g_psensor->curr_exp) {
        read_reg(0x03, &reg_h);//[12:8]
        read_reg(0x04, &reg_l);//[7:0]
        exp_lines = ((reg_h & 0x1F)<<8) |  (reg_l & 0xFF);

        g_psensor->curr_exp = (exp_lines * pinfo->t_row + 500) / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line;
    u32 vblank;
	
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;
	
    if (1>exp_line)
        exp_line = 1;

    if (exp_line > 8191)//2^13
        exp_line = 8191;

    if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) 
    {
        vblank=exp_line+SENSOR_NON_EXP_LINE- SENSOR_WIN_HEIGHT-16;
        write_reg(0x07, (vblank >> 8) & 0x1F);
        write_reg(0x08, (vblank) & 0xFF);
        pinfo->long_exp = 1;
    } 
    else 
    {
        if (pinfo->long_exp)
        {
            vblank = (g_sGC1024info[pinfo->currentmode].vts) - SENSOR_WIN_HEIGHT-16;        
            write_reg(0x07, (vblank >> 8) & 0x1F);
            write_reg(0x08, (vblank) & 0xFF);
            pinfo->long_exp = 0;
        }
    }


    //Update shutter
    write_reg(0x03, (exp_line >> 8) & 0x1F);//[12:8]    
    write_reg(0x04, (exp_line & 0xFF));//[7:0]

    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("curr_exp = %d, t_row=%d, exp_lines=%d\n",g_psensor->curr_exp, pinfo->t_row, exp_lines);

    return ret;

}


static void adjust_blk(int fps)
{

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;
    u32 vblank;

    max_fps = g_sGC1024info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sGC1024info[pinfo->currentmode].vts) * max_fps / fps;

    vblank = pinfo->vts - SENSOR_WIN_HEIGHT-16;

    write_reg(0x07, (vblank >> 8) & 0x1F);
    write_reg(0x08, (vblank) & 0xff);

    get_exp();
    set_exp(g_psensor->curr_exp);

    g_psensor->fps = fps;

    //isp_info("adjust_blk fps=%d, vts=%d, vblank=%d\n", fps, pinfo->vts,vblank);

}

static u32 get_gain(void)
{
    u32 reg;

    u32 Anlg = 0,  Pre_gain_h = 0, Pre_gain_l = 0, gain_total = 0;

    if (g_psensor->curr_gain == 0) {
        //Analog gain
        read_reg(0xb6, &reg);
        Anlg = reg;
        //Pre-gain
        read_reg(0xb1, &reg); //[3:0]
        Pre_gain_h = reg;
        read_reg(0xb2, &reg); //[7:2]
        Pre_gain_l = ((reg>>2) &0x1F);

        if(Anlg == 0){
            gain_total = 64*  1 * (Pre_gain_h*100+(Pre_gain_l*100/64))/100;
        }
        if(Anlg == 1){
            gain_total = 64*165 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }

        if(Anlg == 2){
            gain_total = 64*187 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 3){
            gain_total = 64*308 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 4){
            gain_total = 64*350 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 5){
            gain_total = 64*582 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 6){
            gain_total = 64*670 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 7){
            gain_total = 64*1070 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 8){
            gain_total = 64*1580 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 9){
            gain_total = 64*2140 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        if(Anlg == 0x0A){
            gain_total = 64*3080 * (Pre_gain_h*100+(Pre_gain_l*100/64))/10000;
        }
        g_psensor->curr_gain = gain_total;
        //printk("get_curr_gain = %d\n",g_psensor->curr_gain);
    }

    return g_psensor->curr_gain;

}

static int set_gain(u32 gain)
{

    int ret = 0;
    int i = 0;
    u8 analog_gain = 0, pre_gain_h = 0, pre_gain_l = 0;

    // search most suitable gain into gain table
    if (gain > gain_table[NUM_OF_GAINSET - 1].total_gain)
        gain = gain_table[NUM_OF_GAINSET - 1].total_gain;
    else if (gain < gain_table[0].total_gain)
        gain = gain_table[0].total_gain;
    else{
        for (i=0; i<NUM_OF_GAINSET; i++) {
            if (gain_table[i].total_gain > gain)
        	    break;
        }
    }

    analog_gain = gain_table[i-1].analog_gain;
    pre_gain_h = gain_table[i-1].pre_gain_h;
    pre_gain_l = gain_table[i-1].pre_gain_l;

    //Analog gain
    write_reg(0xb6, analog_gain);
    //Pre-gain
    write_reg(0xb1, (pre_gain_h &0xF)); //[3:0]
    write_reg(0xb2, ((pre_gain_l & 0x3F) << 2)); //[7:2]

    g_psensor->curr_gain = gain_table[i-1].total_gain;

    //printk("g_psensor->curr_gain = %d\n",g_psensor->curr_gain);

    return ret;

}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->mirror) {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_BG;
        else
            g_psensor->bayer_type = BAYER_GR;
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

    read_reg(0x17, &reg);
    return (reg & BIT0) ? 1 : 0;
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror = enable;
	
    read_reg(0x17, &reg);
    if (pinfo->flip)
        reg |= BIT1;
    else
        reg &=~BIT1;

    if (enable) 
        reg |= BIT0;
    else 
        reg &=~BIT0;
    
    write_reg(0x17, reg);
    update_bayer_type();

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x17, &reg);
    return (reg & BIT1) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->flip = enable;

    read_reg(0x17, &reg);
	
    if (pinfo->mirror)
        reg |= BIT0;
    else
        reg &=~BIT0;

    if (enable)
        reg |= BIT1;
    else 
        reg &=~BIT1;
	
    write_reg(0x17, reg);
    update_bayer_type();

    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    int ret = 0;

    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("%dx%d exceeds maximum resolution %dx%d!\n",
            width, height, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT);
        return -1;
    }

    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width  = g_sGC1024info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sGC1024info[pinfo->currentmode].img_h;

    g_psensor->img_win.width = width;
    g_psensor->img_win.height  = height;
    g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;;
    g_psensor->img_win.y  = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;;

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

    //only one sensor mode 720P
    pInitTable=sensor_720p_init_regs;
    InitTable_Num=NUM_OF_720P_INIT_REGS;

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
    if ((ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height)) < 0)
        return ret;

    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);

    calculate_t_row();
    adjust_blk(g_psensor->fps);

    if (ret == 0)
        pinfo->is_init = true;
	
    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current shutter_width and gain setting
    get_exp();
    get_gain();
    //isp_info("initial end ...\n");
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void gc1024_deconstruct(void)
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

static int gc1024_construct(u32 xclk, u16 width, u16 height)
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

    pinfo->currentmode = MODE_720P;
	
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = IF_PARALLEL;
    g_psensor->protocol = 0;
    g_psensor->num_of_lane = g_sGC1024info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sGC1024info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sGC1024info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
    g_psensor->inv_clk = inv_clk;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;
    g_psensor->max_exp = 5000; // 0.5 sec
    g_psensor->min_gain = gain_table[0].total_gain;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].total_gain;
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
	
    return 0;

construct_err:
    gc1024_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init gc1024_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp32X.ko(%x)!\n",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }


    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = gc1024_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit gc1024_exit(void)
{
    isp_unregister_sensor(g_psensor);
    gc1024_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(gc1024_init);
module_exit(gc1024_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor GC1024");
MODULE_LICENSE("GPL");
