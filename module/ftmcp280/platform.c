#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <mach/platform/platform_io.h>
#include <mach/ftpmu010.h>

#include "enc_driver/define.h"

#define USE_LOCAL_GATING_CLOCK_SETTING

#ifdef SUPPORT_PWM
    extern unsigned int pwm;
#endif

#ifdef USE_LOCAL_GATING_CLOCK_SETTING
#ifdef GM8210
    static pmuReg_t regH264EArray[] = {
        /* reg_off  bit_masks    lock_bits    init_val     init_mask */
        {0x28,      (0x1 << 31), (0x1 << 31), (0x0 << 31), (0x0 << 31)},
        {0xA0,      (0x3 << 1),  (0x3 << 1),  (0x0 << 1),  (0x0 << 1)},
        {0xB0,      (0x1 << 25)|(0x1 << 3), (0x1 << 25)|(0x1 << 3), (0x0 << 25)|(0x0 << 3), (0x0 << 25)|(0x0 << 3)},
        {0xB4,      (0x1 << 30)|(0x1 << 8), (0x1 << 30)|(0x1 << 8), (0x0 << 30)|(0x0 << 8), (0x0 << 30)|(0x0 << 8)},
    };
#elif defined(GM8287)
    static pmuReg_t regH264EArray[] = {
        /* reg_off  bit_masks    lock_bits    init_val     init_mask */
        {0x28,      (0x1 << 31), (0x1 << 31), (0x0 << 31), (0x0 << 31)},
        {0xA0,      (0x1 << 1),  (0x1 << 1),  (0x0 << 1),  (0x0 << 1) },
        {0xB0,      (0x1 << 25), (0x1 << 25), (0x0 << 25), (0x0 << 25)},
        {0xB4,      (0x1 << 30), (0x1 << 30), (0x0 << 30), (0x0 << 30)},
    };
#elif defined(GM8139)
    static pmuReg_t regH264EArray[] = {
        /* reg_off  bit_masks    lock_bits    init_val     init_mask */
        //{0x8c,      0x7,         0x7,         0x0,         0x0        },
        {0x8c,      0xFFFF0007,  0xFFFF0007,  0x0,         0x0        },
        {0xA0,      (0x1 << 1),  (0x1 << 1),  (0x0 << 1),  (0x0 << 1) },
        {0xB0,      (0x1 << 13), (0x1 << 13), (0x0 << 13), (0x0 << 13)},
        {0xB4,      (0x1 << 19), (0x1 << 19), (0x0 << 19), (0x0 << 19)},
    };
#elif defined(GM8136)
    static pmuReg_t regH264EArray[] = {
        /* reg_off  bit_masks    lock_bits    init_val     init_mask */
        //{0x38,      0x7,         0x7,         0x0,         0x0        },        
        {0x8c,      0xFFFF0007,  0xFFFF0007,  0x0,         0x0        },
        {0xA0,      (0x1 << 1),  (0x1 << 1),  (0x0 << 1),  (0x0 << 1) },
        {0xB0,      (0x1 << 13), (0x1 << 13), (0x0 << 13), (0x0 << 13)},
        {0xB4,      (0x1 << 19), (0x1 << 19), (0x0 << 19), (0x0 << 19)},
    };
#endif
static pmuRegInfo_t h264e_clk_info = {
    "H264E_CLK",
    ARRAY_SIZE(regH264EArray),
    ATTR_TYPE_PLL4,
    regH264EArray
};
static int ftpmu010_h264e_fd = -1;    //pmu
#else
extern int ftpmu010_h264e_fd;
#endif

#ifdef USE_LOCAL_GATING_CLOCK_SETTING
int pf_mcp280_clk_init(void)
{
    ftpmu010_h264e_fd = ftpmu010_register_reg(&h264e_clk_info);
    if (unlikely(ftpmu010_h264e_fd < 0)){
        //printk("H264 Enc registers to PMU fail! \n");
        return -1;
    }
        
    return 0;
}

int pf_mcp280_clk_exit(void)
{
    ftpmu010_deregister_reg(ftpmu010_h264e_fd);
    ftpmu010_h264e_fd = -1;
    //printk("H264 enc pmu deregister\n");
    return 0;
}
#endif

int pf_mcp280_clk_on(void)
{
    if (ftpmu010_h264e_fd < 0) {
    #ifdef USE_LOCAL_GATING_CLOCK_SETTING
        if (pf_mcp280_clk_init() < 0)
    #endif
            return -1;
    }
#ifdef GM8210
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0x03<<1, 0x03<<1);
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x0<<25)|(0x0<<3), (0x1<<25)|(0x1<<3));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x0<<30)|(0x0<<8), (0x1<<30)|(0x1<<8));
#elif defined(GM8287)
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0x01<<1, 0x01<<1);
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x0<<25), (0x1<<25));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x0<<30), (0x1<<30));
#elif defined(GM8139)
    #ifdef SUPPORT_PWM
    if (pwm&0xFFFF)
        ftpmu010_write_reg(ftpmu010_h264e_fd, 0x8c, ((pwm&0xFFFF)<<16)|0x03, 0xFFFF0007);
    else
        ftpmu010_write_reg(ftpmu010_h264e_fd, 0x8c, 0x01,      0xFFFF0007);
    #endif
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0x01<<1,   0x01<<1);
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x0<<13), (0x1<<13));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x0<<19), (0x1<<19));
#elif defined(GM8136)
    //ftpmu010_write_reg(ftpmu010_h264e_fd, 0x38, 0x01, 0x07);    // axic2
    #ifdef SUPPORT_PWM
    if (pwm&0xFFFF)
        ftpmu010_write_reg(ftpmu010_h264e_fd, 0x8c, ((pwm&0xFFFF)<<16)|0x03, 0xFFFF0007);
    else
        ftpmu010_write_reg(ftpmu010_h264e_fd, 0x8c, 0x01,      0xFFFF0007);
    #endif
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0x01<<1, 0x01<<1);
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x0<<13), (0x1<<13));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x0<<19), (0x1<<19));
#endif
    return 0;
}

int pf_mcp280_clk_off(void)
{
    if (ftpmu010_h264e_fd < 0)
        return 0;
#ifdef GM8210
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x1<<25)|(0x1<<3), (0x1<<25)|(0x1<<3));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x1<<30)|(0x1<<8), (0x1<<30)|(0x1<<8));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0, 0x03<<1);
#elif defined(GM8287)
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x1<<25), (0x1<<25));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x1<<30), (0x1<<30));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0, 0x01<<1);
#elif defined(GM8139)
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x1<<13), (0x1<<13));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x1<<19), (0x1<<19));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0, 0x01<<1);
#elif defined(GM8136)
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB0, (0x1<<13), (0x1<<13));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xB4, (0x1<<19), (0x1<<19));
    ftpmu010_write_reg(ftpmu010_h264e_fd, 0xA0, 0, 0x01<<1);
#endif

    return 0;
}

