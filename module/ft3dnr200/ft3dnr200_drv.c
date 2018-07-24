#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <linux/synclink.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include <log.h>
#include <video_entity.h>

#include "frammap_if.h"
#include "ft3dnr200.h"
#include "ft3dnr200_util.h"
#ifdef VIDEOGRAPH_INC
#include "ft3dnr200_vg.h"
#endif

#ifdef DRV_CFG_USE_KTHREAD
#include <linux/delay.h>
#include <linux/kthread.h>
#endif

#ifdef DRV_CFG_USE_TASKLET
struct tasklet_struct   add_table_tasklet;
void ft3dnr_add_table_task(unsigned long data);
static void ft3dnr_add_table_wakeup(void);
#else
#ifdef DRV_CFG_USE_KTHREAD
static int ft3dnr_add_table_thread(void *data);
static void ft3dnr_add_table_wakeup(void);
static struct task_struct *pthread_add_table = NULL;
static wait_queue_head_t wq_add_table;
static int event_add_table = 0;
static volatile int  pthread_ready = 0;
#endif /* DRV_CFG_USE_KTHREAD */
#endif

#define MODULE_NAME "DN" // De-Noise

/* module parameter */
/* declare YCSWAP number */
int src_yc_swap = 1;
module_param(src_yc_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(src_yc_swap, "source yc swap");

int src_cbcr_swap = 0;
module_param(src_cbcr_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(src_cbcr_swap, "source cbcr swap");

int dst_yc_swap = 1;
module_param(dst_yc_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dst_yc_swap, "destination yc swap");

int dst_cbcr_swap = 0;
module_param(dst_cbcr_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dst_cbcr_swap, "destination cbcr swap");

int ref_yc_swap = 1;
module_param(ref_yc_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ref_yc_swap, "reference yc swap");

int ref_cbcr_swap = 0;
module_param(ref_cbcr_swap, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ref_cbcr_swap, "reference cbcr swap");

int pwm = 0;
module_param(pwm, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pwm, "pwm");

char *res_cfg = "";
module_param(res_cfg, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(res_cfg, "set resolution configuration");

int max_minors = 0;
module_param(max_minors, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_minors, "set max minors driver supported");

char *config_path = "/mnt/mtd/";
module_param(config_path, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(config_path, "set path for vg config file");

int power_save = 0; // turn off the 3dnr clock when vg job empty, but under real
                    // world testing, the actural saved power is very little.
module_param(power_save, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(power_save, "enable/disable power saving mode");

int tmnr_en = 1;
module_param(tmnr_en, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(tmnr_en, "enable/disable temporal noise reduction");

extern int platform_init(void);
extern int platform_exit(void);
extern int platform_set_pwm(u32 pwm);
extern irqreturn_t ft3dnr_interrupt(int irq, void *devid);
extern u32 callback_period;

ft3dnr_priv_t priv;

static int clock_on = 0;
static int clock_off = 0;
static bool dump_job_flow = false;

int perf_period = 0;

static DECLARE_DELAYED_WORK(process_job_work, 0);
static struct proc_dir_entry *entity_proc;
static struct proc_dir_entry *version_proc;
static struct proc_dir_entry *memdump_proc;
static struct proc_dir_entry *jobflow_proc;
static struct proc_dir_entry *ispset_proc;

/* each resolution's width/height, reference from vg ioctl/spec_parser.c */
static struct res_base_info_t res_base_info[MAX_RES_IDX] = {
    { "8M",    ALIGN16_UP(3840), ALIGN16_UP(2160)},  // 3840 x 2160  : 8294400
    { "7M",    ALIGN16_UP(3264), ALIGN16_UP(2176)},  // 3264 x 2176  : 7102464
    { "6M",    ALIGN16_UP(3072), ALIGN16_UP(2048)},  // 3072 x 2048  : 6291456
    { "5M",    ALIGN16_UP(2592), ALIGN16_UP(1944)},  // 2596 x 1952  : 5067392
    { "4M",    ALIGN16_UP(2304), ALIGN16_UP(1728)},  // 2304 x 1728  : 3981312
    { "3M",    ALIGN16_UP(2048), ALIGN16_UP(1536)},  // 2048 x 1536  : 3145728
    { "2M",    ALIGN16_UP(1920), ALIGN16_UP(1080)},  // 1920 x 1088  : 2088960
    { "1080P", ALIGN16_UP(1920), ALIGN16_UP(1080)},  // 1920 x 1088  : 2088960
    { "1.3M",  ALIGN16_UP(1280), ALIGN16_UP(1024)},  // 1280 x 1024  : 1310720
    { "1.2M",  ALIGN16_UP(1280), ALIGN16_UP(960)},   // 1280 x 960   : 1228800
    { "1080I", ALIGN16_UP(1920), ALIGN16_UP(540)},   // 1920 x 544   : 1044480
    { "1M",    ALIGN16_UP(1280), ALIGN16_UP(800)},   // 1280 x 800   : 1024000
    { "720P",  ALIGN16_UP(1280), ALIGN16_UP(720)},   // 1280 x 720   : 921600
    { "960H",  ALIGN16_UP(960),  ALIGN16_UP(576)},   // 960  x 576   : 552960
    { "SVGA",  ALIGN16_UP(800),  ALIGN16_UP(600)},   // 800  x 608   : 486400
    { "720I",  ALIGN16_UP(1280), ALIGN16_UP(360)},   // 1280 x 368   : 471040
    { "D1",    ALIGN16_UP(720),  ALIGN16_UP(576)},   // 720  x 576   : 414720
    { "VGA",   ALIGN16_UP(640),  ALIGN16_UP(480)},   // 640  x 480   : 307200
    { "nHD",   ALIGN16_UP(640),  ALIGN16_UP(360)},   // 640  x 368   : 235520
    { "2CIF",  ALIGN16_UP(360),  ALIGN16_UP(596)},   // 368  x 596   : 219328
    { "HD1",   ALIGN16_UP(720),  ALIGN16_UP(288)},   // 720  x 288   : 207360
    { "CIF",   ALIGN16_UP(360),  ALIGN16_UP(288)},   // 368  x 288   : 105984
    { "QCIF",  ALIGN16_UP(176),  ALIGN16_UP(144)},   // 176  x 144   : 25344
    { "NONE",                0,                0},   // reserve
    { "NONE",                0,                0},   // reserve
    { "NONE",                0,                0},   // reserve
    { "NONE",                0,                0},   // reserve
    { "NONE",                0,                0}    // reserve
};

static inline int ft3dnr_get_tmnr_motion_th_val(int type, int nrWidth, int nrHeight, int mult);
void ft3dnr_proc_remove(void);
static int ft3dnr_parse_gmlib_cfg(void);
static int ft3dnr_parse_module_param_res_cfg(char *line);

int ft3dnr_is_dump_job_flow(void)
{
    return dump_job_flow;
}

void ft3dnr_add_clock_on(void)
{
    clock_on++;
}

void ft3dnr_add_clock_off(void)
{
    clock_off++;
}

u32 ft3dnr_get_ip_base(void)
{
    return priv.engine.ft3dnr_reg;
}

u8 ft3dnr_get_ip_irq(void)
{
    return priv.engine.irq;
}

/* Note: ft3dnr_init_one() will be run before ft3dnr_drv_init() */
static int ft3dnr_init_one(struct platform_device *pdev)
{
	struct resource *mem, *irq;
	unsigned long vaddr;
    const char *devname[FT3DNR200_DEV_MAX] = {"ft3dnr200"};

    //printk("%s(%d)\n", __FUNCTION__, pdev->id);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		printk("no mem resource?\n");
		return -ENODEV;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		printk("no irq resource?\n");
		return -ENODEV;
	}

    vaddr = (unsigned long)ioremap_nocache(mem->start, PAGE_ALIGN(mem->end - mem->start));
    if (unlikely(!vaddr))
        panic("%s, no virtual address! \n", __FUNCTION__);

    priv.engine.ft3dnr_reg = vaddr;
    priv.engine.irq = irq->start;

	/* hook irq */
    if (request_irq(priv.engine.irq, &ft3dnr_interrupt, 0,
                    devname[FT3DNR200_DEV_0], (void *)FT3DNR200_DEV_0) < 0) {
        printk("Unable to allocate IRQ %d\n", priv.engine.irq);
    } else {
        printk("request IRQ%d with ft3dnr_interrupt() ok!\n", priv.engine.irq);
    }

    /* pwm */
    if (pwm != 0) {
        if (platform_set_pwm(pwm) < 0)
            printk("failed to set pwm\n");
    }

    return 0;
}

static int ft3dnr_remove_one(struct platform_device *pdev)
{
    ft3dnr_proc_remove();
    free_irq(priv.engine.irq, (void *)FT3DNR200_DEV_0);
    __iounmap((void *)priv.engine.ft3dnr_reg);

    return 0;
}

static struct platform_driver ft3dnr200_driver = {
    .probe	= ft3dnr_init_one,
    .remove	= ft3dnr_remove_one,
	.driver = {
	        .name = "FT3DNR",
	        .owner = THIS_MODULE,
	    },
};

void ft3dnr_init_default_param(void)
{
    /* dma wc/rc wait value */
    priv.engine.wc_wait_value = 0;
    priv.engine.rc_wait_value = 0;

    /*********** CTRL ************/
    priv.default_param.ctrl.spatial_en = 0;
    priv.default_param.ctrl.temporal_en = 1;
    priv.default_param.ctrl.tnr_learn_en = 1; // synchronize to temporal_en
    priv.default_param.ctrl.ee_en = 1;
    priv.default_param.ctrl.tnr_rlt_w = 1;
    priv.engine.ycbcr.src_yc_swap = src_yc_swap;
    priv.engine.ycbcr.src_cbcr_swap = src_cbcr_swap;
    priv.engine.ycbcr.dst_yc_swap = dst_yc_swap;
    priv.engine.ycbcr.dst_cbcr_swap = dst_cbcr_swap;
    priv.engine.ycbcr.ref_yc_swap = ref_yc_swap;
    priv.engine.ycbcr.ref_cbcr_swap = ref_cbcr_swap;

    /*********** MRNR ************/
    /* Y L0 edg threshold*/
    priv.default_param.mrnr.Y_L_edg_th[0][0] = 208;
    priv.default_param.mrnr.Y_L_edg_th[0][1] = 260;
    priv.default_param.mrnr.Y_L_edg_th[0][2] = 224;
    priv.default_param.mrnr.Y_L_edg_th[0][3] = 192;
    priv.default_param.mrnr.Y_L_edg_th[0][4] = 187;
    priv.default_param.mrnr.Y_L_edg_th[0][5] = 172;
    priv.default_param.mrnr.Y_L_edg_th[0][6] = 172;
    priv.default_param.mrnr.Y_L_edg_th[0][7] = 172;
    /* Y L1 edg threshold*/
    priv.default_param.mrnr.Y_L_edg_th[1][0] = 93;
    priv.default_param.mrnr.Y_L_edg_th[1][1] = 115;
    priv.default_param.mrnr.Y_L_edg_th[1][2] = 99;
    priv.default_param.mrnr.Y_L_edg_th[1][3] = 82;
    priv.default_param.mrnr.Y_L_edg_th[1][4] = 75;
    priv.default_param.mrnr.Y_L_edg_th[1][5] = 74;
    priv.default_param.mrnr.Y_L_edg_th[1][6] = 74;
    priv.default_param.mrnr.Y_L_edg_th[1][7] = 74;
    /* Y L2 edg threshold*/
    priv.default_param.mrnr.Y_L_edg_th[2][0] = 43;
    priv.default_param.mrnr.Y_L_edg_th[2][1] = 52;
    priv.default_param.mrnr.Y_L_edg_th[2][2] = 43;
    priv.default_param.mrnr.Y_L_edg_th[2][3] = 36;
    priv.default_param.mrnr.Y_L_edg_th[2][4] = 32;
    priv.default_param.mrnr.Y_L_edg_th[2][5] = 31;
    priv.default_param.mrnr.Y_L_edg_th[2][6] = 31;
    priv.default_param.mrnr.Y_L_edg_th[2][7] = 31;
    /* Y L3 edg threshold*/
    priv.default_param.mrnr.Y_L_edg_th[3][0] = 21;
    priv.default_param.mrnr.Y_L_edg_th[3][1] = 23;
    priv.default_param.mrnr.Y_L_edg_th[3][2] = 19;
    priv.default_param.mrnr.Y_L_edg_th[3][3] = 16;
    priv.default_param.mrnr.Y_L_edg_th[3][4] = 14;
    priv.default_param.mrnr.Y_L_edg_th[3][5] = 13;
    priv.default_param.mrnr.Y_L_edg_th[3][6] = 13;
    priv.default_param.mrnr.Y_L_edg_th[3][7] = 13;
    /* Cb L edg threshold */
    priv.default_param.mrnr.Cb_L_edg_th[0] = 202;
    priv.default_param.mrnr.Cb_L_edg_th[1] = 186;
    priv.default_param.mrnr.Cb_L_edg_th[2] = 133;
    priv.default_param.mrnr.Cb_L_edg_th[3] = 81;
    /* Cr L edg threshold */
    priv.default_param.mrnr.Cr_L_edg_th[0] = 119;
    priv.default_param.mrnr.Cr_L_edg_th[1] = 110;
    priv.default_param.mrnr.Cr_L_edg_th[2] = 80;
    priv.default_param.mrnr.Cr_L_edg_th[3] = 49;
    /* Y L0 smooth threshold */
    priv.default_param.mrnr.Y_L_smo_th[0][0] = 109;
    priv.default_param.mrnr.Y_L_smo_th[0][1] = 137;
    priv.default_param.mrnr.Y_L_smo_th[0][2] = 118;
    priv.default_param.mrnr.Y_L_smo_th[0][3] = 101;
    priv.default_param.mrnr.Y_L_smo_th[0][4] = 98;
    priv.default_param.mrnr.Y_L_smo_th[0][5] = 91;
    priv.default_param.mrnr.Y_L_smo_th[0][6] = 91;
    priv.default_param.mrnr.Y_L_smo_th[0][7] = 91;
    /* Y L1 smooth threshold */
    priv.default_param.mrnr.Y_L_smo_th[1][0] = 66;
    priv.default_param.mrnr.Y_L_smo_th[1][1] = 82;
    priv.default_param.mrnr.Y_L_smo_th[1][2] = 70;
    priv.default_param.mrnr.Y_L_smo_th[1][3] = 59;
    priv.default_param.mrnr.Y_L_smo_th[1][4] = 53;
    priv.default_param.mrnr.Y_L_smo_th[1][5] = 53;
    priv.default_param.mrnr.Y_L_smo_th[1][6] = 53;
    priv.default_param.mrnr.Y_L_smo_th[1][7] = 53;
    /* Y L2 smooth threshold */
    priv.default_param.mrnr.Y_L_smo_th[2][0] = 36;
    priv.default_param.mrnr.Y_L_smo_th[2][1] = 43;
    priv.default_param.mrnr.Y_L_smo_th[2][2] = 35;
    priv.default_param.mrnr.Y_L_smo_th[2][3] = 30;
    priv.default_param.mrnr.Y_L_smo_th[2][4] = 26;
    priv.default_param.mrnr.Y_L_smo_th[2][5] = 26;
    priv.default_param.mrnr.Y_L_smo_th[2][6] = 26;
    priv.default_param.mrnr.Y_L_smo_th[2][7] = 26;
    /* Y L3 smooth threshold */
    priv.default_param.mrnr.Y_L_smo_th[3][0] = 21;
    priv.default_param.mrnr.Y_L_smo_th[3][1] = 23;
    priv.default_param.mrnr.Y_L_smo_th[3][2] = 19;
    priv.default_param.mrnr.Y_L_smo_th[3][3] = 16;
    priv.default_param.mrnr.Y_L_smo_th[3][4] = 14;
    priv.default_param.mrnr.Y_L_smo_th[3][5] = 13;
    priv.default_param.mrnr.Y_L_smo_th[3][6] = 13;
    priv.default_param.mrnr.Y_L_smo_th[3][7] = 13;
    /* Cb L smooth threshold */
    priv.default_param.mrnr.Cb_L_smo_th[0] = 126;
    priv.default_param.mrnr.Cb_L_smo_th[1] = 116;
    priv.default_param.mrnr.Cb_L_smo_th[2] = 83;
    priv.default_param.mrnr.Cb_L_smo_th[3] = 50;
    /* Cr L smooth threshold */
    priv.default_param.mrnr.Cr_L_smo_th[0] = 75;
    priv.default_param.mrnr.Cr_L_smo_th[1] = 69;
    priv.default_param.mrnr.Cr_L_smo_th[2] = 50;
    priv.default_param.mrnr.Cr_L_smo_th[3] = 30;
    /* Y L nr strength */
    priv.default_param.mrnr.Y_L_nr_str[0] = 11;
    priv.default_param.mrnr.Y_L_nr_str[1] = 11;
    priv.default_param.mrnr.Y_L_nr_str[2] = 11;
    priv.default_param.mrnr.Y_L_nr_str[3] = 11;
    /* C L nr strength */
    priv.default_param.mrnr.C_L_nr_str[0] = 13;
    priv.default_param.mrnr.C_L_nr_str[1] = 13;
    priv.default_param.mrnr.C_L_nr_str[2] = 13;
    priv.default_param.mrnr.C_L_nr_str[3] = 13;
    ft3dnr_nr_set_strength(DEF_MRNR_NR_STR);

    /*********** TMNR ************/
    priv.default_param.tmnr.Y_var = 10;
    priv.default_param.tmnr.Cb_var = 12;
    priv.default_param.tmnr.Cr_var = 12;
    if (priv.default_param.tmnr.Y_var >= 32)
        priv.default_param.tmnr.K = 3;
    else
        priv.default_param.tmnr.K = 2;
    priv.default_param.tmnr.auto_recover = 0;
    priv.default_param.tmnr.rapid_recover = 0;
    priv.default_param.tmnr.trade_thres = 64;
    priv.default_param.tmnr.suppress_strength = 18;
    priv.default_param.tmnr.NF = 5;
    priv.default_param.tmnr.var_offset = 2;
    priv.default_param.tmnr.motion_var = min(12, priv.default_param.tmnr.Y_var / 3);
    priv.default_param.tmnr.Motion_top_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TL,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_top_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TN,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_top_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TR,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_nrm_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NL,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_nrm_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NN,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_nrm_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NR,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_bot_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BL,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_bot_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BN,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);
    priv.default_param.tmnr.Motion_bot_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BR,FRM_HD_WIDTH,FRM_HD_HEIGHT,DEF_MOTION_TH_MULT);

    /*********** SP ************/
    priv.default_param.sp.nl_gain_enable = 1;
    priv.default_param.sp.edg_wt_enable = 0;
    priv.default_param.sp.rec_bright_clip = 128;
    priv.default_param.sp.rec_dark_clip = 128;
    priv.default_param.sp.rec_peak_gain = 16;
    priv.default_param.sp.hpf_enable[0] = 1;
    priv.default_param.sp.hpf_enable[1] = 1;
    priv.default_param.sp.hpf_enable[2] = 0;
    priv.default_param.sp.hpf_enable[3] = 0;
    priv.default_param.sp.hpf_enable[4] = 0;
    priv.default_param.sp.hpf_enable[5] = 0;
    priv.default_param.sp.hpf_use_lpf[0] = 0;
    priv.default_param.sp.hpf_use_lpf[1] = 0;
    priv.default_param.sp.hpf_use_lpf[2] = 0;
    priv.default_param.sp.hpf_use_lpf[3] = 0;
    priv.default_param.sp.hpf_use_lpf[4] = 0;
    priv.default_param.sp.hpf_use_lpf[5] = 0;
    priv.default_param.sp.hpf0_5x5_coeff[0] = -2;
    priv.default_param.sp.hpf0_5x5_coeff[1] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[2] = -10;
    priv.default_param.sp.hpf0_5x5_coeff[3] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[4] = -2;
    priv.default_param.sp.hpf0_5x5_coeff[5] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[6] = 0;
    priv.default_param.sp.hpf0_5x5_coeff[7] = 12;
    priv.default_param.sp.hpf0_5x5_coeff[8] = 0;
    priv.default_param.sp.hpf0_5x5_coeff[9] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[10] = -10;
    priv.default_param.sp.hpf0_5x5_coeff[11] = 12;
    priv.default_param.sp.hpf0_5x5_coeff[12] = 48;
    priv.default_param.sp.hpf0_5x5_coeff[13] = 12;
    priv.default_param.sp.hpf0_5x5_coeff[14] = -10;
    priv.default_param.sp.hpf0_5x5_coeff[15] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[16] = 0;
    priv.default_param.sp.hpf0_5x5_coeff[17] = 12;
    priv.default_param.sp.hpf0_5x5_coeff[18] = 0;
    priv.default_param.sp.hpf0_5x5_coeff[19] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[20] = -2;
    priv.default_param.sp.hpf0_5x5_coeff[21] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[22] = -10;
    priv.default_param.sp.hpf0_5x5_coeff[23] = -6;
    priv.default_param.sp.hpf0_5x5_coeff[24] = -2;
    priv.default_param.sp.hpf1_3x3_coeff[0] = -1;
    priv.default_param.sp.hpf1_3x3_coeff[1] = -2;
    priv.default_param.sp.hpf1_3x3_coeff[2] = -1;
    priv.default_param.sp.hpf1_3x3_coeff[3] = -2;
    priv.default_param.sp.hpf1_3x3_coeff[4] = 12;
    priv.default_param.sp.hpf1_3x3_coeff[5] = -2;
    priv.default_param.sp.hpf1_3x3_coeff[6] = -1;
    priv.default_param.sp.hpf1_3x3_coeff[7] = -2;
    priv.default_param.sp.hpf1_3x3_coeff[8] = -1;
    priv.default_param.sp.hpf2_1x5_coeff[0] = 0;
    priv.default_param.sp.hpf2_1x5_coeff[1] = 0;
    priv.default_param.sp.hpf2_1x5_coeff[2] = 1;
    priv.default_param.sp.hpf2_1x5_coeff[3] = 0;
    priv.default_param.sp.hpf2_1x5_coeff[4] = 0;
    priv.default_param.sp.hpf3_1x5_coeff[0] = 0;
    priv.default_param.sp.hpf3_1x5_coeff[1] = 0;
    priv.default_param.sp.hpf3_1x5_coeff[2] = 1;
    priv.default_param.sp.hpf3_1x5_coeff[3] = 0;
    priv.default_param.sp.hpf3_1x5_coeff[4] = 0;
    priv.default_param.sp.hpf4_5x1_coeff[0] = 0;
    priv.default_param.sp.hpf4_5x1_coeff[1] = 0;
    priv.default_param.sp.hpf4_5x1_coeff[2] = 1;
    priv.default_param.sp.hpf4_5x1_coeff[3] = 0;
    priv.default_param.sp.hpf4_5x1_coeff[4] = 0;
    priv.default_param.sp.hpf5_5x1_coeff[0] = 0;
    priv.default_param.sp.hpf5_5x1_coeff[1] = 0;
    priv.default_param.sp.hpf5_5x1_coeff[2] = 1;
    priv.default_param.sp.hpf5_5x1_coeff[3] = 0;
    priv.default_param.sp.hpf5_5x1_coeff[4] = 0;
    priv.default_param.sp.hpf_gain[0] = 80;
    priv.default_param.sp.hpf_gain[1] = 48;
    priv.default_param.sp.hpf_gain[2] = 0;
    priv.default_param.sp.hpf_gain[3] = 0;
    priv.default_param.sp.hpf_gain[4] = 0;
    priv.default_param.sp.hpf_gain[5] = 0;
    ft3dnr_sp_set_strength(DEF_EE_SP_STR);
    priv.default_param.sp.hpf_shift[0] = 7;
    priv.default_param.sp.hpf_shift[1] = 4;
    priv.default_param.sp.hpf_shift[2] = 0;
    priv.default_param.sp.hpf_shift[3] = 0;
    priv.default_param.sp.hpf_shift[4] = 0;
    priv.default_param.sp.hpf_shift[5] = 0;
    priv.default_param.sp.hpf_coring_th[0] = 2;
    priv.default_param.sp.hpf_coring_th[1] = 2;
    priv.default_param.sp.hpf_coring_th[2] = 0;
    priv.default_param.sp.hpf_coring_th[3] = 0;
    priv.default_param.sp.hpf_coring_th[4] = 0;
    priv.default_param.sp.hpf_coring_th[5] = 0;
    priv.default_param.sp.hpf_bright_clip[0] = 64;
    priv.default_param.sp.hpf_bright_clip[1] = 96;
    priv.default_param.sp.hpf_bright_clip[2] = 128;
    priv.default_param.sp.hpf_bright_clip[3] = 128;
    priv.default_param.sp.hpf_bright_clip[4] = 128;
    priv.default_param.sp.hpf_bright_clip[5] = 128;
    priv.default_param.sp.hpf_dark_clip[0] = 128;
    priv.default_param.sp.hpf_dark_clip[1] = 128;
    priv.default_param.sp.hpf_dark_clip[2] = 128;
    priv.default_param.sp.hpf_dark_clip[3] = 128;
    priv.default_param.sp.hpf_dark_clip[4] = 128;
    priv.default_param.sp.hpf_dark_clip[5] = 128;
    priv.default_param.sp.hpf_peak_gain[0] = 16;
    priv.default_param.sp.hpf_peak_gain[1] = 16;
    priv.default_param.sp.hpf_peak_gain[2] = 16;
    priv.default_param.sp.hpf_peak_gain[3] = 16;
    priv.default_param.sp.hpf_peak_gain[4] = 16;
    priv.default_param.sp.hpf_peak_gain[5] = 16;
    priv.default_param.sp.gau_5x5_coeff[0] = 0;
    priv.default_param.sp.gau_5x5_coeff[1] = 0;
    priv.default_param.sp.gau_5x5_coeff[2] = 0;
    priv.default_param.sp.gau_5x5_coeff[3] = 0;
    priv.default_param.sp.gau_5x5_coeff[4] = 0;
    priv.default_param.sp.gau_5x5_coeff[5] = 0;
    priv.default_param.sp.gau_5x5_coeff[6] = 1;
    priv.default_param.sp.gau_5x5_coeff[7] = 2;
    priv.default_param.sp.gau_5x5_coeff[8] = 1;
    priv.default_param.sp.gau_5x5_coeff[9] = 0;
    priv.default_param.sp.gau_5x5_coeff[10] = 0;
    priv.default_param.sp.gau_5x5_coeff[11] = 2;
    priv.default_param.sp.gau_5x5_coeff[12] = 4;
    priv.default_param.sp.gau_5x5_coeff[13] = 2;
    priv.default_param.sp.gau_5x5_coeff[14] = 0;
    priv.default_param.sp.gau_5x5_coeff[15] = 0;
    priv.default_param.sp.gau_5x5_coeff[16] = 1;
    priv.default_param.sp.gau_5x5_coeff[17] = 2;
    priv.default_param.sp.gau_5x5_coeff[18] = 1;
    priv.default_param.sp.gau_5x5_coeff[19] = 0;
    priv.default_param.sp.gau_5x5_coeff[20] = 0;
    priv.default_param.sp.gau_5x5_coeff[21] = 0;
    priv.default_param.sp.gau_5x5_coeff[22] = 0;
    priv.default_param.sp.gau_5x5_coeff[23] = 0;
    priv.default_param.sp.gau_5x5_coeff[24] = 0;
    priv.default_param.sp.gau_shift = 8;
    priv.default_param.sp.nl_gain_curve.index[0] = 3;
    priv.default_param.sp.nl_gain_curve.index[1] = 28;
    priv.default_param.sp.nl_gain_curve.index[2] = 69;
    priv.default_param.sp.nl_gain_curve.index[3] = 113;
    priv.default_param.sp.nl_gain_curve.value[0] = 92;
    priv.default_param.sp.nl_gain_curve.value[1] = 128;
    priv.default_param.sp.nl_gain_curve.value[2] = 109;
    priv.default_param.sp.nl_gain_curve.value[3] = 26;
    priv.default_param.sp.nl_gain_curve.slope[0] = 3925;
    priv.default_param.sp.nl_gain_curve.slope[1] = 184;
    priv.default_param.sp.nl_gain_curve.slope[2] = -59;
    priv.default_param.sp.nl_gain_curve.slope[3] = -241;
    priv.default_param.sp.nl_gain_curve.slope[4] = 0;
    priv.default_param.sp.edg_wt_curve.index[0] = 5;
    priv.default_param.sp.edg_wt_curve.index[1] = 32;
    priv.default_param.sp.edg_wt_curve.index[2] = 64;
    priv.default_param.sp.edg_wt_curve.index[3] = 200;
    priv.default_param.sp.edg_wt_curve.value[0] = 16;
    priv.default_param.sp.edg_wt_curve.value[1] = 128;
    priv.default_param.sp.edg_wt_curve.value[2] = 128;
    priv.default_param.sp.edg_wt_curve.value[3] = 128;
    priv.default_param.sp.edg_wt_curve.slope[0] = 409;
    priv.default_param.sp.edg_wt_curve.slope[1] = 530;
    priv.default_param.sp.edg_wt_curve.slope[2] = 0;
    priv.default_param.sp.edg_wt_curve.slope[3] = 0;
    priv.default_param.sp.edg_wt_curve.slope[4] = -81;

    memcpy(&priv.isp_param, &priv.default_param, sizeof(ft3dnr_global_t));
    priv.isp_param.ctrl.spatial_en = 1;
    priv.isp_param.ctrl.tnr_learn_en = 1;

    ft3dnr_init_global();
}

void ft3dnr_joblist_add(ft3dnr_job_t *job)
{
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_LOCK;
#endif

    list_add_tail(&job->job_list, &priv.engine.qlist);

#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_UNLOCK;
#endif
}

static void ft3dnr_set_mrnr_block_free(ft3dnr_job_t *job)
{
    int blockNum = job->ll_blk.mrnr_num;

    if ((priv.engine.mrnr_table >> blockNum & 0x1) == 0)
        printk("[3DNR] error!! mrnr block %d should be used\n", blockNum);

    priv.engine.mrnr_table ^= (0x1 << blockNum);
    priv.engine.mrnr_cnt++;
}

/*
 *  set ft3dnr link list memory block as used
 */
static inline void ft3dnr_set_mrnr_block_used(ft3dnr_job_t *job, int useBlock)
{
    priv.engine.mrnr_table |= (0x1<<useBlock);
    priv.engine.mrnr_cnt--;
    job->ll_blk.mrnr_num = useBlock;
}

/*
 * get ft3dnr link list memory block number, blk = 0 --> free, blk = 1 --> used
 */
int ft3dnr_get_next_free_mrnr_block(ft3dnr_job_t *job)
{
    int freeBlock=-1;
    int blkIdx;

    for (blkIdx=0; blkIdx<MAX_MRNR_BLK; blkIdx++) {
        if ((priv.engine.mrnr_table >> blkIdx & 0x1) == 0) {
            freeBlock = blkIdx;
            break;
        }
    }

    if (freeBlock == -1) {
        printk("[3DNR] error, can't get free mrnr\n");
        return -1;
    }

    ft3dnr_set_mrnr_block_used(job, freeBlock);

    return 0;
}

static void ft3dnr_set_sp_block_free(ft3dnr_job_t *job)
{
    int spNum = job->ll_blk.sp_num;
    u8 spTable = priv.engine.sp_table;

    if ((spTable >> spNum & 0x1) == 0)
        printk("[3DNR] error!! SP block %d should be used\n", spNum);

    priv.engine.sp_table ^= (0x1 << spNum);
    priv.engine.sp_cnt++;
}

static inline void ft3dnr_set_sp_block_used(ft3dnr_job_t *job, int useBlock)
{
    priv.engine.sp_table |= (0x1 << useBlock);
    priv.engine.sp_cnt--;
    job->ll_blk.sp_num = useBlock;
}

int ft3dnr_get_free_sp_block(ft3dnr_job_t *job)
{
    int freeBlock = -1;
    int blkIdx;

    for (blkIdx=0; blkIdx<MAX_SP_BLK; blkIdx++) {
        if ((priv.engine.sp_table >> blkIdx & 0x1) == 0) {
            freeBlock = blkIdx;
            break;
        }
    }

    if (freeBlock == -1) {
        printk("[3DNR] error, can't get free sp block\n");
        return -1;
    }

    ft3dnr_set_sp_block_used(job, freeBlock);

    return 0;
}

static void ft3dnr_set_ll_block_free(ft3dnr_job_t *job)
{
    int blockNum = job->ll_blk.blk_num;

    if ((priv.engine.sram_table >> blockNum & 0x1) == 0)
        printk("[3DNR] error!! ll block %d should be used\n", blockNum);

    priv.engine.sram_table ^= (0x1 << blockNum);
    priv.engine.blk_cnt++;
}

/*
 *  set ft3dnr link list memory block as used
 */
static inline void ft3dnr_set_ll_block_used(ft3dnr_job_t *job, int useBlock)
{
    priv.engine.sram_table |= (0x1 << useBlock);
    priv.engine.blk_cnt--;
    job->ll_blk.blk_num = useBlock;
}

/*
 * get ft3dnr link list memory block number, blk = 0 --> free, blk = 1 --> used
 */
int ft3dnr_get_next_free_ll_block(ft3dnr_job_t *job)
{
    int blkIdx;
    int freeBlock, nextFreeBlock=-1;

    freeBlock = priv.engine.null_blk;

    // Error Checking
    if ((priv.engine.sram_table >> freeBlock & 0x1) == 1) {
        printk("[3DNR] error!! this blk %d should be free\n", freeBlock);
        return -1;
    }

    job->ll_blk.status_addr = freeBlock * FT3DNR_LL_MEM_BLOCK;
    ft3dnr_set_ll_block_used(job, freeBlock);

    for (blkIdx=0; blkIdx<MAX_LL_BLK; blkIdx++) {
        if ((priv.engine.sram_table >> blkIdx & 0x1) == 0) {
            nextFreeBlock = blkIdx;
            break;
        }
    }

    if (nextFreeBlock == -1) {
        printk("[3DNR] error, can't get free blk\n");
        return -1;
    }

    job->ll_blk.next_blk = nextFreeBlock;
    priv.engine.null_blk = nextFreeBlock;

    return 0;
}

int ft3dnr_write_table(ft3dnr_job_t *job, int chain_cnt, int last_job)
{
    bool dump_job_flow = IS_DUMP_JOB_FLOW;
    unsigned long flags = 0;

    if (perf_period) {
        static unsigned long j1 = 0, j2 = 0;
        static u32 f_cnt[MAX_CH_NUM];
        int i;

        if (job->chan_id < MAX_CH_NUM) {
            f_cnt[job->chan_id]++;
            j2 = jiffies;
            if (time_after(j2, j1)) {
                if ((j2 - j1) >= (perf_period * HZ)) {
                    for (i = 0; i < MAX_CH_NUM; i++) {
                        if (f_cnt[i]) {
                            printk(" [%s]: ch%d(%4dx%4d) job in  ==> %u frames in %u ms\n", MODULE_NAME, i,
                                    priv.chan_param[i].dim.src_bg_width, priv.chan_param[i].dim.src_bg_height,
                                    f_cnt[i], jiffies_to_msecs(j2-j1));
                            f_cnt[i] = 0;
                        }
                    }
                    j1 = j2;
                }
            } else {
                j1 = j2;
                memset(f_cnt, 0, sizeof(f_cnt));
            }
        } else {
            printk("%s(%s): chan id %d exceeds max %d\n", DRIVER_NAME, __func__, job->chan_id, MAX_CH_NUM - 1);
        }
    }

    DRV_SPIN_LOCK(flags);
    if (ft3dnr_get_next_free_ll_block(job) < 0) {
        DRV_SPIN_UNLOCK(flags);
        return -1;
    }
    if (ft3dnr_get_next_free_mrnr_block(job) < 0) {
        DRV_SPIN_UNLOCK(flags);
        return -1;
    }
    if (ft3dnr_get_free_sp_block(job) < 0) {
        DRV_SPIN_UNLOCK(flags);
        return -1;
    }
    DRV_SPIN_UNLOCK(flags);

    JOB_FLOW_DBG("next free LL block -> %d\n", job->ll_blk.blk_num);
    JOB_FLOW_DBG("next free MRNR block -> %d\n", job->ll_blk.mrnr_num);
    JOB_FLOW_DBG("next free SP block -> %d\n", job->ll_blk.sp_num);

    if (ft3dnr_set_lli_blk(job, chain_cnt, last_job) < 0)
        return -1;

    list_move_tail(&job->job_list, &priv.engine.slist);
    job->status = JOB_STS_SRAM;

    JOB_FLOW_DBG("job(0x%x) moved to slist\n", job->job_id);

    /* update node from V.G status */
    if (job->fops && job->fops->update_status)
        job->fops->update_status(job, JOB_STS_SRAM);

    return 0;
}

int get_job_cnt(void)
{
    int blk_cnt=0, mrnr_cnt=0, sp_cnt=0, job_cnt=0;

    blk_cnt = priv.engine.blk_cnt;
    mrnr_cnt = priv.engine.mrnr_cnt;
    sp_cnt = priv.engine.sp_cnt;

    if (mrnr_cnt == 0)
        job_cnt = 0;
    if (mrnr_cnt >= 1 && sp_cnt >= 1 && blk_cnt >= 2)
        job_cnt = 1;
    if (mrnr_cnt >= 2 && sp_cnt >= 2 && blk_cnt >= 3)
        job_cnt = 2;

    return job_cnt;
}

/*
 * workqueue to add jobs to link list memory
 */
void ft3dnr_add_table(void)
{
    ft3dnr_job_t *job, *ne;
    job_node_t *node;
    int job_cnt = 0, last_job = 0, chain_cnt = 0;
    int chain_job = 0;
    unsigned long flags = 0;
    bool dump_job_flow = IS_DUMP_JOB_FLOW;
    ft3dnr_dma_t    *dma;

    // get currently processable count of 3DNR engine
#ifdef DRV_CFG_USE_TASKLET
    DRV_SPIN_LOCK(flags);
#else
    DRV_SEMA_LOCK;
#endif

    job_cnt = get_job_cnt();

    JOB_FLOW_DBG("currently affordable job cnt -> %d\n", job_cnt);

    if (job_cnt > 0) {
        // check the process count with queue list
        list_for_each_entry_safe(job, ne, &priv.engine.qlist, job_list) {
            if (job->status != JOB_STS_QUEUE)
                continue;

            chain_job++;

            if (chain_job < job_cnt)
                continue;
            else
                break;
        }

        chain_cnt = chain_job;

        // write 3DNR parameter to link-list table
        if (chain_job > 0) {
            list_for_each_entry_safe(job, ne, &priv.engine.qlist, job_list) {
                if (job->status != JOB_STS_QUEUE)
                    continue;

                chain_job--;

                if (chain_job == 0)
                    last_job = 1;
                else
                    last_job = 0;

                ft3dnr_write_table(job, chain_cnt, last_job);

                if (chain_job == 0)
                    break;
            }
        }

#ifndef DRV_CFG_USE_TASKLET
        DRV_SPIN_LOCK(flags);
#endif
        if (!ENGINE_BUSY()) {
            if (!list_empty(&priv.engine.slist)) {
                if (priv.engine.ll_mode == 1) {
                    mark_engine_start();
                    ft3dnr_write_status();
                }
                /* first time to trigger 3DNR, set ll_fire */
                if (priv.engine.ll_mode == 0) {
                    ft3dnr_write_status();
                    mark_engine_start();
                    if (ft3dnr_fire() < 0)
                        printk("fail to fire!\n");
                    else
                        priv.engine.ll_mode = 1;
                    JOB_FLOW_DBG("job(0x%x) has fired!\n", job->job_id);
                }
                // move jobs from sram list into working list
                list_for_each_entry_safe(job, ne, &priv.engine.slist, job_list) {
                    list_move_tail(&job->job_list, &priv.engine.wlist);
                    job->status = JOB_STS_ONGOING;

                    dma = &job->param.dma;
                    node = (job_node_t *)job->private;
                    DEBUG_M(LOG_WARNING, node->engine, node->minor, "buffer dst=0x%08x src=0x%08x var=0x%08x ee=0x%08x mot=0x%08x ref_r=0x%08x ref_w=0x%08x  job ongoing\n",
                        dma->dst_addr, dma->src_addr, dma->var_addr, dma->ee_addr, dma->mot_addr, dma->ref_r_addr, dma->ref_w_addr);

                    /* update node status */
                    if (job->fops && job->fops->update_status)
                        job->fops->update_status(job, JOB_STS_ONGOING);
                }
                LOCK_ENGINE();
            }
        }
#ifndef DRV_CFG_USE_TASKLET
        DRV_SPIN_UNLOCK(flags);
#endif
    }

#ifdef DRV_CFG_USE_TASKLET
    DRV_SPIN_UNLOCK(flags);
#else
    DRV_SEMA_UNLOCK;
#endif
}

void ft3dnr_stop_channel(int chanID)
{
    ft3dnr_job_t *job, *ne, *parent, *job1, *ne1;
#ifdef DRV_CFG_USE_TASKLET
    unsigned long flags;
#endif

#ifdef DRV_CFG_USE_TASKLET
    DRV_SPIN_LOCK(flags);
#else
    DRV_SEMA_LOCK;
#endif

    list_for_each_entry_safe(job, ne, &priv.engine.qlist, job_list) {
        if (job->chan_id == chanID) {
            parent = job->parent;
            if (parent->status != JOB_STS_QUEUE)
                continue;

            list_for_each_entry_safe(job1, ne1, &priv.engine.qlist, job_list) {
                if (job1->parent == parent)
                    job1->status = JOB_STS_FLUSH;
            }
        }
    }

    list_for_each_entry_safe(job, ne, &priv.engine.qlist, job_list) {
        if (job->status == JOB_STS_FLUSH) {
            if (job->fops && job->fops->update_status)
                job->fops->update_status(job, JOB_STS_FLUSH);
            list_del(&job->job_list);
            kmem_cache_free(priv.job_cache, job);
            MEMCNT_SUB(&priv, 1);
        }
    }

#ifdef DRV_CFG_USE_TASKLET
    DRV_SPIN_UNLOCK(flags);
#else
    DRV_SEMA_UNLOCK;
#endif
}



/*
 * free job from ISR, free blocks, and callback to V.G
 */
void ft3dnr_job_free(ft3dnr_job_t *job)
{
    ft3dnr_set_ll_block_free(job);
    ft3dnr_set_mrnr_block_free(job);
    ft3dnr_set_sp_block_free(job);

    if (job->need_callback) {
        if (job->fops && job->fops->callback)
            job->fops->callback(job);
    }

    list_del(&job->job_list); //remove from working list (priv.engine.wlist)
    kmem_cache_free(priv.job_cache, job);
    MEMCNT_SUB(&priv, 1);

#ifdef DRV_CFG_USE_TASKLET
    ft3dnr_add_table_wakeup();
#else
#ifdef DRV_CFG_USE_KTHREAD
    ft3dnr_add_table_wakeup();
#else
    PREPARE_DELAYED_WORK(&process_job_work, (void *)ft3dnr_add_table);
    queue_delayed_work(priv.job_workq, &process_job_work, callback_period);
#endif
#endif
}

void ft3dnr_put_job(void)
{
#ifdef DRV_CFG_USE_TASKLET
    ft3dnr_add_table_wakeup();
#else
#ifdef DRV_CFG_USE_KTHREAD
    ft3dnr_add_table_wakeup();
#else
    PREPARE_DELAYED_WORK(&process_job_work, (void *)ft3dnr_add_table);
    queue_delayed_work(priv.job_workq, &process_job_work, callback_period);
#endif
#endif
}

static void ft3dnr_lli_timeout(unsigned long data)
{
    u32 base;

    printk("FT3DNR timeout !!\n");

    base = priv.engine.ft3dnr_reg;

    ft3dnr_dump_reg();
    ft3dnr_sw_reset();
    UNLOCK_ENGINE();
    mark_engine_finish();
    damnit(MODULE_NAME);
}

static int ft3dnr_get_var_size(int nrWidth, int nrHeight)
{
    int varSize=-1;
    int x,y;

    if ((nrWidth<=0)||(nrHeight<=0))
        goto out;

    x = (((nrWidth-100)+(VARBLK_WIDTH-1))/VARBLK_WIDTH)+1;
    y = (((((nrHeight+(VARBLK_HEIGHT-1))/VARBLK_HEIGHT)+1)+7)/8)*8;

    varSize = x*y;

out:
    return varSize;
}

static int ft3dnr_var_buf_alloc(void)
{
    int ret = 0;
    int res_idx=0;
    int var_sz;
    char buf_name[20];
    struct frammap_buf_info buf_info;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt ; res_idx++) {
        buf = &(priv.res_cfg[res_idx].var_buf);

        var_sz = ft3dnr_get_var_size(priv.res_cfg[res_idx].width, priv.res_cfg[res_idx].height);

        memset(&buf_info, 0, sizeof(struct frammap_buf_info));

        buf_info.size = var_sz;
        buf_info.alloc_type = ALLOC_NONCACHABLE;
        sprintf(buf_name, "ft3dnr_var.%d", res_idx);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
        buf_info.name = buf_name;
#endif

        ret = frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info);
        if (ret < 0) {
            printk("FT3DNR var.%d buffer allocate failed!\n", res_idx);
            goto exit;
        }

        buf->vaddr = (void *)buf_info.va_addr;
        buf->paddr = buf_info.phy_addr;
        buf->size  = buf_info.size;
        strcpy(buf->name, buf_name);

        printk("FT3DNR var.%d buf size %#x width %d height %d phy addr %#x\n", res_idx, buf->size,
                priv.res_cfg[res_idx].width, priv.res_cfg[res_idx].height, buf->paddr);

        /* init buffer */
        memset(buf->vaddr, 0x8, buf->size);
    }

exit:
    return ret;
}

void ft3dnr_var_buf_free(void)
{
    int res_idx = 0;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt; res_idx++) {
        buf = &(priv.res_cfg[res_idx].var_buf);
        if (buf->vaddr) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
            frm_free_buf_ddr(buf->vaddr);
#else
            struct frammap_buf_info info;

            memset(&info,0x0,sizeof(struct frammap_buf_info));
            info.va_addr  = (u32)(buf->vaddr);
            info.phy_addr = buf->paddr;
            info.size     = buf->size;
            frm_free_buf_ddr(&info);
#endif
        }
    }
}

