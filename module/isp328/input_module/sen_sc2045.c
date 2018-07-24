/**
 * @file sen_sc2045.c
 * sc2045 sensor driver
 *
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 * 20160425 author RobertChuang  Ver1.0
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_sc2045"
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
#define SENSOR_NAME         "SC2045"
#define SENSOR_MAX_WIDTH        1920
#define SENSOR_MAX_HEIGHT       1080
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        4
#define Sensor_MAX_EXP          4095
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF
#define Gain2X                   128

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


static sensor_info_t g_sSC2045info[]=
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
    //[ParaList]
    {0x0100, 0x00}, // STOP STREAM
    {0x3039, 0x31},
    {0x3d08, 0x00},
    {0x3e14, 0x30},  //ramp config
    {0x3637, 0xbc},  //bf
    {0x3638, 0x84},
    {0x5000, 0x07},  //dpc
    {0x5001, 0x45},
    {0x5780, 0x15},  //manual mode
    {0x5782, 0x03},  //white thres
    {0x57a0, 0x10},  //black thres
    {0x3300, 0x08},  //timing
    {0x3306, 0x38}, 
    {0x3308, 0x10},
    {0x330b, 0x90},
    {0x3367, 0x08}, //precharge
    {0x330e, 0x60},
    {0x3334, 0x40}, //comp all high
    {0x3e03, 0x03}, //ae
    {0x3e08, 0x00},
    {0x3e01, 0x40},
    {0x3416, 0x10},
    {0x3e0f, 0x90},
    {0x3631, 0x80}, //analog
    {0x3635, 0xe0},
    {0x3620, 0x82},
    {0x3621, 0x28},
    {0x3627, 0x03},
    {0x3626, 0x02},
    {0x3622, 0x0e}, //blksun
    {0x3630, 0xb4},
    {0x3633, 0x8c},
    {0x363a, 0x14},
    // nonoverlap 
    {0x303f, 0x82},
    {0x3c03, 0x08},
    {0x320c, 0x03}, // hts=2640=1320x2 
    {0x320d, 0xe8},
    {0x3f00, 0x06}, //sram write
    {0x3f04, 0x01},
    {0x3f05, 0x48},
    {0x4501, 0xa4},
    {0x3039, 0x07}, //135M PLL
    {0x303a, 0x8e},
    {0x0100, 0x01}, //OUTPUT                
};
#define NUM_OF_1080P_DVP_INIT_REGS (sizeof(sensor_1080p_dvp_init_regs) / sizeof(sensor_reg_t))

typedef struct _gain_set
{
    uint16_t gain_reg; 
    //unsigned char analog;
    int gain_x;
} gain_set_t;

static gain_set_t gain_table[] =
{    
    {0x0010,  64},{0x0012,  72},{0x0014,  80},{0x0016,  88},
    {0x0018,  96},{0x001A, 104},{0x001C, 112},{0x001E, 120},
    {0x0020, 128},{0x0024, 144},{0x0028, 160},{0x002C, 176},
    {0x0030, 192},{0x0034, 208},{0x0038, 224},{0x003C, 240},
    {0x0040, 256},{0x0048, 288},{0x0050, 320},{0x0058, 352},
    {0x0060, 384},{0x0068, 416},{0x0070, 448},{0x0078, 480},
    {0x0080, 512},{0x0090, 576},{0x00A0, 640},{0x00B0, 704},
    {0x00C0, 768},{0x00D0, 832},{0x00E0, 896},{0x00F0, 960},
    {0x0100,1024},{0x0120,1152},{0x0140,1280},{0x0160,1408},
    {0x0180,1536},{0x01A0,1664},{0x01C0,1792},{0x01E0,1920},
    {0x0200,2048},{0x0240,2304},{0x0280,2560},{0x02C0,2816},    
    {0x0300,3072},{0x0340,3328},{0x0380,3584},{0x03C0,3840},    
    {0x03E0,3968}
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
	
    pinfo->pclk= g_sSC2045info[pinfo->currentmode].pclk;
	
    //isp_info("sc2045 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sSC2045info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC2045info[pinfo->currentmode].t_row;

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
    u32 exp_line,tmp_reg;

    pinfo->hts=g_sSC2045info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC2045info[pinfo->currentmode].t_row;
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1>exp_line)
        exp_line = 1;

    if (exp_line>=Sensor_MAX_EXP) {
        tmp_reg=(Sensor_MAX_EXP+SENSOR_NON_EXP_LINE)-0x200;		
		exp_line=Sensor_MAX_EXP;
        pinfo->t_row= (exp * 1000)/(Sensor_MAX_EXP+SENSOR_NON_EXP_LINE);                  
        pinfo->hts= pinfo->t_row*(g_sSC2045info[pinfo->currentmode].hts)/(g_sSC2045info[pinfo->currentmode].t_row);	
        write_reg(0x320C, ((pinfo->hts) >> 8) & 0xFF);    
        write_reg(0x320D, ((pinfo->hts) & 0xFF));			
        write_reg(0x320E, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
        write_reg(0x320F, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) & 0xFF));		
        write_reg(0x336A, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) >> 8) & 0xFF); 
        write_reg(0x336B, ((Sensor_MAX_EXP+SENSOR_NON_EXP_LINE) & 0xFF));
        write_reg(0x3368, (tmp_reg) >> 8);
        write_reg(0x3369, (tmp_reg) & 0xff);
        pinfo->long_exp = 1;		
    } else {
        write_reg(0x320C, (pinfo->hts) >> 8);
        write_reg(0x320D, (pinfo->hts) & 0xff);			
        if (exp_line > ((pinfo->vts)-SENSOR_NON_EXP_LINE)) {
            tmp_reg=((exp_line)+SENSOR_NON_EXP_LINE)-0x200;	
            write_reg(0x320E, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF);    
            write_reg(0x320F, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
            write_reg(0x336A, ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF); 
            write_reg(0x336B, ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF));
            write_reg(0x3368, (tmp_reg) >> 8);
            write_reg(0x3369, (tmp_reg) & 0xff);
            pinfo->long_exp = 1;
        } else {
            if (pinfo->long_exp) {
                tmp_reg=(pinfo->vts)-0x200;
                write_reg(0x320E, (pinfo->vts >> 8) & 0xFF);        
                write_reg(0x320F, (pinfo->vts & 0xFF));
                write_reg(0x336A, (pinfo->vts) >> 8);
                write_reg(0x336B, (pinfo->vts) & 0xff);
                write_reg(0x3368, (tmp_reg) >> 8);
                write_reg(0x3369, (tmp_reg) & 0xff);
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
    u32 max_fps,tmp_reg;

    max_fps = g_sSC2045info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sSC2045info[pinfo->currentmode].vts) * max_fps / fps;

    if ((pinfo->vts) > SENSOR_MAX_VTS)
        pinfo->vts=	SENSOR_MAX_VTS;

    tmp_reg=(pinfo->vts)-0x200;
   		
    write_reg(0x320E, (pinfo->vts) >> 8);
    write_reg(0x320F, (pinfo->vts) & 0xff);
    write_reg(0x336A, (pinfo->vts) >> 8);
    write_reg(0x336B, (pinfo->vts) & 0xff);
    write_reg(0x3368, (tmp_reg) >> 8);
    write_reg(0x3369, (tmp_reg) & 0xff);
           
    get_exp();
    set_exp(g_psensor->curr_exp);
	
	pinfo->fps = fps;
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    u32 gain_reg_h,gain_reg_l,gain;

    if (!g_psensor->curr_gain) {
        read_reg(0x3e08, &gain_reg_h);		
        read_reg(0x3e09, &gain_reg_l);
		gain=((gain_reg_h & 0xFF)<<8)|(gain_reg_l & 0xFF);
        g_psensor->curr_gain = (gain / 16) * 64;        
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

    write_reg(0x3e08, ((gain_table[i-1].gain_reg)>>8) & 0xFF);    
    write_reg(0x3e09, (gain_table[i-1].gain_reg) & 0xFF);
    	           

    if (gain_table[i-1].gain_x < Gain2X) {	  	  	  	  
        write_reg(0x3630, 0xB4);	      	      	      	      
    } else {	  
        write_reg(0x3630, 0x94);
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
        if (pinfo->flip) {	//mirror_flip
            write_reg(0x5001, 0xC5);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x06);       
        } else {            //mirror
            write_reg(0x5001, 0xC5);        
            write_reg(0x3220, 0x00);       
            write_reg(0x3221, 0x06);       
        }
    } else {
        if (pinfo->flip) {	//flip
            write_reg(0x5001, 0x45);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x00); 
        } else {            //normal
            write_reg(0x5001, 0x45);
            write_reg(0x3220, 0x00);        
            write_reg(0x3221, 0x00);       
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
        if (pinfo->mirror) { //mirror +flip
            write_reg(0x5001, 0xC5);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x06);       
        } else {		     //flip
            write_reg(0x5001, 0x45);        
            write_reg(0x3220, 0x06);       
            write_reg(0x3221, 0x00);       
        }
    } else {
        if (pinfo->mirror) {  //mirror
            write_reg(0x5001, 0xC5);        
            write_reg(0x3220, 0x00);       
            write_reg(0x3221, 0x06);       
        } else {		      //normal
            write_reg(0x5001, 0x45);
            write_reg(0x3220, 0x00);        
            write_reg(0x3221, 0x00);        
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
    g_psensor->out_win.width = g_sSC2045info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sSC2045info[pinfo->currentmode].img_h;	

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
static void sc2045_deconstruct(void)
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

static int sc2045_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->interface = g_sSC2045info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sSC2045info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sSC2045info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sSC2045info[pinfo->currentmode].bayer;
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
    sc2045_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc2045_init(void)
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

    if ((ret = sc2045_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc2045_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc2045_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc2045_init);
module_exit(sc2045_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor sc2045");
MODULE_LICENSE("GPL");
