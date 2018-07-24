#ifndef _FAVC_DEC_VG_H_
#define _FAVC_DEC_VG_H_

#include "platform.h"
#include "video_entity.h"
#include "favc_dec_entity.h"
#include <linux/semaphore.h> /* for struct semaphore */

#define MODULE_NAME         "FD"    /* two bytes character */
#define ENTITY_ENGINES      FTMCP300_NUM       /* number of hardware engines */
#define ENTITY_MINORS       64                 /* job's minor number must be within the rage: 0 ~ (ENTITY_MINORS-1) */
#define MAX_CHN_NUM         ENTITY_MINORS      /* the max number of channels that may co-exist at a time */
#define LL_NUM              2                  /* number of linked list per engine */
#define DEFAULT_CHN_NUM     MAX_CHN_NUM        /* the default value for chn_num */
#define USE_VG_WORKQ        0        /* define this to nonzero to use VG workq instread of Linux kernel workqueue (this is for MODULE_LICENSE("GM")) */
#define USE_KTHREAD         1        /* define this to nonzero to use k_thread instread of Linux kernel workqueue */ 
#define FREE_BUF_CB			1		 /* define this to nonzero to release channel buffer in callback_scheduler */
/*
 * Defines
 */
#define ENABLE_VIDEO_RESERVE 1 // define this to 1 to use VG's API functions to reserve/free job's output buffer
#define DEBUG_VIDEO_RESERVE  0
#define ENABLE_POC_SEQ_PROP // define this to use POC/SEQ property
//#define DEBUG_LIST // define this to print debug message at list accessing
#define CODEC_PANDING_LEN    4 // define this for cutting VG-padded-bytes off at the end of input bitstream 
                               //(It is padded/reserved by VG so that this driver can fill EOS bytes)
#define USE_SINGLE_PREP_LIST  1  // define this to use single prepare list to keep ready job order
#define FORCE_SAME_PREP_ORDER 0
#define SW_TIMEOUT_ENABLE   1       // define this to non-zero to use a timer to do software timeout
#define PROF_CB_SCH         0

#define MAX_DEFAULT_WIDTH  1920  /* NOTE: HW capability: 4096 pixels */
#define MAX_DEFAULT_HEIGHT 1088  /* NOTE: HW capability: 4096 pixels */


#if USE_VG_WORKQ
#include "vg_workqueue.h"

typedef struct vg_work_struct                vp_work_t;
typedef struct vg_workqueue_struct           vp_workqueue_t;
#define VP_PREPARE_WORK(_work, _func)              PREPARE_VG_WORK((_work), (_func))
#define VP_INIT_WORK(_work, _func)                 INIT_VG_WORK((_work), (_func))
#define vp_queue_delayed_work(wq, work, delay_ms)  start_delayed_vg_work((wq), (work), (delay_ms))
#define vp_cancel_delayed_work(work)               cancel_delayed_vg_work((work))
#define vp_create_workqueue(name)                  create_vg_workqueue((name))
#define vp_destroy_workqueue(wq)                   destroy_vg_workqueue((wq))
#define vp_set_workqueue_nice(wq, nice)            set_vg_workqueue_nice((wq), (nice))
#define vp_flush_workqueue(wq)                     flush_vg_workqueue((wq))
#endif // USE_VG_WORKQ


#define MAX_NAME    50
#define MAX_README  100
struct property_map_t {
    int id;
    char name[MAX_NAME];
    char readme[MAX_README];
};

struct property_record_t {
#define MAX_RECORD 25 //100
    struct video_property_t property[MAX_RECORD];
    struct video_entity_t   *entity;
    int    job_id;
    int    err_num;
};


/* for calculating engine utilization */
struct utilization_record_t {
    unsigned int utilization_start;  // the start time of an engine utilization measurment (at engine start)
    unsigned int utilization_record; // the result of the latest utilization measurment
    unsigned int engine_start;       // the engine starting time
    unsigned int engine_end;         // the engine ending time
    unsigned int engine_time;        // the last engine running time (end time - star time)
};




struct favcd_gop_job_item_t;

enum err_position{
    ERR_NONE = 0,
    ERR_TRIG = 1,
    ERR_ISR = 2,
    ERR_SW_TIMEOUT = 3,
};

struct favcd_job_item_t {
    struct video_entity_job_t *job;
    int                 job_id; // record job->id
    short               engine; // indicate which hw engine
    short               minor;
    int                 seq_num; // put job sequence number. for debug
    int                 cb_seq_num; // callback job sequence number

