#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "portab.h"
#include "../h264e_ioctl.h"

#define MAX_NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE 256
#define MAX_VALUE_OF_CPB_CNT 32
#define MAX_SLICE_GROUPS 8
#define MAX_SPS_NUM 32
#define MAX_PPS_NUM 256
#define MAX_SLICE_GROUP_NUM 8
#define EXTENDED_SAR 255
#define DISBALE_DELTA_SCALE 0xFF

typedef struct hrd_parameters_t
{
	uint8_t  cpb_cnt_minus1;                                // ue(v) (0~31)
	uint8_t  bit_rate_scale;                                // u(4)
	uint8_t  cpb_size_scale;                                // u(4)

	uint32_t bit_rate_value_minus1[MAX_VALUE_OF_CPB_CNT];   // ue(v)
	uint32_t cpb_size_value_minus1[MAX_VALUE_OF_CPB_CNT];   // ue(v)
	byte     cbr_flag[MAX_VALUE_OF_CPB_CNT];                // u(1)

	uint8_t  initial_cpb_removal_delay_length_minus1;       // u(5)
	uint8_t  cpb_removal_delay_length_minus1;               // u(5)
	uint8_t  dpb_output_delay_length_minus1;                // u(5)
	uint8_t  time_offset_length;                            // u(5)
} HRD_Parameters;

typedef struct vui_parameter_set_t
{
	byte     aspect_ratio_info_present_flag;            // u(1)
	uint32_t aspect_ratio_idc;                          // u(8)
	uint32_t sar_width;                                 // u(16)
	uint32_t sar_height;                                // u(16)

	byte     overscan_info_present_flag;                // u(1)
	byte     overscan_appropriate_flag;                 // u(1)
	byte     video_signal_type_present_flag;            // u(1)
	uint8_t  video_format;                              // u(3)
	byte     video_full_range_flag;                     // u(1)
	byte     colour_description_present_flag;           // u(1)
	uint32_t colour_primaries;                          // u(8)
	uint32_t transfer_characteristics;                  // u(8)
	uint32_t matrix_coefficients;                       // u(8)
	byte     chroma_loc_info_present_flag;              // u(1)
	uint8_t  chroma_sample_loc_type_top_field;          // ue(v) (0~5)
	uint8_t  chroma_sample_loc_type_bottom_field;       // ue(v) (0~5)

	byte     timing_info_present_flag;                  // u(1)
	uint32_t num_units_in_tick;                         // u(32)
	uint32_t time_scale;                                // u(32)
	byte     fixed_frame_rate_flag;                     // u(1)

	byte     nal_hrd_parameters_present_flag;           // u(1)
	HRD_Parameters nal_hrd_parameters;
	byte     vcl_hrd_parameters_present_flag;           // u(1)
	HRD_Parameters vcl_hrd_parameters;
	
	byte     low_delay_hrd_flag;                        // u(1)
	byte     pic_struct_present_flag;                   // u(1)
	byte     bitstream_restriction_flag;                // u(1)
	byte     motion_vectors_over_pic_boundaries_flag;   // u(1)

	uint8_t  max_bytes_per_pic_denom;                   // ue(v) (0~16)
	uint8_t  max_bits_per_mb_denom;                     // ue(v) (0~16)
	uint8_t  log2_max_mv_length_horizontal;             // ue(v)
	uint8_t  log2_max_mv_length_vertical;               // ue(v)
	uint8_t  num_reorder_frames;                        // ue(v)
	uint8_t  max_dec_frame_buffering;                   // ue(v)
} VUI_Parameters;

