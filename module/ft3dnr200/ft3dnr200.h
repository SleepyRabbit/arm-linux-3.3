#ifndef __FT3DNR_H__
#define __FT3DNR_H__

#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <mach/gm_jiffies.h>
#include <ft3dnr200_isp_api.h>

#include "ft3dnr200_mem.h"
#include "ft3dnr200_ctrl.h"
#include "ft3dnr200_mrnr.h"
#include "ft3dnr200_tmnr.h"
#include "ft3dnr200_ee.h"
#include "ft3dnr200_dma.h"

#ifdef CONFIG_VIDEO_FASTTHROUGH
#define DRV_CFG_USE_TASKLET
#else
#define DRV_CFG_USE_KTHREAD
#endif

#define VERSION                 "2016_0823_1650"
#define MAX_CH_NUM              8      // how many channels this module can support in default
#define MAX_LL_BLK              4
#define MAX_MRNR_BLK            2
#define MAX_SP_BLK              2

#define FT3DNR_REG_MEM_LEN      0x210

// LL BLOCK
#define FT3DNR_LL_MEM_BASE      0x400
#define FT3DNR_LL_MEM_SIZE      0x800
#define FT3DNR_LL_MEM_BLOCK     0xC0    // 192 bytes
#define FT3DNR_LL_MEM           FT3DNR_LL_MEM_BASE+FT3DNR_LL_MEM_SIZE //0xC00
// MRNR
#define FT3DNR_MRNR_MEM_BASE    0xA00
#define FT3DNR_MRNR_MEM_BLOCK   0x100   // 256 bytes
// SHARPEN
#define FT3DNR_SP_MEM_BASE      0x700
#define FT3DNR_SP_MEN_BLOCK     0x180   // 384 bytes

#define FT3DNR_LLI_TIMEOUT      2000    ///< 2 sec

#define VARBLK_WIDTH            128
#define VARBLK_HEIGHT           32
#define WIDTH_LIMIT		        (VARBLK_WIDTH - 2)
#define HEIGHT_LIMIT	        (VARBLK_HEIGHT - 2)
#define VAR_BIT_COUNT           8
#define TH_RETIO                7

/* GM8136 supports maximum HD resolution with 30 fps */
#define FRM_HD_WIDTH            1920
#define FRM_HD_HEIGHT           1088

#define MOT_BUF_SIZE            66*8
#define DEF_MOTION_TH_MULT      2
#define DEF_MRNR_NR_STR         128
#define DEF_EE_SP_STR           128

#define FT3DNR_SRC_ADDR         THDNR_DMA_SRC_ADDR
#define FT3DNR_DST_ADDR         THDNR_DMA_DES_ADDR
#define FT3DNR_VAR_ADDR         THDNR_DMA_VAR_ADDR
#define FT3DNR_MOT_ADDR         THDNR_DMA_MOT_ADDR
#define FT3DNR_EE_ADDR          THDNR_DMA_EE_ADDR
#define FT3DNR_REF_R_ADDR       THDNR_DMA_REF_R_ADDR
#define FT3DNR_REF_W_ADDR       THDNR_DMA_REF_W_ADDR

#define FT3DNR_PROC_NAME        "thdnr200"
#define DRIVER_NAME             "ft3dnr200"

#define DRV_SPIN_LOCK(FLAGS)    spin_lock_irqsave(&priv.lock, FLAGS)
#define DRV_SPIN_UNLOCK(FLAGS)  spin_unlock_irqrestore(&priv.lock, FLAGS)
#define DRV_SEMA_LOCK           down(&priv.sema_lock)
#define DRV_SEMA_UNLOCK         up(&priv.sema_lock)

#define ALIGN16_UP(x)           (((x + 15) >> 4) << 4)
#define MAX_LINE_CHAR           256
#define MAX_RES_IDX             (23 + 5)    // extend 5 reserved space to add new resolution from vg config
#define GMLIB_FILENAME          "gmlib.cfg"
#define ISP_INFO_FILENAME       "/proc/isp328/info"

#define JOB_FLOW_DBG(fmt,args...)   if(dump_job_flow)printk("<JOB_FLOW> "fmt,##args)

#define ISP_USE_ZERO
#undef ISP_USE_ZERO

typedef enum {
#if defined(ISP_USE_ZERO)
    SRC_TYPE_ISP        = 0,
    SRC_TYPE_DECODER    = 1,
#else
    SRC_TYPE_DECODER    = 0,
    SRC_TYPE_ISP        = 1,
#endif
    SRC_TYPE_SENSOR     = 2
} job_source_type_t;

struct res_base_info_t {
    char name[7];
    int width;
    int height;
};

