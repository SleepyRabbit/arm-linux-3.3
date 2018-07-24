#ifdef LINUX
	#include <linux/kernel.h>
	#include <linux/string.h>
#else
	#include <string.h>
#endif
#include "define.h"
#include "slicehdr.h"
#include "vlc.h"
#include "global.h"
#include "quant.h"
#include "h264_reg.h"

#define IGNORE_PARAM_ERROR
static int ref_pic_list_reordering(DecoderParams *p_Dec, SliceHeader *sh);
static int parse_pred_weight_table(DecoderParams *p_Dec, SeqParameterSet *sps, SliceHeader *sh);

/*
 * ref_pic_list_modification syntax 
 */
static int ref_pic_list_reordering(DecoderParams *p_Dec, SliceHeader *sh)
{
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int i, val;

	if (sh->slice_type!=I_SLICE){
		sh->ref_pic_list_reordering_flag_l0 = READ_U(p_Par, 1);
		if (sh->ref_pic_list_reordering_flag_l0){
			i = 0;
			do {
				val = sh->reordering_of_ref_pic_list[0][i].reordering_of_pic_nums_idc = READ_UE(p_Par);
            #ifndef IGNORE_PARAM_ERROR
				if (val > 3) {
				    E_DEBUG("unknown reorder idc %d\n", val);
				    return H264D_PARSING_ERROR;
				}
            #endif
				if (val==0 || val==1)
					sh->reordering_of_ref_pic_list[0][i].abs_diff_pic_num_minus1 = READ_UE(p_Par);
				else if (val==2)
					sh->reordering_of_ref_pic_list[0][i].long_term_pic_num = READ_UE(p_Par);
				i++;
			} while(val!=3 && i<MAX_LIST_SIZE);
		}
	}
	if (sh->slice_type==B_SLICE)	{
		sh->ref_pic_list_reordering_flag_l1 = READ_U(p_Par, 1);
		if (sh->ref_pic_list_reordering_flag_l1){
			i = 0;
			do {
				val = sh->reordering_of_ref_pic_list[1][i].reordering_of_pic_nums_idc = READ_UE(p_Par);
            #ifndef IGNORE_PARAM_ERROR
				if (val > 3) {
				    E_DEBUG("unknown reorder idc %d\n", val);
				    return H264D_PARSING_ERROR;
				}
            #endif
				if (val==0 || val==1)
					sh->reordering_of_ref_pic_list[1][i].abs_diff_pic_num_minus1 = READ_UE(p_Par);
				else if (val==2)
					sh->reordering_of_ref_pic_list[1][i].long_term_pic_num = READ_UE(p_Par);
				i++;
			} while(val!=3 && i<MAX_LIST_SIZE);
		}
	}
	return H264D_OK;
}

/*
 * parse pred_weight_table syntax
 */
