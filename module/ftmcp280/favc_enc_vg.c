/* favc_enc_vg.c */
/***************************************************************************
 * Header Files
 ***************************************************************************/
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/kallsyms.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <linux/pci.h>
#include <asm/cacheflush.h>
#include <mach/fmem.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include "log.h"
#include "video_entity.h"
#include "favc_enc_entity.h"
#include "favc_enc_vg.h"
#include "frammap_if.h"
#include "h264e_ioctl.h"
#include "enc_driver/H264V_enc.h"
#include "favce_param.h"
#include "favce_buffer.h"
#ifdef REF_POOL_MANAGEMENT
    #include "mem_pool.h"
#endif

/***************************************************************************
 * Static Definitions
 ***************************************************************************/
#ifdef GM8210
    #define FTMCP280_0_BASE_ADDRESS     H264E_FTMCP280_PA_BASE
    #define FTMCP280_0_BASE_SIZE        H264E_FTMCP280_0_PA_SIZE
    #define FTMCP280_0_VCACHE_ADDRESS   0x90100000
    #define FTMCP280_0_IRQ              H264E_FTMCP280_0_IRQ

    #define FTMCP280_1_BASE_ADDRESS     H264E_FTMCP280_1_PA_BASE
    #define FTMCP280_1_BASE_SIZE        H264E_FTMCP280_1_PA_SIZE
    #define FTMCP280_1_VCACHE_ADDRESS   0x90300000
    #define FTMCP280_1_IRQ              H264E_FTMCP280_1_IRQ
    #define DEFAULT_MAX_CHN_NUM 128
#elif defined(GM8287)
    #define FTMCP280_0_BASE_ADDRESS     H264E_FTMCP280_0_PA_BASE
    #define FTMCP280_0_BASE_SIZE        H264E_FTMCP280_0_PA_SIZE
    #define FTMCP280_0_VCACHE_ADDRESS   0x90100000
    #define FTMCP280_0_IRQ              H264E_FTMCP280_0_IRQ
    #define DEFAULT_MAX_CHN_NUM 64
#elif defined(GM8139)
    #define FTMCP280_0_BASE_ADDRESS     H264E_FTMCP280_0_PA_BASE
    #define FTMCP280_0_BASE_SIZE        H264E_FTMCP280_0_PA_SIZE
    #define FTMCP280_0_VCACHE_ADDRESS   0x92d00000
    #define FTMCP280_0_IRQ              H264E_FTMCP280_0_IRQ
    #define DEFAULT_MAX_CHN_NUM 8
#elif defined(GM8136)
    #define FTMCP280_0_BASE_ADDRESS     H264E_FTMCP280_0_PA_BASE
    #define FTMCP280_0_BASE_SIZE        H264E_FTMCP280_0_PA_SIZE
    #define FTMCP280_0_VCACHE_ADDRESS   0x92d00000
    #define FTMCP280_0_IRQ              H264E_FTMCP280_0_IRQ
    #define DEFAULT_MAX_CHN_NUM 4
#endif
#define MAX_JOB_NUM     128  // for callback list
//#define DEFAULT_MAX_CHN_NUM 128

// set maximum resolution (for allocate shared memory)
#ifdef AUTOMATIC_SET_MAX_RES
    unsigned int h264e_max_width = 0;
#else
    unsigned int h264e_max_width = DEFAULT_MAX_WIDTH;
#endif
module_param(h264e_max_width, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_max_width, "Max frame width");
#ifdef AUTOMATIC_SET_MAX_RES
    unsigned int h264e_max_height = 0;
#else
    unsigned int h264e_max_height = DEFAULT_MAX_HEIGHT;
#endif
module_param(h264e_max_height, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_max_height, "Max frame height");
unsigned int h264e_max_b_frame = 0;
module_param(h264e_max_b_frame, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_max_b_frame, "Max B frame Number");
#ifdef RECORD_BS
    unsigned int h264e_rec_bs_size = 0;
    module_param(h264e_rec_bs_size, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_rec_bs_size, "Max rec size of bs buffer");
