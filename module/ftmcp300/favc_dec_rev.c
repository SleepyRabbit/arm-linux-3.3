#include <linux/interrupt.h> /* for in_interrupt() */
#include "favc_dec_rev.h"
#include "favc_dec_dbg.h"


#if REV_PLAY /* skip the whole file if REV_PLAY = 0 */

extern struct list_head *favcd_gop_minor_head;  // gop job group head of each channel
extern struct list_head  favcd_standby_head;    // standby job list head
extern struct list_head *favcd_fail_minor_head; // failed job list head of each channel
extern struct list_head  favcd_callback_head;   // sort by callback order

/* extern functions */
int __favcd_mark_buf_released(struct favcd_data_t *dec_data, int buf_idx);
void *fkmalloc(size_t size);
void fkfree(void *ptr);
int __favcd_output_all_frame(struct favcd_data_t *dec_data);
void trigger_prepare(void);
int favcd_set_out_property_poc(struct favcd_data_t *dec_data, struct favcd_job_item_t *job_item);



void mark_output_group_as_reserved(struct favcd_gop_job_item_t *gop_job_item)
{
    int i;
    int start_idx;
    int end_idx;

    start_idx = gop_job_item->next_output_job_idx - (gop_job_item->output_group_size - 1);
    if(start_idx < 0)
        start_idx = 0;

    end_idx = gop_job_item->next_output_job_idx;

    DEBUG_M(LOG_REV_PLAY, 0, 0, "(%d)mark_output_group_as_reserved: %d - %d (next_output:%d group_size:%d gop_size:%d)\n", gop_job_item->chn_idx, start_idx, end_idx, 
        gop_job_item->next_output_job_idx, gop_job_item->output_group_size, gop_job_item->gop_size);


    for(i = start_idx; i <= end_idx; i++){
        gop_job_item->is_reserved[i] = 1;
    }
}


/*
 * mark non-reserve buffers in release list as released
 */
int __favcd_process_gop_release_list(struct favcd_data_t *dec_data)
{
    unsigned long flags;
    struct favcd_gop_job_item_t *gop_job_item;
    //struct favcd_job_item_t *job_item;
    int i;
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail_list and callback list */
    
    list_for_each_entry(gop_job_item, &favcd_gop_minor_head[dec_data->minor], gop_job_list) {
        for(i = 0; i < gop_job_item->gop_size; i++){
            /* mark the released buffer that is not reserved any more as released */
            if(gop_job_item->is_released[i] == 1 && gop_job_item->is_outputed[i] == 1){
                __favcd_mark_buf_released(dec_data, gop_job_item->gop_jobs[i]->buf_idx);
                gop_job_item->is_released[i] = 3;
            }
#if 0
            else if(gop_job_item->is_released[i] == 1 && gop_job_item->is_reserved[i] == 0){
                __favcd_mark_buf_released(dec_data, gop_job_item->gop_jobs[i]->buf_idx);
                gop_job_item->is_released[i] = 3;
            }
#endif
        }

#if 0//GOP_JOB_STOP_HANDLING
        /* release all jobs */
        if(gop_job_item->stop_flg || gop_job_item->err_flg){
            for(i = 0; i < gop_job_item->gop_size; i++){
                job_item = gop_job_item->gop_jobs[i];
                if(job_item == NULL){
                    continue;
                }
                if(gop_job_item->is_released[i] == 0){
                    __favcd_mark_buf_released(dec_data, job_item->buf_idx);
                    DEBUG_M(LOG_ERROR, dec_data->engine, dec_data->minor, "found an un-released buffer: %d for job: %d seq:%d\n", job_item->buf_idx, job_item->job_id, job_item->seq_num);
                    gop_job_item->is_released[i] = 3;
                }
            }
        }
#endif
    }
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list and callback list */

    return 0;
}


void set_gop_job_item_bs_buf(struct favcd_gop_job_item_t *gop_job_item, struct favcd_job_item_t *job_item, int bs_idx)
{
    DEBUG_M(LOG_REV_PLAY, 0, 0, "set_gop_job_item_bs_buf to %d  for job %d seq:%d st:%d\n", bs_idx, job_item->job_id, job_item->seq_num, job_item->status);
    //DUMP_MSG(D_REV, private_data[job_item->minor].dec_handle, "set_gop_job_item_bs_buf to %d  for job %d seq:%d st:%d\n", bs_idx, job_item->job_id, job_item->seq_num, job_item->status);

    job_item->bs_addr_va = job_item->job->in_buf.buf[0].addr_va + gop_job_item->bs_offset[bs_idx];
    job_item->bs_addr_pa = job_item->job->in_buf.buf[0].addr_pa + gop_job_item->bs_offset[bs_idx];
    job_item->bs_size = gop_job_item->bs_len[bs_idx];
    job_item->bs_idx = bs_idx;
}

