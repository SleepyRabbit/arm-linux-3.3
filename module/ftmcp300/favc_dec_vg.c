/* favc_dec_vg.c */
#include <linux/kthread.h>
#include <linux/delay.h>
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
#include <linux/semaphore.h>
#if (LINUX_VERSION_CODE == KERNEL_VERSION(3,3,0))
#include <asm/cacheflush.h>
#endif
#include <linux/platform_device.h>
#include <mach/ftpmu010.h>
#include <mach/fmem.h> /* fmem_dcache_sync() */
 

//#include "log.h"    //include log system "printm","damnit"...
#include "video_entity.h"   //include video entity manager
#include "dec_driver/define.h"
#include "favc_dec_entity.h"
#include "favc_dec_vg.h"
#include "frammap_if.h"
#include "favc_dec_proc.h"
#include "dec_driver/H264V_dec.h"
#include "dec_driver/global.h"
#include "favcd_param.h"
#include "dec_driver/favc_dec_ll.h"
#include "favc_dec_dbg.h"
#include "favc_dec_rev.h"


#define MULTI_FORMAT_DECODER  // define this to register this driver to multi-format decoder (if not define, register to VG directly)
#ifdef MULTI_FORMAT_DECODER
#include "../decoder/decoder_vg.h"
#endif // MULTI_FORMAT_DECODER


/* warnings/errors for incorrect config */
#if USE_MULTICHAN_LINK_LIST
    #error MULTICHAN_LINK_LIST is not yet impelmented for VG driver
#endif
#if USE_LINK_LIST
    #warning linked list error handling is not complete
#endif

#if USE_KTHREAD
    static int ftmcp300_pp_thread(void *data);
    static int ftmcp300_cb_thread(void *data);
    
    static void ftmcp300_pp_wakeup(void);
    static void ftmcp300_cb_wakeup(void);
    
    static struct task_struct *pp_thread = NULL;
    static struct task_struct *cb_thread = NULL;
    
    static wait_queue_head_t pp_thread_wait;
    static wait_queue_head_t cb_thread_wait;
    
    static int pp_wakeup_event = 0;
    static int cb_wakeup_event = 0;
    
    static volatile int pp_thread_ready = 0;
    static volatile int cb_thread_ready = 0;
    
    unsigned int  debug_value = 0xAAAAAAAA;
#endif


//#define STATIC static
#define STATIC  /* to make functions non-static, so that its name will occur on stack trace when kernel oops */



/* 
 * Module parameters 
 */
int chn_num = DEFAULT_CHN_NUM;
module_param(chn_num, int, S_IRUGO); /* set at module loading time only */
MODULE_PARM_DESC(chn_num, "channel number");

