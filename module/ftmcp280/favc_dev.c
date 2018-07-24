/* favc_dev.c */
#ifdef FIE8150
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
    #include <asm/arch/fmem.h>
#endif

#else
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/kfifo.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <mach/platform/platform_io.h>
#include <mach/ftpmu010.h>
#endif

#ifdef ALLOCMEM_FROM_RAMMAP
#include "frammap_if.h"
#endif

#include "favc_dev.h"
#include "enc_driver/define.h"
#include "enc_driver/H264V_enc.h"
#include "h264e_ioctl.h"

#undef  PFX
#define PFX	        "FMCP300"
#include "debug.h"
#define RSVD_SZ     0x10

unsigned int mcp280_max_width = MAX_DEFAULT_WIDTH;
unsigned int mcp280_max_height = MAX_DEFAULT_HEIGHT;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
	module_param(mcp280_max_width, uint, S_IRUGO|S_IWUSR);
	module_param(mcp280_max_height, uint, S_IRUGO|S_IWUSR);
#else
	MODULE_PARM(mcp280_max_width,"i");
	MODULE_PARM(mcp280_max_height, "i");
#endif
MODULE_PARM_DESC(mcp280_max_width, "Max Width");
MODULE_PARM_DESC(mcp280_max_height, "Max Height");

#ifdef EVALUATION_PERFORMANCE
#include "performance.h"
FRAME_TIME dec_frame;
TOTAL_TIME dec_total;

uint64 time_delta(struct timeval *start, struct timeval *stop)
{
	uint64 secs, usecs;
  
	secs = stop->tv_sec - start->tv_sec;
	usecs = stop->tv_usec - start->tv_usec;
	if (usecs < 0) {
		secs--;
		usecs += 1000000;
  }
	return secs * 1000000 + usecs;
}

void dec_ap_performance_count(void)
{
	dec_total.ap_total += (dec_frame.ap_end - dec_frame.ap_start);
}

void dec_slice_performance_count(void)
{
	//dec_total.sw_total += (dec_frame.sw_tri_end - dec_frame.sw_tri_start) + (dec_frame.sw_sync_end - dec_frame.sw_sync_start);
	dec_total.sw_tri_total += (dec_frame.sw_tri_end - dec_frame.sw_tri_start);
	//dec_total.sw_sync_total += (dec_frame.sw_sync_end - dec_frame.sw_sync_start);
	dec_total.hw_total += (dec_frame.hw_end - dec_frame.hw_start);
}

void dec_performance_report(void)
{
	printk("dec sw tri %llu, dec sw sync %llu, hw %llu, ap %llu\n", dec_total.sw_tri_total, dec_total.sw_sync_total, dec_total.hw_total, dec_total.ap_total);
}

void dec_performance_reset(void)
{
	dec_total.hw_total = 0;
	dec_total.sw_tri_total = 0;
	dec_total.sw_sync_total = 0;
	dec_total.ap_total = 0;
}

unsigned int get_counter(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
  
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

extern int drv_264e_init(void);
extern void drv_264e_exit(void);

static int __init favc_enc_init(void)
{
	if (drv_264e_init() < 0)
		return -1;
	return 0;
}

static void __exit favc_enc_clean(void)
{
	drv_264e_exit();
	//return 0;
}

module_init(favc_enc_init);
module_exit(favc_enc_clean);
MODULE_AUTHOR("Grain Media Corp.");
MODULE_LICENSE("Grain Media License");
MODULE_DESCRIPTION("FTMCP280 driver");
