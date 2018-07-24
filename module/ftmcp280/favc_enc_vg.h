#ifndef _FAVC_ENC_VG_H_
#define _FAVC_ENC_VG_H_

#include "video_entity.h"
#include "enc_driver/define.h"
#include "debug.h"

#define MAX_NAME    50
#define MAX_README  100
struct property_map_t {
    int id;
    char name[MAX_NAME];
    char readme[MAX_README];
};

struct property_record_t {
#define MAX_RECORD 100
    struct video_property_t property[MAX_RECORD];
    struct video_entity_t   *entity;
    int    job_id;
};
/*
#define LOG_ERROR     0
#define LOG_WARNING   1
#define LOG_QUIET     2
*/
struct job_item_t {
    void                *job;
    int                 job_id; //record job->id
    int                 engine; //indicate while hw engine
    int                 chn;
    struct list_head    engine_list; //use to add engine_head
    struct list_head    minor_list; //use to add minor_head
    //struct list_head    callback_list;
    unsigned int        realloc_time;
    int                 out_buf_alloc;
    
    int                 is_b_frame;
    //void                *root_job;
    //int                 need_callback;//need to callback root_job

#define TYPE_NORMAL_JOB     0
#define TYPE_MULTI_JOB      1
    int                 type;

#define DRIVER_STATUS_STANDBY   1
#define DRIVER_STATUS_ONGOING   2
#define DRIVER_STATUS_FINISH    3
#define DRIVER_STATUS_FAIL      4
#define DRIVER_STATUS_KEEP      5
#define DRIVER_STATUS_BUFFERED  6
    int                 status;
    unsigned int        puttime;    //putjob time
    unsigned int        starttime;  //start engine time
    unsigned int        finishtime; //finish engine time
    unsigned int        callbacktime;
#ifdef HANDLE_BS_CACHEABLE2
    unsigned int        bs_length;
#endif
#ifdef NEW_JOB_ITEM_ALLOCATE
    unsigned int        job_item_id;
#endif
#ifdef ENABLE_CHECKSUM
    unsigned int        checksum_type;
#endif
#ifdef OUTPUT_SLICE_OFFSET
    unsigned int        slice_num;
    unsigned int        slice_offset[MAX_SLICE_NUM];
#endif
#ifdef LOCK_JOB_STAUS_ENABLE
#define ID_PRE_PROCESS      0x01
#define ID_POST_PROCESS     0x02
    unsigned int        process_state;
#endif
};

typedef enum{
	FUN_START       = 0x100,
    FUN_STOP        = 0x200,
    FUN_DONE_STOP   = 0x300,
    FUN_DONE_PREV   = 0x400,
    FUN_DONE_CUR    = 0x500,
    FUN_WORK        = 0x600,
    FUN_TRI_FAIL    = 0x700
} CALLBACK_FUNCTION;


#if 0 //allocation from videograph
#include "ms/ms_core.h"
#define driver_alloc(x) ms_small_fast_alloc(x)
#define driver_free(x) ms_small_fast_free(x)
#else
/*
#define driver_alloc(x) do {    \
        kmalloc(x,GFP_ATOMIC);  \
        allocate_cnt++;         \
        printk("job allocate: %d\n", allocate_cnt); \
    while (0)
#define driver_free(x) do { \
        kfree(x);           \
        allocate_cnt--;     \
        printk("job free: %d\n", allocate_cnt); \
    } while (0)
*/
#endif

int favce_vg_init(void);
void favce_vg_close(void);
void work_scheduler(int dev);
//void mark_engine_start(void *param);
//void mark_engine_finish(void *job_param);
//void trigger_callback_finish(void *job_param);
//void trigger_callback_fail(void *parm);
//int favce_preprocess(void *parm, struct favce_pre_data_t *data)
//int favce_postprocess(void *parm, void *priv);

#endif