int __favcd_reuse_gop_job_item(struct favcd_job_item_t *curr_job_item)
{
    struct favcd_gop_job_item_t *gop_job_item = curr_job_item->gop_job_item;
    struct favcd_job_item_t *job_item;
    int i;
    int reuse_cnt = 0;

    //if(gop_job_item->next_dec_bs_idx > gop_job_item->next_output_job_idx){
    if(gop_job_item->next_dec_bs_idx + gop_job_item->pp_rdy_job_cnt > gop_job_item->next_output_job_idx){
        DEBUG_M(LOG_REV_PLAY, 0, 0, "__favcd_reuse_gop_job_item next_dec_bs_idx:%d + pp_rdy_job_cnt:%d > next_output_job_idx:%d\n", 
            gop_job_item->next_dec_bs_idx, gop_job_item->pp_rdy_job_cnt, gop_job_item->next_output_job_idx);
        return reuse_cnt;
    }
    
    for(i = 0; i < gop_job_item->gop_size; i++){
        job_item = gop_job_item->gop_jobs[i];
        if(job_item == NULL)
            continue;
        
        DEBUG_M(LOG_REV_PLAY, 0, 0, "__favcd_reuse_gop_job_item %d is_outputed:%d is_released:%d is_reserved:%d (id:%d seq:%d st:%d)\n", 
            i, gop_job_item->is_outputed[i], gop_job_item->is_released[i], gop_job_item->is_reserved[job_item->bs_idx], 
            job_item->job_id, job_item->seq_num, job_item->status);
        
#if REALLOC_BUF_FOR_GOP_JOB
        //if(gop_job_item->is_outputed[i] == 0 && gop_job_item->is_released[i] == 3)
        if(gop_job_item->is_outputed[i] == 0 && gop_job_item->is_released[i] == 3 && gop_job_item->is_reserved[job_item->bs_idx] == 0)
#else
        if(gop_job_item->is_outputed[i] == 0 && gop_job_item->is_released[i] == 1)
#endif
        {
            //add_gop_job_item(gop_job_item, gop_job_item->gop_jobs[i]);
            
            SET_JOB_STATUS(job_item, DRIVER_STATUS_STANDBY);
#if REALLOC_BUF_FOR_GOP_JOB == 0
            set_gop_job_item_bs_buf(gop_job_item, job_item, gop_job_item->next_dec_bs_idx);
#else
            job_item->set_bs_flg = 0;
#endif
            /* clear flags to avoid output it before it is decoded */
            gop_job_item->is_released[i] = 0;
            gop_job_item->dec_done[i] = 0;
            gop_job_item->got_poc[i] = 0;
            job_item->prepared_flg = 0;

            reuse_cnt++;

#if REALLOC_BUF_FOR_GOP_JOB
            /* add job item to prepare_list */
            gop_job_item->pp_rdy_job_cnt++;
            __favcd_list_add_tail(&job_item->standby_list, &favcd_standby_head);
#else
            /* add job item to ready_list */
            __favcd_list_add_tail(&job_item->ready_list, &favcd_ready_head);
            private_data[job_item->minor].ready_job_cnt++;
            gop_job_item->next_dec_bs_idx++;
#endif
            
            if(gop_job_item->next_dec_bs_idx + gop_job_item->pp_rdy_job_cnt > gop_job_item->next_output_job_idx){
                break;
            }
            
        }
    }

    return reuse_cnt;
}

/*
 * 1. output all frames if necessary
 * 2. perform seek back
 * 3. reuse decoded job if job number is not enough
 */
