/**
 * @file sen_sc1020.c
 * SC1020 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 * 20151106 author RobertChuang
 */
//no finish
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_sc1020"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1280 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT     720 //ISP Output size
#define DEFAULT_XCLK        27000000

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

//=============================================================================
// global
//=============================================================================
#define SENSOR_NAME          "SC1020"
#define SENSOR_MAX_WIDTH        1280
#define SENSOR_MAX_HEIGHT        720
#define SENSOR_MAX_VTS         65535
#define SENSOR_NON_EXP_LINE        4
#define SENSOR_ISINIT              0
#define SENSOR_LONG_EXP            0
#define DELAY_CODE            0xFFFF

//normal setting
#define  DEFAULT_720P_MODE              MODE_720P
#define  DEFAULT_720P_VTS                     750
#define  DEFAULT_720P_HTS                    1800
#define  DEFAULT_720P_WIDTH                  1280
#define  DEFAULT_720P_HEIGHT                  720
#define  DEFAULT_720P_MIRROR                    0
#define  DEFAULT_720P_FLIP                      0
#define  DEFAULT_720P_TROW                    444
#define  DEFAULT_720P_PCLK               40500000
#define  DEFAULT_720P_MAXFPS                   30
#define  DEFAULT_720P_LANE                      1
#define  DEFAULT_720P_RAWBIT                RAW12
#define  DEFAULT_720P_BAYER              BAYER_BG
#define  DEFAULT_720P_DVP_INTERFACE   IF_PARALLEL

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
    enum SENSOR_IF interface;		
} sensor_info_t;


static sensor_info_t g_sSC1020info[]=
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
    DEFAULT_720P_DVP_INTERFACE,    
    }
};

typedef struct sensor_reg {
    u16 addr;  
    u16 val;
} sensor_reg_t;

static sensor_reg_t sensor_720P_init_regs[] ={
    {0x3000,0x01}, ///////////soft reset
    {0x3003,0x01}, //soft reset
    {0x3010,0x01}, //
    {0x3011,0xd6}, //27MHz intput sclk for pclk 40.5MHz output
    {0x3004,0x04}, //digtal clk div
    {0x3208,0x05}, //reg_dvpho[15:8]//isp output hsize reg value
    {0x3209,0x00}, //reg_dvpho[7:0] //isp output hsize reg value
    {0x320a,0x02}, //reg_dvpho[15:8]//isp output vsize reg value
    {0x320b,0xd0}, //reg_dvpho[7:0] //isp output vsize reg value
    {0x3210,0x00}, //xstart
    {0x3211,0x52}, //
    {0x3212,0x00}, //ystart
    {0x3213,0x08}, //
    {0x3928,0x00}, //bit[0] 0_8channel_en 1_4channel_en
    {0x3500,0x10}, //bit[5]max_int_time_sel,turn off sub-1row exposure control in AEC
    {0x5000,0x00}, //isp_ctrl [0] cip_en [1]wc_en [2] bc_en [3] awb_gain_en[6] gamma_en [7] lens_en09 //0x09
    {0x3e0e,0x52}, //[6] dcg_auto_en,bit[3:0]finegain_start
    {0x3e03,0x03}, //open/close AEC/AGC 0x00 open / 0x03 close
    {0x3300,0x33}, //
    {0x3301,0x61}, //
    {0x3302,0x30},
    {0x3303,0x50}, //
    {0x3304,0x5e}, //
    {0x3305,0x72}, //
    {0x331e,0x56}, ////pre_charge tx width
    {0x3635,0x14}, //vref_sa1  reduce sa1 current 10mA
    {0x3631,0x88}, //hvdd
    {0x3622,0x1e}, //open blksun cancellation
    {0x3630,0x60}, //
    {0x3634,0x90}, //txvdd 3V
    {0x3606,0x00}, //
    {0x3633,0xcc}, //
    {0x3620,0x8F}, //
    {0x3623,0x08}, //
    {0x3600,0x3c}, //reduce adc current 5mA
    {0x320c,0x07}, //hts[15:8]
    {0x320d,0x08}, //hts[7:0]
    {0x320e,0x02}, //vts[15:8]
    {0x320f,0xee}, //vts[7:0]
    {0x3d08,0x00}, //bit[0] dvpclk_inv
    {0x3907,0x03}, //target
    {0x3908,0xFF}, //
    {0x3415,0x00}, // 
    {0x3416,0xc0}, //
    {0x3100,0x07}, //
    {0x3102,0x10}, //
    {0x3103,0x80}, //
    {0x3104,0x0f}, //
    {0x3105,0xc0}, //
    {0x310a,0x0f}, //
    {0x310b,0xc0}, //
    {0x310c,0x10}, //
    {0x310d,0x70}, //
    {0x3330,0x12}, //rs timing 0x00
    {0x3324,0x00}, //
    {0x3325,0x00}, //
    {0x3326,0x00}, //
    {0x3327,0x00}, //
    {0x3320,0x06}, //
    {0x3321,0xa2}, //
    {0x3322,0x01}, //
    {0x3323,0x47}, //
    {0x332c,0x03}, //
    {0x332d,0x00}, //
    {0x332e,0x07}, //
    {0x332f,0x07}, //
    {0x3328,0x06}, //
    {0x3329,0xa2}, //
    {0x332a,0x01}, //
    {0x332b,0x47}, //
    {0x3610,0x03},
    {DELAY_CODE,5000},  
    {0x3620,0x85}, //increase vrbit voltage for easier transfer and faster powerup
    {0x3630,0x60}, //adjust for blksun cancel
    // new all high timing
    {0x3300,0x22},
    {0x3301,0x22},
    {0x3315,0x44},
    {0x330d,0x48},
    {0x3307,0x03},
    {0x3780,0x11},             
};
#define NUM_OF_720P_INIT_REGS (sizeof(sensor_720P_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  acg;
   // u8  hcg;
    u16 total_gain;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00,    64}, {0x02,    72}, {0x04,    79}, {0x06,    87},
    {0x08,    94}, {0x0B,   105}, {0x0D,   113}, {0x0F,   120},
    {0x81,   132}, {0x83,   147}, {0x85,   160}, {0x87,   175},
    {0x8A,   197}, {0x8B,   205}, {0x8E,   226}, {0x91,   248},	
    {0x91,   263}, {0x93,   293}, {0x95,   320}, {0x97,   350},
    {0x99,   380}, {0x9C,   425}, {0x9E,   452}, {0xB0,   497},	
    {0xB1,   526}, {0xB3,   586}, {0xB5,   641}, {0xB8,   730},
    {0xBA,   790}, {0xBC,   849}, {0xBE,   904}, {0xF0,   993},	
    {0xF1,  1053}, {0xF3,  1172}, {0xF5,  1281}, {0xF7,  1401},
    {0xFA,  1579}, {0xFC,  1699}, {0xFE,  1808}, {0xFF,  1867},	

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
    u8 buf[2],buf2[2];
       
    buf[0]        = (addr >> 8) & 0xFF;
    buf[1]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0;
    msgs[0].len   = 2;
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

    buf[0]     = (addr >> 8) & 0xFF;
    buf[1]     = addr & 0xFF;
    buf[2]     = val & 0xFF;
    msgs.addr  = I2C_ADDR;
    msgs.flags = 0;
    msgs.len   = 3;
    msgs.buf   = buf;

    //isp_info("write_reg %04x %02x\n", addr, val);
    return sensor_i2c_transfer(&msgs, 1);
}

