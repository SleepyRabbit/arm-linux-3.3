/**
 * @file lens_g2005.h
 *  header file of lens_g2005.c.
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 */

#ifndef __LENS_G2005_H
#define __LENS_G2005_H

#include <linux/delay.h>
#include "isp_input_inf.h"


// --------------------------------------------------------------------------------------------------------------------------------
#define TDELAY(X)    ;//udelay(X)


typedef enum
{
    G2005_MODE_SETUP       = 0x0000,
    G2005_C12_DRIVE_MODE   = 0x4000,
    G2005_C34_DRIVE_MODE   = 0x8000,
    G2005_SERIAL_CTL_INPUT = 0xC000,
    G2005_REG_UNKNOWN      = 0xF000
} G2005_REG_CMD;

typedef enum
{
    G2005_CHNEL1,
    G2005_CHNEL2,
    G2005_CHNEL3,
    G2005_CHNEL4,
    G2005_CHNEL_UNKNOWN
} G2005_CHNEL_NUM;

typedef enum
{
    G2005_MOTOR_SPD_4X      = 4000,
    G2005_MOTOR_SPD_2X      = 2000,
    G2005_MOTOR_SPD_1X      = 1000,
    G2005_MOTOR_SPD_HALF    =  500,
    G2005_MOTOR_SPD_UNKNOWN
} G2005_MOTOR_SPEED;

typedef enum
{
    MOTOR_FOCUS,
    MOTOR_ZOOM,
    MOTOR_IRIS,
    MOTOR_UNKNOWN
} MOTOR_SEL;


typedef struct motor_config
{
    G2005_CHNEL_NUM chnel;
    int  def_pdir;
    volatile int work_status;
    bool move_to_pi;
    int  min_pos;
    int  max_pos;
    volatile int residure_step;
    volatile int curr_pos;
    int  ctl_pin_state;
    int  idx_ofst;
    int  idx_acc;
    int  move_dir;
    G2005_MOTOR_SPEED move_spd;
    int  backlash_comp;
} motor_config_t;

typedef struct spi_data_fmt
{
    union {
        struct {
            u8 A0:1;
            u8 A1:1;
        } A;

        u8 addr;        // register address
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
        } D;

        u16 data;       // written data
    };
} spi_data_fmt_t;


#define FOCUS_DIR_INIT         0
#define FOCUS_DIR_INF          1
#define FOCUS_DIR_NEAR         (-1)
#define ZOOM_DIR_INIT          0
#define ZOOM_DIR_TELE          1
#define ZOOM_DIR_WIDE          (-1)

#define MOTOR_STATUS_FREE      0
#define MOTOR_STATUS_BUSY      1

#define MOTOR_START_WAIT       3
#define MOTOR_END_WAIT         4


const u8 RotateForward[] = {2, 0, 6, 4};
const u8 RotateReverse[] = {6, 0, 2, 4};

const int IN1A1B2ATbl[][3] =
{
    {1, 1, 0},// FF
    {0, 1, 1},// OF
    {0, 1, 0},// RF
    {0, 0, 1},// RO
    {0, 0, 0},// RR
    {1, 0, 1},// OR
    {1, 0, 0},// FR
    {1, 1, 1},// FO
};

const u8 FwdRvsConvert[] = {0, 3, 2, 1};


const u16 g_reg_init_tab[] =
{
    G2005_SERIAL_CTL_INPUT + 0x0001,
    G2005_SERIAL_CTL_INPUT + 0x000E,
    G2005_C12_DRIVE_MODE   + 0x0030,
    G2005_C34_DRIVE_MODE   + 0x0000,
    G2005_MODE_SETUP       + 0x3003,
};

#define MAX_REG_TAB_SIZE    ARRAY_SIZE(g_reg_init_tab)




// --------------------------------------------------------------------------------------------------------------------------------
#define LENS_NAME           "GMT-G2005"
#define LENS_ID             "hc1-v10"         // *


// config GPIO --------------------------------------------------------------------------------------------------------------------
#define SPI_GMT_CS_PIN      GPIO_ID(0, 1)     // *
#define SPI_GMT_DAT_PIN     GPIO_ID(0, 4)     // *
#define SPI_GMT_CLK_PIN     GPIO_ID(0, 2)     // *

#define G2005_RESET_PIN     GPIO_ID(1, 22)    // *
#define G2005_IN1A_PIN      GPIO_ID(1, 19)    // *
#define G2005_IN1B_PIN      GPIO_ID(1, 20)    // *
#define G2005_IN2A_PIN      GPIO_ID(1, 21)    // *