int job_seq_num = 0;
module_param(job_seq_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(job_seq_num, "current job's seq number");

#if REV_PLAY
int gop_job_seq_num = 0;
module_param(gop_job_seq_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(gop_job_seq_num, "current gop job's seq number");

unsigned int max_gop_size = MAX_GOP_SIZE;
module_param(max_gop_size, uint, S_IRUGO);
MODULE_PARM_DESC(max_gop_size, "pre-alloc rev play job_item slots per ch");

unsigned int gop_output_group_size = 0;
module_param(gop_output_group_size, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(gop_output_group_size, "output group size for gop_job_item");
#endif

int cb_seq_num = 0;
module_param(cb_seq_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(cb_seq_num, "current job's callback seq number");

int trace_job = 0;
module_param(trace_job, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(trace_job, "print debug char at job processing");

int blk_read_dis = BLOCK_READ_DISABLE;
module_param(blk_read_dis, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(blk_read_dis, "value for 0x24 bit 21:block read disable");

int sw_reset_timeout_ret = SW_RESET_TIMEOUT_RET;
module_param(sw_reset_timeout_ret, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_reset_timeout_ret, "1: return at sw reset timeout. 0: not return");

int clear_sram = CLEAR_SRAM_AT_INIT;
module_param(clear_sram, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clear_sram, "whether to fill ref list 0/1 up");

unsigned int mcp300_sw_reset = 0;
module_param(mcp300_sw_reset, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_sw_reset, "whether to enable sw reset");

unsigned int clean_en = 1;
module_param(clean_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clean_en, "clean bitstream buffer cache enable");

unsigned int pad_en = 0;
module_param(pad_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pad_en, "codec padding enable");

unsigned int err_bs = 0;
module_param(err_bs, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(err_bs, "number of error bitstream to be generated");

unsigned int err_bs_type = 0;
module_param(err_bs_type, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(err_bs_type, "type of generated error");

unsigned int bs_padding_size = PADDING_SIZE;
module_param(bs_padding_size, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(bs_padding_size, "bitstream buffer padding size");

unsigned int mcp300_codec_padding_size = CODEC_PANDING_LEN;
module_param(mcp300_codec_padding_size, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_codec_padding_size, "codec padding size");

/* set maximum resolution (for allocating memory for a engine (shared between channels)) */
unsigned int mcp300_max_width = MAX_DEFAULT_WIDTH;
module_param(mcp300_max_width, uint, S_IRUGO); /* set at module loading time only */
MODULE_PARM_DESC(mcp300_max_width, "Max Width");

unsigned int mcp300_max_height = MAX_DEFAULT_HEIGHT;
module_param(mcp300_max_height, uint, S_IRUGO); /* set at module loading time only */
MODULE_PARM_DESC(mcp300_max_height, "Max Height");

int mcp300_max_buf_num = MAX_DEC_BUFFER_NUM;
module_param(mcp300_max_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_max_buf_num, "Max Buffer Number per channel");

int extra_buf_num = 1;
module_param(extra_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(extra_buf_num, "Extra Buffer Number per channel");

int chk_buf_num = 1;
module_param(chk_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chk_buf_num, "whether to compare max buf num and ref num");

int max_pp_buf_num = MAX_DEC_BUFFER_NUM;
module_param(max_pp_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_pp_buf_num, "Max Buffer Number can be used in prepare_job");

unsigned int mcp300_alloc_mem_from_vg = 1;
module_param(mcp300_alloc_mem_from_vg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_alloc_mem_from_vg, "Allocate memory from VG");

unsigned int mcp300_b_frame = 0; /* 0: not support B-frame, 1: support B-frame */
module_param(mcp300_b_frame, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(mcp300_b_frame, "Whether to allocate memory required by B-frame");

int clear_mbinfo = 0;
module_param(clear_mbinfo, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clear_mbinfo, "whether to clear mbinfo memory after memory allocation");

int engine_num = -1; /* <0: engine = (minor % engine_num)  >= 0: force engine number */
module_param(engine_num, int, S_IRUGO); /* set at module loading time only */
MODULE_PARM_DESC(engine_num, "enabled engines: -1-enable all, 0-enable engine 0 only, 1-enable engine 1 only");

int log_level = 0; /* larger level, more message */
module_param(log_level, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(log_level, "leg message level");

int dbg_mode = 0; /*  0: normal mode, >0: debug mode */
module_param(dbg_mode, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dbg_mode, "debug mode");

/*
 * module parameters for dump data to file
 */
#define DEF_DUMP_FILE_PATH "/mnt/nfs/"
char file_path_buf[128] = DEF_DUMP_FILE_PATH;
char *file_path = file_path_buf;
module_param(file_path, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(file_path, "path for saving file");

int save_bs_cnt = 0; /* number of bitstreams to be saved */
module_param(save_bs_cnt, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(save_bs_cnt, "number of input bitstreams to be saved (in job number)");

int save_mbinfo_cnt = 0;
module_param(save_mbinfo_cnt, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(save_mbinfo_cnt, "number of mbinfo to be saved (in job number)");

int save_output_cnt = 0;
module_param(save_output_cnt, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(save_output_cnt, "number of output YUV to be saved (in job number)");


int save_err_bs_flg = 0;
module_param(save_err_bs_flg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(save_err_bs_flg, "enable save bitstream at the next decoding error");

int reserve_buf = 0; /* whether to call video_free_buffer() and video_reserve_buffer() */
module_param(reserve_buf, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(reserve_buf, "enable reserve buffer");

int output_mbinfo = 0; /* whether to get mbinfo for each channel */
module_param(output_mbinfo, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(output_mbinfo, "enable output mbinfo for each channel");

int chroma_interlace = 3; /* chroma interlance: 0: set it to 0, 1: set it to 1, 3: set HW chroma interlance to 1 at field coding, set it to 0 at frame coding */
module_param(chroma_interlace, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chroma_interlace, "chroma interlance: 0: set it to 0, 1: set it to 1, 3: set HW chroma interlance to 1 at field coding, set it to 0 at frame coding");

int use_ioremap_wc = 0; /* whether to use ioremap_wc() instead of ioremap_nocache() NOTE:this is not yet tested for a long time, so it is disabled by default */
module_param(use_ioremap_wc, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(use_ioremap_wc, "enable ioremap_wc");

unsigned int callback_period = 3; /* in msecs */
module_param(callback_period, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(callback_period, "callback workq trigger period in msecs");

unsigned int prepare_period = 3; /* in msecs */
module_param(prepare_period, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(prepare_period, "perpare workq trigger period in msecs");

int print_out_verbose = 0; /* whether to print verbose message at damnit or dumplog is read */
module_param(print_out_verbose, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(print_out_verbose, "enable print verbose message at damnit or dumplog is read");

int buf_ddr_id = DDR_ID_SYSTEM;
module_param(buf_ddr_id, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(buf_ddr_id, "ddr id for allocating intra/linked list buffer");

unsigned int vc_cfg = (ENABLE_VCACHE_REF << 2)|(ENABLE_VCACHE_SYSINFO << 1)|ENABLE_VCACHE_ILF; /* vcache config */
module_param(vc_cfg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(vc_cfg, "vcache config");

unsigned int lose_pic_handle_flags = LOSS_PIC_RETURN_ERR | LOSS_PIC_PRINT_ERR;
module_param(lose_pic_handle_flags, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(lose_pic_handle_flags, "lose picture handling flags: bit 0: whether to print error message. bit 1: whether to return error");

unsigned int hw_timeout_delay = 0;
module_param(hw_timeout_delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(hw_timeout_delay, "hardware timeout value: 0: use default value");

#if SW_TIMEOUT_ENABLE
unsigned int sw_timeout_delay = HZ * 3;
module_param(sw_timeout_delay, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_timeout_delay, "software timeout period in msecs");
#endif

int output_buf_num = 16;
module_param(output_buf_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(output_buf_num, "number of output buffer");

unsigned int dump_cond = 0;
module_param(dump_cond, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dump_cond, "dump enable bit mask");

unsigned int sw_timeout = 0;
module_param(sw_timeout, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(sw_timeout, "testing sw timeout enable");

unsigned int delay_en = 0;
module_param(delay_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(delay_en, "for testing ilf error");

unsigned int clear_en = 0;
module_param(clear_en, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clear_en, "for testing ilf error");

unsigned int err_handle = 1;
module_param(err_handle, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(err_handle, "error handling method: 0: re-init and skip until I-frame after major errors only.  1: re-init for all error");

unsigned int clk_sel = 0;
module_param(clk_sel, uint, S_IRUGO);/* set at module loading time only */
MODULE_PARM_DESC(clk_sel, "select clock source");

unsigned int rec_prop = 1;
module_param(rec_prop, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(rec_prop, "record property: 0: record input property only. 1: record all property. otherwise no record");

unsigned int print_prop = 0;
module_param(print_prop, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(print_prop, "enable print output property");

unsigned int en_src_fr_prop = 1;
module_param(en_src_fr_prop, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(en_src_fr_prop, "enable output src_frame_rate property");

unsigned int chip_ver_limit = FTMCP300_CHIP_VER_VAL; /* 0: skip chip version check. != 0: compare PMU CHIP version with this version */
module_param(chip_ver_limit, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chip_ver_limit, "chip version check value");

unsigned int chip_ver_mask = FTMCP300_CHIP_VER_MASK; /* mask used at comparing chip version */
module_param(chip_ver_mask, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(chip_ver_mask, "chip version mask value");

unsigned int max_level_idc = MAX_LEVEL_IDC; /* MAX supported level. Set this to 51 when testing with 4K2K patterns */
module_param(max_level_idc, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_level_idc, "max supported level");

unsigned int minor_prof = 1;
module_param(minor_prof, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(minor_prof, "minor profiling enable");



/* 1 : if vcache has only one reference memory, driver may set ref1 local base according to current frame width 
 * 0 : define 0 to avoid changing VCACHE 0x30 (LOCAL_BASE) register on 8210
 */
unsigned int vcache_one_ref = VCACHE_ONE_REFERENCE; 
module_param(vcache_one_ref, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(vcache_one_ref, "vcache has one reference memory");

unsigned int max_ref1_width = MAX_REF1_WIDTH; /* max width to enable vcache ref 1 */
module_param(max_ref1_width, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_ref1_width, "max width to enable vcache ref 1");

//2014.09.23 enable recording fail list channel to reduce CPU utilization
unsigned int record_fail_list_chn = 1;
module_param(record_fail_list_chn, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(record_fail_list_chn, "enable recording fail list channel to reduce CPU utilization");

/*
 * Global variables
 */


#if USE_LINK_LIST
struct buffer_info_t ll_buf[ENTITY_ENGINES * LL_NUM] = { {0, 0, 0} };
#endif
struct buffer_info_t mcp300_intra_buffer[ENTITY_ENGINES] = {{0, 0, 0}};
DecoderEngInfo favcd_engine_info[ENTITY_ENGINES];
struct favcd_data_t    *private_data = NULL;

//2014.09.23 record fail_list_chn
char *fail_list_idx = NULL;

spinlock_t favc_dec_lock;                   // lock for callback list/ready list entry, job status ...
struct semaphore  favc_dec_sem;             // for protecting engine list / minor list
struct list_head  favcd_global_head;        // in put job order
struct list_head *favcd_minor_head = NULL;  // job list head of each channel
struct list_head *favcd_fail_minor_head = NULL;  // failed job list head of each channel
struct list_head  favcd_standby_head;         // standby job list head
#if USE_SINGLE_PREP_LIST
struct list_head  favcd_prepare_head;         // prepare job list head
#endif
struct list_head *favcd_prepare_minor_head = NULL;  // prepare job list head of each channel
struct list_head  favcd_ready_head;           // ready job list head
struct list_head  favcd_callback_head;        // sort by callback order

#if REV_PLAY
struct list_head *favcd_gop_minor_head = NULL; // gop job group head of each channel
#endif



char *plt_str = FTMCP300_PLT_STR;
char favcd_ver_str[128] = {0};


/* 
 * profiling record
 */
struct pp_prof_data     pp_prof_data;
struct cb_prof_data     cb_prof_data;
struct isr_prof_data    isr_prof_data[ENTITY_ENGINES];
struct job_prof_data    job_prof_data;



/* 
 * property 
 */

enum favcd_property_id {
#ifdef MULTI_FORMAT_DECODER
    ID_DUMMY = (ID_COUNT + 1),
#else // !MULTI_FORMAT_DECODER
    ID_DUMMY = (MAX_WELL_KNOWN_ID_NUM + 1), //start from 100
    ID_YUV_WIDTH_THRESHOLD,       // ID of yuv_width_threshold
    ID_SUB_YUV_RATIO,             // ID of sub_yuv_ratio
#endif // !MULTI_FORMAT_DECODER

#if defined(ENABLE_POC_SEQ_PROP) & !defined(MULTI_FORMAT_DECODER)
    ID_POC,                       // ID of POC value
    ID_SEQ,                       // ID of sequence value
#endif // ENABLE_POC_SEQ_PROP & !MULTI_FORMAT_DECODER
};



struct property_map_t favcd_property_map[] = {
    /************ input property *************/
    {ID_DST_FMT,            "dst_fmt",             "destination format"},
    {ID_BITSTREAM_SIZE,     "bitstream_size",      "input bitstream size (including padded bytes)"},
    {ID_YUV_WIDTH_THRESHOLD,"yuv_width_threshold", "if decoded width is larger than this and sub_yuv_ratio != 0, then output sub yuv"},
    {ID_SUB_YUV_RATIO,      "sub_yuv_ratio",       "ratio value 1:N. N=1(sub yuv disabled), 2 or 4."},
#if REV_PLAY
    {ID_DEC_DIRECT,         "direct",              "output direction. 0:forward 1:backward"},
    {ID_DEC_GOP,            "gop_size",            "number of frames in GOP."},
    {ID_DEC_OUT_GROUP_SIZE, "output_group_size",   "number of jobs to be outputed in one forward decoding pass"},
#endif
    /************ output property *************/
    {ID_SUB_YUV,            "sub_yuv",             "if output sub yuv exists"},
    {ID_DST_DIM,            "dst_dim",             "cropped destination width/height"},
    {ID_SRC_INTERLACE,      "src_interlace",       "0:frame coded, 1:field coded"},
    {ID_SRC_FMT,            "src_fmt",             "source format"},
    {ID_SRC_FRAME_RATE,     "src_frame_rate",      "source frame rate in fps"},
#ifdef ENABLE_POC_SEQ_PROP
    {ID_POC,                "poc",                 "picture display order"},
    {ID_SEQ,                "seq",                 "another picture display order(for debug)"},
#endif // ENABLE_POC_SEQ_PROP
    /************ input & output property *************/
    {ID_DST_XY,             "dst_xy",              "destination x and y position"},
    {ID_DST_BG_DIM,         "dst_bg_dim",          "destination background width/height(as input: buffer size; as output: non-cropped size)"},
    /************ output property (for querying only)*************/
    {ID_FPS_RATIO,          "fps_ratio",           "XXXXYYYY. Output fps = src_frame_rate * XXXX/YYYY"},
};

const unsigned int property_map_size = ARRAY_SIZE(favcd_property_map);


/* 
 * workqueue / timer
 */
#if USE_VG_WORKQ
vp_workqueue_t *favcd_cb_wq = NULL;
vp_work_t process_callback_work;
vp_workqueue_t *favcd_prepare_wq = NULL;
vp_work_t process_prepare_work;
#else // !USE_VG_WORKQ
static struct workqueue_struct *favcd_cb_wq = NULL;
static DECLARE_DELAYED_WORK(process_callback_work, 0);
static struct workqueue_struct *favcd_prepare_wq = NULL;
static DECLARE_DELAYED_WORK(process_prepare_work, 0);
#endif // !USE_VG_WORKQ


void work_scheduler(struct isr_prof_data *isr_prof_ptr);
void callback_scheduler(void);
void prepare_job(void);


void trigger_callback(void);
void trigger_prepare(void);
void favcd_dump_job_bs(struct favcd_job_item_t *job_item);
int __favcd_output_all_frame(struct favcd_data_t *dec_data);


#if SW_TIMEOUT_ENABLE
struct timer_list sw_timeout_timer[ENTITY_ENGINES];
#endif // SW_TIMEOUT_ENABLE



/* utilization */
unsigned int utilization_period = 5; //5sec calculation (the duration of an engine utilization measurment)
struct utilization_record_t eng_util[ENTITY_ENGINES];

/* property lastest record */
struct property_record_t *property_record = NULL;


#define __PRIVATE_FUNCTION_DECLARATION__


#if ENABLE_VIDEO_RESERVE
unsigned int resv_out_cnt = 0;
unsigned int free_out_cnt = 0;

/* 
 * wrapper functions for video_reserve_buffer(), video_free_buffer() 
 * avoid to call these functions in test entity
 */
void *__video_reserve_buffer(struct video_entity_job_t *job, int dir)
{
//    DEBUG_M(LOG_WARNING, -1, -1, "calling video_reserve_buffer for job:%d dir:%d buf:%08X\n", job->id, dir, job->out_buf.buf[0].addr_pa);

    resv_out_cnt++;
    
    if(reserve_buf == 0){
        return (void *)-1;
    }
#ifdef A369
    return (void *)-1;
#else
    return video_reserve_buffer(job, dir);
#endif
}

int __video_free_buffer(void *rev_handle)
{
    free_out_cnt++;

    if(rev_handle == (void *)-1){ /* buffer is reserved when reserve_buf==0 */
        return 0;
    }
    
#ifdef A369
    return 0;
#else
    return video_free_buffer(rev_handle);
#endif
}
#endif // ENABLE_VIDEO_RESERVE


/*
 * allocate/free functions for job item
 */
unsigned int alloc_cnt = 0;
unsigned int free_cnt = 0;
struct kmem_cache *job_cache = NULL;
const char cache_name[10] = "favc_dec";

static inline void *__alloc_job_item(unsigned int size)
{
    void *ptr = kmem_cache_alloc(job_cache, GFP_KERNEL);
    if(ptr == NULL){
        return NULL;
    }
    favcd_alloc_check(ptr);
    alloc_cnt++;
    return ptr;
}

static inline void __free_job_item(void *ptr)
{
    favcd_free_check(ptr);
    if(ptr){
        kmem_cache_free(job_cache, ptr);
        free_cnt++;
    }
}



STATIC int __favcd_job_cleanup(struct favcd_job_item_t *job_item);


/*
 * function called by low level driver when a serious error occurred
 */
void favcd_damnit(char *str)
{
    printk("favcd_damnit at: %s\n", str);
    damnit(MODULE_NAME);
}



#define __BUFFER_MANAGEMENT__

/*
 * <<<<<<<<<<<<<<<<<< Start of Buffer allocation/release functions >>>>>>>>>>>>>>>>>>>>>>>>>>
 */
unsigned int fkmalloc_cnt = 0;
unsigned int fkfree_cnt = 0;
unsigned int fkmalloc_size = 0;

/*
 * allocate memory for decoder data structure (accessed by CPU only)
 */
void *fkmalloc(size_t size)
{
	unsigned int  sz = size;
	void          *ptr;

    if(in_interrupt()){
        ptr = kmalloc(sz, GFP_ATOMIC);
    }else{
        ptr = kmalloc(sz, GFP_KERNEL);
    }
	
	if (ptr == 0) {
		printk("Fail to fkmalloc for addr 0x%x size 0x%x\n", (unsigned int)ptr, (unsigned int)sz);
		return 0;
	}

    fkmalloc_cnt++;
    fkmalloc_size += size;
	return ptr;
}

/*
 * free memory for decoder data structure (accessed by CPU only)
 */
void fkfree(void *ptr)
{
	if (ptr){
		kfree(ptr);
        fkfree_cnt++;
    }
}



/* 
 * allocate DMA-able buffer via frammap 
 */
STATIC int allocate_buffer(struct buffer_info_t *buf, int size)
{
    struct frammap_buf_info buf_info;

    DEBUG_M(LOG_TRACE, 0, 0, "allocate_buffer enter 0x%08X size:%d\n", (int)buf, size);

    memset(&buf_info, 0, sizeof(struct frammap_buf_info));
    buf_info.size = size;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    buf_info.align = 64;
    buf_info.name = "favcd_dec";
#endif
    buf_info.alloc_type = ALLOC_NONCACHABLE;
    
    if (frm_get_buf_ddr(buf_ddr_id, &buf_info) < 0){
        DEBUG_M(LOG_ERROR, 0, 0, "frm_get_buf_ddr failed\n");
        FAVCD_DAMNIT("allocate memory via frammap failed\n");
        return -1;
    }
    buf->addr_va = (unsigned int)buf_info.va_addr;
    buf->addr_pa = (unsigned int)buf_info.phy_addr;
    buf->size = size;
    DEBUG_M(LOG_TRACE, 0, 0, "allocate_buffer return 0x%08X (va:0x%08X pa:0x%08X)\n", (int)buf, buf->addr_va, buf->addr_pa);
    return 0;
}


/* 
 * free DMA-able buffer via frammap 
 */
STATIC int free_buffer(struct buffer_info_t *buf)
{
    int ret;
    DEBUG_M(LOG_TRACE, 0, 0, "free_buffer enter 0x%08X size:%u\n", (int)buf, buf->size);

    if (buf->addr_va) {
        DEBUG_M(LOG_TRACE, 0, 0, "free buffer 0x%08X (va:0x%08X pa:0x%08X)\n", (int)buf, buf->addr_va, buf->addr_pa);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
        ret = frm_free_buf_ddr((void*)buf->addr_va);
#else
        ret = frm_free_buf_ddr(buf);
#endif
        if (ret < 0){
            DEBUG_M(LOG_ERROR, 0, 0, "frm_free_buf_ddr failed: %08X\n", buf->addr_va);
            return -1;
        }
        buf->addr_va = 0;
        buf->addr_pa = 0;
    }
    DEBUG_M(LOG_TRACE, 0, 0, "free_buffer return\n");
    return 0;
}


/*
 * release DMA-able memory for one engine
 */
STATIC int release_per_engine_buffer(int engine)
{
#if USE_LINK_LIST
    int i;
#endif
    
    DEBUG_M(LOG_TRACE, engine, 0, "(%d) release_per_engine_buffer enter\n", engine);
    free_buffer(&mcp300_intra_buffer[engine]);
#if USE_LINK_LIST
    /* free linked list data structure */
    for(i = 0; i < LL_NUM; i++){
        int ll_idx = engine * LL_NUM + i;
        free_buffer(&ll_buf[ll_idx]);
    }
#endif
    
    DEBUG_M(LOG_TRACE, engine, 0, "(%d) release_per_engine_buffer return\n", engine);
    return 0;
}


/*
 * allocate DMA-able memory for one engine
 */
STATIC int allocate_per_engine_buffer(int engine, int max_width)
{
    int ret;
#if USE_LINK_LIST
    int i;
#endif

    DEBUG_M(LOG_TRACE, engine, 0, " allocate_per_engine_buffer enter: %d %d\n", engine, max_width);
    if(engine >= ENTITY_ENGINES){
        DEBUG_M(LOG_ERROR, engine, 0, " invalid engine index: %d %d\n", engine, max_width);
        return 0;
    }

    /* allocate memory for intra buffer */
	/* size = one MB row * 32 byte (mcp300_max_width * 2) */
	ret = allocate_buffer(&mcp300_intra_buffer[engine], max_width * 2);
    if (ret < 0) {
        DEBUG_M(LOG_ERROR, 0, 0, "allocate intra buffer error\n");
        goto allocate_fail;
    }

#if USE_LINK_LIST
    /* allocate linked list data structure */
    for(i = 0; i < LL_NUM; i++){
        int ll_idx = engine * LL_NUM + i;
        
        ret = allocate_buffer(&ll_buf[ll_idx], sizeof(*favcd_engine_info[engine].codec_ll[0]));
        if (ret < 0) {
            DEBUG_M(LOG_ERROR, 0, 0, "allocate intra buffer error\n");
            goto allocate_fail;
        }
        printk("allocate memory for linked list done: %d\n", sizeof(*favcd_engine_info[engine].codec_ll[0]));
        
    }
#endif /* USE_LINK_LIST */


    DEBUG_M(LOG_TRACE, engine, 0, "(%d) allocate_per_engine_buffer return: %d\n", engine, max_width);
    return 0;
    
allocate_fail:
    DEBUG_M(LOG_TRACE, engine, 0, "(%d) allocate_per_engine_buffer return fail: %d\n", engine, max_width);
    release_per_engine_buffer(engine);
    
    return -1;
}

/*
 * <<<<<<<<<<<<<<<<<< Start of Buffer allocation/release functions >>>>>>>>>>>>>>>>>>>>>>>>>>
 */


/*
 * Start of VG per-channel buffer pool manage functions
 * NOTE: a typical life cycle of buffer:
 * allocate_dec_buffer --> push_dec_buffer  --
 *     free_dec_buffer <--  pop_dec_buffer <-|
 */

/* 
 * Insert an allocated buffer into an unused slot of the per-channel buffer pool and get its buffer index
 * dec_buf_ptr: dec buffer to be pushed in to an unused slot of dec_buf array
 */
int push_dec_buffer(struct favcd_data_t *dec_data, BufferAddr *dec_buf_ptr)
{
    int i, buf_idx;

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) push_dec_buffer\n", dec_data->engine, dec_data->minor);
    
    /* find an unused buffer */
    /* NOTE: should use lock here */
    buf_idx = -1;
    for (i = 0; i < MAX_DEC_BUFFER_NUM; i++) {
        if (!dec_data->dec_buf[i].is_used) {
            buf_idx = i;
            break;
        }
    }

    if(buf_idx < 0){
        /* can not find an unused buffer */
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) push_dec_buffer: reconstruct buffer is full\n", dec_data->engine, dec_data->minor);
        FAVCD_DAMNIT("no avaliable decode buffer index\n");
        return -1;
    }

    if(dec_buf_ptr->job_item == NULL){
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) push_dec_buffer: no associated job for buffer\n", dec_data->engine, dec_data->minor);
        FAVCD_DAMNIT("no associated job for buffer\n");
        return -1;
    }

    dec_buf_ptr->job_item->buf_idx = buf_idx;
    dec_buf_ptr->job_item->released_flg = 0;
    
    dec_buf_ptr->is_used = 1; /* mark buffer as used */
    dec_data->dec_buf[buf_idx] = *dec_buf_ptr;
    dec_data->used_buffer_num++;

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) push_dec_buffer idx: %d\n", dec_data->engine, dec_data->minor, buf_idx);
    DUMP_MSG(D_VG_BUF, dec_data->dec_handle, "(%d,%d) push_dec_buffer idx: %d\n", dec_data->engine, dec_data->minor, buf_idx);

    return buf_idx;
}


/* 
 * Remove a buffer from the per-channel buffer pool
 * dec_buf_ptr: poped dec buffer
 */
int pop_dec_buffer(struct favcd_data_t *dec_data, BufferAddr *dec_buf_ptr, int buf_idx)
{
    struct favcd_job_item_t *job_item;
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) pop_dec_buffer %d\n", dec_data->engine, dec_data->minor, buf_idx);
    DUMP_MSG(D_VG_BUF, dec_data->dec_handle, "(%d,%d) pop_dec_buffer %d\n", dec_data->engine, dec_data->minor, buf_idx);
    if(dec_data->dec_buf[buf_idx].is_used == 0){
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) pop_dec_buffer buf_idx:%d buf unused\n", dec_data->engine, dec_data->minor, buf_idx);
        return -1;
    }

    job_item = dec_data->dec_buf[buf_idx].job_item;

    if(job_item == NULL){ /* unlink job_item to buffer */
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) pop_dec_buffer buf_idx:%d not associated job\n", dec_data->engine, dec_data->minor, buf_idx);
        return -1;
    }


    /* santy check for job status */
    switch(job_item->status){
#if REV_PLAY
        case DRIVER_STATUS_STANDBY:
#endif
        case DRIVER_STATUS_FAIL:
        case DRIVER_STATUS_KEEP:
            break;
        case DRIVER_STATUS_READY:
        case DRIVER_STATUS_ONGOING:
        default:
            DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) pop_dec_buffer buf_idx:%d invalid job status: %d id: %d seq: %d\n", 
                dec_data->engine, dec_data->minor, buf_idx, job_item->status, job_item->job_id, job_item->seq_num);
            printk("invalid job status at pop_dec_buffer\n");
            return -1;
            break;
            
    }

    /* unlink job_item to buffer */
    job_item->buf_idx = -1;
    job_item->released_flg = 1;
    
    dec_data->dec_buf[buf_idx].is_used = 0;
    *dec_buf_ptr = dec_data->dec_buf[buf_idx];
    dec_data->used_buffer_num--;
        
    return 0;
}


/* 
 * free DMA-able buffer for each job item
 */
int free_dec_buffer(BufferAddr *dec_buf_ptr)
{
    struct favcd_job_item_t *job_item;
    int engine, minor;
    int ret;
    int job_id;
    
    job_item = dec_buf_ptr->job_item;

    if(job_item == NULL){
        engine = minor = -1;
        DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) free_dec_buffer freeing buffer without job assigned\n", -1, -1);
        if(DAMNIT_AT_ERR(dbg_mode)){
            FAVCD_DAMNIT("no associated job to decoder buffer\n");
        }
        return -1;
    }
    
    /* cleanup for job */
    job_id = job_item->job_id;
    engine = job_item->engine;
    minor = job_item->minor;
#if 1
    if(job_item->released_flg == 0){
        DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) free_dec_buffer freeing a job that is still referenced: %d\n", engine, minor, job_item->job_id);
        if(DAMNIT_AT_ERR(dbg_mode)){
            FAVCD_DAMNIT("free a job item with a buffer still being referenced\n");
        }
        return -1;
    }
#endif

#if 1
    if((job_item->status == DRIVER_STATUS_WAIT_POC) && (job_item->stored_flg == 1)){
        /* NOTE: This may happen if low level driver remove un-output frame via release list. 
                 It is handled by __favcd_handle_unoutput_buffer() and it should never happen */
        DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) free_dec_buffer freeing a job that is waiting POC: %d seq:%d\n", engine, minor, job_item->job_id, job_item->seq_num);
        if(DAMNIT_AT_ERR(dbg_mode)){
            FAVCD_DAMNIT("{%d,%d) free_dec_buffer freeing a job that is waiting POC: %d seq:%d\n", engine, minor, job_item->job_id, job_item->seq_num);
        }
    }
#endif

    /* free buffer */
    if(dec_buf_ptr->mbinfo_va){
        if(dec_buf_ptr->alloc_from_vg){
            video_free_buffer_simple((void *)dec_buf_ptr->mbinfo_va);
        }else{
            struct buffer_info_t buf;
            buf.addr_va = dec_buf_ptr->mbinfo_va;
            /* NOTE: not necessary, for debug only */
            buf.addr_pa = dec_buf_ptr->mbinfo_pa;
            buf.size = job_item->Y_sz;
            
            if(free_buffer(&buf) < 0){
                printk("free_dec_buffer failed: 0x%08X\n", buf.addr_va);
                return -1;
            }
        }
    }


    ret = __favcd_job_cleanup(job_item);
#if 0
    if(ret){
        printk("__favcd_job_cleanup ret:%d in free_dec_buffer %d buf_idx:%d\n", ret, job_id, job_item->buf_idx);
    }else{
        printk("__favcd_job_cleanup ret:%d in free_dec_buffer %d\n", ret, job_id);
    }
#endif
    
    
    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) free_dec_buffer free %08X done\n", engine, minor, dec_buf_ptr->mbinfo_va);

    return 0;
}


/* 
 * allocate DMA-able buffer for each job item
 */
int allocate_dec_buffer(BufferAddr *dec_buf_ptr, struct favcd_job_item_t *job_item)
{
    struct video_entity_job_t *job = job_item->job;

    DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "(%d,%d) allocate_dec_buffer job %d\n", job_item->engine, job_item->minor, job_item->job_id);

    
    /* mbinfo buffer */
    if(mcp300_b_frame || output_mbinfo){
        if(mcp300_alloc_mem_from_vg){
            unsigned int addr_va;
            unsigned int addr_pa;
            addr_va = (unsigned int)video_alloc_buffer_simple(job, job_item->Y_sz, 0, &addr_pa);
            if(addr_va == 0){
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "allocate_dec_buffer for mbinfo failed: %d\n", job_item->Y_sz);
                return -1;
            }
            dec_buf_ptr->mbinfo_va = addr_va;
            dec_buf_ptr->mbinfo_pa = addr_pa;
            dec_buf_ptr->alloc_from_vg = 1;
        }else{
            struct buffer_info_t buf;
            if(allocate_buffer(&buf, job_item->Y_sz) < 0){
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "allocate_dec_buffer for mbinfo failed: %d\n", job_item->Y_sz);
                return -1;
            }
            dec_buf_ptr->mbinfo_va = buf.addr_va;
            dec_buf_ptr->mbinfo_pa = buf.addr_pa;
            dec_buf_ptr->alloc_from_vg = 0;
        }
        if(clear_mbinfo){
            memset((void *)dec_buf_ptr->mbinfo_va, 0, job_item->Y_sz);
        }
    }else{
        /* no need to allocate mbinfo */
        dec_buf_ptr->mbinfo_va = 0;
        dec_buf_ptr->mbinfo_pa = 0;
        dec_buf_ptr->alloc_from_vg = 0;
    }


    /* reconstruct buffer */
    dec_buf_ptr->start_va = job->out_buf.buf[0].addr_va;
    dec_buf_ptr->start_pa = job->out_buf.buf[0].addr_pa;

    /* scaled buffer */
    if(job_item->sub_yuv_en && (job->out_buf.btype == TYPE_YUV422_RATIO_FRAME)){
        dec_buf_ptr->scale_va = job->out_buf.buf[1].addr_va;
        dec_buf_ptr->scale_pa = job->out_buf.buf[1].addr_pa;
    }else{
        dec_buf_ptr->scale_va = 0;
        dec_buf_ptr->scale_pa = 0;
    }

    dec_buf_ptr->job_item = job_item; /* link job_item to the buffer */
    dec_buf_ptr->is_released = 0;
    dec_buf_ptr->is_outputed = 0;
    dec_buf_ptr->is_started = 0;
    //job_item->released_flg = 0;
    
    DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "(%d,%d) allocate_dec_buffer job %d addr:%08X\n", job_item->engine, job_item->minor, job_item->job_id, dec_buf_ptr->mbinfo_va);

    return 0;
}

/*
 * End of VG per-channel buffer pool manage functions
 */

/* 
 *  prepare parameters for H264Dec_OneFrame() 
 */
int favcd_set_buffer(struct favcd_data_t *dec_data, FAVC_DEC_BUFFER *pBuf, int idx)
{
    struct favcd_job_item_t *job_item;
    
    job_item = dec_data->curr_job;
    idx = job_item->buf_idx;

    if(idx < 0 || idx >= MAX_DEC_BUFFER_NUM){
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "set_buffer invalid idx: %d for job %d seq:%d\n", idx, job_item->job_id, job_item->seq_num);
        FAVCD_DAMNIT("set_buffer invalid idx:%d\n", idx);
        return -1;
    }

    if(dec_data->dec_buf[idx].is_used == 0){
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "set_buffer to an unused idx: %d\n", idx);
        FAVCD_DAMNIT("set buffer to an unused idx:%d\n", idx);
        return -1;
    }
    
    pBuf->buffer_index = idx;
    pBuf->dec_yuv_buf_virt = (unsigned char *) dec_data->dec_buf[idx].start_va;
    pBuf->dec_yuv_buf_phy  = (unsigned char *) dec_data->dec_buf[idx].start_pa;
    pBuf->dec_mbinfo_buf_virt = (unsigned char *) dec_data->dec_buf[idx].mbinfo_va;
    pBuf->dec_mbinfo_buf_phy = (unsigned char *) dec_data->dec_buf[idx].mbinfo_pa;
    pBuf->dec_scale_buf_virt = (unsigned char *) dec_data->dec_buf[idx].scale_va;
    pBuf->dec_scale_buf_phy = (unsigned char *) dec_data->dec_buf[idx].scale_pa;

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, 
        "(%d,%d) set_buffer ref: %x(%x), mbinfo: %x(%x), scale: %x(%x)\n", 
        dec_data->engine, dec_data->minor, 
        (unsigned int)pBuf->dec_yuv_buf_virt, (unsigned int)pBuf->dec_yuv_buf_phy,
        (unsigned int)pBuf->dec_mbinfo_buf_virt, (unsigned int)pBuf->dec_mbinfo_buf_phy,
        (unsigned int)pBuf->dec_scale_buf_virt, (unsigned int)pBuf->dec_scale_buf_phy);
    
    return 0;
}


#define __JOB_PROPERTY__
/*
 * start of functions for process job's property
 */

/*
 * Copy parsed property from job_item to per-channel data
 */
int favcd_get_in_property(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item)
{
    dec_data->dst_fmt = job_item->dst_fmt;
    dec_data->yuv_width_thrd = job_item->yuv_width_thrd;
    dec_data->sub_yuv_en = job_item->sub_yuv_en;
    dec_data->sub_yuv_ratio = job_item->sub_yuv_ratio;
    if(dec_data->dst_bg_dim != job_item->dst_bg_dim){
        dec_data->need_init_flg = INIT_NORMAL;
        dec_data->dst_bg_dim = job_item->dst_bg_dim;
    }

    return 0;
}


/*
 * Record job's input property to an per-channel property record (starting from prop_rec_idx)
 * retval: the index of the last recorded property + 1
 */
int favcd_record_in_property(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item)
{
    struct video_property_t *property;
    int idx = job_item->minor;
    int i = 0;
    int prop_rec_idx = 0; /* recording start from idx 0 */

    
    property = job_item->job->in_prop;
    for(i = 0; i < sizeof(job_item->job->in_prop)/sizeof(job_item->job->in_prop[0]); i++){
        if(property[i].id == ID_NULL){
            break;
        }
        if(prop_rec_idx >= MAX_RECORD){
            break;
        }
        property_record[idx].property[prop_rec_idx].id = property[i].id;
        property_record[idx].property[prop_rec_idx].value = property[i].value;
        prop_rec_idx++;
    }
    /* point i to the last record index if it exceed MAX_RECORD */
    if (prop_rec_idx >= MAX_RECORD) {
        prop_rec_idx = MAX_RECORD - 1;
    }

    /* pad a ID_NULL at the end of input property */
    property_record[idx].property[prop_rec_idx].id = ID_NULL;
    property_record[idx].property[prop_rec_idx].value = 0;
    
    /* record job info */
    property_record[idx].entity = job_item->job->entity;
    property_record[idx].job_id = job_item->job->id;
    property_record[idx].err_num = job_item->err_num;

    return prop_rec_idx;
}


/*
 * Record job's input/output property to an per-channel property record
 */
int favcd_record_all_property(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item)
{
    struct video_property_t *property;
    int idx = job_item->minor;
    int i = 0;
    int prop_rec_idx = 0;

    prop_rec_idx = favcd_record_in_property(dec_data, job_item);
    prop_rec_idx++; /* skip the ID_NULL at the end of input property */

    /* NOTE: input/output property are seperated by a ID_NULL element */

    property = job_item->job->out_prop;
    for(i = 0; i < sizeof(job_item->job->out_prop)/sizeof(job_item->job->out_prop[0]); i++){
        if(property[i].id == ID_NULL){
            break;
        }
        if(prop_rec_idx >= MAX_RECORD){
            break;
        }
        property_record[idx].property[prop_rec_idx].id = property[i].id;
        property_record[idx].property[prop_rec_idx].value = property[i].value;
        prop_rec_idx++;
    }
    /* point i to the last record index if it exceed MAX_RECORD */
    if (prop_rec_idx >= MAX_RECORD) {
        prop_rec_idx = MAX_RECORD - 1;
    }
    
    /* pad a ID_NULL at the end of output property */
    property_record[idx].property[prop_rec_idx].id = ID_NULL;
    property_record[idx].property[prop_rec_idx].value = 0;

    return 0;
}


/*
 * Print job's property (for debug only)
 */
void print_property(struct favcd_job_item_t *job_item, struct video_property_t *property)
{
    struct property_map_t *map;
    int i;

    printk("(%d)job:%d seq:%d\n", job_item->minor, job_item->job_id, job_item->seq_num);

    for(i = 0; i < MAX_PROPERTYS; i++){
        if(property[i].id == ID_NULL){
            break;
        }

        map = favcd_get_propertymap(property[i].id);
        if (map) {
            if(property[i].id == ID_DST_BG_DIM || property[i].id == ID_DST_DIM || property[i].id == ID_DST_XY){
                printk("%-3d  %19s  %08X    %4dx%4d  %s\n", property[i].id, map->name, property[i].value, (property[i].value >> 16), (property[i].value & 0xFFFF), map->readme);
            }else{
                printk("%-3d  %19s  %08X     %8d  %s\n", property[i].id, map->name, property[i].value, property[i].value, map->readme);
            }
        }
    }
}


/*
 * copy property value from job's input property to job item
 */
int favcd_parse_in_property_to_job_item(struct favcd_job_item_t *job_item)
{
    struct video_entity_job_t *job;
    struct video_property_t *property;
    int idx;
    int i = 0;
    unsigned int width;
    unsigned int height;
    unsigned int parsed_flg = 0;
    unsigned int err_flg = 0;

    job = job_item->job;
    if(job == NULL){
        DEBUG_J(LOG_ERROR, engine, minor, "decoder: job ptr is null(job_item:%08X id:%d)\n", (unsigned int)job_item, job_item->job_id);
        return -1;
    }
    idx = job_item->minor;
    property = job->in_prop;
    if(property == NULL){
        DEBUG_J(LOG_ERROR, engine, minor, "decoder: job property is null(job_item:%08X id:%d)\n", (unsigned int)job_item, job_item->job_id);
        return -1;
    }

    /* set the default value for the optional input property */
    job_item->sub_yuv_en = 0;

    while (property[i].id != 0) {
        switch (property[i].id) {
#if REV_PLAY
            case ID_DEC_GOP:
                job_item->gop_size = property[i].value;
                if(job_item->gop_size == 0){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: invalid value for gop size(%d)\n", job_item->gop_size);
                    err_flg = 1;
                }
                break;
            case ID_DEC_DIRECT:
                job_item->direct = property[i].value;
                if(job_item->direct != 0 && job_item->direct != 1){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: invalid value for direct(%d)\n", job_item->direct);
                    err_flg = 1;
                }
                break;
                
            case ID_DEC_OUT_GROUP_SIZE:
                job_item->output_group_size = property[i].value;
                break;
#endif
            case ID_BITSTREAM_SIZE:
                job_item->in_size = property[i].value;

#if CODEC_PANDING_LEN
                // check if bitstream size is too small
                if(job_item->in_size <= mcp300_codec_padding_size){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: incorrect bitstream size (< %d)\n", mcp300_codec_padding_size);
                    err_flg = 1;
                }
                // cut padded size off (4 bytes is added by VG)
                job_item->in_size -= mcp300_codec_padding_size;
#endif // CODEC_PANDING_LEN
                parsed_flg |= 1 << 0;
                break;
            case ID_DST_FMT:
                if(property[i].value != TYPE_YUV422_FRAME &&
                   property[i].value != TYPE_YUV422_RATIO_FRAME) {
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: unsupported DST_FMT:%d\n", property[i].value);
                    err_flg = 1;
                }else{
                    job_item->dst_fmt = property[i].value;
                }
                parsed_flg |= 1 << 1;
                break;
            case ID_YUV_WIDTH_THRESHOLD:
                job_item->yuv_width_thrd = property[i].value;
                parsed_flg |= 1 << 2;
                break;
            case ID_SUB_YUV_RATIO:
                if(property[i].value != 2 && property[i].value != 4 && 
                   property[i].value != 0 && property[i].value != 1){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: unsupported SUB_YUV_RATIO:%d\n", property[i].value);
                    err_flg = 1;
                    job_item->sub_yuv_en = 0;
                }else{
                	if(property[i].value == 2 || property[i].value == 4){
						job_item->sub_yuv_en = 1;
						job_item->sub_yuv_ratio = property[i].value;
                	}else{
						job_item->sub_yuv_en = 0;
            		}
                }
                /* NOTE: this input property is optional. no need to check it */
                //parsed_flg |= 1 << 5; 
                break;
            case ID_DST_XY:
                if(property[i].value != 0){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: DST_XY is non-zero:%d\n", property[i].value);
                    err_flg = 1;
                }
                job_item->dst_xy = property[i].value;
                parsed_flg |= 1 << 3;
                break;    
            case ID_DST_BG_DIM:
                job_item->dst_bg_dim = property[i].value;
                width = job_item->dst_bg_dim >> 16;
                height = job_item->dst_bg_dim & 0xFFFF;
                if((width & 15) || (height & 15)){
                    DEBUG_J(LOG_ERROR, engine, minor, "decoder: width/height in DST_BG_DIM is not a multiple of 16:%dx%d(%08X)\n", width, height, job_item->dst_bg_dim);
                    err_flg = 1;
                }

                if(width > mcp300_max_width){
                    if(TREAT_WARN_AS_ERR(dbg_mode)){
                        DEBUG_J(LOG_ERROR, engine, minor, "decoder: width in DST_BG_DIM (%d) > max width (%d), which results in memory waste\n", width, mcp300_max_width);
                        err_flg = 1;
                    }else{
                        DEBUG_M(LOG_ERROR, engine, minor, "decoder: width in DST_BG_DIM (%d) > max width (%d), which results in memory waste\n", width, mcp300_max_width);
                    }
                }
                
                job_item->Y_sz = width * height;
                parsed_flg |= 1 << 4;
                break;
            default:
                break;
        }
        i++;
    }


    job_item->bs_addr_va = job->in_buf.buf[0].addr_va;
    job_item->bs_addr_pa = job->in_buf.buf[0].addr_pa;
    job_item->bs_size = job_item->in_size;

#if REV_PLAY
    if(job_item->direct)
        memcpy(&job_item->gop_size, (void *)(job_item->bs_addr_va + job_item->bs_size), 4);
    else
        job_item->gop_size = 0;
#endif

    /* force sub_yuv disabled if dst_fmt is inocrrect */
    if(job_item->sub_yuv_en && job_item->dst_fmt != TYPE_YUV422_RATIO_FRAME){
        job_item->sub_yuv_en = 0;
    }

    if(parsed_flg != ((1 << 5) - 1)){
        DEBUG_J(LOG_ERROR, engine, minor, "decoder: missing required input property: %08X\n", parsed_flg);
        job_item->in_property_parsed_flg = -1; /* mark as error */
        return -1;
    }

    job_item->in_property_parsed_flg = parsed_flg;

    if(err_flg){
        return -1;
    }
    
    if(print_prop)
        print_property(job_item, job->in_prop);

    return 0;
}


/*
 * set output property ID_SEQ/ID_POC (output order related property)
 */
int favcd_set_out_property_poc(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item)
{
    int ret = 0;
#ifdef ENABLE_POC_SEQ_PROP    
    struct video_entity_job_t *job = job_item->job;
    short poc = job_item->poc;
    int i;
    int set_poc_flg = 0;
    int set_seq_flg = 0;
    
    struct video_property_t *property = job->out_prop;
    for(i = 0; i < MAX_PROPERTYS; i++){
        if(property[i].id == ID_POC){
            if(property[i].value != poc){
                DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "poc inconsist:%d %d\n", property[i].value, poc);
            }
            property[i].value = poc;
            set_poc_flg = 1;
        } else if(property[i].id == ID_SEQ){
            property[i].value = dec_data->out_fr_cnt;
            set_seq_flg = 1;
        } else if(property[i].id == ID_NULL){
            if(set_poc_flg == 0)
                ret = -1;
            if(set_seq_flg == 0)
                ret = -1;
            break;
        }
    }
#endif /* ENABLE_POC_SEQ_PROP */

    return ret;
}


/*
 * Set job's output property
 */
int favcd_set_out_property(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item)
{
    int i = 0;
    struct video_entity_job_t *job = job_item->job;
    struct video_property_t *property = job->out_prop;
	FAVC_DEC_FRAME *apFrame = &dec_data->dec_frame.apFrame;
    unsigned int width;
    unsigned int height;


    if(apFrame->i8Valid <= 0){
        if(job_item->status != DRIVER_STATUS_FAIL){
            DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, 
                "(%d,%d) job %d seq:%d output poperty src invalid %d (use values of the previous job)\n", job_item->engine, job_item->minor, job_item->job_id, job_item->seq_num, apFrame->i8Valid);
#if HALT_AT_ERR
            printk("(%d,%d) job %d seq:%d output poperty src invalid %d (use values of the previous job)\n", job_item->engine, job_item->minor, job_item->job_id, job_item->seq_num, apFrame->i8Valid);
            //while(1);
#endif
        }
    }

#if REV_PLAY
    if(job_item->gop_job_item)
    {
        //struct favcd_gop_job_item_t *gop_job_item = job_item->gop_job_item;
        job_item->gop_job_item->dec_done[job_item->job_idx_in_gop] = 1;
    }
#endif

    job_item->is_idr_flg = apFrame->bIsIDR;

    /***************************************************************************
        <<<<<<<< Add YOUR Self code here >>>>>>>
     **************************************************************************/
    /* time_scale, unit_in_tick */
    dec_data->time_scale = apFrame->u32TimeScale;
    dec_data->unit_in_tick = apFrame->u32UnitsInTick;
        
    if(en_src_fr_prop){
        property[i].id = ID_SRC_FRAME_RATE;
        if(dec_data->unit_in_tick == 0){
            property[i].value = 30;
        }else{
            property[i].value = dec_data->time_scale / dec_data->unit_in_tick / 2;
        }
        i++;
    }

    #if 0
    property[i].id = ID_FPS_RATIO;
    property[i].value = 0x00010001;
    i++;
    #endif

    property[i].id = ID_DST_FMT;
    property[i].value = dec_data->dst_fmt;
    i++;

    property[i].id = ID_DST_XY;
    property[i].value = 0;
    i++;

    width = apFrame->u16FrameWidth;
    height = apFrame->u16FrameHeight;

    property[i].id = ID_DST_BG_DIM;
    property[i].value = (width << 16) | (height & 0xFFFF); /* non-cropped frame size */
    i++;

    /* cut corpped size off */
    width -= apFrame->u16CroppingLeft;
    width -= apFrame->u16CroppingRight;
    height -= apFrame->u16CroppingTop;
    height -= apFrame->u16CroppingBottom;

    job_item->dst_dim = dec_data->dst_dim = (width << 16) | (height & 0xFFFF);
    property[i].id = ID_DST_DIM;
    property[i].value = dec_data->dst_dim; /* cropped frame size */
    i++;

    property[i].id = ID_SUB_YUV;
    property[i].value = apFrame->bHasScaledFrame;
    i++;

    property[i].id = ID_SRC_INTERLACE; /* frame coded or field coded */
    dec_data->src_interlace = apFrame->bFieldCoding;
    property[i].value = dec_data->src_interlace;
    i++;

    property[i].id = ID_SRC_FMT;
    property[i].value = TYPE_BITSTREAM_H264;
    i++;


    job_item->poc = apFrame->i16POC;
#ifdef ENABLE_POC_SEQ_PROP
    property[i].id = ID_POC;
    property[i].value = apFrame->i16POC;
    i++;
    
    property[i].id = ID_SEQ;
    property[i].value = 0;
    i++;
#endif //ENABLE_POC_SEQ_PROP
    
    property[i].id = ID_NULL;
    property[i].value = 0;
    i++;

    if(print_prop)
        print_property(job_item, job->out_prop);
    
    return 1;
}

/*
 * end of functions for process job's property
 */




/*
 * record engine starting time for performance profiling
 */
void mark_engine_start(int engine)
{
    struct utilization_record_t *util = &eng_util[engine];
    unsigned long flags;
    unsigned int curr_time = 0;
    
    if(utilization_period == 0)
        return;
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); // to protect eng_util
    
    curr_time = FAVCD_JIFFIES;
    DEBUG_M(LOG_INFO, engine, 0, "(%d) mark engine start %04X %u\n", engine, (unsigned int)(jiffies & 0xFFFF), curr_time);

    if (util->engine_start != 0){
        DEBUG_M(LOG_ERROR, engine, 0, "Warning to nested use dev %d mark_engine_start function!\n", engine);
    }
    
    //PERF_MSG("mark_engine_inv %d <\n", engine);
    PERF_MSG("mark_engine %d >\n", engine);
    util->engine_start = curr_time;
    util->engine_end = 0;
    if (util->utilization_start == 0) {
        util->utilization_start = util->engine_start;
        util->engine_time = 0;
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); // to protect eng_util
    
}


/*
 * record engine ending time for performance profiling
 */
void mark_engine_finish(int engine)
{
    struct utilization_record_t *util = &eng_util[engine];
    unsigned long flags;
    unsigned int curr_time = 0;

    if(utilization_period == 0)
        return;

    favcd_spin_lock_irqsave(&favc_dec_lock, flags); // to protect eng_util
    curr_time = FAVCD_JIFFIES;
    DEBUG_M(LOG_INFO, engine, 0, "(%d) mark engine finish %04X %u\n", engine, (unsigned int)(jiffies & 0xFFFF), curr_time);
    //PERF_MSG("mark_engine_inv %d >\n", engine);
        
    if (util->engine_end != 0){
        DEBUG_M(LOG_ERROR, engine, 0, "(%d) Warning to nested use engine %d mark_engine_finish function!\n", engine, engine);
    }

    
    PERF_MSG("mark_engine %d <\n", engine);
    /* accumulate engine running time */
    util->engine_end = curr_time;
    if (util->engine_end > util->engine_start)
        util->engine_time += util->engine_end - util->engine_start;

    /* caculate engine utilization within a utilization_period-seconds time */
    if (util->utilization_start > util->engine_end) {
        /* exception: engine aborted at the last time or jiffies overflow */
        util->utilization_start = 0;
        util->engine_time = 0;
    } else if ((util->utilization_start <= util->engine_end) &&
            (util->engine_end - util->utilization_start >= utilization_period*FAVCD_HZ)) {
        /* a utilization measurment period has passed, caculate the engine utilization in this period */
        unsigned int utilization;
        utilization = (unsigned int)((100 * util->engine_time) / (util->engine_end - util->utilization_start));
        if (utilization)
            util->utilization_record = utilization;

        /* clear 'measurement start time' and 'engine running time' for the next measurement */
        util->utilization_start = 0;
        util->engine_time = 0;
    }
    /* clear engine start time to indicate a running time measurement is done (it is ok to do the next measurement) */
    util->engine_start = 0;
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); // to protect eng_util

}



#define __PRIVATE_FUNCTION__



STATIC int __favcd_add_job_of_buf_to_callback(struct favcd_data_t *dec_data, int buf_idx)
{
    struct favcd_job_item_t *job_item;
    unsigned long flags;
    int output_flg;
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail_list */
    
    job_item = favcd_get_linked_job_of_buf(dec_data, buf_idx);
    if(job_item == NULL){
        DEBUG_M(LOG_ERROR, dec_data->engine, 0, "no job is recorded for idx: %d\n", buf_idx);
        favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list */
        return -1;
    }

    /* santy check 1: job status */
    if(job_item->status == DRIVER_STATUS_WAIT_POC){ /* normal path */
        SET_JOB_STATUS(job_item, DRIVER_STATUS_KEEP);
    }else if(job_item->status == DRIVER_STATUS_FAIL){ /* error handling path */
        DEBUG_M(LOG_WARNING, dec_data->engine, 0, "got poc for job that has error: job:%d buf:%d st:%d\n", job_item->job_id, buf_idx, job_item->status);
        if(!list_empty(&job_item->fail_list)){
            DEBUG_M(LOG_ERROR, dec_data->engine, 0, "got poc for job that has error and added to fail_list: job:%d buf:%d st:%d\n", job_item->job_id, buf_idx, job_item->status);
#if 1
            /* remove it from fail_list */
            __favcd_list_del(&job_item->fail_list);
            __favcd_init_list_head(&job_item->fail_list);
			fail_list_idx[job_item->minor]--;
            goto err_ret;
#endif
        }
    }else{
        DEBUG_M(LOG_ERROR, dec_data->engine, 0, "got poc for job that is not waiting poc: job:%d buf:%d st:%d\n", job_item->job_id, buf_idx, job_item->status);
        goto err_ret;
    }

    /* santy check 2: check if job already added to callback list before */
    if(!list_empty(&job_item->callback_list)){
        DEBUG_M(LOG_ERROR, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d poc:%d already added to callback list: %d\n", 
            job_item->engine, job_item->minor, job_item->job_id, buf_idx, job_item->poc, dec_data->dec_buf[buf_idx].is_outputed);
        goto err_ret;
    }

    /* job status is DRIVER_STATUS_WAIT_POC */
    SET_JOB_STATUS(job_item, DRIVER_STATUS_KEEP);

    
    /* job is not yet added to callback list */
#if REV_PLAY
    /* for gop_job_item, defer adding job to callback list to __favcd_process_gop_output_list() */
    if(job_item->gop_job_item){
        struct favcd_gop_job_item_t *gop_job_item;
        gop_job_item = job_item->gop_job_item;
        gop_job_item->got_poc[job_item->job_idx_in_gop] = 1;
        if(gop_job_item->output_tbl_idx < gop_job_item->gop_size){
            gop_job_item->output_tbl[gop_job_item->output_tbl_idx++] = job_item->bs_idx;
        }
        output_flg = 0; /* callback latter */
    }else{
        output_flg = 1;
        
    }
#else
    output_flg = 1;
#endif

    if(output_flg){
        favcd_set_out_property_poc(dec_data, job_item);
        dec_data->out_fr_cnt++;
        dec_data->dec_buf[buf_idx].is_outputed = 1;
        __favcd_list_add_tail(&job_item->callback_list, &favcd_callback_head);
        DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d added to callback list\n", 
            job_item->engine, job_item->minor, job_item->job_id, buf_idx);
    }else{
        DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d not added to callback list\n", 
            job_item->engine, job_item->minor, job_item->job_id, buf_idx);
    }
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list */
    
    return 0;

err_ret:
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list */
    return -1;
}


/*
 * Mark buffer as released
 */
int __favcd_mark_buf_released(struct favcd_data_t *dec_data, int buf_idx)
{
    if(dec_data->dec_buf[buf_idx].is_used == 0){
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "mark an unused buffer as released. idx: %d \n", buf_idx);
        favcd_print_dec_buf(dec_data);
        FAVCD_DAMNIT("mark an unused buffer as released. idx: %d \n", buf_idx);
        return 1;
    }
    if(dec_data->dec_buf[buf_idx].is_released){
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "mark a released buffer as released. idx: %d \n", buf_idx);
        FAVCD_DAMNIT("mark a released buffer as released. idx: %d \n", buf_idx);
        return 1;
    }
    DUMP_MSG(D_VG_BUF, dec_data->dec_handle, "(%d,%d) mark buf released %d\n", dec_data->engine, dec_data->minor, buf_idx);
    dec_data->dec_buf[buf_idx].is_released = 1;
    //dec_data->dec_buf[buf_idx].job_item->released_flg = 1;
    return 0;
}


STATIC void __favcd_append_output_list(struct favcd_data_t *dec_data, unsigned char buf_idx)
{
    dec_data->dec_frame.apFrame.mOutputFrame[dec_data->dec_frame.apFrame.u8OutputPicNum++].i8BufferIndex = buf_idx;
}


STATIC void __favcd_append_release_list(struct favcd_data_t *dec_data, unsigned char buf_idx)
{
    dec_data->dec_frame.u8ReleaseBuffer[dec_data->dec_frame.u8ReleaseBufferNum++] = buf_idx;
}


/*
 * mark buffers in release list as released
 */
STATIC int __favcd_process_release_list(struct favcd_data_t *dec_data)
{
    int i;
    int buf_idx;
    unsigned char *buf_idx_array = dec_data->dec_frame.u8ReleaseBuffer;
    unsigned char buf_num = dec_data->dec_frame.u8ReleaseBufferNum;
#if REV_PLAY
    struct favcd_job_item_t *job_item;
    struct favcd_gop_job_item_t *gop_job_item;
#endif

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "mark ReleaseBufferNum:%d\n", buf_num);
    for(i = 0; i < buf_num; i++){
        buf_idx = buf_idx_array[i];
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "[%d] idx: %d\n", i, buf_idx);
        
#if REV_PLAY
        job_item = dec_data->dec_buf[buf_idx].job_item;
        gop_job_item = job_item->gop_job_item;
        if(gop_job_item == NULL){
            /* non gop jobs */
            __favcd_mark_buf_released(dec_data, buf_idx);
        }else{
            /* gop jobs */
#if REALLOC_BUF_FOR_GOP_JOB
            //if(gop_job_item->is_reserved[job_item->bs_idx] == 0)
            if(1) /* always released */
#else
            if(gop_job_item->is_reserved[job_item->bs_idx] == 0 && gop_job_item->is_outputed[job_item->job_idx_in_gop] == 1)
#endif
            {
                __favcd_mark_buf_released(dec_data, buf_idx);
                gop_job_item->is_released[job_item->job_idx_in_gop] = 3;
                DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "mark job released %d\n", job_item->job_id);
            }else{
                /* do not release reserved jobs */
                gop_job_item->is_released[job_item->job_idx_in_gop] = 1;
                DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "skip reserved job %d\n", job_item->job_id);
            }
        }
#else
        __favcd_mark_buf_released(dec_data, buf_idx);
#endif
    }

#if REV_PLAY
    __favcd_process_gop_release_list(dec_data);
#endif

    return 0;
}
/*
 * put output buffer to callback list
 */
STATIC int __favcd_process_output_list(struct favcd_data_t *dec_data, int flush_flg)
{
    int i;
	FAVC_DEC_FRAME *apFrame = &dec_data->dec_frame.apFrame;
    
    DEBUG_M(LOG_TRACE, dec_data->engine,  dec_data->minor, "(%d,%d) __favcd_process_output_list out pic num:%d\n", 
        dec_data->engine,  dec_data->minor, apFrame->u8OutputPicNum);


    for(i = 0; i < apFrame->u8OutputPicNum; i++){
        int buf_idx;
        int poc;

        buf_idx = apFrame->mOutputFrame[i].i8BufferIndex;
        poc = apFrame->mOutputFrame[i].i16POC;
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "[%d] poc: %d idx: %d\n", i, poc, buf_idx);

        /* add job to callback_list as the order of POC */
        __favcd_add_job_of_buf_to_callback(dec_data, buf_idx);
    }

#if REV_PLAY
    __favcd_process_gop_output_list(dec_data, flush_flg);
#endif
    
    return 0;
}


/*
 * Handle the buffer that is relased but not yet outputed
 */
STATIC int __favcd_handle_unoutput_buffer(struct favcd_data_t *dec_data)
{
#if CHECK_UNOUTPUT
    int i;
    int output_cnt = 0;

    for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
        if(dec_data->dec_buf[i].is_used && dec_data->dec_buf[i].is_released && dec_data->dec_buf[i].is_started && (dec_data->dec_buf[i].is_outputed == 0)){
            DEBUG_M(LOG_ERROR, dec_data->engine,  dec_data->minor, "(%d,%d) __favcd_handle_unoutput_buffer buf_idx:%d\n", 
                dec_data->engine,  dec_data->minor, i);
            __favcd_add_job_of_buf_to_callback(dec_data, i);
            output_cnt++;
        }
    }

    if(output_cnt){
        if(TREAT_WARN_AS_ERR(dbg_mode)){
            FAVCD_DAMNIT("(%d,%d) __favcd_handle_unoutput_buffer output buf num:%d\n", dec_data->engine,  dec_data->minor, output_cnt);
        }
    }
#endif    
    return 0;
}


/*
 * Free all internally allocated buffer of a channel
 */
STATIC int __favcd_cleanup_dec_buf(struct favcd_data_t *dec_data)
{
    int i;
    int ret;
    unsigned long flags;
    BufferAddr buf_addr;
    
    for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
        if(dec_data->dec_buf[i].is_used){
            favcd_spin_lock_irqsave(&favc_dec_lock, flags);
            if(dec_data->dec_buf[i].is_released == 0){
                __favcd_mark_buf_released(dec_data, i);
            }
#if CHECK_UNOUTPUT
            if(dec_data->dec_buf[i].is_used && dec_data->dec_buf[i].is_released && 
               dec_data->dec_buf[i].is_started && (dec_data->dec_buf[i].is_outputed == 0)){
                DEBUG_M(LOG_ERROR, dec_data->engine,  dec_data->minor, "(%d,%d) __favcd_cleanup_dec_buf unoutput buf_idx:%d\n", 
                    dec_data->engine,  dec_data->minor, i);
                __favcd_add_job_of_buf_to_callback(dec_data, i);
            }
#endif
            ret = pop_dec_buffer(dec_data, &buf_addr, i);
            favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);
            if(ret){
                DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "{%d,%d) __favcd_cleanup_dec_buf pop_dec_buffer %d error\n", dec_data->engine, dec_data->minor, i);
                FAVCD_DAMNIT("{%d,%d) __favcd_cleanup_dec_buf pop_dec_buffer %d error\n", dec_data->engine, dec_data->minor, i);
                continue;
            }
            
            ret = free_dec_buffer(&buf_addr);
            if(ret){
                DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "{%d,%d) __favcd_cleanup_dec_buf free_dec_buffer %d error\n", dec_data->engine, dec_data->minor, i);
            }
        }
    }

    return 0;
}