#endif
unsigned int h264e_max_chn = DEFAULT_MAX_CHN_NUM;
module_param(h264e_max_chn, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_max_chn, "Max channel number");
unsigned int h264e_snapshot_chn = 0;
module_param(h264e_snapshot_chn, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_snapshot_chn, "Snapshot channel number");
unsigned int h264e_yuv_swap = 0;
module_param(h264e_yuv_swap, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(h264e_yuv_swap, "swap uyvy");
#ifdef SUPPORT_PWM
    unsigned int pwm = 0;
    module_param(pwm, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(pwm, "pwm period");
#endif
// Tuba 20140127: add module parameter to specify configure path
char *config_path = "/mnt/mtd";
module_param(config_path, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(config_path, "set config path");

// Tuba 20140325: add ioremap ncb
unsigned int use_ioremap_wc = 0;
module_param(use_ioremap_wc, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(use_ioremap_wc, "ioremap wc");

// Tuba 20140524: dynamic assign engine
#ifdef DYNAMIC_ENGINE
    unsigned int h264e_dynamic_engine = 1;
    module_param(h264e_dynamic_engine, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_dynamic_engine, "dynamic assign engine");
#endif

#ifdef OUTPUT_SLICE_OFFSET
    // Tuba 20140521: enable output slice offset
    unsigned int h264e_slice_offset = 0;
    module_param(h264e_slice_offset, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_slice_offset, "output slice offset");
#endif

// Tuba 20140604: ref/rec the same buffer
#ifdef SAME_REC_REF_BUFFER
    unsigned int h264e_one_ref_buf = 1;
    module_param(h264e_one_ref_buf, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_one_ref_buf, "one reference buffer");
#endif

#ifdef TIGHT_BUFFER_ALLOCATION
    unsigned int h264e_tight_buf = 1;
    module_param(h264e_tight_buf, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_tight_buf, "tight buffer allocation");
#endif
#ifdef USE_CONFIG_FILE
    unsigned int h264e_user_config = 0;
    module_param(h264e_user_config, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_user_config, "use user configure");
#endif
#ifdef DISABLE_WRITE_OUT_REC
    unsigned int h264e_dis_wo_buf = 1;
    module_param(h264e_dis_wo_buf, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_dis_wo_buf, "disable write out rec mv l1");
#endif
#ifdef DYNAMIC_GOP_ENABLE
    unsigned int h264e_dynamic_gop = 0; // 1: by roi region
    module_param(h264e_dynamic_gop, uint, S_IRUGO|S_IWUSR);
    MODULE_PARM_DESC(h264e_dynamic_gop, "dynamic gop");
#endif

/* scheduler */
#ifdef USE_KTHREAD
    static struct task_struct *favce_cb_task = NULL;
    static wait_queue_head_t favce_cb_thread_waitqueue;
    static int favce_cb_wakeup_event = 0;
    static volatile int favce_cb_thread_ready = 0;
    #if defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)
        #ifndef TWO_ENGINE
        static struct task_struct *favce_job_task = NULL;
        static wait_queue_head_t favce_job_waitqueue;
        static int favce_job_wakeup_event = 0;
        static volatile int favce_job_thread_ready = 0;
        #else
        static struct task_struct *favce_job_task[2] = {NULL, NULL};
        static wait_queue_head_t favce_job_waitqueue[2];
        static int favce_job_wakeup_event[2] = {0, 0};
        static volatile int favce_job_thread_ready[2] = {0, 0};
        #endif
    #endif
#else
    static struct workqueue_struct *favce_cb_wq = NULL;
    static DECLARE_DELAYED_WORK(process_callback_work, 0);
    //#ifdef REORDER_JOB
    #if defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)
        #ifndef TWO_ENGINE
        static struct workqueue_struct *favce_reorder_job_wq = NULL;
        static DECLARE_DELAYED_WORK(process_reorder_job_work, 0);
        #else
        static struct workqueue_struct *favce_reorder_job_wq[2] = {NULL, NULL};
        static DECLARE_DELAYED_WORK(process_reorder_job_work0, 0);
        static DECLARE_DELAYED_WORK(process_reorder_job_work1, 0);
        #endif
    #endif
#endif

/*************** global parameter ***************/
#ifdef TWO_ENGINE
    //#define FIX_TWO_ENGINE
    int procAssignEngine = 0;
    static unsigned long favce_base_address_va[2] = {0, 0};
    static unsigned long favce_vcache_address_va[2] = {0, 0};
    static int irq_no[2] = {-1, -1};
    struct buffer_info_t mcp280_sysinfo_buffer[MAX_ENGINE_NUM];
    struct buffer_info_t mcp280_mvinfo_buffer[MAX_ENGINE_NUM];
    struct buffer_info_t mcp280_l1_col_buffer[MAX_ENGINE_NUM];
    //static int chn_2_engine[MAX_CHN_NUM];
    int *chn_2_engine = NULL;
    static int favce_cur_chn[MAX_ENGINE_NUM];
    #ifdef DYNAMIC_ENGINE
        int engine_cost[MAX_ENGINE_NUM];
        #ifdef CHECK_ENGINE_BALANCE
            unsigned int gHWUtlThd = 90;
            unsigned int gHWUtlDiffThd = 5;
            unsigned int gUnbalanceCounter = 0;
            unsigned int gForceBalanceMaxNum = 5;
        #endif
    #endif
#else
    static unsigned long favce_base_address_va = 0;
    static unsigned long favce_vcache_address_va = 0;
    static int irq_no = -1;
    struct buffer_info_t mcp280_sysinfo_buffer = {0, 0, 0};
    struct buffer_info_t mcp280_mvinfo_buffer = {0, 0, 0};
    struct buffer_info_t mcp280_l1_col_buffer = {0, 0, 0};
    static int favce_cur_chn = 0;
#endif


/* proc system */
unsigned int callback_period = 3;       // msec
unsigned int callback_period_jiffies;   // ticks

/* utilization */
unsigned int utilization_period = 5; //5sec calculation
#ifdef TWO_ENGINE
    unsigned int utilization_start[MAX_ENGINE_NUM], utilization_record[MAX_ENGINE_NUM];
    unsigned int engine_start[MAX_ENGINE_NUM], engine_end[MAX_ENGINE_NUM];
    unsigned int engine_time[MAX_ENGINE_NUM];
#else
    unsigned int utilization_start, utilization_record;
    unsigned int engine_start, engine_end;
    unsigned int engine_time;
#endif

/* property lastest record */
struct property_record_t *property_record = NULL;

/* log & debug message */
int log_level = LOG_WARNING;    //larger level, more message
// 0: LOG_EMERGY, 1: LOG_ERROR, 2: LOG_WARNING, 3: LOG_DEBUG, 4: LOG_INFO
#ifdef MEM_CNT_LOG
    int allocate_cnt = 0;
#endif

#ifdef REF_POOL_MANAGEMENT
/* memory pool suitable buffer checker */
unsigned char *mem_buf_checker[MAX_ENGINE_NUM];
#endif

#ifdef RECORD_BS
    // two record bitstream buffer: pinpon buffer
    struct buffer_info_t mcp280_rec_bs_buffer[2] = {{0, 0, 0},{0, 0, 0}};
    extern int dump_bitstream(void *ptr, int size, int chn);
#endif

//#define USE_SPIN_LOCK
static spinlock_t favc_enc_lock;
#ifdef MEM_CNT_LOG
    static spinlock_t mem_cnt_lock;
#endif
static struct semaphore favc_enc_mutex;
static struct list_head *favce_engine_head = NULL;
static struct list_head *favce_minor_head = NULL;

/* variable */
struct favce_data_t     *private_data = NULL;

//#ifdef RC_ENABLE
extern struct rc_entity_t *rc_dev;
//#endif
/* property */
//struct video_entity_t *driver_entity;

enum property_id {
    ID_START = (MAX_WELL_KNOWN_ID_NUM + 1),
    ID_RC_MODE,
    ID_MAX_BITRATE,
//    ID_WATERMARK_ENABLE,
    ID_WATERMARK_PATTERN,
    ID_MB_INFO_OFFSET,
    ID_MB_INFO_LENGTH,
    ID_FORCE_INTRA,
    ID_FIELD_CODING,
    ID_SLICE_MB_ROW,
    ID_GRAY_SCALE,
    // user data
    ID_VUI_FORMAT_PARAM,
    ID_VUI_SAR_PARAM,    
    // ROI_QP
//    ID_ROI_QP_ENABLE,
    ID_ROI_QP_REGION0,
    ID_ROI_QP_REGION1,
    ID_ROI_QP_REGION2,
    ID_ROI_QP_REGION3,
    ID_ROI_QP_REGION4,
    ID_ROI_QP_REGION5,
    ID_ROI_QP_REGION6,
    ID_ROI_QP_REGION7,
    ID_PROFILE,     // Tuba 20140129: add profile setting
    ID_QP_OFFSET,   // Tuba 20141114: add qp offset setting
#ifdef HANDLE_2FRAME
    ID_USE_FRAME_NUM,
    ID_SRC_BG_SIZE,
#endif
    ID_MAX,
};

struct property_map_t property_map[] = {
    {ID_SRC_FMT,"src_fmt","source format"},                 // in prop: source format
    {ID_SRC_XY,"src_xy","roi xy"},                          // in prop: source xy
    {ID_SRC_BG_DIM,"src_bg_dim","source dim"},              // in prop: source bg dim
    {ID_SRC_DIM,"src_dim","encode resolution"},             // in prop: source dim
    {ID_IDR_INTERVAL,"idr_interval","I frame interval"},    // in prop: gop
    {ID_B_FRM_NUM,"b_frm_num","B frame number"},            // in prop: gop
    {ID_SRC_INTERLACE,"src_interlace","src interlace"},     // in prop: source type
    {ID_DI_RESULT,"di_result","di input"},                  // in prop: source type
    {ID_RC_MODE,"rc_mode","rate control mode"},             // in prop: rate control
    {ID_SRC_FRAME_RATE,"src_frame_rate","source frame rate"},   // in prop: frame rate
    {ID_FPS_RATIO,"fps_ratio","frame rate ratio"},          // in prop: frame rate
    {ID_INIT_QUANT,"init_quant","initial quant"},           // in prop: rate control
    {ID_MAX_QUANT,"max_quant","maximum quant"},             // in prop: rate control
    {ID_MIN_QUANT,"min_quant","minimum quant"},             // in prop: rate control
    {ID_BITRATE,"bitrate","target bitrate (Kb)"},           // in prop: rate control
    {ID_MAX_BITRATE,"max_bitrate","maximum bitrate (Kb)"},  // in prop: rate control
    {ID_MOTION_ENABLE,"motion_en","output motion info"},    // in prop: output motion info
    {ID_WATERMARK_PATTERN,"wk_pattern","watermark pattern"},// in prop: watermark
    // user data
    {ID_VUI_FORMAT_PARAM,"vui_foramt_param","user data"},   // in prop: vui video format information: default 0x0A020202
    // timing_info_present_flag[28]: encode frame rate int bitstream
    // video_format[25:27]: default 5
    // full_range[24]: default 0 
    // colour_primaries[16:23]: default 2
    // transfer_characteristics[8:15]: default 2
    // matrix_coefficient[0:7]: default 2
    {ID_VUI_SAR_PARAM,"vui_sar_param","user data"},         // in prop: vui sar infomation
    {ID_FORCE_INTRA,"force_intra","force intra"},           // in prop: force intra
    {ID_FIELD_CODING,"field_coding","field coding"},        // in prop: field coding (0: frame, 1: field)
    {ID_SLICE_MB_ROW,"slice_mb_row","mb row number of each slice"}, // in prop: multislice (row number of one), 0: no multislice
    {ID_GRAY_SCALE,"gray_scale","gray scale"},              // in prop: 0: encode 420, 1: encode 400
	// roi qp enable when any ROI QP region is assigned
    {ID_ROI_QP_REGION0,"roi_qp_region0","roi qp region 0"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION1,"roi_qp_region1","roi qp region 1"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION2,"roi_qp_region2","roi qp region 2"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION3,"roi_qp_region3","roi qp region 3"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION4,"roi_qp_region4","roi qp region 4"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION5,"roi_qp_region5","roi qp region 5"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION6,"roi_qp_region6","roi qp region 6"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_ROI_QP_REGION7,"roi_qp_region7","roi qp region 7"}, // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
    {ID_DIDN_MODE,"didn_mode","didn mode"},                 // in prop: didn
    {ID_PROFILE,"profile","h264 profile"},                  // Tuba 20140129: add profile setting
    {ID_QP_OFFSET,"qp_offset","h264 qp setting"},           // Tuba 20141114: add qp offset setting
    {ID_FAST_FORWARD,"fast_forward",""},                    // Tuba 20140327: fast forward

    {ID_QUANT,"quant","quant"},                             // out prop: quant
    {ID_MB_INFO_OFFSET,"mb_info_offset",""},                // out prop: mb offset
    {ID_MB_INFO_LENGTH,"mb_info_length",""},                // out prop: mb length
    {ID_BITSTREAM_OFFSET,"bitstream_offset",""},            // out prop: bitstream offset
    {ID_SLICE_TYPE,"slice_type",""},                        // out prop: slice type
    {ID_BITSTREAM_SIZE,"bitstream_size",""},                // out prop: bitstream
    {ID_CHECKSUM,"check_sum",""},                           // in/out prop: checksum
    {ID_REF_FRAME,"ref_frame",""},                          // out prop
#ifdef OUTPUT_SLICE_OFFSET
    {ID_SLICE_OFFSET1,"slice_offset1",""},
    {ID_SLICE_OFFSET2,"slice_offset2",""},
    {ID_SLICE_OFFSET3,"slice_offset3",""},
#endif
#ifdef HANDLE_2FRAME
    {ID_SRC2_BG_DIM,"src2_bg_dim","source2 dim"},
    {ID_SRC2_XY,"src2_xy","roi2 xy"},
    {ID_SRC2_DIM,"src2_dim","encode resolution2"},
    {ID_USE_FRAME_NUM,"frame_idx","src frame idx"},
    {ID_SRC_BG_SIZE,"src_bg_frame_size","src bg frame size"},
#endif
#ifdef ENABLE_SWAP_WH
    {ID_CLOCKWISE,"clockwise","rotation image"},
#endif
};

#ifdef REORDER_JOB
    #define MAX_REORDER_CNT 10
    int sleep_time = 0;
//static struct job_item_t *reorder_job_item[MAX_ENGINE_NUM];
#endif


/************* global parameter *************/
FAVC_ENC_PARAM gEncParam;
FAVC_ENC_DIDN_PARAM gDiDnParam;
FAVC_ENC_SCALE_LIST gScalingList;
#ifdef USE_CONFIG_FILE
    #ifdef ENABLE_USER_CONFIG
    struct favce_param_data_t gEncConfigure[MAX_CFG_NUM+1];
    #endif
#endif

//int procForceUpdate[MAX_CHN_NUM] = {0};
int *procForceUpdate = NULL;
int procForceDirectDump = 0;
#ifdef GM8139
    int default_configure = CFG_PERFORMANCE;    // CFG_MIDDLE, CFG_PERFORMANCE, CFG_QUALITY, CFG_USER
#elif defined(GM8136)
    int default_configure = CFG_PERFORMANCE;    // CFG_MIDDLE, CFG_PERFORMANCE, CFG_QUALITY, CFG_USER
#else
    int default_configure = CFG_PERFORMANCE;    // CFG_MIDDLE, CFG_PERFORMANCE, CFG_QUALITY, CFG_USER
#endif
int adapt_search_range = 0; // Tuba 20131105: enable/disable adaptive search range by resolution
int realloc_cnt_limit = 1000;
int over_spec_limit = DEFAULT_MAX_CHN_NUM*30;
int gDidnMode = -1;
static const unsigned char DiDnCombination[9] = {0, 1, 3, 4, 5, 7, 11, 12, 15};
// BIT0: spatial de-interlace, BIT1: temporal de-interlace
// BIT2: spatial de-noise,     BIT3: temporal de-noise
int forceQP = -1;

// parameter of rate control
int gIPOffset = 2;
int gPBOffset = 2;
int gQPStep = 1;
int gMinQuant = 15;
int gMaxQuant = 51;
unsigned int gLevelIdc = 0;
#ifdef OVERFLOW_RETURN_VALID_VALUE
    int gOverflowReturnLength = 1024;
#endif
#ifdef OVERFLOW_REENCODE
    int g_bitrate_factor = 249;
#endif
//#define EVALUATION_BITRATE
#ifdef EVALUATION_BITRATE
    #define MAX_REC_CHN DEFAULT_MAX_CHN_NUM
    int rc_bitrate_period = 5;
    int rc_start[MAX_REC_CHN] = {0};
    unsigned int rc_total_bit[MAX_REC_CHN] = {0};
    int rc_frame_num[MAX_REC_CHN] = {0};
    void reset_bitrate_evaluation(int chn);
#endif
int dump_bitrate = 0;
#ifdef HANDLE_PANIC_STATUS
    int gPanicFlag = 0;
#endif
#ifdef ENABLE_MB_RC
    unsigned int gMBRCBasicUnit = 1;
    unsigned int gMBQPLimit = 5;
#endif
#ifdef DYNAMIC_GOP_ENABLE
    unsigned int gop_factor = 4;
    unsigned int measure_static_event = 2;
    unsigned int iframe_disable_roi = 1;
#endif

#define ISR_SPIN_LOCK

static void release_used_recon(ReconBuffer *ref_buf, int chn);
static int release_all_buffer(struct favce_data_t *enc_data);

/************* favc_enc_entity *************/
extern int test_and_set_engine_busy(int);
extern int is_engine_idle(int engine);
extern void set_engine_idle(int engine);
extern void set_engine_busy(int engine);

extern int favce_proc_init(void);
extern void favce_proc_close(void);
extern int pf_mcp280_clk_init(void);
extern int pf_mcp280_clk_on(void);
extern int pf_mcp280_clk_off(void);
extern int pf_mcp280_clk_exit(void);
extern int favce_dump_buffer(int chn, unsigned char *buf, int size, int isBS, int keyframe);
extern unsigned int video_gettime_us(void);
extern unsigned int video_gettime_ms(void);
#ifdef ENABLE_CHECKSUM
extern int calculate_checksum(unsigned int buf_pa, unsigned int len, int type); 
#endif

#ifdef NEW_JOB_ITEM_ALLOCATE
    #ifdef GM8136
    #define MAX_JOB_ARRAY_NUM   DEFAULT_MAX_CHN_NUM*4
    #else
    #define MAX_JOB_ARRAY_NUM   1024
    #endif
    static struct job_item_t job_array[MAX_JOB_ARRAY_NUM];
    static int job_used[MAX_JOB_ARRAY_NUM] = {0};
    //static int job_idx = 0;
#endif

struct job_item_t *mcp280_job_alloc(int size)
{
#ifdef NEW_JOB_ITEM_ALLOCATE
    struct job_item_t *job_item = NULL;
    int i;
    unsigned long flags;
    spin_lock_irqsave(&favc_enc_lock, flags);
    for (i = 0; i < MAX_JOB_ARRAY_NUM; i++) {
    #if 0
        if (0 == job_used[job_idx]) {
            job_item = &job_array[job_idx];
            //job_item->job_item_id = job_idx;
        #ifdef LOCK_JOB_STAUS_ENABLE
            job_item->process_state = 0;
        #endif
            job_used[job_idx] = 1;
            job_idx = (job_idx+1)%MAX_JOB_ARRAY_NUM;
            break;
        }
        job_idx = (job_idx+1)%MAX_JOB_ARRAY_NUM;
    #else
        if (0 == job_used[i]) {
            job_item = &job_array[i];
        #ifdef LOCK_JOB_STAUS_ENABLE
            job_item->process_state = 0;
        #endif
            job_used[i] = 1;
            break;
        }
    #endif
    }
    spin_unlock_irqrestore(&favc_enc_lock, flags);
    return job_item;
#else
    struct job_item_t *job_item = NULL;
    #ifdef MEM_CNT_LOG
        unsigned long flags;
    #endif
    job_item = (struct job_item_t*)kmalloc(size, GFP_ATOMIC);
    #ifdef MEM_CNT_LOG
        spin_lock_irqsave(&mem_cnt_lock, flags);
        allocate_cnt++;
        spin_unlock_irqrestore(&mem_cnt_lock, flags);
    #endif
    return job_item;
#endif
}

void mcp280_job_free(void *ptr)
{
#ifdef NEW_JOB_ITEM_ALLOCATE
    struct job_item_t *job_item = (struct job_item_t *)ptr;
    unsigned long flags;
    spin_lock_irqsave(&favc_enc_lock, flags);
    if (job_item->job_item_id < MAX_JOB_ARRAY_NUM) {
        job_used[job_item->job_item_id] = 0;
    }
    spin_unlock_irqrestore(&favc_enc_lock, flags);
#else
    #ifdef MEM_CNT_LOG
        unsigned long flags;
    #endif

    // Tuba 20131216: add pointer checker
    if (ptr) {
        kfree(ptr);
    #ifdef MEM_CNT_LOG
        spin_lock_irqsave(&mem_cnt_lock, flags);    
        allocate_cnt--;
        spin_unlock_irqrestore(&mem_cnt_lock, flags);
    #endif
    }
#endif
}

static void print_property(struct video_entity_job_t *job, struct video_property_t *property, int is_out)
{
    int i;
    int engine = ENTITY_FD_ENGINE(job->fd);
    int minor = ENTITY_FD_MINOR(job->fd);
    
    for (i = 0; i < MAX_PROPERTYS; i++) {
        if (property[i].id == ID_NULL)
            break;
        if (i == 0) {
            if (is_out) {
                DEBUG_M(LOG_INFO, "{%d,%d} job %d out property:\n", engine, minor, job->id);
            }
            else {
                DEBUG_M(LOG_INFO, "{%d,%d} job %d in property:\n", engine, minor, job->id);
            }
        }
        DEBUG_M(LOG_INFO, "  ID:%d,Value:0x%x\n", property[i].id, property[i].value);
    }
}

static int favce_parse_in_property(struct favce_data_t *enc_data, struct video_entity_job_t *job)
{
    int i = 0;
    int wk_non_exist = 1;
    //struct favce_data_t *enc_data = (struct favce_data_t *)param;
    int idx = ENTITY_FD_MINOR(job->fd);
#ifdef SUPPORT_VG2
    struct video_property_t *property = job->in_prop.p;
#else
    struct video_property_t *property = job->in_prop;
#endif
    unsigned int prop_mask = 0;
    unsigned int vui_format_param, vui_sar_param;
    int field_coding, slice_mb_row, gray_scale;
    int didn_mode;
    unsigned int src_frame_rate, fps_ratio, init_quant, bitrate, max_bitrate;
    int min_quant, max_quant;
    int src_interlace = -1, di_result = -1;
#ifdef ENABLE_FAST_FORWARD
    int fast_forward;
#endif
    int profile_exist = 0;
#ifdef HANDLE_2FRAME
    int frame_buf_idx = 0;
    unsigned int src_bg_dim[2] = {0,0}, enc_bg_dim = 0;
    unsigned int src_dim[2] = {0,0}, enc_dim = 0;
    unsigned int src_xy[2] = {0,0}, enc_xy = 0;
#endif
#ifdef ENABLE_SWAP_WH
    unsigned int swap_wh = 0;
#endif
    unsigned int qp_offset = (gIPOffset&0xFF) | ((gEncParam.i8ROIDeltaQP&0xFF)<<8);

    vui_format_param = vui_sar_param = 0;
    field_coding = slice_mb_row = gray_scale = 0;
    src_frame_rate = fps_ratio = init_quant = bitrate = max_bitrate = 0;
    didn_mode = 0;
    min_quant = gMinQuant;
    max_quant = gMaxQuant;
    enc_data->motion_flag = 0;
    enc_data->roi_qp_enable = 0;
    enc_data->roi_qp_region[0] = enc_data->roi_qp_region[1] = 
    enc_data->roi_qp_region[2] = enc_data->roi_qp_region[3] = 
    enc_data->roi_qp_region[4] = enc_data->roi_qp_region[5] = 
    enc_data->roi_qp_region[6] = enc_data->roi_qp_region[7] = 0;
    enc_data->slice_type = -1;
    //enc_data->profile = 0;
#ifdef ENABLE_FAST_FORWARD
    fast_forward = 0;
#endif
#ifdef ENABLE_CHECKSUM
    enc_data->checksum_type = 0;
#endif
    while (property[i].id != 0) {
        switch (property[i].id) {
            case ID_SRC_FMT:
                if (enc_data->src_fmt != property[i].value)
                    enc_data->updated |= H264E_REINIT;
                enc_data->src_fmt = property[i].value;
                prop_mask |= PROP_MASK_SRC_FMT;
                break;
            case ID_SRC_XY:
            #ifdef HANDLE_2FRAME
                src_xy[0] = property[i].value;
            #else
                if (enc_data->src_xy != property[i].value)
                    enc_data->updated |= H264E_ROI_REINIT;
                enc_data->src_xy = property[i].value;
            #endif
                prop_mask |= PROP_MASK_SRC_XY;
                break;
            case ID_SRC_BG_DIM:
            #ifdef HANDLE_2FRAME
                src_bg_dim[0] = property[i].value;
                prop_mask |= PROP_MASK_SRC_BG_DIM;
            #else
                if (enc_data->src_bg_dim != property[i].value)
                    enc_data->updated |= H264E_REINIT;
                enc_data->src_bg_dim = property[i].value;
                prop_mask |= PROP_MASK_SRC_BG_DIM;
            #endif
                break;
            case ID_SRC_DIM:
            #ifdef HANDLE_2FRAME
                src_dim[0] = property[i].value;
            #else
                if (enc_data->src_dim != property[i].value)
                    enc_data->updated |= H264E_REINIT;
                enc_data->src_dim = property[i].value;
            #endif
                prop_mask |= PROP_MASK_SRC_DIM;
                //enc_data->frame_width = EM_PARAM_WIDTH(enc_data->src_dim);
                //enc_data->frame_height = EM_PARAM_HEIGHT(enc_data->src_dim);
                break;
            case ID_IDR_INTERVAL:
                if (enc_data->idr_interval != property[i].value) {
                #ifdef ENABLE_MB_RC
                    enc_data->updated |= H264E_GOP_REINIT|H264E_RC_REINIT;
                #else
                    enc_data->updated |= H264E_GOP_REINIT;
                #endif
                }
                enc_data->idr_interval = property[i].value;
                prop_mask |= PROP_MASK_IDR_INTERVAL;
                break;
            case ID_B_FRM_NUM:
                if (property[i].value > h264e_max_b_frame) {
                    DEBUG_E(LOG_ERROR, "b frame number must be less than max_b_frame_num, %d > %d\n", property[i].value, h264e_max_b_frame);                    
                }
                else {
                    if (enc_data->b_frm_num != property[i].value)
                        enc_data->updated |= H264E_GOP_REINIT;
                    enc_data->b_frm_num = property[i].value;
                }
                prop_mask |= PROP_MASK_B_FRAME;
                break;
        #ifdef ENABLE_FAST_FORWARD
            case ID_FAST_FORWARD:
            #ifdef DISABLE_WRITE_OUT_REC
                if (h264e_dis_wo_buf)
                    fast_forward = property[i].value;
                else {
                #ifdef SAME_REC_REF_BUFFER
                    if (h264e_one_ref_buf) {
                        if (property[i].value > 1) {
                            DEBUG_W(LOG_WARNING, "when use one reference buffer, driver can not support skip reference\n");
                        }
                        fast_forward = 0;   // fast forward must be zero to avoid wrong ref
                    }
                    else
                        fast_forward = property[i].value;
                #else
                    fast_forward = property[i].value;
                #endif
                }
            #else
                #ifdef SAME_REC_REF_BUFFER
                if (h264e_one_ref_buf) {
                    if (property[i].value > 1) {
                        DEBUG_W(LOG_WARNING, "when use one reference buffer, driver can not support skip reference\n");
                    }
                    fast_forward = 0;   // fast forward must be zero to avoid wrong ref
                }
                else
                    fast_forward = property[i].value;
                #else
                fast_forward = property[i].value;
                #endif
            #endif
                break;
        #endif
            case ID_RC_MODE:
                if (enc_data->rc_mode != property[i].value) {
                    enc_data->updated |= H264E_RC_REINIT;
                }
                enc_data->rc_mode = property[i].value;
                prop_mask |= PROP_MASK_RC_MODE;
                break;
            case ID_SRC_FRAME_RATE:
                src_frame_rate = property[i].value;
                break;
            case ID_FPS_RATIO:
                fps_ratio = property[i].value;
                break;
            case ID_INIT_QUANT:
                init_quant = property[i].value;
                break;
            case ID_MAX_QUANT:
                max_quant = property[i].value;
                break;
            case ID_MIN_QUANT:
                min_quant = property[i].value;
                break;
            case ID_BITRATE:
                bitrate = property[i].value;
                break;
            case ID_MAX_BITRATE:
                max_bitrate = property[i].value;
                break;
            case ID_MOTION_ENABLE:
                enc_data->motion_flag = property[i].value;
                break;
            case ID_WATERMARK_PATTERN:
                if (enc_data->watermark_pattern != property[i].value || 0 == enc_data->watermark_flag) {
                    enc_data->updated |= H264E_WK_UPDATE;
                    enc_data->watermark_flag = 1;
                    enc_data->watermark_init_interval = 1;
                    enc_data->watermark_pattern = property[i].value;
                }
                wk_non_exist = 0;
                break;
            case ID_VUI_FORMAT_PARAM:
                vui_format_param = property[i].value;
                break;
            case ID_VUI_SAR_PARAM:
                vui_sar_param = property[i].value;
                break;
            case ID_FIELD_CODING:
                field_coding = property[i].value;
                break;
            case ID_SLICE_MB_ROW:
                slice_mb_row = property[i].value;
                break;
            case ID_GRAY_SCALE:
                gray_scale = property[i].value;
                break;
            case ID_ROI_QP_REGION0:
                enc_data->roi_qp_region[0] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION1:
                enc_data->roi_qp_region[1] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION2:
                enc_data->roi_qp_region[2] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION3:
                enc_data->roi_qp_region[3] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION4:
                enc_data->roi_qp_region[4] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION5:
                enc_data->roi_qp_region[5] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION6:
                enc_data->roi_qp_region[6] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_ROI_QP_REGION7:
                enc_data->roi_qp_region[7] = property[i].value;
                enc_data->roi_qp_enable = 1;
                break;
            case ID_SRC_INTERLACE:
                //enc_data->src_interlace = property[i].value;
				// Tuba 20150611: prevent more than one property of src_interlace
                if (src_interlace < 0)
                    src_interlace = property[i].value;
                else
                    src_interlace |= property[i].value;
                break;
            case ID_DI_RESULT:
                //enc_data->di_result = property[i].value;
                di_result = property[i].value;
                break;
            case ID_DIDN_MODE:
                if (gDidnMode < 0) {
                    didn_mode = property[i].value;
                }
                else {
                    didn_mode = 
                    enc_data->input_didn = gDidnMode;
                }
                break;
            case ID_SLICE_TYPE:
                // no check slice type when first frame, because first frame marker is fill after parsing all property
                // force I frame when 
                enc_data->slice_type = property[i].value;
                break;
            // Tuba 20140129: add profile setting
            case ID_PROFILE:
                if (enc_data->profile != property[i].value)
                    enc_data->updated |= H264E_REINIT;
                enc_data->profile = property[i].value;
                profile_exist = 1;
                break;
            case ID_QP_OFFSET:
                if (0 != (property[i].value&0xFFFF))
                    qp_offset = property[i].value;
                break;
            // Tuba 20140129 end
        #ifdef ENABLE_CHECKSUM
            case ID_CHECKSUM:
                enc_data->checksum_type = property[i].value;
                break;
        #endif
        #ifdef HANDLE_2FRAME
            case ID_SRC2_BG_DIM:
                src_bg_dim[1] = property[i].value;
                break;
            case ID_SRC2_DIM:
                src_dim[1] = property[i].value;
                break;
            case ID_SRC2_XY:
                src_xy[1] = property[i].value;
                break;
            case ID_USE_FRAME_NUM:
                frame_buf_idx = property[i].value;
                break;
            case ID_SRC_BG_SIZE:
                enc_data->src_bg_frame_size = property[i].value;
                break;
        #endif
        #ifdef ENABLE_SWAP_WH
            case ID_CLOCKWISE:
                swap_wh = 1;
                break;
        #endif
            default:
                break;
        }
        if (i < MAX_RECORD) {
            property_record[idx].property[i].id = property[i].id;
            property_record[idx].property[i].value = property[i].value;
        }
        i++;
    }
    if (enc_data->updated | H264E_REINIT) {
        if (0 == profile_exist)
            enc_data->profile = 0;
    }
/*
    if (0 == profile_exist) {
        if (enc_data->profile != gEncParam.u32Profile) {
            enc_data->profile = gEncParam.u32Profile;
            enc_data->updated |= H264E_REINIT;
        }
    }
*/
    if (h264e_max_b_frame) {
        if ((prop_mask&PROP_MASK_NECESSARY) != PROP_MASK_NECESSARY) {
            DEBUG_E(LOG_ERROR, "some necessary properties do not fill (prop_mask = 0x%x)\n", prop_mask);
            DEBUG_E(LOG_ERROR, "src_fmt: 0x%x, src_xy: 0x%x, src_bg_dim: 0x%x, src_dim: 0x%x, idr_interval: 0x%x, b_frame: 0x%x, rc_mode: 0x%x\n",  
                PROP_MASK_SRC_FMT, PROP_MASK_SRC_XY, PROP_MASK_SRC_BG_DIM, PROP_MASK_SRC_DIM,
                PROP_MASK_IDR_INTERVAL, PROP_MASK_B_FRAME, PROP_MASK_RC_MODE);
            damnit(MODULE_NAME);
            return -1;
        }
    }
    else {
        if ((prop_mask&(PROP_MASK_NECESSARY&(~PROP_MASK_B_FRAME))) != (PROP_MASK_NECESSARY&(~PROP_MASK_B_FRAME))) {
            DEBUG_E(LOG_ERROR, "some necessary properties do not fill (prop_mask = 0x%x)\n", prop_mask);
            DEBUG_E(LOG_ERROR, "src_fmt: 0x%x, src_xy: 0x%x, src_bg_dim: 0x%x, src_dim: 0x%x, idr_interval: 0x%x, rc_mode: 0x%x\n",  
                PROP_MASK_SRC_FMT, PROP_MASK_SRC_XY, PROP_MASK_SRC_BG_DIM, PROP_MASK_SRC_DIM,
                PROP_MASK_IDR_INTERVAL, PROP_MASK_RC_MODE);
            damnit(MODULE_NAME);
            return -1;
        }
    }
    
    if (wk_non_exist && enc_data->watermark_flag) {
        enc_data->updated |= H264E_WK_UPDATE;
        enc_data->watermark_flag = 0;
        enc_data->watermark_init_interval = 0;
        enc_data->watermark_pattern = 0;
    }
    if (enc_data->vui_format_param != vui_format_param || enc_data->vui_sar_param != vui_sar_param) {
        enc_data->vui_format_param = vui_format_param;
        enc_data->vui_sar_param = vui_sar_param;
        enc_data->updated |= H264E_VUI_REINIT;
    }
    if (enc_data->field_coding != field_coding || 
        enc_data->slice_mb_row != slice_mb_row || 
        enc_data->gray_scale != gray_scale) {
        enc_data->field_coding = field_coding;
        enc_data->slice_mb_row = slice_mb_row;
        enc_data->gray_scale = gray_scale;
        enc_data->updated |= H264E_REINIT;
    }
    // Tuba 20150119: add src_interlace & di_result update
    if (src_interlace < 0) {
        src_interlace = 0;
        favce_wrn("{chn%d} no src_interlace\n", enc_data->chn);
        //dump_property_value(NULL, enc_data->chn);
    }
    if (di_result < 0) {
        di_result = 0;
        favce_wrn("{chn%d} no di_result\n", enc_data->chn);
        //dump_property_value(NULL, enc_data->chn);
    }
    if (src_interlace != enc_data->src_interlace || di_result != enc_data->di_result) {
        enc_data->src_interlace = src_interlace;
        enc_data->di_result = di_result;
        enc_data->updated |= H264E_REINIT;
    }
    // Tuba 20150119: end
    if (enc_data->input_didn != didn_mode) {
        enc_data->input_didn = didn_mode;
        enc_data->updated |= H264E_REINIT;
    }
    if (enc_data->src_frame_rate != src_frame_rate || enc_data->fps_ratio != fps_ratio) {
        enc_data->src_frame_rate = src_frame_rate;
        enc_data->fps_ratio = fps_ratio;
        enc_data->updated |= (H264E_RC_REINIT|H264E_VUI_REINIT);
    }
    if (enc_data->init_quant != init_quant || enc_data->bitrate != bitrate || enc_data->max_bitrate != max_bitrate || 
        enc_data->min_quant != min_quant || enc_data->max_quant != max_quant) {
        enc_data->init_quant = init_quant;
        enc_data->bitrate = bitrate;
        enc_data->max_bitrate = max_bitrate;
        enc_data->min_quant = min_quant;
        enc_data->max_quant = max_quant;
        enc_data->updated |= H264E_RC_REINIT;
    }
#ifdef ENABLE_FAST_FORWARD
    if (enc_data->fast_forward != fast_forward) {
        enc_data->updated |= H264E_GOP_REINIT;
        enc_data->fast_forward = fast_forward;
    }     
#endif
    if ((qp_offset&0xFF) != (enc_data->qp_offset&0xFF)) {
        signed char ip_offset = (signed char)(qp_offset&0xFF);
        if (ip_offset < -51 || ip_offset > 51)
            ip_offset = gIPOffset;
        if (ip_offset != enc_data->ip_offset)
            enc_data->updated |= H264E_RC_REINIT;
        enc_data->ip_offset = ip_offset;
    }
    enc_data->qp_offset = qp_offset;

#ifdef HANDLE_2FRAME
    switch (enc_data->src_fmt) {
        case TYPE_YUV422_2FRAMES_2FRAMES:   // [0,1],[2,3]
            if (frame_buf_idx < 2) {
                frame_buf_idx = 0;
                enc_bg_dim = src_bg_dim[0];
                enc_dim = src_dim[0];
                enc_xy = src_xy[0];
            }
            else {
                enc_bg_dim = src_bg_dim[1];
                enc_dim = src_dim[1];
                enc_xy = src_xy[1];
            }
            break;
        case TYPE_YUV422_FRAME_2FRAMES:     // [0],[1,2]
        case TYPE_YUV422_FRAME_FRAME:       // [0],[1]        
            if (frame_buf_idx < 1) {
                enc_bg_dim = src_bg_dim[0];
                enc_dim = src_dim[0];
                enc_xy = src_xy[0];                
            }
            else {
                enc_bg_dim = src_bg_dim[1];
                enc_dim = src_dim[1];
                enc_xy = src_xy[1];
            }
            break;
        default:
            enc_bg_dim = src_bg_dim[0];
            enc_dim = src_dim[0];
            enc_xy = src_xy[0];
            break;
    }
    if (enc_data->src_bg_dim != enc_bg_dim || enc_data->src_dim != enc_dim || enc_data->frame_buf_idx != frame_buf_idx)
        enc_data->updated |= H264E_REINIT;
    enc_data->src_bg_dim = enc_bg_dim;
    enc_data->src_dim = enc_dim;
    enc_data->src_xy = enc_xy;
    enc_data->frame_buf_idx = frame_buf_idx;
#endif
#ifdef ENABLE_SWAP_WH
    if (swap_wh != enc_data->swap_wh) {
        enc_data->swap_wh = swap_wh;
        enc_data->updated |= H264E_REINIT;
    }
#endif

    property_record[idx].property[i].id = property_record[idx].property[i].value = 0;
    property_record[idx].entity = job->entity;
    property_record[idx].job_id = job->id;

    if (procForceUpdate[enc_data->chn]) {
        enc_data->updated |= H264E_REINIT;
        favce_info("{chn%d} force update!\n", enc_data->chn);
        procForceUpdate[enc_data->chn] = 0;
    }

#ifdef SUPPORT_VG2
    print_property(job,job->in_prop.p, 0);
#else
    print_property(job,job->in_prop, 0);
#endif
    return 1;
}


int favce_set_out_property(void *param, struct video_entity_job_t *job)
{
    int i = 0;
    struct favce_data_t *enc_data = (struct favce_data_t *)param;
#ifdef SUPPORT_VG2
    struct video_property_t *property = job->out_prop.p;
#else
    struct video_property_t *property = job->out_prop;
#endif

    property[i].id = ID_SLICE_TYPE;
    property[i].value = enc_data->slice_type;
    i++;

    property[i].id = ID_BITSTREAM_SIZE;
    property[i].value = enc_data->bs_size;
    i++;

    property[i].id = ID_BITSTREAM_OFFSET;
    property[i].value = enc_data->bs_offset;
    i++;

#ifdef ENABLE_CHECKSUM
    if (enc_data->checksum_type) {
        property[i].id = ID_CHECKSUM;
        //property[i].value = enc_data->checksum_result;
        i++;
    }
#endif

#ifdef ENABLE_FAST_FORWARD
    if (enc_data->fast_forward) {
        property[i].id = ID_REF_FRAME;
        property[i].value = enc_data->ref_frame;
        i++;
    }
#endif

#ifdef OUTPUT_SLICE_OFFSET
    if (h264e_slice_offset) {
        struct job_item_t *job_item = (struct job_item_t *)enc_data->cur_job;
        int n;
        for (n = 1; n < job_item->slice_num; n++) {
            if (1 == n)
                property[i].id = ID_SLICE_OFFSET1;
            else if (2 == n)
                property[i].id = ID_SLICE_OFFSET2;
            else if (3 == n)
                property[i].id = ID_SLICE_OFFSET3;
            i++;
        }
    }
#endif

    property[i].id = ID_DI_RESULT;
    property[i].value = enc_data->real_didn_mode;
    i++;

    property[i].id = ID_NULL;
    i++;

#ifdef SUPPORT_VG2
    print_property(job, job->out_prop.p, 1);
#else
    print_property(job, job->out_prop, 1);
#endif
    return 1;
}

#ifdef OUTPUT_SLICE_OFFSET
#define MAX_SC_SEARCH_BYTE  21
static int get_accurate_slice_offset(unsigned char *bs_buf, int slice_num, unsigned int *slice_offset)
{
    int idx, pos;
    //for (idx = 0 ; idx < slice_num; idx++) {
    for (idx = 1 ; idx < slice_num; idx++) {
        // find start code
        for (pos = slice_offset[idx]; pos < slice_offset[idx] + MAX_SC_SEARCH_BYTE; pos++) {
            if ((0 == bs_buf[pos] && 0 == bs_buf[pos+1] && 1 == bs_buf[pos+2]) ||
                (0 == bs_buf[pos] && 0 == bs_buf[pos+1] && 0 == bs_buf[pos+2] && 1 == bs_buf[pos+3])){
                slice_offset[idx] = pos;
                break;
            }
        }
    }
    return 0;
}
#endif

static void mark_engine_start(int engine)
{
#ifdef TWO_ENGINE
    if (engine_start[engine] != 0)
        DEBUG_M(LOG_WARNING, "Warning to nested use engine%d mark_engine_start function!\n", engine);
    engine_start[engine] = video_gettime_us();
    engine_end[engine] = 0;
    if (utilization_start[engine] == 0) {
        utilization_start[engine] = engine_start[engine];
        engine_time[engine] = 0;
    }
#else
    if (engine_start != 0)
        DEBUG_M(LOG_WARNING, "Warning to nested use mark_engine_start function!\n");
    engine_start = video_gettime_us();
    engine_end = 0;
    if (utilization_start == 0) {
        utilization_start = engine_start;
        engine_time = 0;
    }
#endif
}

//void mark_engine_finish(void *job_param)
static void mark_engine_finish(int engine)
{
#ifdef TWO_ENGINE
    if (engine_end[engine] != 0)
        DEBUG_M(LOG_WARNING, "Warning to nested use engine%d mark_engine_finish function!\n", engine);
    //engine_end[engine] = jiffies;
    engine_end[engine] = video_gettime_us();
    if (engine_end[engine] > engine_start[engine])
        engine_time[engine] += engine_end[engine] - engine_start[engine];
    if (utilization_start[engine] > engine_end[engine]) {
        utilization_start[engine] = 0;
        engine_time[engine] = 0;
    }
    else if ((utilization_start[engine] <= engine_end[engine]) &&
            (engine_end[engine] - utilization_start[engine] >= utilization_period*1000000)) {
        unsigned int utilization;
        utilization = (unsigned int)((100 * engine_time[engine]) /
            (engine_end[engine] - utilization_start[engine]));
        if (utilization)
            utilization_record[engine] = utilization;
        utilization_start[engine] = 0;
        engine_time[engine] = 0;
    #ifdef CHECK_ENGINE_BALANCE
        if (h264e_dynamic_engine) {
		// Tuba 20141220: fxi bug of wrong checker (unsigned substractor)
            if (utilization_record[0] > 0 && utilization_record[1] > 0) {
                unsigned int utl_diff = 0;
                if (utilization_record[0] > utilization_record[1])
                    utl_diff = utilization_record[0] - utilization_record[1];
                else
                    utl_diff = utilization_record[1] - utilization_record[0];
                if (utl_diff > gHWUtlDiffThd && 
                    (utilization_record[0] > gHWUtlThd || utilization_record[1] > gHWUtlThd)) {
                    if (gUnbalanceCounter < gForceBalanceMaxNum) {
                        int i;
                        for (i = 0; i < h264e_max_chn; i++)
                            procForceUpdate[i] = 1;
                        DEBUG_M(LOG_WARNING, "force engine balance, cnt %d, utilization:(%d,%d), cost:(%d,%d)\n", 
                            gUnbalanceCounter, utilization_record[0], utilization_record[1], engine_cost[0], engine_cost[1]);
                        gUnbalanceCounter++;
                    }
                }
                else {
                    gUnbalanceCounter = 0;
                }
            }
            /*
            if ((utilization_record[engine] > 0 && utilization_record[engine^0x01] > 0) &&
                iAbs(utilization_record[engine] - utilization_record[engine^0x01]) > gHWUtlDiffThd && 
                (utilization_record[engine] > gHWUtlThd || utilization_record[engine^0x01] > gHWUtlThd)) {
            //if (utilization_record[engine] > gHWUtilizationThd && utilization_record[engine^0x01] < gHWUtilizationThd) {
                if (gUnbalanceCounter < gForceBalanceMaxNum) {
                    int i;
                    for (i = 0; i < h264e_max_chn; i++)
                        procForceUpdate[i] = 1;
                    DEBUG_M(LOG_WARNING, "force engine balance, cnt %d, utilization:(%d,%d), cost:(%d,%d)\n", 
                        gUnbalanceCounter, utilization_record[0], utilization_record[1], engine_cost[0], engine_cost[1]);
                    gUnbalanceCounter++;
                }
            }
            else {
                gUnbalanceCounter = 0;
            }
            */
        }
    #endif
    }
    engine_start[engine] = 0;
#else
    if (engine_end != 0)
        DEBUG_M(LOG_WARNING, "Warning to nested use mark_engine_finish function!\n");
    engine_end = video_gettime_us();
    if (engine_end > engine_start)
        engine_time += engine_end - engine_start;
    if (utilization_start > engine_end) {
        utilization_start = 0;
        engine_time = 0;
    }
    else if ((utilization_start <= engine_end) &&
            (engine_end - utilization_start >= utilization_period*1000000)) {
        unsigned int utilization;
        utilization = (unsigned int)((100 * engine_time) /
            (engine_end - utilization_start));
        if (utilization)
            utilization_record = utilization;
        utilization_start = 0;
        engine_time = 0;
    }        
    engine_start = 0;
#endif
}

int favce_preprocess(void *parm, struct favce_pre_data_t *data)
{
    struct job_item_t *job_item = (struct job_item_t *)parm;
    struct video_property_t property[MAX_PROCESS_PROPERTYS];
    int i = 0;
    int ret = 0;

    ret = video_preprocess(job_item->job, (void *)property);
    DEBUG_M(LOG_INFO, "{chn%d} preprocess done\n", job_item->chn);
    if (ret < 0)
        return ret;
    memset(data, 0, sizeof(struct favce_pre_data_t));

    for (i = 0; i < MAX_PROCESS_PROPERTYS; i++) {
        if (ID_NULL == property[i].id)
            break;
        if (ID_FORCE_INTRA == property[i].id) {
            if (1 == property[i].value) {
                data->force_intra = 1;
                data->updated |= H264E_FORCE_INTRA;
                DEBUG_M(LOG_INFO, "{chn%d} force intra\n", job_item->chn);
            }
            break;
        }
    }
    return data->updated;
}

int favce_postprocess(void *parm, void *priv)
{
    struct job_item_t *job_item = (struct job_item_t *)parm;
#ifdef LOCK_JOB_STAUS_ENABLE
    job_item->process_state |= ID_POST_PROCESS;
#endif
    return video_postprocess(job_item->job, priv);
}

#ifdef DIRECT_CALLBACK
void callback_scheduler(void)
{
    int engine;
    struct job_item_t *job_item, *job_item_next;
    unsigned long flags;

    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        spin_lock_irqsave(&favc_enc_lock, flags);
        list_for_each_entry_safe (job_item, job_item_next, &favce_engine_head[engine], engine_list) {
            if (DRIVER_STATUS_FINISH == job_item->status || DRIVER_STATUS_FAIL == job_item->status) {
                //DEBUG_M(LOG_DEBUG, "put job %d to callback array\n", job_item->job_id);
                list_del(&job_item->engine_list);
                list_del(&job_item->minor_list);
                mcp280_job_free(job_item);
            }
        }
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    }
}
#else   // DIRECT_CALLBACK
void callback_scheduler(void)
{
    int engine;
    struct video_entity_job_t *job;
    struct job_item_t *job_item, *job_item_next;
    struct job_item_t *job_list[MAX_JOB_NUM];
    unsigned long flags;
    int num, i;
    // Tuba 20140606: add re-callback to callback other jobs, when number of job is over the array size
    int is_job_overflow;

    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
    re_callback:
        is_job_overflow = 0;
        num = 0;
        spin_lock_irqsave(&favc_enc_lock, flags);
        list_for_each_entry_safe (job_item, job_item_next, &favce_engine_head[engine], engine_list) {
            if (DRIVER_STATUS_FINISH == job_item->status || DRIVER_STATUS_FAIL == job_item->status) {
                job_list[num++] = job_item;
                //DEBUG_M(LOG_DEBUG, "put job %d to callback array\n", job_item->job_id);
                list_del(&job_item->engine_list);
                list_del(&job_item->minor_list);
                if (num >= MAX_JOB_NUM) {
                    is_job_overflow = 1;
                    break;
                }
            }
        }
        spin_unlock_irqrestore(&favc_enc_lock, flags);
        for (i = 0; i < num; i++) {
            job_item = job_list[i];
            job = (struct video_entity_job_t *)job_item->job;
            if (DRIVER_STATUS_FINISH == job_item->status) {
                job->status = JOB_STATUS_FINISH;
				DEBUG_M(LOG_DEBUG, "{chn%d} callback finish job %d\n", job_item->chn, job_item->job_id);
            #ifdef HANDLE_BS_CACHEABLE2
                fmem_dcache_sync((void *)job->out_buf.buf[0].addr_va, job_item->bs_length, DMA_FROM_DEVICE);
            #endif
            #ifdef ENABLE_CHECKSUM
                if (job_item->checksum_type) {
				#ifdef SUPPORT_VG2
                    struct video_property_t *property = job->out_prop.p;
				#else
                    struct video_property_t *property = job->out_prop;
    			#endif
                    unsigned int value;
                    int i = 0;
                    value = calculate_checksum(job->out_buf.buf[0].addr_pa, job_item->bs_length, job_item->checksum_type&0x0F);
                    while (property[i].id != 0 && i < MAX_OUT_PROP_NUM) {
                        if (ID_CHECKSUM == property[i].id) {
                            property[i].value = value;
                            break;
                        }
                        i++;
                    }
               }
            #endif
            #ifdef OUTPUT_SLICE_OFFSET
                if (h264e_slice_offset) {
                    int i = 0, n, id;
			    #ifdef SUPPORT_VG2
                    struct video_property_t *property = job->out_prop.p;
			    #else
                    struct video_property_t *property = job->out_prop;
				#endif
                    get_accurate_slice_offset((unsigned char *)job->out_buf.buf[0].addr_va, job_item->slice_num, job_item->slice_offset);
                    // set property value
                    for (n = 1; n < job_item->slice_num; n++) {
                        i = 0;
                        if (1 == n)
                            id = ID_SLICE_OFFSET1;
                        else if (2 == n)
                            id = ID_SLICE_OFFSET2;
                        else if (3 == n)
                            id = ID_SLICE_OFFSET3;
                        while (property[i].id != 0 && i < MAX_OUT_PROP_NUM) {
                            if (id == property[i].id) {
                                property[i].value = job_item->slice_offset[n];
                                break;
                            }
                            i++;
                        }
                    }
                }
            #endif
                job->callback(job_item->job);
                mcp280_job_free(job_item);
            }
            else if (DRIVER_STATUS_FAIL == job_item->status) {
                job->status = JOB_STATUS_FAIL;
				DEBUG_M(LOG_DEBUG, "{chn%d} callback fail job %d\n", job_item->chn, job_item->job_id);
                job->callback(job_item->job);
                mcp280_job_free(job_item);
            }
        }
        if (is_job_overflow) {
            DEBUG_M(LOG_DEBUG, "callback array overflow, re-callback other jobs\n");
            goto re_callback;
        }
    }
}
#endif  // DIRECT_CALLBACK

#ifdef USE_KTHREAD
static int favce_cb_thread(void *data)
{
    int status;
    favce_cb_thread_ready = 1;
    do {
        status  = wait_event_timeout(favce_cb_thread_waitqueue, favce_cb_wakeup_event, callback_period_jiffies);
        if (0 ==status)
            continue;   /* timeout */
        favce_cb_wakeup_event = 0;
        callback_scheduler();
    } while (!kthread_should_stop());
    favce_cb_thread_ready = 0;
    return 0;
}
static void favce_cb_wakeup(void)
{
    favce_cb_wakeup_event = 1;
    wake_up(&favce_cb_thread_waitqueue);
}
#endif

void trigger_callback_finish(void *job_param, int fun_id)
{
    struct job_item_t *job_item = (struct job_item_t *)job_param;
#ifdef DIRECT_CALLBACK
    struct video_entity_job_t *job;
#endif
    int ret = 0;
#ifdef LOCK_JOB_STAUS_ENABLE
    unsigned long flags;
    if (0 != job_item->process_state && (ID_PRE_PROCESS|ID_POST_PROCESS) != job_item->process_state) {
        favce_err("cb_fin: job %u, pre-post error 0x%x, from %d\n", job_item->job_id, job_item->process_state, fun_id);
        damnit(MODULE_NAME);
        // something wrong
    }
    if (0 == in_irq()) {
        spin_lock_irqsave(&favc_enc_lock, flags);
        job_item->status = DRIVER_STATUS_FINISH;
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    }
    else
#endif
    job_item->status = DRIVER_STATUS_FINISH;

#ifdef DIRECT_CALLBACK
    job = (struct video_entity_job_t *)job_item->job;
    job->status = JOB_STATUS_FINISH;
	DEBUG_M(LOG_DEBUG, "{chn%d} callback finish job %d\n", job_item->chn, job_item->job_id);
    #ifdef HANDLE_BS_CACHEABLE2
    fmem_dcache_sync((void *)job->out_buf.buf[0].addr_va, job_item->bs_length, DMA_FROM_DEVICE);
    #endif
    #ifdef ENABLE_CHECKSUM
    if (job_item->checksum_type) {
        struct video_property_t *property = job->out_prop.p;
        unsigned int value;
        int i = 0;
        value = calculate_checksum(job->out_buf.buf[0].addr_pa, job_item->bs_length, job_item->checksum_type&0x0F);
        while (property[i].id != 0 && i < MAX_OUT_PROP_NUM) {
            if (ID_CHECKSUM == property[i].id) {
                property[i].value = value;
                break;
            }
            i++;
        }
   }
   #endif
   #ifdef OUTPUT_SLICE_OFFSET
   if (h264e_slice_offset) {
        int i = 0, n, id;
        struct video_property_t *property = job->out_prop.p;
        get_accurate_slice_offset((unsigned char *)job->out_buf.buf[0].addr_va, job_item->slice_num, job_item->slice_offset);
        // set property value
        for (n = 1; n < job_item->slice_num; n++) {
            i = 0;
            if (1 == n)
                id = ID_SLICE_OFFSET1;
            else if (2 == n)
                id = ID_SLICE_OFFSET2;
            else if (3 == n)
                id = ID_SLICE_OFFSET3;
            while (property[i].id != 0 && i < MAX_OUT_PROP_NUM) {
                if (id == property[i].id) {
                    property[i].value = job_item->slice_offset[n];
                    break;
                }
                i++;
            }
        }
    }
    #endif
    job->callback(job_item->job);
#endif

#ifdef USE_KTHREAD
    favce_cb_wakeup();
#else
    PREPARE_DELAYED_WORK(&process_callback_work, (void *)callback_scheduler);
    ret = queue_delayed_work(favce_cb_wq, &process_callback_work, callback_period_jiffies);
#endif
    DEBUG_M(LOG_DEBUG, "job %d trigger callback finish (ret = %d)\n", job_item->job_id, ret);
}

void trigger_callback_fail(void *parm, int fun_id)
{
    struct job_item_t *job_item = (struct job_item_t *)parm;
#ifdef DIRECT_CALLBACK
    struct video_entity_job_t *job;
#endif
    int ret = 0;
#ifdef LOCK_JOB_STAUS_ENABLE
    unsigned long flags;
    if (0 != job_item->process_state && (ID_PRE_PROCESS|ID_POST_PROCESS) != job_item->process_state) {
        favce_err("cb_fail: job %u, pre-post error 0x%x, from %d\n", job_item->job_id, job_item->process_state, fun_id);
        damnit(MODULE_NAME);
        // something wrong
    }
    if (0 == in_irq()) {
        spin_lock_irqsave(&favc_enc_lock, flags);
        job_item->status = DRIVER_STATUS_FAIL;
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    }
    else
#endif    
    job_item->status = DRIVER_STATUS_FAIL;
    // Tuba 20130318: not use callback list when max_b_frame = 0

#ifdef DIRECT_CALLBACK
    job = (struct video_entity_job_t *)job_item->job;
    job->status = JOB_STATUS_FAIL;
	DEBUG_M(LOG_DEBUG, "{chn%d} callback fail job %d\n", job_item->chn, job_item->job_id);
    job->callback(job_item->job);
#endif

#ifdef USE_KTHREAD
    favce_cb_wakeup();
#else
    PREPARE_DELAYED_WORK(&process_callback_work, (void *)callback_scheduler);
    ret = queue_delayed_work(favce_cb_wq, &process_callback_work, callback_period_jiffies);
#endif
    DEBUG_M(LOG_DEBUG, "job %d trigger callback fail (ret = %d)\n", job_item->job_id, ret);
}

void *fkmalloc(size_t size, uint8_t alignment, uint8_t reserved_size)
{
	unsigned int  sz = size;//+RSVD_SZ+alignment+reserved_size;
	void          *ptr;
#ifdef MEM_CNT_LOG
    unsigned long flags;    
#endif

	ptr = kmalloc(sz, GFP_ATOMIC);
	
	if (ptr==0) {
		DEBUG_E(LOG_ERROR, "Fail to fkmalloc for addr 0x%x size 0x%x\n",(unsigned int)ptr, (unsigned int)sz);
		return 0;
	}
#ifdef MEM_CNT_LOG
    spin_lock_irqsave(&mem_cnt_lock, flags);    
    allocate_cnt++;
    spin_unlock_irqrestore(&mem_cnt_lock, flags);
#endif
	return ptr;
}

void fkfree(void *ptr)
{
#ifdef MEM_CNT_LOG
    unsigned long flags;
#endif
	if (ptr) {
		kfree(ptr);
    #ifdef MEM_CNT_LOG
        spin_lock_irqsave(&mem_cnt_lock, flags);    
        allocate_cnt--;
        spin_unlock_irqrestore(&mem_cnt_lock, flags);
    #endif
    }
}

int set_configure_value(FAVC_ENC_PARAM *pParam)
{
    //int di_enable;

#ifdef USE_CONFIG_FILE
    if (CFG_USER <= default_configure)
        return 0;
#else
    if (CFG_USER == default_configure)
        return 0;
#endif

    //di_enable = ((pParam->u8DiDnMode&TEMPORAL_DEINTERLACE) ? 1:0) & enable_quality_mode;

    if (CFG_QUALITY == default_configure) {
        pParam->u32HorizontalSR[0]      = 64;
        pParam->u32VerticalSR[0]        = 32;
        pParam->u32HorizontalSR[1]      = 64;
        pParam->u32VerticalSR[1]        = 32;
        pParam->u32HorizontalSR[2]      = 64;
        pParam->u32VerticalSR[2]        = 32;
        pParam->bDisableCoefThd         = 1;
    }
    else if (CFG_MIDDLE == default_configure) {
        pParam->u32HorizontalSR[0]      = 48;
        pParam->u32VerticalSR[0]        = 24;
        pParam->u32HorizontalSR[1]      = 48;
        pParam->u32VerticalSR[1]        = 24;
        pParam->u32HorizontalSR[2]      = 48;
        pParam->u32VerticalSR[2]        = 24;
        pParam->bDisableCoefThd         = 1;
    }
    else {  // PERFORMANCE
        pParam->u32HorizontalSR[0]      = 32;
        pParam->u32VerticalSR[0]        = 16;
        pParam->u32HorizontalSR[1]      = 32;
        pParam->u32VerticalSR[1]        = 16;
        pParam->u32HorizontalSR[2]      = 32;
        pParam->u32VerticalSR[2]        = 16;
        pParam->bDisableCoefThd         = 0;
	#ifdef NEW_SEARCH_RANGE
        if (adapt_search_range) {
            // Tuba 20130628: new search range defition
            // 1080p: 64x32
            // 720p: 64x24
            // D1 & didn on: 64x32
            // D1 & didn off: 64x16
            // cif: 32x24
            if (enc_data->frame_width < 720 && enc_data->frame_height < 480) {  // < D1
                pParam->u32HorizontalSR[0]  = 32;
                pParam->u32VerticalSR[0]    = 24;
            }
            else {
                if (enc_data->frame_width >= 720)
                    pParam->u32HorizontalSR[0]  = 64;
                else
                    pParam->u32HorizontalSR[0]  = 32;
                if (enc_data->frame_height > 1024)
                    pParam->u32VerticalSR[0] = 32;
                else if (enc_data->frame_height > 700)
                    pParam->u32VerticalSR[0] = 24;
                else
                    pParam->u32VerticalSR[0] = 16;
            }
       }
	#endif
    }

    if (CFG_PERFORMANCE == default_configure) {
        pParam->bTransform8x8Mode       = 0;
        pParam->u8PInterPredModeDisable = 0x06; // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->u8BInterPredModeDisable = 0x0E; // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->bIntra4x4Mode           = 0;    // 0: 5 mode, 1: 9mode
        pParam->bFastIntra4x4           = 1;
        pParam->bIntra8x8Disable        = 1;
        pParam->bIntra16x16PlaneDisable = 1;
    }
    else if (CFG_QUALITY == default_configure) {
        pParam->bTransform8x8Mode       = 1;
        pParam->u8PInterPredModeDisable = 0x0;  // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->u8BInterPredModeDisable = 0x0;  // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->bIntra4x4Mode           = 1;    // 0: 5 mode, 1: 9mode
        pParam->bFastIntra4x4           = 0;
        pParam->bIntra8x8Disable        = 0;
        pParam->bIntra16x16PlaneDisable = 0;
    }
    else {  // CFG_MIDDLE
        pParam->bTransform8x8Mode       = 1;
        pParam->u8PInterPredModeDisable = 0x06; // Tuba 20140520: for 8139 1080p 60 fps
        pParam->u8BInterPredModeDisable = 0x0E;
        pParam->bIntra4x4Mode           = 1;    // 0: 5 mode, 1: 9mode
        pParam->bFastIntra4x4           = 1;
        pParam->bIntra8x8Disable        = 0;
        pParam->bIntra16x16PlaneDisable = 0;
        /*
        pParam->bTransform8x8Mode       = 1;
        pParam->u8PInterPredModeDisable = 0x0;  // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->u8BInterPredModeDisable = 0x0E; // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
        pParam->bIntra4x4Mode           = 1;    // 0: 5 mode, 1: 9mode
        pParam->bFastIntra4x4           = 0;
        pParam->bIntra8x8Disable        = 1;
        pParam->bIntra16x16PlaneDisable = 1;
        */
    }
    return 0;
}

static int favce_set_default_param(FAVC_ENC_PARAM *pParam, FAVC_ENC_VUI_PARAM *pVUI, struct favce_data_t *enc_data)
{
    int i;
    //int di_enable;
#ifdef USE_CONFIG_FILE
    int profile, idx, level, entropy_coding;
    idx = (enc_data->profile>>16)&0xFF;
    if (h264e_user_config) {
    #ifdef ENABLE_USER_CONFIG
        if (idx > 0 && idx <= MAX_CFG_NUM && gEncConfigure[idx].valid) {    // avoid idx 0
            memcpy((void *)pParam, (void *)(&gEncConfigure[idx].enc_param), sizeof(FAVC_ENC_PARAM));
            enc_data->ip_offset = gEncConfigure[idx].ip_offset;
            enc_data->config_idx = idx;
        }
        else 
    #endif
        {
            memcpy((void *)pParam, (void *)(&gEncParam), sizeof(FAVC_ENC_PARAM));
            enc_data->config_idx = -1;
        }
    }
    else {
    #ifdef ENABLE_USER_CONFIG
        if (idx > 0 && idx < 4) {
            memcpy((void *)pParam, (void *)(&gEncConfigure[idx].enc_param), sizeof(FAVC_ENC_PARAM));
            enc_data->config_idx = idx;
        }
        else 
    #endif
        {
            memcpy((void *)pParam, (void *)(&gEncParam), sizeof(FAVC_ENC_PARAM));
            enc_data->config_idx = -1;
        }
    }
    profile = (enc_data->profile&0xFF);
    if (BASELINE == profile || MAIN == profile || FREXT_HP == profile)
        pParam->u32Profile = profile;
    level = (enc_data->profile>>8)&0xFF;
    if (level > 0 && level <= 51)
        pParam->u32LevelIdc = level;
    else
        pParam->u32LevelIdc = gLevelIdc;
    entropy_coding = (enc_data->profile>>24)&0xFF;
    if (1 == entropy_coding)
        pParam->bEntropyCoding = 0;
    else if (2 == entropy_coding)
        pParam->bEntropyCoding = 1;
    else
        pParam->bEntropyCoding = gEncParam.bEntropyCoding;
#else
    memcpy((void *)pParam, (void *)(&gEncParam), sizeof(FAVC_ENC_PARAM));
#endif
    /* fix parameter */
    //pParam->bAbandonOverflow        = 0;
/*
    pParam->u32Profile              = 100;  // high profile
    pParam->u8NumRefFrames          = 1;
    pParam->u8CabacInitIDC          = 0;
    pParam->bSADSource              = 0;    // auto check (didn)
    pParam->bScalingMatrixFlag      = 0;
    pParam->u8ResendSPSPPS          = 1;
*/
    /*  0: output sps and pps only first IDR frame
     *  1: outptu sps and pps each I frame
     *  2: output sps and pps each frame */

    // GOP
    pParam->u32IDRPeriod            = enc_data->idr_interval;
    pParam->u8BFrameNumber          = enc_data->b_frm_num;
#ifdef ENABLE_FAST_FORWARD
    pParam->u32FastForward          = enc_data->fast_forward;
#endif
    //pParam->u32Profile              = enc_data->profile&0xFF;     // Tuba 20140129: add profile setting

    pParam->bSrcImageType       = 1;
#ifndef HANDLE_2FRAME
    // source type
    if (TYPE_YUV422_FRAME == enc_data->src_fmt)
        pParam->bSrcImageType       = 1;
    else if (TYPE_YUV422_FIELDS == enc_data->src_fmt)
        pParam->bSrcImageType       = 1;
    else if (TYPE_YUV422_2FRAMES == enc_data->src_fmt)
        pParam->bSrcImageType       = 0;
    else
        pParam->bSrcImageType       = 1;
#else
    if (TYPE_YUV422_2FRAMES == enc_data->src_fmt)
        pParam->bSrcImageType       = 0;
    else
        pParam->bSrcImageType       = 1;
    if (enc_data->src_fmt >= TYPE_YUV422_2FRAMES_2FRAMES && enc_data->src_fmt <= TYPE_YUV422_FRAME_FRAME) {
        if (0 == enc_data->frame_buf_idx)
            enc_data->src_buf_offset = 0;
        else {
            /* not handle frame idx be bottom frame of 2 frame */
            if (0 == enc_data->src_bg_frame_size) {
                favce_err("2 frame mode, src bg frame size can not be zero\n");
                damnit(MODULE_NAME);
                return -1;
            }
            if (TYPE_YUV422_2FRAMES_2FRAMES == enc_data->src_fmt)
                enc_data->src_buf_offset = enc_data->src_bg_frame_size * 2;
            else
                enc_data->src_buf_offset = enc_data->src_bg_frame_size;
        }
    }
#endif
    //pParam->bIntTopBtm = 1;

    // resolution
#ifdef ENABLE_SWAP_WH
    if (enc_data->swap_wh) {
        pParam->u32SourceWidth      = EM_PARAM_HEIGHT(enc_data->src_bg_dim);
        pParam->u32SourceHeight     = EM_PARAM_WIDTH(enc_data->src_bg_dim);
    }
    else {
        pParam->u32SourceWidth          = EM_PARAM_WIDTH(enc_data->src_bg_dim);
        pParam->u32SourceHeight         = EM_PARAM_HEIGHT(enc_data->src_bg_dim);
    }
#else
    pParam->u32SourceWidth          = EM_PARAM_WIDTH(enc_data->src_bg_dim);
    pParam->u32SourceHeight         = EM_PARAM_HEIGHT(enc_data->src_bg_dim);
#endif
    if (0 == pParam->u32SourceWidth || 0 == pParam->u32SourceHeight) {
        favce_err("{chn%d} source resolution cat not be zero %d x %d\n", enc_data->chn, pParam->u32SourceWidth, pParam->u32SourceHeight);
        return -1;
    }
    if (pParam->u32SourceWidth&0x0F) {
        favce_err("chn%d} source frame width(%d) must be mutiple of 16!\n", enc_data->chn, pParam->u32SourceWidth);
        return -1;
    }
    if (enc_data->field_coding) {
        if (pParam->u32SourceHeight&0x1F) {
            favce_err("{chn%d} field coding: source frame height(%d) must be mutiple of 32!\n", enc_data->chn, pParam->u32SourceHeight);
            return -1;        
        }        
    }
    else {
        if (pParam->u32SourceHeight&0x0F) {
            favce_err("{chn%d} source frame height(%d) must be mutiple of 16!\n", enc_data->chn, pParam->u32SourceHeight);
            return -1;
        }
    }
    if (enc_data->src_dim != enc_data->src_bg_dim) {
        // set crop & roi check
        int width, height;
        int step_mb = 16, mask_mb = 0x0F;
        int step_width = 1, step_height = 1;
    #ifdef ENABLE_SWAP_WH
        if (enc_data->swap_wh) {
            width = EM_PARAM_HEIGHT(enc_data->src_dim);
            height = EM_PARAM_WIDTH(enc_data->src_dim);
        }
        else {
            width = EM_PARAM_WIDTH(enc_data->src_dim);
            height = EM_PARAM_HEIGHT(enc_data->src_dim);
        }
    #else
        width = EM_PARAM_WIDTH(enc_data->src_dim);
        height = EM_PARAM_HEIGHT(enc_data->src_dim);
    #endif
        if (enc_data->field_coding) {
            step_mb = 32;
            mask_mb = 0x1F;
        }
        if (width&0x0F)
            pParam->u32CroppingRight = 16 - (width&0x0F);
        if (height&mask_mb)
            pParam->u32CroppingBottom = step_mb - (height&mask_mb);
        if (enc_data->gray_scale) {
            if (enc_data->field_coding)
                step_height = 2;
        }
        else {
            step_width = 2;
            if (enc_data->field_coding)
                step_height = 4;
            else
                step_height = 2;
        }
        if (width & (step_width - 1)) {
            DEBUG_W(LOG_WARNING, "frame width must be mutiple of %d. frame width = %d, force to %d\n", step_width, width, ((width + 15)>>4) << 4);
            width = ((width + 16)>>4) << 4;
            pParam->u32CroppingRight = 0;
        }
        else 
            pParam->u32CroppingRight /= step_width;
        if (height & (step_height - 1)) {
            DEBUG_W(LOG_WARNING, "frame height must be mutiple of %d. frame height = %d, force to %d\n", step_height, height, ((height + step_mb - 1)/step_mb)*step_mb);
            height = ((height + step_mb - 1)/step_mb)*step_mb;
            pParam->u32CroppingBottom = 0;
        }
        else
            pParam->u32CroppingBottom /= step_height;
        // set roi
        pParam->u32ROI_X            = EM_PARAM_X(enc_data->src_xy);
        pParam->u32ROI_Y            = EM_PARAM_Y(enc_data->src_xy);

        pParam->u32ROI_Width        = ((width + 15)/16)*16;
        //pParam->u32ROI_Height       = ((height + 15)/16)*16;
        pParam->u32ROI_Height       = ((height + step_mb - 1)/step_mb)*step_mb;
        enc_data->frame_width       = pParam->u32ROI_Width;
        enc_data->frame_height      = pParam->u32ROI_Height;
    }
    else {
    #ifdef ENABLE_SWAP_WH
        if (enc_data->swap_wh) {
            enc_data->frame_width = EM_PARAM_HEIGHT(enc_data->src_dim);
            enc_data->frame_height = EM_PARAM_WIDTH(enc_data->src_dim);
        }
        else {
            enc_data->frame_width = EM_PARAM_WIDTH(enc_data->src_dim);
            enc_data->frame_height = EM_PARAM_HEIGHT(enc_data->src_dim);
        }
    #else
        enc_data->frame_width       = EM_PARAM_WIDTH(enc_data->src_dim);
        enc_data->frame_height      = EM_PARAM_HEIGHT(enc_data->src_dim);
    #endif
    }
    enc_data->mb_info_size = enc_data->frame_width * enc_data->frame_height / 32;
    // Tuba 20131210: bs offset must be 64 byte align (motion buffer write out unit)
    enc_data->mb_info_size = (enc_data->mb_info_size + 63)/64*64;

    // coding format
    if ((enc_data->frame_height&0x1F) && enc_data->field_coding) {
        DEBUG_W(LOG_WARNING, "field coding but frame height(%d) is not mutiple of 32, force frame coding\n", enc_data->frame_height);
        pParam->bFieldCoding        = 0;
    }
    else
        pParam->bFieldCoding        = enc_data->field_coding;

    if (enc_data->gray_scale)
        pParam->bChromaFormat       = 0;
    else
        pParam->bChromaFormat       = 1;

    //enc_data->didn_mode             = gDidnMode;
    /* if source is interlace & no de-interlace => di is enable, otherwise disable de-interlace
     * if field coding => disable de-interlace  */
    if (enc_data->src_interlace && (0 == enc_data->di_result) && (0 == enc_data->field_coding))
        enc_data->didn_mode = enc_data->input_didn;
    else {
        if (enc_data->input_didn & (TEMPORAL_DEINTERLACE | SPATIAL_DEINTERLACE)) {
            DEBUG_M(LOG_WARNING, "{chn %d} force to disbale deinterlace (src_interlace = %d, di_result = %d, field coding = %d)\n", 
                enc_data->chn, enc_data->src_interlace, enc_data->di_result, enc_data->field_coding);
        }
        enc_data->didn_mode = enc_data->input_didn & (TEMPORAL_DENOISE|SPATIAL_DENOISE);    // disbale deinterlace
    }
    if (gDidnMode < 0) {	// Tuba 20140317: fix didn error (use wrong didn setting)
        enc_data->real_didn_mode = 
        pParam->u8DiDnMode          = enc_data->didn_mode;
    }
    else {
        enc_data->real_didn_mode = 
        pParam->u8DiDnMode          = gDidnMode;
    }
#ifndef TEMPORAL_DENOISE_ENABLE
    if (pParam->u8DidnMode & TEMPORAL_DENOISE) {
        DEBUG_M(LOG_WARNING, "unsupport temporal denoise, disable temporal denoise\n");
        pParam->u8DidnMode &= ~TEMPORAL_DENOISE;
    }
#endif
    /* didn constraint */
    for (i = 0; i < 9; i++) {
        if (pParam->u8DiDnMode == DiDnCombination[i])
            break;
    }
    if (9 == i) {   // Tuba 20140224: update didn input parameter error, force bypass
        DEBUG_W(LOG_WARNING, "unsupport this didn 0x%x, force bypass\n", pParam->u8DiDnMode);
        pParam->u8DiDnMode = 0;
    }
    //di_enable = ((pParam->u8DiDnMode&TEMPORAL_DEINTERLACE) ? 1:0) & enable_quality_mode;
    pParam->u32SliceMBRowNumber     = enc_data->slice_mb_row;

    // watermark
    pParam->bWatermarkCtrl          = enc_data->watermark_flag;
    pParam->u32WatermarkPattern     = enc_data->watermark_pattern;
    pParam->u32WKReinitPeriod       = 1;    // when CABAC, only be 1
    
/*
    pParam->u8Log2MaxFrameNum       = 5;    // fixed
    pParam->u8Log2MaxPOCLsb         = 5;    // fixed

    pParam->i8ChromaQPOffset0       = 6;
    pParam->i8ChromaQPOffset1       = 6;
    pParam->i8DisableDBFIdc         = 0;
    pParam->i8DBFAlphaOffset        = 6;
    pParam->i8DBFBetaOffset         = 6;

    // search range
    pParam->u32HorizontalBSR[0]     = 32;
    pParam->u32VerticalBSR[0]       = 16;
    pParam->u32HorizontalBSR[1]     = 32;
    pParam->u32VerticalBSR[1]       = 16;
*/
    if (0 == enc_data->src_frame_rate) {
        pParam->u32num_unit_in_tick     = 1;
        pParam->u32time_unit            = 30;
    }
    else {
        if (0 == EM_PARAM_N(enc_data->fps_ratio) || 0 == EM_PARAM_M(enc_data->fps_ratio)) {
            pParam->u32num_unit_in_tick     = 1;
            pParam->u32time_unit            = enc_data->src_frame_rate;
        }
        else {
            pParam->u32num_unit_in_tick     = EM_PARAM_N(enc_data->fps_ratio);
            pParam->u32time_unit            = enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio);
        }
    }

/************** fill default configure *****************/
    if (enc_data->config_idx < 0)
        set_configure_value(pParam);
    if (forceQP > 0)
        pParam->i8FixedQP[I_SLICE] = pParam->i8FixedQP[P_SLICE] = pParam->i8FixedQP[B_SLICE] = forceQP;
    else {
        //pParam->i8FixedQP[I_SLICE] = pParam->i8FixedQP[P_SLICE] = pParam->i8FixedQP[B_SLICE] = enc_data->init_quant;
        pParam->i8FixedQP[I_SLICE] = iClip3(1, 51, enc_data->init_quant - enc_data->ip_offset);
        pParam->i8FixedQP[P_SLICE] = iClip3(1, 51, enc_data->init_quant);
        pParam->i8FixedQP[B_SLICE] = iClip3(1, 51, enc_data->init_quant + gPBOffset);
    }
#ifdef ENABLE_MB_RC
    if (gMBRCBasicUnit > 0)
        pParam->u32RCBasicUnit = enc_data->frame_width*gMBRCBasicUnit/16;
    else
        pParam->u32RCBasicUnit = enc_data->frame_width/16;
#endif
    if (pParam->bScalingMatrixFlag)
        pParam->pScaleList = &gScalingList;
    else
        pParam->pScaleList = NULL;

    return 0;
}

static int favce_set_vui_params(struct favce_data_t *enc_data)
{
    FAVC_ENC_VUI_PARAM tVUI;
    int fill_vui = 0;
    if (NULL == enc_data->handler)
        return -1;
    memset(&tVUI, 0, sizeof(FAVC_ENC_VUI_PARAM));
    if (enc_data->vui_sar_param) {
        tVUI.u32SarWidth = EM_PARAM_WIDTH(enc_data->vui_sar_param);
        tVUI.u32SarHeight = EM_PARAM_HEIGHT(enc_data->vui_sar_param);
        fill_vui = 1;
    }
    if (enc_data->vui_format_param&BIT28) { // timing_info_present_flag[28]
        tVUI.bTimingInfoPresentFlag = 1;
        if (0 == enc_data->src_frame_rate) {
            tVUI.u32NumUnitsInTick = 1;
            tVUI.u32TimeScale = 30;
        }
        else {
            if (0 == EM_PARAM_N(enc_data->fps_ratio) || 0 == EM_PARAM_M(enc_data->fps_ratio)) {
                tVUI.u32NumUnitsInTick = 1;
                tVUI.u32TimeScale = enc_data->src_frame_rate;
            }
            else {
                tVUI.u32NumUnitsInTick = EM_PARAM_N(enc_data->fps_ratio);
                tVUI.u32TimeScale = enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio);
            }
        }
        fill_vui = 1;
    }
    else
        tVUI.bTimingInfoPresentFlag = 0;

    if (enc_data->vui_format_param&MASK28) {
        tVUI.u8VideoFormat = (enc_data->vui_format_param>>25)&0x07;
        tVUI.bVideoFullRangeFlag = (enc_data->vui_format_param>>24)&0x01;
        tVUI.u8ColourPrimaries = (enc_data->vui_format_param>>16)&0xFF;
        tVUI.u8TransferCharacteristics = (enc_data->vui_format_param>>8)&0xFF;
        tVUI.u8MatrixCoefficients = enc_data->vui_format_param&0xFF;
        fill_vui = 1;
    }
    if (0 == fill_vui)
        encoder_init_vui(enc_data->handler, NULL);
    else
        encoder_init_vui(enc_data->handler, &tVUI);
    return 0;
}

