/**
 * @file sen_sc1135.c
 * sc1135 sensor driver
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

//#define PFX "sen_sc1135"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH      1280 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT      960 //ISP Output size
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
#define SENSOR_NAME         "SC1135"
#define SENSOR_MAX_WIDTH        1280
#define SENSOR_MAX_HEIGHT        960
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        4
#define Sensor_MAX_EXP          4095
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF
#define Gain2X                   128

//DVP setting
#define  DEFAULT_960P_DVP_MODE      MODE_DVP_960P
#define  DEFAULT_960P_DVP_VTS                1000
#define  DEFAULT_960P_DVP_HTS                1800
#define  DEFAULT_960P_DVP_WIDTH              1280
#define  DEFAULT_960P_DVP_HEIGHT              960
#define  DEFAULT_960P_DVP_MIRROR                0
#define  DEFAULT_960P_DVP_FLIP                  0
#define  DEFAULT_960P_DVP_TROW                333
#define  DEFAULT_960P_DVP_PCLK           54000000
#define  DEFAULT_960P_DVP_MAXFPS               30
#define  DEFAULT_960P_DVP_LANE                  2 //don't care
#define  DEFAULT_960P_DVP_RAWBIT            RAW12
#define  DEFAULT_960P_DVP_BAYER          BAYER_GR
#define  DEFAULT_960P_DVP_INTERFACE   IF_PARALLEL

//DVP setting
#define  DEFAULT_720P_DVP_MODE      MODE_DVP_720P
#define  DEFAULT_720P_DVP_VTS                1000
#define  DEFAULT_720P_DVP_HTS                1800
#define  DEFAULT_720P_DVP_WIDTH              1280
#define  DEFAULT_720P_DVP_HEIGHT              720
#define  DEFAULT_720P_DVP_MIRROR                0
#define  DEFAULT_720P_DVP_FLIP                  0
#define  DEFAULT_720P_DVP_TROW                333
#define  DEFAULT_720P_DVP_PCLK           54000000
#define  DEFAULT_720P_DVP_MAXFPS               30
#define  DEFAULT_720P_DVP_LANE                  2 //don't care
#define  DEFAULT_720P_DVP_RAWBIT            RAW12
#define  DEFAULT_720P_DVP_BAYER          BAYER_GR
#define  DEFAULT_720P_DVP_INTERFACE   IF_PARALLEL

#define Gain2X                  128
#define Gain16X                1024

static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_DVP_960P = 0, 
    MODE_DVP_720P,      
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


static sensor_info_t g_sSC1135info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_960P_DVP_MODE,        
    DEFAULT_960P_DVP_VTS,
    DEFAULT_960P_DVP_HTS,
    DEFAULT_960P_DVP_WIDTH,
    DEFAULT_960P_DVP_HEIGHT,
    DEFAULT_960P_DVP_MIRROR,
    DEFAULT_960P_DVP_FLIP,
    DEFAULT_960P_DVP_TROW,
    DEFAULT_960P_DVP_PCLK,
    DEFAULT_960P_DVP_MAXFPS,
    DEFAULT_960P_DVP_LANE,
    DEFAULT_960P_DVP_RAWBIT,
    DEFAULT_960P_DVP_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_960P_DVP_INTERFACE,    
    },
    {
    SENSOR_ISINIT,
    DEFAULT_720P_DVP_MODE,        
    DEFAULT_720P_DVP_VTS,
    DEFAULT_720P_DVP_HTS,
    DEFAULT_720P_DVP_WIDTH,
    DEFAULT_720P_DVP_HEIGHT,
    DEFAULT_720P_DVP_MIRROR,
    DEFAULT_720P_DVP_FLIP,
    DEFAULT_720P_DVP_TROW,
    DEFAULT_720P_DVP_PCLK,
    DEFAULT_720P_DVP_MAXFPS,
    DEFAULT_720P_DVP_LANE,
    DEFAULT_720P_DVP_RAWBIT,
    DEFAULT_720P_DVP_BAYER,
    SENSOR_LONG_EXP,
    DEFAULT_720P_DVP_INTERFACE,    
    },      
};

typedef struct sensor_reg {
    u16 addr;  
    u16 val;
} sensor_reg_t;

static sensor_reg_t sensor_dvp_init_regs[] = {
    {0x3000,0x01},//manualstreamenbale         
    {0x3003,0x01},//softreset                  
    {0x3400,0x53},                             
    {0x3416,0xc0},                             
    {0x3d08,0x00},                             
    {0x3e03,0x03},                             
    {0x3928,0x00},                             
    {0x3630,0x58},                             
    {0x3612,0x00},                             
    {0x3632,0x41},                             
    {0x3635,0x00}, //20160328                  
    {0x3500,0x10},                             
    {0x3620,0x44},                             
    {0x3633,0x7c},                             
    {0x3780,0x0b},                             
    {0x3300,0x33},                             
    {0x3301,0x38},                             
    {0x3302,0x30},                             
    {0x3303,0xa0}, //20160307B                 
    {0x3304,0x18},                             
    {0x3305,0x72},                             
    {0x331e,0xa0},                             
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
    /*DPC*/                                           
    {0x3101,0x9b},                             
    {0x3114,0x03},                             
    {0x3115,0xd1},                             
    {0x3211,0x60},                             
    {0x5780,0xff},                             
    {0x5781,0x04}, //20160328                  
    {0x5785,0x0c}, //20160328                  
    {0x5000,0x00},//0x66 disable sensor DPC                                                                        
    {0x3e0f,0x90},	                           
    {0x3631,0x80},                                                                       
    {0x3310,0x83},                                                                        
    {0x3336,0x01},                                                                     
    {0x3337,0x00},                                                                     
    {0x3338,0x03},                                                                      
    {0x3339,0xe8},                                                                       
    {0x3335,0x10},                                                                        
    {0x3880,0x00},                             
    /*27Minput54Moutputpixelclockfrequency*/
    {0x3010,0x07},                                           
    {0x3011,0x46},                                           
    {0x3004,0x04},
    {0x3610,0x2b},
    /*configFramelengthandwidth*/
    {0x320c,0x07},
    {0x320d,0x08},
    {0x320e,0x03},
    {0x320f,0xe8},
};
#define NUM_OF_DVP_INIT_REGS (sizeof(sensor_dvp_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_960p_dvp_init_regs[] = {
    /*configOutputwindowposition*/
    {0x3210,0x00},
    {0x3211,0x60},
    {0x3212,0x00},
    {0x3213,0x05},
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
    /*powerconsumptionreductionforXiongMai*/
    {0x3330,0x0d},
    {0x3320,0x06},
    {0x3321,0xe8},
    {0x3322,0x01},
    {0x3323,0xc0},
    {0x3600,0x54},
};
#define NUM_OF_960P_DVP_INIT_REGS (sizeof(sensor_960p_dvp_init_regs) / sizeof(sensor_reg_t))

