#ifndef _SLICE_HEADER_H_
#define _SLICE_HEADER_H_

#include "portab.h"
#include "define.h"

#define HORZ_MB(sr) ((((sr)/2) + ((sr)%2))*2+2)

typedef struct slice_header_t {
    uint32_t first_mb_in_slice;
    //uint8_t  slice_type;              // store in decode picture
    //uint32_t pic_parameter_set_id;    // only one pic_parameter_set
    uint32_t frame_num;
    //byte     field_pic_flag;          // store in p_Vid
    //byte     bottom_field_flag;
    uint32_t idr_pic_id; 
    //uint32_t pic_order_cnt_lsb;       // store in decode_picture
    //int      delta_pic_order_cnt_bottom;
    int      delta_pic_order_cnt[2];

    //uint8_t  redundant_pic_cnt;       // (0~127)
    byte     direct_spatial_mv_pred_flag;
    /*
    byte     num_ref_idx_active_override_minus1;
    uint8_t  num_ref_idx_l0_active_minus1;
    uint8_t  num_ref_idx_l1_active_minus1;
    */

    // ref_pic_list_reordering
    //byte     ref_pic_list_reordering_flag_l0;
    //byte     ref_pic_list_reordering_flag_l1;

    // pred_weight_table
    /*
    uint8_t  luma_log2_weight_denom;
    uint8_t  chroma_log2_weight_denom;
    byte     luma_weight_l0_flag[MAX_LIST_SIZE];
    int16_t  luma_weight_l0[MAX_LIST_SIZE];
    int16_t  luma_offset_l0[MAX_LIST_SIZE];
    byte     chroma_weight_l0_flag[MAX_LIST_SIZE];
    int16_t  chroma_weight_l0[MAX_LIST_SIZE][2];
    int16_t  chroma_offset_l0[MAX_LIST_SIZE][2];
    byte     luma_weight_l1_flag[MAX_LIST_SIZE];
    int16_t  luma_weight_l1[MAX_LIST_SIZE];
    int16_t  luma_offset_l1[MAX_LIST_SIZE];
    byte     chroma_weight_l1_flag[MAX_LIST_SIZE];
    int16_t  chroma_weight_l1[MAX_LIST_SIZE][2];
    int16_t  chroma_offset_l1[MAX_LIST_SIZE][2];
    */

    // dec_ref_pic_marking
    byte     no_output_of_prior_pics_flag;
    byte     long_term_reference_flag;
    //byte     adaptive_ref_pic_marking_mode_flag;

    //uint8_t  cabac_init_idc;
    int8_t   slice_qp_delta;

    uint8_t  disable_deblocking_filter_idc; // (0~2)
    int8_t   slice_alpha_c0_offset_div2;    // (-6~6)
    int8_t   slice_beta_offset_div2;        // (-6~6)

    // slice information
    uint32_t start_Y_pos;
    uint32_t end_Y_pos;
    byte     last_slice;
    //uint32_t slice_number;
    uint32_t slice_index;
} SliceHeader;

int write_slice_header(void *ptEncHandle, void *ptVid, SliceHeader *currSlice, void *ptPic);
int encode_one_slice(void *ptEncHandle);
//int encode_one_slice_trigger(void *ptEncHandle);
//int encode_one_slice_sync(void *ptEncHandle);
#endif

