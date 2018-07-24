/**
 * @file lens_an41908.h
 *  header file of lens_an41908.c.
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 * For SC1135 sensor
 */

#ifndef __LENS_AN41908_H
#define __LENS_AN41908_H

#include <linux/delay.h>
#include "isp_input_inf.h"


//#define FZ_PLS_PIN_EXIST


// --------------------------------------------------------------------------------------------------------------------------------
#define TDELAY(X)    ;//ndelay(5*X)    // 200kHz from oscilloscope


typedef enum
{
    AN41908_PULSE1_START_TIME      = 0x06,
    AN41908_PULSE1_WIDTH           = 0x07,
    AN41908_PULSE2_START_TIME      = 0x08,
    AN41908_PULSE2_WIDTH           = 0x09,
    AN41908_VD_IS_FZ_POLARITY      = 0x0B,
    AN41908_PWM_FREQ               = 0x20,
    AN41908_FZ_TEST                = 0x21,
    AN41908_MOTOR1_PHASE_COR       = 0x22,
    AN41908_MOTOR1_MAX_PULSE_WIDTH = 0x23,
    AN41908_SET_MOTOR1_STEP        = 0x24,
    AN41908_MOTOR1_STEP_CYCLE      = 0x25,
    AN41908_MOTOR2_PHASE_COR       = 0x27,
    AN41908_MOTOR2_MAX_PULSE_WIDTH = 0x28,
    AN41908_SET_MOTOR2_STEP        = 0x29,
    AN41908_MOTOR2_STEP_CYCLE      = 0x2A,
    AN41908_IRCUT_CTL              = 0x2C,
    AN41908_REG_UNKNOWN            = 0x3F
} AN41908_REG_CMD;

typedef enum
{
    AN41908_SET_MOTOR_CFG,
    AN41908_SET_MOTOR_SPD,
    AN41908_SET_MOTOR_STEP,
    AN41908_CMD_UNKNOWN
} AN41908_CMD_TYPE;

typedef enum
{
    AN41908_CHNEL1,
    AN41908_CHNEL2,
    AN41908_CHNEL3,
    AN41908_CHNEL4,
    AN41908_CHNEL_UNKNOWN
} AN41908_CHNEL_NUM;

typedef enum
{
    AN41908_MODE_64MACRO  = 8,
    AN41908_MODE_128MACRO = 16,
    AN41908_MODE_256MACRO = 32,
    AN41908_MODE_UNKNOWN
} AN41908_DRIVE_MODE;

typedef enum
{
    AN41908_MOTOR_SPD_4X,
    AN41908_MOTOR_SPD_2X,
    AN41908_MOTOR_SPD_1X,
    AN41908_MOTOR_SPD_HALF,
    AN41908_MOTOR_SPD_QUARTER,
    AN41908_MOTOR_SPD_UNKNOWN
} AN41908_MOTOR_SPEED;

typedef enum
{
    MOTOR_FOCUS,
    MOTOR_ZOOM,
    MOTOR_IRIS,
    MOTOR_UNKNOWN
} MOTOR_SEL;


typedef struct motor_config
{
    AN41908_CHNEL_NUM chnel;
    AN41908_DRIVE_MODE mode;    // excitation mode
    int  excite_en;
    int  def_pdir;
    int  pwm_freq;
    volatile int work_status;
    int  motor_status;          // used in PLS pin situation
    int  max_step_size;
    bool move_to_pi;
    int  min_pos;
    int  max_pos;
    volatile int curr_pos;
    bool set_final_step;        // used in PLS pin situation
    int  move_dir;
    AN41908_MOTOR_SPEED move_spd;
    int  backlash_comp;
    u16  max_pulse_width;
    int  dly_time;
} motor_config_t;

typedef struct spi_data_fmt
{
    union {
        struct {
            u8 A0:1;
            u8 A1:1;
            u8 A2:1;
            u8 A3:1;
            u8 A4:1;
            u8 A5:1;
        } A;

        u8 addr;        // register address
    };

    union {
        struct {
            u8 RW:1;    // 0: write / 1: read
            u8 C1:1;    // un-used
        } M;

        u8 rw;
    };

    union {
        struct {
            u8 D0:1;
            u8 D1:1;
            u8 D2:1;
            u8 D3:1;
            u8 D4:1;
            u8 D5:1;
            u8 D6:1;
            u8 D7:1;
            u8 D8:1;
            u8 D9:1;
            u8 D10:1;
            u8 D11:1;
            u8 D12:1;
            u8 D13:1;
            u8 D14:1;
            u8 D15:1;
        } D;

        u16 data;       // written data
    };
} spi_data_fmt_t;

