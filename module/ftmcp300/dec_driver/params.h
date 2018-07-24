#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "portab.h"
#include "define.h"

#define MAX_NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE 256
#define MAX_VALUE_OF_CPB_CNT 32
//#define MAX_SLICE_GROUPS 8


typedef struct hrd_parameters_t
{
	uint32_t bit_rate_value_minus1[MAX_VALUE_OF_CPB_CNT];    // ue(v)
	uint32_t cpb_size_value_minus1[MAX_VALUE_OF_CPB_CNT];    // ue(v)

	uint8_t  cpb_cnt_minus1;                                  // ue(v) (0~31)
	uint8_t  bit_rate_scale;                                  // u(4)
	uint8_t  cpb_size_scale;                                  // u(4)
	uint8_t  initial_cpb_removal_delay_length_minus1;         // u(5)
	uint8_t  cpb_removal_delay_length_minus1;                 // u(5)
	uint8_t  dpb_output_delay_length_minus1;                  // u(5)
	uint8_t  time_offset_length;                              // u(5)
	byte     cbr_flag[MAX_VALUE_OF_CPB_CNT];                  // u(1)
}HRD_Parameters;

typedef struct vui_parameter_set_t
{
	uint32_t num_units_in_tick;                                // u(32)
	uint32_t time_scale;                                       // u(32)

	uint16_t sar_width;                                        // u(16)
	uint16_t sar_height;                                       // u(16)

	//byte     aspect_ratio_info_present_flag;                   // u(1)
	//byte     overscan_info_present_flag;                       // u(1)
	byte     overscan_appropriate_flag;                        // u(1)
	//byte     video_signal_type_present_flag;                   // u(1)
	byte     video_full_range_flag;                            // u(1)
	//byte     colour_description_present_flag;                  // u(1)
	//byte     chroma_location_info_present_flag;                // u(1)
	byte     timing_info_present_flag;                         // u(1)
	byte     fixed_frame_rate_flag;                            // u(1)
	byte     nal_hrd_parameters_present_flag;                  // u(1)
	byte     vcl_hrd_parameters_present_flag;                  // u(1)
	byte     low_delay_hrd_flag;                               // u(1)
	byte     pic_struct_present_flag;                          // u(1)
	byte     bitstream_restriction_flag;                       // u(1)
	byte     motion_vectors_over_pic_boundaries_flag;          // u(1)
	
	uint8_t  aspect_ratio_idc;                                 // u(8)
	uint8_t  video_format;                                     // u(3)
	uint8_t  colour_primaries;                                 // u(8)
	uint8_t  transfer_characteristics;                         // u(8)
	uint8_t  matrix_coefficients;                              // u(8)
	uint8_t  chroma_sample_loc_type_top_field;                 // ue(v) (0~5)
	uint8_t  chroma_sample_loc_type_bottom_field;              // ue(v) (0~5)
	uint8_t  max_bytes_per_pic_denom;                          // ue(v) (0~16)
	uint8_t  max_bits_per_mb_denom;                            // ue(v) (0~16)
	uint8_t  log2_max_mv_length_vertical;                      // ue(v)
	uint8_t  log2_max_mv_length_horizontal;                    // ue(v)
	uint8_t  num_reorder_frames;                               // ue(v)
	uint8_t  max_dec_frame_buffering;                          // ue(v)

	HRD_Parameters nal_hrd_parameters;
	HRD_Parameters vcl_hrd_parameters;
}VUI_Parameters;

struct seq_parameter_set_rbsp_t
{
	uint32_t pic_width_in_mbs_minus1;                  // ue(v)
	uint32_t pic_height_in_map_units_minus1;           // ue(v)

	int	     offset_for_non_ref_pic;                   // se(v)
	int      offset_for_top_to_bottom_field;           // se(v)
	int      offset_for_ref_frame[MAX_NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE];  // se(v)
	int8_t   ScalingList4x4[6][16];                    // se(v) (-128~127)
	int8_t   ScalingList8x8[2][64];                    // se(v) (-128~127)

	uint8_t  seq_parameter_set_id;                     // ue(v) (0~31)
	uint8_t  chroma_format_idc;                        // ue(v) (0~3)
	uint8_t  profile_idc;                              // u(8)
	uint8_t  level_idc;                                // u(8)
	//uint8_t  bit_depth_luma_minus8;                    // ue(v) (0~4)
	//uint8_t  bit_depth_chroma_minus8;                  // ue(v)	(0~4)
	uint8_t  log2_max_frame_num;
	uint8_t  log2_max_pic_order_cnt_lsb; 		       // ue(v) (0~12)
	uint8_t  pic_order_cnt_type;                       // ue(v) (0~2)
	uint8_t  num_ref_frames;                           // ue(v) (0~maxDpbSize(16))
	uint16_t num_ref_frames_in_pic_order_cnt_cycle;    // ue(v) (0~255)