int __favcd_gop_job_post_process(struct favcd_job_item_t *curr_job_item)
{
    struct favcd_gop_job_item_t *gop_job_item;
    //struct favcd_job_item_t *job_item;
    //int i;
    unsigned long flags;
    int trig_pp_flg = 0;
    struct favcd_data_t *dec_data;
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail_list and callback list */

    gop_job_item = curr_job_item->gop_job_item;
    if(gop_job_item == NULL){
        goto ret;
    }
    
    dec_data = favcd_get_dec_data(curr_job_item->minor);


    if(gop_job_item->output_cnt == gop_job_item->gop_size){
        /* flush buffers */
        __favcd_output_all_frame(dec_data);
        DEBUG_M(LOG_REV_PLAY, 0, 0, "output all frame of the gop (gop size: %d output_cnt:%d next_output_job_idx:%d)\n", 
            gop_job_item->gop_size, gop_job_item->output_cnt, gop_job_item->next_output_job_idx);
        goto ret;
    }

    if(gop_job_item->seek_flg){
        DEBUG_M(LOG_REV_PLAY, 0, 0, "re-decode from idx 0 (gop size: %d output_cnt:%d next_output_job_idx:%d)\n",
            gop_job_item->gop_size, gop_job_item->output_cnt, gop_job_item->next_output_job_idx);

        /* flush buffers */
        __favcd_output_all_frame(dec_data);

        if(gop_job_item->output_cnt == gop_job_item->gop_size){
            DEBUG_M(LOG_REV_PLAY, 0, 0, "skip seek back since all frame of the gop has been outputed (gop size: %d output_cnt:%d next_output_job_idx:%d)\n", 
                gop_job_item->gop_size, gop_job_item->output_cnt, gop_job_item->next_output_job_idx);
            goto skip_seek;
        }

        
#if 1
        gop_job_item->next_dec_bs_idx = 0;
#else
        /* find the next dec_bs_idx */
        /* FIXME: what if there are ready jobs? */
        for(i = gop_job_item->next_output_job_idx - 1; i >= 0; i--){
            if(gop_job_item->is_reserved[i]){
                gop_job_item->next_dec_bs_idx = i + 1;
                break;
            }
        }
        
        if(i < 0){
            gop_job_item->next_dec_bs_idx = 0;
        }
#endif
#if 0
        if(gop_job_item->next_dec_bs_idx > gop_job_item->next_output_job_idx){
            gop_job_item->next_dec_bs_idx = 0;
        }
#endif

#if GOP_JOB_STOP_HANDLING
        if(gop_job_item->stop_flg == 1 || gop_job_item->err_flg == 1){
            DEBUG_M(LOG_REV_PLAY, 0, 0, "skip reuse gop job itme due to stop(%d) or err(%d)\n", gop_job_item->stop_flg, gop_job_item->err_flg);
            goto skip_seek;
        }
#endif
        
        mark_output_group_as_reserved(gop_job_item);
        __favcd_reuse_gop_job_item(curr_job_item);

skip_seek:
        gop_job_item->seek_flg = 0;
#if REALLOC_BUF_FOR_GOP_JOB
        trig_pp_flg = 1;
#else
        trig_pp_flg = 0;
#endif
    }
    else if((gop_job_item->pp_rdy_job_cnt + gop_job_item->next_dec_bs_idx) <= gop_job_item->next_output_job_idx){
        /* reuse job_items if pp/ready job number is not enough */
        DEBUG_M(LOG_REV_PLAY, 0, 0, "calling __favcd_reuse_gop_job_item due to pp_rdy_job_cnt:%d + dec_idx:%d <= output_idx:%d (curr_job:%d seq:%d)\n", 
            gop_job_item->pp_rdy_job_cnt, gop_job_item->next_dec_bs_idx, gop_job_item->next_output_job_idx, curr_job_item->job_id, curr_job_item->seq_num);

#if GOP_JOB_STOP_HANDLING
        if(gop_job_item->stop_flg == 1 || gop_job_item->err_flg == 1){
            DEBUG_M(LOG_REV_PLAY, 0, 0, "skip reuse gop job item due to stop(%d) or err(%d)\n", gop_job_item->stop_flg, gop_job_item->err_flg);
        }else 
#endif
        {
            if(__favcd_reuse_gop_job_item(curr_job_item) != 0){
#if REALLOC_BUF_FOR_GOP_JOB
                trig_pp_flg = 1;
#endif
            }else{
                DEBUG_M(LOG_REV_PLAY, 0, 0, "calling __favcd_reuse_gop_job_item return 0\n");
            }
        }
    }
    else{
        DEBUG_M(LOG_REV_PLAY, 0, 0, "skip __favcd_reuse_gop_job_item due to pp_rdy_job_cnt:%d + dec_idx:%d > output_idx:%d\n", 
            gop_job_item->pp_rdy_job_cnt, gop_job_item->next_dec_bs_idx, gop_job_item->next_output_job_idx);
    }
    
    if(trig_pp_flg)
        trigger_prepare();

ret:    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list and callback list */


    return 0;
}



