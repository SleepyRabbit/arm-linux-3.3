#ifdef LINUX
	#include <linux/kernel.h>
	#include <linux/string.h>
//    #if defined(LINUX) & defined(VG_INTERFACE)
//    #include "log.h"    //include log system "printm","damnit"...
//    #endif
#else
	#include <string.h>
#endif
#include "global.h"
#include "params.h"
#include "vlc.h"
#include "quant.h"
#include "h264_reg.h"
#include "sei.h"



static const uint8_t ZZ_SCAN[16]  =
{  0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

static const uint8_t ZZ_SCAN8[64] =
{  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};



#if 0 //USE_SW_PARSER == 0
static int hw_rbsp_trailing_bits(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	uint32_t tmp;
	uint8_t len;

	tmp = show_bits(ptReg_io, 1);

	if (tmp!=0x01){
        byte_align(ptReg_io);
		return -1;
	}
	else
		tmp = read_u(ptReg_io, 1);
    
	len = bit_offset(ptReg_io);
	tmp = 0;
	if (len != 0)
		tmp = read_u(ptReg_io, 8 - len);
	if (tmp != 0){
		return -1;
	}
	return 0;
}


void rbsp_slice_trailing_bits(DecoderParams *p_Dec)
{
    hw_rbsp_trailing_bits(p_Dec);
}
#else
void rbsp_slice_trailing_bits(DecoderParams *p_Dec)
{
    /* not required, as the next operation is always "find the next start code" */
}
#endif /* USE_SW_PARSER == 0 */


static int rbsp_trailing_bits(DecoderParams *p_Dec)
{
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	uint32_t tmp;
	uint8_t len;

	tmp = SHOW_BITS(p_Par, 1);

	if (tmp!=0x01){
        BYTE_ALIGN(p_Par);
		return -1;
	}
	else
		tmp = READ_U(p_Par, 1);
    
	len = BIT_OFFSET(p_Par);
#if 0
    if(len){
        printk("rbsp_trailing_bits: len:%d\n", len);
    }
#endif
	tmp = 0;
	if (len != 0)
		tmp = READ_U(p_Par, 8 - len);
	if (tmp != 0){
		return -1;
	}
	return 0;
}


/*
 * test if bit buffer contains only stop bit
 */
static int more_rbsp_data(DecoderParams *p_Dec)
{
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	uint32_t tmp, remain_bits;
	byte _bit_offset;
    _bit_offset = BIT_OFFSET(p_Par);
	tmp = SHOW_BITS(p_Par, 32); /* NOTE: sw_show_bits() pads bits after the end of bitstream as 0 */
	remain_bits = ((tmp>>24)&0xFF)>>_bit_offset; /* get remain bits of the current byte (bits from read pointer to the end of current byte), shift it to LSB */
	tmp = tmp << (8-_bit_offset); /* shift data to start of the next byte (only 24 bits at MSB are always valid) */
	remain_bits = remain_bits << _bit_offset; /* shift bits to MSB for the ease of comparing stop bit */
	if (remain_bits!=0x80 || (tmp>>10)!=0)   /* has more data if (not stop bit) or (not start code) */
		return 1;
	else
		return 0;
}


/* 
 * syntax for scaling list matrix values (in SPS or PPS)
 */
static void scaling_list(DecoderParams *p_Dec, int8_t *scalingList, int sizeOfScalingList, byte *useDefaultScalingMatrixFlag)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int lastScale = 8, nextScale = 8;
	int delta_scale, j, scanj;
	
	for (j = 0; j<sizeOfScalingList; j++){
		scanj = (sizeOfScalingList==16) ? ZZ_SCAN[j]:ZZ_SCAN8[j];
		if (nextScale!=0){
			delta_scale = READ_SE(p_Par);
			nextScale = (lastScale + delta_scale + 256)%256;
			*useDefaultScalingMatrixFlag = (scanj==0 && nextScale==0) ? 1 : 0;
		}
		scalingList[scanj] = (nextScale==0) ? lastScale:nextScale;
		lastScale = scalingList[scanj];
	}
}