/*
 * output all frames (kept by low level driver) of the channel and move them to callback list
 */
STATIC int __favcd_output_all_frame(struct favcd_data_t *dec_data)
{
    int ret;
    FAVC_DEC_FRAME_IOCTL *dec_frame = &dec_data->dec_frame;
    
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "{%d,%d) 0x%04X __favcd_output_all_frame: H264Dec_OutputAllPicture() calling\n", 
        dec_data->engine, dec_data->minor, ((unsigned int)current & 0xFFFF));
    ret = H264Dec_OutputAllPicture(dec_data->dec_handle, dec_frame, 1);
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "{%d,%d) 0x%04X __favcd_output_all_frame: H264Dec_OutputAllPicture() return %d\n", 
        dec_data->engine, dec_data->minor, ((unsigned int)current & 0xFFFF), ret);
    
    if(ret < 0){
        DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) 0x%04X __favcd_output_all_frame: H264Dec_OutputAllPicture() return error\n", 
            dec_data->engine, dec_data->minor, ((unsigned int)current & 0xFFFF));
        FAVCD_DAMNIT("H264Dec_OutputAllPicture() output frame error\n");
        return ret;
    }

    __favcd_process_output_list(dec_data, 1);
    __favcd_process_release_list(dec_data);
    __favcd_handle_unoutput_buffer(dec_data);

    return 0;
}


/*
 * handle decoding error (setting init flags and output buffers)
 */
STATIC int favcd_error_handling(struct favcd_data_t *dec_data, int err_num)
{
    if(err_handle == 1){
        /* reinit for all errors */
        dec_data->need_init_flg = INIT_DUE_TO_ERR;
    }else{
        /* reinit for some errors only */
        switch(err_num){
            /* list of ignored errors */
            case H264D_ERR_VLD_ERROR:
                /* NOTE: VLD error seems acceptable */
            case H264D_ERR_BS_EMPTY:
                break;
                
            default:
                dec_data->need_init_flg = INIT_DUE_TO_ERR;
                break;
        }
        //dec_data->need_init_flg = INIT_NORMAL; 
    }
    
    if(dec_data->need_init_flg != INIT_DONE){
        /*  reinit is not enought. dec_buf and job_item needs to be cleaned up too */
        __favcd_output_all_frame(dec_data);
    }

    return 0;
}


STATIC int __favcd_job_init(struct favcd_job_item_t *job_item)
{
    /* clear flags */
    job_item->callback_flg = 0;
    job_item->released_flg = 1; /* 1: not stored in DPB */
    job_item->stored_flg = 0;
    job_item->prepared_flg = 0;
    job_item->removed_flg = 0;
    
    job_item->buf_idx = -1;
    job_item->res_out_buf = NULL;
    job_item->res_flg = 0;

    job_item->Y_sz = 0;
    job_item->mbinfo_dump_flg = 0;
    job_item->bs_dump_flg = 0;
    job_item->output_dump_flg = 0;
    
    job_item->in_property_parsed_flg = 0;
    job_item->addr_chk_flg = 0;
    job_item->is_idr_flg = 0;
    job_item->err_num = H264D_OK;
    job_item->err_pos = ERR_NONE;
    job_item->parser_offset = 0;

    job_item->poc = -1;

#if REV_PLAY
    job_item->direct = 0;
    job_item->gop_size = 0;
    
    job_item->gop_job_item = NULL;
    job_item->set_bs_flg = 0;
    job_item->bs_idx = -1;
    job_item->output_group_size = 0;
#endif

    __favcd_init_list_head(&job_item->global_list);
    __favcd_init_list_head(&job_item->minor_list);
    __favcd_init_list_head(&job_item->callback_list);
    __favcd_init_list_head(&job_item->fail_list);
    __favcd_init_list_head(&job_item->standby_list);
#if USE_SINGLE_PREP_LIST    
    __favcd_init_list_head(&job_item->prepare_list);
#endif
    __favcd_init_list_head(&job_item->prepare_minor_list);
    __favcd_init_list_head(&job_item->ready_list);

    /* init profiling info */
    //job_item->prof_put_time = 0;
    job_item->prof_prepare_time = 0;
    job_item->prof_ready_time = 0;
    job_item->prof_start_time = 0;
    job_item->prof_finish_time = 0;
    job_item->prof_callback_time = 0;
    
    return 0;
}


STATIC int __favcd_job_reserve_buf(struct favcd_job_item_t *job_item)
{
#if ENABLE_VIDEO_RESERVE
    if(job_item->res_flg){ /* error checking. This should never happen */
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) job %d __favcd_job_reserve_buf for a reserved job\n",
            job_item->engine, job_item->minor, job_item->job_id);
        FAVCD_DAMNIT("__favcd_job_reserve_buf for a reserved job\n");
    }
    
    DUMP_MSG(D_RES, private_data[job_item->minor].dec_handle, 
        "__video_reserve_buffer for job:%d seq:%d\n", job_item->job->id, job_item->seq_num);
    DEBUG_M(LOG_INFO, -1, -1, "calling video_reserve_buffer for job:%d buf:%08X\n", job_item->job->id, (unsigned int)&job_item->job->out_buf.buf[0].addr_pa);
    job_item->res_out_buf = __video_reserve_buffer(job_item->job, 1);
    if (job_item->res_out_buf == NULL)
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "Error to reserve this buffer, job_id(0x%x)\n",(int)job_item->job);
    job_item->res_flg = 1;
#endif // ENABLE_VIDEO_RESERVE
    return 0;
}

/*
 * release job's reserved output buffer and memory of job_item
 * NOTE: job_items should be removed from any list to avoid errors
 */
STATIC int __favcd_job_cleanup(struct favcd_job_item_t *job_item)
{
    int ret;
    /* release job item when:
     * 1. job's output buffer is not referenced
     * 2. job has been callback
     */
    if(job_item->released_flg && job_item->callback_flg) {
#if ENABLE_VIDEO_RESERVE
        if(job_item->res_flg) {
            DUMP_MSG(D_RES, private_data[job_item->minor].dec_handle, 
                "__video_free_buffer for job:%d seq:%d\n", job_item->job->id, job_item->seq_num);
            DEBUG_M(LOG_INFO, -1, -1, "calling video_free_buffer for job:%d buf:%08X\n", job_item->job->id, (unsigned int)&job_item->job->out_buf.buf[0].addr_pa);
            if(__video_free_buffer(job_item->res_out_buf) != 0)
            {
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) job %d video_free_buffer error\n",
                    job_item->engine, job_item->minor, job_item->job_id);
            }
            job_item->res_flg = 0;
			//printm(MODULE_NAME,"calling video_free_buffer for job:%d buf:%08X\n", job_item->job->id, (unsigned int)&job_item->job->out_buf.buf[0].addr_pa);
        }
#if 0
        else{ // !job_item->res_flg 
            //error checking. it happens when a job is fail
            DEBUG_M(LOG_WARNING, job_item->engine, job_item->minor, 
                "{%d,%d) job %d __favcd_job_cleanup for a non-reserved job\n",
                job_item->engine, job_item->minor, job_item->job_id);
            //FAVCD_DAMNIT();
        }
#endif
#endif // ENABLE_VIDEO_RESERVE

#if REV_PLAY
        if(job_item->gop_job_item){
            struct favcd_gop_job_item_t *gop_job_item = job_item->gop_job_item;
            gop_job_item->gop_jobs[job_item->job_idx_in_gop] = NULL;
            gop_job_item->clean_cnt++;
            
            if(gop_job_item->next_output_job_idx == -1 && gop_job_item->clean_cnt == gop_job_item->gop_size){
                DUMP_MSG(D_REV, private_data[job_item->minor].dec_handle, "(%d) freeing gop_job_item %d\n", job_item->minor, job_item->job_id);
                __favcd_list_del(&gop_job_item->gop_job_list);
                fkfree(gop_job_item);
                job_item->gop_job_item = NULL;
#if GOP_JOB_STOP_HANDLING
            }else if((gop_job_item->stop_flg || gop_job_item->err_flg) && (gop_job_item->clean_cnt == gop_job_item->job_cnt)){
                DUMP_MSG(D_REV, private_data[job_item->minor].dec_handle, "(%d) freeing gop_job_item %d at stopping: clean:%d job_cnt:%d\n", 
                    job_item->minor, job_item->job_id,
                    gop_job_item->clean_cnt, gop_job_item->job_cnt);
                __favcd_list_del(&gop_job_item->gop_job_list);
                fkfree(gop_job_item);
                job_item->gop_job_item = NULL;
#endif
            }
        }
#endif

        PERF_MSG("__free_job_item > %08X\n", (unsigned int)job_item);
        __free_job_item(job_item);
        PERF_MSG("__free_job_item < %08X\n", (unsigned int)job_item);
        return 3;
    }
    
    ret = (job_item->released_flg << 1) | (job_item->callback_flg);
    return ret; //job_item is not released
}


STATIC struct video_entity_job_t *__favcd_set_callback_job_status(struct favcd_job_item_t *job_item)
{
    struct video_entity_job_t *job;

    job = job_item->job;
    if(job == NULL){
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) job %d has null job pointer\n", job_item->engine, job_item->minor, job_item->job_id);
        return NULL;
    }

    if(job_item->status == DRIVER_STATUS_FAIL || job_item->err_num != H264D_OK){
        job->status = JOB_STATUS_FAIL;
    } else if (job_item->status == DRIVER_STATUS_FINISH || job_item->status == DRIVER_STATUS_KEEP) {
        job->status = JOB_STATUS_FINISH;
    } else {
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) job %d status %d callback incorrect\n", job_item->engine, job_item->minor, job_item->job_id, job_item->status);
        return NULL;
    }
    return job;
}


/*
 * remove job from all lists
 * NOTE: must be called when IRQ is disabled
 */
