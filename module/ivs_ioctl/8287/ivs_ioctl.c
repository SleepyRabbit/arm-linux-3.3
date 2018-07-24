#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>    /* HDIO_GETGEO */
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/miscdevice.h>

#include <mach/fmem.h>
#include <asm/cacheflush.h>		 

#include "ivs_ioctl.h"
#include "frammap_if.h"


//	printf debug message
#define IVS_DBG_ENABLE 		1

//	ivs minor and irq number
#define IVS_MINOR  			30
#define IVS_IRQ  			47



//	support ivs number	
#define IVS_IDX_MASK_n(n) 	(0x1<<n)
#define IVS_IDX_FULL    	0xFFFFFFFF
static int ivs_idx  = 0;


struct dec_private
{
    int dev;                    // an ID for each decoder open. range from 0 to (MAX_DEC_NUM - 1). also a index to dec_data
};

static struct dec_private dec_data[MAX_IVS_NUM] = {{0}};

//	memory allocate
#define IMEM_SIZE 			(4096*2160*2+1920*1080*6)
unsigned char *ivs_virt = 0; 
unsigned int   ivs_phy  = 0;

//	ivs clk and control base register
volatile unsigned int *ivs_reg_base=0;
volatile unsigned int *clk_reg_base=0;

//	support irq 
int isr_condition_flag = 0 ;
wait_queue_head_t ivs_queue;

#if (LINUX_VERSION_CODE == KERNEL_VERSION(3,3,0))
static DEFINE_SPINLOCK(avcdec_request_lock); 
#else
static spinlock_t avcdec_request_lock = SPIN_LOCK_UNLOCKED;
#endif

struct semaphore favc_dec_mutex;

void hsi(ivs_param_t ivs_param)
{
	//	copy information from frame_info
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;
	char swap = ivs_param.swap ;
	
	//	control register
	volatile unsigned int *reg_resolution ;
	volatile unsigned int *reg_rchan1_str ;
	volatile unsigned int *reg_rchan1_off ;
	volatile unsigned int *reg_wchan1_str ;
	volatile unsigned int *reg_wchan2_str ;
	volatile unsigned int *reg_wchan3_str ;
	volatile unsigned int *reg_parameter ;
	volatile unsigned int *reg_ivs_state ;
	volatile unsigned int *reg_control ;	
	unsigned int ivs_state ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width * 2) & 0xFFFF) ;

    reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

    reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;
    
	reg_wchan1_str = ivs_reg_base + (0x28 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 1 ;

    reg_wchan2_str = ivs_reg_base + (0x2c >> 2) ;
	*reg_wchan2_str = ((ivs_phy + (img_width * img_height * 4)) & 0xFFFFFFFC) | 0 ;
    
	reg_wchan3_str = ivs_reg_base + (0x30 >> 2) ;		
	*reg_wchan3_str = ((ivs_phy + (img_width * img_height * 5)) & 0xFFFFFFFC) | 0 ;

    reg_parameter = ivs_reg_base + (0x04 >> 2) ;
	*reg_parameter = 0 | (swap & 0x3) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;
    *reg_control = 0 | (7 << 6) | 1 ;		

    ivs_state          = *reg_ivs_state ;

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution ) ;
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str ) ;
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off ) ;   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str ) ;  
	printk("reg_wchan2_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan2_str,   *reg_wchan2_str ) ;  
	printk("reg_wchan3_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan3_str,   *reg_wchan3_str ) ;
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter  ) ; 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control    ) ;    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state       ) ;    		      	
#endif
}

void rgb(ivs_param_t ivs_param)
{
	//	copy information from frame_info
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;
	char swap = ivs_param.swap ;
	
	//	control register
	volatile unsigned int *reg_resolution ;
	volatile unsigned int *reg_rchan1_str ;
	volatile unsigned int *reg_rchan1_off ;
	volatile unsigned int *reg_wchan1_str ;
	volatile unsigned int *reg_wchan2_str ;
	volatile unsigned int *reg_wchan3_str ;
	volatile unsigned int *reg_parameter ;
	volatile unsigned int *reg_ivs_state ;
	volatile unsigned int *reg_control ;	
	unsigned int ivs_state ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width * 2) & 0xFFFF);

    reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

    reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;

    reg_wchan1_str = ivs_reg_base + (0x28 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 0 ;

    reg_wchan2_str = ivs_reg_base + (0x2c >> 2) ;		
	*reg_wchan2_str = ((ivs_phy + (img_width * img_height * 3)) & 0xFFFFFFFC) | 0 ;  

    reg_wchan3_str = ivs_reg_base + (0x30 >> 2) ;		
	*reg_wchan3_str = ((ivs_phy + (img_width * img_height * 4)) & 0xFFFFFFFC) | 0 ; 

    reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 | (1 << 9) | (swap & 0x3) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2);

	reg_control = ivs_reg_base + (0x00 >> 2);
    *reg_control = 0 | (7 << 6) | 1;		

    ivs_state          = *reg_ivs_state ;

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution  );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str  );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off  );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str  );  
	printk("reg_wchan2_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan2_str,   *reg_wchan2_str  );  
	printk("reg_wchan3_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan3_str,   *reg_wchan3_str  );
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter   ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control     );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state        );    		      	
#endif
}

