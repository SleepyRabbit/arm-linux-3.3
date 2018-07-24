/**
 * @file sen_hm2130.c
 * hm2130 sensor driver
 *
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 * 20160406 author RobertChuang
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>

//#define PFX "sen_hm2130"
#include "isp_dbg.h"
#include "isp_input_inf.h"
#include "isp_api.h"

//=============================================================================
// module parameters
//=============================================================================
#define DEFAULT_IMAGE_WIDTH     1920 //ISP Output size	
#define DEFAULT_IMAGE_HEIGHT    1080 //ISP Output size
#define DEFAULT_XCLK        27000000
#define DEFAULT_INTERFACE    IF_MIPI

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

static ushort interface = DEFAULT_INTERFACE;
module_param( interface, ushort, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "interface type");

//=============================================================================
// global parameters
//=============================================================================
#define SENSOR_NAME          "HM2130"
#define SENSOR_MAX_WIDTH         1920
#define SENSOR_MAX_HEIGHT        1080
#define SENSOR_MAX_VTS          65534
#define SENSOR_NON_EXP_LINE        10
#define SENSOR_ISINIT               0
#define SENSOR_LONG_EXP             0
#define DELAY_CODE             0xFFFF

//mipi setting
#define  DEFAULT_1080P_MIPI_MODE    MODE_MIPI_1080P
#define  DEFAULT_1080P_MIPI_VTS                1113
#define  DEFAULT_1080P_MIPI_HTS                1280
#define  DEFAULT_1080P_MIPI_WIDTH              1920
#define  DEFAULT_1080P_MIPI_HEIGHT             1080
#define  DEFAULT_1080P_MIPI_MIRROR                0
#define  DEFAULT_1080P_MIPI_FLIP                  0
#define  DEFAULT_1080P_MIPI_TROW                299
#define  DEFAULT_1080P_MIPI_PCLK          427500000
#define  DEFAULT_1080P_MIPI_MAXFPS               30
#define  DEFAULT_1080P_MIPI_LANE                  2
#define  DEFAULT_1080P_MIPI_RAWBIT            RAW10
#define  DEFAULT_1080P_MIPI_BAYER          BAYER_BG
#define  DEFAULT_1080P_MIPI_INTERFACE       IF_MIPI
#define  DEFAULT_1080P_MIPI_UPDATEREG             0

static sensor_dev_t *g_psensor = NULL;

//sensor Mode
enum SENSOR_MODE {
    MODE_MIPI_1080P = 0,  
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
    int updatereg;
} sensor_info_t;


static sensor_info_t g_sHM2130info[]=
{
    {
        SENSOR_ISINIT,
        DEFAULT_1080P_MIPI_MODE,        
        DEFAULT_1080P_MIPI_VTS,
        DEFAULT_1080P_MIPI_HTS,
        DEFAULT_1080P_MIPI_WIDTH,
        DEFAULT_1080P_MIPI_HEIGHT,
        DEFAULT_1080P_MIPI_MIRROR,
        DEFAULT_1080P_MIPI_FLIP,
        DEFAULT_1080P_MIPI_TROW,
        DEFAULT_1080P_MIPI_PCLK,
        DEFAULT_1080P_MIPI_MAXFPS,
        DEFAULT_1080P_MIPI_LANE,
        DEFAULT_1080P_MIPI_RAWBIT,
        DEFAULT_1080P_MIPI_BAYER,
        SENSOR_LONG_EXP,
        DEFAULT_1080P_MIPI_INTERFACE,
        DEFAULT_1080P_MIPI_UPDATEREG,   
    }, 
};

typedef struct sensor_reg {
    u16 addr;  
    u8 val;
} sensor_reg_t;

static sensor_reg_t sensor_1080p_mipi_init_regs[] = {
    //B version
    //---------------------------------------------------
    //soft reset
    {0x0103, 0x00},//soft reset
    //---------------------------------------------------
    // PLL ( 1080p60fps,RAW10,MIPI: pclk=85MHz, pkt_clk=114MHz )
    //---------------------------------------------------
    // adclk
    {0x0304 ,0x2A},// [5]=pll_en_d, [4]=pll_bypass_d, [3:2]=RP_ctrl[1:0], [1:0]=CP_ctrl[1:0]
    {0x0305 ,0x0C},// pre_div[4:0] 0C=> f_base=2MHz
    {0x0307 ,0x4C},// post_div[7:0] AA=> vco=340MHz
    {0x0303 ,0x04},// vt_sys[4:0] 04=> adclk=vco, sys_clk=vco/8; 00=> adclk=vco, sys_clk=vco/4
    // mipi_clk
    {0x0309 ,0x00},// MIPI_CP/CN=vco/n(full-size), =vco/2n(bin2 or sub2 mode)
    {0x030A ,0x0A},// mipi_pll3_d[4:0] [5,1,0]=CP_ctrl[2:0], [3:2]=RP_ctrl[1:0] [4]:bypass
    {0x030D ,0x02},// pre_div[4:0] 02=> f_base=12MHz
    {0x030F ,0x12},// post_div[6:0] 14=> mipi_clk=240MHz
    // SSCG
    {0x5268 ,0x01},// SSCG_disable, 1=disable 
    {0x5264 ,0x24},// SSCG upper HB(fixed value)
    {0x5265 ,0x92},// SSCG upper LB(fixed value)
    {0x5266 ,0x23},// SSCG lower HB 23,07,02~0.4438%
    {0x5267 ,0x07},// SSCG lower LB
    {0x5269 ,0x02},// SSCG jump step
    //activate PLL
    {0x0100 ,0x02},// mode_select: 0h=stby, 2h=pwr_up, 1h/3h=streaming.
    //---------------------------------------------------
    // B9 mipi-tx pseudo-buffer setting
    //---------------------------------------------------
    {0x0100 ,0x02},// mode_select: 0h=stby, 2h=pwr_up, 1h/3h=streaming.
    {0x0111 ,0x01},// B9: 1'b0:1lane, 1'b1:2lane 
    {0x0112 ,0x0A},// DataFormat : 0x08:RAW8, 0x0A:RAW10, 0x0C:RAW12 
    {0x0113 ,0x0A},// DataFormat : 0x08:RAW8, 0x0A:RAW10, 0x0C:RAW12 
    {0x4B20 ,0x8E},// clock always on(9E) / clock always on while sending packet(BE)
    {0x4B18 ,0x12},// [7:0] FULL_TRIGGER_TIME
    {0x4B02 ,0x05},// TLPX
    {0x4B43 ,0x07},// CLK_PREPARE
    {0x4B05 ,0x1C},// CLK_ZERO
    {0x4B0E ,0x00},// clk_front_porch
    {0x4B0F ,0x0D},// CLK_BACK_PORCH
    {0x4B06 ,0x06},// CLK_TRAIL
    {0x4B39 ,0x0B},// CLK_EXIT
    {0x4B42 ,0x07},// HS_PREPARE
    {0x4B03 ,0x0C},// HS_ZERO
    {0x4B04 ,0x07},// HS_TRAIL
    {0x4B3A ,0x0B},// HS_EXIT
    {0x4B51 ,0x80},//
    {0x4B52 ,0x09},//
    {0x4B52 ,0xC9},//
    {0x4B57 ,0x07},//
    {0x4B68 ,0x6B},// [7:6]:VC[1:0], [5:0]:DT[5:0]    
    //---------------------------------------------------
    // B9 Setting (ID:24)
    //---------------------------------------------------
    {0x0350 ,0x37},// [4]d_gain enable, [5]analog d_gain enable 
    {0x5030 ,0x10},// [0]:Stagger HDR enable
    {0x5032 ,0x02},// HDR INTG Long(HB)
    {0x5033 ,0xD1},// HDR INTG Long(LB)
    {0x5034 ,0x01},// HDR INTG Short(HB)
    {0x5035 ,0x67},// HDR INTG Short(LB)
    {0x5229 ,0x90},// [0]oe_d
    {0x5061 ,0x00},// Parallel out [0] : 1/0 -> on/off, [1]:Long/Short : 1/0 ,[2]:parallel clock gating: 1: gating 0:no gating
    {0x5062 ,0x94},//
    {0x50F5 ,0x06},//
    {0x5230 ,0x00},// [1] 12bit mode enable
    {0x526C ,0x00},// [1:0]TDM_sel_d<1:0> 00/01/10/11=3/4/5/6 bit TDM.
    // Change 0x5230, 0x0112, 0x0113, 0x526C,0x50AD, 0x50AE together for RAW10<-->RAW12.
    {0x5254 ,0x08},// Staggered HDR INTG-S Delay(LB)
    {0x522B ,0x00},// NonHDR set to 0x00, HDR set to 0x78
    {0x4144 ,0x08},//
    {0x4148 ,0x03},//
    {0x4024 ,0x40},// [6]:MIPI enable
    {0x4B66 ,0x00},//
    {0x4B31 ,0x06},// [3]Inverse Digital PKT Clock 0 : PKT = PKT 1:PKT = PKT 
    
    //---------------------------------------------------
    // Frame Length & Row Time
    //---------------------------------------------------
    // For full size
    // Frame length >= 1102
    // Frame length >= (INTG+2)
    // Frame length is even number
    //Frame Length 1088 + 12 + 2
    {0x0340 ,0x04},// frame length(HB)
    {0x0341 ,0x59},// frame length(LB)
    //Row Time 
    {0x0342 ,0x05},// line length(HB)
    {0x0343 ,0x00},// line length(LB)
    //Window Size
    {0x034C ,0x07},// x_output_size(HB)
    {0x034D ,0x88},// x_output_size(LB)
    {0x034E ,0x04},// y_output_size(HB)
    {0x034F ,0x40},// y_output_size(LB)    
    {0x0101 ,0x00},// [0]=mirror, [1]=flip
    {0x4020 ,0x10},// [4]: 0:pclk=pclk, 1:pclk=~pclk.    
    //---------------------------------------------------
    // Analog
    //---------------------------------------------------    
    {0x50DD ,0x01},// GAIN STRETEGY: 0=SMIA, 1=HII
    {0x0350 ,0x37},// 	;[4]d_gain enable, [5]analog d_gain enable ; enable digital gain(including digital a_gain) overwrite main setting.
    {0x4131 ,0x01},// enable BLI for gain block
    {0x4132 ,0x20},// BLI target(need the same as BLC target)
    {0x5011 ,0x00},// BLC manual offset HB, [5]=0:add, [5]=1:subtract
    {0x5015 ,0x00},// [3]: Black/ Test row output enable
    {0x501D ,0x1C},// enable BLC dithering; BLC by color. enable BLC hysteresis(check).
    {0x501E ,0x00},// BLC target(HB)
    {0x501F ,0x20},// BLC target(LB)    
    // VRNP ON/OFF by INT
    {0x50D5 ,0xF0},// -ve pump auto off by INT for G1/G2 balace and improve SNR (Simon) [4]:NEGPUMP maskout bad frame 1: don't maskout
    {0x50D7 ,0x12},// MUX_NEGPUMP_OFF = NEGPUMP_Extend_off(0x50D7*16)
    {0x50BB ,0x14},// MUX_NEGPUMP_ON = NEGPUMP_Extend_on(0x50BB*16)    
    //TS
    {0x5040 ,0x07},// [0]:ts_en_d
    //0x5047 is TS output value.
    //Analog
    {0x50B7 ,0x00},// DIN_OFFSET(HB)
    {0x50B8 ,0x35},// DIN_OFFSET(LB)
    {0x50B9 ,0xFF},// RST_THRESHOLD(HB), FFFF=no clamp
    {0x50BA ,0xFF},// RST_THRESHOLD(LB), FFFF=no clamp
    {0x5200 ,0x26},// atest_reserve_left_1_d, [1:0]: rampbuf_bias_sel_d<3:2>, [2]=n_lowpwr_en_d(VRPP)
    {0x5201 ,0x00},// atest_reserve_left_2_d,
    {0x5202 ,0x00},// atest_reserve_right_1_d,
    {0x5203 ,0x00},// atest_reserve_right_2_d,
    {0x5217 ,0x01},// [1:0]:avdd16_sel_d<1:0>, [2]:avdd16_ldo_pd_d
    {0x5219 ,0x03},// [1:0]: vrst_low_sel_d_d<1:0>  improve banding
    {0x5234 ,0x01},// [7:0] vrpp_samle_d pulse width
    // ldo setting
    {0x526B ,0x03},// [0] LDO_T_en_d, [1] LDO_L_en_d
    {0x4C00 ,0x00},// [1] : ext_ldo_en
    //{0x526B ,0x00},// [0] LDO_T_en_d, [1] LDO_L_en_d (for advdd=2.0V work around)
    //{0x4C00 ,0x02},// [1] : ext_ldo_en (for advdd=2.0V work around)
    // for advdd=2.0V work around--> change 526B, 4C00, 4B3B    
    // MIPI analog setting
    {0x0310 ,0x00},// [2]=polar_det_enb_d, [1]=avdd16_dvdd_en_d,  [0]: 1:bypass MIPI LDO , 0: enable MIPI LDO
    {0x4B31 ,0x06},// [1]: DATA_LAT_PHASE_SEL_D(PHASE_SEL, MIPI1[7]),[3]Inverse Digital PKT Clock 0 : PKT = PKT 1:PKT = PKT 
    {0x4B3B ,0x02},// [7]: NC   
    {0x4B44 ,0x0C},// mipi duty adjust
    {0x4B45 ,0x01},// mipi skew adjust according to test result.    
    // Updated 20160119 //
    {0x50A1 ,0x00},// row_tg_cds_r_reg[7:4] row_tg_rst_f_reg[3:0]
    {0x50AA ,0x2E},// cds_rst_f_reg[7:4] ca_rst_f_reg[3:0] 
    {0x50AC ,0x44},// (0119 RFPN) ;[7:4]ramp_start_r2_reg<3:0> [3:0]ramp_start_r1_reg<3:0>
    {0x50AB ,0x04},// ramp_rst_f_reg[7:4] comp_rst_f_reg[3:0]
    {0x50A0 ,0xB0},// (0119 RFPN) ;sf_bias_en_r_reg[7:4] rt_rst_f_reg_buf1[3:0]
    {0x50A2 ,0x1B},//ramp_offset_en_f2_reg[7:0]  improve banding 
    {0x50AF ,0x00},// [7:4]row_tg_cds_f_reg,<3:0> [3:0]ramp_offset_en_f1_reg<3:0>
    {0x5208 ,0x55},// [2:0]clamp_stg1_d [5:3]clamp_stg2_d
    {0x5209 ,0x03},// [1:0]sf_bias_d [3:2]sf_biasn_d     0F-> 03 reduce banding
    {0x520B ,0x41},// [0]ramp_sample_en_d [1]dacout_mode_en_d [2]rgain_float_en_d [3]rampout_byp_en_d [4]adc12b_en_d [5]cai_sample_en_d [6]bl_clamp_en_d [7]bl_float_en_d
    //It will over written adc12b_en_d again in 12b d    
    {0x520D ,0x40},// [2:0]ca_rst_sr_d [5:3]cds_sr_d [6]sf_sig_clamp_en_d [7]gcnt_buffer_byp_en_d 
    {0x5214 ,0x18},// [0]bgp_pwr_save_en_d [1]bgp_pwr_save_mode_d [2]bgp_sample_byp_d [4:3]comp1_bias_sel_d<1:0> [6:5]comp2_bias_sel_d<1:0>
    {0x5215 ,0x0B},// (0119 saturation) ;[1:0]rampbuf_bias_sel_d<1:0> [4:2]ramp_range_sel_d<2:0>
    {0x5216 ,0x00},// [0]sf_biasn_sel_d [1]sf_bias_sel_d
    {0x521A ,0x10},// [0]otp_pump_en_1_d [4:1]vrpp_buffer_d<3:0> [6:5]vrpp_sel_d<1:0>
    {0x521B ,0x24},// [2:0]rxhi_sel_d<2:0> [5:3]sxhi_sel_d<2:0>
    {0x5232 ,0x04},// (0119 saturation) ;[2:0]Vrnp_clk_d 0:div2 , 1:div4, 2:div8, 3 div16, 4 div32, 5 div64
    {0x5233 ,0x03},// (0119 RGB difference) ;[2:0]Vrpp_clk_d 0:no div 1: div2, 2:div4, 3 div8, 4 div16, 5 div32, 6 div64    
    //--ca-gain table    
    {0x5106 ,0xF0},// 64x gain no refer to 32x gain
    {0x510E ,0xC1},// 64x gain no refer to 32x gain
    {0x5166 ,0xF0},// 64x gain no refer to 32x gain
    {0x516E ,0xC1},// 64x gain no refer to 32x gain
    {0x5196 ,0xF0},// 64x gain no refer to 32x gain
    {0x519E ,0xC1},// 64x gain no refer to 32x gain
    {0x51C0 ,0x80},// CAGAINTB_000_1 ;(0122 CA-gain compensation)=>80
    {0x51C4 ,0x80},// CAGAINTB_001_1 ;(0122 CA-gain compensation)=>80
    {0x51C8 ,0x80},// CAGAINTB_010_1 ;(0122 CA-gain compensation)=>81
    {0x51CC ,0x80},// CAGAINTB_011_1 ;(0122 CA-gain compensation)=>82
    {0x51D0 ,0x80},// CAGAINTB_100_1 ;(0122 CA-gain compensation)=>84
    {0x51D4 ,0x80},// CAGAINTB_101_1 ;(0122 CA-gain compensation)=>88  gain x16
    {0x51D8 ,0x80},// CAGAINTB_110_1 ;(0122 CA-gain compensation)=>88  gain x32
    {0x51DC ,0x80},// CAGAINTB_111_1   gain x64
    {0x51C1 ,0x03},// CAGAINTB_000_2
    {0x51C5 ,0x13},// CAGAINTB_001_2  x01
    {0x51C9 ,0x17},// CAGAINTB_010_2  x02
    {0x51CD ,0x27},// CAGAINTB_011_2  x04  banding fixed
    {0x51D1 ,0x27},// CAGAINTB_100_2  x08  banding fixed
    {0x51D5 ,0x2B},// CAGAINTB_101_2  x16  banding fixed
    {0x51D9 ,0x2B},// CAGAINTB_110_2  x32  banding fixed
    {0x51DD ,0x2B},// CAGAINTB_111_2  x64  banding fixed
    {0x51C2 ,0x4B},// CAGAINTB_000_3
    {0x51C6 ,0x4B},// CAGAINTB_001_3  x01
    {0x51CA ,0x4B},// CAGAINTB_010_3  x02
    {0x51CE ,0x39},// CAGAINTB_011_3  x04 banding fixed
    {0x51D2 ,0x39},// CAGAINTB_100_3  x08 banding fixed
    {0x51D6 ,0x39},// CAGAINTB_101_3  x16 banding fixed
    {0x51DA ,0x39},// CAGAINTB_110_3  x32 banding fixed
    {0x51DE ,0x39},// CAGAINTB_111_3  x64 banding fixed
    {0x51C3 ,0x10},// CAGAINTB_000_4 ;from shmoo result->78
    {0x51C7 ,0x10},// (0119 dark-sun) ;CAGAINTB_001_4 ;from shmoo result->78
    {0x51CB ,0x10},// (0119 dark-sun) ;CAGAINTB_010_4 By Andrew
    {0x51CF ,0x08},// (0119 dark-sun) ;CAGAINTB_011_4 By Andrew
    {0x51D3 ,0x08},// (0119 dark-sun) ;CAGAINTB_100_4 By Andrew
    {0x51D7 ,0x08},// CAGAINTB_101_4
    {0x51DB ,0x08},// CAGAINTB_110_4
    {0x51DF ,0x00},// CAGAINTB_111_4
    //--ca-gain table
    {0x51E0 ,0x94},
    {0x51E2 ,0x94},
    {0x51E4 ,0x94},
    {0x51E6 ,0x94},
    {0x51E1 ,0x00},
    {0x51E3 ,0x00},
    {0x51E5 ,0x00},
    {0x51E7 ,0x00},
    //End 20160119//
    //SSCG setting //20160226 by YL.
    {0x5264 ,0x23},// SSCG lower HB 23,07,02~0.4438%
    {0x5265 ,0x07},// SSCG lower LB
    {0x5266 ,0x24},// SSCG upper HB(fixed value)
    {0x5267 ,0x92},// SSCG upper LB(fixed value)
    {0x5268 ,0x01},// [0]:SSCG disable,1=disable.
    //End of SSCG setting//20160226    
    // Display dark row:
    // MIPI:     Set 0x5015[3]=1 + 0x4B6A=0x00
    // Parallel: Set 0x5015[3]=1
    //---------------------------------------------------
    // Static BPC
    //---------------------------------------------------
    {0xBAA2 ,0xC0},
    {0xBAA2 ,0x40},
    {0xBA90 ,0x01},// define BPC_OTP_Enable
    {0xBA93 ,0x02},    
    //---------------------------------------------------
    //Dynamic BPC Line Buffer Power Saving
    //---------------------------------------------------
    {0x3110 ,0x0B},// [0]:static enable[1]:dynamic enable[2]DCC enable[3]DCC controlled by gain table
    {0x373E ,0x8A},// [7]: power saving with blanking 
    {0x373F ,0x8A},// [7]: power saving with blanking 
    {0x3701 ,0x08},// Dynamic BPC CH1 HotPix TH
    {0x3709 ,0x08},// Dynamic BPC CH2 HotPix TH
    //Dynamic BPC for sub_sampling
    {0x3713 ,0x00},// Strength of CH1 Dynamic BPC
    {0x3717 ,0x00},// Strength of CH2 Dynamic BPC    
    //---------------------------------------------------
    // Temperature Sensor offset
    //---------------------------------------------------
    {0x5043 ,0x01},// Temperature Sensor offset from [0] 0:OTP / 1:I2C
    {0x501D ,0x00},// BLC 4-ch individually
    {0x5040 ,0x01},// [0] Temp disable
    {0x5044 ,0x05},// Temp IIR enable[0], Weight=2h
    {0x6000 ,0x0F},// [3:0] BLD upper bound
    {0x6001 ,0xFF},// BLD upper bound Lb
    {0x6002 ,0x1F},// [3:0] BLD low bound Hb [4] BLD low bound sign bit
    {0x6003 ,0xFF},// BLD low bound Lb
    {0x6004 ,0x82},// [3:0] BLD INTG shift [4] BLD A part sel from Reg or OTP
    //[5] Read long & short sel [6] BLD Dmax_clip [7] BLD enable
    {0x6005 ,0x00},// [1:0] BLD Cpix bb Hb [2] BLD Cpix bb sign bit [4] BLD Cpix bb sel
    {0x6006 ,0x00},// [7:0] BLD Cpix bb Lb
    {0x6007 ,0x00},// [1:0] BLD Cpix gb Hb [2] BLD Cpix gb sign bit [4] BLD Cpix gb sel
    {0x6008 ,0x00},// [7:0] BLD Cpix gb Lb
    {0x6009 ,0x00},// [1:0] BLD Cpix gr Hb [2] BLD Cpix gr sign bit [4] BLD Cpix gr sel
    {0x600A ,0x00},// [7:0] BLD Cpix gr Lb
    {0x600B ,0x00},// [1:0] BLD Cpix rr Hb [2] BLD Cpix rr sign bit [4] BLD Cpix rr sel
    {0x600C ,0x00},// [7:0] BLD Cpix rr Lb
    {0x600D ,0x20},// [6:0] BLD A part [7] BLD A part sign bit
    {0x600E ,0x00},// [0] BLD Tth Hb
    {0x600F ,0xA1},// [7:0] BLD Tth Lb
    {0x6010 ,0x01},// [0] BLD Tstep ctrl
    {0x6011 ,0x00},// [5:0] BLD Dt1 Hb [6] BLD Dt1 sign bit
    {0x6012 ,0x0F},// [7:0] BLD Dt1 Lb
    {0x6013 ,0x00},// [5:0] BLD Dt2 Hb [6] BLD Dt2 sign bit
    {0x6014 ,0x24},// [7:0] BLD Dt2 Lb
    {0x6015 ,0x00},// [5:0] BLD Dt3 Hb [6] BLD Dt3 sign bit
    {0x6016 ,0x3F},// [7:0] BLD Dt3 Lb
    {0x6017 ,0x00},// [5:0] BLD Dt4 Hb [6] BLD Dt4 sign bit
    {0x6018 ,0x73},// [7:0] BLD Dt4 Lb
    {0x6019 ,0x00},// [5:0] BLD Dt5 Hb [6] BLD Dt5 sign bit
    {0x601A ,0xC3},// [7:0] BLD Dt5 Lb
    {0x601B ,0x01},// [5:0] BLD Dmax Hb [6] BLD Dmax sign bit
    {0x601C ,0x4F},// [7:0] BLD Dmax Lb
    //---------------------------------------------------
    // Turn on rolling shutter
    //---------------------------------------------------
    {0x0000 ,0x00},// was 0000
    {0x0104 ,0x01},// group hold
    {0x0104 ,0x00},// group hold
    
    {0x0100 ,0x03},// mode_select: 0h=stby, 2h=pwr_up, 1h/3h=streaming.
};
#define NUM_OF_1080P_MIPI_INIT_REGS (sizeof(sensor_1080p_mipi_init_regs) / sizeof(sensor_reg_t))

typedef struct gain_set {
    u8  a_gain;
    u16 total_gain;     // UFIX 7.6
} gain_set_t;

static const gain_set_t gain_table[] = {
/*  //SMIA 16X
    {0x00,	64  }, {0x02,	72  }, {0x04,	80  }, {0x06,	88  }, 
    {0x08,	96  }, {0x0A,	104 }, {0x0C,	112 }, {0x0E,	120 },
    {0x10,	128 }, {0x14,	144 }, {0x18,	160 }, {0x1C,	176 },
    {0x20,	192 }, {0x24,	208 }, {0x28,	224 }, {0x2C,	240 }, 
    {0x30,	256 }, {0x38,	288 }, {0x40,	320 }, {0x48,	352 }, 
    {0x50,	384 }, {0x58,	416 }, {0x60,	448 }, {0x68,	480 },
    {0x70,	512 }, {0x80,	576 }, {0x90,	640 }, {0xA0,	704 },
    {0xB0,	768 }, {0xC0,	832 }, {0xD0,	896 }, {0xE0,	960 }, 
    {0xF0,	1024}, 
*/
    //HII 32X   
    {0x00,	64  }, {0x02,	72  }, {0x04,	80  }, {0x06,	88  }, 
    {0x08,	96  }, {0x0A,	104 }, {0x0C,	112 }, {0x0E,	120 },
    {0x10,	128 }, {0x12,	144 }, {0x14,	160 }, {0x16,	176 },
    {0x18,	192 }, {0x1A,	208 }, {0x1C,	224 }, {0x1E,	240 }, 
    {0x20,	256 }, {0x22,	288 }, {0x24,	320 }, {0x26,	352 }, 
    {0x28,	384 }, {0x2A,	416 }, {0x2C,	448 }, {0x2E,	480 },
    {0x30,	512 }, {0x32,	576 }, {0x34,	640 }, {0x36,	704 },
    {0x38,	768 }, {0x3A,	832 }, {0x3C,	896 }, {0x3E,	960 }, 
    {0x40,	1024}, {0x42,  1152 }, {0x44,  1280 }, {0x46,  1408 },  
    {0x48,	1536}, {0x4A,  1664 }, {0x4C,  1792 }, {0x4E,  1920 }, 
    {0x50,	2048}
};
#define NUM_OF_GAINSET (sizeof(gain_table) / sizeof(gain_set_t))