typedef struct ft3ndr_ctrl {
    bool        spatial_en;     // enable spatial noise reduction
    bool        temporal_en;    // enable temporal noise reduction
    bool        tnr_learn_en;   // enable temporal strength learning
    bool        ee_en;          // enable edge enhancement
    bool        tnr_rlt_w;      // write out TMNR result as next frame reference
} ft3dnr_ctrl_t;

typedef struct ft3dnr_dim {
    u16                 src_bg_width;
    u16                 src_bg_height;
    u16                 src_x;
    u16                 src_y;
    u16                 nr_width;
    u16                 nr_height;
    u16                 org_width;
    u16                 org_height;
} ft3dnr_dim_t;

typedef struct ft3dnr_dma {
    u32                 dst_addr;
    u32                 src_addr;
    u32                 var_addr;
    u32                 ee_addr;
    u32                 mot_addr;
    u32                 ref_r_addr;
    u32                 ref_w_addr;
} ft3dnr_dma_t;

typedef struct ft3dnr_param {
    unsigned short      src_fmt;
    unsigned short      dst_fmt;
    ft3dnr_ctrl_t       ctrl;
    ft3dnr_dim_t        dim;
    ft3dnr_dma_t        dma;
    mrnr_param_t        mrnr;
    tmnr_param_t        tmnr;
	sp_param_t          sp;
    int                 mrnr_id;
    int                 tmnr_id;
    int                 sp_id;
    int                 src_type;
} ft3dnr_param_t;

enum {
    FT3DNR200_DEV_0,
    FT3DNR200_DEV_MAX
};

typedef enum {
    TYPE_FIRST          = 0x1,      // job from VG is more than 1 job in ft3dnr, set as first job
    TYPE_MIDDLE         = 0x2,      // job from VG is more than 3 job in ft3dnr, job is neither first job nor last job
    TYPE_LAST           = 0x4,      // job from VG is more than 1 job in ft3dnr, set as last job
    TYPE_ALL            = 0x8,      // job from VG is only 1 job in ft3dnr
    TYPE_TEST
} job_perf_type_t;

typedef enum {
    SINGLE_JOB          = 0x1,      // single job
    MULTI_JOB           = 0x2,      // multi job
    NONE_JOB
} job_type_t;

/* job status */
typedef enum {
    JOB_STS_QUEUE       = 0x1,      // wait for adding to link list memory
    JOB_STS_SRAM        = 0x2,      // add to link list memory, but not change last job's "next link list pointer" to this job
    JOB_STS_ONGOING     = 0x4,      // already change last job's "next link list pointer" to this job
    JOB_STS_DONE        = 0x8,      // job finish
    JOB_STS_FLUSH       = 0x10,     // job to be flush
    JOB_STS_DONOTHING   = 0x20,     // job do nothing
    JOB_STS_TEST
} job_status_t;

typedef enum {
    MOTION_TH_TL        = 0, //top_lft_th
    MOTION_TH_TN        = 2, //top_nrm_th
    MOTION_TH_TR        = 4, //top_rgt_th
    MOTION_TH_NL        = 6, //nrm_lft_th
    MOTION_TH_NN        = 8, //nrm_nrm_th
    MOTION_TH_NR        = 10, //nrm_rgt_th
    MOTION_TH_BL        = 12, //bot_lft_th
    MOTION_TH_BN        = 14, //bot_nrm_th
    MOTION_TH_BR        = 16  //bot_rgt_th
} motion_th_t;

/*
 * link list block info
 */
typedef struct ll_blk_info {
    u32     status_addr;
    u8      blk_num;
    u8      mrnr_num;
    u8      sp_num;
    u8      next_blk;
    u8      is_last;
} ft3dnr_ll_blk_t;

typedef struct ft3dnr_job {
    int                     job_id;
    int                     chan_id;
    ft3dnr_param_t          param;
    job_type_t              job_type;
    job_status_t            status;
    ft3dnr_ll_blk_t         ll_blk;
    struct list_head        job_list;               // job list
    struct f_ops            *fops;
    char                    need_callback;
    int                     job_cnt;
    void                    *parent;                // point to parent job
    void                    *private;
    job_perf_type_t         perf_type;              // for mark engine start bit, videograph 1 job maybe have more than 1 job in scaler300
                                                    // mark engine start at first job, other dont't need to mark, use perf_mark to check.
} ft3dnr_job_t;

typedef struct ft3dnr_dma_buf {
    char        name[20];
    void        *vaddr;
    dma_addr_t  paddr;
    size_t      size;
} ft3dnr_dma_buf_t;

#define MAP_CHAN_NONE -1

typedef struct _res_cfg {
    char    res_type[7];
    u16     width;
    u16     height;
    size_t  size;
    ft3dnr_dma_buf_t    var_buf;
    ft3dnr_dma_buf_t    mot_buf;
    ft3dnr_dma_buf_t    ref_buf;
    int     map_chan;    // which channel used
} res_cfg_t;