#ifdef RC_ENABLE
static const int CompressRateTable[52] = {
    83,
    78, 74, 71, 69, 65, 63, 61, 58, 56, 54, // 1~10
    51, 49, 47, 45, 43, 41, 39, 38, 36, 34, // 11~20
    33, 31, 29, 28, 26, 25, 23, 22, 20, 19, // 21~30
    18, 16, 15, 14, 12, 11, 10,  9,  8,  7, // 31~40
     6,  6,  5,  4,  4,  3,  3,  2,  2,  1, // 41~51
     1};

#define REASONABLE_MIN_QUANT    15
static int getMinMaxQuant(void)
{
    int compress_rate;
    int qp;
#ifndef PARSING_GMLIB_BY_VG
    parse_param_cfg(NULL, &compress_rate);
#endif
    // Tuba 20140402: avoid nonreasonable min quant
    for (qp = 1; qp < REASONABLE_MIN_QUANT; qp++) {
        if (CompressRateTable[qp] < compress_rate)
            break;
    }
    gMinQuant = qp;

    return 0;
}

static int favce_rc_init_param(struct favce_data_t *enc_data)
{
    struct rc_init_param_t rc_param;

    // Tuba 20131029: add channel number
    rc_param.chn = enc_data->chn;
    rc_param.rc_mode = enc_data->rc_mode;

    if (0 == enc_data->src_frame_rate) {
        rc_param.fincr = 1;
        rc_param.fbase = 30;
    }
    else {
        if (0 == EM_PARAM_N(enc_data->fps_ratio) || 0 == EM_PARAM_M(enc_data->fps_ratio)) {
            rc_param.fincr = 1;
            rc_param.fbase = enc_data->src_frame_rate;
        }
        else {
            rc_param.fincr = EM_PARAM_N(enc_data->fps_ratio);
            rc_param.fbase = enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio);
        }
    }
    rc_param.bitrate = enc_data->bitrate;
    rc_param.max_bitrate = enc_data->max_bitrate;
    rc_param.mb_count = enc_data->frame_width * enc_data->frame_height / 256;
    if (rc_param.mb_count < 0)
        rc_param.mb_count = 1;
    rc_param.idr_period = enc_data->idr_interval;
    rc_param.bframe = enc_data->b_frm_num;