//=============================================================================
// I2C
//=============================================================================
#define I2C_NAME            SENSOR_NAME
#define I2C_ADDR            0x48 >> 1
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
	
    pinfo->pclk = g_sHM2130info[pinfo->currentmode].pclk;
	
    isp_info("hm2130 Sensor: pclk(%d) XCLK(%d)\n", pinfo->pclk, xclk);

    return pinfo->pclk;
}

static void calculate_t_row(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    pinfo->hts = g_sHM2130info[pinfo->currentmode].hts;
    pinfo->t_row = g_sHM2130info[pinfo->currentmode].t_row;

    isp_info("h_total=%d, t_row=%d\n", pinfo->hts, pinfo->t_row);
}

static u32 get_exp(void)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    uint32_t exp_line_h,exp_line_l;
    uint32_t lines;
	
    if (!g_psensor->curr_exp) {
        read_reg(0x0202, &exp_line_h);
        read_reg(0x0203, &exp_line_l);
        lines = (((exp_line_h & 0xFF) << 8)|(exp_line_l & 0xFF));
        g_psensor->curr_exp = (lines * (pinfo->t_row) + 500) / 1000;		
    }
    
    return g_psensor->curr_exp;
}

static int set_exp(u32 exp)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    int ret = 0;
    uint32_t exp_line;
    uint32_t ext_vts;
		
    exp_line = ((exp * 1000) + pinfo->t_row /2 ) / pinfo->t_row;

    if (1 > exp_line)
        exp_line = 1;

    if(exp_line > (SENSOR_MAX_VTS - SENSOR_NON_EXP_LINE))
        exp_line = SENSOR_MAX_VTS - SENSOR_NON_EXP_LINE;

    if (exp_line > ((pinfo->vts) - SENSOR_NON_EXP_LINE)) 
    {
        ext_vts = exp_line+SENSOR_NON_EXP_LINE;
        if (((ext_vts%2) != 0) && (ext_vts < SENSOR_MAX_VTS))
           ext_vts++; 		
        write_reg(0x0340, (ext_vts >> 8) & 0xFF);    
        write_reg(0x0341, (ext_vts & 0xFF));
        pinfo->long_exp = 1;
    } 
    else 
    {
        if (pinfo->long_exp)
        {       
            write_reg(0x0340, (pinfo->vts >> 8) & 0xFF);        
            write_reg(0x0341, (pinfo->vts & 0xFF));			
            pinfo->long_exp = 0;
        }
    }
	
    write_reg(0x0202, (exp_line >> 8) & 0xFF);
    write_reg(0x0203, (exp_line & 0xFF));
    write_reg(0x0000, 0x00);			
    write_reg(0x0104, 0x01); 
    write_reg(0x0104, 0x00);
    write_reg(0x0000, 0x00);	
	
    pinfo->updatereg = 1;	
    g_psensor->curr_exp = ((exp_line * pinfo->t_row) + 500) / 1000;

    //printk("set_exp lines=%d t_row=%d exp=%d\n", exp_line, pinfo->t_row, g_psensor->curr_exp);
	
    return ret;

}

