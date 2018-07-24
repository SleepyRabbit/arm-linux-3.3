#ifdef LINUX
	#include <linux/string.h>
	#include <linux/kernel.h>
#else
	#include <string.h>
#endif
#include "refbuffer.h"
#include "global.h"

int dump_list(void *param)
{
#ifdef LINUX
    VideoParams *p_Vid = (VideoParams*)param;
    int slice_type = p_Vid->dec_picture->slice_type;
    int i;

    if (I_SLICE == slice_type)
        printk("I slice poc %d\n", p_Vid->dec_picture->poc);
    else {
        if (P_SLICE == slice_type) {
            printk("P slice poc %d: [ref %d]", p_Vid->dec_picture->poc, p_Vid->list_size[0]);
            for (i = 0; i<p_Vid->list_size[0]; i++)
                printk(" poc%d(0x%x[%d])", p_Vid->ref_list[0][i]->poc, (uint32_t)p_Vid->ref_list[0][i]->recpic_luma_phy, p_Vid->ref_list[0][i]->phy_index);
            printk("\n");
        }
        else if (B_SLICE == slice_type)
            printk("B slice poc %d: [l0] %d, [l1] %d\n", p_Vid->dec_picture->poc, p_Vid->ref_list[0][0]->poc, p_Vid->ref_list[1][0]->poc);
    }
#endif
    return 0;
}

void dump_dpb(void *pvid)
{
#ifdef LINUX
    VideoParams *p_Vid = (VideoParams*)pvid;
    DecodedPictureBuffer *p_Dpb = p_Vid->p_Dpb;
    int i;

    for (i =0; i<p_Dpb->used_size; i++) {
        printk("fs %d: used %d, ref %d\n", i, p_Dpb->fs[i]->is_used, p_Dpb->fs[i]->is_ref);
        if (3 == p_Dpb->fs[i]->is_used)
            printk("\tfrmae: poc %d (%08x, %08x)\n", p_Dpb->fs[i]->frame->poc_lsb, (uint32_t)p_Dpb->fs[i]->frame->recpic_luma_phy, (uint32_t)p_Dpb->fs[i]->frame->recpic_chroma_phy);
    }
#endif
}

