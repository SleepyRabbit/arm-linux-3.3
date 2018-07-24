/**
 * @file em_api.h (wrapper)
 *  EM API for video entity (wrapper V1)
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/08/09 02:55:26 $
 *
 * ChangeLog:
 *  $Log: em_api.h,v $
 *  Revision 1.2  2016/08/09 02:55:26  ivan
 *  update for v1 wrapper
 *
 */
#ifndef _EM_API_H_
#define _EM_API_H_
#include "common.h"
#include "job.h"

#define ALLOC_AUTO  -1  //auto for em_alloc_fd
#define EM_MAX_PCH_COUNT (64 + 1)   //max 64 display division

struct process_t {
    struct list_head    list;
    int high_priority;
    int (*func)(int fd, int ver, void *in_prop, void *out_prop,
        unsigned int in_ddr_id, unsigned int *in_pa, unsigned int *in_size,
        unsigned int out_ddr_id, unsigned int *out_pa, unsigned int *out_size,
        unsigned int root_time, void *priv_param);
    void *priv_param;
};

struct video_data_cap_t {
    unsigned int pool_dim;
    unsigned int pch;
    unsigned int dst_fmt;
    unsigned int dst_bg_dim;
    unsigned int dst_xy;
    unsigned int dst_dim;
    unsigned int buf_offset;
    unsigned int buf_size;
    unsigned int src_xy;
    unsigned int src_dim;
    unsigned int src_bg_dim;
    unsigned int target_frame_rate;
    unsigned int fps_ratio;
    unsigned int cap_feature;
    unsigned int flip_rotate;
};

struct video_data_display0_t {
    unsigned int src_bg_dim;
    unsigned int src_dim;
    unsigned int src_xy;
    unsigned int disp1_dim;
    unsigned int disp1_xy;
    unsigned int src_fmt;
    unsigned int src_frame_rate;
    unsigned int feature; //feature: [4:0]latency drop enable,  0(disable)  1(enable)
};

struct vpe_pch_param {
    unsigned int pch;
    unsigned int src_fmt;
    unsigned int src_xy;
    unsigned int src_dim;
    unsigned int src_bg_dim;
    
    unsigned int dst_fmt;
    unsigned int dst_xy;
    unsigned int dst_dim;
    unsigned int dst_bg_dim;
    unsigned int dup_dst_bg_dim;

    unsigned int vpe_feature;
    unsigned int didn_mode;
    unsigned int image_quality;   //  [7:0] SPNR 0~100, [15:8] TMNR 0~64, [23:16] Sharpness 0~100
    unsigned int flip_rotate;
    unsigned int buf_offset;
    unsigned int buf_size;
    unsigned int auto_aspect_ratio;
    /* PIP */
    unsigned int src_xy2;
    unsigned int src_dim2;
    unsigned int dst_xy2;
    unsigned int dst_dim2;
    unsigned int hole_xy;
    unsigned int hole_dim;
    /* face detection, cascade scaling*/
    unsigned int cas_scl_ratio;
    unsigned int cas_scl_offset;
    /* Non-property data */
    unsigned int crop_xy;
    unsigned int crop_dim;
    unsigned int crop_bg_dim;
    unsigned int crop_xy2;
    unsigned int crop_dim2;
    unsigned int crop_bg_dim2;

};   

struct video_data_vpe_t {
    unsigned int pool_dim;
    unsigned int pch;
    unsigned int max_pch_index;
    struct vpe_pch_param pch_param[EM_MAX_PCH_COUNT]; //search from 0 ~ max_pch_index
};


struct video_data_decode_t {
    unsigned int pch;
    unsigned int dst_xy;
    unsigned int dst_bg_dim;
    unsigned int bitstream_size;
    unsigned int dst_fmt;
    unsigned int yuv_width_threshold;
    unsigned int sub_yuv_ratio;
};

#define PURPOSE_VIDEO_BS_IN 0
#define PURPOSE_AUDIO_BS_IN 1
#define PURPOSE_RAW_DATA_IN 2


struct video_data_datain_t {
    unsigned int pch;
    unsigned int slice_type;
    unsigned int width;
    unsigned int height;
    unsigned int fmt;
};