static int __favcd_get_next_output_job_idx(struct favcd_data_t *dec_data, struct favcd_gop_job_item_t *gop_job_item)
{
    int i;
    short bs_idx;
    
    //DEBUG_M(LOG_ERROR, 0, 0, "__favcd_get_next_output_job_idx tbl_idx:%d next_output_job_idx:%d\n", 
    //    gop_job_item->output_tbl_idx, gop_job_item->next_output_job_idx);

    DUMP_MSG(D_REV, dec_data->dec_handle, 
        "__favcd_get_next_output_job_idx tbl_idx:%d next_output_job_idx:%d\n", 
        gop_job_item->output_tbl_idx, gop_job_item->next_output_job_idx);


    if(gop_job_item->next_output_job_idx < 0)
        return -1;

    if(gop_job_item->next_output_job_idx >= gop_job_item->output_tbl_idx){
        return -1;
    }

    bs_idx = gop_job_item->output_tbl[gop_job_item->next_output_job_idx];
    for(i = 0; i < gop_job_item->output_tbl_idx; i++){
        if(gop_job_item->gop_jobs[i] && gop_job_item->gop_jobs[i]->bs_idx == bs_idx){
            return i;
        }
    }

    return -1;
}


/*
 * handling job output for gop job items
 * flush_flg: whether to add job with released buffer (buf_idx == -1) to callback list
 */