static sensor_reg_t sensor_720p_dvp_init_regs[] = {
    /*configOutputwindowposition*/
    {0x3210,0x00},
    {0x3211,0x60},
    {0x3212,0x00},
    {0x3213,0x81},
    /*configOutputimagesize*/
    {0x3208,0x05},
    {0x3209,0x00},
    {0x320a,0x02},
    {0x320b,0xd0},
    /*configFramestartphysicalposition*/
    {0x3202,0x00},
    {0x3203,0x08},
    {0x3206,0x03},
    {0x3207,0xcf},
    /*powerconsumptionreductionforXiongMai*/
    {0x3330,0x0d},
    {0x3320,0x06},
    {0x3321,0xe8},
    {0x3322,0x01},
    {0x3323,0xc0},
    {0x3600,0x54},
};
#define NUM_OF_720P_DVP_INIT_REGS (sizeof(sensor_720p_dvp_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    uint8_t a_gain;
    uint8_t d_gain;
    //unsigned char analog;
    u32 gain_x;
} gain_set_t;

static gain_set_t gain_table[] =
{    
    {0x00,0x00,  64}, {0x02,0x00,  72}, {0x04,0x00,  79}, {0x06,0x00,  87},
    {0x08,0x00,  94}, {0x0A,0x00, 102}, {0x0C,0x00, 109}, {0x0F,0x00, 120},
    {0x10,0x00, 128}, {0x12,0x00, 143}, {0x14,0x00, 158}, {0x16,0x00, 173},
    {0x18,0x00, 188}, {0x1A,0x00, 203}, {0x1C,0x00, 218}, {0x1F,0x00, 240},	
    {0x30,0x00, 256}, {0x32,0x00, 286}, {0x34,0x00, 316}, {0x36,0x00, 346},
    {0x38,0x00, 376}, {0x3A,0x00, 407}, {0x3C,0x00, 437}, {0x3E,0x00, 467},	
    {0x70,0x00, 512}, {0x72,0x00, 572}, {0x74,0x00, 632}, {0x76,0x00, 693},
    {0x78,0x00, 753}, {0x7A,0x00, 813}, {0x7C,0x00, 873}, {0x7E,0x00, 933},	
    {0x70,0x01,1012}, {0x72,0x01,1144}, {0x74,0x01,1265}, {0x76,0x01,1385},
    {0x78,0x01,1505}, {0x7A,0x01,1626}, {0x7C,0x01,1746}, {0x7E,0x01,1866},
    {0x70,0x03,2048}, {0x72,0x03,2289}, {0x74,0x03,2529}, {0x76,0x03,2770},
    {0x78,0x03,3011}, {0x7A,0x03,3251}, {0x7C,0x03,3492}, {0x7E,0x03,3732},
    {0x7F,0x03,3853}   
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
	
    pinfo->pclk= g_sSC1135info[pinfo->currentmode].pclk;
	
    //isp_info("sc1135 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sSC1135info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC1135info[pinfo->currentmode].t_row;

    //isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 exp_line,exp_line_h,exp_line_l;
	
    if (!g_psensor->curr_exp) {
        read_reg(0x3E01, &exp_line_h);
        read_reg(0x3E02, &exp_line_l);		
        exp_line=((exp_line_h & 0xFF)<<4)|((exp_line_l & 0xF0)>>4);             
        g_psensor->curr_exp = (exp_line * (pinfo->t_row) + 500) / 1000;
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line,tmp_reg,tmp_reg1;

    pinfo->hts=g_sSC1135info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC1135info[pinfo->currentmode].t_row;
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (exp_line>=Sensor_MAX_EXP) {
        tmp_reg=(Sensor_MAX_EXP+SENSOR_NON_EXP_LINE)-0x2E8;       		
        exp_line=Sensor_MAX_EXP;
        pinfo->t_row= (exp * 1000)/(Sensor_MAX_EXP+SENSOR_NON_EXP_LINE);                  
        pinfo->hts= pinfo->t_row*(g_sSC1135info[pinfo->currentmode].hts)/(g_sSC1135info[pinfo->currentmode].t_row);	
        tmp_reg1= (pinfo->hts)-0x20; 
        write_reg(0x320C, ((pinfo->hts) >> 8) & 0xFF);    
        write_reg(0x320D, ((pinfo->hts) & 0xFF));			
        write_reg(0x320E, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
        write_reg(0x320F, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) & 0xFF));		
        write_reg(0x3336, ((tmp_reg) >> 8) & 0xFF); 
        write_reg(0x3337, ((tmp_reg) & 0xFF));
        write_reg(0x3338, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
        write_reg(0x3339, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) & 0xFF));	                
        write_reg(0x3320, (tmp_reg1) >> 8);
        write_reg(0x3321, (tmp_reg1) & 0xff);
        pinfo->long_exp = 1;		
    } else {
        tmp_reg1= (pinfo->hts)-0x20;     
        write_reg(0x320C, (pinfo->hts) >> 8);
        write_reg(0x320D, (pinfo->hts) & 0xff); 		
        if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) {
            tmp_reg=((exp_line)+SENSOR_NON_EXP_LINE)-0x2E8;	
            write_reg(0x320E, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
            write_reg(0x320F, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
            write_reg(0x3336, (tmp_reg >> 8) & 0xFF); 
            write_reg(0x3337, (tmp_reg & 0xFF));			
            write_reg(0x3338, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF); 
            write_reg(0x3339, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
            write_reg(0x3320, (tmp_reg1) >> 8);
            write_reg(0x3321, (tmp_reg1) & 0xff);  			
            pinfo->long_exp = 1;
        } else {
            if (pinfo->long_exp) {
                tmp_reg=(pinfo->vts)-0x2E8;
                write_reg(0x320E, (pinfo->vts >> 8) & 0xFF);        
                write_reg(0x320F, (pinfo->vts & 0xFF));
                write_reg(0x3336, (tmp_reg >> 8) & 0xFF); 
                write_reg(0x3337, (tmp_reg & 0xFF));				
                write_reg(0x3338, (pinfo->vts >> 8) & 0xFF); 
                write_reg(0x3339, (pinfo->vts & 0xFF)); 
                write_reg(0x3320, (tmp_reg1) >> 8);
                write_reg(0x3321, (tmp_reg1) & 0xff);  				
                pinfo->long_exp = 0;
            }
        }
    }		
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

    max_fps = g_sSC1135info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sSC1135info[pinfo->currentmode].vts) * max_fps / fps;

    if ((pinfo->vts) > SENSOR_MAX_VTS)
        pinfo->vts=	SENSOR_MAX_VTS;

    tmp_reg=(pinfo->vts)-0x2E8;
    tmp_reg1=(pinfo->hts)-0x20;    		

    write_reg(0x320E, (pinfo->vts) >> 8);
    write_reg(0x320F, (pinfo->vts) & 0xff);
    write_reg(0x3336, (tmp_reg) >> 8);
    write_reg(0x3337, (tmp_reg) & 0xff);
    write_reg(0x3338, (pinfo->vts) >> 8);
    write_reg(0x3339, (pinfo->vts) & 0xff);
    write_reg(0x3320, (tmp_reg1) >> 8);
    write_reg(0x3321, (tmp_reg1) & 0xff);
           
    get_exp();
    set_exp(g_psensor->curr_exp);
	
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    uint32_t a_gain,d_gain;
    int i;
    if (!g_psensor->curr_gain) {
    	        	
        read_reg(0x3e09, &a_gain);
        read_reg(0x3e08, &d_gain);		
        if (a_gain > gain_table[NUM_OF_GAINSET - 1].a_gain)
            a_gain = gain_table[NUM_OF_GAINSET - 1].a_gain;
        else if(a_gain < gain_table[0].a_gain)
            a_gain = gain_table[0].a_gain;
        // search most suitable gain into gain table
        for (i=0; i<NUM_OF_GAINSET; i++) {
            if ((gain_table[i].a_gain > a_gain) && (gain_table[i].d_gain>=d_gain))
                break;
            else if((gain_table[i].a_gain < a_gain) && (gain_table[i].d_gain>d_gain))
                break;
        }        
        g_psensor->curr_gain = gain_table[i-1].gain_x;        
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
	
    write_reg(0x3e08, gain_table[i-1].d_gain);   	     
    write_reg(0x3e09, gain_table[i-1].a_gain);
          
    if (gain_table[i-1].gain_x < Gain2X)   	  	  	  
        write_reg(0x3630, 0x80);      	      	      	      	      
    else   
        write_reg(0x3630, 0x58);    
	  
    g_psensor->curr_gain = gain_table[i-1].gain_x;

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
            g_psensor->bayer_type = BAYER_BG;        
        else{
            g_psensor->bayer_type = BAYER_GR;        
        }
    }
    
    printk ("#mirror:%d flip:%d type:%d\n", pinfo->mirror, pinfo->flip, g_psensor->bayer_type);
}

