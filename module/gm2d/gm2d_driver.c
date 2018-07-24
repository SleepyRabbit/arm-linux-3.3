/*
 * Revision History
 * VER1.1 create
 * VER1.2 2014/2/22 support osg using gm2d for 8287
 *        2014/3/13 divid by 2 with heigh and insert dummy command between 
 *        blit commad 
 * VER1.3 2014/3/27 support remainder cmd fire function 
 * VER1.4 2014/3/28 add more debug message 
 * VER1.5 2014/4/03 add util proc function
 * VER1.5 2014/4/07 modify util proc function
 * VER1.6 2014/7/09 add 8287 e version osg solution (unlock bus)
 * VER1.7 2014/7/30 add proc for unlock and lock bus flag and checkin
 * VER1.8 2014/8/04 add 8136 platform
 * VER1.9 2014/8/27 add WORAROUND (in init pmu to reset IP in gm8287.c)
 * VER2.0 2014/8/29 add support 8136 
 * VER2.1 2014/9/3  doesn't init pmu in gm8139 , add osg support unlock 29 bit 
 *                  in 8136 
 * VER2.2 2014/9/5  fix app ctl-c cause AHB error gm2d_file_release() 
 * VER2.3 2014/9/11 add support 8135 for unlock bus for osg 
 * VER2.4 2014/9/24 e ver and 8136 & 8135 use compiler flag
 * VER2.5 2015/2/10 support RGB1555 color mode for gm8136 only
 * VER2.6 2015/3/27 support LCD shift blit for gm8136 only
 * VER2.7 2015/3/31 change LCD shift blit to UYVU TO RGB for gm8136 only
 */

#include <linux/param.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
// NISH20120523 disable  //#include <video/gm2dlcd.h>
#include <linux/interrupt.h> 	// NISH20120523
#include <linux/version.h>		// NISH20120523
#include <linux/proc_fs.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
	#include <linux/platform_device.h>
	#include <mach/platform/platform_io.h>
	//#include <asm/dma-mapping.h>
	//#include <mach/platform/spec.h>
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
	#include <linux/device.h>
	#include <asm/arch/fmem.h>
	#include <asm/arch/platform/spec.h>
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
#include <linux/io.h>
#include <linux/irq.h>
#else
#include <asm/io.h>
#endif

#include "gm2d_driver.h"
#include "gm2d_platform.h"

#ifdef GM2D_OSG_USING
#include "gm2d/gm2d_osg_if.h"
#include <linux/semaphore.h>
#endif

#include <mach/ftpmu010.h>

extern int platform_init(unsigned short pwm_v);
extern void platform_exit(void);
extern void platform_power_save_mode(int is_enable);

/* proc init.
 */
static struct proc_dir_entry *gm2d_proc_root = NULL;
static struct proc_dir_entry *gm2d_share_bufer_info = NULL;
static struct proc_dir_entry *gm2d_cmd_bufer_info = NULL;


static DECLARE_WAIT_QUEUE_HEAD(wait_idle);
struct gm2d_dev_data* g_p_gm2d_dev = NULL;
static int g_dump_dbg_flag = 0;

#ifdef GM2D_OSG_USING

struct gm2d_osg_dev_data{
    unsigned int osg_driver_use;
    struct frammap_buf_info gm2d_osg_buf_info;
    struct gm2d_shared   *gm2d_osg_share ;
};
/*
 * utilization structure
 */
typedef struct gm2d_eng_util
{
	unsigned int utilization_period; //5sec calculation
	unsigned int utilization_period_record;
	unsigned int utilization_start;
	unsigned int utilization_record;
	unsigned int engine_start;
	unsigned int engine_end;
	unsigned int engine_time;
    unsigned int engine_time_record;
}gm2d_util_t;

gm2d_util_t g_utilization;

#define ENGINE_START 	    g_utilization.engine_start
#define ENGINE_END 		    g_utilization.engine_end
#define ENGINE_TIME	        g_utilization.engine_time
#define ENGINE_TIME_RECORD 	g_utilization.engine_time_record

#define UTIL_PERIOD_RECORD  g_utilization.utilization_period_record
#define UTIL_PERIOD 	    g_utilization.utilization_period
#define UTIL_START		    g_utilization.utilization_start
#define UTIL_RECORD 	    g_utilization.utilization_record

static int g_command_owner_key = 0;

static DECLARE_WAIT_QUEUE_HEAD(osg_waitidle);

struct semaphore    osg_fire_sem; 
static int g_dump_osg_flag = 0;
struct gm2d_osg_dev_data*  g_gm2d_osg_devdata = NULL;

static struct proc_dir_entry *gm2d_osgshare_bufer_info = NULL;
static struct proc_dir_entry *gm2d_osgcmd_bufer_info = NULL;
static struct proc_dir_entry *gm2d_osg_util_info = NULL;
static struct proc_dir_entry *gm2d_osg_lock_info = NULL;

#define GM2D_OSG_LOCK_MUTEX          down_timeout(&osg_fire_sem,10*HZ)  
#define GM2D_OSG_UNLOCK_MUTEX        up(&osg_fire_sem)  
#define GM2D_OSG_CMD_IS_ENOUGH(m ,n) (((m + n) > GM2D_CWORDS) ? 0 : 1) 

#define DECLARE_OSG_SHARE struct gm2d_shared* osg_pshared = g_gm2d_osg_devdata->gm2d_osg_share
#define DECLARE_OSG_CMD unsigned int *osg_cmd_list =(unsigned int*)&osg_pshared->cbuffer[osg_pshared->sw_buffer]
#define init_MUTEX(sema) sema_init(sema, 1)
#define GM2D_BLIT_CMD_NUM        26
#define GM2D_FILLRECT_CMD_NUM    22
#define GM2D_DRAWRECT_CMD_NUM    40
#define GM2D_XYTOREG(x, y) ((y) << 16 | ((x) & 0xffff))


#define GM2D_OSG_SET_OSG_OWER_KEY        (g_command_owner_key=1)
#define GM2D_OSG_SET_NORMAL_OWER_KEY     (g_command_owner_key=0)
#define GM2D_OSG_GET_OWER_KEY            (g_command_owner_key)
#define GM2D_OSG_SRCCOPY                 (0x01 << 16 | 0x02)
#define GM2D_OSG_BLIT_CLIP_V              2
#define GM2D_SRCALPHA_MODE               (0x06 << 16 | 0x02)

#endif

#ifdef GM2D_LCD_BLIT_USING

#define GM2D_LCD_CMD_IS_ENOUGH(m ,n) (((m + n) > GM2D_CWORDS) ? 0 : 1) 

struct gm2d_lcd_dev_data{
    unsigned int lcd_driver_use;
    struct frammap_buf_info gm2d_lcd_buf_info;
    struct gm2d_shared   *gm2d_lcd_share ;
};

static int g_dump_lcd_flag = 0;
struct gm2d_lcd_dev_data*  g_gm2d_lcd_devdata = NULL;
static struct proc_dir_entry *gm2d_lcdshare_bufer_info = NULL;
static struct proc_dir_entry *gm2d_lcdcmd_bufer_info = NULL;

static DECLARE_WAIT_QUEUE_HEAD(lcdblit_waitidle);
#define GM2D_OSG_SET_LCD_OWER_KEY        (g_command_owner_key=2)
#define GM2D_LCDBLIT_CMD_NUM             26
#endif
/*
 * Module Parameter
 */
static unsigned short pwm = 0xFFFF;
module_param(pwm, ushort, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pwm, "gm2d PWM Value");

static int g_e_ver_8287 = 0;

#ifdef GM2D_OSG_USING
static int g_bus_lock_flag = 0;
#endif

#define GM2D_VERSION "GM2D VER:2.7"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
static unsigned int gm2d_va_to_pa(unsigned int addr) 
{
    return (unsigned int)frm_va2pa(addr);
}
#else
static unsigned int gm2d_va_to_pa(unsigned int addr) 
{    
	pmd_t *pmd;    
	pte_t *pte;    
	pgd_t *pgd;    
    
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
    pud_t *pud;
    #endif
    
	pgd = pgd_offset(current->mm, addr);    
	if (!pgd_none(*pgd)) {       
        #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
        pud = pud_offset(pgd, addr);
        pmd = pmd_offset(pud, addr);     
        #else
        pmd = pmd_offset(pgd, addr);     
        #endif
        
	    if (!pmd_none(*pmd)) {            
	        pte = pte_offset_map(pmd, addr);          
	        if (!pmd_none(*pmd))            
	        {     
	            pte = pte_offset_map(pmd, addr);                
	            if (pte_present(*pte))  
	                return (unsigned int)page_address(pte_page(*pte)) + (addr & (PAGE_SIZE - 1)) - PAGE_OFFSET;                
	             
	        }           
	    }      
	}  
    PRINT_E("%s can't find paddr 0x%08X\n", __FUNCTION__, addr);
	return 0;
}
#endif
#if 0/*debug command*/
static void drv_clear_registers(struct gm2d_drv_data* p_drv_data, struct gm2d_dev_data* p_dev_data)
{
	unsigned int i = 0;

	printk("\nclear reg:\n");
	for(i = 0; i < (GM2D_SHADERCTRL+1); i += 4)
	{
		gm2d_writel(i, 0);
	}
	printk("\n");
}

static void drv_dump_registers(struct gm2d_drv_data* p_drv_data, struct gm2d_dev_data* p_dev_data)
{
	unsigned int i = 0;

	printk("\nreg:\n");
	for(i = 0; i < (GM2D_SHADERCTRL+1); i += 4)
	{
	    #if 0
		printk("0x%02X 0x%08X\t", i, gm2d_readl(i));
		if(((i/4) % 4) ==  3)
		{
			printk("\n");
		}
		#else
		printk("0x%02X 0x%08X\n", i, gm2d_readl(i));
		#endif
	}
	printk("GM2D_CMDADDR %08X GM2D_CMDSIZE %08X\n", 
		gm2d_readl(GM2D_CMDADDR),
		gm2d_readl(GM2D_CMDSIZE)
	);
	printk("\n");
}
#endif