#ifdef USE_CONFIG_FILE
    #ifdef ENABLE_USER_CONFIG
    if (h264e_user_config && enc_data->config_idx > 0 && gEncConfigure[enc_data->config_idx].valid)
        rc_param.ip_offset = gEncConfigure[enc_data->config_idx].ip_offset;
    else
    #endif
        rc_param.ip_offset = enc_data->ip_offset;
#else
    rc_param.ip_offset = enc_data->ip_offset;
    //rc_param.ip_offset = gIPOffset;
#endif
    rc_param.pb_offset = gPBOffset;
    rc_param.qp_constant = enc_data->init_quant;
    rc_param.qp_step = gQPStep;

    rc_param.max_quant = enc_data->max_quant;
    rc_param.min_quant = enc_data->min_quant;

    // Tuba 20130722: automatically assign min/max quant
    //getMinMaxQuant(&rc_param);
	// Tuba 20130723: not compute min_quant every reinit rate control
    //rc_param.max_quant = gMaxQuant;
    //rc_param.min_quant = gMinQuant;

    rc_param.rate_tolerance_fix = 2048;     // rate_tolerance = rate_tolerance_fix / 4096

    if (rc_dev) {
        return rc_dev->rc_init(&enc_data->rc_handler, &rc_param);
    }
    return 0;
}
#endif

static int favce_init_param(struct favce_data_t *enc_data)
{
    FAVC_ENC_IOCTL_PARAM tParam;
    FAVC_ENC_PARAM *apParam = &tParam.apParam;
    FAVC_ENC_VUI_PARAM tVUI;
    int ret = 0, i;

    memset(&tParam, 0 ,sizeof(FAVC_ENC_IOCTL_PARAM));

    if (favce_set_default_param(apParam, &tVUI, enc_data) < 0)
        return -1;
/*
    if (enc_data->frame_width > h264_max_width || enc_data->frame_height > h264_max_height) {
        printk("frame resolution is over max resolution!\n");
        return -1;
    }
*/
    // base address
    // must get virtual address
#ifdef TWO_ENGINE
    #if defined(FIX_TWO_ENGINE)|defined(DYNAMIC_ENGINE)
        tParam.u32BaseAddr = favce_base_address_va[enc_data->engine];
        tParam.u32VcacheBaseAddr = favce_vcache_address_va[enc_data->engine];
        //DEBUG_M(LOG_MIDDLE, "init param chn = %d, engine = %d\n", enc_data->chn, enc_data->engine);
    #else
        tParam.u32BaseAddr = favce_base_address_va[procAssignEngine];
        tParam.u32VcacheBaseAddr = favce_vcache_address_va[procAssignEngine];
    #endif
#else
    tParam.u32BaseAddr = favce_base_address_va;
    tParam.u32VcacheBaseAddr = favce_vcache_address_va;
#endif
    tParam.pfnMalloc = fkmalloc;
    tParam.pfnFree = fkfree;

#ifdef ACTUALLY_REFERNCE_SIZE
    if (enc_data->buf_width != enc_data->frame_width || enc_data->buf_height != enc_data->frame_height)
#else
    if (enc_data->buf_width != apParam->u32SourceWidth || enc_data->buf_height != apParam->u32SourceHeight)
#endif
    {
        //DEBUG_M(LOG_INFO, "{chn%d} allocate common buffer\n", enc_data->chn);
        /* buf_width & buf_height is mutiple of 16/32 */
        // check not exceed max buffer size
        if (apParam->u32SourceWidth * apParam->u32SourceHeight > h264e_max_width * h264e_max_height || apParam->u32SourceWidth > h264e_max_width) {
            DEBUG_E(LOG_ERROR, "source size is over max size (%d x %d > %d x %d)\n",
                apParam->u32SourceWidth, apParam->u32SourceHeight, h264e_max_width, h264e_max_height);
            return -1;
        }
    #ifdef ACTUALLY_REFERNCE_SIZE
        enc_data->buf_width = enc_data->frame_width;
        enc_data->buf_height = enc_data->frame_height;
    #else
        enc_data->buf_width = apParam->u32SourceWidth;
        enc_data->buf_height = apParam->u32SourceHeight;
    #endif
    }
    for (i = 0; i < MAX_RECON_BUF_NUM; i++)
        release_used_recon(&enc_data->ref_info[i], enc_data->chn);
#ifdef TEMPORAL_DENOISE_ENABLE
    tParam.u8DnResultFormat = 2;
#else
    tParam.u8DnResultFormat = 0;
#endif
    if (gDiDnParam.bUpdateDiDnParam)
        tParam.ptDiDnParam = &gDiDnParam;
    else
        tParam.ptDiDnParam = NULL;

    enc_data->queue_num = enc_data->queue_idx = 0;

#ifdef ENABLE_MB_RC
    if (EM_VRC_EVBR == enc_data->rc_mode) {
        int ip_offset;
        #ifdef ENABLE_USER_CONFIG
        if (h264e_user_config && enc_data->config_idx > 0 && gEncConfigure[enc_data->config_idx].valid)
            ip_offset = gEncConfigure[enc_data->config_idx].ip_offset;
        else
        #endif
            ip_offset = enc_data->ip_offset;
        apParam->u8RCMinQP[I_SLICE] = enc_data->init_quant - ip_offset;
        apParam->u8RCMinQP[P_SLICE] = enc_data->init_quant;
    }
    else {
        int ip_offset;
        #ifdef ENABLE_USER_CONFIG
        if (h264e_user_config && enc_data->config_idx > 0 && gEncConfigure[enc_data->config_idx].valid)
            ip_offset = gEncConfigure[enc_data->config_idx].ip_offset;
        else
        #endif
            ip_offset = enc_data->ip_offset;
        apParam->u8RCMinQP[I_SLICE] = iMax(1, enc_data->min_quant - ip_offset);
        apParam->u8RCMinQP[P_SLICE] = enc_data->min_quant;
    }
    apParam->u8RCMaxQP[I_SLICE] =
    apParam->u8RCMaxQP[P_SLICE] = enc_data->max_quant;
#endif

    if (NULL == enc_data->handler) {
        //DEBUG_M(LOG_INFO, "encoder init start\n");
        if ((ret = encoder_create(&enc_data->handler, &tParam, enc_data->chn)) < 0) {
            DEBUG_E(LOG_ERROR, "{chn%d} initialize encoder error: %d\n", enc_data->chn, ret);
            return ret;
        }
    }
    else {
        //DEBUG_M(LOG_INFO, "encoder re-init start\n");
        if ((ret = encoder_reset(enc_data->handler, &tParam, enc_data->chn)) < 0) {
            DEBUG_E(LOG_ERROR, "{chn%d} re-initial encoder error: %d\n", enc_data->chn, ret);
            return ret;
        }
    }

    return 0;
}

static int favce_get_start_bframe(int engine, int chn)
{
    struct favce_data_t *enc_data;
    enc_data = private_data + chn;
    return enc_data->trigger_b;
}

static ReconBuffer *get_unused_recon(struct favce_data_t *enc_data)
{
    unsigned int Y_sz = enc_data->buf_width * enc_data->buf_height;
    int i;

    for (i = 0; i < MAX_RECON_BUF_NUM; i++) {
        if (!enc_data->ref_info[i].is_used) {
            allocate_pool_buffer(&enc_data->ref_info[i].rec_luma_virt, 
                &enc_data->ref_info[i].rec_luma_phy, enc_data->res_pool_type, enc_data->chn);
            if (0 == enc_data->ref_info[i].rec_luma_virt)
                return NULL;
            enc_data->ref_info[i].rec_chroma_virt = enc_data->ref_info[i].rec_luma_virt + Y_sz;
            enc_data->ref_info[i].rec_chroma_phy = enc_data->ref_info[i].rec_luma_phy + Y_sz;
            enc_data->ref_info[i].is_used = 1;
            DEBUG_M(LOG_INFO, "{chn%d} allocate ref buf %d, pa 0x%x, va 0x%x, size %d\n", enc_data->chn, i, 
                (uint32_t)enc_data->ref_info[i].rec_luma_phy, (uint32_t)enc_data->ref_info[i].rec_luma_virt, Y_sz*3/2);
            return &enc_data->ref_info[i];
        }
    }
    return NULL;
}

static void release_used_recon(ReconBuffer *ref_buf, int chn)
{
    int idx = ref_buf->rec_buf_idx;
    if (ref_buf->rec_luma_virt) {
        DEBUG_M(LOG_INFO, "release ref buffer %d, pa 0x%x, va 0x%x\n", idx, (uint32_t)ref_buf->rec_luma_phy, (uint32_t)ref_buf->rec_luma_virt);
        release_pool_buffer(ref_buf->rec_luma_virt, chn);
    }
    memset(ref_buf, 0, sizeof(ReconBuffer));
    ref_buf->rec_buf_idx = idx;
}

static void release_rec_buffer(ReconBuffer *recBufList, unsigned char release_idx, int cur_rec_buf_idx, int chn)
{
    int idx;
    DEBUG_M(LOG_INFO, "release idx = %d, cur buf idx = %d\n", release_idx, cur_rec_buf_idx);
    if (release_idx != NO_RELEASE_BUFFER) {
        if (release_idx == RELEASE_ALL_BUFFER) {
            for (idx = 0; idx<MAX_RECON_BUF_NUM; idx++)
                if (idx != cur_rec_buf_idx)
                    release_used_recon(&recBufList[idx], chn);
        }
        else
            release_used_recon(&recBufList[release_idx], chn);
    }
}

static int favce_set_frame_buffer(FAVC_ENC_IOCTL_FRAME *pFrame, struct favce_data_t *enc_data, int b_frame)
{
    int i;
    int bs_offset = 0;

    // set source buffer (in buffer)
    if (!b_frame)
        pFrame->mSourceBuffer = &enc_data->cur_src_buf;
    else {
        pFrame->mSourceBuffer = &enc_data->enc_BQueue[enc_data->queue_idx].frame_info;
        enc_data->queue_idx++;
    }
    // set didn buffer
    /*
    if (!b_frame) { // only when temporal di/dn enable
        pFrame->pu8DiDnRef0_virt = (unsigned char *)enc_data->didn_buffer[enc_data->didn_ref_idx].addr_va;
        pFrame->pu8DiDnRef0_phy  = (unsigned char *)enc_data->didn_buffer[enc_data->didn_ref_idx].addr_pa;
        pFrame->pu8DiDnRef1_virt = (unsigned char *)enc_data->didn_buffer[enc_data->didn_ref_idx^0x01].addr_va;
        pFrame->pu8DiDnRef1_phy  = (unsigned char *)enc_data->didn_buffer[enc_data->didn_ref_idx^0x01].addr_pa;
    }
    */
    pFrame->mSourceBuffer->src_buffer_virt = enc_data->in_addr_va;
    pFrame->mSourceBuffer->src_buffer_phy = enc_data->in_addr_pa;
	#ifdef TWO_ENGINE
        pFrame->u32SysInfoAddr_virt = mcp280_sysinfo_buffer[enc_data->engine].addr_va;
        pFrame->u32SysInfoAddr_phy = mcp280_sysinfo_buffer[enc_data->engine].addr_pa;
        if (!enc_data->motion_flag) {
            pFrame->u32MVInfoAddr_virt = mcp280_mvinfo_buffer[enc_data->engine].addr_va;
            pFrame->u32MVInfoAddr_phy = mcp280_mvinfo_buffer[enc_data->engine].addr_pa;
        }
        else {
            bs_offset = enc_data->mb_info_size;
            pFrame->u32MVInfoAddr_virt = enc_data->out_addr_va;
            pFrame->u32MVInfoAddr_phy = enc_data->out_addr_pa;
        }
        if (h264e_max_b_frame) {
            pFrame->u32L1ColMVInfoAddr_virt = enc_data->L1ColMVInfo_buffer.addr_va;
            pFrame->u32L1ColMVInfoAddr_phy = enc_data->L1ColMVInfo_buffer.addr_pa;
        }
        else {
            pFrame->u32L1ColMVInfoAddr_virt = mcp280_l1_col_buffer[enc_data->engine].addr_va;
            pFrame->u32L1ColMVInfoAddr_phy = mcp280_l1_col_buffer[enc_data->engine].addr_pa;
        }
    #else	// TWO_ENGINE
        pFrame->u32SysInfoAddr_virt = mcp280_sysinfo_buffer.addr_va;
        pFrame->u32SysInfoAddr_phy = mcp280_sysinfo_buffer.addr_pa;
        if (!enc_data->motion_flag) {
        #ifdef DISABLE_WRITE_OUT_REC
            if (0 == h264e_dis_wo_buf) {
                pFrame->u32MVInfoAddr_virt = mcp280_mvinfo_buffer.addr_va;
                pFrame->u32MVInfoAddr_phy = mcp280_mvinfo_buffer.addr_pa;
            }
            else {
                pFrame->u32MVInfoAddr_virt = UNKNOWN_ADDRESS;
                pFrame->u32MVInfoAddr_phy = UNKNOWN_ADDRESS;
            }
        #else
            pFrame->u32MVInfoAddr_virt = mcp280_mvinfo_buffer.addr_va;
            pFrame->u32MVInfoAddr_phy = mcp280_mvinfo_buffer.addr_pa;
        #endif
        }
        else {
            //bs_offset = enc_data->buf_width * enc_data->buf_height / 32;
            bs_offset = enc_data->mb_info_size;
            pFrame->u32MVInfoAddr_virt = enc_data->out_addr_va;
            pFrame->u32MVInfoAddr_phy = enc_data->out_addr_pa;
        }
        if (h264e_max_b_frame) {
            pFrame->u32L1ColMVInfoAddr_virt = enc_data->L1ColMVInfo_buffer.addr_va;
            pFrame->u32L1ColMVInfoAddr_phy = enc_data->L1ColMVInfo_buffer.addr_pa;
        }
        else {
        #ifdef DISABLE_WRITE_OUT_REC
            if (0 == h264e_dis_wo_buf) {
                pFrame->u32L1ColMVInfoAddr_virt = mcp280_l1_col_buffer.addr_va;
                pFrame->u32L1ColMVInfoAddr_phy = mcp280_l1_col_buffer.addr_pa;
            }
            else {
                pFrame->u32L1ColMVInfoAddr_virt = UNKNOWN_ADDRESS;
                pFrame->u32L1ColMVInfoAddr_phy = UNKNOWN_ADDRESS;
            }
        #else
            pFrame->u32L1ColMVInfoAddr_virt = mcp280_l1_col_buffer.addr_va;
            pFrame->u32L1ColMVInfoAddr_phy = mcp280_l1_col_buffer.addr_pa;
        #endif
        }
    #endif

    // set bs buffer (out buffer)
    pFrame->u32BSBuffer_virt = (enc_data->out_addr_va + bs_offset);
    pFrame->u32BSBuffer_phy = (enc_data->out_addr_pa + bs_offset);
    pFrame->u32BSBufferSize = enc_data->out_size - bs_offset;
    // set roi QP
    if (enc_data->roi_qp_enable) {
        signed char qp_offset = (signed char)((enc_data->qp_offset>>8)&0xFF);
        // Tuba 20150611: add handler of fixed qp
        if (ROI_DELTA_QP == gEncParam.u8RoiQPType) {
            if (0 != qp_offset && qp_offset > -26 && qp_offset < 25)
                pFrame->apFrame.i8ROIQP = qp_offset;
            else
                pFrame->apFrame.i8ROIQP = gEncParam.i8ROIDeltaQP;
        }
        else if (ROI_FIXED_QP == gEncParam.u8RoiQPType) {
            if (qp_offset > 0 && qp_offset <= 51)
                pFrame->apFrame.i8ROIQP = qp_offset;
            else
                pFrame->apFrame.i8ROIQP = gEncParam.u8ROIFixQP;
        }
        for (i = 0; i < ROI_REGION_NUM; i++) {
            if (enc_data->roi_qp_region[i]) {
                pFrame->apFrame.sROIQPRegion[i].enable = 1;
                // roi qp region (x[24:31], y[16:23], w[8:15], h[0:7])
                pFrame->apFrame.sROIQPRegion[i].roi_x = ((enc_data->roi_qp_region[i]>>24)&0xFF)*16;
                pFrame->apFrame.sROIQPRegion[i].roi_y = ((enc_data->roi_qp_region[i]>>16)&0xFF)*16;
                pFrame->apFrame.sROIQPRegion[i].roi_width = ((enc_data->roi_qp_region[i]>>8)&0xFF)*16;
                pFrame->apFrame.sROIQPRegion[i].roi_height = (enc_data->roi_qp_region[i]&0xFF)*16;
            }
        }
    }
    // Tuba 20130329: change reserved buffer to reserved job
    //pFrame->u32PrevSrcBuffer_phy = enc_data->reserved_buf.buf[0].addr_pa;
    if (enc_data->prev_job) {
        struct video_entity_job_t *job;
        job = ((struct job_item_t*)enc_data->prev_job)->job;
    #ifdef HANDLE_2FRAME
        pFrame->u32PrevSrcBuffer_phy = job->in_buf.buf[0].addr_pa + enc_data->src_buf_offset;
    #else
        pFrame->u32PrevSrcBuffer_phy = job->in_buf.buf[0].addr_pa;
    #endif
    }
    else
        pFrame->u32PrevSrcBuffer_phy = 0;
#ifdef TEMPORAL_DENOISE_ENABLE
    //if (enc_data->didn_mode & TEMPORAL_DENOISE) {
    if (enc_data->real_didn_mode & TEMPORAL_DENOISE) {
        struct video_entity_job_t *job;
        if (enc_data->prev_job) {
            job = ((struct job_item_t*)enc_data->prev_job)->job;
        #ifdef HANDLE_2FRAME
            pFrame->u32DiDnRef0_phy = job->in_buf.buf[0].addr_pa + enc_data->src_buf_offset;
        #else
            pFrame->u32DiDnRef0_phy = job->in_buf.buf[0].addr_pa;
        #endif
        }
        job = ((struct job_item_t*)enc_data->cur_job)->job;
    #ifdef HANDLE_2FRAME
        pFrame->u32DiDnRef1_phy = job->in_buf.buf[0].addr_pa + enc_data->src_buf_offset;
    #else
        pFrame->u32DiDnRef1_phy = job->in_buf.buf[0].addr_pa;
    #endif
    }
#endif

    // get reconstructed buffer
#ifdef SAME_REC_REF_BUFFER
    if (h264e_one_ref_buf) {
    #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
        if (enc_data->small_buf_idx < 0)
            pFrame->mReconBuffer = &enc_data->ref_info[0];
        else {
            pFrame->mReconBuffer = &enc_data->ref_info[enc_data->small_buf_idx];
        }
    #else
        pFrame->mReconBuffer = &enc_data->ref_info[0];
    #endif
    }
    else {
        if ((pFrame->mReconBuffer = get_unused_recon(enc_data)) == NULL)
            return -1;
	}
#else
    if ((pFrame->mReconBuffer = get_unused_recon(enc_data)) == NULL)
        return -1;
#endif
    enc_data->cur_rec_buf_idx = pFrame->mReconBuffer->rec_buf_idx;
    //pFrame->new_frame = 1;

    return 0;

}