struct virtual_dev {
    int fake_struc;
};


#define FOCUS_DIR_INIT              0
#define FOCUS_DIR_INF               1
#define FOCUS_DIR_NEAR              (-1)
#define ZOOM_DIR_INIT               0
#define ZOOM_DIR_TELE               1
#define ZOOM_DIR_WIDE               (-1)

#define MOTOR_STATUS_FREE           0
#define MOTOR_STATUS_BUSY           1

#define LENS_MAX_PATH_SIZE          32

#define STANDBY_PEAK_PULSE_WIDTH    0x3232    // set to 30%  (vary with PWMMOD=21) 0.3*21*8
#define GOHOME_FPEAK_PULSE_WIDTH    0xFCFC    // set to 150% (vary with PWMMOD=21) 1.5*21*8
#define GOHOME_ZPEAK_PULSE_WIDTH    0xFCFC    // set to 150% (vary with PWMMOD=21) 1.5*21*8


const int g_reg_init_tab[] =
{
    (AN41908_VD_IS_FZ_POLARITY << 16)      + 0x0080,
    (AN41908_PWM_FREQ << 16)               + 0x5503,
    (AN41908_FZ_TEST << 16)                + 0x0087,
    (AN41908_MOTOR1_PHASE_COR << 16)       + 0x1602,
    (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) + STANDBY_PEAK_PULSE_WIDTH,
    (AN41908_MOTOR1_STEP_CYCLE << 16)      + 0x009D,
    (AN41908_MOTOR2_PHASE_COR << 16)       + 0x1602,
    (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) + STANDBY_PEAK_PULSE_WIDTH,
    (AN41908_MOTOR2_STEP_CYCLE << 16)      + 0x0139,
};

#define MAX_REG_TAB_SIZE    ARRAY_SIZE(g_reg_init_tab)




// --------------------------------------------------------------------------------------------------------------------------------
#define LENS_NAME                  "RELMON-MS41929"
#define LENS_ID                    "gwell1-v10"      // *


// config GPIO --------------------------------------------------------------------------------------------------------------------
#define SPI_PANASONIC_CS_PIN       GPIO_ID(0, 3)     // *
#define SPI_PANASONIC_RXD_PIN      GPIO_ID(0, 2)     // *
#define SPI_PANASONIC_TXD_PIN      GPIO_ID(0, 1)     // *
#define SPI_PANASONIC_CLK_PIN      GPIO_ID(0, 4)     // *
#define AN41908_VD_FZ_PIN          GPIO_ID(0, 29)    // *
#define AN41908_RESET_PIN          -1                // *

#ifndef FZ_PLS_PIN_EXIST
#define MOT_FOCUS_PLS_PIN          37
#define MOT_ZOOM_PLS_PIN           38
#define FOCUS_PSUM_DELAY           33                // unit: msec (12*AN41908_MOTOR1_STEP_CYCLE/24MHz)*255=30 msec
#define ZOOM_PSUM_DELAY            63                // unit: msec (12*AN41908_MOTOR2_STEP_CYCLE/24MHz)*255=59 msec
#else
#define MOT_FOCUS_PLS_PIN          GPIO_ID(1, 28)    // *
#define MOT_ZOOM_PLS_PIN           GPIO_ID(1, 29)    // *
#define FOCUS_PSUM_DELAY           1
#define ZOOM_PSUM_DELAY            1
#endif


// config lens parameters ---------------------------------------------------------------------------------------------------------
#define NEAR_BOUND                 0                 // *
#define INF_BOUND                  2000              // *
#define WIDE_BOUND                 0                 // *
#define TELE_BOUND                 1700              // *
#define WIDE_FOCAL_VALUE           28                // * 2.8  mm
#define TELE_FOCAL_VALUE           120               // * 12.0 mm
#define INIT_FOCUS_POS             100               // *
#define INIT_ZOOM_POS              (20+370)          // *