// config lens parameters ---------------------------------------------------------------------------------------------------------
#define NEAR_BOUND             0              // *
#define INF_BOUND              2120           // *
#define WIDE_BOUND             0              // *
#define TELE_BOUND             1700           // *
#define WIDE_FOCAL_VALUE       28             // * 2.8  mm
#define TELE_FOCAL_VALUE       120            // * 12 mm
#define INIT_FOCUS_POS         40             // *
#define INIT_ZOOM_POS          410            // *

#define FOCUS_BACKLASH_CORR    15             // * correction for focus backlash
#define ZOOM_BACKLASH_CORR     24             // * correction for zoom backlash


// config motor IC parameters -----------------------------------------------------------------------------------------------------
static motor_config_t g_motor_focus_cfg =
{
    .chnel         = G2005_CHNEL1,            // *
    .def_pdir      = 0,                       // *
    .work_status   = MOTOR_STATUS_FREE,
    .move_to_pi    = false,
    .min_pos       = NEAR_BOUND,
    .max_pos       = INF_BOUND,
    .residure_step = 0,
    .curr_pos      = NEAR_BOUND,
    .ctl_pin_state = 0x00,
    .idx_ofst      = 0,
    .idx_acc       = 0,
    .move_dir      = FOCUS_DIR_INIT,
    .move_spd      = G2005_MOTOR_SPD_2X,      // *
    .backlash_comp = FOCUS_BACKLASH_CORR,
};

static motor_config_t g_motor_zoom_cfg =
{
    .chnel         = G2005_CHNEL2,            // *
    .def_pdir      = 0,                       // *
    .work_status   = MOTOR_STATUS_FREE,
    .move_to_pi    = false,
    .min_pos       = WIDE_BOUND,
    .max_pos       = TELE_BOUND,
    .residure_step = 0,
    .curr_pos      = WIDE_BOUND,
    .ctl_pin_state = 0x00,
    .idx_ofst      = 0,
    .idx_acc       = 0,
    .move_dir      = ZOOM_DIR_INIT,
    .move_spd      = G2005_MOTOR_SPD_2X,      // *
    .backlash_comp = ZOOM_BACKLASH_CORR,
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
    {    0,         10,  280},         // *
    {  200,        550,  820},         // *
    {  400,        930, 1200},         // *
    {  600,       1180, 1450},         // *
    {  800,       1400, 1670},         // *
    { 1000,       1560, 1830},         // *
    { 1200,       1680, 1950},         // *
    { 1400,       1760, 2030},         // *
    { 1600,       1820, 2090},         // *
    { TELE_BOUND, 1840, INF_BOUND},    // *
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
    *  [9:8]   0:TDI            1:GPIO_0[04],  2:PWM7
    *  [7:6]   0:TDO            1:GPIO_0[03],  2:PWM6
    *  [5:4]   0:TMS            1:GPIO_0[02],  2:PWM5
    *  [3:2]   0:TCK            1:GPIO_0[01],  2:PWM4
    *  [1:0]   0:NTRST          1:GPIO_0[00],
    */
    {
     .reg_off   = 0x50,    // *
     .bits_mask = BIT9 | BIT8 | BIT5 | BIT4 | BIT3 | BIT2,
     .lock_bits = BIT9 | BIT8 | BIT5 | BIT4 | BIT3 | BIT2,
     .init_val  = 0x00000114,
     .init_mask = BIT9 | BIT8 | BIT5 | BIT4 | BIT3 | BIT2,
    },
    {
     .reg_off   = 0x5C,    // *
     .bits_mask = BIT31 | BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT25 | BIT24,
     .lock_bits = BIT31 | BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT25 | BIT24,
     .init_val  = 0x00000000,
     .init_mask = BIT31 | BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT25 | BIT24,
    },
};

static pmuRegInfo_t isp_lens_pmu_reg_info = {
    "ISP_LENS_813x",
    ARRAY_SIZE(lens_pmu_reg),
    ATTR_TYPE_AHB,
    &lens_pmu_reg[0]
};


extern bool pwm_dev_request(int id);
extern void pwm_tmr_start(int id);
extern void pwm_tmr_stop(int id);
extern void pwm_dev_release(int id);
extern void pwm_clksrc_switch(int id, int clksrc);
extern void pwm_set_freq(int id, u32 freq);
extern void pwm_set_duty_steps(int id, u32 duty_steps);
extern void pwm_set_duty_ratio(int id, u32 duty_ratio);
extern void pwm_set_interrupt_mode(int id);
extern void pwm_update(int id);
extern void pwm_clr_intrpt(int id);
extern void pwm_mode_change(int id, int mode);
extern void pwm_interrupt_enable(int id);
extern void pwm_interrupt_disable(int id);


extern int lens_dev_construct(lens_dev_t *pdev);
extern void lens_dev_deconstruct(lens_dev_t *pdev);

#endif    /* __LENS_G2005_H */