#ifdef REORDER_JOB
int favce_move_all_job_tail(struct list_head *head, struct job_item_t *cur_job)
{
    struct job_item_t *job_item, *job_item_next;//, *target_job;
    // put current job at tail
    // Tuba 20140418: del list, use list_for_each_entry_safe
    list_for_each_entry_safe (job_item, job_item_next, head, engine_list) {
    //list_for_each_entry(job_item, head, engine_list) {
        if (job_item->job_id == cur_job->job_id) {
            list_del(&cur_job->engine_list);
            list_add_tail(&cur_job->engine_list, head);
            break;
        }            
    }
    // put other job with the same channel to tail
    do {
        list_for_each_entry_safe (job_item, job_item_next, head, engine_list) {
        //list_for_each_entry(job_item, head, engine_list) {
            if (job_item->chn == cur_job->chn && DRIVER_STATUS_STANDBY == job_item->status)
                break; 
        }
        if (job_item->job_id == cur_job->job_id)
            break;
        DEBUG_M(LOG_DEBUG, "put job %d to tail\n", job_item->job_id);
        list_del(&job_item->engine_list);
        list_add_tail(&job_item->engine_list, head);
    } while (1);
    return 0;
}

int favce_reorder_job(struct job_item_t *cur_job)
{
    int engine = cur_job->engine;

    DEBUG_M(LOG_WARNING, "{chn%d} job %d reorder\n", cur_job->chn, cur_job->job_id);
    if (0 == cur_job->realloc_time) {
        cur_job->realloc_time = video_gettime_ms() & 0xFFFF;
        DEBUG_M(LOG_DEBUG, "{job %d} realloc start 0x%x\n", cur_job->job_id, cur_job->realloc_time);
    }
    cur_job->status = DRIVER_STATUS_STANDBY;
    favce_move_all_job_tail(&favce_engine_head[engine], cur_job);
    return JOB_REORDER;
}
#endif

#ifdef REF_POOL_MANAGEMENT
int register_mem_pool(struct favce_data_t *enc_data, int engine)
{
    int i, chn;
    //unsigned char checker[MAX_CHN_NUM];
    struct favce_data_t *encp;
    unsigned long flags = 0;
    int ret = 0;
    int release_one = 0;
    int release_cnt = 0;
    int over_spec = 0;

    DEBUG_M(LOG_DEBUG, "chn %d: reg type %d, re_reg = %d, over_cnt = %d\n", 
        enc_data->chn, enc_data->res_pool_type, enc_data->re_reg_cnt, enc_data->over_spec_cnt);

    if (realloc_cnt_limit > 0 && enc_data->re_reg_cnt > realloc_cnt_limit) {
        //printm("FE", "chn %d: reg type %d, re_reg = %d\n", enc_data->chn, enc_data->res_pool_type, enc_data->re_reg_cnt);
        DEBUG_E(LOG_ERROR, "[error] buffer re-allocate over %d times!\n", realloc_cnt_limit);
        dump_ref_pool(NULL);
        dump_chn_pool(NULL);
        damnit(MODULE_NAME);
        return -1;
    }
    if (enc_data->over_spec_cnt > over_spec_limit) {
        DEBUG_E(LOG_ERROR, "[error] allocate buffer over spec!\n");
        dump_ref_pool(NULL);
        dump_chn_pool(NULL);
        damnit(MODULE_NAME);
        return -1;
    }

    release_all_buffer(enc_data);
    deregister_ref_pool(enc_data->chn);
    //enc_data->res_pool_type = -1;
    // check vcache enable
//#if defined(ACTUALLY_REFERNCE_SIZE)&defined(ONE_REF_BUF_VCACHE_OFF_SMALL)
#ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
    if (h264e_one_ref_buf && 0 == encoder_check_vcache_enable(enc_data->handler))
        enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width*2, enc_data->buf_height);
    else
        enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width, enc_data->buf_height);
#else
    enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width, enc_data->buf_height);
#endif
    if (enc_data->res_pool_type < 0) {
        /* Tuba 20130718
         *  MEM_ERROR:              something error
         *  NO_SUITABLE_BUF:        can not find suitable buffer, maybe change layout
         *  OVER_SPEC:              --> find suitable buffer but it is full <-- delete
         *                          return error
         *  NOT_OVER_SPEC_ERROR:    find suitable buffer but someone use wrong buffer
         */
        switch (enc_data->res_pool_type) {
        case MEM_ERROR:
            DEBUG_E(LOG_ERROR, "ref buffer error\n");
            damnit(MODULE_NAME);
            return -1;
            break;
        case NO_SUITABLE_BUF:
            DEBUG_E(LOG_ERROR, "no suitable buffer\n");
            damnit(MODULE_NAME);
            return -1;
            break;
        case OVER_SPEC:
            DEBUG_M(LOG_DEBUG, "[pool] release one of buffer which is the same resolution\n");
            //memset(checker, 1, sizeof(checker));
            over_spec = 1;
            if (mark_suitable_buf(enc_data->chn, mem_buf_checker[engine], enc_data->buf_width, enc_data->buf_height) > 0) {
                DEBUG_M(LOG_DEBUG, "[pool] over spec release one buffer\n");
                release_one = 1;
            }
            else {
                DEBUG_M(LOG_DEBUG, "[pool] over spec release all buffer\n");
                for (i = 0; i < h264e_max_chn; i++) {
                    encp = private_data + i;
                    if (encp->res_pool_type >= 0)
                        mem_buf_checker[engine][i] = 1;
                    else
                        mem_buf_checker[engine][i] = 0;
                }
                //memset(mem_buf_checker[engine], 1, sizeof(unsigned char)*h264e_max_chn);
            }
            //return -1;            
            break;
        case NOT_OVER_SPEC_ERROR:
            DEBUG_M(LOG_WARNING, "[pool] release all non-suitable buffer\n");
            if (0 == check_reg_buf_suitable(mem_buf_checker[engine])) {
                if (0 == mark_suitable_buf(enc_data->chn, mem_buf_checker[engine], enc_data->buf_width, enc_data->buf_height)) {
                    //DEBUG_M(LOG_WARNING, "[pool] over spec release all buffer\n");
                    memset(mem_buf_checker[engine], 1, sizeof(unsigned char)*h264e_max_chn);
                }
            }
            break;
        default:
            DEBUG_E(LOG_ERROR, "unknown error\n");
            damnit(MODULE_NAME);            
            return -1;
            break;
        }
        if (over_spec) {
            enc_data->over_spec_cnt++;
            //DEBUG_M(LOG_WARNING, "over spec, release one buffer (%d)\n", enc_data->over_spec_cnt);
        }
        else
            enc_data->over_spec_cnt = 0;

        chn = enc_data->chn;
        for (i = 0; i < h264e_max_chn; i++) {
            chn++;
            if (chn >= h264e_max_chn)
                chn = 0;
            if (0 == mem_buf_checker[engine][chn]) {
                continue;
            }
        #ifdef TWO_ENGINE
            if (chn == favce_cur_chn[engine^0x01])
                continue;
        #endif
            encp = private_data + chn;
            /*
            if (encp->res_pool_type < 0 && encp->handler) {
                DEBUG_E(LOG_ERROR, "{chn%d} res_pool_type is not assign\n", encp->chn);
            }
            */
            encp->re_reg_cnt++;
            release_all_buffer(encp);
            deregister_ref_pool(encp->chn);
            DEBUG_M(LOG_WARNING, "{chn%d} force release ref buffer\n", encp->chn);
            encp->res_pool_type = -1;
            if (encp->handler) {
            #ifdef ENABLE_FAST_FORWARD
                encoder_reset_gop_param(encp->handler, encp->idr_interval, encp->b_frm_num, enc_data->fast_forward);
            #else
                encoder_reset_gop_param(encp->handler, encp->idr_interval, encp->b_frm_num);
            #endif
            }
            release_cnt++;
            if (over_spec) {
                encp->over_spec_cnt = enc_data->over_spec_cnt;
                DEBUG_M(LOG_WARNING, "over spec, release one buffer chn %d (%d)\n", chn, enc_data->over_spec_cnt);
            }
            if (release_one)
                break;
        }
        enc_data->re_reg_cnt++;

        if (release_cnt > 0) {  // re-register
        #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
            if (h264e_one_ref_buf && 0 == encoder_check_vcache_enable(enc_data->handler))
                enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width*2, enc_data->buf_height);
            else
                enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width, enc_data->buf_height);
        #else
            enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width, enc_data->buf_height);
        #endif        
            //enc_data->res_pool_type = register_ref_pool(enc_data->chn, enc_data->buf_width, enc_data->buf_height);
            if (enc_data->res_pool_type < 0) {
                enc_data->re_reg_cnt++;
                // re-order
			#ifdef REORDER_JOB
                if (0 == in_irq())
                    spin_lock_irqsave(&favc_enc_lock, flags);
                ret = favce_reorder_job((struct job_item_t *)enc_data->cur_job);
                if (0 == in_irq())
                    spin_unlock_irqrestore(&favc_enc_lock, flags);
			#endif
            }            
        }
        else
            ret = -1;
    }
    else
        enc_data->re_reg_cnt = 0;
    return ret;
}
#endif

#ifdef DYNAMIC_ENGINE
static int assign_chn_2_engine(struct favce_data_t *enc_data, int ori_engine, struct job_item_t *cur_job)
{
    unsigned long flags = 0;
    //int ret = 0;
    if (0 == in_irq())
        spin_lock_irqsave(&favc_enc_lock, flags);
    DEBUG_M(LOG_DEBUG, "{chn%d} cost %d, chn to engine %d (%d, %d)\n", enc_data->chn, enc_data->channel_cost,
        chn_2_engine[enc_data->chn], engine_cost[0], engine_cost[1]);
    if (enc_data->channel_cost > 0) {
        engine_cost[ori_engine] -= enc_data->channel_cost;
        //chn_2_engine[enc_data->chn] = -1;
        enc_data->channel_cost = 0;
    }
    // assign engine
    if (engine_cost[0] > engine_cost[1]) {
        enc_data->engine = 1;
        chn_2_engine[enc_data->chn] = 1;
    }
    else {
        enc_data->engine = 0;
        chn_2_engine[enc_data->chn] = 0;
    }
	// Tuba 20140612: add frame rarte checker (divide by zero)
    if (0 == EM_PARAM_M(enc_data->fps_ratio) || 0 == EM_PARAM_N(enc_data->fps_ratio)) {
        if (0 == enc_data->src_frame_rate)  // if no frame rate
            enc_data->channel_cost =  (EM_PARAM_WIDTH(enc_data->src_dim)+15)/16 * (EM_PARAM_HEIGHT(enc_data->src_dim)+15)/16 * 30;
        else
            enc_data->channel_cost =  (EM_PARAM_WIDTH(enc_data->src_dim)+15)/16 * (EM_PARAM_HEIGHT(enc_data->src_dim)+15)/16 * enc_data->src_frame_rate;
    }
    else
        enc_data->channel_cost =  (EM_PARAM_WIDTH(enc_data->src_dim)+15)/16 * (EM_PARAM_HEIGHT(enc_data->src_dim)+15)/16 *
            enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio) / EM_PARAM_N(enc_data->fps_ratio);
    DEBUG_M(LOG_DEBUG, "assign chn %d to engine %d, cost = %d (%d, %d)\n", 
            enc_data->chn, enc_data->engine, enc_data->channel_cost, engine_cost[0], engine_cost[1]);
    engine_cost[enc_data->engine] += enc_data->channel_cost;
    //mcp280_debug("assign chn %d to engine %d, cost = %d (%d, %d)\n", 
    //        enc_data->chn, enc_data->engine, enc_data->channel_cost, engine_cost[0], engine_cost[1]);
    // move all jobs which are the same engine
    if (ori_engine != enc_data->engine) {
        struct job_item_t *job_item, *job_item_next;
        struct job_item_t *job_list[MAX_JOB_NUM];
        int job_num = 0, i;
        cur_job->status = DRIVER_STATUS_STANDBY;
        list_for_each_entry_safe(job_item, job_item_next, &favce_engine_head[ori_engine], engine_list) {
            //if (job_item->chn == enc_data->chn && DRIVER_STATUS_STANDBY == job_item->status) {
            if (job_item->chn == enc_data->chn) {
                job_list[job_num] =job_item;
                job_item->engine = enc_data->engine;
                list_del(&job_item->engine_list);
                job_num++;
                DEBUG_M(LOG_DEBUG, "move job %d from engine%d to engine%d\n", job_item->job_id, ori_engine, enc_data->engine);
                //mcp280_debug("move job %d from engine%d to engine%d\n", job_item->job_id, ori_engine, enc_data->engine);
                if (job_num >= MAX_JOB_NUM)
                    break;
            }
        }
        for (i = 0; i < job_num; i++)
            list_add_tail(&job_list[i]->engine_list, &favce_engine_head[enc_data->engine]);
    }
    if (0 == in_irq())
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    return 0;
}
#endif

static int favce_start_job(struct job_item_t *job_item, int b_frame)
{
    struct favce_data_t *enc_data;
    struct favce_pre_data_t pre_data;
    struct video_entity_job_t *job;
    int engine, chn;
    //FAVC_ENC_IOCTL_FRAME tFrame;
    FAVC_ENC_IOCTL_FRAME *pFrame;
    int ret = 0;
    unsigned long flags = 0;

    DEBUG_M(LOG_DEBUG, "{chn%d} engine%d start job %d\n", job_item->chn, job_item->engine, job_item->job_id);
    job = (struct video_entity_job_t *)job_item->job;
    engine = job_item->engine;
    chn = job_item->chn;
    enc_data = private_data + chn;
    pFrame = &enc_data->tFrame;
    job_item->is_b_frame = b_frame;

    if (favce_parse_in_property(enc_data, job) < 0) {
        DEBUG_E(LOG_ERROR, "{chn%d} parsing property error\n", enc_data->chn);
        return -1;
    }
    enc_data->engine = engine;
    enc_data->chn = chn;
    enc_data->cur_job = (void *)job_item;
    memset(pFrame, 0 , sizeof(FAVC_ENC_IOCTL_FRAME));
    if (enc_data->updated) {
        DEBUG_M(LOG_DEBUG, "{engine%d, chn%d} job %d: update 0x%x\n", job_item->engine, job_item->chn, job_item->job_id, enc_data->updated);
        //mcp280_debug("{engine%d, chn%d} job %d: update 0x%x\n", job_item->engine, job_item->chn, job_item->job_id, enc_data->updated);
        /*  H264E_REINIT        0x01
         *  H264E_RC_REINIT     0x02
         *  H264E_GOP_REINIT    0x04
         *  H264E_ROI_REINIT    0x08
         *  H264E_VUI_REINIT    0x10
         *  H264E_FORCE_INTRA   0x20    */
        if (H264E_REINIT & enc_data->updated) {
        #ifdef DYNAMIC_ENGINE
            if (h264e_dynamic_engine) {
                assign_chn_2_engine(enc_data, engine, job_item);
                if (enc_data->engine != engine) {
                    enc_data->updated = H264E_REINIT;
                    return CHANGE_ENGINE;
                }
                job_item->engine = engine = enc_data->engine;
            }
        #endif
            enc_data->first_frame = 1;
            if (favce_init_param(enc_data) < 0)
                return -1;
            release_all_buffer(enc_data);
        #ifdef REF_POOL_MANAGEMENT
            deregister_ref_pool(enc_data->chn);
            enc_data->res_pool_type = -1;
        #endif
            if (enc_data->prev_job) {
                trigger_callback_finish(enc_data->prev_job, FUN_START);
                enc_data->prev_job = NULL;
            }
            favce_set_vui_params(enc_data);
            if (rc_dev) {
                if (favce_rc_init_param(enc_data) < 0) {
                    DEBUG_E(LOG_ERROR, "{chn%d} rc init fail\n", job_item->chn);
                    enc_data->rc_handler = NULL;
                }
            }
        }
        else {
            if (H264E_RC_REINIT & enc_data->updated) {
                // update rate control parameter
				if (rc_dev) {
	                if (favce_rc_init_param(enc_data) < 0) {
	                    DEBUG_E(LOG_ERROR, "{chn%d} rc init fail\n", job_item->chn);
                        enc_data->rc_handler = NULL;
                    }
	            }
            }
            if (H264E_GOP_REINIT & enc_data->updated) {
                release_all_buffer(enc_data);
            #ifdef SAME_REC_REF_BUFFER
                if (h264e_one_ref_buf) {
                    deregister_ref_pool(enc_data->chn);
                    enc_data->res_pool_type = -1;
                }
            #endif
               //encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num);
            #ifdef ENABLE_FAST_FORWARD
                encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num, enc_data->fast_forward);
            #else
                encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num);
            #endif
            }
            if (H264E_VUI_REINIT & enc_data->updated) {
                favce_set_vui_params(enc_data);
            }
            if (H264E_WK_UPDATE & enc_data->updated) {
                encoder_reset_wk_param(enc_data->handler, enc_data->watermark_flag, 
                    enc_data->watermark_init_interval, enc_data->watermark_pattern);
            }
        }
        enc_data->updated = 0;
        enc_data->stop_job = 0;
    #ifdef CHECK_ENGINE_BALANCE
        // can not reset counter, because re-balance will update this parameter
        //gUnbalanceCounter = 0;
    #endif
    }
    // check pre process input parameter
    if (NULL == enc_data->handler) {
        DEBUG_E(LOG_ERROR, "{chn%d} encoder is not initialized!\n", enc_data->chn);
        return -1;
    }

    // must before register buffer (because release non-suitable buffer must avoid next engine)
    if (0 == in_irq()) {
        spin_lock_irqsave(&favc_enc_lock, flags);
    #ifdef TWO_ENGINE
        favce_cur_chn[engine] = job_item->chn;
    #else
        favce_cur_chn = job_item->chn;
    #endif
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    }
    else {
    #ifdef TWO_ENGINE
        favce_cur_chn[engine] = job_item->chn;
    #else
        favce_cur_chn = job_item->chn;
    #endif
    }
#ifdef DYNAMIC_GOP_ENABLE
    if (1 == h264e_dynamic_gop && 1 == enc_data->motion_state) {   // check I frame
        if (I_SLICE == encoder_get_slice_type(enc_data->handler)) {
            encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval*gop_factor, enc_data->b_frm_num, enc_data->fast_forward);
            enc_data->motion_state = 2;
            DEBUG_M(LOG_DEBUG, "static: change gop %d\n", enc_data->idr_interval*gop_factor);
        }
    }
    else if (1 == h264e_dynamic_gop && 3 == enc_data->motion_state) {
        if (encoder_check_iframe(enc_data->handler, enc_data->idr_interval)) {
            encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num, enc_data->fast_forward);
            enc_data->motion_state = 0;
            DEBUG_M(LOG_DEBUG, "motion: change gop %d, cnt = %d\n", enc_data->idr_interval, enc_data->static_event_cnt);
        }
    }
#endif    
    
#ifdef REF_POOL_MANAGEMENT
    if (0 == in_irq())
        spin_lock_irqsave(&favc_enc_lock, flags);        
    if (enc_data->res_pool_type < 0) {
        ret = register_mem_pool(enc_data, engine);
        if (ret < 0) {
            if (0 == in_irq())
                spin_unlock_irqrestore(&favc_enc_lock, flags);
            return ret;
        }
    #ifdef SAME_REC_REF_BUFFER
        if (h264e_one_ref_buf) {
            // 1. check vcache enable
            if (0 == encoder_check_vcache_enable(enc_data->handler)) {
            #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
                int ref_size, frame_size;
                frame_size = enc_data->frame_width*enc_data->frame_height;
                ref_size = get_ref_buffer_size(enc_data->res_pool_type);
                if (ref_size >= frame_size*2) {
                    if (allocate_pool_buffer(&enc_data->ref_info[0].rec_luma_virt, &enc_data->ref_info[0].rec_luma_phy, enc_data->res_pool_type, enc_data->chn) < 0)
                        return -1;
                    if (0 == enc_data->ref_info[0].rec_luma_virt)
                        return -1;
                    enc_data->ref_info[0].rec_chroma_virt = enc_data->ref_info[0].rec_luma_virt + frame_size;
                    enc_data->ref_info[0].rec_chroma_phy = enc_data->ref_info[0].rec_luma_phy + frame_size;
                    enc_data->ref_info[0].is_used = 1;

                    enc_data->ref_info[1].rec_luma_phy = enc_data->ref_info[0].rec_luma_phy + frame_size*3/2;
                    enc_data->ref_info[1].rec_chroma_phy = enc_data->ref_info[1].rec_luma_phy + frame_size;
                    enc_data->ref_info[1].is_used = 1;
                    enc_data->small_buf_idx = 0;
                }
                else {
                    favce_err("{chn%d} use one reference buffer, but can not allocate suitable buffer for resolution %d x %d (buffer size %d, type %d)\n", 
                        enc_data->chn, enc_data->frame_width, enc_data->frame_height, ref_size, enc_data->res_pool_type);
                    return -1;
                }
            #else
                favce_err("{chn%d} when use one reference buffer, encoder support width from 208 to %d (cur width is %d)\n", 
                    enc_data->chn, VCACHE_MAX_REF0_WIDTH, enc_data->frame_width);
                return -1;
            #endif
            }
            else {
                if (allocate_pool_buffer(&enc_data->ref_info[0].rec_luma_virt, &enc_data->ref_info[0].rec_luma_phy, enc_data->res_pool_type, enc_data->chn) < 0)
                    return -1;
                if (0 == enc_data->ref_info[0].rec_luma_virt)
                    return -1;
                enc_data->ref_info[0].rec_chroma_virt = enc_data->ref_info[0].rec_luma_virt + enc_data->buf_width*enc_data->buf_height;
                enc_data->ref_info[0].rec_chroma_phy = enc_data->ref_info[0].rec_luma_phy + enc_data->buf_width*enc_data->buf_height;
                enc_data->ref_info[0].is_used = 1;
            #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
                enc_data->small_buf_idx = -1;
            #endif
            }
            //DEBUG_M(LOG_INFO, "{chn%d} allocate ref buf %d, pa 0x%x, va 0x%x, size %d\n", enc_data->chn, i, 
            //    (uint32_t)enc_data->ref_info[i].rec_luma_phy, (uint32_t)enc_data->ref_info[i].rec_chroma_phy, Y_sz*3/2);
        }
    #endif
    }
    else
        enc_data->re_reg_cnt = 0;
    //enc_data->res_pool_type = register_ref_pool(job_item->chn, enc_data->buf_width, enc_data->buf_height);
    if (0 == in_irq())
        spin_unlock_irqrestore(&favc_enc_lock, flags);