static int ft3dnr_sp_buf_alloc(void)
{
    int ret = 0;
    int height, bufSize;
    char buf_name[20];
    struct frammap_buf_info bufInfo;

    height = priv.res_cfg[0].height; // refer to largest height of resolution
    bufSize = ((((height+31)/32)+1)*32)*8*2;

    memset(&bufInfo, 0, sizeof(struct frammap_buf_info));
    bufInfo.size = bufSize;
    bufInfo.alloc_type = ALLOC_NONCACHABLE;
    sprintf(buf_name, "ft3dnr_sp.%d", 0);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    bufInfo.name = buf_name;
#endif

    ret = frm_get_buf_ddr(DDR_ID_SYSTEM, &bufInfo);
    if (ret < 0) {
        printk("FT3DNR sharpen buffer allocate failed!\n");
        goto exit;
    }

    priv.engine.sp_buf.vaddr = (void *)bufInfo.va_addr;
    priv.engine.sp_buf.paddr = bufInfo.phy_addr;
    priv.engine.sp_buf.size  = bufInfo.size;
    strcpy(priv.engine.sp_buf.name, buf_name);

    printk("FT3DNR sp.%d buf size %#x width %d height %d phy addr %#x\n", 0, priv.engine.sp_buf.size,
            priv.res_cfg[0].width, priv.res_cfg[0].height, priv.engine.sp_buf.paddr);

    /* init buffer */
    memset(priv.engine.sp_buf.vaddr, 0x0, priv.engine.sp_buf.size);

exit:
    return ret;
}

