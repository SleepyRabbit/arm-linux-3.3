/**
 * @file sen_sc1035.c
 * Panasonic SC1035 sensor driver
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
#define Gain25X                 1652
#define Gain2X                  126  


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

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static uint wdr = 0;
module_param(wdr, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wdr, "WDR mode");

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME         "SC1035"
#define SENSOR_MAX_WIDTH    1280
#define SENSOR_MAX_HEIGHT   960
#define ID_SC1035          0x20

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
    int HMax;
} sensor_info_t;

typedef struct sensor_reg {
    u16  addr;
    u16 val;
} sensor_reg_t;

#define DELAY_CODE      0xFFFF

static sensor_reg_t sc1035_init_regs[] = {
    {0x3000,0x01},//manualstreamenbale
    {0x3003,0x01},//softreset
    {0x3400,0x53},
    {0x3416,0xc0},
    {0x3d08,0x00},
    {0x5000,0x09},
    {0x3e03,0x03},
    {0x3928,0x00},
    {0x3630,0x58},
    {0x3612,0x00},
    {0x3632,0x41},
    {0x3635,0x04},
    {0x3500,0x10},
    {0x3631,0x80},
    {0x3620,0x44},
    {0x3633,0x7c},
    {0x3780,0x0b},
    {0x3300,0x33},
    {0x3301,0x38},
    {0x3302,0x30},
    {0x3303,0x66},
    {0x3304,0xa0},
    {0x3305,0x72},
    {0x331e,0x56},
    {0x321e,0x00},
    {0x321f,0x0a},
    {0x3216,0x0a},
    {0x3115,0x0a},
    {0x3332,0x38},
    {0x5054,0x82},
    {0x3622,0x26},
    {0x3907,0x02},
    {0x3908,0x00},
    {0x3601,0x18},
    {0x3315,0x44},
    {0x3308,0x40},
    {0x3223,0x22},//vysncmode[5]
    {0x3e0e,0x50},
    //DPC
    {0x3101,0x9b},
    {0x3114,0x03},
    {0x3115,0xd1},
    {0x3211,0x60},
    {0x5780,0xff},
    {0x5781,0x7f},
    {0x5785,0xff},
    {0x5000,0x00},
    
    {0x3e0f,0x10},	
    {0x3631,0x82},  

    {0x3010,0x07}, //*27Minput54Moutputpixelclockfrequency*/
    {0x3011,0x46},
    {0x3004,0x04},//54M
    {0x3610,0x2b},

    /*configFramelengthandwidth*/
    {0x320c,0x07}, //1800
    {0x320d,0x08}, //1000
    {0x320e,0x03},
    {0x320f,0xe8},
    
    /*configOutputwindowposition*/
    {0x3210,0x00},
    {0x3211,0x60},
    {0x3212,0x00},
    {0x3213,0x09},
    
    /*configOutputimagesize*/
    {0x3208,0x05},
    {0x3209,0x00},
    {0x320a,0x03},
    {0x320b,0xc0},
    
    /*configFramestartphysicalposition*/
    {0x3202,0x00},
    {0x3203,0x08},
    {0x3206,0x03},
    {0x3207,0xcf},
    
    /*powerconsumptionreduction*/
    {0x3330,0x0d},
    {0x3320,0x06},
    {0x3321,0xe8},
    {0x3322,0x01},
    {0x3323,0xc0},
    {0x3600,0x54},
};
#define NUM_OF_SC1035_INIT_REGS (sizeof(sc1035_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    unsigned char DCG; 
    unsigned char analog;
    int gain_x;
} gain_set_t;

