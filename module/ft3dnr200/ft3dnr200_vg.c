#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include <ft3dnr200_isp_api.h>
#include "ft3dnr200_vg.h"
#include "ft3dnr200_dbg.h"
#include "ft3dnr200_util.h"

#include <log.h>    //include log system "printm","damnit"...
#include <video_entity.h>   //include video entity manager
#ifdef DRV_CFG_USE_KTHREAD
#include <linux/kthread.h>
#endif

#ifdef DRV_CFG_USE_TASKLET
struct tasklet_struct   callback_tasklet;
void ft3dnr_callback_task(unsigned long data);
static void ft3dnr_cb_wakeup(void);
#else
#ifdef DRV_CFG_USE_KTHREAD
static int ft3dnr_cb_thread(void *data);
static void ft3dnr_cb_wakeup(void);
static struct task_struct *cb_thread = NULL;
static wait_queue_head_t cb_thread_wait;
static int cb_wakeup_event = 0;
static volatile int  cb_thread_ready = 0;
#endif /* DRV_CFG_USE_KTHREAD */
#endif

#define MODULE_NAME  "DN"

extern int platform_clock_control(bool val);

global_info_t global_info;
vg_proc_t     vg_proc_info;
struct workqueue_struct *callback_workq;
static DECLARE_DELAYED_WORK(process_callback_work, 0);
static void ft3dnr_callback_process(void *);

#if 0
#define DRV_VG_SPIN_LOCK(FLAGS)     spin_lock_irqsave(&global_info.lock, FLAGS)
#define DRV_VG_SPIN_UNLOCK(FLAGS)   spin_unlock_irqrestore(&global_info.lock, FLAGS)
#define DRV_VG_SEMA_LOCK            down(&global_info.sema_lock)
#define DRV_VG_SEMA_UNLOCK          up(&global_info.sema_lock)
#endif

extern int max_minors;
extern int power_save;
extern int perf_period;

/* utilization */
static unsigned int utilization_period = 5; // 5 sec calculation
static unsigned int utilization_start[1], utilization_record[1];
static unsigned int engine_start[1], engine_end[1];
static unsigned int engine_time[1];

/* proc system */
unsigned int callback_period = 0;               //ticks
#ifdef DRV_CFG_USE_TASKLET
unsigned int job_pre_callback_period = 0;       //ms
#endif

/* property lastest record */
struct property_record_t *property_record;

/* log & debug message */
#define MAX_FILTER  5
unsigned int log_level = LOG_ERROR;
static int include_filter_idx[MAX_FILTER]; //init to -1, ENGINE<<16|MINOR
static int exclude_filter_idx[MAX_FILTER]; //init to -1, ENGINE<<16|MINOR

static int isp_info_exist = -1; // -1:unchecked, 0:not exist, 1:exist

struct property_map_t property_map[] = {
    {ID_SRC_FMT,     "src_fmt",     "source format"},
    {ID_SRC_XY,      "src_xy",      "source x and y position"},
    {ID_SRC_BG_DIM,  "src_bg_dim",  "source background width/height"},
    {ID_SRC_DIM,     "src_dim",     "source width/height"},
    {ID_SRC_TYPE,    "src_type",    "source type, 0: decoder 1: from isp 2: sensor"},
    {ID_ORG_DIM,     "org_dim",     "original width/height"}
};

int is_print(int engine,int minor)
{
    int i;
    if(include_filter_idx[0] >= 0) {
        for(i = 0; i < MAX_FILTER; i++)
            if(include_filter_idx[i] == ((engine << 16) | minor))
                return 1;
    }

    if(exclude_filter_idx[0] >= 0) {
        for(i = 0; i < MAX_FILTER; i++)
            if(exclude_filter_idx[i] == ((engine << 16) | minor))
                return 0;
    }
    return 1;
}

int ft3dnr_get_entity_mem_cnt(void)
{
    int cnt=0;

    cnt = atomic_read(&global_info.mem_cnt);

    return cnt;
}

static int parse_isp_cap_window(char *filename)
{
    char line[64], ch;
    char pattern[] = "cap window";
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int win_width = 0, win_height = 0;
    int j = 0, ret = 0, record = 0;
    int nread = 0;

    fs = get_fs();
    set_fs(get_ds());
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        printk("%s(%s): file %s not found\n", DRIVER_NAME, __func__, filename);
        return -ENOENT;
    }

    /* the format of isp cap window info is
     *     cap window           1920x1080
     */
    while ((nread = (int) vfs_read(fd, &ch, 1, &offset)) == 1) {
        if (ch == pattern[j++]) {
            if (j == strlen(pattern))
                break;
            else
                continue;
        } else {
            j = 0;
            continue;
        }
    }

    if (nread == 1) {
        j = 0;
        while ((nread = (int) vfs_read(fd, &ch, 1, &offset)) == 1) {
            if (ch == 0xA) {
                line[j++] = ch;
                break;
            }
            if (record) {
                line[j++] = ch;
                continue;
            }
            if ((ch >= '0') && (ch <= '9')) {
                line[j++] = ch;
                record = 1;
                continue;
            }
        }
    } else { // EOF
        ret = -EINVAL;
    }

    filp_close(fd, NULL);
    set_fs(fs);

    if ((ret == 0) && (j > 0))
        sscanf(line, "%dx%d", &win_width, &win_height);
    //printk("--> isp cap window = %dx%d\n", win_width, win_height);

    if ((win_width > 0) && (win_height > 0)) {
        priv.curr_max_dim.src_bg_width = win_width;
        priv.curr_max_dim.src_bg_height = win_height;
    } else {
        ret = -EINVAL;
    }

    return ret;
}

static int driver_parse_in_property(job_node_t *node, struct video_entity_job_t *job)
{
    int i=0, chan_id=0, ret;
    struct video_property_t *property = job->in_prop.p;
    struct chan_param chanParam;

    chan_id = node->chan_id;

    // keep old channel dim info
    memset(&chanParam,0x0,sizeof(struct chan_param));
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_LOCK;
#endif
    memcpy((void *)&chanParam,(void *)&priv.chan_param[chan_id],sizeof(struct chan_param));
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_UNLOCK;
#endif

    /* fill up the input parameters */
    while(property[i].id != 0) {
        switch (property[i].id) {
            case ID_SRC_FMT:
                    node->param.src_fmt = property[i].value;
                break;
            case ID_SRC_XY:
                    node->param.dim.src_x = EM_PARAM_X(property[i].value);
                    node->param.dim.src_y = EM_PARAM_Y(property[i].value);
                break;
            case ID_SRC_DIM:
                    node->param.dim.nr_width = EM_PARAM_WIDTH(property[i].value);
                    node->param.dim.nr_height = EM_PARAM_HEIGHT(property[i].value);
                break;
            case ID_SRC_BG_DIM:
                    node->param.dim.src_bg_width = EM_PARAM_WIDTH(property[i].value);
                    node->param.dim.src_bg_height = EM_PARAM_HEIGHT(property[i].value);
                break;
            case ID_SRC_TYPE:
                    node->param.src_type = property[i].value;
                break;
            case ID_ORG_DIM:
                    node->param.dim.org_width = EM_PARAM_WIDTH(property[i].value);
                    node->param.dim.org_height = EM_PARAM_HEIGHT(property[i].value);
                break;
            default:
                break;
        }
        i++;
    }

    if (isp_info_exist == -1) {
        if (node->param.src_type == SRC_TYPE_ISP) {
            if (parse_isp_cap_window(ISP_INFO_FILENAME) < 0) {
                //printk("%s(%s): parse isp cap window info fail\n", DRIVER_NAME, __func__);
                isp_info_exist = 0;
            } else {
                //printk("%s(%s): isp cap window %dx%d\n", DRIVER_NAME, __func__, priv.curr_max_dim.src_bg_width, priv.curr_max_dim.src_bg_height);
                isp_info_exist = 1;
            }
        } else {
            isp_info_exist = 0;
        }
    }

    if (isp_info_exist == 0) {
        /* because the mrnr parameter is configured for main stream,
         * but we have no idea which one of resolutions is main stream,
         * so we guess that the largest one is it.
         */
        if (node->param.dim.src_bg_width > priv.curr_max_dim.src_bg_width) {
            memcpy(&priv.curr_max_dim, &node->param.dim, sizeof(ft3dnr_dim_t));
            //priv.isp_param.mrnr_id++; // force to update mrnr parameter
            //printk("%s(%s): update isp cap window to %dx%d\n", DRIVER_NAME, __func__, priv.curr_max_dim.src_bg_width, priv.curr_max_dim.src_bg_height);

            if (node->param.src_type != SRC_TYPE_ISP) {
                priv.default_param.tmnr.Motion_top_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TL,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_top_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TN,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_top_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_TR,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_nrm_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NL,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_nrm_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NN,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_nrm_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_NR,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_bot_lft_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BL,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_bot_nrm_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BN,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
                priv.default_param.tmnr.Motion_bot_rgt_th = ft3dnr_get_tmnr_motion_th_val(MOTION_TH_BR,priv.curr_max_dim.src_bg_width,priv.curr_max_dim.src_bg_height,DEF_MOTION_TH_MULT);
            }
        }

        // For speeding up converging time from motion to static
        if (node->param.src_type != SRC_TYPE_ISP) {
            static int frame_cnt = 0;

            if (node->param.dim.src_bg_width == priv.curr_max_dim.src_bg_width) {
                if (frame_cnt == 29) {
                    priv.default_param.tmnr.K = 5;
                } else if (frame_cnt == 30) {
                    frame_cnt = 0;
                    if (priv.default_param.tmnr.Y_var >= 32)
                        priv.default_param.tmnr.K = 3;
                    else
                        priv.default_param.tmnr.K = 2;
                }
                frame_cnt++;
            }
        }
    }

    // save to chan_param DB
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_LOCK;
#endif
    memcpy((void *)&priv.chan_param[chan_id].dim,(void *)&node->param.dim,sizeof(ft3dnr_dim_t));
    priv.chan_param[chan_id].src_fmt = node->param.src_fmt;
    priv.chan_param[chan_id].src_type = node->param.src_type;
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_UNLOCK;
#endif

    /* if src/dst/roi width/height is changed, do not reference to previous job */
    ret = memcmp(&chanParam.dim, &node->param.dim, sizeof(ft3dnr_dim_t));

    return ret;
}