	//byte     residual_colour_transform_flag;           // u(1)
	//byte     qpprime_y_zero_transform_bypass_flag;     // u(1)
	byte     seq_scaling_matrix_present_flag;          // u(1)
	byte     seq_scaling_list_present_flag[8];         // u(1) 8  for high profile (6:4x4 + 2:8x8)
	byte     UseDefaultScalingMatrix4x4Flag[6];
	byte     UseDefaultScalingMatrix8x8Flag[2];
	byte     delta_pic_order_always_zero_flag;         // u(1)
	byte     gaps_in_frame_num_value_allowed_flag;     // u(1)
	byte     frame_mbs_only_flag;                      // u(1)
	byte     mb_adaptive_frame_field_flag;             // u(1)
	byte     direct_8x8_inference_flag;                // u(1)
	//byte     frame_cropping_flag;                      // u(1)
	uint32_t frame_cropping_rect_left_offset;          // ue(v)
	uint32_t frame_cropping_rect_right_offset;         // ue(v)
	uint32_t frame_cropping_rect_top_offset;           // ue(v)
	uint32_t frame_cropping_rect_bottom_offset;        // ue(v)
	byte     vui_parameters_present_flag;              // u(1)

	byte     b_unsupport;
	byte     b_syntax_error;
	VUI_Parameters vui_seq_parameters;

	byte     bUpdate;   // if parsing sps & not be use, sps is updated

	// parsing sps => bUpdate = 1
	// slice uses this sps => bUpdate = 0
	// sps is re-parsed => bUpdate = 1
};


struct pic_parameter_set_rbsp_t
{
	uint8_t  pic_parameter_set_id;                     // ue(v) (0~255)
	uint8_t  seq_parameter_set_id;                     // ue(v) (0~31)
	uint8_t  num_slice_groups_minus1;                  // ue(v) (0~7)

	uint8_t  num_ref_idx_l0_active_minus1;             // ue(v)
	uint8_t  num_ref_idx_l1_active_minus1;             // ue(v)
	uint8_t  weighted_bipred_idc;                      // u(2)  (0~2)
	int8_t   chroma_qp_index_offset;                   // se(v) (-12~12)

	byte     entropy_coding_mode_flag;                 // u(1)
	byte     pic_order_present_flag;                   // u(1)
	byte     weighted_pred_flag;                       // u(1)
	byte     deblocking_filter_control_present_flag;   // u(1)
	byte     constrained_intra_pred_flag;              // u(1)
	byte     redundant_pic_cnt_present_flag;           // u(1)
	byte     transform_8x8_mode_flag;                  // u(1)
	byte     pic_scaling_matrix_present_flag;          // u(1)
	byte     pic_scaling_list_present_flag[8];         // u(1)
	byte     UseDefaultScalingMatrix4x4Flag[6];
	byte     UseDefaultScalingMatrix8x8Flag[6];

	int8_t   pic_init_qp;                              // se(v) (-26~25)
	//int8_t   pic_init_qs;                              // se(v) (-26~25)
	int8_t   second_chroma_qp_index_offset;            // se(v) (-12~12)
	int8_t   ScalingList4x4[6][16];                    // se(v)
	int8_t   ScalingList8x8[2][64];                    // se(v)
	byte     b_unsupport;     /* this pps has unsupported syntax element */
	byte     b_syntax_error;  /* this pps contains syntax error */

	byte     bUpdate;   // if parsing sps & not be use, sps is updated
	/* set to 1 at tne end of parsing pps
     * clear to 0 at slice header referencing this pps 
     */

	// parsing pps => bUpdate = 1
	// slice uses this pps => bUpdate = 0
	// pps is re-parsed => bUpdate = 1
};

int readSPS(DecoderParams *p_Dec, VideoParams *p_Vid);
int readPPS(DecoderParams *p_Dec, VideoParams *p_Vid);
int access_unit_delimiter(DecoderParams *p_Dec, VideoParams *p_Vid);
int readSEI(DecoderParams *p_Dec, VideoParams *p_Vid);
void rbsp_slice_trailing_bits(DecoderParams *p_Dec);

#endif