struct f_ops {
    void  (*callback)(ft3dnr_job_t *job);
    void  (*update_status)(ft3dnr_job_t *job, int status);
    int   (*pre_process)(ft3dnr_job_t *job);
    int   (*post_process)(ft3dnr_job_t *job);
    void  (*mark_engine_start)(ft3dnr_job_t *job);
    void  (*mark_engine_finish)(ft3dnr_job_t *job);
    int   (*flush_check)(ft3dnr_job_t *job);
    //void  (*record_property)(ft3dnr_job_t *job);
};

#define LOCK_ENGINE()        (priv.engine.busy=1)
#define UNLOCK_ENGINE()      (priv.engine.busy=0)
#define ENGINE_BUSY()        (priv.engine.busy==1)

typedef struct ft3dnr_global {
    ft3dnr_ctrl_t               ctrl;
    mrnr_param_t                mrnr;
    tmnr_param_t                tmnr;
    sp_param_t                  sp;
    int                         mrnr_id;
    int                         tmnr_id;
    int                         sp_id;
} ft3dnr_global_t;

typedef struct ft3dnr_ycbcr {
    u8      src_yc_swap;
    u8      src_cbcr_swap;
    u8      dst_yc_swap;
    u8      dst_cbcr_swap;
    u8      ref_yc_swap;
    u8      ref_cbcr_swap;
} ft3dnr_ycbcr_t;

/* channel base parameters. */
typedef struct chan_param {
    int                 chan_id;
    int                 mrnr_id;
    int                 tmnr_id;
    int                 sp_id;
    unsigned short      src_fmt;
    int                 src_type;
    ft3dnr_dma_buf_t    *var_buf_p;
    ft3dnr_dma_buf_t    *mot_buf_p;
    ft3dnr_dma_buf_t    *ref_buf_p;
    res_cfg_t           *ref_res_p;
    ft3dnr_dim_t        dim;
} chan_param_t;

typedef struct ft3dnr_eng {
    u8                          irq;
    u8                          busy;
    int                         blk_cnt;
    int                         mrnr_cnt;
    int                         sp_cnt;
    u8                          mrnr_table;
    u8                          sram_table;
    u8                          sp_table;
    u32                         ft3dnr_reg;     // ft3dnr base address
    u8                          null_blk;
    struct list_head            qlist;          // record engineX queue list from V.G, job's status = JOB_STS_QUEUE
    struct list_head            slist;          // record engineX sram list, job's status = JOB_STS_SRAM
    struct list_head            wlist;          // record engineX working list, job's status = JOB_STS_ONGOING
    struct timer_list           timer;
    u32                         timeout;
    u8                          ll_mode;
    ft3dnr_dma_buf_t            sp_buf;         // sharpen, egde enhancement
    u16                         wc_wait_value;
    u16                         rc_wait_value;
    ft3dnr_ycbcr_t              ycbcr;
} ft3dnr_eng_t;

typedef struct ft3dnr_priv {
    spinlock_t                  lock;
    atomic_t                    mem_cnt;
    ft3dnr_eng_t                engine;
    ft3dnr_global_t             default_param;
    ft3dnr_global_t             isp_param;
    struct semaphore            sema_lock;          ///< driver semaphore lock
    struct kmem_cache           *job_cache;
    struct workqueue_struct     *job_workq;
    struct chan_param           *chan_param;
    res_cfg_t                   *res_cfg;           // record the dim and sort by little-end
    u8                          res_cfg_cnt;        // total count of allocated res_cfg
    ft3dnr_dim_t                curr_max_dim;       // record the max dim currently processed
    u32                         reg_ary[0x1E4/4];   // record the register content set by isp for max dim
    u8                          *curr_reg;          // type transformation only, points to reg_ary[]
} ft3dnr_priv_t;

/*
 * extern global variables
 */
extern ft3dnr_priv_t priv;

void ft3dnr_put_job(void);
void ft3dnr_joblist_add(ft3dnr_job_t *job);
void ft3dnr_init_global(void);
int ft3dnr_set_lli_blk(ft3dnr_job_t *job, int job_cnt, int last_job);
int ft3dnr_fire(void);
void ft3dnr_write_status(void);
int ft3dnr_write_register(ft3dnr_job_t *job);
void ft3dnr_job_free(ft3dnr_job_t *job);
void ft3dnr_stop_channel(int chanID);
void ft3dnr_sw_reset(void);
void ft3dnr_dump_reg(void);
u32 ft3dnr_get_ip_base(void);
u8 ft3dnr_get_ip_irq(void);
void ft3dnr_add_clock_on(void);
void ft3dnr_add_clock_off(void);
void ft3dnr_init_tmnr_global(u32 base, tmnr_param_t *tmnr, ft3dnr_dim_t *dim);
void ft3dnr_init_sp_global(u32 base, sp_param_t *sp, ft3dnr_dim_t *dim);
#endif
