
/**
 * @file sen_soif02.c
 * OmniVision soif02 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
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

static uint interface = IF_PARALLEL;
module_param(interface, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint inv_clk = 1;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static ushort ch_num = 2;
module_param(ch_num, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_num, "channel number");


//=============================================================================
// global
//============================================================================
#define SENSOR_NAME         "SOIF02"
#define SENSOR_MAX_WIDTH    1920
#define SENSOR_MAX_HEIGHT   1080


static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    u32 HMax;
    u32 VMax;
    u32 t_row;          // unit: 1/10 us
    int mirror;
    int flip;
    u32 sys_reg;
} sensor_info_t;

typedef struct sensor_reg {
    u8 addr;
    u8 val;
} sensor_reg_t;

typedef enum
{
    SOIF02_AGAIN_REG        = 0x00,
    SOIF02_EXPOSURE_L_REG   = 0x01,
    SOIF02_EXPOSURE_H_REG   = 0x02,
    SOIF02_VERSION_REG      = 0x09,
    SOIF02_PID_H_REG        = 0x0A,
    SOIF02_PID_L_REG        = 0x0B,
    SOIF02_DGAIN_REG        = 0x0D,
    SOIF02_PLL1_REG         = 0x0E,
    SOIF02_PLL2_REG         = 0x0F,
    SOIF02_PLL3_REG         = 0x10,
    SOIF02_CLK_REG          = 0x11,
    SOIF02_SYS_REG          = 0x12,
    SOIF02_SIGNAL_POL_REG   = 0x1E,
    SOIF02_GROUP_LATCH_REG  = 0x1F,
    SOIF02_FRAMEW_L_REG     = 0x20,
    SOIF02_FRAMEW_H_REG     = 0x21,
    SOIF02_FRAMEH_L_REG     = 0x22,
    SOIF02_FRAMEH_H_REG     = 0x23,
    SOIF02_IMGW_OUT_L_REG   = 0x24,    // width
    SOIF02_IMGH_OUT_L_REG   = 0x25,    // height
    SOIF02_IMG_OUT_H_REG    = 0x26,    // [0:3]: width / [4:7]: height
    SOIF02_IMGW_START_L_REG = 0x27,    // width
    SOIF02_IMGH_START_L_REG = 0x28,    // height
    SOIF02_IMG_START_H_REG  = 0x29,    // [0:3]: width / [4:7]: height
    SOIF02_VSYNC_POS_L_REG  = 0x43,
    SOIF02_VSYNC_POS_H_REG  = 0x44,    // [0:3]
    SOIF02_LBLC_CTL_REG     = 0x47,
    SOIF02_BLC_TARGET_REG   = 0x49,
    SOIF02_BLC_CTL_REG      = 0x4A,
    SOIF02_UNKNOWN_REG      = 0x100
} SOIF02_REG;

#define DELAY_CODE    0xFF

static const sensor_reg_t sensor_init_regs[] = {   
    // INI start
    {0x12, 0x40},
    // DVP setting
    {0x0D, 0xA4},
    // PLL setting
    {0x0E, 0x10},    // pre_divider=1
    {0x0F, 0x09},    // PLL_CLK=VCO/(1+9)
    {0x10, 0x1F},    // VCO=27MHz*0x1F=837MHz
    {0x11, 0x80},
    // frame/window
    {0x20, 0x58},
    {0x21, 0x02},
    {0x22, 0x8C},
    {0x23, 0x04},
    {0x24, 0xE0},
    {0x25, 0x38},
    {0x26, 0x41},
    {0x27, 0xC8},
    {0x28, 0x12},
    {0x29, 0x00},
    {0x2A, 0xC1},
    {0x2B, 0x18},
    {0x2C, 0x02},
    {0x2D, 0x00},
    {0x2E, 0x14},
    {0x2F, 0x44},
    // sensor timing
    {0x30, 0x8C},
    {0x31, 0x06},
    {0x32, 0x10},
    {0x33, 0x0C},
    {0x34, 0x66},
    {0x35, 0xD1},
    {0x36, 0x0E},
    {0x37, 0x34},
    {0x38, 0x14},
    {0x39, 0x82},
    {0x3A, 0x08},
    // interface
    {0x1D, 0xFF},
    {0x1E, 0x1F},
    {0x6C, 0x90},
    {0x70, 0xE1},
    {0x71, 0x4A},
    {0x72, 0x8A},
    {0x73, 0x53},
    {0x74, 0x52},
    {0x75, 0x2B},
    {0x76, 0x60},
    {0x77, 0x09},
    // array/AnADC/PWC
    {0x41, 0xCD},
    {0x42, 0x23},
    {0x67, 0x70},
    {0x68, 0x02},
    {0x69, 0x74},
    {0x6A, 0x4A},
    // SRAM/RAMP
    {0x5A, 0x04},
    {0x5C, 0x8F},
    {0x5D, 0xB0},
    {0x5E, 0xE6},
    {0x60, 0x16},
    {0x62, 0x30},
    {0x64, 0x58},
    {0x65, 0x00},
    // AE/AG/ABLC
    {0x47, 0x80},
    {0x50, 0x02},
    {0x13, 0x87},
    {0x14, 0x80},
    {0x16, 0xC0},
    {0x17, 0x40},
    {0x18, 0x22},
    {0x19, 0x81},
    {0x4A, 0x03},
    {0x49, 0x10},
    // INI end
    {0x12, 0x00},
    // auto Vramp
    {0x45, 0x19},
    {0x1F, 0x01},
    // PWDN setting
};

#define NUM_OF_INIT_REGS    (sizeof(sensor_init_regs) / sizeof(sensor_reg_t))

static const sensor_reg_t sensor_mipi_init_regs[] = {   
    {0x12, 0x40},
    {0x0D, 0xA4},
    {0x0E, 0x10},
    {0x0F, 0x19},
    {0x10, 0x1e},
    {0x11, 0x80},
    {0x20, 0x58},
    {0x21, 0x02},
    {0x22, 0x66},
    {0x23, 0x04},
    {0x24, 0xE0},
    {0x25, 0x38},
    {0x26, 0x41},
    {0x27, 0xC7},
    {0x28, 0x10},
    {0x29, 0x00},
    {0x2A, 0xC1},
    {0x2B, 0x38},
    {0x2C, 0x04},
    {0x2D, 0x01},
    {0x2E, 0x13},
    {0x2F, 0x44},
    {0x30, 0x8C},
    {0x31, 0x06},
    {0x32, 0x10},
    {0x33, 0x0C},
    {0x34, 0x66},
    {0x35, 0xD1},
    {0x36, 0x0E},
    {0x37, 0x34},
    {0x38, 0x14},
    {0x39, 0x82},
    {0x3A, 0x08},
    {0x3B, 0x00},
    {0x3C, 0xFF},
    {0x3D, 0x90},
    {0x3E, 0xA0},
    {0x1D, 0x00},
    {0x1E, 0x00},
    {0x6C, 0x40},
    {0x70, 0x61},
    {0x71, 0x4A},
    {0x72, 0x4A},
    {0x73, 0x33},
    {0x74, 0x52},
    {0x75, 0x2B},
    {0x76, 0x60},
    {0x77, 0x09},
    {0x78, 0x0B},
    {0x41, 0xCD},
    {0x42, 0x23},
    {0x67, 0x70},
    {0x68, 0x02},
    {0x69, 0x74},
    {0x6A, 0x4A},
    {0x5A, 0x04},
    {0x5C, 0x8f},
    {0x5D, 0xB0},
    {0x5E, 0xe6},
    {0x60, 0x16},
    {0x62, 0x30},
    {0x63, 0x80},
    {0x64, 0x58},
    {0x65, 0x00},
    {0x12, 0x00},
    {0x47, 0x80},
    {0x50, 0x02},
    {0x13, 0x85},
    {0x14, 0x80},
    {0x16, 0xC0},
    {0x17, 0x40},
    {0x18, 0x19},
    {0x19, 0x81},
    {0x4a, 0x03},
    {0x49, 0x10},
    {0x45, 0x19},
    {0x1F, 0x01},
};

#define NUM_OF_MIPI_INIT_REGS    (sizeof(sensor_mipi_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  ag_reg;
    u32 gain_x;
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00,   64}, {0x01,   68}, {0x02,   72}, {0x03,   76},
    {0x04,   80}, {0x05,   84}, {0x06,   88}, {0x07,   92},
    {0x08,   96}, {0x09,  100}, {0x0A,  104}, {0x0B,  108},
    {0x0C,  112}, {0x0D,  116}, {0x0E,  120}, {0x0F,  124},
    {0x10,  128}, {0x11,  136}, {0x12,  144}, {0x13,  152},
    {0x14,  160}, {0x15,  168}, {0x16,  176}, {0x17,  184},
    {0x18,  192}, {0x19,  200}, {0x1A,  208}, {0x1B,  216},
    {0x1C,  224}, {0x1D,  232}, {0x1E,  240}, {0x1F,  248},
    {0x20,  256}, {0x21,  272}, {0x22,  288}, {0x23,  304},
    {0x24,  320}, {0x25,  336}, {0x26,  352}, {0x27,  368},
    {0x28,  384}, {0x29,  400}, {0x2A,  416}, {0x2B,  432},
    {0x2C,  448}, {0x2D,  464}, {0x2E,  480}, {0x2F,  496},
    {0x30,  512}, {0x31,  544}, {0x32,  576}, {0x33,  608},
    {0x34,  640}, {0x35,  672}, {0x36,  704}, {0x37,  736},
    {0x38,  768}, {0x39,  800}, {0x3A,  832}, {0x3B,  864},
    {0x3C,  896}, {0x3D,  928}, {0x3E,  960}, {0x3F,  992},
#if 0    // gain 16X~32X
    {0x40, 1024}, {0x41, 1088}, {0x42, 1152}, {0x43, 1216},
    {0x44, 1280}, {0x45, 1344}, {0x46, 1408}, {0x47, 1472},
    {0x48, 1536}, {0x49, 1600}, {0x4A, 1664}, {0x4B, 1728},
    {0x4C, 1792}, {0x4D, 1856}, {0x4E, 1920}, {0x4F, 1984}
#endif
};

#define NUM_OF_GAINSET    (sizeof(gain_table) / sizeof(gain_set_t))


//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x80 >> 1)
#include "i2c_common.c"


//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg msgs[2];
    unsigned char  tmp[1], tmp2[2];

    tmp[0]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;    // write
    msgs[0].len   = 1;
    msgs[0].buf   = tmp;

    tmp2[0]       = 0;    // clear
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1;    // read
    msgs[1].len   = 1;
    msgs[1].buf   = tmp2;

    if (sensor_i2c_transfer(msgs, 2)) {
        return -1;
    }

    *pval = tmp2[0];

    return 0;
}

static int write_reg(u32 addr, u32 val)
{
    struct i2c_msg msgs;
    unsigned char buf[3];

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
    u32 reg_l, reg_h;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    fps = CLAMP(fps, 2, 30);

    pinfo->VMax = g_psensor->pclk / fps / pinfo->HMax;
    reg_h = (pinfo->VMax >> 8) & 0xFF;
    reg_l = pinfo->VMax & 0xFF;
    write_reg(SOIF02_FRAMEH_H_REG, reg_h);
    write_reg(SOIF02_FRAMEH_L_REG, reg_l);

    g_psensor->fps = fps;
}

static u32 get_pclk(u32 xclk)
{
    u32 pix_clk, pll_reg[3] = {0};

    read_reg(SOIF02_PLL1_REG, &(pll_reg[0]));    // pre_divider [1:0]
    pll_reg[0] &= 0x03;
    pll_reg[0] += 1;

    read_reg(SOIF02_PLL2_REG, &(pll_reg[1]));    // clock divider
    pll_reg[1] &= 0x0F;
    pll_reg[1] += 1;

    read_reg(SOIF02_PLL3_REG, &(pll_reg[2]));    // VCO

    pix_clk = xclk * pll_reg[2] / pll_reg[0] / pll_reg[1];

    printk("pixel clock %d\n", pix_clk);

    return pix_clk;
}

static int set_size(u32 width, u32 height)
{
    int ret = 0;

    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("%dx%d exceeds maximum resolution %dx%d!\n", width, height, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT);
        return -EINVAL;
    }

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = SENSOR_MAX_WIDTH;
    g_psensor->out_win.height = SENSOR_MAX_HEIGHT;
    g_psensor->img_win.x = ((SENSOR_MAX_WIDTH - width) / 2) & ~BIT0;
    g_psensor->img_win.y = ((SENSOR_MAX_HEIGHT - height) / 2) & ~BIT0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;

    return ret;
}

static u32 get_exp(void)
{
    u32 lines, exp_h, exp_l;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (g_psensor->curr_exp == 0) {
        read_reg(SOIF02_EXPOSURE_H_REG, &exp_h);
        read_reg(SOIF02_EXPOSURE_L_REG, &exp_l);
        lines = (exp_h << 8) | exp_l;
        g_psensor->curr_exp = (lines * pinfo->t_row + 1000 / 2) / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    u32 exp_lines, tmp, max_exp_lines;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    exp = CLAMP(exp, g_psensor->min_exp, g_psensor->max_exp);

    exp_lines = ((1000 * exp) + pinfo->t_row / 2) / pinfo->t_row;
    exp_lines = MAX(1, exp_lines);

    max_exp_lines = pinfo->VMax - 5;    // hardware limitation

    tmp = (exp_lines > max_exp_lines) ? (exp_lines + 5) : (max_exp_lines + 5);

    write_reg(SOIF02_FRAMEH_H_REG, (tmp >> 8) & 0xFF);
    write_reg(SOIF02_FRAMEH_L_REG, (tmp & 0xFF));
    write_reg(SOIF02_EXPOSURE_L_REG, (exp_lines & 0xFF));
    write_reg(SOIF02_EXPOSURE_H_REG, ((exp_lines >> 8) & 0xFF));

    g_psensor->curr_exp = ((exp_lines * pinfo->t_row) + 1000 / 2) / 1000;

    //isp_dbg("g_psensor->curr_exp=%d, max_exp_lines=%d, exp_lines=%d, tmp=%d\n", g_psensor->curr_exp, max_exp_lines, exp_lines, tmp);

    return 0;
}

static u32 get_gain(void)
{
    u32 gain_total, ag_reg, dg_reg, mply_gain = 1;

     if (g_psensor->curr_gain == 0) {
        read_reg(SOIF02_AGAIN_REG, &ag_reg);
        read_reg(SOIF02_DGAIN_REG, &dg_reg);
        dg_reg &= 0x03;       // [1:0]
        gain_total = 16 + (ag_reg & 0x0F);    // 16 means 1X
        gain_total = ((ag_reg & BIT4) != 0) ? (2 * gain_total) : gain_total;
        gain_total = ((ag_reg & BIT5) != 0) ? (4 * gain_total) : gain_total;
        gain_total = ((ag_reg & BIT6) != 0) ? (8 * gain_total) : gain_total;
        gain_total *= 4;

        if (dg_reg == 0) {
            mply_gain = 1;    // 1X
        }
        else if (dg_reg == 3) {
            mply_gain = 4;    // 4X
        }
        else {
            mply_gain = 2;    // 2X
        }

        g_psensor->curr_gain = gain_total = gain_total * mply_gain;

        //printk("ag_reg=%d, dg_reg=%d, mply_gain=%d, get_curr_gain=%d\n", ag_reg, dg_reg, mply_gain, g_psensor->curr_gain);
    }

    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    int i, ret = 0;
    u32 a_gain;

    // search most suitable gain into gain table
    if (gain >= gain_table[NUM_OF_GAINSET-1].gain_x){
        gain = gain_table[NUM_OF_GAINSET-1].gain_x;
        i = NUM_OF_GAINSET;
    }
    else if (gain < gain_table[0].gain_x){
        gain = gain_table[0].gain_x;
        i = 1;
    }
	else{
        for (i = 0; i < NUM_OF_GAINSET; i++) {
            if (gain_table[i].gain_x > gain) {
                break;
            }
        }
    }

    a_gain = (i > 0) ? gain_table[i-1].ag_reg : gain_table[0].ag_reg;

    write_reg(SOIF02_AGAIN_REG, a_gain);

    g_psensor->curr_gain = (i > 0) ? gain_table[i-1].gain_x : gain_table[0].gain_x;

    return ret;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->mirror) {
        g_psensor->bayer_type = (pinfo->flip != 0) ? BAYER_RG : BAYER_GB;
    } else {
        g_psensor->bayer_type = (pinfo->flip != 0) ? BAYER_GR : BAYER_BG;
    }
}

static int get_mirror(void)
{
    u32 reg;

    read_reg(SOIF02_SYS_REG, &reg);

    return (reg & BIT5) ? 1 : 0;
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (enable) {
        pinfo->sys_reg |= BIT5;
    }
    else {
        pinfo->sys_reg &= ~BIT5;
    }

    write_reg(SOIF02_SYS_REG, pinfo->sys_reg);
    pinfo->mirror = enable;
    update_bayer_type();

    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(SOIF02_SYS_REG, &reg);

    return (reg & BIT4) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (enable) {
        pinfo->sys_reg |= BIT4;
    }
    else {
        pinfo->sys_reg &=~BIT4;
    }

    write_reg(SOIF02_SYS_REG, pinfo->sys_reg);
    pinfo->flip = enable;
    update_bayer_type();

    return 0;
}

static int set_fps(int fps)
{
    adjust_vblk(fps);

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
    u32 reg_h, reg_l;
    int i, ret = 0;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    const sensor_reg_t* pInitTable;
    u8 InitTable_Num;
	
    if (pinfo->is_init == true) {
        return 0;
    }

    if(IF_PARALLEL==g_psensor->interface){
        pInitTable=sensor_init_regs;
        InitTable_Num=NUM_OF_INIT_REGS;		
    }else{
        pInitTable=sensor_mipi_init_regs;
        InitTable_Num=NUM_OF_MIPI_INIT_REGS;	
    }	

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
        mdelay(1);
        //printk("[%d]%x=%x\n", i, sensor_init_regs[i].addr, sensor_init_regs[i].val);		
    }

    read_reg(SOIF02_FRAMEH_H_REG, &reg_h);
    read_reg(SOIF02_FRAMEH_L_REG, &reg_l);
    pinfo->VMax = (reg_h << 8) | reg_l;
    g_psensor->pclk = get_pclk(g_psensor->xclk);

    read_reg(SOIF02_FRAMEW_H_REG, &reg_h);
    read_reg(SOIF02_FRAMEW_L_REG, &reg_l);
    pinfo->HMax = (reg_h << 8) | reg_l;
    pinfo->HMax *= 4;
    pinfo->t_row = (pinfo->HMax * 10000) / (g_psensor->pclk / 1000);

    g_psensor->min_exp = MAX(1, (pinfo->t_row + 1000 / 2) / 1000);

    //printk("pinfo->VMax=%d, pinfo->HMax=%d, pinfo->t_row=%d, g_psensor->min_exp=%d\n", pinfo->VMax, pinfo->HMax, pinfo->t_row, g_psensor->min_exp);

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);
    if (ret < 0) {
        return ret;
    }

    adjust_vblk(g_psensor->fps);

    // set mirror and flip
    set_mirror(mirror);
    set_flip(flip);

    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current shutter_width and gain setting
    get_exp();
    get_gain();

    pinfo->is_init = true;

    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void soif02_deconstruct(void)
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

static int soif02_construct(u32 xclk, u16 width, u16 height)
{
    sensor_info_t *pinfo = NULL;
    int ret = 0;

    // initial CAP_RST
    ret = isp_api_cap_rst_init();
    if (ret < 0) {
        return ret;
    }

    // clear CAP_RST
    isp_api_cap_rst_set_value(0);

    // set EXT_CLK frequency and turn on it.
    ret = isp_api_extclk_set_freq(xclk);
    if (ret < 0) {
        return ret;
    }

    ret = isp_api_extclk_onoff(1);
    if (ret < 0) {
        return ret;
    }

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
    g_psensor->num_of_lane = ch_num;
    g_psensor->protocol = 0;
    g_psensor->fmt = RAW10;
    g_psensor->bayer_type = BAYER_BG;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->inv_clk = inv_clk;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;
    g_psensor->max_exp = 5000;    // 0.5 sec
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET-1].gain_x;
    g_psensor->exp_latency = 0;
    g_psensor->gain_latency = 1;
    g_psensor->fps = fps;
    g_psensor->fn_init         = init;
    g_psensor->fn_read_reg     = read_reg;
    g_psensor->fn_write_reg    = write_reg;
    g_psensor->fn_set_size     = set_size;
    g_psensor->fn_get_exp      = get_exp;
    g_psensor->fn_set_exp      = set_exp;
    g_psensor->fn_get_gain     = get_gain;
    g_psensor->fn_set_gain     = set_gain;
    g_psensor->fn_get_property = get_property;
    g_psensor->fn_set_property = set_property;

    ret = sensor_init_i2c_driver();
    if (ret < 0) {
        goto construct_err;
    }

    return 0;

construct_err:
    soif02_deconstruct();

    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init soif02_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp32x.ko(%x)!\n", ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (g_psensor == NULL) {
        ret = -ENODEV;
        goto init_err1;
    }

    ret = soif02_construct(sensor_xclk, sensor_w, sensor_h);
    if (ret < 0) {
        printk("soif02_construct Error!\n");
        goto init_err2;
    }

    // register sensor device to ISP driver
    ret = isp_register_sensor(g_psensor);
    if (ret < 0) {
        printk("isp_register_sensor Error!\n");
        goto init_err2;
    }

    return ret;

init_err2:
    kfree(g_psensor);

init_err1:
    return ret;
}

static void __exit soif02_exit(void)
{
    isp_unregister_sensor(g_psensor);

    soif02_deconstruct();

    if (g_psensor != NULL) {
        kfree(g_psensor);
    }
}

module_init(soif02_init);
module_exit(soif02_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor Soif02");
MODULE_LICENSE("GPL");