static int parse_pred_weight_table(DecoderParams *p_Dec, SeqParameterSet *sps, SliceHeader *sh)
{
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	byte luma_l0_flag, luma_l1_flag, chroma_l0_flag = 0, chroma_l1_flag = 0;
	int16_t lw_0, lo_0, lw_1, lo_1;
	int16_t cw_0_cb, co_0_cb, cw_0_cr, co_0_cr;
	int16_t cw_1_cb, co_1_cb, cw_1_cr, co_1_cr;
	int i;

	sh->luma_log2_weight_denom = READ_UE(p_Par); // 0~7
	if (sh->luma_log2_weight_denom > 7) {
	    E_DEBUG("luma weight denom should not exceed 7 (%d)\n", sh->luma_log2_weight_denom);
	    return H264D_PARSING_ERROR;
    }
	if (sps->chroma_format_idc) {
		sh->chroma_log2_weight_denom = READ_UE(p_Par);   // 0~7
		if (sh->chroma_log2_weight_denom > 7) {
    		E_DEBUG("chroma weight denom should not exceed 7 (%d)\n", sh->chroma_log2_weight_denom);
    		return H264D_PARSING_ERROR;
        }
    }
	else {
		sh->chroma_log2_weight_denom = 0;
    }
	for (i = 0; i<=sh->num_ref_idx_l0_active_minus1; i++) {
		lw_0 = lo_0 = 0;
		cw_0_cb = co_0_cb = cw_0_cr = co_0_cr = 0;
		luma_l0_flag = READ_U(p_Par, 1);
		if (luma_l0_flag) {
			lw_0 = READ_SE(p_Par);
			lo_0 = READ_SE(p_Par);
			// if not check decoder can still work, because it will not out of range (register use 8 bit(-127~128))
        #ifndef IGNORE_PARAM_ERROR
			if ((lw_0 < -127 || lw_0 > 128) || (lo_0 < -127 || lo_0 > 128)) {
                E_DEBUG("l0 luma weight or offset out of range (w: %d, o: %d)\n", lw_0, lo_0);
			    return H264D_PARSING_ERROR;
            }
        #endif
		}
		if (sps->chroma_format_idc) {
			chroma_l0_flag = READ_U(p_Par, 1);
			if (chroma_l0_flag) {
				cw_0_cb = READ_SE(p_Par);
				co_0_cb = READ_SE(p_Par);
				cw_0_cr = READ_SE(p_Par);
				co_0_cr = READ_SE(p_Par);
				// if not check decoder can still work, because it will not out of range (register use 8 bit(-127~128))
            #ifndef IGNORE_PARAM_ERROR
				if ((cw_0_cb < -127 || cw_0_cb > 128) || (co_0_cb < -127 || co_0_cb > 128)) {
				    E_DEBUG("l0 cb weight or offset out of rnage (w: %d, o: %d\n", cw_0_cb, co_0_cb);
				    return H264D_PARSING_ERROR;
				}
				if ((cw_0_cr < -127 || cw_0_cr > 128) || (co_0_cr < -127 || co_0_cr > 128)) {
				    E_DEBUG("l0 cr weight or offset out of rnage (w: %d, o: %d\n", cw_0_cr, co_0_cr);
				    return H264D_PARSING_ERROR;
				}
            #endif
			}
		}
		sh->weightl0[i*2] = (((chroma_l0_flag&0x01)<<17) | ((luma_l0_flag&0x01)<<16) | ((lo_0&0xFF)<<8) | (lw_0&0xFF));
		sh->weightl0[i*2 + 1] = CONCAT_4_8BITS_VALUES(co_0_cr, cw_0_cr, co_0_cb, cw_0_cb);
	}
	if (sh->slice_type==B_SLICE){
		for (i = 0; i<=sh->num_ref_idx_l1_active_minus1; i++){
			lw_1 = lo_1 = 0;
			cw_1_cb = co_1_cb = cw_1_cr = co_1_cr = 0;
			luma_l1_flag = READ_U(p_Par, 1);
			if (luma_l1_flag){
				lw_1 = READ_SE(p_Par);
				lo_1 = READ_SE(p_Par);
				// if not check decoder can still work, because it will not out of range (register use 8 bit(-127~128))
            #ifndef IGNORE_PARAM_ERROR
    			if ((lw_1 < -127 || lw_1 > 128) || (lo_1 < -127 || lo_1 > 128)) {
                    E_DEBUG("l1 luma weight or offset out of range (w: %d, o: %d)\n", lw_1, lo_1);
    			    return H264D_PARSING_ERROR;
                }
            #endif
			}
			if (sps->chroma_format_idc){
				chroma_l1_flag = READ_U(p_Par, 1);
				if (chroma_l1_flag){
					cw_1_cb = READ_SE(p_Par);
					co_1_cb = READ_SE(p_Par);
					cw_1_cr = READ_SE(p_Par);
					co_1_cr = READ_SE(p_Par);
    				// if not check decoder can still work, because it will not out of range (register use 8 bit(-127~128))
                #ifndef IGNORE_PARAM_ERROR
    				if ((cw_1_cb < -127 || cw_1_cb > 128) || (co_1_cb < -127 || co_1_cb > 128)) {
    				    E_DEBUG("l1 cb weight or offset out of rnage (w: %d, o: %d\n", cw_1_cb, co_1_cb);
    				    return H264D_PARSING_ERROR;
    				}
    				if ((cw_1_cr < -127 || cw_1_cr > 128) || (co_1_cr < -127 || co_1_cr > 128)) {
    				    E_DEBUG("l1 cr weight or offset out of rnage (w: %d, o: %d\n", cw_1_cr, co_1_cr);
    				    return H264D_PARSING_ERROR;
    				}
                #endif
				}
			}
            sh->weightl1[i*2] = (((chroma_l1_flag&0x01)<<17) | ((luma_l1_flag&0x01)<<16) | ((lo_1&0xFF)<<8) | (lw_1&0xFF));
            sh->weightl1[i*2 + 1] = CONCAT_4_8BITS_VALUES(co_1_cr, cw_1_cr, co_1_cb, cw_1_cb);
		}
	}
	return H264D_OK;
}


/*
 * dec_ref_pic_marking syntax
 */
int dec_ref_pic_marking(DecoderParams *p_Dec, VideoParams *p_Vid, SliceHeader *sh)
{
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	int val, idx;

	if (p_Vid->idr_flag){
		sh->no_output_of_prior_pics_flag = READ_U(p_Par, 1);
		//sh->no_output_of_prior_pics_flag = 1; /* for testing idr_memory_management() */
		sh->long_term_reference_flag = READ_U(p_Par, 1);
	}
	else {
		sh->adaptive_ref_pic_marking_mode_flag = READ_U(p_Par, 1);
		if (sh->adaptive_ref_pic_marking_mode_flag) {
			idx = 0;
			do {
				val = p_Vid->drpmb[idx].memory_management_control_operation = READ_UE(p_Par);
            #ifndef IGNORE_PARAM_ERROR
				if (val > 6) {
				    E_DEBUG("unknown mmco %d\n", val);
				    return H264D_PARSING_ERROR;
				}
            #endif
				if (val==1 || val==3)
					p_Vid->drpmb[idx].difference_of_pic_nums_minus1 = READ_UE(p_Par);
				if (val==2)
					p_Vid->drpmb[idx].long_term_pic_num = READ_UE(p_Par);
				if (val==3 || val==6)
					p_Vid->drpmb[idx].long_term_frame_idx = READ_UE(p_Par);
				if (val==4)
					p_Vid->drpmb[idx].max_long_term_frame_idx_plus1 = READ_UE(p_Par);
				idx++;
				if (idx >= MAX_MMCO_NUM) {
					W_DEBUG("too many mmco\n");
					break;
				}
			 } while(val!=0);
		}
	}
	return H264D_OK;
}