int __favcd_process_gop_output_list(struct favcd_data_t *dec_data, int flush_flg)
{
    unsigned long flags;
    struct favcd_job_item_t *job_item;
    struct favcd_gop_job_item_t *gop_job_item;
    int next_output_job_idx; /* job index in put_job order */
    
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for fail_list and callback list */

    DUMP_MSG(D_REV, dec_data->dec_handle, "__favcd_process_gop_output_list for gop_job_item ch: %d flush:%d\n", dec_data->minor, flush_flg);

    DEBUG_M(LOG_REV_PLAY, 0, 0, "__favcd_process_gop_output_list for gop_job_item ch: %d flush:%d\n", dec_data->minor, flush_flg);
    
    list_for_each_entry(gop_job_item, &favcd_gop_minor_head[dec_data->minor], gop_job_list) {
        
        DEBUG_M(LOG_REV_PLAY, 0, 0, "__favcd_process_gop_output_list gop_job_item next_output_job_idx:%d flush:%d id:%08X\n", 
            gop_job_item->next_output_job_idx, flush_flg, gop_job_item->gop_id);
//        DUMP_MSG(D_REV, dec_data->dec_handle, "__favcd_process_gop_output_list gop_job_item next_output_job_idx:%d flush:%d id:%08X\n", 
//            gop_job_item->next_output_job_idx, flush_flg, gop_job_item->gop_id);

#if 0
        if(gop_job_item->stop_flg || gop_job_item->err_flg){
            DEBUG_M(LOG_REV_PLAY, 0, 0, "__favcd_process_gop_output_list for gop_job_item ch: %d flush:%d\n", dec_data->minor, flush_flg);
            continue;
        }
#endif
        
        //while(gop_job_item->next_output_job_idx >= 0)
        while(1)
        {
            /* skip output if the output frame is not yet decoded */
#if 0
            if(gop_job_item->got_poc[gop_job_item->next_output_job_idx] == 0){
                break;
            }
#endif
            next_output_job_idx = __favcd_get_next_output_job_idx(dec_data, gop_job_item);
            //DUMP_MSG(D_REV, dec_data->dec_handle, "gop_job_item next_output_job_idx:%d->%d flush:%d\n", 
            //    gop_job_item->next_output_job_idx, next_output_job_idx, flush_flg);
            DEBUG_M(LOG_REV_PLAY, 0, 0, "gop_job_item next_output_job_idx:%d->%d flush:%d\n", 
                gop_job_item->next_output_job_idx, next_output_job_idx, flush_flg);

            if(next_output_job_idx < 0){
                //DEBUG_M(LOG_ERROR, 0, 0, "next_output_job_idx < 0: %d\n", next_output_job_idx);
                //goto skip_output_gop_job;
                break;
            }
            
            if(gop_job_item->dec_done[next_output_job_idx] == 0){
                DEBUG_M(LOG_REV_PLAY, 0, 0, "gop job idx %d has not been decoded (reusing)\n", next_output_job_idx);
                break;
            }
            
            if(gop_job_item->is_outputed[next_output_job_idx]){
                if((gop_job_item->stop_flg == 0) && (gop_job_item->err_flg == 0)){
                    //DEBUG_M(LOG_REV_PLAY, 0, 0, "gop job idx %d has been outputed and gop job is not stopping or error\n", next_output_job_idx);
                    DEBUG_M(LOG_ERROR, 0, 0, "gop job idx %d has been outputed and gop job is not stopping or error\n", next_output_job_idx);
                    FAVCD_DAMNIT("(%d) gop job idx %d has been outputed and gop job is not stopping or error\n", dec_data->minor, next_output_job_idx);
                }
                DEBUG_M(LOG_REV_PLAY, 0, 0, "gop job idx %d has been outputed\n", next_output_job_idx);
                break;
            }

        
            job_item = gop_job_item->gop_jobs[next_output_job_idx];
            if(flush_flg == 0 && job_item->buf_idx == -1){
                DEBUG_M(LOG_REV_PLAY, 0, 0, "buf of gop job idx %d has been released, need to re-decode\n", next_output_job_idx);
                /* FIXME: need to schedule jobs to decode frame of gop_job_item->next_output_job_idx */
                break;
            }
        
            favcd_set_out_property_poc(dec_data, job_item);
            dec_data->out_fr_cnt++;
            
            if(job_item->buf_idx >= 0){
                dec_data->dec_buf[job_item->buf_idx].is_outputed = 1;
            }
            
            __favcd_list_add_tail(&job_item->callback_list, &favcd_callback_head);

            DEBUG_M(LOG_INFO, dec_data->engine, 0, "(%d,%d) gop job %d linked to buf %d added to callback list (flash:%d, next_output_job_idx:%d)\n", 
                job_item->engine, job_item->minor, job_item->job_id, job_item->buf_idx, flush_flg, gop_job_item->next_output_job_idx);

            DUMP_MSG(D_REV, dec_data->dec_handle, "(%d,%d) gop job %d linked to buf %d added to callback list (flash:%d, next_output_job_idx:%d)\n", 
                job_item->engine, job_item->minor, job_item->job_id, job_item->buf_idx, flush_flg, gop_job_item->next_output_job_idx);
            
            gop_job_item->is_outputed[next_output_job_idx] = 1;
            gop_job_item->is_reserved[job_item->bs_idx] = 0; /* clear reserved flag for outputed job */
#if 0
            if(gop_job_item->is_released[gop_job_item->next_output_job_idx] == 1){
                __favcd_mark_buf_released(dec_data, gop_job_item->gop_jobs[i]->buf_idx);
            }
#endif
            gop_job_item->output_cnt++;
            gop_job_item->next_output_job_idx--;
            gop_job_item->seek_flg = 1;
        }

#if GOP_JOB_STOP_HANDLING
        /* put all un-output jobs to fail list */
        if(gop_job_item->stop_flg || gop_job_item->err_flg){
            int i;

            DEBUG_M(LOG_REV_PLAY, 0, 0, "handling stop or err for gop job id: %08X\n", gop_job_item->gop_id);
            
            for(i = 0; i < gop_job_item->gop_size; i++){
                job_item = gop_job_item->gop_jobs[i];
                if(job_item && (gop_job_item->is_outputed[i] == 0)){

                    DEBUG_M(LOG_REV_PLAY, 0, 0, "handling stop or err for gop job id: %08X job: %d seq:%d st:%s buf_idx:%d\n", 
                        gop_job_item->gop_id, job_item->job_id, job_item->seq_num, favcd_job_status_str(job_item->status), job_item->buf_idx);
                    
#if 1
                    if((job_item->status == DRIVER_STATUS_READY) || (job_item->status == DRIVER_STATUS_STANDBY)){
                        DEBUG_M(LOG_REV_PLAY, 0, 0, "skip ready/standby gop job\n");
                        /* skip ready jobs, handle it via work_scheduler */
                        continue;
                    }
#endif

                    if(job_item->status != DRIVER_STATUS_FAIL)
                    {
                        SET_JOB_STATUS(job_item, DRIVER_STATUS_FAIL);
                        favcd_set_out_property_poc(dec_data, job_item);
                        dec_data->out_fr_cnt++;
                        
                        if(job_item->buf_idx >= 0){
                            dec_data->dec_buf[job_item->buf_idx].is_outputed = 1;
#if 0
                            if(gop_job_item->is_released[i] <= 1){
                                gop_job_item->is_released[i] = 3;
                                DEBUG_M(LOG_TRACE, dec_data->engine, dec_data->minor, "mark job released %d in __favcd_process_gop_output_list\n", job_item->job_id);
                                __favcd_mark_buf_released(dec_data, job_item->buf_idx);
                            }
#endif
                        }
                    }
                    __favcd_list_add_tail(&job_item->fail_list, &favcd_fail_minor_head[dec_data->minor]);

                    gop_job_item->is_outputed[i] = 1;
                    if(job_item->bs_idx < gop_job_item->gop_size && job_item->bs_idx >= 0){
                        gop_job_item->is_reserved[job_item->bs_idx] = 0; /* clear reserved flag for outputed job */
                    }
                }
            }
        }
#endif
    }


//skip_output_gop_job:
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* lock for fail_list and callback list */

    return 0;
}