#endif

    pFrame->u32ROI_x = EM_PARAM_X(enc_data->src_xy);
    pFrame->u32ROI_y = EM_PARAM_Y(enc_data->src_xy);

    pFrame->apFrame.bForceIntra = 0;
    pFrame->new_frame = 1;   // Tuba 20130326_0: fill new_frame to decide slice_type
    pFrame->mSourceBuffer = &enc_data->cur_src_buf;

    pre_data.force_intra = 0;
    if (b_frame || (encoder_get_slice_type(enc_data->handler) != B_SLICE)) {
        if (0 == job_item->out_buf_alloc) {
            favce_preprocess(job_item, &pre_data);
            job_item->out_buf_alloc = 1;
        }
        if (job->out_buf.buf[0].addr_va == 0 || job->out_buf.buf[0].size == 0) {
            DEBUG_M(LOG_ERROR, "{chn%d} job out buffer address = 0 (0x%x) or size = 0 (%d)\n", 
                enc_data->chn, job->out_buf.buf[0].addr_va, job->out_buf.buf[0].size);
            //damnit(MODULE_NAME);
            return -1;
        }
    #ifdef LOCK_JOB_STAUS_ENABLE
        job_item->process_state |= ID_PRE_PROCESS;
    #endif
        if (pre_data.force_intra) {
            enc_data->force_intra = 1;
            DEBUG_M(LOG_DEBUG, "{chn%d} receive force intra\n", enc_data->chn);
        }
    #ifdef HANDLE_2FRAME
        enc_data->in_addr_va = job->in_buf.buf[0].addr_va + enc_data->src_buf_offset;
        enc_data->in_addr_pa = job->in_buf.buf[0].addr_pa + enc_data->src_buf_offset;
    #else
        enc_data->in_addr_va = job->in_buf.buf[0].addr_va;
        enc_data->in_addr_pa = job->in_buf.buf[0].addr_pa;
    #endif
        enc_data->in_size = job->in_buf.buf[0].size;
        enc_data->out_addr_va = job->out_buf.buf[0].addr_va;
        enc_data->out_addr_pa = job->out_buf.buf[0].addr_pa;
        enc_data->out_size = job->out_buf.buf[0].size;
        // Tuba 20130326_0: allocate ref frame only when actually encode one frame
        if (favce_set_frame_buffer(pFrame, enc_data, b_frame) < 0) {
        #ifdef REORDER_JOB
            if (job_item->realloc_time) {
                unsigned int end = video_gettime_ms()&0xFFFF;
                unsigned int period;
                if (end >= job_item->realloc_time)
                    period = end - job_item->realloc_time;
                else 
                    period = 0x10000 + end - job_item->realloc_time;
                if (period > 500) {
                    DEBUG_E(LOG_ERROR, "{chn%d} job %d reallocate buffer more than %d ms (%d -> %d)\n", 
                        job_item->chn, job_item->job_id, period, job_item->realloc_time, end);
                    damnit(MODULE_NAME);
                    return -1;
                }                    
            }
            if (0 == in_irq())
                spin_lock_irqsave(&favc_enc_lock, flags);
            #ifdef RELEASE_ALL_REFERENCE
            /* release all referecne buffer because block */
            {
                int i;
                struct favce_data_t *encp;
                for (i = 0; i < h264e_max_chn; i++) {
                #ifdef TWO_ENGINE
                    if (i == favce_cur_chn[engine^0x01])
                        continue;
                #endif
                    encp = private_data + i;
                    if (encp->handler) {
                        release_all_buffer(encp);
                    #ifdef SAME_REC_REF_BUFFER
                        if (h264e_one_ref_buf) {
                            deregister_ref_pool(enc_data->chn);
                            enc_data->res_pool_type = -1;
                        }
                    #endif
                        //encoder_reset_gop_param(encp->handler, encp->idr_interval, encp->b_frm_num);
                    #ifdef ENABLE_FAST_FORWARD
                        encoder_reset_gop_param(encp->handler, encp->idr_interval, encp->b_frm_num, encp->fast_forward);
                    #else
                        encoder_reset_gop_param(encp->handler, encp->idr_interval, encp->b_frm_num);
                    #endif
                    }
                }
            }
            #else
                release_all_buffer(enc_data);
                #ifdef SAME_REC_REF_BUFFER
                    if (h264e_one_ref_buf) {
                        deregister_ref_pool(enc_data->chn);
                        enc_data->res_pool_type = -1;
                    }
                #endif
                #ifdef ENABLE_FAST_FORWARD
                    encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num, enc_data->fast_forward);
                #else
                    encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num);
                #endif
            #endif  // RELEASE_ALL_REFERENCE

            ret = favce_reorder_job(job_item);
            if (0 == in_irq())
                spin_unlock_irqrestore(&favc_enc_lock, flags);
            return ret;
        #else
            DEBUG_M(LOG_ERROR, "{chn%d} encoder set frame buffer error\n", enc_data->chn);
            //damnit(MODULE_NAME);
            return -1;
        #endif
        }
        //if (enc_data->force_intra && !b_frame) {
        if (enc_data->force_intra && !b_frame && enc_data->slice_type < 0) {
            DEBUG_M(LOG_INFO, "{chn%d} force intra\n", enc_data->chn);
            pFrame->apFrame.bForceIntra = 1;
            enc_data->force_intra = 0;
        }
    }
    if (enc_data->first_frame) {
        if (enc_data->slice_type >= 0 && EM_SLICE_TYPE_I != enc_data->slice_type) {
            DEBUG_W(LOG_WARNING, "first frame must be I frame\n");
            enc_data->slice_type = EM_SLICE_TYPE_I;
        }
    }
    pFrame->apFrame.i32ForceSliceType = enc_data->slice_type;

    if (0 == in_irq())
        spin_lock_irqsave(&favc_enc_lock, flags);

    //enc_data->cur_job = job_item;
    ret = encode_one_frame_trigger(enc_data->handler, pFrame, enc_data->first_frame, b_frame);
    if (b_frame)
        enc_data->queue_num--;
    if (ret < 0) {
        DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger one frame error\n", enc_data->chn);
    #ifdef SAME_REC_REF_BUFFER
        if (0 == h264e_one_ref_buf)
    #endif
        release_used_recon(pFrame->mReconBuffer, enc_data->chn);
        //return -1;
        goto exit_favce_start_job;
    }
    else if (FRAME_WAIT == ret) {
        // Tuba 20130326_0: B frame not assign refernece frame
        //release_used_recon(tFrame.mReconBuffer);
        // keep frame info
        memcpy(&enc_data->enc_BQueue[enc_data->queue_num].frame_info, pFrame->mSourceBuffer, sizeof(SourceBuffer));
        enc_data->enc_BQueue[enc_data->queue_num].job_item = job_item;
        enc_data->queue_num++;
        enc_data->trigger_b = 0;
        //DEBUG_M(LOG_INFO, "{chn%d} buffered B frame (queue num %d, idx %d)\n", enc_data->chn, enc_data->queue_num, enc_data->queue_idx);
        ret = FRAME_WAIT;
        goto exit_favce_start_job;
        //return FRAME_WAIT;
    }
    else {
        //enc_data->queue_num = 0;
        if (!b_frame) {
            enc_data->queue_idx = 0;
            enc_data->trigger_b = 1;
        }
    }
    DEBUG_M(LOG_INFO, "{chn%d} encoder trigger one slice: job %d\n", enc_data->chn, job_item->job_id);

#ifdef RC_ENABLE
    if (rc_dev) {
    #ifdef OVERFLOW_REENCODE
        enc_data->cur_qp = encoder_rc_set_quant(enc_data->handler, 
            rc_dev->rc_get_quant, enc_data->rc_handler, enc_data->force_qp);
        enc_data->force_qp = -1;
    #else
        encoder_rc_set_quant(enc_data->handler, 
            rc_dev->rc_get_quant, enc_data->rc_handler, -1);
    #endif
    }
    else {
    #ifdef OVERFLOW_REENCODE
        enc_data->cur_qp = encoder_rc_set_quant(enc_data->handler, NULL, enc_data->rc_handler, -1);
    #else
        encoder_rc_set_quant(enc_data->handler, NULL, enc_data->rc_handler, -1);
    #endif
    }
#endif
    if ((ret = encode_one_slice_trigger(enc_data->handler)) < 0) {
        DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger one slice error: %d\n", enc_data->chn, ret);
    #ifdef SAME_REC_REF_BUFFER
        if (0 == h264e_one_ref_buf)
    #endif
        release_used_recon(pFrame->mReconBuffer, enc_data->chn);
        goto exit_favce_start_job;
        //return -1;
    }
    
    DEBUG_M(LOG_INFO, "{chn%d} trigger slice done\n", enc_data->chn);
    enc_data->first_frame = 0;
    mark_engine_start(engine);

exit_favce_start_job:
    if (0 == in_irq())
        spin_unlock_irqrestore(&favc_enc_lock, flags);

    return ret;
}

#if defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)
#ifdef USE_KTHREAD
#ifndef TWO_ENGINE
static int favce_engine_schedule(void *data)
{
    int status;

    favce_job_thread_ready = 1;
    do {
        status = wait_event_timeout(favce_job_waitqueue, favce_job_wakeup_event, 1);
        if (0 ==status)
            continue;   /* timeout */
        favce_job_wakeup_event = 0;
        work_scheduler(0);
    } while (!kthread_should_stop());
    favce_job_thread_ready = 0;
    return 0;
}
static void favce_engine_wakeup(void)
{
    favce_job_wakeup_event = 1;
    wake_up(&favce_job_waitqueue);
}
#else   // TWO_ENGINE
static int favce_engine0_schedule(void *data)
{
    int status;

    favce_job_thread_ready[0] = 1;
    do {
        status = wait_event_timeout(favce_job_waitqueue[0], favce_job_wakeup_event[0], 1);
        if (0 ==status)
            continue;   /* timeout */
        favce_job_wakeup_event[0] = 0;
        work_scheduler(0);
    } while (!kthread_should_stop());
    favce_job_thread_ready[0] = 0;
    return 0;
}
static void favce_engine0_wakeup(void)
{
    favce_job_wakeup_event[0] = 1;
    wake_up(&favce_job_waitqueue[0]);
}

static int favce_engine1_schedule(void *data)
{
    int status;

    favce_job_thread_ready[1] = 1;
    do {
        status = wait_event_timeout(favce_job_waitqueue[1], favce_job_wakeup_event[1], 1);
        if (0 ==status)
            continue;   /* timeout */
        favce_job_wakeup_event[1] = 0;
        work_scheduler(1);
    } while (!kthread_should_stop());
    favce_job_thread_ready[1] = 0;
    return 0;
}
static void favce_engine1_wakeup(void)
{
    favce_job_wakeup_event[1] = 1;
    wake_up(&favce_job_waitqueue[1]);
}
#endif  // TWO_ENGINE
#else   // USE_KTHREAD
//#ifdef REORDER_JOB
void work_scheduler_engine0(void)
{
    if (sleep_time) {
        set_current_state(TASK_INTERRUPTIBLE);
        DEBUG_M(LOG_DEBUG, "{engine0} allocate fail: sleep %d ms\n", sleep_time);
        schedule_timeout(msecs_to_jiffies(sleep_time));
    }
    DEBUG_M(LOG_INFO, "engine0 wk\n");
    work_scheduler(0);
}
#ifdef TWO_ENGINE
void work_scheduler_engine1(void)
{
    if (sleep_time) {
        set_current_state(TASK_INTERRUPTIBLE);
        DEBUG_M(LOG_DEBUG, "{engine1} allocate fail: sleep %d ms\n", sleep_time);
        schedule_timeout(msecs_to_jiffies(sleep_time));
    }
    DEBUG_M(LOG_INFO, "engine1 wk\n");
    work_scheduler(1);
}
#endif  // TWO_ENGINE
#endif  // USE_KTHREAD
#endif  // defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)

/************************************
 * scan job list
 * if DRIVER_STATUS_STANDBY
 *  start job
 * else if DRIVER_STATUS_BUFFERED
 *  check start b
 *  start job
 * if start job error
 *  ret = 0 & restart list
 ***********************************/
void work_scheduler(int engine)
{
/* if not call by work queue, does not need to spin lock */
    struct job_item_t *job_item;
    struct job_item_t *target_job, *buffer_job;
    int ret2 = 0;
    //int b_frame = 0;
    //int mutex_on = 0;
    unsigned long flags = 0;

    //engine = cur_work_engine;
#ifdef HANDLE_PANIC_STATUS
    if (gPanicFlag) {
        DEBUG_M(LOG_ERROR, "system panic, stop!\n");
        return;
    }
#endif
    // Tuba 20140120: add engine checker
    if (engine >= MAX_ENGINE_NUM) {
        DEBUG_M(LOG_ERROR, "engine over max engine number (%d >= %d)\n", engine, MAX_ENGINE_NUM);
        return;
    }
    if (list_empty(&favce_engine_head[engine])) {
        DEBUG_M(LOG_INFO, "{engine%d} list empty", engine);
        return;
    }
find_next_job:
    if (0 == in_irq()) {
        spin_lock_irqsave(&favc_enc_lock, flags);
    }
    if (!is_engine_idle(engine)) {
        DEBUG_M(LOG_INFO, "engine%d is not idle\n", engine);
        //damnit(MODULE_NAME);
        if (0 == in_irq()) {
            spin_unlock_irqrestore(&favc_enc_lock, flags);
        }
        return;
    }
    target_job = NULL;
    buffer_job = NULL;
    list_for_each_entry(job_item, &favce_engine_head[engine], engine_list) {
        if (job_item->status == DRIVER_STATUS_STANDBY) {
            target_job = job_item;
            break;
        }
        else if (job_item->status == DRIVER_STATUS_BUFFERED && favce_get_start_bframe(engine, job_item->chn)) {
            buffer_job = job_item;
            break;
        }
    }
    job_item = NULL;
    if (target_job || buffer_job) {
        if (target_job) {
            job_item = target_job;
            job_item->is_b_frame = 0;
        }
        else if (buffer_job) {
            job_item = buffer_job;
            job_item->is_b_frame = 1;
        }
        //set_engine_busy(job_item->engine);
        set_engine_busy(engine);
        job_item->status = DRIVER_STATUS_ONGOING;
    }
    if (0 == in_irq()) {
        spin_unlock_irqrestore(&favc_enc_lock, flags);
    }
    if (job_item) {
        ret2 = favce_start_job(job_item, 0);
        #ifdef REORDER_JOB
        if (JOB_REORDER == ret2 || ONLY_ONE_JOB == ret2) {
            set_engine_idle(engine);
        #ifdef USE_KTHREAD
            #ifndef TWO_ENGINE
            favce_engine_wakeup();
            #else
            if (0 == engine)
                favce_engine0_wakeup();
            else if (1 == engine)
                favce_engine1_wakeup();
            #endif
        #else   // USE_KTHREAD
            #ifdef TWO_ENGINE
            if (0 == engine) {
                PREPARE_DELAYED_WORK(&process_reorder_job_work0, (void *)work_scheduler_engine0);
                queue_delayed_work(favce_reorder_job_wq[0], &process_reorder_job_work0, 0);
            }
            else if (1 == engine) {
                PREPARE_DELAYED_WORK(&process_reorder_job_work1, (void *)work_scheduler_engine1);
                queue_delayed_work(favce_reorder_job_wq[1], &process_reorder_job_work1, 0);
            }
            #else
            PREPARE_DELAYED_WORK(&process_reorder_job_work, (void *)work_scheduler_engine0);
            queue_delayed_work(favce_reorder_job_wq, &process_reorder_job_work, 0);
            #endif
        #endif
        }
        #endif
        #ifdef DYNAMIC_ENGINE
        else if (CHANGE_ENGINE == ret2) {
            int other_engine = engine^0x01;
            set_engine_idle(engine);
            if (0 == other_engine) {    // Tuba 20140701: fix bug. when change engine, must trigger changed engine
                if (is_engine_idle(other_engine)) {
                    DEBUG_M(LOG_DEBUG, "job change to engine %d, engine idle\n", other_engine);
                #ifdef USE_KTHREAD
                    favce_engine0_wakeup();
                #else
                    PREPARE_DELAYED_WORK(&process_reorder_job_work0, (void *)work_scheduler_engine0);
                    queue_delayed_work(favce_reorder_job_wq[0], &process_reorder_job_work0, 0);
                #endif
                }
            }
            else if (1 == other_engine) {
                if (is_engine_idle(other_engine)) {
                    DEBUG_M(LOG_DEBUG, "job change to engine %d, engine idle\n", other_engine);
                #ifdef USE_KTHREAD
                    favce_engine1_wakeup();
                #else
                    PREPARE_DELAYED_WORK(&process_reorder_job_work1, (void *)work_scheduler_engine1);
                    queue_delayed_work(favce_reorder_job_wq[1], &process_reorder_job_work1, 0);
                #endif
                }
            }
            goto find_next_job;
        }
        #endif
        else if (ret2 < 0) {
            struct video_entity_job_t *job;
            DEBUG_M(LOG_ERROR, "job %d callback fail\n", job_item->job_id);
            job = (struct video_entity_job_t *)job_item->job;
            if (job->out_buf.buf[0].addr_pa != 0) {
                favce_postprocess(job_item, 0);
            }
            set_engine_idle(engine);
            trigger_callback_fail(job_item, FUN_WORK);
            goto find_next_job; // find next job
        }                    
        else if (FRAME_WAIT == ret2) {
        #ifdef LOCK_JOB_STAUS_ENABLE
            if (0 == in_irq()) {
                spin_lock_irqsave(&favc_enc_lock, flags);
                job_item->status = DRIVER_STATUS_BUFFERED;
                spin_unlock_irqrestore(&favc_enc_lock, flags);
            }
            else
        #endif
            job_item->status = DRIVER_STATUS_BUFFERED;
            set_engine_idle(engine);
            //ret = 0;    // job buffered, find next job
            goto find_next_job;
        }
    }
}


static int favce_putjob(void *parm)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)parm;
    struct job_item_t *job_item;
    int idx;
    unsigned long flags;

    DEBUG_M(LOG_INFO, "{chn%d} put job %d\n", ENTITY_FD_MINOR(job->fd), job->id);
    job_item = mcp280_job_alloc(sizeof(struct job_item_t));
    if (job_item == 0) {
        favce_err("can not allocate job item job %d\n", job->id);
        job->status = JOB_STATUS_FAIL;
        job->callback(job);
        damnit(MODULE_NAME);
        return JOB_STATUS_ONGOING;
    }

    job_item->job = job;
    job_item->job_id = job->id;
    job_item->chn = ENTITY_FD_MINOR(job->fd);
    if (job_item->chn >= h264e_max_chn)
        panic("Error to use %s chn %d, max is %d\n", MODULE_NAME, job_item->chn, h264e_max_chn);

    if (job->in_buf.buf[0].addr_va == 0) {
        DEBUG_E(LOG_ERROR, "job %d in buffer address = 0\n", job->id);
        damnit(MODULE_NAME);
        return JOB_STATUS_ONGOING;
    }

    // Tuba 21031030: get out buffer by out buffer address
    job_item->status = DRIVER_STATUS_STANDBY;
    job_item->puttime = video_gettime_us();
    job_item->starttime = job_item->finishtime = 0;
    job_item->realloc_time = 0;
    job_item->out_buf_alloc = 0;
#ifdef HANDLE_BS_CACHEABLE2
    job_item->bs_length = 0;
#endif

    // Tuba 20140717: lock assign engine & insert to engine list to protect chn2engine & engine list is the same
    spin_lock_irqsave(&favc_enc_lock, flags);   // lock chn_2_engine
#ifdef TWO_ENGINE
    #ifdef FIX_TWO_ENGINE
        job_item->engine = chn_2_engine[job_item->chn];
        idx = job_item->chn;
    #elif defined(DYNAMIC_ENGINE)
        //if (chn_2_engine[job_item->chn] < 0)
        //    job_item->engine = 0;
        //else 
            job_item->engine = chn_2_engine[job_item->chn];
        idx = job_item->chn;
    #else
        job_item->engine = procAssignEngine;
        idx = job_item->chn;
    #endif
    //spin_unlock_irqrestore(&favc_enc_lock, flags);
#else
    job_item->engine = 0;
    idx = job_item->chn;
#endif

    INIT_LIST_HEAD(&job_item->engine_list);
    INIT_LIST_HEAD(&job_item->minor_list);
    //spin_lock_irqsave(&favc_enc_lock, flags);
    list_add_tail(&job_item->engine_list, &favce_engine_head[job_item->engine]);
    list_add_tail(&job_item->minor_list, &favce_minor_head[idx]);
    spin_unlock_irqrestore(&favc_enc_lock, flags);
    //DEBUG_M(LOG_INFO, "{chn%d} put job %d to engine %d\n", job_item->chn, job->id, job_item->engine);
    //mcp280_debug("{chn%d} put job %d to engine %d\n", job_item->chn, job->id, job_item->engine);    
    // Tuba 20130703: check engine idle @ work_scheduler
    work_scheduler(job_item->engine);

    return JOB_STATUS_ONGOING;
}

static int release_all_buffer(struct favce_data_t *enc_data)
{
    int i;
    for (i = 0; i < MAX_RECON_BUF_NUM; i++)
        release_used_recon(&enc_data->ref_info[i], enc_data->chn);
    DEBUG_M(LOG_DEBUG, "{chn%d} release all reference\n", enc_data->chn);
    return 0;
}

#ifdef SUPPORT_VG2
static int favce_stop(unsigned int fd)
#else
static int favce_stop(void *parm, int engine, int minor)
#endif
{
    struct job_item_t *job_item;
#ifdef SUPPORT_VG2
	int minor = ENTITY_FD_MINOR(fd);
#else
    int idx = minor;
#endif
    struct favce_data_t *enc_data = private_data + minor;
    int job_ongoing = 0;
    unsigned long flags;

    spin_lock_irqsave(&favc_enc_lock, flags);
    list_for_each_entry (job_item, &favce_minor_head[minor], minor_list) {
        if (job_item->status == DRIVER_STATUS_STANDBY || job_item->status == DRIVER_STATUS_BUFFERED) {
            trigger_callback_fail((void *)job_item, FUN_STOP);
            DEBUG_M(LOG_DEBUG, "{chn%d} stop job, job %d is fail\n", minor, job_item->job_id);
        }
        else if (job_item->status == DRIVER_STATUS_KEEP) {
            trigger_callback_finish((void *)job_item, FUN_STOP);
            enc_data->prev_job = NULL;
            DEBUG_M(LOG_DEBUG, "{chn%d} job %d is reserved\n", minor, job_item->job_id);
        }
        else if (job_item->status == DRIVER_STATUS_ONGOING) {
            job_ongoing = 1;
            DEBUG_M(LOG_DEBUG, "{chn%d} job %d is on-going\n", minor, job_item->job_id);
        }
    }
    enc_data->first_frame = 1;
    //enc_data->idr_interval = -1;
    enc_data->updated = H264E_REINIT;   // Tuba 20140407: force update GOP when stop job
    
    if (!job_ongoing) {
        release_all_buffer(enc_data);
    #ifdef REF_POOL_MANAGEMENT
        deregister_ref_pool(enc_data->chn);
        enc_data->res_pool_type = -1;
    #endif
    }
    else {
        //down(&favc_enc_mutex);
        enc_data->stop_job = 1;
        //up(&favc_enc_mutex);
    }
#ifdef DYNAMIC_ENGINE
    if (enc_data->channel_cost > 0) {
        engine_cost[enc_data->engine] -= enc_data->channel_cost;
        enc_data->channel_cost = 0;
    }
#endif
#ifdef REF_POOL_MANAGEMENT
    enc_data->re_reg_cnt = 0;
    enc_data->over_spec_cnt = 0;
#endif
    spin_unlock_irqrestore(&favc_enc_lock, flags);

#ifdef EVALUATION_BITRATE
    reset_bitrate_evaluation(minor);
#endif

    return 0;
}

#ifdef HANDLE_PANIC_STATUS
// damnit notify function
int favce_panic_handler(int data)
{
    gPanicFlag = 1;
    return 0;
}

int favce_printout_handler(int data)
{
    return 0;
}
#endif

static struct property_map_t *favce_get_propertymap(int id)
{
    int i;
    
    for (i = 0; i < sizeof(property_map)/sizeof(struct property_map_t); i++) {
        if (property_map[i].id == id) {
            return &property_map[i];
        }
    }
    return 0;
}


static int favce_queryid(void *parm, char *str)
{
    int i;
    
    for (i = 0; i < sizeof(property_map)/sizeof(struct property_map_t); i++) {
        if (strcmp(property_map[i].name,str) == 0) {
            return property_map[i].id;
        }
    }
    DEBUG_W(LOG_WARNING, "queryid: Error to find name %s\n", str);
    return -1;
}


static int favce_querystr(void *parm, int id, char *string)
{
    int i;
    for (i = 0; i < sizeof(property_map)/sizeof(struct property_map_t); i++) {
        if (property_map[i].id == id) {
            memcpy(string, property_map[i].name, sizeof(property_map[i].name));
            return 0;
        }
    }
    DEBUG_W(LOG_WARNING, "querystr: Error to find id %d\n", id);
    return -1;
}