static void ft3dnr_sp_buf_free(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    frm_free_buf_ddr(priv.engine.sp_buf.vaddr);
#else
    struct frammap_buf_info buf_info;

    buf_info.va_addr  = (u32)priv.engine.sp_buf.vaddr;
    buf_info.phy_addr = priv.engine.sp_buf.paddr;
    buf_info.size     = priv.engine.sp_buf.size;
    frm_free_buf_ddr(&buf_info);
#endif
}

static int ft3dnr_mot_buf_alloc(void)
{
    int ret = 0;
    int res_idx=0;
    char buf_name[20];
    struct frammap_buf_info buf_info;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt ; res_idx++) {
        buf = &(priv.res_cfg[res_idx].mot_buf);

        memset(&buf_info, 0, sizeof(struct frammap_buf_info));

        buf_info.size = MOT_BUF_SIZE;
        buf_info.alloc_type = ALLOC_NONCACHABLE;
        sprintf(buf_name, "ft3dnr_mot.%d", res_idx);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
        buf_info.name = buf_name;
#endif

        ret = frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info);
        if (ret < 0) {
            printk("FT3DNR mot.%d buffer allocate failed!\n", res_idx);
            goto exit;
        }

        buf->vaddr = (void *)buf_info.va_addr;
        buf->paddr = buf_info.phy_addr;
        buf->size  = buf_info.size;
        strcpy(buf->name, buf_name);

        printk("FT3DNR mot.%d buf size %#x width %d height %d phy addr %#x\n", res_idx, buf->size,
                priv.res_cfg[res_idx].width, priv.res_cfg[res_idx].height, buf->paddr);

        /* init buffer */
        memset(buf->vaddr, 0x1, buf->size);
    }

