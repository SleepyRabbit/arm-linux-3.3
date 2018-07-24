#ifndef _FAVC_DEV_H_
#define _FAVC_DEV_H_
#if 0
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/ioport.h>       /* request_region */
#include <linux/interrupt.h>    /* mark_bh */
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
	#include <asm/arch/platform/spec.h>
#else
	#include <asm/arch/cpe/cpe.h>
#endif
#endif
//#include "ioctl_h264d.h"

#define DIV_10000(n) ((n+0xFFFF)/0x10000)*0x10000
#define DIV_1000(n) ((n+0xFFF)/0x1000)*0x1000
#define DIV_100(n) ((n+0xFF)/0x100)*0x100
//#define ALIGN_UP_1000(n)	((n)+0xFFF)

#ifdef RESOLUTION_4K4K
    #define MAX_DEFAULT_WIDTH 4096
    #define MAX_DEFAULT_HEIGHT 4096
#elif defined(RESOLUTION_4K2K)
    #define MAX_DEFAULT_WIDTH 4096
    #define MAX_DEFAULT_HEIGHT 2160
#elif defined(RESOLUTION_1080P)
    #define MAX_DEFAULT_WIDTH 1920
    #define MAX_DEFAULT_HEIGHT 1088
#else
    #define MAX_DEFAULT_WIDTH 720
    #define MAX_DEFAULT_HEIGHT 576
#endif

/**************************************************/
#ifdef FIE8150
	//#define FPGA_EXT0_VA_BASE 0
	//#define FPGA_EXT0_0_IRQ 0
/*	
	#undef MCP_FTMCP300_VA_BASE
	#undef MCP_FTMCP300_0_IRQ

	#define MCP_FTMCP300_VA_BASE FPGA_EXT0_VA_BASE
	#define MCP_FTMCP300_0_IRQ FPGA_EXT1_IRQ
*/	
	#define MCP_FTMCP280_VA_BASE FPGA_EXT0_VA_BASE
	#define FMCP280_IRQ FPGA_EXT1_IRQ
#elif defined(FPGA_8210)
    //#define FTMCP280_BASE_ADDRESS   0x90000000
    //#define FTMCP280_VCACHE_ADDRESS 0x90100000
    //#define FTMCP280_IRQ            29
    //#define FTMCP280_IRQ_1          28
#endif
/*************************************************/

#define FAVC_DECODER_MINOR  30
#define FAVC_ENCODER_MINOR  31

#define MAX_CHN_NUM         128

/*
extern int favc_decoder_open(struct inode *inode, struct file *filp);
extern int favc_decoder_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int favc_decoder_mmap(struct file *file, struct vm_area_struct *vma);
extern int favc_decoder_release(struct inode *inode, struct file *filp);
*/
void *fkmalloc(size_t size, uint8_t alignment, uint8_t reserved_size);
void fkfree(void *mem_ptr);
void * fconsistent_alloc(uint32_t size, uint8_t align_size, uint8_t reserved_size, void **phy_ptr);
void fconsistent_free(void * virt_ptr, void * phy_ptr);

#define BUFPOOL_IDX(dev,idx)	((((dev)&0xff)<<8) | ((idx)&0xff))
#define BUFPOOL_DEV(idx)	(((idx)>>8)&0xff)

/*
typedef struct {
	void (*handler)(int, void *);		// irq
	#ifdef SUPPORT_VG
		int (*job_tri) (struct v_job_t *job);	//return >=0 OK, <0 fail
		int (*job_done) (struct v_job_t *job);	// 0: done, < 0: fail, > 0: not yet
		struct v_job_t *curjob;
	#endif
} mcp300_dev;
*/

// buffer type
#define BS_BUF          0
#define SYS_INFO_BUF    1
#define MB_INFO_BUF     2
#define L1_COL_MB_BUF   3
#define DIDN_REF_BUF    4
#define SOURCE_BUF      5
#define REF_BUF         6

#if 0
#define C_DEBUG(fmt, args...) printk("FMCP: " fmt, ## args)
#else
#define C_DEBUG(fmt, args...) 
#endif

//#define ERR_DEBUG(fmt, args...) printk("error: " fmt, ## args)

struct buffer_info_ioctl_t
{
    unsigned int addr_va;
    unsigned int addr_pa;
    int size;
    int type;
};

extern int init_proc(void);
extern void exit_proc(void);

#endif

