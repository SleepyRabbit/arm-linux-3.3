#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <mach/platform-GM8136/platform_io.h>
#include <mach/ftpmu010.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <linux/synclink.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/delay.h>

#include "adda308.h"
#include <adda308_api.h>
#include "audio_dev_ioctl.h"
#include <platform.h>

#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>

/***************************Revision History**************************/
// ver.150324 | add device node for ioctl interface to get/set speaker volume
// ver.150115 | 1. Dynamic select MCLK base on PLL1 for ADDA308 & SSP
//              2. Add HPF proc nodes for DMIC record noise  
/*********************************************************************/

#define SUPPORT_POWER_PD
#define VERSION     150324 //2015_0324
adda308_priv_t priv;

extern int adda308_proc_init(void);
extern void adda308_proc_remove(void);

static u32 chip_id = 0;
static u32 bond_id = 0;

typedef enum {
    DMIC_PMU_64H,
    DMIC_PMU_50H
} adda308_DMIC_PMU_REG;

#define adda308_DMIC_DEF_PMU  DMIC_PMU_50H

static pmuReg_t DMIC_Reg[] = {
    /* reg_off, bit_masks, lock_bits, init_val, init_mask */
    {  0x64,    0xf<<12,   0x0,       0xa<<12,  0xf<<12}, // Select PMU for DMIC(Mode2) 0x64 bit 12,13,14,15
    {  0x50,    0xf<<6,    0x0,       0xf<<6,   0xf<<6}   // Select PMU for DMIC(Mode2) 0x50 bit 6,7,8,9,
};

static u32 curr_mclk = 0;
static audio_pll_mclk_map_t PLL1_MCLK_Map[] = {
    { 860000000, 24571428 },
    { 590000000, 24583333 },
    { 762000000, 24580645 },
    { 885000000, 24583333 },
    { 712500000, 24568965 },
    { 442500000, 24583333 }
};


struct adda308_dev_t {
    char                name[16];   ///< device name
    struct semaphore    lock;       ///< device locker
    struct miscdevice   miscdev;    ///< device node for ioctl access
};

static struct adda308_dev_t adda308_dev;

static void adda308_PMU_Select_DMIC_Input(pmuReg_t *reg);