/*
 * store cropped size in pixels (Y-plane samples) into ptDecHandle
 */
int store_cropped_size(void *ptDecHandle, void *ptVid)
{
	DecoderParams *p_Dec = (DecoderParams*)ptDecHandle;
	VideoParams *p_Vid = (VideoParams*)ptVid;
    unsigned int crop_unit_x = 1;
    unsigned int crop_unit_y = 1;
    
    if(p_Vid->active_sps->chroma_format_idc == 0) { /* grayscale */
        if(p_Vid->field_pic_flag == 0) { /* Frame coding */
            crop_unit_x = 1;
            crop_unit_y = 1;
        } else { /* Field coding */
            crop_unit_x = 1;
            crop_unit_y = 2;
        }
    } else if(p_Vid->active_sps->chroma_format_idc == 1) { /* 420 */
        if(p_Vid->field_pic_flag == 0) { /* Frame coding */
            crop_unit_x = 2;
            crop_unit_y = 2;
        } else { /* Field coding */
            crop_unit_x = 2;
            crop_unit_y = 4;
        }
    } else {
        LOG_PRINT(LOG_INFO, "chroma_format_idc:%d\n", p_Vid->active_sps->chroma_format_idc);
        //printk("chroma_format_idc:%d\n", p_Vid->active_sps->chroma_format_idc);
    }
    LOG_PRINT(LOG_INFO, "chroma_format_idc:%d field:%d unit_x:%d unit_y:%d l:%d r:%d t:%d b:%d\n", 
        p_Vid->active_sps->chroma_format_idc, p_Vid->field_pic_flag,
        crop_unit_x, crop_unit_y,
        p_Vid->active_sps->frame_cropping_rect_left_offset,
        p_Vid->active_sps->frame_cropping_rect_right_offset,
        p_Vid->active_sps->frame_cropping_rect_top_offset,
        p_Vid->active_sps->frame_cropping_rect_bottom_offset);

    p_Dec->u16CroppingLeft = p_Vid->active_sps->frame_cropping_rect_left_offset * crop_unit_x;
    p_Dec->u16CroppingRight = p_Vid->active_sps->frame_cropping_rect_right_offset * crop_unit_x;
    p_Dec->u16CroppingTop = p_Vid->active_sps->frame_cropping_rect_top_offset * crop_unit_y;
    p_Dec->u16CroppingBottom = p_Vid->active_sps->frame_cropping_rect_bottom_offset * crop_unit_y;

    p_Dec->u16FrameWidthCropped = p_Dec->u16FrameWidth - p_Dec->u16CroppingLeft - p_Dec->u16CroppingRight;
    p_Dec->u16FrameHeightCropped = p_Dec->u16FrameHeight - p_Dec->u16CroppingTop - p_Dec->u16CroppingBottom;

	return H264D_OK;
}


