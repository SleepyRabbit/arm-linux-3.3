/**
 * @file lens_lv8044.h
 *  header file of lens_lv8044.c.
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 *
 */

#ifndef __LENS_LV8044_H
#define __LENS_LV8044_H

#include <linux/delay.h>
#include "isp_input_inf.h"


// --------------------------------------------------------------------------------------------------------------------------------
#define TDELAY(X)    //ndelay(5*X)


typedef enum
{
    LV8044_CMD_SET_MODE,
    LV8044_CMD_SET_CURR,
    LV8044_CMD_RESET,
    LV8044_CMD_ENABLE,
    LV8044_CMD_SET_DIR_ENABLE,
    LV8044_CMD_UNKNOWN
} LV8044_CMD_TYPE;

typedef enum
{
    LV8044_CHNEL1,
    LV8044_CHNEL2,
    LV8044_CHNEL3,
    LV8044_CHNEL4,
    LV8044_CHNEL5,
    LV8044_CHNEL6,
    LV8044_CHNEL_UNKNOWN
} LV8044_CHNEL_NUM;

typedef enum
{
    LV8044_MODE_2PHASE       = 1,
    LV8044_MODE_12PHASE_FULL = 2,
    LV8044_MODE_12PHASE      = 3,
    LV8044_MODE_4W12PHASE    = 16,    // 64 microstep
    LV8044_MODE_PWM,
    LV8044_MODE_UNKNOWN
} LV8044_DRIVE_MODE;

typedef enum
{
    LV8044_MOTOR_SPD_4X      = 16,
    LV8044_MOTOR_SPD_2X      = 8,
    LV8044_MOTOR_SPD_1X      = 4,
    LV8044_MOTOR_SPD_HALF    = 2,
    LV8044_MOTOR_SPD_QUARTER = 1,
    LV8044_MOTOR_SPD_UNKNOWN
} LV8044_MOTOR_SPEED;

typedef enum
{
    MOTOR_FOCUS,
    MOTOR_ZOOM,
    MOTOR_IRIS,
    MOTOR_UNKNOWN
} MOTOR_SEL;


typedef struct motor_config
{
    LV8044_CHNEL_NUM chnel;
    LV8044_DRIVE_MODE mode;    // excitation mode
    int  pwm_sig_input;
    int  def_pdir;
    int  pwm_freq;
    volatile int work_status;
    int  motor_status;
    int  max_step_size;
    bool move_to_pi;
    int  min_pos;
    int  max_pos;
    volatile int curr_pos;
    bool set_final_step;
    int  move_dir;
    LV8044_MOTOR_SPEED move_spd;
    int  backlash_comp;
    int  dly_time;
} motor_config_t;