static gain_set_t gain_table[] =
{    
    {0, 0x00, 64 }, 
    {0, 0x01, 67 }, 
    {0, 0x02, 71 }, 
    {0, 0x03, 74 }, 
    {0, 0x04, 78 }, 
    {0, 0x05, 82 }, 
    {0, 0x06, 86 }, 
    {0, 0x07, 90 }, 
    {0, 0x08, 94 }, 
    {0, 0x09, 97 }, 
    {0, 0x0a, 101}, 
    {0, 0x0b, 104}, 
    {0, 0x0c, 108}, 
    {0, 0x0d, 112}, 
    {0, 0x0e, 116}, 
    {0, 0x0f, 120}, 
    {0, 0x82, 126}, 
    {0, 0x83, 133}, 
    {0, 0x84, 139}, 
    {0, 0x85, 146}, 
    {0, 0x86, 152}, 
    {0, 0x87, 159}, 
    {0, 0x88, 166}, 
    {0, 0x89, 172}, 
    {0, 0x8a, 179}, 
    {0, 0x8b, 186}, 
    {0, 0x8c, 192}, 
    {0, 0x8d, 199}, 
    {0, 0x8e, 206}, 
    {0, 0x8f, 213}, 
    {0, 0x90, 226}, 
    {0, 0x91, 239}, 
    {0, 0x92, 252}, 
    {0, 0x93, 266}, 
    {0, 0x94, 279}, 
    {0, 0x95, 293}, 
    {0, 0x96, 305}, 
    {0, 0x97, 319}, 
    {0, 0x98, 332}, 
    {0, 0x99, 346}, 
    {0, 0x9a, 359}, 
    {0, 0x9b, 373}, 
    {0, 0x9c, 385}, 
    {0, 0x9d, 399}, 
    {0, 0x9e, 412}, 
    {0, 0x9f, 426}, 
    {0, 0xb0, 453}, 
    {0, 0xb1, 479}, 
    {0, 0xb2, 506}, 
    {0, 0xb3, 532}, 
    {0, 0xb4, 559}, 
    {0, 0xb5, 586}, 
    {0, 0xb6, 612}, 
    {0, 0xb7, 639}, 
    {0, 0xb8, 666}, 
    {0, 0xb9, 692}, 
    {0, 0xba, 719}, 
    {0, 0xbb, 746}, 
    {0, 0xbc, 772}, 
    {0, 0xbd, 799}, 
    {0, 0xbe, 826}, 
    {0, 0xbf, 852}, 
    {0, 0xf0, 906}, 
    {0, 0xf1, 959},
    {0, 0xf2, 1012},
    {0, 0xf3, 1065},
    {0, 0xf4, 1119},
    {0, 0xf5, 1172},
    {0, 0xf6, 1225},
    {0, 0xf7, 1279},
    {0, 0xf8, 1332},
    {0, 0xf9, 1385},
    {0, 0xfa, 1438},
    {0, 0xfb, 1492},
    {0, 0xfc, 1545},
    {0, 0xfd, 1598},
    {0, 0xfe, 1652},
    {0, 0xff, 1705},
#if 0 
//due to digital gain bug, become pinky on over-exposured area
    {1, 0xf0, 1812},
    {1, 0xf1, 1918},
    {1, 0xf2, 2025},
    {1, 0xf3, 2131},
    {1, 0xf4, 2238},
    {1, 0xf5, 2344},
    {1, 0xf6, 2451},
    {1, 0xf7, 2558},
    {1, 0xf8, 2664},
    {1, 0xf9, 2771},
    {1, 0xfa, 2878},
    {1, 0xfb, 2984},
    {1, 0xfc, 3091},
    {1, 0xfd, 3198},
    {1, 0xfe, 3304},
    {1, 0xff, 3411},
    {3, 0xf0, 3624},
    {3, 0xf1, 3838},
    {3, 0xf2, 4051},
    {3, 0xf3, 4264},
    {3, 0xf4, 4477},
    {3, 0xf5, 4690},
    {3, 0xf6, 4903},
    {3, 0xf7, 5117},
    {3, 0xf8, 5330},
    {3, 0xf9, 5543},
    {3, 0xfa, 5756},
    {3, 0xfb, 5970},
    {3, 0xfc, 6183},
    {3, 0xfd, 6396},
    {3, 0xfe, 6609},
    {3, 0xff, 6823},
#endif    
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

static int init(void);
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
    u32 digital_test;
    u32 M, N, S, val0, val1, sclk_div;
    
    read_reg(0x3010, &val0);
    S = (val0>>4) & 0x7;
    N = (val0>>1) & 0x7;    
    if (0 == N)
        N = 10;
    else if(1 == N)
        N = 15;
    else
        N *= 10;

    digital_test = (val0>>7) & 0x1;

    read_reg(0x3011, &val1);
    M = ((val1>>3) & 0x1F) | ((val0&0x1)<<5);

    read_reg(0x3004, &sclk_div); 

    if(digital_test){
        pclk = xclk;
    } else { 
        pclk = (xclk / 1000) * (64 - M) * 10 / ( N * (S+1)) / sclk_div * 1000;
    }
    return pclk;
}
static u32 get_exp(void);
static int set_exp(u32 exp);
static void adjust_vblk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;
    
    pinfo->HMax = g_psensor->pclk / (fps * pinfo->VMax);    
    reg_h = (pinfo->HMax >> 8) & 0xFF;
    reg_l = pinfo->HMax & 0xFF;
    write_reg(0x320C, reg_h);
    write_reg(0x320D, reg_l);
    pinfo->t_row = (pinfo->HMax * 10000) / (g_psensor->pclk / 1000);
    printk("pinfo->HMax=%d, pinfo->VMax=%d, fps=%d, pinfo->t_row=%d, g_psensor->pclk=%d\n", pinfo->HMax, pinfo->VMax, fps, pinfo->t_row, g_psensor->pclk);
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;

    g_psensor->fps = fps;

    //to avoid exposure line exceed frame_length_lines
    get_exp();    
    set_exp(g_psensor->curr_exp);
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
    pinfo->HMax = line_w;

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

    g_psensor->pclk = get_pclk(g_psensor->xclk);
    adjust_vblk(g_psensor->fps);

    //to avoid exposure line exceed frame_length_lines
    get_exp();
    set_exp(g_psensor->curr_exp);
    
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
        num_of_line_pclk = (high << 4) | (low>>4);
        g_psensor->curr_exp = num_of_line_pclk * pinfo->t_row / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line, reg_h, reg_l, int_time, denoisevalue, vts;

    exp_line = MAX(1, (exp * 1000 + pinfo->t_row / 2 ) / pinfo->t_row);

    if(exp_line > pinfo->VMax - 4){
        write_reg(0x320E, (exp_line >> 8) & 0xFF);
        write_reg(0x320F, exp_line & 0xFF);
    }
    else{        
        write_reg(0x320E, (pinfo->VMax >> 8) & 0xFF);
        write_reg(0x320F, pinfo->VMax & 0xFF);        
    }
    //Exp[11:0] = 0x3E01[7:0], 0x3E02[7:4];
	  if ((exp_line-2) >= 0xFFF) // HW Limit
    {
		    write_reg(0x3E01, (0xfff >> 4) & 0xFF);
		    write_reg(0x3E02, (0xfff << 4 )& 0xFF); 		
	  }else{
		    write_reg(0x3E01, ((exp_line-2) >> 4) & 0xFF);
		    write_reg(0x3E02, ((exp_line-2) << 4 )& 0xFF); 
	  }


	  // denoise follow exp time	  
	  read_reg(0x320E, &reg_h);
	  read_reg(0x320F, &reg_l);
	  vts = (reg_h << 8) | reg_l;
	  read_reg(0x3E01, &reg_h);
	  read_reg(0x3E02, &reg_l);
	  int_time = (reg_h << 4) + ((reg_l>>4) & 0x0f);
	  //printk("int_time=%d, pinfo->VMax=%d.\r\n", int_time, pinfo->VMax);
	  
	  if(int_time <= (vts - 304))
	  {
	      denoisevalue = 0xe0;
	  }
	  else if((int_time <= vts - 4) && (int_time > (vts - 304)))
	  {	      
	      denoisevalue = 33*(vts-4-int_time)/50 + 0x1a;	      
	  }
	  else{
	      denoisevalue = 0x1a;
	  }
	  write_reg(0x331e,denoisevalue);
	  
	  //printk("denoisevalue=%d\n", denoisevalue);
    
    g_psensor->curr_exp = MAX(1, (exp_line * pinfo->t_row + 500)/ 1000);

    return ret;
}