int favcd_check_gop_job(struct favcd_job_item_t *job_item)
{
    struct favcd_data_t *dec_data;

    /* check for reverse play only */
    if(job_item->direct == 0){
        return 0;
    }

    dec_data = favcd_get_dec_data(job_item->minor);

    /* check gop size */
    if(job_item->gop_size == 0){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d gop size == 0", job_item->minor, job_item->job_id);
        return 1;
    }
    
    if(job_item->gop_size > MAX_GOP_SIZE){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d gop size(%d) exceed the limit:%d", 
            job_item->minor, job_item->job_id, job_item->gop_size, MAX_GOP_SIZE);
        return 1;
    }

    //return 1; // test error handling

    return 0;
}


static int add_gop_job_item(struct favcd_gop_job_item_t *gop_job_item, struct favcd_job_item_t *job_item)
{
    if(job_item->gop_job_item == NULL){
        if(gop_job_item->gop_id != job_item->job->in_buf.buf[0].addr_va){
            DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d seq:%d gop_id miss match: %d != %d\n", 
                job_item->minor, job_item->job_id, job_item->seq_num, gop_job_item->gop_id, job_item->job->in_buf.buf[0].addr_va);
            return 1;
        }
        
        if(gop_job_item->gop_size != job_item->gop_size){
            DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d seq:%d gop_size miss match: %d != %d\n", 
                job_item->minor, job_item->job_id, job_item->seq_num, gop_job_item->gop_size, job_item->gop_size);
            return 1;
        }
        
        if(gop_job_item->job_cnt == gop_job_item->gop_size){
            DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d seq:%d in gop job buf overflow\n", 
                job_item->minor, job_item->job_id, job_item->seq_num);
            return 1;
        }
        
        job_item->job_idx_in_gop = gop_job_item->job_cnt;
    }


#if 0
    if(gop_job_item->next_dec_bs_idx > gop_job_item->next_output_job_idx){
        DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "(%d) job %d seq:%d in gop job dec idx overflow: %d > %d\n", 
            job_item->minor, job_item->job_id, job_item->seq_num,
            gop_job_item->next_dec_bs_idx, gop_job_item->next_output_job_idx);
        return 1;
    }
#endif

#if 0//REALLOC_BUF_FOR_GOP_JOB
    /* update the bitstream buffer offset/size */
    set_gop_job_item_bs_buf(gop_job_item, job_item, gop_job_item->next_dec_bs_idx);
    gop_job_item->next_dec_bs_idx++;
#endif
    
    if(job_item->gop_job_item == NULL){
        gop_job_item->gop_jobs[gop_job_item->job_cnt] = job_item;
        gop_job_item->job_cnt++;
        job_item->gop_job_item = gop_job_item;
    }

    return 0;
}