typedef struct spi_data_fmt
{
    union {
        struct {
            u8 b0:1;
            u8 b1:1;
            u8 b2:1;
            u8 b3:1;
            u8 b4:1;
            u8 b5:1;
            u8 b6:1;
            u8 b7:1;
        } D;

        u8 data;
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

#define DIR_CW                 0
#define DIR_CCW                1




// --------------------------------------------------------------------------------------------------------------------------------
#define LENS_NAME              "SANYO-LV8044"
#define LENS_ID                "bc1-v10"         // *


// config GPIO --------------------------------------------------------------------------------------------------------------------
#define SPI_SANYO_STB_PIN      GPIO_ID(0, 28)    // *
#define SPI_SANYO_CLK_PIN      GPIO_ID(0, 29)    // *
#define SPI_SANYO_DAT_PIN      GPIO_ID(0, 30)    // *
#define SPI_SANYO_RESET_PIN    -1                // *

#define MOT_FOCUS_PIN          129
#define MOT_ZOOM_PIN           131
#define FOCUS_PSUM_DELAY       ((1024000/LV8044_MODE_4W12PHASE)/(2000/1))            // *
#define ZOOM_PSUM_DELAY        ((1024000/LV8044_MODE_4W12PHASE)/(2000/2))            // *


// config lens parameters ---------------------------------------------------------------------------------------------------------
#define NEAR_BOUND             0                 // *
#define INF_BOUND              2300              // *
#define WIDE_BOUND             0                 // *
#define TELE_BOUND             2150              // *
#define WIDE_FOCAL_VALUE       28                // * 2.8   mm
#define TELE_FOCAL_VALUE       120               // * 12 mm
#define INIT_FOCUS_POS         50                // *
#define INIT_ZOOM_POS          160               // *

#define FOCUS_BACKLASH_CORR    34                // * correction for focus backlash
#define ZOOM_BACKLASH_CORR     34                // * correction for zoom backlash


// config motor IC parameters -----------------------------------------------------------------------------------------------------
static motor_config_t g_motor_focus_cfg =
{
    .chnel         = LV8044_CHNEL1,                  // * LV8044_CHNEL1 or LV8044_CHNEL3
    .mode          = LV8044_MODE_4W12PHASE,
    .pwm_sig_input = 2,                              // * PWM2
    .def_pdir      = 1,                              // *
    .pwm_freq      = 2000,
    .work_status   = MOTOR_STATUS_FREE,
    .motor_status  = MOTOR_STATUS_FREE,
    .max_step_size = 1024,
    .move_to_pi    = false,
    .min_pos       = NEAR_BOUND,
    .max_pos       = INF_BOUND,
    .curr_pos      = NEAR_BOUND,
    .set_final_step  = false,
    .move_dir      = FOCUS_DIR_INIT,
    .move_spd      = LV8044_MOTOR_SPD_2X,            // *
    .backlash_comp = FOCUS_BACKLASH_CORR,
    .dly_time      = FOCUS_PSUM_DELAY,
};

static motor_config_t g_motor_zoom_cfg =
{
    .chnel         = LV8044_CHNEL3,                  // * LV8044_CHNEL1 or LV8044_CHNEL3
    .mode          = LV8044_MODE_4W12PHASE,
    .pwm_sig_input = 3,                              // * PWM3
    .def_pdir      = 1,                              // *
    .pwm_freq      = 2000,
    .work_status   = MOTOR_STATUS_FREE,
    .motor_status  = MOTOR_STATUS_FREE,
    .max_step_size = 1024,
    .move_to_pi    = false,
    .min_pos       = WIDE_BOUND,
    .max_pos       = TELE_BOUND,
    .curr_pos      = WIDE_BOUND,
    .set_final_step  = false,
    .move_dir      = ZOOM_DIR_INIT,
    .move_spd      = LV8044_MOTOR_SPD_1X,            // *
    .backlash_comp = ZOOM_BACKLASH_CORR,
    .dly_time      = ZOOM_PSUM_DELAY,
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
    {    0,         20,  340},         // *
    {  200,        540,  860},         // *
    {  400,        960, 1270},         // *
    {  600,       1250, 1550},         // *
    {  800,       1460, 1760},         // *
    { 1000,       1650, 1940},         // *
    { 1200,       1770, 2050},         // *
    { 1400,       1860, 2140},         // *
    { 1600,       1920, 2200},         // *
    { 1800,       1970, 2250},         // *
    { 2000,       2000, 2280},         // *
    { TELE_BOUND, 2010, INF_BOUND}     // *
};

#define MAX_ZF_TAB_SIZE    ARRAY_SIZE(g_zoom_focus_tab)


// config pinmux ------------------------------------------------------------------------------------------------------------------
static pmuReg_t lens_pmu_reg[] = {
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
     .bits_mask = BIT15 | BIT14 | BIT13 | BIT12 | BIT11 | BIT10,
     .lock_bits = BIT15 | BIT14 | BIT13 | BIT12 | BIT11 | BIT10,
     .init_val  = 0x00000000,
     .init_mask = BIT15 | BIT14 | BIT13 | BIT12 | BIT11 | BIT10,
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
extern void pwm_set_pulse_num(int id, int num);
extern void pwm_mode_change(int id, int mode);
extern void pwm_interrupt_enable(int id);
extern void pwm_interrupt_disable(int id);


extern int lens_dev_construct(lens_dev_t *pdev);
extern void lens_dev_deconstruct(lens_dev_t *pdev);

#endif    /* __LENS_LV8044_H */

