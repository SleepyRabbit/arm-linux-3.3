#ifdef LINUX
	#include <linux/kernel.h>
#endif
#include "global.h"
#include "params.h"
#include "vlc.h"
#include "define.h"
#include "../debug.h"

typedef struct {
    unsigned int w;
    unsigned int h;
    unsigned int idc;
} VUI_Aspect_Ratio;

extern int getDpbSize(SeqParameterSet *sps);

static const byte ZZ_SCAN[16]  =
{  0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

static const byte ZZ_SCAN8[64] =
{  0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

extern int paking_idx;

static unsigned iCeilLog2( unsigned uiVal)
{
	unsigned uiTmp = uiVal-1;
	unsigned uiRet = 0;
	
	if (uiVal == 0)
	    return 0;

	while( uiTmp != 0 ) {
		uiTmp >>= 1;
		uiRet++;
	}
	return uiRet;
}

void prepare_vui(VUI_Parameters *vui, FAVC_ENC_VUI_PARAM *p_VUIParam)
{
#if 1
    if (0 == p_VUIParam->u32SarWidth && 0 == p_VUIParam->u32SarHeight)
        vui->aspect_ratio_info_present_flag = 0;
    else {
        int i;
        VUI_Aspect_Ratio sar[14] = {
            { 1,   1, 1 }, { 12, 11, 2 }, { 10, 11, 3 }, { 16, 11, 4 },
            { 40, 33, 5 }, { 24, 11, 6 }, { 20, 11, 7 }, { 32, 11, 8 },
            { 80, 33, 9 }, { 18, 11, 10}, { 15, 11, 11}, { 64, 33, 12},
            { 160,99, 13}, { 0, 0, -1 }
        };
        vui->aspect_ratio_info_present_flag = 1;
        for (i = 0; sar[i].idc != -1; i++) {
            if (sar[i].w == p_VUIParam->u32SarWidth && sar[i].h == p_VUIParam->u32SarHeight)
                break;
        }
        if (sar[i].idc != -1)
            vui->aspect_ratio_idc = sar[i].idc;
        else {
            vui->aspect_ratio_idc = 255;
            vui->sar_width = p_VUIParam->u32SarWidth;
            vui->sar_height = p_VUIParam->u32SarHeight;
        }
    }
#else
	if (p_VUIParam->u8AspectRatioIdc == 0)
		vui->aspect_ratio_info_present_flag = 0;
	else {
		vui->aspect_ratio_info_present_flag = 1;
		vui->aspect_ratio_idc = p_VUIParam->u8AspectRatioIdc;
		if (vui->aspect_ratio_idc == EXTENDED_SAR) {
			vui->sar_width = p_VUIParam->u32SarWidth;
			vui->sar_height = p_VUIParam->u32SarHeight;
		}
	}
#endif
	vui->overscan_info_present_flag = 0;
	if (p_VUIParam->u8VideoFormat == 5 && p_VUIParam->u8ColourPrimaries == 2 && \
		p_VUIParam->u8TransferCharacteristics == 2 && p_VUIParam->u8MatrixCoefficients == 2 && \
		p_VUIParam->bVideoFullRangeFlag == 0)
		vui->video_signal_type_present_flag = 0;
	else {
        vui->video_signal_type_present_flag = 1;
		vui->video_format = p_VUIParam->u8VideoFormat;
		vui->video_full_range_flag = p_VUIParam->bVideoFullRangeFlag;
		if (p_VUIParam->u8ColourPrimaries == 2 && p_VUIParam->u8TransferCharacteristics == 2 && p_VUIParam->u8MatrixCoefficients == 2) 
			vui->colour_description_present_flag = 0;
		else {
			vui->colour_description_present_flag = 1;
			vui->colour_primaries = p_VUIParam->u8ColourPrimaries;
			vui->transfer_characteristics = p_VUIParam->u8TransferCharacteristics;
			vui->matrix_coefficients = p_VUIParam->u8MatrixCoefficients;
		}
	}
	if (p_VUIParam->u8ChromaSampleLocTypeTopField == 0 && p_VUIParam->u8ChromaSampleLocTypeBottomField == 0)
        vui->chroma_loc_info_present_flag = 0;
    else {
		vui->chroma_loc_info_present_flag = 1;
		vui->chroma_sample_loc_type_top_field = p_VUIParam->u8ChromaSampleLocTypeTopField;
		vui->chroma_sample_loc_type_bottom_field = p_VUIParam->u8ChromaSampleLocTypeBottomField;
	}
    if (0 == p_VUIParam->u32NumUnitsInTick || 0 == p_VUIParam->u32TimeScale)
    	vui->timing_info_present_flag = 0;
    else {
        vui->timing_info_present_flag = 1;
        vui->num_units_in_tick = p_VUIParam->u32NumUnitsInTick;
        vui->time_scale = p_VUIParam->u32TimeScale * 2;
    }
	vui->nal_hrd_parameters_present_flag = 0;
	vui->vcl_hrd_parameters_present_flag = 0;
	vui->pic_struct_present_flag = 0;
	vui->bitstream_restriction_flag = 0;	
}

static void ScalingList(int *inScale, int *outScale, int *deltaScale, byte *useDefault, int size)
{
	int j, scanj;
	int lastScale, nextScale;

	lastScale = 8;
	nextScale = 8;

	for(j = 0; j < size; j++)
	{
		scanj = (size==16) ? ZZ_SCAN[j] : ZZ_SCAN8[j];
		if(nextScale!=0)
		{
			deltaScale[j] = inScale[scanj] - lastScale;	// Calculate delta from the scalingList data from the input file
			if (deltaScale[j] > 127)
				deltaScale[j] -= 256;
			else if (deltaScale[j] < -128)
				deltaScale[j] += 256;

			nextScale = inScale[scanj];
			*useDefault |= (scanj==0 && nextScale==0);	// Check first matrix value for zero
		}
		else
			deltaScale[j] = DISBALE_DELTA_SCALE;
		outScale[scanj] = (short)((nextScale==0) ? lastScale : nextScale);	// Update the actual scalingList matrix with the correct values
		lastScale = outScale[scanj];
	}
}

int prepare_sps(void *ptVid, SeqParameterSet *p_SPS, FAVC_ENC_PARAM *pParam, int sps_id)
{
	VideoParams *p_Vid = (VideoParams *)ptVid;
    //int mb_count = pParam->u32SourceHeight * pParam->u32SourceWidth / 256;
	int i;
	/*	720p: level 21: max_dpb = 1
	 *	      level 22: max_dpb = 2
	 *	      level 31: max_dpb = 4
	 *	1080p: level 31: max_dpb = 2
	 *	       level 40: max_dpb = 3 */
	/*
	p_SPS->profile_idc = FREXT_HP;
    p_SPS->level_idc = 40;
    */
    p_SPS->profile_idc = pParam->u32Profile;
    /*
    set profile level when actually encode resolution done
#ifndef SAME_AS_CMODEL
    if (mb_count <= 1620)
        p_SPS->level_idc = 30;
    else if (mb_count <= 8192)
        p_SPS->level_idc = 40;
    else
        p_SPS->level_idc = 51;
#else
    if (mb_count <= 8192)
        p_SPS->level_idc = 40;
    else
        p_SPS->level_idc = 51;
#endif
    */

	//if (pParam->u8NumRefFrames > MAX_NUM_REF_FRAMES)      // check in check_param
	//	return H264E_ERR_INPUT_PARAM;
	p_Vid->p_ref1_field_temporal = 0;
	/* if reference number = 1 & GOP contains B frames & field coding & useing direct temporal mode
	 * P use one reference frame, and B use two referece frames
	 * so the refernece nubmer must be two */
    /*	if GOP has B frame, p_ref_num = 1, field coding, and temporal direct
	 *	bottom P slice: ref0 num = 2
	 *	    TopP0      TopB      TopP1
	 *	    BotP0      BotB      BotP1 (l0: TopP1 ==> BotP0, TopP1)
	 *	           (temporal direct: BotP0 & BotP1) */
	if (pParam->u8BFrameNumber && pParam->u8NumRefFrames == 1) {
		if (pParam->bFieldCoding && !pParam->bDirectSpatialMVPred)
			p_Vid->p_ref1_field_temporal = 1;
		pParam->u8NumRefFrames = 2;
		p_Vid->ref_l0_num = 1;
	}
	else 
		p_Vid->ref_l0_num = pParam->u8NumRefFrames;
	// add warnning message
	p_SPS->num_ref_frames = pParam->u8NumRefFrames;
	p_SPS->seq_parameter_set_id = sps_id;
	
	if (pParam->bChromaFormat)
		p_SPS->chroma_format_idc = FORMAT420;	// 4:2:0
	else
		p_SPS->chroma_format_idc = FORMAT400;	// 4:0:0

	// scale list
	if (pParam->bScalingMatrixFlag)
		p_SPS->seq_scaling_matrix_present_flag = 1;
	else
		p_SPS->seq_scaling_matrix_present_flag = 0;
	//for (i = 0; i<8; i++)
	//	p_SPS->seq_scaling_matrix_present_flag |= pParam->bScaleListPresent[i];
	if (p_SPS->seq_scaling_matrix_present_flag && pParam->pScaleList != NULL) {
		for (i = 0; i<8; i++) {
			//p_SPS->seq_scaling_list_present_flag[i] = p_SPS->seq_scaling_matrix_present_flag;
			//p_SPS->seq_scaling_list_present_flag[i] = pParam->mScaleList.bScalingListFlag[i];
			p_SPS->seq_scaling_list_present_flag[i] = pParam->pScaleList->bScalingListFlag[i];
			if (p_SPS->seq_scaling_list_present_flag[i]) {
				if (i<6)
					//ScalingList(pParam->mScaleList.i32ScaleList4x4[i], p_SPS->ScalingList4x4[i], p_SPS->delta_scale4x4[i], &p_SPS->UseDefaultScalingMatrix4x4Flag[i], 16);
					ScalingList(pParam->pScaleList->i32ScaleList4x4[i], p_SPS->ScalingList4x4[i], p_SPS->delta_scale4x4[i], &p_SPS->UseDefaultScalingMatrix4x4Flag[i], 16);
				else
					//ScalingList(pParam->mScaleList.i32ScaleList8x8[i-6], p_SPS->ScalingList8x8[i-6], p_SPS->delta_scale8x8[i-6], &p_SPS->UseDefaultScalingMatrix8x8Flag[i-6], 64);
					ScalingList(pParam->pScaleList->i32ScaleList8x8[i-6], p_SPS->ScalingList8x8[i-6], p_SPS->delta_scale8x8[i-6], &p_SPS->UseDefaultScalingMatrix8x8Flag[i-6], 64);
			}
		}
        //printk("transform 8x8: %d\n", pParam->bTransform8x8Mode);
        if (0 == pParam->bTransform8x8Mode) {
            p_SPS->seq_scaling_list_present_flag[6] = p_SPS->seq_scaling_list_present_flag[7] = 0;            
            //printk("disable scale 8x8\n");
        }
	}

	//p_SPS->log2_max_frame_num_minus4 = iClip3(4, 16, pParam->u8Log2MaxFrameNum) - 4;
	p_SPS->log2_max_frame_num_minus4 = pParam->u8Log2MaxFrameNum - 4;   // clip is done in check_param

#if 1
	if (1<<pParam->u8Log2MaxPOCLsb < (pParam->u8BFrameNumber+1)*4)
		pParam->u8Log2MaxPOCLsb = iCeilLog2((pParam->u8BFrameNumber+1)*4);
#else   // JM
    if (1<<pParam->u8Log2MaxPOCLsb < (pParam->u8BFrameNumber+1)*4)
        pParam->u8Log2MaxPOCLsb = iCeilLog2(pParam->u8FrameNumber)+3;
#endif
	//p_SPS->log2_max_pic_order_cnt_lsb_minus4 = iClip3(4, 16, pParam->u8Log2MaxPOCLsb) - 4;
	p_SPS->log2_max_pic_order_cnt_lsb_minus4 = pParam->u8Log2MaxPOCLsb - 4; // clip in done in check_param
	p_SPS->pic_order_cnt_type = 0;

    // image information
    p_Vid->field_pic_flag = pParam->bFieldCoding;
/*
	if (pParam->bFieldCoding) {
		p_Vid->field_pic_flag = 1;
		if (pParam->u32SourceHeight & 0x1F) {
			return H264E_ERR_INPUT_PARAM;
        }
	}
	else {
		p_Vid->field_pic_flag = 0;
		if (pParam->u32SourceWidth & 0x0F)
			return H264E_ERR_INPUT_PARAM;
	}
*/
	p_Vid->source_height = pParam->u32SourceHeight;
	p_Vid->source_width = pParam->u32SourceWidth;
	if (pParam->u32SourceHeight == 0 || pParam->u32SourceWidth == 0) {
        favce_err("encode input source dimension is zero %d x %d\n", pParam->u32SourceHeight, pParam->u32SourceWidth);
		return H264E_ERR_INPUT_PARAM;
    }
	// roi
	p_Vid->roi_x = pParam->u32ROI_X;
	p_Vid->roi_y = pParam->u32ROI_Y;
	p_Vid->roi_width = (pParam->u32ROI_Width == 0 ? p_Vid->source_width : pParam->u32ROI_Width);
	p_Vid->roi_height = (pParam->u32ROI_Height == 0 ? p_Vid->source_height : pParam->u32ROI_Height);
	if (p_Vid->roi_width != p_Vid->source_width || p_Vid->roi_height != p_Vid->source_height)
		p_Vid->roi_flag = 1;
	else
		p_Vid->roi_flag = 0;

    /* constraint: (1) min support width & height (max size is check in ioctl)
     *             (2) encoded frame size must be align to macroblock 
     *             (3) encoded frame can not exceed source image 
     *             (4) input address of source buffer must be 8-byte align          */
	if (p_Vid->roi_width<SUPPORT_MIN_WIDTH || p_Vid->roi_height<SUPPORT_MIN_HEIGHT) {
	    favce_err("encode frame (%dx%d) is smaller than hardware support (%dx%d)\n", p_Vid->roi_width, p_Vid->roi_height, SUPPORT_MIN_WIDTH, SUPPORT_MIN_HEIGHT);
		return H264E_ERR_INPUT_PARAM;
    }
    if (p_Vid->roi_width & 0x0F) {
        favce_err("encode width (%d) must be multiple of 16\n", p_Vid->roi_width);
        return H264E_ERR_INPUT_PARAM;
    }
    if (p_Vid->field_pic_flag && (p_Vid->roi_height & 0x1F)) {
        favce_err("when field coding, encode height (%d) must be multiple of 32\n", p_Vid->roi_width);
        return H264E_ERR_INPUT_PARAM;
    }
    else if (p_Vid->roi_height & 0x0F) {
        favce_err("encode height (%d) must be multiple of 16\n", p_Vid->roi_width);
        return H264E_ERR_INPUT_PARAM;
    }
    if (p_Vid->roi_x + p_Vid->roi_width > p_Vid->source_width) {
	    favce_err("roi width is exceed source image, src width = %d, roi width = %d, roi x = %d\n",
            p_Vid->source_width, p_Vid->roi_width, p_Vid->roi_x);
		return H264E_ERR_INPUT_PARAM;
    }
    if (p_Vid->roi_y + p_Vid->roi_height > p_Vid->source_height) {
	    favce_err("roi height is exceed source image, src = %d, roi height = %d, roi y %d\n",
            p_Vid->source_height, p_Vid->roi_height, p_Vid->roi_y);
		return H264E_ERR_INPUT_PARAM;
    }

    if (p_Vid->roi_x & 0x03) {
        favce_err("x position (%d) of roi must be multiple of 4\n", p_Vid->roi_x);
        return H264E_ERR_INPUT_PARAM;
    }

    if (0 == pParam->bSrcImageType && (p_Vid->roi_y & 0x01)) {
        favce_err("y position of roi must be multiple of 2 when src_img_type = 0\n");
        return H264E_ERR_INPUT_PARAM;
    }

	p_Vid->frame_width = p_Vid->roi_width;
	p_Vid->frame_height = p_Vid->roi_height;
	p_Vid->total_mb_number = p_Vid->frame_width*p_Vid->frame_height/256;
	p_Vid->mb_row = p_Vid->frame_width/16;
#ifndef SAME_AS_CMODEL
    if (pParam->u32LevelIdc > 0)
        p_SPS->level_idc = pParam->u32LevelIdc;
    else {
        if (p_Vid->total_mb_number <= 1620)
            p_SPS->level_idc = 30;
        else if (p_Vid->total_mb_number <= 8192)
            p_SPS->level_idc = 40;
        else
            p_SPS->level_idc = 51;
    }
#else
    if (p_Vid->total_mb_number <= 8192)
        p_SPS->level_idc = 40;
    else
        p_SPS->level_idc = 51;
#endif    

//showValue(p_Vid->field_pic_flag);
	if (p_Vid->field_pic_flag) {
		p_SPS->frame_mbs_only_flag = 0;
		//showValue(0x77770000);
	}
	else {
		p_SPS->frame_mbs_only_flag = 1;
		//showValue(0x77771111);
	}
	p_SPS->pic_width_in_mbs_minus1 = (p_Vid->frame_width>>4) - 1;
	p_SPS->pic_height_in_map_units_minus1 = (p_Vid->frame_height>>4)/(2-p_SPS->frame_mbs_only_flag) - 1;
	p_SPS->direct_8x8_inference_flag = 1;//pParam->bDirect8x8;

	p_Vid->p_Dpb->max_size = getDpbSize(p_SPS);
    // Tuba 20150316: not check dbg buffer
    /*
	if (p_SPS->num_ref_frames > p_Vid->p_Dpb->max_size) {
	    favce_err("number of reference frames is larger then max dpb size (%d > %d)\n", p_SPS->num_ref_frames, p_Vid->p_Dpb->max_size);
		return H264E_ERR_INPUT_PARAM;
	}
	*/
	if (pParam->u32CroppingLeft || pParam->u32CroppingRight || pParam->u32CroppingTop || pParam->u32CroppingBottom) {
	    p_SPS->frame_cropping_flag = 1;
	    p_SPS->frame_cropping_rect_left_offset = pParam->u32CroppingLeft;
	    p_SPS->frame_cropping_rect_right_offset = pParam->u32CroppingRight;
	    p_SPS->frame_cropping_rect_top_offset = pParam->u32CroppingTop;
	    p_SPS->frame_cropping_rect_bottom_offset = pParam->u32CroppingBottom;
    }
    else {
        p_SPS->frame_cropping_flag = 0;
    }

	return H264E_OK;
}

int prepare_pps(void *ptVid, PicParameterSet *p_PPS, FAVC_ENC_PARAM *pParam, int pps_id)
{
	VideoParams *p_Vid = (VideoParams *)ptVid;
	//SeqParameterSet *p_SPS = p_Vid->active_sps;

	p_PPS->pic_parameter_set_id = pps_id;
	p_PPS->seq_parameter_set_id = p_Vid->active_sps->seq_parameter_set_id;
	p_PPS->entropy_coding_mode_flag = pParam->bEntropyCoding;

	if (pParam->bFieldCoding)
		p_PPS->pic_order_present_flag = 1;
	else
		p_PPS->pic_order_present_flag = 0;
	//p_PPS->num_slice_groups_minus1 = 0;
	
	p_PPS->num_ref_idx_l0_active_minus1 = (p_Vid->ref_l0_num == 0 ? 0 : p_Vid->ref_l0_num-1);
	p_PPS->num_ref_idx_l1_active_minus1 = 0;

	p_PPS->weighted_pred_flag = 0;
	p_PPS->weighted_bipred_idc = 0;
	//p_PPS->pic_init_qp_minus26 = pParam->u8InitialQP - 26;
	p_PPS->pic_init_qp_minus26 = 0;
	//p_PPS->pic_init_qs_minus26 = 0;
	p_PPS->chroma_qp_index_offset = pParam->i8ChromaQPOffset0;

	if (pParam->i8DisableDBFIdc < 0)
		p_PPS->deblocking_filter_control_present_flag = 0;
	else {
		p_Vid->currSlice->disable_deblocking_filter_idc = pParam->i8DisableDBFIdc;
		p_Vid->currSlice->slice_alpha_c0_offset_div2 = pParam->i8DBFAlphaOffset>>1;
		p_Vid->currSlice->slice_beta_offset_div2 = pParam->i8DBFBetaOffset>>1;
		p_PPS->deblocking_filter_control_present_flag = 1;
	}
	//p_PPS->constrained_intra_pred_flag;
	//p_PPS->redundant_pic_cnt_present_flag;
	p_PPS->transform_8x8_mode_flag = pParam->bTransform8x8Mode;
	p_PPS->pic_scaling_matrix_present_flag = 0;
	p_PPS->second_chroma_qp_index_offset = pParam->i8ChromaQPOffset1;

	return H264E_OK;
}

/*
static void write_hrd(EncoderParams *p_Enc, HRD_Parameters *hrd)
{
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	int SchedSelIdx;
	ue_v(ptReg_cmd, hrd->cpb_cnt_minus1);
	u_v(ptReg_cmd, hrd->bit_rate_scale, 4);
	u_v(ptReg_cmd, hrd->cpb_size_scale, 4);
	for (SchedSelIdx = 0; SchedSelIdx<=hrd->cpb_cnt_minus1; SchedSelIdx++) {
		ue_v(ptReg_cmd, hrd->bit_rate_value_minus1[SchedSelIdx]);
		ue_v(ptReg_cmd, hrd->cpb_size_value_minus1[SchedSelIdx]);
		u_v(ptReg_cmd, hrd->cbr_flag[SchedSelIdx], 1);
	}
	u_v(ptReg_cmd, hrd->initial_cpb_removal_delay_length_minus1, 5);
	u_v(ptReg_cmd, hrd->cpb_removal_delay_length_minus1, 5);
	u_v(ptReg_cmd, hrd->dpb_output_delay_length_minus1, 5);
	u_v(ptReg_cmd, hrd->time_offset_length, 5);
}
*/

static int write_vui(EncoderParams *p_Enc, VUI_Parameters *vui)
{
#ifdef AUTO_PAKING_HEADER
    volatile H264Reg_Mem *ptReg = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	//volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#else
    volatile H264Reg_Cmd *ptReg = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#endif

    paking_idx = 0;
	u_v(ptReg, vui->aspect_ratio_info_present_flag, 1);
	if (vui->aspect_ratio_info_present_flag) {
		u_v(ptReg, vui->aspect_ratio_idc, 8);
		if (vui->aspect_ratio_idc == EXTENDED_SAR) {
			u_v(ptReg, vui->sar_width, 16);
			u_v(ptReg, vui->sar_height, 16);
		}
	}
	u_v(ptReg, vui->overscan_info_present_flag, 1);
	if (vui->overscan_info_present_flag)
		u_v(ptReg, vui->overscan_appropriate_flag, 1);
	u_v(ptReg, vui->video_signal_type_present_flag, 1);
	if (vui->video_signal_type_present_flag) {
		u_v(ptReg, vui->video_format, 3);
		u_v(ptReg, vui->video_full_range_flag, 1);
		u_v(ptReg, vui->colour_description_present_flag, 1);
		if (vui->colour_description_present_flag) {
			u_v(ptReg, vui->colour_primaries, 8);
			u_v(ptReg, vui->transfer_characteristics, 8);
			u_v(ptReg, vui->matrix_coefficients, 8);
		}
	}
	u_v(ptReg, vui->chroma_loc_info_present_flag, 1);
	if (vui->chroma_loc_info_present_flag) {
		ue_v(ptReg, vui->chroma_sample_loc_type_top_field);
		ue_v(ptReg, vui->chroma_sample_loc_type_bottom_field);
	}
	u_v(ptReg, vui->timing_info_present_flag, 1);   // always zero
	if (vui->timing_info_present_flag) {
		u_v(ptReg, vui->num_units_in_tick, 32);
		u_v(ptReg, vui->time_scale, 32);
		u_v(ptReg, vui->fixed_frame_rate_flag, 1);
	}
	u_v(ptReg, vui->nal_hrd_parameters_present_flag, 1);    // always zero
//	if (vui->nal_hrd_parameters_present_flag)
//		write_hrd(p_Enc, &vui->nal_hrd_parameters);
	u_v(ptReg, vui->vcl_hrd_parameters_present_flag, 1);    // always zero
//	if (vui->vcl_hrd_parameters_present_flag)
//		write_hrd(p_Enc, &vui->vcl_hrd_parameters);
	if (vui->nal_hrd_parameters_present_flag || vui->vcl_hrd_parameters_present_flag)
		u_v(ptReg, vui->low_delay_hrd_flag, 1);
	u_v(ptReg, vui->pic_struct_present_flag, 1);    // always zero
	u_v(ptReg, vui->bitstream_restriction_flag, 1); // always zero
	if (vui->bitstream_restriction_flag) {
		u_v(ptReg, vui->motion_vectors_over_pic_boundaries_flag, 1);
		ue_v(ptReg, vui->max_bytes_per_pic_denom);
		ue_v(ptReg, vui->max_bits_per_mb_denom);
		ue_v(ptReg, vui->log2_max_mv_length_horizontal);
		ue_v(ptReg, vui->log2_max_mv_length_vertical);
		ue_v(ptReg, vui->num_reorder_frames);
		ue_v(ptReg, vui->max_dec_frame_buffering);
	}
/*
#ifdef AUTO_PAKING_HEADER
	if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
        return H264E_ERR_POLLING_TIMEOUT;
    paking_idx = 0;
#endif
*/
	return H264E_OK;
}

int write_sps(void *ptEncHandle, SeqParameterSet *sps)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
#ifdef AUTO_PAKING_HEADER
    volatile H264Reg_Mem *ptReg = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#else
    volatile H264Reg_Cmd *ptReg = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#endif
	unsigned i;
	int j;

    paking_idx = 0;
	// nalu
	u_v_prev(ptReg, 0x00000001, 32);	// start code
	u_v_prev(ptReg, 0x67, 8);	// SPS

	u_v(ptReg, sps->profile_idc, 8);
	u_v(ptReg, 0, 8);
	u_v(ptReg, sps->level_idc, 8);

	ue_v(ptReg, sps->seq_parameter_set_id);
	if (sps->profile_idc == FREXT_HP || sps->profile_idc == FREXT_Hi10P || \
		sps->profile_idc == FREXT_Hi422 || sps->profile_idc == FREXT_Hi444) {
		ue_v(ptReg, sps->chroma_format_idc);
		if (sps->chroma_format_idc == 3)    // chroma_format_idc is always 0 or 1
			u_v(ptReg, 0, 1);               // sps->residual_colour_transform_flag
		ue_v(ptReg, 0);                     // sps->bit_depth_luma_minus8
		ue_v(ptReg, 0);                     // sps->bit_depth_chroma_minus8
		u_v(ptReg, 0, 1);                   // qpprime_y_zero_transform_bypass_flag
		u_v(ptReg, sps->seq_scaling_matrix_present_flag, 1);
		if (sps->seq_scaling_matrix_present_flag) {
//printk("scaling matrix: %d\n", paking_idx*4);
			for (i = 0; i<6; i++) {
                u_v(ptReg, sps->seq_scaling_list_present_flag[i], 1);
            #ifdef AUTO_PAKING_HEADER
                if (paking_idx == 64) {
                    if (paking_last_syntax(ptReg, ptReg_cmd) < 0) {  // paking last syntax
                        favce_err("scale 4x4 flag %d timeout\n", i);
                        return H264E_ERR_POLLING_TIMEOUT;
                    }
                    paking_idx = 0;
                }
            #endif
                if (sps->seq_scaling_list_present_flag[i]) {
    				for (j = 0; j<16; j++) {
    					if (sps->delta_scale4x4[i][j] != DISBALE_DELTA_SCALE) {
    						se_v(ptReg, sps->delta_scale4x4[i][j]);
                        #ifdef AUTO_PAKING_HEADER
                            if (paking_idx == 64) {
                                if (paking_last_syntax(ptReg, ptReg_cmd) < 0) {  // paking last syntax
                                    favce_err("scale 4x4 %d timeout\n", i);
                                    return H264E_ERR_POLLING_TIMEOUT;
                                }
                                paking_idx = 0;
                            }
                        #endif
                        }
    				}
                }
			}
			for (i = 0; i<2; i++) {
    			u_v(ptReg, sps->seq_scaling_list_present_flag[i+6], 1);
    	    #ifdef AUTO_PAKING_HEADER
				if (paking_idx == 64) {
                    if (paking_last_syntax(ptReg, ptReg_cmd) < 0) {  // paking last syntax
                        favce_err("scale 8x8 flag %d timeout\n", i);
                        return H264E_ERR_POLLING_TIMEOUT;
                    }
                    paking_idx = 0;
                }
            #endif
                if (sps->seq_scaling_list_present_flag[i+6]) {
    				for (j = 0; j<64; j++) {
    					if (sps->delta_scale8x8[i][j] != DISBALE_DELTA_SCALE) {
    						se_v(ptReg, sps->delta_scale8x8[i][j]);
                	    #ifdef AUTO_PAKING_HEADER
    						if (paking_idx == 64) {
                                if (paking_last_syntax(ptReg, ptReg_cmd) < 0) {  // paking last syntax
                                    favce_err("scale 8x8 %d timeout\n", i);
                                    return H264E_ERR_POLLING_TIMEOUT;
                                }
                                paking_idx = 0;
                            }
                        #endif
                        }
    				}
                }
			}
		}
	}

#ifdef AUTO_PAKING_HEADER
    if (paking_idx > 32) {
        if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
            return H264E_ERR_POLLING_TIMEOUT;
        paking_idx = 0;
    }
#endif

	ue_v(ptReg, sps->log2_max_frame_num_minus4);
	ue_v(ptReg, sps->pic_order_cnt_type);
	//if (sps->pic_order_cnt_type == 0)
		ue_v(ptReg, sps->log2_max_pic_order_cnt_lsb_minus4);
	/*
	else if (sps->pic_order_cnt_type == 1) {
		u_v(ptReg_cmd, sps->delta_pic_order_always_zero_flag, 1);
		se_v(ptReg_cmd, sps->offset_for_non_ref_pic);
		se_v(ptReg_cmd, sps->offset_for_top_to_bottom_field);
		ue_v(ptReg_cmd, sps->num_ref_frames_in_pic_order_order_cnt_cycle);
		for (i = 0; i<sps->num_ref_frames_in_pic_order_order_cnt_cycle; i++)
			se_v(ptReg_cmd, sps->offset_for_ref_frame[i]);
	}
	*/
	ue_v(ptReg, sps->num_ref_frames);
	u_v(ptReg, 0, 1);	// gaps_in_frame_num_value_allowed_flag
	ue_v(ptReg, sps->pic_width_in_mbs_minus1);
	ue_v(ptReg, sps->pic_height_in_map_units_minus1);
	u_v(ptReg, sps->frame_mbs_only_flag, 1);
	if (!sps->frame_mbs_only_flag)
		u_v(ptReg, 0, 1);	// sps->mb_adaptive_frame_field_flag
	u_v(ptReg, sps->direct_8x8_inference_flag, 1);
	//u_v(ptReg, 0, 1);	// sps->frame_cropping_flag

	u_v(ptReg, sps->frame_cropping_flag, 1);	// sps->frame_cropping_flag
	if (sps->frame_cropping_flag) {
		ue_v(ptReg, sps->frame_cropping_rect_left_offset);
		ue_v(ptReg, sps->frame_cropping_rect_right_offset);
		ue_v(ptReg, sps->frame_cropping_rect_top_offset);
		ue_v(ptReg, sps->frame_cropping_rect_bottom_offset);
	}
	u_v(ptReg, sps->vui_parameters_present_flag, 1);

    if (sps->vui_parameters_present_flag) {
    #ifdef AUTO_PAKING_HEADER
    	if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
            return H264E_ERR_POLLING_TIMEOUT;
        paking_idx = 0;
    #endif
		write_vui(p_Enc, &sps->vui_seq_parameter);
    }
    rbsp_trailing_bits(ptReg);
#ifdef AUTO_PAKING_HEADER
	if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
        return H264E_ERR_POLLING_TIMEOUT;
    paking_idx = 0;
#else
    while (!(ptReg->STS0 & BIT1));
#endif
    showValue(0x1010000F);
    showValue(ptReg_cmd->STS1);
    return H264E_OK;
}

int write_pps(void *ptEncHandle, PicParameterSet *pps)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
#ifdef AUTO_PAKING_HEADER
    volatile H264Reg_Mem *ptReg = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#else
    volatile H264Reg_Cmd *ptReg = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#endif
	uint32_t profile_idc = 	p_Enc->p_Vid->active_sps->profile_idc;

    paking_idx = 0;
	//u_v_prev(ptReg, 0x00000001, 32);	// start code
	u_v_prev(ptReg, 0x000000, 16);	// start code
    u_v(ptReg, 0x0001, 16);	// start code
	u_v_prev(ptReg, 0x68, 8);	// PPS

	ue_v(ptReg, pps->pic_parameter_set_id);
	ue_v(ptReg, pps->seq_parameter_set_id);
	u_v(ptReg, pps->entropy_coding_mode_flag, 1);
	u_v(ptReg, pps->pic_order_present_flag, 1);	// pps->pic_order_present_flag
	ue_v(ptReg, 0);		// pps->num_slice_groups_minus1
	
	ue_v(ptReg, pps->num_ref_idx_l0_active_minus1);
	ue_v(ptReg, pps->num_ref_idx_l1_active_minus1);
	u_v(ptReg, pps->weighted_pred_flag, 1);
	u_v(ptReg, pps->weighted_bipred_idc, 2);
	se_v(ptReg, pps->pic_init_qp_minus26);
	se_v(ptReg, 0);		// pic_init_qs_minus26
	se_v(ptReg, pps->chroma_qp_index_offset);
	u_v(ptReg, pps->deblocking_filter_control_present_flag, 1);
	u_v(ptReg, 0, 1);	// constrained_intra_pred_flag
	u_v(ptReg, 0, 1);	// redundant_pic_cnt_present_flag

	if (profile_idc>=FREXT_HP) {
		u_v(ptReg, pps->transform_8x8_mode_flag, 1);
		u_v(ptReg, pps->pic_scaling_matrix_present_flag, 1);
		se_v(ptReg, pps->second_chroma_qp_index_offset);
	}
    rbsp_trailing_bits(ptReg);
#ifdef AUTO_PAKING_HEADER
    if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
        return H264E_ERR_POLLING_TIMEOUT;
#endif
#ifndef AUTO_PAKING_HEADER
	while (!(ptReg->STS0 & BIT1));
#endif
	showValue(0x1010001F);
    showValue(ptReg_cmd->STS1);
    return H264E_OK;
}