static void adjust_blk(int fps)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    u32 max_fps;

    max_fps = g_sHM2130info[pinfo->currentmode].fps;
    
    if (fps > max_fps)
        fps = max_fps;
        
    pinfo->vts = (g_sHM2130info[pinfo->currentmode].vts) * max_fps / fps;

    if ((pinfo->vts) > SENSOR_MAX_VTS)
        pinfo->vts = SENSOR_MAX_VTS;
		
    isp_info("adjust_blk fps=%d, vts=%d\n", fps, pinfo->vts);
     
    write_reg(0x0340, ((pinfo->vts)>>8) & 0xFF);
    write_reg(0x0341, (pinfo->vts) & 0xFF);
		 	
    get_exp();
    set_exp(g_psensor->curr_exp);
	
    g_psensor->fps = fps;
}

static u32 get_gain(void)
{
    uint32_t a_gain;
    int i=0;

    if (!g_psensor->curr_gain) {
		
        read_reg(0x0205, &a_gain);			
        if (a_gain > gain_table[NUM_OF_GAINSET - 1].a_gain)
            a_gain = gain_table[NUM_OF_GAINSET - 1].a_gain;
        else if(a_gain < gain_table[0].a_gain)
            a_gain = gain_table[0].a_gain;

        for (i=0; i<NUM_OF_GAINSET; i++) {
            if (gain_table[i].a_gain > a_gain)
                break;
        }
        g_psensor->curr_gain = (gain_table[i-1].total_gain);			
    }	
    return g_psensor->curr_gain;
}

