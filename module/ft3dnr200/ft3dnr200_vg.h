#ifndef __FT3DNR200_VG_H__
#define __FT3DNR200_VG_H__

#ifdef DRV_CFG_USE_TASKLET
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#endif

#include "ft3dnr200.h"
#include "common.h" //videograph

#define LOG_ERROR       0
#define LOG_WARNING     1

extern unsigned int log_level;
int is_print(int engine,int minor);
#if 1
#define DEBUG_M(level,engine,minor,fmt,args...) { \
    if((log_level>=level)&&is_print(engine,minor)) \
        printm(MODULE_NAME,fmt, ## args); }
#else
#define DEBUG_M(level,engine,minor,fmt,args...) { \
    if((log_level>=level)&&is_print(engine,minor)) \
        printk(fmt,## args);}
#endif


#define MAX_NAME                    50
#define MAX_README                  100

#define ENTITY_ENGINES              1   /* for videograph view */
#define ENTITY_MINORS               MAX_CH_NUM

#define IS_DUMP_JOB_FLOW            ft3dnr_is_dump_job_flow()
#define DUMP_JOB_FLOW               (ft3dnr_is_dump_job_flow()==true)

#define DRV_VG_SPIN_LOCK(FLAGS)     spin_lock_irqsave(&global_info.lock, FLAGS)
#define DRV_VG_SPIN_UNLOCK(FLAGS)   spin_unlock_irqrestore(&global_info.lock, FLAGS)
#define DRV_VG_SEMA_LOCK            down(&global_info.sema_lock)
#define DRV_VG_SEMA_UNLOCK          up(&global_info.sema_lock)

#ifdef DRV_CFG_USE_TASKLET
#define DRV_VG_LIST_LOCK(x)         spin_lock_irqsave(&global_info.list_lock, x)
#define DRV_VG_LIST_UNLOCK(x)       spin_unlock_irqrestore(&global_info.list_lock, x)
#endif

struct property_map_t {
    int id:24;
    char name[MAX_NAME];
    char readme[MAX_README];
};

struct property_record_t {
#define MAX_RECORD MAX_PROPERTYS
    struct video_property_t property[MAX_RECORD];
    struct video_entity_t   *entity;
    int    job_id;
};

typedef enum {
    SINGLE_NODE        = 0,
    MULTI_NODE         = 1,
    NONE_NODE
} node_type_t;

typedef struct buffer_index {
    int var_buf_idx;
    int mot_buf_idx;
    int ref_buf_idx;
    int sp_buf_idx;
} buffer_index_t;

#ifdef DRV_CFG_USE_TASKLET
typedef enum {
    CALLBACK_TIMER_OFF        = 0,
    CALLBACK_TIMER_ACTIVE     = 1,
    CALLBACK_TIMER_CLEAR      = 2
} callback_timer_status;
#endif

/*
 * node structure
 */
typedef struct job_node {
    int                         job_id;         // record job->id
    char                        engine;         // indicate which hw engine
    int                         minor;          // indicate which channel
    int                         chan_id;        // indicate engine * minors + minor
    job_status_t                status;
    ft3dnr_param_t              param;
    //struct job_node             *refer_to;      /* which node is referenced by me */
    struct list_head            plist;          // parent list
    struct list_head            clist;          // child list
    node_type_t                 type;
    char                        need_callback;  // need to callback root_job
    void                        *parent;        // point to root node
    void                        *private;       // private_data, now points to job of videograph */
    unsigned int                puttime;        // putjob time
    unsigned int                starttime;      // start engine time
    unsigned int                finishtime;     // finish engine time
    //atomic_t                    ref_cnt;
    bool                        fmt_changed;
    bool                        ref_res_changed;
    buffer_index_t              buf_index;
#ifdef DRV_CFG_USE_TASKLET
    struct hrtimer              pre_callback_timer;
    callback_timer_status       pre_callback_status; // 0: no timer 1: callback timer enabled 2: callbacked and timer clear
#endif
} job_node_t;

typedef struct vg_proc {
    struct proc_dir_entry   *root;
    struct proc_dir_entry   *info;
    struct proc_dir_entry   *cb;
    struct proc_dir_entry   *property;
    struct proc_dir_entry   *job;
    struct proc_dir_entry   *filter;
    struct proc_dir_entry   *level;
    struct proc_dir_entry   *ratio;
    struct proc_dir_entry   *util;
    struct proc_dir_entry   *res_cfg;
    struct proc_dir_entry   *perf;
#ifdef DRV_CFG_USE_TASKLET
    struct proc_dir_entry   *job_pre_callback;
#endif
} vg_proc_t;

typedef struct global_info {
    spinlock_t              lock;
#ifdef DRV_CFG_USE_TASKLET
    spinlock_t              list_lock;
#endif
    struct semaphore        sema_lock;          ///< vg semaphore lock
    struct list_head        node_list;          // record job list from V.G
    struct kmem_cache       *node_cache;
    //job_node_t              *ref_node[ENTITY_MINORS];
    atomic_t                mem_cnt;
} global_info_t;

/* vg init function
 */
int ft3dnr_vg_init(void);

/*
 * Exit point of video graph
 */
void ft3dnr_vg_driver_clearnup(void);

void mark_engine_start(void);
void mark_engine_finish(void);
int ft3dnr_get_entity_mem_cnt(void);
int ft3dnr_is_dump_job_flow(void);

#endif