static int driver_stop(unsigned int fd)
{
    int engine = ENTITY_FD_ENGINE(fd);
    int minor  = ENTITY_FD_MINOR(fd);
    int chanID = MAKE_IDX(ENTITY_MINORS, engine, minor);
    job_node_t *node, *ne;
#ifdef DRV_CFG_USE_TASKLET
    unsigned long flags;
#endif
    //printk("%s minor %d, idx %d\n", __func__, minor, idx);

    ft3dnr_stop_channel(chanID);

#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_LOCK(flags);
#else
    DRV_VG_SEMA_LOCK;
#endif

    list_for_each_entry_safe(node, ne, &global_info.node_list, plist) {
        if (node->chan_id == chanID) {
            //REFCNT_DEC(node);
            printm(MODULE_NAME, "dnstop ch%d %d\n", chanID, node->job_id);
        }
    }

#if 0
    if (global_info.ref_node[chanID]) {
        /* decrease the reference count of this node */
        if (global_info.ref_node[chanID]->status == JOB_STS_FLUSH || global_info.ref_node[chanID]->status == JOB_STS_DONE) {
            REFCNT_DEC(global_info.ref_node[chanID]);
            global_info.ref_node[chanID] = NULL;
        }
    }
#endif

#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_UNLOCK(flags);
#else
    DRV_VG_SEMA_UNLOCK;
#endif

#ifdef DRV_CFG_USE_TASKLET
    ft3dnr_cb_wakeup();
#else
#ifdef DRV_CFG_USE_KTHREAD
    ft3dnr_cb_wakeup();
#else
    /* schedule the delay workq */
    PREPARE_DELAYED_WORK(&process_callback_work,(void *)ft3dnr_callback_process);
    queue_delayed_work(callback_workq, &process_callback_work, callback_period);
#endif
#endif
    return 0;
}

static int driver_queryid(void *parm,char *str)
{
    int i;

    for (i = 0; i < sizeof(property_map) / sizeof(struct property_map_t); i++) {
        if (strcmp(property_map[i].name,str) == 0) {
            return property_map[i].id;
        }
    }
    printk("driver_queryid: Error to find name %s\n",str);
    return -1;
}

static int driver_querystr(void *parm,int id,char *string)
{
    int i;

    for (i = 0; i < sizeof(property_map) / sizeof(struct property_map_t); i++) {
        if (property_map[i].id == id) {
            memcpy(string,property_map[i].name,sizeof(property_map[i].name));
            return 0;
        }
    }
    printk("driver_querystr: Error to find id %d\n",id);
    return -1;
}

void ft3dnr_callback_process(void *parm)
{
    job_node_t *currNode, *nextNode, *cn, *nn;
    struct video_entity_job_t *job, *child;
    int pCallbacked = 0, cCallbacked = 0;
    bool dump_job_flow = IS_DUMP_JOB_FLOW;
#ifdef DRV_CFG_USE_TASKLET
    unsigned long flags;
#endif
    //struct timespec ts;

#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_LOCK(flags);
#else
    DRV_VG_SEMA_LOCK;
#endif

    list_for_each_entry_safe(currNode, nextNode, &global_info.node_list, plist) {
        // reset callback check flag
        pCallbacked = cCallbacked = 0;
        if (currNode->status == JOB_STS_DONE || currNode->status == JOB_STS_FLUSH || currNode->status == JOB_STS_DONOTHING) {
            job = (struct video_entity_job_t *)currNode->private;
            if (currNode->status == JOB_STS_DONE || currNode->status == JOB_STS_DONOTHING)
                job->status = JOB_STATUS_FINISH;
            else
                job->status = JOB_STATUS_FAIL;

            //getrawmonotonic(&ts);
            //printk("callback id=%8d at %8ld %8ld  %8ld status=%d  starttime=%u finishtime=%u\n", currNode->job_id, ts.tv_sec, ts.tv_nsec, ts.tv_nsec-currNode->puttime, job->status, currNode->starttime, currNode->finishtime);

            if (currNode->need_callback) {
#ifdef DRV_CFG_USE_TASKLET
                if (currNode->pre_callback_status!=CALLBACK_TIMER_CLEAR) {
                    //DRV_VG_LIST_UNLOCK(flags);
                    job->callback(currNode->private); //callback root job
                    //DRV_VG_LIST_LOCK(flags);

                    /*
                    if (currNode->pre_callback_status==CALLBACK_TIMER_ACTIVE) {
                        //del_timer(&currNode->pre_callback_timer);
                        hrtimer_cancel( &currNode->pre_callback_timer );
                        currNode->pre_callback_status = CALLBACK_TIMER_CLEAR;
                    }
                    */
                }
#else
                job->callback(currNode->private); //callback root job
#endif
                JOB_FLOW_DBG("job(0x%x) has been callbacked!\n", job->id);
                currNode->need_callback = 0;
                pCallbacked = 1;


                /* Currently, only single job assigned, skip the statistic for clist,
                 * just focus on plist
                 */
                if (perf_period) {
                    static unsigned long jj1 = 0, jj2 = 0;
                    static u32 ff_cnt[MAX_CH_NUM];
                    int i;

                    if (currNode->chan_id < MAX_CH_NUM) {
                        ff_cnt[currNode->chan_id]++;
                        jj2 = jiffies;
                        if (time_after(jj2, jj1)) {
                            if ((jj2 - jj1) >= (perf_period * HZ)) {
                                for (i = 0; i < MAX_CH_NUM; i++) {
                                    if (ff_cnt[i]) {
                                        printk(" [%s]: ch%d(%4dx%4d) job out ==> %u frames in %u ms\n", MODULE_NAME, i,
                                                priv.chan_param[i].dim.src_bg_width, priv.chan_param[i].dim.src_bg_height,
                                                ff_cnt[i], jiffies_to_msecs(jj2-jj1));
                                        ff_cnt[i] = 0;
                                    }
                                }
                                jj1 = jj2;
                            }
                        } else {
                            jj1 = jj2;
                            memset(ff_cnt, 0, sizeof(ff_cnt));
                        }
                    } else {
                        printk("%s(%s): chan id %d exceeds max %d\n", DRIVER_NAME, __func__, currNode->chan_id, MAX_CH_NUM - 1);
                    }
                }
            }
            else
                pCallbacked = 1;

            /* callback & free child node */
            list_for_each_entry_safe(cn, nn, &currNode->clist ,clist) {
                if (cn->status == JOB_STS_DONE || cn->status == JOB_STS_FLUSH || cn->status == JOB_STS_DONOTHING) {
                    child = (struct video_entity_job_t *)cn->private;
                    if (cn->status == JOB_STS_DONE || cn->status == JOB_STS_DONOTHING)
                        child->status = JOB_STATUS_FINISH;
                    else
                        child->status = JOB_STATUS_FAIL;

                    //getrawmonotonic(&ts);
                    //printk("callback id=%8d at %8ld %8ld  %8ld status=%d  ---\n", currNode->job_id, ts.tv_sec, ts.tv_nsec, ts.tv_nsec-currNode->puttime, job->status);

                    if (cn->need_callback) {
#ifdef DRV_CFG_USE_TASKLET
                        if (cn->pre_callback_status!=CALLBACK_TIMER_CLEAR) {
                            //DRV_VG_LIST_UNLOCK(flags);
                            child->callback(cn->private); //callback root node
                            //DRV_VG_LIST_LOCK(flags);

                            /*
                            if (cn->pre_callback_status==CALLBACK_TIMER_ACTIVE) {
                                //del_timer(&cn->pre_callback_timer);
                                hrtimer_cancel( &cn->pre_callback_timer );
                                cn->pre_callback_status = CALLBACK_TIMER_CLEAR;
                            }
                            */
                        }
#else
                        child->callback(cn->private); //callback root node
#endif
                        //child->callback(currNode->private); //callback root node
                        JOB_FLOW_DBG("job(0x%x) has been callbacked!\n", job->id);
                        cn->need_callback = 0;
                        cCallbacked = 1;
                    }
                    else
                        cCallbacked = 1;
                }
            }

            /* free child node */
            if (cCallbacked == 1) {
                list_for_each_entry_safe(cn, nn, &currNode->clist ,clist) {
                    list_del(&cn->clist);
#ifdef DRV_CFG_USE_TASKLET
                    if (cn->pre_callback_status!=CALLBACK_TIMER_ACTIVE) {
                        //del_timer(&cn->pre_callback_timer);
                        hrtimer_cancel( &cn->pre_callback_timer );

                        kmem_cache_free(global_info.node_cache, cn);
                        MEMCNT_SUB(&global_info, 1);

                        DEBUG_M(LOG_WARNING, cn->engine, cn->minor, "kmem_cache_free (free child node) job id=%d\n", cn->job_id);
                    }
#else
                    kmem_cache_free(global_info.node_cache, cn);
                    MEMCNT_SUB(&global_info, 1);
#endif
                }
            }

            /* free parent node */
            if ((pCallbacked == 1) || (cCallbacked == 1)) {
                list_del(&currNode->plist);
#ifdef DRV_CFG_USE_TASKLET
                if (currNode->pre_callback_status!=CALLBACK_TIMER_ACTIVE) {
                    //del_timer(&currNode->pre_callback_timer);
                    hrtimer_cancel( &currNode->pre_callback_timer );

                    kmem_cache_free(global_info.node_cache, currNode);
                    MEMCNT_SUB(&global_info, 1);

                    DEBUG_M(LOG_WARNING, currNode->engine, currNode->minor, "kmem_cache_free (free parent node) job id=%d\n", currNode->job_id);
                }
#else
                kmem_cache_free(global_info.node_cache, currNode);
                MEMCNT_SUB(&global_info, 1);
#endif
            }
        }
        else
            break;
    }

    if (power_save) {
        if (list_empty(&global_info.node_list)) {
            ft3dnr_add_clock_off();
            platform_clock_control(0);
        }
    }

#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_UNLOCK(flags);
#else
    DRV_VG_SEMA_UNLOCK;
#endif
}


