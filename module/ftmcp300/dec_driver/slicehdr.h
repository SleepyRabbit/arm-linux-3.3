#ifndef _SLICEHDR_H_
#define _SLICEHDR_H_

#include "define.h"
#include "portab.h"
//#include "global.h"
#define MAX_HW_POC 0x7FFF

typedef struct reorder_ref_pic_list_t
{
	int32_t abs_diff_pic_num_minus1;
	int32_t long_term_pic_num;
	uint8_t  reordering_of_pic_nums_idc; // (0~3)
}ReorderRefPicList;

typedef struct _slice_header {
	uint32_t first_mb_in_slice;
	uint32_t pic_order_cnt_lsb;
	uint32_t currMbAddr;

	int      delta_pic_order_cnt_bottom;
	int      delta_pic_order_cnt[2];

	uint8_t  slice_type;
	uint32_t pic_parameter_set_id;
	//uint8_t  redundant_pic_cnt; // (0~127)
	uint8_t  num_ref_idx_l0_active_minus1;
	uint8_t  num_ref_idx_l1_active_minus1;
	uint8_t  luma_log2_weight_denom;	// (0~7)
	uint8_t  chroma_log2_weight_denom; // (0~7)
	uint8_t  cabac_init_idc; // (0~2)
	uint8_t  disable_deblocking_filter_idc; // (0~2)

	int8_t   SliceQPy;				// range : -(sps->bit_depth_luma_minus8+8) ~ 51
	int8_t   slice_alpha_c0_offset_div2;	// (-6~6)
	int8_t   slice_beta_offset_div2;		// (-6~6)

	//byte     field_pic_flag;
	byte     bottom_field_flag;
	byte     direct_spatial_mv_pred_flag;      //!< Direct Mode type to be used (0: Temporal, 1: Spatial)
	//byte     num_ref_idx_active_override_flag;
	
	// ref_pic_list_reordering()
	byte     ref_pic_list_reordering_flag_l0;
	byte     ref_pic_list_reordering_flag_l1;

	// dec_ref_pic_marking()
	byte     no_output_of_prior_pics_flag;
	byte     long_term_reference_flag;
	byte     adaptive_ref_pic_marking_mode_flag;
	//byte     sp_for_switch_flag;

	ReorderRefPicList reordering_of_ref_pic_list[2][MAX_LIST_SIZE];
    
    uint32_t weightl0[64];
    uint32_t weightl1[64];
}SliceHeader;

int read_slice_header(void *ptDecHandle, void *ptVid, void *ptSliceHdr);
void decode_poc(void *ptVid);

#endif