exit:
    return ret;
}

void ft3dnr_mot_buf_free(void)
{
    int res_idx = 0;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt; res_idx++) {
        buf = &(priv.res_cfg[res_idx].mot_buf);
        if (buf->vaddr) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
            frm_free_buf_ddr(buf->vaddr);
#else
            struct frammap_buf_info info;

            memset(&info,0x0,sizeof(struct frammap_buf_info));
            info.va_addr  = (u32)(buf->vaddr);
            info.phy_addr = buf->paddr;
            info.size     = buf->size;
            frm_free_buf_ddr(&info);
#endif
        }
    }
}

static int ft3dnr_ref_buf_alloc(void)
{
    int ret = 0, res_idx;
    char buf_name[20];
    struct frammap_buf_info buf_info;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt; res_idx++) {
        buf = &(priv.res_cfg[res_idx].ref_buf);

        memset(&buf_info, 0, sizeof(struct frammap_buf_info));

        buf_info.size = priv.res_cfg[res_idx].size * 2; //YUV422
        buf_info.alloc_type = ALLOC_NONCACHABLE;
        sprintf(buf_name, "ft3dnr_ref.%d", res_idx);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
        buf_info.name = buf_name;
#endif

        ret = frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info);
        if (ret < 0) {
            printk("FT3DNR ref.%d buffer allocate failed!\n", res_idx);
            goto exit;
        }

        buf->vaddr = (void *)buf_info.va_addr;
        buf->paddr = buf_info.phy_addr;
        buf->size  = buf_info.size;
        strcpy(buf->name, buf_name);

        printk("FT3DNR ref.%d buf size %#x width %d height %d phy addr %#x\n", res_idx, buf->size,
                priv.res_cfg[res_idx].width, priv.res_cfg[res_idx].height, buf->paddr);

        /* init buffer */
        memset(buf->vaddr, 0x0, buf->size);
    }

