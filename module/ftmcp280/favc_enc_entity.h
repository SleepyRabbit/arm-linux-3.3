
#ifndef _FAVC_ENC_ENTITY_H_
#define _FAVC_ENC_ENTITY_H_

#include "log.h"
#include "h264e_ioctl.h"
#include "favce_param.h"
#include "favce_buffer.h"
#include "enc_driver/H264V_enc.h"

// 1: reinit, 2: reinit rate control, 3: reinit GOP, 4: reinit roi xy
#define H264E_REINIT        0x01
#define H264E_RC_REINIT     0x02
#define H264E_GOP_REINIT    0x04
#define H264E_ROI_REINIT    0x08
#define H264E_VUI_REINIT    0x10
#define H264E_FORCE_INTRA   0x20
#define H264E_WK_UPDATE     0x40

#define PROP_MASK_SRC_FMT       0x0001
#define PROP_MASK_SRC_XY        0x0002
#define PROP_MASK_SRC_BG_DIM    0x0004
#define PROP_MASK_SRC_DIM       0x0008
#define PROP_MASK_IDR_INTERVAL  0x0010
#define PROP_MASK_B_FRAME       0x0020
#define PROP_MASK_RC_MODE       0x0040
#define PROP_MASK_NECESSARY     0x007F

#define MAX_RECON_BUF_NUM   4


struct buffered_b_frame_t
{
    void *job_item;
    SourceBuffer frame_info;
};

struct favce_coding_format_t {
    unsigned char field_coding;
    unsigned char entropy_coding;
    unsigned char grayscale_coding;
    int slice_mb_row;
    unsigned char didn_mode;
    /*          [3]                 [2]                   [1]                     [0]
     *  temporal de-noise    spatial de-noise    temporal de-interlace    spatial de-interlace
     *  if u8BFrameNumber > 0, temporal de-noise and temporal de-interlace must be disable */
};

struct favce_data_t {
    int engine;//indicate which engine
    int chn;

    FAVC_ENC_IOCTL_FRAME tFrame;
    
    unsigned int src_fmt;
    unsigned int src_xy;
    unsigned int src_bg_dim;
    unsigned int src_dim;

    // rate control
    int rc_mode;
    int init_quant;
    int max_quant;
    int min_quant;
    int bitrate;
    int max_bitrate;

    unsigned int src_frame_rate;
    unsigned int fps_ratio;

    // gop
    int idr_interval;
    int b_frm_num;
#ifdef ENABLE_FAST_FORWARD
    int fast_forward;
#endif
    int slice_type;
    int field_coding;
    int slice_mb_row;
    int gray_scale;
    int watermark_flag;
    int watermark_init_interval;
    unsigned int watermark_pattern;
    int frame_width;
    int frame_height;
    int quant;
    int first_frame;
    int roi_qp_enable;
    unsigned int roi_qp_region[ROI_REGION_NUM];

    unsigned int profile;
    unsigned int qp_offset;
    signed char ip_offset;
#ifdef USE_CONFIG_FILE
    int config_idx;
#endif

    int motion_flag;
    //int mb_info_offset;
    int mb_info_size;
    int bs_offset;
    int bs_size;

    int buf_width;
    int buf_height;

    int force_intra;

    void *handler;
#ifdef RC_ENABLE
    void *rc_handler;
#endif
    int stop_job;
    int updated;    // 1: reinit, 2: reinit rate control, 3: reinit GOP, 4: reinit roi xy
    unsigned int out_addr_va;
    unsigned int out_addr_pa;
    int out_size;
    unsigned int in_addr_va;
    unsigned int in_addr_pa;
    int in_size;
    SourceBuffer cur_src_buf;
    struct buffer_info_t L1ColMVInfo_buffer;
    //struct buffer_info_t didn_buffer[2];
    //struct buffer_info_t mvinfo_buffer;
    int didn_ref_idx;

    // buffered B frame
    struct buffered_b_frame_t enc_BQueue[3];
    int queue_num;
    int queue_idx;
    int trigger_b;

    //SourceBuffer cur_frame_info;
    ReconBuffer ref_info[MAX_RECON_BUF_NUM];
    int cur_rec_buf_idx;
    //int release_idc;
    FAVC_ROI_QP_REGION region[8];

    unsigned int vui_format_param;
    unsigned int vui_sar_param;

    int src_interlace;
    int di_result;
    int input_didn;
    int didn_mode;
    int real_didn_mode;

    void *cur_job;
    void *prev_job;
#ifdef REF_POOL_MANAGEMENT
    int res_pool_type;
    int re_reg_cnt;
    int over_spec_cnt;
#endif
#ifdef OVERFLOW_REENCODE
    int cur_qp;
    int force_qp;
#endif
#ifdef ENABLE_CHECKSUM
    unsigned int checksum_type;
    //unsigned int checksum_result;
#endif
#ifdef ENABLE_FAST_FORWARD
    int ref_frame;
#endif
#ifdef DYNAMIC_ENGINE
    int channel_cost;
#endif
#ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
    int small_buf_idx;  // 0,1: rec buffer, -1: not use swap buffer
#endif
#ifdef HANDLE_2FRAME
    unsigned int src_buf_offset;
    int frame_buf_idx;
    unsigned int src_bg_frame_size;
#endif
#ifdef ENABLE_SWAP_WH
    unsigned int swap_wh;
#endif
#ifdef DYNAMIC_GOP_ENABLE
    unsigned int static_event_cnt;
    unsigned int motion_state;  // 0: motion, 1: motion -> static, 2: static, 3: static -> motion
#endif
};

struct favce_pre_data_t{
    int force_intra;
    //int src_xy;
    unsigned int updated;
};

#ifdef USE_CONFIG_FILE
struct favce_param_data_t
{
    FAVC_ENC_PARAM enc_param;
    FAVC_ENC_DIDN_PARAM didn_param;
    int ip_offset;
    int valid;
};
#endif

#endif