#define MAX_ROI_QP_NUMBER   8
struct video_data_h264_e_t {
    unsigned int src_fmt;
    unsigned int src_bg_dim;
    unsigned int src_xy;
    unsigned int src_dim;
    unsigned int enc_feature;
    unsigned int idr_interval;
    unsigned int src_frame_rate;
    unsigned int fps_ratio;
    unsigned int rc_mode;
    unsigned int quant_value;
    unsigned int bitrate;
    unsigned int max_bitrate;
    unsigned int fast_forward;
    unsigned int watermark_pattern;
    unsigned int vui_format_param;
    unsigned int vui_sar_param;
    unsigned int didn_mode;
    unsigned int image_quality;   //  [7:0] SPNR 0~100, [15:8] TMNR 0~64, [23:16] Sharpness 0~100
    unsigned int slice_type;
    unsigned int profile;
    unsigned int checksum;
    unsigned int flip_rotate;
    unsigned int qp_offset;
    unsigned int roi_qp_region0;
    unsigned int roi_qp_region1;
    unsigned int roi_qp_region2;
    unsigned int roi_qp_region3;
    unsigned int roi_qp_region4;
    unsigned int roi_qp_region5;
    unsigned int roi_qp_region6;
    unsigned int roi_qp_region7;
    unsigned int roi_root_time;
};

struct video_data_mpeg4_e_t {
    unsigned int src_fmt;
    unsigned int src_bg_dim;
    unsigned int src_xy;
    unsigned int src_dim;
    unsigned int enc_feature;
    unsigned int idr_interval;
    unsigned int src_frame_rate;
    unsigned int fps_ratio;
    unsigned int rc_mode;
    unsigned int quant_value;
    unsigned int bitrate;
    unsigned int max_bitrate;
    unsigned int slice_type;
};

struct video_data_jpeg_e_t {
    unsigned int src_fmt;
    unsigned int src_bg_dim;
    unsigned int src_xy;
    unsigned int src_dim;
    unsigned int enc_feature;
    unsigned int restart_interval;
    unsigned int image_quality;
    unsigned int src_frame_rate;
    unsigned int fps_ratio;
    unsigned int rc_mode;
    unsigned int bitrate;
    unsigned int max_bitrate;
};

struct aenc_pch_param {
    unsigned int pch;
    unsigned int encode_type;
    unsigned int bitrate;
    unsigned int buf_offset;
    unsigned int buf_length;
};

struct video_data_aenc_t {
    unsigned int pch;
    unsigned int sample_rate;
    unsigned int sample_bits;
    unsigned int tracks;
    unsigned int ch_bmp;
    unsigned int ch_bmp2;
    unsigned int max_pch_index;
    unsigned int frame_samples;
    struct aenc_pch_param pch_param[EM_MAX_PCH_COUNT]; //search from 0 ~ max_pch_index
};

struct video_data_adec_t {
    unsigned int sample_rate;
    unsigned int sample_bits;
    unsigned int tracks;
    unsigned int encode_type;
    unsigned int ch_bmp;
    unsigned int ch_bmp2;
    unsigned int bitstream_size;
    unsigned int block_size;
    unsigned int resample_ratio;
};

struct video_data_dataout_t {
    unsigned int identifier;
    unsigned int slice_type;
    unsigned int bs_size;
    unsigned int frame_rate;
};

struct video_data_osg_t {
    unsigned int src_bg_dim;
    unsigned int src_fmt;
    unsigned int osg_param_1;
};

struct video_data_ivs_t {
    unsigned int pch;
    unsigned int src_bg_dim;
    unsigned int src_fmt;
    unsigned int cas_scl_offset;
    unsigned int fdt_global_1;
    unsigned int fdt_global_2;
};

struct video_data_dnr_t {
    unsigned int src_bg_dim;
    unsigned int src_fmt;
};

struct video_user_param_t {
    char            name[16];
    unsigned short  id;
    unsigned int    value;
};

struct video_user_work_t {
    int type;
    int priority;
    int need_stop;
    unsigned int fd;
    unsigned int chip;
    unsigned int engine; //for specific engine
    unsigned int minor;  //for specific minor
    unsigned int in_ddr_id;
    unsigned int in_addr_pa;
    unsigned int in_addr_va;
    unsigned int in_size;
    unsigned int out_ddr_id;
    unsigned int out_addr_pa;
    unsigned int out_addr_va;
    unsigned int out_size;
    struct video_user_param_t in_param[MAX_PROCESS_PROPERTIES];
    struct video_user_param_t out_param[MAX_PROCESS_PROPERTIES];
    struct video_entity_job_t *first_job;
    struct video_user_work_t *next;
};

