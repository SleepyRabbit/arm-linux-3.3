#include "platform.h"
#include "irda_drv.h"
#include "irda_dev.h"
#include <mach/ftpmu010.h>
#include <linux/delay.h>                                                                                                                                                  
#include <linux/kthread.h>

#define PRINT_E(args...) do { printk(args); }while(0)
#define PRINT_I(args...) do { printk(args); }while(0)

#if defined(CONFIG_PLATFORM_GM8139) || defined(CONFIG_PLATFORM_GM8136) 
#define CLKOFF           0xB8
#define CLKOFF_IRDET    (0x1<<3)
#else
#define CLKOFF           0xB8
#define CLKOFF_IRDET    (0x1<<11)
#endif

#ifdef CONFIG_PLATFORM_GM8287
#define GPIO_REG		 0x5C
#define GPIO_MSK		(0x11<<18)/* 18,19 bit define GPIO 15,16 function */
#define GPIO_value      (0x0)
#elif defined (CONFIG_PLATFORM_GM8139) || defined(CONFIG_PLATFORM_GM8136) 
#define GPIO_REG		 0x50
#define GPIO_MSK		(0xc0)/* 6,7 bit 00 define GPIO0_3 unction */
#define GPIO_value      (0x40)/*bit 6 ,7 is 01 ,GPIO mode*/

#define GPIO_TDO_MSK	(0xC0)/* 8,9 bit 00 define GPIO0_4 unction */
#define GPIO_TDO_value  (0x40)/*bit 8 ,9 is 01 ,GPIO mode*/

#define GPIO_TMS_MSK	(0x30)/* 4,5 bit 00 define GPIO0_2 unction */
#define GPIO_TMS_value  (0x10)/*bit 4 ,5 is 01 ,GPIO mode*/

#define GPIO_TCK_MSK	(0x0C)/* 2,3 bit 00 define GPIO0_1 unction */
#define GPIO_TCK_value  (0x04)/*bit 2 ,3 is 01 ,GPIO mode*/

#define GPIO_ALL_MSK	(0xFF)/* 2,3 bit 00 define GPIO0_1 unction */

#if defined(CONFIG_PLATFORM_GM8136) 
#define PLL_REG         (0x28)
#define ADDA_DIVID_REG  (0x74)

#define PLL_ADDA_MASK   (BIT16|BIT15)
#define PLL_ADDA_PLL1   (0x01)
#define PLL_ADDA_PLL2   (0x10) 
#define PLL1_CNTP3_DIV_MASK (0x07000000) //24~26

#define ADDA_DIVID_MASK (0x3F000000) //24~29
#define IRDA_CLK_DIV_DEFAULT  0x77  
//#define ADDA_PLL1_CLK      (885*1000*1000) //885M
#define IRDA_DEFAULT_CLK   (100*1000) //100K
#define IRDA_POLLING_DELAY  200  
#define IRDA_PLL1_MODE      0x01
#define IRDA_PLL2_MODE      0x02
#define IRDA_PLL3_MODE      0x04


static u32 g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;    

static struct task_struct *irda_thread = NULL;
static volatile int irda_thread_runing = 0;

#endif

struct gpio_reg_dfn{
    unsigned int unit_mask;
    unsigned int unit_value;
};

static struct gpio_reg_dfn g_gpio_reg[] = {
 {0,0}, //dummy
 {GPIO_TCK_MSK,GPIO_TCK_value},
 {GPIO_TMS_MSK,GPIO_TMS_value},
 {GPIO_TDO_MSK,GPIO_TDO_value},
};


#else
#define GPIO_REG		 0x5C
#define GPIO_MSK		(0x1<<30)
#define GPIO_value      (0x0)
#endif

static int scu_fd;

/* *INDENT-OFF* */
static pmuReg_t pmu_reg[] = {
/*  {reg_off,   bits_mask,     lock_bits,    init_val,    init_mask } */ 
    {CLKOFF,    CLKOFF_IRDET,  CLKOFF_IRDET, 0x0,         0x0}, // on/off
#if defined (CONFIG_PLATFORM_GM8139) || defined(CONFIG_PLATFORM_GM8136) 
    {GPIO_REG,  GPIO_ALL_MSK,  GPIO_MSK, 	 0x0,         0x0}, // on/off
#else
    {GPIO_REG,  GPIO_MSK,  	   GPIO_MSK, 	 0x0,         0x0}, // on/off
#endif    
};


