#ifdef LINUX
    #include <linux/kernel.h>
	#include <linux/string.h>
#else
	#include <string.h>
#endif	
#include "sei.h"
#include "params.h"
#include "vlc.h"
#include "global.h"

unsigned CeilLog2(unsigned uiVal)
{
	uint32_t uiTmp = uiVal-1;
	uint32_t uiRet = 0;

	while(uiTmp!=0)
	{
		uiTmp >>= 1;
		uiRet++;
	}
	return uiRet;
}

uint32_t ExpGolombLen(unsigned uiVal)
{
	uint32_t uiTmp = uiVal+1;
	uint32_t uiRet = 0;

	while(uiTmp!=0)
	{
		uiTmp >>= 1;
		uiRet++;
	}
	return ((uiRet-1)<<1) + 1;
}

uint32_t ExpGolombSELen(int iVal)
{
	uint32_t uiTmp = (iVal<0 ? -iVal : iVal);
	uint32_t uiRet = 0;

	while(uiTmp!=0)
	{
		uiTmp >>= 1;
		uiRet++;
	}
	return (uiRet<<1) + 1;
}

uint32_t interpret_buffering_period_info(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	SeqParameterSet *sps;
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int seq_parameter_set_id, initial_cpb_removal_delay, initial_cpb_removal_delay_offset;
	uint32_t k, len = 0;

	seq_parameter_set_id = READ_UE(p_Par);
	len += ExpGolombLen(seq_parameter_set_id);
	sps = p_Vid->SPS[seq_parameter_set_id];

	if (sps->vui_parameters_present_flag){
		if (sps->vui_seq_parameters.nal_hrd_parameters_present_flag){
			for (k=0; k<sps->vui_seq_parameters.nal_hrd_parameters.cpb_cnt_minus1+1; k++){
				initial_cpb_removal_delay = READ_U(p_Par,        sps->vui_seq_parameters.nal_hrd_parameters.initial_cpb_removal_delay_length_minus1+1);
				initial_cpb_removal_delay_offset = READ_U(p_Par, sps->vui_seq_parameters.nal_hrd_parameters.initial_cpb_removal_delay_length_minus1+1);
				len += (sps->vui_seq_parameters.nal_hrd_parameters.initial_cpb_removal_delay_length_minus1+1)*2;
				
			}
		}
		if (sps->vui_seq_parameters.vcl_hrd_parameters_present_flag){
			for (k=0; k<sps->vui_seq_parameters.vcl_hrd_parameters.cpb_cnt_minus1+1; k++){
				initial_cpb_removal_delay = READ_U(p_Par,        sps->vui_seq_parameters.vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1+1);
				initial_cpb_removal_delay_offset = READ_U(p_Par, sps->vui_seq_parameters.vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1+1);
				len += (sps->vui_seq_parameters.vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1+1)*2;
			}
		}
	}
	
	return len;
}