int init_lists(void *vid, int slice_type)
{
	VideoParams *p_Vid = (VideoParams*)vid;
	//SliceHeader *sh = p_Vid->currSlice;
	DecodedPictureBuffer *p_Dpb = p_Vid->p_Dpb;
	StorablePicture *sp_l0, *sp_l1, *sp_top_l0, *sp_btm_l0, *sp_top_l1, *sp_btm_l1;
	StorablePicture *pic = p_Vid->dec_picture;
	int min_poc, max_poc, min_poc_b, max_poc_b;
	int i, top_idx, bottom_idx;
 	uint32_t list0idx;

	if (slice_type==I_SLICE){
		p_Vid->list_size[0] = p_Vid->list_size[1] = 0;
		return H264E_OK;
	}

	if (slice_type == P_SLICE)
	{
		list0idx = 0;
		if (!p_Vid->field_pic_flag){
			for (i = p_Dpb->used_size-1; i>=0; i--)
				p_Vid->ref_list[0][list0idx++] = p_Dpb->fs[i]->frame;
			p_Vid->list_size[0] = iMin(list0idx, p_Vid->ref_l0_num);
		}
		else {	// field
			top_idx = p_Dpb->used_size-1;
			bottom_idx = p_Dpb->used_size-1;
			if (p_Vid->dec_picture->structure == TOP_FIELD) {
				while ((top_idx>=0 || bottom_idx>=0) && list0idx<p_Vid->ref_l0_num) {
					while (top_idx>=0) {
						if (p_Dpb->fs[top_idx]->is_ref & 1) {
							p_Vid->ref_list[0][list0idx++] = p_Dpb->fs[top_idx--]->topField;
							break;
						}
						top_idx--;
					}
					while (bottom_idx >= 0) {
						if (p_Dpb->fs[bottom_idx]->is_ref & 2) {
							p_Vid->ref_list[0][list0idx++] = p_Dpb->fs[bottom_idx--]->btmField;
							break;
						}
						bottom_idx--;
					}
				}
			}
			else {
				while ((top_idx>=0 || bottom_idx>=0) && list0idx<p_Vid->ref_l0_num) {
					while (bottom_idx >= 0) {
						if (p_Dpb->fs[bottom_idx]->is_ref & 2) {
							p_Vid->ref_list[0][list0idx++] = p_Dpb->fs[bottom_idx--]->btmField;
							break;
						}
						bottom_idx--;
					}
					while (top_idx>=0) {
						if (p_Dpb->fs[top_idx]->is_ref & 1) {
							p_Vid->ref_list[0][list0idx++] = p_Dpb->fs[top_idx--]->topField;
							break;
						}
						top_idx--;
					}
				}
			}
			p_Vid->list_size[0] = iMin(list0idx, p_Vid->ref_l0_num);
		}	// end of P field
	}
	else {// B slice
		list0idx = 0;
		if (!p_Vid->field_pic_flag) {
			max_poc = INT_MIN;
			min_poc = INT_MAX;
			sp_l0 = sp_l1 = NULL;
			for (i = 0; i<p_Dpb->used_size; i++) {
				if (p_Dpb->fs[i]->poc < pic->poc && p_Dpb->fs[i]->poc > max_poc) {
					sp_l0 = p_Dpb->fs[i]->frame;
					max_poc = sp_l0->poc;
				}
				else if (p_Dpb->fs[i]->poc > pic->poc && p_Dpb->fs[i]->poc < min_poc) {
					sp_l1 = p_Dpb->fs[i]->frame;
					min_poc = sp_l1->poc;
				}
			}
			if (sp_l0 == NULL && sp_l1 == NULL)
				return H264E_ERR_REF;
			if (sp_l0)
				p_Vid->ref_list[0][0] = sp_l0;
			else
				p_Vid->ref_list[0][0] = sp_l1;
			if (sp_l1)
				p_Vid->ref_list[1][0] = sp_l1;
			else
				p_Vid->ref_list[1][0] = sp_l0;
		}
		else	// field
		{
			max_poc = max_poc_b = INT_MIN;
			min_poc = min_poc_b = INT_MAX;
			sp_top_l0 = sp_btm_l0 = sp_top_l1 = sp_btm_l1 = NULL;
			for (i = 0; i<p_Dpb->used_size; i++) {
				if (p_Dpb->fs[i]->is_ref & 1) {
					if (p_Dpb->fs[i]->topField->poc < pic->poc && p_Dpb->fs[i]->topField->poc > max_poc) {
						sp_top_l0 = p_Dpb->fs[i]->topField;
						max_poc = sp_top_l0->poc;
					}
					if (p_Dpb->fs[i]->topField->poc > pic->poc && p_Dpb->fs[i]->topField->poc < min_poc) {
						sp_top_l1 = p_Dpb->fs[i]->topField;
						min_poc = sp_top_l1->poc;
					}
				}
				if (p_Dpb->fs[i]->is_ref & 2) {
					if (p_Dpb->fs[i]->btmField->poc < pic->poc && p_Dpb->fs[i]->btmField->poc > max_poc_b) {
						sp_btm_l0 = p_Dpb->fs[i]->btmField;
						max_poc_b = sp_btm_l0->poc;
					}
					if (p_Dpb->fs[i]->btmField->poc > pic->poc && p_Dpb->fs[i]->btmField->poc < min_poc_b) {
						sp_btm_l1 = p_Dpb->fs[i]->btmField;
						min_poc_b = sp_btm_l1->poc;
					}
				}
			}

/*****************************************************************
    [ref 0]    [ref 1]    [curPic]    [ref 2]

 case 1: 3 reference
    L0: ref 0 top -> ref 0 bottom -> ref 1 top -> ref 1 bottom -> ref 2 top -> ref 2 bottom
    L1: ref 2 top -> ref 2 bottom -> ref 1 top -> ref 1 bottom -> ref 0 top -> ref 2 bottom

 case 2: 2 reference
    L0: ref 1 top -> ref 1 bottom -> ref 2 top -> ref 2 bottom
    L2: ref 2 top -> ref 2 bottom -> ref 1 top -> ref 1 bottom

 case 3: 1 reference
    top                                             bottom
    ref 1
    L0: ref 1 top -> ref 1 bottom       L0: ref 1 bottom -> ref 1 top
    L1: ref 1 bottom -> ref 1 top       L1: ref 1 top -> ref 1 bottom

    top
    ref 2                                           bottom
    L0: ref 2 top -> ref 2 bottom       L0: ref 2 bottom -> ref 2 top
    L1: ref 2 bottom -> ref 2 top       L1: ref 2 top -> ref 2 bottom
 
******************************************************************/
			if (sp_top_l0 == NULL)
				sp_top_l0 = sp_btm_l0;
			if (sp_btm_l0 == NULL)
				sp_btm_l0 = sp_top_l0;
			if (sp_top_l1 == NULL)
				sp_top_l1 = sp_btm_l1;
			if (sp_btm_l1 == NULL)
				sp_btm_l1 = sp_top_l1;

			if (p_Vid->dec_picture->structure == TOP_FIELD) {
				if (sp_top_l0)
					p_Vid->ref_list[0][0] = sp_top_l0;
				else {
					if (sp_top_l1)
						p_Vid->ref_list[0][0] = sp_top_l1;
					else
						return H264E_ERR_REF;
				}
				if (sp_top_l1) {
                    if (p_Vid->ref_list[0][0] == sp_top_l1)
                        p_Vid->ref_list[1][0] = sp_btm_l1;
                    else
    					p_Vid->ref_list[1][0] = sp_top_l1;
                }
				else {
					if (sp_top_l0)
						p_Vid->ref_list[1][0] = sp_top_l0;
					else 
						return H264E_ERR_REF;
				}
			}
			else if (p_Vid->dec_picture->structure == BOTTOM_FIELD)
			{
				if (sp_btm_l0)
					p_Vid->ref_list[0][0] = sp_btm_l0;
				else {
					if (sp_btm_l1)
						p_Vid->ref_list[0][0] = sp_btm_l1;
					else
						return H264E_ERR_REF;
				}
				if (sp_btm_l1) {
                    if (p_Vid->ref_list[0][0] == sp_btm_l1)
                        p_Vid->ref_list[1][0] = sp_top_l1;
                    else
    					p_Vid->ref_list[1][0] = sp_btm_l1;
                }
				else {
					if (sp_btm_l0)
						p_Vid->ref_list[1][0] = sp_btm_l0;
					else
						return H264E_ERR_REF;
				}
			}
		}
		p_Vid->list_size[0] = p_Vid->list_size[1] = 1;
	}
    //dump_list(p_Vid);

	return H264E_OK;
}

