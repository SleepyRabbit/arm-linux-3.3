#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#ifndef A369
#include <mach/platform/platform_io.h>
#endif
#include <mach/ftpmu010.h>
#include "platform.h"

#ifdef A369
unsigned long favcd_base_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS};
unsigned long __initdata favcd_base_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS_SIZE};
unsigned long favcd_vcache_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS};
unsigned long __initdata favcd_vcache_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS_SIZE};
int irq_no[FTMCP300_NUM] = {FTMCP300_DEC0_IRQ};
const char irq_name[FTMCP300_NUM][20] = {"ftmcp300-0"};
#endif

#ifdef GMXXXX /* an example of adding new platform of one mcp300 engine */
unsigned long favcd_base_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS};
unsigned long __initdata favcd_base_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS_SIZE};
unsigned long favcd_vcache_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS};
unsigned long __initdata favcd_vcache_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS_SIZE};
int irq_no[FTMCP300_NUM] = {FTMCP300_DEC0_IRQ};
const char irq_name[FTMCP300_NUM][20] = {"ftmcp300-0"};
#endif

#ifdef GM8287
unsigned long favcd_base_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS};
unsigned long __initdata favcd_base_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS_SIZE};
unsigned long favcd_vcache_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS};
unsigned long __initdata favcd_vcache_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS_SIZE};
int irq_no[FTMCP300_NUM] = {FTMCP300_DEC0_IRQ};
const char irq_name[FTMCP300_NUM][20] = {"ftmcp300-0"};
#endif

#ifdef GM8210
unsigned long favcd_base_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS, FTMCP300_DEC1_BASE_ADDRESS};
unsigned long __initdata favcd_base_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_BASE_ADDRESS_SIZE, FTMCP300_DEC1_BASE_ADDRESS_SIZE};
unsigned long favcd_vcache_address_pa[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS, FTMCP300_DEC1_VCACHE_ADDRESS};
unsigned long __initdata favcd_vcache_address_size[FTMCP300_NUM] = {FTMCP300_DEC0_VCACHE_ADDRESS_SIZE, FTMCP300_DEC1_VCACHE_ADDRESS_SIZE};
int irq_no[FTMCP300_NUM] = {FTMCP300_DEC0_IRQ, FTMCP300_DEC1_IRQ};
const char irq_name[FTMCP300_NUM][20] = {"ftmcp300-0", "ftmcp300-1"};
#endif


#ifdef GMXXXX /* an example of adding new platform of one mcp300 engine */
#warning check if the following bits is correct
static pmuReg_t regH264DArray[] = {
    /* reg_off  bit_masks    lock_bits    init_val     init_mask */
    {0x28,      (0x1 << 30), (0x1 << 30), (0x0 << 30), (0x0 << 30)}, /* bit 30: 0-aclk, 1-pll4out3 */
    {0xA0,      (0x1 << 3),  (0x1 << 3),  (0x0 << 3),  (0x0 << 3)},  /* bit 3: engine 0 aclk reset, (0-active) */
    {0xB0,      (0x1 << 24), (0x1 << 24), (0x0 << 24), (0x0 << 24)}, /* bit 24: engine 0 AXI clock on/off (0-active) */
    {0xB4,      (0x1 << 29), (0x1 << 29), (0x0 << 29), (0x0 << 29)}, /* bit 29: engine 1 AHB clock on/off (0-active) */
};
#endif

#ifdef GM8287
static pmuReg_t regH264DArray[] = {
    /* reg_off  bit_masks    lock_bits    init_val     init_mask */
    {0x28,      (0x1 << 30), (0x1 << 30), (0x0 << 30), (0x0 << 30)}, /* bit 30: 0-aclk, 1-pll4out3 */
    {0xA0,      (0x1 << 3),  (0x1 << 3),  (0x0 << 3),  (0x0 << 3)},  /* bit 3: engine 0 aclk reset, (0-active) */
    {0xB0,      (0x1 << 24), (0x1 << 24), (0x0 << 24), (0x0 << 24)}, /* bit 24: engine 0 AXI clock on/off (0-active) */
    {0xB4,      (0x1 << 29), (0x1 << 29), (0x0 << 29), (0x0 << 29)}, /* bit 29: engine 1 AHB clock on/off (0-active) */
};
#endif

#ifdef GM8210
static pmuReg_t regH264DArray[] = {
    /* reg_off  bit_masks    lock_bits    init_val     init_mask */
    {0x28,      (0x1 << 30), (0x1 << 30), (0x0 << 30), (0x0 << 30)}, /* bit 30: 0-aclk, 1-pll4out3 */
    {0xA0,      (0x3 << 3),  (0x3 << 3),  (0x0 << 3),  (0x0 << 3)},  /* bit 3: engine 0 aclk reset, (0-active), bit 4: engine 1 aclk reset (0-active) */
    {0xB0,      (0x1 << 24)|(0x1 << 2), (0x1 << 24)|(0x1 << 2), (0x0 << 24)|(0x0 << 2), (0x0 << 24)|(0x0 << 2)}, /* bit 2: engine 1 AXI clock on/off (0-clk on) bit 24: engine 0 AXI clock on/off (0-active) */
    {0xB4,      (0x1 << 29)|(0x1 << 7), (0x1 << 29)|(0x1 << 7), (0x0 << 29)|(0x0 << 7), (0x0 << 29)|(0x0 << 7)}, /* bit 7: engine 1 AHB clock on/off (0-clk on) bit 29: engine 1 AHB clock on/off (0-active) */
};
#endif