uint32_t interpret_picture_timing_info(DecoderParams *p_Dec, VideoParams *p_Vid)
{
	SeqParameterSet *sps = p_Vid->active_sps;
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);

	int cpb_removal_delay, dpb_output_delay, picture_structure_present_flag, picture_structure;
	int clock_time_stamp_flag;
	int ct_type, nuit_field_based_flag, counting_type, full_timestamp_flag, discontinuity_flag, cnt_dropped_flag, nframes;
	int seconds_value, minutes_value, hours_value, seconds_flag, minutes_flag, hours_flag, time_offset;
	int NumClockTS = 0;
	int i, CpbDpbDelaysPresentFlag;
	int cpb_removal_len = 24;
	int dpb_output_len  = 24;
	uint32_t len = 0;

	if (sps == NULL){
		return 0;
	}

	CpbDpbDelaysPresentFlag = ((sps->vui_parameters_present_flag) && 
		((sps->vui_seq_parameters.nal_hrd_parameters_present_flag != 0) || 
		(sps->vui_seq_parameters.vcl_hrd_parameters_present_flag != 0)));

	if (CpbDpbDelaysPresentFlag ){
		if (sps->vui_parameters_present_flag){
			if (sps->vui_seq_parameters.nal_hrd_parameters_present_flag){
				cpb_removal_len = sps->vui_seq_parameters.nal_hrd_parameters.cpb_removal_delay_length_minus1 + 1;
				dpb_output_len = sps->vui_seq_parameters.nal_hrd_parameters.dpb_output_delay_length_minus1  + 1;
			}
			else if (sps->vui_seq_parameters.vcl_hrd_parameters_present_flag){
				cpb_removal_len = sps->vui_seq_parameters.vcl_hrd_parameters.cpb_removal_delay_length_minus1 + 1;
				dpb_output_len = sps->vui_seq_parameters.vcl_hrd_parameters.dpb_output_delay_length_minus1  + 1;
			}
		}
		if ((sps->vui_seq_parameters.nal_hrd_parameters_present_flag) || (sps->vui_seq_parameters.vcl_hrd_parameters_present_flag)){
			cpb_removal_delay = READ_U(p_Par, cpb_removal_len);
			dpb_output_delay  = READ_U(p_Par, dpb_output_len);
			len += (cpb_removal_len + dpb_output_len);
		}
	}

	if (!sps->vui_parameters_present_flag)
		picture_structure_present_flag = 0;
	else
		picture_structure_present_flag  =  sps->vui_seq_parameters.pic_struct_present_flag;

	if (picture_structure_present_flag){
		picture_structure = READ_U(p_Par, 4);
		len += 4;
		switch (picture_structure)
		{
		case 0:
		case 1:
		case 2:
			NumClockTS = 1;
			break;
		case 3:
		case 4:
		case 7:
			NumClockTS = 2;
			break;
		case 5:
		case 6:
		case 8:
			NumClockTS = 3;
			break;
		default:
			break;
		}
		for (i = 0; i<NumClockTS; i++){
			clock_time_stamp_flag = READ_U(p_Par, 1);
			len += 1;
			if (clock_time_stamp_flag){
				ct_type = READ_U(p_Par, 2);
				nuit_field_based_flag = READ_U(p_Par, 1);
				counting_type = READ_U(p_Par, 5);
				full_timestamp_flag = READ_U(p_Par, 1);
				discontinuity_flag = READ_U(p_Par, 1);
				cnt_dropped_flag = READ_U(p_Par, 1);
				nframes = READ_U(p_Par, 8);
				len += 19;
				if (full_timestamp_flag){
					seconds_value = READ_U(p_Par, 6);
					minutes_value = READ_U(p_Par, 6);
					hours_value = READ_U(p_Par, 5);
					len += 17;
				}	// end of if (full_timestamp_flag)
				else {
					seconds_flag = READ_U(p_Par, 1);
					len += 1;
					if (seconds_flag){
						seconds_value = READ_U(p_Par, 6);
						minutes_flag = READ_U(p_Par, 1);
						len += 7;
						if(minutes_flag){
							minutes_value = READ_U(p_Par, 6);
							hours_flag = READ_U(p_Par, 1);
							len += 7;
							if(hours_flag){
								hours_value = READ_U(p_Par, 5);
								len += 5;
							}	// end of if (hours_falg)
						}	// end of if (minutes_flag)
					}	// end of if (seconds_flag)
				}	// end of else (!full_timestamp_flag)
				{
					int time_offset_length;
					if (sps->vui_seq_parameters.vcl_hrd_parameters_present_flag)
						time_offset_length = sps->vui_seq_parameters.vcl_hrd_parameters.time_offset_length;
					else if (sps->vui_seq_parameters.nal_hrd_parameters_present_flag)
						time_offset_length = sps->vui_seq_parameters.nal_hrd_parameters.time_offset_length;
					else
						time_offset_length = 24;
					if (time_offset_length){
						time_offset = READ_U(p_Par, time_offset_length);
						len += time_offset_length;
						time_offset = -(time_offset & (1<<(time_offset_length-1))) | time_offset;
					}
					else
						time_offset = 0;
				}
			}	// end of (clock_time_stamp_flag)
		}	// end of for
	}	// end of if (picture_structure_present_flag)
	return len;
}