/*
 * param: private data
 */
static int driver_set_out_property(void *param, struct video_entity_job_t *job)
{
    //ft3dnr_job_t   *job_item = (ft3dnr_job_t *)param;

    return 1;
}

/*
 * callback function for job finish. In ISR.
 */
static void driver_callback(ft3dnr_job_t *job)
{
    job_node_t *node, *parent, *curr, *ne;

    if (job->job_type == MULTI_JOB) {
        node = (job_node_t *)job->private;
        driver_set_out_property(job, node->private);
        parent = list_entry(node->clist.next, job_node_t, clist);
        parent->status = job->status;

#if 0
        if (parent->refer_to)
            REFCNT_DEC(parent->refer_to);
#endif

        list_for_each_entry_safe(curr, ne, &parent->clist, clist) {
            curr->status = job->status;
            driver_set_out_property(job, curr->private);

#if 0
            if (curr->refer_to)
                REFCNT_DEC(curr->refer_to);
#endif
        }
    }
    else {
        node = (job_node_t *)job->private;
        node->status = job->status;
        driver_set_out_property(job, node->private);

#if 0
        if (node->refer_to)
            REFCNT_DEC(node->refer_to);
#endif
    }

#ifdef DRV_CFG_USE_TASKLET
    ft3dnr_cb_wakeup();
#else
#ifdef DRV_CFG_USE_KTHREAD
    ft3dnr_cb_wakeup();
#else
    /* schedule the delay workq */
    PREPARE_DELAYED_WORK(&process_callback_work,(void *)ft3dnr_callback_process);
    queue_delayed_work(callback_workq, &process_callback_work, callback_period);
#endif
#endif
}

static void driver_update_status(ft3dnr_job_t *job, int status)
{
#ifdef DRV_CFG_USE_TASKLET
    struct timespec ts;
#endif
    job_node_t *node;

    node = (job_node_t *)job->private;
    node->status = status;

#ifdef DRV_CFG_USE_TASKLET
    getrawmonotonic(&ts);
    if (status == JOB_STS_ONGOING) node->starttime = ts.tv_nsec;
    if (status == JOB_STS_DONE) node->finishtime = ts.tv_nsec;
#endif
}

static int driver_preprocess(ft3dnr_job_t *job)
{
    return 0;
}

static int driver_postprocess(ft3dnr_job_t *job)
{
    return 0;
}

static void driver_mark_engine_start(ft3dnr_job_t *job_item)
{
    int j = 0;
    job_node_t  *node = NULL;
    struct video_entity_job_t *job;
    struct video_property_t *property;
    int idx;

    node = (job_node_t *)job_item->private;
    job = (struct video_entity_job_t *)node->private;
    property = job->in_prop.p;
    idx = MAKE_IDX(max_minors,ENTITY_FD_ENGINE(job->fd),ENTITY_FD_MINOR(job->fd));

    while (property[j].id != 0) {
        if (j < MAX_RECORD) {
            property_record[idx].job_id = job->id;
            property_record[idx].property[j].id = property[j].id;
            property_record[idx].property[j].pch = property[j].pch;
            property_record[idx].property[j].value = property[j].value;
        }
        j++;
    }

}

static void driver_mark_engine_finish(ft3dnr_job_t *job_item)
{
}

static int driver_flush_check(ft3dnr_job_t *job_item)
{
    return 0;
}

void mark_engine_start(void)
{
    int dev=0;
    //unsigned long flags;

    //DRV_SPIN_LOCK(flags);

    if (engine_start[dev] != 0)
        printk("[FT3DNR]Warning to nested use dev%d mark_engine_start function!\n",dev);

    engine_start[dev] = jiffies;

    engine_end[dev] = 0;
    if (utilization_start[dev] == 0) {
        utilization_start[dev] = engine_start[dev];
        engine_time[dev] = 0;
    }

    //DRV_SPIN_UNLOCK(flags);

}

void mark_engine_finish(void)
{
    int dev = 0;
    //unsigned long flags;

    //DRV_SPIN_LOCK(flags);

    if (engine_end[dev] != 0)
        printk("[FT3DNR]Warning to nested use dev%d mark_engine_finish function!\n",dev);

    engine_end[dev] = jiffies;

    if (engine_end[dev] > engine_start[dev])
        engine_time[dev] += engine_end[dev] - engine_start[dev];

    if (utilization_start[dev] > engine_end[dev]) {
        utilization_start[dev] = 0;
        engine_time[dev] = 0;
    } else if ((utilization_start[dev] <= engine_end[dev]) &&
        (engine_end[dev] - utilization_start[dev] >= utilization_period * HZ)) {

        utilization_record[dev] = (100*engine_time[dev]) / (engine_end[dev] - utilization_start[dev]);

        utilization_start[dev] = 0;
        engine_time[dev] = 0;
    }
    engine_start[dev]=0;

    //DRV_SPIN_UNLOCK(flags);
}

/* callback functions called from the core
 */
struct f_ops callback_ops = {
    callback:           &driver_callback,
    update_status:      &driver_update_status,
    pre_process:        &driver_preprocess,
    post_process:       &driver_postprocess,
    mark_engine_start:  &driver_mark_engine_start,
    mark_engine_finish: &driver_mark_engine_finish,
    flush_check:        &driver_flush_check,
};

/*
 * Add a node to nodelist
 */
static void vg_nodelist_add(job_node_t *node)
{
    list_add_tail(&node->plist, &global_info.node_list);
}

int cal_mrnr_param(job_node_t *node)
{
    int i, j, idx;
    unsigned int dist0, dist1, layerRatio, scalingRatio;
    mrnr_param_t *pmrnr = NULL;

    if (node->param.src_type == SRC_TYPE_ISP) /* channel from isp */
        pmrnr = &priv.isp_param.mrnr;
    else /* channel from decoder/sensor, use default mrnr */
        pmrnr = &priv.default_param.mrnr;

    if (priv.curr_max_dim.src_bg_width)
        scalingRatio = (node->param.dim.src_bg_width << 6) / priv.curr_max_dim.src_bg_width;
    else
        panic("%s(%s): division by zero", DRIVER_NAME, __func__);

    if (scalingRatio >= 64) {
        memcpy(&node->param.mrnr, pmrnr, sizeof(mrnr_param_t));
        return 0;
    }

    for (i = 0; i < 4; i++) //layer
    {
        layerRatio = scalingRatio >> i;

        if (layerRatio <= 8) {
        	dist0 = 1;
        	dist1 = 0;
        	idx = 4;
        }
        else if (layerRatio <= 16) {
        	dist0 = 16 - layerRatio;
        	dist1 = layerRatio - 8;
        	idx = 3;
        }
        else if (layerRatio <= 32) {
        	dist0 = 32 - layerRatio;
        	dist1 = layerRatio - 16;
        	idx = 2;
        }
        else {
        	dist0 = 64 - layerRatio;
        	dist1 = layerRatio - 32;
        	idx = 1;
        }

        //Y channel thresholds
        for (j = 0; j < 8; j++) {
            node->param.mrnr.Y_L_edg_th[i][j] = ((pmrnr->Y_L_edg_th[(min(idx,3) - 1)][j] * dist1 +
                                                pmrnr->Y_L_edg_th[min(idx,3)][j] * dist0) / (dist0 + dist1));
            node->param.mrnr.Y_L_smo_th[i][j] = ((pmrnr->Y_L_smo_th[(min(idx,3) - 1)][j] * dist1 +
                                                pmrnr->Y_L_smo_th[min(idx,3)][j] * dist0) / (dist0 + dist1));
        }
        //Cb channel thresholds
        node->param.mrnr.Cb_L_edg_th[i] = (unsigned int)((pmrnr->Cb_L_edg_th[min(idx,3) - 1] * dist1 +
                                            pmrnr->Cb_L_edg_th[min(idx,3)] * dist0) / (dist0 + dist1));
        node->param.mrnr.Cb_L_smo_th[i] = (unsigned int)((pmrnr->Cb_L_smo_th[min(idx,3) - 1] * dist1 +
                                            pmrnr->Cb_L_smo_th[min(idx,3)] * dist0) / (dist0 + dist1));

        //Cr channel thresholds
        node->param.mrnr.Cr_L_edg_th[i] = (unsigned int)((pmrnr->Cr_L_edg_th[min(idx,3) - 1] * dist1 +
                                            pmrnr->Cr_L_edg_th[min(idx,3)] * dist0) / (dist0 + dist1));
        node->param.mrnr.Cr_L_smo_th[i] = (unsigned int)((pmrnr->Cr_L_smo_th[min(idx,3) - 1] * dist1 +
                                            pmrnr->Cr_L_smo_th[min(idx,3)] * dist0) / (dist0 + dist1));

        /* copy noise reduction strength */
        node->param.mrnr.Y_L_nr_str[i] = pmrnr->Y_L_nr_str[i];
        node->param.mrnr.C_L_nr_str[i] = pmrnr->C_L_nr_str[i];
    }

    return 0;
}

int get_mrnr_param(job_node_t *node)
{
#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_LOCK;
#endif

    /* calculate mrnr param */
    cal_mrnr_param(node);

#ifndef DRV_CFG_USE_TASKLET
    DRV_SEMA_UNLOCK;
#endif
    return 0;
}

