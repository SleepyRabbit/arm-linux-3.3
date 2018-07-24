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
#define LENS_ID                "tvt1-v10"        // *


// config GPIO --------------------------------------------------------------------------------------------------------------------
#define SPI_SANYO_STB_PIN      GPIO_ID(0, 10)    // *
#define SPI_SANYO_CLK_PIN      GPIO_ID(0, 11)    // *
#define SPI_SANYO_DAT_PIN      GPIO_ID(0, 12)    // *
#define SPI_SANYO_RESET_PIN    GPIO_ID(0, 13)    // *

#define MOT_FOCUS_PIN          129
#define MOT_ZOOM_PIN           131
#define FOCUS_PSUM_DELAY       ((1024000/LV8044_MODE_4W12PHASE)/(2000/1))            // *
#define ZOOM_PSUM_DELAY        ((1024000/LV8044_MODE_4W12PHASE)/(2000/2))            // *


// config lens parameters ---------------------------------------------------------------------------------------------------------
#define NEAR_BOUND             0                 // *
#define INF_BOUND              1420              // *
#define WIDE_BOUND             0                 // *
#define TELE_BOUND             700               // *
#define WIDE_FOCAL_VALUE       30                // * 3.0   mm
#define TELE_FOCAL_VALUE       105               // * 10.5 mm
#define INIT_FOCUS_POS         400               // *
#define INIT_ZOOM_POS          20                // *

#define FOCUS_BACKLASH_CORR    5                 // * correction for focus backlash
#define ZOOM_BACKLASH_CORR     8                 // * correction for zoom backlash


// config motor IC parameters -----------------------------------------------------------------------------------------------------
static motor_config_t g_motor_focus_cfg =
{
    .chnel         = LV8044_CHNEL1,                  // * LV8044_CHNEL1 or LV8044_CHNEL3
    .mode          = LV8044_MODE_4W12PHASE,
    .pwm_sig_input = 1,                              // * PWM1
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
    .def_pdir      = 0,                              // *
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
    {   0,          20,  210},         // *
    {  52,         310,  500},         // *
    { 100,         510,  700},         // *
    { 200,         820, 1000},         // *
    { 300,        1020, 1200},         // *
    { 400,        1140, 1320},         // *
    { 500,        1210, 1390},         // *
    { 600,        1240, INF_BOUND},    // *
    { TELE_BOUND, 1240, INF_BOUND}     // *
};

#define MAX_ZF_TAB_SIZE    ARRAY_SIZE(g_zoom_focus_tab)


// config pinmux ------------------------------------------------------------------------------------------------------------------
static pmuReg_t lens_pmu_reg[] = {
    /*
     * Multi-Function Port Setting Register 1 [offset=0x54]
     * ----------------------------------------------------------------
     *  [31:30] 0:GPIO_0[22],   1:SPI_TXD,     2:NAND_WEN
     *  [29:28] 0:GPIO_0[21],   1:SPI_SCLK,    2:NAND_CLE
     *  [27:26] 0:GPIO_0[20],   1:SPI_FS,      2:NAND_CE0
     *  [25:24] 0:GPIO_0[19],   1:I2C_SDA,     2:PWM1
     *  [23:22] 0:GPIO_0[18],   1:I2C_SCL,     2:PWM0
     *  [21:20] 0:GPIO_0[17],
     *  [19:18] 0:GPIO_0[16],
     *  [17:16] 0:GPIO_0[15],
     *  [15:14] 0:GPIO_0[14],
     *  [13:12] 0:GPIO_0[13],   1:CAP0_D4,     2:Bayer_D0  3:x
     *  [11:10] 0:GPIO_0[12],   1:CAP0_D5,     2:Bayer_D1  3:x
     *  [9:8]   0:GPIO_0[11],   1:CAP0_D6,     2:Bayer_D2  3:x
     *  [7:6]   0:GPIO_0[10],   1:CAP0_D7,     2:Bayer_D3  3:x
     *  [5:4]   0:GPIO_0[09],   1:CAP0_CLK,    2:x         3:x
     *  [3:2]   0:GPIO_0[08],
     *  [1:0]   0:GPIO_0[07],
     */
    {
     .reg_off   = 0x54,    // *
     .bits_mask = BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6,
     .lock_bits = BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6,
     .init_val  = 0x00000000,
     .init_mask = BIT13 | BIT12 | BIT11 | BIT10 | BIT9 | BIT8 | BIT7 | BIT6,
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