static u32 get_gain(void)
{
    u32 gain, i;

    if (!g_psensor->curr_gain) {
        read_reg(0x3e09, &gain);
        // read_reg(0x3e0f, gain);
        
        for (i=0; i<NUM_OF_GAINSET; i++) {
            // if (gain_table[i].analog == gain)
            if (gain_table[i].analog > gain)
            {
                g_psensor->curr_gain = gain_table[i - 1].gain_x;
                break;
            }
        }
        if (i < NUM_OF_GAINSET)
            g_psensor->curr_gain = gain_table[i].gain_x;
    }
    return g_psensor->curr_gain;

}

static int set_gain(u32 gain)
{
    int ret = 0;
    int i;
    int reg ;

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
    
    read_reg(0x3e0f, &reg);
    reg &= 0xFC;
    reg |= gain_table[i].DCG;
    write_reg(0x3e0f, reg);
    
    if(gain_table[i].gain_x >= Gain25X)//enable dpc    
    {        
        write_reg(0x3211,0x60);
        write_reg(0x5000,0x66);
    }else{
        write_reg(0x3211,0x60);
        write_reg(0x5000,0x00);
    }																		

    g_psensor->curr_gain = gain_table[i].gain_x;
	
    return ret;

}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->mirror) {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GB;        
        else
            g_psensor->bayer_type = BAYER_GR;        
    } else {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_BG;        
        else{
            g_psensor->bayer_type = BAYER_GR;        
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

    // ##important## 
    if(enable){
        //disable RNC
        write_reg(0x3400, 0x52);
        //set BLC target
        write_reg(0x3907, 0x00);        //set BLC target
        write_reg(0x3908, 0xC0);        //set BLC target
        write_reg(0x3781, 0x08);        //adjust offset   
        write_reg(0x3211, 0x07);        //           
    }
    else{
        //enable RNC
        write_reg(0x3400, 0x53);        
        //BLC target = {0x3415, 0x3416} != {0x3907, 0x3908}
        write_reg(0x3907, 0x02);        //set random raw noise buffer size
        write_reg(0x3908, 0x00);        //set random raw noise buffer size
        write_reg(0x3211, 0x62);        //adjust offset     
        write_reg(0x3781, 0x10);        //   
    }
    
    pinfo->mirror = enable;
    update_bayer_type();

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
static void set_xclk(void)
{
    return;
#define CRYSTAL_CLOCK   24000000
#define CHIP_CLOCK      27000000

    if(g_psensor->xclk == CHIP_CLOCK){
        // [PLL Enabled 27Mhz to 54MMhz 960P@30fps]
        write_reg(0x3010, 0x07);
        write_reg(0x3011, 0x46);
        write_reg(0x3004, 0x04);
        mdelay(100);
    }
    else if(g_psensor->xclk == CRYSTAL_CLOCK){
        // [PLL Enabled 24Mhz to 54Mhz  960P@30fps]
        write_reg(0x3010, 0x08);
        write_reg(0x3011, 0xE6);
        write_reg(0x3004, 0x04);
        mdelay(100);
    }
    else{
        isp_err("Input XCLK=%d, set XCLK=%d\n",g_psensor->xclk, CHIP_CLOCK);
    }

#undef CRYSTAL_CLOCK
#undef CHIP_CLOCK
}

static int init(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    sensor_reg_t *sensor_init_regs;
    int num_of_init_regs;
    u32 id;
    int ret = 0;
    int i;

    if (pinfo->is_init)
        return 0;
    
    printk("[SC1035] init start \n");  
    read_reg(0x5781, &id);
    if (id == ID_SC1035) {
        sensor_init_regs = sc1035_init_regs;
        num_of_init_regs = NUM_OF_SC1035_INIT_REGS;
    }else{
    }    

    for (i=0; i<num_of_init_regs; i++) {
        if(sensor_init_regs[i].addr == DELAY_CODE){
            mdelay(sensor_init_regs[i].val);
        }
        else if(write_reg(sensor_init_regs[i].addr, sensor_init_regs[i].val) < 0) {
            isp_err("%s init fail!!\n", SENSOR_NAME);
            return -EINVAL;
        }
    }

    set_xclk(); // set xclk
    g_psensor->pclk = get_pclk(g_psensor->xclk);
    printk("[SC1035] pclk(%d) XCLK(%d)\n", g_psensor->pclk, g_psensor->xclk);

    // set mirror and flip status
    set_mirror(mirror);
    set_flip(flip);
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
    printk("[SC1035] init done \n");    
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void sc1035_deconstruct(void)
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

static int sc1035_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->bayer_type = BAYER_GR;        //BAYER_GR;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_LOW;
    g_psensor->inv_clk = 1; // boli==0; evb==1
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

    if ((ret = init()) < 0)
        goto construct_err;

    return 0;

construct_err:
    sc1035_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc1035_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp320.ko(%x)!\n",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = sc1035_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc1035_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc1035_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc1035_init);
module_exit(sc1035_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor SC1035");
MODULE_LICENSE("GPL");