STATIC void __favcd_remove_job_from_all_list(struct favcd_job_item_t *job_item)
{
    if(job_item->removed_flg){
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "job already removed: seq: %d\n", job_item->seq_num);
        FAVCD_DAMNIT("job already removed: seq: %d\n", job_item->seq_num);
        return;
    }

    if(!list_empty(&job_item->global_list)){
        __favcd_list_del(&job_item->global_list);
        __favcd_init_list_head(&job_item->global_list);
    }
    
    if(!list_empty(&job_item->minor_list)){
        __favcd_list_del(&job_item->minor_list);
        __favcd_init_list_head(&job_item->minor_list);
    }
    
    if(!list_empty(&job_item->callback_list)){
        __favcd_list_del(&job_item->callback_list);
        __favcd_init_list_head(&job_item->callback_list);
    }

    if(!list_empty(&job_item->fail_list)){
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) __favcd_job_callback remove job %d from fail_list\n", job_item->engine, job_item->minor, job_item->job_id);
        __favcd_list_del(&job_item->fail_list);
        __favcd_init_list_head(&job_item->fail_list);
		fail_list_idx[job_item->minor]--;
    }

    if(!list_empty(&job_item->ready_list)){
        struct favcd_data_t *dec_data;
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) __favcd_job_callback remove job %d from ready_list\n", job_item->engine, job_item->minor, job_item->job_id);
        __favcd_list_del(&job_item->ready_list);
        __favcd_init_list_head(&job_item->ready_list);
        dec_data = private_data + job_item->minor;
        dec_data->ready_job_cnt--;
    }

    if(!list_empty(&job_item->standby_list)){
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) __favcd_job_callback remove job %d from standby_list\n", job_item->engine, job_item->minor, job_item->job_id);
        __favcd_list_del(&job_item->standby_list);
    }

#if USE_SINGLE_PREP_LIST    
    if(!list_empty(&job_item->prepare_list)){
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) __favcd_job_callback remove job %d from prepare_list\n", job_item->engine, job_item->minor, job_item->job_id);
        __favcd_list_del(&job_item->prepare_list);
    }
#endif
    
    if(!list_empty(&job_item->prepare_minor_list)){
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) __favcd_job_callback remove job %d from prepare_minor_list\n", job_item->engine, job_item->minor, job_item->job_id);
        __favcd_list_del(&job_item->prepare_minor_list);
    }

    job_item->removed_flg = 1;

}



static void set_job_item_prof_record(struct favcd_job_item_t *job_item, struct job_prof_record *job_prof_rec)
{
    int i;
    struct video_entity_job_t *job = job_item->job;
    struct video_property_t *property = job->out_prop;
    int max_prop = sizeof(job->out_prop) / sizeof(job->out_prop[0]);
    int prop_found = 0;

    
    job_prof_rec->put_period   = job_item->prof_put_period;
    job_prof_rec->put_time     = job_item->prof_put_time;
    job_prof_rec->prepare_time = job_item->prof_prepare_time;
    job_prof_rec->ready_time   = job_item->prof_ready_time;
    job_prof_rec->start_time   = job_item->prof_start_time;
    job_prof_rec->finish_time  = job_item->prof_finish_time;
    job_prof_rec->callback_time = job_item->prof_callback_time;
    job_prof_rec->err_num = job_item->err_num;
    job_prof_rec->seq_num = job_item->seq_num;
    job_prof_rec->cb_seq_num = job_item->cb_seq_num;
    job_prof_rec->minor  = job_item->minor;
    job_prof_rec->engine = job_item->engine;

    job_prof_rec->poc = job_item->poc;
    job_prof_rec->dst_dim = job_item->dst_dim;
    job_prof_rec->dst_bg_dim = job_item->dst_bg_dim;
    job_prof_rec->in_size = job_item->bs_size;
    job_prof_rec->is_idr_flg = job_item->is_idr_flg;

    job_prof_rec->flags = 0;
    job_prof_rec->dst_dim_non_cropped = 0;
    for(i = 0; i < max_prop; i++){
        if(property[i].id == ID_NULL)
            break;
        if(property[i].id == ID_DST_BG_DIM){
            job_prof_rec->dst_dim_non_cropped = property[i].value;
            prop_found |= (1 << 0);
        }else if(property[i].id == ID_SUB_YUV){
            if(property[i].value)
                job_prof_rec->flags |= JOB_ITEM_FLAGS_SUB_YUV_EN;
            prop_found |= (1 << 1);
        }else if(property[i].id == ID_SRC_INTERLACE){
            if(property[i].value)
                job_prof_rec->flags |= JOB_ITEM_FLAGS_SRC_INTERLACE_EN;
            prop_found |= (1 << 2);
        }

        /* skip the loop when the intrested prop has been got */
        if(prop_found == 0x7)
            break;
    }

    //printk("i:%d prop_found:%d max_prop:%d\n", i, prop_found, max_prop);
    
}


void callback_scheduler(void)
{
	int i;
    int engine;
    int minor;
    unsigned long flags;
    struct favcd_job_item_t *job_item;
    struct favcd_job_item_t *job_item_next;
    int idx;
    int ret;
    struct favcd_data_t *dec_data;
    struct video_entity_job_t *cb_job;

    /* for profiling */
    //static unsigned int cb_sch_period = 0;
    static unsigned int cb_sch_last_start = 0;
    unsigned int  cb_sch_period;
    
    unsigned int  stop_chn_cnt = 0;
    unsigned int  cb_job_cnt = 0;

    
    unsigned int  callback_cnt = 0;
    unsigned int  minor_job_cnt = 0;
    unsigned int  minor_job_add_cnt = 0;
    
    unsigned int  cb_sch_start;
    unsigned int  cb_sch_end;
    unsigned int  cb_sch_dur;

    unsigned int  scan_list_start;
    unsigned int  scan_list_end;
    unsigned int  scan_list_dur = 0;
    
    unsigned int  cb_job_start = 0;
    unsigned int  cb_job_end = 0;
    unsigned int  cb_job_dur = 0;
    
    unsigned int  cb_job_avg_dur = 0;
    unsigned int  cb_rec_idx;

#if 0
	unsigned int  callback_chn[MAX_CHN_NUM];	
	unsigned int  callback_chn_c = 0;
#endif

    cb_rec_idx = cb_prof_data.cb_sch_cnt % CB_PROF_RECORD_NUM;
    cb_prof_data.cb_sch_cnt++;
    
    cb_sch_start = _FAVCD_JIFFIES;
    cb_sch_period = cb_sch_start - cb_sch_last_start;
    cb_sch_last_start = cb_sch_start;


    PERF_MSG("callback_sch >\n");
    DEBUG_M(LOG_INFO, 0, 0, " callback_scheduler enter\n");

    /* save bitstream after damnit */
    if(SAVE_BS_AT_ERR(dbg_mode) && save_err_bs_flg)
    {
        favcd_save_job_bitstream_to_file();
        save_err_bs_flg = 0;
    }


    /* the stop handler at the end of callback_scheduler will handle stopping 
     * according to the captured stopping flag 
     * (WHY? to avoid race between callback_scheduler and stop)
     */

    for (minor = 0; minor < chn_num; minor++) {
        idx = minor;
        dec_data = &(private_data[idx]);
        engine = dec_data->engine;

        /* if the channel is stopping, skip it until the current job is done 
         * and then output all frames to be outputed if this channel is stopping */
        if(dec_data->stopping_flg){
#if REV_PLAY
            struct favcd_gop_job_item_t *gop_job_item;
#endif

            /* wait until no ready job and no ongoing/current job */
            if((dec_data->ready_job_cnt != 0) || (dec_data->curr_job != NULL))
            {
                if(dec_data->curr_job != NULL){
                    DEBUG_M(LOG_WARNING, engine, minor, "(%d, %d) callback_scheduler stopping, wait until current job %d status %d not ongoing\n",
                        engine, minor, dec_data->curr_job->job_id, dec_data->curr_job->status);
                }
                if(dec_data->ready_job_cnt != 0){
                    DEBUG_M(LOG_WARNING, engine, minor, "(%d, %d) callback_scheduler stopping, wait until zero ready job %d\n",
                        engine, minor, dec_data->ready_job_cnt);
                }
                /* skip the handling to stop of this channel since there are still ongoing job */
                //dec_data->handling_stop_flg = 0; 
                continue;
            }
            
#if REV_PLAY
#if GOP_JOB_STOP_HANDLING
            /* wait until no ready gop job and no ongoing/current gop job */
            //DEBUG_M(LOG_REV_PLAY, 0, 0, "{%d,%d) __favcd_remove_standby_jobs set stop_flg via favcd_gop_minor_head\n", dec_data->engine, dec_data->minor);
            list_for_each_entry (gop_job_item, &favcd_gop_minor_head[idx], gop_job_list) {
                if(gop_job_item->pp_rdy_job_cnt || gop_job_item->ongoing_flg){
                    continue;
                }
                //gop_job_item->stop_flg = 1;
                //DEBUG_M(LOG_REV_PLAY, 0, 0, "{%d,%d) __favcd_remove_standby_jobs set stop_flg via favcd_gop_minor_head gop id:%08X\n", 
                //    dec_data->engine, dec_data->minor, gop_job_item->gop_id);
            }
#endif
#endif
			if(dec_data->dec_handle == NULL){
                DEBUG_M(LOG_WARNING, engine, minor, "(%d, %d) callback_scheduler stopping for an unused channel\n", engine, minor);
                continue;
            }

            DEBUG_M(LOG_WARNING, engine, minor, "(%d, %d) callback_scheduler will handle stopping for channel %d\n", engine, minor, dec_data->minor);
            dec_data->handling_stop_flg = 1;

            /* NOTE: At here, the channel is stopping and no on-going job is running */
            /* get all buffers to be ouputed */
            DEBUG_M(LOG_INFO, engine, minor, "(%d, %d) callback_scheduler __favcd_output_all_frame %d\n", engine, minor, dec_data->minor);
			__favcd_output_all_frame(dec_data);
            stop_chn_cnt++;
        }

		

        DRV_LOCK_G(); /* lock for minor list */
        favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for callback list */
		//printk("fail_list_idx = %d %d\n",minor,fail_list_idx[minor]);
		if ((fail_list_idx[minor] > 0) || (record_fail_list_chn == 0)) {
		scan_list_start = _FAVCD_JIFFIES;

#if 0 // for debug only
        /* scan through the fail list and print all job_item */
        __CHK_LIST_HEAD(&favcd_fail_minor_head[idx]);

        minor_job_cnt = 0;
        list_for_each_entry_safe(job_item, job_item_next, &favcd_fail_minor_head[idx], fail_list)
        //list_for_each_entry(job_item, &favcd_fail_minor_head[dec_data->minor], fail_list)
        {
            minor_job_cnt++;
            DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d from fail list preprocessing: minor_job_cnt %d a\n", 
                job_item->engine, job_item->minor, job_item->job_id, job_item->buf_idx, minor_job_cnt);

            DEBUG_M(LOG_INFO, dec_data->engine, 0, "h:%08X p:%08X n:%08X\n", 
                (unsigned int)&job_item->fail_list,
                (unsigned int)job_item->fail_list.prev,
                (unsigned int)job_item->fail_list.next);
#if 1
            if(minor_job_cnt == 1){
                //__favcd_check_list_error(entry, 3, entry_name, func_name);
                if(favcd_fail_minor_head[idx].next != &job_item->fail_list){
                    DEBUG_M(LOG_ERROR, dec_data->engine, 0, "error type 1: h->n %08X != j %08X\n", 
                        (unsigned int)favcd_fail_minor_head[idx].next,
                        (unsigned int)&job_item->fail_list);
                }

                if(&favcd_fail_minor_head[idx] != job_item->fail_list.prev){
                    DEBUG_M(LOG_ERROR, dec_data->engine, 0, "error type 2: h %08X != j->p %08X\n", 
                        (unsigned int)&favcd_fail_minor_head[idx],
                        (unsigned int)job_item->fail_list.prev);
                }
            }
#endif
        }
        minor_job_cnt = 0;
#endif

        /* scan through the fail list and add jobs to callback list */
        list_for_each_entry_safe(job_item, job_item_next, &favcd_fail_minor_head[idx], fail_list)
        {
            minor_job_cnt++;

            DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d from fail list processing: minor_job_cnt %d x list_empty:%d\n", 
                job_item->engine, job_item->minor, job_item->job_id, job_item->buf_idx, minor_job_cnt, list_empty(&favcd_fail_minor_head[idx]));
            
#if 0
            DEBUG_M(LOG_INFO, dec_data->engine, 0, "list_empty:%d h:%08X p:%08X n:%08X\n", list_empty(&favcd_fail_minor_head[idx]), 
                (unsigned int)&favcd_fail_minor_head[idx],
                (unsigned int)favcd_fail_minor_head[idx].prev,
                (unsigned int)favcd_fail_minor_head[idx].next);
#endif

            __favcd_list_del(&job_item->fail_list);
            __favcd_init_list_head(&job_item->fail_list);
            if(!list_empty(&job_item->callback_list)){ 
                DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) job %d status %d in fail list should not be added into callback list before: minor_job_cnt %d\n",
                    job_item->engine, job_item->minor, job_item->job_id, job_item->status, minor_job_cnt);
                FAVCD_DAMNIT("job %d status %d in fail list should not be added into callback list before\n", job_item->job_id, job_item->status);
                continue; 
            }
            __favcd_list_add_tail(&job_item->callback_list, &favcd_callback_head);
            DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) job %d linked to buf %d moved from fail list to callback list: minor_job_cnt %d\n", 
                job_item->engine, job_item->minor, job_item->job_id, job_item->buf_idx, minor_job_cnt);

            minor_job_add_cnt++;
        }

        scan_list_end = _FAVCD_JIFFIES;
        scan_list_dur += (scan_list_end - scan_list_start);		
		fail_list_idx[minor] = 0;
		}
		favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for callback list */
        DRV_UNLOCK_G(); /* unlock for minor list */
		
    }

#if 0
	favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for callback list, dec_data */
	list_for_each_entry_safe(job_item, job_item_next, &favcd_callback_head, callback_list)
    {
    	int hit = 0;
    	for (minor = 0; minor < callback_chn_c; minor++)
    		if (callback_chn[minor] == job_item->minor) {
				hit = 1;
				break;
			}

		if (hit == 0) {
			callback_chn[callback_chn_c] = job_item->minor;
			callback_chn_c++;	
		}
	}
	favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);

	for (minor = 0; minor < callback_chn_c; minor++) {
		idx = callback_chn[minor];
		dec_data = &(private_data[idx]);

		/* release buffers for this channel */
        if(dec_data->used_buffer_num){
            for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
                int rel_flg = 0;
                BufferAddr buf_addr;
				
                ret = 0;
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); // lock for dec_data->dec_buf
                if(dec_data->dec_buf[i].is_used && dec_data->dec_buf[i].is_released){
                    ret = pop_dec_buffer(dec_data, &buf_addr, i);
                    rel_flg = 1;
                }
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);

                if(ret){
                    FAVCD_DAMNIT("callback_scheduler pop_dec_buffer %d error\n", i);
                }
                
                if(rel_flg && ret == 0){
                    ret = free_dec_buffer(&buf_addr);
		            if(ret){
                        DEBUG_M(LOG_ERROR, 0, 0, "callback_scheduler free_dec_buffer %d error\n", i);
                        FAVCD_DAMNIT("callback_scheduler free_dec_buffer %d error\n", i);
                    }else{
                        DEBUG_M(LOG_TRACE, 0, 0, "callback_scheduler free_dec_buffer %d done\n", i);
                    }					
						   buf_addr.job_item->job->id, (unsigned int)&(buf_addr.job_item->job->out_buf.buf[0].addr_pa));
                } 
            }
        }
	}
#endif

    DRV_LOCK_G(); /* lock for engine/minor list */
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for callback list, dec_data */

    /* scan through the callback list of each engine/channel */
    list_for_each_entry_safe(job_item, job_item_next, &favcd_callback_head, callback_list)
    {
    	int cb_minor;
        unsigned int curr_callback_time;
        callback_cnt++;

        curr_callback_time = jiffies;
        DEBUG_M(LOG_INFO, engine, minor, "{%d,%d) job %d seq:%d "
#if REV_PLAY
            "bs_idx:%d "
#endif
            "status %d callback (period:%d)\n", 
            job_item->engine, job_item->minor, job_item->job_id, job_item->seq_num,
#if REV_PLAY
            job_item->bs_idx,
#endif
            job_item->status, curr_callback_time - private_data[job_item->minor].last_callback_time);
        private_data[job_item->minor].last_callback_time = curr_callback_time;
        
        PERF_MSG("callback_job >\n");

		cb_minor = job_item->minor;
        cb_job = __favcd_set_callback_job_status(job_item);
        if(cb_job == NULL){
            /* job pointer is null or job_item status is incorrect, stop the callback process */
            /* NOTE: this should never happen */
            if(DAMNIT_AT_ERR(dbg_mode)){
                FAVCD_DAMNIT("job %d status %d incorrect at callback or has null job pointer: %p\n", job_item->job_id, job_item->status, job_item->job);
            }
            break;
        }
        __favcd_remove_job_from_all_list(job_item);

        favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for callback list, dec_data */

        /* dump data to file if necessary */
        favcd_save_mbinfo_to_file(job_item);
        favcd_save_output_to_file(job_item);
        favcd_record_bitstream_to_file(job_item);
        favcd_copy_recorded_file(job_item);
        
        /* reserve the buffer of this job before callback */
        if(job_item->released_flg == 0)
		{
            __favcd_job_reserve_buf(job_item);
        }

        /* check job status before callback */
        if(cb_job->status != JOB_STATUS_FAIL && cb_job->status != JOB_STATUS_FINISH){
            DEBUG_M(LOG_ERROR, 0, 0, "job %d incorrect status at callback: %d\n", cb_job->id, cb_job->status);
            if(DAMNIT_AT_ERR(dbg_mode)){
                FAVCD_DAMNIT("job %d incorrect status at callback: %d\n", cb_job->id, cb_job->status);
            }
        }

        if(PRINT_ERR_IN_ISR(dbg_mode) == 0){
            favcd_print_job_error(job_item);
        }

        /* print job error to master */
        if(PRINT_ERR_TO_MASTER(dbg_mode)){
            if(favcd_err_to_master(job_item->err_num)){
                master_print("decoder: %s(job id:%d seq:%d chn:%d err:%d)\n", favcd_err_str(job_item->err_num), 
                    job_item->job_id, job_item->seq_num, job_item->minor, job_item->err_num);
            }
        }

#if FREE_BUF_CB
		dec_data = &(private_data[cb_minor]);

		/* release buffers for this channel */
		if(dec_data->used_buffer_num){
			for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
				int rel_flg = 0;
				BufferAddr buf_addr;
				
				ret = 0;
				favcd_spin_lock_irqsave(&favc_dec_lock, flags); // lock for dec_data->dec_buf
				if(dec_data->dec_buf[i].is_used && dec_data->dec_buf[i].is_released){
					ret = pop_dec_buffer(dec_data, &buf_addr, i);
					rel_flg = 1;
				}
				favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);

				if(ret){
					FAVCD_DAMNIT("callback_scheduler pop_dec_buffer %d error\n", i);
				}
				
				if(rel_flg && ret == 0){
					ret = free_dec_buffer(&buf_addr);
					if(ret){
						DEBUG_M(LOG_ERROR, 0, 0, "callback_scheduler free_dec_buffer %d error\n", i);
						FAVCD_DAMNIT("callback_scheduler free_dec_buffer %d error\n", i);
					}else{
						DEBUG_M(LOG_TRACE, 0, 0, "callback_scheduler free_dec_buffer %d done\n", i);
					}					
				} 
			}
		}
#endif
		
        if(cb_job->callback == NULL){
            DEBUG_M(LOG_ERROR, 0, 0, "job %d has null callback pointer\n", cb_job->id);
            DEBUG_M(LOG_ERROR, 0, 0, "dumpping job ptr:%p  size:%d\n", cb_job, sizeof(*cb_job));
            favcd_dump_mem(cb_job, sizeof(*cb_job));
            
            DEBUG_J(LOG_ERROR, 0, 0, "decoder: job %d has null callback pointer: %p\n", cb_job->id, cb_job->callback);
            if(DAMNIT_AT_ERR(dbg_mode)){
                FAVCD_DAMNIT("decoder: job %d has null callback pointer: %p\n", cb_job->id, cb_job->callback);
            }
        }else{
            cb_job_start = _FAVCD_JIFFIES;
            cb_job->callback(cb_job);
            cb_job_end = _FAVCD_JIFFIES;
        }

        TRACE_JOB('C');
        
        favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for callback list, dec_data */
        if(rec_prop == 0){
            favcd_record_in_property(&private_data[job_item->minor], job_item);
        }else if(rec_prop == 1){
            favcd_record_all_property(&private_data[job_item->minor], job_item);
        }

        job_item->prof_callback_time = _FAVCD_JIFFIES;
        job_item->cb_seq_num = cb_seq_num++;
        set_job_item_prof_record(job_item, &job_prof_data.job_prof_record[job_item->cb_seq_num % JOB_PROF_RECORD_NUM]);
        job_prof_data.job_cnt++;
        
        cb_job_dur += (cb_job_end - cb_job_start);
        cb_job_cnt++;

        job_item->callback_flg = 1; /* mark callback flag */

        /* release job_item and its buffer */
        if(job_item->released_flg){
            ret = __favcd_job_cleanup(job_item);
            if(ret != 3){
                DEBUG_M(LOG_WARNING, job_item->engine, job_item->minor, "{%d,%d) job %d callback __favcd_job_cleanup error: ret: %d\n", job_item->engine, job_item->minor, job_item->job_id, ret);
            }else{
                job_item = NULL; /* can not access this job_item anymore */
            }
        }
            
        PERF_MSG("callback_job <\n");
    }
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for callback list, dec_data */
    DRV_UNLOCK_G(); /* unlock for engine/minor list */

    /* check if any channels are stopping */
    if(stop_chn_cnt){
        for (minor = 0; minor < chn_num; minor++) {
            dec_data = &(private_data[minor]);
            if(dec_data == NULL){
                continue;
            }
            
            if(dec_data->handling_stop_flg == 0)
                continue; /* typical case */

            /* rare case: the channel is stopping */
            engine = dec_data->engine;
            
                
            DEBUG_M(LOG_INFO, 0, 0, " callback_scheduler handling stopping (%d,%d)\n", engine, minor);


            /* release buffers for the stopping channel. NOTE: need to be in non-atomic context */
            /* cleanup used buffers in dec_data->dec_buf */
            __favcd_cleanup_dec_buf(dec_data);
            dec_data->need_init_flg = INIT_NORMAL;
            dec_data->out_fr_cnt = 0;

            dec_data->handling_stop_flg = 0;
            dec_data->stopping_flg = 0;

            DEBUG_M(LOG_INFO, 0, 0, " callback_scheduler free buffers (%d,%d)\n", engine, minor);

            /* trigger prepare_job again in case there are job put into driver at stopping */
            trigger_prepare();
        }
    }


    cb_sch_end = _FAVCD_JIFFIES;
    cb_sch_dur = cb_sch_end - cb_sch_start;

    DEBUG_M(LOG_INFO, 0, 0, " callback_scheduler return\n");
    //PERF_MSG("callback_sch <\n");
    if(cb_job_cnt == 0){
        cb_job_avg_dur = 0;
    }else{
        cb_job_avg_dur = cb_job_dur / cb_job_cnt;
    }
    PERF_MSG_N(PROF_CB, "cb job_dur/cnt: %d/%d=%d cb_cnt:%d cb_add/scan:%d/%d stop_chn:%d scan_dur:%d dur:%d period:%d\n", 
        cb_job_dur, cb_job_cnt, cb_job_avg_dur, callback_cnt, minor_job_add_cnt, minor_job_cnt, stop_chn_cnt, scan_list_dur, cb_sch_dur, cb_sch_period);

#if 1
    /* record profiling data */

    down(&cb_prof_data.sem);
    //favcd_spin_lock_irqsave(&favc_dec_lock, flags);
    cb_prof_data.cb_job_cnt += cb_job_cnt;
    if(cb_job_cnt == 0 && stop_chn_cnt == 0){
        cb_prof_data.cb_waste_cnt++;
    }
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_period = cb_sch_period;
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_start = cb_sch_start;
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_sch_dur = cb_sch_dur;
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_job_dur = cb_job_dur;
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_job_cnt = cb_job_cnt;
    cb_prof_data.cb_prof_rec[cb_rec_idx].cb_stop_cnt = stop_chn_cnt;
    //favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);
    
    up(&cb_prof_data.sem);
#endif
    //printk("callback_sch <\n");
}


/*
 * queue an work of calling callback_scheduler() into workqueue
 */
void trigger_callback(void)
{
    int ret;
    unsigned long flags;
    int sch_done_flg = 0;
    
    DEBUG_M(LOG_INFO, 0, 0, " trigger_callback enter\n");
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for favcd_cb_wq */
    
#if USE_VG_WORKQ
    VP_PREPARE_WORK(&process_callback_work, (void *)callback_scheduler);
    ret = vp_queue_delayed_work(favcd_cb_wq, &process_callback_work, callback_period);
    sch_done_flg = (ret==0)?1:0;
#else //!USE_VG_WORKQ
#if USE_KTHREAD
    ftmcp300_cb_wakeup(); 
#else
    PREPARE_DELAYED_WORK(&process_callback_work, (void *)callback_scheduler);
    ret = queue_delayed_work(favcd_cb_wq, &process_callback_work, msecs_to_jiffies(callback_period));
    sch_done_flg = (ret)?1:0;
#endif
#endif //!USE_VG_WORKQ

    if(sch_done_flg)
    {
        int idx = cb_prof_data.cb_trig_cnt % CB_PROF_RECORD_NUM;
        cb_prof_data.cb_trig_cnt++;
        cb_prof_data.cb_prof_rec[idx].cb_trig = _FAVCD_JIFFIES;
    }

    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for favcd_cb_wq */
    DEBUG_M(LOG_INFO, 0, 0, " trigger_callback return: %s %d\n", sch_done_flg?"success":"failed", ret);
}