    /* list */
    struct list_head    global_list;   // use to add favcd_global_head
    struct list_head    minor_list;    // use to add favcd_minor_head
    struct list_head    fail_list;     // use to add favcd_fail_minor_head
    struct list_head    callback_list; // use to add favcd_callback_head
    struct list_head    standby_list;  // use to add favcd_standby_head
#if USE_SINGLE_PREP_LIST
    struct list_head    prepare_list;  // use to add favcd_prepare_head
#endif
    struct list_head    prepare_minor_list;  // use to add favcd_prepare_minor_head
    struct list_head    ready_list;    // use to add favcd_ready_head

    int                 buf_idx;       // buf_idx associated to this job

    /* flags for deciding whether a job_item can be freed */
    /* NOTE: job_item can be freed if (callback_flg == 1 && released_flg == 1) */
    unsigned char       callback_flg;  /* indicates the job has been callback and removed from all lists */
    unsigned char       released_flg;  /* indicates the buffer associated with this job is not pushd into dec_buf (used by the low level driver) */
    unsigned char       stored_flg;    /* indicates the buffer associated with this job has been stored into DPB */
    unsigned char       prepared_flg;  /* whether the job_item has been prepared */
    unsigned char       removed_flg;   /* whether the job_item has been removed by __favcd_remove_job_from_all_list() */

    unsigned char       in_property_parsed_flg;
    unsigned char       addr_chk_flg;  /* indicates whether the address has been checked */
    unsigned char       mbinfo_dump_flg;
    unsigned char       bs_dump_flg;
    unsigned char       output_dump_flg;
    unsigned char       is_idr_flg;
#if REV_PLAY
#define NOT_NEW_GOP 0
#define IS_NEW_GOP  1
#define UNKNWON_GOP 2
    //unsigned char       new_gop_flg;  /* 0: same as the previous gop. 1: new gop. 2: undecided */
    unsigned char       output_group_size;
    unsigned char       job_idx_in_gop;
    unsigned char       set_bs_flg;
    unsigned char       bs_idx;
    struct favcd_gop_job_item_t *gop_job_item;
#endif
        
    
                                  // 0U: unknown
#define DRIVER_STATUS_STANDBY   1 // 1S: standby.
#define DRIVER_STATUS_READY     2 // 2R: ready.
#define DRIVER_STATUS_ONGOING   3 // 3O: on-going.
#define DRIVER_STATUS_WAIT_POC  4 // 4W: wait poc. job is processed by HW engine and is waiting for POC to be determined by lowwer level decoder driver
#define DRIVER_STATUS_KEEP      5 // 5K: keep.
#define DRIVER_STATUS_FINISH    6 // 6D: done.
#define DRIVER_STATUS_FAIL      7 // 7F: fail.
    unsigned char       status;

    unsigned char       res_flg;       /* indicates whether video_reserve_buffer() is called for output buffer */
    void               *res_out_buf; // info of reserved output buffer
    
    unsigned int        puttime;    // putjob time
    unsigned int        starttime;  // start engine time
    unsigned int        finishtime; // finish engine time
    int                 err_num;    /* error number during the decoding (for printing error message in callback thread)*/
    
    /* info for showing error message */
    enum err_position   err_pos;       /* where the error is detected (trig or ISR) */
    unsigned int        parser_offset; /* parsser offset when error occurred */

#if 1//REV_PLAY
    /* input bitstream address/size of the frame to be decoded */
    unsigned int        bs_addr_va;
    unsigned int        bs_addr_pa;
    unsigned int        bs_size;
#endif
    
    /* input property */
    unsigned int  in_size;
    unsigned int  dst_fmt;
    unsigned int  dst_xy;
    unsigned int  dst_bg_dim;
    unsigned int  yuv_width_thrd;
    unsigned char sub_yuv_ratio;
#if REV_PLAY
    unsigned char direct;          /* 0:forward 1:backward */
    unsigned int  gop_size;        /* GOP size (for REV_PLAY) */
#endif

    /* derive from input property */    
    unsigned char sub_yuv_en;      /* sub YUV enable */
    unsigned int  Y_sz;            /* calculated from dst_bg_dim */

    /* output property */
    unsigned int  dst_dim;
    short         poc;

    /* job_item profiling */
    unsigned int   prof_put_period;
    unsigned int   prof_put_time;
    unsigned int   prof_prepare_time;
    unsigned int   prof_ready_time;
    unsigned int   prof_start_time;
    unsigned int   prof_finish_time;
    unsigned int   prof_callback_time;
};



#define PP_PROF_RECORD_NUM  64
struct pp_prof_record {
    unsigned int   pp_period;
    unsigned int   pp_trig;
    unsigned int   pp_start;
    unsigned int   pp_dur;
    unsigned int   pp_parse_dur;
    unsigned int   pp_rel_dur;
    unsigned int   pp_job_dur;
    unsigned short pp_parse_cnt;
    unsigned short pp_rel_cnt;
    unsigned short pp_job_cnt;
    unsigned short pp_stop_cnt;
};