static int set_gain(u32 gain)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;	
	
    uint8_t a_gain=0;
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
	
    a_gain = gain_table[i-1].a_gain;	
 	
    write_reg(0x0205, a_gain);
    write_reg(0x4132, 0x20);
    write_reg(0x0000, 0x00);			
    write_reg(0x0104, 0x01); 
    write_reg(0x0104, 0x00);
    write_reg(0x0000, 0x00);	

    pinfo->updatereg = 1;	
    g_psensor->curr_gain = gain_table[i-1].total_gain;
    //isp_info("g_psensor->curr_gain=%d\n", g_psensor->curr_gain);
	
    return 0;    
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
    read_reg(0x0101, &reg);
    return (reg & BIT0) ? 1 : 0;    
}

static int set_mirror(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    uint32_t reg;

    pinfo->mirror= enable; 

    read_reg(0x0101, &reg);

    if (pinfo->flip)
        reg |=BIT1;
    else
        reg &=~BIT1;
    
    if (enable)
        reg |= BIT0;
    else
        reg &= ~BIT0;
	 	
    write_reg(0x0101, reg);			
    write_reg(0x0000, 0x00);			
    write_reg(0x0104, 0x01); 
    write_reg(0x0104, 0x00);
    write_reg(0x0000, 0x00);		
    update_bayer_type();	
    pinfo->updatereg = 1;
	
    return 0;
}