uint32_t interpret_pan_scan_rect_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int pan_scan_rect_cancel_flag;
	int pan_scan_cnt_minus1, i;
	int pan_scan_rect_repetition_period;
	int pan_scan_rect_id, pan_scan_rect_left_offset, pan_scan_rect_right_offset;
	int pan_scan_rect_top_offset, pan_scan_rect_bottom_offset;
	uint32_t len = 0;

	pan_scan_rect_id = READ_UE(p_Par);
	len += ExpGolombLen(pan_scan_rect_id);

	pan_scan_rect_cancel_flag = READ_U(p_Par, 1);
	len += 1;
	if (!pan_scan_rect_cancel_flag){
		pan_scan_cnt_minus1 = READ_UE(p_Par);
		len += ExpGolombLen(pan_scan_cnt_minus1);
		for (i = 0; i<=pan_scan_cnt_minus1; i++){
			pan_scan_rect_left_offset = READ_SE(p_Par);
			pan_scan_rect_right_offset = READ_SE(p_Par);
			pan_scan_rect_top_offset = READ_SE(p_Par);
			pan_scan_rect_bottom_offset = READ_SE(p_Par);
			len += ExpGolombSELen(pan_scan_rect_left_offset);
			len += ExpGolombSELen(pan_scan_rect_right_offset);
			len += ExpGolombSELen(pan_scan_rect_top_offset);
			len += ExpGolombSELen(pan_scan_rect_bottom_offset);
		}
		pan_scan_rect_repetition_period = READ_UE(p_Par);
		len += ExpGolombLen(pan_scan_rect_repetition_period);
	}
	return len;
}

uint32_t interpret_filler_payload_info(DecoderParams *p_Dec, int size)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int payload_cnt = 0;
	byte payload;
	uint32_t len = 0;

	while (payload_cnt<size){
		if ((payload = READ_U(p_Par, 8)) == 0xFF){
			payload_cnt++;
		}
		len += 8;
	}
	return len;
}

uint32_t interpret_user_data_registered_itu_t_t35_info(DecoderParams *p_Dec, int size)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int offset = 0;
	byte itu_t_t35_country_code, itu_t_t35_country_code_extension_byte, payload_byte;

	itu_t_t35_country_code = READ_U(p_Par, 8);
	offset++;
	if(itu_t_t35_country_code == 0xFF){
		itu_t_t35_country_code_extension_byte = READ_U(p_Par, 8);
		offset++;
	}
	while (offset < size){
		payload_byte = READ_U(p_Par, 8);
		offset ++;
	}
	return offset*8;
}

uint32_t interpret_user_data_unregistered_info(DecoderParams *p_Dec, int size)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int offset = 0;
	byte payload_byte;

	for (offset = 0; offset<16; offset++)
		READ_U(p_Par, 8);
	while (offset < size){
		payload_byte = READ_U(p_Par, 8);
		offset ++;
	}
	return offset*8;
}

uint32_t interpret_recovery_point_info(DecoderParams *p_Dec, VideoParams *p_Vid)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int recovery_frame_cnt, exact_match_flag, broken_link_flag, changing_slice_group_idc;
	uint32_t len = 0;

	recovery_frame_cnt = READ_UE(p_Par);
	len = ExpGolombLen(recovery_frame_cnt);
	exact_match_flag = READ_U(p_Par, 1);
	broken_link_flag = READ_U(p_Par, 1);
	changing_slice_group_idc = READ_U(p_Par, 2);

	return len + 3;
	//p_Vid->recovery_point = 1;
	//p_Vid->recovery_frame_cnt = recovery_frame_cnt;
}

