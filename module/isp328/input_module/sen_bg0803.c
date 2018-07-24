/**
 * @file sen_bg0803.c
 * BG 0803 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
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

#define PFX "sen_bg0803"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH          1920	
#define DEFAULT_IMAGE_HEIGHT         1080
#define DEFAULT_XCLK             27000000

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

static int ch_num = 2;
module_param(ch_num, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ch_num, "channel number");

static int fps = 30;
module_param(fps, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fps, "frame per second");

static uint inv_clk = 0;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");
//=============================================================================
// global parameters
//=============================================================================
#define SENSOR_NAME              "BG0803"
#define SENSOR_MAX_WIDTH             1920
#define SENSOR_MAX_HEIGHT            1080
#define SENSOR_MAX_VBLANK           65535
#define SENSOR_NON_EXP_LINE             1
#define SENSOR_ISINIT                   0
#define SENSOR_LONG_EXP                 0
#define DELAY_CODE                 0xFFFF

//1080P mode
#define  DEFAULT_1080_MODE     MODE_1080P
#define  DEFAULT_1080P_WIDTH         1920
#define  DEFAULT_1080P_HEIGHT        1084
#define  DEFAULT_1080P_MIRROR           0
#define  DEFAULT_1080P_FLIP             0
#define  DEFAULT_1080P_TROW           355
#define  DEFAULT_1080P_PCLK      81000000
#define  DEFAULT_1080P_MAXFPS          25
#define  DEFAULT_1080P_RAWBIT       RAW12
#define  DEFAULT_1080P_BAYER     BAYER_BG
#define  DEFAULT_1080P_VBLANK          33

static sensor_dev_t *g_psensor = NULL;

// sensor Mode
enum SENSOR_MODE {
    MODE_1080P = 0,
    MODE_MAX
};

typedef struct sensor_info {
    bool is_init;
    enum SENSOR_MODE currentmode; 
    int img_w;
    int img_h;
    int mirror;
    int flip;
    u16 t_row; // unit is 1/10 us
    u32 pclk;
    int fps;
    enum SRC_FMT rawbit;	
    enum BAYER_TYPE bayer;	
    int vblank;
    int long_exp;
} sensor_info_t;


static sensor_info_t g_sBG0803info[]=
{
    {
    SENSOR_ISINIT,
    DEFAULT_1080_MODE,
    DEFAULT_1080P_WIDTH,
    DEFAULT_1080P_HEIGHT,
    DEFAULT_1080P_MIRROR,
    DEFAULT_1080P_FLIP,
    DEFAULT_1080P_TROW,
    DEFAULT_1080P_PCLK,
    DEFAULT_1080P_MAXFPS,
    DEFAULT_1080P_RAWBIT,
    DEFAULT_1080P_BAYER,
    DEFAULT_1080P_VBLANK,
    SENSOR_LONG_EXP,
    },
};

typedef struct sensor_reg {
    u16 addr;
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_1080P_init_regs[] = {  
//   1.8v setting
    {0xf0, 0x00},                                        
    {0x05, 0x1c}, //vstart          
  	{0x08, 0x04}, //set vsize                                      
    {0x09, 0x3c},
    {0x0e, 0x03},                                        
    {0x0f, 0xc0}, //rowtime                              
    {0x11, 0x01}, //rmp2ivrst        
    {0x13, 0x06},
    {0x1e, 0x08}, //VTHC                                   
    {0x20, 0x0d}, //vetical mirror                                        
    {0x21, 0x00}, 
    {0x22, 0x21}, //vblank=0x25                          
    {0x29, 0x1a}, //rstb_w                                
    {0x2a, 0x1a}, //txb                                   
    {0x2c, 0x01}, //cnt_rts_w                             
    {0x2d, 0x00},                                        
    {0x2e, 0x01}, //dummy timming                         
    {0x31, 0x2f}, //rstb2cmp_rst                          
    {0x32, 0x0f},                                        
    {0x33, 0x01}, //rmp1totxb                             
    {0x34, 0x3a}, //tx2rmp2                               
    {0x35, 0x01}, //ivrst2rsel                            
    {0x36, 0x01}, //read start position                   
    {0x37, 0x03},                                        
    {0x38, 0x00}, //Rst2 width H                         
    {0x39, 0x36}, //Rst2 width L                         
    {0x3a, 0x00}, //Tck2 width H                         
    {0x3b, 0x34}, //Tck2 width L       
    {0x3d, 0x0f}, 						 
    {0x3e, 0x01},
    {0x4a, 0x00},                                        
    {0x4b, 0x02}, //rmp2ltcstg2                           
    {0x4f, 0x02}, //11bit mode ramp                       
    {0x52, 0xdc}, //pll=297M  rmpclk=148.5M                         
    {0x53, 0x2e}, //PCLK = S81Mhz           
    {0x54, 0x02},                                        
    {0x5f, 0x02}, //ibias_sw_w                            
    {0x63, 0x02}, //ltcstg2_rmp2                          
    {0x64, 0x05},                                        
    {0x65, 0x00},                                        
    {0x67, 0x07},                                        
    {0x68, 0xc0}, //ramp length                           
    {0x69, 0x01}, //flip_tog                              
    {0x6a, 0x03}, //tog_flip                              
    {0x6b, 0x01}, //cnten_flip                            
    {0x6d, 0x01}, //pclk=74.25M                           
    {0x7f, 0x00},                                        
    {0x81, 0x18}, //rst=vth,tx=vth,rs=vth                
    {0x82, 0x14}, //ibias  //0x06 to 0x14 improve fix vertical pattern
    {0x83, 0x01}, //ipixel   
    {0x84, 0x08},
    {0x86, 0x02},    
    {0x89, 0x21}, //PD internal LDO             
    {0x8a, 0x03}, //driver                       
    {0x8c, 0x03}, //driver
    {0xb9, 0x21},                                        
    {0xb0, 0x00},                                        
    {0xb1, 0x0c}, //pga gain setting                     
    {0xb7, 0x04}, //digital gain setting                 
    {0xc0, 0x02}, //vcm_comp setting   
    {0xd0, 0x01},
	  //page1
    {0xf0, 0x01},                                        
    {0x09, 0x01}, //AD order     
    {0x20, 0x00}, 
   	{0x21, 0x70}, //OB
  	{0x22, 0x00},
  	{0x23, 0xef},
  	{0x24, 0x00},
  	{0x25, 0x08}, //blcc base 
    {0xd1, 0x40},                                        
    {0xd3, 0x40},                                        
    {0xd5, 0x40},                                        
    {0xd7, 0x40},
    {0xf1, 0x01}, //enable dpc        
    {0xf2, 0x3f}, //dpc th                               
    {0xf3, 0x3f}, //dpt th                               
    {0xf0, 0x00},                                                      
    {0x88, 0x04},
    {0x6c, 0x01}, //Pclk & Pclk2x phase adjust           
    {0x8d, 0x00}, //oen                                  
    {0x8e, 0x00}, //oen                                  
    {0x1d, 0x01},
};
#define NUM_OF_1080P_INIT_REGS (sizeof(sensor_1080P_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  analog_gain;
    u8  digit_gain_h;
    u8  digit_gain_l;
    u32 total_gain; // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x9F, 0x1, 0x00,  64 }, {0x8D, 0x1, 0x00,  72 }, {0x7F, 0x1, 0x00,  80 },
    {0x73, 0x1, 0x00,  88 }, {0x6A, 0x1, 0x00,  96 }, {0x62, 0x1, 0x00, 104 },
    {0x5B, 0x1, 0x00, 112 }, {0x55, 0x1, 0x00, 120 }, {0x4F, 0x1, 0x00, 128 },
    {0x46, 0x1, 0x00, 145 }, {0x3F, 0x1, 0x00, 161 }, {0x3A, 0x1, 0x00, 175 },
    {0x35, 0x1, 0x00, 192 }, {0x31, 0x1, 0x00, 208 }, {0x2D, 0x1, 0x00, 226 },
    {0x2A, 0x1, 0x00, 242 }, {0x28, 0x1, 0x00, 254 }, {0x23, 0x1, 0x00, 290 },
    {0x20, 0x1, 0x00, 318 }, {0x1D, 0x1, 0x00, 351 }, {0x1A, 0x1, 0x00, 391 },
    {0x18, 0x1, 0x00, 424 }, {0x17, 0x1, 0x00, 442 }, {0x15, 0x1, 0x00, 484 },
    {0x14, 0x1, 0x00, 509 }, {0x12, 0x1, 0x00, 565 }, {0x10, 0x1, 0x00, 636 },
    {0x0F, 0x1, 0x09, 704 }, {0x0E, 0x1, 0x14, 768 }, {0x0D, 0x1, 0x10, 832 },
    {0x0C, 0x1, 0x0E, 896 }, {0x0C, 0x1, 0x21, 960 }, {0x0C, 0x1, 0x35,1024 },
  /*{0x0C, 0x1, 0x5B,1152 }, {0x0C, 0x1, 0x82,1280 }, {0x0C, 0x1, 0xA9,1408 },
    {0x0C, 0x1, 0xCF,1536 }, {0x0C, 0x1, 0xF6,1664 }, {0x0C, 0x2, 0x1C,1792 },
    {0x0C, 0x2, 0x43,1920 }, {0x0C, 0x2, 0x6A,2048 }, {0x0C, 0x2, 0xB7,2304 },
    {0x0C, 0x3, 0x04,2560 }, {0x0C, 0x3, 0x52,2816 }, {0x0C, 0x3, 0x9F,3072 },
    {0x0C, 0x3, 0xEC,3328 }, {0x0C, 0x4, 0x39,3584 }, {0x0C, 0x4, 0x87,3840 },
    {0x0C, 0x4, 0xD4,4096 }, {0x0C, 0x5, 0x6F,4608 }, {0x0C, 0x6, 0x09,5120 },
    {0x0C, 0x6, 0xA4,5632 }, {0x0C, 0x7, 0x3E,6144 }, {0x0C, 0x7, 0xD9,6656 },
    {0x0C, 0x8, 0x73,7168 }, {0x0C, 0x9, 0x0E,7680 }, {0x0C, 0x9, 0xA9,8192 },
    {0x0C, 0xA, 0xDE,9216 }, {0x0C, 0xC, 0x13,10240}, {0x0C, 0xD, 0x48,11264},
    {0x0C, 0xE, 0x7D,12288}, {0x0C, 0xF, 0xB2,13312},*/
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x64 >> 1
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    u8 buf[2],buf2[2];
       
    buf[0]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
    msgs[0].buf   = buf;

    buf2[0]       = 0;
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1;
    msgs[1].len   = 1;
    msgs[1].buf   = buf2;

    if (sensor_i2c_transfer(msgs, 2))
        return -1;

    *pval = buf2[0];
    return 0;
}