int get_tmnr_param(job_node_t *node)
{
    u32 base = priv.engine.ft3dnr_reg;
    int chanID = node->chan_id;

    /* channel from decoder/sensor, copy default tmnr */
    if (node->param.src_type == SRC_TYPE_DECODER || node->param.src_type == SRC_TYPE_SENSOR) {
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.tmnr, &priv.default_param.tmnr, sizeof(tmnr_param_t));
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }

    /* channel from isp */
    if (node->param.src_type == SRC_TYPE_ISP) {
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.tmnr, &priv.isp_param.tmnr, sizeof(tmnr_param_t));
        if ((priv.chan_param[chanID].tmnr_id) != (priv.isp_param.tmnr_id)) {
            ft3dnr_init_tmnr_global(base, &node->param.tmnr, &node->param.dim);
            priv.chan_param[chanID].tmnr_id = priv.isp_param.tmnr_id;
        }
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }

    return 0;
}

void get_sp_param(job_node_t *node)
{
    u32 base = priv.engine.ft3dnr_reg;
    int chanID = node->chan_id;

    // channel from decoder/sensor
    if (node->param.src_type == SRC_TYPE_DECODER || node->param.src_type == SRC_TYPE_SENSOR) {
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.sp, &priv.default_param.sp, sizeof(sp_param_t));
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }

    // channel from ISP
    if (node->param.src_type == SRC_TYPE_ISP) {
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.sp, &priv.isp_param.sp, sizeof(sp_param_t));
        if ((priv.chan_param[chanID].sp_id) != (priv.isp_param.sp_id)) {
            ft3dnr_init_sp_global(base, &node->param.sp, &node->param.dim);
            priv.chan_param[chanID].sp_id = priv.isp_param.sp_id;
        }
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }
}

#if 0
int cal_his_param(job_node_t *node)
{
    unsigned int width_constraint  = 0;
	unsigned int height_constraint = 0;
    ft3dnr_ctrl_t   *ctrl = &node->param.ctrl;
    ft3dnr_dim_t    *dim = &node->param.dim;
    ft3dnr_his_t    *his = &node->param.his;

	/* his th active only when tnr_learn_en == 1 */
	if ((ctrl->temporal_en == 0) || (ctrl->tnr_learn_en == 0))
		return 0;

    /* get width_constraint */
	if ((dim->nr_width % VARBLK_WIDTH) == 0)
		width_constraint = VARBLK_WIDTH;
	else
		width_constraint = dim->nr_width % VARBLK_WIDTH;

    /* get height_constraint */
	if ((dim->nr_height % VARBLK_HEIGHT) == 0)
		height_constraint = VARBLK_HEIGHT;
	else
		height_constraint = dim->nr_height % VARBLK_HEIGHT;

    /* cal th */
    his->norm_th = WIDTH_LIMIT * HEIGHT_LIMIT * TH_RETIO / 10;

	his->right_th = (width_constraint - 2) * HEIGHT_LIMIT * TH_RETIO / 10;

    his->bot_th = WIDTH_LIMIT * (height_constraint - 2) * TH_RETIO / 10;

    his->corner_th = (width_constraint - 2) * (height_constraint - 2) * TH_RETIO / 10;

    return 0;
}
#endif

