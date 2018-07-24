
#ifndef _FAVC_DEC_REV_H_
#define _FAVC_DEC_REV_H_

//#include "dec_driver/define.h"
#include "favc_dec_vg.h"

/* for REV_PLAY */
#define MAX_BS_SIZE       (4*1024*1024)    /* max input bitstream data size in VG driver. for checking bitstream in rev_play mode */
#define MAX_GOP_SIZE      64

struct favcd_gop_job_item_t {
    unsigned int              gop_id;   /* use bitstream buffer address as GOP ID */
    unsigned int              seq_num;  /* sequence number */
    unsigned int              first_job_seq_num;  /* sequence number of the first job of the GOP. for debug only */
    unsigned int              gop_size; 
    struct favcd_job_item_t  *gop_jobs[MAX_GOP_SIZE];
    unsigned int              bs_len[MAX_GOP_SIZE];
    unsigned int              bs_offset[MAX_GOP_SIZE];

    /* flags in put_job order */
    /* FIXME: use bits of one byte for the following flags */
    unsigned char             got_poc[MAX_GOP_SIZE];     /* whether the poc of the job is got via output list (in put_job order) */
    unsigned char             is_outputed[MAX_GOP_SIZE]; /* (in put_job order) */
    unsigned char             is_released[MAX_GOP_SIZE]; /* whether the job is released via release list. bit0: (in put_job order) */
    unsigned char             dec_done[MAX_GOP_SIZE];    /* (in put_job order) */

    /* flags in output order */
    /* FIXME: use a output_group_cnt instead */
    unsigned char             is_reserved[MAX_GOP_SIZE]; /* whether to reserve job and do not mark it as released before output (in output order) */

    unsigned char             output_tbl[MAX_GOP_SIZE];  /* for looking up the bs_idx (array indexed by output order) */
    unsigned char             output_tbl_idx;

    /* flags */    
    /* FIXME: merge the following flags */
#if GOP_JOB_STOP_HANDLING
    unsigned char             stop_flg;
    unsigned char             err_flg;
    unsigned char             ongoing_flg;
#endif
    unsigned char             seek_flg;                  /* whether seek back should be performed at next decoding */

    unsigned char             output_group_size;         /* the number of jobs to be outputed in one forward decoding pass */
    unsigned char             clean_cnt;
    unsigned char             output_cnt;
    unsigned char             chn_idx;
    unsigned char             job_cnt;
    unsigned char             pp_rdy_job_cnt;            /* for deciding whether to put decoded job back to prepare list
                                                            NOTE: this value may be under estimated */
    struct list_head          gop_job_list;
    int                       next_output_job_idx;
    int                       next_dec_bs_idx;           /* next dec bs index */
};


#if DBG_REV_PLAY
#define LOG_REV_PLAY LOG_ERROR
#else
#define LOG_REV_PLAY LOG_INFO
#endif


int __favcd_process_gop_release_list(struct favcd_data_t *dec_data);
void set_gop_job_item_bs_buf(struct favcd_gop_job_item_t *gop_job_item, struct favcd_job_item_t *job_item, int bs_idx);
int __favcd_reuse_gop_job_item(struct favcd_job_item_t *curr_job_item);
int __favcd_gop_job_post_process(struct favcd_job_item_t *curr_job_item);
int __favcd_process_gop_output_list(struct favcd_data_t *dec_data, int flush_flg);
int favcd_check_gop_job(struct favcd_job_item_t *job_item);
int prepare_gop_job_item(struct favcd_job_item_t *job_item);





#endif // _FAVC_DEC_DBG_H_