int read_slice_header(void *ptDecHandle, void *ptVid, void *ptSliceHdr)
{
	DecoderParams *p_Dec = (DecoderParams*)ptDecHandle;
	VideoParams *p_Vid = (VideoParams*)ptVid;
	SliceHeader *sh = (SliceHeader*)ptSliceHdr;
	SeqParameterSet *sps;
	PicParameterSet *pps;
	uint32_t val;
	byte flag;
	uint32_t PicWidthInMbs, FrameHeightInMbs, frame_width, frame_height, max_dpb_size;
//	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);

#if CLR_SLICE_HDR
    memset(sh, 0, sizeof(SliceHeader) - sizeof(sh->weightl0) - sizeof(sh->weightl1));
#endif
//printk("first mb: %08x\n", SHOW_BITS(p_Dec, 32));
	sh->first_mb_in_slice = READ_UE(p_Par);
#if 0
	if (p_Vid->first_slice && sh->first_mb_in_slice != 0)
	    return H264D_PARSING_ERROR;
#else
	if (p_Vid->first_slice ^ (sh->first_mb_in_slice == 0)) {
        /* first slice and first mb index != 0, 
        or not first slice and first mb index == 0 */
        E_DEBUG("first mb(%d) must be zero while first slice(%d) or first mb(%d) can not be zero while not first slice(%d)\n", 
            sh->first_mb_in_slice, p_Vid->first_slice, sh->first_mb_in_slice, p_Vid->first_slice);
	    return H264D_PARSING_ERROR;
	}
#endif

	val = READ_UE(p_Par);	/* slice type: 0~9 */
	val = (val>4 ? val-5 : val);
	if (val >= 3) {
        /* support P(0)/B(1)/I(2) slice type */
        /* SP_SLICE(3), SI_SLICE(4) is not supported */
		E_DEBUG("unsupport slice type %d\n", val);
		return H264D_ERR_HW_UNSUPPORT;
	}
	if (p_Dec->bUnsupportBSlice && val == B_SLICE) {
	    E_DEBUG("unsupport B slice\n");
	    return H264D_ERR_DRV_UNSUPPORT;
	}
	p_Vid->dec_pic->slice_type = sh->slice_type = val;
	sh->pic_parameter_set_id = READ_UE(p_Par);


	/* get PPS */
	if (sh->pic_parameter_set_id > MAX_PPS_NUM)
		pps = NULL;
	else
		pps = p_Vid->PPS[sh->pic_parameter_set_id];

    if(pps == NULL){
		E_DEBUG("pps %d does not exist\n", sh->pic_parameter_set_id);
        return H264D_NO_PPS;
		//return H264D_PARSING_ERROR;
    }

    /* get SPS */
    if (pps->seq_parameter_set_id > MAX_SPS_NUM)
        sps = NULL;
    else
        sps = p_Vid->SPS[pps->seq_parameter_set_id];
    if (sps == NULL) {
        E_DEBUG("sps %d does not exist\n", pps->seq_parameter_set_id);
        return H264D_NO_SPS;
        //return H264D_PARSING_ERROR;
    }

    
    //printk("pps id: %d, sps id: %d\n", sh->pic_parameter_set_id, pps->seq_parameter_set_id);

    if (sps->bUpdate || p_Vid->active_sps != sps) {
        DUMP_MSG(D_BUF_REF, p_Dec, "sps update: %d active_sps change:%d\n", sps->bUpdate, (p_Vid->active_sps != sps)?1:0);
        
		PicWidthInMbs = sps->pic_width_in_mbs_minus1 + 1;
		FrameHeightInMbs = (sps->pic_height_in_map_units_minus1 + 1)<<(1 - sps->frame_mbs_only_flag);
		frame_width = PicWidthInMbs << 4;
		frame_height = FrameHeightInMbs << 4;
		if (p_Dec->u16FrameWidth == 0)
		    p_Dec->u16FrameWidth = frame_width;
		if (p_Dec->u16FrameHeight == 0)
		    p_Dec->u16FrameHeight = frame_height;
		if (p_Dec->u16FrameWidth != frame_width || p_Dec->u16FrameHeight != frame_height) {
		    LOG_PRINT(LOG_WARNING, "frame size is changing (%d x %d -> %d x %d)\n", p_Dec->u16FrameWidth, p_Dec->u16FrameHeight, frame_width, frame_height);
#if 0 
		    return H264D_PARSING_ERROR;
#else // do not treat resolution change as error
            p_Dec->u16FrameHeight = frame_height;
            p_Dec->u16FrameWidth = frame_width;
#endif
		}
		if (sps->b_unsupport) {
		    E_DEBUG("hw unsupport this sps\n");
		    return H264D_ERR_HW_UNSUPPORT;
		}
		if (sps->b_syntax_error) {
		    E_DEBUG("syntax error of this sps\n");
		    return H264D_PARSING_ERROR;
		}
		if ((frame_width > p_Dec->u16MaxWidth) || (frame_height > p_Dec->u16MaxHeight)) {
    		E_DEBUG("frame size is larger than max frame size (%dx%d > %dx%d)\n", frame_width, frame_height, p_Dec->u16MaxWidth, p_Dec->u16MaxHeight);
    		return H264D_SIZE_OVERFLOW;
    	}
    	if (frame_width * frame_height == 0) {
    	    E_DEBUG("frame size is zero (%d x %d)\n", frame_width, frame_height);
    	    return H264D_SIZE_OVERFLOW;
        }

        max_dpb_size = getDpbSize(sps);
		//p_Vid->listD->max_size = getDpbSize(sps) + 1;
		DUMP_MSG(D_BUF_REF, p_Dec, "used size:%d max dpb size:%d\n", p_Vid->listD->used_size, max_dpb_size);

        if (sps->num_ref_frames > max_dpb_size) {
            E_DEBUG("num_ref_frame is larger than max buffer size (%d > %d)\n", sps->num_ref_frames, max_dpb_size);
            return H264D_PARSING_ERROR;
        }
        /*
	    if (sps->num_ref_frames > (p_Vid->listD->max_size-1)) {
            E_DEBUG("num_ref_frame is larger than max buffer size (%d > %d)\n", sps->num_ref_frames, p_Vid->listD->max_size-1);
            sps->num_ref_frames = p_Vid->listD->max_size-1;
            //return H264D_ERR_HEADER;
        }
        */
        if (p_Vid->listD->used_size >= max_dpb_size + 1) {
            W_DEBUG("used size(%d) >= max dpb size (%d), flush dpb buffer\n", p_Vid->listD->used_size, max_dpb_size+1);
            DUMP_MSG(D_BUF_REF, p_Dec, "used size(%d) >= max dpb size (%d), flush dpb buffer\n", p_Vid->listD->used_size, max_dpb_size+1);
            flush_dpb(p_Vid);
            //return H264D_PARSING_ERROR;
        }

		if ((p_Vid->max_buffered_num == MAX_BUFFERED) || 
			(p_Vid->max_buffered_num > max_dpb_size)) {
			p_Vid->max_buffered_num = max_dpb_size;
        }

        LOG_PRINT(LOG_INFO, "max_buffered_num: %d num_ref_frame:%d max_dpb_size:%d\n", 
            p_Vid->max_buffered_num, sps->num_ref_frames, max_dpb_size);
        
        if(sps->num_ref_frames > MAX_REF_FRAME_NUM) {
           E_DEBUG("unsupported num_ref_frames:%d > MAX_REF_FRAME_NUM %d\n", sps->num_ref_frames, MAX_REF_FRAME_NUM);
           return H264D_ERR_BUF_NUM_NOT_ENOUGH;
        }

        {
            extern int output_buf_num;
            if(p_Vid->max_buffered_num == 0 || output_buf_num == 0){
                /* display order == decode order */
                if(sh->slice_type == B_SLICE){
                    W_DEBUG("max buffer num(%d) or output buffer num(%d) is 0 at decoding b slice: output order may be incorrect\n", p_Vid->max_buffered_num, output_buf_num);
                    return H264D_ERR_BUF_NUM_NOT_ENOUGH;
                }
            }
        }
        {
            extern int chk_buf_num;
            if(chk_buf_num){
                /* check if the buffer number is enough */
                if(p_Vid->max_buffered_num > 0){
                    if(sps->num_ref_frames > p_Vid->max_buffered_num) {
                        E_DEBUG("num_ref_frame is larger than current max buffer num (%d > %d)\n", 
                            sps->num_ref_frames, p_Vid->max_buffered_num);
                        return H264D_ERR_BUF_NUM_NOT_ENOUGH;
                    }
                }
            }
        }
        

/*
        if (p_Vid->listD->used_size >= p_Vid->listD->max_size) {
            E_DEBUG("used size(%d) overflow (%d)\n", p_Vid->listD->used_size, p_Vid->listD->max_size);
            printk("used size(%d) overflow (%d)\n", p_Vid->listD->used_size, p_Vid->listD->max_size);

            return H264D_PARSING_ERROR;
        }
*/
        sps->bUpdate = 0;

		p_Vid->active_sps = sps;
		p_Vid->PicWidthInMbs = PicWidthInMbs;
		p_Vid->FrameHeightInMbs = FrameHeightInMbs;
		p_Vid->frame_width = frame_width;
		p_Vid->frame_height = frame_height;
		p_Vid->listD->max_size = max_dpb_size + 1;
	}

	if (pps->bUpdate || p_Vid->active_pps != pps) {
        //printk("pps update\n");
		if (pps->b_unsupport) {
		    E_DEBUG("hw unsupport this pps\n");
		    return H264D_ERR_HW_UNSUPPORT;
		}
		if (pps->b_syntax_error) {
		    E_DEBUG("syatax error of this pps\n");
		    return H264D_PARSING_ERROR;
		}
		pps->bUpdate = 0;
        p_Vid->active_pps = pps;
	}
	//p_Vid->active_pps = pps;

    /* calculate corpped size (for deciding whether scaling should be enabled) */
    store_cropped_size(p_Dec, p_Vid);


	p_Vid->frame_num = READ_U(p_Par, sps->log2_max_frame_num);
	if (p_Vid->idr_flag)
		p_Vid->prev_frame_num = p_Vid->frame_num;

	if(sps->frame_mbs_only_flag){
		p_Vid->structure = FRAME;
		p_Vid->field_pic_flag = 0;
	} else {
		p_Vid->field_pic_flag = READ_U(p_Par, 1);
		if (p_Vid->field_pic_flag){
			sh->bottom_field_flag = READ_U(p_Par, 1);
			p_Vid->structure = sh->bottom_field_flag ? BOTTOM_FIELD : TOP_FIELD;
		} else {
			p_Vid->structure = FRAME;
			sh->bottom_field_flag = 0;
		}
	}

	p_Vid->MbaffFrameFlag = (sps->mb_adaptive_frame_field_flag && !p_Vid->field_pic_flag);
    LOG_PRINT(LOG_INFO, "MbaffFrameFlag: %d\n", p_Vid->MbaffFrameFlag);

	sh->currMbAddr = sh->first_mb_in_slice << p_Vid->MbaffFrameFlag;
	p_Vid->PicSizeInMbs = p_Vid->PicWidthInMbs * (p_Vid->FrameHeightInMbs>>p_Vid->field_pic_flag);
	if (sh->currMbAddr >= p_Vid->PicSizeInMbs) {
	    E_DEBUG("first mb is larger than total mb count (%d >= %d)\n", sh->first_mb_in_slice, p_Vid->PicSizeInMbs);
	    return H264D_PARSING_ERROR;
	}
	if (p_Vid->idr_flag)
		READ_UE(p_Par);	// idr_pic_id
	if(sps->pic_order_cnt_type==0){
		sh->pic_order_cnt_lsb = READ_U(p_Par, sps->log2_max_pic_order_cnt_lsb);
		if(pps->pic_order_present_flag && !p_Vid->field_pic_flag)
			sh->delta_pic_order_cnt_bottom = READ_SE(p_Par);
		else
			sh->delta_pic_order_cnt_bottom = 0;
	}

	if(sps->pic_order_cnt_type==1 && !sps->delta_pic_order_always_zero_flag){
		sh->delta_pic_order_cnt[0] = READ_SE(p_Par);
		if(pps->pic_order_present_flag && !p_Vid->field_pic_flag)
			sh->delta_pic_order_cnt[1] = READ_SE(p_Par);
	}
	else {
		if (sps->pic_order_cnt_type == 1)
			sh->delta_pic_order_cnt[0] = sh->delta_pic_order_cnt[1] = 0;
	}

	if(pps->redundant_pic_cnt_present_flag){
		READ_UE(p_Par);	/* redundant_pic_cnt */
    }
	if(sh->slice_type==B_SLICE)
		sh->direct_spatial_mv_pred_flag = READ_U(p_Par, 1);
	else 
		sh->direct_spatial_mv_pred_flag = 0;
	sh->num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_active_minus1;
	sh->num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_active_minus1;

	if(sh->slice_type==P_SLICE || sh->slice_type==B_SLICE){
		flag = READ_U(p_Par, 1);	// num_ref_idx_active_override_flag
		if(flag){
			sh->num_ref_idx_l0_active_minus1 = READ_UE(p_Par);
			if(sh->slice_type==B_SLICE)
				sh->num_ref_idx_l1_active_minus1 = READ_UE(p_Par);
		}
	}
    /* add to avoid reference number too large */
	if (!p_Vid->field_pic_flag) {
    	sh->num_ref_idx_l0_active_minus1 = iMin(15, sh->num_ref_idx_l0_active_minus1);
    	sh->num_ref_idx_l1_active_minus1 = iMin(15, sh->num_ref_idx_l1_active_minus1);
    }
    else {
    	sh->num_ref_idx_l0_active_minus1 = iMin(31, sh->num_ref_idx_l0_active_minus1);
    	sh->num_ref_idx_l1_active_minus1 = iMin(31, sh->num_ref_idx_l1_active_minus1);
    }
    

    if(SHOULD_DUMP(D_BUF_SLICE, p_Dec)){
        DUMP_MSG(D_BUF_SLICE, p_Dec, "bf ref_pic_list_reordering slice type:%d\n", sh->slice_type);
        dump_dpb(p_Dec, 0);
    }


	/* reference picture list reordering */
	if (ref_pic_list_reordering(p_Dec, sh) < 0)
	    return H264D_PARSING_ERROR;

	/* prediction weight table */
	if((pps->weighted_pred_flag && sh->slice_type==P_SLICE) ||
		(pps->weighted_bipred_idc==1 && sh->slice_type==B_SLICE)) {
		if (parse_pred_weight_table(p_Dec, sps, sh) < 0)
		    return H264D_PARSING_ERROR;
    }
	else if ((pps->weighted_bipred_idc==2) && sh->slice_type==B_SLICE){
		sh->luma_log2_weight_denom = 5;
		sh->chroma_log2_weight_denom = 5;
	}
	else {
		sh->luma_log2_weight_denom = 0;
		sh->chroma_log2_weight_denom = 0;
		//set_pred_weight_table_default(p_Dec, sps, sh);
	}


	/* decoded reference picture marking */
	if(p_Vid->nalu.nal_ref_idc){
        if(SHOULD_DUMP(D_BUF_SLICE, p_Dec)){
            DUMP_MSG(D_BUF_SLICE, p_Dec, "bf dec_ref_pic_marking slice type:%d\n", sh->slice_type);
            dump_dpb(p_Dec, 0);
        }
		if (dec_ref_pic_marking(p_Dec, p_Vid, sh) < 0)
		    return H264D_PARSING_ERROR;
    }

	if(pps->entropy_coding_mode_flag && sh->slice_type != I_SLICE) {
		sh->cabac_init_idc = READ_UE(p_Par);
		if (sh->cabac_init_idc > 2) {
			E_DEBUG("cabac_init_idc %d is out of range\n", sh->cabac_init_idc);
			return H264D_PARSING_ERROR;
		}
	}
	sh->SliceQPy = pps->pic_init_qp + READ_SE(p_Par); /* slice_qp_delta */
	if (sh->SliceQPy > 51) {
		E_DEBUG("SliceQPy %d is out of range\n", sh->SliceQPy);
		return H264D_PARSING_ERROR;
	}

	set_qp(p_Vid);

	sh->slice_alpha_c0_offset_div2 = sh->slice_beta_offset_div2 = 0;
	if(pps->deblocking_filter_control_present_flag){
		sh->disable_deblocking_filter_idc = READ_UE(p_Par);
		if (sh->disable_deblocking_filter_idc > 2) {
			E_DEBUG("disable_deblocking_filter_idx %d is out of range\n", sh->disable_deblocking_filter_idc);
			return H264D_PARSING_ERROR;
		}
		if(sh->disable_deblocking_filter_idc != 1){
			sh->slice_alpha_c0_offset_div2 = READ_SE(p_Par);
			sh->slice_beta_offset_div2 = READ_SE(p_Par);
			if (sh->slice_alpha_c0_offset_div2 < -6 || sh->slice_alpha_c0_offset_div2 > 6 || \
				sh->slice_beta_offset_div2 < -6 || sh->slice_beta_offset_div2 > 6) {
				E_DEBUG("alpha %d or beta %d is out of range\n", sh->slice_alpha_c0_offset_div2, sh->slice_beta_offset_div2);
				return H264D_PARSING_ERROR;
			}
		}
	}

    /* NOTE: no slice_group_change_cycle here, since slice group is not supported */

	return H264D_OK;
}