static void readHRD(DecoderParams *p_Dec, HRD_Parameters *hrd)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int SchedSelIdx;
	
	hrd->cpb_cnt_minus1 = READ_UE(p_Par);
	hrd->bit_rate_scale = READ_U(p_Par, 4);
	hrd->cpb_size_scale = READ_U(p_Par, 4);
	for (SchedSelIdx = 0; SchedSelIdx<=hrd->cpb_cnt_minus1; SchedSelIdx++){
		hrd->bit_rate_value_minus1[SchedSelIdx] = READ_UE(p_Par);
		hrd->cpb_size_value_minus1[SchedSelIdx] = READ_UE(p_Par);
		hrd->cbr_flag[SchedSelIdx] = READ_U(p_Par, 1);
	}
	hrd->initial_cpb_removal_delay_length_minus1 = READ_U(p_Par, 5);
	hrd->cpb_removal_delay_length_minus1 = READ_U(p_Par, 5);
	hrd->dpb_output_delay_length_minus1 = READ_U(p_Par, 5);
	hrd->time_offset_length = READ_U(p_Par, 5);
}

static void readVUI(DecoderParams *p_Dec, VUI_Parameters *vui)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	byte flag;
	
	flag = READ_U(p_Par, 1);		// aspect_ratio_info_present_flag
	if (flag){
		vui->aspect_ratio_idc = READ_U(p_Par, 8);
		if (vui->aspect_ratio_idc==255){
			vui->sar_width  = READ_U(p_Par, 16);
			vui->sar_height = READ_U(p_Par, 16);
		}
	}
	
	flag = READ_U(p_Par, 1);		// overscan_info_present_flag
	if (flag)
		vui->overscan_appropriate_flag = READ_U(p_Par, 1);
	flag = READ_U(p_Par, 1);		// video_signal_type_present_flag
	vui->matrix_coefficients = 2;
	if (flag){
		vui->video_format = READ_U(p_Par, 3);
		vui->video_full_range_flag = READ_U(p_Par, 1);
		flag = READ_U(p_Par, 1);	// colour_description_present_flag
		if (flag){
			vui->colour_primaries = READ_U(p_Par, 8);
			vui->transfer_characteristics = READ_U(p_Par, 8);
			vui->matrix_coefficients = READ_U(p_Par, 8);
		}
	}

	flag = READ_U(p_Par, 1);		// chroma_location_info_present_flag  
	if (flag){
		vui->chroma_sample_loc_type_top_field = READ_UE(p_Par);
		vui->chroma_sample_loc_type_bottom_field = READ_UE(p_Par);
	}

	flag = READ_U(p_Par, 1);		// timing_info_present_flag
	vui->timing_info_present_flag = flag;
	if (flag){
		vui->num_units_in_tick = READ_U(p_Par, 32);
		vui->time_scale = READ_U(p_Par, 32);
		vui->fixed_frame_rate_flag = READ_U(p_Par, 1);
	}
	
	vui->nal_hrd_parameters_present_flag = READ_U(p_Par, 1);
	if (vui->nal_hrd_parameters_present_flag)
		readHRD(p_Dec, &vui->nal_hrd_parameters);
	vui->vcl_hrd_parameters_present_flag     = READ_U(p_Par, 1);
	if (vui->vcl_hrd_parameters_present_flag)
		readHRD(p_Dec, &vui->vcl_hrd_parameters);

	if (vui->nal_hrd_parameters_present_flag || vui->vcl_hrd_parameters_present_flag)
		vui->low_delay_hrd_flag = READ_U(p_Par, 1);
	vui->pic_struct_present_flag = READ_U(p_Par, 1);

	vui->bitstream_restriction_flag = READ_U(p_Par, 1);
	if (vui->bitstream_restriction_flag){
		vui->motion_vectors_over_pic_boundaries_flag = READ_U(p_Par, 1);
		vui->max_bytes_per_pic_denom = READ_UE(p_Par);
		vui->max_bits_per_mb_denom = READ_UE(p_Par);
		vui->log2_max_mv_length_horizontal = READ_UE(p_Par);
		vui->log2_max_mv_length_vertical = READ_UE(p_Par);
		vui->num_reorder_frames = READ_UE(p_Par);
		vui->max_dec_frame_buffering = READ_UE(p_Par);
	}
}

