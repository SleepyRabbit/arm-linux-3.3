/*
 * Copyright (C) 2016 Faraday Corp. (http://www.faraday-tech.com)
 *
 * Modified by Justin
 */


//#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>

#include <mach/hardware.h>
#include <mach/platform/board.h>
#include <mach/platform/platform_io.h>
#include <linux/export.h>

extern int gm_sys_suspend(void);
extern int gm_sys_suspend_sz;
extern int enter_low_freq(void);
extern void exit_low_freq(int value);
u32 pm_reg_bases[4];

/*
 * Both STANDBY and MEM suspend states are handled the same with no
 * loss of CPU or memory state
 */
int gm_pm_enter(u32 sram_base, u32 ddr_base, u32 gpio_base, int pin)
{
	int (*gm_suspend_ptr) (u32, u32 *);
	void *sram_swap_area;
	int value;

	/* Allocate some space for backup SRAM data */
	sram_swap_area = kmalloc(gm_sys_suspend_sz, GFP_KERNEL);
	if (!sram_swap_area) {
		printk(KERN_ERR
		       "PM Suspend: cannot allocate memory to backup SRAM\n");
		return -ENOMEM;
	}

	/* Backup a small area of SRAM used for the suspend code */
	memcpy(sram_swap_area, (void *) sram_base,	gm_sys_suspend_sz);

	/*
	 * Copy code to suspend system into SRAM. The suspend code
	 * needs to run from SRAM as DRAM may no longer be available.
	 */
	memcpy((void *) sram_base, &gm_sys_suspend, gm_sys_suspend_sz);
	flush_icache_range((unsigned long)sram_base,
		(unsigned long)(sram_base) + gm_sys_suspend_sz);

	pm_reg_bases[0] = PMU_FTPMU010_VA_BASE;
	pm_reg_bases[1] = ddr_base;
	pm_reg_bases[2] = gpio_base;
	pm_reg_bases[3] = 1 << pin;

	printk("System suspend\n");

	/* Jump to suspend code in SRAM */
	gm_suspend_ptr = (void *) sram_base;
	flush_cache_all();

	local_irq_disable();
	value = enter_low_freq();

	(void) gm_suspend_ptr(0, pm_reg_bases);

	exit_low_freq(value);
	local_irq_enable();

	/* Return and restore original IRAM contents */
	memcpy((void *) sram_base, sram_swap_area,	gm_sys_suspend_sz);
	printk("System Resume\n");

	kfree(sram_swap_area);

	return 0;
}