/*!
 ************************************************************************
 * \brief
 *    To calculate the poc values
 *        based upon JVT-F100d2
 *  POC200301: Until Jan 2003, this function will calculate the correct POC
 *    values, but the management of POCs in buffered pictures may need more work.
 * \return
 *    none
 ************************************************************************
 */
void decode_poc(void *ptVid)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
	SeqParameterSet *sps = p_Vid->active_sps;
	SliceHeader *sh = p_Vid->slice_hdr;
	int MaxPicOrderCntLsb = (1 << (sps->log2_max_pic_order_cnt_lsb));
	unsigned int MaxFrameNum = (1 << (sps->log2_max_frame_num));
	int i;
	int absFrameNum, expectedPicOrderCnt, expectedDeltaPerPicOrderCntCycle;
	int picOrderCntCycleCnt, frameNumInPicOrderCntCycle;
	int tempPicOrderCnt;

	switch (sps->pic_order_cnt_type)
	{
	case 0: /* POC MODE 0: set POC explicitly in each slice header */
		// 1st
		if(p_Vid->idr_flag)
			p_Vid->prevPicOrderCntMsb = p_Vid->prevPicOrderCntLsb = 0;
		else
		{
			if (p_Vid->last_has_mmco_5)
			{
				if (p_Vid->last_pic_bottom_field)
				{
					p_Vid->prevPicOrderCntMsb = 0;
					p_Vid->prevPicOrderCntLsb = 0;
				}
				else
				{
					p_Vid->prevPicOrderCntMsb = 0;
					p_Vid->prevPicOrderCntLsb = p_Vid->toppoc;
				}
			}
		}
		/* Calculate the MSBs of current picture */
		if ((int)sh->pic_order_cnt_lsb < p_Vid->prevPicOrderCntLsb && (p_Vid->prevPicOrderCntLsb - (int)sh->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb/2))
			p_Vid->PicOrderCntMsb = p_Vid->prevPicOrderCntMsb + MaxPicOrderCntLsb;
		else if ((int)sh->pic_order_cnt_lsb > p_Vid->prevPicOrderCntLsb && ((int)sh->pic_order_cnt_lsb - p_Vid->prevPicOrderCntLsb) > (MaxPicOrderCntLsb/2))
			p_Vid->PicOrderCntMsb = p_Vid->prevPicOrderCntMsb - MaxPicOrderCntLsb;
		else
			p_Vid->PicOrderCntMsb = p_Vid->prevPicOrderCntMsb;

		// 2nd
		if(p_Vid->field_pic_flag==0) /* frame pic */
		{
			p_Vid->toppoc = p_Vid->PicOrderCntMsb + sh->pic_order_cnt_lsb;
			p_Vid->bottompoc = p_Vid->toppoc + sh->delta_pic_order_cnt_bottom;
			p_Vid->ThisPOC = p_Vid->framepoc = (p_Vid->toppoc < p_Vid->bottompoc)? p_Vid->toppoc : p_Vid->bottompoc;
		}
		else if (sh->bottom_field_flag == FALSE)   /* top field */
			p_Vid->ThisPOC = p_Vid->toppoc = p_Vid->PicOrderCntMsb + sh->pic_order_cnt_lsb;
		else                                       /* bottom field */
			p_Vid->ThisPOC = p_Vid->bottompoc = p_Vid->PicOrderCntMsb + sh->pic_order_cnt_lsb;
		p_Vid->framepoc = p_Vid->ThisPOC;
		
		if (p_Vid->frame_num != p_Vid->prevFrameNum)
			p_Vid->prevFrameNum = p_Vid->frame_num;
		if (p_Vid->nalu.nal_ref_idc)
		{
			p_Vid->prevPicOrderCntLsb = sh->pic_order_cnt_lsb;
			p_Vid->prevPicOrderCntMsb = p_Vid->PicOrderCntMsb;
		}
		break;
    case 1: /* POC MODE 1: set expected increments in SPS; only send a delta if there is any change to expected order */
        if (p_Vid->last_has_mmco_5){
            p_Vid->prevFrameNumOffset = 0;
            p_Vid->prevFrameNum = 0;
        }
		// 1st
		if (p_Vid->idr_flag)
			p_Vid->FrameNumOffset = 0;
		else if (p_Vid->prevFrameNum > p_Vid->frame_num)
			p_Vid->FrameNumOffset = p_Vid->prevFrameNumOffset + MaxFrameNum;
		else
			p_Vid->FrameNumOffset = p_Vid->prevFrameNumOffset;

		// 2nd
		if (sps->num_ref_frames_in_pic_order_cnt_cycle)
			absFrameNum = p_Vid->FrameNumOffset + p_Vid->frame_num;
		else
			absFrameNum = 0;
		if (p_Vid->nalu.nal_ref_idc==0 && absFrameNum>0)
			absFrameNum--;

		// 4th
		expectedDeltaPerPicOrderCntCycle = 0;
		for (i = 0; i<sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			expectedDeltaPerPicOrderCntCycle += sps->offset_for_ref_frame[i];

		// 3rd and 5th
		if (absFrameNum>0)
		{
			picOrderCntCycleCnt = (absFrameNum-1)/sps->num_ref_frames_in_pic_order_cnt_cycle;
			frameNumInPicOrderCntCycle = (absFrameNum-1)%sps->num_ref_frames_in_pic_order_cnt_cycle;
			expectedPicOrderCnt = picOrderCntCycleCnt * expectedDeltaPerPicOrderCntCycle;
			for (i = 0; i<=frameNumInPicOrderCntCycle; i++)
				expectedPicOrderCnt += sps->offset_for_ref_frame[i];
		}
		else
			expectedPicOrderCnt = 0;
		if (p_Vid->nalu.nal_ref_idc==0)
			expectedPicOrderCnt += sps->offset_for_non_ref_pic;

		// 6th
		if (!p_Vid->field_pic_flag)
		{
			p_Vid->toppoc = expectedPicOrderCnt + sh->delta_pic_order_cnt[0];
			p_Vid->bottompoc = p_Vid->toppoc + sps->offset_for_top_to_bottom_field + sh->delta_pic_order_cnt[1];
			p_Vid->ThisPOC = p_Vid->framepoc = (p_Vid->toppoc < p_Vid->bottompoc)? p_Vid->toppoc : p_Vid->bottompoc;
		}
		else if (sh->bottom_field_flag == FALSE)
			p_Vid->ThisPOC = p_Vid->toppoc = expectedPicOrderCnt + sh->delta_pic_order_cnt[0];
		else
			p_Vid->ThisPOC = p_Vid->bottompoc = expectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sh->delta_pic_order_cnt[0];
		p_Vid->framepoc = p_Vid->ThisPOC;

		p_Vid->prevFrameNum = p_Vid->frame_num;
		p_Vid->prevFrameNumOffset = p_Vid->FrameNumOffset;
		break;
	case 2: /* POC MODE 2: display order same as decoding order. POC is derived from frame_num */
		if (p_Vid->idr_flag){ /* IDR picture */
			p_Vid->FrameNumOffset = 0; /* first pic of IDRGOP */
			p_Vid->ThisPOC = p_Vid->framepoc = p_Vid->toppoc = p_Vid->bottompoc = 0;
		} else {
			if (p_Vid->last_has_mmco_5){
				p_Vid->prevFrameNumOffset = 0;
				p_Vid->prevFrameNum = 0;
			}
			
			if (p_Vid->prevFrameNum > p_Vid->frame_num)
				p_Vid->FrameNumOffset = p_Vid->prevFrameNumOffset + MaxFrameNum;
			else
				p_Vid->FrameNumOffset = p_Vid->prevFrameNumOffset;

			tempPicOrderCnt = p_Vid->FrameNumOffset + p_Vid->frame_num;
			if (!p_Vid->nalu.nal_ref_idc)
				p_Vid->ThisPOC = (2*tempPicOrderCnt - 1);
			else
				p_Vid->ThisPOC = 2*tempPicOrderCnt;

			if (!p_Vid->field_pic_flag)
				p_Vid->toppoc = p_Vid->bottompoc = p_Vid->framepoc = p_Vid->ThisPOC;
			else if (sh->bottom_field_flag)
				p_Vid->bottompoc = p_Vid->framepoc = p_Vid->ThisPOC;
			else
				p_Vid->toppoc = p_Vid->framepoc = p_Vid->ThisPOC;
		}

		p_Vid->prevFrameNum = p_Vid->frame_num;
		p_Vid->prevFrameNumOffset = p_Vid->FrameNumOffset;
        break;
	default:
		break;
	}
}