#define SSP_MAX_USED    2
//input_mode: 0 -> analog MIC,  1->line-in , 2 -> digital MIC
uint input_mode = 0;
module_param(input_mode, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(input_mode, "input mode can be analog MIC/line in/digital MIC");
//output_mode: 0 -> spk out, 1 -> line out
uint output_mode = DA_SPEAKER_OUT;
module_param(output_mode, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(output_mode, "output mode can be speaker out/line out");
//enable_dmic: 0 -> disable, 1 -> enable,
uint enable_dmic = 0;
module_param(enable_dmic, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(enable_dmic, "enable digital MIC");
//enable BTL or not
uint enable_BTL = 1;
module_param(enable_BTL, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(enable_BTL, "select 1: speaker mode or 0: headphone mode");
//single-ended / differential mode, 1 --> single ended mode, 0 --> differential mode
uint single_end = 1;
module_param(single_end, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(single_end, "select 1: single ended or 0: differential mode");
//ALC enable
uint enable_ALC = 0;
//module_param(enable_ALC, uint, S_IRUGO|S_IWUSR);
//MODULE_PARM_DESC(enable_ALC, "enable ALC");
//ALCNGTH, from -78, -72, -66, -60, -54, -48, -42, -36dB
int ALCNGTH = -36;
module_param(ALCNGTH, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCNGTH, "ALC noise gate threshold");
//ALC MIN, from -27, -24, -21, -18, -15, -12, -9, -6dB
int ALCMIN = -27;
module_param(ALCMIN, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCMIN, "ALC minimum gain");
//ALC MAX, from -6, 0, 6, 12, 18, 24, 30, 36dB
int ALCMAX = 36;
module_param(ALCMAX, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCMAX, "ALC maximum gain");
//ALC HOLD TIME, 0:0us, 1:250us, 2:500us, 3:1ms, ... , max is 15 ,Time is doubled with every step
int ALCHOLT = 0;
module_param(ALCHOLT, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCHOLT, "ALC hold time");
//ALC TARGET LEVEL, -4dBFS ~ -34dBFS
int ALCTL = -4;
module_param(ALCTL, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCTL, "ALC target level");
//ALC DECAY TIME, 0:104us, 1:208us, ... , max is 15. Time is doubled with every step
int ALCDCYT = 0;
module_param(ALCDCYT, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCDCYT, "ALC decay time");
//ALC ATTACK TIME, 0:63.3us, 1:125us, ... , max is 15. Time is doubled with every step
int ALCATKT = 0;
module_param(ALCATKT, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ALCATKT, "ALC attack time");

//L channel input analog Gain, from -27 ~ +36 dB
int LIV = 15;
module_param(LIV, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(LIV, "L channel input analog gain");
//L channel input mute, 0 -> unmute, 1 -> mute
int LIM = 0;
module_param(LIM, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(LIM, "L channel input mute");
//L channel input digital Gain, -50 ~ 30 dB
int LADV = 0;
module_param(LADV, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(LADV, "L channel input digital gain");
//L channel output digital Gain, -50 ~ 30 dB
int LDAV = 0;
module_param(LDAV, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(LDAV, "L channel output digital gain");
//L channel output mute, 0 -> unmute, 1 -> mute
int LHM = 0;
module_param(LHM, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(LHM, "L channel output mute");
//speaker output Gain, -40 ~ +6 dB
int SPV = 0;
module_param(SPV, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(SPV, "speaker volume");
//High pass filter
int HPF = 1; //0:disable(line-out); 1:enable(speaker out)
module_param(HPF, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(HPF, "high pass filter");
//HPFAPP: Select audio mode or appilcation mode
int HPFAPP = 1; //0:audio mode; 1:application mode
module_param(HPFAPP, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(HPFAPP, "select audio/application mode");
//HPFCUT: cut-off frequency under application mode
int HPFCUT = 7;
module_param(HPFCUT, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(HPFCUT, "cut-off frequency under application mode");
//DAMIXER: simulate mono audio source to output stereo audio
int DAMIXER = 0;
module_param(DAMIXER, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(DAMIXER, "DAMIXER enable or not");
// ADDA is master or is slave
int ISMASTER = 1;
module_param(ISMASTER, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ISMASTER, "select 1: ADDA as master, 0: ADDA as slave");
// audio output enable
static int audio_out_enable[SSP_MAX_USED] = {1,0};
module_param_array(audio_out_enable, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(audio_out_enable, "is output enable for each audio interface");
//use SSP0 as rx/tx or use SSP0 as rx, SSP1 as tx --> keep it, but don't use
uint same_i2s_tx_rx = 1;
module_param(same_i2s_tx_rx, uint, S_IRUGO|S_IWUSR);
// test mode only for testing adda performance, set micboost as 0db
uint test_mode = 0;
module_param(test_mode, uint, S_IRUGO|S_IWUSR);
// used to control ADDA codec's power
int power_control = 1;
module_param(power_control, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(power_control, "ADDA power control");

// ADDA clock source from PLL2
int clk_pll2 = 0;
int reback = 0;
int ssp1_rec_src = 1; // 0:from ADDA; 1:from external codec
module_param(ssp1_rec_src, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ssp1_rec_src, "SSP1 Rec Source: 0:ADDA, 1:External CODEC");

extern int platform_init(void);
extern int platform_exit(void);

#if DEBUG_ADDA308
void dump_adda308_reg(void)
{
	u32 tmp = 0;
	u32 base = priv.adda308_reg;
	DEBUG_ADDA308_PRINT("\n0x28 = %x\n", ftpmu010_read_reg(0x28));
	DEBUG_ADDA308_PRINT("0x74 = %x\n", ftpmu010_read_reg(0x74));
	DEBUG_ADDA308_PRINT("0x7C = %x\n", ftpmu010_read_reg(0x7C));
	DEBUG_ADDA308_PRINT("0xb8 = %x\n", ftpmu010_read_reg(0xb8));
	DEBUG_ADDA308_PRINT("0xac = %x\n", ftpmu010_read_reg(0xac));
	/* adda wrapper */
	printk("ADDA Wrapper\n");
	tmp = *(volatile unsigned int *)(base + 0x0);
	DEBUG_ADDA308_PRINT("0x00 = %x\n", tmp);
	tmp = *(volatile unsigned int *)(base + 0x4);
	DEBUG_ADDA308_PRINT("0x04 = %x\n", tmp);
	tmp = *(volatile unsigned int *)(base + 0x8);
	DEBUG_ADDA308_PRINT("0x08 = %x\n", tmp);
	tmp = *(volatile unsigned int *)(base + 0xc);
	DEBUG_ADDA308_PRINT("0x0c = %x\n", tmp);
	tmp = *(volatile unsigned int *)(base + 0x10);
	DEBUG_ADDA308_PRINT("0x10 = %x\n", tmp);
}
#endif//end of DEBUG_ADDA300

/*
 * Create engine and FIFO
 */
int add_engine(unsigned long addr)
{
    priv.adda308_reg = addr;

    return 0;
}

int adda308_get_ssp1_rec_src(void)
{
    return ssp1_rec_src;
}
EXPORT_SYMBOL(adda308_get_ssp1_rec_src);


/* Note: adda308_init_one() will be run before adda308_drv_init() */
static int adda308_init_one(struct platform_device *pdev)
{
	struct resource *mem;
	unsigned long   vaddr;
    printk("adda308_init_one [%d] version %d\n", pdev->id, VERSION);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		printk("no mem resource?\n");
		return -ENODEV;
	}

    vaddr = (unsigned long)ioremap_nocache(mem->start, PAGE_ALIGN(mem->end - mem->start));
    if (unlikely(!vaddr))
        panic("%s, no virtual address! \n", __func__);

	add_engine(vaddr);

    return 0;
}

static int adda308_remove_one(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver adda308_driver = {
    .probe	= adda308_init_one,
    .remove	=  __devexit_p(adda308_remove_one),
	.driver = {
	        .name = "adda308",
	        .owner = THIS_MODULE,
	    },
};

/*
 * ADDA main clock is 24MHz(line out) or 24.5454MHz(speaker out)
 * at GM8136
 * speaker out mode:
 * ssp source from PLL1/cntp3 (810/3/11MHz)
 * when ADDA is master, SSP is slave, SSP main clock is 24.5454MHz, meet sample rate:8/16/32/48k
 --------------------------------------------------------------------------------------------------
 * line out mode
 * ssp source from PLL2 (600/25MHz)
 * when ADDA is master, SSP is slave, SSP main clock is 24(600/25)MHz, meet sample rate:8/16/22.05/32/44.1/48k
 */
static void _adda308_ssp_mclk_init(int ssp_idx)
{
    int ret = 0;

    if (ISMASTER == 1) {
        if (output_mode == DA_SPEAKER_OUT)
            ret = plat_set_ssp_clk(ssp_idx, curr_mclk, clk_pll2);
        else
            ret = plat_set_ssp_clk(ssp_idx, 24000000, clk_pll2);
    }
    else
        ret = plat_set_ssp_clk(ssp_idx, 24000000, clk_pll2);

    if (ret < 0)
        printk("ADDA set ssp%d main clock fail\n", ssp_idx);
}

void adda308_ssp_mclk_init(void)
{
    //int ret = 0;

    // ssp0
    _adda308_ssp_mclk_init(0);

    // ssp1
    if (audio_out_enable[1] == 1)
        _adda308_ssp_mclk_init(1);        

}

void adda308_mclk_init(u32 hz)
{
	volatile u32 tmp = 0;
    volatile u32 mask = 0;
    volatile u32 pvalue = 0;
	unsigned long div = 0;
	unsigned long curr_div = 0;
	volatile u32 cntp3 = 0;

	pvalue = ftpmu010_read_reg(0x74);
	curr_div = (pvalue & 0x3f000000) >> 24;

	//give adda308 24.5454 or 24MHz main clock
    tmp = 0;
    mask = (BIT24|BIT25|BIT26|BIT27|BIT28|BIT29);

    if (clk_pll2 == 0)      // clock source from pll1 cntp3
        div = PLL1_CLK_IN / (cntp3 + 1);
    if (clk_pll2 == 1)      // clock source from pll2
        div = PLL2_CLK_IN;
    if (clk_pll2 == 2)      // clock source from pll3
        div = PLL3_CLK_IN;

    div /= hz;
    div -= 1;

    // set clock divided value
    if (div != curr_div) {
        tmp |= (div << 24);
        DEBUG_ADDA308_PRINT("adda308 gets divisor %ld, PPL3_CLK = %d, mclk = %lu \n",
                div, PLL3_CLK_IN, PLL3_CLK_IN/(div+1));

        if (ftpmu010_write_reg(priv.adda308_fd, 0x74, tmp, mask) < 0) {
            panic("In %s: setting offset(0x%x) failed.\n", __func__, 0x74);
        }
    }

    // set clock source, adda default clock source from pll3, set from pll2
    mask = 0;
    tmp = 0;

    mask = BIT15 | BIT16;
    if (clk_pll2 == 0)
        tmp = 1 << 15;
    if (clk_pll2 == 1)
        tmp = 2 << 15;
    if (clk_pll2 == 2)
        tmp = 0 << 15;

    if (ftpmu010_write_reg(priv.adda308_fd, 0x28, tmp, mask) < 0) {
        panic("In %s: setting offset(0x%x) failed.\n", __func__, 0x28);
    }
}

void ADDA308_Set_DA_Source(int da_src)
{
    volatile u32 val = 0;
    volatile u32 mask = BIT27;

    //printk("%s, da_src(%d)\n", __func__, da_src);

    if (da_src >= DA_SRC_MAX) {
        printk("error DA source: %d\n", da_src);
        return;
    }

    val = (da_src == DA_SRC_SSP0)?(val & ~(1<<27)):(val | (1<<27));

    if (ftpmu010_write_reg(priv.adda308_fd, 0x7C, val, mask) < 0)
        panic("In %s: setting offset(0x%x) failed.\n", __func__, 0x7C);
}

/*
 * set adda308 DA source select
 */
static void adda308_da_select(void)
{
	//int ret = 0;
	volatile u32 tmp = 0;
    volatile u32 mask = 0;

    tmp = 0;
    mask = BIT27;

    if ((audio_out_enable[0] == 1) && (audio_out_enable[1] == 0))  // adda da source select SSP0
        tmp &= ~(1 << 27);
    if ((audio_out_enable[0] == 0) && (audio_out_enable[1] == 1))  // adda da source select SSP1
        tmp |= (1 << 27);
    if ((audio_out_enable[0] == 1) && (audio_out_enable[1] == 1))  // adda da source select SSP0, SSP1 for decoder
        tmp &= ~(1 << 27);

	//Audio DA source select. 0:SSP_0, 1: SSP_1
    if (ftpmu010_write_reg(priv.adda308_fd, 0x7C, tmp, mask) < 0)
        panic("In %s: setting offset(0x%x) failed.\n", __func__, 0x7C);
#if 0	
    // When ADDA(DA) select source from SSP1, SSP1 also have to select source from ADDA(DA)
    if ((audio_out_enable[0] == 0) && (audio_out_enable[1] == 1)) {  // SSP1 source select ADDA(DA)
        ret = plat_set_ssp1_src(PLAT_SRC_ADDA);
        if (ret < 0)
            printk("%s set ssp1 source fail\n", __func__);
    }
    if ((audio_out_enable[0] == 1) && (audio_out_enable[1] == 1)) {  // SSP1 source select DECODER(DA)
        ret = plat_set_ssp1_src(PLAT_SRC_ADDA);
        if (ret < 0)
            printk("%s set ssp1 source fail\n", __func__);
    }
#endif
}

static void adda308_select_ssp1_rec_source(void)
{
    if (ssp1_rec_src == 0)
        plat_set_ssp1_src(PLAT_SRC_ADDA);
    else if (ssp1_rec_src == 1)
        plat_set_ssp1_src(PLAT_SRC_DECODER);
    else
        printk(KERN_ERR"[ADDA308] Error, unknown SSP1 rec source value %d!\n", ssp1_rec_src);
}

void adda308_set_HPF(int hpf)
{
    volatile u32 base=0, tmp=0;

    base = priv.adda308_reg;
    tmp = *(volatile unsigned int *)(base+0x8);

    tmp = (hpf==0)?(tmp & ~(0x1 << 4)):(tmp | (0x1 << 4));

    *(volatile unsigned int *)(base + 0x8) = tmp;
}

void adda308_set_HPFAPP(int hpfapp)
{
    volatile u32 base=0, tmp=0;

    base = priv.adda308_reg;
    tmp = *(volatile unsigned int *)(base+0x10);

    tmp = (hpfapp==0)?(tmp & ~(0x1 << 5)):(tmp | (0x1 << 5));

    *(volatile unsigned int *)(base + 0x10) = tmp;
}

void adda308_set_HPFCUT(int hpfcut)
{
    volatile u32 base=0, tmp=0;
    u8 p0, p1, p2;

    base = priv.adda308_reg;
    tmp = *(volatile unsigned int *)(base+0x10);

    p0 = hpfcut & 0x1; //HPFCUT2
    p1 = (hpfcut & 0x2) >> 1; //HPFCUT1
    p2 = (hpfcut & 0x4) >> 2; //HPFCUT0

    tmp = (p0==0)?(tmp & ~(0x1 << 6)):(tmp | (0x1 << 6));
    tmp = (p1==0)?(tmp & ~(0x1 << 7)):(tmp | (0x1 << 7));
    tmp = (p2==0)?(tmp & ~(0x1 << 8)):(tmp | (0x1 << 8));

    *(volatile unsigned int *)(base + 0x10) = tmp;
}

void adda308_i2s_reset(void)
{
	volatile u32 tmp = 0;
	u32 base = 0;
	/* ALC related */
    u32 ALCNGTH_reg_val = 0x0;	//0x0 is the default. -36 dB reg val
    u32 ALCMIN_reg_val 	= 0x0;  //0x0 is the default. -27 dB reg val
    u32 ALCMAX_reg_val 	= 0x0;  //0x0 is the default. -6 dB reg val
    u32 ALCTL_reg_val 	= 0x0;  //0x0 is the default. -34dbFS
    u32 ALCHOLT_reg_val = 0x0;  //0x0 is the default. 0us
    u32 ALCDCYT_reg_val = 0x0;  //0x0 is the default. 104us 
    u32 ALCATKT_reg_val = 0x0;  //0x0 is the default. 63.3us 
    
    u32 IV_reg_val  = 0x1B;   //0x1B is the default 0 dB reg val, 0x3F is 36dB, 0x25 is 10dB
    u32 IM_reg_val  = 0;      //0 -> unmute, 1 -> mute
    u32 ADV_reg_val = 0x61;   //0x61 is the default 0 dB reg val
    u32 DAV_reg_val = 0x61;   //0x3F is the default 0 dB reg val
    u32 HM_reg_val  = 0;      //0 -> unmute, 1 -> mute
    u32 SPV_reg_val = 0x0;    //0x39 is the default 0 dB reg val, 0x2A is -15dB

    base = priv.adda308_reg;

    if ((ALCNGTH > -36) || (ALCNGTH < -78)) {
        printk("%s: ALCNGTH(%d) MUST between -78 ~ -36dB, use default(-78dB)\n", __func__, ALCNGTH);
        ALCNGTH = -78;
    }

    if ((ALCMIN > -6) || (ALCMIN < -27)) {
        printk("%s: ALCMIN(%d) MUST between -27 ~ -6dB, use default(-6dB)\n", __func__, ALCMIN);
        ALCMIN = -6;
    }

    if ((ALCMAX > 36) || (ALCMAX < -6)) {
        printk("%s: ALCMAX(%d) MUST between -6 ~ +36dB, use default(-6dB)\n", __func__, ALCMAX);
        ALCMAX = -6;
    }
	
	if (ALCTL > -4 || ALCTL < -34) {
    	printk("%s: ALCTL(%d) MUST between -34 ~ -4dBFS, use default(-34dB)\n", __func__, ALCTL);
		ALCTL = -34;
	}
	
    if ((LIV > 36) || (LIV < -27)) {
        printk("%s: LIV(%ddB) MUST between -27dB ~ 36dB, use default(-2dB)\n", __func__, LIV);
        LIV = -2;
    }

    if ((LADV < -50) || (LADV > 30)) {
        printk("%s: LADV(%ddB) MUST between -50dB ~ 30dB, use default(0dB)\n", __func__, LADV);
        LADV = 0;
    }

    if ((LDAV < -50) || (LDAV > 30)) {
        printk("%s: DAV(%ddB) MUST between -50dB ~ 30dB, use default(-1dB)\n", __func__, LDAV);
        LDAV = -1;
    }

    if ((SPV < 0) || (SPV > 3)) {
        printk("%s: SPV Setting(%d) MUST between 0 ~ 3, use default(0)\n", __func__, SPV);
        SPV = 0;
    }

    if (LIM != 0) { LIM = 1; }
    if (LHM != 0) { LHM = 1; }

	/* ALC Reg value */
    ALCNGTH_reg_val += (ALCNGTH - (-78))/6; //refer to spec
    ALCMIN_reg_val  += (ALCMIN - (-27))/3;  //refer to spec
    ALCMAX_reg_val  += (ALCMAX - (-6))/6;   //refer to spec
	ALCTL_reg_val   += (ALCTL - (-34))/2;   // ALCTL range is from -34 ~ -4, 2.0-dB per steps. (reg value is from 0 ~ 15)	
	ALCHOLT_reg_val = ALCHOLT & 0x0F;
    ALCDCYT_reg_val = ALCDCYT & 0x0F;
    ALCATKT_reg_val = ALCATKT & 0x0F;
    
    IV_reg_val += LIV;
    ADV_reg_val += LADV;
    DAV_reg_val += LDAV;
    SPV_reg_val += SPV;
    IM_reg_val = LIM;
    HM_reg_val = LHM;

    // ===== start ADDA 00h =====
	tmp = *(volatile unsigned int *)(base + 0x0);

    // prevent the background noise when playing silence, 2014/12/29
    tmp &= ~(0x1 << 2);

    if (reback == 1)    // enable reback
        tmp |= (0x1 << 3);
    else
        tmp &= ~(0x1 << 3);

    tmp &= ~(0x3 << 14);
    tmp |= (0x1 << 14);             // audio interface is i2s serial data mode

    tmp &= ~(0x1 << 18);            // enable ADC frontend power
    tmp &= ~(0x1 << 20);            // enable ADC power
    tmp &= ~(0x1 << 22);            // enable DAC power
   	tmp |= (0x01 << 27);			//disable speaker power, to avoid power-up noise, enable it after SSP tx enable
    tmp &= ~(0x1 << 28);            // enable MIC bias power
    tmp |= (0x1 << 30);             // set mic mode as single-end input

    tmp &= ~(0x1 << 16);
    if (input_mode == 2)            // enable digital MIC
        tmp |= (0x1 << 16);

    *(volatile unsigned int *)(base + 0x0) = tmp;

    // ===== end ADDA 00h =====



    // ===== start ADDA 04h =====
	tmp = *(volatile unsigned int *)(base + 0x4);

    tmp &= ~0x3;
    tmp &= ~(0x3 << 2);            // disable boost, 0 dB added (line-in)
    if (test_mode == 0 && input_mode == 0)      // when input is MIC mode
        tmp |= (0x1 << 2);         // enable boost, 20 dB added

    // set input unmute
    tmp &= ~(0x1 << 4);
    tmp |= (IM_reg_val << 4);

    // clear IV setting
	tmp &= ~(0x3F << 6);

    // set IV based on user's setting
    tmp |= (IV_reg_val << 6);

    // clear ADV setting
    tmp &= ~(0x7F << 18);

    // set RADV, LADV based on user's setting
    tmp |= (ADV_reg_val << 18);

    // apply 04h setting
    *(volatile unsigned int *)(base + 0x4) = tmp;
    // ===== end ADDA 04h =====

    // ===== start ADDA 08h =====
	tmp = *(volatile unsigned int *)(base + 0x8);
	tmp &= ~(0x3);                  // ADC gain 1.5db
	tmp |= 0x1;

    tmp &= ~(0x1 << 2);             // disable ADC DEM
    tmp |= (0x1 << 3);              // enable ADC DWA


    if (HPF == 1)                   // enable HPE or not
        tmp |= (0x1<<4);
    else
        tmp &= ~(0x1<<4);

	if (enable_ALC)                 //enable ALC or not
        tmp |= (1 << 5);
    else
        tmp &= ~(1 << 5);
    // ALC noise gate threshold
    tmp &= ~(0x7 << 7);
    tmp |= (ALCNGTH_reg_val << 7);
    // ALC MAX / ALC MIN
    tmp &= ~(0x7 << 10);
    tmp &= ~(0x7 << 13);
    tmp |= (ALCMAX_reg_val << 10);
    tmp |= (ALCMIN_reg_val << 13);
	//ALCTL  / ALCATKT / ALCDCYT / ALCHOLT
	tmp &= ~(0xFFFF << 16);
	tmp |= (ALCTL_reg_val << 16);
	tmp |= (ALCATKT_reg_val << 20);
	tmp |= (ALCDCYT_reg_val << 24);
	tmp |= (ALCHOLT_reg_val << 28);	

    *(volatile unsigned int *)(base + 0x8) = tmp;
    // ===== end ADDA 08h =====

    // ===== start ADDA 0Ch =====
	tmp = *(volatile unsigned int *)(base + 0xc);

    tmp &= ~(0x7F);                 // set DAV
    tmp |= DAV_reg_val;

    tmp &= ~(0x1 << 7);             // line out un-mute

    if (output_mode == DA_SPEAKER_OUT)   //speaker out
        tmp |= (0x1 << 8);          // line out power down
    else
        tmp &= ~(0x1 << 8);         // enable line out power 

    tmp &= ~(0x3 << 9);             // speaker gain
    tmp |= (0x1 << 9);

    tmp &= ~(0x3 << 12);
    tmp |= (0x1 << 12);            // SDMGAIN (-2db)

    tmp |= (0x1 << 14);             // enable DAC modulator

    tmp &= ~(0x1 << 16);
    tmp |= (HM_reg_val << 16);

    tmp &= ~(0x3 << 25);            // speaker out power compensation
    tmp |= (SPV_reg_val << 25);

    *(volatile unsigned int *)(base + 0xc) = tmp;
    // ===== end ADDA 0Ch =====

    // ===== start ADDA 10h =====
	tmp = *(volatile unsigned int *)(base + 0x10);

    // clear HYS
    tmp &= ~(0x3);
    if (output_mode == DA_SPEAKER_OUT)
        tmp |= 0x1;                 // audio_hys1

    // clear DT
    tmp &= ~(0x3 << 2);
    tmp |= (0x01 << 2);

    // set cm_en to default
    tmp |= (0x1 << 4);

    // HPFAPP
    if (HPFAPP == 0)
        tmp &= ~(0x1 << 5);
    else
        tmp |= (0x1 << 5);
    
    // HPFCUT    
    tmp |= (0x7 << 6);

    // HPFCUT2
    if ((HPFCUT & 0x1) == 0)
        tmp &= ~(0x1 << 6);
    // HPFCUT1
    if (((HPFCUT & 0x2) >> 1) == 0)
        tmp &= ~(0x1 << 7);
    // HPFCUT0
    if (((HPFCUT & 0x4) >> 2) == 0)
        tmp &= ~(0x1 << 8);

    tmp &= ~(0x1 << 9);           // HPFGATEST1
    tmp &= ~(0x1 << 10);          // HPFGATEST0

    // IRSEL
	tmp &= ~(0x07 << 16);           // reset IRSEL setting
	tmp |= (0x04 << 16);            // set IRSEL 4 as normal, 2 as power saving mode

    // SCFPD
    if (output_mode == DA_SPEAKER_OUT)
        tmp |= (0x01 << 19);          // power down SCF in LOUT
    else
        tmp &= ~(0x01 << 19);          // enable SCF in LOUT

    // IRSEL SPK
    tmp &= ~(0x7 << 24);
    tmp |= (0x04 << 24);

    tmp &= ~(0x01 << 28);
    if (power_control == 0) // power down
        tmp |= (0x01 << 28);

    *(volatile unsigned int *)(base + 0x10) = tmp;
    // ===== end ADDA 10h =====

}

static int db_to_vol(u32 db_reg_val)
{
	unsigned int min_db   = 0x2F; //-50db setting in adda308
	unsigned int max_db   = 0x7F; // 30db setting in adda308
	unsigned int db_range = max_db - min_db; 
	
    unsigned int min_vol   = 0;
	unsigned int max_vol   = 100;
	unsigned int vol_range = 100; //0 ~ 100

	unsigned int vol;

	if (db_reg_val >= max_db)
		vol = max_vol;
	else if (db_reg_val <= min_db)
		vol = min_vol;
	else
		vol = ( ((db_reg_val - min_db) * vol_range) / db_range );

	printk("[adda308_drv] db[0x%x] to vol[%d] \n", db_reg_val, vol);
	
	return vol;
}

static u32 vol_to_db(int vol)
{
	unsigned int min_db   = 0x2F; //-50db setting in adda308
	unsigned int max_db   = 0x7F; // 30db setting in adda308
	unsigned int db_range = max_db - min_db; 
	
    unsigned int min_vol   = 0;
	unsigned int max_vol   = 100;
	unsigned int vol_range = 100; //0 ~ 100

	u32 db_reg_val;

	if (vol >= max_vol)
		db_reg_val = max_db;
	else if (vol <= min_vol)
		db_reg_val = min_db;
	else
		db_reg_val = ((vol * db_range) / vol_range) + min_db;

	printk("[adda308_drv] vol[%d] to db[0x%x]\n", vol, db_reg_val);

	return db_reg_val;
	
}

static void adda308_dav_set(int vol)
{
	u32 tmp = 0;
	u32 base = 0;
	
    base = priv.adda308_reg;

	tmp = *(volatile unsigned int *)(base + 0x0C);
	tmp &= ~(0x7F); 
	tmp |= vol_to_db(vol);
	
    *(volatile unsigned int *)(base + 0x0C) = tmp;
}

static void adda308_dav_get(int *vol)
{
	u32 tmp = 0;
    u32 base = 0;
 
    base = priv.adda308_reg;

    //bit0~bit6 (7F) in offset 0x0C is adda dav
    tmp = *(volatile unsigned int *)(base + 0x0C) & 0x7F;

	*vol = db_to_vol(tmp);
}

void adda308_enable_spkout(void)
{
	unsigned char spk_off = 0;
	volatile u32 tmp = 0;
	u32 base = 0;
	
	base = priv.adda308_reg;
    tmp  = *(volatile unsigned int *)(base + 0x0);

	/*  bit 27 
	     1 : speaker off
	     0 : speaker on
	 */
	spk_off = (tmp & (0x01<<27)) >> 27;
	
	if (spk_off) {
		/*speak out normal mode*/ 
		tmp &= ~(0x01 << 27);
		*(volatile unsigned int *)(base + 0x0) = tmp;
	}
    
}
EXPORT_SYMBOL(adda308_enable_spkout);

void adda308_disable_spkout(void)
{
	unsigned char spk_off = 0;
	volatile u32 tmp = 0;
	u32 base = 0;

	base = priv.adda308_reg;
    tmp  = *(volatile unsigned int *)(base + 0x0);

	/*  bit 27 
	     1 : speaker off
	     0 : speaker on
	 */
	spk_off = (tmp & (0x01<<27)) >> 27;

	if (!spk_off) {    
		/*speak out power down*/ 
	    tmp |= (0x01 << 27);
		*(volatile unsigned int *)(base + 0x0) = tmp;
	}
    
}
EXPORT_SYMBOL(adda308_disable_spkout);

void adda308_i2s_apply(void)
{
    u32 tmp = 0;
    u32 base = 0;

	base = priv.adda308_reg;

    //apply ADDA setting, reset it!!
	tmp = *(volatile unsigned int *)(base + 0x0);
	tmp |= 0x01;
    *(volatile unsigned int *)(base + 0x0) = tmp;

    /* Note: ADDA308's IP designer has adiviced that the delay time
             should be 100ms to avoid some issue, but there will be 
             a short noise during 100 ms delay. Currently change back
             10 ms. 2014/10/09
			 Originally, it delay 10ms. now chage to 3ms to shorten initial time
    */
	mdelay(3);

    tmp &= ~0x1;
    *(volatile unsigned int *)(base + 0x0) = tmp;
}
EXPORT_SYMBOL(adda308_i2s_apply);

/* when tx & rx at same i2s, we have to set ad/da together */
void adda302_set_fs(const enum adda308_fs_frequency fs)
{
	u32 tmp = 0;
    u32 base = priv.adda308_reg;

	tmp = *(volatile unsigned int *)(base + 0x0);

	tmp &= ~(0xFF << 4);

	switch(fs){
		case ADDA302_FS_8K:
            if (output_mode == DA_SPEAKER_OUT) {
                tmp |= (0x03 << 4); // 1536
                tmp |= (0x03 << 8); // 1536
            }
            else {
                tmp |= (0x04 << 4); // 1500
                tmp |= (0x04 << 8); // 1500
            }
			break;
		case ADDA302_FS_16K:
            if (output_mode == DA_SPEAKER_OUT) {
                tmp |= (0x02 << 4); //768
                tmp |= (0x02 << 8); //768
            }
            else {
                tmp |= (0x01 << 4); //750
                tmp |= (0x01 << 8); //750
            }
			break;
		case ADDA302_FS_22_05K:
			tmp |= (0x08 << 4); //544
			tmp |= (0x08 << 8); //544
			break;
		case ADDA302_FS_32K:
            tmp |= (0x07 << 4); //384
			tmp |= (0x07 << 8); //384
			break;
		case ADDA302_FS_44_1K:
			tmp |= (0x0d << 4); //272
			tmp |= (0x0d << 8); //272
			break;
		case ADDA302_FS_48K:
			if (output_mode == DA_SPEAKER_OUT) {
    			tmp |= (0x0f << 4); //256
    			tmp |= (0x0f << 8); //256
    		}
    		else {
    		    tmp |= (0x06 << 4); //250
    			tmp |= (0x06 << 8); //250
    		}
			break;
		default:
			DEBUG_ADDA308_PRINT("%s fails: fs wrong value\n", __FUNCTION__);
			return;
	}

    *(volatile unsigned int *)(base + 0x0) = tmp;

    //adda308_i2s_apply();

#if DEBUG_ADDA308
	DEBUG_ADDA308_PRINT("In %s", __FUNCTION__);
	dump_adda308_reg();
#endif
}
// loopback function is DAC to ADC
void adda302_set_loopback(int on)
{
    u32 tmp = 0;
    u32 base = priv.adda308_reg;

    tmp = *(volatile unsigned int *)(base + 0x0);

    if (on == 1)
        tmp |= (0x1 << 3);            // enable loopback

    if (on == 0)
        tmp &= ~(0x1 << 3);             // disable loopback

    *(volatile unsigned int *)(base + 0x0) = tmp;
}
EXPORT_SYMBOL(adda302_set_loopback);

/* set AD power down */
void adda302_set_ad_powerdown(int off)
{
    u32 tmp = 0;
    u32 base = priv.adda308_reg;

    tmp = *(volatile unsigned int *)(base + 0x0);
    /* power on ad */
    if (off == 0) {
        tmp &= ~(0x1 << 18);            // enable ADC frontend power
        tmp &= ~(0x1 << 20);            // enable ADC power
    }
    /* power down ad */
    if (off == 1) {
        tmp |= (0x1 << 18);            // power down ADC frontend power
        tmp |= (0x1 << 20);            // power down ADC power
    }

    *(volatile unsigned int *)(base + 0x0) = tmp;
}

/* set DA power down */
void adda302_set_da_powerdown(int off)
{
    u32 tmp = 0;
    u32 base = priv.adda308_reg;

    tmp = *(volatile unsigned int *)(base + 0x0);
    /* power on da */
    if (off == 0) {
        tmp &= ~(0x1 << 22);             // enable DAC power
    }
    /* power down da */
    if (off == 1) {
        tmp |= ~(0x1 << 22);            // power down DAC power
    }

    *(volatile unsigned int *)(base + 0x0) = tmp;
}

/* when tx & rx not at same i2s, we have to set ad/da frame sync separately,
   because audio mclk rx/tx should be 12MHz,
   here we set audio ad mclk mode ratio base on mclk 12MHz,
*/
void adda302_set_ad_fs(const enum adda308_fs_frequency fs)
{
	u32 tmp = 0;
	u32 base = priv.adda308_reg;

	tmp = *(volatile unsigned int *)(base + 0x0);

	tmp &= ~(0xF << 4);

	switch(fs){
		case ADDA302_FS_8K:
            if (output_mode == DA_SPEAKER_OUT)
                tmp |= (0x03 << 4); //1536
            else
                tmp |= (0x04 << 4); //1500
			break;
		case ADDA302_FS_16K:
            if (output_mode == DA_SPEAKER_OUT)
                tmp |= (0x02 << 4); //768
            else
                tmp |= (0x01 << 4); //750
			break;
		case ADDA302_FS_22_05K:
			tmp |= (0x08 << 4); //544
			break;
		case ADDA302_FS_32K:
            tmp |= (0x7 << 4);  //384
			break;
		case ADDA302_FS_44_1K:
    		tmp |= (0x0d << 4); //272
			break;
		case ADDA302_FS_48K:
            if (output_mode == DA_SPEAKER_OUT)
                tmp |= (0x0f << 4); //256
            else
                tmp |= (0x06 << 4); //250
			break;
		default:
			DEBUG_ADDA308_PRINT("%s fails: fs wrong value\n", __FUNCTION__);
			return;
	}

	*(volatile unsigned int *)(base + 0x0) = tmp;
	/*Must reset after frame sync setting, or noise may occur. 

  	  It may be trade off.
	  
	  Because it may affect da when tx & rx are not at the same i2s.
	  For the purpose, tx & rx are not at the same i2s,
	  it may not reset adda after sampling rate setting.	
	 */
	adda308_i2s_apply();

#if DEBUG_ADDA308
	DEBUG_ADDA308_PRINT("In %s", __FUNCTION__);
	dump_adda308_reg();
#endif
}

/* when tx & rx not at same i2s, we have to set ad/da frame sync separately,
   because audio mclk rx/tx should be 12MHz,
   here we set audio da mclk mode ratio base on mclk 12MHz
*/
void adda302_set_da_fs(const enum adda308_fs_frequency fs)
{
	u32 tmp = 0;
	u32 base = priv.adda308_reg;

	tmp = *(volatile unsigned int *)(base + 0x0);

	tmp &= ~(0xF << 8);

	switch(fs){
		case ADDA302_FS_8K:
            if (output_mode == DA_SPEAKER_OUT)
                tmp |= (0x03 << 8); //1536
            else
                tmp |= (0x04 << 8); //1500
			break;
		case ADDA302_FS_16K:
            if (output_mode == DA_SPEAKER_OUT)
                tmp |= (0x02 << 8); //768
            else
                tmp |= (0x01 << 8); //750
			break;
		case ADDA302_FS_22_05K:
			tmp |= (0x08 << 8); //544
			break;
		case ADDA302_FS_32K:
			tmp |= (0x07 << 8); //384
			break;
		case ADDA302_FS_44_1K:
			tmp |= (0x0d << 8); //272
			break;
		case ADDA302_FS_48K:
			if (output_mode == DA_SPEAKER_OUT)
			    tmp |= (0x0f << 8); //256
			else
			    tmp |= (0x06 << 8); //250
			break;
		default:
			DEBUG_ADDA308_PRINT("%s fails: fs wrong value\n", __FUNCTION__);
			return;
	}

    *(volatile unsigned int *)(base + 0x0) = tmp;

	/*Must reset after frame sync setting, or noise may occur. 

  	  It may be trade off.
	  
	  Because it may affect ad when tx & rx are not at the same i2s.
	  For the purpose, tx & rx are not at the same i2s,
	  it may not reset adda after sampling rate setting.	
	 */
	adda308_i2s_apply();

#if DEBUG_ADDA308
	DEBUG_ADDA308_PRINT("In %s", __FUNCTION__);
	dump_adda308_reg();
#endif
}
/* adda308 initial sequence
 * 1. enable iso_enable & psw_pd
   2. reset
   3. set adda mclk
   4. set default ad/da mclk mode
   5. set adda as slave
   6. reset
*/
static void adda308_i2s_ctrl_init(void)
{
	u32 tmp = 0;
	u32 base = 0;

	base = priv.adda308_reg;
    //1. set psw_pd = 1, iso_enable = 0
    tmp = *(volatile unsigned int *)(base + 0x10);
    tmp |= (0x1 << 29);
    tmp &= ~(1 << 28);
    *(volatile unsigned int *)(base + 0x10) = tmp;
    //2. reset
    adda308_i2s_apply();

    //3. set adda mclk
    if (output_mode == DA_SPEAKER_OUT)
        adda308_mclk_init(curr_mclk);
    else
        adda308_mclk_init(24000000);
    //4. set default ad/da mclk mode
	tmp = *(volatile unsigned int *)(base + 0x0);
    tmp |= (0x03 << 8);     // default MCLK DA mode set to serve 8K sampling rate, you should change this when changing sampling rate
	tmp |= (0x01 << 14);    // I2S mode
	tmp |= (0x03 << 4);     // default MCLK AD mode set to serve 8K sampling rate, you should change this when changing sampling rate
    //5. set adda as slave mode
	tmp &= ~(0x01 << 1);    // set ADAA Slave Mode

    *(volatile unsigned int *)(base + 0x0) = tmp;
    //6. reset
    adda308_i2s_apply();
}

static void adda308_i2s_init(void)
{
    u32 tmp = 0;
	u32 base = 0;

	base = priv.adda308_reg;

    adda308_i2s_ctrl_init();
    // set default value
    adda308_i2s_reset();
    //7. set master
    tmp = *(volatile unsigned int *)(base + 0x0);
    if (ISMASTER == 1)
	    tmp |= (0x01 << 1);     // set ADDA Master Mode
    *(volatile unsigned int *)(base + 0x0) = tmp;
    // reset
    adda308_i2s_apply();
    //adda308_i2s_apply();
    //adda308_i2s_powerdown(0);
}

static void adda308_get_chip_id(void)
{
    u32 temp;

    temp = ftpmu010_read_reg(0x00);

    bond_id = (temp & 0xf) | ((temp >> 4) & 0xf0);
    chip_id = (ftpmu010_get_attr(ATTR_TYPE_CHIPVER) >> 16) & 0xFFFF;

    if (chip_id != 0x8135 && chip_id != 0x8136)
        panic("adda308 can not recognize this chip_id %x\n", chip_id);
}

int adda308_get_output_mode(void)
{
    return output_mode;
}

int adda308_get_LDAV(void)
{
    return LDAV;
}

int adda308_get_LADV(void)
{
    return LADV;
}

int adda308_get_LIV(void)
{
    return LIV;
}

int adda308_find_mclk(void)
{
    int i, max;
    unsigned long pll1;

    pll1 = PLL1_CLK_IN;
    max = ARRAY_SIZE(PLL1_MCLK_Map);

    for (i=0; i<max; i++) {
        if (PLL1_MCLK_Map[i].pll1 == pll1) {
            curr_mclk = PLL1_MCLK_Map[i].mclk; 
            break;
        }
    }

    return (i>=max)?(-1):(0);
}

//-------------------adda308_miscdev-------------------------------------
static int adda308_miscdev_open(struct inode *inode, struct file *file)
{
	
	int ret = 0;
	struct adda308_dev_t *pdev = NULL;

	/* lookup device */
	if(adda308_dev.miscdev.minor == iminor(inode)) {
		pdev = &adda308_dev;	
	}

	if(!pdev) {
		ret = -EINVAL;
		goto exit;
	}

	file->private_data = (void *)pdev;
	//printk("adda308_miscdev_open \n");
	
exit:
	return ret;

}

static long adda308_miscdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	struct audio_dev_ioc_vol vol_data;

	struct adda308_dev_t *pdev = (struct adda308_dev_t *)file->private_data;
	
	down(&pdev->lock);
		
	if(_IOC_TYPE(cmd) != AUDIO_DEV_IOC_MAGIC) {
        ret = -ENOIOCTLCMD;
        goto exit;
    }

    switch(cmd) {	
		case AUDIO_DEV_GET_SPEAKER_VOL:
			adda308_dav_get(&vol_data.data);
			ret = (copy_to_user((void __user *)arg, &vol_data, sizeof(vol_data))) ? (-EFAULT) : 0;
			break;
			
		case AUDIO_DEV_SET_SPEAKER_VOL:
			if(copy_from_user(&vol_data, (void __user *)arg, sizeof(vol_data))) {
                ret = -EFAULT;
                goto exit;
            }
			adda308_dav_set(vol_data.data);
			break;
			
		default :
			break;
   	}

exit:	

	up(&pdev->lock);
    return ret;
}

static int adda308_miscdev_release(struct inode *inode, struct file *file)
{	
	//printk("adda308_miscdev_release \n");
    return 0;
}

static struct file_operations adda308_miscdev_fops = {
    .owner          = THIS_MODULE,
    .open           = adda308_miscdev_open,
    .release        = adda308_miscdev_release,
    .unlocked_ioctl = adda308_miscdev_ioctl,
};

static int adda308_miscdev_init(void) 
{
	int ret=0;

	//printk("adda308_miscdev_open \n");
	memset(&adda308_dev.miscdev, 0, sizeof(struct miscdevice));
	
	/* create device node */
	adda308_dev.miscdev.name  = DEVICE_NAME;
	adda308_dev.miscdev.minor = MISC_DYNAMIC_MINOR;
	adda308_dev.miscdev.fops  = (struct file_operations *)&adda308_miscdev_fops;

    ret = misc_register(&adda308_dev.miscdev);
	if (ret) {
		printk("cannot register %s miscdev on (err=%d)\n", adda308_dev.miscdev.name, ret);
		adda308_dev.miscdev.name = 0;
	}

    return ret;
}

static void adda308_miscdev_exit(void) 
{
	//printk("adda308_miscdev_exit \n");
    misc_deregister(&adda308_dev.miscdev);
}
//--------------------------------------------------------
static int __init adda308_drv_init(void)
{
    memset(&priv, 0x0, sizeof(adda308_priv_t));

    /* init the clock and add the device .... */
    platform_init();

    if (adda308_find_mclk() < 0)
        panic("can not decide mclk base on PLL1(%d)\n", PLL1_CLK_IN);

    printk("ADDA308 current use MCLK=%d\n", curr_mclk);

    /* Register the driver */
	if (platform_driver_register(&adda308_driver)) {
		printk("Failed to register ADDA driver\n");
		return -ENODEV;
	}

    adda308_get_chip_id();

    if (output_mode == DA_SPEAKER_OUT)
        clk_pll2 = 0;       // speaker out, source from PLL1/cntp3 : 24.5454MHz
    else {
        clk_pll2 = 1;       // line out, source from PLL2 : 24MHz
    }

    // set SSP Main clock : 24MHz
    adda308_ssp_mclk_init();

    // select da source from ADDA or decoder
    adda308_da_select();
    // select SSP1 AD source
    adda308_select_ssp1_rec_source();


    // Change PMU setting
    if (input_mode == 2)
        adda308_PMU_Select_DMIC_Input(&DMIC_Reg[adda308_DMIC_DEF_PMU]);

    // init adda308
    adda308_i2s_init();

    adda308_proc_init();
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
			sema_init(&adda308_dev.lock, 1);
#else
			init_MUTEX(&adda308_dev.lock);
#endif

	//init adda308 ioctl
	adda308_miscdev_init();

    return 0;
}

static void adda308_drv_power_down(void)
{
	u32 base, tmp;
	base = priv.adda308_reg;
	/* 0x00 */
	tmp = *(volatile unsigned int *)(base + 0x0);
    tmp |= (0x1 << 18);            // disable ADC frontend power
    tmp |= (0x1 << 20);            // disable ADC power
    tmp |= (0x1 << 22);            // disable DAC power
   	tmp |= (0x1 << 27);			   //disable speaker power, to avoid power-up noise, enable it after SSP tx enable
    tmp |= (0x1 << 28);            // disable MIC bias power
    *(volatile unsigned int *)(base + 0x0) = tmp;

	/* 0x0C */
	tmp = *(volatile unsigned int *)(base + 0xc);
    tmp |= (0x1 << 8);          // line out power down
    *(volatile unsigned int *)(base + 0xc) = tmp;

	/* 0x10 */
	tmp = *(volatile unsigned int *)(base + 0x10);
	tmp |= (0x01 << 19);          // power down SCF in LOUT
	*(volatile unsigned int *)(base + 0x10) = tmp;
}

static void __exit adda308_drv_clearnup(void)
{
	/* power down adda function */
	adda308_drv_power_down();
	
    adda308_proc_remove();

    platform_driver_unregister(&adda308_driver);
	
	//unregister adda308 ioctl
	adda308_miscdev_exit();

    platform_exit();

    __iounmap((void *)priv.adda308_reg);
}

ADDA308_SSP_USAGE
ADDA308_Check_SSP_Current_Usage(void)
{
    //return (same_i2s_tx_rx==1)?(adda308_SSP0RX_SSP0TX):(ADDA203_SSP0RX_SSP1TX);
    if ((audio_out_enable[0] == 1) && (audio_out_enable[1] == 0))  // adda da source select SSP0
        return ADDA308_SSP0RX_SSP0TX;
    if ((audio_out_enable[0] == 0) && (audio_out_enable[1] == 1))  // adda da source select SSP1
        return ADDA308_SSP0RX_SSP1TX;
    if ((audio_out_enable[0] == 1) && (audio_out_enable[1] == 1))  // adda da source select SSP0, SSP1 for decoder
        return ADDA308_SSP0RX_SSP0TX;

    return 0;
}
EXPORT_SYMBOL(ADDA308_Check_SSP_Current_Usage);

static void
adda308_PMU_Select_DMIC_Input(pmuReg_t *reg)
{
    if (!reg) {
        panic("%s(%d) NULL reg!\n",__FUNCTION__,__LINE__);
        return;
    }

    if (ftpmu010_write_reg(priv.adda308_fd, reg->reg_off, reg->init_val, reg->bits_mask) < 0) {
        panic("%s(%d) setting offset(0x%x) failed.\n",__FUNCTION__,__LINE__,reg->reg_off);
        return;
    }

    printk("%s ok!\n",__FUNCTION__);
}

EXPORT_SYMBOL(adda302_set_ad_fs);
EXPORT_SYMBOL(adda302_set_da_fs);
EXPORT_SYMBOL(adda302_set_fs);
EXPORT_SYMBOL(adda302_set_ad_powerdown);
EXPORT_SYMBOL(adda302_set_da_powerdown);
EXPORT_SYMBOL(adda308_get_output_mode);
EXPORT_SYMBOL(ADDA308_Set_DA_Source);
EXPORT_SYMBOL(adda308_get_LDAV);
EXPORT_SYMBOL(adda308_get_LADV);
EXPORT_SYMBOL(adda308_get_LIV);

module_init(adda308_drv_init);
module_exit(adda308_drv_clearnup);
MODULE_LICENSE("GPL");
