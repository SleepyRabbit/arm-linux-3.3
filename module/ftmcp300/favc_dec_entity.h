
#ifndef _FAVC_DEC_ENTITY_H_
#define _FAVC_DEC_ENTITY_H_

#include "dec_driver/H264V_dec.h" // for FAVC_DEC_FRAME_IOCTL, FAVC_DEC_BUFFER


enum buffer_info_flag_t{
    BUF_INFO_USED = 1,
};

struct buffer_info_t
{
    unsigned int addr_pa;
    unsigned int addr_va;
    unsigned int size;
    unsigned int flags;
};


typedef struct {
    unsigned char is_used;      /* whether it is allocated(pushed into dec_buf) */
    unsigned char is_released;  /* whether it is mark as released (during processing release list) */
    unsigned char is_outputed;  /* whether it is mark as outputed (during processing output list) */
    unsigned char is_started;   /* whether it is mark as started (during processing ready job list) */
    unsigned char alloc_from_vg; /* 1: allocated from VG. 0: allocated from frammap */
    unsigned int  start_va;
    unsigned int  start_pa;
    unsigned int  mbinfo_va;
    unsigned int  mbinfo_pa;
    unsigned int  scale_va;
    unsigned int  scale_pa;
    struct favcd_job_item_t *job_item;
} BufferAddr;



/* values for need_init_flg */
enum init_cond {
    INIT_DONE = 0,
    INIT_NORMAL = 1,
    INIT_DUE_TO_ERR = 2,
};

struct favcd_data_t {
    int engine; /* indicate which engine this channel is bound to. -1 means no engine is bound */
    int minor;

    DecoderParams           *dec_handle;      // pointer to private data of lowwer level decoder driver
    FAVC_DEC_FRAME_IOCTL     dec_frame;       // for storing info of each decoding frame
    FAVC_DEC_BUFFER          recon_buf;       // for reconstruct buffer
    BufferAddr               dec_buf[MAX_DEC_BUFFER_NUM];         // buffers of a channel
    unsigned int             dec_buf_dim;     // width/height of buffers in dec_buf
    int                      used_buffer_num; // number of used buffers in dec_buf array
    int                      ready_job_cnt;   // number of jobs of this channel that is prepared (NOTE: may or may not in ready list)
    unsigned int             last_callback_time; // jiffies at last calling callback 

    /* input property */
    unsigned int  dst_xy;
    unsigned int  dst_bg_dim;
    unsigned short yuv_width_thrd;
    unsigned char  sub_yuv_ratio;
    unsigned char  sub_yuv_en;      // sub YUV enable

    /* input/output property */
    unsigned int  dst_fmt;

    /* output property */
    unsigned int  dst_dim;
    unsigned short crop_top;
    unsigned short crop_bottom;
    unsigned short crop_left;
    unsigned short crop_right;
    unsigned int  out_fr_cnt;
    unsigned int  unit_in_tick;
    unsigned int  time_scale;
    unsigned char src_interlace;

    unsigned char stopping_flg;    // to indicate VG has stopped this channel
    unsigned char handling_stop_flg; // to indicate callback scheduler is handling stop (for capturing stopping flag at start of callback_scheduler())

    enum init_cond need_init_flg;  // to indicate the decoder needs to be re-initialized

    struct favcd_job_item_t *curr_job;

#if RECORDING_BS
    /* for recording bitstream */
    unsigned int   err_cnt;
    unsigned char  rec_en;
#endif
};


int test_engine_idle(int dev);
int test_and_set_engine_busy(int dev);
void set_engine_idle(int engine);
#endif // _FAVC_DEC_ENTITY_H_