static int write_reg(u32 addr, u32 val)
{
    struct i2c_msg  msgs;
    u8   buf[3];

    buf[0]     = addr & 0xFF;
    buf[1]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 2;
    msgs.buf   = buf;

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk=g_sBG0803info[pinfo->currentmode].pclk;
	
    isp_info("BG0803 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
   
    pinfo->t_row=g_sBG0803info[pinfo->currentmode].t_row;

    isp_info("t_row=%d\n", pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;
    u32 exp_line;

    if (!g_psensor->curr_exp) {
        write_reg(0xf0,0x00);
        read_reg(0x0c, &reg_h);
        read_reg(0x0d, &reg_l);
        exp_line = (((reg_h & 0xFF) << 8) | (reg_l & 0xFF));
        g_psensor->curr_exp = (exp_line * pinfo->t_row + 500) / 1000;
    }

    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line, vts, vblank;

    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;
    vts=(pinfo->vblank)+(g_sBG0803info[pinfo->currentmode].img_h)+8;

    if(2>exp_line)
        exp_line = 2;
    
    if(exp_line >= vts) 
    {
        vblank = exp_line+SENSOR_NON_EXP_LINE-(g_sBG0803info[pinfo->currentmode].img_h)-8;
        write_reg(0xf0,0x00);
        write_reg(0x21, vblank >> 8);
        write_reg(0x22, vblank & 0xff);
        pinfo->long_exp = 1;        
    }
    else
    {
        if (pinfo->long_exp) 
        {
            write_reg(0xf0,0x00);
            write_reg(0x21, ((pinfo->vblank >> 8) & 0xFF));
            write_reg(0x22, (pinfo->vblank & 0xFF));
            pinfo->long_exp = 0;
        }
    }
    
    write_reg(0x0c,((exp_line>>8)&0xFF) );
    write_reg(0x0d,(exp_line &0xFF) );
    write_reg(0x1d,0x02);

    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;
    // printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;
}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps,vts,new_vts;
    

    max_fps = g_sBG0803info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;

    vts= (g_sBG0803info[pinfo->currentmode].img_h)+(g_sBG0803info[pinfo->currentmode].vblank)+8;
    new_vts=vts * max_fps / fps;

    pinfo->vblank =new_vts-8-(g_sBG0803info[pinfo->currentmode].img_h);

    if((pinfo->vblank)>SENSOR_MAX_VBLANK)
        pinfo->vblank=SENSOR_MAX_VBLANK;
	
    isp_info("adjust_blk fps=%d, vblank=%d\n", fps, pinfo->vblank);

    write_reg(0xf0,0x00);
    write_reg(0x21, (pinfo->vblank) >> 8);
    write_reg(0x22, (pinfo->vblank) & 0xff);
    write_reg(0x1d,0x02);

    get_exp();
    set_exp(g_psensor->curr_exp);
	g_psensor->fps = fps;
}


static u32 get_gain(void)
{
    u32 a_gain, d_gain_h,d_gain_l;
    if (!g_psensor->curr_gain) 
    {
        write_reg(0xf0, 0x00); // Select Page 0
        read_reg(0xb1, &a_gain); // AGain
        read_reg(0xb7, &d_gain_h); // DGain h
        read_reg(0xb8, &d_gain_l); //DGain l    
        g_psensor->curr_gain = 64*(160/(a_gain+1))*(d_gain_h*100+(d_gain_l*100/256))/100;
    }
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    u8 again = 0;
    u8 dgain_h=0,dgain_l=0;
	static u8 pre_digit_h=0,pre_digit_l=0;
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

    again = gain_table[i-1].analog_gain;
    dgain_h = gain_table[i-1].digit_gain_h;
    dgain_l = gain_table[i-1].digit_gain_l;

    write_reg(0xF0, 0x00); // Select Page 0
    write_reg(0xb0, 0x00); // AGain formula default 0
    write_reg(0xb1, again); // AGain
    write_reg(0xc0, (again/10)); // AGain
    if((pre_digit_h!=dgain_h)||(pre_digit_l!=dgain_l))
    {
        isp_api_wait_frame_end(); //Dgain need to set in Vsync low V
        write_reg(0xb7, dgain_h); // DGain H
        write_reg(0xb8, dgain_l); // DGain L
    }
	
	pre_digit_h=dgain_h;
    pre_digit_l=dgain_l;
	
    g_psensor->curr_gain = gain_table[i-1].total_gain;

    return 0;

}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->flip)
    {	
        if (pinfo->mirror)
        {
            write_reg(0xf0, 0x00); 
            write_reg(0x05, 0x1c);
			      write_reg(0x1d, 0x02);
            g_psensor->bayer_type = BAYER_GB;
        }
		    else
        {
            write_reg(0xf0, 0x00); 
            write_reg(0x05, 0x1d);
            write_reg(0x1d, 0x02);	
            g_psensor->bayer_type = BAYER_GR;
        }
    }
    else
    {
        if (pinfo->mirror)
        {
            write_reg(0xf0, 0x00); 
            write_reg(0x05, 0x1d);
            write_reg(0x1d, 0x02);	
            g_psensor->bayer_type = BAYER_GB;
        }
        else
        {   
            write_reg(0xf0, 0x00); 
            write_reg(0x05, 0x1c);
            write_reg(0x1d, 0x02);
            g_psensor->bayer_type = BAYER_GR;
        }
    }
}

