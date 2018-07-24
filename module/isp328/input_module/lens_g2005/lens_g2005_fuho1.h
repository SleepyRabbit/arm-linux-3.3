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
#define LENS_ID             "fuho1-v10"       // *


// config GPIO --------------------------------------------------------------------------------------------------------------------
#define SPI_GMT_CS_PIN      GPIO_ID(0, 13)    // *
#define SPI_GMT_DAT_PIN     GPIO_ID(0, 15)    // *
#define SPI_GMT_CLK_PIN     GPIO_ID(0, 17)    // *

#define G2005_RESET_PIN     GPIO_ID(0, 9)     // *
#define G2005_IN1A_PIN      GPIO_ID(0,10)     // *
#define G2005_IN1B_PIN      GPIO_ID(0,11)     // *
#define G2005_IN2A_PIN      GPIO_ID(0,12)     // *


// config lens parameters ---------------------------------------------------------------------------------------------------------
#define NEAR_BOUND             0              // *
#define INF_BOUND              2040           // *
#define WIDE_BOUND             0              // *
#define TELE_BOUND             800            // *
#define WIDE_FOCAL_VALUE       30             // * 3.0  mm
#define TELE_FOCAL_VALUE       105            // * 10.5 mm
#define INIT_FOCUS_POS         1120           // *
#define INIT_ZOOM_POS          40             // *

#define FOCUS_BACKLASH_CORR    27             // * correction for focus backlash
#define ZOOM_BACKLASH_CORR     20             // * correction for zoom backlash


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
    .move_spd      = G2005_MOTOR_SPD_1X,      // *
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
    {   0,          10,  270},         // *
    { 100,         510,  760},         // *
    { 200,         860, 1110},         // *
    { 300,        1130, 1380},         // *
    { 400,        1350, 1600},         // *
    { 500,        1520, 1770},         // *
    { 600,        1640, 1890},         // *
    { 700,        1730, 1980},         // *
    { TELE_BOUND, 1800, INF_BOUND},    // *
};

#define MAX_ZF_TAB_SIZE     ARRAY_SIZE(g_zoom_focus_tab)


// config pinmux ------------------------------------------------------------------------------------------------------------------
static pmuReg_t lens_pmu_reg[] = {
    /*
     * Multi-Function Port Setting Register 1 [offset=0x54]
     * ----------------------------------------------------------------
     *  [21:20] 0:GPIO_0[17],
     *  [19:18] 0:GPIO_0[16],
     *  [17:16] 0:GPIO_0[15],
     *  [15:14] 0:GPIO_0[14],
     *  [13:12] 0:GPIO_0[13],   1:CAP0_D4,  2:Bayer_D0  3:x
     *  [11:10] 0:GPIO_0[12],   1:CAP0_D5,  2:Bayer_D1  3:x
     *  [9:8]   0:GPIO_0[11],   1:CAP0_D6,  2:Bayer_D2  3:x
     *  [7:6]   0:GPIO_0[10],   1:CAP0_D7,  2:Bayer_D3  3:x
     *  [5:4]   0:GPIO_0[09],   1:CAP0_CLK, 2:x         3:x
     *  [3:2]   0:GPIO_0[08],
     *  [1:0]   0:GPIO_0[07],
     */
    {
     .reg_off   = 0x54,    // *
     .bits_mask = BIT21 | BIT20 | BIT17 | BIT16 | BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4,
     .lock_bits = BIT21 | BIT20 | BIT17 | BIT16 | BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4,
     .init_val  = 0x00000000,
     .init_mask = BIT21 | BIT20 | BIT17 | BIT16 | BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6 | BIT5 | BIT4,
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