int readSPS(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	SeqParameterSet *p_SPS;
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	uint32_t profile_idc, cs_flag, level_idc, seq_parameter_set_id;
	//uint32_t cropping_tmp;	// frame_cropping_rect_left_offset, frame_cropping_rect_right_offset, frame_cropping_rect_top_offset, frame_cropping_rect_bottom_offset;
	uint8_t bit_depth_tmp;	// bit_depth_luma_minus8, bit_depth_chroma_minus8;
	int i;
	byte flag = 0;
    extern unsigned int max_level_idc;

    // Support High Profile at Level 4.1
	profile_idc = READ_U(p_Par, 8);
	cs_flag = READ_U(p_Par, 8);
	#if 1 // check if profile is supported
	if ((profile_idc!=BASELINE) && (profile_idc!=MAIN) && (profile_idc!=EXTENDED) &&
        (profile_idc!=FREXT_HP) && (profile_idc!=FREXT_Hi10P) && (profile_idc!=FREXT_Hi422) &&
		(profile_idc!=FREXT_Hi444) && ((cs_flag&0x0F)!=0x00)){
		E_DEBUG("error unsupport profile %d\n", profile_idc);
		flag = 1;
		//return H264D_F_UNSUPPORT;
	}
	#endif
	level_idc = READ_U(p_Par, 8);
	seq_parameter_set_id = READ_UE(p_Par);

	if (seq_parameter_set_id > MAX_SPS_NUM) {
		E_DEBUG("sps id is larger than MAX_SPS_NUM (%d > %d)\n", seq_parameter_set_id, MAX_SPS_NUM);
		return H264D_PARSING_ERROR;
	}

	if (p_Vid->SPS[seq_parameter_set_id] == NULL){
		//if ((p_Vid->SPS[seq_parameter_set_id] = (SeqParameterSet*)p_Dec->pfnMalloc(sizeof(SeqParameterSet), p_Dec->u32CacheAlign, p_Dec->u32CacheAlign))==NULL) 
        if ((p_Vid->SPS[seq_parameter_set_id] = (SeqParameterSet*)p_Dec->pfnMalloc(sizeof(SeqParameterSet)))==NULL) 
        {
			E_DEBUG("allocate sps error\n");
			return H264D_ERR_MEMORY;
		}
	}
	p_SPS = p_Vid->SPS[seq_parameter_set_id];
	memset(p_SPS, 0, sizeof(SeqParameterSet));

    if ((profile_idc != BASELINE) && (profile_idc != MAIN) && (profile_idc != EXTENDED) && (profile_idc != FREXT_HP)) {
        W_DEBUG("unsupprt this profile %d\n", profile_idc);
        p_SPS->b_unsupport = 1;
    }
   	p_SPS->profile_idc = profile_idc;
    
   	// check if level <= 4.1 (MAX_LEVEL_IDC)
    if (level_idc > max_level_idc) {
        W_DEBUG("unsupprt level (%d) > %d\n", level_idc, max_level_idc);
        p_SPS->b_unsupport = 1;
    }
//printk("sps: %08x\n", SHOW_BITS(p_Dec, 32));
	p_SPS->level_idc = level_idc;
	p_SPS->seq_parameter_set_id = seq_parameter_set_id;
	p_SPS->chroma_format_idc = 1;		// default value of chroma_format_idc = 1 (4:2:0)
	p_SPS->seq_scaling_matrix_present_flag = 0;

	if (profile_idc==FREXT_HP || profile_idc==FREXT_Hi10P || profile_idc==FREXT_Hi422 || profile_idc==FREXT_Hi444){
		p_SPS->chroma_format_idc = READ_UE(p_Par);
		if (p_SPS->chroma_format_idc > 1) {
		    W_DEBUG("chroma format idc (%d) not support > 1\n", p_SPS->chroma_format_idc);
            p_SPS->b_syntax_error = 1;
        }
		if (p_SPS->chroma_format_idc==3)
			flag = READ_U(p_Par, 1);		// residual_colour_transform_flag or separate_colour_plane_flag
		bit_depth_tmp = READ_UE(p_Par);	// bit_depth_luma_minus8
		bit_depth_tmp = READ_UE(p_Par);	// bit_depth_chroma_minus8
		flag = READ_U(p_Par, 1);			// qpprime_y_zero_transform_bypass_flag
		p_SPS->seq_scaling_matrix_present_flag = READ_U(p_Par, 1);
		if (p_SPS->seq_scaling_matrix_present_flag){
			for (i = 0; i<8; i++){
				p_SPS->seq_scaling_list_present_flag[i] = READ_U(p_Par, 1);
				if (p_SPS->seq_scaling_list_present_flag[i]){
					if (i<6)
						scaling_list(p_Dec, p_SPS->ScalingList4x4[i], 16, &p_SPS->UseDefaultScalingMatrix4x4Flag[i]);
					else
						scaling_list(p_Dec, p_SPS->ScalingList8x8[i-6], 64, &p_SPS->UseDefaultScalingMatrix8x8Flag[i-6]);
				}
			}
		}
	}
	
	p_SPS->log2_max_frame_num = READ_UE(p_Par) + 4;	// 0~12
	if (p_SPS->log2_max_frame_num > 16) {
	    W_DEBUG("log2 max frame num (%d) > 16\n", p_SPS->log2_max_frame_num);
	    p_SPS->b_syntax_error = 1;
    }
	p_SPS->pic_order_cnt_type = READ_UE(p_Par);		// 0~2
	if (p_SPS->pic_order_cnt_type > 2) {
	    W_DEBUG("pic order cnt type should be between 0 and 2 (%d)\n", p_SPS->pic_order_cnt_type);
	    p_SPS->b_syntax_error = 1;
    }
	if (p_SPS->pic_order_cnt_type==0) {
		p_SPS->log2_max_pic_order_cnt_lsb = READ_UE(p_Par) + 4;	// 0~12
		if (p_SPS->log2_max_pic_order_cnt_lsb > 16) {
		    W_DEBUG("log2 max pic order cnt lsb (%d) > 16\n", p_SPS->log2_max_pic_order_cnt_lsb);
		    p_SPS->b_syntax_error = 1;
        }
    }
	else if (p_SPS->pic_order_cnt_type== 1)	{
		p_SPS->delta_pic_order_always_zero_flag = READ_U(p_Par, 1);
		p_SPS->offset_for_non_ref_pic = READ_SE(p_Par);
		p_SPS->offset_for_top_to_bottom_field = READ_SE(p_Par);
		p_SPS->num_ref_frames_in_pic_order_cnt_cycle = READ_UE(p_Par);
		p_SPS->num_ref_frames_in_pic_order_cnt_cycle = iMin(255, p_SPS->num_ref_frames_in_pic_order_cnt_cycle);
		for (i = 0; i<p_SPS->num_ref_frames_in_pic_order_cnt_cycle; i++)
			p_SPS->offset_for_ref_frame[i] = READ_SE(p_Par);
	}
	
	p_SPS->num_ref_frames = READ_UE(p_Par);  // constraint: < max dpb
	p_SPS->gaps_in_frame_num_value_allowed_flag = READ_U(p_Par, 1);
	p_SPS->pic_width_in_mbs_minus1 = READ_UE(p_Par);
	p_SPS->pic_height_in_map_units_minus1 = READ_UE(p_Par);
	p_SPS->frame_mbs_only_flag = READ_U(p_Par, 1);
	p_SPS->mb_adaptive_frame_field_flag = 0;
	if(!p_SPS->frame_mbs_only_flag)
		p_SPS->mb_adaptive_frame_field_flag = READ_U(p_Par, 1);
	p_SPS->direct_8x8_inference_flag = READ_U(p_Par, 1);
	flag = READ_U(p_Par, 1);				// frame_cropping_flag
	if (flag){
	    //W_DEBUG("unsupport croping\n");	    
        LOG_PRINT(LOG_INFO, "croping flag = 1\n");	    
	    //p_SPS->b_unsupport = 1;   // no use
	    p_SPS->frame_cropping_rect_left_offset = READ_UE(p_Par);     // frame_cropping_rect_left_offset
		p_SPS->frame_cropping_rect_right_offset = READ_UE(p_Par);    // frame_cropping_rect_right_offset
		p_SPS->frame_cropping_rect_top_offset = READ_UE(p_Par);      // frame_cropping_rect_top_offset
		p_SPS->frame_cropping_rect_bottom_offset = READ_UE(p_Par);   // frame_cropping_rect_bottom_offset
	}
	else {
	    p_SPS->frame_cropping_rect_left_offset = 0;
		p_SPS->frame_cropping_rect_right_offset = 0;
		p_SPS->frame_cropping_rect_top_offset = 0;
		p_SPS->frame_cropping_rect_bottom_offset = 0;
	}
	p_SPS->vui_parameters_present_flag = READ_U(p_Par, 1);

	if (p_SPS->vui_parameters_present_flag){
		readVUI(p_Dec, &p_SPS->vui_seq_parameters);
        LOG_PRINT(LOG_INFO, "timing_info:%d time_scale:%08X  num_units_in_tick:%08X\n", 
            p_SPS->vui_seq_parameters.timing_info_present_flag,
            p_SPS->vui_seq_parameters.time_scale, 
            p_SPS->vui_seq_parameters.num_units_in_tick);
    }

	p_SPS->bUpdate = 1;

//printk("<update> sps id %d: num ref frame = %d\n", p_SPS->seq_parameter_set_id, p_SPS->num_ref_frames);

	if (rbsp_trailing_bits(p_Dec)<0) {
		W_DEBUG("sps rbsp trailing bit error\n");
		return H264D_PARSING_WARNING;
	}
	
	return H264D_OK;
}