exit:
    return ret;
}

static void ft3dnr_ref_buf_free(void)
{
    int res_idx = 0;
    ft3dnr_dma_buf_t *buf = NULL;

    for (res_idx = 0; res_idx < priv.res_cfg_cnt; res_idx++) {
        buf = &(priv.res_cfg[res_idx].ref_buf);
        if (buf->vaddr) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
            frm_free_buf_ddr(buf->vaddr);
#else
            struct frammap_buf_info info;

            memset(&info,0x0,sizeof(struct frammap_buf_info));
            info.va_addr  = (u32)(buf->vaddr);
            info.phy_addr = buf->paddr;
            info.size     = buf->size;
            frm_free_buf_ddr(&info);
#endif
        }
    }
}

static int proc_version_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nTHDNR200 version = %s\n", VERSION);
    seq_printf(sfile, "set clock_on times = %d\n", clock_on);
    seq_printf(sfile, "set clock_off times = %d\n\n", clock_off);

    return 0;
}

static int proc_mem_dump(struct seq_file *sfile, void *v)
{
    int memIdx;
    u32 ipBase, size=0x0208;
    unsigned long flags;

    ipBase = ft3dnr_get_ip_base();

    // print header
    seq_printf(sfile, "\n%-12s%-11s%-11s%-11s%-11s\n","Address","0x00","0x04","0x08","0x0C");

    seq_printf(sfile, "--------------------------------------------------------\n");

    DRV_SPIN_LOCK(flags);

    // print content
    for (memIdx=0; memIdx<size; memIdx+=0x10) {
        seq_printf(sfile, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x\n",
           (ipBase+memIdx),*(u32 *)(ipBase+memIdx),*(u32 *)(ipBase+memIdx+0x4),
           *(u32 *)(ipBase+memIdx+0x8),*(u32 *)(ipBase+memIdx+0xC));
    }

    DRV_SPIN_UNLOCK(flags);

    seq_printf(sfile, "\n");

    return 0;
}