void integral(ivs_param_t ivs_param)
{
	//	copy information from ivs_param
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;

    char file_format = ivs_param.file_format ;
    char endian = ivs_param.endian ;
	char swap = ivs_param.swap ;
	
	//	control register
	volatile unsigned int *reg_resolution ;
	volatile unsigned int *reg_rchan1_str ;
	volatile unsigned int *reg_rchan1_off ;
	volatile unsigned int *reg_wchan1_str ;
	volatile unsigned int *reg_parameter ;
	volatile unsigned int *reg_ivs_state ;
	volatile unsigned int *reg_control ;	
	unsigned int ivs_state ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
    *reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width << ((1 - file_format) & 0x1)) & 0xFFFF) ;
        
	reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;
	
	reg_wchan1_str = ivs_reg_base + (0x34 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 2 ; ;
		
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 | ((file_format & 0x1) << 5) | (swap & 0x3) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;
    *reg_control = 0 | (1 << 9) | ((endian & 0x1) << 2) | 1 ;

	ivs_state          = *reg_ivs_state ;
	
#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution   );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str   );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off   );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str   );  
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter    ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control      );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state         );    		      	
#endif
}

void s_integral(ivs_param_t ivs_param)
{
	//	copy information from ivs_param
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;

    char file_format = ivs_param.file_format ;
    char endian = ivs_param.endian ;
	char swap = ivs_param.swap ;
	
	//	control register
	volatile unsigned int *reg_resolution ;
	volatile unsigned int *reg_rchan1_str ;
	volatile unsigned int *reg_rchan1_off ;
	volatile unsigned int *reg_wchan1_str ;
	volatile unsigned int *reg_parameter ;
	volatile unsigned int *reg_ivs_state ;
	volatile unsigned int *reg_control ;	
	unsigned int ivs_state ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
    *reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width << ((1 - file_format) & 0x1)) & 0xFFFF) ;

	reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;
	
	reg_wchan1_str = ivs_reg_base + (0x38 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 2 ; ;
		
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 | ((file_format & 0x1) << 5) | (swap & 0x3) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;
    *reg_control = 0 | (1 << 10) | ((endian & 0x1) << 2) | 1 ;
       
	ivs_state          = *reg_ivs_state ;
	
#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution   );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str   );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off   );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str   );  
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter    ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control      );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state         );    		      	
#endif
}

void de_interleave(ivs_param_t ivs_param)
{
	//	copy information from frame_info
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;
	char swap = ivs_param.swap ;
	
	//	control register
	volatile unsigned int *reg_resolution ;
	volatile unsigned int *reg_rchan1_str ;
	volatile unsigned int *reg_rchan1_off ;
	volatile unsigned int *reg_wchan1_str ;
	volatile unsigned int *reg_wchan2_str ;
	volatile unsigned int *reg_parameter ;
	volatile unsigned int *reg_ivs_state ;
	volatile unsigned int *reg_control ;	
	unsigned int ivs_state ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | (img_width * 2 & 0xFFFF);

	reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 2 * (img_width - img_width) ;
	
	reg_wchan1_str = ivs_reg_base + (0x20 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 0 ;
	
	reg_wchan2_str = ivs_reg_base + (0x24 >> 2) ;		
	*reg_wchan2_str = ((ivs_phy + (img_width * img_height * 3)) & 0xFFFFFFFC) | 1 ;
	
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
	*reg_parameter = 0 | (swap & 0x3) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;	
    *reg_control = 0 | (3 << 4) | 1 ;
	
	ivs_state          = *reg_ivs_state ;        

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str );  
	printk("reg_wchan2_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan2_str,   *reg_wchan2_str );  
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter  ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control    );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state       );    		      	
#endif
}