int sanity_check(job_node_t *node)
{
    int chn = node->chan_id;
    ft3dnr_param_t *param = &node->param;
    int ret = 0;

    if (param->src_type > SRC_TYPE_SENSOR) {
        dn_err("Error! source type not from decoder/isp/sensor %d\n", param->src_type);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source type [%d] not from decoder/isp/sensor\n", chn, param->src_type);
        ret = -1;
    }

    if (param->dim.src_bg_width % 4 != 0) {
        dn_err("Error! source background width [%d] not 4 alignment\n", param->dim.src_bg_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background width [%d] not 4 alignment\n", chn, param->dim.src_bg_width);
        ret = -1;
    }

    if (param->dim.src_bg_width == 0) {
        dn_err("Error! source background width [%d] is zero\n", param->dim.src_bg_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background width [%d] is zero\n", chn, param->dim.src_bg_width);
        ret = -1;
    }

    if (param->dim.src_bg_width < 132) {
        dn_err("Error! source background width [%d] < 132\n", param->dim.src_bg_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background width [%d] < 132\n", chn, param->dim.src_bg_width);
        ret = -1;
    }

    if (param->dim.src_bg_height % 2 != 0) {
        dn_err("Error! source background height [%d] not 2 alignment\n", param->dim.src_bg_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background height [%d] not 2 alignment\n", chn, param->dim.src_bg_height);
        ret = -1;
    }

    if (param->dim.src_bg_height == 0) {
        dn_err("Error! source background height [%d] is zero\n", param->dim.src_bg_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background height [%d] is zero\n", chn, param->dim.src_bg_height);
        ret = -1;
    }

    if (param->dim.src_bg_height < 66) {
        dn_err("Error! source background height [%d] < 66\n", param->dim.src_bg_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source background height [%d] < 66\n", chn, param->dim.src_bg_height);
        ret = -1;
    }

    if (param->dim.src_x % 4 != 0) {
        dn_err("Error! source x [%d] not 4 alignment\n", param->dim.src_x);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source x [%d] not 4 alignment\n", chn, param->dim.src_x);
        ret = -1;
    }

    if (param->dim.src_y % 2 != 0) {
        dn_err("Error! source y [%d] not 2 alignment\n", param->dim.src_y);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] source y [%d] not 2 alignment\n", chn, param->dim.src_y);
        ret = -1;
    }

    if (param->dim.nr_width % 4 != 0) {
        dn_err("Error! noise width [%d] not 4 alignment\n", param->dim.nr_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise width [%d] not 4 alignment\n", chn, param->dim.nr_width);
        ret = -1;
    }

    if (param->dim.nr_width == 0) {
        dn_err("Error! noise width [%d] is zero\n", param->dim.nr_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise width [%d] is zero\n", chn, param->dim.nr_width);
        ret = -1;
    }

    if (param->dim.nr_width < 132) {
        dn_err("Error! noise width [%d] < 132\n", param->dim.nr_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise width [%d] < 132\n", chn, param->dim.nr_width);
        ret = -1;
    }

    if (param->dim.nr_height % 2 != 0) {
        dn_err("Error! noise height [%d] not 2 alignment\n", param->dim.nr_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise height [%d] not 2 alignment\n", chn, param->dim.nr_height);
        ret = -1;
    }

    if (param->dim.nr_height == 0) {
        dn_err("Error! noise height [%d] is zero\n", param->dim.nr_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise height [%d] is zero\n", chn, param->dim.nr_height);
        ret = -1;
    }

    if (param->dim.nr_height < 66) {
        dn_err("Error! noise height [%d] < 66\n", param->dim.nr_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] noise height [%d] < 66\n", chn, param->dim.nr_height);
        ret = -1;
    }

    if ((param->dim.src_x + param->dim.nr_width) > param->dim.src_bg_width) {
        dn_err("Error! (src x [%d] + nr width [%d]) over src background width [%d]\n", param->dim.src_x, param->dim.nr_width, param->dim.src_bg_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] src x [%d] nr width [%d] src bg width [%d]\n", chn, param->dim.src_x, param->dim.nr_width, param->dim.src_bg_width);
        ret = -1;
    }

    if ((param->dim.src_y + param->dim.nr_height) > param->dim.src_bg_height) {
        dn_err("Error! (src y [%d] + nr height [%d]) over src background height [%d]\n", param->dim.src_y, param->dim.nr_height, param->dim.src_bg_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] src y [%d] nr height [%d] src bg height [%d]\n", chn, param->dim.src_y, param->dim.nr_height, param->dim.src_bg_height);
        ret = -1;
    }

    if (param->dim.org_width == 0) {
        dn_err("Error! original height [%d] is zero\n", param->dim.org_width);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] original width [%d] is zero\n", chn, param->dim.org_width);
        ret = -1;
    }

    if (param->dim.org_height == 0) {
        dn_err("Error! original height [%d] is zero\n", param->dim.org_height);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] original height [%d] is zero\n", chn, param->dim.org_height);
        ret = -1;
    }

    if (param->src_fmt != TYPE_YUV422_FRAME) {
        dn_err("Error! 3DNR not support source format %x\n", param->src_fmt);
        DEBUG_M(LOG_ERROR, node->engine, node->minor, "Error! ch [%d] 3DNR not support source format [%x]\n", chn, param->src_fmt);
        ret = -1;
    }

    return ret;
}

int vg_set_dn_param(job_node_t *node)
{
    int chan_id = 0;
    //job_node_t *ref_node = NULL;
    int ret = 0;

    chan_id = node->chan_id;
    //ref_node = global_info.ref_node[chan_id];

    ret = sanity_check(node);
    if (ret < 0) {
        dn_err("Error! sanity check fail\n");
        return -1;
    }

    // channel source is from decoder/sensor
    if (node->param.src_type == SRC_TYPE_DECODER || node->param.src_type == SRC_TYPE_SENSOR) {
        /* copy global default ctrl param */
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.ctrl, &priv.default_param.ctrl, sizeof(ft3dnr_ctrl_t));
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }
    // channel source is from isp
    if (node->param.src_type == SRC_TYPE_ISP) {
        /* copy global isp ctrl param */
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_LOCK;
#endif
        memcpy(&node->param.ctrl, &priv.isp_param.ctrl, sizeof(ft3dnr_ctrl_t));
#ifndef DRV_CFG_USE_TASKLET
        DRV_SEMA_UNLOCK;
#endif
    }

    if (node->fmt_changed || node->ref_res_changed)
        node->param.ctrl.temporal_en = 0;

    /* get mrnr param */
    get_mrnr_param(node);

    /* get tmnr param */
    get_tmnr_param(node);

    /* get sp param */
    get_sp_param(node);

    return 0;
}

static inline int assign_frame_buffers(job_node_t *node)
{
    int ch;
    bool dump_job_flow = IS_DUMP_JOB_FLOW;
    int i, start_idx = -1;
    size_t dim_size;

#if 0
    // 3DNR cares only ISP frame
    if (node->param.src_type != SRC_TYPE_ISP)
        return 0;
#endif

    ch = node->chan_id;

    dim_size = (u32) node->param.dim.src_bg_width * (u32) node->param.dim.src_bg_height;

    if ((priv.chan_param[ch].ref_res_p == NULL) ||
            ((priv.chan_param[ch].ref_res_p) && (priv.chan_param[ch].ref_res_p->size < dim_size)))
    {
        if (priv.chan_param[ch].ref_res_p == NULL) {
            start_idx = priv.res_cfg_cnt - 1;
        } else {
            for (i = 0; i < priv.res_cfg_cnt; i++) {
                if (priv.res_cfg[i].map_chan == ch)
                    break;
            }
            start_idx = i - 1;
        }

        for (i = start_idx; i >= 0 ; i--) {
            // serach first unused and matched ref buffer from little-end
            if ((priv.res_cfg[i].map_chan == MAP_CHAN_NONE) && (priv.res_cfg[i].size >= dim_size))
                break;
        }

        if (i < 0) {
            for (i = start_idx; i >= 0 ; i--) {
                // serach first matched ref buffer from little-end, but mapped to other channel
                if (priv.res_cfg[i].size >= dim_size)
                    break;
            }
        }

        if (i < 0) {
            // specfied resolution buffer not available
            printk("%s: ERROR!!!\n", DRIVER_NAME);
            printk("%s: required resolution %dx%d is over spec\n", DRIVER_NAME,
                node->param.dim.src_bg_width, node->param.dim.src_bg_height);
            printk("%s: current supported resolution list:\n", DRIVER_NAME);
            for (i = 0; i < priv.res_cfg_cnt; i++)
                printk("%s: %d. %dx%d\n", DRIVER_NAME, i, priv.res_cfg[i].width, priv.res_cfg[i].height);

            return -1;
        } else {
            if (priv.res_cfg[i].map_chan != MAP_CHAN_NONE) {
                int ch_id = priv.res_cfg[i].map_chan;

                priv.chan_param[ch_id].ref_res_p = NULL;
                priv.chan_param[ch_id].var_buf_p = NULL;
                priv.chan_param[ch_id].mot_buf_p = NULL;
                priv.chan_param[ch_id].ref_buf_p = NULL;
                priv.res_cfg[i].map_chan = MAP_CHAN_NONE;
            }

            // release old res_cfg
            if (priv.chan_param[ch].ref_res_p)
                priv.chan_param[ch].ref_res_p->map_chan = MAP_CHAN_NONE;

            // allocate new res_cfg
            priv.chan_param[ch].ref_res_p = &priv.res_cfg[i];
            priv.chan_param[ch].var_buf_p = &priv.res_cfg[i].var_buf;
            priv.chan_param[ch].mot_buf_p = &priv.res_cfg[i].mot_buf;
            priv.chan_param[ch].ref_buf_p = &priv.res_cfg[i].ref_buf;
            priv.res_cfg[i].map_chan = ch;

            node->ref_res_changed = true;
        }
    }

    // REF buffer address
    node->param.dma.ref_r_addr = node->param.dma.ref_w_addr =
                                            priv.chan_param[ch].ref_buf_p->paddr;

    // VAR buffer address
    node->param.dma.var_addr = priv.chan_param[ch].var_buf_p->paddr;

    // MOT buffer address
    node->param.dma.mot_addr = priv.chan_param[ch].mot_buf_p->paddr;

    // SP buffer address
    node->param.dma.ee_addr = priv.engine.sp_buf.paddr;

    JOB_FLOW_DBG("ref_addr -> 0x%x | var_addr -> 0x%x | mot_addr -> 0x%x | ee_addr -> 0x%x\n",
                 node->param.dma.ref_r_addr,node->param.dma.var_addr,node->param.dma.mot_addr,node->param.dma.ee_addr);

    return 0;
}

int ft3dnr_init_multi_job(job_node_t *node, ft3dnr_job_t *parent)
{
    job_node_t *curr, *ne, *last_node;

    last_node = list_entry(node->clist.prev, job_node_t, clist);

    list_for_each_entry_safe(curr, ne, &node->clist, clist) {
        ft3dnr_job_t *job = NULL;

        if (in_interrupt() || in_atomic() || irqs_disabled())
            job = kmem_cache_alloc(priv.job_cache, GFP_ATOMIC);
        else
            job = kmem_cache_alloc(priv.job_cache, GFP_KERNEL);

        if (unlikely(!job))
            panic("%s, No memory for job! \n", __func__);

        MEMCNT_ADD(&priv, 1);
        memset(job, 0x0, sizeof(ft3dnr_job_t));
        memcpy(&job->param, &curr->param, sizeof(ft3dnr_param_t));

        job->job_type = MULTI_JOB;
        job->perf_type = TYPE_MIDDLE;

        if (curr == last_node)
            job->perf_type = TYPE_LAST;

        INIT_LIST_HEAD(&job->job_list);
        job->chan_id = curr->chan_id;
        job->job_id = curr->job_id;
        job->status = JOB_STS_QUEUE;
        job->need_callback = curr->need_callback;
        job->fops = &callback_ops;
        job->parent = parent;
        job->private = curr;

        ft3dnr_joblist_add(job);
    }

    return 0;
}

int ft3dnr_init_job(job_node_t *node, int mcnt)
{
    ft3dnr_job_t *job = NULL;
    ft3dnr_job_t *parent = NULL;

    if (in_interrupt() || in_atomic() || irqs_disabled())
        job = kmem_cache_alloc(priv.job_cache, GFP_ATOMIC);
    else
        job = kmem_cache_alloc(priv.job_cache, GFP_KERNEL);

    if (unlikely(!job))
        panic("%s, No memory for job! \n", __func__);

    MEMCNT_ADD(&priv, 1);

    memset(job, 0x0, sizeof(ft3dnr_job_t));
    memcpy(&job->param, &node->param, sizeof(ft3dnr_param_t));

    if (mcnt > 0) {
        job->job_type = MULTI_JOB;
        job->perf_type = TYPE_FIRST;
    }
    else {
        job->job_type = SINGLE_JOB;
        job->perf_type = TYPE_ALL;
    }

    INIT_LIST_HEAD(&job->job_list);
    job->chan_id = node->chan_id;
    job->job_id = node->job_id;
    job->status = JOB_STS_QUEUE;
    job->need_callback = node->need_callback;
    job->fops = &callback_ops;
    parent = job;
    job->parent = parent;
    job->private = node;

    ft3dnr_joblist_add(parent);

    if (!list_empty(&node->clist))
        ft3dnr_init_multi_job(node, parent);

    /* add root job to node list */
    vg_nodelist_add(node);

    ft3dnr_put_job();

    return 0;
}

#ifdef DRV_CFG_USE_TASKLET
enum hrtimer_restart pre_callback_timer_fn( struct hrtimer *timer )
{
    //struct timespec ts;
    unsigned long flags;
    job_node_t *node_item = container_of(timer, job_node_t, pre_callback_timer);
    struct video_entity_job_t *job = (struct video_entity_job_t *)node_item->private;

    ft3dnr_dma_t    *dma = &node_item->param.dma;
    DEBUG_M(LOG_WARNING, node_item->engine, node_item->minor, "buffer dst=0x%08x src=0x%08x var=0x%08x ee=0x%08x mot=0x%08x ref_r=0x%08x ref_w=0x%08x  pre_callback id=%d\n",
        dma->dst_addr, dma->src_addr, dma->var_addr, dma->ee_addr, dma->mot_addr, dma->ref_r_addr, dma->ref_w_addr, node_item->job_id);


    DRV_VG_LIST_LOCK(flags);
    //getrawmonotonic(&ts);
    //printk("    pre_callback_timer_fn %x id=%8d at %8ld %8ld --> %8ld\n", node_item, node_item->job_id, ts.tv_sec, ts.tv_nsec, ts.tv_nsec-node_item->puttime);

    if (node_item->need_callback)
    {
        job->status = JOB_STATUS_FINISH;
        job->callback(node_item->private); //callback root job
    }
    node_item->pre_callback_status = CALLBACK_TIMER_CLEAR;
    DRV_VG_LIST_UNLOCK(flags);

    return HRTIMER_NORESTART;
}
#endif

static int driver_putjob(void *parm)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)parm;
    job_node_t *node_item=0, *root_node_item=0;
    int multi_jobs=0, chan_id=0, jobCnt=0;
    int ret=0;
    bool dump_job_flow = IS_DUMP_JOB_FLOW;
    ft3dnr_dma_t    *dma;
#ifdef DRV_CFG_USE_TASKLET
    unsigned long flags;
    //struct timespec ts;
#endif

    do {
        chan_id = MAKE_IDX(max_minors, ENTITY_FD_ENGINE(job->fd), ENTITY_FD_MINOR(job->fd));

        JOB_FLOW_DBG("job(0x%x) coming..(%d) channel %d\n", job->id, jobCnt++, chan_id);

        if (in_interrupt() || in_atomic() || irqs_disabled())
            node_item=kmem_cache_alloc(global_info.node_cache, GFP_ATOMIC);
        else
            node_item=kmem_cache_alloc(global_info.node_cache, GFP_KERNEL);

        if (unlikely(!node_item))
            panic("%s, no memory! \n", __func__);

        MEMCNT_ADD(&global_info, 1);

        memset(node_item, 0x0, sizeof(job_node_t));

#ifdef DRV_CFG_USE_TASKLET
        //init_timer(&node_item->pre_callback_timer);
        hrtimer_init( &node_item->pre_callback_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
        node_item->pre_callback_timer.function = &pre_callback_timer_fn;
        node_item->pre_callback_status = CALLBACK_TIMER_OFF;
#endif

        node_item->engine = ENTITY_FD_ENGINE(job->fd);
        if (node_item->engine > ENTITY_ENGINES)
            panic("Error to use %s engine %d, max is %d\n",MODULE_NAME,node_item->engine,ENTITY_ENGINES);

        node_item->minor = ENTITY_FD_MINOR(job->fd);
        if (node_item->minor > max_minors)
            panic("Error to use %s minor %d, max is %d\n",MODULE_NAME,node_item->minor,max_minors);

        node_item->chan_id = chan_id;

        // parse the parameters and assign to param structure
#ifdef DRV_CFG_USE_TASKLET
        DRV_VG_LIST_LOCK(flags);
#endif
        ret = driver_parse_in_property(node_item, job);

#ifdef DRV_CFG_USE_TASKLET
        DRV_VG_LIST_UNLOCK(flags);
#endif

        node_item->fmt_changed = (ret)?(true):(false);
        if (node_item->fmt_changed)
            JOB_FLOW_DBG("ch%d format changed!\n",chan_id);

#if 0
        /* parameters are changed, should not reference to previous frame */
        if ((ret == 1) && global_info.ref_node[chan_id]) {
            REFCNT_DEC(global_info.ref_node[chan_id]);
            global_info.ref_node[chan_id] = NULL;
        }

        ref_node_item = global_info.ref_node[chan_id];
#endif

        node_item->job_id = job->id;
        node_item->status = JOB_STS_QUEUE;
        node_item->private = job;
        //node_item->refer_to = ref_node_item;
        node_item->puttime = jiffies;
        node_item->starttime = 0;
        node_item->finishtime = 0;

        JOB_FLOW_DBG("put time: %x\n",node_item->puttime);
        //getrawmonotonic(&ts);
        //node_item->puttime = ts.tv_nsec;
        //printk("putjob %x  id=%8d at %8ld %8ld\n", node_item, node_item->job_id, ts.tv_sec, ts.tv_nsec);

        ret = assign_frame_buffers(node_item);
        if (ret < 0)
            goto err;


        node_item->param.dma.src_addr = job->in_buf.buf[0].addr_pa;
        node_item->param.dma.dst_addr = job->out_buf.buf[0].addr_pa;

        dma = &node_item->param.dma;
        DEBUG_M(LOG_WARNING, node_item->engine, node_item->minor, "buffer dst=0x%08x src=0x%08x var=0x%08x ee=0x%08x mot=0x%08x ref_r=0x%08x ref_w=0x%08x  putjob id=%d\n",
            dma->dst_addr, dma->src_addr, dma->var_addr, dma->ee_addr, dma->mot_addr, dma->ref_r_addr, dma->ref_w_addr, node_item->job_id);

#ifdef DRV_CFG_USE_TASKLET
        DRV_VG_LIST_LOCK(flags);
#endif
        ret = vg_set_dn_param(node_item);
#ifdef DRV_CFG_USE_TASKLET
        DRV_VG_LIST_UNLOCK(flags);
#endif
        if (ret < 0) {
            dn_err("Error! set_dn_param fail\n");
            goto err;
        }

#if 0
        REFCNT_INC(node_item);   //add the reference count of this node
		/* update to new */
        global_info.ref_node[chan_id] = node_item;
#endif

        INIT_LIST_HEAD(&node_item->plist);
        INIT_LIST_HEAD(&node_item->clist);

        if (job->next_job == 0) { //last job
            node_item->need_callback = 1;

#ifdef DRV_CFG_USE_TASKLET
            /* start pre-callback timer */
            if (job_pre_callback_period)
            {
                ktime_t ktime;
                ktime = ktime_set( 0, job_pre_callback_period*1000*1000 );
                DRV_VG_LIST_LOCK(flags);
                node_item->pre_callback_status = CALLBACK_TIMER_ACTIVE;
                hrtimer_start( &node_item->pre_callback_timer, ktime, HRTIMER_MODE_REL );
                DRV_VG_LIST_UNLOCK(flags);
            }
#endif

            if (multi_jobs > 0) { // multi job's last job
                multi_jobs++;
                list_add_tail(&node_item->clist, &root_node_item->clist);
            }
        }
        else { // multi-job, but it is not the last one!
            node_item->need_callback = 0;
            multi_jobs++;
            if (multi_jobs == 1)                //record root job
                root_node_item = node_item;
            else
                list_add_tail(&node_item->clist, &root_node_item->clist);
        }

        if (multi_jobs > 0) {                   // multi jobs
            node_item->parent = root_node_item;
            node_item->type = MULTI_NODE;
        }
        else {                                  // single job
            root_node_item = node_item;
            node_item->parent = node_item;
            node_item->type = SINGLE_NODE;
        }

        job = job->next_job;

    } while (job);

#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_LOCK(flags);
#else
    DRV_VG_SEMA_LOCK;
#endif

    if (power_save) {
        if (list_empty(&global_info.node_list)) {
            ft3dnr_add_clock_on();
            platform_clock_control(1);
        }
    }
    ft3dnr_init_job(root_node_item, multi_jobs);
#ifdef DRV_CFG_USE_TASKLET
    DRV_VG_LIST_UNLOCK(flags);
#else
    DRV_VG_SEMA_UNLOCK;
#endif

    return JOB_STATUS_ONGOING;

err:
    printk("%s: putjob error\n", DRIVER_NAME);
    if (multi_jobs == 0) {
#ifdef DRV_CFG_USE_TASKLET
        //del_timer(&node_item->pre_callback_timer);
        hrtimer_cancel( &node_item->pre_callback_timer );
#endif
        kmem_cache_free(global_info.node_cache, node_item);
        MEMCNT_SUB(&global_info, 1);
    }
    else {
        job_node_t *curr, *ne;
        /* free child node */
        list_for_each_entry_safe(curr, ne, &root_node_item->clist, clist) {
#ifdef DRV_CFG_USE_TASKLET
            //del_timer(&curr->pre_callback_timer);
            hrtimer_cancel( &curr->pre_callback_timer );
#endif
            kmem_cache_free(global_info.node_cache, curr);
            MEMCNT_SUB(&global_info, 1);
        }
        /* free parent node */
#ifdef DRV_CFG_USE_TASKLET
        //del_timer(&root_node_item->pre_callback_timer);
        hrtimer_cancel( &root_node_item->pre_callback_timer );
#endif
        kmem_cache_free(global_info.node_cache, root_node_item);
        MEMCNT_SUB(&global_info, 1);
    }
    printm(MODULE_NAME, "PANIC!! putjob error\n");
    damnit(MODULE_NAME);
    return JOB_STATUS_FAIL;
}

struct video_entity_ops_t driver_ops = {
    putjob:      &driver_putjob,
    stop:        &driver_stop,
    queryid:     &driver_queryid,
    querystr:    &driver_querystr,
};

struct video_entity_t ft3dnr_entity = {
    type:       TYPE_3DNR,
    name:       "3dnr",
    engines:    ENTITY_ENGINES,
    minors:     ENTITY_MINORS,
    ops:        &driver_ops
};

int ft3dnr_log_panic_handler(int data)
{
    printm(MODULE_NAME, "PANIC!! Processing Start\n");
    printm(MODULE_NAME, "PANIC!! Processing End\n");
    return 0;
}

int ft3dnr_log_printout_handler(int data)
{
    job_node_t  *node, *ne, *curr, *ne1;

    printm(MODULE_NAME, "PANIC!! PrintOut Start\n");

    list_for_each_entry_safe(node, ne, &global_info.node_list ,plist) {
        if(node->status == JOB_STS_ONGOING)
            printm(MODULE_NAME, "ONGOING JOB ID(%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);
        else if(node->status == JOB_STS_QUEUE)
            printm(MODULE_NAME, "PENDING JOB ID(%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);
        else if(node->status == JOB_STS_SRAM)
            printm(MODULE_NAME, "SRAM JOB ID(%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);
        else if(node->status == JOB_STS_DONOTHING)
            printm(MODULE_NAME, "DONOTHING JOB ID(%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);
        else if(node->status == JOB_STS_DONE)
            printm(MODULE_NAME, "DONE JOB ID(%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);
        else
            printm(MODULE_NAME, "FLUSH JOB ID (%x) TYPE(%x) need_callback(%d)\n", node->job_id, node->type, node->need_callback);

        list_for_each_entry_safe(curr, ne1, &node->clist ,clist) {
            if(curr->status == JOB_STS_ONGOING)
                printm(MODULE_NAME, "ONGOING JOB ID(%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
            else if(curr->status == JOB_STS_QUEUE)
                printm(MODULE_NAME, "PENDING JOB ID(%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
            else if(node->status == JOB_STS_SRAM)
                printm(MODULE_NAME, "SRAM JOB ID(%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
            else if(curr->status == JOB_STS_DONOTHING)
                printm(MODULE_NAME, "DONOTHING JOB ID(%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
            else if(curr->status == JOB_STS_DONE)
                printm(MODULE_NAME, "DONE JOB ID(%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
            else
                printm(MODULE_NAME, "FLUSH JOB ID (%x) TYPE(%x) need_callback(%d)\n", curr->job_id, curr->type, curr->need_callback);
        }
    }

    printm(MODULE_NAME, "PANIC!! PrintOut End\n");
    return 0;
}

void print_info(void)
{
    return;
}

static struct property_map_t *driver_get_propertymap(int id)
{
    int i;

    for(i = 0; i < sizeof(property_map) / sizeof(struct property_map_t); i++) {
        if(property_map[i].id == id) {
            return &property_map[i];
        }
    }
    return 0;
}


void print_filter(void)
{
    int i;

    printk("\nUsage:\n#echo [0:exclude/1:include] Engine Minor > filter\n");
    printk("Driver log Include:");

    for(i = 0;i < MAX_FILTER; i++)
        if(include_filter_idx[i] >= 0)
            printk("{%d,%d},",IDX_ENGINE(include_filter_idx[i],ENTITY_MINORS),
                IDX_MINOR(include_filter_idx[i],ENTITY_MINORS));

    printk("\nDriver log Exclude:");
    for(i = 0; i < MAX_FILTER; i++)
        if(exclude_filter_idx[i] >= 0)
            printk("{%d,%d},",IDX_ENGINE(exclude_filter_idx[i],ENTITY_MINORS),
                IDX_MINOR(exclude_filter_idx[i],ENTITY_MINORS));
    printk("\n");
}

static int vg_proc_cb_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nCallback Period = %d (ticks)\n",callback_period);

    return 0;
}

static ssize_t vg_proc_cb_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  mode_set;
    char value_str[16] = {'\0'};
    struct seq_file       *sfile    = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &mode_set);

    callback_period = mode_set;

    seq_printf(sfile, "\nCallback Period =%d (ticks)\n",callback_period);

    return count;
}

static int vg_proc_filter_show(struct seq_file *sfile, void *v)
{
    print_filter();

    return 0;
}

static ssize_t vg_proc_filter_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i;
    int engine,minor,mode;

    sscanf(buffer, "%d %d %d",&mode,&engine,&minor);

    if (mode == 0) { //exclude
        for (i = 0; i < MAX_FILTER; i++) {
            if (exclude_filter_idx[i] == -1) {
                exclude_filter_idx[i] = (engine << 16) | (minor);
                break;
            }
        }
    } else if (mode == 1) {
        for (i = 0; i < MAX_FILTER; i++) {
            if (include_filter_idx[i] == -1) {
                include_filter_idx[i] = (engine << 16) | (minor);
                break;
            }
        }
    }
    print_filter();
    return count;
}

static int vg_proc_info_show(struct seq_file *sfile, void *v)
{
    print_info();

    return 0;
}

static ssize_t vg_proc_info_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    print_info();

    return count;
}

static int vg_proc_job_show(struct seq_file *sfile, void *v)
{
    unsigned long flags;
    struct video_entity_job_t *job;
    job_node_t  *node, *child;
    char *st_string[] = {"QUEUE", "SRAM", "ONGOING", "FINISH", "FLUSH", "DONOTHING"}, *string = NULL;
    char *type_string[]={"   ","(M)","(*)"}; //type==0,type==1 && need_callback=0, type==1 && need_callback=1

    seq_printf(sfile, "\nSystem ticks=0x%x\n", (int)jiffies & 0xffff);

    seq_printf(sfile, "Chnum  Job_ID     Status     Puttime      start    end \n");
    seq_printf(sfile, "===========================================================\n");

    DRV_VG_SPIN_LOCK(flags);

    list_for_each_entry(node, &global_info.node_list, plist) {
        if (node->status & JOB_STS_QUEUE)       string = st_string[0];
        if (node->status & JOB_STS_SRAM)        string = st_string[1];
        if (node->status & JOB_STS_ONGOING)     string = st_string[2];
        if (node->status & JOB_STS_DONE)        string = st_string[3];
        if (node->status & JOB_STS_FLUSH)       string = st_string[4];
        if (node->status & JOB_STS_DONOTHING)   string = st_string[5];
        job = (struct video_entity_job_t *)node->private;
        seq_printf(sfile, "%-5d %s %-9d  %-9s  %08x  %08x  %08x \n", node->minor,
        type_string[(node->type*0x3)&(node->need_callback+1)],
        job->id, string, node->puttime, node->starttime, node->finishtime);

        list_for_each_entry(child, &node->clist, clist) {
            if (child->status & JOB_STS_QUEUE)       string = st_string[0];
            if (child->status & JOB_STS_SRAM)        string = st_string[1];
            if (child->status & JOB_STS_ONGOING)     string = st_string[2];
            if (child->status & JOB_STS_DONE)        string = st_string[3];
            if (child->status & JOB_STS_FLUSH)       string = st_string[4];
            if (child->status & JOB_STS_DONOTHING)   string = st_string[5];
            job = (struct video_entity_job_t *)child->private;
            seq_printf(sfile, "%-5d %s %-9d  %-9s  %08x  %08x  %08x \n", child->minor,
            type_string[(child->type*0x3)&(child->need_callback+1)],
            job->id, string, child->puttime, child->starttime, child->finishtime);
        }
    }

    DRV_VG_SPIN_UNLOCK(flags);

    return 0;
}

static int vg_proc_level_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nLog level = %d (0:emerge 1:error 2:debug)\n\n", log_level);

    return 0;
}

static ssize_t vg_proc_level_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  level;
    char value_str[16] = {'\0'};
    struct seq_file       *sfile    = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &level);

    log_level = level;

    seq_printf(sfile, "\nLog level = %d (0:emerge 1:error 2:debug)\n\n", log_level);

    return count;
}

static unsigned int property_engine = 0, property_minor = 0;
static int vg_proc_property_show(struct seq_file *sfile, void *v)
{
    int i = 0;
    struct property_map_t *map;
    unsigned int id, value, pch;
    unsigned long flags;

    int idx = MAKE_IDX(ENTITY_MINORS, property_engine, property_minor);

    DRV_VG_SPIN_LOCK(flags);

    seq_printf(sfile, "\n%s engine%d ch%d job %d\n",property_record[idx].entity->name,
        property_engine,property_minor,property_record[idx].job_id);
    seq_printf(sfile, "=============================================================\n");
    seq_printf(sfile, "ID  Name(string) Value(hex)  Readme\n");

    do {
        pch = property_record[idx].property[i].pch;
        id = property_record[idx].property[i].id;
        if (id == ID_NULL)
            break;

        value = property_record[idx].property[i].value;

        if ((map=driver_get_propertymap(id)) != NULL) {
            //printk("%02d  %12s  %09d  %s\n",id,map->name,value,map->readme);
            seq_printf(sfile, "%02d %02d  %12s  %08x  %s\n",pch, id, map->name, value, map->readme);
        }
        i++;
    } while(1);

    seq_printf(sfile, "\n");

    DRV_VG_SPIN_UNLOCK(flags);

    return 0;
}

static int vg_proc_util_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\n HW Utilization\nPeriod : %d(sec)\nUtilization : %d\n\n",
                                    utilization_period,utilization_record[0]);

    return 0;
}

static int vg_proc_res_cfg_show(struct seq_file *sfile, void *v)
{
    int i;
    unsigned long flags;

    seq_printf(sfile, "\nReference Buffer Status\n");
    seq_printf(sfile, "=============================================================\n");
    seq_printf(sfile, " Type  Dimension  Alloc_Minor\n");

    DRV_VG_SPIN_LOCK(flags);

    for (i = 0; i < priv.res_cfg_cnt; i++)
        seq_printf(sfile, "%5s  %4dx%4d  %c\n", priv.res_cfg[i].res_type,
                    priv.res_cfg[i].width, priv.res_cfg[i].height,
                    priv.res_cfg[i].map_chan == MAP_CHAN_NONE ? 'N' : (priv.res_cfg[i].map_chan + 0x30));

    seq_printf(sfile, "\n");

    DRV_VG_SPIN_UNLOCK(flags);

    return 0;
}

static int vg_proc_perf_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "performance period = %d seconds\n", perf_period);

    return 0;
}

static ssize_t vg_proc_perf_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  mode_set;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &mode_set);

    perf_period = mode_set;

    printk("Set performance period = %d seconds\n", perf_period);

    return count;
}

#ifdef DRV_CFG_USE_TASKLET
static int vg_proc_job_pre_callback_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nJob Pre-Callback Period = %d (ms)\n",job_pre_callback_period);

    return 0;
}

static ssize_t vg_proc_job_pre_callback_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int  mode_set;
    char value_str[16] = {'\0'};
    struct seq_file       *sfile    = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &mode_set);

    job_pre_callback_period = mode_set;

    seq_printf(sfile, "\nJob Pre-Callback Period =%d (ms)\n",job_pre_callback_period);

    return count;
}
#endif