static int favce_getproperty(void *parm, int engine, int minor, char *string)
{
    int id;
    unsigned int value = 0;
    struct favce_data_t *enc_data;
    if (engine >= MAX_ENGINE_NUM) {
        DEBUG_E(LOG_ERROR, "Error: over engine number %d\n", MAX_ENGINE_NUM);
        return -1;
    }
    if (minor >= h264e_max_chn) {
        DEBUG_E(LOG_ERROR, "Error: over minor number %d\n", h264e_max_chn);
        return -1;
    }

    enc_data = private_data + minor;
    id = favce_queryid(parm, string);
    switch (id) {
        case ID_SRC_FMT:
            value = enc_data->src_fmt;
            break;
        case ID_SRC_XY:
            value = enc_data->src_xy;
            break;
        case ID_SRC_BG_DIM:
            value = enc_data->src_bg_dim;
            break;
        case ID_SRC_DIM:
            value = enc_data->src_dim;
            break;
        case ID_IDR_INTERVAL:
            value = enc_data->idr_interval;
            break;
        case ID_B_FRM_NUM:
            value = enc_data->b_frm_num;
            break;
        case ID_SRC_FRAME_RATE:
            value = enc_data->src_frame_rate;
            break;
        case ID_FPS_RATIO:
            value = enc_data->fps_ratio;
            break;
        case ID_MOTION_ENABLE:
            value = enc_data->motion_flag;
            break;
        case ID_WATERMARK_PATTERN:
            value = enc_data->watermark_pattern;
            break;
        case ID_SRC_INTERLACE:
            value = enc_data->src_interlace;
            break;
        case ID_DI_RESULT:
            value = enc_data->di_result;
            break;
        case ID_INIT_QUANT:
            value = enc_data->init_quant;
            break;
        default:
            break;
    }
    return value;
}


#ifdef EVALUATION_BITRATE
static int evaluate_bitrate(struct favce_data_t *enc_data)
{
    if (dump_bitrate) {
        int end = video_gettime_us();
        if (0 == rc_start[enc_data->chn]) {
            if (end < 0x70000000)
                rc_start[enc_data->chn] = end;
        }
        if (end > rc_start[enc_data->chn] && end - rc_start[enc_data->chn] > rc_bitrate_period * 1000000) {
            int factor;
            int fbase, fincr;
            if (0 == EM_PARAM_N(enc_data->fps_ratio) || 0 == EM_PARAM_M(enc_data->fps_ratio)) {
                fincr = 1;
                fbase = enc_data->src_frame_rate;
            }
            else {
                fincr = EM_PARAM_N(enc_data->fps_ratio);
                fbase = enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio);
            }
            factor = rc_frame_num[enc_data->chn] * fincr;
            if (factor > 0) {
                int fr = fbase / fincr;
                printk("{chn%d} bitrate = %d Kb, (%d / %d)\n", enc_data->chn, 
                    rc_total_bit[enc_data->chn] / 1000 * fr / rc_frame_num[enc_data->chn], 
					rc_total_bit[enc_data->chn], rc_frame_num[enc_data->chn]);
            }
            rc_start[enc_data->chn] = 0;
            rc_total_bit[enc_data->chn] = 0;
            rc_frame_num[enc_data->chn] = 0;
        }
        rc_total_bit[enc_data->chn] += enc_data->bs_size * 8;
        rc_frame_num[enc_data->chn]++;
    }
    return 0;
}
void reset_bitrate_evaluation(int chn)
{
    rc_start[chn] = 0;
    rc_total_bit[chn] = 0;
    rc_frame_num[chn] = 0;
}
#endif

#ifdef OVERFLOW_REENCODE
int update_overflow_qp(struct favce_data_t *enc_data, int mb_x, int mb_y, int encoded_field_done)
{
    int mb_w, mb_h, enc_mb_cnt, total_mb_cnt;
    int over_factor;
    unsigned int qp = 0, fact = g_bitrate_factor;
    mb_w = enc_data->frame_width / 16;
    mb_h = enc_data->frame_height / 16;
    total_mb_cnt = mb_w * mb_h;
    if (encoded_field_done)
        enc_mb_cnt = mb_y * mb_w + mb_x + total_mb_cnt/2;
    else
        enc_mb_cnt = mb_y * mb_w + mb_x;
    over_factor = (enc_mb_cnt << 8) / total_mb_cnt;
    // compress rate: qp 0.9 ~ 0.95
    for (qp = 1; qp < 51; qp++) {
        if (fact < over_factor)
            break;
        fact = (fact * g_bitrate_factor) >> 8;
    }
    if (rc_dev) {
        //struct rc_init_param_t rc_param;
        //enc_data->init_quant = enc_data->init_quant + qp;
        //rc_param.ip_offset = gIPOffset;
        //rc_param.pb_offset = gPBOffset;
        //rc_param.qp_constant = enc_data->init_quant + qp;
        rc_dev->rc_reset_param(enc_data->rc_handler, qp);
        enc_data->force_qp = enc_data->cur_qp + qp;
        DEBUG_M(LOG_DEBUG, "force qp = %d (%d + %d)\n", enc_data->force_qp, enc_data->cur_qp, qp);
    }
    else {
        // update fixed qp
        int fixed_qp[3];
        fixed_qp[I_SLICE] = iClip3(1, 51, enc_data->init_quant + qp - enc_data->ip_offset);
        fixed_qp[P_SLICE] = iClip3(1, 51, enc_data->init_quant + qp);
        fixed_qp[B_SLICE] = iClip3(1, 51, enc_data->init_quant + qp + gPBOffset);
        encode_update_fixed_qp(enc_data->handler, fixed_qp[I_SLICE], fixed_qp[P_SLICE], fixed_qp[B_SLICE]);
    }
    return 0;
}
#endif
#ifdef DYNAMIC_GOP_ENABLE
static int favce_check_motion(struct favce_data_t *enc_data)
{
    int i, motion = 0;
    if (enc_data->roi_qp_enable) {
        for (i = 0; i < ROI_REGION_NUM; i++) {
            if (enc_data->roi_qp_region[i] & 0xFFFF) {
                motion = 1;
                break;
            }
        }
    }
    return motion;
}
#endif

static int favce_set_out_data(struct favce_data_t *enc_data, FAVC_ENC_IOCTL_FRAME *pFrame, int err_type)
{
    struct job_item_t *job_item = (struct job_item_t *)enc_data->cur_job;

    if (NULL == pFrame) {
    #ifdef OVERFLOW_RETURN_VALID_VALUE
        if (H264E_ERR_BS_OVERFLOW ==err_type)
            enc_data->bs_size = iMin(gOverflowReturnLength, enc_data->out_size);
    #else
        enc_data->bs_offset = enc_data->bs_size = 0;
    #endif
    #ifdef HANDLE_BS_CACHEABLE2
        job_item->bs_length = 0;
    #endif
    #ifdef ENABLE_FAST_FORWARD
        enc_data->ref_frame = 0;
    #endif
    #ifdef ENABLE_CHECKSUM
        job_item->checksum_type = 0;
    #endif
    }
    else {
        if (enc_data->motion_flag)
            enc_data->bs_offset = enc_data->mb_info_size;
            //enc_data->bs_offset = enc_data->buf_width * enc_data->buf_height / 32;
        else
            enc_data->bs_offset = 0;
        enc_data->bs_size = pFrame->apFrame.u32ResBSLength;
        enc_data->slice_type = pFrame->apFrame.u8FrameSliceType;
    #ifdef HANDLE_BS_CACHEABLE2
        job_item->bs_length = enc_data->bs_offset + enc_data->bs_size;
    #endif
    #ifdef ENABLE_FAST_FORWARD
        enc_data->ref_frame = pFrame->apFrame.bRefFrame;
    #endif
    #ifdef ENABLE_CHECKSUM
        if (0x100 == (enc_data->checksum_type&0xF00) || 
            (0x200 == (enc_data->checksum_type&0xF00) && (I_SLICE == enc_data->slice_type))) {
            job_item->checksum_type = enc_data->checksum_type & 0x0F;
            if (job_item->checksum_type < 1 || job_item->checksum_type > 3) {
                enc_data->checksum_type = job_item->checksum_type = 0;
            }
        }
        else {
            enc_data->checksum_type = job_item->checksum_type = 0;
        }
    #endif
    #ifdef OUTPUT_SLICE_OFFSET
        if (h264e_slice_offset) {
            job_item->slice_num = iMin(pFrame->apFrame.u32SliceNumber, MAX_OUTPUT_SLICE_NUM);
            memcpy(job_item->slice_offset, pFrame->apFrame.u32SliceOffset, sizeof(unsigned int)*pFrame->apFrame.u32SliceNumber);
        }
    #endif 
    }

    favce_set_out_property(enc_data, job_item->job);
    favce_postprocess(job_item, 0); // post process must after set out property
    return 0;
}

static int favce_trigger_next_field(struct favce_data_t *enc_data)
{
    int ret;
    FAVC_ENC_IOCTL_FRAME tFrame;
    tFrame.new_frame = 0;
    DEBUG_M(LOG_INFO, "favce_trigger_next_field\n");
    ret = encode_one_frame_trigger(enc_data->handler, &tFrame, 0, 0);
    if (ret < 0) {
        DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger next field frame error\n", enc_data->chn);
    #ifdef SAME_REC_REF_BUFFER
        if (0 == h264e_one_ref_buf)
    #endif
        release_used_recon(tFrame.mReconBuffer, enc_data->chn);
        return -1;
    }
    //DEBUG_M(LOG_INFO, "encoder trigger one slice\n");
    if (encode_one_slice_trigger(enc_data->handler) < 0) {
        DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger one slice error\n", enc_data->chn);
        return -1;
    }
    return 0;
}

static void favce_trigger_fail(struct favce_data_t *enc_data, int err_type)
{
	// need assign out property when encode error
#ifdef SAME_REC_REF_BUFFER
    if (0 == h264e_one_ref_buf)
#endif
	release_rec_buffer(enc_data->ref_info, enc_data->cur_rec_buf_idx, enc_data->cur_rec_buf_idx, enc_data->chn);
    favce_set_out_data(enc_data, NULL, err_type); // post process @ favce_set_out_data 
    //favce_postprocess(enc_data->cur_job, 0);
    trigger_callback_fail(enc_data->cur_job, FUN_TRI_FAIL);
    set_engine_idle(enc_data->engine);
    //cur_work_engine = enc_data->engine;
    work_scheduler(enc_data->engine);
}

static int favce_process_done(struct favce_data_t *enc_data)
{
    FAVC_ENC_IOCTL_FRAME tFrame;
    int ret = 0, ret2 = 0;

    //DEBUG_M(LOG_INFO, "process done\n");
    ret = encode_one_slice_sync(enc_data->handler);
    if (ret < 0) {
    #ifndef OVERFLOW_REENCODE
        DEBUG_E(LOG_ERROR, "{chn%d} encoder error %d\n", enc_data->chn, ret);
        encoder_abandon_rest(enc_data->handler);
        favce_trigger_fail(enc_data, ret);
        return ret;
    #else
        if (H264E_ERR_BS_OVERFLOW == ret && enc_data->cur_qp < enc_data->max_quant) {
            int mb_x, mb_y, encoded_field_done = 0;
            struct job_item_t *job_item = (struct job_item_t *)enc_data->cur_job;
        #ifdef AVOID_TDN_REENCODE
            if (enc_data->real_didn_mode&TEMPORAL_DENOISE) {
                DEBUG_E(LOG_ERROR, "{chn%d} when temporal denoise is enable, encoder can not re-encoder\n", enc_data->chn);
                //encode_get_current_xy(enc_data->handler, &mb_x, &mb_y, &encoded_field_done);
                //update_overflow_qp(enc_data, mb_x, mb_y, encoded_field_done);
                // update qp to avoid overflow again                
                encoder_abandon_rest(enc_data->handler);
                favce_trigger_fail(enc_data, H264E_ERR_BS_OVERFLOW);
                return ret;
            }
        #endif
        #ifdef SAME_REC_REF_BUFFER
            if (h264e_one_ref_buf) {
                // reset gop
            #ifdef ENABLE_FAST_FORWARD
                encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num, enc_data->fast_forward);
            #else
                encoder_reset_gop_param(enc_data->handler, enc_data->idr_interval, enc_data->b_frm_num);
            #endif
            }
        #endif
            encode_get_current_xy(enc_data->handler, &mb_x, &mb_y, &encoded_field_done);
            DEBUG_M(LOG_WARNING, "{chn%d} bitstream overflow, cur xy = %d, %d (field %d)\n", enc_data->chn, mb_x, mb_y, encoded_field_done);
            DEBUG_M(LOG_WARNING, "{chn%d} bitstream overflow, res %d x %d, bs buffer size %d (qp %d)\n", 
                enc_data->chn, enc_data->frame_width, enc_data->frame_height, enc_data->out_size, enc_data->cur_qp);
        #ifdef SAME_REC_REF_BUFFER
            if (0 == h264e_one_ref_buf)
        #endif
            release_rec_buffer(enc_data->ref_info, enc_data->cur_rec_buf_idx, enc_data->cur_rec_buf_idx, enc_data->chn);
            encoder_abandon_rest(enc_data->handler);
            // start job but not allocate buffer
            update_overflow_qp(enc_data, mb_x, mb_y, encoded_field_done);
            favce_start_job(job_item, job_item->is_b_frame);
            #ifdef EVALUATION_BITRATE
            re_encode_cnt++;
            #endif
        }
        else {
            DEBUG_E(LOG_ERROR, "{chn%d} encoder error %d (qp = %d, max qp %d)\n", enc_data->chn, ret, enc_data->cur_qp, enc_data->max_quant);
            encoder_abandon_rest(enc_data->handler);
            favce_trigger_fail(enc_data, ret);
            return ret;
        }
    #endif
        return ret;
    }
    else if (SLICE_DONE == ret) {    // trigger next slice next 
        DEBUG_M(LOG_INFO, "trigger next slice\n");
        if (encode_one_slice_trigger(enc_data->handler) < 0) {
            DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger next slice fail\n", enc_data->chn);
            favce_trigger_fail(enc_data, H264E_ERR_TRIGGER_SLICE);
            return -1;
        }
        mark_engine_start(enc_data->engine);
    }
    else if (PLANE_DONE == ret) {
        ret2 = encode_one_frame_sync(enc_data->handler, &tFrame);
        //DEBUG_M(LOG_INFO, "frame sync ret = %d\n", ret2);
        if (ret2 < 0) {
            DEBUG_E(LOG_ERROR, "{chn%d} encoder sync fail, %d\n", enc_data->chn, ret2);
            favce_trigger_fail(enc_data, ret2);
            return ret2;
        }
        else if (FIELD_DONE == ret2) {  // trigger next field
            DEBUG_M(LOG_INFO, "trigger next field\n");
            if (favce_trigger_next_field(enc_data) < 0) {
                DEBUG_E(LOG_ERROR, "{chn%d} encoder trigger next field fail\n", enc_data->chn);
                favce_trigger_fail(enc_data, H264E_ERR_TRIGGER_SLICE);
                return -1;
            }
            mark_engine_start(enc_data->engine);
        }
        else if (FRAME_DONE == ret2) {  // trigger next job
        #ifdef RC_ENABLE
            if (rc_dev && enc_data->rc_handler)
                encoder_rc_update(enc_data->handler, rc_dev->rc_update, enc_data->rc_handler);
        #endif
        #ifdef DYNAMIC_GOP_ENABLE
            if (1 == h264e_dynamic_gop) {
                int motion = favce_check_motion(enc_data);                
                if (motion) {
                    enc_data->static_event_cnt = 0;
                    if (2 == enc_data->motion_state) {
                        enc_data->motion_state = 3;
                        DEBUG_M(LOG_DEBUG, "[%d] static -> motion detect\n", enc_data->motion_state);
                    }
                    else if (1 == enc_data->motion_state) {
                        enc_data->motion_state = 0;
                        DEBUG_M(LOG_DEBUG, "[%d] motion detect\n", enc_data->motion_state);
                    }
                }
                else {
                    if (I_SLICE == tFrame.apFrame.u8FrameSliceType) {
                        //DEBUG_M(LOG_DEBUG, "[%d] I slice motion = %d, cnt = %d\n", 
                        //    enc_data->motion_state, motion, enc_data->static_event_cnt);
                        if (!motion)
                            enc_data->static_event_cnt++;
                        if (enc_data->static_event_cnt > measure_static_event && 0 == enc_data->motion_state) {
                            enc_data->motion_state = 1;    // motion -> static
                            DEBUG_M(LOG_DEBUG, "static detect (%d)\n", enc_data->static_event_cnt);
                        }
                    }
                }
            }
        #endif
            // release refernce frame
            
        #ifdef SAME_REC_REF_BUFFER
            #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
            if (h264e_one_ref_buf) {
                // Tuba 20150326: check not swap reference buffer
                if (enc_data->small_buf_idx >= 0 && NO_RELEASE_BUFFER != tFrame.u8ReleaseIdx
                    && tFrame.u8ReleaseIdx != enc_data->cur_rec_buf_idx)
                    enc_data->small_buf_idx ^= 0x01;
            }
            else
                release_rec_buffer(enc_data->ref_info, tFrame.u8ReleaseIdx, enc_data->cur_rec_buf_idx, enc_data->chn);
			#else
			if (0 == h264e_one_ref_buf)
                release_rec_buffer(enc_data->ref_info, tFrame.u8ReleaseIdx, enc_data->cur_rec_buf_idx, enc_data->chn);
            #endif
        #else
            release_rec_buffer(enc_data->ref_info, tFrame.u8ReleaseIdx, enc_data->cur_rec_buf_idx, enc_data->chn);
        #endif
            favce_set_out_data(enc_data, &tFrame, 0);
        #ifdef EVALUATION_BITRATE
            evaluate_bitrate(enc_data);
        #endif
        #ifdef RECORD_BS
            dump_bitstream((void *)(enc_data->out_addr_va + enc_data->bs_offset), enc_data->bs_size, enc_data->chn);
        #endif
            //if ((enc_data->didn_mode&TEMPORAL_DEINTERLACE) && !(enc_data->didn_mode&TEMPORAL_DENOISE)) {
            //if ((enc_data->didn_mode&TEMPORAL_DEINTERLACE) || (enc_data->didn_mode&TEMPORAL_DENOISE)) {
            if ((enc_data->real_didn_mode&TEMPORAL_DEINTERLACE) || (enc_data->real_didn_mode&TEMPORAL_DENOISE)) {
                struct job_item_t *job_item;
                if (enc_data->prev_job) {
                    job_item = (struct job_item_t *)enc_data->prev_job;
                    DEBUG_M(LOG_INFO, "{chn%d} callback previous job %d\n", enc_data->chn, job_item->job_id);
                    trigger_callback_finish(enc_data->prev_job, FUN_DONE_PREV);
                }
                if (enc_data->stop_job) {
                    trigger_callback_finish(enc_data->cur_job, FUN_DONE_STOP);
                    enc_data->prev_job = NULL;
                }
                else {
                    job_item = (struct job_item_t *)enc_data->cur_job;
                    job_item->status = DRIVER_STATUS_KEEP;
                    enc_data->prev_job = enc_data->cur_job;
                }
                /*
                struct video_entity_job_t *job = ((struct job_item_t*)enc_data->cur_job)->job;
                if (enc_data->reserved_buf.buf[0].addr_va) {
                    DEBUG_M(LOG_INFO, "{chn%d} release buffer 0x%x\n", enc_data->chn, enc_data->reserved_buf.buf[0].addr_pa);
                    video_free_buffer(enc_data->reserved_buf);
                }
                memcpy(&enc_data->reserved_buf, &job->in_buf, sizeof(enc_data->reserved_buf));
                video_reserve_buffer(enc_data->cur_job, 0);
                DEBUG_M(LOG_INFO, "{chn%d} reserved buffer 0x%x\n", enc_data->chn, enc_data->reserved_buf.buf[0].addr_pa);
                */
            }
            else
                trigger_callback_finish(enc_data->cur_job, FUN_DONE_CUR);
            set_engine_idle(enc_data->engine);
            // Tuba 20130820: mark current channel -1
            #ifdef TWO_ENGINE
                favce_cur_chn[enc_data->engine] = -1;
            #else
                favce_cur_chn = -1;
            #endif
            work_scheduler(enc_data->engine);
        }                        
    }        
    return 0;
}

static irqreturn_t favce_int_process(int irq, unsigned int * base, int engine)
{
    struct favce_data_t *enc_data;
    unsigned long flags;
    // receive interrupt
    spin_lock_irqsave(&favc_enc_lock, flags);
    mark_engine_finish(engine);
	//DEBUG_M(LOG_MIDDLE, "{chn%d} job done\n", favce_cur_chn[engine]);
    DEBUG_M(LOG_INFO, "{engine%d} h264e isr\n", engine);
#ifdef TWO_ENGINE
    enc_data = private_data + favce_cur_chn[engine];
#else
    enc_data = private_data + favce_cur_chn;
#endif

    if (NULL == enc_data->handler) {
        favce_err("{chn%d} engine%d isr, but not initialized\n", enc_data->chn, engine);
        spin_unlock_irqrestore(&favc_enc_lock, flags);
        damnit(MODULE_NAME);
        return IRQ_HANDLED;
    }
    encoder_receive_irq(enc_data->handler);
    favce_process_done(enc_data);
    //favce_set_out_data(enc_data->cur_job, 0, 0);
    if (enc_data->stop_job) {
        DEBUG_M(LOG_INFO, "{chn%d} process done & release all buffer\n", enc_data->chn);
        release_all_buffer(enc_data);
        enc_data->stop_job = 0;
    #ifdef REF_POOL_MANAGEMENT
        deregister_ref_pool(enc_data->chn);
        enc_data->res_pool_type = -1;
    #endif
    }
    spin_unlock_irqrestore(&favc_enc_lock, flags);
    return IRQ_HANDLED;
}


static irqreturn_t favce_int_handler(int irq, void *dev)
{
	return favce_int_process(irq, (unsigned int *)dev, 0);
}

#ifdef TWO_ENGINE
static irqreturn_t favce_int_handler_2(int irq, void *dev)
{
//#if defiend(FIX_TWO_ENGINE)|defined(DYNAMICENGINE)
#ifdef TWO_ENGINE
    return favce_int_process(irq, (unsigned int *)dev, 1);
#else
    return favce_int_process(irq, (unsigned int *)dev, 0);
#endif
}
#endif

int dump_property_value(char *str, unsigned int chn)
{
    int len = 0;
    int i = 0;
    struct property_map_t *map;
    unsigned int id, value;
    //unsigned int address, size;
    int idx = chn;

    len += sprintf(str+len, "FAVCE %s ch%d job %d\n", 
        property_record[idx].entity->name, chn, property_record[idx].job_id);
    len += sprintf(str+len, "=============================================================\n");
    len += sprintf(str+len, " ID      Name(string) Value(hex)  Readme\n");
    do {
        id = property_record[idx].property[i].id;
        if (id == ID_NULL)
            break;
        value = property_record[idx].property[i].value;
        map = favce_get_propertymap(id);
        if (map) {
            len += sprintf(str+len, "%3d  %16s  %08x  %s\n", id, map->name, value, map->readme);
        }
        i++;
    } while (1);

    return len;
}

//value=999 means all
int dump_job_info(char *str, unsigned int chn)
{
    int len = 0;
    int engine;
    struct job_item_t *job_item;
    char *st_string[] = {"STANDBY", "ONGOING", " FINISH", "   FAIL", "   KEEP", " BUFFER"};
    int idx = chn;
    unsigned long flags;

    spin_lock_irqsave(&favc_enc_lock, flags);
    if (999 == chn) {
        len += sprintf(str + len, "Engine Minor  Job_ID   Status   Puttime\n");
        len += sprintf(str + len, "=======================================\n");
        for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
            list_for_each_entry(job_item, &favce_engine_head[engine], engine_list) {
                len += sprintf(str + len, "%d      %02d     %7d  %s   0x%04x\n", 
                    job_item->engine, job_item->chn, 
                    job_item->job_id, st_string[job_item->status - 1], job_item->puttime & 0xffff);
            }
        }
    } 
    else if (chn < h264e_max_chn) {
        if (list_empty(&favce_minor_head[idx]) == 0) {
            len += sprintf(str + len, "Engine Minor  Job_ID   Status   Puttime\n");
            len += sprintf(str + len, "=======================================\n");
        }
        list_for_each_entry(job_item, &favce_minor_head[idx], minor_list) {
            len += sprintf(str + len, "%d      %02d     %04d     %s   0x%04x\n", 
                job_item->engine, job_item->chn, 
                job_item->job_id, st_string[job_item->status - 1], job_item->puttime & 0xffff);
        }
    }
    spin_unlock_irqrestore(&favc_enc_lock, flags);

    return len;
}

struct video_entity_ops_t favce_ops ={
    putjob:      &favce_putjob,
    stop:        &favce_stop,
    queryid:     &favce_queryid,
    querystr:    &favce_querystr,
    getproperty: &favce_getproperty,
    //register_clock: &driver_register_clock,
};    


struct video_entity_t favce_entity={
    type:       TYPE_H264_ENC,
    name:       "favce",
    engines:    1,
    minors:     DEFAULT_MAX_CHN_NUM,
    ops:        &favce_ops
};