#define FOCUS_BACKLASH_CORR        27                // * correction for focus backlash
#define ZOOM_BACKLASH_CORR         34                // * correction for zoom backlash


// config motor IC parameters -----------------------------------------------------------------------------------------------------
static motor_config_t g_motor_focus_cfg =
{
    .chnel           = AN41908_CHNEL2,               // *
    .mode            = AN41908_MODE_64MACRO,
    .excite_en       = 0x0400,
    .def_pdir        = 0,                            // *
    .pwm_freq        = 4500,    // must be the same with zoom's one
    .work_status     = MOTOR_STATUS_FREE,
    .motor_status    = MOTOR_STATUS_FREE,
    .max_step_size   = 255/AN41908_MODE_64MACRO,
    .move_to_pi      = false,
    .min_pos         = NEAR_BOUND,
    .max_pos         = INF_BOUND,
    .curr_pos        = NEAR_BOUND,
    .set_final_step  = false,
    .move_dir        = FOCUS_DIR_INIT,
    .move_spd        = AN41908_MOTOR_SPD_2X,         // *
    .backlash_comp   = FOCUS_BACKLASH_CORR,
    .max_pulse_width = STANDBY_PEAK_PULSE_WIDTH,
    .dly_time        = FOCUS_PSUM_DELAY,
};

static motor_config_t g_motor_zoom_cfg =
{
    .chnel           = AN41908_CHNEL1,               // *
    .mode            = AN41908_MODE_64MACRO,
    .excite_en       = 0x0400,
    .def_pdir        = 0,                            // *
    .pwm_freq        = 4500,    // must be the same with focus's one
    .work_status     = MOTOR_STATUS_FREE,
    .motor_status    = MOTOR_STATUS_FREE,
    .max_step_size   = 255/AN41908_MODE_64MACRO,
    .move_to_pi      = false,
    .min_pos         = WIDE_BOUND,
    .max_pos         = TELE_BOUND,
    .curr_pos        = WIDE_BOUND,
    .set_final_step  = false,
    .move_dir        = ZOOM_DIR_INIT,
    .move_spd        = AN41908_MOTOR_SPD_1X,         // *
    .backlash_comp   = ZOOM_BACKLASH_CORR,
    .max_pulse_width = STANDBY_PEAK_PULSE_WIDTH,
    .dly_time        = ZOOM_PSUM_DELAY,
};


// set zoom vs focus range --------------------------------------------------------------------------------------------------------
typedef struct _LENS_Z_F_TABLE
{
    int zoom_pos;
    int focus_min;
    int focus_max;
} LENS_Z_F_TAB, *P_LENS_Z_F_TAB;

const LENS_Z_F_TAB g_zoom_focus_tab[] =
{
    {    0,         20,  210},         // *
    {  100,        320,  510},         // *
    {  200,        540,  730},         // *
    {  300,        750,  940},         // *
    {  400,        890, 1090},         // *
    {  600,       1170, 1370},         // *
    {  800,       1380, 1580},         // *
    { 1000,       1540, 1740},         // *
    { 1200,       1650, 1850},         // *
    { 1400,       1730, 1930},         // *
    { 1600,       1790, 1990},         // *
    { TELE_BOUND, 1810, INF_BOUND},    // *
};

#define MAX_ZF_TAB_SIZE     ARRAY_SIZE(g_zoom_focus_tab)