static ssize_t vg_proc_property_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_engine = 0, mode_minor = 0;
    char value_str[16] = {'\0'};
    struct seq_file       *sfile    = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d %d\n", &mode_engine, &mode_minor);

    property_engine = mode_engine;
    property_minor = mode_minor;

    seq_printf(sfile, "\nLookup engine=%d minor=%d\n\n",property_engine,property_minor);

    return count;
}

static ssize_t vg_proc_util_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_set = 0;
    char value_str[16] = {'\0'};
    struct seq_file       *sfile    = (struct seq_file *)file->private_data;

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d \n", &mode_set);
    utilization_period = mode_set;

    seq_printf(sfile, "\nUtilization Period =%d(sec)\n",utilization_period);

    return count;
}

static int vg_proc_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_info_show, PDE(inode)->data);
}

static int vg_proc_cb_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_cb_show, PDE(inode)->data);
}

static int vg_proc_filter_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_filter_show, PDE(inode)->data);
}

static int vg_proc_job_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_job_show, PDE(inode)->data);
}

static int vg_proc_level_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_level_show, PDE(inode)->data);
}

static int vg_proc_property_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_property_show, PDE(inode)->data);
}

static int vg_proc_util_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_util_show, PDE(inode)->data);
}

static int vg_proc_res_cfg_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_res_cfg_show, PDE(inode)->data);
}