#ifdef AES_ENCRYPT
int write_sei(void *ptEncHandle, uint8_t sei_type, int sei_length, char *sei_data)
{
    EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
#ifdef AUTO_PAKING_HEADER
    volatile H264Reg_Mem *ptReg = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
    volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#else
    volatile H264Reg_Cmd *ptReg = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#endif
    int i, len;
    int bs_cnt = 0;

    paking_idx = 0;
    u_v_prev(ptReg, 0x00000001, 32);	// start code
	u_v_prev(ptReg, 0x06, 8);	// SEI
	
	u_v(ptReg, sei_type, 8);
    bs_cnt = 6;
	len = sei_length;
	for (i = 0; i<sei_length; i+=255) {
	    if (len > 0xFF) {
	        u_v(ptReg, 0xFF, 8);
        }
	    else {
	        u_v(ptReg, len, 8);
        }
	    len -= 0xFF;
        bs_cnt++;
	}
	for (i = 0; i<sei_length; i++) {
    #ifdef AUTO_PAKING_HEADER
        if (bs_cnt >= 32) {
            if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
                return H264E_ERR_POLLING_TIMEOUT;
            bs_cnt = 0;
            paking_idx = 0;
        }
    #endif
        u_v(ptReg, sei_data[i], 8);
        bs_cnt++;        
	    //u_v(ptReg, i&0xFF, 8);
    }
    rbsp_trailing_bits(ptReg);
#ifdef AUTO_PAKING_HEADER
    if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
        return H264E_ERR_POLLING_TIMEOUT;
#endif
    return 0;
}
#endif