// config pinmux ------------------------------------------------------------------------------------------------------------------
static pmuReg_t lens_pmu_reg[] = {
    /*
    * Multi-Function Port Setting Register 0 [offset=0x50]
    * ----------------------------------------------------------------
    *  [31:30] 0:x,             1:CAP_HI8_D2,  2:Bayer_D6
    *  [29:28] 0:x,             1:CAP_HI8_D3,  2:Bayer_D7
    *  [27:26] 0:x,             1:CAP_HI8_D4,  2:Bayer_D8
    *  [25:24] 0:x,             1:CAP_HI8_D5,  2:Bayer_D9
    *  [23:22] 0:x,             1:CAP_HI8_D6,  2:Bayer_D10
    *  [21:20] 0:x,             1:CAP_HI8_D7,  2:Bayer_D11
    *  [19:18] 0:x,             1:CAP_VS,      2:Bayer_VS
    *  [17:16] 0:x,             1:CAP_HS,      2:Bayer_HS
    *  [15:14] 0:x,             1:CAP0_CLK,    2:Bayer_CLK
    *  [13:12] 0:GPIO_0[06],    1:CAP_CLKOUT   2:x
    *  [11:10] 0:GPIO_0[05],    1:X
    *  [9:8]   0:TDI            1:GPIO_0[04],
    *  [7:6]   0:TDO            1:GPIO_0[03],
    *  [5:4]   0:TMS            1:GPIO_0[02],
    *  [3:2]   0:TCK            1:GPIO_0[01],
    *  [1:0]   0:NTRST          1:GPIO_0[00],
    */
    {
     .reg_off   = 0x50,    // *
     .bits_mask = BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2,
     .lock_bits = BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2,
     .init_val  = 0x00000154,
     .init_mask = BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2,
    },

    /*
     * Multi-Function Port Setting Register 2 [offset=0x58]
     * ----------------------------------------------------------------
     *  [31:30] 0:GPIO_1[06],   1:UART0_SOUT,  2:x
     *  [29:28] 0:GPIO_1[05],   1:UART0_SIN,   2:x
     *  [27:26] 0:GPIO_1[04],   1:SD_CLK,      2:NAND_ALE  3:UART2_SOUT
     *  [25:24] 0:GPIO_1[03],   1:SD_CD,       2:NAND_BUSY 3:UART2_SIN
     *  [23:22] 0:GPIO_1[02],   1:SD_CMD_RSP,  2:x
     *  [21:20] 0:GPIO_1[01],   1:SD_CLK,      2:x
     *  [19:18] 0:GPIO_1[00],   1:SD_CD,       2:x
     *  [17:16] 0:GPIO_0[31],   1:SSP1_SCLK,   2:NAND_D7   3:x
     *  [15:14] 0:GPIO_0[30],   1:SSP1_RXD,    2:NAND_D6   3:x
     *  [13:12] 0:GPIO_0[29],   1:SSP1_TXD,    2:NAND_D5   3:x
     *  [11:10] 0:GPIO_0[28],   1:SSP1_FS,     2:NAND_D4   3:x
     *  [9:8]   0:GPIO_0[27],   1:SDC_D3,      2:NAND_D3   3:x
     *  [7:6]   0:GPIO_0[26],   1:SDC_D2,      2:NAND_D2   3:x
     *  [5:4]   0:GPIO_0[25],   1:SDC_D1,      2:NAND_D1   3:x
     *  [3:2]   0:GPIO_0[24],   1:SDC_D0,      2:NAND_D0   3:x
     *  [1:0]   0:GPIO_0[23],   1:SPI_RXD,     2:NAND_REN  3:x
     */
    {
     .reg_off   = 0x58,    // *
     .bits_mask = BIT13 | BIT12,
     .lock_bits = BIT13 | BIT12,
     .init_val  = 0x00000000,
     .init_mask = BIT13 | BIT12,
    },

#ifdef FZ_PLS_PIN_EXIST
    /*
     * Multi-Function Port Setting Register 5 [offset=0x64]
     * ----------------------------------------------------------------
     *  [31:30] 0:
     *  [29:28] 0:
     *  [27:26] 0:
     *  [25:24] 0:
     *  [23:22] 0:
     *  [21:20] 0:
     *  [19:18] 0:
     *  [17:16] 0:
     *  [15:14] 0:
     *  [13:12] 0:
     *  [11:10] 0:
     *  [9:8]   0:
     *  [7:6]   0:
     *  [5:4]   0:
     *  [3:2]   0:
     *  [1:0]   0:
     */
    {
     .reg_off   = 0x64,    // *
     .bits_mask = BIT15 | BIT14 | BIT13 | BIT12,
     .lock_bits = BIT15 | BIT14 | BIT13 | BIT12,
     .init_val  = 0x00000000,
     .init_mask = BIT15 | BIT14 | BIT13 | BIT12,
    },
#endif
};


static pmuRegInfo_t isp_lens_pmu_reg_info = {
    "ISP_LENS_813x",
    ARRAY_SIZE(lens_pmu_reg),
    ATTR_TYPE_AHB,
    &lens_pmu_reg[0]
};


extern int lens_dev_construct(lens_dev_t *pdev);
extern void lens_dev_deconstruct(lens_dev_t *pdev);

#endif    /* __LENS_AN41908_H */