static int proc_job_flow_dump(struct seq_file *sfile, void *v)
{
    seq_printf(sfile,"\ndump_job_flow -> %d\n\n", dump_job_flow);

    return 0;
}

static ssize_t proc_job_flow_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int is_dump = 0;
    char value_str[16] = {'\0'};
    //struct seq_file *sfile = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d \n", &is_dump);
    dump_job_flow = is_dump;

    return count;
}

static int proc_isp_set_dump(struct seq_file *sfile, void *v)
{
    int i;

    seq_printf(sfile, "offset    0x00     0x04     0x08     0x0C\n");
    seq_printf(sfile, "-------------------------------------------\n");

    for (i = 0; i < sizeof(priv.reg_ary); i += 4) {
        if (!(i % 0x10))
            seq_printf(sfile, "0x%04X ", i);
        seq_printf(sfile, " %08x", *(u32 *)&priv.curr_reg[i]);
        if (i && !((i + 4) % 0x10))
            seq_printf(sfile, "\n");
    }
    seq_printf(sfile, "\n");

    return 0;
}

static int proc_version_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_version_show, PDE(inode)->data);
}

static int proc_mem_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_mem_dump, PDE(inode)->data);
}

static int proc_jobflow_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_job_flow_dump, PDE(inode)->data);
}

static int proc_ispset_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_isp_set_dump, PDE(inode)->data);
}