static u32 get_pclk(u32 xclk)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    pinfo->pclk= g_sSC1020info[pinfo->currentmode].pclk;
	
    //isp_info("SC1020 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts=g_sSC1020info[pinfo->currentmode].hts;
    pinfo->t_row=g_sSC1020info[pinfo->currentmode].t_row;

    //isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;

    max_fps = g_sSC1020info[pinfo->currentmode].fps;
    if (fps > max_fps)
        fps = max_fps;
    pinfo->vts = (g_sSC1020info[pinfo->currentmode].vts) * max_fps / fps;

    //isp_info("adjust_blk fps=%d, vts=%d\n", fps, pinfo->vts);

    write_reg(0x320E, (pinfo->vts) >> 8);
    write_reg(0x320F, (pinfo->vts) & 0xff);
    g_psensor->fps = fps;
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 lines, exp_line_l,exp_line_h;

    if (!g_psensor->curr_exp)
    {
        read_reg(0x3E01, &exp_line_h);
        read_reg(0x3E02, &exp_line_l);
        lines = (((exp_line_h & 0xFF) << 4)|((exp_line_l>>4) & 0x0F));
        g_psensor->curr_exp = (lines * (pinfo->t_row) + 500) / 1000;
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    u32 exp_line,denoisevalue,temp_vts,temp_vts_h,temp_vts_l;
    
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1>exp_line)
        exp_line = 1;
	if(4095<=exp_line)
	    exp_line = 4095;	

    if (exp_line >((pinfo->vts)-SENSOR_NON_EXP_LINE)) 
    {
        temp_vts_h= ((exp_line+SENSOR_NON_EXP_LINE) >> 8) & 0xFF;
	    temp_vts_l= ((exp_line+SENSOR_NON_EXP_LINE) & 0xFF);
        write_reg(0x320E, temp_vts_h);    
        write_reg(0x320F, temp_vts_l);
        pinfo->long_exp = 1;
    } 
    else 
    {
        temp_vts_h= (pinfo->vts >> 8) & 0xFF;
	    temp_vts_l= (pinfo->vts & 0xFF);  
		
        if (pinfo->long_exp)
        {            
            write_reg(0x320E, temp_vts_h);        
            write_reg(0x320F, temp_vts_l);
            pinfo->long_exp = 0;
        }

    }

    write_reg(0x3E01, ((exp_line >>4) & 0xFF));
    write_reg(0x3E02, ((exp_line & 0xF)<<4)|0x0);

    temp_vts=(temp_vts_h << 8)|(temp_vts_l & 0xff);

    if(exp_line <= (temp_vts - 304))
	{
	    denoisevalue = 0x80;
	}
	else if((exp_line < (temp_vts - 4)) && (exp_line > (temp_vts - 304)))
	{	      
	    denoisevalue =17*(temp_vts-4-exp_line)/50 + 0x1a;      
	}
	else
	{
	    denoisevalue = 0x1a;
	}

	write_reg(0x331e,denoisevalue);

    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