/*
 * queue an work of calling prepare_job() into workqueue
 */
void trigger_prepare(void)
{
    int ret;
    unsigned long flags;
    int sch_done_flg = 0;
    
    DEBUG_M(LOG_INFO, 0, 0, " trigger_prepare enter\n");
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for favcd_prepare_wq */
#if USE_VG_WORKQ
    VP_PREPARE_WORK(&process_prepare_work, (void *)prepare_job);
    ret = vp_queue_delayed_work(favcd_prepare_wq, &process_prepare_work, prepare_period);
    sch_done_flg = (ret==0)?1:0;
#else //!USE_VG_WORKQ
#if USE_KTHREAD
    ftmcp300_pp_wakeup();
#else
    PREPARE_DELAYED_WORK(&process_prepare_work, (void *)prepare_job);
    ret = queue_delayed_work(favcd_prepare_wq, &process_prepare_work, msecs_to_jiffies(prepare_period));
    sch_done_flg = (ret)?1:0;
#endif
#endif //!USE_VG_WORKQ

    if(sch_done_flg)
    {
        int idx = pp_prof_data.pp_trig_cnt % PP_PROF_RECORD_NUM;
        pp_prof_data.pp_trig_cnt++;
        pp_prof_data.pp_prof_rec[idx].pp_trig = _FAVCD_JIFFIES;
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for favcd_prepare_wq */
    DEBUG_M(LOG_INFO, 0, 0, " trigger_prepare return: %s %d\n", sch_done_flg?"success":"failed", ret);

}



#define __SW_TIMEOUT__
void favcd_sw_timeout_handler(unsigned long data)
{
    struct favcd_data_t *dec_data;
    struct favcd_job_item_t *job_item;
    unsigned long flags;
    int chn_idx;
    unsigned int engine_idx = data;
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); // critical section 10 start
    if(engine_idx >= ENTITY_ENGINES){
        DEBUG_M(LOG_ERROR, 0, 0, "favcd_sw_timeout_handler invalid engine index:%d\n", engine_idx);
        goto err_ret;
    }
    
    chn_idx = favcd_engine_info[engine_idx].chn_idx;
    if(chn_idx >= chn_num || chn_idx < 0){
        DEBUG_M(LOG_ERROR, 0, 0, "favcd_sw_timeout_handler invalid chn_idx:%d\n", chn_idx);
        goto err_ret;
    }
    
    dec_data = &private_data[chn_idx];
    if(dec_data == NULL){
        DEBUG_M(LOG_ERROR, 0, 0, "favcd_sw_timeout_handler channel pointer is null\n");
        goto err_ret;
    }

    job_item = dec_data->curr_job;
    if(job_item == NULL){
        DEBUG_M(LOG_ERROR, 0, 0, "favcd_sw_timeout_handler current job is null\n");
        goto err_ret;
    }
    
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) favcd_sw_timeout_handler enter\n", dec_data->engine, dec_data->minor);

    
    // do timeout handling if HW is still busy
    if(!test_engine_idle(dec_data->engine)){
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_sw_timeout_handler engine busy, handling timeout\n", dec_data->engine, dec_data->minor);

        H264Dec_OneFrameErrorHandler(dec_data->dec_handle, &dec_data->dec_frame);
        H264Dec_UnbindEngine(dec_data->dec_handle);

        /* report errors for the current job */
        SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
        job_item->err_num = H264D_ERR_SW_TIMEOUT;
        job_item->stored_flg = dec_data->dec_frame.bPicStored;
        job_item->err_pos = ERR_SW_TIMEOUT;
#if USE_SW_PARSER
        job_item->parser_offset = sw_byte_offset(&dec_data->dec_handle->stream);
#endif
#if REV_PLAY && GOP_JOB_STOP_HANDLING
        if(job_item->gop_job_item){
            job_item->gop_job_item->err_flg= 1;
        }
#endif
        if(PRINT_ERR_IN_ISR(dbg_mode)){
            favcd_print_job_error(job_item);
        }

        if(job_item->stored_flg == 0){
            /* add job to fail list only when the picture is not stored to DPB, 
               which means it will not be returned via output list */
            //__favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[job_item->minor]);
            __favcd_append_output_list(dec_data, job_item->buf_idx);//dec_data->dec_frame.ptReconstBuf->buffer_index);
            __favcd_append_release_list(dec_data, job_item->buf_idx);//dec_data->dec_frame.ptReconstBuf->buffer_index);

        }
        if(DAMNIT_AT_ERR(dbg_mode)){
            favcd_dump_job_bs(job_item);
            H264Dec_DumpReg(dec_data->dec_handle, LOG_ERROR);
            FAVCD_DAMNIT("job %d sw timeout error: %d(%s) buf:%d stored:%d\n", job_item->engine, job_item->minor, job_item->job_id, job_item->err_num, 
                favcd_err_str(job_item->err_num), dec_data->dec_frame.ptReconstBuf->buffer_index, dec_data->dec_frame.bPicStored);
        }
        
        job_item->finishtime = jiffies;
        job_item->prof_finish_time = _FAVCD_JIFFIES;
        mark_engine_finish(job_item->engine);
        
        /* NOTE: the order of the following functions must be fixed */
        favcd_set_out_property(dec_data, job_item);
        __favcd_process_output_list(dec_data, 0);
        __favcd_process_release_list(dec_data);
        __favcd_handle_unoutput_buffer(dec_data);

        /* mark HW engine idle */
        set_engine_idle(job_item->engine);

        /* unbind engine */
        dec_data->curr_job = NULL; /* mark channel idle */
#if REV_PLAY && GOP_JOB_STOP_HANDLING
        if(job_item->gop_job_item){
            job_item->gop_job_item->ongoing_flg = 0;
        }
#endif
        dec_data->engine = -1; 

        favcd_error_handling(dec_data, job_item->err_num);
        
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "favcd_sw_timeout_handler trigger_callback\n");
        trigger_callback();
        
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "calling work_scheduler()\n");
        work_scheduler(NULL);
    }
    

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_sw_timeout_handler return\n", dec_data->engine, dec_data->minor);
err_ret:
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); // critical section 10 stop
    return;

    
    
}

/*
 * set a timer for handling decode timeout
 */
void favcd_set_sw_timeout(struct favcd_data_t *dec_data)
{
    int engine;
#define CHK_REENRTENT
#ifdef CHK_REENRTENT
    static int enter_flg = 0;
    int tmp;
#endif // CHK_REENRTENT    
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) favcd_set_sw_timeout enter\n",
        dec_data->engine, dec_data->minor);

    engine = dec_data->engine;

#ifdef CHK_REENRTENT
    tmp = enter_flg;
    enter_flg = 1;
    if(tmp){
        FAVCD_DAMNIT("sw timeout re-enterred\n");
        //while(1);
    }
#endif // CHK_REENRTENT    


#if SW_TIMEOUT_ENABLE
    init_timer(&(sw_timeout_timer[engine]));
    sw_timeout_timer[engine].function = favcd_sw_timeout_handler;
    sw_timeout_timer[engine].expires = jiffies + sw_timeout_delay;
    sw_timeout_timer[engine].data = (unsigned int)engine;
    //favcd_print_list(&(sw_timeout_timer[engine].entry));
    add_timer(&(sw_timeout_timer[engine]));
#endif // SW_TIMEOUT_ENABLE

    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) favcd_set_sw_timeout return\n", dec_data->engine, dec_data->minor);

#ifdef CHK_REENRTENT
    enter_flg = 0;
#endif // CHK_REENRTENT    

}


/*
 * cancel the timer for handling decode timeout
 */
void favcd_unset_sw_timeout(struct favcd_data_t *dec_data)
{
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) favcd_unset_sw_timeout enter\n", dec_data->engine, dec_data->minor);
#if SW_TIMEOUT_ENABLE
    del_timer(&sw_timeout_timer[dec_data->engine]);
#endif // SW_TIMEOUT_ENABLE
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) favcd_unset_sw_timeout return\n", dec_data->engine, dec_data->minor);
}



/*
 * set parameters that may updated for each frame (such as scaling) for a channel
 */
STATIC int favcd_update_param(struct favcd_data_t *dec_data)
{
    FAVC_DEC_FR_PARAM  dec_param;
    int ret;
    
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_update_param enter\n", dec_data->engine, dec_data->minor);


	/* scale options */
    dec_param.bScaleEnable = dec_data->sub_yuv_en;
    dec_param.u8ScaleType = 0;
    dec_param.u8ScaleRow = dec_data->sub_yuv_ratio;
    dec_param.u8ScaleColumn = dec_data->sub_yuv_ratio;
    dec_param.u16ScaleYuvWidthThrd = dec_data->yuv_width_thrd;

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_SetScale\n", dec_data->engine, dec_data->minor);
    ret = H264Dec_SetScale(&dec_param, dec_data->dec_handle);
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_SetScale return %d\n", dec_data->engine, dec_data->minor, ret);
    if (ret < 0) {
        return -EFAULT;
    }

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_update_param return\n", dec_data->engine, dec_data->minor);

    return 0;
}



/*
 * set init parameters for a channel
 * init_flg: the reason of init
 */
STATIC int favcd_init_param(struct favcd_data_t *dec_data, enum init_cond init_flg)
{
    FAVC_DEC_PARAM dec_param;
    int ret;
    
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_init_param enter\n", dec_data->engine, dec_data->minor);

    if(dec_data->dec_handle == NULL) {
        /* should not happen */
        FAVCD_DAMNIT("(%d, %d) null dec_handle\n", dec_data->engine, dec_data->minor);
        return -EFAULT;
    }

    PERF_MSG("init_dec_param_t >\n");

    memset(&dec_param, 0, sizeof(dec_param));
    
    dec_param.u16MaxWidth = ((dec_data->dst_bg_dim >> 16) & 0xFFFF);
    if(dec_param.u16MaxWidth > mcp300_max_width){
        DEBUG_M(LOG_WARNING, dec_data->engine, dec_data->minor, "(%d,%d) favcd_init_param shrink max width: %d->%d\n", 
            dec_data->engine, dec_data->minor, dec_param.u16MaxWidth, mcp300_max_width);
        dec_param.u16MaxWidth = mcp300_max_width;
    }
    dec_param.u16MaxHeight = (dec_data->dst_bg_dim & 0xFFFF);
    if(dec_param.u16MaxHeight > mcp300_max_height){
        DEBUG_M(LOG_WARNING, dec_data->engine, dec_data->minor, "(%d,%d) favcd_init_param shrink max height: %d->%d\n", 
            dec_data->engine, dec_data->minor, dec_param.u16MaxHeight, mcp300_max_width);
        dec_param.u16MaxHeight = mcp300_max_height;
    }

    dec_param.bUnsupportBSlice = (mcp300_b_frame==0?1:0);
    dec_param.bOutputMBinfo = output_mbinfo;
    dec_param.bChromaInterlace = chroma_interlace;
    dec_param.u8MaxBufferedNum = mcp300_max_buf_num;
    dec_param.bVCCfg = vc_cfg;
    dec_param.u8LosePicHandleFlags = lose_pic_handle_flags;

    dec_param.fr_param.bScaleEnable = dec_data->sub_yuv_en;
    dec_param.fr_param.u8ScaleType = 0;
    dec_param.fr_param.u8ScaleRow = dec_data->sub_yuv_ratio;
    dec_param.fr_param.u8ScaleColumn = dec_data->sub_yuv_ratio;
    dec_param.fr_param.u16ScaleYuvWidthThrd = dec_data->yuv_width_thrd;

    PERF_MSG("init_dec_param_t <\n");
    
    /* set parameter */
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_Init\n", dec_data->engine, dec_data->minor);
    PERF_MSG("H264Dec_Init >\n");
    ret = H264Dec_Init(&dec_param, dec_data->dec_handle);
    PERF_MSG("H264Dec_Init <\n");
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_Init return\n", dec_data->engine, dec_data->minor);
    
    if (ret < 0) {
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_Init failed\n", dec_data->engine, dec_data->minor);
        return -EFAULT;
    }

    if(init_flg == INIT_DUE_TO_ERR){
        /* init is triggerd by decoding error */
        ret = H264Dec_SetFlags(dec_data->dec_handle, DEC_SKIP_TIL_IDR_DUE_TO_ERR);
        if (ret < 0) {
            DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_SetFlags failed\n", dec_data->engine, dec_data->minor);
            return -EFAULT;
        }
    }

    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) favcd_init_param return\n", dec_data->engine, dec_data->minor);

    return 0;
}


/*
 * pad bytes (start code) at the end of bitstream (HW reqirement)
 */
STATIC void favcd_pad_bitstream(struct favcd_job_item_t *job_item)
{
    static const unsigned char EndOfStreamByte[4] = {0x00, 0x00, 0x00, 0x01};
    int padded_size;
    unsigned int in_addr_va = job_item->job->in_buf.buf[0].addr_va;
    
    /* padding EOS, 4 bytes */
    memcpy((void *)(in_addr_va + job_item->bs_size), EndOfStreamByte, sizeof(EndOfStreamByte));
    padded_size = job_item->bs_size + sizeof(EndOfStreamByte);

}


/* clean D-cache for input bitstream */
STATIC void favcd_clean_bitstream(struct favcd_job_item_t *job_item)
{
    PERF_MSG("clean_dc >\n");
    fmem_dcache_sync((void *)job_item->job->in_buf.buf[0].addr_va, job_item->bs_size + mcp300_codec_padding_size, DMA_TO_DEVICE);
    PERF_MSG("clean_dc <\n");
}



/*
 * pass the input bitsteam address/size to lowwer level driver, and fill padding bytes
 */
STATIC int favcd_set_bitstream(struct favcd_data_t *dec_data)
{
    int ret;
    struct favcd_job_item_t *job_item = dec_data->curr_job;
    unsigned int in_addr_va = job_item->bs_addr_va;//job_item->job->in_buf.buf[0].addr_va;
    unsigned int in_addr_pa = job_item->bs_addr_pa;//job_item->job->in_buf.buf[0].addr_pa;

    /* checking input buffer */
    //favcd_print_buf((unsigned char *)job_item->in_addr_va, job_item->bs_size);
    
    DEBUG_M(LOG_INFO, dec_data->engine, dec_data->minor, "(%d,%d) job %d seq: %d bitstream va: 0x%08X pa: 0x%08X size: %d\n",
        dec_data->engine, dec_data->minor, dec_data->curr_job->job_id, dec_data->curr_job->seq_num,
        in_addr_va, in_addr_pa, job_item->bs_size);

    DUMP_MSG(D_BUF_FR, dec_data->dec_handle, "(%d,%d) job %d seq: %d bitstream va: 0x%08X pa: 0x%08X size: %d\n",
        dec_data->engine, dec_data->minor, dec_data->curr_job->job_id, dec_data->curr_job->seq_num, 
        in_addr_va, in_addr_va, job_item->bs_size);

    /* set bitstream address to lowwer level driver */
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_SetBitstreamBuf\n", dec_data->engine, dec_data->minor);
    ret = H264Dec_SetBitstreamBuf(dec_data->dec_handle, in_addr_pa, in_addr_va, job_item->bs_size);
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_SetBitstreamBuf return: %d\n", dec_data->engine, dec_data->minor, ret);
    
    if(ret < 0){
        DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "copy move and refilll bs error\n");
        return -1;
    }
    

    return 0;
}


#define __START_JOB__

/* check if job's input/output buffer address are not null and propery aligned */
int favcd_check_job_addr(struct favcd_job_item_t *job_item)
{
    struct video_entity_job_t *job;
    int err_flg = 0;

    job = (struct video_entity_job_t *)job_item->job;

    if (job->in_buf.buf[0].addr_pa == 0 || job->out_buf.buf[0].addr_pa == 0) {
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d in/out buf is null", 
            job_item->engine, job_item->minor, job_item->job_id);
        err_flg = 1;
    }


    if(job->in_buf.buf[0].addr_pa & 0x7){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d input buf0 addr (%08X) is not multiple of 8\n",
            job_item->engine, job_item->minor, job_item->job_id, job->out_buf.buf[0].addr_pa);
        err_flg = 1;
    }
    
    if(job->out_buf.buf[0].addr_pa & 0xFF){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d output buf0 addr (%08X) is not a multiple of 256\n", 
            job_item->engine, job_item->minor, job_item->job_id, job->out_buf.buf[0].addr_pa);
        err_flg = 1;
    }

    if(job->out_buf.btype == TYPE_YUV422_RATIO_FRAME){
        if(job->out_buf.buf[1].addr_pa == 0){
            DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d output buf1 null pointer error when btype is TYPE_YUV422_RATIO_FRAME\n", 
                job_item->engine, job_item->minor, job_item->job_id);
            err_flg = 1;
        }
        if(job->out_buf.buf[1].addr_pa & 0x7){
           DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d output buf1 addr (%08X) is not multiple of 8\n",
               job_item->engine, job_item->minor, job_item->job_id, job->out_buf.buf[1].addr_pa);
           err_flg = 1;
        }
    }
    
    if(err_flg){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d,%d) job %d has invalid in/out address\n", 
            job_item->engine, job_item->minor, job_item->job_id);
    }

    return err_flg;

}


STATIC int favcd_start_job(struct favcd_job_item_t *job_item)
{
    struct favcd_data_t *dec_data;
    struct video_entity_job_t *job;
    int engine, minor;
    int ret;
    FAVC_DEC_FRAME_IOCTL *dec_frame;

    PERF_MSG("start_job >\n");

    job = (struct video_entity_job_t *)job_item->job;
    engine = job_item->engine;
    minor = job_item->minor;
    
    DEBUG_M(LOG_INFO, engine, minor, "(%d,%d) 0x%04X favcd_start_job %d enter\n", engine, minor, (unsigned int)current&0xffff, job_item->job_id);
    PERF_MSG("start_job:%d %d\n", job_item->job_id, engine); /* NOTE: do not change this. This is for parse_log.pl to identify start of a job  */

    dec_data = private_data + minor;
    dec_data->engine = engine;
    dec_frame = &dec_data->dec_frame;
    dec_frame->bPicStored = 0;
    dec_frame->u8ReleaseBufferNum = 0;
    dec_frame->apFrame.u8OutputPicNum = 0;

    PERF_MSG("in_property >\n");
    ret = favcd_get_in_property(dec_data, job_item);
    PERF_MSG("in_property <\n");
    if(ret){
        DEBUG_M(LOG_ERROR, engine, minor, "parsing property error\n");
        goto err_ret1;
    }
    
    /* NOTE: input bitstream size has been set via property, not from here */
    if(dec_data->need_init_flg != INIT_DONE) {
        DEBUG_M(LOG_WARNING, engine, minor, "(%d,%d) job %d init decoder: %dx%d size:%d\n",
            engine, minor, job_item->job_id, job_item->dst_bg_dim >> 16, job_item->dst_bg_dim & 0xFFFF, job_item->bs_size);
        
        /* initialize lower level driver */
        DEBUG_M(LOG_TRACE, engine, 0, "(%d,%d) 0x%04X favcd_start_job %d calling favcd_init_param\n", engine, minor, (unsigned int)current&0xffff, job_item->job_id);
        PERF_MSG("init_param >\n");
        ret = favcd_init_param(dec_data, dec_data->need_init_flg);
        PERF_MSG("init_param <\n");
        if(ret){
            DEBUG_M(LOG_ERROR, engine, 0, "(%d,%d) job %d favcd_init_param error\n", engine, minor, job_item->job_id);
            goto err_ret1;
        }
        dec_data->need_init_flg = INIT_DONE;
    }else{
        /* update parameters to lower level driver */
        DEBUG_M(LOG_TRACE, engine, 0, "(%d,%d) 0x%04X favcd_start_job %d calling favcd_update_param\n", engine, minor, (unsigned int)current&0xffff, job_item->job_id);
        PERF_MSG("update_param >\n");
        favcd_update_param(dec_data);
        PERF_MSG("update_param <\n");
    }
    
    H264Dec_BindEngine(dec_data->dec_handle, &favcd_engine_info[engine]);
    
    if(mcp300_sw_reset){
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_Reset\n", dec_data->engine, dec_data->minor);
        H264Dec_Reset(dec_data->dec_handle);
        DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "(%d,%d) H264Dec_Reset return\n", dec_data->engine, dec_data->minor);
    }

    PERF_MSG("fill_bs >\n");
    ret = favcd_set_bitstream(dec_data);
    PERF_MSG("fill_bs <\n");
    if (ret < 0) {
        DEBUG_M(LOG_ERROR, engine, minor, "(%d,%d) job %d set bitstream error\n", engine, minor, job_item->job_id);
        goto err_ret2;
    }

    /* prepare parameters for H264Dec_OneFrame() */
    favcd_set_buffer(dec_data, &dec_data->recon_buf, job_item->buf_idx);
    dec_frame->ptReconstBuf = &dec_data->recon_buf;
    dec_frame->apFrame.i8Valid = 0; /* mark the output as unset */

    job_item->starttime = jiffies;
#if EXC_HDR_PARSING_FROM_UTIL == 0
    mark_engine_start(job_item->engine);
#endif

    /* trigger decoder */
    DEBUG_M(LOG_INFO, engine, minor,"(%d,%d) job %d seq: %d bs_idx:%d H264Dec_OneFrameStart call\n", job_item->engine, job_item->minor, 
        job_item->job_id, job_item->seq_num, 
#if REV_PLAY
        job_item->bs_idx
#else
        0
#endif
        );
    PERF_MSG("dec_one >\n");
    ret = H264Dec_OneFrameStart(dec_data->dec_handle, dec_frame);
    PERF_MSG("dec_one <\n");
    DEBUG_M(LOG_INFO, engine, minor, "(%d,%d) job %d H264Dec_OneFrameStart return %d\n", job_item->engine, job_item->minor, job_item->job_id, ret);
    
    if(ret) { 
        /* handling decode start error */
        job_item->finishtime = jiffies;
        job_item->prof_finish_time = _FAVCD_JIFFIES;
#if EXC_HDR_PARSING_FROM_UTIL == 1
        if(dec_frame->u8FrameTriggerCnt){
            mark_engine_finish(job_item->engine);
        }
#else
        mark_engine_finish(job_item->engine);
#endif
        H264Dec_UnbindEngine(dec_data->dec_handle);

        SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
        job_item->err_num = ret;
        job_item->err_pos = ERR_TRIG;
        job_item->stored_flg = dec_data->dec_frame.bPicStored;
#if USE_SW_PARSER
        job_item->parser_offset = sw_byte_offset(&dec_data->dec_handle->stream);
#endif
#if REV_PLAY && GOP_JOB_STOP_HANDLING
        if(job_item->gop_job_item){
            job_item->gop_job_item->err_flg = 1;
        }
#endif

        if(PRINT_ERR_IN_ISR(dbg_mode)){
            favcd_print_job_error(job_item);
        }

        if(DAMNIT_AT_ERR(dbg_mode)){
            FAVCD_DAMNIT("job %d trigger failed: %d(%s)\n", job_item->job_id, ret, favcd_err_str(ret));
        }

        /* if picture is not stored into DPB, append it into output/release list */
        if(dec_frame->bPicStored == 0){
            __favcd_append_output_list(dec_data, job_item->buf_idx);
            __favcd_append_release_list(dec_data, job_item->buf_idx);
        }
        
        favcd_set_out_property(dec_data, job_item);
        __favcd_process_output_list(dec_data, 0);
        __favcd_process_release_list(dec_data);
        __favcd_handle_unoutput_buffer(dec_data);

        favcd_error_handling(dec_data, job_item->err_num);
        
        /* return fail, let work_scheduler to find the next job */
        PERF_MSG("start_job < e\n");
        return -1;
    }

    #if 0
    dec_data->curr_job = NULL; // for testing error handling
    #endif
    #if 0
    H264Dec_UnbindEngine(dec_data->dec_handle); // for testing error handling
    #endif

    /* decode triggered */
    favcd_set_sw_timeout(dec_data);

    PERF_MSG("start_job <\n");

    return 0;

err_ret2:
    H264Dec_UnbindEngine(dec_data->dec_handle);
err_ret1:

    SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
    /* if picture is not stored into DPB, mark it as released */
    job_item->stored_flg = dec_data->dec_frame.bPicStored;
#if REV_PLAY && GOP_JOB_STOP_HANDLING
    if(job_item->gop_job_item){
        job_item->gop_job_item->err_flg= 1;
    }
#endif

    if(job_item->stored_flg == 0){
        __favcd_add_job_of_buf_to_callback(dec_data, job_item->buf_idx);
        __favcd_mark_buf_released(dec_data, job_item->buf_idx);
    }
    
    PERF_MSG("start_job < e\n");
    return -1;
}


/*
 * get number of allowed buffer per channel
 */
unsigned int get_buf_num_allowed(void)
{
    unsigned int buf_num_allowed;
    
#if LIMIT_VG_BUF_NUM
    if((mcp300_b_frame == 0) && (output_mbinfo == 0)){
        /* use max_pp_buf_num as the max allowed buffer number if no mbinfo buffer is required */
        buf_num_allowed = max_pp_buf_num;
    }else{
        buf_num_allowed = mcp300_max_buf_num + extra_buf_num;
    }
#else
    buf_num_allowed = max_pp_buf_num;
#endif

    /* buffer number allowed must be within the range: 2~MAX_DEC_BUFFER_NUM */
    if(buf_num_allowed > MAX_DEC_BUFFER_NUM){
        buf_num_allowed = MAX_DEC_BUFFER_NUM;
    }
    
    /* at least 2 buffers are required to keep decode going */
    if(buf_num_allowed < 2){
        buf_num_allowed = 2;
    }

    return buf_num_allowed;
}

/*
 * Parse input property and get buffer from per-channel buffer pool for each job
 */