static int get_mirror(void)
{
    u32 reg;
    write_reg(0xf0, 0x00);  //select page0    
    read_reg(0x20, &reg);
    return (reg & BIT1) ? 1 : 0; 
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    
    u32 reg,reg2,reg3;

    pinfo->mirror= enable ;
    write_reg(0xf0, 0x00);  //select page0   
    read_reg(0x20, &reg);

    if(pinfo->flip)
    {
        reg |=BIT0;
        reg2 =0x06;
    }
    else
    {
        reg &=~BIT0;
        reg2 =0x05;
    }
	
    if (enable)
    {
        reg |= BIT1;
        reg3 = 0x00;
    }
    else
    {
        reg &= ~BIT1;
        reg3 = 0x01;
    }

    reg |= 0x0c;//make sure bit[3:2]=b'11
	
    write_reg(0xf0, 0x00);  //select page0    
    write_reg(0x20, reg);
    write_reg(0x13, reg2);
    write_reg(0x1d, 0x02);	
    write_reg(0xf0, 0x01);  //select page1  	
    write_reg(0x09, reg3);  //select page1 
    update_bayer_type();
    return 0;
}

static int get_flip(void)
{
    u32 reg;
    write_reg(0xf0, 0x00);  //select page0    
    read_reg(0x20, &reg);
    return (reg & BIT0) ? 1 : 0; 
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg,reg2,reg3;

    pinfo->flip= enable ;
    write_reg(0xf0, 0x00);  //select page0   	
    read_reg(0x20, &reg);

    if (pinfo->mirror)
    {
        reg |= BIT1;
        reg3 = 0x00;
    }
    else
    {
        reg &= ~BIT1;
        reg3 = 0x01;
    }
	
    if (enable)
    {
        reg |= BIT0;
        reg2 = 0x06;
    }
    else
    {
        reg &= ~BIT0;
        reg2 = 0x05;
    }

    reg |= 0x0c;//make sure bit[3:2]=b'11
	
    write_reg(0xf0, 0x00);  //select page0    
    write_reg(0x20, reg);
    write_reg(0x13, reg2);	
    write_reg(0x1d, 0x02);   
    write_reg(0xf0, 0x01);	
    write_reg(0x09, reg3);   	

    update_bayer_type();
    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    int ret = 0;
    
    isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT))
    {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }
	
    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sBG0803info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sBG0803info[pinfo->currentmode].img_h;	

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
    //isp_info("start initial...\n");
    pinfo->mirror = mirror;
    pinfo->flip = flip;

    pInitTable=sensor_1080P_init_regs;
    InitTable_Num=NUM_OF_1080P_INIT_REGS;

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
static void bg0803_deconstruct(void)
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

static int bg0803_construct(u32 xclk, u16 width, u16 height)
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
    pinfo->currentmode=MODE_1080P;
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = IF_PARALLEL;
    g_psensor->num_of_lane = 4;
    g_psensor->fmt =  g_sBG0803info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sBG0803info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->inv_clk = inv_clk;
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
    bg0803_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init bg0803_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp32X.ko(%x)!",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = bg0803_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit bg0803_exit(void)
{
    isp_unregister_sensor(g_psensor);
    bg0803_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(bg0803_init);
module_exit(bg0803_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor BG0803");
MODULE_LICENSE("GPL");