int readPPS(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	PicParameterSet *p_PPS;
	SeqParameterSet *p_SPS;
	uint32_t pic_parameter_set_id, i;
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int qs;
//sprintk("pps: %08x\n", SHOW_BITS(p_Dec, 32));
	pic_parameter_set_id = READ_UE(p_Par);
	if (pic_parameter_set_id > MAX_PPS_NUM) {
		E_DEBUG("pps id is larger than MAX_SPS_NUM (%d > %d)\n", pic_parameter_set_id, MAX_PPS_NUM);
		return H264D_PARSING_ERROR;
	}
	if (p_Vid->PPS[pic_parameter_set_id] == NULL){
		//if ((p_Vid->PPS[pic_parameter_set_id] = (PicParameterSet*)p_Dec->pfnMalloc(sizeof(PicParameterSet), p_Dec->u32CacheAlign, p_Dec->u32CacheAlign)) == NULL) 
        if ((p_Vid->PPS[pic_parameter_set_id] = (PicParameterSet*)p_Dec->pfnMalloc(sizeof(PicParameterSet))) == NULL) 
        {
			E_DEBUG("alloc error\n");
			return H264D_ERR_MEMORY;
		}
	}
	p_PPS = p_Vid->PPS[pic_parameter_set_id];
	memset(p_PPS, 0, sizeof(PicParameterSet));
	p_PPS->pic_parameter_set_id = pic_parameter_set_id;
//printk("sps: %08x\n", SHOW_BITS(p_Dec, 32));
	p_PPS->seq_parameter_set_id = READ_UE(p_Par);
	if (p_PPS->seq_parameter_set_id < MAX_SPS_NUM)
		p_SPS = p_Vid->SPS[p_PPS->seq_parameter_set_id];
	else {
		E_DEBUG("sps id is larger than MAX_SPS_NUM (%d > %d)\n", p_PPS->seq_parameter_set_id, MAX_SPS_NUM);
		return H264D_PARSING_ERROR;
	}
//printk("cabac: %08x\n", SHOW_BITS(p_Dec, 32));
	p_PPS->entropy_coding_mode_flag = READ_U(p_Par, 1);
//printk("popf: %08x\n", SHOW_BITS(p_Dec, 32));
	p_PPS->pic_order_present_flag = READ_U(p_Par, 1);
//printk("num slice group: %08x\n", SHOW_BITS(p_Dec, 32));
	p_PPS->num_slice_groups_minus1 = READ_UE(p_Par);
//printk("end: %08x\n", SHOW_BITS(p_Dec, 32));
	if (p_PPS->num_slice_groups_minus1 != 0)
	{
		W_DEBUG("unsupport num_slice_groups (%d) > 0\n", p_PPS->num_slice_groups_minus1);
		p_PPS->b_unsupport = 1;
		//return H264D_F_UNSUPPORT;
	}
	// not parsing num_slice_groups_minus1>0

	p_PPS->num_ref_idx_l0_active_minus1 = READ_UE(p_Par);	// 0~32
	p_PPS->num_ref_idx_l1_active_minus1 = READ_UE(p_Par);	// 0~32
	p_PPS->weighted_pred_flag = READ_U(p_Par, 1);
	p_PPS->weighted_bipred_idc = READ_U(p_Par, 2);	// 0~2
	if (p_PPS->weighted_bipred_idc > 2) {
	    W_DEBUG("pps weighted bipred idx is out of range (%d)\n", p_PPS->weighted_bipred_idc);
	    p_PPS->b_syntax_error = 1;
	}	
	p_PPS->pic_init_qp = READ_SE(p_Par) + 26;
	if (p_PPS->pic_init_qp < 0 || p_PPS->pic_init_qp > 51) {
	    W_DEBUG("pps init qp is out of range (%d)\n", p_PPS->pic_init_qp);
	    p_PPS->b_syntax_error = 1;
    }
	qs = READ_SE(p_Par);
	p_PPS->chroma_qp_index_offset = READ_SE(p_Par);	// -12~12
	if (p_PPS->chroma_qp_index_offset < -12 || p_PPS->chroma_qp_index_offset > 12) {
	    W_DEBUG("pps chroma qp offset is out of range (%d)\n", p_PPS->chroma_qp_index_offset);
	    p_PPS->b_syntax_error = 1;
	}
	p_PPS->second_chroma_qp_index_offset = p_PPS->chroma_qp_index_offset;
	p_PPS->deblocking_filter_control_present_flag = READ_U(p_Par, 1);
	p_PPS->constrained_intra_pred_flag = READ_U(p_Par, 1);
	p_PPS->redundant_pic_cnt_present_flag = READ_U(p_Par, 1);
	
	p_PPS->transform_8x8_mode_flag = 0;
	p_PPS->pic_scaling_matrix_present_flag = 0;

	if (more_rbsp_data(p_Dec)){
		p_PPS->transform_8x8_mode_flag = READ_U(p_Par, 1);
		p_PPS->pic_scaling_matrix_present_flag = READ_U(p_Par, 1);
		if (p_PPS->pic_scaling_matrix_present_flag){
			for (i = 0; i<6 + 2*p_PPS->transform_8x8_mode_flag; i++){
				p_PPS->pic_scaling_list_present_flag[i] = READ_U(p_Par, 1);
				if (p_PPS->pic_scaling_list_present_flag[i]){
					if (i<6)
						scaling_list(p_Dec, p_PPS->ScalingList4x4[i], 16, &p_PPS->UseDefaultScalingMatrix4x4Flag[i]);
					else
						scaling_list(p_Dec, p_PPS->ScalingList8x8[i-6], 64, &p_PPS->UseDefaultScalingMatrix8x8Flag[i-6]);
				}
			}
		}
		p_PPS->second_chroma_qp_index_offset = READ_SE(p_Par);
		if (p_PPS->second_chroma_qp_index_offset < -12 || p_PPS->second_chroma_qp_index_offset > 12) {
		    W_DEBUG("pps chroma second qp offset out of range (%d)\n", p_PPS->second_chroma_qp_index_offset);
		    p_PPS->b_syntax_error = 1;
		}
	}

	p_PPS->bUpdate = 1;

//printk("<update> pps id %d\n", p_PPS->pic_parameter_set_id);

	if (rbsp_trailing_bits(p_Dec)<0) {
		W_DEBUG("warnning: pps rbsp trailing bit error\n");
		return H264D_PARSING_WARNING;
	}

	return H264D_OK;
}