void prepare_job(void)
{
    struct favcd_job_item_t *job_item;
    struct favcd_job_item_t *job_item_next;
    struct favcd_data_t *dec_data;
#if REV_PLAY
    struct favcd_gop_job_item_t *gop_job_item;
#endif
    unsigned long flags;
    int minor;
    int ret;
    int pp_job_cnt = 0;
    int i, chn;
    BufferAddr buf_addr;
    int buf_num_allowed;

    /* for profiling */
    static unsigned int last_pp_start = 0;
    unsigned int pp_start;
    unsigned int pp_end;
    unsigned int pp_dur;
    unsigned int pp_period;
    
    unsigned int pp_parse_cnt = 0;
    unsigned int pp_parse_start;
    unsigned int pp_parse_end;
    unsigned int pp_parse_dur;
    
    unsigned int pp_rel_start;
    unsigned int pp_rel_end;
    unsigned int pp_rel_dur = 0;
    unsigned int pp_rel_cnt = 0;

    unsigned int pp_job_start;
    unsigned int pp_job_end;
    unsigned int pp_job_dur = 0;

    unsigned int pp_stop_chn_cnt = 0;
    

    pp_start = _FAVCD_JIFFIES;
    pp_period = pp_start - last_pp_start;
    last_pp_start = pp_start;

    PERF_MSG("p <<\n");

    DEBUG_M(LOG_TRACE, 0, 0, "prepare_job enter\n");

    buf_num_allowed = get_buf_num_allowed();

    DRV_LOCK_G(); /* lock for standby list and stopping flag */

    pp_parse_start = _FAVCD_JIFFIES;
    /* process job in standby list */
    //list_for_each_entry_safe(job_item, job_item_next, &favcd_standby_head, standby_list)
    while(1)
    {
#if REV_PLAY        
        favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for standby_list */
#endif
        if(list_empty(&favcd_standby_head)){
#if REV_PLAY  
            favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for standby_list */
#endif
            break;
        }
        
        pp_parse_cnt++;
        job_item = list_entry(favcd_standby_head.next, struct favcd_job_item_t, standby_list);
        __favcd_list_del(&job_item->standby_list);
        __favcd_init_list_head(&job_item->standby_list);
        
#if REV_PLAY
        favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for standby_list */
#endif
        
        job_item->prof_prepare_time = _FAVCD_JIFFIES;

        /* skip fail job (caused by stopping) */
        if(job_item->status == DRIVER_STATUS_FAIL){
            DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) prepare_job skip due to stopping: job %d status %s\n",
                job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));
            continue;
        }
        
        chn = job_item->minor;

        /* check address and address alignment: it is a one time task per job */
        if(job_item->addr_chk_flg == 0){
            if(favcd_check_job_addr(job_item)){
                DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) job %d status %s job address error\n",
                    job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail list */
                SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                __favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[chn]);
				fail_list_idx[chn]++;
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for fail list */
                job_item->addr_chk_flg = -1; /* mark as error */
                
                if(DAMNIT_AT_ERR(dbg_mode)){
                    FAVCD_DAMNIT("{%d,%d) job %d status %s job address error\n",
                    job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));
                }
                continue;
            }
            job_item->addr_chk_flg = 1;
        }
        
        /* parse input property */
        if(job_item->in_property_parsed_flg == 0){
            DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) parse in property job %d status %s\n",
                job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));

            if(favcd_parse_in_property_to_job_item(job_item)){
                DEBUG_M(LOG_ERROR, engine, minor, "{%d,%d) parse in property job %d status %s error\n",
                    job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail list */
                SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                __favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[chn]);
				fail_list_idx[chn]++;
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for fail list */
                
                if(DAMNIT_AT_ERR(dbg_mode)){
                    FAVCD_DAMNIT("parse in property job %d status %s error\n", job_item->job_id, favcd_job_status_str(job_item->status));
                }
                continue;
            }

#if REV_PLAY
            if(prepare_gop_job_item(job_item)){
                
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail list */
                SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                __favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[chn]);
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for fail list */
                
                if(DAMNIT_AT_ERR(dbg_mode)){
                    FAVCD_DAMNIT("prepare gop job item for job %d status %s error\n", job_item->job_id, favcd_job_status_str(job_item->status));
                }
                continue;
            }
#endif

#if REV_PLAY == 0
            /* modify input bitstream */
            if(pad_en){
                favcd_pad_bitstream(job_item);
            }
#endif

            if(err_bs){
                favcd_gen_err_bitstream(job_item);
                DEBUG_M(LOG_ERROR, engine, minor, "(%d) add bitstream error to job in property job %d seq: %d\n",
                    job_item->minor, job_item->job_id, job_item->seq_num);
                err_bs--;
            }

            /* clean D-cache */
            if(clean_en){
                favcd_clean_bitstream(job_item);
            }
            
            /* save bitstream: after in_size has been parsed */
            favcd_save_bitstream_to_file(job_item);
            
        }
        favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for parepare list */
        __favcd_list_add_tail(&job_item->prepare_minor_list, &favcd_prepare_minor_head[job_item->minor]);
#if USE_SINGLE_PREP_LIST
        __favcd_list_add_tail(&job_item->prepare_list, &favcd_prepare_head);
#endif
        favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for parepare list */

    }

    pp_parse_end = _FAVCD_JIFFIES;
    pp_parse_dur = pp_parse_end - pp_parse_start;

    
    /* process job in each channel's prepare list */
    for(chn = 0; chn < chn_num; chn++)
    {
        dec_data = private_data + chn;
        
        DEBUG_M(LOG_TRACE, 0, 0, "prepare_job for channel %d\n", chn);

        /* skip the job of stopping channel */
        if(dec_data->stopping_flg){
            DEBUG_M(LOG_TRACE, 0, 0, "prepare_job for channel %d skip due to stopping\n", chn);
            pp_stop_chn_cnt++;
            continue;
        }

        /* release buffers for this channel */
        if(dec_data->used_buffer_num){
            pp_rel_start = _FAVCD_JIFFIES;
            for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
                int rel_flg = 0;
                
                ret = 0;
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); // lock for dec_data->dec_buf
                if(dec_data->dec_buf[i].is_used && dec_data->dec_buf[i].is_released){
                    ret = pop_dec_buffer(dec_data, &buf_addr, i);
                    rel_flg = 1;
                }
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);

                if(ret){
                    FAVCD_DAMNIT("prepare_job pop_dec_buffer %d error\n", i);
                }
                
                if(rel_flg && ret == 0){
                    pp_rel_cnt++;
                    ret = free_dec_buffer(&buf_addr);
                    if(ret){
                        DEBUG_M(LOG_ERROR, 0, 0, "prepare_job free_dec_buffer %d error\n", i);
                        FAVCD_DAMNIT("prepare_job free_dec_buffer %d error\n", i);
                    }else{
                        DEBUG_M(LOG_TRACE, 0, 0, "prepare_job free_dec_buffer %d done\n", i);
                    }					
                }
                
            }
            pp_rel_end = _FAVCD_JIFFIES;
            pp_rel_dur += pp_rel_end - pp_rel_start;
        }

        /* scan each channel's prepare list to prepare jobs */
        list_for_each_entry_safe(job_item, job_item_next, &favcd_prepare_minor_head[chn], prepare_minor_list) {
            minor = job_item->minor;

            DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) prepare_job %08X job %d status %s start buf_idx: %d used_buf:%d\n",
                job_item->engine, job_item->minor, (unsigned int)job_item, job_item->job_id, 
                favcd_job_status_str(job_item->status), job_item->buf_idx, dec_data->used_buffer_num);


            /* skip non-standby jobs (which should never happen) */
            if(job_item->status != DRIVER_STATUS_STANDBY){
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) prepare_job job %d st:%d is not standby\n",
                    job_item->engine, job_item->minor, job_item->job_id, job_item->status);
                
                /* remove job from prepare list */
                __favcd_list_del(&job_item->prepare_minor_list);
                __favcd_init_list_head(&job_item->prepare_minor_list);
#if USE_SINGLE_PREP_LIST
                __favcd_list_del(&job_item->prepare_list);
                __favcd_init_list_head(&job_item->prepare_list);
#endif                
                FAVCD_DAMNIT("prepare_job job %d st:%d is not standby\n", job_item->job_id, job_item->status);
                continue; /* skip this job */
            }

            /* 
             * if resolution change, jobs can not be processed until ready list is empty and dec_buf is cleaned up 
             */
            if(job_item->dst_bg_dim != dec_data->dec_buf_dim){
                /* wait until no jobs is in ready list and no job is ongoing */
                if((dec_data->ready_job_cnt == 0) && (dec_data->curr_job == NULL)){
                    /* cleanup dec_buf */
                    /* NOTE: at the first time of decoding, dec_handle may be NULL, and no need to free buffers for it */
                    if(dec_data->dec_handle){
                        DEBUG_M(LOG_TRACE, 0, minor, "cleanup dec_buf\n");
                        __favcd_output_all_frame(dec_data);
                        __favcd_cleanup_dec_buf(dec_data);
                    }
                    dec_data->dec_buf_dim = job_item->dst_bg_dim;
                }else{
                    /* can not prepare jobs of this channel */
                    DEBUG_M(LOG_TRACE, 0, minor, "can not prepare jobs %d for ch %d due to resolution change: %dx%d->%dx%d ready_job_cnt:%d\n", 
                        job_item->job_id, minor,
                        (dec_data->dec_buf_dim >> 16), dec_data->dec_buf_dim & 0xFFFF,
                        (job_item->dst_bg_dim >> 16), job_item->dst_bg_dim & 0xFFFF, dec_data->ready_job_cnt);
                    break; /* skip this channel */
                }
            }


            /* prepare buffer for the job */

            /* skip this channel if buffer number exceed the limit */
            if(dec_data->used_buffer_num >= buf_num_allowed){
                DEBUG_M(LOG_TRACE, 0, minor, "skip job of ch:%d due to buffer limit(%d >= %d)\n", minor, dec_data->used_buffer_num, buf_num_allowed);
                break;
            }

#if REV_PLAY
            gop_job_item = job_item->gop_job_item;
            if(gop_job_item && job_item->set_bs_flg == 0){
                if(gop_job_item->next_dec_bs_idx > gop_job_item->next_output_job_idx){
                    DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, "(%d) job %d seq:%d in gop job dec idx overflow: %d > %d\n", 
                        job_item->minor, job_item->job_id, job_item->seq_num,
                        gop_job_item->next_dec_bs_idx, gop_job_item->next_output_job_idx);
                    break; /* skip this channel */
                }
                /* update the bitstream buffer offset/size */
                set_gop_job_item_bs_buf(job_item->gop_job_item, job_item, gop_job_item->next_dec_bs_idx);
                
                favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for ready_list, ready_job_cnt and job status */
                gop_job_item->next_dec_bs_idx++;
                gop_job_item->pp_rdy_job_cnt--;
                job_item->set_bs_flg = 1;
                favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for ready_list and ready_job_cnt */
            }
#endif
            
            pp_job_start = _FAVCD_JIFFIES;
            ret = allocate_dec_buffer(&buf_addr, job_item);
            if(ret){
                DEBUG_M(LOG_WARNING, job_item->engine, job_item->minor, "{%d,%d) prepare_job job %d allocate_dec_buffer failed\n", job_item->engine, job_item->minor, job_item->job_id);
                break; /* skip this channel */
            }

            ret = push_dec_buffer(dec_data, &buf_addr);
            if(ret < 0){
                DEBUG_M(LOG_WARNING, job_item->engine, job_item->minor, "{%d,%d) prepare_job job %d push_dec_buffer failed\n", job_item->engine, job_item->minor, job_item->job_id);
                free_dec_buffer(&buf_addr);
                break; /* skip this channel */
            }

            /* remove job from prepare list */
            __favcd_list_del(&job_item->prepare_minor_list);
            __favcd_init_list_head(&job_item->prepare_minor_list);

            /* add job into ready list */
            favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for ready_list, ready_job_cnt and job status */
            job_item->prepared_flg = 1;
            

#if USE_SINGLE_PREP_LIST == 0
            job_item->prof_ready_time = _FAVCD_JIFFIES;
            SET_JOB_STATUS(job_item, DRIVER_STATUS_READY);
            __favcd_list_add_tail(&job_item->ready_list, &favcd_ready_head);
            pp_job_cnt++;
#endif
            dec_data->ready_job_cnt++;
            favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for ready_list and ready_job_cnt */

            pp_job_end = _FAVCD_JIFFIES;
            pp_job_dur += pp_job_end - pp_job_start;
            
            DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "{%d,%d) prepare_job %08X job %d status %s done\n",
                job_item->engine, job_item->minor, (unsigned int)job_item, job_item->job_id, favcd_job_status_str(job_item->status));
        }

    }
#if USE_SINGLE_PREP_LIST
    /* NOTE: if total buffer number < (ch * (max_buf_num + 1)), 
             force the job order here may cause dead lock */
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for ready_list, ready_job_cnt and job status */
    list_for_each_entry_safe(job_item, job_item_next, &favcd_prepare_head, prepare_list) {
        if(job_item->prepared_flg){
            /* remove the leading ready job from prepare list and add job to ready list */
            __favcd_list_del(&job_item->prepare_list);
            __favcd_init_list_head(&job_item->prepare_list);
            
            job_item->prof_ready_time = _FAVCD_JIFFIES;
            SET_JOB_STATUS(job_item, DRIVER_STATUS_READY);
            __favcd_list_add_tail(&job_item->ready_list, &favcd_ready_head);
            pp_job_cnt++;
        }
#if FORCE_SAME_PREP_ORDER
        else {
            /* break the loop when the first non-ready job is encountered */
            break;
        }
#endif
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for ready_list and ready_job_cnt */
#endif

    DRV_UNLOCK_G(); /* unlock for standby list and stopping flag */


    /* tigger work_scheduler if necessary */
    if(pp_job_cnt){
        work_scheduler(NULL);
    }
#if 0
    else if(stb_job_cnt){
        /* no job is prepared, but there is standby jobs, trigger prepare thread */
        /* NOTE: calling trigger_prepare() here may result in high CPU loading */
        //trigger_prepare();
    }
#endif

    pp_end = _FAVCD_JIFFIES;
    pp_dur = pp_end - pp_start;

    PERF_MSG_N(PROF_PP, "pp parse cnt:%d dur:%d job_cnt:%d job_dur:%d rel_cnt:%d rel_dur:%d dur:%d period:%d\n", 
        pp_parse_cnt, pp_parse_dur, pp_job_cnt, pp_job_dur, pp_rel_cnt, pp_rel_dur, pp_dur, pp_period);

#if 1
    /* record profiling data */
    i = pp_prof_data.pp_cnt % PP_PROF_RECORD_NUM;
    down(&pp_prof_data.sem);
    
    pp_prof_data.pp_cnt++;
    pp_prof_data.pp_job_cnt += pp_job_cnt;
    if(pp_parse_cnt == 0 && pp_rel_cnt == 0 && pp_job_cnt == 0){
        pp_prof_data.pp_waste_cnt++;
    }
    pp_prof_data.pp_prof_rec[i].pp_period = pp_period;
    pp_prof_data.pp_prof_rec[i].pp_start = pp_start;
    pp_prof_data.pp_prof_rec[i].pp_dur = pp_dur;
    pp_prof_data.pp_prof_rec[i].pp_parse_dur = pp_parse_dur;
    pp_prof_data.pp_prof_rec[i].pp_parse_cnt = pp_parse_cnt;
    pp_prof_data.pp_prof_rec[i].pp_rel_dur = pp_rel_dur;
    pp_prof_data.pp_prof_rec[i].pp_rel_cnt = pp_rel_cnt;
    pp_prof_data.pp_prof_rec[i].pp_job_dur = pp_job_dur;
    pp_prof_data.pp_prof_rec[i].pp_job_cnt = pp_job_cnt;
    pp_prof_data.pp_prof_rec[i].pp_stop_cnt = pp_stop_chn_cnt;
    
    up(&pp_prof_data.sem);
#endif

    
    DEBUG_M(LOG_TRACE, 0, 0, "prepare_job return\n");
}


/*
 * Find an idle engine, an ready job of a idle channel and start decoding
 */
void work_scheduler(struct isr_prof_data *isr_prof_ptr)
{
    unsigned long flags;
    struct favcd_job_item_t *job_item;
    struct favcd_job_item_t *job_item_next;
    int ret;
    int job_started_flg = 0;
    int job_started_cnt = 0;
    int need_callback_flg = 0;
    int engine;
    int engine_start, engine_end;
    struct favcd_data_t *dec_data;
    int jiffies_tmp = _FAVCD_JIFFIES;
    int buf_idx;

    /* for profiling */
    unsigned int work_start;
    unsigned int work_end;
    unsigned int work_dur;
    
    unsigned int start_job_start;
    unsigned int start_job_end;
    unsigned int start_job_dur = 0;

    favcd_spin_lock_irqsave(&favc_dec_lock, flags);  /* critical section: lock the whole work_scheduler */
    PERF_MSG("work_sch >\n");
    work_start = _FAVCD_JIFFIES;

    /* decide the range of engines to be selected */
    if(engine_num == -1){
        engine_start = 0;
        engine_end = ENTITY_ENGINES - 1;
    }else{
        /* if engine_num >= 0, use one engine (with index == engine_num) only */
        engine_start = engine_end = engine_num;
    }

    for(engine = engine_start; engine <= engine_end; engine++){
        if (list_empty(&favcd_ready_head)){
            DEBUG_M(LOG_WARNING, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X return list empty\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);
            break; /* break loop since no job to do */
        }

        job_started_flg = 0;
        DEBUG_M(LOG_INFO, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X enter\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);

        /* skip busy engine */
        if (test_engine_idle(engine) == 0) {
            DEBUG_M(LOG_INFO, engine, 0, "(%d) 0x%04X work_scheduler skip busy engine\n", engine, (unsigned int)current&0xffff);
            continue;
        }

        /* scan through ready list and find a job to do */
        list_for_each_entry_safe(job_item, job_item_next, &favcd_ready_head, ready_list) {
        
            DEBUG_M(LOG_INFO, engine, 0, "(%d) 0x%04X work_scheduler job_item 0x%08X (%d,%d) id: %d st: %s\n", 
                engine, (unsigned int)current&0xffff, (unsigned int)job_item, 
                job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));

            dec_data = private_data + job_item->minor;

            /* check job status */
            if (job_item->status != DRIVER_STATUS_READY) {
                if(dec_data->stopping_flg == 0){
                    /* job status in ready list should always be ready, unless it is stopping */
#if REV_PLAY
                    if(job_item->gop_job_item != NULL){
                        if(job_item->gop_job_item->stop_flg == 0 && job_item->gop_job_item->err_flg == 0){
                            /* gop job status in ready list should always be ready, unless it is stopping or got err */
                            DEBUG_M(LOG_ERROR, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X got not ready got job and not stoping or err\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);
                            FAVCD_DAMNIT("work_scheduler got not ready gop job and not stoping\n");
                        }
                    }else{
                        DEBUG_M(LOG_ERROR, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X got not ready job and not stoping\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);
                        FAVCD_DAMNIT("work_scheduler got not ready job and not stoping\n");
                    }
#else
                    DEBUG_M(LOG_ERROR, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X got not ready job and not stoping\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);
                    FAVCD_DAMNIT("work_scheduler got not ready job and not stoping\n");
#endif
                }
                if(job_item->status != DRIVER_STATUS_FAIL || list_empty(&job_item->fail_list)){
                    /* job status in ready list at stopping should be in fail status and added to fail_list */
                    DEBUG_M(LOG_ERROR, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X invalid stopping job\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff);
                    FAVCD_DAMNIT("work_scheduler invalid stopping job\n");
                }
                
                DEBUG_M(LOG_INFO, engine, 0, "(%d) 0x%04X work_scheduler skip stopped job_item 0x%08X (%d,%d) id: %d st: %s\n", 
                    engine, (unsigned int)current&0xffff, (unsigned int)job_item, 
                    job_item->engine, job_item->minor, job_item->job_id, favcd_job_status_str(job_item->status));

                SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                /* if picture is not stored into DPB, mark it as released */
                job_item->stored_flg = 0;
#if REV_PLAY && GOP_JOB_STOP_HANDLING
                if(job_item->gop_job_item){
                    job_item->gop_job_item->err_flg = 1;
                }
#endif
                if(job_item->stored_flg == 0){
                    __favcd_add_job_of_buf_to_callback(dec_data, job_item->buf_idx);
                    __favcd_mark_buf_released(dec_data, job_item->buf_idx);
                }

                
                /* remove invalid jobs from ready list */
                __favcd_list_del(&job_item->ready_list);
                __favcd_init_list_head(&job_item->ready_list);
                dec_data->ready_job_cnt--;
                continue;
            }

#if 0
            /* skip job of stopping channel */
            if (dec_data->stopping_flg) {
                /* skip this job_item, and work_scheduler() will be triggered again after stopping is handled */
                DEBUG_M(LOG_INFO, engine, 0, "(%d,%d) 0x%04X work_scheduler skip job_item 0x%08X %d dec (%d,%d) stopping\n", job_item->engine, job_item->minor, (unsigned int)current&0xffff, (unsigned int)job_item, job_item->job_id, dec_data->engine, dec_data->minor);
                continue;
            }
#endif
            
            /* skip jobs of busy channel */
            if(dec_data->curr_job != NULL) {
                if(dec_data->curr_job == job_item){
                    DEBUG_M(LOG_ERROR, engine, 0, "(%d,%d) 0x%04X work_scheduler re-schedule the same job %d %d\n",
                        job_item->engine, job_item->minor, (unsigned int)current&0xffff, job_item->job_id, job_item->seq_num);
                }else{
                    DEBUG_M(LOG_INFO, engine, 0, "(%d,%d) 0x%04X work_scheduler skip job_item 0x%08X %d dec (%d,%d) is busy for job %d %d\n",
                        job_item->engine, job_item->minor, (unsigned int)current&0xffff, (unsigned int)job_item, job_item->job_id, 
                        dec_data->engine, dec_data->minor, dec_data->curr_job->job_id, dec_data->curr_job->seq_num);
                }
                continue;
            }
            
            if (test_and_set_engine_busy(engine))
            {
                SET_JOB_STATUS(job_item, DRIVER_STATUS_ONGOING);

                /* NOTE: need to set curr_job before releasing lock, so that the stop can know a job is started */
                dec_data->curr_job = job_item; /* bind channel data and job_item */ 
#if REV_PLAY && GOP_JOB_STOP_HANDLING
                if(job_item->gop_job_item){
                    job_item->gop_job_item->ongoing_flg = 1;
                }
#endif
                dec_data->engine = engine; /* bind channel data and engine */ 
                job_item->engine = engine; /* mark the job_item is scheduled to run on which engine */ 

                job_started_flg = 1;

                job_item->prof_start_time = start_job_start = _FAVCD_JIFFIES;
                ret = favcd_start_job(job_item);
                start_job_end = _FAVCD_JIFFIES;
                start_job_dur += start_job_end - start_job_start;
                
                if(ret < 0) {
                    /* engine start failed, restore status to find another job */
                    set_engine_idle(engine);
                    //SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                    //__favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[idx]);
                    dec_data->curr_job = NULL;
#if REV_PLAY && GOP_JOB_STOP_HANDLING
                    if(job_item->gop_job_item){
                        job_item->gop_job_item->ongoing_flg = 0;
                    }
#endif
                    dec_data->engine = -1;

                    need_callback_flg = 1;
                    job_started_flg = 0;
                    DEBUG_M(LOG_WARNING, engine, 0, "(%d) 0x%04X work_scheduler start job_item 0x%08X %d failed st: %d\n",
                        engine, (unsigned int)current&0xffff, (unsigned int)job_item, job_item->job_id, job_item->status);
                }
            }else{
                DEBUG_M(LOG_WARNING, engine, 0, "(%d) 0x%04X work_scheduler skip job_item 0x%08X busy:%d %d\n",
                    engine, (unsigned int)current&0xffff, (unsigned int)job_item, job_item->engine, test_engine_idle(job_item->engine));
            }

            
            /* remove job from ready_list if job is started or failed */
            if(job_started_flg || need_callback_flg){
                __favcd_list_del(&job_item->ready_list);
                __favcd_init_list_head(&job_item->ready_list);
                dec_data->ready_job_cnt--;
            }
            
            if(job_started_flg){ /* job started */
                buf_idx = job_item->buf_idx;
                if(buf_idx < 0 || buf_idx >= MAX_DEC_BUFFER_NUM){
                    DEBUG_M(LOG_ERROR, engine, job_item->minor, "(%d,%d) invalid buf_idx:%d\n", job_item->engine, job_item->minor, buf_idx);
                }
                dec_data->dec_buf[buf_idx].is_started = 1;
                job_started_cnt++;
                break;
            }
        }

        DEBUG_M(LOG_TRACE, engine, 0, "(%d) 0x%04X work_scheduler 0x%04X return job_started:%d\n", engine, (unsigned int)current&0xffff, jiffies_tmp&0xffff, job_started_flg);
    }

    if(need_callback_flg) {
        DEBUG_M(LOG_INFO, 0, 0, "0x%04X work_scheduler trigger_callback\n", (unsigned int)current&0xffff);
        trigger_callback();
    }

    if(job_started_cnt == 0){
        trigger_prepare();
    }

    work_end = _FAVCD_JIFFIES;
    work_dur = work_end - work_start;

    PERF_MSG_N(PROF_WSCH, "work_sch < work dur:%4d start: %10d start job dur: %4d cnt:%d\n", work_dur, work_start, start_job_dur, job_started_cnt);

    if(isr_prof_ptr){ /* work_scheduler is called in ISR */
        /* record the profiling info of work_scheduler for the current ISR */
        int idx = isr_prof_ptr->isr_cnt % ISR_PROF_RECORD_NUM;
        isr_prof_ptr->isr_prof_rec[idx].isr_work_dur = work_dur;
        isr_prof_ptr->isr_prof_rec[idx].isr_start_job_dur = start_job_dur;
        isr_prof_ptr->isr_prof_rec[idx].isr_start_job_cnt = job_started_cnt;
    }

    PERF_MSG("work_sch <\n");
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* critical section: lock the whole work_scheduler */

}


