#ifdef LINUX
	#include <linux/kernel.h>
#endif
#include "slice_header.h"
#include "params.h"
#include "vlc.h"
#include "global.h"

extern int paking_idx;

int write_slice_header(void *ptEncHandle, void *ptVid, SliceHeader *currSlice, void *ptPic)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
#ifdef AUTO_PAKING_HEADER
    volatile H264Reg_Mem *ptReg = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#else
    volatile H264Reg_Cmd *ptReg = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
#endif

	VideoParams *p_Vid = (VideoParams *)ptVid;
	StorablePicture *pic = (StorablePicture *)ptPic;
	PicParameterSet *pps = p_Vid->active_pps;
	SeqParameterSet *sps = p_Vid->active_sps;
	uint32_t tmp;

    paking_idx = 0;
	if (currSlice->first_mb_in_slice == 0)
		u_v_prev(ptReg, 0x00000001, 32);	// start code
	else
		u_v_prev(ptReg, 0x000001, 24);
	if (pic->idr_flag)
		tmp = 0x65;
	else
		tmp = (((pic->used_for_ref&0x03)<<5) | 0x01);
	u_v_prev(ptReg, tmp, 8);	// slice header

	ue_v(ptReg, currSlice->first_mb_in_slice);
	ue_v(ptReg, pic->slice_type + 5);
	ue_v(ptReg, p_Vid->active_pps->pic_parameter_set_id);
	u_v(ptReg, pic->frame_num, p_Vid->active_sps->log2_max_frame_num_minus4+4);
    
	if (!sps->frame_mbs_only_flag) {
		//u_v(ptReg_cmd, p_Vid->field_pic_flag, 1);	// field_pic_flag
		u_v(ptReg, 1, 1);

		if (pic->structure == TOP_FIELD)
			u_v(ptReg, 0, 1);	// top field
		else
			u_v(ptReg, 1, 1);	// bottom field
	}

	if (pic->idr_flag) {
        //ue_v(ptReg, p_Vid->encoded_frame_num&0x01);
        // Tuba 20131022: increase index of idr for skip detection
    #ifndef STRICT_IDR_IDX
        ue_v(ptReg, p_Vid->encoded_frame_num&0x07);
    #else
        ue_v(ptReg, p_Vid->idr_cnt&0x07);
        p_Vid->idr_cnt++;
    #endif
    }
	if (p_Vid->active_sps->pic_order_cnt_type == 0) {
		u_v(ptReg, pic->poc_lsb, p_Vid->active_sps->log2_max_pic_order_cnt_lsb_minus4+4);
		if (pps->pic_order_present_flag && !p_Vid->field_pic_flag)
			se_v(ptReg, 1);	// delta_pic_order_cnt_bottom
	}

	if (pic->slice_type == B_SLICE)
		u_v(ptReg, currSlice->direct_spatial_mv_pred_flag, 1);
	if (pic->slice_type == B_SLICE || pic->slice_type == P_SLICE) {
		if ((pps->num_ref_idx_l0_active_minus1+1) != p_Vid->list_size[0]) {	
		// num_ref_idx_l1_active_minus1 & list_size[1]-1 is always 0
			u_v(ptReg, 1, 1);
			ue_v(ptReg, p_Vid->list_size[0]-1);
			if (pic->slice_type == B_SLICE)
				ue_v(ptReg, p_Vid->list_size[1]-1);
    	}
		else
			u_v(ptReg, 0, 1);
	}

	// ref_pic_list_reordering
	if (pic->slice_type != I_SLICE)
		u_v(ptReg, 0, 1);	// no reordering
	if (pic->slice_type == B_SLICE)
		u_v(ptReg, 0, 1);

	// pred_weight_table

	// dec_ref_pic_marking
	if (pic->used_for_ref) {
		if (pic->idr_flag) {
			u_v(ptReg, currSlice->no_output_of_prior_pics_flag, 1);
			u_v(ptReg, currSlice->long_term_reference_flag, 1);
		}
		else
			u_v(ptReg, 0, 1);	// adaptive_ref_pic_marking_mode_flag
	}

	if (pps->entropy_coding_mode_flag && pic->slice_type != I_SLICE)
		ue_v(ptReg, p_Vid->cabac_init_idc);
	se_v(ptReg, p_Vid->currQP - (pps->pic_init_qp_minus26+26));	// slice_qp_delta
	if (pps->deblocking_filter_control_present_flag) {
		ue_v(ptReg, currSlice->disable_deblocking_filter_idc);
		if (currSlice->disable_deblocking_filter_idc != 1) {
            se_v(ptReg, currSlice->slice_alpha_c0_offset_div2);
			se_v(ptReg, currSlice->slice_beta_offset_div2);
		}
	}

#ifdef AUTO_PAKING_HEADER
    if (paking_last_syntax(ptReg, ptReg_cmd) < 0)  // paking last syntax
        return H264E_ERR_POLLING_TIMEOUT;
#else
	while (!(ptReg->STS0 & BIT1));
#endif

	return H264E_OK;
}