static pmuRegInfo_t pmu_reg_info = {
    DEV_NAME,
    ARRAY_SIZE(pmu_reg),
    ATTR_TYPE_NONE,
    &pmu_reg[0],
};
/* *INDENT-ON* */

int register_scu(void)
{
    scu_fd = ftpmu010_register_reg(&pmu_reg_info);
    if (scu_fd < 0) {
        printk("Failed to register %s scu\n", DEV_NAME);
        goto exit;
    }

    return 0;

  exit:
    return -1;
}


int deregister_scu(void)
{
#if	defined(CONFIG_PLATFORM_GM8136) 

	if (irda_thread)
        kthread_stop(irda_thread);
    
    while (irda_thread_runing == 1)
        msleep(1);
#endif    
    if (scu_fd >= 0) {
        if (ftpmu010_deregister_reg(scu_fd)) {
            printk("Failed to deregister %s scu\n", DEV_NAME);
            goto exit;
        }
        scu_fd = -1;
    }
    return 0;

  exit:
    return -1;
}

int clock_on(void)
{
    if (unlikely(scu_fd < 0)) {
        goto exit;
    }
    
    if (ftpmu010_write_reg(scu_fd, CLKOFF, 0, CLKOFF_IRDET)) {
    //if (ftpmu010_write_reg(scu_fd, CLKOFF, 1, CLKOFF_IRDET)) {
        PRINT_E("Failed to enable %s scu\n", DEV_NAME);
        goto exit;
    }

    return 0;

  exit:
    return -1;
}

int clock_off(void)
{
    if (unlikely(scu_fd < 0)) 
    {
        goto exit;
    }
   
    if (unlikely(ftpmu010_write_reg(scu_fd, CLKOFF, CLKOFF_IRDET, CLKOFF_IRDET))) 
    {
        PRINT_E("Failed to disable %s scu\n", DEV_NAME);
        goto exit;
    }

    return 0;

  exit:
    return -1;
}
#if defined(CONFIG_PLATFORM_GM8136)
void irda_set_irda_clk_default(void)
{
	g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;
}
u32 irda_get_irda_clk(void)
{
	return g_irda_clk_div;
}

int irda_calcu_irda_clkdivid(u32 kind,u32 cntp_div,u32 adda_div)
{
	
	u32 mclk_adda;
	u32 irda_clk;

	if(kind == IRDA_PLL1_MODE){
		if(adda_div==0 || cntp_div==0){
			printk("error adda_div || cntp_div is zero\n");
			g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;
			return -1;
		}
	}else if(kind == IRDA_PLL2_MODE){
		if(adda_div==0 ){
			printk("error adda_div is zero\n");
			g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;
			return -1;
		}

	}else{
		if(adda_div==0 ){
			printk("error adda_div is zero\n");
			g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;
			return -1;
		}
	}

	if(kind == IRDA_PLL1_MODE)
		mclk_adda = (PLL1_CLK_IN/(cntp_div+1))/(adda_div+1);
	else if(kind == IRDA_PLL2_MODE)
		mclk_adda = (PLL2_CLK_IN)/(adda_div+1);
	else{
		mclk_adda = (PLL3_CLK_IN)/(adda_div+1);	
	}	

	irda_clk = mclk_adda/IRDA_DEFAULT_CLK;

	if(irda_clk <=0){
		printk("irda div factor isn't support(%d-%d-%d-%d)\n",mclk_adda,cntp_div,adda_div,irda_clk);
		g_irda_clk_div = IRDA_CLK_DIV_DEFAULT;
		return -1;
	}else if(irda_clk > 0x100){
		irda_clk = 0x100;
	}
	
	g_irda_clk_div	= irda_clk-1;
	return 0;	

}
static int irda_polling_thread(void *private)
{
	unsigned int pmu_mfp,div_cntp,div_val;
	
    irda_thread_runing = 1;   
   
    do {  
		pmu_mfp = ftpmu010_read_reg(PLL_REG);
        div_cntp = (pmu_mfp&PLL1_CNTP3_DIV_MASK)>>24;
		pmu_mfp = (pmu_mfp&PLL_ADDA_MASK)>>15;
        
		if(pmu_mfp == PLL_ADDA_PLL1){
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL1_MODE,div_cntp,div_val);
		}else if(pmu_mfp&PLL_ADDA_PLL2){
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL2_MODE,div_cntp,div_val);
		}else{
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL3_MODE,div_cntp,div_val);
		}
        msleep(IRDA_POLLING_DELAY);
        
    }while(!kthread_should_stop());
    
    irda_thread_runing = 0;
    return 0;
}