#define __VG_OPS__
STATIC int favcd_putjob(void *parm)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)parm;
    struct favcd_job_item_t *job_item = NULL;
    int idx;
    struct favcd_data_t *dec_data;
    int engine;
    static unsigned int last_put_time = 0;
#if REV_PLAY
    unsigned long flags;
#endif

    PERF_MSG("favcd_putjob > %08X\n", (unsigned int)parm);

    TRACE_JOB('P'); /* for debug only */

    /* select dec_data according to channel number */
    idx = ENTITY_FD_MINOR(job->fd);
    if (idx >= chn_num){
        DEBUG_J(LOG_ERROR, 0, 0, "decoder: Error to use %s minor %d, chn_num is %d (job id:%d)\n", MODULE_NAME, idx, chn_num, job->id);
        goto err_ret;
    }

    /* multi jobs should never happen to decoder, for now */
    if(job->next_job != 0){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "{%d,%d) job %d has multi_job. should never happen for decoder\n", job_item->engine, job_item->minor, job_item->job_id);
        goto err_ret;
    }

    engine = ENTITY_FD_ENGINE(job->fd);
    if (engine != 0){ /* only allow engine 0 now */
        DEBUG_J(LOG_ERROR, 0, 0, "Error to use %s engine %d, must be 0\n", MODULE_NAME, engine);
        goto err_ret;
    }

    if(job->callback == NULL){
        DEBUG_M(LOG_ERROR, 0, 0, "job %d has null callback pointer\n", job->id);
        DEBUG_M(LOG_ERROR, 0, 0, "dumpping job ptr:%p  size:%d callback offset:%d\n", job, sizeof(*job), (unsigned int)&job->callback - (unsigned int)job);
        favcd_dump_mem(job, sizeof(*job));
        DEBUG_J(LOG_ERROR, 0, 0, "decoder: job %d has null callback pointer: %p\n", job->id, job->callback);
        goto err_ret;
    }


    dec_data = private_data + idx;

    PERF_MSG("__alloc_job_item > %d\n", idx);
    job_item = __alloc_job_item(sizeof(struct favcd_job_item_t));
    if (job_item == 0){
        DEBUG_M(LOG_ERROR, 0, 0, "Error to putjob %s (allocate job_item failed)\n", MODULE_NAME);
        FAVCD_DAMNIT("Error to putjob %s (allocate job_item failed)\n", MODULE_NAME);
        return JOB_STATUS_FAIL;
    }
    PERF_MSG("__alloc_job_item < %d %08X\n", idx, (unsigned int)job_item);

    __favcd_job_init(job_item);
    job_item->job = job;
    job_item->job_id = job->id;
    job_item->engine = 0;
    job_item->minor = idx;
    job_item->seq_num = job_seq_num++;

    SET_JOB_STATUS(job_item, DRIVER_STATUS_STANDBY);
    job_item->puttime = jiffies;
    job_item->prof_put_time = _FAVCD_JIFFIES;
    job_item->prof_put_period = job_item->prof_put_time - last_put_time;
    last_put_time = job_item->prof_put_time;

    DEBUG_M(LOG_INFO, job_item->engine, job_item->minor, "{%d,%d) job %d status %d puttime 0x%x\n", job_item->engine, job_item->minor, job->id, job->status, (int)job_item->puttime&0xffff);

    DRV_LOCK_G(); /* lock for engine list, minor list */
    idx = job_item->minor;
    __favcd_list_add_tail(&job_item->global_list, &favcd_global_head);
    __favcd_list_add_tail(&job_item->minor_list, &favcd_minor_head[idx]);
#if REV_PLAY
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for standby_list */
#endif
    __favcd_list_add_tail(&job_item->standby_list, &favcd_standby_head);
#if REV_PLAY
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for standby_list */
#endif

    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) put job %d status %d\n", job_item->engine, job_item->minor, job_item->job_id, job_item->status);
    DUMP_MSG(D_VG, dec_data->dec_handle, "(%d)put job %d seq: %d\n", job_item->minor, job_item->job_id, job_item->seq_num);
    DRV_UNLOCK_G(); /* unlock for engine list, minor list */
		
	trigger_prepare();
    
    PERF_MSG("favcd_putjob < %08X\n", (unsigned int)parm);

    return JOB_STATUS_ONGOING;

err_ret:
    /* this should never happen */
    if(DAMNIT_AT_ERR(dbg_mode)){
        FAVCD_DAMNIT("Put job failed\n");
    }
    return JOB_STATUS_FAIL;
}


/*
 * Scan through the minor_list and change the job status from STANDBY to FAIL
 * (this function is used to stop the channel)
 */
STATIC int __favcd_remove_standby_jobs(struct favcd_data_t *dec_data)
{
    struct favcd_job_item_t *job_item;
    int updated_jobs = 0;
    int idx = dec_data->minor;
#if REV_PLAY
    struct favcd_gop_job_item_t *gop_job_item;
#endif

    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) __favcd_remove_standby_jobs enter: %d\n", dec_data->engine, dec_data->minor, idx);
    list_for_each_entry (job_item, &favcd_minor_head[idx], minor_list) {
        //if (job_item->status == DRIVER_STATUS_STANDBY || job_item->status == DRIVER_STATUS_READY) 
        if (job_item->status == DRIVER_STATUS_STANDBY) 
        {
            //printk("add job to fail list:%d status:%s\n", job_item->job_id, favcd_job_status_str(job_item->status));
            SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
            __favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[idx]);
			fail_list_idx[idx]++;
#if REV_PLAY
            if(job_item->gop_job_item){
                gop_job_item = job_item->gop_job_item;
                gop_job_item->pp_rdy_job_cnt--;
#if GOP_JOB_STOP_HANDLING
                gop_job_item->stop_flg = 1;
                DEBUG_M(LOG_REV_PLAY, 0, 0, "{%d,%d) __favcd_remove_standby_jobs set stop_flg for job %d seq:%d gop id:%08X\n", 
                    dec_data->engine, dec_data->minor, job_item->job_id, job_item->seq_num, gop_job_item->gop_id);
#endif
            }
#endif
            updated_jobs++;
        }
    }

#if REV_PLAY
#if GOP_JOB_STOP_HANDLING
    DEBUG_M(LOG_REV_PLAY, 0, 0, "{%d,%d) __favcd_remove_standby_jobs set stop_flg via favcd_gop_minor_head\n", dec_data->engine, dec_data->minor);
    list_for_each_entry (gop_job_item, &favcd_gop_minor_head[idx], gop_job_list) {
        gop_job_item->stop_flg = 1;
        DEBUG_M(LOG_REV_PLAY, 0, 0, 
            "{%d,%d) __favcd_remove_standby_jobs set stop_flg via favcd_gop_minor_head gop id:%08X seq:%d gop_size:%d\n", 
            dec_data->engine, dec_data->minor, gop_job_item->gop_id, gop_job_item->seq_num, gop_job_item->gop_size);
    }
#endif
#endif
    
    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) __favcd_remove_standby_jobs return\n", dec_data->engine, dec_data->minor);
    return updated_jobs;
}


#define __PUBLIC_FUNCTION__

int favcd_stop(void *parm_unused, int engine, int minor)
{
    unsigned long flags;
    struct favcd_data_t *dec_data;
    int chg_jobs;

    if(engine != 0){
        DEBUG_M(LOG_ERROR, 0, 0, "Error to stop %s engine %d, must be 0\n", MODULE_NAME, engine);
        FAVCD_DAMNIT("Error to stop %s engine %d, must be 0\n", MODULE_NAME, engine);
    }

    dec_data = private_data + minor;
    engine = dec_data->engine; //not important
    
    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) favcd_stop enter\n", engine, minor);
    DRV_LOCK_G(); /* lock for minor list */
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for job status and stoping flag */
    if(dec_data->stopping_flg == 0){
        /*
         * change status of jobs in minor_list from standby to fail
         * to make the channel has no more jobs to do
         */
        chg_jobs = __favcd_remove_standby_jobs(dec_data);
        DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) favcd_stop remove standby jobs %d\n", engine, minor, chg_jobs);
        
        /*
         * set stopping_flg to signify:
         * 1. work_scheduler(): do not schedule job of this channel
         * 2. callback_scheduler: handling stop when no job is ongoing, then release all resources after callback, and trigger prepare_job() again
         * 3. prepare_job(): do not prepare job of this channel
         */
        dec_data->stopping_flg = 1;
        trigger_callback();
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for job status and stoping flag */
    DRV_UNLOCK_G(); /* unlock for minor list */

    DEBUG_M(LOG_TRACE, engine, minor, "{%d,%d) favcd_stop return\n", engine, minor);
    
    return 0;
}

struct favcd_data_t *favcd_get_dec_data(int chn_idx)
{
    if(chn_idx < 0 || chn_idx >= chn_num)
        return NULL;

    return &private_data[chn_idx];

}


struct favcd_job_item_t *favcd_get_linked_job_of_buf(struct favcd_data_t *dec_data, int buf_idx)
{
    return dec_data->dec_buf[buf_idx].job_item;
}

struct property_map_t *favcd_get_propertymap(int id)
{
    int i;
    
    for (i = 0; i < sizeof(favcd_property_map)/sizeof(struct property_map_t); i++) {
        if (favcd_property_map[i].id == id) {
            return &favcd_property_map[i];
        }
    }
    return 0;
}


/* query property ID by a name string */
STATIC int favcd_queryid(void *parm, char *str)
{
    int i;
    for (i = 0; i < sizeof(favcd_property_map)/sizeof(struct property_map_t); i++) {
        if (strcmp(favcd_property_map[i].name,str) == 0) {
            return favcd_property_map[i].id;
        }
    }
    //printk("favcd_queryid: Error to find name %s\n", str);
    return -1;
}


/* query property name string by a ID */
STATIC int favcd_querystr(void *parm, int id, char *string)
{
    int i;
    for (i = 0; i < sizeof(favcd_property_map)/sizeof(struct property_map_t); i++) {
        if (favcd_property_map[i].id == id) {
            memcpy(string, favcd_property_map[i].name, sizeof(favcd_property_map[i].name));
            return 0;
        }
    }
    printk("favcd_querystr: Error to find id %d\n", id);
    return -1;
}

/* get the property value of a certain engine/channel (query by property name string) */
STATIC int favcd_getproperty(void *parm, int engine, int minor, char *string)
{
    int id,value = -1;
    struct favcd_data_t *dec_data;
    if (engine > 0) {
        printk("Error over engine number %d\n", 0);
        return -1;
    }
    if (minor >= chn_num) {
        printk("Error over minor number  %d >= %d\n", minor, chn_num);
        return -1;
    }

    dec_data = private_data + minor;

    id = favcd_queryid(parm, string);
    switch (id) {        
        case ID_SRC_FMT:
            value = TYPE_BITSTREAM_H264;
            break;
        case ID_DST_FMT:
            value = dec_data->dst_fmt;
            break;
        case ID_DST_XY:
            value = dec_data->dst_xy;
            break;    
        case ID_DST_BG_DIM:
            value = dec_data->dst_bg_dim;
            break;
        case ID_DST_DIM:
            value = dec_data->dst_dim;
            break;
        case ID_SRC_INTERLACE:
            value = dec_data->src_interlace;
            break;
        case ID_YUV_WIDTH_THRESHOLD:
            value = dec_data->yuv_width_thrd;
            break;
        case ID_SUB_YUV_RATIO:
            value = dec_data->sub_yuv_ratio;
            break;
        case ID_SRC_FRAME_RATE:
            //value = dec_data->unit_in_tick;
            if(dec_data->unit_in_tick == 0){
                value = 30;
            }else{
                value = dec_data->time_scale / dec_data->unit_in_tick / 2;
            }
            break;   
        case ID_FPS_RATIO:
            //value = dec_data->time_scale;
            value = 0x00010001;
            break;    
        default:
            break;
    }
    return value;
}



#define __ISR__
STATIC irqreturn_t favcd_int_process(int irq, unsigned int engine_idx)
{
    struct favcd_data_t *dec_data;
    struct favcd_job_item_t *job_item;
    int chn_idx;
    int ret;
    DecoderEngInfo *eng_info;

    /* for profiling */
    int idx;
    struct isr_prof_data *isr_prof_ptr;
    unsigned int rec_start;
    unsigned int rec_end;
    unsigned int dec_start;
    unsigned int dec_end;
    unsigned int mark_start = 0;
    unsigned int mark_end = 0;
    unsigned int buf_start = 0;
    unsigned int buf_end = 0;
    unsigned int trig_start = 0;
    unsigned int trig_end = 0;


    TRACE_JOB('I');
    PERF_MSG("trig <\n");
    PERF_MSG("ISR > %d\n", irq);
    DEBUG_M(LOG_TRACE, 0, 0, "ISR %d entered\n", irq);

    if(engine_idx >= ENTITY_ENGINES){
        DEBUG_M(LOG_ERROR, 0, 0, "invalid engine index:%d\n", engine_idx);
        goto isr_ret2;
    }

    eng_info = &favcd_engine_info[engine_idx];
    isr_prof_ptr = &isr_prof_data[engine_idx];
    
    rec_start = _FAVCD_JIFFIES;

    H264Dec_ReceiveIRQ(eng_info);
    
    chn_idx = eng_info->chn_idx;
    if(chn_idx >= chn_num || chn_idx < 0){
        DEBUG_M(LOG_ERROR, 0, 0, "invalid chn_num:%d for engine %d\n", chn_idx, engine_idx);
        FAVCD_DAMNIT("ISR entered. invalid chn_num\n");
        goto isr_ret2;
    }
    
    dec_data = &private_data[chn_idx];
    job_item = dec_data->curr_job;
    
    if(job_item == NULL) {
        DEBUG_M(LOG_ERROR, 0, 0, "curr job is null\n");
        FAVCD_DAMNIT("ISR entered. curr job is null\n");
        goto isr_ret2;
    }
    
    DEBUG_M(LOG_INFO, job_item->engine, job_item->minor, "(%d,%d) ISR job %d ISR entered\n", job_item->engine, job_item->minor, job_item->job_id);

    if (H264Dec_DispatchIRQ(dec_data->dec_handle, eng_info) != 0) {
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "(%d,%d) ISR job %d ISR but not slice done\n", job_item->engine, job_item->minor, job_item->job_id);
    }
    favcd_unset_sw_timeout(dec_data); /* remove the timer for software timeout */
    rec_end = _FAVCD_JIFFIES;


#if EXC_HDR_PARSING_FROM_UTIL
    mark_engine_finish(engine_idx);
#endif /* EXC_HDR_PARSING_FROM_UTIL */


    dec_start = _FAVCD_JIFFIES;
    PERF_MSG("ISR_dec_rest >\n");
    DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "(%d,%d) ISR job %d H264Dec_OneFrameNext call\n", job_item->engine, job_item->minor, job_item->job_id);
    ret = H264Dec_OneFrameNext(dec_data->dec_handle, &dec_data->dec_frame);
    DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "(%d,%d) ISR job %d H264Dec_OneFrameNext return: %d\n", job_item->engine, job_item->minor, job_item->job_id, ret);
    PERF_MSG("ISR_dec_rest <\n");

    if(H264D_OK == ret) { /* HW Triggerd again */
        favcd_set_sw_timeout(dec_data); /* set timer since the HW engine is triggered */
        DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "(%d,%d) ISR job %d trigger HW again\n", job_item->engine, job_item->minor, job_item->job_id);
        dec_end = _FAVCD_JIFFIES;
        goto isr_ret;
    }else if(ret < 0) {/* decoding error */
        SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
        job_item->err_num = ret;
        job_item->stored_flg = dec_data->dec_frame.bPicStored;
        job_item->err_pos = ERR_ISR;
#if USE_SW_PARSER
        job_item->parser_offset = sw_byte_offset(&dec_data->dec_handle->stream);
#endif

#if REV_PLAY && GOP_JOB_STOP_HANDLING
        if(job_item->gop_job_item){
            job_item->gop_job_item->err_flg = 1;
        }
#endif
        if(PRINT_ERR_IN_ISR(dbg_mode)){
            favcd_print_job_error(job_item);
        }

        if(DAMNIT_AT_ERR(dbg_mode)){
            favcd_dump_job_bs(job_item);
            H264Dec_DumpReg(dec_data->dec_handle, LOG_ERROR);
            FAVCD_DAMNIT("ISR job %d decode error: %d(%s)\n", job_item->engine, job_item->minor, job_item->job_id, ret, favcd_err_str(ret));
        }
    } else { 
        /* ret > 0. one frame is successfully decoded */
        DEBUG_M(LOG_INFO, job_item->engine, job_item->minor, "(%d,%d) ISR job %d done\n", job_item->engine, job_item->minor, job_item->job_id);
        job_item->stored_flg = dec_data->dec_frame.bPicStored;
        SET_JOB_STATUS(job_item, DRIVER_STATUS_WAIT_POC);
    }
    
    /* frame done or decoding error */

    dec_end = _FAVCD_JIFFIES;
    H264Dec_UnbindEngine(dec_data->dec_handle);

    mark_start = _FAVCD_JIFFIES;
    
    job_item->finishtime = jiffies;
    job_item->prof_finish_time = _FAVCD_JIFFIES;
#if EXC_HDR_PARSING_FROM_UTIL == 0
    mark_engine_finish(job_item->engine);
#endif
    set_engine_idle(job_item->engine);
    dec_data->curr_job = NULL;
#if REV_PLAY && GOP_JOB_STOP_HANDLING
    if(job_item->gop_job_item){
        job_item->gop_job_item->ongoing_flg = 0;
    }
#endif
    dec_data->engine = -1;
    
    mark_end = _FAVCD_JIFFIES;


    buf_start = _FAVCD_JIFFIES;
    PERF_MSG("ISR_buf_mgm >\n");
    if(job_item->stored_flg == 0){ 
        /* handle non-stored job via output/release list */
        __favcd_append_output_list(dec_data, job_item->buf_idx);
        __favcd_append_release_list(dec_data, job_item->buf_idx);//dec_data->dec_frame.ptReconstBuf->buffer_index);
    }
    
    /* NOTE: the order of the following functions must be fixed */
    favcd_set_out_property(dec_data, job_item);
    __favcd_process_output_list(dec_data, 0);
    __favcd_process_release_list(dec_data);
    __favcd_handle_unoutput_buffer(dec_data);

#if REV_PLAY
    __favcd_gop_job_post_process(job_item); /* FIXME: TBD */
#endif
    
    PERF_MSG("ISR_buf_mgm <\n");

    if(job_item->err_num != H264D_OK){
        favcd_error_handling(dec_data, job_item->err_num);
    }



    buf_end = _FAVCD_JIFFIES;


    trig_start = _FAVCD_JIFFIES;
    PERF_MSG("ISR_trig_wq >\n");
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "ISR trigger_callback\n");
    trigger_callback();
    
    DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "ISR calling work_scheduler()\n");
    work_scheduler(isr_prof_ptr);
    
    PERF_MSG("ISR_trig_wq <\n");
    trig_end = _FAVCD_JIFFIES;

    DEBUG_M(LOG_INFO, job_item->engine, job_item->minor, "(%d,%d) ISR return\n", job_item->engine, job_item->minor);

isr_ret:
    idx = isr_prof_ptr->isr_cnt % ISR_PROF_RECORD_NUM;
    isr_prof_ptr->isr_prof_rec[idx].isr_rec_dur  = rec_end - rec_start;
    isr_prof_ptr->isr_prof_rec[idx].isr_dec_dur  = dec_end - dec_start;
    isr_prof_ptr->isr_prof_rec[idx].isr_mark_dur = mark_end - mark_start;
    isr_prof_ptr->isr_prof_rec[idx].isr_buf_dur  = buf_end - buf_start;
    isr_prof_ptr->isr_prof_rec[idx].isr_trig_dur = trig_end - trig_start;

isr_ret2:
    
    
    PERF_MSG("ISR < %d\n", irq);
    TRACE_JOB('i');
    return IRQ_HANDLED;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
STATIC irqreturn_t favcd_int_handler(int irq, void *dev)
#else
STATIC irqreturn_t favcd_int_handler(int irq, void *dev, struct pt_regs *dummy)
#endif
{
    irqreturn_t ret;
#if DISABLE_IRQ_IN_ISR
    unsigned long flags;
#endif
#if CHK_ISR_DURATION    
    unsigned int isr_start_jiffies;
    unsigned int isr_end_jiffies;
    unsigned int dur;
    struct isr_prof_data *isr_prof_ptr;
    int idx;
    unsigned int eng_idx = (unsigned int)dev;
    
    isr_start_jiffies = FAVCD_JIFFIES;
#endif

#if DISABLE_IRQ_IN_ISR
    favcd_spin_lock_irqsave(&favc_dec_lock, flags);
#endif

    ret = favcd_int_process(irq, eng_idx);

#if CHK_ISR_DURATION
    isr_end_jiffies = FAVCD_JIFFIES;
    dur = isr_end_jiffies - isr_start_jiffies;
    PERF_MSG_N(PROF_ISR, "ISR %d %u\n", irq, dur);
    if(dur > 250){
        PERF_MSG_N(PROF_ISR, "ISR %d too long %u\n", irq, dur);
    }
    
    /* record for ISR profiling */
    isr_prof_ptr = &isr_prof_data[eng_idx];
    
    idx = isr_prof_ptr->isr_cnt % ISR_PROF_RECORD_NUM;

    isr_prof_ptr->isr_prof_rec[idx].isr_start = isr_start_jiffies;
    isr_prof_ptr->isr_prof_rec[idx].isr_period = isr_start_jiffies - isr_prof_ptr->last_isr_start;
    isr_prof_ptr->last_isr_start = isr_start_jiffies;
    
    isr_prof_ptr->isr_cnt++;
    isr_prof_ptr->isr_prof_rec[idx].isr_dur = dur;
    if(isr_prof_ptr->max_isr_dur < dur){
        isr_prof_ptr->max_isr_dur = dur;
    }
    if(isr_prof_ptr->min_isr_dur > dur){
        isr_prof_ptr->min_isr_dur = dur;
    }
#endif


#if DISABLE_IRQ_IN_ISR
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);
#endif

	return ret;
}


struct video_entity_ops_t favcd_ops = {
    putjob:      &favcd_putjob,
    stop:        &favcd_stop,
    queryid:     &favcd_queryid,
    querystr:    &favcd_querystr,
    getproperty: &favcd_getproperty,
};    


struct video_entity_t favcd_entity = {
    type:       TYPE_DECODER,
    name:       "favcd",
    engines:    1, /* ENTITY_ENGINES, */
    minors:     ENTITY_MINORS,
    ops:        &favcd_ops
};

#ifdef MULTI_FORMAT_DECODER
#ifdef PARSING_RESOLUTION
int favcd_multidec_putjob(void *parm, int width, int height);
#else // PARSING_RESOLUTION
int favcd_multidec_putjob(void *parm);
#endif // PARSING_RESOLUTION
int favcd_multidec_stop(void *parm, int engine, int minor);
int favcd_multidec_getproperty(void *parm, int engine, int minor, char *string);

struct decoder_entity_t favcd_multidec_entity = {
    decoder_type: TYPE_H264,
    put_job:      &favcd_multidec_putjob,
    stop_job:     &favcd_multidec_stop,
    get_property: &favcd_multidec_getproperty,
};

#ifdef PARSING_RESOLUTION
int favcd_multidec_putjob(void *parm, int width, int height)
#else
int favcd_multidec_putjob(void *parm)
#endif
{
    return favcd_putjob(parm);
}

int favcd_multidec_stop(void *parm, int engine, int minor)
{
    return favcd_stop(parm, engine, minor);
}

int favcd_multidec_getproperty(void *parm, int engine, int minor, char *string)
{
    return favcd_getproperty(parm, engine, minor, string);
}

#endif // MULTI_FORMAT_DECODER



void favcd_get_mv_16x16_from_mbinfo(u32 *buf, u32 size, u8 *mbinfo_buf, unsigned int mb_cnt)
{
    int i;
    u32 *dst_ptr = buf;
    u32 *src_ptr = (u32 *)(mbinfo_buf + 128);
    u32 dst_bytes = 0;

#if 0
    printk("src:\n");
    for(i = 0; i < 32; i++){
        printk("%d:%08X\n", i, src_ptr[i]);
    }
#endif
    
    while((mb_cnt > 0) && (dst_bytes + 32 < size)){
        for(i = 0; i < 8; i++){
            dst_ptr[i] = src_ptr[i * 4];
        }
        dst_ptr += 8;    /* move to the next 32 bytes */
        src_ptr += 64;   /* move to the next 256 bytes */
        dst_bytes += 32;
        mb_cnt--;
    }

#if 1 
    /* fill-up the rest of the buffer */
    if(mb_cnt > 0){
        u32 words_left = (size - dst_bytes) / 4;
        for(i = 0; i < words_left; i++){
            dst_ptr[i] = src_ptr[i * 4];
        }
    }
#endif

#if 0
    printk("dst:\n");
    dst_ptr = buf;
    for(i = 0; i < 8; i++){
        printk("%d:%08X\n", i, dst_ptr[i]);
    }
#endif

}

/*
 * Geting MB info (including motion vector) of a specified channel
 */