extern void dec_ref_pic_marking(DecoderParams *p_Dec, VideoParams *p_Vid, SliceHeader *sh);
uint32_t interpret_dec_ref_pic_marking_repetition_info(DecoderParams *p_Dec, VideoParams *p_Vid)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int original_idr_flag, original_frame_num;
	int original_field_pic_flag, original_bottom_field_flag;
	DecRefPicMarking old_drpm[MAX_MMCO_NUM];
	int old_idr_flag , old_no_output_of_prior_pics_flag, old_long_term_reference_flag , old_adaptive_ref_pic_marking_mode_flag;
	uint32_t len = 0;

	original_idr_flag = READ_U(p_Par, 1);
	len += 1;
	original_frame_num = READ_UE(p_Par);
	len += ExpGolombLen(original_frame_num);

	if (!p_Vid->active_sps->frame_mbs_only_flag){
		original_field_pic_flag = READ_U(p_Par, 1);
		len += 1;
		if (original_field_pic_flag){
			original_bottom_field_flag = READ_U(p_Par, 1);
			len += 1;
		}
	}

	// we need to save everything that is probably overwritten in dec_ref_pic_marking()
	memcpy(old_drpm, p_Vid->drpmb, sizeof(DecRefPicMarking)*MAX_MMCO_NUM);
	old_idr_flag = p_Vid->idr_flag;

	old_no_output_of_prior_pics_flag = p_Vid->slice_hdr->no_output_of_prior_pics_flag;
	old_long_term_reference_flag = p_Vid->slice_hdr->long_term_reference_flag;
	old_adaptive_ref_pic_marking_mode_flag = p_Vid->slice_hdr->adaptive_ref_pic_marking_mode_flag;

	// set new initial values
	p_Vid->idr_flag = original_idr_flag;

	dec_ref_pic_marking(p_Dec, p_Vid, p_Vid->slice_hdr);

	// restore old values in p_Vid
	memcpy(p_Vid->drpmb, old_drpm, sizeof(DecRefPicMarking)*MAX_MMCO_NUM);
	p_Vid->idr_flag = old_idr_flag;
	p_Vid->slice_hdr->no_output_of_prior_pics_flag = old_no_output_of_prior_pics_flag;
	p_Vid->slice_hdr->long_term_reference_flag = old_long_term_reference_flag;
	p_Vid->slice_hdr->adaptive_ref_pic_marking_mode_flag = old_adaptive_ref_pic_marking_mode_flag;
	
	return len;
}

uint32_t interpret_spare_pic(DecoderParams *p_Dec, VideoParams *p_Vid)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int i,j;
	int target_frame_num = 0, target_bottom_field_flag = 0;
	int num_spare_pics, map_unit_cnt, zero_run_length;
	int spare_field_flag = 0, delta_spare_frame_num, spare_bottom_field_flag, spare_area_idc, spare_unit_flag;
	uint32_t PicSizeInMapUnits = p_Vid->PicWidthInMbs * (p_Vid->active_sps->pic_height_in_map_units_minus1+1);
	uint32_t len = 0;

	target_frame_num = READ_UE(p_Par);
	len += ExpGolombLen(target_frame_num);
	spare_field_flag = READ_U(p_Par, 1);
	len += 1;
	if (spare_field_flag){
		target_bottom_field_flag = READ_U(p_Par, 1);
		len += 1;
	}
	num_spare_pics = 1 + READ_UE(p_Par);
	len += ExpGolombLen(num_spare_pics-1);

	for (i=0; i<num_spare_pics; i++){
		delta_spare_frame_num = READ_UE(p_Par);
		len += ExpGolombLen(delta_spare_frame_num);
		if (spare_field_flag){
			spare_bottom_field_flag = READ_U(p_Par, 1);
			len += 1;
		}
		spare_area_idc = READ_UE(p_Par);
		len += ExpGolombLen(spare_area_idc);
		if (spare_area_idc == 1){
			for (j = 0; j<PicSizeInMapUnits; j++){
				spare_unit_flag = READ_U(p_Par, 1);
				len += 1;
			}
		}
		else if (spare_area_idc == 2){
			map_unit_cnt = 0;
			for (j = 0; map_unit_cnt<PicSizeInMapUnits; i++){
				zero_run_length = READ_UE(p_Par);
				len += ExpGolombLen(zero_run_length);
				map_unit_cnt += zero_run_length + 1;
			}
		}
	}
	
	return len;
}

uint32_t interpret_scene_information(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int scene_id, scene_transition_type, second_scene_id, scene_info_present_flag;
	uint32_t len = 0;

	scene_info_present_flag = READ_U(p_Par, 1);
	len += 1;
	if (scene_info_present_flag){
		scene_id = READ_UE(p_Par);
		len += ExpGolombLen(scene_id);
		scene_transition_type = READ_UE(p_Par);
		len += ExpGolombLen(scene_transition_type);
		if (scene_transition_type > 3){
			second_scene_id = READ_UE(p_Par);
			len += ExpGolombLen(second_scene_id);
		}
	}
	
	return len;
}