int readSEI(DecoderParams *p_Dec, VideoParams *p_Vid)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int payload_type = 0;
	int payload_size = 0;
	int i;
	uint8_t tmp_byte;
	//uint32_t tmp, flag;

	do
	{
		payload_type = 0;
		tmp_byte = READ_U(p_Par, 8);
		while (tmp_byte	== 0xFF)
		{
			payload_type += 255;
#if 0
        #ifdef LINUX
            //if (p_Dec->u32CurrIRQ & BIT1)
            if (p_Dec->bBSEmpty)
        #else
            if (ptReg_io->DEC_INTSTS & BIT1)
        #endif
#else
            if(BS_EMPTY(p_Dec))
#endif
    			return H264D_ERR_BS_EMPTY;			
			tmp_byte = READ_U(p_Par, 8);
		}
		payload_type +=	tmp_byte;	// this	is the last	byte

		payload_size = 0;
		tmp_byte = READ_U(p_Par, 8);
		while (tmp_byte	== 0xFF)
		{
			payload_size += 255;
#if 0
        #ifdef LINUX
            //if (p_Dec->u32CurrIRQ & BIT1)
            if (p_Dec->bBSEmpty)
        #else
            if (ptReg_io->DEC_INTSTS & BIT1)
        #endif
#else
            if(BS_EMPTY(p_Dec))
#endif
                return H264D_ERR_BS_EMPTY;
			tmp_byte = READ_U(p_Par, 8);
		}
		payload_size +=	tmp_byte;	// this is the last byte
		
		DUMP_MSG(D_PARSE, p_Dec,"set type: %d (%d)\n", payload_type, payload_size);
//printk("sei type %d, sei size %d\n", payload_type, payload_size);
//		if ((ret = sei_payload(p_Dec, p_Vid, payload_type, payload_size))<0)
//			return ret;
//		for (i = ret; i<(payload_size-1)*8; i+=8)
//			flush_bits(p_Dec, 8);
//		flush_bits(p_Dec, payload_size*8 - i);
		for (i = 0; i < payload_size; i++) {
		    // check start code
/*
		    if (i <= payload_size - 4) {
		        tmp = SHOW_BITS(p_Dec, 32);
            	flag = ptReg_io->BITSHIFT;
            	if ((((tmp>>8)&0xFFFFFF) == 0x000001 && (flag&(BIT10|BIT11)) == 0) || 
            	    (tmp == 0x00000001 && (flag&(BIT9|BIT10|BIT11)) == 0))
            	    return H264D_PARSING_ERROR;
            }
            else if (i <= payload_size - 3) {
                tmp = SHOW_BITS(p_Dec, 24);
            	flag = ptReg_io->BITSHIFT & (BIT10|BIT11);
            	if (tmp == 0x000001 && flag == 0)
            	    return H264D_PARSING_ERROR;
            }
*/
			SKIP_BITS(p_Par, 8);
		}
		tmp_byte = SHOW_BITS(p_Par, 8);
#if 0
	#ifdef LINUX
        //if (p_Dec->u32CurrIRQ & BIT1)
        if (p_Dec->bBSEmpty)
    #else
        if (ptReg_io->DEC_INTSTS & BIT1)
    #endif
#else
        if(BS_EMPTY(p_Dec))
#endif
			return H264D_ERR_BS_EMPTY;
	} while(tmp_byte != 0x80);    // more_rbsp_data()

	if (rbsp_trailing_bits(p_Dec)<0) {
		W_DEBUG("warnning: sei trailing bits error\n");
		return H264D_PARSING_WARNING;
	}

	return H264D_OK;
}

int access_unit_delimiter(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	//volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);

	p_Vid->primary_pic_type = READ_U(p_Par, 3);
	if (rbsp_trailing_bits(p_Dec)<0) {
		W_DEBUG("aud rbsp trailing bits error\n");
		return H264D_PARSING_WARNING;
	}
		
	return H264D_OK;
}