static int get_flip(void)
{
    u32 reg;

    read_reg(0x0101, &reg);
    return (reg & BIT1) ? 1 : 0;
}

static int set_flip(int enable)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
    uint32_t reg;
    
    pinfo->flip= enable;

    read_reg(0x0101, &reg);

    if (pinfo->mirror)
        reg |= BIT0;
    else
        reg &= ~BIT0;
    if (enable)
        reg |= BIT1;
    else
        reg &= ~BIT1;
  	
    write_reg(0x0101, reg);			
    write_reg(0x0000, 0x00);			
    write_reg(0x0104, 0x01); 
    write_reg(0x0104, 0x00);
    write_reg(0x0000, 0x00);		
    update_bayer_type();
    pinfo->updatereg = 1;
	
    return 0;
}

static int set_size(u32 width, u32 height)
{
    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;

    int ret = 0;
    
    //isp_info("width=%d, height=%d\n", width, height);
    // check size
    if ((width > SENSOR_MAX_WIDTH) || (height > SENSOR_MAX_HEIGHT)) {
        isp_err("Exceed maximum sensor resolution!\n");
        return -1;
    }	

    // update sensor information
    g_psensor->out_win.x = 0;
    g_psensor->out_win.y = 0;
    g_psensor->out_win.width = g_sHM2130info[pinfo->currentmode].img_w;
    g_psensor->out_win.height = g_sHM2130info[pinfo->currentmode].img_h;	
    
    g_psensor->img_win.width = width;
    g_psensor->img_win.height = height;

    g_psensor->img_win.x = ((g_psensor->out_win.width - g_psensor->img_win.width) >>1) & ~BIT0;		
    g_psensor->img_win.y = ((g_psensor->out_win.height - g_psensor->img_win.height) >> 1) & ~BIT0;

    return ret;
}

