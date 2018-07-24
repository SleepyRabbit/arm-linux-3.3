#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <mach/platform/platform_io.h>
#include <mach/ftpmu010.h>

static int pmonitor_fd;

static pmuReg_t pmu_reg[] = {
    /* offset, bits_mask, lock_bits, init_val, init_mask */
    {0xB8,      0x1 << 15, 0x1 << 15, 0x0,      0x1 << 15},
};

static pmuRegInfo_t pmu_reg_info = {
    "pMonitor",
    ARRAY_SIZE(pmu_reg),
    ATTR_TYPE_NONE,
    &pmu_reg[0]
};

void platform_init(void)
{
    pmonitor_fd = ftpmu010_register_reg(&pmu_reg_info);
    if (pmonitor_fd < 0)
        panic("pMonitor register pmu fail!");
}

void platform_exit(void)
{
    ftpmu010_deregister_reg(pmonitor_fd);
}

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("Process Monitor");
MODULE_LICENSE("GPL");