static void drv_dump_buffer(unsigned int addr, int size)
{
	unsigned int i = 0;
    unsigned int* ptr = (unsigned int*)addr;
	unsigned int paddr = gm2d_va_to_pa(addr);

	
	printk("\ndrv buffer: vir %08X phy %08X\n", addr, paddr);
	for(i = 0; i < size; i++)
	{
	    #if 1
		printk("0x%08X 0x%08X\t", paddr+i*4, *ptr);
		#else
		printk("0x%02X 0x%08X\t", i, *ptr);
		#endif
		ptr++;
		if((i % 2) ==  1)
		{
			printk("\n");
		}
	}
	printk("\n");

}


#ifdef GM2D_OSG_USING

static void gm2d_mark_engine_start(void)
{
    ENGINE_START = jiffies;
    ENGINE_END = 0;
    if(UTIL_START == 0)
	{
        UTIL_START = ENGINE_START;
        ENGINE_TIME = 0;
    }
}

static void gm2d_mark_engine_end(void)
{
    unsigned int utilization = 0;
    
    ENGINE_END = jiffies;

    if (ENGINE_END > ENGINE_START)
        ENGINE_TIME += ENGINE_END - ENGINE_START;

    if (UTIL_START > ENGINE_END) {
        UTIL_START = 0;
        ENGINE_TIME = 0;
    } else if ((UTIL_START <= ENGINE_END) && ((ENGINE_END - UTIL_START) >= (UTIL_PERIOD * HZ))) {
        
		if ((ENGINE_END - UTIL_START) == 0)
		{
			//printk("div by 0!!\n");
		}
		else
		{
        	utilization = (unsigned int)((100*ENGINE_TIME) / (ENGINE_END - UTIL_START));
            //printk("utilization=%d,%d-%d-%d\n",utilization,ENGINE_TIME,ENGINE_END,UTIL_START);
            ENGINE_TIME_RECORD = ENGINE_TIME;
            UTIL_PERIOD_RECORD = ENGINE_END - UTIL_START;   
        }
        if (utilization)
            UTIL_RECORD = utilization;
        UTIL_START = 0;
        ENGINE_TIME= 0;

    }
    ENGINE_START=0;
}

static int proc_set_osgdbg_flag(struct file *file, const char *buffer,unsigned long count, void *data)
{
    int mode_set = 0;
    sscanf(buffer, "%d", &mode_set);
    g_dump_osg_flag = mode_set;
    //printk("g_dump_dbg_flag =%d \n", g_dump_dbg_flag);
    return count;
}

static int proc_read_osgdbg_flag(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    return sprintf(page, "g_dump_osg_flag =%d\n", g_dump_osg_flag);
}

static int proc_dump_osgshare_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
  	struct gm2d_shared *  tmp_share;
	int len = 0;

	tmp_share = g_gm2d_osg_devdata->gm2d_osg_share;

	len += sprintf(page + len, "osg hw_running: %d \n", tmp_share->hw_running);
    len += sprintf(page + len, "osg sw_buffer_idx: %d \n", tmp_share->sw_buffer);
    len += sprintf(page + len, "osg sw_ready: %d \n", tmp_share->sw_ready);
    len += sprintf(page + len, "osg sw_now: 0x%08x \n", (unsigned int)tmp_share->sw_now);
    len += sprintf(page + len, "osg sw_preparing: %d \n", tmp_share->sw_preparing);
    len += sprintf(page + len, "osg num_starts: %d \n", tmp_share->num_starts);
    len += sprintf(page + len, "osg num_interrupts: %d \n", tmp_share->num_interrupts);
    len += sprintf(page + len, "osg num_waits: %d \n", tmp_share->num_waits);
    len += sprintf(page + len, "osg commandbuf-1: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[0]);
    len += sprintf(page + len, "osg commandbuf-2: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[1]);
    len += sprintf(page + len, "osg use driver : %d \n",g_gm2d_osg_devdata->osg_driver_use );

    return len;
}

int gm2d_osg_emmit_cmd(void)
{
    //unsigned int stime,etime,difftime;
    struct gm2d_dev_data* p_dev_data = g_p_gm2d_dev;
    unsigned int hw_ready;
	unsigned int cmd_addr = 0; 
    int ret = 0;
    DECLARE_OSG_SHARE; 

    //stime =  jiffies;
  

    if((ret = GM2D_OSG_LOCK_MUTEX) < 0){
        printk("osg emit error, wait resource timeout(%d)\n",ret);
		return -1;
    }
    gm2d_mark_engine_start();
    //platform_power_save_mode(0);
    
    if (osg_pshared->hw_running || gm2d_readl(GM2D_STATUS) )
    {
        printk("%s list hw=%x state=%x\n",__FUNCTION__,osg_pshared->hw_running, gm2d_readl(GM2D_STATUS));
        goto emit_err;
    }
    if(osg_pshared->sw_ready == 0)
    {
        printk("%s emit command size is zero(%d)\n",__FUNCTION__,osg_pshared->sw_ready);
        goto emit_err;
    }
   
    GM2D_OSG_SET_OSG_OWER_KEY;
    osg_pshared->num_starts++;

	/* signal hw_running */
	osg_pshared->hw_running = 1;

		/* switch buffers */
	osg_pshared->sw_buffer = 1 - osg_pshared->sw_buffer;

	/* empty current buffer */
	hw_ready = osg_pshared->sw_ready;
	osg_pshared->sw_ready = 0;

	/* start the hardware; GM2D_INTERRUPT bit 31 enables generation of edge triggered interrupt */
	/*dump buffer*/
	if(g_dump_osg_flag)
	    drv_dump_buffer((unsigned int)osg_pshared->cbuffer[1 - osg_pshared->sw_buffer],hw_ready);

	cmd_addr = gm2d_va_to_pa((unsigned int)osg_pshared->cbuffer[1 - osg_pshared->sw_buffer]); 
		
	#if (LEVEL_TRIGGERED == 1)
		gm2d_writel(GM2D_INTERRUPT, (0 << 31) | 0x2);
	#else
		gm2d_writel(GM2D_INTERRUPT, (1 << 31) | 0x2);
	#endif

    if(g_dump_osg_flag)
        printk("\nIssue cmd_addr=%x , size=%d\n",cmd_addr,hw_ready);

	gm2d_writel(GM2D_CMDADDR, cmd_addr); 
	gm2d_writel(GM2D_CMDSIZE, hw_ready);   
    
    ret = wait_event_interruptible_timeout(osg_waitidle, !gm2d_readl(GM2D_STATUS), 10 * HZ);
	ret = (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
	if(g_dump_osg_flag)
	    printk("\n*****Emmit GM2D_WAIT_IDLE***** %d 0x%08X\n", ret, gm2d_readl( GM2D_STATUS));
    gm2d_mark_engine_end();
    //etime =  jiffies;
    //difftime = (etime >= stime) ? (etime - stime) : (0xffffffff - stime + etime);
    //printk("gm2d osg blit sjiff =%u ejiff =%u difftime=%u\n",stime,etime,difftime);
    //GM2D_OSG_UNLOCK_MUTEX;
    //platform_power_save_mode(1);
    return ret;
emit_err:
    GM2D_OSG_UNLOCK_MUTEX;
    //platform_power_save_mode(1);
    return -1;
}

int gm2d_osg_remainder_fire(void)
{
    int ret = 0;
    DECLARE_OSG_SHARE;

    if(osg_pshared->sw_ready == 0)
        return 0;
    if((ret = gm2d_osg_emmit_cmd())< 0 ){
        if(printk_ratelimit())
            printk("gm2d_osg_remainder_fire fail(%d)(%d)\n",__LINE__,ret);
        return -1;
    }
    return 0;
}
EXPORT_SYMBOL(gm2d_osg_remainder_fire);
int gm2d_osg_do_8136_blit(gm2d_osg_blit_t* blit_pam,int fire)
{
    DECLARE_OSG_SHARE; 
    unsigned int *osg_cmd_list ;
    //unsigned int clip_num = 0,i,src_paddr,dy;
    //int          cmdset = 0;
    int          ret = 0;
   
    if(!blit_pam->dst_paddr || !blit_pam->dst_w|| !blit_pam->dst_h 
       ||!blit_pam->src_paddr || !blit_pam->src_w ||!blit_pam->src_h){
        printk("%s invalide parameter dst w-h-pa(%d-%d-%x) src w-h-pa(%d-%d-%x)\n"
               ,__func__,blit_pam->dst_w,blit_pam->dst_h,blit_pam->dst_paddr
               ,blit_pam->src_w,blit_pam->src_h,blit_pam->src_paddr);
        return  -1;            
    }

   
    if(!GM2D_OSG_CMD_IS_ENOUGH(osg_pshared->sw_ready , GM2D_BLIT_CMD_NUM)){
        if(gm2d_osg_emmit_cmd()< 0 ){
            printk("gm2d_osg_emmit_cmd fail(%d)\n",__LINE__);
            return -1;
        }        
    }
  
    g_gm2d_osg_devdata->osg_driver_use = 1;
    platform_power_save_mode(0);
    
    osg_cmd_list =(unsigned int*)&osg_pshared->cbuffer[osg_pshared->sw_buffer];  

    /*DESTINATION SETTING*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
    osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->dst_paddr;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
    osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->dst_w * 2) & 0xffff);
    /*alway RGB5650 mode*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w ,blit_pam->dst_h);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_MODE;
    //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
    if(g_bus_lock_flag)
        osg_cmd_list[osg_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (1 << 29) | GM2D_UYVY; 
    else 
        osg_cmd_list[osg_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (0 << 29) | GM2D_UYVY;  
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BLEND;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
    /*SET CLIP*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMIN;;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMAX;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w,blit_pam->dst_h);
    /*SOURCE SETTING*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCSTRIDE;
    osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->src_w*2) & 0xffff);
    /*alway RGB5650 mode*/
    /*SOURCE COLORKEY */
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCCOLORKEY;;
    osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->src_colorkey;
    /*GM2D_SWAP_YUV*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_SWAP_YUV;    
    osg_cmd_list[osg_pshared->sw_ready++] = 0x1<<31|0x1<<0 |0x1<<1;// | 0x1 <<24 ;//|0x1<<18 |0x1<<19 | 0x1 <<25 ;
    /*blit use src colorkey*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCADDR;
    osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->src_paddr;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCRESOL;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->src_w,blit_pam->src_h);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_DSTADDR;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dx, blit_pam->dy);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_BLIT_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = (1 << 27) | GM2D_RGBA5650;
 
    
    if(fire){
         if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }   
    }
   
    return 0;
}

int gm2d_osg_do_blit(gm2d_osg_blit_t* blit_pam,int fire)
{
#if defined(CONFIG_PLATFORM_GM8136)
    return gm2d_osg_do_8136_blit(blit_pam,fire);
#else
    DECLARE_OSG_SHARE; 
    unsigned int *osg_cmd_list ;
    unsigned int clip_num = 0,i,src_paddr,dy;
    int          cmdset = 0;
    int          ret = 0;
   
    if(!blit_pam->dst_paddr || !blit_pam->dst_w|| !blit_pam->dst_h 
       ||!blit_pam->src_paddr || !blit_pam->src_w ||!blit_pam->src_h){
        printk("%s invalide parameter dst w-h-pa(%d-%d-%x) src w-h-pa(%d-%d-%x)\n"
               ,__func__,blit_pam->dst_w,blit_pam->dst_h,blit_pam->dst_paddr
               ,blit_pam->src_w,blit_pam->src_h,blit_pam->src_paddr);
        return  -1;            
    }

    if(blit_pam->src_h % GM2D_OSG_BLIT_CLIP_V !=0){
        printk("%s sourcd heigh isn't align 2(h=%d)\n",__func__,blit_pam->src_h );
        return  -1;        
    }

    if(g_e_ver_8287){
       if(!GM2D_OSG_CMD_IS_ENOUGH(osg_pshared->sw_ready , GM2D_BLIT_CMD_NUM)){
	       if(gm2d_osg_emmit_cmd()< 0 ){
               printk("gm2d_osg_emmit_cmd fail(%d)\n",__LINE__);
            return -1;
           }        
	   }
    }
    else{
        clip_num = blit_pam->src_h / GM2D_OSG_BLIT_CLIP_V;

        if(!GM2D_OSG_CMD_IS_ENOUGH(osg_pshared->sw_ready , (20+14*clip_num)))
        {
            if((ret = gm2d_osg_emmit_cmd())< 0 ){
                if(printk_ratelimit())
                    printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
                return -1;
            }      
        }
    }
    g_gm2d_osg_devdata->osg_driver_use = 1;
    platform_power_save_mode(0);
    
    osg_cmd_list =(unsigned int*)&osg_pshared->cbuffer[osg_pshared->sw_buffer];   

    if(g_e_ver_8287){
        /*DESTINATION SETTING*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
        osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->dst_paddr;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
        osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->dst_w * 2) & 0xffff);
        /*alway RGB5650 mode*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w ,blit_pam->dst_h);
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_MODE;
        //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
        if(g_bus_lock_flag)
            osg_cmd_list[osg_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (1 << 29) | GM2D_RGBA5650; 
        else    
            osg_cmd_list[osg_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (0 << 29) | GM2D_RGBA5650;  
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BLEND;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
        /*SET CLIP*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMIN;;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMAX;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w,blit_pam->dst_h);
        /*SOURCE SETTING*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCSTRIDE;
        osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->src_w*2) & 0xffff);
        /*alway RGB5650 mode*/
        /*SOURCE COLORKEY */
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCCOLORKEY;;
        osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->src_colorkey;
        /*blit use src colorkey*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCADDR;
        osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->src_paddr;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCRESOL;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->src_w,blit_pam->src_h);
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_DSTADDR;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dx, blit_pam->dy);
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_BLIT_CMD;
        osg_cmd_list[osg_pshared->sw_ready++] = (1 << 27) | GM2D_RGBA5650;
    }
    else{
        /*DESTINATION SETTING*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
    	osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->dst_paddr;
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
    	osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->dst_w * 2) & 0xffff);/*alway RGB5650 mode*/
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w ,blit_pam->dst_h);
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_MODE;
        //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
        osg_cmd_list[osg_pshared->sw_ready++] = (1 << 31) | (1 << 23) | (0 << 29) | GM2D_RGBA5650;  
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BLEND;
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
        /*SET CLIP*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMIN;;
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMAX;
    	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_w,blit_pam->dst_h);
        /*SOURCE SETTING*/
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCSTRIDE;
    	osg_cmd_list[osg_pshared->sw_ready++] = ((blit_pam->src_w*2) & 0xffff); /*alway RGB5650 mode*/
        /*SOURCE COLORKEY */
        osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCCOLORKEY;;
    	osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->src_colorkey;
        /*blit use src colorkey*/
        

        for(i = 0 ; i < clip_num ; i++){

            dy =  GM2D_OSG_BLIT_CLIP_V*i ;    
            src_paddr = blit_pam->src_paddr + dy*(blit_pam->src_w*2);     

            osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
    	    osg_cmd_list[osg_pshared->sw_ready++] = blit_pam->dst_paddr;     
            osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCADDR;
        	osg_cmd_list[osg_pshared->sw_ready++] = src_paddr;//blit_pam->src_paddr;
        	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_SRCRESOL;
            osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->src_w,GM2D_OSG_BLIT_CLIP_V);
        	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_BLIT_DSTADDR;
        	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dx,blit_pam->dy+dy);
        	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_BLIT_CMD;
        	osg_cmd_list[osg_pshared->sw_ready++] = (1 << 27) | GM2D_RGBA5650;

            /*dummy fillrect command ,let AHB bus can be free for other user*/
            /*Because Blit will lock AHB bus*/
            osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
    	    osg_cmd_list[osg_pshared->sw_ready++] = 0xf0000000;/*no exist memory*/
    	    if(cmdset==0){
                cmdset= 1;
                osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
                osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->src_w,4);
            }
            osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
        	osg_cmd_list[osg_pshared->sw_ready++] = 2;
            
        }
    }
    
    if(fire){
         if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }   
    }
   
    return 0;