int init_gop_job_item(struct favcd_gop_job_item_t *gop_job_item, struct favcd_job_item_t *job_item)
{
    int i, j;
    unsigned int bs_offset;
    extern int gop_job_seq_num;
    extern unsigned int gop_output_group_size;
    
    gop_job_item->gop_id = job_item->job->in_buf.buf[0].addr_va;
    gop_job_item->seq_num = gop_job_seq_num++;
    gop_job_item->first_job_seq_num = job_item->seq_num;

    gop_job_item->chn_idx = job_item->minor;
#if GOP_JOB_STOP_HANDLING
    gop_job_item->stop_flg = 0;
    gop_job_item->err_flg = 0;
    gop_job_item->ongoing_flg = 0;
#endif
    gop_job_item->gop_size = job_item->gop_size;
    
    if(job_item->gop_job_item != 0){
        gop_job_item->output_group_size = job_item->output_group_size;
    }else if(gop_output_group_size != 0){
        gop_job_item->output_group_size = gop_output_group_size;
    }else{/* gop_output_group_size == 0 */
        gop_job_item->output_group_size = iMax(1, gop_job_item->gop_size / 2); /* assume dec_fps == 2 * disp_fps */
    }
    gop_job_item->output_group_size = iMin(gop_job_item->gop_size, gop_job_item->output_group_size); /* output group size must < gop_size */
        
    gop_job_item->job_cnt = 0;
    gop_job_item->pp_rdy_job_cnt = 0;
    gop_job_item->clean_cnt = 0;
    gop_job_item->output_cnt = 0;
    
    gop_job_item->output_tbl_idx = 0;
    gop_job_item->next_output_job_idx = gop_job_item->gop_size - 1;
    gop_job_item->next_dec_bs_idx = 0;
    gop_job_item->seek_flg = 0;
    memset(gop_job_item->dec_done, 0, sizeof(gop_job_item->dec_done));
    memset(gop_job_item->got_poc, 0, sizeof(gop_job_item->got_poc));
    memset(gop_job_item->gop_jobs, 0, sizeof(gop_job_item->gop_jobs));
    memset(gop_job_item->is_outputed, 0, sizeof(gop_job_item->is_outputed));
    memset(gop_job_item->is_released, 0, sizeof(gop_job_item->is_released));
    memset(gop_job_item->is_reserved, 0, sizeof(gop_job_item->is_reserved));


    memcpy(gop_job_item->bs_len, (void *)(job_item->job->in_buf.buf[0].addr_va + job_item->bs_size + 4), gop_job_item->gop_size * 4);
    gop_job_item->bs_offset[0] = 0;
    bs_offset = 0;
    
    /* decide which job will be kept (not released until it is outputed) */
    for(i = 0, j = 0; i < gop_job_item->gop_size; i++, j++){
        gop_job_item->bs_offset[i] = bs_offset;
        bs_offset += gop_job_item->bs_len[i];
    }

    if(job_item->bs_size != bs_offset){
        DEBUG_M(LOG_ERROR, 0, 0, "(%d)bistream size inconsist: size at end of buf(%d) != input property size(%d) (job id:%d seq:%d)\n", 
            bs_offset, job_item->bs_size,
            gop_job_item->chn_idx, job_item->job_id, job_item->seq_num);

        for(i = 0; i < gop_job_item->gop_size; i++){
            DEBUG_M(LOG_ERROR, 0, 0, "bs_len[%d]:%d\n", i, gop_job_item->bs_len[i]);
        }

        return 1;
    }

    mark_output_group_as_reserved(gop_job_item);

    __favcd_init_list_head(&gop_job_item->gop_job_list);

    return 0;
}