void histogram(ivs_param_t ivs_param)
{
    //	copy information from frame_info
	unsigned int img_width = ivs_param.img_width; 
	unsigned int img_height = ivs_param.img_height;
    char file_format = ivs_param.file_format;
    char endian = ivs_param.endian;
    char swap = ivs_param.swap;
	
	//	control register
	volatile unsigned int *reg_resolution;
	volatile unsigned int *reg_rchan1_str;
	volatile unsigned int *reg_rchan1_off;
	volatile unsigned int *reg_wchan1_str;
	volatile unsigned int *reg_parameter;
	volatile unsigned int *reg_ivs_state;
	volatile unsigned int *reg_control;	
	unsigned int ivs_state;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
    *reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width << ((1 - file_format) & 0x1)) & 0xFFFF) ;
    
	reg_rchan1_str = ivs_reg_base + (0x10 >> 2);
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2);	
	*reg_rchan1_off = 0;
	
	reg_wchan1_str = ivs_reg_base + (0x24 >> 2) ;
	*reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 2 ;
	
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 | (1 << 8) | ((file_format & 0x1) << 5) | (swap & 0x3) ;
    
	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;
    *reg_control = 0 | (1 << 5) |
	
	ivs_state          = *reg_ivs_state ;        
#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str );  
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter  ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control    );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state       );    		      	
#endif
}