#endif

int irda_SetPinMux(int gpio_num)
{
 	unsigned int pmu_mfps = ftpmu010_read_reg(GPIO_REG);
	int ret = 0;
#if defined (CONFIG_PLATFORM_GM8139) || defined(CONFIG_PLATFORM_GM8136)    
    int tmp_num;
#endif
#if defined(CONFIG_PLATFORM_GM8136)
	u32 div_val,div_cntp;
#endif
    
#if defined (CONFIG_PLATFORM_GM8139) || defined(CONFIG_PLATFORM_GM8136)
    /*support 1~3*/
    if(gpio_num < 1 ||gpio_num > 3)
        tmp_num = 3;
    else
        tmp_num = gpio_num;

    pmu_mfps &= ~(g_gpio_reg[tmp_num].unit_mask);

    if (unlikely((ret = ftpmu010_write_reg(scu_fd, GPIO_REG, g_gpio_reg[tmp_num].unit_value, g_gpio_reg[tmp_num].unit_mask))!= 0)){
		PRINT_E("%s set as 0x%08X\n", __func__, pmu_mfps);
	}
	pmu_mfps = ftpmu010_read_reg(GPIO_REG);
    
#else
	pmu_mfps &= ~(GPIO_MSK);

	/* set 0 to select GPIO input pin*/
   	if (unlikely((ret = ftpmu010_write_reg(scu_fd, GPIO_REG, GPIO_value, GPIO_MSK))!= 0)){
		PRINT_E("%s set as 0x%08X\n", __func__, pmu_mfps);
	}
	pmu_mfps = ftpmu010_read_reg(GPIO_REG);
#endif

#if defined(CONFIG_PLATFORM_GM8136)
	pmu_mfps = ftpmu010_read_reg(PLL_REG);
    div_cntp = (pmu_mfps&PLL1_CNTP3_DIV_MASK)>>24;
	pmu_mfps = (pmu_mfps&PLL_ADDA_MASK)>>15;
	
	if(pmu_mfps == PLL_ADDA_PLL1){
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL1_MODE,div_cntp,div_val);
		}else if(pmu_mfps&PLL_ADDA_PLL2){
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL2_MODE,div_cntp,div_val);
		}else{
			div_val = ftpmu010_read_reg(ADDA_DIVID_REG);
			div_val = (div_val &ADDA_DIVID_MASK) >> 24;
			irda_calcu_irda_clkdivid(IRDA_PLL3_MODE,div_cntp,div_val);
	}

	irda_thread = kthread_create(irda_polling_thread,NULL, "irda thread");
    if (IS_ERR(irda_thread)){
        printk("irda: Error in creating kernel thread! \n");
        return -1;
    }

    wake_up_process(irda_thread); 
#endif

    return ret;
}
#if 1
int scu_probe(struct scu_t *p_scu)
{
    int ret = 0;
    
    
    if (unlikely((ret = register_scu()) < 0)) 
    {
        printk("register_scu errr\n");
        goto err1;
    }
    
    if (unlikely((ret = clock_on()) < 0)) 
    {
        printk("enable_scu errr\n");
        goto err2;
    }

    return 0;

    err2:
        deregister_scu();
    err1:
        return ret;
}

int scu_remove(struct scu_t *p_scu)
{
    clock_off();
    deregister_scu();

    return 0;
}
#endif