static int vg_proc_perf_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_perf_show, PDE(inode)->data);
}

#ifdef DRV_CFG_USE_TASKLET
static int vg_proc_job_pre_callback_open(struct inode *inode, struct file *file)
{
    return single_open(file, vg_proc_job_pre_callback_show, PDE(inode)->data);
}
#endif


static struct file_operations vg_proc_cb_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_cb_open,
    .write  = vg_proc_cb_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_info_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_info_open,
    .write  = vg_proc_info_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_filter_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_filter_open,
    .write  = vg_proc_filter_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_job_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_job_open,
    //.write  = vg_proc_job_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_level_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_level_open,
    .write  = vg_proc_level_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_property_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_property_open,
    .write  = vg_proc_property_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations vg_proc_util_ops = {
    .owner   = THIS_MODULE,
    .open    = vg_proc_util_open,
    .write   = vg_proc_util_write,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static struct file_operations vg_proc_res_cfg_ops = {
    .owner   = THIS_MODULE,
    .open    = vg_proc_res_cfg_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static struct file_operations vg_proc_perf_ops = {
    .owner   = THIS_MODULE,
    .open    = vg_proc_perf_open,
    .write   = vg_proc_perf_write,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

#ifdef DRV_CFG_USE_TASKLET
static struct file_operations vg_proc_job_pre_callback_ops = {
    .owner  = THIS_MODULE,
    .open   = vg_proc_job_pre_callback_open,
    .write  = vg_proc_job_pre_callback_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};
#endif

#define ENTITY_PROC_NAME "videograph/ft3dnr"

static int vg_proc_init(void)
{
    int ret = 0;

    /* create proc */
    vg_proc_info.root = create_proc_entry(ENTITY_PROC_NAME, S_IFDIR|S_IRUGO|S_IXUGO, NULL);
    if(vg_proc_info.root == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.root->owner = THIS_MODULE;
#endif

    /* info */
    vg_proc_info.info = create_proc_entry("info", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.info == NULL) {
        printk("error to create %s/info proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    (vg_proc_info.info)->proc_fops  = &vg_proc_info_ops;
    (vg_proc_info.info)->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    (vg_proc_info.info)->owner      = THIS_MODULE;
#endif

    /* cb */
    vg_proc_info.cb = create_proc_entry("callback_period", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.cb == NULL) {
        printk("error to create %s/cb proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    (vg_proc_info.cb)->proc_fops  = &vg_proc_cb_ops;
    (vg_proc_info.cb)->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.cb->owner      = THIS_MODULE;
#endif

    /* property */
    vg_proc_info.property = create_proc_entry("property", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.property == NULL) {
        printk("error to create %s/property proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.property->proc_fops  = &vg_proc_property_ops;
    vg_proc_info.property->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.property->owner      = THIS_MODULE;
#endif

    /* job */
    vg_proc_info.job = create_proc_entry("job", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.job == NULL) {
        printk("error to create %s/job proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.job->proc_fops  = &vg_proc_job_ops;
    vg_proc_info.job->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.job->owner      = THIS_MODULE;
#endif

    /* filter */
    vg_proc_info.filter = create_proc_entry("filter", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.filter == NULL) {
        printk("error to create %s/filter proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.filter->proc_fops  = &vg_proc_filter_ops;
    vg_proc_info.filter->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.filter->owner      = THIS_MODULE;
#endif

    /* level */
    vg_proc_info.level = create_proc_entry("level", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.level == NULL) {
        printk("error to create %s/level proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.level->proc_fops  = &vg_proc_level_ops;
    vg_proc_info.level->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.level->owner      = THIS_MODULE;
#endif

    /* utilization */
    vg_proc_info.util = create_proc_entry("utilization", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.util == NULL) {
        printk("error to create %s/util proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.util->proc_fops  = &vg_proc_util_ops;
    vg_proc_info.util->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.util->owner      = THIS_MODULE;
#endif

    /* res_cfg */
    vg_proc_info.res_cfg = create_proc_entry("res_cfg", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.res_cfg == NULL) {
        printk("error to create %s/res_cfg proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.res_cfg->proc_fops  = &vg_proc_res_cfg_ops;
    vg_proc_info.res_cfg->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.res_cfg->owner      = THIS_MODULE;
#endif

    /* perf */
    vg_proc_info.perf = create_proc_entry("perf", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.perf == NULL) {
        printk("error to create %s/perf proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    vg_proc_info.perf->proc_fops  = &vg_proc_perf_ops;
    vg_proc_info.perf->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.perf->owner      = THIS_MODULE;
#endif

#ifdef DRV_CFG_USE_TASKLET
    /* job pre-callback */
    vg_proc_info.job_pre_callback = create_proc_entry("job_pre_callback", S_IRUGO|S_IXUGO, vg_proc_info.root);
    if(vg_proc_info.job_pre_callback == NULL) {
        printk("error to create %s/job_pre_callback proc\n", ENTITY_PROC_NAME);
        ret = -EFAULT;
    }
    (vg_proc_info.job_pre_callback)->proc_fops  = &vg_proc_job_pre_callback_ops;
    (vg_proc_info.job_pre_callback)->data       = (void *)&vg_proc_info;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_proc_info.job_pre_callback->owner      = THIS_MODULE;
#endif
#endif
    return 0;
}

void vg_proc_remove(void)
{
    if(vg_proc_info.root) {
        if (vg_proc_info.cb != 0)
            remove_proc_entry(vg_proc_info.cb->name, vg_proc_info.root);
        if (vg_proc_info.util != 0)
            remove_proc_entry(vg_proc_info.util->name, vg_proc_info.root);
        if (vg_proc_info.property != 0)
            remove_proc_entry(vg_proc_info.property->name, vg_proc_info.root);
        if (vg_proc_info.job != 0)
            remove_proc_entry(vg_proc_info.job->name, vg_proc_info.root);
        if (vg_proc_info.filter != 0)
            remove_proc_entry(vg_proc_info.filter->name, vg_proc_info.root);
        if (vg_proc_info.level != 0)
            remove_proc_entry(vg_proc_info.level->name, vg_proc_info.root);
        if (vg_proc_info.info != 0)
            remove_proc_entry(vg_proc_info.info->name, vg_proc_info.root);
        if (vg_proc_info.res_cfg != 0)
            remove_proc_entry(vg_proc_info.res_cfg->name, vg_proc_info.root);
        if (vg_proc_info.perf != 0)
            remove_proc_entry(vg_proc_info.perf->name, vg_proc_info.root);
#ifdef DRV_CFG_USE_TASKLET
        if (vg_proc_info.job_pre_callback != 0)
            remove_proc_entry(vg_proc_info.job_pre_callback->name, vg_proc_info.root);
#endif
        remove_proc_entry(vg_proc_info.root->name, vg_proc_info.root->parent);
    }
}


int ft3dnr_vg_init(void)
{
    int ret = 0;

    ft3dnr_entity.minors = max_minors;

    property_record = kzalloc(sizeof(struct property_record_t) * ENTITY_ENGINES * max_minors, GFP_KERNEL);
    if (ZERO_OR_NULL_PTR(property_record))
        panic("%s: allocate memory fail for property_record(0x%p)!\n", __func__, property_record);

    /* global information */
    memset(&global_info, 0x0, sizeof(global_info_t));

    INIT_LIST_HEAD(&global_info.node_list);

    video_entity_register(&ft3dnr_entity);

    /* spinlock */
    spin_lock_init(&global_info.lock);
#ifdef DRV_CFG_USE_TASKLET
    spin_lock_init(&global_info.list_lock);
#endif

    /* init semaphore lock */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
    sema_init(&global_info.sema_lock, 1);
#else
    init_MUTEX(&global_info.sema_lock);
#endif

    /* create memory cache */
    global_info.node_cache = kmem_cache_create("ft3dnr_vg", sizeof(job_node_t), 0, 0, NULL);
    if (global_info.node_cache == NULL)
        panic("ft3dnr: fail to create cache!");

#ifdef DRV_CFG_USE_TASKLET
	/* tasklet */
	tasklet_init(&callback_tasklet, ft3dnr_callback_task, (unsigned long) 0);
#else
#ifdef DRV_CFG_USE_KTHREAD
    init_waitqueue_head(&cb_thread_wait);
    cb_thread = kthread_create(ft3dnr_cb_thread, NULL, "ft3dnr_cbd");
    if (IS_ERR(cb_thread))
        panic("%s, create cb_thread fail ! \n", __func__);
    wake_up_process(cb_thread);
#else
    /* create workqueue */
    callback_workq = create_workqueue("ft3dnr_callback");
    if (callback_workq == NULL) {
        printk("ft3dnr: error in create callback workqueue! \n");
        return -1;
    }
#endif
#endif

    /* register log system callback function */
    ret = register_panic_notifier(ft3dnr_log_panic_handler);
    if (ret < 0) {
        printk("ft3dnr register log system panic notifier failed!\n");
        return -1;
    }

    ret = register_printout_notifier(ft3dnr_log_printout_handler);
    if (ret < 0) {
        printk("ft3dnr register log system printout notifier failed!\n");
        return -1;
    }

    /* vg proc init */
    ret = vg_proc_init();
    if (ret < 0)
        printk("ft3dnr videograph proc node init failed!\n");

    memset(engine_time, 0, sizeof(unsigned int));
    memset(engine_start, 0, sizeof(unsigned int));
    memset(engine_end, 0, sizeof(unsigned int));
    memset(utilization_start, 0, sizeof(unsigned int));
    memset(utilization_record, 0, sizeof(unsigned int));

    return 0;
}

void ft3dnr_vg_driver_clearnup(void)
{
    /* vg proc remove */
    vg_proc_remove();

    video_entity_deregister(&ft3dnr_entity);

#ifdef DRV_CFG_USE_TASKLET
    tasklet_kill(&callback_tasklet);
#else
#ifdef DRV_CFG_USE_KTHREAD
    if (cb_thread) {
        kthread_stop(cb_thread);
        /* wait thread to be terminated */
        while (cb_thread_ready) {
            msleep(10);
        }
    }
#else
    /* destroy workqueue */
    destroy_workqueue(callback_workq);
#endif
#endif

    kfree(property_record);
    kmem_cache_destroy(global_info.node_cache);
}

#ifdef DRV_CFG_USE_TASKLET
void ft3dnr_callback_task(unsigned long data)
{
    ft3dnr_callback_process(NULL); /* callback process */
}

static void ft3dnr_cb_wakeup(void)
{
    tasklet_schedule(&callback_tasklet);
}
#else
#ifdef DRV_CFG_USE_KTHREAD
static int ft3dnr_cb_thread(void *data)
{
    int status;

    if (data) {}

    /* ready */
    cb_thread_ready = 1;

    do {
        status = wait_event_timeout(cb_thread_wait, cb_wakeup_event, msecs_to_jiffies(5*1000));
        if (status == 0)
            continue;   /* timeout */
        cb_wakeup_event = 0;

        if (!kthread_should_stop())
            ft3dnr_callback_process(NULL); /* callback process */
    } while(!kthread_should_stop());

    cb_thread_ready = 0;
    return 0;
}

static void ft3dnr_cb_wakeup(void)
{
    cb_wakeup_event = 1;
    wake_up(&cb_thread_wait);
}
#endif /* DRV_CFG_USE_KTHREAD */
#endif