struct pp_prof_data {
    unsigned int          pp_trig_cnt;
    unsigned int          pp_cnt;
    unsigned int          pp_job_cnt;
    unsigned int          pp_waste_cnt;
    struct semaphore      sem;
    struct pp_prof_record pp_prof_rec[PP_PROF_RECORD_NUM];
};



#define CB_PROF_RECORD_NUM  64
struct cb_prof_record {
    unsigned int   cb_period;
    unsigned int   cb_trig;
    unsigned int   cb_start;
    unsigned int   cb_sch_dur;
    unsigned int   cb_job_dur;
    unsigned short cb_job_cnt;
    unsigned short cb_stop_cnt;
};


struct cb_prof_data {
    unsigned int          cb_trig_cnt;
    unsigned int          cb_sch_cnt;
    unsigned int          cb_job_cnt;
    unsigned int          cb_waste_cnt;
    struct semaphore      sem;
    struct cb_prof_record cb_prof_rec[CB_PROF_RECORD_NUM];
};


#define ISR_PROF_RECORD_NUM  64
struct isr_prof_record {
    unsigned int   isr_period;
    unsigned int   isr_start;
    unsigned int   isr_dur;
    unsigned int   isr_rec_dur;
    unsigned int   isr_dec_dur;
    unsigned int   isr_mark_dur;
    unsigned int   isr_buf_dur;
    unsigned int   isr_trig_dur;

    unsigned int   isr_work_dur;
    unsigned int   isr_start_job_dur;
    unsigned int   isr_start_job_cnt;
};


struct isr_prof_data {
    unsigned int           isr_cnt;
    unsigned int           max_isr_dur;
    unsigned int           min_isr_dur;
    unsigned int           last_isr_start;
    struct isr_prof_record isr_prof_rec[ISR_PROF_RECORD_NUM];
};


#define JOB_PROF_RECORD_NUM  64
struct job_prof_record {
    unsigned int   put_period;
    unsigned int   put_time;
    unsigned int   prepare_time;
    unsigned int   ready_time;
    unsigned int   start_time;
    unsigned int   finish_time;
    unsigned int   callback_time;
    unsigned int   err_num;
    unsigned int   seq_num;
    unsigned int   cb_seq_num;
    unsigned char  minor;
    unsigned char  engine;
    unsigned char  is_idr_flg;
#define JOB_ITEM_FLAGS_SUB_YUV_EN        (1 << 0)
#define JOB_ITEM_FLAGS_SRC_INTERLACE_EN  (1 << 1)
    unsigned char  flags;
    short          poc;
    unsigned int   dst_dim;
    unsigned int   dst_dim_non_cropped;
    unsigned int   dst_bg_dim;
    unsigned int   in_size;
};


struct job_prof_data {
    unsigned int           job_cnt;
    struct job_prof_record job_prof_record[JOB_PROF_RECORD_NUM];
};



//#define GET_TIME_USE_JIFFIES
#ifdef GET_TIME_USE_JIFFIES
#define FAVCD_HZ        HZ
#define FAVCD_JIFFIES   jiffies
#else // GET_TIME_USE_JIFFIES

//extern unsigned int video_gettime_us(void); //functions defined in kernel
//extern unsigned int video_gettime_ms(void); //functions defined in kernel

//#define FAVCD_HZ        1000
//#define FAVCD_JIFFIES   video_gettime_ms()
#define FAVCD_HZ        1000000
#define FAVCD_JIFFIES   video_gettime_us()
#endif // GET_TIME_USE_JIFFIES

/* minor profiling functions */
#if EN_MINOR_PROF
#define _FAVCD_HZ        (minor_prof?FAVCD_HZ:0)
#define _FAVCD_JIFFIES   (minor_prof?FAVCD_JIFFIES:0)
#else
#define _FAVCD_HZ        0
#define _FAVCD_JIFFIES   0
#endif



/*
 * print message functions for VG driver
 */
#if DISABLE_ALL_MSG
#define DEBUG_M_COND(level) (0)
#elif EN_WARN_ERR_MSG_ONLY
#define DEBUG_M_COND(level) (level <= LOG_WARNING)
#else
#define DEBUG_M_COND(level) (1)
#endif

