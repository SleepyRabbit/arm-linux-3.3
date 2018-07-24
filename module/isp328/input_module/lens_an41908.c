/**
 * @file lens_an41908.c
 *  driver of AN41908 motor driver
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
#include "lens_an41908/lens_an41908_qihan1.h"


static u8 lens_driver_support = 3;    // bit0: focus, bit1: zoom, bit2: iris
module_param(lens_driver_support, byte, S_IRUGO);
MODULE_PARM_DESC(lens_driver_support, "lens driver support");


static lens_dev_t *g_plens = NULL;
static DEFINE_SPINLOCK(lock_ctl);


static void an41908_write_spi_cmd(spi_data_fmt_t send_data)
{
    u32 i;
    unsigned long flags;

    spin_lock_irqsave(&lock_ctl, flags);

    gpio_set_value(AN41908_VD_FZ_PIN, 1);
    TDELAY(3);
    gpio_set_value(AN41908_VD_FZ_PIN, 0);

    // set CS high => send data => set CS low
    gpio_set_value(SPI_PANASONIC_CS_PIN, 1);
    TDELAY(1);

    for (i = 0; i < 6; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);

        if ((send_data.addr >> i) & 0x01) {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 1);
        }
        else {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);
    }
    for (i = 0; i < 2; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);

        gpio_set_value(SPI_PANASONIC_TXD_PIN, 0);
        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);
    }

    for (i = 0; i < 16; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);

        if ((send_data.data >> i) & 0x01) {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 1);
        }
        else {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);
    }


    gpio_set_value(SPI_PANASONIC_CS_PIN, 0);
    TDELAY(1);
    gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);
    TDELAY(2);
    gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
    TDELAY(2);

    spin_unlock_irqrestore(&lock_ctl, flags);
}

static u16 an41908_read_spi_cmd(spi_data_fmt_t recv_data)
{
    u32 i;
    unsigned long flags;

    spin_lock_irqsave(&lock_ctl, flags);

    // set CS high => send data => set CS low
    gpio_set_value(SPI_PANASONIC_CS_PIN, 1);
    TDELAY(1);

    for (i = 0; i < 6; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);

        if ((recv_data.addr >> i) & 0x01) {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 1);
        }
        else {
            gpio_set_value(SPI_PANASONIC_TXD_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);
    }
    for (i = 0; i < 2; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);

        gpio_set_value(SPI_PANASONIC_TXD_PIN, 1);
        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);
    }

    recv_data.data = 0x0000;
    for (i = 0; i < 16; i++) {
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);
        TDELAY(1);
        gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
        TDELAY(1);

        if (gpio_get_value(SPI_PANASONIC_RXD_PIN)) {
            recv_data.data |= (1 << i);
        }
    }

    gpio_set_value(SPI_PANASONIC_CS_PIN, 0);
    TDELAY(1);
    gpio_set_value(SPI_PANASONIC_CLK_PIN, 0);
    TDELAY(2);
    gpio_set_value(SPI_PANASONIC_CLK_PIN, 1);
    TDELAY(2);

    spin_unlock_irqrestore(&lock_ctl, flags);

    return recv_data.data;
}

static int an41908_transmit_cmd(AN41908_CHNEL_NUM chnel, AN41908_CMD_TYPE cmd_type, int cmd_arg)
{
    int ret = 0, reg_cmd, reg_tmp;
    motor_config_t *motor_cfg;
    spi_data_fmt_t spi_ctl;

    motor_cfg = (g_motor_focus_cfg.chnel == chnel) ? &g_motor_focus_cfg : ((g_motor_zoom_cfg.chnel == chnel) ? &g_motor_zoom_cfg : NULL);

    switch (cmd_type) {
        case AN41908_SET_MOTOR_CFG:
            reg_cmd = (cmd_arg >> 16) & AN41908_REG_UNKNOWN;
            spi_ctl.data = cmd_arg & 0xFFFF;

            if ((reg_cmd >= AN41908_PULSE1_START_TIME) && (reg_cmd <= AN41908_IRCUT_CTL)) {
                spi_ctl.addr = (u8)reg_cmd;
            }
            else {
                ret = -1;
            }
            break;

        case AN41908_SET_MOTOR_SPD:
            spi_ctl.addr = AN41908_PWM_FREQ;
            spi_ctl.data = cmd_arg & 0xFFFF;
            break;

        case AN41908_SET_MOTOR_STEP:
            if (motor_cfg->chnel == AN41908_CHNEL1) {
                spi_ctl.addr = AN41908_SET_MOTOR1_STEP;
            }
            else if (motor_cfg->chnel == AN41908_CHNEL2) {
                spi_ctl.addr = AN41908_SET_MOTOR2_STEP;
            }
            else {
                ret = -1;
            }

            if (motor_cfg->mode == AN41908_MODE_64MACRO) {
                reg_tmp = 0x3000;
            }
            else if (motor_cfg->mode == AN41908_MODE_128MACRO) {
                reg_tmp = 0x2000;
            }
            else if (motor_cfg->mode == AN41908_MODE_256MACRO) {
                reg_tmp = 0x1000;
            }
            else {
                reg_tmp = 0;
                ret = -1;
            }

            if (motor_cfg->def_pdir == 0) {
                spi_ctl.data = (cmd_arg >= 0) ? ((0x0100 | motor_cfg->excite_en | reg_tmp) | (cmd_arg & 0x00FF)) : ((motor_cfg->excite_en | reg_tmp) | (ABS(cmd_arg) & 0x00FF));
            }
            else {
                spi_ctl.data = (cmd_arg >= 0) ? ((motor_cfg->excite_en | reg_tmp) | (cmd_arg & 0x00FF)) : ((0x0100 | motor_cfg->excite_en | reg_tmp) | (ABS(cmd_arg) & 0x00FF));
            }
            break;

        case AN41908_CMD_UNKNOWN:
        default:
            ret = -1;
            break;
    }

    if (ret != -1) {
        an41908_write_spi_cmd(spi_ctl);
    }
    else {
        isp_err("an41908_transmit_cmd err!\n");
    }

    return ret;
}

static u16 an41908_receive_cmd(AN41908_CHNEL_NUM chnel, AN41908_CMD_TYPE cmd_type, int cmd_arg)
{
    int cmd_status = 0;
    motor_config_t *motor_cfg;
    spi_data_fmt_t spi_ctl;

    motor_cfg = (g_motor_focus_cfg.chnel == chnel) ? &g_motor_focus_cfg : ((g_motor_zoom_cfg.chnel == chnel) ? &g_motor_zoom_cfg : NULL);

    switch (cmd_type) {
        case AN41908_SET_MOTOR_CFG:
            if (cmd_arg < (AN41908_REG_UNKNOWN << 16)) {
                spi_ctl.addr = (cmd_arg >> 16) & AN41908_REG_UNKNOWN;
            }
            else {
                cmd_status = -1;
            }
            break;

        case AN41908_SET_MOTOR_SPD:
            spi_ctl.addr = AN41908_PWM_FREQ;
            break;

        case AN41908_SET_MOTOR_STEP:
            if (motor_cfg->chnel == AN41908_CHNEL1) {
                spi_ctl.addr = AN41908_SET_MOTOR1_STEP;
            }
            else if (motor_cfg->chnel == AN41908_CHNEL2) {
                spi_ctl.addr = AN41908_SET_MOTOR2_STEP;
            }
            else {
                cmd_status = -1;
            }
            break;

        case AN41908_CMD_UNKNOWN:
        default:
            cmd_status = -1;
            break;
    }

    if (cmd_status != -1) {
        spi_ctl.data = an41908_read_spi_cmd(spi_ctl) & 0xFFFF;
    }
    else {
        spi_ctl.data = 0xFFFF;
        isp_err("an41908_receive_cmd err!\n");
    }

    return spi_ctl.data;
}

static int set_motor_pos(MOTOR_SEL chnel, s32 step_pos)
{
    motor_config_t *motor_cfg;

    motor_cfg = (chnel == MOTOR_FOCUS) ? &g_motor_focus_cfg : ((chnel == MOTOR_ZOOM) ? &g_motor_zoom_cfg : NULL);

    if (motor_cfg == NULL) {
        return -1;
    }

    if (step_pos != 0) {
        motor_cfg->work_status = motor_cfg->motor_status = MOTOR_STATUS_BUSY;

        an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_STEP, AN41908_MODE_64MACRO*step_pos);
    }
    else {
        an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_STEP, 0);

#ifndef FZ_PLS_PIN_EXIST
        motor_cfg->work_status = MOTOR_STATUS_FREE;
#endif
    }

    return step_pos;
}

//-------------------------------------------------------------------------
#ifdef FZ_PLS_PIN_EXIST
static irqreturn_t an41908_gpio_isr(int irq, void *dev_id)
{
    if (gpio_interrupt_check(MOT_FOCUS_PLS_PIN)) {
        g_motor_focus_cfg.motor_status = MOTOR_STATUS_FREE;

        if (g_motor_focus_cfg.set_final_step == true) {
            g_motor_focus_cfg.set_final_step = false;
            g_motor_focus_cfg.work_status = MOTOR_STATUS_FREE;
        }
        gpio_interrupt_clear(MOT_FOCUS_PLS_PIN);
        //isp_info("Focus GPIO ISR\n");
    }
    else if (gpio_interrupt_check(MOT_ZOOM_PLS_PIN)) {
        g_motor_zoom_cfg.motor_status = MOTOR_STATUS_FREE;

        if (g_motor_zoom_cfg.set_final_step == true) {
            g_motor_zoom_cfg.set_final_step = false;
            g_motor_zoom_cfg.work_status = MOTOR_STATUS_FREE;
        }
        gpio_interrupt_clear(MOT_ZOOM_PLS_PIN);
        //isp_info("Zoom GPIO ISR\n");
    }

    return IRQ_HANDLED;
}
#endif

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
#ifdef FZ_PLS_PIN_EXIST
    int cnt = 0;

    while ((g_motor_focus_cfg.motor_status != MOTOR_STATUS_FREE) || (g_motor_zoom_cfg.motor_status != MOTOR_STATUS_FREE)) {
        if (cnt++ >= timeout_ms) {
            pin = (g_motor_focus_cfg.motor_status != MOTOR_STATUS_FREE) ? MOT_FOCUS_PLS_PIN : MOT_ZOOM_PLS_PIN;
            isp_err("wait_move_end timeout pin(%d)!\n", pin);
            return -1;
        }
        msleep(1);
    }
#endif

    return 0;
}

//-------------------------------------------------------------------------
// FOCUS CONTROL
//-------------------------------------------------------------------------
static s32 near_focus(int steps, const int gear_backlash, const AN41908_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(MOT_FOCUS_PLS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos - steps) < near_bound)) {
            steps = g_motor_focus_cfg.curr_pos - near_bound;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_focus_cfg.move_dir = FOCUS_DIR_NEAR;
        }

        set_motor_pos(MOTOR_FOCUS, -set_step);

        g_motor_focus_cfg.curr_pos -= steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 inf_focus(int steps, const int gear_backlash, const AN41908_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(MOT_FOCUS_PLS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos + steps) > inf_bound)) {
            steps = inf_bound - g_motor_focus_cfg.curr_pos;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_focus_cfg.move_dir = FOCUS_DIR_INF;
        }

        set_motor_pos(MOTOR_FOCUS, set_step);

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
static s32 wide_zoom(int steps, const int gear_backlash, const AN41908_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, wide_bound = g_zoom_focus_tab[0].zoom_pos;

    if (wait_move_end(MOT_ZOOM_PLS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos - steps) < wide_bound)) {
            steps = g_motor_zoom_cfg.curr_pos - wide_bound;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_zoom_cfg.move_dir = ZOOM_DIR_WIDE;
        }

        set_motor_pos(MOTOR_ZOOM, -set_step);

        g_motor_zoom_cfg.curr_pos -= steps;
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 tele_zoom(int steps, const int gear_backlash, const AN41908_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, tele_bound = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos;

    if (wait_move_end(MOT_ZOOM_PLS_PIN, 200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos + steps) > tele_bound)) {
            steps = tele_bound - g_motor_zoom_cfg.curr_pos;
        }

        set_step = steps + gear_backlash;

        if (set_step != 0) {
            g_motor_zoom_cfg.move_dir = ZOOM_DIR_TELE;
        }

        set_motor_pos(MOTOR_ZOOM, set_step);

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
    #ifdef FZ_PLS_PIN_EXIST
                        g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                        msleep(g_motor_focus_cfg.dly_time);
                    } while (rest_step > 0);
                }

                rest_step = focus_pos - g_motor_focus_cfg.curr_pos;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);

                inf_focus(0, 0, g_motor_focus_cfg.move_spd, 0);
            }
            else {
                // compensate backlash first
                if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                    rest_step = g_motor_focus_cfg.backlash_comp;

                    do {
                        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                        near_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                        rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                        g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                        msleep(g_motor_focus_cfg.dly_time);
                    } while (rest_step > 0);
                }

                rest_step = g_motor_focus_cfg.curr_pos - focus_pos;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);

                near_focus(0, 0, g_motor_focus_cfg.move_spd, 0);
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

    // set peak pulse width to 130%
    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }

    msleep(1);    // for separate 2 SPI command time distance

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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = steps;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            inf_focus(0, 0, g_motor_focus_cfg.move_spd, 1);
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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = -steps;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;

            near_focus(0, 0, g_motor_focus_cfg.move_spd, 1);
        }
        else {
            isp_warn("focus_move reach lower bound!\n");
        }
    }
    else {
        isp_warn("focus_move steps=0!\n");
    }

    // restore standby peak pulse width
    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
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

    // set peak pulse width to 130%
    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_zoom_cfg.max_pulse_width);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_zoom_cfg.max_pulse_width);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }

    msleep(1);    // for separate 2 SPI command time distance

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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = steps;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += tele_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            tele_zoom(0, 0, g_motor_zoom_cfg.move_spd, 1);
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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = -steps;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;

            wide_zoom(0, 0, g_motor_zoom_cfg.move_spd, 1);
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

    // restore standby peak pulse width
    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
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

    // set peak pulse width to 130%
    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }

    msleep(1);    // for separate 2 SPI command time distance

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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = pos - g_motor_focus_cfg.curr_pos;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            inf_focus(0, 0, g_motor_focus_cfg.move_spd, 1);
        }
        else if (pos < g_motor_focus_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                rest_step = g_motor_focus_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                    near_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
                    rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                    g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_focus_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = g_motor_focus_cfg.curr_pos - pos;

            do {
                set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
                ret += near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
                g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
                g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_focus_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;

            near_focus(0, 0, g_motor_focus_cfg.move_spd, 1);
        }
        else {
            isp_warn("focus_move_to_pos step=0!\n");
        }
    }
    else {
        isp_warn("focus_move_to_pos illegal(%d, %d, %d)\n", near_bound, inf_bound, g_motor_focus_cfg.curr_pos);
    }

    // restore standby peak pulse width
    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
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

    // set peak pulse width to 130%
    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_zoom_cfg.max_pulse_width);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | g_motor_zoom_cfg.max_pulse_width);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | g_motor_focus_cfg.max_pulse_width);
    }

    msleep(1);    // for separate 2 SPI command time distance

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
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = pos - g_motor_zoom_cfg.curr_pos;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += tele_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            tele_zoom(0, 0, g_motor_zoom_cfg.move_spd, 1);
        }
        else if (pos < g_motor_zoom_cfg.curr_pos) {
            // compensate backlash first
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_WIDE) {
                rest_step = g_motor_zoom_cfg.backlash_comp;

                do {
                    set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                    wide_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
                    rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                    g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                    g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                    msleep(g_motor_zoom_cfg.dly_time);
                } while (rest_step > 0);
            }

            rest_step = g_motor_zoom_cfg.curr_pos - pos;

            do {
                set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
                ret += wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 1);
                rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
                g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
                g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
                g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
                msleep(g_motor_zoom_cfg.dly_time);
            } while (rest_step > 0);

            ret = -ret;

            wide_zoom(0, 0, g_motor_zoom_cfg.move_spd, 1);
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

    // restore standby peak pulse width
    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
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

    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }

    rest_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].focus_max + INIT_FOCUS_POS + (INF_BOUND/10);

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
#else
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
#endif
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);

    near_focus(0, 0, g_motor_focus_cfg.move_spd, 0);

#if 1    // increase go home consistency
    // release motor excitation
    g_motor_focus_cfg.excite_en = 0x0000;

    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_STEP, 0);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_STEP, 0);
    }

    msleep(500);

    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | 0x5454);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | 0x5454);
    }

    // tighten motor and gear
    g_motor_focus_cfg.excite_en = 0x0400;

    rest_step = INF_BOUND / 20;

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        near_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
#else
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
#endif
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);

    near_focus(0, 0, g_motor_focus_cfg.move_spd, 0);

    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }
#endif

    if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
        rest_step = g_motor_focus_cfg.backlash_comp;

        do {
            set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
            inf_focus(0, set_step, g_motor_focus_cfg.move_spd, 0);
            rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
            g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
    #else
            g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
    #endif
            g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
            msleep(g_motor_focus_cfg.dly_time);
        } while (rest_step > 0);
    }

    rest_step = INIT_FOCUS_POS;

    do {
        set_step = (rest_step > g_motor_focus_cfg.max_step_size) ? g_motor_focus_cfg.max_step_size : rest_step;
        inf_focus(set_step, 0, g_motor_focus_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_focus_cfg.dly_time = (rest_step <= 0) ? 0 : FOCUS_PSUM_DELAY;
#else
        g_motor_focus_cfg.dly_time = FOCUS_PSUM_DELAY * set_step / g_motor_focus_cfg.max_step_size + 1;
#endif
        g_motor_focus_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_focus_cfg.dly_time);
    } while (rest_step > 0);

    inf_focus(0, 0, g_motor_focus_cfg.move_spd, 0);

    g_motor_focus_cfg.curr_pos = g_zoom_focus_tab[0].focus_min;

    g_motor_focus_cfg.move_to_pi = true;

    if (g_motor_focus_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_focus_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }

    up(&pdev->lock);

    return 1;
}

static s32 zoom_move_to_pi(void)
{
    s32 set_step, rest_step;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | GOHOME_ZPEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | GOHOME_ZPEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | GOHOME_FPEAK_PULSE_WIDTH);
    }

    rest_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos + INIT_ZOOM_POS + (TELE_BOUND/10);

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
#else
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
#endif
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);

    wide_zoom(0, 0, g_motor_zoom_cfg.move_spd, 0);

#if 1    // increase go home consistency
    // release motor excitation
    g_motor_zoom_cfg.excite_en = 0x0000;

    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_STEP, 0);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_STEP, 0);
    }

    msleep(500);

    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | 0x5454);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | 0x5454);
    }

    // tighten motor and gear
    g_motor_zoom_cfg.excite_en = 0x0400;

    rest_step = TELE_BOUND / 20;

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        wide_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
#else
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
#endif
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);

    wide_zoom(0, 0, g_motor_zoom_cfg.move_spd, 0);

    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | GOHOME_ZPEAK_PULSE_WIDTH);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | GOHOME_ZPEAK_PULSE_WIDTH);
    }
#endif

    if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_TELE) {
        rest_step = g_motor_zoom_cfg.backlash_comp;

        do {
            set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
            tele_zoom(0, set_step, g_motor_zoom_cfg.move_spd, 0);
            rest_step -= set_step;
    #ifdef FZ_PLS_PIN_EXIST
            g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
    #else
            g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
    #endif
            g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
            msleep(g_motor_zoom_cfg.dly_time);
        } while (rest_step > 0);
    }

    rest_step = INIT_ZOOM_POS;

    do {
        set_step = (rest_step > g_motor_zoom_cfg.max_step_size) ? g_motor_zoom_cfg.max_step_size : rest_step;
        tele_zoom(set_step, 0, g_motor_zoom_cfg.move_spd, 0);
        rest_step -= set_step;
#ifdef FZ_PLS_PIN_EXIST
        g_motor_zoom_cfg.dly_time = (rest_step <= 0) ? 0 : ZOOM_PSUM_DELAY;
#else
        g_motor_zoom_cfg.dly_time = ZOOM_PSUM_DELAY * set_step / g_motor_zoom_cfg.max_step_size + 1;
#endif
        g_motor_zoom_cfg.set_final_step = (rest_step <= 0) ? true : false;
        msleep(g_motor_zoom_cfg.dly_time);
    } while (rest_step > 0);

    tele_zoom(0, 0, g_motor_zoom_cfg.move_spd, 0);

    g_motor_zoom_cfg.curr_pos = g_zoom_focus_tab[0].zoom_pos;

    pdev->focus_low_bound = g_zoom_focus_tab[0].focus_min;
    pdev->focus_hi_bound  = g_zoom_focus_tab[0].focus_max;

    update_focus_pos();

    g_motor_zoom_cfg.move_to_pi = true;

    if (g_motor_zoom_cfg.chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }
    else if (g_motor_zoom_cfg.chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
        an41908_transmit_cmd(AN41908_CHNEL2, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_MAX_PULSE_WIDTH << 16) | STANDBY_PEAK_PULSE_WIDTH);
    }

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
    s32 ret;
    int wt_reg_value;

    if (ctl_state == 1) {
        g_ircut_state = 1;
    }
    else if (ctl_state == 2) {
        g_ircut_state = 2;
    }
    else {
        g_ircut_state = 0;
    }

    wt_reg_value = 0x04 + g_ircut_state;

    ret = an41908_transmit_cmd(MOTOR_FOCUS, AN41908_SET_MOTOR_CFG, (AN41908_IRCUT_CTL << 16) | wt_reg_value);

    return ret;
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
#ifdef FZ_PLS_PIN_EXIST
    struct gpio_interrupt_mode mode;
    struct virtual_dev dev;
#endif

    isp_info("SpiInit\n");

    // request GPIO
    ret = gpio_request(SPI_PANASONIC_CS_PIN, "SPI_CS");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_PANASONIC_CLK_PIN, "SPI_CLK");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_PANASONIC_RXD_PIN, "SPI_RXD");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_PANASONIC_TXD_PIN, "SPI_TXD");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(AN41908_VD_FZ_PIN, "FOCUS_ZOOM_VD");
    if (ret < 0) {
        return ret;
    }
#ifdef FZ_PLS_PIN_EXIST
    ret = gpio_request(MOT_FOCUS_PLS_PIN, "MOT_FOCUS_PLS_PIN");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(MOT_ZOOM_PLS_PIN, "MOT_ZOOM_PLS_PIN");
    if (ret < 0) {
        return ret;
    }
#endif

    if (AN41908_RESET_PIN >= 0) {
        ret = gpio_request(AN41908_RESET_PIN, "RESET");
        if (ret < 0) {
            return ret;
        }
    }

    // init GPIO
    gpio_direction_output(SPI_PANASONIC_CS_PIN, 0);
    gpio_direction_output(SPI_PANASONIC_CLK_PIN, 1);
    gpio_direction_input(SPI_PANASONIC_RXD_PIN);
    gpio_direction_output(SPI_PANASONIC_TXD_PIN, 1);
    gpio_direction_output(AN41908_VD_FZ_PIN, 0);
#ifdef FZ_PLS_PIN_EXIST
    gpio_direction_input(MOT_FOCUS_PLS_PIN);
    gpio_direction_input(MOT_ZOOM_PLS_PIN);

    mode.trigger_method = GPIO_INT_TRIGGER_EDGE;
    mode.trigger_edge_nr = GPIO_INT_SINGLE_EDGE;
    mode.trigger_rise_neg = GPIO_INT_FALLING;
    gpio_interrupt_setup(MOT_FOCUS_PLS_PIN, &mode);

    mode.trigger_method = GPIO_INT_TRIGGER_EDGE;
    mode.trigger_edge_nr = GPIO_INT_SINGLE_EDGE;
    mode.trigger_rise_neg = GPIO_INT_FALLING;
    gpio_interrupt_setup(MOT_ZOOM_PLS_PIN, &mode);

    if (request_irq(gpio_to_irq(MOT_FOCUS_PLS_PIN), an41908_gpio_isr, IRQF_SHARED, "gpio-pls1", &dev) < 0) {
        isp_err("%s fail: request_irq#1 NG!\n", __FUNCTION__);
        ret = -1;
    }
    if (request_irq(gpio_to_irq(MOT_ZOOM_PLS_PIN), an41908_gpio_isr, IRQF_SHARED, "gpio-pls2", &dev) < 0) {
        isp_err("%s fail: request_irq#2 NG!\n", __FUNCTION__);
        ret = -1;
    }

    gpio_interrupt_enable(MOT_FOCUS_PLS_PIN);
    gpio_interrupt_enable(MOT_ZOOM_PLS_PIN);
#endif

    if (AN41908_RESET_PIN >= 0) {
        // reset motor driver IC
        gpio_direction_output(AN41908_RESET_PIN, 1);
        gpio_set_value(AN41908_RESET_PIN, 0);
        msleep(2);
        gpio_set_value(AN41908_RESET_PIN, 1);
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
    gpio_free(SPI_PANASONIC_CS_PIN);
    gpio_free(SPI_PANASONIC_CLK_PIN);
    gpio_free(SPI_PANASONIC_RXD_PIN);
    gpio_free(SPI_PANASONIC_TXD_PIN);
    gpio_free(AN41908_VD_FZ_PIN);
#ifdef FZ_PLS_PIN_EXIST
    gpio_free(MOT_FOCUS_PLS_PIN);
    gpio_free(MOT_ZOOM_PLS_PIN);
#endif

    if (AN41908_RESET_PIN >= 0) {
        gpio_free(AN41908_RESET_PIN);
    }

    if (g_lens_pmu_fd != 0) {
        ftpmu010_deregister_reg(g_lens_pmu_fd);
        g_lens_pmu_fd = 0;
    }

    return 0;
}

// following table is based on 27MHz input to AN41908
const int PWMFreqTbl[][12] = {{ 27200,  30100,  40200,  54400,  64900,  76700, 108900, 129800, 160700, 210900, 306800, 562500},    // unit: Hz
                              {0x5F00, 0x5C00, 0x5500, 0x3F00, 0x3A00, 0x3600, 0x1F00, 0x1A00, 0x1500, 0x1000, 0x0B00, 0x0600}};

#define MAX_PWM_TAB_SIZE    sizeof(PWMFreqTbl)/sizeof(int)/2

static int an41908_global_freq(int freq)
{
    int i, output_freq;

    output_freq = CLAMP(freq, PWMFreqTbl[0][0], PWMFreqTbl[0][MAX_PWM_TAB_SIZE-1]);    // unit: Hz

    for (i = 0; i < MAX_PWM_TAB_SIZE; i++) {
        if (output_freq <= PWMFreqTbl[0][i]) {
            break;
        }
    }

    i = CLAMP(i, 0, MAX_PWM_TAB_SIZE-1);

    return PWMFreqTbl[1][i];
}

static int an41908_speed2freq(AN41908_MOTOR_SPEED speed)
{
    int ret_reg, rotate_freq;

    rotate_freq = (g_motor_focus_cfg.pwm_freq + 240/2) / 240;    // 240 is our max pulse number

    switch (speed) {
        case AN41908_MOTOR_SPD_4X:
            rotate_freq *= 4;
            break;

        case AN41908_MOTOR_SPD_2X:
            rotate_freq *= 2;
            break;

        case AN41908_MOTOR_SPD_1X:
        default:
            rotate_freq *= 1;
            break;

        case AN41908_MOTOR_SPD_HALF:
            rotate_freq /= 2;
            break;

        case AN41908_MOTOR_SPD_QUARTER:
            rotate_freq /= 4;
            break;
    }

    ret_reg = (27 * 1000000 / rotate_freq + 240 * 24 / 2) / (240 * 24);

    return ret_reg;
}

int an41908_motor_init(MOTOR_SEL motor_sel)
{
    u16 rd_reg_value, reg_tmp;
    int wt_reg_value;
    motor_config_t *motor_cfg;

    motor_cfg = (motor_sel == MOTOR_FOCUS) ? &g_motor_focus_cfg : ((motor_sel == MOTOR_ZOOM) ? &g_motor_zoom_cfg : NULL);

    if ((motor_sel >= MOTOR_IRIS) || (motor_cfg == NULL)) {
        return -1;
    }

    // update global PWM frequency
    wt_reg_value = an41908_global_freq(10*motor_cfg->pwm_freq);
    reg_tmp = (wt_reg_value >> 8) & 0x001F;    // save out PWMMOD value

    if ((rd_reg_value = an41908_receive_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_SPD, 0)) == 0xFFFF) {
        return -1;
    }
    wt_reg_value |= (rd_reg_value & 0x00FF);
    an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_SPD, wt_reg_value);

    motor_cfg->max_pulse_width = 13 * 8 * reg_tmp / 10;    // set 130% peak pulse width
    motor_cfg->max_pulse_width = CLAMP(motor_cfg->max_pulse_width, 0x10, 0xFF);
    motor_cfg->max_pulse_width |= ((motor_cfg->max_pulse_width << 8) & 0xFF00);

    // update excitation mode
    if (motor_cfg->mode == AN41908_MODE_64MACRO) {
        wt_reg_value = 0x3000;
    }
    else if (motor_cfg->mode == AN41908_MODE_128MACRO) {
        wt_reg_value = 0x2000;
    }
    else if (motor_cfg->mode == AN41908_MODE_256MACRO) {
        wt_reg_value = 0x1000;
    }
    else {
        wt_reg_value = 0x1000;
    }

    if ((rd_reg_value = an41908_receive_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_STEP, 0)) == 0xFFFF) {
        return -1;
    }
    wt_reg_value |= (rd_reg_value & 0xCF00);
    an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_STEP, wt_reg_value);

    // update respective PWM frequency for achieving desired moving speed
    wt_reg_value = an41908_speed2freq(motor_cfg->move_spd) & 0xFFFF;

    if (motor_cfg->chnel == AN41908_CHNEL1) {
        an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR1_STEP_CYCLE << 16) | wt_reg_value);
    }
    else if (motor_cfg->chnel == AN41908_CHNEL2) {
        an41908_transmit_cmd(motor_cfg->chnel, AN41908_SET_MOTOR_CFG, (AN41908_MOTOR2_STEP_CYCLE << 16) | wt_reg_value);
    }

    return 0;
}

int lens_an41908_construct(void)
{
    int i, ret = 0;
    lens_dev_t *pdev = g_plens;

    for (i = 0; i < MAX_REG_TAB_SIZE; i++) {
        an41908_transmit_cmd(AN41908_CHNEL1, AN41908_SET_MOTOR_CFG, g_reg_init_tab[i]);
    }

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
        ret |= an41908_motor_init(MOTOR_FOCUS);
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        pdev->capability        |= LENS_SUPPORT_ZOOM;
        pdev->fn_zoom_move_to_pi = zoom_move_to_pi;
        pdev->fn_zoom_get_pos    = get_zoom_pos;
        pdev->fn_zoom_set_pos    = zoom_move_to_pos;
        pdev->fn_zoom_move       = zoom_move;
        ret |= an41908_motor_init(MOTOR_ZOOM);
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

void lens_an41908_deconstruct(void)
{
    isp_info("Lens driver exit\n");

    if (lens_driver_support & LENS_SUPPORT_FOCUS) {
        ;    // do nothing currently
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        ;    // do nothing currently
    }

    SpiExit();
}

//============================================================================
// module initialization / finalization
//============================================================================
static int __init lens_an41908_init(void)
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
    if (lens_an41908_construct() < 0) {
        ret = -ENODEV;
        isp_err("[ERROR]lens_an41908_construct() fail!\n");

        kfree(g_plens);
        g_plens = NULL;
    }

    isp_register_lens(g_plens);

    return ret;
}

static void __exit lens_an41908_exit(void)
{
    // remove lens
    lens_an41908_deconstruct();

    isp_unregister_lens(g_plens);

    if (g_plens != NULL) {
        kfree(g_plens);
        g_plens = NULL;
    }
}

module_init(lens_an41908_init);
module_exit(lens_an41908_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("LENS_AN41908");
MODULE_LICENSE("GPL");