uint32_t interpret_subsequence_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int sub_seq_layer_num, sub_seq_id, first_ref_pic_flag, leading_non_ref_pic_flag, last_pic_flag, sub_seq_frame_num_flag, sub_seq_frame_num;
	uint32_t len = 0;

	sub_seq_layer_num = READ_UE(p_Par);
	len += ExpGolombLen(sub_seq_layer_num);
	sub_seq_id = READ_UE(p_Par);
	len += ExpGolombLen(sub_seq_id);
	first_ref_pic_flag = READ_U(p_Par, 1);
	leading_non_ref_pic_flag = READ_U(p_Par, 1);
	last_pic_flag = READ_U(p_Par, 1);
	sub_seq_frame_num_flag = READ_U(p_Par, 1);
	len += 4;
	if (sub_seq_frame_num_flag){
		sub_seq_frame_num = READ_UE(p_Par);
		len += ExpGolombLen(sub_seq_frame_num);
	}
	
	return len;
}


uint32_t interpret_subsequence_layer_characteristics_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int num_sub_layers, accurate_statistics_flag, average_bit_rate, average_frame_rate;
	int i;
	uint32_t len = 0;

	num_sub_layers = 1 + READ_UE(p_Par);
	len += ExpGolombLen(num_sub_layers-1);
	for (i=0; i<num_sub_layers; i++){
		accurate_statistics_flag = READ_U(p_Par, 1);
		average_bit_rate = READ_U(p_Par, 16);
		average_frame_rate = READ_U(p_Par, 16);
		len += 33;
	}
	
	return len;
}

uint32_t interpret_subsequence_characteristics_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int accurate_statistics_flag, average_bit_rate, average_frame_rate;
	int sub_seq_layer_num, sub_seq_id, duration_flag, sub_seq_duration, average_rate_flag;
	int num_referenced_subseqs, ref_sub_seq_layer_num, ref_sub_seq_id, ref_sub_seq_direction;
	int i;
	uint32_t len = 0;

	sub_seq_layer_num = READ_UE(p_Par);
	len += ExpGolombLen(sub_seq_layer_num);
	sub_seq_id = READ_UE(p_Par);
	len += ExpGolombLen(sub_seq_id);
	duration_flag = READ_U(p_Par, 1);
	len += 1;
	if (duration_flag){
		sub_seq_duration = READ_U(p_Par, 32);
		len += 32;
	}

	average_rate_flag = READ_U(p_Par, 1);
	len += 1;
	if (average_rate_flag){
		accurate_statistics_flag = READ_U(p_Par, 1);
		average_bit_rate = READ_U(p_Par, 16);
		average_frame_rate = READ_U(p_Par, 16);
		len += 33;
	}
	num_referenced_subseqs = READ_UE(p_Par);
	len += ExpGolombLen(num_referenced_subseqs);
	for (i = 0; i<num_referenced_subseqs; i++){
		ref_sub_seq_layer_num = READ_UE(p_Par);
		len += ExpGolombLen(ref_sub_seq_layer_num);
		ref_sub_seq_id = READ_UE(p_Par);
		len += ExpGolombLen(ref_sub_seq_id);
		ref_sub_seq_direction = READ_U(p_Par, 1);
		len += 1;
	}
	
	return len;
}

uint32_t interpret_full_frame_freeze_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int full_frame_freeze_repetition_period;
	full_frame_freeze_repetition_period = READ_UE(p_Par);
	return ExpGolombLen(full_frame_freeze_repetition_period);
}

uint32_t interpret_full_frame_snapshot_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int snapshot_id;
	snapshot_id = READ_UE(p_Par);
	return ExpGolombLen(snapshot_id);
}

uint32_t interpret_progressive_refinement_start_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int progressive_refinement_id, num_refinement_steps_minus1;
	progressive_refinement_id = READ_UE(p_Par);
	num_refinement_steps_minus1 = READ_UE(p_Par);
	return (ExpGolombLen(progressive_refinement_id) + ExpGolombLen(num_refinement_steps_minus1));
}

uint32_t interpret_progressive_refinement_end_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int progressive_refinement_id;
	progressive_refinement_id = READ_UE(p_Par);
	return ExpGolombLen(progressive_refinement_id);
}