#define DEBUG_M_PFX(level, prefix, engine, minor, fmt, args...) do {   \
    if(DEBUG_M_COND(level)) {                            \
        const char *fmt_str = fmt;                       \
        char *pfx_str = prefix;                          \
        if (log_level >= level){                         \
            printm(pfx_str, fmt_str, ## args);           \
        }                                                \
        if (((log_level >= 0x10) && ((log_level & 0xF) >= level)) ||   \
            (PRINT_ERR_TO_CONSOLE(dbg_mode) && level == LOG_ERROR)){   \
            if(pfx_str != NULL) printk("[%s]", pfx_str); \
            printk(fmt_str, ## args);                    \
        }                                                \
    }\
    }while(0)

#define DEBUG_M(level, engine, minor, fmt, args...)       DEBUG_M_PFX(level, MODULE_NAME, engine, minor, fmt, ## args)
#define DEBUG_M_NPFX(level, engine, minor, fmt, args...)  DEBUG_M_PFX(level, NULL, engine, minor, fmt, ## args)

/*
 * print message function for incorrect value in job structure from middleware
 */
#define DEBUG_J(level, engine, minor, fmt, args...) do { \
       const char *fmt_str = fmt;                        \
        if (log_level >= level){                         \
            printm(MODULE_NAME, fmt_str, ## args);       \
        }                                                \
        if (((log_level >= 0x10) && ((log_level & 0xF) >= level)) ||   \
            ((PRINT_ERR_TO_CONSOLE(dbg_mode) && level == LOG_ERROR))){ \
            printk("[" MODULE_NAME "]"); \
            printk(fmt_str, ## args);    \
        }                                                \
        if(!in_interrupt()){                             \
            master_print(fmt_str, ## args);              \
        }                                                \
        damnit(MODULE_NAME);                             \
    }while(0)



extern int save_err_bs_flg;
#define FAVCD_DAMNIT(fmt, args...) do { \
    if(!in_interrupt()){                \
        master_print(fmt, ## args);     \
    }                                   \
    save_err_bs_flg = 1;                \
    damnit(MODULE_NAME);                \
} while(0)



#define SET_JOB_STATUS(job_ptr, st) do{         \
            DEBUG_M(LOG_INFO, (job_ptr)->engine, (job_ptr)->minor, \
            "(%d,%d) job %d chg st:%s->%s %s\n", (job_ptr)->engine, (job_ptr)->minor, (job_ptr)->job_id, \
            favcd_job_status_str((job_ptr)->status), favcd_job_status_str((st)), __FUNCTION__);  \
            (job_ptr)->status = (st);                \
        } while(0)
    


/* lock for VG layer */
    
extern spinlock_t favc_dec_lock;
#if 1
#define favcd_spin_lock_irqsave(lock_ptr, flags)       spin_lock_irqsave(lock_ptr, flags)
#define favcd_spin_unlock_irqrestore(lock_ptr, flags)  spin_unlock_irqrestore(lock_ptr, flags)
#else
// for tracing spin lock/unlock
#define favcd_spin_lock_irqsave(lock_ptr, flags)        do{ spin_lock_irqsave(lock_ptr, flags);       printm(MODULE_NAME, "[L]   lock %s\n", __FUNCTION__); } while(0)
#define favcd_spin_unlock_irqrestore(lock_ptr, flags)   do{ spin_unlock_irqrestore(lock_ptr, flags);  printm(MODULE_NAME, "[L] unlock %s\n", __FUNCTION__); } while(0)
#endif

extern struct semaphore  favc_dec_sem;
#if TRACE_SEM
#define DRV_LOCK_G() do{ \
        DEBUG_M(LOG_DEBUG, -1, -1, "DRV_LOCK_G cnt:%d in %s >\n", (favc_dec_sem.count), __FUNCTION__);         \
        down(&favc_dec_sem);    \
        DEBUG_M(LOG_DEBUG, -1, -1, "DRV_LOCK_G cnt:%d in %s <\n", (favc_dec_sem.count), __FUNCTION__);         \
    }while(0)

#define DRV_UNLOCK_G(x) do{ \
        DEBUG_M(LOG_DEBUG, -1, -1, "DRV_UNLOCK_G cnt:%d in %s >\n", (favc_dec_sem.count), __FUNCTION__);         \
        up(&favc_dec_sem);    \
        DEBUG_M(LOG_DEBUG, -1, -1, "DRV_UNLOCK_G cnt:%d in %s <\n", (favc_dec_sem.count), __FUNCTION__);         \
    }while(0)

#else
#define DRV_LOCK_G(x) down(&favc_dec_sem)
#define DRV_UNLOCK_G(x) up(&favc_dec_sem)
#endif




int __init favcd_vg_init(void);
void favcd_vg_cleanup(void);

//void trigger_callback(void);

struct favcd_job_item_t *favcd_get_linked_job_of_buf(struct favcd_data_t *dec_data, int buf_idx);
struct property_map_t *favcd_get_propertymap(int id);
struct favcd_data_t *favcd_get_dec_data(int chn_idx);

/* stop channel */
int favcd_stop(void *parm, int engine, int minor);


#endif

