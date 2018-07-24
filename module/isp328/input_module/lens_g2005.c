/**
 * @file lens_g2005.c
 *  driver of G2005 motor driver
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
#include "lens_g2005/lens_g2005_fuho1.h"


static u8 lens_driver_support = 3;    // bit0: focus, bit1: zoom, bit2: iris
module_param(lens_driver_support, byte, S_IRUGO);
MODULE_PARM_DESC(lens_driver_support, "lens driver support");

static int lens_pwm_ch = 7;    // 0~7
module_param(lens_pwm_ch, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lens_pwm_ch, "lens pwm ch sel");


static lens_dev_t *g_plens = NULL;
static DEFINE_SPINLOCK(lock_ctl);


static void g2005_write_spi_cmd(const spi_data_fmt_t send_data)
{
    int i;
    unsigned long flags;

    spin_lock_irqsave(&lock_ctl, flags);

    // set CS high => send data => set CS low
    gpio_set_value(SPI_GMT_CS_PIN, 1);
    TDELAY(1);

    for (i = 1; i >= 0; i--) {
        gpio_set_value(SPI_GMT_CLK_PIN, 0);

        if ((send_data.addr >> i) & 0x01) {
            gpio_set_value(SPI_GMT_DAT_PIN, 1);
        }
        else {
            gpio_set_value(SPI_GMT_DAT_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_GMT_CLK_PIN, 1);
        TDELAY(1);
    }

    for (i = 13; i >= 0; i--) {
        gpio_set_value(SPI_GMT_CLK_PIN, 0);

        if ((send_data.data >> i) & 0x01) {
            gpio_set_value(SPI_GMT_DAT_PIN, 1);
        }
        else {
            gpio_set_value(SPI_GMT_DAT_PIN, 0);
        }

        TDELAY(1);
        gpio_set_value(SPI_GMT_CLK_PIN, 1);
        TDELAY(1);
    }

    gpio_set_value(SPI_GMT_CS_PIN, 0);
    TDELAY(1);
    gpio_set_value(SPI_GMT_DAT_PIN, 1);
    TDELAY(1);
    gpio_set_value(SPI_GMT_CLK_PIN, 0);
    TDELAY(2);
    gpio_set_value(SPI_GMT_CLK_PIN, 1);
    TDELAY(2);

    spin_unlock_irqrestore(&lock_ctl, flags);
}

static void g2005_transmit_cmd(u16 cmd_arg)
{
    spi_data_fmt_t spi_ctl;

    spi_ctl.addr = (cmd_arg >> 14) & 0x0003;
    spi_ctl.data = cmd_arg & 0x3FFF;

    g2005_write_spi_cmd(spi_ctl);
}

static irqreturn_t generate_pwm(int irq, void *dev_id)
{
    int tmp, stage_idx, tbl_idx;
    static int stable_start_cnt = 0, stable_end_cnt = 0;
    unsigned long flags;
    motor_config_t *motor_cfg;

    motor_cfg = (g_motor_focus_cfg.residure_step != 0) ? &g_motor_focus_cfg : (g_motor_zoom_cfg.residure_step != 0 ? &g_motor_zoom_cfg : NULL);

    spin_lock_irqsave(&lock_ctl, flags);

    pwm_clr_intrpt(lens_pwm_ch);    // clear interrupt flag

    // skip a period of time from power off -> power on
    if (++stable_start_cnt <= MOTOR_START_WAIT) {
        stable_end_cnt = 0;

        spin_unlock_irqrestore(&lock_ctl, flags);

        return IRQ_HANDLED;
    }

    if (motor_cfg != NULL) {
        tmp = (motor_cfg->residure_step >= 0) ? motor_cfg->residure_step : -motor_cfg->residure_step;

        if (tmp > 1) {
            if (motor_cfg->residure_step > 0) {
                if (motor_cfg->idx_ofst >= 256) {    // positive direction originally
                    stage_idx = motor_cfg->idx_ofst - 256;
                }
                else {    // negative direction originally
                    stage_idx = FwdRvsConvert[motor_cfg->idx_ofst];
                }
            }
            else {
                if (motor_cfg->idx_ofst >= 256) {    // positive direction originally
                    stage_idx = FwdRvsConvert[motor_cfg->idx_ofst-256];
                }
                else {    // negative direction originally
                    stage_idx = motor_cfg->idx_ofst;
                }
            }

            stage_idx += motor_cfg->idx_acc;
            stage_idx = stage_idx % 4;

            motor_cfg->idx_acc++;

            if (motor_cfg->def_pdir == 1) {
                if (motor_cfg->residure_step > 0) {
                    tbl_idx = RotateReverse[stage_idx];
                    gpio_set_value(G2005_IN1A_PIN, IN1A1B2ATbl[tbl_idx][0]);
                    gpio_set_value(G2005_IN1B_PIN, IN1A1B2ATbl[tbl_idx][1]);
                    ///gpio_set_value(G2005_IN2A_PIN, IN1A1B2ATbl[tbl_idx][2]);

                    motor_cfg->residure_step--;
                }
                else {    // motor_cfg->residure_step < 0
                    tbl_idx = RotateForward[stage_idx];
                    gpio_set_value(G2005_IN1A_PIN, IN1A1B2ATbl[tbl_idx][0]);
                    gpio_set_value(G2005_IN1B_PIN, IN1A1B2ATbl[tbl_idx][1]);
                    ///gpio_set_value(G2005_IN2A_PIN, IN1A1B2ATbl[tbl_idx][2]);

                    motor_cfg->residure_step++;
                }
            }
            else {
                if (motor_cfg->residure_step > 0) {
                    tbl_idx = RotateForward[stage_idx];
                    gpio_set_value(G2005_IN1A_PIN, IN1A1B2ATbl[tbl_idx][0]);
                    gpio_set_value(G2005_IN1B_PIN, IN1A1B2ATbl[tbl_idx][1]);
                    ///gpio_set_value(G2005_IN2A_PIN, IN1A1B2ATbl[tbl_idx][2]);

                    motor_cfg->residure_step--;
                }
                else {    // motor_cfg->residure_step < 0
                    tbl_idx = RotateReverse[stage_idx];
                    gpio_set_value(G2005_IN1A_PIN, IN1A1B2ATbl[tbl_idx][0]);
                    gpio_set_value(G2005_IN1B_PIN, IN1A1B2ATbl[tbl_idx][1]);
                    ///gpio_set_value(G2005_IN2A_PIN, IN1A1B2ATbl[tbl_idx][2]);

                    motor_cfg->residure_step++;
                }
            }

            if ((motor_cfg->residure_step == 1) || (motor_cfg->residure_step == -1)) {
                motor_cfg->ctl_pin_state = (IN1A1B2ATbl[tbl_idx][1] << 8) + IN1A1B2ATbl[tbl_idx][0];
                motor_cfg->idx_ofst = (motor_cfg->residure_step == 1) ? (256 + ((stage_idx + 1) % 4)) : (0 + ((stage_idx + 1) % 4));
            }
        }
        else {
            if (++stable_end_cnt > MOTOR_END_WAIT) {
                pwm_interrupt_disable(lens_pwm_ch);
                motor_cfg->residure_step = 0;
                g2005_transmit_cmd(G2005_SERIAL_CTL_INPUT + 0x0006);    // power down
                motor_cfg->work_status = MOTOR_STATUS_FREE;
                stable_start_cnt = 0;
            }
        }
    }
    else {
        pwm_interrupt_disable(lens_pwm_ch);
        g2005_transmit_cmd(G2005_SERIAL_CTL_INPUT + 0x0006);        // power down
        g_motor_focus_cfg.work_status = MOTOR_STATUS_FREE;
        g_motor_zoom_cfg.work_status  = MOTOR_STATUS_FREE;
    }

    spin_unlock_irqrestore(&lock_ctl, flags);

    return IRQ_HANDLED;
}

static int g2005_pwm_init(void)
{
    // set the IRQ type
    irq_set_irq_type(49, IRQ_TYPE_LEVEL_HIGH);

    // set ISR
    if (request_irq(49, generate_pwm, IRQF_DISABLED, "PWM", NULL) < 0) {
        isp_err("%s: request_irq 49 failed\n", __FUNCTION__);
        return -1;
    }

    // request PWM
    if (pwm_dev_request(lens_pwm_ch) == 0) {
        isp_err("Fail to request PWM%u!\n", lens_pwm_ch);
        return -1;
    }

    pwm_tmr_stop(lens_pwm_ch);
    pwm_clksrc_switch(lens_pwm_ch, 0);
    pwm_set_freq(lens_pwm_ch, (u32)g_motor_focus_cfg.move_spd);
    pwm_set_duty_steps(lens_pwm_ch, 100);
    pwm_set_duty_ratio(lens_pwm_ch, 100/2);
    pwm_mode_change(lens_pwm_ch, 1);    // 0: ONE_SHOT / 1: PWM_INTERVAL / 2: PWM_REPEAT
    pwm_update(lens_pwm_ch);    // need update
    pwm_tmr_start(lens_pwm_ch);

    return 0;
}

static void set_motor_pos(const MOTOR_SEL chnel, const s32 step_pos, const G2005_MOTOR_SPEED move_speed)
{
    motor_config_t *motor_cfg;

    motor_cfg = (chnel == MOTOR_FOCUS) ? &g_motor_focus_cfg : ((chnel == MOTOR_ZOOM) ? &g_motor_zoom_cfg : NULL);

    if ((motor_cfg != NULL) && (step_pos != 0)) {
        motor_cfg->work_status = MOTOR_STATUS_BUSY;

        // restore original pin state
        gpio_set_value(G2005_IN1A_PIN, motor_cfg->ctl_pin_state & 0x01);
        gpio_set_value(G2005_IN1B_PIN, (motor_cfg->ctl_pin_state >> 8) & 0x01);

        if (chnel == MOTOR_FOCUS) {
            if (motor_cfg->chnel == G2005_CHNEL1) {
                g2005_transmit_cmd(0x3003);
            }
            else {
                g2005_transmit_cmd(0x3000);
            }
        }
        else {
            if (motor_cfg->chnel == G2005_CHNEL2) {
                g2005_transmit_cmd(0x3000);
            }
            else {
                g2005_transmit_cmd(0x3003);
            }
        }

        g2005_transmit_cmd(G2005_SERIAL_CTL_INPUT + 0x000E);    // power on

        motor_cfg->idx_acc = 0;
        motor_cfg->residure_step = (step_pos > 0) ? (step_pos + 1) : (step_pos - 1);

        pwm_clr_intrpt(lens_pwm_ch);    // clear interrupt flag
        pwm_set_freq(lens_pwm_ch, (u32)move_speed);
        pwm_update(lens_pwm_ch);        // need update
        pwm_interrupt_enable(lens_pwm_ch);
    }
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

static int wait_move_end(const int timeout_ms)
{
    int cnt = 0;

    while ((g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE) || (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE)) {
        if (cnt++ >= timeout_ms) {
            if (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE) {
                isp_err("wait_move_end timeout focus!\n");
            }
            else {
                isp_err("wait_move_end timeout zoom!\n");
            }
            return -1;
        }
        msleep(1);
    }

    return 0;
}

//-------------------------------------------------------------------------
// FOCUS CONTROL
//-------------------------------------------------------------------------
static s32 near_focus(int steps, const G2005_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos - steps) < near_bound)) {
            steps = g_motor_focus_cfg.curr_pos - near_bound;
        }

        if (steps != 0) {
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_NEAR) {
                set_step = steps + g_motor_focus_cfg.backlash_comp;
                g_motor_focus_cfg.move_dir = FOCUS_DIR_NEAR;
            }
            else {
                set_step = steps;
            }

            set_motor_pos(MOTOR_FOCUS, -set_step, focus_speed);

            g_motor_focus_cfg.curr_pos -= steps;
        }
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 inf_focus(int steps, const G2005_MOTOR_SPEED focus_speed, const int check_bound)
{
    int near_bound, inf_bound, set_step;

    calc_focus_range(&near_bound, &inf_bound);

    if (wait_move_end(200) == 0) {
        if ((check_bound != 0) && ((g_motor_focus_cfg.curr_pos + steps) > inf_bound)) {
            steps = inf_bound - g_motor_focus_cfg.curr_pos;
        }

        if (steps != 0) {
            if (g_motor_focus_cfg.move_dir != FOCUS_DIR_INF) {
                set_step = steps + g_motor_focus_cfg.backlash_comp;
                g_motor_focus_cfg.move_dir = FOCUS_DIR_INF;
            }
            else {
                set_step = steps;
            }

            set_motor_pos(MOTOR_FOCUS, set_step, focus_speed);

            g_motor_focus_cfg.curr_pos += steps;
        }
    }
    else {
        steps = 0;
    }

    return steps;
}

//-------------------------------------------------------------------------
// ZOOM CONTROL
//-------------------------------------------------------------------------
static s32 wide_zoom(int steps, const G2005_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, wide_bound = g_zoom_focus_tab[0].zoom_pos;

    if (wait_move_end(200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos - steps) < wide_bound)) {
            steps = g_motor_zoom_cfg.curr_pos - wide_bound;
        }

        if (steps != 0) {
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_WIDE) {
                set_step = steps + g_motor_zoom_cfg.backlash_comp;
                g_motor_zoom_cfg.move_dir = ZOOM_DIR_WIDE;
            }
            else {
                set_step = steps;
            }

            set_motor_pos(MOTOR_ZOOM, -set_step, zoom_speed);

            g_motor_zoom_cfg.curr_pos -= steps;
        }
    }
    else {
        steps = 0;
    }

    return steps;
}

static s32 tele_zoom(int steps, const G2005_MOTOR_SPEED zoom_speed, const int check_bound)
{
    int set_step, tele_bound = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos;

    if (wait_move_end(200) == 0) {
        if ((check_bound != 0) && ((g_motor_zoom_cfg.curr_pos + steps) > tele_bound)) {
            steps = tele_bound - g_motor_zoom_cfg.curr_pos;
        }

        if (steps != 0) {
            if (g_motor_zoom_cfg.move_dir != ZOOM_DIR_TELE) {
                set_step = steps + g_motor_zoom_cfg.backlash_comp;
                g_motor_zoom_cfg.move_dir = ZOOM_DIR_TELE;
            }
            else {
                set_step = steps;
            }

            set_motor_pos(MOTOR_ZOOM, set_step, zoom_speed);

            g_motor_zoom_cfg.curr_pos += steps;
        }
    }
    else {
        steps = 0;
    }

    return steps;
}

//-------------------------------------------------------------------------
static void update_focus_pos(void)
{
    s32 set_step;
    int focus_pos, near_bound, inf_bound;

    calc_focus_range(&near_bound, &inf_bound);

    if ((g_motor_focus_cfg.curr_pos <= near_bound) || (g_motor_focus_cfg.curr_pos >= inf_bound)) {
        focus_pos = (near_bound + inf_bound) / 2;

        if (focus_pos != g_motor_focus_cfg.curr_pos) {
            if (focus_pos > g_motor_focus_cfg.curr_pos) {
                set_step = focus_pos - g_motor_focus_cfg.curr_pos;

                inf_focus(set_step, g_motor_focus_cfg.move_spd, 0);

                do {
                    msleep(1);
                } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);
            }
            else if (focus_pos < g_motor_focus_cfg.curr_pos) {
                set_step = g_motor_focus_cfg.curr_pos - focus_pos;

                near_focus(set_step, g_motor_focus_cfg.move_spd, 0);

                do {
                    msleep(1);
                } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);
            }
            else {
                ;    // impossible condition
            }
        }
    }
}

static s32 focus_move(s32 steps)
{
    s32 ret = 0, set_step;
    int near_bound, inf_bound;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    calc_focus_range(&near_bound, &inf_bound);

    if (steps > 0) {
        if ((g_motor_focus_cfg.curr_pos + steps) <= inf_bound) {
            set_step = steps;

            ret = inf_focus(set_step, g_motor_focus_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);
        }
        else {
            isp_warn("focus_move reach upper bound!\n");
        }
    }
    else if (steps < 0) {
        if ((g_motor_focus_cfg.curr_pos + steps) >= near_bound) {
            set_step = -steps;

            ret = near_focus(set_step, g_motor_focus_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);

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
    s32 ret = 0, set_step;
    int near_bound, inf_bound;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    steps = (steps * (g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos - g_zoom_focus_tab[0].zoom_pos) + 50) / 100;

    if (steps > 0) {
        if ((g_motor_zoom_cfg.curr_pos + steps) <= g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos) {
            set_step = steps;

            ret = tele_zoom(set_step, g_motor_zoom_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);
        }
        else {
            isp_warn("zoom_move reach upper bound!\n");
        }
    }
    else if (steps < 0) {
        if ((g_motor_zoom_cfg.curr_pos + steps) >= g_zoom_focus_tab[0].zoom_pos) {
            set_step = -steps;

            ret = wide_zoom(set_step, g_motor_zoom_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);

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
    s32 ret = 0, near_bound, inf_bound, set_step;
    lens_dev_t *pdev = g_plens;

    if (g_motor_focus_cfg.move_to_pi == false) {
        return 0;
    }

    down(&pdev->lock);

    calc_focus_range(&near_bound, &inf_bound);

    if ((pos >= near_bound) && (pos <= inf_bound)) {
        if (pos > g_motor_focus_cfg.curr_pos) {
            set_step = pos - g_motor_focus_cfg.curr_pos;

            ret = inf_focus(set_step, g_motor_focus_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);
        }
        else if (pos < g_motor_focus_cfg.curr_pos) {
            set_step = g_motor_focus_cfg.curr_pos - pos;

            ret = near_focus(set_step, g_motor_focus_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);

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
    s32 ret = 0, zoom_pos_min, zoom_pos_max, set_step;
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
            set_step = pos - g_motor_zoom_cfg.curr_pos;

            ret = tele_zoom(set_step, g_motor_zoom_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);
        }
        else if (pos < g_motor_zoom_cfg.curr_pos) {
            set_step = g_motor_zoom_cfg.curr_pos - pos;

            ret = wide_zoom(set_step, g_motor_zoom_cfg.move_spd, 1);

            do {
                msleep(1);
            } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);

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
    s32 set_step;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    set_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].focus_max + INIT_FOCUS_POS + (INF_BOUND/10);

    near_focus(set_step, G2005_MOTOR_SPD_1X, 0);

    do {
        msleep(1);
    } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);

    msleep(500);    // increase go home consistency

    set_step = INIT_FOCUS_POS;

    inf_focus(set_step, g_motor_focus_cfg.move_spd, 0);

    do {
        msleep(1);
    } while (g_motor_focus_cfg.work_status != MOTOR_STATUS_FREE);

    g_motor_focus_cfg.curr_pos = g_zoom_focus_tab[0].focus_min;

    g_motor_focus_cfg.move_to_pi = true;

    up(&pdev->lock);

    return 1;
}

static s32 zoom_move_to_pi(void)
{
    s32 set_step;
    lens_dev_t *pdev = g_plens;

    down(&pdev->lock);

    set_step = g_zoom_focus_tab[MAX_ZF_TAB_SIZE-1].zoom_pos + INIT_ZOOM_POS + (TELE_BOUND/10);

    wide_zoom(set_step, G2005_MOTOR_SPD_1X, 0);

    do {
        msleep(1);
    } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);

    msleep(500);    // increase go home consistency

    set_step = INIT_ZOOM_POS;

    tele_zoom(set_step, g_motor_zoom_cfg.move_spd, 0);

    do {
        msleep(1);
    } while (g_motor_zoom_cfg.work_status != MOTOR_STATUS_FREE);

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
    ret = gpio_request(SPI_GMT_CS_PIN, "SPI_CS");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_GMT_DAT_PIN, "SPI_DAT");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(SPI_GMT_CLK_PIN, "SPI_CLK");
    if (ret < 0) {
        return ret;
    }

    if (G2005_RESET_PIN >= 0) {
        ret = gpio_request(G2005_RESET_PIN, "RESET");
        if (ret < 0) {
            return ret;
        }
    }

    ret = gpio_request(G2005_IN1A_PIN, "IN1A");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(G2005_IN1B_PIN, "IN1B");
    if (ret < 0) {
        return ret;
    }
    ret = gpio_request(G2005_IN2A_PIN, "IN2A");
    if (ret < 0) {
        return ret;
    }

    // init GPIO
    gpio_direction_output(SPI_GMT_CS_PIN, 0);
    gpio_direction_output(SPI_GMT_DAT_PIN, 1);
    gpio_direction_output(SPI_GMT_CLK_PIN, 1);

    if (G2005_RESET_PIN >= 0) {
        // reset motor driver IC
        gpio_direction_output(G2005_RESET_PIN, 1);
        gpio_set_value(G2005_RESET_PIN, 0);
        msleep(2);
        gpio_set_value(G2005_RESET_PIN, 1);
    }

    gpio_direction_output(G2005_IN1A_PIN, 0);
    gpio_direction_output(G2005_IN1B_PIN, 0);
    gpio_direction_output(G2005_IN2A_PIN, 0);

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
    gpio_free(SPI_GMT_CS_PIN);
    gpio_free(SPI_GMT_DAT_PIN);
    gpio_free(SPI_GMT_CLK_PIN);

    if (G2005_RESET_PIN >= 0) {
        gpio_free(G2005_RESET_PIN);
    }

    gpio_free(G2005_IN1A_PIN);
    gpio_free(G2005_IN1B_PIN);
    gpio_free(G2005_IN2A_PIN);

    if (g_lens_pmu_fd != 0) {
        ftpmu010_deregister_reg(g_lens_pmu_fd);
        g_lens_pmu_fd = 0;
    }

    return 0;
}

int lens_g2005_construct(void)
{
    int i, ret = 0;
    lens_dev_t *pdev = g_plens;

    for (i = 0; i < MAX_REG_TAB_SIZE; i++) {
        g2005_transmit_cmd(g_reg_init_tab[i]);
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
    }
    if (lens_driver_support & LENS_SUPPORT_ZOOM) {
        pdev->capability        |= LENS_SUPPORT_ZOOM;
        pdev->fn_zoom_move_to_pi = zoom_move_to_pi;
        pdev->fn_zoom_get_pos    = get_zoom_pos;
        pdev->fn_zoom_set_pos    = zoom_move_to_pos;
        pdev->fn_zoom_move       = zoom_move;
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

    ret = g2005_pwm_init();

    sema_init(&pdev->lock, 1);

    return ret;
}

void lens_g2005_deconstruct(void)
{
    isp_info("Lens driver exit\n");

    pwm_tmr_stop(lens_pwm_ch);
    pwm_dev_release(lens_pwm_ch);

    free_irq(49, NULL);

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
static int __init lens_g2005_init(void)
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
    if (lens_g2005_construct() < 0) {
        ret = -ENODEV;
        isp_err("[ERROR]lens_g2005_construct() fail!\n");

        kfree(g_plens);
        g_plens = NULL;
    }

    isp_register_lens(g_plens);

    return ret;
}

static void __exit lens_g2005_exit(void)
{
    // remove lens
    lens_g2005_deconstruct();

    isp_unregister_lens(g_plens);

    if (g_plens != NULL) {
        kfree(g_plens);
        g_plens = NULL;
    }
}

module_init(lens_g2005_init);
module_exit(lens_g2005_exit);

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("LENS_GMT_G2005");
MODULE_LICENSE("GPL");