static struct file_operations proc_version_ops = {
    .owner   = THIS_MODULE,
    .open    = proc_version_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static struct file_operations proc_memdump_ops = {
    .owner   = THIS_MODULE,
    .open    = proc_mem_dump_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static struct file_operations proc_jobflow_ops = {
    .owner   = THIS_MODULE,
    .open    = proc_jobflow_open,
    .write   = proc_job_flow_write,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static struct file_operations proc_ispset_ops = {
    .owner   = THIS_MODULE,
    .open    = proc_ispset_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static int ft3dnr_proc_init(void)
{
    /* create proc */
    entity_proc = create_proc_entry(FT3DNR_PROC_NAME, S_IFDIR | S_IRUGO | S_IXUGO, NULL);
    if (entity_proc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        return -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    entity_proc->owner = THIS_MODULE;
#endif

    /* version */
    version_proc = create_proc_entry("version", S_IRUGO|S_IXUGO, entity_proc);
    if (version_proc == NULL) {
        printk("error to create %s/version proc\n", FT3DNR_PROC_NAME);
        return -EFAULT;
    }
    version_proc->proc_fops  = &proc_version_ops;
    version_proc->data       = (void *)&entity_proc;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    version_proc->owner      = THIS_MODULE;
#endif

    /* memory dump */
    memdump_proc = create_proc_entry("mem_dump", S_IRUGO|S_IXUGO, entity_proc);
    if (memdump_proc == NULL) {
        printk("error to create %s/mem_dump proc\n", FT3DNR_PROC_NAME);
        return -EFAULT;
    }
    memdump_proc->proc_fops  = &proc_memdump_ops;
    memdump_proc->data       = (void *)&entity_proc;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    memdump_proc->owner      = THIS_MODULE;
#endif

    /* job flow */
    jobflow_proc = create_proc_entry("job_flow", S_IRUGO|S_IXUGO, entity_proc);
    if (jobflow_proc == NULL) {
        printk("error to create %s/job_flow proc\n", FT3DNR_PROC_NAME);
        return -EFAULT;
    }
    jobflow_proc->proc_fops  = &proc_jobflow_ops;
    jobflow_proc->data       = (void *)&entity_proc;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    jobflow_proc->owner      = THIS_MODULE;
#endif

    /* isp set */
    ispset_proc = create_proc_entry("isp_set", S_IRUGO|S_IXUGO, entity_proc);
    if (ispset_proc == NULL) {
        printk("error to create %s/isp_set proc\n", FT3DNR_PROC_NAME);
        return -EFAULT;
    }
    ispset_proc->proc_fops  = &proc_ispset_ops;
    ispset_proc->data       = (void *)&entity_proc;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ispset_proc->owner      = THIS_MODULE;
#endif

    /* MEM */
    ft3dnr_mem_proc_init(entity_proc);
    /* CTRL */
    ft3dnr_ctrl_proc_init(entity_proc);
    /* MRNR */
    ft3dnr_mrnr_proc_init(entity_proc);
    /* TMNR */
    ft3dnr_tmnr_proc_init(entity_proc);
    /* EE */
    ft3dnr_ee_proc_init(entity_proc);
    /* DMA */
    ft3dnr_dma_proc_init(entity_proc);

    return 0;
}

void ft3dnr_proc_remove(void)
{
    if (version_proc != 0)
        remove_proc_entry(version_proc->name, entity_proc);
    if (memdump_proc != 0)
        remove_proc_entry(memdump_proc->name, entity_proc);
    if (jobflow_proc != 0)
        remove_proc_entry(jobflow_proc->name, entity_proc);
    if (ispset_proc != 0)
        remove_proc_entry(ispset_proc->name, entity_proc);

    /* MEM */
    ft3dnr_mem_proc_remove(entity_proc);
    /* CTRL */
    ft3dnr_ctrl_proc_remove(entity_proc);
    /* MRNR */
    ft3dnr_mrnr_proc_remove(entity_proc);
    /* TMNR */
    ft3dnr_tmnr_proc_remove(entity_proc);
    /* EE */
    ft3dnr_ee_proc_remove(entity_proc);
    /* DMA */
    ft3dnr_dma_proc_remove(entity_proc);

    if (entity_proc)
        remove_proc_entry(entity_proc->name, entity_proc->parent);
}

static int __init ft3dnr_drv_init(void)
{
    int ret = 0;

    printk("FT3DNR200 Version %s\n", VERSION);

    memset(&priv, 0x0, sizeof(ft3dnr_priv_t));

    INIT_LIST_HEAD(&priv.engine.qlist);
    INIT_LIST_HEAD(&priv.engine.slist);
    INIT_LIST_HEAD(&priv.engine.wlist);

    priv.engine.blk_cnt = MAX_LL_BLK;
    priv.engine.mrnr_cnt = MAX_MRNR_BLK;
    priv.engine.sp_cnt = MAX_SP_BLK;
    priv.engine.sram_table = 0xF0;
    priv.engine.mrnr_table = 0xFC;
    priv.engine.sp_table = 0xFC;

#ifdef DRV_CFG_USE_TASKLET
	/* tasklet */
	tasklet_init(&add_table_tasklet, ft3dnr_add_table_task, (unsigned long) 0);
#else
#ifdef DRV_CFG_USE_KTHREAD
    init_waitqueue_head(&wq_add_table);
    pthread_add_table = kthread_create(ft3dnr_add_table_thread, NULL, "ft3dnr_add_jobd");
    if (IS_ERR(pthread_add_table))
        panic("%s, create pthread_add_table fail ! \n", __func__);
    wake_up_process(pthread_add_table);
#else
    /* create workqueue */
    priv.job_workq = create_workqueue(MODULE_NAME);
    if (priv.job_workq == NULL) {
        printk("FT3DNR200: error in create workqueue! \n");
        return -1;
    }
#endif
#endif

    /* init the clock and add the device .... */
    platform_init();

    /* Register the driver */
	if (platform_driver_register(&ft3dnr200_driver)) {
		printk("Failed to register FT3DNR200 driver\n");
		return -ENODEV;
	}

    spin_lock_init(&priv.lock);

	/* init semaphore lock */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
    sema_init(&priv.sema_lock, 1);
#else
    init_MUTEX(&priv.sema_lock);
#endif

	/* create memory cache */
	priv.job_cache = kmem_cache_create(DRIVER_NAME, sizeof(ft3dnr_job_t), 0, 0, NULL);
    if (priv.job_cache == NULL)
        panic("%s, no memory for job_cache! \n", __func__);

    if (max_minors <= 0)
        max_minors = MAX_CH_NUM;

    if ((res_cfg != NULL) && (strlen(res_cfg) != 0)) {
        ret = ft3dnr_parse_module_param_res_cfg(res_cfg);
        if (ret)
            panic("%s(%s): incorrect value of module parameter res_cfg\n", DRIVER_NAME, __func__);
    } else {
        /* parse gmlib.cfg */
        ret = ft3dnr_parse_gmlib_cfg();
        if (ret) {
            panic("%s(%s): falied to parse vg config \"%s\"\n", DRIVER_NAME, __func__, GMLIB_FILENAME);
            return ret;
        }
    }

    priv.curr_max_dim.src_bg_width = 0;
    priv.curr_max_dim.src_bg_height = 0;

    priv.curr_reg = (u8 *) priv.reg_ary;

    priv.chan_param = (chan_param_t *) kzalloc(sizeof(chan_param_t) * (max_minors), GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(priv.chan_param))
        panic("%s: allocate memory fail for priv.chan_param(0x%p)!\n", __func__, priv.chan_param);

    if (ft3dnr_sp_buf_alloc() < 0) {
        printk("failed to alloc sp buf!\n");
        return -ENOMEM;
    }

    if (tmnr_en) {
        if (ft3dnr_mot_buf_alloc() < 0) {
            printk("failed to alloc mot buf!\n");
            return -ENOMEM;
        }

        if (ft3dnr_var_buf_alloc() < 0) {
            printk("failed to alloc var buf!\n");
            return -ENOMEM;
        }

        if (ft3dnr_ref_buf_alloc() < 0) {
            printk("failed to alloc ref buf!\n");
            return -ENOMEM;
        }
    }

#ifdef VIDEOGRAPH_INC
    ft3dnr_vg_init();
#endif

    ft3dnr_init_default_param();

    /* init timer */
    init_timer(&priv.engine.timer);
    priv.engine.timer.function = ft3dnr_lli_timeout;
    priv.engine.timer.data = FT3DNR200_DEV_0;
    priv.engine.timeout = msecs_to_jiffies(FT3DNR_LLI_TIMEOUT);

    /* init proc */
    ft3dnr_proc_init();

    return 0;
}

static void __exit ft3dnr_drv_exit(void)
{
#ifdef VIDEOGRAPH_INC
    ft3dnr_vg_driver_clearnup();
#endif

#ifdef DRV_CFG_USE_TASKLET
    tasklet_kill(&add_table_tasklet);
#else
#ifdef DRV_CFG_USE_KTHREAD
    if (pthread_add_table) {
        kthread_stop(pthread_add_table);
        /* wait thread to be terminated */
        while (pthread_ready) {
            msleep(10);
        }
    }
#else
    /* destroy workqueue */
    destroy_workqueue(priv.job_workq);
#endif
#endif

    platform_driver_unregister(&ft3dnr200_driver);

    /* delete timer */
    del_timer(&priv.engine.timer);

    /* destroy the memory cache */
    kmem_cache_destroy(priv.job_cache);

    ft3dnr_sp_buf_free();
    if (tmnr_en) {
        ft3dnr_var_buf_free();
        ft3dnr_mot_buf_free();
        ft3dnr_ref_buf_free();
    }

    kfree(priv.chan_param);
    kfree(priv.res_cfg);

    platform_exit();
}

#ifdef DRV_CFG_USE_TASKLET
/*
  *  The shorter ISR is better, so this process take charge of  callback to videograph
  *  return the job to the videograph
  */
void ft3dnr_add_table_task(unsigned long data)
{
    ft3dnr_add_table(); /* add job process */
}

static void ft3dnr_add_table_wakeup(void)
{
    tasklet_schedule(&add_table_tasklet);
}
#else
#ifdef DRV_CFG_USE_KTHREAD
static int ft3dnr_add_table_thread(void *data)
{
    int status;

    if (data) {}

    /* ready */
    pthread_ready = 1;

    do {
        status = wait_event_timeout(wq_add_table, event_add_table, msecs_to_jiffies(5*1000));
        if (status == 0)
            continue;   /* timeout */
        event_add_table = 0;

        if (!kthread_should_stop())
            ft3dnr_add_table(); /* add job process */
    } while(!kthread_should_stop());

    pthread_ready = 0;
    return 0;
}

static void ft3dnr_add_table_wakeup(void)
{
    event_add_table = 1;
    wake_up(&wq_add_table);
}
#endif /* DRV_CFG_USE_KTHREAD */
#endif

EXPORT_SYMBOL(ft3dnr_put_job);

static int getline(char *line, int size, struct file *fd, unsigned long long *offset)
{
    char ch;
    int lineSize = 0, ret;

    memset(line, 0, size);
    while ((ret = (int)vfs_read(fd, &ch, 1, offset)) == 1) {
        if (ch == 0xD) // CR
            continue;
        if (lineSize >= MAX_LINE_CHAR) {
            printk("Line buf is not enough %d! (%s)\n", MAX_LINE_CHAR, line);
            break;
        }
        line[lineSize++] = ch;
        if (ch == 0xA) // LF
            break;
    }
    return lineSize;
}

static int readline(struct file *fd, int size, char *line, unsigned long long *offset)
{
    int ret = 0;
    do {
        ret = getline(line, size, fd, offset);
    } while (((line[0] == 0xa)) && (ret != 0));
    return ret;
}

static int get_res_idx(char *res_name)
{
    int i;
    for (i = 0; i < MAX_RES_IDX; i++) {
        if (strcmp(res_base_info[i].name, res_name) == 0)
            return i;
    }
    return -1;
}

static int get_config_res_name(char *name, char *line, int offset, int size)
{
    int i = 0;
    while (',' == line[offset] || ' ' == line[offset]) {
    	if (offset >= size)
    		return -1;
    	offset++;
    }
    while ('/' != line[offset]) {
    	name[i] = line[offset];
    	i++;
    	offset++;
    }
    name[i] = '\0';
    return offset;
}

static int parse_res_cfg_line(char *line, int offset, int line_size, res_cfg_t *res_cfg_p, int *res_cfg_cnt)
{
    char res_name[7];
    int i, ii, ret = 0, idx;
    int res_chs, res_fps, ddr_idx;
    int res_cfg_idx = 0;

    i = offset;

    while (1) {
        i = get_config_res_name(res_name, line, i, line_size);
        if (i >= line_size)
            break;
        sscanf(&line[i], "/%d/%d/%d", &res_chs, &res_fps, &ddr_idx);
        idx = get_res_idx(res_name);

        if (idx >= 0) {
            for (ii = 0; ii < res_chs; ii++) {
                if (res_cfg_idx < MAX_CH_NUM) {
                    memcpy(res_cfg_p[res_cfg_idx].res_type, &res_name, sizeof(res_name));
                    res_cfg_p[res_cfg_idx].width = res_base_info[idx].width;
                    res_cfg_p[res_cfg_idx].height = res_base_info[idx].height;
                    res_cfg_p[res_cfg_idx].size = (u32) res_base_info[idx].width * (u32) res_base_info[idx].height;
                    res_cfg_p[res_cfg_idx].map_chan = MAP_CHAN_NONE;
                    res_cfg_idx++;
                } else {
                    panic("%s(%s): required channel number exceeds max %d\n", DRIVER_NAME, __func__, MAX_CH_NUM);
                    ret = -EINVAL;
                    break;
                }
            }
        } else {
            // if res_name not matched
            panic("%s(%s): res name \"%s\" not support\n", DRIVER_NAME, __func__, res_name);
        }
        while (',' != line[i] && i < line_size)  i++;
        if (i == line_size)
            break;
        if (ret)
            break;
    }

    if (ret == 0)
        *res_cfg_cnt = res_cfg_idx;

    return ret;
}

static void parse_additional_resolution(char *line, int offset, int line_size)
{
    char res_name[7];
    int res_width, res_height;
    int i, j;

    i = offset;

    while (1) {
        i = get_config_res_name(res_name, line, i, line_size);
        if (i >= line_size)
            break;
        sscanf(&line[i], "/%d/%d", &res_width, &res_height);
        res_width = ALIGN16_UP(res_width);
        res_height = ALIGN16_UP(res_height);

        for (j = 0; j < MAX_RES_IDX; j++) {
            if (strcmp(res_base_info[j].name, res_name) == 0) {
                if ((res_width * res_height) > (res_base_info[j].width * res_base_info[j].height)) {
                    res_base_info[j].width = res_width;
                    res_base_info[j].height = res_height;
                }
                break;
            } else if (strcmp(res_base_info[j].name, "NONE") == 0) {
                strcpy(res_base_info[j].name, res_name);
                res_base_info[j].width = res_width;
                res_base_info[j].height = res_height;
                break;
            }
        }
        if (j >= MAX_RES_IDX)
            panic("%s(%s): too many additional resolution!\n", DRIVER_NAME, __func__);

        while (',' != line[i] && i < line_size)  i++;
        if (i == line_size)
            break;
    }
}

static int ft3dnr_parse_gmlib_cfg(void)
{
    char line[MAX_LINE_CHAR];
    char filename[80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int i, line_size;
    res_cfg_t   *l_res_cfg = NULL, *f_res_cfg = NULL;
    int         l_res_cfg_cnt = 0, f_res_cfg_cnt = 0, res_cfg_array_sz;
    int         ii, jj, ret = 0;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, GMLIB_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        printk("%s(%s): file %s not found\n", DRIVER_NAME, __func__, filename);
        return -ENOENT;
    }

    // parse [RESOLUTION] first
    while (readline(fd, sizeof(line), line, &offset)) {
        if (strstr(line, "[RESOLUTION]")) {
            while (readline(fd, sizeof(line), line, &offset)) {
                if ('\n' == line[0] || '\0' == line[0] || '[' == line[0]) {
                    break;
                }
                line_size = strlen(line);
                if (';' == line[0])
                    continue;
                i = 0;
                while ('=' != line[i])  i++;
                i++;
                while (' ' == line[i])  i++;

                parse_additional_resolution(line, i, line_size);
            }
        }
    }

    vfs_llseek(fd, 0, 0);
    offset = 0;

    res_cfg_array_sz = sizeof(res_cfg_t) * MAX_CH_NUM;

    l_res_cfg = (res_cfg_t *) kzalloc(res_cfg_array_sz, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(l_res_cfg))
        panic("%s: allocate memory fail for l_res_cfg(0x%p)!\n", __func__, l_res_cfg);

    f_res_cfg = (res_cfg_t *) kzalloc(res_cfg_array_sz, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(f_res_cfg))
        panic("%s: allocate memory fail for f_res_cfg(0x%p)!\n", __func__, f_res_cfg);

    f_res_cfg_cnt = 0;

    while (readline(fd, sizeof(line), line, &offset)) {
        if (strstr(line, "[ENCODE_DIDN]")) {
            while (readline(fd, sizeof(line), line, &offset)) {
                if ('\n' == line[0] || '\0' == line[0] || '[' == line[0]) {
                    break;
                }
                line_size = strlen(line);
                if (';' == line[0])
                    continue;
                i = 0;
                while ('=' != line[i])  i++;
                i++;
                while (' ' == line[i])  i++;

                memset(l_res_cfg, 0, res_cfg_array_sz);
                l_res_cfg_cnt = 0;

                ret = parse_res_cfg_line(line, i, line_size, l_res_cfg, &l_res_cfg_cnt);
                if (ret)
                    break;

                // sort size of temp res_cfg from big to little
                for (ii = 0; ii < (l_res_cfg_cnt - 1); ii++) {
                    res_cfg_t   tmp_res, *res_big;

                    res_big = &l_res_cfg[ii];
                    for (jj = ii + 1; jj < l_res_cfg_cnt; jj++) {
                        if (l_res_cfg[jj].size > res_big->size)
                            res_big = &l_res_cfg[jj];
                    }
                    memcpy(&tmp_res, &l_res_cfg[ii], sizeof(res_cfg_t));
                    memcpy(&l_res_cfg[ii], res_big, sizeof(res_cfg_t));
                    memcpy(res_big, &tmp_res, sizeof(res_cfg_t));
                }

                // update temp res_cfg to final result
                for (ii = 0; ii < l_res_cfg_cnt; ii++) {
                    if (l_res_cfg[ii].size > f_res_cfg[ii].size)
                        memcpy(&f_res_cfg[ii], &l_res_cfg[ii], sizeof(res_cfg_t));
                }

                if (l_res_cfg_cnt > f_res_cfg_cnt)
                    f_res_cfg_cnt = l_res_cfg_cnt;
            }
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);

    priv.res_cfg_cnt = f_res_cfg_cnt;

    if (priv.res_cfg_cnt == 0)
        panic("%s(%s): falied to parse vg config \"%s\", priv.res_cfg_cnt = %d\n", DRIVER_NAME, __func__, GMLIB_FILENAME, priv.res_cfg_cnt);
    priv.res_cfg = (res_cfg_t *) kzalloc(sizeof(res_cfg_t) * priv.res_cfg_cnt, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(priv.res_cfg))
        panic("%s: allocate memory fail for priv.res_cfg(0x%p)\n", __func__, priv.res_cfg);

    // update final res_cfg to priv
    memcpy(priv.res_cfg, f_res_cfg, sizeof(res_cfg_t) * priv.res_cfg_cnt);

    kfree(l_res_cfg);
    kfree(f_res_cfg);

    return ret;
}

static int ft3dnr_parse_module_param_res_cfg(char *line)
{
    int i, line_size;
    res_cfg_t   *l_res_cfg;
    int         l_res_cfg_cnt = 0, res_cfg_array_sz;
    int         ii, jj, ret = 0;

    line_size = strlen(line);

    i = 0;
    while (' ' == line[i])  i++;

    res_cfg_array_sz = sizeof(res_cfg_t) * MAX_CH_NUM;

    l_res_cfg = (res_cfg_t *) kzalloc(res_cfg_array_sz, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(l_res_cfg))
        panic("%s: allocate memory fail for l_res_cfg(0x%p)!\n", __func__, l_res_cfg);

    l_res_cfg_cnt = 0;

    ret = parse_res_cfg_line(line, i, line_size, l_res_cfg, &l_res_cfg_cnt);
    if (ret)
        goto exit;

    // sort size of temp res_cfg from big to little
    for (ii = 0; ii < (l_res_cfg_cnt - 1); ii++) {
        res_cfg_t   tmp_res, *res_big;

        res_big = &l_res_cfg[ii];
        for (jj = ii + 1; jj < l_res_cfg_cnt; jj++) {
            if (l_res_cfg[jj].size > res_big->size)
                res_big = &l_res_cfg[jj];
        }
        memcpy(&tmp_res, &l_res_cfg[ii], sizeof(res_cfg_t));
        memcpy(&l_res_cfg[ii], res_big, sizeof(res_cfg_t));
        memcpy(res_big, &tmp_res, sizeof(res_cfg_t));
    }

    priv.res_cfg_cnt = l_res_cfg_cnt;

    priv.res_cfg = (res_cfg_t *) kzalloc(sizeof(res_cfg_t) * priv.res_cfg_cnt, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(priv.res_cfg))
        panic("%s: allocate memory fail for priv.res_cfg(0x%p)\n", __func__, priv.res_cfg);

    // update final res_cfg to priv
    memcpy(priv.res_cfg, l_res_cfg, sizeof(res_cfg_t) * priv.res_cfg_cnt);

exit:
    kfree(l_res_cfg);

    return ret;
}

module_init(ft3dnr_drv_init);
module_exit(ft3dnr_drv_exit);
MODULE_LICENSE("GPL");