int prepare_gop_job_item(struct favcd_job_item_t *job_item)
{
    struct favcd_gop_job_item_t *gop_job_item;
    int new_gop_flg;
    unsigned long flags;

    
    DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, "(%d) prepare_gop_job_item for job %d seq:%d direct:%d\n", 
        job_item->minor, job_item->job_id, job_item->seq_num, job_item->direct);

    /* handle reverse play only */
    if(job_item->direct == 0){
        job_item->gop_job_item = NULL;
        return 0;
    }

    if(favcd_check_gop_job(job_item)){
        return 1;
    }

    /* check if the job is belong to a new GOP */
    if(list_empty(&favcd_gop_minor_head[job_item->minor])){
        new_gop_flg = IS_NEW_GOP;
    }else{
        gop_job_item = (struct favcd_gop_job_item_t *)
            list_entry(favcd_gop_minor_head[job_item->minor].prev, struct favcd_gop_job_item_t, gop_job_list);

        /* FIXME: need a new method to detect the new GOP */
        if(gop_job_item->gop_id == job_item->job->in_buf.buf[0].addr_va){
            new_gop_flg = NOT_NEW_GOP;
            
            if(gop_job_item->job_cnt == gop_job_item->gop_size){
                DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, 
                    "(%d) job %d seq:%d in gop job buf overflow. treat it as new gop job item\n", 
                    job_item->minor, job_item->job_id, job_item->seq_num);
                new_gop_flg = IS_NEW_GOP;
            }
        }else{
            new_gop_flg = IS_NEW_GOP;
            if(gop_job_item->job_cnt != gop_job_item->gop_size){
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, 
                    "(%d) job %d seq:%d job of a new GOP is comming when the job number of previous gop(%d) != gop_size(%d) (gop id:%08X seq:%d job_seq:%d)\n", 
                    job_item->minor, job_item->job_id, job_item->seq_num, 
                    gop_job_item->job_cnt, gop_job_item->gop_size,
                    gop_job_item->gop_id, gop_job_item->seq_num, gop_job_item->first_job_seq_num);
                
#if GOP_JOB_STOP_HANDLING
                gop_job_item->err_flg = 1;
                DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "(%d) set err_flag (gop id:%08X seq:%d job_seq:%d clean:%d job_cnt:%d)\n", 
                    job_item->minor, gop_job_item->gop_id, gop_job_item->seq_num, gop_job_item->first_job_seq_num,
                    gop_job_item->clean_cnt, gop_job_item->job_cnt);
                if(gop_job_item->clean_cnt == gop_job_item->job_cnt){
                    DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "(%d) freeing gop_job_item %d at err: clean:%d job_cnt:%d\n", 
                        job_item->minor, job_item->job_id, gop_job_item->clean_cnt, gop_job_item->job_cnt);
                    __favcd_list_del(&gop_job_item->gop_job_list);
                    fkfree(gop_job_item);
                }
#endif
            }
        }
    }

    if(new_gop_flg == IS_NEW_GOP){
        gop_job_item = fkmalloc(sizeof(*gop_job_item));
        if(gop_job_item == NULL){
            DEBUG_J(LOG_ERROR, job_item->engine, job_item->minor, "decoder: (%d) job %d failed to allocate memory for gop_job_item\n", job_item->minor, job_item->job_id);
            return 1;
        }
        if(init_gop_job_item(gop_job_item, job_item)){
            goto err_ret;
        }
        
        if(add_gop_job_item(gop_job_item, job_item)){
            goto err_ret;
        }
        __favcd_list_add_tail(&gop_job_item->gop_job_list, &favcd_gop_minor_head[job_item->minor]);
        
    }else{
    
        if(add_gop_job_item(gop_job_item, job_item)){
            goto err_ret;
        }

#if 0//GOP_JOB_STOP_HANDLING
        if(gop_job_item->stop_flg || gop_job_item->err_flg){
            goto err_ret;
        }
#endif
    }

    //gop_job_item = job_item->gop_job_item;
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for pp_rdy_job_cnt */
    gop_job_item->pp_rdy_job_cnt++;
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for pp_rdy_job_cnt */

    DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, "(%d) prepare_gop_job_item for job %d seq:%d direct:%d new_gop_flg:%d gop_id:%08X done\n", 
        job_item->minor, job_item->job_id, job_item->seq_num, job_item->direct, new_gop_flg, gop_job_item->gop_id);
    
    /* normal return */
    return 0;

err_ret:
    job_item->gop_job_item = NULL;
    if(new_gop_flg == IS_NEW_GOP){
        fkfree(gop_job_item);
    }
    
#if GOP_JOB_STOP_HANDLING
    DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, "(%d) prepare_gop_job_item for job %d seq:%d direct:%d new_gop_flg:%d gop_id:%08X err (stop:%d err:%d)\n", 
        job_item->minor, job_item->job_id, job_item->seq_num, job_item->direct, new_gop_flg, gop_job_item->gop_id, 
        gop_job_item->stop_flg, gop_job_item->err_flg);
#else
    DEBUG_M(LOG_REV_PLAY, job_item->engine, job_item->minor, "(%d) prepare_gop_job_item for job %d seq:%d direct:%d new_gop_flg:%d gop_id:%08X err\n", 
        job_item->minor, job_item->job_id, job_item->seq_num, job_item->direct, new_gop_flg, gop_job_item->gop_id);
#endif

    return 1;
}

#endif // REV_PLAY /* skip the whole file if REV_PLAY = 0 */