uint32_t interpret_motion_constrained_slice_group_set_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int num_slice_groups_minus1, slice_group_id, exact_match_flag, pan_scan_rect_flag, pan_scan_rect_id;
	int i;
	int sliceGroupSize;
	uint32_t len = 0;

	num_slice_groups_minus1 = READ_UE(p_Par);
	len += ExpGolombLen(num_slice_groups_minus1);
	sliceGroupSize = CeilLog2(num_slice_groups_minus1 + 1);

	for (i=0; i<=num_slice_groups_minus1; i++){
		slice_group_id = READ_U(p_Par, sliceGroupSize);
		len += sliceGroupSize;
	}

	exact_match_flag = READ_U(p_Par, 1);
	pan_scan_rect_flag = READ_U(p_Par, 1);
	len += 2;
	if (pan_scan_rect_flag){
		pan_scan_rect_id = READ_UE(p_Par);
		len += ExpGolombLen(pan_scan_rect_id);
	}
	
	return len;
}

uint32_t interpret_film_grain_characteristics_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int film_grain_characteristics_cancel_flag;
	int model_id, separate_colour_description_present_flag;
	int film_grain_bit_depth_luma_minus8, film_grain_bit_depth_chroma_minus8, film_grain_full_range_flag, film_grain_colour_primaries, film_grain_transfer_characteristics, film_grain_matrix_coefficients;
	int blending_mode_id, log2_scale_factor, comp_model_present_flag[3];
	int num_intensity_intervals_minus1, num_model_values_minus1;
	int intensity_interval_lower_bound, intensity_interval_upper_bound;
	int comp_model_value;
	int film_grain_characteristics_repetition_period;
	uint32_t len = 0;

	int c, i, j;

	film_grain_characteristics_cancel_flag = READ_U(p_Par, 1);
	len += 1;
	if(!film_grain_characteristics_cancel_flag){
		model_id = READ_U(p_Par, 2);
		separate_colour_description_present_flag = READ_U(p_Par, 2);
		len += 4;
		if (separate_colour_description_present_flag){
			film_grain_bit_depth_luma_minus8 = READ_U(p_Par, 3);
			film_grain_bit_depth_chroma_minus8 = READ_U(p_Par, 3);
			film_grain_full_range_flag = READ_U(p_Par, 1);
			film_grain_colour_primaries = READ_U(p_Par, 8);
			film_grain_transfer_characteristics = READ_U(p_Par, 8);
			film_grain_matrix_coefficients = READ_U(p_Par, 8);
			len += 31;
		}
		blending_mode_id = READ_U(p_Par, 2);
		log2_scale_factor = READ_U(p_Par, 4);
		len += 6;
		for (c = 0; c < 3; c ++){
			comp_model_present_flag[c] = READ_U(p_Par, 1);
			len += 1;
		}
		for (c = 0; c < 3; c ++){
			if (comp_model_present_flag[c]){
				num_intensity_intervals_minus1 = READ_U(p_Par, 8);
				num_model_values_minus1 = READ_U(p_Par, 3);
				len += 11;

				for (i = 0; i<=num_intensity_intervals_minus1; i ++){
					intensity_interval_lower_bound = READ_U(p_Par, 8);
					intensity_interval_upper_bound = READ_U(p_Par, 8);
					len += 16;
					for (j = 0; j <= num_model_values_minus1; j++){
						comp_model_value = READ_SE(p_Par);
						len += ExpGolombSELen(comp_model_value);
					}
				}
			}
		}
		film_grain_characteristics_repetition_period = READ_UE(p_Par);
		len += ExpGolombSELen(film_grain_characteristics_repetition_period);
	}
	
	return len;
}

uint32_t interpret_deblocking_filter_display_preference_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int deblocking_display_preference_cancel_flag;
	int display_prior_to_deblocking_preferred_flag, dec_frame_buffering_constraint_flag, deblocking_display_preference_repetition_period;
	uint32_t len = 0;

	deblocking_display_preference_cancel_flag = READ_U(p_Par, 1);
	len += 1;
	if(!deblocking_display_preference_cancel_flag){
		display_prior_to_deblocking_preferred_flag = READ_U(p_Par, 1);
		dec_frame_buffering_constraint_flag = READ_U(p_Par, 1);
		deblocking_display_preference_repetition_period = READ_UE(p_Par);
		len += (ExpGolombLen(deblocking_display_preference_repetition_period) + 2);
	}
	
	return len;
}