#ifndef A369
static pmuRegInfo_t h264d_clk_info = {
    "H264D_CLK",
    ARRAY_SIZE(regH264DArray),
    ATTR_TYPE_PLL4,
    regH264DArray
};
static int ftpmu010_h264d_fd = -1;    //pmu
#endif


/*
 * register MCP300 PMU registers
 */
int pf_mcp300_clk_init(void)
{
#ifndef A369
    ftpmu010_h264d_fd = ftpmu010_register_reg(&h264d_clk_info);
    if (unlikely(ftpmu010_h264d_fd < 0)){
        printk("H264 dec registers to PMU fail! \n");
        return -1;
    }
#endif
    return 0;
}


/*
 * deregister MCP300 PMU registers
 */
int pf_mcp300_clk_exit(void)
{
#ifndef A369
    ftpmu010_deregister_reg(ftpmu010_h264d_fd);
    ftpmu010_h264d_fd = -1;
#endif
    return 0;
}

/*
 * select clock source, stop reset and enable clock
 */
int pf_mcp300_clk_on(void)
{
    extern unsigned int clk_sel;

#ifndef A369
    if (ftpmu010_h264d_fd < 0) {
        if (pf_mcp300_clk_init() < 0)
            return -1;
    }
#ifdef GM8210
    /* ftpmu010_write_reg(int fd, unsigned int reg_off, unsigned int val, unsigned int mask) */
    if(clk_sel)
        ftpmu010_write_reg(ftpmu010_h264d_fd, 0x28, 0x1<<30, 0x1<<30);                   /* set bit 30: use pll4out3 clk(403MHz) (if not set, use ACLK(396MHz))*/
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0x03<<3, 0x03<<3);                       /* set bit 3, 4: aclk reset inactive */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x0<<24)|(0x0<<2), (0x1<<24)|(0x1<<2)); /* unset bit 2, 24: AXI clock on */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x0<<29)|(0x0<<7), (0x1<<29)|(0x1<<7)); /* unset bit 7, 29: AHB clock on */
#endif
#ifdef GMXXXX /* an example of adding new platform of one mcp300 engine */
    #warning check if the following bits is correct
    /* ftpmu010_write_reg(int fd, unsigned int reg_off, unsigned int val, unsigned int mask) */
    if(clk_sel)
        ftpmu010_write_reg(ftpmu010_h264d_fd, 0x28, 0x1<<30, 0x1<<30); /* set bit 30: use pll4out3 clk(403MHz) (if not set, use ACLK(396MHz))*/
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0x01<<3, 0x01<<3);     /* set bit 3: aclk reset inactive */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x0<<24), (0x1<<24)); /* unset bit 24: AXI clock on */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x0<<29), (0x1<<29)); /* unset bit 29: AHB clock on */
#endif
#ifdef GM8287
    /* ftpmu010_write_reg(int fd, unsigned int reg_off, unsigned int val, unsigned int mask) */
    if(clk_sel)
        ftpmu010_write_reg(ftpmu010_h264d_fd, 0x28, 0x1<<30, 0x1<<30); /* set bit 30: use pll4out3 clk(403MHz) (if not set, use ACLK(396MHz))*/
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0x01<<3, 0x01<<3);     /* set bit 3: aclk reset inactive */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x0<<24), (0x1<<24)); /* unset bit 24: AXI clock on */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x0<<29), (0x1<<29)); /* unset bit 29: AHB clock on */
#endif
#endif
    return 0;
}


/*
 * disable clock and assert reset
 */
int pf_mcp300_clk_off(void)
{
#ifndef A369
    if (ftpmu010_h264d_fd < 0)
        return 0;
    
#ifdef GM8210
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x1<<24)|(0x1<<2), (0x1<<24)|(0x1<<2)); /* set bit 2, 24: AXI clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x1<<29)|(0x1<<7), (0x1<<29)|(0x1<<7)); /* set bit 7, 29: AHB clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0, 0x03<<3);                             /* unset bit 3, 4: aclk reset active */
#endif
#ifdef GMXXXX /* an example of adding new platform of one mcp300 engine */
    #warning check if the following bits is correct
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x1<<24), (0x1<<24)); /* set bit 24: AXI clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x1<<29), (0x1<<29)); /* set bit 29: AHB clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0, 0x01<<3);           /* unset bit 3: aclk reset active */
#endif
#ifdef GM8287
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB0, (0x1<<24), (0x1<<24)); /* set bit 24: AXI clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xB4, (0x1<<29), (0x1<<29)); /* set bit 29: AHB clock off */
    ftpmu010_write_reg(ftpmu010_h264d_fd, 0xA0, 0, 0x01<<3);           /* unset bit 3: aclk reset active */
#endif
#endif

    return 0;
}