void convolution(ivs_param_t ivs_param)
{
	//	copy information from frame_info
	unsigned int img_width  = ivs_param.img_width  ; 
	unsigned int img_height = ivs_param.img_height ;
	char endian = ivs_param.endian ;
    char shift  = ivs_param.convolution_shift ;
	
	//	control register
	volatile unsigned int *reg_resolution   ;
	volatile unsigned int *reg_rchan1_str   ;
	volatile unsigned int *reg_rchan1_off   ;
	volatile unsigned int *reg_wchan1_str   ;
	volatile unsigned int *reg_wchan2_str   ;
	volatile unsigned int *reg_wchan3_str   ;
    volatile unsigned int *reg_filer_mask0  ;
    volatile unsigned int *reg_filer_mask1  ;
    volatile unsigned int *reg_filer_mask2  ;
    volatile unsigned int *reg_filer_mask3  ;
    volatile unsigned int *reg_filer_mask4  ;
    volatile unsigned int *reg_filer_mask5  ;
    volatile unsigned int *reg_filer_mask6  ;
    volatile unsigned int *reg_parameter    ;
	volatile unsigned int *reg_ivs_state    ;
	volatile unsigned int *reg_control      ;	
	unsigned int ivs_state                  ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | (img_width & 0xFFFF);

    reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

    reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 2 * (img_width - img_width);

    reg_wchan1_str = ivs_reg_base + (0x28 >> 2) ;
    if (shift == 0)
	    *reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 1 ;
    else
        *reg_wchan1_str = ivs_phy + (img_width * img_height * 2) ;
    
    reg_wchan2_str = ivs_reg_base + (0x2c >> 2) ;		
	*reg_wchan2_str = ((ivs_phy + (img_width * img_height * 4)) & 0xFFFFFFFC) | 1 ;

    reg_wchan3_str = ivs_reg_base + (0x30 >> 2) ;		
	*reg_wchan3_str = ((ivs_phy + (img_width * img_height * 6)) & 0xFFFFFFFC) | 1 ;

    reg_filer_mask0 = ivs_reg_base + (0x80 >> 2) ;
    *reg_filer_mask0 = *reg_filer_mask0 | (ivs_param.template_element[0] & 0xFF) ;   
    *reg_filer_mask0 = *reg_filer_mask0 | ((ivs_param.template_element[1] & 0xFF) << 8) ;
    *reg_filer_mask0 = *reg_filer_mask0 | ((ivs_param.template_element[2] & 0xFF) << 16) ;
    *reg_filer_mask0 = *reg_filer_mask0 | ((ivs_param.template_element[3] & 0xFF) << 24) ;
    
    reg_filer_mask1 = ivs_reg_base + (0x84 >> 2) ;
    *reg_filer_mask1 = *reg_filer_mask1 | (ivs_param.template_element[4] & 0xFF) ;   
    *reg_filer_mask1 = *reg_filer_mask1 | ((ivs_param.template_element[5] & 0xFF) << 8) ;
    *reg_filer_mask1 = *reg_filer_mask1 | ((ivs_param.template_element[6] & 0xFF) << 16) ;
    *reg_filer_mask1 = *reg_filer_mask1 | ((ivs_param.template_element[7] & 0xFF) << 24) ;
    
    reg_filer_mask2 = ivs_reg_base + (0x88 >> 2) ;
    *reg_filer_mask2 = *reg_filer_mask2 | (ivs_param.template_element[8] & 0xFF) ;   
    *reg_filer_mask2 = *reg_filer_mask2 | ((ivs_param.template_element[9] & 0xFF) << 8) ;
    *reg_filer_mask2 = *reg_filer_mask2 | ((ivs_param.template_element[10] & 0xFF) << 16) ;
    *reg_filer_mask2 = *reg_filer_mask2 | ((ivs_param.template_element[11] & 0xFF) << 24) ;

    reg_filer_mask3 = ivs_reg_base + (0x8c >> 2) ;
    *reg_filer_mask3 = *reg_filer_mask3 | (ivs_param.template_element[12] & 0xFF) ;   
    *reg_filer_mask3 = *reg_filer_mask3 | ((ivs_param.template_element[13] & 0xFF) << 8) ;
    *reg_filer_mask3 = *reg_filer_mask3 | ((ivs_param.template_element[14] & 0xFF) << 16) ;
    *reg_filer_mask3 = *reg_filer_mask3 | ((ivs_param.template_element[15] & 0xFF) << 24) ;
    
    reg_filer_mask4 = ivs_reg_base + (0x90 >> 2) ;
    *reg_filer_mask4 = *reg_filer_mask4 | (ivs_param.template_element[16] & 0xFF) ;   
    *reg_filer_mask4 = *reg_filer_mask4 | ((ivs_param.template_element[17] & 0xFF) << 8) ;
    *reg_filer_mask4 = *reg_filer_mask4 | ((ivs_param.template_element[18] & 0xFF) << 16) ;
    *reg_filer_mask4 = *reg_filer_mask4 | ((ivs_param.template_element[19] & 0xFF) << 24) ;

    reg_filer_mask5 = ivs_reg_base + (0x94 >> 2) ;
    *reg_filer_mask5 = *reg_filer_mask5 | (ivs_param.template_element[20] & 0xFF) ;   
    *reg_filer_mask5 = *reg_filer_mask5 | ((ivs_param.template_element[21] & 0xFF) << 8) ;
    *reg_filer_mask5 = *reg_filer_mask5 | ((ivs_param.template_element[22] & 0xFF) << 16) ;
    *reg_filer_mask5 = *reg_filer_mask5 | ((ivs_param.template_element[23] & 0xFF) << 24) ;
    
    reg_filer_mask6 = ivs_reg_base + (0x98 >> 2) ;
    *reg_filer_mask6 = *reg_filer_mask6 | (ivs_param.template_element[24] & 0xFF) ;  

    reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 ;
    *reg_parameter = *reg_parameter | (1 << 5);
    
	reg_ivs_state = ivs_reg_base + (0x74 >> 2);

	reg_control = ivs_reg_base + (0x00 >> 2);
    if (endian == 1)
        *reg_control = *reg_control | (1 << 2);
    *reg_control = *reg_control | (7 << 6);
    *reg_control = *reg_control | 1;		

    ivs_state          = *reg_ivs_state ;

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution  );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str  );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off  );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str  );  
	printk("reg_wchan2_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan2_str,   *reg_wchan2_str  );  
	printk("reg_wchan3_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan3_str,   *reg_wchan3_str  );
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter   ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control     );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state        );    		      	
#endif
}