int getDpbSize(SeqParameterSet *sps)
{
	int pic_size = (sps->pic_width_in_mbs_minus1 + 1)*(sps->pic_height_in_map_units_minus1 + 1)*(sps->frame_mbs_only_flag?1:2) * 384;
	int size = 0;
	switch (sps->level_idc)
	{
	case 9:
		size = 152064;
		break;
	case 10:
		size = 152064;
		break;
	case 11:
		size = 345600;
		break;
	case 12:
		size = 912384;
		break;
	case 13:
		size = 912384;
		break;
	case 20:
		size = 912384;
		break;
	case 21:
		size = 1824768;
		break;
	case 22:
		size = 3110400;
		break;
	case 30:
		size = 3110400;
		break;
	case 31:
		size = 6912000;
		break;
	case 32:
		size = 7864320;
		break;
	case 40:
		size = 12582912;
		break;
	case 41:
		size = 12582912;
		break;
	case 42:
		size = 13369344;
		break;
	case 50:
		size = 42393600;
		break;
	case 51:
		size = 70778880;
		break;
	default:
		size = 0;
		break;
	}
	size /= pic_size;
	size = (size<16 ? size : 16);
    // Tuba 20150316: at least one dbg buffer
    if (0 == size)
        size = 1;
	return size;
}

static void remove_picture_from_dpb(VideoParams *p_Vid, DecodedPictureBuffer *p_Dpb, int pos)
{
	FrameStore *fs = p_Dpb->fs[pos];
	FrameStore *tmp_fs;
	int i;

	p_Vid->release_buffer_idx = fs->phy_index;
	//p_Vid->release_buffer_idx[p_Vid->release_num++] = fs->frame->phy_index;

	tmp_fs = p_Dpb->fs[pos];
	tmp_fs->is_ref = 0;
	tmp_fs->is_used = 0;
	for (i = pos; i<(int)(p_Dpb->used_size-1); i++)
		p_Dpb->fs[i] = p_Dpb->fs[i+1];
	
	p_Dpb->fs[p_Dpb->used_size-1] = tmp_fs;
	p_Dpb->used_size--;
}

static void sliding_window_memory_management(VideoParams *p_Vid, DecodedPictureBuffer *p_Dpb, int num_ref_frames, StorablePicture *pic)
{
	uint32_t i;

	if (p_Dpb->used_size == num_ref_frames) {
		for (i = 0; i<p_Dpb->used_size; i++) {
			if (p_Dpb->fs[i]->is_ref) {
				remove_picture_from_dpb(p_Vid, p_Vid->p_Dpb, i);
				break;
			}
		}
	}
	//update_stref_list(p_Dpb);
	//pic->is_long_term = 0;
}

static void idr_memory_management(VideoParams *p_Vid)
{
	DecodedPictureBuffer *p_Dpb = p_Vid->p_Dpb;
    int i;

	//flush_dpb(p_Vid);
	//memset(p_Dpb, 0, sizeof(DecodedPictureBuffer));
    for (i = 0; i<p_Dpb->used_size; i++) {
        p_Dpb->fs[i]->is_ref = 0;
        p_Dpb->fs[i]->is_used = 0;
    }
	p_Vid->release_buffer_idx = RELEASE_ALL_BUFFER;
	p_Dpb->used_size = 0;

	p_Dpb->last_picture = NULL;
	//p_Dpb->st_ref_size = 0;
	//p_Dpb->lt_ref_size = 0;

	//p_Dpb->max_long_term_pic_idx = -1;
	//pic->is_long_term = 0;
}