#endif    
}
EXPORT_SYMBOL(gm2d_osg_do_blit);

int gm2d_osg_fillrect(gm2d_osg_mask_t* mask_pam,int fire)
{
    DECLARE_OSG_SHARE; 
    unsigned int *osg_cmd_list ;
    int ret =0;
    
    if(!GM2D_OSG_CMD_IS_ENOUGH(osg_pshared->sw_ready , GM2D_FILLRECT_CMD_NUM)){
	   //DEBUG_OP("%s cmd is exceed then emit\n",__func__); 
        if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }
	}

    osg_cmd_list =(unsigned int*)&osg_pshared->cbuffer[osg_pshared->sw_buffer];
    /*DESTINATION SETTING*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
	osg_cmd_list[osg_pshared->sw_ready++] = mask_pam->dst_paddr;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
	osg_cmd_list[osg_pshared->sw_ready++] = ((mask_pam->dst_w * 2) & 0xffff);/*alway RGB5650 mode*/
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dst_w ,mask_pam->dst_h);
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_MODE;
    //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
    osg_cmd_list[osg_pshared->sw_ready++] = (1 << 31) | (1 << 23) | (0 << 29) | GM2D_RGBA5650;   
    /*SET CLIP*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMIN;;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMAX;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dst_w,mask_pam->dst_h);
    /*color*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_COLOR;;
	osg_cmd_list[osg_pshared->sw_ready++] = mask_pam->color;
    /*fillrect*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BLEND;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_STARTXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dx1,mask_pam->dy1);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dx2,mask_pam->dy2);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = 0x02;

    if(fire){
         if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }   
    }

    return 0;
}

int gm2d_osg_drawrect(gm2d_osg_mask_t* mask_pam,int fire)
{
    DECLARE_OSG_SHARE; 
    unsigned int *osg_cmd_list ;
    int       ret;
    
    int x1 = mask_pam->dx1;
    int y1 = mask_pam->dy1;
    int x2 = mask_pam->dx2;
    int y2 = mask_pam->dy2;
    
    if(!GM2D_OSG_CMD_IS_ENOUGH(osg_pshared->sw_ready , GM2D_DRAWRECT_CMD_NUM)){
	   //DEBUG_OP("%s cmd is exceed then emit\n",__func__); 
        if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }
	}

    if(mask_pam->border_sz == 0 || (mask_pam->dx1 == mask_pam->dx2)
       || (mask_pam->dy1 == mask_pam->dy2) ){
        printk("%s invalide parameter x1-y1-x2-y2(%d-%d-%d-%d) sz=%d\n"
               ,__func__,mask_pam->dx1,mask_pam->dy1,mask_pam->dx2,mask_pam->dy2,mask_pam->border_sz);
        return  -1;            
    }

    osg_cmd_list =(unsigned int*)&osg_pshared->cbuffer[osg_pshared->sw_buffer];   
    
    /*DESTINATION SETTING*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BAR;
	osg_cmd_list[osg_pshared->sw_ready++] = mask_pam->dst_paddr;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
	osg_cmd_list[osg_pshared->sw_ready++] = ((mask_pam->dst_w * 2) & 0xffff);/*alway RGB5650 mode*/
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dst_w ,mask_pam->dst_h);
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_MODE;
    //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
    osg_cmd_list[osg_pshared->sw_ready++] = (1 << 31) | (1 << 23) | (0 << 29) | GM2D_RGBA5650;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_TARGET_BLEND;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
    /*SET CLIP*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMIN;;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CLIPMAX;
	osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(mask_pam->dst_w,mask_pam->dst_h);
    /*color*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_COLOR;;
	osg_cmd_list[osg_pshared->sw_ready++] = mask_pam->color;
    /*fillrect for 4*/
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_STARTXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x1, y1);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x1+mask_pam->border_sz, y2);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = 0x02;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_STARTXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x1, y2);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x2, y2-mask_pam->border_sz);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = 0x02;         
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_STARTXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x1, y1);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x2, y1+mask_pam->border_sz);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = 0x02;         
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_STARTXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x2, y1);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_DRAW_ENDXY;
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_XYTOREG(x2-mask_pam->border_sz, y2);
    osg_cmd_list[osg_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_DRAW_CMD;
    osg_cmd_list[osg_pshared->sw_ready++] = 0x02;

    if(fire){
         if((ret = gm2d_osg_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_osg_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }   
    }

    return 0;

}


int gm2d_osg_do_mask(gm2d_osg_mask_t* mask_pam,int fire)
{
  
    if(!mask_pam->dst_paddr || !mask_pam->dst_w|| !mask_pam->dst_h        
       || (mask_pam->type != GM2D_0SG_BORDER_TURE&&mask_pam->type != GM2D_0SG_BORDER_HORROW )){
            printk("%s invalide parameter dst w-h-pa(%d-%d-%x) type(%d)\n"
                   ,__func__,mask_pam->dst_w,mask_pam->dst_h,mask_pam->dst_paddr,mask_pam->type);
            return  -1;            
    }

    platform_power_save_mode(0);

    if(mask_pam->type == GM2D_0SG_BORDER_TURE){
        if(gm2d_osg_fillrect(mask_pam,fire) < 0){
            printk("gm2d_osg_fillrect fail\n");
            return  -1;                
        }
    }

    
    if(mask_pam->type == GM2D_0SG_BORDER_HORROW){
        if(gm2d_osg_drawrect(mask_pam,fire) < 0){
            printk("gm2d_osg_drawrect\n");
            return  -1;                
        }
    }

    return 0;

}
EXPORT_SYMBOL(gm2d_osg_do_mask);

static int proc_osgwrite_util_info(struct file *file, const char *buffer,unsigned long count, void *data)
{
    int mode_set=0;
    sscanf(buffer, "%d", &mode_set);
    UTIL_PERIOD = mode_set;
    printk("\nGM2D Utilization Period =%d(sec)\n",  UTIL_PERIOD);
	
    return count;
}

static int proc_osgread_util_info(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

    len += sprintf(page + len, "\nGM2D Period=%d(ms) consume time=%d(ms)\n",
                   (UTIL_PERIOD_RECORD*1000/HZ),(ENGINE_TIME_RECORD*1000/HZ));   

    *eof = 1;
    return len;
}

static int proc_osgwrite_lock(struct file *file, const char *buffer,unsigned long count, void *data)
{
    int mode_set=0;
    sscanf(buffer, "%d", &mode_set);
    
    if(mode_set !=0 && mode_set !=1){
        printk("\nGM2D bus lock invalid value(%d)\n",  mode_set);
        return count;
    }
    g_bus_lock_flag = mode_set;
  
    printk("\nGM2D bus lock =%d\n",  g_bus_lock_flag);
	
    return count;
}

static int proc_osgread_lock(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

    len += sprintf(page + len, "\nGM2D bus lock =%d\n",g_bus_lock_flag);   

    *eof = 1;
    return len;
}

#endif
#if defined(GM2D_LCD_BLIT_USING)
static int proc_set_lcddbg_flag(struct file *file, const char *buffer,unsigned long count, void *data)
{
    int mode_set = 0;
    sscanf(buffer, "%d", &mode_set);
    g_dump_lcd_flag = mode_set;
    //printk("g_dump_dbg_flag =%d \n", g_dump_dbg_flag);
    return count;
}

static int proc_read_lcddbg_flag(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    return sprintf(page, "g_dump_lcdblit_flag =%d\n", g_dump_lcd_flag);
}

static int proc_dump_lcdshare_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
  	struct gm2d_shared *  tmp_share;
	int len = 0;

	tmp_share = g_gm2d_lcd_devdata->gm2d_lcd_share;

	len += sprintf(page + len, "lcdblit hw_running: %d \n", tmp_share->hw_running);
    len += sprintf(page + len, "lcdblit sw_buffer_idx: %d \n", tmp_share->sw_buffer);
    len += sprintf(page + len, "lcdblit sw_ready: %d \n", tmp_share->sw_ready);
    len += sprintf(page + len, "lcdblit sw_now: 0x%08x \n", (unsigned int)tmp_share->sw_now);
    len += sprintf(page + len, "lcdblit sw_preparing: %d \n", tmp_share->sw_preparing);
    len += sprintf(page + len, "lcdblit num_starts: %d \n", tmp_share->num_starts);
    len += sprintf(page + len, "lcdblit num_interrupts: %d \n", tmp_share->num_interrupts);
    len += sprintf(page + len, "lcdblit num_waits: %d \n", tmp_share->num_waits);
    len += sprintf(page + len, "lcdblit commandbuf-1: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[0]);
    len += sprintf(page + len, "lcdblit commandbuf-2: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[1]);
    //len += sprintf(page + len, "osg use driver : %d \n",g_gm2d_osg_devdata->osg_driver_use );

    return len;
}
int gm2d_lcdblit_sanity_check(gm2d_lcd_blit_t* blit_pam)
{
	if(!blit_pam->dst_bg_paddr || !blit_pam->src_bg_paddr)
		return -1;
	
	if(blit_pam->dst_bg_w%2 || blit_pam->src_bg_w%2)
		return -1;

	if((blit_pam->src_clp_x + blit_pam->src_clp_w) > blit_pam->src_bg_w )
		return -1;

	if((blit_pam->src_clp_y + blit_pam->src_clp_h) > blit_pam->src_bg_h )
		return -1;

	if((blit_pam->dst_off_x + blit_pam->dst_off_w) > blit_pam->dst_bg_w )
		return -1;

	if((blit_pam->dst_off_y + blit_pam->dst_off_h) > blit_pam->dst_bg_h )
		return -1;

	if((blit_pam->dst_dx + blit_pam->src_clp_w) > blit_pam->dst_off_w )
		return -1;
	
	if((blit_pam->dst_dy + blit_pam->src_clp_h) > blit_pam->dst_off_h )
		return -1;
	
	return 0;
}

int gm2d_lcdblit_emmit_cmd(void)
{
    //unsigned int stime,etime,difftime;
    struct gm2d_dev_data* p_dev_data = g_p_gm2d_dev;
    struct gm2d_shared* lcd_pshared = g_gm2d_lcd_devdata->gm2d_lcd_share;
    unsigned int hw_ready;
	unsigned int cmd_addr = 0; 
    int ret = 0;
    DECLARE_OSG_SHARE; 

    //stime =  jiffies;
  

    if((ret = GM2D_OSG_LOCK_MUTEX) < 0){
        printk("osg emit error, wait resource timeout(%d)\n",ret);
		return -1;
    }
    //gm2d_mark_engine_start();
    //platform_power_save_mode(0);
    
    if (lcd_pshared->hw_running || gm2d_readl(GM2D_STATUS) )
    {
        printk("%s list hw=%x state=%x\n",__FUNCTION__,lcd_pshared->hw_running, gm2d_readl(GM2D_STATUS));
        goto lcdemit_err;
    }
    if(lcd_pshared->sw_ready == 0)
    {
        printk("%s emit command size is zero(%d)\n",__FUNCTION__,lcd_pshared->sw_ready);
        goto lcdemit_err;
    }
   
    GM2D_OSG_SET_LCD_OWER_KEY;
    osg_pshared->num_starts++;

	/* signal hw_running */
	lcd_pshared->hw_running = 1;

		/* switch buffers */
	lcd_pshared->sw_buffer = 1 - lcd_pshared->sw_buffer;

	/* empty current buffer */
	hw_ready = lcd_pshared->sw_ready;
	lcd_pshared->sw_ready = 0;

	/* start the hardware; GM2D_INTERRUPT bit 31 enables generation of edge triggered interrupt */
	/*dump buffer*/
	if(g_dump_lcd_flag)
	    drv_dump_buffer((unsigned int)lcd_pshared->cbuffer[1 - lcd_pshared->sw_buffer],hw_ready);

	cmd_addr = gm2d_va_to_pa((unsigned int)lcd_pshared->cbuffer[1 - lcd_pshared->sw_buffer]); 
		
	#if (LEVEL_TRIGGERED == 1)
		gm2d_writel(GM2D_INTERRUPT, (0 << 31) | 0x2);
	#else
		gm2d_writel(GM2D_INTERRUPT, (1 << 31) | 0x2);
	#endif

    if(g_dump_lcd_flag)
        printk("\nIssue cmd_addr=%x , size=%d\n",cmd_addr,hw_ready);

	gm2d_writel(GM2D_CMDADDR, cmd_addr); 
	gm2d_writel(GM2D_CMDSIZE, hw_ready);   
    
    ret = wait_event_interruptible_timeout(lcdblit_waitidle, !gm2d_readl(GM2D_STATUS), 10 * HZ);
	ret = (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
	if(g_dump_lcd_flag)
	    printk("\n*****Emmit GM2D_WAIT_IDLE***** %d 0x%08X\n", ret, gm2d_readl( GM2D_STATUS));
   
    return ret;
lcdemit_err:
    GM2D_OSG_UNLOCK_MUTEX;
    //platform_power_save_mode(1);
    return -1;
}
int gm2d_lcd_do_blit(gm2d_lcd_blit_t* blit_pam,int fire)
{
    struct gm2d_shared* lcd_pshared = g_gm2d_lcd_devdata->gm2d_lcd_share; 
    unsigned int *osg_cmd_list ;
	u32 dst_pa,src_pa;
	
    //unsigned int clip_num = 0,i,src_paddr,dy;
    //int          cmdset = 0;
    int          ret = 0;

   	if(blit_pam == NULL){
		printk("%s NULL parameter \n",__func__);
		return -1;
	}
    if(gm2d_lcdblit_sanity_check(blit_pam)<0){
        printk("%s invalide parameter dst w-h-pa(%d-%d-%x) dst blit(%d-%d-%d-%d) src w-h-pa(%d-%d-%x) src clip (%d-%d-%d-%d) dxt (%d-%d)\n"
               ,__func__,blit_pam->dst_bg_w,blit_pam->dst_bg_h,blit_pam->dst_bg_paddr,blit_pam->dst_off_x,blit_pam->dst_off_y
			   ,blit_pam->dst_off_w,blit_pam->dst_off_h,blit_pam->src_bg_w,blit_pam->src_bg_h,blit_pam->src_bg_paddr
			   ,blit_pam->src_clp_x,blit_pam->src_clp_y,blit_pam->src_clp_w,blit_pam->src_clp_h
			   ,blit_pam->dst_dx,blit_pam->dst_dy);
        return  -1;            
    }

   
    if(!GM2D_LCD_CMD_IS_ENOUGH(lcd_pshared->sw_ready , GM2D_LCDBLIT_CMD_NUM)){
        if(gm2d_lcdblit_emmit_cmd()< 0 ){
            printk("gm2d_lcdblit_emmit_cmd fail(%d)\n",__LINE__);
            return -1;
        }        
    }
  
    platform_power_save_mode(0);

	dst_pa =blit_pam->dst_bg_paddr+(blit_pam->dst_off_x*2)+(blit_pam->dst_off_y *blit_pam->dst_bg_w *2);
	src_pa =blit_pam->src_bg_paddr+(blit_pam->src_clp_x*2)+(blit_pam->src_clp_y *blit_pam->src_bg_w *2);;
	
    osg_cmd_list =(unsigned int*)&lcd_pshared->cbuffer[lcd_pshared->sw_buffer];  

    /*DESTINATION SETTING*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_TARGET_BAR;
    osg_cmd_list[lcd_pshared->sw_ready++] = dst_pa;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_TARGET_STRIDE;
    osg_cmd_list[lcd_pshared->sw_ready++] = (blit_pam->dst_bg_w*2) & 0xffff;
    /*alway YUV422 mode*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_TARGET_RESOLXY;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_off_w,blit_pam->dst_off_h);
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_TARGET_MODE;
    //IS:bit 29 disables 16->32 packing, bits 31/23 set on 32-bit mode
    if(g_bus_lock_flag)
        osg_cmd_list[lcd_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (1 << 29) | GM2D_RGBA5650;
    else
        osg_cmd_list[lcd_pshared->sw_ready++] = (0 << 31) | (0 << 23) | (0 << 29) | GM2D_RGBA5650;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_TARGET_BLEND;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_OSG_SRCCOPY;
    /*SET CLIP*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_CLIPMIN;;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_XYTOREG(0, 0);
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_CLIPMAX;
    osg_cmd_list[lcd_pshared->sw_ready++] =  GM2D_XYTOREG(blit_pam->dst_off_w,blit_pam->dst_off_h);//GM2D_XYTOREG(blit_pam->dst_w,blit_pam->dst_h);
    /*SOURCE SETTING*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_BLIT_SRCSTRIDE;
    osg_cmd_list[lcd_pshared->sw_ready++] = (blit_pam->src_bg_w*2) & 0xffff;   
    /*GM2D_SWAP_YUV*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_SWAP_YUV;
    osg_cmd_list[lcd_pshared->sw_ready++] = 0x1< 24 ;//DO SRC BIGEND    
    /*blit use src colorkey*/
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_BLIT_SRCADDR;
    osg_cmd_list[lcd_pshared->sw_ready++] = src_pa;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_BLIT_SRCRESOL;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->src_clp_w,blit_pam->src_clp_h);
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_BLIT_DSTADDR;
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_XYTOREG(blit_pam->dst_dx,blit_pam->dst_dy);
    osg_cmd_list[lcd_pshared->sw_ready++] = GM2D_CMDLIST_WAIT | GM2D_BLIT_CMD;
    osg_cmd_list[lcd_pshared->sw_ready++] = (1 << 27) | GM2D_UYVY;
 
    
    if(fire){
         if((ret = gm2d_lcdblit_emmit_cmd())< 0 ){
            if(printk_ratelimit())
                printk("gm2d_lcdblit_emmit_cmd fail(%d)(%d)\n",__LINE__,ret);
            return -1;
        }   
    }
   
    return 0;
}
EXPORT_SYMBOL(gm2d_lcd_do_blit);

#endif

static int proc_set_dbg_flag(struct file *file, const char *buffer,unsigned long count, void *data)
{
    int mode_set = 0;
    sscanf(buffer, "%d", &mode_set);
    g_dump_dbg_flag = mode_set;
    //printk("g_dump_dbg_flag =%d \n", g_dump_dbg_flag);
    return count;
}

static int proc_read_dbg_flag(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    return sprintf(page, "g_dump_dbg_flag =%d\n", g_dump_dbg_flag);
}

static int proc_dump_share_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct gm2d_shared *  tmp_share;
	struct gm2d_dev_data* tmp_dev ;
	int len = 0;

	tmp_dev = g_p_gm2d_dev;
	tmp_share = tmp_dev->p_shared;

	len += sprintf(page + len, "hw_running: %d \n", tmp_share->hw_running);
    len += sprintf(page + len, "sw_buffer_idx: %d \n", tmp_share->sw_buffer);
    len += sprintf(page + len, "sw_ready: %d \n", tmp_share->sw_ready);
    len += sprintf(page + len, "sw_now: 0x%08x \n", (unsigned int)tmp_share->sw_now);
    len += sprintf(page + len, "sw_preparing: %d \n", tmp_share->sw_preparing);
    len += sprintf(page + len, "num_starts: %d \n", tmp_share->num_starts);
    len += sprintf(page + len, "num_interrupts: %d \n", tmp_share->num_interrupts);
    len += sprintf(page + len, "num_waits: %d \n", tmp_share->num_waits);
    len += sprintf(page + len, "commandbuf-1: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[0]);
    len += sprintf(page + len, "commandbuf-2: 0x%08x \n", (unsigned int)&tmp_share->cbuffer[1]);
    len += sprintf(page + len, "driver use count: %d \n",  g_p_gm2d_dev->driver_count);

    return len;
}
#if 0/*debug used*/
static void drv_dump_shared(struct gm2d_drv_data* p_drv_data, struct gm2d_dev_data* p_dev_data)
{
	//int i = 0;
	//int size = sizeof(struct gm2d_shared) >> 2;
	//unsigned int* ptr = (unsigned int*)p_dev_data->p_shared;
	struct gm2d_shared* pshared = p_dev_data->p_shared;

	printk("\n");
	printk("**********************************************\n");
	printk("dump DRV shared\n");
	printk("shared V %08X %08X %08X\n",	
		(unsigned int)pshared, 
		(unsigned int)pshared->cbuffer[0], 
		(unsigned int)pshared->cbuffer[1]
	);
	printk("shared P %08X %08X %08X\n",	
		gm2d_va_to_pa((unsigned int)pshared), 
		gm2d_va_to_pa((unsigned int)pshared->cbuffer[0]), 
		gm2d_va_to_pa((unsigned int)pshared->cbuffer[1])
	);

	printk("sw_buffer \t%d\t",	pshared->sw_buffer);
	printk("sw_ready \t%d\t", pshared->sw_ready);
	printk("sw_preparing \t%d\t",	pshared->sw_preparing);
	printk("hw_running \t%d\n", pshared->hw_running);
	
	printk("num_starts \t%d\t",	pshared->num_starts);
	printk("num_interrupts \t%d\t",	pshared->num_interrupts);
	printk("num_waits \t%d\n",	pshared->num_waits);
	printk("buffer[0] 0x%08X\t", pshared->cbuffer[0][GM2D_CWORDS-4]);
	printk("buffer[1] 0x%08X\n", pshared->cbuffer[1][GM2D_CWORDS-4]);

	#if 0
	for(i = 0; i < size; i++)
	{
		printk("%04X %08X\t", i*4, *ptr);
		if( (i%8) == 7)
		{
			printk("\n");
		}
		ptr++;
	}
	#endif
	printk("**********************************************\n");
	printk("\n");
	
}
#endif
irqreturn_t gm2d_isr(int irq, unsigned int* ptr)
{
    //struct gm2d_drv_data *p_drv_data = NULL;
    struct gm2d_dev_data *p_dev_data = (struct gm2d_dev_data*)ptr;
	DECLARE_SHA_DATA;
    
#ifdef GM2D_OSG_USING    
    DECLARE_OSG_SHARE; 
#endif
#if defined(GM2D_LCD_BLIT_USING)
    struct gm2d_shared* lcd_pshared = g_gm2d_lcd_devdata->gm2d_lcd_share; 
#endif

	DEBUG_PROC_ENTRY;

	/* clear interrupt */
	#if (LEVEL_TRIGGERED == 1)
	gm2d_writel(GM2D_INTERRUPT, (0 << 31) | 0x2);
	#endif

    /* debug register and commands */
    #if 0
	printk("\n*****gm2d_interrupt*****\n");
    drv_dump_registers();
	
	drv_dump_buffer(
		(unsigned int)__va(gm2d_readl(gm2d_regs, GM2D_CMDADDR)),
		gm2d_readl(gm2d_regs, GM2D_CMDSIZE)
	);
	
	#endif
    
#ifdef GM2D_OSG_USING
    if(GM2D_OSG_GET_OWER_KEY == 0){
    	/* update stats */
    	pshared->num_interrupts++;

    	/* signal hardware not running */
    	pshared->hw_running = 0;

    	/* wake up any threads waiting for the hardware to finish */
    	wake_up_interruptible(&wait_idle);
    }
#if defined(GM2D_LCD_BLIT_USING)
    else if(GM2D_OSG_GET_OWER_KEY == 1){
        osg_pshared->num_interrupts++;

    	/* signal hardware not running */
    	osg_pshared->hw_running = 0;

    	/* wake up any threads waiting for the hardware to finish */
    	wake_up_interruptible(&osg_waitidle);
    }
    else{
        lcd_pshared->num_interrupts++;

    	/* signal hardware not running */
    	lcd_pshared->hw_running = 0;

    	/* wake up any threads waiting for the hardware to finish */
    	wake_up_interruptible(&lcdblit_waitidle);

    }

#else
    else {
        osg_pshared->num_interrupts++;

    	/* signal hardware not running */
    	osg_pshared->hw_running = 0;

    	/* wake up any threads waiting for the hardware to finish */
    	wake_up_interruptible(&osg_waitidle);
    }
#endif    
    GM2D_OSG_UNLOCK_MUTEX;
#else
    /* update stats */
	pshared->num_interrupts++;

	/* signal hardware not running */
	pshared->hw_running = 0;

	/* wake up any threads waiting for the hardware to finish */
	wake_up_interruptible(&wait_idle);
#endif
	return IRQ_HANDLED;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
irqreturn_t gm2d_interrupt(int irq, void *ptr)
#else
irqreturn_t gm2d_interrupt(int irq, void *ptr, struct pt_regs *dummy)
#endif
{
	return gm2d_isr(irq, (unsigned int *)ptr);
}

static int gm2d_file_open(struct inode *inode, struct file *filp)
{
    int ret_of_ft2dge_driver_open = 0;

    struct gm2d_drv_data* p_drv_data = NULL;
    struct gm2d_dev_data* p_dev_data = NULL;

    /* allocate mem for drv data */
    p_drv_data = kzalloc(sizeof(struct gm2d_drv_data), GFP_KERNEL);
    p_drv_data->p_dev_data = g_p_gm2d_dev;

    p_dev_data = g_p_gm2d_dev;

    /* increase driver count */	
    DRV_COUNT_INC(); 

    /* assign to private_data */
    filp->private_data = p_drv_data;	

    platform_power_save_mode(0);
    return ret_of_ft2dge_driver_open;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
static int gm2d_file_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#else
static long gm2d_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	int ret;
	unsigned int hw_ready;
	unsigned int cmd_addr = 0; 
    
    DECLARE_DRV_DATA;
    DECLARE_DEV_DATA;
    DECLARE_SHA_DATA;

	DEBUG_PROC_ENTRY;

	switch (cmd)
	{
	case GM2D_WAIT_IDLE:
		/* update stats */
		pshared->num_waits++;

		/* wait for the idle event and return 0 on success, error code on failure */
		{
			ret = wait_event_interruptible_timeout(wait_idle, !gm2d_readl(GM2D_STATUS), 10 * HZ);
			ret = (ret > 0) ? 0 : (ret < 0) ? ret : -ETIMEDOUT;
			if(g_dump_dbg_flag)
				printk("\n*****GM2D_WAIT_IDLE***** %d 0x%08X\n", ret, gm2d_readl( GM2D_STATUS));
            
		}
        //platform_power_save_mode(1);
		return ret;

	case GM2D_START_BLOCK:
		/* return immediately if there are no unprocessed words */
		if(g_dump_dbg_flag)
			printk("\nGM2D_START_BLOCK\n");

        //platform_power_save_mode(0);

		if (!pshared->sw_ready)
		{
			PRINT_E("GM2D_START_BLOCK error, zero sw_ready\n");
			return -1;
		}       
 
		#if 0
		printk("GM2D_START_BLOCK start\n");
		
		drv_dump_buffer(
			(unsigned int)shared->cbuffer[shared->sw_buffer],
			shared->sw_ready);
		#endif
        
        #ifdef GM2D_OSG_USING
        
        if((ret = GM2D_OSG_LOCK_MUTEX) < 0){
            PRINT_E("GM2D_START_BLOCK error, wait resource timeout(%d)\n",ret);
			return -1;
        }
            
        GM2D_OSG_SET_NORMAL_OWER_KEY;
        #endif
        
		/* update stats */
		pshared->num_starts++;

		/* signal hw_running */
		pshared->hw_running = 1;

		/* switch buffers */
		pshared->sw_buffer = 1 - pshared->sw_buffer;

		/* empty current buffer */
		hw_ready = pshared->sw_ready;
		pshared->sw_ready = 0;

		/* start the hardware; GM2D_INTERRUPT bit 31 enables generation of edge triggered interrupt */
		/*dump buffer*/
		if(g_dump_dbg_flag)
			drv_dump_buffer((unsigned int)pshared->cbuffer[1 - pshared->sw_buffer],hw_ready);

		cmd_addr = gm2d_va_to_pa((unsigned int)pshared->cbuffer[1 - pshared->sw_buffer]); 
		
		#if (LEVEL_TRIGGERED == 1)
		gm2d_writel(GM2D_INTERRUPT, (0 << 31) | 0x2);
		#else
		gm2d_writel(GM2D_INTERRUPT, (1 << 31) | 0x2);
		#endif
		if(g_dump_dbg_flag)
        	printk("\nIssue cmd_addr=%x , size=%d\n",cmd_addr,hw_ready);

		gm2d_writel(GM2D_CMDADDR, cmd_addr); 
		gm2d_writel(GM2D_CMDSIZE, hw_ready);

        
        #if PSEUDO_HARDWARE == 1
        gm2d_isr(THIKK2D_IRQ, (unsigned int*)p_dev_data);
        #endif 
        
        //GM2D_OSG_UNLOCK_MUTEX;
		return 0;
        
    case GM2D_RESET:
		{
			gm2d_writel(GM2D_STATUS, (1 << 31));
			
			do
			{
				PRINT_W("reset\n");
			}	
			while (gm2d_readl(GM2D_STATUS));
			return 0;
    	}
    
	case GM2D_PRINT_PHY:
		{
			unsigned int temp = (unsigned int)gm2d_va_to_pa((unsigned int)arg);
			
			PRINT_I("GM2D_PRINT_PHY %08X\n", temp);
			return 0;
		}
    
	default:
		PRINT_E("gm2d_ioctl: Unknown ioctl 0x%08x has been requested\n", cmd);
		return -EINVAL;
	}
}

static int gm2d_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned int size, gfx_size, io_size;
	int ret;
	//unsigned int* ptr = NULL;
    
    DECLARE_DRV_DATA;
    DECLARE_DEV_DATA;
    DECLARE_SHA_DATA;

	DEBUG_PROC_ENTRY;

	/* just allow mapping at offset 0 */
	if (vma->vm_pgoff)
		return -EINVAL;

	/* check size of requested mapping */
	size = vma->vm_end - vma->vm_start;
	gfx_size = PAGE_ALIGN(sizeof(struct gm2d_shared));
	io_size = PAGE_ALIGN(GM2D_REGFILE_SIZE);

	/* reject if size is not exactly what we expect */
	if (size != gfx_size + io_size)
		return -EINVAL;

	/* set reserved and I/O flag for the area */
	vma->vm_flags |= VM_RESERVED | VM_IO;

	/* select uncached access */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); //harry_test, enable

	/* map lower portion to shared data */
	if ((ret = remap_pfn_range(vma, vma->vm_start, gm2d_va_to_pa((unsigned int)pshared) >> PAGE_SHIFT, gfx_size, vma->vm_page_prot)))
	{
        PRINT_E("%s map data error\n",__FUNCTION__);
        goto ERR1;
    }		

	/* map upper portion to io registers */
	if ((ret = remap_pfn_range(vma, vma->vm_start + gfx_size, GM2D_PA_START >> PAGE_SHIFT, io_size, vma->vm_page_prot)))
	{
        PRINT_E("%s map io error\n",__FUNCTION__);
        goto ERR2;
	}

	#if 0
	ptr = (unsigned int*)vma->vm_start;
	printk("\ngm2d_mmap P %08X V %08X \n\n", 
		(unsigned int)gm2d_va_to_pa((unsigned int)shared),
		(unsigned int)vma->vm_start
		);
	
	printk("\ngm2d_mmap V %08X P %08X \n\n", (unsigned int)vma->vm_start, (unsigned int)gm2d_va_to_pa(vma->vm_start));
    #endif
    
	return 0;

    ERR2:
		do_munmap(vma->vm_mm, vma->vm_start, gfx_size);
    ERR1:
        return ret;
}
	


static int gm2d_file_release(struct inode *inode, struct file *filp)
{
    DECLARE_DRV_DATA;
    DECLARE_DEV_DATA;

	DEBUG_PROC_ENTRY;
    
    /* free mem for drv data */
    kfree(p_drv_data);
    
    DRV_COUNT_DEC();
    
/*if close clock when thin2d is running ,such like ctl-c to break APP    */
/*it will cause AHB bus error , Once if user or osg use it , it will open*/
/*until rmmod module. Only for gm8136                                    */
#if 0 
#ifdef GM2D_OSG_USING
    if(DRV_COUNT_GET() == 0 && g_gm2d_osg_devdata->osg_driver_use != 1){
        //platform_power_save_mode(1);
    }
#else
    if(DRV_COUNT_GET() == 0 ){
        //platform_power_save_mode(1);
    }
#endif
#endif
    return 0;
}





#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
int gm2d_drv_probe(struct device * dev)
#else
int gm2d_drv_probe(struct platform_device *pdev)
#endif
{
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
    struct platform_device* pdev = to_platform_device(dev); // for version under 2.6.24 
    #endif
    struct gm2d_dev_data *p_dev_data = NULL;
    //unsigned int chip_id = 0;
    int ret_of_gm2d_probe = 0;


    /* set drv data */
    p_dev_data = g_p_gm2d_dev;


    if (unlikely(p_dev_data == NULL)) 
    {
        printk("%s fails: kzalloc not OK", __FUNCTION__);
        goto err1;
    }
    
    p_dev_data->id = pdev->id;



    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    platform_set_drvdata(pdev, p_dev_data);
    #else
    dev_set_drvdata(dev, p_dev_data);
    #endif
    
   
    /* setup resource */
    p_dev_data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

    if (unlikely((!p_dev_data->res))) 
    {
        PRINT_E("%s fails: platform_get_resource not OK", __FUNCTION__);
        goto err2;
    }

    p_dev_data->irq_no = platform_get_irq(pdev, 0);
	
    if (unlikely(p_dev_data->irq_no < 0)) 
    {
        PRINT_E("%s fails: platform_get_irq not OK", __FUNCTION__);
        goto err2;
    }

    p_dev_data->mem_len = p_dev_data->res->end - p_dev_data->res->start + 1;
    p_dev_data->pbase = p_dev_data->res->start;

    if (unlikely(!request_mem_region(p_dev_data->res->start, p_dev_data->mem_len, pdev->name))) {
        PRINT_E("%s fails: request_mem_region not OK", __FUNCTION__);
        goto err2;
    }

    p_dev_data->vbase = (uint32_t) ioremap(p_dev_data->pbase, p_dev_data->mem_len);

    if (unlikely(p_dev_data->vbase == 0)) 
    {
        PRINT_E("%s fails: ioremap_nocache not OK", __FUNCTION__);
        goto err3;
    }
    

    ret_of_gm2d_probe = request_irq(
        p_dev_data->irq_no, 
        gm2d_interrupt, 
        #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
        SA_INTERRUPT, 
        #else
        0,
        #endif
        GM2D_DEV_NAME, 
        p_dev_data
    ); 

    if (unlikely(ret_of_gm2d_probe != 0)) 
    {
        PRINT_E("%s fails: request_irq not OK %d\n", __FUNCTION__, ret_of_gm2d_probe);
        goto err3;
    }
    
    if(unlikely(platform_init(pwm) < 0))
    {
        PRINT_E("%s platform init fail!\n", 
          __FUNCTION__);
        goto err3;
    }

#if defined(CONFIG_PLATFORM_GM8287)
    if(ftpmu010_get_attr(ATTR_TYPE_PMUVER) >= 0x03) /*E ver*/
        g_e_ver_8287 = 1;  
    else
        g_e_ver_8287 = 0;   
#elif defined(CONFIG_PLATFORM_GM8136)
        g_e_ver_8287 = 1;
#else
        g_e_ver_8287 = 0;
#endif

   
	/* check for gm2d accelerator presence ,8139 pmu is n't init , so don't*/
	/* check it                                                               */
#if !defined(CONFIG_PLATFORM_GM8139) 
    if (gm2d_readl(GM2D_IDREG) != GM2D_MAGIC)
    {
    	PRINT_E("%s checking magic...ERROR! 0x%08X\n", 
          __FUNCTION__, 
          gm2d_readl(GM2D_IDREG));
        //goto err3;

    }    
#endif

#if 0    
    /* init pmu */
    ret_of_gm2d_probe = gm2d_scu_probe(&p_dev_data->gm2d_scu);
    if (unlikely(ret_of_gm2d_probe != 0)) 
    {
        PRINT_E("%s fails: gm2d_scu_probe not OK %d\n", __FUNCTION__, ret_of_gm2d_probe);
        goto err3;
    }
#endif   
   
    #if 1
    PRINT_I("** %s ** %s done, vbase 0x%08X, pbase 0x%08X 0x%08X (%d)\n",
    GM2D_VERSION,
    __FUNCTION__, 
    p_dev_data->vbase, 
    p_dev_data->pbase, 
    (uint32_t)p_dev_data,
    g_e_ver_8287
    );
    #endif

    platform_power_save_mode(1);
    
    return ret_of_gm2d_probe;
    

    err3:
        release_mem_region(p_dev_data->pbase, p_dev_data->mem_len);

    err2:
        #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
        platform_set_drvdata(pdev, NULL);
        #else
        dev_set_drvdata(dev, NULL);
        #endif
        

    err1:
        platform_power_save_mode(1);
        return ret_of_gm2d_probe;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
static int gm2d_drv_remove(struct platform_device *pdev)
#else
int gm2d_drv_remove(struct device * dev)
#endif
{      
	struct gm2d_dev_data* p_dev_data = NULL;
	
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	p_dev_data = (struct gm2d_dev_data *)platform_get_drvdata(pdev);	
	#else
	p_dev_data = (struct gm2d_dev_data *)dev_get_drvdata(dev);
	#endif

    platform_exit();

    /* remove resource */   

    iounmap((void __iomem *)p_dev_data->vbase);
    release_mem_region(p_dev_data->pbase, p_dev_data->mem_len);

    free_irq(p_dev_data->irq_no, p_dev_data);

	/* free device memory, and set drv data as NULL	*/
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	platform_set_drvdata(pdev, NULL);
	#else
	dev_set_drvdata(dev, NULL);
	#endif
    
	
	return 0;
}
static void gm2d_platform_release(struct device *device)
{
	/* this is called when the reference count goes to zero */
}

static struct resource gm2d_dev_resource[] = {
	[0] = {
		.start = GM2D_PA_START,
		.end = GM2D_PA_END,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = GRA_FT2DGRA_0_IRQ,
		.end = GRA_FT2DGRA_0_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device gm2d_platform_device = {
	.name	= GM2D_DEV_NAME,
	.id	= 0,	
	.num_resources = ARRAY_SIZE(gm2d_dev_resource),
	.resource = gm2d_dev_resource,
	.dev	= {
		.release = gm2d_platform_release,
	}
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
static struct platform_driver gm2d_platform_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = GM2D_DEV_NAME,
	},

	.probe	= gm2d_drv_probe,
	.remove = gm2d_drv_remove,
};

#else
static struct device_driver gm2d_platform_driver = {
	.owner = THIS_MODULE,
    .name = GM2D_DEV_NAME,
	.bus = &platform_bus_type,
    .probe = gm2d_drv_probe,
    .remove = gm2d_drv_remove,
};
#endif

static struct file_operations gm2d_fops = {
	.owner		    = THIS_MODULE,
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
	.ioctl		    = gm2d_file_ioctl,
	#else
    .unlocked_ioctl = gm2d_file_ioctl,
    #endif
	.mmap		    = gm2d_file_mmap,
	.open           = gm2d_file_open,
	.release        = gm2d_file_release
};

static struct miscdevice gm2d_misc_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= GM2D_DEV_NAME,
	.fops		= &gm2d_fops,
};

static int gm2d_proc_init(void)
{
	int ret = 0;
    struct proc_dir_entry *p;

    p = create_proc_entry("gm2d", S_IFDIR | S_IRUGO | S_IXUGO, NULL);

	if (p == NULL) {
        ret = -ENOMEM;
        goto end;
    }

    gm2d_proc_root = p;

    /*
     * debug message
     */
    gm2d_share_bufer_info = create_proc_entry("share_buf", S_IRUGO, gm2d_proc_root);

	if (gm2d_share_bufer_info == NULL)
        panic("Fail to create proc gm2d_dump_share_info!\n");
    gm2d_share_bufer_info->read_proc = (read_proc_t *) proc_dump_share_info;
    gm2d_share_bufer_info->write_proc = NULL;

	
	gm2d_cmd_bufer_info = create_proc_entry("dm_buf_flag", S_IRUGO, gm2d_proc_root);
	
	if (gm2d_cmd_bufer_info == NULL)
		  panic("Fail to create proc gm2d_cmd_bufer_info!\n");
	gm2d_cmd_bufer_info->read_proc = (read_proc_t *) proc_read_dbg_flag;
	gm2d_cmd_bufer_info->write_proc =(write_proc_t *)proc_set_dbg_flag;
    
#ifdef GM2D_OSG_USING
    gm2d_osgshare_bufer_info = create_proc_entry("osg_sharebuf", S_IRUGO, gm2d_proc_root);

	if (gm2d_osgshare_bufer_info == NULL)
        panic("Fail to create proc osg gm2d_dump_share_info!\n");
    gm2d_osgshare_bufer_info->read_proc = (read_proc_t *) proc_dump_osgshare_info;
    gm2d_osgshare_bufer_info->write_proc = NULL;

	
	gm2d_osgcmd_bufer_info = create_proc_entry("osg_dmbuf_flag", S_IRUGO, gm2d_proc_root);
	
	if (gm2d_osgcmd_bufer_info == NULL)
		  panic("Fail to create proc gm2d_cmd_bufer_info!\n");
	gm2d_osgcmd_bufer_info->read_proc = (read_proc_t *) proc_read_osgdbg_flag;
	gm2d_osgcmd_bufer_info->write_proc =(write_proc_t *)proc_set_osgdbg_flag;

    gm2d_osg_util_info = create_proc_entry("gm2d_osg_util", S_IRUGO, gm2d_proc_root);
    if (gm2d_osg_util_info == NULL)
        panic("Fail to create proc osg_util_info!\n");
    gm2d_osg_util_info->read_proc = (read_proc_t *) proc_osgread_util_info;
    gm2d_osg_util_info->write_proc = (write_proc_t *)proc_osgwrite_util_info;

    gm2d_osg_lock_info = create_proc_entry("gm2d_bus_lock", S_IRUGO, gm2d_proc_root);
    if (gm2d_osg_lock_info == NULL)
        panic("Fail to create proc osg_util_info!\n");
    gm2d_osg_lock_info->read_proc = (read_proc_t *) proc_osgread_lock;
    gm2d_osg_lock_info->write_proc = (write_proc_t *)proc_osgwrite_lock;
#endif 

#if defined(GM2D_LCD_BLIT_USING)

	gm2d_lcdshare_bufer_info = create_proc_entry("lcd_sharebuf", S_IRUGO, gm2d_proc_root);

	if (gm2d_lcdshare_bufer_info == NULL)
        panic("Fail to create proc lcd gm2d_dump_share_info!\n");
    gm2d_lcdshare_bufer_info->read_proc = (read_proc_t *) proc_dump_lcdshare_info;
    gm2d_lcdshare_bufer_info->write_proc = NULL;

	
	gm2d_lcdcmd_bufer_info = create_proc_entry("lcd_dmbuf_flag", S_IRUGO, gm2d_proc_root);
	
	if (gm2d_lcdcmd_bufer_info == NULL)
		  panic("Fail to create proc gm2d_lcdcmd_bufer_info!\n");
	gm2d_lcdcmd_bufer_info->read_proc = (read_proc_t *) proc_read_lcddbg_flag;
	gm2d_lcdcmd_bufer_info->write_proc =(write_proc_t *)proc_set_lcddbg_flag;

#endif

end:
    return ret;
}

static void gm2d_proc_release(void)
{
	if (gm2d_share_bufer_info != NULL)
        remove_proc_entry(gm2d_share_bufer_info->name, gm2d_proc_root);
	
	if (gm2d_cmd_bufer_info != NULL)
        remove_proc_entry(gm2d_cmd_bufer_info->name, gm2d_proc_root);
    
#ifdef GM2D_OSG_USING
    if (gm2d_osgshare_bufer_info != NULL)
        remove_proc_entry(gm2d_osgshare_bufer_info->name, gm2d_proc_root);
	
	if (gm2d_osgcmd_bufer_info != NULL)
        remove_proc_entry(gm2d_osgcmd_bufer_info->name, gm2d_proc_root);   

    if (gm2d_osg_util_info != NULL)
        remove_proc_entry(gm2d_osg_util_info->name, gm2d_proc_root);   

    if (gm2d_osg_lock_info != NULL)
        remove_proc_entry(gm2d_osg_lock_info->name, gm2d_proc_root);   
#endif    

#if defined(GM2D_LCD_BLIT_USING)
	if(gm2d_lcdshare_bufer_info)
		remove_proc_entry(gm2d_lcdshare_bufer_info->name, gm2d_proc_root);

	if(gm2d_lcdcmd_bufer_info)
		remove_proc_entry(gm2d_lcdcmd_bufer_info->name, gm2d_proc_root);	
#endif

    if (gm2d_proc_root != NULL)
		remove_proc_entry(gm2d_proc_root->name, NULL);
}

static int gm2d_dev_data_init(void)
{
    struct gm2d_dev_data* p_dev_data = NULL;
	char buf_name[20];
	
    /* alloc mem for global dev */
    if (g_p_gm2d_dev == NULL)
    {
        g_p_gm2d_dev = kzalloc(sizeof(struct gm2d_dev_data), GFP_KERNEL);
    }

    p_dev_data = g_p_gm2d_dev;
    
    /* reset driver count as 0 */
    DRV_COUNT_RESET();

    /* init shared data */
	p_dev_data->shared_buf_info.alloc_type = ALLOC_NONCACHABLE;
    sprintf(buf_name, "gm2d_m");
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    p_dev_data->shared_buf_info.name = buf_name;
#endif

    p_dev_data->shared_buf_info.size = sizeof(struct gm2d_shared);
    if (frm_get_buf_ddr(DDR_ID_SYSTEM, &p_dev_data->shared_buf_info)) 
    {
        PRINT_E("%s Failed to allocate from frammap\n", __FUNCTION__);
        goto err1;
    }
    
	p_dev_data->p_shared= 
        (struct gm2d_shared*)p_dev_data->shared_buf_info.va_addr;
    
    memset(p_dev_data->p_shared, 0x00, sizeof(struct gm2d_shared));
    
    /* alloc mem to simulate hardware registers */
    #if PSEUDO_HARDWARE == 1
    p_dev_data->vbase = (unsigned int)kzalloc(
        (GM2D_PA_END - GM2D_PA_START +1),
        GFP_KERNEL
    );
    printk("PSEUDO_HARDWARE p_dev_data->vbase 0x%08X\n", p_dev_data->vbase);
    #endif    

    /* proc for debug */
    gm2d_proc_init();
    
#ifdef GM2D_OSG_USING
    if(g_gm2d_osg_devdata ==NULL)
    {
        g_gm2d_osg_devdata = kzalloc(sizeof(struct gm2d_osg_dev_data), GFP_KERNEL);
        if(g_gm2d_osg_devdata == NULL)
            panic("gm2d osg can't allocate memory\n");        
    }

      /* init shared data */
	g_gm2d_osg_devdata->gm2d_osg_buf_info.alloc_type = ALLOC_NONCACHABLE;
    sprintf(buf_name, "gm2d_osg");
	
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    g_gm2d_osg_devdata->gm2d_osg_buf_info.name = buf_name;
    #endif

    g_gm2d_osg_devdata->gm2d_osg_buf_info.size = sizeof(struct gm2d_shared);
    if (frm_get_buf_ddr(DDR_ID_SYSTEM, &g_gm2d_osg_devdata->gm2d_osg_buf_info)) 
    {
        PRINT_E("%s Failed to allocate from frammap\n", __FUNCTION__);
        goto err1;
    }
    
	g_gm2d_osg_devdata->gm2d_osg_share= 
        (struct gm2d_shared*)g_gm2d_osg_devdata->gm2d_osg_buf_info.va_addr;
    
    memset(g_gm2d_osg_devdata->gm2d_osg_share, 0x00, sizeof(struct gm2d_shared));
    init_MUTEX(&osg_fire_sem);

    memset(&g_utilization,0x0,sizeof(gm2d_util_t));
    UTIL_PERIOD = 1;
#endif

#if defined(GM2D_LCD_BLIT_USING)
	if(g_gm2d_lcd_devdata ==NULL)
    {
        g_gm2d_lcd_devdata = kzalloc(sizeof(struct gm2d_lcd_dev_data), GFP_KERNEL);
        if(g_gm2d_lcd_devdata == NULL)
            panic("gm2d lcd blit can't allocate memory\n");        
    }

      /* init shared data */
	g_gm2d_lcd_devdata->gm2d_lcd_buf_info.alloc_type = ALLOC_NONCACHABLE;
    sprintf(buf_name, "gm2d_lcd");
	
    #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    g_gm2d_lcd_devdata->gm2d_lcd_buf_info.name = buf_name;
    #endif

    g_gm2d_lcd_devdata->gm2d_lcd_buf_info.size = sizeof(struct gm2d_shared);
    if (frm_get_buf_ddr(DDR_ID_SYSTEM, &g_gm2d_lcd_devdata->gm2d_lcd_buf_info)) 
    {
        PRINT_E("%s Failed to allocate from frammap %d\n", __FUNCTION__,__LINE__);
        goto err1;
    }
    
	g_gm2d_lcd_devdata->gm2d_lcd_share= 
        (struct gm2d_shared*)g_gm2d_lcd_devdata->gm2d_lcd_buf_info.va_addr;
    
    memset(g_gm2d_lcd_devdata->gm2d_lcd_share, 0x00, sizeof(struct gm2d_shared));
#endif

    return 0;

    err1:
        return -1;
}

static int gm2d_dev_data_exit(struct gm2d_dev_data* p_dev_data)
{
			
	/* release proc function */
	gm2d_proc_release();
    
    #if PSEUDO_HARDWARE == 1
    kfree((unsigned int*)p_dev_data->vbase);
    #endif

    frm_free_buf_ddr(p_dev_data->shared_buf_info.va_addr);
    
    kfree (g_p_gm2d_dev);
    g_p_gm2d_dev = NULL;

#ifdef GM2D_OSG_USING
    frm_free_buf_ddr(g_gm2d_osg_devdata->gm2d_osg_buf_info.va_addr);
    kfree (g_gm2d_osg_devdata);
    g_gm2d_osg_devdata = NULL;

#endif

#if defined(GM2D_LCD_BLIT_USING)	
	frm_free_buf_ddr(g_gm2d_lcd_devdata->gm2d_lcd_buf_info.va_addr);
    kfree (g_gm2d_lcd_devdata);
    g_gm2d_lcd_devdata = NULL;
#endif

    return 0;
}

static int __init gm2d_module_init(void)
{
	int ret;
	DEBUG_PROC_ENTRY;
	printk("%s\n", __FUNCTION__);
	
    /* init device data */
    ret = gm2d_dev_data_init();
    if (unlikely(ret != 0)) 
    {
        printk("%s fails: ft2dge_dev_data_init %d\n", __FUNCTION__, ret);
        goto ERR1;
    }
    
	
    #if PSEUDO_HARDWARE == 0	
    /* register device */
	if ((ret = platform_device_register(&gm2d_platform_device)))
	{
		PRINT_E("Failed to register platform device '%s'\n", gm2d_platform_device.name);
		goto ERR1;
	}

    /* register driver */
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if ((ret = platform_driver_register(&gm2d_platform_driver)))
	{
		PRINT_E("Failed to register platform driver '%s'\n", gm2d_platform_driver.driver.name);
		goto ERR2;
	}
	#else
	if ((ret = driver_register(&gm2d_platform_driver)))
	{
		PRINT_E("Failed to register platform driver '%s'\n", gm2d_platform_driver.name);
		goto ERR2;
	}
	#endif
    
    #endif
	if ((ret = misc_register(&gm2d_misc_device)))
	{
		PRINT_E("Failed to register misc device '%s'\n", gm2d_misc_device.name);
		goto ERR3;
	}	
	
	return 0;

        
    ERR3:
		#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		platform_driver_unregister(&gm2d_platform_driver);
		#else
		driver_unregister(&gm2d_platform_driver);
		#endif
        
    ERR2:
		platform_device_unregister(&gm2d_platform_device);
        
    ERR1:
        return ret;
        
}  


static void __exit gm2d_module_exit(void)
{
	DEBUG_PROC_ENTRY;
	printk("%s\n", __FUNCTION__);
	misc_deregister(&gm2d_misc_device);
	platform_device_unregister(&gm2d_platform_device);
	
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	platform_driver_unregister(&gm2d_platform_driver);
	#else
	driver_unregister(&gm2d_platform_driver);
	#endif
    
    gm2d_dev_data_exit(g_p_gm2d_dev);
}

module_init(gm2d_module_init);
module_exit(gm2d_module_exit);

MODULE_AUTHOR("GM Technology Corp."); 
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("gm2d kernel device driver");