static int get_mirror(void)
{
/*  no support
    u32 reg;

    read_reg(0x321D, &reg);
    return (reg & BIT0) ? 1 : 0;
*/
    return 0;
}

static int set_mirror(int enable)
{
/*  no support
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
*/
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

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    sensor_reg_t* pInitTable;
    u32 InitTable_Num;    
    int i,ret = 0;
	
    //isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT))
    {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }

    if (720<height) {
        pinfo->currentmode=MODE_DVP_960P;
        pInitTable=sensor_960p_dvp_init_regs;
        InitTable_Num=NUM_OF_960P_DVP_INIT_REGS;        
    } else {
        pinfo->currentmode=MODE_DVP_720P; 
        pInitTable=sensor_720p_dvp_init_regs;
        InitTable_Num=NUM_OF_720P_DVP_INIT_REGS; 
    }
      
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
	
    set_mirror(pinfo->mirror);
    set_flip(pinfo->flip);
    calculate_t_row();
    adjust_blk(g_psensor->fps);

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sSC1135info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sSC1135info[pinfo->currentmode].img_h;	

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
    pInitTable=sensor_dvp_init_regs;
    InitTable_Num=NUM_OF_DVP_INIT_REGS;

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
static void sc1135_deconstruct(void)
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

static int sc1135_construct(u32 xclk, u16 width, u16 height)
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
    if(720<height)
        pinfo->currentmode=MODE_DVP_960P;
    else
        pinfo->currentmode=MODE_DVP_720P;            
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sSC1135info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sSC1135info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sSC1135info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sSC1135info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
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
    sc1135_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc1135_init(void)
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

    if ((ret = sc1135_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc1135_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc1135_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc1135_init);
module_exit(sc1135_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor sc1135");
MODULE_LICENSE("GPL");