void morphology(ivs_param_t ivs_param)
{
	//	copy information from frame_info
	unsigned int img_width  = ivs_param.img_width  ; 
	unsigned int img_height = ivs_param.img_height ;
	char endian = ivs_param.endian ;
    char shift  = ivs_param.convolution_shift ;
	
	//	control register
	volatile unsigned int *reg_resolution   ;
	volatile unsigned int *reg_rchan1_str   ;
	volatile unsigned int *reg_rchan1_off   ;
	volatile unsigned int *reg_wchan1_str   ;
    volatile unsigned int *reg_parameter    ;
	volatile unsigned int *reg_ivs_state    ;
	volatile unsigned int *reg_control      ;	
	unsigned int ivs_state                  ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | (img_width & 0xFFFF);

    reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

    reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 2 * (img_width - img_width);

    reg_wchan1_str = ivs_reg_base + (0x30 >> 2) ;
    *reg_wchan1_str = ivs_phy + (img_width * img_height * 2) ;
    
    reg_parameter = ivs_reg_base + (0x04 >> 2) ;
    *reg_parameter = 0 ;
    *reg_parameter = *reg_parameter | (1 << 24) | (200 << 16) | (1 << 10) | (1 << 5) | (0 << 2);
   
	reg_ivs_state = ivs_reg_base + (0x74 >> 2);

	reg_control = ivs_reg_base + (0x00 >> 2);
    if (endian == 1)
        *reg_control = *reg_control | (1 << 2);
    *reg_control = *reg_control | (1 << 8);
    *reg_control = *reg_control | 1;		

    ivs_state          = *reg_ivs_state ;

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution   addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution  );
	printk("reg_rchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str  );
	printk("reg_rchan1_off   addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off  );   
	printk("reg_wchan1_str   addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str  );  
	printk("reg_parameter    addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,    *reg_parameter   ); 
	printk("reg_control      addr:0x%08X val:%08X\n", (unsigned int)reg_control,      *reg_control     );    
	printk("reg_ivs_state    addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state        );    		      	
#endif
}

void sad(ivs_param_t ivs_param)
{
	//	copy information from ivs_param
	unsigned int img_width	= ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;
    unsigned int b_th = ivs_param.threshold ;
	char endian  = ivs_param.endian ;
	char blksize = ivs_param.block_size ;
	
	//	control register
	volatile unsigned int *reg_resolution	;
	volatile unsigned int *reg_rchan1_str	;
	volatile unsigned int *reg_rchan1_off	;
	volatile unsigned int *reg_rchan2_str	;
	volatile unsigned int *reg_rchan2_off	;
	volatile unsigned int *reg_wchan1_str   ;
	volatile unsigned int *reg_parameter	;
	volatile unsigned int *reg_ivs_state	;
	volatile unsigned int *reg_control		;	
	unsigned int ivs_state                  ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
	*reg_resolution = ((img_height & 0xFFFF) << 16) | (img_width & 0xFFFF);

	reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;

	reg_rchan2_str = ivs_reg_base + (0x18 >> 2) ;
	*reg_rchan2_str = ivs_phy + (img_width * img_height) ;

	reg_rchan2_off = ivs_reg_base + (0x1c >> 2) ;	
	*reg_rchan2_off = 0 ;
	
	reg_wchan1_str = ivs_reg_base + (0x20 >> 2) ;
    *reg_wchan1_str = ((ivs_phy + (img_width * img_height * 2)) & 0xFFFFFFFC) | 1 ;
    	
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
	*reg_parameter = 0 | ((b_th & 0xFFFF) << 16) | (1 << 5) |((blksize & 0x3) << 2) ;

	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;	
    *reg_control = 0 | (1 << 12) | ((endian & 0x1) << 2) | (1 << 1) | 1 ;

	ivs_state	       = *reg_ivs_state ;		 

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution     addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution  );
	printk("reg_rchan1_str     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str  );
	printk("reg_rchan1_off     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off  );	
	printk("reg_rchan2_str     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan2_str,   *reg_rchan2_str  );
	printk("reg_rchan2_off     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan2_off,   *reg_rchan2_off  );	
	printk("reg_wchan1_str     addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str  );	
	printk("reg_parameter      addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,	*reg_parameter   ); 
	printk("reg_control        addr:0x%08X val:%08X\n", (unsigned int)reg_control,	    *reg_control	 );	  
	printk("reg_ivs_state      addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state        ); 				
#endif	
}

void roster(ivs_param_t ivs_param)
{
	//	copy information from ivs_param
	unsigned int img_width  = ivs_param.img_width ; 
	unsigned int img_height = ivs_param.img_height ;
    unsigned int raster_op = ivs_param.raster_op ;
    char file_format = ivs_param.file_format ;
    char endian = ivs_param.endian ;
	
	//	control register
	volatile unsigned int *reg_resolution	;
	volatile unsigned int *reg_rchan1_str	;
	volatile unsigned int *reg_rchan1_off	;
	volatile unsigned int *reg_rchan2_str	;
	volatile unsigned int *reg_rchan2_off	;
	volatile unsigned int *reg_wchan1_str   ;
	volatile unsigned int *reg_parameter	;
	volatile unsigned int *reg_ivs_state	;
	volatile unsigned int *reg_control		;	
	unsigned int ivs_state                  ;

	//	set register value
	reg_resolution = ivs_reg_base + (0x08 >> 2) ;
    if (file_format == 1)
	    *reg_resolution = ((img_height & 0xFFFF) << 16) | (img_width & 0xFFFF);
    else
        *reg_resolution = ((img_height & 0xFFFF) << 16) | ((img_width/8) & 0xFFFF);
    
	reg_rchan1_str = ivs_reg_base + (0x10 >> 2) ;
	*reg_rchan1_str = ivs_phy ;

	reg_rchan1_off = ivs_reg_base + (0x14 >> 2) ;	
	*reg_rchan1_off = 0 ;

	reg_rchan2_str = ivs_reg_base + (0x18 >> 2) ;
    if (file_format == 1)
	    *reg_rchan2_str = ivs_phy + (img_width*img_height) ;
    else
        *reg_rchan2_str = ivs_phy + (img_width*img_height/8) ;

	reg_rchan2_off = ivs_reg_base + (0x1c >> 2) ;	
	*reg_rchan2_off = 0 ;
	
	reg_wchan1_str = ivs_reg_base + (0x24 >> 2) ;
	*reg_wchan1_str = ivs_phy + (img_width * img_height * 2);
		
	reg_parameter = ivs_reg_base + (0x04 >> 2) ;
	*reg_parameter = 0 | ((raster_op & 0xF) << 12) | (1 << 5); 
   
	reg_ivs_state = ivs_reg_base + (0x74 >> 2) ;

	reg_control = ivs_reg_base + (0x00 >> 2) ;
    *reg_control = 0 | (1 << 13) | ((endian & 0x1) << 2) | (1 << 1) | 1 ;
    
    ivs_state = *reg_ivs_state ;		 

#if(IVS_DBG_ENABLE == 1)
	printk("reg_resolution     addr:0x%08X val:%08X\n", (unsigned int)reg_resolution,   *reg_resolution  );
	printk("reg_rchan1_str     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_str,   *reg_rchan1_str  );
	printk("reg_rchan1_off     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan1_off,   *reg_rchan1_off  );	
	printk("reg_rchan2_str     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan2_str,   *reg_rchan2_str  );
	printk("reg_rchan2_off     addr:0x%08X val:%08X\n", (unsigned int)reg_rchan2_off,   *reg_rchan2_off  );	
	printk("reg_wchan1_str     addr:0x%08X val:%08X\n", (unsigned int)reg_wchan1_str,   *reg_wchan1_str  );	
	printk("reg_parameter      addr:0x%08X val:%08X\n", (unsigned int)reg_parameter,	*reg_parameter   ); 
	printk("reg_control        addr:0x%08X val:%08X\n", (unsigned int)reg_control,	    *reg_control	 );	  
	printk("reg_ivs_state      addr:0x%08X val:%08X\n", (unsigned int)reg_ivs_state,    ivs_state        ); 				
#endif	
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
long ivs_ioctl( struct file *filp, unsigned int cmd, unsigned long arg)
#else
int ivs_ioctl( struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	int ret = 0;
	ivs_param_t ivs_param ;
	unsigned int API_version ;

	volatile unsigned int *reg_sys_rst ;
	volatile unsigned int *reg_74 ;	

	unsigned long time ;
	
	reg_sys_rst = ivs_reg_base + (0x00 >> 2) ;	
	reg_74      = ivs_reg_base + (0x74 >> 2) ;

	down(&favc_dec_mutex);
    switch (cmd) {

		case IVS_INIT:
			if((ret = copy_from_user((void *)&API_version, (void *)arg, sizeof(unsigned int))) )
			{
				printk("copy from user error: %d %d\n", ret, sizeof(API_version));
				up(&favc_dec_mutex);
				return -4;
			}
			
			if( (API_version&0xFFFFFFF0) != (IVS_VER&0xFFFFFFF0) )
			{
				printk("Fail API Version \n");
				up(&favc_dec_mutex);
				return -2;
			}
		break;

		case IVS_HSI_CONVERSION:
			if ( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
			
            hsi(ivs_param) ;
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));     
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("hsi conversion isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;	
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}				

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt		
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("hsi conversion receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;

        case IVS_RGB_CONVERSION:
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
			
            rgb(ivs_param) ;
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));     
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("rgb conversion isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;	
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}				

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt		
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("rgb conversion receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;  

        case IVS_INTEGRAL_IMAGE:
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}

        	integral(ivs_param) ;
        	wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout

			if( isr_condition_flag==0 )	//timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("integral image isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("integral image receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
        break;

        case IVS_SQUARED_INTEGRAL_IMAGE:
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}

        	s_integral(ivs_param) ;
        	wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout

			if( isr_condition_flag==0 )	//timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("squared integral image isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("squared integral image receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
        break;
        
        case IVS_DE_INTERLEAVE_YC:    
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
            
			de_interleave(ivs_param) ;			
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("de interleave isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt			
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("de interleave receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;

        case IVS_HISTOGRAM:    
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
            
			histogram(ivs_param) ;			
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("histogram isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt			
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("histogram receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;

        case IVS_CONVOLUTION:    
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
            
			convolution(ivs_param) ;			
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("convolution isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt			
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("convolution receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;

        case IVS_MORPHOLOGY:    
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}

            morphology(ivs_param);			
            wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout
			if( isr_condition_flag==0 )	//	timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("morphology isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 						//	interrupt			
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("morphology receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
		break;
       		
        case IVS_SAD:
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
            sad(ivs_param) ;			
        	wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout

			if( isr_condition_flag==0 )	//timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("sad isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("sad receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
        break;		

        case IVS_RASTER_OPERATION:
			if( (ret = copy_from_user((void *)&ivs_param, (void *)arg, sizeof(ivs_param))) ) 
			{
				printk("copy from user error: %d %d\n", ret, sizeof(ivs_param));
				up(&favc_dec_mutex);
				return -4;
			}
            roster(ivs_param) ;			
        	wait_event_timeout(ivs_queue, isr_condition_flag, msecs_to_jiffies(3000));  // fixed timout

			if( isr_condition_flag==0 )	//timeout
			{
#if(IVS_DBG_ENABLE == 1)				
				printk("roster isr timeout, reg_74 = %d\n",*reg_74) ;
#endif
				
				//	software reset
				*reg_sys_rst = (1<<31) ;
				*reg_sys_rst = 0;

				//	polling reg_74(ivs status register)
				time = jiffies ;
				while( *reg_74==0 && ((jiffies-time)<1000) ) {}	//	softreset make hardware stop and would cause isr, so polling reg_74(ivs_status)			

				up(&favc_dec_mutex);
				return -5;
			} 
			else 
			{		
				isr_condition_flag = 0;
#if(IVS_DBG_ENABLE == 1)				
				printk("roster receive isr ,reg_74 = %d\n",*reg_74) ;
#endif
			}
        break;	
          
        default:
            printk("undefined ioctl cmd %x\n", cmd);
            up(&favc_dec_mutex);
			return -6;
       	break;
       	
    }
	up(&favc_dec_mutex);
    return 0;
}

int ivs_mmap(struct file *file, struct vm_area_struct *vma)
{	
 	int size = vma->vm_end - vma->vm_start;
	vma->vm_pgoff = ivs_phy >> PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;
		
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
		kfree( (void*)ivs_virt ) ;
		return -EAGAIN;
	}
	return 0;
}

int ivs_open(struct inode *inode, struct file *filp)
{
 	int idx = 0;
    struct dec_private * decp;

	if((ivs_idx & IVS_IDX_FULL) == IVS_IDX_FULL)  {
		printk("Decoder Device Service Full,0x%x!\n",ivs_idx);
		return -EFAULT;
	}

    down(&favc_dec_mutex);
	for (idx = 0; idx < MAX_IVS_NUM; idx ++) {
		if( (ivs_idx&IVS_IDX_MASK_n(idx)) == 0) {
			ivs_idx |= IVS_IDX_MASK_n(idx);
			break;
		}
	}
    up(&favc_dec_mutex);

    decp = (struct dec_private *)&dec_data[idx];
    memset(decp,0,sizeof(struct dec_private));

    decp->dev = idx;
    filp->private_data = decp;
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
        MOD_INC_USE_COUNT;
    #endif
  
    return 0; /* success */
}

int ivs_release(struct inode *inode, struct file *filp)
{
	struct dec_private *decp = filp->private_data ;
	int dev = decp->dev;
 	down(&favc_dec_mutex);
	ivs_idx&=(~(IVS_IDX_MASK_n(dev)));
    up(&favc_dec_mutex);  
    return 0;
}

static struct file_operations ivs_fops = {
	.owner = THIS_MODULE,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
	.unlocked_ioctl = ivs_ioctl,
#else
    .ioctl          = ivs_ioctl,
#endif
    .mmap           = ivs_mmap,
    .open           = ivs_open,
    .release        = ivs_release,
};
    
static struct miscdevice ivs_dev = {
    .minor          = IVS_MINOR,
    .name           = "ivs",
    .fops           = &ivs_fops,
};

void ivs_isr(int irq, void *dev_id)
{	
	//	IVS	Interrupt flag reg
	volatile unsigned int *clear_itr = ivs_reg_base + (0x78 >> 2) ;
	
	//	clear IVS interrupt
	*clear_itr  = *clear_itr  | 1 ;

	isr_condition_flag = 1 ;
	wake_up(&ivs_queue);	

   
}

static irqreturn_t ivs_int_hd( int irq, void *dev_id)
{
	ivs_isr(irq, dev_id);
	return IRQ_HANDLED;
}

void enable_clk_and_stop_reset(void)
{
    volatile unsigned int *ivs_reg_rst;
    volatile unsigned int *ivs_reg_apb;
    volatile unsigned int *ivs_reg_axi;
    
    ivs_reg_rst  = clk_reg_base + (0xa0 >> 2);		
    ivs_reg_axi  = clk_reg_base + (0xb0 >> 2);
    ivs_reg_apb  = clk_reg_base + (0xbc >> 2);

	*ivs_reg_rst = *ivs_reg_rst | (1 << 14) ;
	*ivs_reg_axi = *ivs_reg_axi & (~(1<<5)) ;
	*ivs_reg_apb = *ivs_reg_apb & (~(1<<9)) ; 
}

int ivs_init_ioctl(void)
{	
	//  variable for pre-allocate memory
	char mem_name[16];
	struct frammap_buf_info binfo;
	unsigned int ret = 0;

	clk_reg_base = ioremap_nocache(0x99000000, PAGE_ALIGN(0x100000));
	if( clk_reg_base<=0 ) {
		printk("Fail to use ioremap_nocache, clk_reg_base = %x!\n", (unsigned int)clk_reg_base);
		goto init_clk_base_error;
	}

	ivs_reg_base = ioremap_nocache(0x9b700000, PAGE_ALIGN(0x100000));
	if( ivs_reg_base<=0 ) {
		printk("Fail to use ioremap_nocache, ivs_reg_base = %x!\n", (unsigned int)ivs_reg_base);
		goto init_ivs_base_error;
	}
	enable_clk_and_stop_reset();

	//  initital driver
	if( misc_register(&ivs_dev)<0) {
		printk("ivs misc-register fail");
		goto misc_register_fail;
	}
		
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
	sema_init( &favc_dec_mutex, 1 );
#else
	init_MUTEX( &favc_dec_mutex);
#endif


	if( request_irq( IVS_IRQ, ivs_int_hd, 0, "IVS IRQ", NULL) != 0){   
		printk("IVS interrupt request failed, IRQ=%d", IVS_IRQ);
		goto init_irq_fun_error;
	}
	
	init_waitqueue_head(&ivs_queue);


	//  pre-allocate memory
	strcpy( mem_name, "ivs_mem" ) ;
	binfo.size = PAGE_ALIGN(IMEM_SIZE);	
	binfo.align = 4096;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	binfo.name = mem_name;
	binfo.alloc_type = ALLOC_NONCACHABLE;
#endif
	ret = frm_get_buf_info(0, &binfo);
	if (ret == 0) {
		ivs_virt = (unsigned int)binfo.va_addr;
		ivs_phy = binfo.phy_addr;
	}
	else
		goto allocate_mem_error;

	return 0;

allocate_mem_error:
	free_irq(IVS_IRQ, NULL);
init_irq_fun_error:
	misc_deregister(&ivs_dev);
misc_register_fail:
	iounmap(ivs_reg_base);
init_ivs_base_error:
	iounmap(clk_reg_base);
init_clk_base_error:

	printk("init error\n");
	return -1;
}

/*	ivs_exit_ioctl : rmmod driver close function 
 *	
 *	Flowchart :
 *  1. release all resource (clk_reg_base, ivs_reg_base, ivs_dev, IVS_IRQ, memory)
*/

int ivs_exit_ioctl(void)
{
	iounmap(ivs_reg_base);
	misc_deregister(&ivs_dev);
	free_irq(IVS_IRQ, NULL);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))            
	frm_release_buf_info((void *)ivs_virt);
#else                   
{
	struct frammap_buf_info binfo;            
	binfo.va_addr = (unsigned int)ivs_virt;            
	binfo.phy_addr = (unsigned int)ivs_phy;            
	frm_release_buf_info(0, &binfo);
}
#endif
	printk("cleanup\n");
	return 0;
}