static int insert_picture_in_dpb(VideoParams *p_Vid, FrameStore *fs, StorablePicture **pic)
{
	StorablePicture *tmp_sp;
	int pic_struct;

	switch ((*pic)->structure){
	case FRAME:
		tmp_sp = fs->frame;
		fs->frame = *pic;
		*pic = tmp_sp;

		fs->is_used = 3;
		if (fs->frame->used_for_ref){
			fs->is_ref = 3;
			/*
			if ((*pic)->is_long_term){
				fs->is_long = 3;
				fs->long_term_frame_idx = (*pic)->long_term_frame_idx;
			}
			*/
		}
		//dpb_split_field(p_Vid, fs);
		fs->poc = fs->frame->poc;
		fs->frame_num = fs->frame->frame_num;
		fs->phy_index = fs->frame->phy_index;
		pic_struct = FRAME;
		break;
	case TOP_FIELD:
		memcpy((void *)fs->topField, *pic, sizeof(StorablePicture));
		// bottom field use the same field information as top field
		/*
		tmp_sp = fs->topField;
		fs->topField = *pic;
		*pic = tmp_sp;
		*/

		fs->is_used |= 1;
		if (fs->topField->used_for_ref)
			fs->is_ref |= 1;

		if (fs->is_used == 3)
			fs->poc = iMin(fs->topField->poc, fs->btmField->poc);
		else
			fs->poc = fs->topField->poc;
		fs->frame_num = fs->topField->frame_num;
		fs->phy_index = fs->topField->phy_index;
		pic_struct = TOP_FIELD;
		break;
	case BOTTOM_FIELD:
		tmp_sp = fs->btmField;
		fs->btmField = *pic;
		*pic = tmp_sp;

		fs->is_used |= 2;
		if (fs->btmField->used_for_ref)
			fs->is_ref |= 2;

		if (fs->is_used == 3)
			fs->poc = iMin(fs->topField->poc, fs->btmField->poc);
		else
			fs->poc = fs->btmField->poc;
		fs->frame_num = fs->btmField->frame_num;
		fs->phy_index = fs->btmField->phy_index;
		pic_struct = BOTTOM_FIELD;
		break;
	default:
		// error
		pic_struct = -1;
		break;
	}

	return pic_struct;
}


int store_picture_in_dpb(void *vid, StorablePicture **pic)
{
	VideoParams *p_Vid = (VideoParams*)vid;
	int pic_struct;
	DecodedPictureBuffer *p_Dpb = p_Vid->p_Dpb;
	//p_Vid->last_pic_bottom_field = ((*pic)->structure == BOTTOM_FIELD);

	if ((*pic)->used_for_ref == 0) {	// not store
		if ((*pic)->structure == BOTTOM_FIELD || (*pic)->structure == FRAME) {
			//p_Vid->release_buffer_idx[p_Vid->release_num++] = (*pic)->phy_index;
			p_Vid->release_buffer_idx = (*pic)->phy_index;
			return FRAME_DONE;
		}
		else
			return FIELD_DONE;
	}

	if ((*pic)->idr_flag)
		idr_memory_management(p_Vid);

	if ((*pic)->structure == BOTTOM_FIELD && p_Dpb->last_picture) {
		insert_picture_in_dpb(p_Vid, p_Dpb->last_picture, pic);
		p_Vid->dec_picture = (*pic);
		p_Dpb->last_picture = NULL;
		return FRAME_DONE;
	}

	// if not reference ==> remove
	if ((!(*pic)->idr_flag) && ((*pic)->used_for_ref))
		sliding_window_memory_management(p_Vid, p_Dpb, p_Vid->active_sps->num_ref_frames, *pic);

	pic_struct = insert_picture_in_dpb(p_Vid, p_Dpb->fs[p_Dpb->used_size], pic);
	p_Vid->dec_picture = (*pic);
	if (pic_struct != FRAME)
		p_Dpb->last_picture = p_Dpb->fs[p_Dpb->used_size];
	else
		p_Dpb->last_picture = NULL;
	p_Dpb->used_size++;
//dump_list(p_Vid);

	if (pic_struct == FRAME)
		return FRAME_DONE;

	return FIELD_DONE;
}					

int flush_all_picture_in_dbp(void *vid)
{
    VideoParams *p_Vid = (VideoParams *)vid;
    idr_memory_management(p_Vid);
    return 0;
}