int favcd_get_mbinfo(u32 minor, FAVCD_MBINFO_FMT_T fmt, u32 *buf, u32 size)
{
    struct favcd_data_t *dec_data;
    struct favcd_job_item_t *job_item;
    unsigned long flags;
    unsigned int width;
    unsigned int height;
    unsigned int output_size = 0;
    unsigned int output_flag = 0;
    int i;

    if(minor < 0  || minor >= chn_num){
        DEBUG_M(LOG_ERROR, 0, 0, "invalid minor number: %d\n", minor);
        return -1;
    }

    if(fmt >= FAVCD_MBINFO_FMT_MAX){
        DEBUG_M(LOG_ERROR, 0, 0, "invalid fmt: %d\n", fmt);
        return -1;
    }

    dec_data = private_data + minor;

    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for dec_data and job status */

    DEBUG_M(LOG_TRACE, job_item->engine, job_item->minor, "favcd_get_mbinfo: minor:%d fmt:%d size:%d\n", minor, fmt, size);

    for(i = 0; i < MAX_DEC_BUFFER_NUM; i++) {
        if(dec_data->dec_buf[i].is_used){
            job_item = favcd_get_linked_job_of_buf(dec_data, i);
            if(job_item == NULL){
                continue;
            }
            if ((job_item->status == DRIVER_STATUS_FINISH) || 
                (job_item->status == DRIVER_STATUS_KEEP) || 
                (job_item->status == DRIVER_STATUS_WAIT_POC)) {

                if(dec_data->dec_buf[i].mbinfo_va == 0){
                    continue;
                }

                DEBUG_M(LOG_TRACE, 0, 0, "(%d) get mv from job %d\n", job_item->minor, job_item->job_id);
                
                switch(fmt){
                    case FAVCD_MBINFO_FMT_RAW:
                        width = job_item->dst_dim >> 16;
                        height = job_item->dst_dim & 0xFFFF;
                        
                        /* determine output size */
                        if(size > (width * height)){
                            output_size = (width * height);
                        }else{
                            output_size = size;
                        }
                        
                        memcpy(buf, (char *)dec_data->dec_buf[i].mbinfo_va, output_size);
                        output_flag = 1;
                        break;
                    case FAVCD_MBINFO_FMT_16X16_BLK:
                        width = job_item->dst_dim >> 16;
                        height = job_item->dst_dim & 0xFFFF;
                        favcd_get_mv_16x16_from_mbinfo(buf, size, (char *)dec_data->dec_buf[i].mbinfo_va, (width * height) >> 8);
                        output_flag = 1;
                        break;
                    default:
                        break;
                }

                /* stop finding the next job if mbinfo is copied */
                if(output_flag)
                    break;
                    
                }
        }
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for dec_data and job status */

    if(output_flag == 0){
        DEBUG_M(LOG_WARNING, 0, 0, "(%d) no mv info available\n", minor);
        return -1;
    }
    
    return 0;
}

EXPORT_SYMBOL(favcd_get_mbinfo);


/*
 * Show dirver info message at module loading
 * NOTE: The meesage can be quaried via config proc node
 */
STATIC void favcd_msg(void)
{
    char __plt_str[16] = FTMCP300_PLT_STR;
    if(strcmp(plt_str, FTMCP300_PLT_STR) != 0){
        snprintf(__plt_str, sizeof(__plt_str), "%s/" FTMCP300_PLT_STR, plt_str);
    }
#if ENTITY_ENGINES == 1
    snprintf(favcd_ver_str, sizeof(favcd_ver_str), 
        "FAVC Decoder IRQ mode(%d) PLT:%s, version %d.%d.%d.%d built @ %s %s", FTMCP300_DEC0_IRQ, __plt_str, 
        H264D_VER_MAJOR, H264D_VER_MINOR, H264D_VER_MINOR2, H264D_VER_BRANCH,  __DATE__, __TIME__);
#elif ENTITY_ENGINES == 2/* ENTITY_ENGINES == 2 */
    snprintf(favcd_ver_str, sizeof(favcd_ver_str), 
        "FAVC Decoder IRQ mode(%d, %d) PLT:%s, version %d.%d.%d.%d built @ %s %s", FTMCP300_DEC0_IRQ, FTMCP300_DEC1_IRQ, __plt_str,
        H264D_VER_MAJOR, H264D_VER_MINOR, H264D_VER_MINOR2, H264D_VER_BRANCH,  __DATE__, __TIME__);
#else
    #error invalid ENTITY_ENGINES
#endif
    printk("%s\n", favcd_ver_str);
}



/*
 * check chip version and load chip/platform dependent parameters
 */
STATIC int __init favcd_chk_chip_ver(void)
{
    unsigned int ver;
    unsigned int chip_ver;

    ver = ftpmu010_get_attr(ATTR_TYPE_CHIPVER);
    
    chip_ver = (ver >> 16) & chip_ver_mask;

    if(chip_ver_limit){
        /* check chip version */
        if(chip_ver != (chip_ver_limit & chip_ver_mask)){
            DEBUG_M(LOG_ERROR, 0, 0, "chip version incorrect (driver build for %04X, chip version %04X mask %04X)\n", chip_ver_limit, chip_ver, chip_ver_mask);
            return -EINVAL;
        }

        /* set parameters for specific platform */
        switch(chip_ver_limit){
            case 0x8210: /* insmod with chip_ver_limit=0x8210 chip_ver_mask=0x0 to use parameters for 8210 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_8210;
                max_ref1_width = MAX_REF1_WIDTH_8210;
                engine_num = -1; /* enable all engines */
                plt_str = FTMCP300_PLT_STR_8210;
                break;
            case 0x8280: /* insmod with chip_ver_limit=0x8280 chip_ver_mask=0x0 to use parameters for 8287 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_8287;
                max_ref1_width = MAX_REF1_WIDTH_8287;
                engine_num = 0; /* enable engine 0 only */
                plt_str = FTMCP300_PLT_STR_8287;
                break;
            case 0xA369: /* insmod with chip_ver_limit=0x8280 chip_ver_mask=0x0 to use parameters for A369 */
                vcache_one_ref = VCACHE_ONE_REFERENCE_A369;
                max_ref1_width = MAX_REF1_WIDTH_A369;
                engine_num = 0; /* enable engine 0 only */
                plt_str = FTMCP300_PLT_STR_A369;
                break;
            default:
                break;
        }
    }

    if(chip_ver == 0x8210){ /* enable workaround for 8210 version A */
        unsigned int eco_ver;
        eco_ver = ftpmu010_get_attr(ATTR_TYPE_PMUVER);
        if(eco_ver == 0){
            mcp300_sw_reset = 1;
        }
    }

    return 0;
}


/*
 * check module parameter
 */
STATIC int __init favcd_chk_module_param(void)
{
    if(chn_num < 1 || chn_num > MAX_CHN_NUM){
        printk("chn_num exceeds the valid range: 1 ~ %d: %d\n", MAX_CHN_NUM, chn_num);
        return -EFAULT;
    }
   
    /* check value of module parameters */
    if(mcp300_max_buf_num < -1 || mcp300_max_buf_num > MAX_DEC_BUFFER_NUM){
        printk("mcp300_max_buf_num (%d) exceed valid range: -1~%d", mcp300_max_buf_num, MAX_DEC_BUFFER_NUM);
        return -EFAULT;
    }

    if(max_pp_buf_num < -1 || max_pp_buf_num > MAX_DEC_BUFFER_NUM){
        printk("max_pp_buf_num (%d) exceed valid range: -1~%d", max_pp_buf_num, MAX_DEC_BUFFER_NUM);
        return -EFAULT;
    }

#if CONFIG_ENABLE_VCACHE == 0
    if((mcp300_b_frame == 0) && (output_mbinfo == 0)){
        printk("error: mbinfo must be allocated at VCACHE disabled\n");
        return -EINVAL;
    }
#endif //CONFIG_ENABLE_VCACHE

    return 0;
}


/*
 * initialize profiling data
 */
STATIC void __init favcd_init_prof_data(void)
{
    int engine;
    memset(&pp_prof_data, 0, sizeof(pp_prof_data));
    sema_init(&pp_prof_data.sem, 1);
    memset(&cb_prof_data, 0, sizeof(cb_prof_data));
    sema_init(&cb_prof_data.sem, 1);

    memset(&isr_prof_data, 0, sizeof(isr_prof_data));
    memset(&job_prof_data, 0, sizeof(job_prof_data));
    
    for(engine = 0; engine < ENTITY_ENGINES; engine++){
        isr_prof_data[engine].max_isr_dur = 0;
        isr_prof_data[engine].min_isr_dur = 0xFFFFFFFF;
    }

    memset(eng_util, 0, sizeof(eng_util));
}


/*
 * stop all channel and wait until all jobs are done
 */
int favcd_flush_jobs(void)
{
    const int max_poll_stop_cnt = 10;
    int poll_stop_cnt;
    int minor;
    int stopping_done_flg;
    int ret;


    for(poll_stop_cnt = 0; poll_stop_cnt < max_poll_stop_cnt; poll_stop_cnt++){
        /* call stop for each channel */
        DEBUG_M(LOG_INFO, 0, 0, "flush jobs cnt:%d\n", poll_stop_cnt);
        for (minor = 0; minor < chn_num; minor++) {
            ret = favcd_stop(NULL, 0, minor);
            if(ret){
                DEBUG_M(LOG_ERROR, 0, minor, "error when stopping channel %d\n", minor);
            }
        }

        /* wait for a while: to avoid race */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(callback_period) * 20);

        /* check if stopping all channel is done */
        stopping_done_flg = 1;
        for (minor = 0; minor < chn_num; minor++) {
            if(private_data[minor].stopping_flg == 1){
                stopping_done_flg = 0;
                break;
            }
        }

        /* return if stopping is done and no job is pending */
        if(stopping_done_flg && list_empty(&favcd_global_head)) {
            return 0;
        }
        
    }

    return -1; /* timeout */
}


static void __vg_global_cleanup(void)
{

    if(favcd_cb_wq) {
#if USE_VG_WORKQ
        vp_flush_workqueue(favcd_cb_wq);
        vp_destroy_workqueue(favcd_cb_wq);
#else //!USE_VG_WORKQ
        destroy_workqueue(favcd_cb_wq);
#endif //!USE_VG_WORKQ
    }

    if(favcd_prepare_wq) {
#if USE_VG_WORKQ
        vp_flush_workqueue(favcd_prepare_wq);
        vp_destroy_workqueue(favcd_prepare_wq);
#else //!USE_VG_WORKQ
        destroy_workqueue(favcd_prepare_wq);
#endif //!USE_VG_WORKQ
    }

    if(private_data) {
        kfree(private_data);
    }
        
    if(property_record) {
        kfree(property_record);
    }

#if REV_PLAY
    if(favcd_gop_minor_head) {
        kfree(favcd_gop_minor_head);
    }
#endif
    if(favcd_prepare_minor_head) {
        kfree(favcd_prepare_minor_head);
    }
    if(favcd_fail_minor_head) {
        kfree(favcd_fail_minor_head);
    }
    if(favcd_minor_head) {
        kfree(favcd_minor_head);
    }
    if (job_cache){
        kmem_cache_destroy(job_cache);
    }

	// tire
	if (fail_list_idx) {
		kfree(fail_list_idx);	
	}
	
    unregister_printout_notifier(favcd_log_printout_handler);
    unregister_panic_notifier(favcd_panic_printout_handler);
}


static void __vg_channel_cleanup(int chn)
{
    if(private_data[chn].dec_handle){
        H264Dec_Release(private_data[chn].dec_handle);
        private_data[chn].dec_handle = NULL;
    }
}


static void __vg_engine_cleanup(int engine)
{
#if USE_LINK_LIST
    int i;
#endif
    if (favcd_engine_info[engine].irq_num) {
        DEBUG_M(LOG_INFO, 0, 0, "free irq[%d] %d\n", engine, favcd_engine_info[engine].irq_num);
        free_irq(favcd_engine_info[engine].irq_num, (void *)engine);
    }
    
#if USE_LINK_LIST
    for(i = 0; i < LL_NUM; i++){
        int ll_idx = engine * LL_NUM + i;
        if (favcd_engine_info[engine].codec_ll[i]) {
            free_buffer(&ll_buf[ll_idx]);
            favcd_engine_info[engine].codec_ll[i] = NULL;
        }
    }
#endif
    release_per_engine_buffer(engine);

    if (favcd_engine_info[engine].pu32BaseAddr) {
        iounmap((void __iomem *)favcd_engine_info[engine].pu32BaseAddr);
    }

    
    if (favcd_engine_info[engine].pu32VcacheBaseAddr) {
        iounmap((void __iomem *)favcd_engine_info[engine].pu32VcacheBaseAddr);
    }
    
}


/*
 * VG driver module cleanup
 */
void favcd_vg_cleanup(void)
{
    int engine;
    int minor;
    int ret;

    if(list_empty(&favcd_global_head) != 0){
        ret = favcd_flush_jobs();
        if(ret){
            DEBUG_M(LOG_INFO, 0, 0, "wait unfinished job timeout\n");
        }
    }
    
#ifdef MULTI_FORMAT_DECODER
    decoder_deregister(TYPE_H264);
#else // !MULTI_FORMAT_DECODER
    video_entity_deregister(&favcd_entity);
#endif //!MULTI_FORMAT_DECODER


    for (engine = 0; engine < ENTITY_ENGINES; engine++) {
        __vg_engine_cleanup(engine);
    }

    pf_mcp300_clk_off();
    pf_mcp300_clk_exit();
    favcd_proc_close();

    if(private_data) {
        for (minor = 0; minor < chn_num; minor++) {
            __vg_channel_cleanup(minor);
        }
    }

    if(fkmalloc_cnt != fkfree_cnt){
        printk("kmalloc/kfree error detected:%d/%d\n", fkmalloc_cnt, fkfree_cnt);
    }

    if (cb_thread) {
        kthread_stop(cb_thread);
        /* wait thread to be terminated */
        while (cb_thread_ready) {
            msleep(10);
        }
    }
    
    if (pp_thread) {
        kthread_stop(pp_thread);
        /* wait thread to be terminated */
        while (pp_thread_ready) {
            msleep(10);
        }
    }

    __vg_global_cleanup();
    
}


static int __init __vg_global_init(void)
{
    int i;
    int ret;

    __favcd_init_list_head(&favcd_global_head);
    __favcd_init_list_head(&favcd_callback_head);
    __favcd_init_list_head(&favcd_standby_head);
    __favcd_init_list_head(&favcd_ready_head);
#if USE_SINGLE_PREP_LIST
    __favcd_init_list_head(&favcd_prepare_head);
#endif
    
    ret = register_printout_notifier(favcd_log_printout_handler);
    if(ret < 0) {
        printk("error: ftmcp300 register log system printout notifier failed!\n");
        goto err_ret;
    }

    ret = register_panic_notifier(favcd_panic_printout_handler);
    if(ret < 0) {
        printk("error: ftmcp300 register log system panic notifier failed!\n");
        goto err_ret;
    }


    job_cache = kmem_cache_create(cache_name, sizeof(struct favcd_job_item_t), 0, 0, NULL);
    if(!job_cache){
        printk("error: ftmcp300 creates cache fail!\n");
        goto err_ret;
    }

	// tire
	fail_list_idx = kzalloc(sizeof(char) * chn_num, GFP_KERNEL);

    favcd_minor_head = kzalloc(sizeof(struct list_head) * chn_num, GFP_KERNEL);
    favcd_fail_minor_head = kzalloc(sizeof(struct list_head) * chn_num, GFP_KERNEL);
    favcd_prepare_minor_head = kzalloc(sizeof(struct list_head) * chn_num, GFP_KERNEL);
#if REV_PLAY
    favcd_gop_minor_head = kzalloc(sizeof(struct list_head) * chn_num, GFP_KERNEL);
#endif
    property_record = kzalloc(sizeof(struct property_record_t) * chn_num, GFP_KERNEL);
    private_data = kzalloc(sizeof(private_data[0]) * chn_num, GFP_KERNEL);       

    if(favcd_minor_head == 0 || favcd_fail_minor_head == 0 || 
#if REV_PLAY
        favcd_gop_minor_head == 0 ||
#endif
        favcd_prepare_minor_head == 0 || property_record == 0 || private_data == 0) 
    {
        printk("error: ftmcp300 failed to allocate memory\n");
        goto err_ret;
    }

    if(property_map_size >= MAX_RECORD){
        printk("warning: number of porperty defined in mcp300(%d) > MAX_RECORD(%d)\n", property_map_size, MAX_RECORD);
    }

    for (i = 0; i < chn_num; i++) {
        __favcd_init_list_head(&favcd_minor_head[i]);
        __favcd_init_list_head(&favcd_fail_minor_head[i]);
        __favcd_init_list_head(&favcd_prepare_minor_head[i]);
		fail_list_idx[i] = 0;
#if REV_PLAY
        __favcd_init_list_head(&favcd_gop_minor_head[i]);
#endif
        DEBUG_M(LOG_INFO, 0, 0, "init list %d done\n", i);
    }
    DEBUG_M(LOG_INFO, 0, 0, "init list done\n");

    spin_lock_init(&favc_dec_lock);
    sema_init(&favc_dec_sem, 1);

#if USE_VG_WORKQ
    VP_INIT_WORK(&process_callback_work, (void *)callback_scheduler);
    favcd_cb_wq = vp_create_workqueue("favcd_cb");
#else // !USE_VG_WORKQ
    favcd_cb_wq = create_workqueue("favcd_cb");
#endif // !USE_VG_WORKQ
    if (!favcd_cb_wq) {
        printk("error: failed to create workqueue favcd_cb\n");
        goto err_ret;
    }


#if USE_VG_WORKQ
    VP_INIT_WORK(&process_prepare_work, (void *)prepare_job);
    favcd_prepare_wq = vp_create_workqueue("favcd_prep");
#else // !USE_VG_WORKQ
    favcd_prepare_wq = create_workqueue("favcd_prep");
#endif // !USE_VG_WORKQ
    if (!favcd_prepare_wq) {
        printk("error: failed to create workqueue favcd_prep\n");
        goto err_ret;
    }
    
    return 0;
    
err_ret:
    return -1;

}

static int __init __vg_channel_init(int chn)
{
    FAVCD_DEC_INIT_PARAM stParam;
    stParam.pfnMalloc = fkmalloc;
    stParam.pfnFree = fkfree;
    stParam.pfnDamnit = favcd_damnit;
#if EXC_HDR_PARSING_FROM_UTIL
    stParam.pfnMarkEngStart = mark_engine_start;
#else
    stParam.pfnMarkEngStart = NULL;//mark_engine_start;
#endif
    
    private_data[chn].engine = -1;
    private_data[chn].minor = chn;
    private_data[chn].need_init_flg = INIT_NORMAL;
    private_data[chn].out_fr_cnt = 0;

    private_data[chn].dec_handle = H264Dec_Create(&stParam, chn);
    if(private_data[chn].dec_handle == NULL){
        printk("error: ftmcp300 failed to allocate memory for dec_handle\n");
        goto err_ret;
    }
    
    return 0;
    
err_ret:
    return -1;
    
}

static int __init __vg_engine_init(int engine)
{
#if USE_LINK_LIST
    int i;
#endif
    
    /* mmap phy register address to virtual address space */
    if(use_ioremap_wc){
        favcd_engine_info[engine].pu32BaseAddr = ioremap_wc(favcd_base_address_pa[engine], PAGE_ALIGN(favcd_base_address_size[engine]));
    }else{
        favcd_engine_info[engine].pu32BaseAddr = ioremap_nocache(favcd_base_address_pa[engine], PAGE_ALIGN(favcd_base_address_size[engine]));
    }
    if (unlikely(!favcd_engine_info[engine].pu32BaseAddr)){
        printk("failed to ioremap for mcp300 register\n");
        goto err_ret;
    }
    favcd_engine_info[engine].u32phy_addr = favcd_base_address_pa[engine];

    if(use_ioremap_wc){
        favcd_engine_info[engine].pu32VcacheBaseAddr = ioremap_wc(favcd_vcache_address_pa[engine], PAGE_ALIGN(favcd_vcache_address_size[engine]));
    }else{
        favcd_engine_info[engine].pu32VcacheBaseAddr = ioremap_nocache(favcd_vcache_address_pa[engine], PAGE_ALIGN(favcd_vcache_address_size[engine]));
    }
    if (unlikely(!favcd_engine_info[engine].pu32VcacheBaseAddr)){
        printk("failed to ioremap for mcp300 vcache register\n");
        goto err_ret;
    }
    favcd_engine_info[engine].u32vc_phy_addr = favcd_vcache_address_pa[engine];

    /* allocate per-engine buffers */
    if(allocate_per_engine_buffer(engine, mcp300_max_width)){
        printk("failed to allocate common buffers\n");
        goto err_ret;
    }

#if USE_LINK_LIST
    /* init linked list data structure */
    for(i = 0; i < LL_NUM; i++){
        int ll_idx = engine * LL_NUM + i;
        struct codec_link_list *codec_ll_ptr = (struct codec_link_list *)ll_buf[ll_idx].addr_va;
        
        codec_ll_ptr->ll_pa = ll_buf[ll_idx].addr_pa;
        ll_init(codec_ll_ptr, ll_idx * LL_MAX_JOBS);
#if SW_LINK_LIST == 2
        ll_set_base_addr(codec_ll_ptr, (unsigned int)favcd_engine_info[engine].pu32BaseAddr, (unsigned int)favcd_engine_info[engine].pu32VcacheBaseAddr);
#endif
        favcd_engine_info[engine].codec_ll[i] = codec_ll_ptr;
    }
#endif /* USE_LINK_LIST */

    if (request_irq(irq_no[engine], &favcd_int_handler, 0, irq_name[engine], (void *)engine) != 0) {
        printk("unable request IRQ %d\n", irq_no[engine]);
        goto err_ret;
    }
    favcd_engine_info[engine].irq_num = irq_no[engine];
    DEBUG_M(LOG_INFO, 0, 0, "irq[%d] no = %d\n", engine, favcd_engine_info[engine].irq_num);

    /* init engine info */
    favcd_engine_info[engine].engine = engine;
    favcd_engine_info[engine].chn_idx = -1;
    favcd_engine_info[engine].intra_vaddr = mcp300_intra_buffer[engine].addr_va;
    favcd_engine_info[engine].intra_paddr = mcp300_intra_buffer[engine].addr_pa;
    favcd_engine_info[engine].intra_length = mcp300_max_width * 2;  /* one MB row * 32 byte */
    favcd_engine_info[engine].vcache_ilf_enable = 0;

    H264Dec_ResetEgnine(&favcd_engine_info[engine]);

    return 0;
    
err_ret:
    return -1;
}


/*
 * VG driver module init
 */
int __init favcd_vg_init(void)
{
    int engine, minor;
    int ret;
    
    ret = favcd_chk_chip_ver();
    if(ret){
        return ret;
    }

    ret = favcd_chk_module_param();
    if(ret){
        return ret;
    }

    favcd_init_prof_data();

    memset(favcd_engine_info, 0, sizeof(favcd_engine_info));

    ret = __vg_global_init();
    if(ret < 0){
        goto err_ret;
    }
        

    for (minor = 0; minor < chn_num; minor++) {
        ret = __vg_channel_init(minor);
        if(ret < 0){
            goto err_ret;
        }
    }
    

    if (favcd_proc_init() < 0) {
        goto err_ret;
    }

    /* stop reset and enable clock */
    ret = pf_mcp300_clk_on();
    if(ret){
        printk("error: failed to enable mcp300 clock and stop reset\n");
        goto err_ret;
    }

    for (engine = 0; engine < ENTITY_ENGINES; engine++) {
        if(engine_num != -1 && engine != engine_num){ /* skip engine that is not enabled */
            continue;
        }
        ret = __vg_engine_init(engine);
        if(ret < 0){
            goto err_ret;
        }
    }

#if USE_KTHREAD
    init_waitqueue_head(&cb_thread_wait);
    init_waitqueue_head(&pp_thread_wait);
    cb_thread = kthread_create(ftmcp300_cb_thread, NULL, "ftmcp300_cb_thread");
    if (IS_ERR(cb_thread))
        panic("%s, create cb_thread fail ! \n", __func__);

    pp_thread = kthread_create(ftmcp300_pp_thread, NULL, "ftmcp300_pp_thread");
    if (IS_ERR(pp_thread))
        panic("%s, create cb_thread fail ! \n", __func__);

    wake_up_process(cb_thread);
    wake_up_process(pp_thread);
#endif

#ifdef MULTI_FORMAT_DECODER
    if(decoder_register(&favcd_multidec_entity, TYPE_H264)) {
        printk("%s:Failed to register multi-format decoder.\n", __FUNCTION__);
        goto err_ret;
    }
#else // !MULTI_FORMAT_DECODER
    favcd_entity.minors = chn_num;
    if(video_entity_register(&favcd_entity)) {
        printk("%s:Failed to register video entity.\n", __FUNCTION__);
        goto err_ret;
    }
#endif // !MULTI_FORMAT_DECODER


    favcd_msg();

    return 0;
    
err_ret:
    favcd_vg_cleanup();
    return -EFAULT;
}

#if USE_KTHREAD
static int ftmcp300_cb_thread(void *data)
{
    int status;

    /* ready */
    cb_thread_ready = 1;

    do {
        status = wait_event_timeout(cb_thread_wait, cb_wakeup_event, msecs_to_jiffies(callback_period));
        if (status == 0)
            continue;   /* timeout */
        cb_wakeup_event = 0;
        /* callback process */
        callback_scheduler();
    } while(!kthread_should_stop());

    cb_thread_ready = 0;
    return 0;
}

static void ftmcp300_cb_wakeup(void)
{
    if (debug_value == 0xFFFFFFFF)
        printm("FD", "ftmcp300_cb_wakeup.... \n");

    cb_wakeup_event = 1;
    wake_up(&cb_thread_wait);
}
static int ftmcp300_pp_thread(void *data)
{
    int status;

    /* ready */
    pp_thread_ready = 1;

    do {
        status = wait_event_timeout(pp_thread_wait, pp_wakeup_event, msecs_to_jiffies(prepare_period));
        if (status == 0)
            continue;   /* timeout */
        pp_wakeup_event = 0;
        /* callback process */
        prepare_job();
    } while(!kthread_should_stop());

    pp_thread_ready = 0;
    return 0;
}

static void ftmcp300_pp_wakeup(void)
{
    if (debug_value == 0xFFFFFFFF)
        printm("FD", "ftmcp300_pp_wakeup.... \n");

    pp_wakeup_event = 1;
    wake_up(&pp_thread_wait);
}
#endif
