/**
 * @file lens_lv8044.c
 *  driver of LV8044 motor driver
 * Copyright (C) 2015 GM Corp. (http://www.grain-media.com)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stddef.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "spi_common.h"
#include "lens_lv8044/lens_lv8044_tvt1.h"


static u8 lens_driver_support = 3;    // bit0: focus, bit1: zoom, bit2: iris
module_param(lens_driver_support, byte, S_IRUGO);
MODULE_PARM_DESC(lens_driver_support, "lens driver support");

static int lens_pwm_ch = 7;    // 0~7
module_param(lens_pwm_ch, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lens_pwm_ch, "lens pwm ch sel");


static lens_dev_t *g_plens = NULL;
static DEFINE_SPINLOCK(lock_ctl);


static void lv8044_write_spi_cmd(const spi_data_fmt_t send_data)
{
    u32 i;
    unsigned long flags;

    spin_lock_irqsave(&lock_ctl, flags);

    // set STB low => send data => set STB high
    gpio_set_value(SPI_SANYO_STB_PIN, 0);
    gpio_set_value(SPI_SANYO_CLK_PIN, 0);
    TDELAY(1);

    for (i = 0; i < 8; i++) {
        if ((send_data.data >> i) & 0x01) {
            gpio_set_value(SPI_SANYO_DAT_PIN, 1);
        }
        else {
            gpio_set_value(SPI_SANYO_DAT_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_SANYO_CLK_PIN, 1);
        TDELAY(1);
        gpio_set_value(SPI_SANYO_CLK_PIN, 0);
        TDELAY(1);
    }

    gpio_set_value(SPI_SANYO_STB_PIN, 1);
    TDELAY(1);
    gpio_set_value(SPI_SANYO_CLK_PIN, 1);
    TDELAY(2);
    gpio_set_value(SPI_SANYO_CLK_PIN, 0);
    TDELAY(2);

    spin_unlock_irqrestore(&lock_ctl, flags);
}

static int lv8044_transmit_cmd(LV8044_CHNEL_NUM chnel, LV8044_CMD_TYPE cmd_type, char cmd_arg)
{
    int ret = 0;
    motor_config_t *motor_cfg;
    spi_data_fmt_t spi_ctl;

    motor_cfg = (g_motor_focus_cfg.chnel == chnel) ? &g_motor_focus_cfg : ((g_motor_zoom_cfg.chnel == chnel) ? &g_motor_zoom_cfg : NULL);

    spi_ctl.data = 0x00;

    if ((chnel == LV8044_CHNEL1) || (chnel == LV8044_CHNEL2)) {
        switch (cmd_type) {
            case LV8044_CMD_SET_MODE:
                spi_ctl.D.b0 = 0;
                spi_ctl.D.b1 = 0;
                spi_ctl.D.b2 = 0;

                if (motor_cfg != NULL) {
                    switch (motor_cfg->mode) {
                        case LV8044_MODE_2PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE_FULL:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 1;
                            break;
                        case LV8044_MODE_4W12PHASE:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 1;
                            break;
                        default:
                            ret = -1;
                            break;
                    }
                }
                else {
                    ret = -1;
                }

                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = 1;    // referense voltage 33%
                spi_ctl.D.b7 = 0;
                break;

            case LV8044_CMD_SET_CURR:
                spi_ctl.D.b0 = 0;
                spi_ctl.D.b1 = 0;
                spi_ctl.D.b2 = 0;

                if (motor_cfg != NULL) {
                    switch (motor_cfg->mode) {
                        case LV8044_MODE_2PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE_FULL:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 1;
                            break;
                        case LV8044_MODE_4W12PHASE:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 1;
                            break;
                        default:
                            ret = -1;
                            break;
                    }
                }
                else {
                    ret = -1;
                }

                if (cmd_arg == 0) {    // referense voltage 100%
                    spi_ctl.D.b5 = 0;
                    spi_ctl.D.b6 = 0;
                }
                else if (cmd_arg == 1) {    // referense voltage 67%
                    spi_ctl.D.b5 = 1;
                    spi_ctl.D.b6 = 0;
                }
                else if (cmd_arg == 2) {    // referense voltage 50%
                    spi_ctl.D.b5 = 0;
                    spi_ctl.D.b6 = 1;
                }
                else {    // referense voltage 33%
                    spi_ctl.D.b5 = 1;
                    spi_ctl.D.b6 = 1;
                }

                spi_ctl.D.b7 = 0;
                break;

            case LV8044_CMD_RESET:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 0;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 0;    // reset
                spi_ctl.D.b6 = 0;    // disable output
                break;

            case LV8044_CMD_SET_DIR_ENABLE:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 0;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = 1;
                break;

            case LV8044_CMD_ENABLE:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 0;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == FOCUS_DIR_INF) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = (cmd_arg != 0) ? 1 : 0;    // enable/disable output
                break;

            case LV8044_CMD_UNKNOWN:
            default:
                ret = -1;
                break;
        }
    }
    else if ((chnel == LV8044_CHNEL3) || (chnel == LV8044_CHNEL4)) {
        switch (cmd_type) {
            case LV8044_CMD_SET_MODE:
                spi_ctl.D.b0 = 0;
                spi_ctl.D.b1 = 1;
                spi_ctl.D.b2 = 0;

                if (motor_cfg != NULL) {
                    switch (motor_cfg->mode) {
                        case LV8044_MODE_2PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE_FULL:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 1;
                            break;
                        case LV8044_MODE_4W12PHASE:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 1;
                            break;
                        default:
                            ret = -1;
                            break;
                    }
                }
                else {
                    ret = -1;
                }

                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = 1;    // referense voltage 33%
                spi_ctl.D.b7 = 1;    // microstep mode
                break;

            case LV8044_CMD_SET_CURR:
                spi_ctl.D.b0 = 0;
                spi_ctl.D.b1 = 1;
                spi_ctl.D.b2 = 0;

                if (motor_cfg != NULL) {
                    switch (motor_cfg->mode) {
                        case LV8044_MODE_2PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE_FULL:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 0;
                            break;
                        case LV8044_MODE_12PHASE:
                            spi_ctl.D.b3 = 0;
                            spi_ctl.D.b4 = 1;
                            break;
                        case LV8044_MODE_4W12PHASE:
                            spi_ctl.D.b3 = 1;
                            spi_ctl.D.b4 = 1;
                            break;
                        default:
                            ret = -1;
                            break;
                    }
                }
                else {
                    ret = -1;
                }

                if (cmd_arg == 0) {    // referense voltage 100%
                    spi_ctl.D.b5 = 0;
                    spi_ctl.D.b6 = 0;
                }
                else if (cmd_arg == 1) {    // referense voltage 67%
                    spi_ctl.D.b5 = 1;
                    spi_ctl.D.b6 = 0;
                }
                else if (cmd_arg == 2) {    // referense voltage 50%
                    spi_ctl.D.b5 = 0;
                    spi_ctl.D.b6 = 1;
                }
                else {    // referense voltage 33%
                    spi_ctl.D.b5 = 1;
                    spi_ctl.D.b6 = 1;
                }

                spi_ctl.D.b7 = 1;    // microstep (stepping motor) mode
                break;

            case LV8044_CMD_RESET:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 1;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 0;    // reset
                spi_ctl.D.b6 = 0;    // disable output
                break;

            case LV8044_CMD_SET_DIR_ENABLE:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 1;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = 1;
                break;

            case LV8044_CMD_ENABLE:
                spi_ctl.D.b0 = 1;
                spi_ctl.D.b1 = 1;
                spi_ctl.D.b2 = 0;
                if (motor_cfg->def_pdir == 0) {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CW : DIR_CCW;
                }
                else {
                    spi_ctl.D.b3 = (motor_cfg->move_dir == ZOOM_DIR_WIDE) ? DIR_CCW : DIR_CW;
                }
                spi_ctl.D.b4 = 0;
                spi_ctl.D.b5 = 1;
                spi_ctl.D.b6 = (cmd_arg != 0) ? 1 : 0;    // enable/disable output
                break;

            case LV8044_CMD_UNKNOWN:
            default:
                ret = -1;
                break;
        }
    }
    else {
        ret = -1;
    }

    if (ret != -1) {
        lv8044_write_spi_cmd(spi_ctl);
    }
    else {
        isp_err("lv8044_transmit_cmd err!\n");
    }

    return ret;
}

static int set_motor_pos(const MOTOR_SEL chnel, s32 step_pos, const LV8044_MOTOR_SPEED move_speed)
{
    motor_config_t *motor_cfg;

    motor_cfg = (chnel == MOTOR_FOCUS) ? &g_motor_focus_cfg : ((chnel == MOTOR_ZOOM) ? &g_motor_zoom_cfg : NULL);

    if ((motor_cfg != NULL) && (step_pos != 0)) {
        motor_cfg->work_status = motor_cfg->motor_status = MOTOR_STATUS_BUSY;

        lv8044_transmit_cmd(motor_cfg->chnel, LV8044_CMD_SET_DIR_ENABLE, motor_cfg->move_dir);
        lv8044_transmit_cmd(motor_cfg->chnel, LV8044_CMD_SET_CURR, 0);

        step_pos = (step_pos >= 0) ? step_pos : -step_pos;

        pwm_set_freq(motor_cfg->pwm_sig_input, (int)move_speed*motor_cfg->pwm_freq/4);

        switch (motor_cfg->mode) {
            case LV8044_MODE_2PHASE:
                step_pos *= 1;
                break;

            case LV8044_MODE_12PHASE_FULL:
            case LV8044_MODE_12PHASE:
            default:
                step_pos *= 2;
                break;

            case LV8044_MODE_4W12PHASE:
                step_pos *= 16;
                break;
        }

        pwm_set_pulse_num(motor_cfg->pwm_sig_input, step_pos);
        pwm_update(motor_cfg->pwm_sig_input);    // need update
        msleep(1);    // must be added for motor power on ready
        pwm_tmr_start(motor_cfg->pwm_sig_input);
    }
    else {
        step_pos = 0;
    }

    return step_pos;
}

static irqreturn_t lv8044_pwm_end_isr(int irq, void *dev_id)
{
    static bool dly_en = false;
    unsigned long flags;
    motor_config_t *motor_cfg;

    spin_lock_irqsave(&lock_ctl, flags);

    motor_cfg = (g_motor_focus_cfg.motor_status == MOTOR_STATUS_BUSY) ? &g_motor_focus_cfg : ((g_motor_zoom_cfg.motor_status == MOTOR_STATUS_BUSY) ? &g_motor_zoom_cfg : NULL);

    if (motor_cfg != NULL) {
        if (dly_en == true) {    // delay 2 msec and then power down
            pwm_tmr_stop(lens_pwm_ch);
            pwm_clr_intrpt(lens_pwm_ch);
            dly_en = false;

            lv8044_transmit_cmd(motor_cfg->chnel, LV8044_CMD_SET_CURR, 3);

            motor_cfg->motor_status = MOTOR_STATUS_FREE;

            if (motor_cfg->set_final_step == true) {
                motor_cfg->set_final_step = false;
                motor_cfg->work_status = MOTOR_STATUS_FREE;
            }
        }
        else {
            pwm_tmr_stop(motor_cfg->pwm_sig_input);
            pwm_clr_intrpt(motor_cfg->pwm_sig_input);
            dly_en = true;

            pwm_set_pulse_num(lens_pwm_ch, 3);    // delay 2 msec and then power down
            pwm_update(lens_pwm_ch);     // need update
            pwm_tmr_start(lens_pwm_ch);
        }
    }
    else {
        isp_err("lv8044_pwm_end_isr err!\n");
    }

    spin_unlock_irqrestore(&lock_ctl, flags);

    return IRQ_HANDLED;
}

//-------------------------------------------------------------------------
static s32 check_busy_all(void)
{
    s32 ret = 0;

    if (lens_driver_support & LENS_SUPPORT_FOCUS) {
        ret |= g_motor_focus_cfg.work_status;
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        ret |= g_motor_zoom_cfg.work_status;
    }

    return (ret);
}

//-------------------------------------------------------------------------
static int calc_focus_range(int *fmin_value, int *fmax_value)
{
    int i, tmp, fmin, fmax;

    fmin = g_zoom_focus_tab[0].focus_min;
    fmax = g_zoom_focus_tab[0].focus_max;

    for (i = 0; i < MAX_ZF_TAB_SIZE; i++) {
        if (g_zoom_focus_tab[i].zoom_pos >= g_motor_zoom_cfg.curr_pos) {
            tmp = (i != 0) ? (g_zoom_focus_tab[i].zoom_pos - g_zoom_focus_tab[i-1].zoom_pos) : 1;

            if (tmp != 0) {
                fmin = (i != 0) ? (g_zoom_focus_tab[i-1].focus_min + (g_zoom_focus_tab[i].focus_min - g_zoom_focus_tab[i-1].focus_min) * (g_motor_zoom_cfg.curr_pos - g_zoom_focus_tab[i-1].zoom_pos) / tmp) : fmin;
                fmax = (i != 0) ? (g_zoom_focus_tab[i-1].focus_max + (g_zoom_focus_tab[i].focus_max - g_zoom_focus_tab[i-1].focus_max) * (g_motor_zoom_cfg.curr_pos - g_zoom_focus_tab[i-1].zoom_pos) / tmp) : fmax;
            }
            else {
                isp_err("Incorrect g_zoom_focus_tab[]!\n");
            }
            break;
        }
    }

    *fmin_value = fmin;
    *fmax_value = fmax;

    return i;
}

static int wait_move_end(int pin, const int timeout_ms)
{
    int cnt = 0;

    while ((g_motor_focus_cfg.motor_status != MOTOR_STATUS_FREE) || (g_motor_zoom_cfg.motor_status != MOTOR_STATUS_FREE)) {
        if (cnt++ >= timeout_ms) {
            pin = (g_motor_focus_cfg.motor_status != MOTOR_STATUS_FREE) ? MOT_FOCUS_PIN : MOT_ZOOM_PIN;
            isp_err("wait_move_end timeout pin(%d)!\n", pin);
            return -1;
        }
        msleep(1);
    }

    return 0;
}

//-------------------------------------------------------------------------
// FOCUS CONTROL
//-------------------------------------------------------------------------
static s32 near_focus(int steps, const int gear_backlash, const LV8044_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(MOT_FOCUS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos - steps) < near_bound)) {
            steps = g_motor_focus_cfg.curr_pos - near_bound;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_focus_cfg.move_dir = FOCUS_DIR_NEAR;
        }

        set_motor_pos(MOTOR_FOCUS, -set_step, focus_speed);

        g_motor_focus_cfg.curr_pos -= steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 inf_focus(int steps, const int gear_backlash, const LV8044_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(MOT_FOCUS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos + steps) > inf_bound)) {
            steps = inf_bound - g_motor_focus_cfg.curr_pos;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_focus_cfg.move_dir = FOCUS_DIR_INF;
        }

        set_motor_pos(MOTOR_FOCUS, set_step, focus_speed);

        g_motor_focus_cfg.curr_pos += steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

//-------------------------------------------------------------------------
// ZOOM CONTROL
//-------------------------------------------------------------------------
static s32 wide_zoom(int steps, const int gear_backlash, const LV8044_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, wide_bound = g_zoom_focus_tab[0].zoom_pos;

    if (wait_move_end(MOT_ZOOM_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos - steps) < wide_bound)) {
            steps = g_motor_zoom_cfg.curr_pos - wide_bound;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_zoom_cfg.move_dir = ZOOM_DIR_WIDE;
        }

        set_motor_pos(MOTOR_ZOOM, -set_step, zoom_speed);

        g_motor_zoom_cfg.curr_pos -= steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 tele_zoom(int steps, const int gear_backlash, const LV8044_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, tele_bound = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos;

    if (wait_move_end(MOT_ZOOM_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos + steps) > tele_bound)) {
            steps = tele_bound - g_motor_zoom_cfg.curr_pos;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_zoom_cfg.move_dir = ZOOM_DIR_TELE;
        }

        set_motor_pos(MOTOR_ZOOM, set_step, zoom_speed);

        g_motor_zoom_cfg.curr_pos += steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

//-------------------------------------------------------------------------
static void update_focus_pos(void)
{
    s32 set_step, rest_step;
    int focus_pos, near_bound, inf_bound;

    calc_focus_range(&near_bound, &inf_bound);

    if ((g_motor_focus_cfg.curr_pos <= near_bound) || (g_motor_focus_cfg.curr_pos >= inf_bound)) {
        focus_pos = (near_bound + inf_bound) / 2;

        if (focus_pos != g_motor_focus_cfg.curr_pos) {
            if (focus_pos > g_motor_focus_cfg.curr_pos) {
                // compensate backlash first
                if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
                    rest_step = g_motor_focus_cfg.backlash_comp;

                    do {
                        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                        inf_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                        rest_step -= set_step;
                        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                        msleep(g_motor_focus_cfg.dly_time);
                    } while (rest_step > 0);
                }

                rest_step = focus_pos - g_motor_focus_cfg.curr_pos;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }
            else {
                // compensate backlash first
                if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                    rest_step = g_motor_focus_cfg.backlash_comp;

                    do {
                        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                        near_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                        rest_step -= set_step;
                        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                        msleep(g_motor_focus_cfg.dly_time);
                    } while (rest_step > 0);
                }

                rest_step = g_motor_focus_cfg.curr_pos - focus_pos;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }
        }
    }
}

static s32 focus_move(s32 steps)
{
    s32 ret = 0, set_step, rest_step;
    int near_bound, inf_bound;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    calc_focus_range(&near_bound, &inf_bound);

    if (steps > 0) {
        if ((g_motor_focus_cfg.curr_pos + steps) <= inf_bound) {
            // compensate backlash first
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
                rest_step = g_motor_focus_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    inf_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = steps;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);
        }
        else {
            isp_warn("focus_move reach upper bound!\n");
        }
    }
    else if (steps < 0) {
        if ((g_motor_focus_cfg.curr_pos + steps) >= near_bound) {
            // compensate backlash first
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                rest_step = g_motor_focus_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    near_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = -steps;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;
        }
        else {
            isp_warn("focus_move reach lower bound!\n");
        }
    }
    else {
        isp_warn("focus_move steps=0!\n");
    }

    up(&pdev->lock);

    return ret;
}

static s32 zoom_move(s32 steps)
{
    s32 ret = 0, set_step, rest_step;
    int near_bound, inf_bound;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    steps = (steps * (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) + 50) / 100;

    if (steps > 0) {
        if ((g_motor_zoom_cfg.curr_pos + steps) <= g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos) {
            // compensate backlash first
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_TELE) {
                rest_step = g_motor_zoom_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                    tele_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = steps;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += tele_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);
        }
        else {
            isp_warn("zoom_move reach upper bound!\n");
        }
    }
    else if (steps < 0) {
        if ((g_motor_zoom_cfg.curr_pos + steps) >= g_zoom_focus_tab[0].zoom_pos) {
            // compensate backlash first
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_WIDE) {
                rest_step = g_motor_zoom_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                    wide_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = -steps;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;
        }
        else {
            isp_warn("zoom_move reach lower bound!\n");
        }
    }
    else {
        isp_warn("zoom_move steps=0!\n");
    }

    calc_focus_range(&near_bound, &inf_bound);

    pdev->curr_zoom_x10   = WIDE_FOCAL_VALUE + ((TELE_FOCAL_VALUE - WIDE_FOCAL_VALUE) * (g_motor_zoom_cfg.curr_pos - g_zoom_focus_tab[0].zoom_pos) + (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) / 2) / (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos);
    pdev->focus_low_bound = near_bound;
    pdev->focus_hi_bound  = inf_bound;

    if (ret != 0) {
        update_focus_pos();
    }

    up(&pdev->lock);

    return ret;
}

static s32 focus_move_to_pos(s32 pos)
{
    s32 ret = 0, near_bound, inf_bound, set_step, rest_step;
    lens_dev_t *pdev = g_plens;

    if (g_motor_focus_cfg.move_to_pi == false) {
        return 0;
    }

    down(&pdev->lock);

    calc_focus_range(&near_bound, &inf_bound);

    if ((pos >= near_bound) && (pos <= inf_bound)) {
        if (pos > g_motor_focus_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
                rest_step = g_motor_focus_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    inf_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = pos - g_motor_focus_cfg.curr_pos;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);
        }
        else if (pos < g_motor_focus_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                rest_step = g_motor_focus_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    near_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = g_motor_focus_cfg.curr_pos - pos;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;
        }
        else {
            isp_warn("focus_move_to_pos step=0!\n");
        }
    }
    else {
        isp_warn("focus_move_to_pos illegal(%d, %d, %d)\n", near_bound, inf_bound, g_motor_focus_cfg.curr_pos);
    }

    up(&pdev->lock);

    return ret;
}

static s32 zoom_move_to_pos(s32 pos)
{
    s32 ret = 0, zoom_pos_min, zoom_pos_max, set_step, rest_step;
    int near_bound, inf_bound;
    lens_dev_t *pdev = g_plens;

    if (g_motor_zoom_cfg.move_to_pi == false) {
        return 0;
    }

    down(&pdev->lock);

    pos = g_zoom_focus_tab[0].zoom_pos + (pos * (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) + 50) / 100;

    zoom_pos_min = g_zoom_focus_tab[0].zoom_pos;
    zoom_pos_max = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos;

    if ((pos >= zoom_pos_min) && (pos <= zoom_pos_max)) {
        if (pos > g_motor_zoom_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_TELE) {
                rest_step = g_motor_zoom_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                    tele_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = pos - g_motor_zoom_cfg.curr_pos;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += tele_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);
        }
        else if (pos < g_motor_zoom_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_WIDE) {
                rest_step = g_motor_zoom_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                    wide_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
                    rest_step -= set_step;
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = g_motor_zoom_cfg.curr_pos - pos;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;
        }
        else {
            isp_warn("zoom_move_to_pos step=0!\n");
        }

        calc_focus_range(&near_bound, &inf_bound);

        pdev->curr_zoom_x10   = WIDE_FOCAL_VALUE + ((TELE_FOCAL_VALUE - WIDE_FOCAL_VALUE) * (g_motor_zoom_cfg.curr_pos - g_zoom_focus_tab[0].zoom_pos) + (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) / 2) / (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos);
        pdev->focus_low_bound = near_bound;
        pdev->focus_hi_bound  = inf_bound;

        if (ret != 0) {
            update_focus_pos();
        }
    }
    else {
        isp_warn("zoom_move_to_pos illegal(%d, %d, %d)\n", g_zoom_focus_tab[0].zoom_pos, g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos, g_motor_zoom_cfg.curr_pos);
    }

    up(&pdev->lock);

    return ret;
}

static s32 get_focus_pos(void)
{
    return g_motor_focus_cfg.curr_pos;
}

static s32 get_zoom_pos(void)
{
    s32 zpos_tmp;

    zpos_tmp = (100 * (g_motor_zoom_cfg.curr_pos - g_zoom_focus_tab[0].zoom_pos) + (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) / 2) / (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos);

    return zpos_tmp;
}

static s32 focus_move_to_pi(void)
{
    s32 set_step, rest_step;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    rest_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].focus_max + INIT_FOCUS_POS + (INF_BOUND/10);

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        near_focus(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);

#if 1    // increase go home consistency
    lv8044_transmit_cmd(g_motor_focus_cfg.chnel, LV8044_CMD_ENABLE, 0);    // release motor excitation

    msleep(500);

    lv8044_transmit_cmd(g_motor_focus_cfg.chnel, LV8044_CMD_ENABLE, 1);    // tighten motor and gear

    rest_step = INF_BOUND / 20;

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        near_focus(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);
#endif

    if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
        rest_step = g_motor_focus_cfg.backlash_comp;

        do {
            set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
            inf_focus(0, set_step, LV8044_MOTOR_SPD_1X, 0);
            rest_step -= set_step;
            g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
            msleep(g_motor_focus_cfg.dly_time);
        } while (rest_step > 0);
    }

    rest_step = INIT_FOCUS_POS;

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        inf_focus(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);

    g_motor_focus_cfg.curr_pos = g_zoom_focus_tab[0].focus_min;

    g_motor_focus_cfg.move_to_pi = true;

    up(&pdev->lock);

    return 1;
}

static s32 zoom_move_to_pi(void)
{
    s32 set_step, rest_step;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    rest_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos + INIT_ZOOM_POS + (TELE_BOUND/10);

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        wide_zoom(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);

#if 1    // increase go home consistency
    lv8044_transmit_cmd(g_motor_zoom_cfg.chnel, LV8044_CMD_ENABLE, 0);    // release motor excitation

    msleep(500);

    lv8044_transmit_cmd(g_motor_zoom_cfg.chnel, LV8044_CMD_ENABLE, 1);    // tighten motor and gear

    rest_step = TELE_BOUND / 20;

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        wide_zoom(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);
#endif

    if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_TELE) {
        rest_step = g_motor_zoom_cfg.backlash_comp;

        do {
            set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
            tele_zoom(0, set_step, LV8044_MOTOR_SPD_1X, 0);
            rest_step -= set_step;
            g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
            msleep(g_motor_zoom_cfg.dly_time);
        } while (rest_step > 0);
    }

    rest_step = INIT_ZOOM_POS;

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        tele_zoom(set_step, 0, LV8044_MOTOR_SPD_1X, 0);
        rest_step -= set_step;
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);

    g_motor_zoom_cfg.curr_pos = g_zoom_focus_tab[0].zoom_pos;

    pdev->focus_low_bound = g_zoom_focus_tab[0].focus_min;
    pdev->focus_hi_bound  = g_zoom_focus_tab[0].focus_max;

    update_focus_pos();

    g_motor_zoom_cfg.move_to_pi = true;

    up(&pdev->lock);

    return 1;
}

static int lens_init(void)
{
    isp_info("lens_init\n");
    isp_info("%s\n", LENS_ID);

    if (check_busy_all() != 0) {
        isp_err("Motor controller is not found!\n");
        return -1;
    }

    if (lens_driver_support & LENS_SUPPORT_FOCUS) {
        if (focus_move_to_pi() != 1) {
            return -1;
        }
    }

    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        msleep(100);

        if (zoom_move_to_pi() != 1) {
            return -1;
        }
    }

    return 0;
}

static s32 g_ircut_state = 0;    // 0: standby / 1: open / 2: close

static s32 set_ircut_state(s32 ctl_state)
{
    if (ctl_state == 1) {
        g_ircut_state = 1;
    }
    else if (ctl_state == 2) {
        g_ircut_state = 2;
    }
    else {
        g_ircut_state = 0;
    }

    return 0;
}

static s32 get_ircut_state(void)
{
    return g_ircut_state;
}


//============================================================================
// external functions
//============================================================================
static int g_lens_pmu_fd = 0;

static int SpiInit(void)
{
    int fd, ret = 0;

    isp_info("SpiInit\n");

    // request GPIO
    ret = gpio_request(SPI_SANYO_STB_PIN, "SPI_STB");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_SANYO_CLK_PIN, "SPI_CLK");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_SANYO_DAT_PIN, "SPI_DAT");
    if (ret < 0) {
        return ret;
    }

    if (SPI_SANYO_RESET_PIN >= 0) {
        ret = gpio_request(SPI_SANYO_RESET_PIN, "RESET");
        if (ret < 0) {
            return ret;
        }
    }

    // init GPIO
    gpio_direction_output(SPI_SANYO_STB_PIN, 1);
    gpio_direction_output(SPI_SANYO_CLK_PIN, 1);
    gpio_direction_output(SPI_SANYO_DAT_PIN, 0);

    if (SPI_SANYO_RESET_PIN >= 0) {
        // reset motor driver IC
        gpio_direction_output(SPI_SANYO_RESET_PIN, 1);
        gpio_set_value(SPI_SANYO_RESET_PIN, 0);
        msleep(2);
        gpio_set_value(SPI_SANYO_RESET_PIN, 1);
    }

    // set pinmux
    if ((fd = ftpmu010_register_reg(&isp_lens_pmu_reg_info)) < 0) {
        isp_err("Register pmu failed!\n");
        ret = -1;
    }

    g_lens_pmu_fd = fd;

    return ret;
}

static int SpiExit(void)
{
    // release GPIO
    gpio_free(SPI_SANYO_STB_PIN);
    gpio_free(SPI_SANYO_CLK_PIN);
    gpio_free(SPI_SANYO_DAT_PIN);

    if (SPI_SANYO_RESET_PIN >= 0) {
        gpio_free(SPI_SANYO_RESET_PIN);
    }

    if (g_lens_pmu_fd != 0) {
        ftpmu010_deregister_reg(g_lens_pmu_fd);
        g_lens_pmu_fd = 0;
    }

    return 0;
}

int lv8044_motor_init(MOTOR_SEL motor_sel)
{
    motor_config_t *motor_cfg;

    motor_cfg = (motor_sel == MOTOR_FOCUS) ? &g_motor_focus_cfg : ((motor_sel == MOTOR_ZOOM) ? &g_motor_zoom_cfg : NULL);

    if ((motor_sel >= MOTOR_IRIS) || (motor_cfg == NULL)) {
        return -1;
    }

    if (lv8044_transmit_cmd(motor_cfg->chnel, LV8044_CMD_SET_MODE, motor_cfg->mode) != 0) {
        return -1;
    }

    // request PWM
    if (pwm_dev_request(motor_cfg->pwm_sig_input) == false) {
        isp_err("Fail to request PWM%d!\n", motor_cfg->pwm_sig_input);
        return -EINVAL;
    }

    motor_cfg->pwm_freq = (motor_cfg->mode != LV8044_MODE_12PHASE) ? ((int)motor_cfg->mode * motor_cfg->pwm_freq) : (((int)motor_cfg->mode -1) * motor_cfg->pwm_freq);
    motor_cfg->max_step_size = (motor_cfg->mode != LV8044_MODE_12PHASE) ? (motor_cfg->max_step_size / (int)motor_cfg->mode) : (motor_cfg->max_step_size / ((int)motor_cfg->mode -1));

    pwm_tmr_stop(motor_cfg->pwm_sig_input);
    pwm_clksrc_switch(motor_cfg->pwm_sig_input, 0);
    pwm_set_freq(motor_cfg->pwm_sig_input, (int)motor_cfg->move_spd*motor_cfg->pwm_freq/4);
    pwm_set_duty_steps(motor_cfg->pwm_sig_input, 100);
    pwm_set_duty_ratio(motor_cfg->pwm_sig_input, 100/2);
    pwm_update(motor_cfg->pwm_sig_input);    // need update
    pwm_mode_change(motor_cfg->pwm_sig_input, 2);    // 0: ONE_SHOT / 1: PWM_INTERVAL / 2: PWM_REPEAT
    pwm_interrupt_enable(motor_cfg->pwm_sig_input);

    return 0;
}

int lens_lv8044_construct(void)
{
    int ret = 0;
    lens_dev_t *pdev = g_plens;

    // set the IRQ type
    irq_set_irq_type(49, IRQ_TYPE_LEVEL_HIGH);

    // set ISR
    if (request_irq(49, lv8044_pwm_end_isr, IRQF_DISABLED, "PWM", NULL) < 0) {
        isp_err("%s: request_irq 49 failed\n", __FUNCTION__);
        ret = -1;
    }

    // for power down delay mechanism
    if (pwm_dev_request(lens_pwm_ch) == false) {
        isp_err("Fail to request PWM7!\n");
        return -EINVAL;
    }

    pwm_tmr_stop(lens_pwm_ch);
    pwm_clksrc_switch(lens_pwm_ch, 0);
    pwm_set_freq(lens_pwm_ch, 2000);    // 1 unit = 1 msec
    pwm_set_duty_steps(lens_pwm_ch, 100);
    pwm_set_duty_ratio(lens_pwm_ch, 100/2);
    pwm_update(lens_pwm_ch);    // need update
    pwm_mode_change(lens_pwm_ch, 2);    // 0: ONE_SHOT / 1: PWM_INTERVAL / 2: PWM_REPEAT
    pwm_interrupt_enable(lens_pwm_ch);

    snprintf(pdev->name, DEV_MAX_NAME_SIZE, LENS_NAME);
    pdev->capability          = 0;
    pdev->fn_focus_move_to_pi = NULL;
    pdev->fn_focus_get_pos    = NULL;
    pdev->fn_focus_set_pos    = NULL;
    pdev->fn_focus_move       = NULL;
    pdev->fn_zoom_move_to_pi  = NULL;
    pdev->fn_zoom_get_pos     = NULL;
    pdev->fn_zoom_set_pos     = NULL;
    pdev->fn_zoom_move        = NULL;

    if (lens_driver_support & LENS_SUPPORT_FOCUS) {
        pdev->capability         |= LENS_SUPPORT_FOCUS;
        pdev->fn_focus_move_to_pi = focus_move_to_pi;
        pdev->fn_focus_get_pos    = get_focus_pos;
        pdev->fn_focus_set_pos    = focus_move_to_pos;
        pdev->fn_focus_move       = focus_move;
        ret |= lv8044_motor_init(MOTOR_FOCUS);
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        pdev->capability        |= LENS_SUPPORT_ZOOM;
        pdev->fn_zoom_move_to_pi = zoom_move_to_pi;
        pdev->fn_zoom_get_pos    = get_zoom_pos;
        pdev->fn_zoom_set_pos    = zoom_move_to_pos;
        pdev->fn_zoom_move       = zoom_move;
        ret |= lv8044_motor_init(MOTOR_ZOOM);
    }

    pdev->fn_init            = lens_init;
    pdev->fn_check_busy      = check_busy_all;
    pdev->fn_ircut_set_state = set_ircut_state;
    pdev->fn_ircut_get_state = get_ircut_state;
    pdev->focus_low_bound    = g_zoom_focus_tab[0].focus_min;
    pdev->focus_hi_bound     = g_zoom_focus_tab[0].focus_max;
    pdev->zoom_stage_cnt     = 100;
    pdev->curr_zoom_index    = 0;
    pdev->curr_zoom_x10      = WIDE_FOCAL_VALUE;

    sema_init(&pdev->lock, 1);

    return ret;
}

void lens_lv8044_deconstruct(void)
{
    isp_info("Lens driver exit\n");

    if (lens_driver_support & LENS_SUPPORT_FOCUS) {
        pwm_tmr_stop(g_motor_focus_cfg.pwm_sig_input);
        pwm_dev_release(g_motor_focus_cfg.pwm_sig_input);
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        pwm_tmr_stop(g_motor_zoom_cfg.pwm_sig_input);
        pwm_dev_release(g_motor_zoom_cfg.pwm_sig_input);
    }

    pwm_dev_release(lens_pwm_ch);
    free_irq(49, NULL);

    SpiExit();
}

//============================================================================
// module initialization / finalization
//============================================================================
static int __init lens_lv8044_init(void)
{
    int ret = 0;

    // check ISP driver version
    if (isp_get_input_inf_ver() != ISP_INPUT_INF_VER) {
        ret = -EFAULT;
        isp_err("Input module version(%x) is not compatibility with fisp32x.ko(%x)!", ISP_INPUT_INF_VER, isp_get_input_inf_ver());
        return ret;
    }

    // allocate lens device information
    g_plens = kzalloc(sizeof(lens_dev_t), GFP_KERNEL);
    if (g_plens == NULL) {
        ret = -ENODEV;
        isp_err("[ERROR]fail to alloc dev_lens!\n");
        return ret;
    }

    // IO initialize
    if (SpiInit() < 0) {
        ret = -ENODEV;
        isp_err("[ERROR]SpiInit() fail!\n");

        kfree(g_plens);
        g_plens = NULL;
    }

    // construct lens device information
    if (lens_lv8044_construct() < 0) {
        ret = -ENODEV;
        isp_err("[ERROR]lens_lv8044_construct() fail!\n");

        kfree(g_plens);
        g_plens = NULL;
    }

    isp_register_lens(g_plens);

    return ret;
}

static void __exit lens_lv8044_exit(void)
{
    // remove lens
    lens_lv8044_deconstruct();

    isp_unregister_lens(g_plens);

    if (g_plens != NULL) {
        kfree(g_plens);
        g_plens = NULL;
    }
}

module_init(lens_lv8044_init);
module_exit(lens_lv8044_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("LENS_LV8044");
MODULE_LICENSE("GPL");

