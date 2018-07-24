/**
 * @file job.h (wrapper)
 *  EM job structure header file (wrapper V1)
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/08/09 02:55:26 $
 *
 * ChangeLog:
 *  $Log: job.h,v $
 *  Revision 1.2  2016/08/09 02:55:26  ivan
 *  update for v1 wrapper
 *
 */
#ifndef _JOB_H_
#define _JOB_H_
#include <linux/jiffies.h>

/* video job status */
#define JOB_STATUS_FAIL     -1
#define JOB_STATUS_ONGOING  0
#define JOB_STATUS_FINISH   1

#define JOB_FLOW_LOG_NUM    32

#define PRIORITY_NORMAL     0
#define PRIORITY_HIGH       1
#define PRIORITY_FORCE      2   //force this job to process right now (needn't add to list handling)

/* job */
struct video_entity_job_t {
    unsigned int id;    // job id
    unsigned int ver;   // version of property
    int fd;         // the entity fd of job
    void *entity;   // point to (struct video_entity_t *)
    int priority;   // providei by EM user, job priority, 0(normal) 1(high)  2(force)
    int status;     // job callback return status by video entity
    int group_id;   // indicate group of capture, 0 for not group job
    int (*callback)(void *);    // callback function by video entity
    int (*src_callback)(void *);// callback function by GS module

    unsigned long long root_time;  //indicate "root" callback time by video entity

    struct video_entity_job_t *next_job; // indicate next job->job->job->NULL

    struct video_properties_t in_prop;   // input property set
    struct video_properties_t out_prop;  // output property set

    struct video_bufinfo_t in_buf;
    struct video_bufinfo_t out_buf;

    void *em_user_priv;  // internal used by EM module
    void *container;     // internal used by EM module
    int  reserved1;

    struct list_head putjob_list;    //internal used by EM module
    struct list_head callback_list;  //internal used by EM module

#define FLAGS_PUTJOB    (1 << 0)    //indicate this job is put to entity already
#define FLAGS_ROOTJOB   (1 << 1)    //indicate first job (multi-job head)
#define FLAGS_USER_DONE (1 << 2)    //indicate user putjob done
    unsigned int internal_flags; // internal flags to indicate state machine

    /* debug log */
    unsigned int flow_log[JOB_FLOW_LOG_NUM];
    unsigned int flow_log_idx;
    unsigned int finished_jiffies;

    unsigned int reserved[10];
};


static inline void video_set_finish_time(void *job_ptr, unsigned int jiffies_plus)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)job_ptr;
    job->finished_jiffies = jiffies + jiffies_plus;
}

static inline unsigned int video_get_finish_time(void *job_ptr)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)job_ptr;
    return job->finished_jiffies;
}

static inline void video_flow_start(void *job_ptr)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)job_ptr;
    char *log_ptr;
    if (job->flow_log_idx)
        return; //already start

    log_ptr = (char *)&job->flow_log[job->flow_log_idx];
    log_ptr[0] = '#'; log_ptr[1] = '#'; log_ptr[2] = '#'; log_ptr[3] = '#';
    job->flow_log_idx = 1;
}

//name: 2 bytes  condiction: 4 bytes
static inline void video_flow_log(void *job_ptr, char name[], char condiction[],
                                  unsigned int timestamp)
{
    struct video_entity_job_t *job = (struct video_entity_job_t *)job_ptr;
    char *log_ptr;
    char cname[4];

    if (job->flow_log_idx < 1)
        return;
    if ((job->flow_log[0] == 0) && (job->flow_log_idx != 0)) {
        printk("Error %s log init procedure (idx %d)\n", name, job->flow_log_idx);
        return;
    }
    if (job->flow_log_idx >= JOB_FLOW_LOG_NUM - 2) {
        printk("%c%c jid(%d) flow_log overflow %d\n", name[0], name[1], job->id, job->flow_log_idx);
        //printm("FL","video_flow_log jid(%d) %c%c %d damnit\n", job->id, name[0], name[1], job->flow_log_idx);
        //damnit("EM");
        return;
    }
    memset(cname, 0, sizeof(cname));
    strcpy(cname, condiction);

    log_ptr = (char *)&job->flow_log[job->flow_log_idx];
    log_ptr[0] = timestamp & 0xff;
    log_ptr[1] = (timestamp >> 8) & 0xff;
    log_ptr[2] = name[0];
    log_ptr[3] = name[1];
    log_ptr[4] = cname[0];
    log_ptr[5] = cname[1];
    log_ptr[6] = cname[2];
    log_ptr[7] = cname[3];
    job->flow_log_idx += 2;
}


#define MAX_BUFFER_INDEX    32
struct entity_buffer_info_t {
    unsigned int offset[MAX_BUFFER_INDEX];
    unsigned short width[MAX_BUFFER_INDEX];
    unsigned short height[MAX_BUFFER_INDEX];
    unsigned int fmt[MAX_BUFFER_INDEX];
};

#endif
