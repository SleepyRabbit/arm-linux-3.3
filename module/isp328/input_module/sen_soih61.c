
/**
 * @file sen_soih61.c
 * OmniVision soih61 sensor driver
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
#define DEFAULT_IMAGE_WIDTH     1280
#define DEFAULT_IMAGE_HEIGHT    720
#define DEFAULT_XCLK            27000000
#define DEFAULT_INTERFACE       IF_PARALLEL

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

static uint inv_clk = 1;
module_param(inv_clk, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(inv_clk, "invert clock, 0:Disable, 1: Enable");

static ushort interface = DEFAULT_INTERFACE;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");
//=============================================================================
// global
//============================================================================
#define SENSOR_NAME         "soih61"
#define SENSOR_MAX_WIDTH    1280
#define SENSOR_MAX_HEIGHT   720

static sensor_dev_t *g_psensor = NULL;

typedef struct sensor_info {
    bool is_init;
    int mirror;
    int flip;
    u32 total_line;    
    u32 reg04;
    u32 t_row;          // unit is 1/10 us
    int htp;
} sensor_info_t;

typedef struct sensor_reg {
    u8 addr;
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_init_regs[] = { 
    //[JXH61_1280x720x60_DVP_10b.reg]
    //INI Start
    {0x12, 0x40},
    //DVP Setting
    {0x0D, 0x40},
    {0x1F, 0x04},
    //PLL Setting
    {0x0E, 0x1d},
    {0x0F, 0x09},
    {0x10, 0x1b},
    {0x11, 0x80},
    //Frame/Window
    {0x20, 0x40},
    {0x21, 0x06},
    {0x22, 0xF8},
    {0x23, 0x02},
    {0x24, 0x00},
    {0x25, 0xD0},
    {0x26, 0x25},
    {0x27, 0x3B},
    {0x28, 0x0D},
    {0x29, 0x01},
    {0x2A, 0x24},
    {0x2B, 0x29},
    {0x2C, 0x04},
    {0x2D, 0x00},
    {0x2E, 0xB9},
    {0x2F, 0x00},
    //Sensor Timing
    {0x30, 0x92},
    {0x31, 0x0A},
    {0x32, 0xAA},
    {0x33, 0x14},
    {0x34, 0x38},
    {0x35, 0x54},
    {0x42, 0x41},
    {0x43, 0x18},
    //Interface
    {0x1D, 0xFF},
    {0x1E, 0x1F},
    {0x6C, 0x90},
    {0x73, 0xB3},
    {0x70, 0x68},
    {0x76, 0x40},
    {0x77, 0x06},
    {0x72, 0x48},
    {0x6D, 0xA2},
    //Array/AnADC/PWC
    {0x48, 0x40},
    {0x60, 0xA4},
    {0x61, 0xFF},
    {0x62, 0x40},
    {0x65, 0x00},
    {0x67, 0x30},
    {0x68, 0x04},
    {0x69, 0x74},
    {0x6F, 0x24},
    //Black Sun
    {0x63, 0x59},
    {0x6A, 0x09},
    //AE/AG/ABLC
    {0x13, 0x87},
    {0x14, 0x80},
    {0x16, 0xC0},
    {0x17, 0x40},
    {0x18, 0x7B},
    {0x38, 0x35},
    {0x39, 0x98},
    {0x4a, 0x03},
    {0x49, 0x10},
    {0x66, 0x20},
    //INI End
    {0x12, 0x00}
    //PWDN Setting
};
#define NUM_OF_INIT_REGS (sizeof(sensor_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  ad_reg;
    u32 gain_x;
} gain_set_t;

static const gain_set_t gain_table[] = {
    {0x00, 64  }, {0x01, 68  }, {0x02, 72  }, {0x03, 76  }, 
    {0x04, 80  }, {0x05, 84  }, {0x06, 88  }, {0x07, 92  }, 
    {0x08, 96  }, {0x09, 100 }, {0x0A, 104 }, {0x0B, 108 }, 
    {0x0C, 112 }, {0x0D, 116 }, {0x0E, 120 }, {0x0F, 124 }, 
    {0x10, 128 }, {0x11, 136 }, {0x12, 144 }, {0x13, 152 }, 
    {0x14, 160 }, {0x15, 168 }, {0x16, 176 }, {0x17, 184 }, 
    {0x18, 192 }, {0x19, 200 }, {0x1A, 208 }, {0x1B, 216 }, 
    {0x1C, 224 }, {0x1D, 232 }, {0x1E, 240 }, {0x1F, 248 }, 
    {0x20, 256 }, {0x21, 272 }, {0x22, 288 }, {0x23, 304 }, 
    {0x24, 320 }, {0x25, 336 }, {0x26, 352 }, {0x27, 368 }, 
    {0x28, 384 }, {0x29, 400 }, {0x2A, 416 }, {0x2B, 432 }, 
    {0x2C, 448 }, {0x2D, 464 }, {0x2E, 480 }, {0x2F, 496 }, 
    {0x30, 512 }, {0x31, 544 }, {0x32, 576 }, {0x33, 608 }, 
    {0x34, 640 }, {0x35, 672 }, {0x36, 704 }, {0x37, 736 }, 
    {0x38, 768 }, {0x39, 800 }, {0x3A, 832 }, {0x3B, 864 }, 
    {0x3C, 896 }, {0x3D, 928 }, {0x3E, 960 }, {0x3F, 992 }, 
#if 0
    //gain16~32
    {0x40, 1024}, {0x41, 1088}, {0x42, 1152}, {0x43, 1216}, 
    {0x44, 1280}, {0x45, 1344}, {0x46, 1408}, {0x47, 1472}, 
    {0x48, 1536}, {0x49, 1600}, {0x4A, 1664}, {0x4B, 1728}, 
    {0x4C, 1792}, {0x4D, 1856}, {0x4E, 1920}, {0x4F, 1984}  
#endif	
};  
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            (0x60 >> 1)//SOI and GM the same ID
//#define I2C_ADDR            (0x6C >> 1)//FUHO
#include "i2c_common.c"

//=============================================================================
// internal functions
//=============================================================================
static int read_reg(u32 addr, u32 *pval)
{
    struct i2c_msg  msgs[2];
    unsigned char   tmp[1], tmp2[2];

    tmp[0]        = addr & 0xFF;
    msgs[0].addr  = I2C_ADDR;
    msgs[0].flags = 0; // write
    msgs[0].len   = 1;
    msgs[0].buf   = tmp;

    tmp2[0]       = 0; // clear
    msgs[1].addr  = I2C_ADDR;
    msgs[1].flags = 1; // read
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

    return sensor_i2c_transfer(&msgs, 1);
}

static void adjust_vblk(int fps)
{    
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 reg_l, reg_h;  
    
    if((fps > 60) || (fps < 5))
        return;                                                                            
    
    if((fps <= 60) && (fps >= 5)){
        pinfo->total_line = g_psensor->pclk / fps / pinfo->htp;
        reg_h = (pinfo->total_line >> 8) & 0xFF;
        reg_l = pinfo->total_line & 0xFF;
        write_reg(0x23, reg_h);
        write_reg(0x22, reg_l);                
        isp_dbg("pinfo->total_line=%d\n",pinfo->total_line);
    }
                
    //Update fps status
    g_psensor->fps = fps;  
    isp_dbg("g_psensor->fps=%d\n", g_psensor->fps);                          
    
}

static u32 get_pclk(u32 xclk)
{
    u32 pixclk;

    pixclk = 36450000;//xclk=27MHz@30fps           
          
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
    g_psensor->img_win.x = 0;//((SENSOR_MAX_WIDTH - width) >> 1) &~BIT0;
    g_psensor->img_win.y = 0;//((SENSOR_MAX_HEIGHT - height) >> 1) &~BIT0;
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
        read_reg(0x02, &sw_u);//high byte
        read_reg(0x01, &sw_l);//low  byte        
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
           
    max_exp_line = pinfo->total_line - 5;//Hardware limitation
   
    if (lines > max_exp_line) {
        tmp = max_exp_line;      
    } else {
        tmp = lines;
    }    

    if (lines > max_exp_line) {
        write_reg(0x23, (lines >> 8) & 0xFF);
        write_reg(0x22, (lines & 0xFF));                
        //printk("lines = %d\n",lines); 
        write_reg(0x01, (lines & 0xFF));
        write_reg(0x02, ((lines >> 8) & 0xFF));
    }else{
        write_reg(0x23, (pinfo->total_line >> 8) & 0xFF);
        write_reg(0x22, (pinfo->total_line & 0xFF)); 
        write_reg(0x01, (tmp & 0xFF));
        write_reg(0x02, ((tmp >> 8) & 0xFF));
    } 
    g_psensor->curr_exp = ((lines * pinfo->t_row) + 500) / 1000;
     
    //printk("g_psensor->curr_exp=%d,max_exp_line=%d,lines=%d,tmp=%d\n", g_psensor->curr_exp, max_exp_line, lines,tmp);

    return 0;
}

static u32 get_gain(void)
{
    u32 gain, reg, mul0, mul1=0;

     if (g_psensor->curr_gain == 0) {       
        read_reg(0x0, &reg);//analog
        read_reg(0x0d, &mul0);//digital
        mul0 = mul0 & 0x03;//[1:0]
        gain = (1+ (reg >> 6)) * (1+ (reg >> 5)) * (1+ (reg >> 4)) * (1 + (reg & 0x0F)/16);
        //printk("gain0=%d\n", gain);
        gain = gain * 1024;
        //printk("gain1=%d\n", gain);
        if(mul0 == 0)
            mul1 = 1;//1x
        else if(mul0 == 1)    
            mul1 = 2;//2x
            
        gain = gain * mul1;     
        g_psensor->curr_gain = gain;
        
        //printk("reg=%d,mul0=%d,mul1=%d\n", reg, mul0, mul1);
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
    
    write_reg(0x00, ad_gain);
    
    g_psensor->curr_gain = gain_table[i-1].gain_x;
        
    //printk("g_psensor->curr_gain=%d, ad_gain=0x%x\n",g_psensor->curr_gain, ad_gain);

    return ret;
}

static void update_bayer_type(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (pinfo->mirror) {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_RG;
        else
            g_psensor->bayer_type = BAYER_GB;
    } else {
        if (pinfo->flip)
            g_psensor->bayer_type = BAYER_GR;
        else            
            g_psensor->bayer_type = BAYER_BG;
    }
}

static int get_mirror(void)
{
    u32 reg;
    read_reg(0x12, &reg);
    return (reg & BIT5) ? 1 : 0;
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;    

    if (enable) {
        pinfo->reg04 |= BIT5;
    } else {
        pinfo->reg04 &=~BIT5;
    }

    write_reg(0x12, pinfo->reg04);
    pinfo->mirror = enable;  
    update_bayer_type();  

    return 0;
}

static int get_flip(void)
{
    u32 reg;
    read_reg(0x12, &reg);
    return (reg & BIT4) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    if (enable) {
        pinfo->reg04 |= BIT4;
    } else {
        pinfo->reg04 &=~BIT4;
    }
    write_reg(0x12, pinfo->reg04);
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
    int ret = 0;
    u32 reg_h, reg_l, Chip_id;
    int i;    

    if (pinfo->is_init)
        return 0;        
       
    isp_info("Sensor Interface : PARALLEL\n");
    isp_info("Sensor Name : H61\n");
    // write initial register value
    for (i=0; i<NUM_OF_INIT_REGS; i++) {
        if (write_reg(sensor_init_regs[i].addr, sensor_init_regs[i].val) < 0) {
            isp_err("failed to initial %s!\n", SENSOR_NAME);
            return -EINVAL;
        }            
    }  

    //Check Chip version
    read_reg(0x09, &Chip_id);
    if(Chip_id == 0x00){//AA VER.       
        isp_info("AA VER.\n");
    }else if(Chip_id == 0x01){//AC VER.        
        isp_info("AC VER.\n");
    }else if(Chip_id == 0x03){//AD VER.        
        isp_info("AD VER.\n");
    }
           
    read_reg(0x23, &reg_h);
    read_reg(0x22, &reg_l);
    pinfo->total_line = ((reg_h << 8) | reg_l);         
    g_psensor->pclk = get_pclk(g_psensor->xclk); 
        
    read_reg(0x21, &reg_h);
    read_reg(0x20, &reg_l);
    pinfo->htp = ((reg_h << 8) | reg_l); 
    pinfo->t_row = (pinfo->htp * 10000) / (g_psensor->pclk / 1000);    
        
    g_psensor->min_exp = (pinfo->t_row + 999) / 1000;             
    
    isp_dbg("pinfo->total_line=%d, pinfo->htp=%d, pinfo->t_row=%d, g_psensor->min_exp=%d\n", pinfo->total_line, pinfo->htp, pinfo->t_row, g_psensor->min_exp);    

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
static void soih61_deconstruct(void)
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

static int soih61_construct(u32 xclk, u16 width, u16 height)
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
    g_psensor->num_of_lane = 1;
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
    g_psensor->max_exp = 5000; // 0.5 sec
    g_psensor->min_gain = gain_table[0].gain_x;
    g_psensor->max_gain = gain_table[NUM_OF_GAINSET - 1].gain_x;    
    g_psensor->exp_latency = 1;
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
        
    if (g_psensor->interface == IF_MIPI){
	      // no frame if init this sensor after mipi inited
    	  if ((ret = init()) < 0)
            goto construct_err;
	  }        
        
    return 0;

construct_err:
    soih61_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init soih61_init(void)
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

    if ((ret = soih61_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit soih61_exit(void)
{
    isp_unregister_sensor(g_psensor);
    soih61_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(soih61_init);
module_exit(soih61_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor soih61");
MODULE_LICENSE("GPL");