static int check_sensor_reg_status(void)
{

    sensor_info_t *pinfo = (sensor_info_t *)g_psensor->private;
	
    if (1 == pinfo->updatereg) {
        isp_api_wait_frame_start();			
        pinfo->updatereg = 0;
    }	

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
        case ID_CHECK_SENSOR_STATUS:
            *(int*)arg = check_sensor_reg_status();
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
    u32 Chip_id_h, Chip_id_l, Chip_id;
    int i,ret = 0;
    
    if (pinfo->is_init)
        return 0;

    pinfo->mirror = mirror;
    pinfo->flip = flip;

    //only one sensor mode 1080p
    pInitTable = sensor_1080p_mipi_init_regs;
    InitTable_Num = NUM_OF_1080P_MIPI_INIT_REGS;

    isp_info("start initial...\n");
    
    //Read ID
    read_reg(0x0000, &Chip_id_h);    
    read_reg(0x0001, &Chip_id_l);
    Chip_id = (Chip_id_h << 8) | Chip_id_l;
    isp_info("Chip_id = HM%x\n", Chip_id);

    // set initial registers
    for (i=0; i<InitTable_Num; i++) {
        if (pInitTable[i].addr == DELAY_CODE) {
            mdelay(pInitTable[i].val);
        } else if (write_reg(pInitTable[i].addr, pInitTable[i].val) < 0) {
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

    isp_info("initial end ...\n"); 	
    return ret;
}

//=============================================================================
// external functions
//=============================================================================
static void hm2130_deconstruct(void)
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

static int hm2130_construct(u32 xclk, u16 width, u16 height)
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
    pinfo->currentmode = MODE_MIPI_1080P;
    g_psensor->capability = SUPPORT_MIRROR | SUPPORT_FLIP;
    g_psensor->xclk = xclk;
    g_psensor->pclk = get_pclk(g_psensor->xclk);	
    g_psensor->interface = g_sHM2130info[pinfo->currentmode].interface;
    g_psensor->num_of_lane = g_sHM2130info[pinfo->currentmode].lane;
    g_psensor->fmt = g_sHM2130info[pinfo->currentmode].rawbit;
    g_psensor->bayer_type = g_sHM2130info[pinfo->currentmode].bayer;
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
    hm2130_deconstruct();
    return ret;
}

//=============================================================================
// module initialization / finalization
//=============================================================================
static int __init hm2130_init(void)
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

    if ((ret = hm2130_construct(sensor_xclk, sensor_w, sensor_h)) < 0)
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

static void __exit hm2130_exit(void)
{
    isp_unregister_sensor(g_psensor);
    hm2130_deconstruct();
    if (g_psensor)
        kfree(g_psensor);
}

module_init(hm2130_init);
module_exit(hm2130_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Sensor hm2130");
MODULE_LICENSE("GPL");