//    printk("set_exp lines=%d  t_row=%d sw_lines_max=%d exp=%d\n",lines, pinfo->t_row, pinfo->sw_lines_max,g_psensor->curr_exp);

    return ret;

}

static u32 get_gain(void)
{
    u32 again_int,again_fine;
    u32 again_reg;
   
    if (g_psensor->curr_gain == 0) 
    {
	        read_reg(0x3E09, &again_reg);
            again_int = (again_reg >> 4) & 0x7;
			again_fine = again_reg & 0xF;
            if(0==again_reg)
                again_int=1;
            else if(1==again_reg)
                again_int=2;
            else if(2==again_reg)
                again_int=4;  
            else
                again_int=8;      
            g_psensor->curr_gain= 64*(again_int*10000+(10000*again_fine/16))/10000;    		
    }
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    u8 acg = 0;
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
	
    if(gain_table[i-1].total_gain<121)
        write_reg(0x3630, 0x80);
	else
		write_reg(0x3630, 0x60);

    acg = gain_table[i-1].acg;
    write_reg(0x3E09, acg);
    
    g_psensor->curr_gain = gain_table[i-1].total_gain;

    return 0;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->flip){
		
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_GR;
        else
            g_psensor->bayer_type = BAYER_GR;
    }
    else{
        if (pinfo->mirror)
            g_psensor->bayer_type = BAYER_BG;
        else
            g_psensor->bayer_type = BAYER_BG;
    }
}

static int get_mirror(void)
{
    u32 reg;
    read_reg(0x321D, &reg);
    return (reg & BIT0) ? 1 : 0;    
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;

    pinfo->mirror= enable; 

    read_reg(0x321D, &reg);
    
    if (enable)
    {    
        reg |= BIT0;
        write_reg(0x321D, reg);
		
        //disable RNC
        write_reg(0x3400, 0x52);
        //set BLC target
        write_reg(0x3907, 0x00);        //set BLC target
        write_reg(0x3908, 0xC0);        //set BLC target
        write_reg(0x3211, 0x07);
        write_reg(0x3781, 0x08);          
              

    }
    else
    {
        reg &= ~BIT0;
        write_reg(0x321D, reg);

		//enable RNC
        write_reg(0x3400, 0x53);        
        //BLC target = {0x3415, 0x3416} != {0x3907, 0x3908}
        write_reg(0x3907, 0x03);        //set random raw noise buffer size
        write_reg(0x3908, 0xFF);        //set random raw noise buffer size
        write_reg(0x3211, 0x52);     
        write_reg(0x3781, 0x10);      
    }
    
    update_bayer_type();

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
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg;
    
    pinfo->flip= enable;

    read_reg(0x321C, &reg);

    if (enable)
        reg |= BIT6;
    else
        reg &= ~BIT6;
    write_reg(0x321C, reg);

	update_bayer_type();

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

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sSC1020info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sSC1020info[pinfo->currentmode].img_h;	

    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;
    g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;
    g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;
    //isp_info("initial end ...\n");
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
    int i, ret = 0;
    
    if (pinfo->is_init)
        return 0;

    pinfo->mirror = mirror;
    pinfo->flip = flip;

    pInitTable=sensor_720P_init_regs;
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
 	
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void sc1020_deconstruct(void)
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

static int sc1020_construct(u32 xclk, u16 width, u16 height)
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
    pinfo->currentmode=MODE_720P;
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sSC1020info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sSC1020info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sSC1020info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sSC1020info[pinfo->currentmode].bayer;
    g_psensor->hs_pol = ACTIVE_HIGH;
    g_psensor->vs_pol = ACTIVE_HIGH;
    g_psensor->inv_clk = 0;
    g_psensor->img_win.x = 0;
    g_psensor->img_win.y = 0;
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;    
    g_psensor->min_exp = 1;    //100us
    g_psensor->max_exp = 5000; // 0.5 sec ,500ms
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
    sc1020_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init sc1020_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        isp_err("input module version(%x) is not equal to fisp320.ko(%x)!",
             ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        ret = -EINVAL;
        goto init_err1;
    }

    g_psensor = kzalloc(sizeof(sensor_dev_t), GFP_KERNEL);
    if (!g_psensor) {
        ret = -ENODEV;
        goto init_err1;
    }

    if ((ret = sc1020_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit sc1020_exit(void)
{
    isp_unregister_sensor(g_psensor);
    sc1020_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(sc1020_init);
module_exit(sc1020_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor 	SC1020");
MODULE_LICENSE("GPL");
