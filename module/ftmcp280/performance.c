/* not ready */
// performance
extern unsigned int video_gettime_max_ms(void);
unsigned int video_gettime_ms(void);

//#define REC_PREF_LOG
#ifdef REC_PREF_LOG
#define MAX_LOG_CNT     100
struct job_log_t
{
    unsigned int put_time;
    //unsigned int tri_time;
    unsigned int start_time;
    unsigned int fin_time;
    unsigned int cb_time;
    int id;
} job_log[MAX_LOG_CNT];
static unsigned int job_cnt = 0;

static void rec_job_log(struct job_item_t *job_item)
{
    if (job_cnt < MAX_LOG_CNT) {
        //printk("rec %d %d\n", job_item->puttime, job_item->callbacktime);
        job_log[job_cnt].put_time = job_item->puttime;
        //job_log[job_cnt].tri_time = job_item->triggertime;
        job_log[job_cnt].start_time = job_item->starttime;
        job_log[job_cnt].fin_time = job_item->finishtime;
        job_log[job_cnt].cb_time = job_item->callbacktime;
        job_log[job_cnt].id = job_item->job_id;
        job_cnt++;        
    }
}

static void dump_job_log(void)
{
    int i;
    for (i = 0; i<job_cnt; i++) {
        printk("job %d\t%d\t%d\t%d\t%d\n", job_log[i].id, job_log[i].put_time, 
            job_log[i].start_time, job_log[i].fin_time, job_log[i].cb_time);
    }
}
#endif