int favce_msg(char *str)
{
    int len = 0;
    if (NULL == str) {
        printk("FAVC Encoder v%d.%d.%d", H264E_VER_MAJOR, H264E_VER_MINOR, H264E_VER_MINOR2);
        if (H264E_VER_BRANCH)
            printk(".%d", H264E_VER_BRANCH);
        printk(", built @ %s %s (%s)\n", __DATE__, __TIME__, PLATFORM_NAME);
    }
    else {
        len += sprintf(str+len, "FAVC Encoder v%d.%d.%d", H264E_VER_MAJOR, H264E_VER_MINOR, H264E_VER_MINOR2);
        if (H264E_VER_BRANCH)
            len += sprintf(str+len, ".%d", H264E_VER_BRANCH);
        len += sprintf(str+len, ", built @ %s %s (%s)\n", __DATE__, __TIME__, PLATFORM_NAME);
    #if 0
        len += sprintf(str+len, "max resolution = %d x %d, max chn = %d", h264e_max_width, h264e_max_height, h264e_max_chn);
        if (h264e_snapshot_chn)
            len += sprintf(str+len, ", snapshot chn = %d", h264e_snapshot_chn);
        if (h264e_yuv_swap)
            len += sprintf(str+len, ", input source YCbYCr");
        else
            len += sprintf(str+len, ", input source CbYCrY");

        if (use_ioremap_wc)
            len += sprintf(str+len, ", reg ncb");
        else
            len += sprintf(str+len, ", reg ncnb");
        len += sprintf(str+len, ", config path = \"%s\"", config_path);
        #ifdef GM8139
            if (pwm & 0xFFFF)
                len += sprintf(str+len, ", pwm = 0x%x", pwm);
        #endif
        len += sprintf(str+len, "\n");
    #else
        len += sprintf(str+len, "module parameter\n");
        len += sprintf(str+len, "====================   ======\n");
        len += sprintf(str+len, "h264e_max_width        %d\n", h264e_max_width);
        len += sprintf(str+len, "h264e_max_height       %d\n", h264e_max_height);
        len += sprintf(str+len, "h264e_max_b_frame      %d\n", h264e_max_b_frame);
        #ifdef RECORD_BS
        len += sprintf(str+len, "h264e_rec_bs_size      %d\n", h264e_rec_bs_size);
        #endif
        len += sprintf(str+len, "h264e_max_chn          %d\n", h264e_max_chn);
        len += sprintf(str+len, "h264e_snapshot_chn     %d\n", h264e_snapshot_chn);
        len += sprintf(str+len, "h264e_yuv_swap         %d\t(0: CbYCrY, 1: YCbYCr)\n", h264e_yuv_swap);
        #ifdef SUPPORT_PWM
        len += sprintf(str+len, "pwm                    0x%04x\n", pwm&0xFFFF);
        #endif
        len += sprintf(str+len, "config_path            \"%s\"\n", config_path);
        len += sprintf(str+len, "use_ioremap_wc         %d\t(0: ref ncnb, 1: ncb)\n", use_ioremap_wc);
        #ifdef DYNAMIC_ENGINE
        len += sprintf(str+len, "h264e_dynamic_engine   %d\n", h264e_dynamic_engine);
        #endif
        #ifdef OUTPUT_SLICE_OFFSET
        len += sprintf(str+len, "h264e_slice_offset     %d\n", h264e_slice_offset);
        #endif
        #ifdef SAME_REC_REF_BUFFER
        len += sprintf(str+len, "h264e_one_ref_buf      %d\n", h264e_one_ref_buf);
        #endif
        #ifdef TIGHT_BUFFER_ALLOCATION
        len += sprintf(str+len, "h264e_tight_buf        %d\n", h264e_tight_buf);
        #endif
        #ifdef USE_CONFIG_FILE
        len += sprintf(str+len, "h264e_user_config      %d\n", h264e_user_config);
        #endif
        #ifdef DISABLE_WRITE_OUT_REC
        len += sprintf(str+len, "h264e_dis_wo_buf       %d\n", h264e_dis_wo_buf);
        #endif
        #ifdef DYNAMIC_GOP_ENABLE
        len += sprintf(str+len, "h264e_dynamic_gop      %d\n", h264e_dynamic_gop);
        #endif
    #endif
    }
    return len;
}

static int init_assign_channel_to_engine(void)
{
#ifdef TWO_ENGINE
    int chn;

    chn_2_engine = kzalloc(sizeof(int) * h264e_max_chn, GFP_KERNEL);
    if (NULL == chn_2_engine) {
        favce_err("Fail to allocate chn_2_engine!\n");
        return -1;
    }

    for (chn = 0; chn < h264e_max_chn; chn++) {
    #ifdef FIX_TWO_ENGINE
        if (chn & 0x02)
            chn_2_engine[chn] = 0x01;
        else
            chn_2_engine[chn] = 0x00;
    #elif defined(DYNAMIC_ENGINE)
    /*
        if (h264e_dynamic_engine)
            chn_2_engine[chn] = -1;
        else {
        */
            if (chn & 0x02)
                chn_2_engine[chn] = 0x01;
            else
                chn_2_engine[chn] = 0x00;
        //}
    #endif
    }
#endif
    return 0;
}

static int favce_init_base_address(void)
{
    int engine;
#ifndef TWO_ENGINE
    /*
    #ifdef REGISTER_NCB
        favce_base_address_va = (unsigned long)ioremap_wc(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    #else
        favce_base_address_va = (unsigned long)ioremap_nocache(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    #endif
    */
    if (use_ioremap_wc)
        favce_base_address_va = (unsigned long)ioremap_wc(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    else
        favce_base_address_va = (unsigned long)ioremap_nocache(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    if (unlikely(!favce_base_address_va))
        panic("[%s] ftmcp280 base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_0_BASE_ADDRESS, FTMCP280_0_BASE_SIZE);
    /*
    #ifdef REGISTER_NCB
        favce_vcache_address_va = (unsigned long)ioremap_wc(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    #else
        favce_vcache_address_va = (unsigned long)ioremap_nocache(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    #endif
    */
    if (use_ioremap_wc)
        favce_vcache_address_va = (unsigned long)ioremap_wc(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    else
        favce_vcache_address_va = (unsigned long)ioremap_nocache(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    if (unlikely(!favce_vcache_address_va))
        panic("[%s] ftmcp280 vcache base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_0_VCACHE_ADDRESS, 0x1000);
    irq_no = FTMCP280_0_IRQ;
    engine = 0;
    //if (request_irq(irq_no, &favce_int_handler, IRQF_DISABLED, "ftmcp280-0", (void *)engine) != 0) {    
    if (request_irq(irq_no, &favce_int_handler, 0, "ftmcp280-0", (void *)engine) != 0) {
        DEBUG_E(LOG_ERROR, "unable request IRQ %d\n", irq_no);
        irq_no = -1;
    }
    #ifdef NO_RESET_REGISTER_PARAM_UPDATE
		encoder_reset_register(favce_base_address_va, favce_vcache_address_va);
    #endif
#else   // TWO_ENGINE
    /*
    #ifndef REGISTER_NCB
        favce_base_address_va[0] = (unsigned long)ioremap_nocache(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    #else
        favce_base_address_va[0] = (unsigned long)ioremap_wc(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    #endif
    */
    if (use_ioremap_wc)
        favce_base_address_va[0] = (unsigned long)ioremap_wc(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    else
        favce_base_address_va[0] = (unsigned long)ioremap_nocache(FTMCP280_0_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_0_BASE_SIZE));
    if (unlikely(!favce_base_address_va[0]))
        panic("[%s] ftmcp280_0 base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_0_BASE_ADDRESS, FTMCP280_0_BASE_SIZE);
    /*
    #ifndef REGISTER_NCB
        favce_vcache_address_va[0] = (unsigned long)ioremap_nocache(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    #else
        favce_vcache_address_va[0] = (unsigned long)ioremap_wc(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    #endif
    */
    if (use_ioremap_wc)
        favce_vcache_address_va[0] = (unsigned long)ioremap_wc(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    else
        favce_vcache_address_va[0] = (unsigned long)ioremap_nocache(FTMCP280_0_VCACHE_ADDRESS, PAGE_ALIGN(0x1000));
    if (unlikely(!favce_vcache_address_va[0]))
        panic("[%s] ftmcp280_0 vcache base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_0_VCACHE_ADDRESS, 0x1000);
    /*
    #ifndef REGISTER_NCB
        favce_base_address_va[1] = (unsigned long)ioremap_nocache(FTMCP280_1_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_1_BASE_SIZE));
    #else
        favce_base_address_va[1] = (unsigned long)ioremap_wc(FTMCP280_1_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_1_BASE_SIZE));
    #endif
    */
    if (use_ioremap_wc)
        favce_base_address_va[1] = (unsigned long)ioremap_wc(FTMCP280_1_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_1_BASE_SIZE));
    else
        favce_base_address_va[1] = (unsigned long)ioremap_nocache(FTMCP280_1_BASE_ADDRESS, PAGE_ALIGN(FTMCP280_1_BASE_SIZE));
    if (unlikely(!favce_base_address_va[1]))
        panic("[%s] ftmcp280_1 base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_1_BASE_ADDRESS, FTMCP280_1_BASE_SIZE);
    /*
    #ifndef REGISTER_NCB
        favce_vcache_address_va[1] = (unsigned long)ioremap_nocache(FTMCP280_1_VCACHE_ADDRESS, PAGE_ALIGN(0x100000));
    #else
        favce_vcache_address_va[1] = (unsigned long)ioremap_wc(FTMCP280_1_VCACHE_ADDRESS, PAGE_ALIGN(0x100000));
    #endif
    */
    if (use_ioremap_wc)
        favce_vcache_address_va[1] = (unsigned long)ioremap_wc(FTMCP280_1_VCACHE_ADDRESS, PAGE_ALIGN(0x100000));
    else
        favce_vcache_address_va[1] = (unsigned long)ioremap_nocache(FTMCP280_1_VCACHE_ADDRESS, PAGE_ALIGN(0x100000));
    if (unlikely(!favce_vcache_address_va[1]))
        panic("[%s] ftmcp280_1 vcache base address no virtual address! (phy 0x%x, size %d)\n", __func__, FTMCP280_1_VCACHE_ADDRESS, 0x1000);
    //mcp280_debug("engine0 0x%x 0x%x\n", (uint32_t)favce_base_address_va[0], (uint32_t)favce_vcache_address_va[0]);
    //mcp280_debug("engine1 0x%x 0x%x\n", (uint32_t)favce_base_address_va[1], (uint32_t)favce_vcache_address_va[1]);
    irq_no[0] = FTMCP280_0_IRQ;
    irq_no[1] = FTMCP280_1_IRQ;
    engine = 0;
    //if (request_irq(irq_no[0], &favce_int_handler, IRQF_DISABLED, "ftmcp280-0", (void *)engine) != 0) {    
    if (request_irq(irq_no[0], &favce_int_handler, 0, "ftmcp280-0", (void *)engine) != 0) {
        DEBUG_E(LOG_ERROR, "unable request IRQ %d\n", irq_no[0]);
        irq_no[0] = -1;
    }
    engine = 1;
    //if (request_irq(irq_no[1], &favce_int_handler_2, IRQF_DISABLED, "ftmcp280-1", (void *)engine) != 0) {
    if (request_irq(irq_no[1], &favce_int_handler_2, 0, "ftmcp280-1", (void *)engine) != 0) {
        DEBUG_E(LOG_ERROR, "unable request IRQ %d\n", irq_no[1]);
        irq_no[1] = -1;
    }
	#ifdef NO_RESET_REGISTER_PARAM_UPDATE
		encoder_reset_register(favce_base_address_va[0], favce_vcache_address_va[0]);
		encoder_reset_register(favce_base_address_va[1], favce_vcache_address_va[1]);
	#endif
#endif  // TWO_ENGINE

    return 0;
}

static void favce_release_base_address(void)
{
    int engine;
#ifdef TWO_ENGINE
    engine = 0;
    if (irq_no[0] >= 0)
        free_irq(irq_no[0], (void *)engine);
    engine = 1;
    if (irq_no[1] >= 0)
        free_irq(irq_no[1], (void *)engine);
    if (favce_base_address_va[0])
        iounmap((void __iomem *)favce_base_address_va[0]);
    if (favce_vcache_address_va[0])
        iounmap((void __iomem *)favce_vcache_address_va[0]);
    if (favce_base_address_va[1])
        iounmap((void __iomem *)favce_base_address_va[1]);
    if (favce_vcache_address_va[1])
        iounmap((void __iomem *)favce_vcache_address_va[1]);
#else
    engine = 0;
    if (irq_no >= 0)
        free_irq(irq_no, (void *)engine);
    if (favce_base_address_va)
        iounmap((void __iomem *)favce_base_address_va);
    if (favce_vcache_address_va)
        iounmap((void __iomem *)favce_vcache_address_va);
#endif
}

static void favce_release_local_parameter(void)
{
    int engine;
    if (favce_engine_head)
        kfree(favce_engine_head);
    favce_engine_head = NULL;
    if (favce_minor_head)
        kfree(favce_minor_head);
    favce_minor_head = NULL;
    if (property_record)
        kfree(property_record);
    property_record = NULL;
    if (private_data)
        kfree(private_data);
    private_data = NULL;
    if (procForceUpdate)
        kfree(procForceUpdate);
    procForceUpdate = NULL;
#ifdef REF_POOL_MANAGEMENT
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        if (mem_buf_checker[engine])
            kfree(mem_buf_checker[engine]);
        mem_buf_checker[engine] = NULL;
    }
#endif
}

static int favce_allocate_local_parameter(void)
{
    int engine, chn, i;
    // init job list
    favce_engine_head = kzalloc(sizeof(struct list_head) * MAX_ENGINE_NUM, GFP_KERNEL);
    if (NULL == favce_engine_head) {
        favce_err("Fail to allocate engine_list!\n");
        goto allocate_fail;
    }
    favce_minor_head = kzalloc(sizeof(struct list_head) * h264e_max_chn, GFP_KERNEL);
    if (NULL == favce_minor_head) {
        favce_err("Fail to allocate minor_list!\n");
        goto allocate_fail;
    }
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++)
        INIT_LIST_HEAD(&favce_engine_head[engine]);
    for (chn = 0; chn < h264e_max_chn; chn++)
        INIT_LIST_HEAD(&favce_minor_head[chn]);

    
    // init proc parameter
    property_record = kzalloc(sizeof(struct property_record_t) * h264e_max_chn, GFP_KERNEL);
    if (NULL == property_record) {
        favce_err("Fail to allocate property_record!\n");
        goto allocate_fail;
    }
#ifdef TWO_ENGINE
    memset(engine_time, 0, sizeof(unsigned int) * MAX_ENGINE_NUM);
    memset(engine_start, 0, sizeof(unsigned int) * MAX_ENGINE_NUM);
    memset(engine_end, 0, sizeof(unsigned int) * MAX_ENGINE_NUM);
    memset(utilization_start, 0, sizeof(unsigned int) * MAX_ENGINE_NUM);
    memset(utilization_record, 0, sizeof(unsigned int) * MAX_ENGINE_NUM);
#else
    engine_time = engine_start = engine_end = 0;
    utilization_start = utilization_record = 0;
#endif

    // init parameter
    private_data = kzalloc(sizeof(struct favce_data_t) * h264e_max_chn, GFP_KERNEL);
    if (NULL == private_data) {
        favce_err("Fail to allocate private_data!\n");
        goto allocate_fail;
    }
    memset(private_data, 0, sizeof(struct favce_data_t) * h264e_max_chn);
    procForceUpdate = kzalloc(sizeof(int) * h264e_max_chn, GFP_KERNEL);
    if (NULL == procForceUpdate) {
        favce_err("Fail to allocate force_update!\n");
        goto allocate_fail;
    }
    memset(procForceUpdate, 0, sizeof(int) * h264e_max_chn);
#ifdef TWO_ENGINE
    memset(mcp280_sysinfo_buffer, 0, sizeof(struct buffer_info_t) * MAX_ENGINE_NUM);
    memset(mcp280_mvinfo_buffer, 0, sizeof(struct buffer_info_t) * MAX_ENGINE_NUM);
    for (chn = 0; chn < h264e_max_chn; chn++) {
        private_data[chn].engine = chn_2_engine[chn];
        private_data[chn].chn = chn;
        private_data[chn].first_frame = 1;
        for (i = 0; i < MAX_RECON_BUF_NUM; i++)
            private_data[chn].ref_info[i].rec_buf_idx = i;
    #ifdef REF_POOL_MANAGEMENT
        private_data[chn].res_pool_type = -1;
    #endif
    }
#else
    for (chn = 0; chn < h264e_max_chn; chn++) {
        private_data[chn].engine = 0;
        private_data[chn].chn = chn;
        private_data[chn].first_frame = 1;
        for (i = 0; i < MAX_RECON_BUF_NUM; i++)
            private_data[chn].ref_info[i].rec_buf_idx = i;
    #ifdef REF_POOL_MANAGEMENT
        private_data[chn].res_pool_type = -1;
    #endif
    }
#endif

#ifdef REF_POOL_MANAGEMENT
    memset(mem_buf_checker, 0, sizeof(unsigned char *)*MAX_ENGINE_NUM);
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        mem_buf_checker[engine] = kzalloc(sizeof(unsigned char) * h264e_max_chn, GFP_KERNEL);
        if (NULL == mem_buf_checker[engine]) {
            favce_err("Fail to allocate mem_buf_checker!\n");
            goto allocate_fail;
        }
    }
#endif
#ifdef NEW_JOB_ITEM_ALLOCATE
    for (i = 0; i < MAX_JOB_ARRAY_NUM; i++)
        job_array[i].job_item_id = i;
#endif
    return 0;

allocate_fail:
    favce_release_local_parameter();

    return -1;
}

int favce_vg_init(void)
{
    // gating clock
    pf_mcp280_clk_on();

    // add snapshot channel to max chn
    h264e_max_chn += h264e_snapshot_chn;

    // automatically set maximal resolution @ reference buffer allocation
    // init_mem_pool must be excuted firstly
#ifdef REF_POOL_MANAGEMENT
    if (init_mem_pool() < 0)
        return -EFAULT;
#endif
    // force max w/h be multiple of 16
#ifdef AUTOMATIC_SET_MAX_RES
    if (0 == h264e_max_width)
        h264e_max_width = DEFAULT_MAX_WIDTH;
    if (0 == h264e_max_height)
        h264e_max_height = DEFAULT_MAX_HEIGHT;
#endif
    h264e_max_width = ((h264e_max_width + 15)/16)*16;
    h264e_max_height = ((h264e_max_height + 15)/16)*16;

    // init channel to engine
    if (init_assign_channel_to_engine() < 0)
        return -EFAULT;

    // vg register
    // Tuba 20141229: set maximal channel id to vg
    favce_entity.minors = h264e_max_chn;
    video_entity_register(&favce_entity);

    // init work queue
#ifdef USE_KTHREAD
    init_waitqueue_head(&favce_cb_thread_waitqueue);
    favce_cb_task = kthread_create(favce_cb_thread, NULL, "favce_cb_thread");
    if (!favce_cb_task)
        return -EFAULT;
    wake_up_process(favce_cb_task);
    #if defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)
        #ifndef TWO_ENGINE
        init_waitqueue_head(&favce_job_waitqueue);
        favce_job_task = kthread_create(favce_engine_schedule, NULL, "favce_engine0_thread");
        if (!favce_job_task)
            return -EFAULT;
        wake_up_process(favce_job_task);
        #else
        init_waitqueue_head(&favce_job_waitqueue[0]);
        favce_job_task[0] = kthread_create(favce_engine0_schedule, NULL, "favce_engine0_thread");
        if (!favce_job_task[0])
            return -EFAULT;
        wake_up_process(favce_job_task[0]);
        init_waitqueue_head(&favce_job_waitqueue[1]);
        favce_job_task[1] = kthread_create(favce_engine1_schedule, NULL, "favce_engine1_thread");
        if (!favce_job_task[1])
            return -EFAULT;
        wake_up_process(favce_job_task[1]);
        #endif
    #endif
#else
    favce_cb_wq = create_workqueue("favce_cb");
    if (!favce_cb_wq) {
        DEBUG_E(LOG_ERROR, "%s:Error to create workqueue favce_cb.\n", __FUNCTION__);
        return -EFAULT;
    }
    INIT_DELAYED_WORK(&process_callback_work, 0);
    #ifdef REORDER_JOB
        #ifndef TWO_ENGINE
            favce_reorder_job_wq = create_workqueue("favce_reorder_job");
            if (!favce_reorder_job_wq) {
                DEBUG_E(LOG_ERROR, "%s:Error to create workqueue favce_reoder_job.\n", __FUNCTION__);
                return -EFAULT;
            }
            INIT_DELAYED_WORK(&process_reorder_job_work, 0);
        #else
            favce_reorder_job_wq[0] = create_workqueue("favce_reorder_job_engine0");
            if (!favce_reorder_job_wq[0]) {
                DEBUG_E(LOG_ERROR, "%s:Error to create workqueue favce_reoder_job_engine0.\n", __FUNCTION__);
                return -EFAULT;
            }
            favce_reorder_job_wq[1] = create_workqueue("favce_reorder_job_engine1");
            if (!favce_reorder_job_wq[1]) {
                DEBUG_E(LOG_ERROR, "%s:Error to create workqueue favce_reoder_job_engine1.\n", __FUNCTION__);
                return -EFAULT;
            }
            INIT_DELAYED_WORK(&process_reorder_job_work0, 0);
            INIT_DELAYED_WORK(&process_reorder_job_work1, 0);
        #endif
    #endif
#endif	// USE_KTHREAD

    if (favce_allocate_local_parameter() < 0)
        return -EFAULT;

    // init spin lock
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
    sema_init(&favc_enc_mutex, 1);
#else
    init_MUTEX(&favc_enc_mutex);
#endif
    spin_lock_init(&favc_enc_lock);
#ifdef MEM_CNT_LOG
    spin_lock_init(&mem_cnt_lock);
#endif

#ifdef HANDLE_PANIC_STATUS
    // register damnit notifier & print out notifier
    if (register_panic_notifier(favce_panic_handler) < 0) {
        printk("favce register log system panic notifier failed!\n");
        return -EFAULT;
    }
    if (register_printout_notifier(favce_printout_handler) < 0) {
        printk("favce register log system printout notifier failed!\n");
        return -EFAULT;
    }
#endif

    // init base address
    favce_init_base_address();

    // init proc
    if (favce_proc_init() < 0)
        return -EFAULT;

    // init buffer
    if (favce_alloc_general_buffer(h264e_max_width, h264e_max_height) < 0)
        return -EFAULT;
    
#ifdef RECORD_BS
    if (h264e_rec_bs_size) {
        allocate_frammap_buffer(&mcp280_rec_bs_buffer[0], h264e_rec_bs_size/2);
        allocate_frammap_buffer(&mcp280_rec_bs_buffer[1], h264e_rec_bs_size/2);
        printk("record bitstream, buffer size = %d\n", h264e_rec_bs_size);
    }
#endif
    getMinMaxQuant();
    callback_period_jiffies = msecs_to_jiffies(callback_period);

#ifdef NEW_JOB_ITEM_ALLOCATE
    memset(job_used, 0, sizeof(job_used));
#endif

    favce_msg(NULL);

    return 0;
}


void favce_vg_close(void)
{
    int chn, i;

#ifdef REF_POOL_MANAGEMENT
    clean_mem_pool();
#endif
    video_entity_deregister(&favce_entity);
    //kfree(favce_engine_head);
    //kfree(favce_minor_head);
    //kfree(property_record);
#ifdef TWO_ENGINE
    for (chn = 0; chn < h264e_max_chn; chn++) {
        // close handler
        struct favce_data_t *enc_data;
        enc_data = &private_data[chn];
        // release all reference frame
        for (i = 0; i < MAX_RECON_BUF_NUM; i++)
            release_used_recon(&enc_data->ref_info[i], chn);
        if (enc_data->handler) {
            encoder_release(enc_data->handler);
            enc_data->handler = NULL;
        }
        //if (enc_data->reserved_buf.buf[0].addr_va)
        //    video_free_buffer(enc_data->reserved_buf);
        if (enc_data->prev_job)
            DEBUG_E(LOG_ERROR, "there is one job not be callback\n");
    #ifdef RC_ENABLE
        if (rc_dev)
            rc_dev->rc_clear(enc_data->rc_handler);
    #endif
    }
    if (chn_2_engine)
        kfree(chn_2_engine);
#else
    for (chn = 0; chn < h264e_max_chn; chn++) {
        // close handler
        struct favce_data_t *enc_data;
        enc_data = &private_data[chn];
        // release all reference frame
        for (i = 0; i < MAX_RECON_BUF_NUM; i++)
            release_used_recon(&enc_data->ref_info[i], chn);
        if (enc_data->handler) {
            encoder_release(enc_data->handler);
            enc_data->handler = NULL;
        }
        /*
        if (enc_data->reserved_buf.buf[0].addr_va)
            video_free_buffer(enc_data->reserved_buf);
        */
        if (enc_data->prev_job)
            DEBUG_E(LOG_ERROR, "there is one job not be callback\n");
    #ifdef RC_ENABLE
        if (rc_dev)
            rc_dev->rc_clear(enc_data->rc_handler);
    #endif
    }
#endif
    favce_release_general_buffer();
    //kfree(private_data);
    //kfree(procForceUpdate);
    favce_release_local_parameter();

#ifdef USE_KTHREAD
    if (favce_cb_task) {
        kthread_stop(favce_cb_task);
        /* wait thread to be terminated */
        while (favce_cb_thread_ready) {
            msleep(10);
        }            
    }
    #if defined(REORDER_JOB)|defined(DYNAMIC_ENGINE)
        #ifndef TWO_ENGINE
        if (favce_job_task) {
            kthread_stop(favce_job_task);
            while (favce_job_thread_ready) {
                msleep(10);
            }
        }
        #else
        if (favce_job_task[0]) {
            kthread_stop(favce_job_task[0]);
            while (favce_job_thread_ready[0]) {
                msleep(10);
            }
        }
        if (favce_job_task[1]) {
            kthread_stop(favce_job_task[1]);
            while (favce_job_thread_ready[1]) {
                msleep(10);
            }
        }
        #endif
    #endif
#else
    if (favce_cb_wq)
        destroy_workqueue(favce_cb_wq);
    #ifdef REORDER_JOB
        #ifndef TWO_ENGINE
            if (favce_reorder_job_wq)
                destroy_workqueue(favce_reorder_job_wq);
        #else
            if (favce_reorder_job_wq[0])
                destroy_workqueue(favce_reorder_job_wq[0]);
            if (favce_reorder_job_wq[1])
                destroy_workqueue(favce_reorder_job_wq[1]);
        #endif
    #endif
#endif

    favce_proc_close();
    favce_release_base_address();

#ifdef HANDLE_PANIC_STATUS
    unregister_panic_notifier(favce_panic_handler);
    unregister_printout_notifier(favce_printout_handler);
#endif

#ifdef RECORD_BS
    release_frammap_buffer(&mcp280_rec_bs_buffer[0]);
    release_frammap_buffer(&mcp280_rec_bs_buffer[1]);
#endif

    pf_mcp280_clk_off();
    pf_mcp280_clk_exit();
}