typedef struct seq_parameter_set_rbsp
{
	uint32_t profile_idc;                           // u(8)
	uint32_t level_idc;                             // u(8)
	uint8_t  seq_parameter_set_id;                  // ue(v)
	uint8_t  chroma_format_idc;                     // ue(v)
	byte     seq_scaling_matrix_present_flag;       // u(1)
	byte     seq_scaling_list_present_flag[8];      // u(1) 8  for high profile (6:4x4 + 2:8x8)
	byte     UseDefaultScalingMatrix4x4Flag[6];
	byte     UseDefaultScalingMatrix8x8Flag[2];
	int      delta_scale4x4[6][16];
	int      delta_scale8x8[2][64];
	int      ScalingList4x4[6][16];                 // se(v) (-128~127)
	int      ScalingList8x8[2][64];                 // se(v) (-128~127)

	uint8_t  log2_max_frame_num_minus4;             // ue(v)
	uint8_t  pic_order_cnt_type;                    // ue(v) (0~2)
	uint8_t  log2_max_pic_order_cnt_lsb_minus4;     // ue(v) (0~12)
	
	uint8_t  num_ref_frames;                        // ue(v) (0~maxDpbSize(16))
	uint32_t pic_width_in_mbs_minus1;               // ue(v)
	uint32_t pic_height_in_map_units_minus1;        // ue(v)
	byte     frame_mbs_only_flag;                   // u(1)
	// 0: pic_height_in_map_units_minus1+1 = field_height
	// 1: pic_height_in_map_units_minus1+1 = frame_height

	byte     frame_cropping_flag;                   // u(1)
	uint32_t frame_cropping_rect_left_offset;       // ue(v)
    uint32_t frame_cropping_rect_right_offset;      // ue(v)
    uint32_t frame_cropping_rect_top_offset;        // ue(v)
    uint32_t frame_cropping_rect_bottom_offset;     // ue(v)

	byte     direct_8x8_inference_flag;             // u(1)
	byte     vui_parameters_present_flag;           // u(1)
	VUI_Parameters vui_seq_parameter;
} SeqParameterSet;

typedef struct pic_parameter_set_rbsp
{
	uint8_t  pic_parameter_set_id;					// ue(v) (0~255)
	uint8_t  seq_parameter_set_id;                  // ue(v) (0~31)
	byte     entropy_coding_mode_flag;              // u(1)
	byte     pic_order_present_flag;                // u(1)

	uint8_t  num_ref_idx_l0_active_minus1;          // ue(v)
	uint8_t  num_ref_idx_l1_active_minus1;          // ue(v)
	byte     weighted_pred_flag;                    // u(1)
	uint8_t  weighted_bipred_idc;                   // u(2)  (0~2)
	int8_t   pic_init_qp_minus26;
	int8_t   chroma_qp_index_offset;                // se(v) (-12~12)

	byte     deblocking_filter_control_present_flag;// u(1)
	byte     transform_8x8_mode_flag;               // u(1)
	byte     pic_scaling_matrix_present_flag;       // u(1)
	/*
	byte     pic_scaling_list_present_flag[8];
	int      delta_scale4x4[16];
	int      delta_scale16x16[64];
	byte     UseDefaultScalingMatrix4x4Flag[6];
	byte     UseDefaultScalingMatrix8x8Flag[2];
	int      ScalingList4x4[6][16];
	int      ScalingList8x8[2][64];
	*/
	int8_t   second_chroma_qp_index_offset;         // se(v) (-12~12)
} PicParameterSet;

int prepare_sps(void *ptVid, SeqParameterSet *p_SPS, FAVC_ENC_PARAM *pParam, int sps_id);
int prepare_pps(void *ptVid, PicParameterSet *p_PPS, FAVC_ENC_PARAM *pParam, int pps_id);
void prepare_vui(VUI_Parameters *vui, FAVC_ENC_VUI_PARAM *p_VUIParam);
int write_sps(void *ptEncHandle, SeqParameterSet *sps);
int write_pps(void *ptEncHandle, PicParameterSet *pps);
#ifdef AES_ENCRYPT
int write_sei(void *ptEncHandle, uint8_t sei_type, int sei_length, char *sei_data);
#endif

#endif