#define FORM_INDEX_AND_DIR_VALUE(index, dir)    (((index)<<4) | (dir))
#define GET_INDEX_FROM_VALUE(value)             (((value) & 0xF0) >> 4)
#define GET_DIR_FROM_VALUE(value)               ((value) & 0xF)
struct buffer_operations {
    int (*buffer_reserve_func)(void *job, int index_and_dir);
    int (*buffer_free_func)(void *binfo, int index);
    int (*buffer_alloc_func)(void *job, int dir, void *binfo);
    int (*buffer_realloc_func)(void *p_video_bufinfo, int job_id);
    int (*buffer_complete_func)(void *p_video_bufinfo, int used_size);
    void *(*buffer_va_to_pa)(void *va);
    int (*entity_clock_tick)(void *clk_data, unsigned int last_jiffies, void *last_job);
};

struct vpd_operations {
    int (*entity_notify_handler)(enum notify_type type, int fd);
};


enum em_frame_type_tag {
    EM_FT_I_FRAME = 0,
    EM_FT_P_FRAME = 1,
};

unsigned int em_query_fd(int type, int chip, int engine, int minor);
unsigned int em_alloc_fd(int type, int chip, int engine, int minor);
int em_free_fd(unsigned int fd);
int em_set_param(int fd, int ver, void *);
int em_destroy_param(int fd, int ver);
int em_set_property(int fd, int ver, void *);
int em_destroy_property(int fd, int ver);
unsigned int em_query_propertyid(int type, void *property_str);
int em_query_propertystr(int type, int id, void *property_str);
int em_sync_tuning_setup_latency_range(int from_fd, int to_fd);
int em_sync_tuning_target_register(int fd, int target_fd, int threshold);
int em_get_bufinfo(int fd, int ver, struct video_bufinfo_t *in_info,
                   struct video_bufinfo_t *out_info);
int em_query_entity_name(int fd, char *name, int n_size);
unsigned int em_query_entity_fd(char *name);
int em_register_buffer_ops(struct buffer_operations *ops);
int em_register_vpd_ops(struct vpd_operations *ops);
unsigned int em_get_property_value(struct video_properties_t *prop, int id, int pch);
unsigned int em_get_property_by_fd(char *msg, int max_msg_len, unsigned int fd);
int em_query_last_entity_fd(int fd);
int em_get_buf_info_directly(struct video_bufinfo_t *binfo, struct video_buf_info_items_t *bitem);
int em_check_preprocess(int fd);
int em_check_postprocess(int fd);
int em_queue_preprocess(int fd, int high_priority,
                        int (*func)(int fd, int ver, void *in_prop, void *out_prop,
                        unsigned int in_ddr_id, unsigned int *in_pa, unsigned int *in_size,
                        unsigned int out_ddr_id, unsigned int *out_pa, unsigned int *out_size,
                        unsigned int root_time, void *priv_param), void *priv_param);
int em_queue_postprocess(int fd, int high_priority,
                        int (*func)(int fd, int ver, void *in_prop, void *out_prop,
                        unsigned int in_ddr_id, unsigned int *in_pa, unsigned int *in_size,
                        unsigned int out_ddr_id, unsigned int *out_pa, unsigned int *out_size,
                        unsigned int root_time, void *priv_param), void *priv_param);
int em_dequeue_preprocess(int fd, int (*func)(int fd, int ver, void *in_prop, void *out_prop,
                        unsigned int in_ddr_id, unsigned int *in_pa, unsigned int *in_size,
                        unsigned int out_ddr_id, unsigned int *out_pa, unsigned int *out_size,
                        unsigned int root_time, void *priv_param));
int em_dequeue_postprocess(int fd, int (*func)(int fd, int ver, void *in_prop, void *out_prop,
                        unsigned int in_ddr_id, unsigned int *in_pa, unsigned int *in_size,
                        unsigned int out_ddr_id, unsigned int *out_pa, unsigned int *out_size,
                        unsigned int root_time, void *priv_param));
int em_user_putjob(struct video_user_work_t *user_work);
int em_fd_exist(int fd);
int em_putjob(struct video_entity_job_t *job);
int em_stopjob(int fd);
int em_register_clock(int fd, void *);
int em_complete_buffer(struct video_bufinfo_t *binfo, int used_size);
int em_realloc_buffer(struct video_bufinfo_t *binfo, int job_id);
int em_get_entity_buffer_info(struct video_entity_job_t *job, int dir, int fd, int ver,
                              struct entity_buffer_info_t *info);
unsigned int em_get_buffer_private(unsigned int addr);
void em_set_buffer_private(unsigned int addr, unsigned int value);
int em_get_p_num_by_fd(unsigned int fd);


#define SIG_KEYFRAME        0x36363838     //send signal to request keyframe

struct em_adjust_display_t{
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
};

struct em_cap_signal_t{
    int entity_fd;
    unsigned int dim;
    unsigned int is_progressive;
};

int em_signal(int fd, int sig, void *param);
#endif
