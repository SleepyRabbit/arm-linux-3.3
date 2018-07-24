/**
 * @file sen_sc1045.c
 * Panasonic SC1045 sensor driver
 *
 * Copyright (C) 2014 GM Corp. (http://www.grain-media.com)
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

static uint binning = 0;
module_param(binning, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(binning, "sensor binning");

static uint fps = 30;
module_param(fps, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "sensor frame rate");

static uint inv_clk = 1;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static uint wdr = 0;
module_param(wdr, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wdr, "WDR mode");

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "SC1045"
#define SENSOR_MAX_WIDTH    1280
#define SENSOR_MAX_HEIGHT   720
#define ID_SC1045          0x20

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
    u32 ang_reg_1;
    u32 dcg_reg;
    int VMax;
} sensor_info_t;

typedef struct sensor_reg {
    u16  addr;
    u16 val;
} sensor_reg_t;

#define DELAY_CODE      0xFFFF

static sensor_reg_t sc1045_init_regs[] = {
    {0x3003,0x01}, //soft reset
    {0x3000,0x00}, //pause for reg writing
    {0x3010,0x01}, //output format 43.2M 1920X750 30fps
    {0x3011,0xc6},
    {0x3004,0x45},
    {0x3e03,0x03}, //exp and gain
    {0x3600,0x94}, //0607 reduce power 
    {0x3610,0x03},
    {0x3634,0x00}, // reduce power
    {0x3620,0x84},
    {0x3631,0x84}, // txvdd 0825
    {0x3622,0x0e}, 
    {0x3633,0x2c}, //0902
    {0x3630,0x88}, 
    {0x3635,0x80}, 
    {0X3310,0X83}, //prechg tx auto ctrl [5] 0902
    {0x3336,0x00},
    {0x3337,0x00},
    {0x3338,0x02},
    {0x3339,0xee},
    {0X331E,0xa0}, //    
    {0x3335,0x10},    
    {0X3315,0X44},
    {0X3308,0X40},
    {0x3330,0x0d}, // sal_en timing,cancel the fpn in low light
    {0x3320,0x05}, //0825
    {0x3321,0x60},
    {0x3322,0x02},
    {0x3323,0xc0},
    {0x3303,0xa0},
    {0x3304,0x60},
    {0x3d04,0x04}, //0806    vsync gen mode
    {0x3d08,0x02},
    {0x3000,0x01}, //recover 
};
#define NUM_OF_SC1045_INIT_REGS (sizeof(sc1045_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    unsigned hcg;
    unsigned char analog;
    int gain_x;
} gain_set_t;

static gain_set_t gain_table[] =
{
#if 1
    //analog gain first
    {0,   0,   64},
    {0,   1,   68},
    {0,   2,   72},
    {0,   3,   76},
    {0,   4,   79},
    {0,   5,   83},
    {0,   6,   86},
    {0,   7,   90},
    {0,   8,   94},
    {0,   9,   98},
    {0,  10,  102},
    {0,  11,  106},
    {0,  12,  109},
    {0,  13,  113},
    {0,  14,  116},
    {0,  15,  120},
    {0,  16,  128},
    {0,  17,  136},
    {0,  18,  143},
    {0,  19,  151},
    {0,  20,  159},
    {0,  21,  165},
    {0,  22,  173},
    {0,  23,  180},
    {0,  24,  188},
    {0,  25,  196},
    {0,  26,  204},
    {0,  27,  211},
    {0,  28,  219},
    {0,  29,  225},
    {0,  30,  233},
    {0,  31,  241},
    {0,  48,  256},
    {0,  49,  271},
    {0,  50,  287},
    {0,  51,  302},
    {0,  52,  317},
    {0,  53,  330},
    {0,  54,  346},
    {0,  55,  361},
    {0,  56,  376},
    {0,  57,  392},
    {0,  58,  407},
    {0,  59,  422},
    {0,  60,  438},
    {0,  61,  451},
    {0,  62,  466},
    {0,  63,  481},
    {0, 112,  512},
    {0, 113,  543},
    {0, 114,  573},
    {0, 115,  604},
    {0, 116,  635},
    {0, 117,  660},
    {0, 118,  691},
    {0, 119,  722},
    {0, 120,  753},
    {0, 121,  783},
    {0, 122,  814},
    {0, 123,  845},
    {0, 124,  876},
    {0, 125,  901},
    {0, 126,  932},
    {0, 127,  963},
    {1, 112, 1024},
    {1, 113, 1086},
    {1, 114, 1146},
    {1, 115, 1208},
    {1, 116, 1270},
    {1, 117, 1320},
    {1, 118, 1382},
    {1, 119, 1444},
    {1, 120, 1506},
    {1, 121, 1566},
    {1, 122, 1628},
    {1, 123, 1690},
    {1, 124, 1752},
    {1, 125, 1802},
    {1, 126, 1864},
    {1, 127, 1926},
#endif
#if 0
    {  0,   64},
    {  1,   68},
    {  2,   72},
    {  3,   76},
    {  4,   79},
    {  5,   83},
    {  6,   86},
    {  7,   90},
    {  8,   94},
    {  9,   98},
    { 10,  102},
    { 11,  106},
    { 12,  109},
    { 13,  113},
    { 14,  116},
    { 15,  120},
    { 16,  128},
    { 17,  136},
    { 18,  143},
    { 19,  151},
    { 20,  159},
    { 21,  165},
    { 22,  173},
    { 23,  180},
    { 24,  188},
    { 25,  196},
    { 26,  204},
    { 27,  211},
    { 28,  219},
    { 29,  225},
    { 30,  233},
    { 31,  241},
    { 48,  256},
    { 49,  271},
    { 50,  287},
    { 51,  302},
    { 52,  317},
    { 53,  330},
    { 54,  346},
    { 55,  361},
    { 56,  376},
    { 57,  392},
    { 58,  407},
    { 59,  422},
    { 60,  438},
    { 61,  451},
    { 62,  466},
    { 63,  481},
    {112,  512},
    {113,  543},
    {114,  573},
    {115,  604},
    {116,  635},
    {117,  660},
    {118,  691},
    {119,  722},
    {120,  753},
    {121,  783},
    {122,  814},
    {123,  845},
    {124,  876},
    {125,  901},
    {126,  932},
    {127,  963},
    {112, 1024},
    {113, 1086},
    {114, 1146},
    {115, 1208},
    {116, 1270},
    {117, 1320},
    {118, 1382},
    {119, 1444},
    {120, 1506},
    {121, 1566},
    {122, 1628},
    {123, 1690},
    {124, 1752},
    {125, 1802},
    {126, 1864},
    {127, 1926},
#endif
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x60 >> 1)
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[2], tmp2[1];

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


static u32 get_pclk(u32 xclk)
{    
    u32 pclk;
    
    pclk = 43200000;
    isp_info("pclk=%d\n", pclk);
    
    return pclk;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 x_start, x_end, y_start, y_end;
    int sen_width, sen_height, line_w, col_h;
    u32 low, high;
    
    // check size
    if ((width * (1 + binning) > SENSOR_MAX_WIDTH) || (height * (1 + binning) > SENSOR_MAX_HEIGHT)) {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }

    x_start = (0 + (SENSOR_MAX_WIDTH - width)/2) & ~BIT0;
    y_start = (2 + (SENSOR_MAX_HEIGHT - height)/2) & ~BIT0;
    x_end = (width - 1 + x_start + 4) | BIT0;
    y_end = (height - 1 + y_start + 4) | BIT0;

    read_reg(0x3208, &high);
    read_reg(0x3209, &low);
    sen_width = high<<8 | low;

    read_reg(0x320A, &high);
    read_reg(0x320B, &low);
    sen_height = high<<8 | low;
   
    read_reg(0x320C, &high); 
    read_reg(0x320D, &low);
    line_w = high<<8 | low;

    read_reg(0x320E, &high);
    read_reg(0x320F, &low);
    col_h = high<<8 | low;

    pinfo->VMax = col_h;
    // update sensor information
    g_psensor->out_win.x = x_start;
    g_psensor->out_win.y = y_start;
    g_psensor->out_win.width = sen_width;//(x_end - x_start + 1);
    g_psensor->out_win.height = sen_height;//(y_end - y_start + 1);

    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width ;
    g_psensor->img_win.height = height;

    return ret;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 num_of_line_pclk;
    u32 low, high;

    if (!g_psensor->curr_exp) {
        read_reg(0x3E02, &low);
        read_reg(0x3E01, &high);
		num_of_line_pclk = high << 4 || (low>>4);
        g_psensor->curr_exp = num_of_line_pclk * pinfo->t_row / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line;
    int frame_len_lines, reg_h, reg_l, exp1;

    exp_line = MAX(1, exp * 1000 / pinfo->t_row);

    if(exp_line > pinfo->VMax - 4){
        write_reg(0x320E, (exp_line >> 8) & 0xFF);
        write_reg(0x320F, exp_line & 0xFF);
        write_reg(0x3338, (exp_line >> 8) & 0xFF);//Update exp2 the same {0x320e,0x320f}
        write_reg(0x3339, exp_line & 0xFF);
        exp1 = exp_line - 0x2EE;
        write_reg(0x3336, ((exp1 >> 8) & 0xFF));//Update exp1 the same {0x320e,0x320f}
        write_reg(0x3337, (exp1 & 0xFF));
    }
    else{
        read_reg(0x320E, &reg_h);
        read_reg(0x320F, &reg_l);
        frame_len_lines = ((reg_h << 8) | reg_l);
        if(frame_len_lines != pinfo->VMax){
            write_reg(0x320E, (pinfo->VMax >> 8) & 0xFF);
            write_reg(0x320F, pinfo->VMax & 0xFF);
            write_reg(0x3338, (pinfo->VMax >> 8) & 0xFF);//Update exp2 the same {0x320e,0x320f}
            write_reg(0x3339, pinfo->VMax & 0xFF);
            exp1 = frame_len_lines - 0x2EE;
            write_reg(0x3336, ((exp1 >> 8) & 0xFF));//Update exp1 the same {0x320e,0x320f}
            write_reg(0x3337, (exp1 & 0xFF));            
        }
    }
    //Exp[11:0] = 0x3E01[7:0], 0x3E02[7:4];
    write_reg(0x3E01, ((exp_line-2) >> 4) & 0xFF);
    write_reg(0x3E02, ((exp_line-2) << 4 )& 0xFF);    

    g_psensor->curr_exp = MAX(1, (exp_line * pinfo->t_row + 500)/ 1000);

    return ret;
}

static u32 get_gain(void)
{
    u32 gain;
    u32 fine_gain, corse_gain, bit6_4;
    u32 hcg, hcg_gain;
    if (!g_psensor->curr_gain) {
        read_reg(0x3e09, &gain);
        read_reg(0x3610, &hcg);
        fine_gain = 64 * (34 + 2 * (gain & 0x0F)) / 34;
        bit6_4 = (gain >> 4) & 0x07;
        
        if(bit6_4 & BIT0)
            corse_gain = 2;
        else if(bit6_4 & BIT1)
            corse_gain = 4;
        else if(bit6_4 & BIT2)
            corse_gain = 8;
        else
            corse_gain = 1;

        if(hcg == 0x1B)
            hcg_gain = 2;
        else
            hcg_gain = 1;

        gain = fine_gain * corse_gain * hcg_gain;

        g_psensor->curr_gain= gain;
        
    }
    return g_psensor->curr_gain;

}

static int set_gain(u32 gain)
{
    int ret = 0;
    int i;
    static u32 pre_hcg=0;
    u32 hcg;

    if (gain > gain_table[NUM_OF_GAINSET - 1].gain_x)
        gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    else if(gain < gain_table[0].gain_x)
        gain = gain_table[0].gain_x;

    // search most suitable gain into gain table
    for (i=0; i<NUM_OF_GAINSET; i++) {
        if (gain_table[i].gain_x > gain)
            break;
    }

    if (i > 0) i--;

    write_reg(0x3e09, gain_table[i].analog);   

    hcg = gain_table[i].hcg;

    if (pre_hcg != hcg){
        pre_hcg = hcg;
        isp_api_wait_frame_end();
	    isp_api_wait_frame_end();
        if(hcg == 1){
            write_reg(0x3610, 0x1B); // 2x analog gain enable.
            //isp_info("analog gain enable\n");
        }
        else{
            write_reg(0x3610, 0x03); // 2x analog gain disable
            //isp_info("analog gain disable\n");
        }
    }
        
    g_psensor->curr_gain = gain_table[i].gain_x;
    //isp_info("g_psensor->curr_gain = %d\n", g_psensor->curr_gain);
    
    return ret;

}

static void adjust_vblk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;
    int line_len_pclk, frame_len_lines, EXP1;

    if (fps > 30)
        fps = 30;
    
    read_reg(0x320C, &reg_h);
    read_reg(0x320D, &reg_l);
    line_len_pclk = ((reg_h << 8) | reg_l);
    pinfo->t_row = (line_len_pclk * 10000) / (g_psensor->pclk / 1000);

    frame_len_lines = g_psensor->pclk / fps / line_len_pclk;   
    EXP1 = frame_len_lines - 0x2EE;
    reg_h = (frame_len_lines >> 8) & 0xFF;
    reg_l = frame_len_lines & 0xFF;
    pinfo->VMax = frame_len_lines;
    isp_info("t_row=%d, VMax=%d\n", pinfo->t_row, pinfo->VMax);
    write_reg(0x320E, reg_h);
    write_reg(0x320F, reg_l);
    write_reg(0x3338, reg_h);//Update exp2 the same {0x320e,0x320f}
    write_reg(0x3339, reg_l);

    reg_h = (EXP1 >> 8) & 0xFF;
    reg_l = EXP1 & 0xFF;
    write_reg(0x3336, reg_h);//Update exp1 the same {0x320e,0x320f}
    write_reg(0x3337, reg_l);    

    get_exp();
    set_exp(g_psensor->curr_exp);

    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;

    g_psensor->fps = fps;

}



static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->mirror) {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GR;		//BAYER_RG; color pattern ok
        else
            g_psensor->bayer_type = BAYER_BG;		//BAYER_RG;
    } else {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GR;		//BAYER_GR;
        else{
            g_psensor->bayer_type = BAYER_BG;		//BAYER_BG;	color pattern ok
        }
    }
	printk ("#mirror:%d flip:%d type:%d\n", pinfo->mirror, pinfo->flip, g_psensor->bayer_type);
}

static int get_mirror(void)
{
    u32 reg;

    read_reg(0x321D, &reg);
    return (reg & BIT0) ? 1 : 0;

}

static int set_mirror(int enable)
{
    u32 reg;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    read_reg(0x321D, &reg);		
    if (enable)
        reg |= BIT0;
    else
        reg &=~BIT0;
    write_reg(0x321D, reg);
#if 1
    if(enable){
        write_reg(0x3400, 0x52);
        write_reg(0x3907, 0x00);
//        write_reg(0x3908, 0xc0);
        write_reg(0x3781, 0x08);
        write_reg(0x3211, 0x07);        
    }
    else{
        write_reg(0x3400, 0x53);
        write_reg(0x3907, 0x03);
//        write_reg(0x3908, 0x40);
        write_reg(0x3781, 0x10);
        write_reg(0x3211, 0x5a);        
    }
    
    pinfo->mirror = enable;
    update_bayer_type();
#endif
    // [Column Correction ReTriggering]

    return 0;

}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x321C, &reg);
    return (reg & BIT6) ? 1 : 0;

}

static int set_flip(int enable)
{
    u32 reg;
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    read_reg(0x321C, &reg);		
    if (enable)
        reg |= BIT6;
    else
        reg &=~BIT6;
    write_reg(0x321C, reg);
    pinfo->flip = enable;
    update_bayer_type();

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
    sensor_reg_t *sensor_init_regs;
    int num_of_init_regs;
    int ret = 0;
    int i;

    if (pinfo->is_init)
        return 0;

    printk("[SC1045] init start \n");  
    sensor_init_regs = sc1045_init_regs;
    num_of_init_regs = NUM_OF_SC1045_INIT_REGS;
    snprintf(g_psensor->name, DEV_MAX_NAME_SIZE, "SC1045");

    for (i=0; i<num_of_init_regs; i++) {
        if(sensor_init_regs[i].addr == DELAY_CODE){
            mdelay(sensor_init_regs[i].val);
        }
        else if(write_reg(sensor_init_regs[i].addr, sensor_init_regs[i].val) < 0) {
            isp_err("%s init fail!!\n", SENSOR_NAME);
            return -EINVAL;
        }
    }

    g_psensor->pclk = get_pclk(g_psensor->xclk);
    printk("[SC1045] pclk(%d) XCLK(%d)\n", g_psensor->pclk, g_psensor->xclk);

    // set image size
    ret = set_size(g_psensor->img_win.width, g_psensor->img_win.height);
    if (ret == 0)
        pinfo->is_init = 1;
    //Must be after set_size()
    adjust_vblk(g_psensor->fps);

    // set mirror and flip status
    set_mirror(mirror);
    set_flip(flip);
    // initial exposure and gain
    set_exp(g_psensor->min_exp);
    set_gain(g_psensor->min_gain);

    // get current exposure and gain setting
    get_exp();
    get_gain();
    printk("[SC1045] init done \n");    
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void sc1045_deconstruct(void)
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

static int sc1045_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->num_of_lane = 0;
    g_psensor->protocol = 0;
    g_psensor->fmt = RAW12;
    g_psensor->bayer_type = BAYER_BG;		//BAYER_GR;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
    g_psensor->inv_clk = inv_clk;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->min_exp = 1;
    g_psensor->max_exp = 5000; // 0.5 sec
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;
    //g_psensor->max_gain = 11424;
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

    //if ((ret = init()) < 0)
    //    goto construct_err;

    return 0;

construct_err:
    sc1045_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc1045_init(void)
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

    if ((ret = sc1045_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc1045_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc1045_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc1045_init);
module_exit(sc1045_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor SC1045");
MODULE_LICENSE("GPL");