uint32_t interpret_stereo_video_info_info(DecoderParams *p_Dec)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int field_views_flags;
	int top_field_is_left_view_flag, current_frame_is_left_view_flag, next_frame_is_second_view_flag;
	int left_view_self_contained_flag;
	int right_view_self_contained_flag;
	uint32_t len = 0;

	field_views_flags = READ_U(p_Par, 1);
	len += 1;
	if (field_views_flags){
		top_field_is_left_view_flag = READ_U(p_Par, 1);
		len += 1;
	}
	else {
		current_frame_is_left_view_flag = READ_U(p_Par, 1);
		next_frame_is_second_view_flag = READ_U(p_Par, 1);
		len += 2;
	}
	left_view_self_contained_flag = READ_U(p_Par, 1);
	right_view_self_contained_flag = READ_U(p_Par, 1);
	len += 2;
	
	return len;
}

uint32_t interpret_reserved_info(DecoderParams *p_Dec, int size)
{
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int offset = 0;
	byte payload_byte;

	while (offset < size){
		payload_byte = READ_U(p_Par, 8);
		offset ++;
	}
	return offset*8;
}
int sei_payload(void *ptDecHandle, void *ptVid, int payload_type, int payload_size)
{
	DecoderParams *p_Dec = (DecoderParams*)ptDecHandle;
	VideoParams *p_Vid = (VideoParams*)ptVid;
	uint32_t len = 0;
	switch (payload_type)
	{
	case SEI_BUFFERING_PERIOD:
		len = interpret_buffering_period_info(p_Dec, p_Vid);
		break;
	case SEI_PIC_TIMING:
		len = interpret_picture_timing_info(p_Dec, p_Vid);
		break;
	case SEI_PAN_SCAN_RECT:
		len = interpret_pan_scan_rect_info(p_Dec);
		break;
	case SEI_FILLER_PAYLOAD:
		len = interpret_filler_payload_info(p_Dec, payload_size);
		break;
	case SEI_USER_DATA_REGISTERED_ITU_T_T35:
		len = interpret_user_data_registered_itu_t_t35_info(p_Dec, payload_size);
		break;
	case SEI_USER_DATA_UNREGISTERED:
		len = interpret_user_data_unregistered_info(p_Dec, payload_size);
		break;
	case SEI_RECOVERY_POINT:
		len = interpret_recovery_point_info(p_Dec, p_Vid);
		break;
	case SEI_DEC_REF_PIC_MARKING_REPETITION:
		len = interpret_dec_ref_pic_marking_repetition_info(p_Dec, p_Vid);
		break;
	case SEI_SPARE_PIC:
		len = interpret_spare_pic(p_Dec, p_Vid);
		break;
	case SEI_SCENE_INFO:
		len = interpret_scene_information(p_Dec);
		break;
	case SEI_SUB_SEQ_INFO:
		len = interpret_subsequence_info(p_Dec);
		break;
	case SEI_SUB_SEQ_LAYER_CHARACTERISTICS:
		len = interpret_subsequence_layer_characteristics_info(p_Dec);
		break;
	case SEI_SUB_SEQ_CHARACTERISTICS:
		len = interpret_subsequence_characteristics_info(p_Dec);
		break;
	case SEI_FULL_FRAME_FREEZE:
		len = interpret_full_frame_freeze_info(p_Dec);
		break;
	case SEI_FULL_FRAME_FREEZE_RELEASE:
		break;
	case SEI_FULL_FRAME_SNAPSHOT:
		len = interpret_full_frame_snapshot_info(p_Dec);
		break;
	case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START:
		len = interpret_progressive_refinement_start_info(p_Dec);
		break;
	case SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END:
		len = interpret_progressive_refinement_end_info(p_Dec);
		break;
	case SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET:
		len = interpret_motion_constrained_slice_group_set_info(p_Dec);
	case SEI_FILM_GRAIN_CHARACTERISTICS:
		len += interpret_film_grain_characteristics_info (p_Dec);
		break;
	case SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE:
		len = interpret_deblocking_filter_display_preference_info(p_Dec);
		break;
	case SEI_STEREO_VIDEO_INFO:
		len = interpret_stereo_video_info_info(p_Dec);
		break;
	default:
		len = interpret_reserved_info(p_Dec, payload_size);
		break;
	}
	return len;
}
