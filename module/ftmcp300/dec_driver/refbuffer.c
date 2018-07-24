#ifdef LINUX
	#include <linux/string.h>
	#include <linux/kernel.h>
#else
	#include <string.h>
#endif
#include "refbuffer.h"
#if defined(LINUX) & defined(VG_INTERFACE)
#include "log.h"
#endif
#include "global.h"
#include "slicehdr.h"
#include "params.h"

#if DISABLE_QSORT
#define q_sort(pbase, total_elems, size, cmp) 
#else
extern void q_sort(void *const pbase, int total_elems, int size, int (*cmp)(const void*, const void*));
#endif
extern void dump_dpb_info(void *ptVid, int mem_info);

static int adaptive_memory_management(VideoParams *p_Vid, StorablePicture *pic);
static int idr_memory_management(VideoParams *p_Vid, StorablePicture *pic);
static void update_stref_list(DecodedPictureBuffer *listD);
static void update_ltref_list(DecodedPictureBuffer *listD);
static void get_smallest_output_poc(DecodedPictureBuffer *listD, int *poc, int *pos);
//static void remove_picture_from_dpb(VideoParams *p_Vid, DecodedPictureBuffer *listD, int pos, int is_rec);


#ifndef VG_INTERFACE
/*!
 ************************************************************************
 * \brief
 *    initialize decoded picture buffer with sane values.
 *
 ************************************************************************
 */
void init_dpb(void *ptDecHandle, int is_rec)
{
    VideoParams *p_Vid = ((DecoderParams *)ptDecHandle)->p_Vid;
    DecodedPictureBuffer *p_Dpb = p_Vid->listD;

    while(p_Dpb->used_size != 0){
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove from dpb at init_dpb:%d\n", p_Dpb->fs[0]->phy_index);
        remove_picture_from_dpb(p_Vid, p_Dpb, 0, is_rec);
    }
    update_stref_list(p_Dpb);
    update_ltref_list(p_Dpb);
    p_Dpb->used_size = 0;
    p_Dpb->lt_ref_size = 0;
    p_Dpb->st_ref_size = 0;
}
#endif


/*!
 ************************************************************************
 * \brief
 *    Initialize memory for a stored picture.
 *
 * \param ptPic
 *      StorablePicture structure
 * \param structure
 *    picture structure
 ************************************************************************
 */
void alloc_storable_picture(StorablePicture *ptPic, PictureStructure structure)
{
	memset(ptPic, 0, sizeof(StorablePicture));
	ptPic->structure = structure;
	ptPic->valid = 1;
	ptPic->sp_frame = NULL;
	ptPic->sp_top = NULL;
	ptPic->sp_btm = NULL;	
}


/*!
 ************************************************************************
 * \brief
 *    Clear memory for a stored picture.
 *
 * \param ptPic
 *      StorablePicture structure
 ************************************************************************
 */
static void free_storable_picture(VideoParams *p_Vid, StorablePicture *ptPic)
{
	memset(ptPic, 0, sizeof(StorablePicture));  /* valid = 0 */
}


/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is long term reference picture
 *
 ************************************************************************
 */
__inline int is_long_ref(StorablePicture *s)
{
	return ((s->used_for_ref) && (s->is_long_term));
}


/*!
 ************************************************************************
 * \brief
 *    returns true, if picture is short term reference picture
 *
 ************************************************************************
 */
__inline int is_short_ref(StorablePicture *s)
{
	return ((s->used_for_ref) && (!s->is_long_term));
}


/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for short-term reference
 ************************************************************************
 */
static int is_short_term_reference(FrameStore* fs)
{
	if (fs->is_used==3) {
		if (fs->frame->valid)
			if ((fs->frame->used_for_ref) && (!fs->frame->is_long_term))
				return 1;
    }

	if (fs->is_used & 1) {
		if (fs->topField->valid)
			if ((fs->topField->used_for_ref) && (!fs->topField->is_long_term))
				return 1;
    }

	if (fs->is_used & 2) {
		if (fs->btmField->valid)
			if ((fs->btmField->used_for_ref) && (!fs->btmField->is_long_term))
				return 1;
    }
	return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for short-term reference
 ************************************************************************
 */
static int is_long_term_reference(FrameStore* fs)
{
	if (fs->is_used==3) {
		if (fs->frame->valid)
			if ((fs->frame->used_for_ref) && (fs->frame->is_long_term))
				return 1;
    }

	if (fs->is_used & 1) {
		if (fs->topField->valid)
			if ((fs->topField->used_for_ref) && (fs->topField->is_long_term))
				return 1;
    }

	if (fs->is_used & 2) {
		if (fs->btmField->valid)
			if ((fs->btmField->used_for_ref) && (fs->btmField->is_long_term))
				return 1;
    }
	return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Check if one of the frames/fields in frame store is used for reference
 ************************************************************************
 */
static int is_used_for_reference(FrameStore *fs)
{
	if (fs->is_ref)
		return 1;

	if (fs->is_used == 3)
		if (fs->frame->used_for_ref)
			return 1;

	if (fs->is_used & 1)
		if (fs->topField->valid)
			if (fs->topField->used_for_ref)
				return 1;

	if (fs->is_used & 2)
		if (fs->btmField->valid)
			if (fs->btmField->used_for_ref)
				return 1;

	return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Generate a frame from top and bottom fields
 ************************************************************************
 */
static void dpb_combine_field(FrameStore *fs)
{
	alloc_storable_picture(fs->frame, FRAME);


	if (fs->btmField->mbinfo_addr > fs->topField->mbinfo_addr) /* mbinfo_addr may be null, but the bottom's mbinfo address will be greater than top's */
    {
		fs->frame->phy_index = fs->topField->phy_index;
		fs->frame->refpic_addr = fs->topField->refpic_addr;
		fs->frame->refpic_addr_phy = fs->topField->refpic_addr_phy;
		fs->frame->mbinfo_addr = fs->topField->mbinfo_addr;
		fs->frame->mbinfo_addr_phy = fs->topField->mbinfo_addr_phy;
		fs->frame->slice_type = fs->topField->slice_type;
		fs->frame->bottom_field_first = fs->topField->bottom_field_first;
	}
	else {
		fs->frame->phy_index = fs->btmField->phy_index;
		fs->frame->refpic_addr = fs->btmField->refpic_addr;
		fs->frame->refpic_addr_phy = fs->btmField->refpic_addr_phy;
		fs->frame->mbinfo_addr = fs->btmField->mbinfo_addr;
		fs->frame->mbinfo_addr_phy = fs->btmField->mbinfo_addr_phy;
		fs->frame->slice_type = fs->btmField->slice_type;
		fs->frame->bottom_field_first = fs->btmField->bottom_field_first;
	}
	
	fs->poc = fs->frame->poc = fs->frame->framepoc = (fs->topField->poc < fs->btmField->poc ? fs->topField->poc : fs->btmField->poc);
	fs->btmField->framepoc = fs->topField->framepoc = fs->frame->poc;
	fs->btmField->toppoc = fs->frame->toppoc = fs->topField->poc;
	fs->topField->botpoc = fs->frame->botpoc = fs->btmField->poc;
	fs->frame->used_for_ref = (fs->topField->used_for_ref && fs->btmField->used_for_ref);
	fs->frame->is_long_term = (fs->topField->is_long_term && fs->btmField->is_long_term);
	if (fs->frame->is_long_term)
		fs->frame->long_term_frame_idx = fs->long_term_frame_idx;
	fs->frame->coded_frame = 0;
	fs->frame->sp_frame = fs->frame;
	fs->frame->sp_top = fs->topField;
	fs->frame->sp_btm = fs->btmField;
	fs->topField->sp_frame = fs->btmField->sp_frame = fs->frame;
	fs->topField->sp_top = fs->btmField->sp_top = fs->topField;
	fs->topField->sp_btm = fs->btmField->sp_btm = fs->btmField;
	fs->frame->valid = 1;
	fs->is_return_output = 0;
}


/*!
 ************************************************************************
 * \brief
 *    Extract top field from a frame
 ************************************************************************
 */
static void dpb_split_field(VideoParams *p_Vid, FrameStore *fs)
{
	fs->poc = fs->frame->poc;

	if (!p_Vid->active_sps->frame_mbs_only_flag){
		alloc_storable_picture(fs->topField, TOP_FIELD);
		alloc_storable_picture(fs->btmField, BOTTOM_FIELD);

		if (fs->frame->toppoc < fs->frame->botpoc){
			fs->topField->phy_index = fs->frame->phy_index;
			fs->btmField->phy_index = fs->frame->phy_index;// + 1;
		}
		else {
			fs->btmField->phy_index = fs->frame->phy_index;
			fs->topField->phy_index = fs->frame->phy_index;// + 1;
		}

		fs->topField->refpic_addr = fs->btmField->refpic_addr = fs->frame->refpic_addr;
		fs->topField->refpic_addr_phy = fs->btmField->refpic_addr_phy = fs->frame->refpic_addr_phy;
		fs->topField->mbinfo_addr = fs->frame->mbinfo_addr;
		fs->topField->mbinfo_addr_phy = fs->frame->mbinfo_addr_phy;
		fs->btmField->mbinfo_addr = fs->frame->mbinfo_addr + p_Vid->frame_width*p_Vid->frame_height/2;
		fs->btmField->mbinfo_addr_phy = fs->frame->mbinfo_addr_phy + p_Vid->frame_width*p_Vid->frame_height/2;
		fs->topField->slice_type = fs->btmField->slice_type = fs->frame->slice_type;
		fs->topField->poc = fs->frame->toppoc;
		fs->btmField->poc = fs->frame->botpoc;
		fs->topField->framepoc = fs->frame->framepoc;
		fs->btmField->framepoc = fs->frame->framepoc;
		fs->topField->botpoc = fs->btmField->botpoc = fs->frame->botpoc;
		fs->topField->toppoc = fs->btmField->toppoc = fs->frame->toppoc;

		fs->topField->used_for_ref = fs->btmField->used_for_ref = fs->frame->used_for_ref;
		fs->topField->is_long_term = fs->btmField->is_long_term = fs->frame->is_long_term;
		fs->long_term_frame_idx = fs->topField->long_term_frame_idx = fs->btmField->long_term_frame_idx = fs->frame->long_term_frame_idx;
		fs->topField->coded_frame = fs->btmField->coded_frame = 1;
		fs->topField->non_existing = fs->btmField->non_existing = fs->frame->non_existing;
		fs->topField->bottom_field_first = fs->btmField->bottom_field_first = fs->frame->bottom_field_first;

		fs->frame->sp_frame = fs->topField->sp_frame = fs->btmField->sp_frame = fs->frame;
		fs->frame->sp_top = fs->topField->sp_top = fs->btmField->sp_top = fs->topField;
		fs->frame->sp_btm = fs->topField->sp_btm = fs->btmField->sp_btm = fs->btmField;

		//fs->topField->org_structure = fs->btmField->org_structure = FRAME;
		fs->topField->valid = fs->btmField->valid = 1;
	}else{
		fs->topField->valid = 0;
		fs->btmField->valid = 0;
		fs->topField->sp_top = fs->topField->sp_btm = fs->topField->sp_frame = NULL;
		fs->btmField->sp_top = fs->btmField->sp_btm = fs->btmField->sp_frame = NULL;
		fs->frame->sp_top = NULL;
		fs->frame->sp_btm = NULL;
		fs->frame->sp_frame = fs->frame;
	}
}


/*!
 ************************************************************************
 * \brief
 *    mark FrameStore unused for reference
 *
 ************************************************************************
 */
static void unmark_for_reference(FrameStore *fs)
{
	if (fs->is_used & 1)
		if (fs->topField->valid)
			fs->topField->used_for_ref = 0;

	if (fs->is_used & 2)
		if (fs->btmField->valid)
			fs->btmField->used_for_ref = 0;

	if (fs->is_used == 3){
		if (fs->topField->valid && fs->btmField->valid){
			fs->topField->used_for_ref = 0;
			fs->btmField->used_for_ref = 0;
		}
		fs->frame->used_for_ref = 0;
	}

    //printk("unmark_for_reference:%d\n", fs->phy_index);

	fs->is_ref = 0;
}


/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark long term picture unused
 ************************************************************************
 */
static void unmark_for_long_term_reference(FrameStore *fs)
{
	if (fs->is_used == 3){
		if (fs->topField->valid && fs->btmField->valid){
			fs->topField->used_for_ref = 0;
			fs->topField->is_long_term = 0;
			fs->btmField->used_for_ref = 0;
			fs->btmField->used_for_ref = 0;
		}
		fs->frame->used_for_ref = 0;
		fs->frame->is_long_term = 0;
	}
	else {
		if (fs->is_used & 1){
			if (fs->topField->valid){
				fs->topField->used_for_ref = 0;
				fs->topField->is_long_term = 0;
			}
		}
		if (fs->is_used & 2){
			if (fs->btmField->valid){
				fs->btmField->used_for_ref = 0;
				fs->btmField->is_long_term = 0;
			}
		}
	}

	fs->is_ref = 0;
	fs->is_long = 0;
}


/*!
 ************************************************************************
 * \brief
 *    Mark a long-term reference frame or complementary field pair unused for referemce
 ************************************************************************
 */
static int unmark_long_term_frame_for_reference_by_frame_idx(DecodedPictureBuffer *listD, int long_term_frame_idx)
{
	uint32_t i;
	int ret = 0;
	for (i = 0; i<listD->lt_ref_size; i++) {
		if (listD->fs_ltRef[i]->long_term_frame_idx == long_term_frame_idx) {
			unmark_for_long_term_reference(listD->fs_ltRef[i]);
			ret++;
//printk("unmark long (poc %d)\n", listD->fs_ltRef[i]->poc);
		}
	}
	return ret;
}


/*!
 ************************************************************************
 * \brief
 *    Mark a long-term reference field unused for reference only if it's not
 *    the complementary field of the picture indicated by picNumX
 ************************************************************************
 */
static int unmark_long_term_field_for_reference_by_frame_idx(VideoParams *p_Vid, PictureStructure structure, int long_term_frame_idx, int mark_current, unsigned curr_frame_num, int curr_pic_num)
{
	DecodedPictureBuffer *listD = p_Vid->listD;
	uint32_t i, MaxFrameNum;
	int ret = 0;

	MaxFrameNum = 1 << p_Vid->active_sps->log2_max_frame_num;
	if (curr_pic_num < 0)
		curr_pic_num += (2 * MaxFrameNum);

	for(i = 0; i<listD->lt_ref_size; i++){
		if (listD->fs_ltRef[i]->long_term_frame_idx == long_term_frame_idx){
//printk("find the long term fs poc%d[%d] long %d\n", listD->fs_ltRef[i]->poc, listD->fs_ltRef[i]->is_used, listD->fs_ltRef[i]->is_long);
			if (structure == TOP_FIELD) {
				if ((listD->fs_ltRef[i]->is_long == 3)) {
					unmark_for_long_term_reference(listD->fs_ltRef[i]);
					ret = 1;
				}
				else {
					if (listD->fs_ltRef[i]->is_long == 1) {
						unmark_for_long_term_reference(listD->fs_ltRef[i]);
						ret = 1;
					}
					else {
						if (mark_current){
							if (listD->last_picture){
								if ((listD->last_picture != listD->fs_ltRef[i]) || (listD->last_picture->frame_num != curr_frame_num)) {
									unmark_for_long_term_reference(listD->fs_ltRef[i]);
									ret = 1;
								}
							}
							else {
								unmark_for_long_term_reference(listD->fs_ltRef[i]);
								ret = 1;
							}
						}
						else {
							if (listD->fs_ltRef[i]->frame_num != (unsigned)(curr_pic_num/2)) {
								unmark_for_long_term_reference(listD->fs_ltRef[i]);
								ret = 1;
							}
						}
					}
				}
			}
			if (structure == BOTTOM_FIELD){
				if (listD->fs_ltRef[i]->is_long == 3) {
					unmark_for_long_term_reference(listD->fs_ltRef[i]);
					ret = 1;
				}
				else {
					if (listD->fs_ltRef[i]->is_long == 2) {
						unmark_for_long_term_reference(listD->fs_ltRef[i]);
						ret = 1;
					}
					else {
						if (mark_current){
							if (listD->last_picture){
								if ((listD->last_picture != listD->fs_ltRef[i]) ||  (listD->last_picture->frame_num != curr_frame_num)) {
									unmark_for_long_term_reference(listD->fs_ltRef[i]);
									ret = 1;
								}
							}
							else {
								unmark_for_long_term_reference(listD->fs_ltRef[i]);
								ret = 1;
							}
						}
						else {
							if (listD->fs_ltRef[i]->frame_num != (unsigned)(curr_pic_num/2)) {
								unmark_for_long_term_reference(listD->fs_ltRef[i]);
								ret = 1;
							}
						}
					}
				}
			}
		}
	}
	return ret;
}


/*!
 ************************************************************************
 * \brief
 *    mark a picture as long-term reference
 ************************************************************************
 */
static void mark_pic_long_term(DecodedPictureBuffer *listD, StorablePicture *pic, int long_term_frame_idx, int picNumX)
{
	unsigned i;
	int add_top, add_bottom;

	if (pic->structure == FRAME){
		for (i = 0; i<listD->st_ref_size; i++){
			if (listD->fs_stRef[i]->is_ref == 3){
				if ((!listD->fs_stRef[i]->frame->is_long_term) && (listD->fs_stRef[i]->frame->pic_num == picNumX)){
					listD->fs_stRef[i]->long_term_frame_idx = listD->fs_stRef[i]->frame->long_term_frame_idx = long_term_frame_idx;
					listD->fs_stRef[i]->frame->long_term_pic_num = long_term_frame_idx;
					listD->fs_stRef[i]->frame->is_long_term = 1;

					if (listD->fs_stRef[i]->topField && listD->fs_stRef[i]->btmField){
						listD->fs_stRef[i]->topField->long_term_frame_idx = listD->fs_stRef[i]->btmField->long_term_frame_idx  = long_term_frame_idx;
						listD->fs_stRef[i]->topField->long_term_pic_num = long_term_frame_idx;
						listD->fs_stRef[i]->btmField->long_term_pic_num = long_term_frame_idx;
						listD->fs_stRef[i]->topField->is_long_term = listD->fs_stRef[i]->btmField->is_long_term = 1;
					}
					listD->fs_stRef[i]->is_long = 3;
//printk("mark frame poc %d long\n", listD->fs_stRef[i]->poc);
					return;
				}
			}
		}
	}
	else {	// field
		if (pic->structure == TOP_FIELD) {
			add_top = 1;
			add_bottom = 0;
		}
		else {
			add_top = 0;
			add_bottom = 1;
		}
		for (i = 0; i<listD->st_ref_size; i++){
			if (listD->fs_stRef[i]->is_ref & 1){
				if ((!listD->fs_stRef[i]->topField->is_long_term) && (listD->fs_stRef[i]->topField->pic_num == picNumX)){
					//if ((listD->fs_stRef[i]->is_long) && (listD->fs_stRef[i]->long_term_frame_idx != long_term_frame_idx))
					listD->fs_stRef[i]->long_term_frame_idx = listD->fs_stRef[i]->topField->long_term_frame_idx = long_term_frame_idx;
					listD->fs_stRef[i]->topField->long_term_pic_num = 2 * long_term_frame_idx + add_top;
					listD->fs_stRef[i]->topField->is_long_term = 1;
					listD->fs_stRef[i]->is_long |= 1;
					if (listD->fs_stRef[i]->is_long == 3){
						listD->fs_stRef[i]->frame->is_long_term = 1;
						listD->fs_stRef[i]->frame->long_term_frame_idx = listD->fs_stRef[i]->frame->long_term_pic_num = long_term_frame_idx;
					}
//printk("mark top field poc %d long\n", listD->fs_stRef[i]->poc);
					return;
				}
			}
			if (listD->fs_stRef[i]->is_ref & 2){
				if ((!listD->fs_stRef[i]->btmField->is_long_term) && (listD->fs_stRef[i]->btmField->pic_num == picNumX)){
					//if ((listD->fs_stRef[i]->is_long_term) && (listD->fs_stRef[i]->long_term_frame_idx != long_term_frame_idx))
					listD->fs_stRef[i]->long_term_frame_idx = listD->fs_stRef[i]->btmField->long_term_frame_idx = long_term_frame_idx;
					listD->fs_stRef[i]->btmField->long_term_pic_num = 2 * long_term_frame_idx + add_bottom;
					listD->fs_stRef[i]->btmField->is_long_term = 1;
					listD->fs_stRef[i]->is_long |= 2;
					if (listD->fs_stRef[i]->is_long == 3){
						listD->fs_stRef[i]->frame->is_long_term = 1;
						listD->fs_stRef[i]->frame->long_term_frame_idx = listD->fs_stRef[i]->frame->long_term_pic_num = long_term_frame_idx;
					}
//printk("mark bottom field poc %d long\n", listD->fs_stRef[i]->poc);
					return;
				}
			}
		}
	}
}


/*!
 ************************************************************************
 * \brief
 *    Perform Sliding window decoded reference picture marking process
 *    This function unmark shor-term ref frame.
 *
 ************************************************************************
 */
static void sliding_window_memory_management(VideoParams *p_Vid, DecodedPictureBuffer *listD, int num_ref_frames, StorablePicture *pic)
{
	uint32_t i;
	//int cnt = 0;
    
    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "sliding: num_ref_frames: %d [st: %d, lt: %d] (ref %d, long %d)\n", 
        p_Vid->active_sps->num_ref_frames, listD->st_ref_size, listD->lt_ref_size, pic->used_for_ref, pic->is_long_term);

	//if ((listD->st_ref_size + listD->lt_ref_size) == num_ref_frames) /* NOTE: this may fail to remove picture once a lt_ref is inserted */
    //if ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames) /* required at error handling */
	//while ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames && cnt < listD->used_size) /* required at error handling */
    while ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames) /* required at error handling */
    {
        int remove_flg = 0;
	    //cnt++;
		for (i = 0; i < listD->used_size; i++){
            /* unmark short term reference list and remove it from dpb if possible */
			if ((listD->fs[i]->is_ref) && (listD->fs[i]->is_long == 0)){
				unmark_for_reference(listD->fs[i]);
				update_stref_list(listD);
                remove_flg |= 2;
				if (listD->fs[i]->is_output) {
                    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove from dpb at sliding:%d\n",  listD->fs[i]->phy_index);
					remove_picture_from_dpb(p_Vid, listD, i, 1);
                    remove_flg |= 1;
				}
				break;
			}
		}
        if(remove_flg == 0){ /* no more picture can be removed from DPB */
            break;
        }
	}
	pic->is_long_term = 0;
}


/*!
 ************************************************************************
 * \brief
 *    Similar to sliding_window_memory_management(). 
 *    This function unmark BOTH shor-term and long-term ref frame.
 *
 ************************************************************************
 */
static void ltst_sliding_window_memory_management(VideoParams *p_Vid, DecodedPictureBuffer *listD, int num_ref_frames, StorablePicture *ptPic)
{
    //int cnt = 0;

    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "ltst sliding: num_ref_frames: %d [st: %d, lt: %d] (ref %d, long %d)\n", 
        p_Vid->active_sps->num_ref_frames, listD->st_ref_size, listD->lt_ref_size, ptPic->used_for_ref, ptPic->is_long_term);

    //if ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames && ptPic->used_for_ref)
    //while ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames && ptPic->used_for_ref && cnt < listD->used_size) 
    while ((listD->st_ref_size + listD->lt_ref_size) >= num_ref_frames && ptPic->used_for_ref) /* may required at error handling */
    {
        int i;
        int remove_flg = 0;
        DUMP_MSG(D_ERR, p_Vid->dec_handle, "ltst sliding dpb: num_ref_frames: %d [st: %d, lt: %d] (ref %d) ch:%d fr:%d tr:%d\n", 
            num_ref_frames, listD->st_ref_size, listD->lt_ref_size, ptPic->used_for_ref, 
            p_Vid->dec_handle->chn_idx, p_Vid->dec_handle->u32DecodedFrameNumber, p_Vid->dec_handle->u32DecodeTriggerNumber);
        
#if HALT_AT_ERR
        printk("ltst_sliding_window_memory_management: ch %d fr:%d\n", p_Vid->dec_handle->chn_idx, p_Vid->dec_handle->u32DecodedFrameNumber);
        printk("sliding dpb: num_ref_frames: %d [st: %d, lt: %d] (ref %d)\n", num_ref_frames, listD->st_ref_size, listD->lt_ref_size, ptPic->used_for_ref);
        while(1);
#endif

        //cnt++;
        for (i = 0; i < listD->used_size; i++) {
            if (listD->fs[i]->is_ref) {
                if (listD->fs[i]->is_long == 0) {
                    unmark_for_reference(listD->fs[i]);
                    update_stref_list(listD);
                    remove_flg |= 2;
                }
                else {
                    unmark_for_long_term_reference(listD->fs[i]);
                    update_ltref_list(listD);
                    remove_flg |= 2;
                }               
                if (listD->fs[i]->is_output){
                    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove from dpb at ltst sliding:%d\n",  listD->fs[i]->phy_index);
                    remove_picture_from_dpb(p_Vid, listD, i, 1);
                    remove_flg |= 1;
                }
                break;
            }
        }
        if(remove_flg == 0){ /* no more picture can be removed from DPB */
            break;
        }
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove overflow dpb: num_ref_frames: %d [st: %d, lt: %d]\n", num_ref_frames, listD->st_ref_size, listD->lt_ref_size);
    }
}


/*!
 ************************************************************************
 * \brief
 *    remove one frame from DPB
 *    modified from remove_frame_from_dpb() of JM
 * \param is_rec
 *      whether to store the removed picture index to release_buffer array
 *
 * NOTE: This function will move elemmets ahead and listD->used_size--
 *       using loop index to remove fs from DPB should notice this:
 *       DO NOT inc index after calling remove_picture_from_dpb() 
 *       if loop does not break after calling this function
 *
 ************************************************************************
 */
void remove_picture_from_dpb(void *_p_Vid, DecodedPictureBuffer *listD, int pos, int is_rec)
{
	FrameStore *fs = listD->fs[pos];
	FrameStore *tmp;
    VideoParams *p_Vid = _p_Vid;
	int i, is_existing = 1;

	switch (listD->fs[pos]->is_used)
	{
	case 3: /* frame */
		if (fs->frame->non_existing)
			is_existing = 0;
		if (fs->frame->valid) 
			free_storable_picture(p_Vid, fs->frame);
		if (fs->topField->valid)
			free_storable_picture(p_Vid, fs->topField);
		if (fs->btmField->valid)
			free_storable_picture(p_Vid, fs->btmField);
		break;
	case 2: /* bottom */
		if (fs->btmField->valid)
			free_storable_picture(p_Vid, fs->btmField);
		break;
	case 1: /* top */
		if (fs->topField->valid)
			free_storable_picture(p_Vid, fs->topField);
		break;
	case 0: /* empty */
	default:
		break;
	}
	if (is_rec && is_existing) {
		p_Vid->release_buffer[p_Vid->release_buffer_num++] = fs->phy_index;
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "\trelease buffer %d\n", fs->phy_index);
        if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) 
        {
            dump_release_list(p_Vid, D_BUF_REF);
        }
    }
	fs->is_used = 0;
	fs->is_long = 0;
	fs->is_ref = 0;
	fs->is_orig_ref = 0;
	fs->valid = 0;

    /* move empty framestore to end of buffer */
	tmp = listD->fs[pos];

	for (i = pos; i<(int)(listD->used_size-1); i++)
		listD->fs[i] = listD->fs[i+1];
	
	listD->fs[listD->used_size-1] = tmp;
	listD->used_size--;
	p_Vid->used_buffer_num -= is_existing;
}


/*
 * remove one non-referenced (and outputed) picture from DPB
 */
static int remove_non_ref_picture_from_dpb(VideoParams *p_Vid, DecodedPictureBuffer *listD)
{
	uint32_t i;
	for (i = 0; i<listD->used_size; i++){
		if ((!is_used_for_reference(listD->fs[i])) && (listD->fs[i]->is_output)){
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove non-ref from dpb at mmco:%d\n",  listD->fs[i]->phy_index);
			remove_picture_from_dpb(p_Vid, listD, i, 1);
//printk("remove %d[%d]\n", listD->fs[i]->poc, listD->fs[i]->phy_index);
			return 1;
		}
	}
	return 0;
}

/*
 * remove partial long term reference picture from DPB
 */
static int check_partial_long_term(VideoParams *p_Vid, DecodedPictureBuffer *listD)
{
    int i = 0;
    while(i < listD->used_size) {
        if (listD->fs[i]->is_ref && listD->fs[i]->is_long && listD->fs[i]->is_ref != listD->fs[i]->is_long) {
            E_DEBUG("poc %d long term (%d) and reference(%d) is not the same, remove this frame\n", listD->fs[i]->poc, listD->fs[i]->is_long, listD->fs[i]->is_ref);
#if 1
            p_Vid->dec_handle->pfnDamnit("long term and reference is not the same\n");
#else
            /* FIXME: remove un-output picture may cause error if buf_num != 0 */
            remove_picture_from_dpb(p_Vid, listD, i, 1);
#endif
        }else{
            i++;
        }
    }
    //if (listD->last_picture->is_long && !pic->is_long_term)
    //    return H264D_PARSING_ERROR;
    return H264D_OK;
}


/*!
 ************************************************************************
 * \brief
 *    Insert the picture into the DPB. A free DPB position is necessary
 *    for frames, .
 *
 * \param p_Vid
 *      image decoding parameters for current picture
 * \param fs
 *    FrameStore into which the picture will be inserted
 * \param p
 *    StorablePicture to be inserted
 *
 ************************************************************************
 */
static void insert_picture_in_dpb(VideoParams *p_Vid, FrameStore *fs, StorablePicture *pic)
{
    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "insert:        poc %d[st:%d] ref%d long%d (phy %d)\n", pic->poc, pic->structure, pic->used_for_ref, pic->is_long_term, pic->phy_index);
    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "before insert: poc %d[us:%d] ref%d long%d (phy %d)\n", fs->poc, fs->is_used, fs->is_ref, fs->is_long, fs->phy_index);
    
	fs->valid = 1;
	switch (pic->structure){
	case FRAME:
		memcpy(fs->frame, pic, sizeof(StorablePicture));
		fs->frame->valid = 1;
		fs->is_used = 3;
		if (pic->is_damaged)
		    fs->is_damaged = 3;
		if (pic->used_for_ref){
			fs->is_ref = 3;
			fs->is_orig_ref = 3;
			if (pic->is_long_term){
				fs->is_long = 3;
				fs->long_term_frame_idx = pic->long_term_frame_idx;
			}
		}
		dpb_split_field(p_Vid, fs);
		if (pic->non_existing)
			fs->is_return_output = 0;
		else {
			fs->is_return_output = pic->is_output;
			p_Vid->used_buffer_num++;
            p_Vid->unoutput_buffer_num++;
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "used buffer %d(frame)\n", p_Vid->used_buffer_num);
		}
		break;
	case TOP_FIELD:
		memcpy(fs->topField, pic, sizeof(StorablePicture));
		//fs->topField->org_structure = TOP_FIELD;
		//tmp_sp = fs->topField;
		//fs->topField = *ppPic;
		//*ppPic = tmp_sp;
		fs->topField->valid = 1;
		fs->topField->sp_top = fs->topField;
		fs->is_used |= 1;
		if (pic->is_damaged)
		    fs->is_damaged |= 1;
		if (pic->used_for_ref){
			fs->is_ref |= 1;
			fs->is_orig_ref |= 1;
			if (pic->is_long_term){
				fs->is_long |= 1;
				fs->long_term_frame_idx = pic->long_term_frame_idx;
			}
		}
		if (fs->is_used == 3) {
			dpb_combine_field(fs);
			fs->is_return_output = pic->is_output;
			//p_Vid->used_buffer_num += (pic->is_output^1);
		}
		else {
			fs->poc = pic->poc;
			fs->is_return_output = pic->is_output;
            if(pic->non_existing == 0){
    			p_Vid->used_buffer_num++; //= pic->non_existing?0:1;//pic->is_output?0:1;//(pic->is_output^1); /* is_output == 1 only when picture is inserted from fill_frame_num_gap */
                p_Vid->unoutput_buffer_num++;
            }
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "used buffer %d(top)\n", p_Vid->used_buffer_num);
		}
		break;
	case BOTTOM_FIELD:
		memcpy(fs->btmField, pic, sizeof(StorablePicture));
		//fs->btmField->org_structure = BOTTOM_FIELD;
		//tmp_sp = fs->btmField;
		//fs->btmField = *ppPic;
		//*ppPic = tmp_sp;
		fs->btmField->valid = 1;
		fs->btmField->sp_btm = fs->btmField;
		fs->is_used |= 2;
		if (pic->is_damaged)
		    fs->is_damaged |= 2;
		if (pic->used_for_ref){
			fs->is_ref |= 2;
			fs->is_orig_ref |= 2;
			if (pic->is_long_term){
				fs->is_long |= 2;
				fs->long_term_frame_idx = pic->long_term_frame_idx;
			}
		}
		if (fs->is_used == 3) {
			dpb_combine_field(fs);
			fs->is_return_output = pic->is_output;
			//p_Vid->used_buffer_num += (pic->is_output^1);
		}
		else {
			fs->poc = pic->poc;
			fs->is_return_output = pic->is_output;
            if(pic->non_existing == 0){
    			p_Vid->used_buffer_num++; // += pic->non_existing?0:1;//pic->is_output?0:1;//(pic->is_output^1);
                p_Vid->unoutput_buffer_num++;
            }
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "used buffer %d(bottom)\n", p_Vid->used_buffer_num);
		}
		break;
	default:
		// error
		break;
	}

	fs->frame_num = pic->pic_num;
	fs->is_output = pic->is_output;
	fs->phy_index = pic->phy_index;

    if(fs){
        p_Vid->last_stored_poc = fs->poc;
    }

    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "after insert:  poc %d[us:%d] ref%d long%d (phy %d)\n", fs->poc, fs->is_used, fs->is_ref, fs->is_long, fs->phy_index);
}


/*!
 ************************************************************************
 * \brief
 *    Store a picture in DPB. This includes checking for space in DPB and
 *    flushing frames.
 *    If we received a frame, we need to check for a new store, if we
 *    got a field, check if it's the second field of an already allocated
 *    store.
 *
 * \param p_Vid
 *      image decoding parameters for current picture
 * \param ptPic
 *    Picture to be stored
 * \return
 *    H264D_PARSING_ERROR: error occurred and the picture is NOT inserted
 *    FRAME_DONE: a frame is inserted
 *    FIELD_DONE: a field is inserted
 *
 ************************************************************************
 */
int store_picture_in_dpb(void *ptVid, StorablePicture *ptPic, byte adaptive_ref_pic_marking_mode_flag)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
	DecodedPictureBuffer *listD = p_Vid->listD;
	p_Vid->last_has_mmco_5 = 0; /* clear here in case error return before calling adaptive_memory_management() */
	p_Vid->last_pic_bottom_field = (ptPic->structure == BOTTOM_FIELD);
    if (!ptPic->non_existing) {
        if (ptPic->structure == FRAME)
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle,"store picture: frame poc %d(long %d, pic num %d), idr %d\n", ptPic->poc, ptPic->is_long_term, ptPic->pic_num, p_Vid->idr_flag);
        else
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle,"store picture: field poc %d(long %d, pic num %d), idr %d\n", ptPic->poc, ptPic->is_long_term, ptPic->pic_num, p_Vid->idr_flag);
    }
    
    if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) {
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "before store:\n");
        dump_dpb_info(p_Vid, 0);
    }

	if (p_Vid->idr_flag) {
	    if (idr_memory_management(p_Vid, ptPic) < 0){
	        return H264D_PARSING_ERROR;
        }
	} else {
	    /* non IDR case */
		if (ptPic->used_for_ref && adaptive_ref_pic_marking_mode_flag) {
            if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) {
                int i;
                if (listD->lt_ref_size > 0) {
                    for (i = 0; i<listD->lt_ref_size; i++)
                        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "poc%d long%d\t", listD->fs_ltRef[i]->poc, listD->fs_ltRef[i]->long_term_frame_idx);
                    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "\n");
                }
            }
			if (adaptive_memory_management(p_Vid, ptPic) < 0) {
                if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) {
                    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "adaptive memory management error\n");
                    dump_dpb_info(p_Vid, 0);
                }
			    return H264D_PARSING_ERROR;
			}
		}
    }

	if ((ptPic->structure==TOP_FIELD)||(ptPic->structure==BOTTOM_FIELD)){
		// check for frame store with same pic_number
		if (listD->last_picture){
		    if (listD->last_picture->is_long && !ptPic->is_long_term) {
		        E_DEBUG("last_picture is long but current is not\n");
		        return H264D_PARSING_ERROR;
		    }
			if (listD->last_picture->frame_num == ptPic->pic_num){
				if (((ptPic->structure == TOP_FIELD) && (listD->last_picture->is_used == 2)) || ((ptPic->structure == BOTTOM_FIELD) && (listD->last_picture->is_used == 1))){
					if (((ptPic->used_for_ref) && (listD->last_picture->is_orig_ref!=0)) || ((!ptPic->used_for_ref) && (listD->last_picture->is_orig_ref==0))){
						insert_picture_in_dpb(p_Vid, listD->last_picture, ptPic);
						update_stref_list(listD);
						update_ltref_list(listD);
						listD->last_picture = NULL;
						ptPic->valid = 0;
						return FRAME_DONE;
					}
				}
			}
		}
	}

	if ((!p_Vid->idr_flag) && (ptPic->used_for_ref) && (!adaptive_ref_pic_marking_mode_flag)) {
		sliding_window_memory_management(p_Vid, listD, p_Vid->active_sps->num_ref_frames, ptPic);
    } 
#if 1 
    else {
        /* NOTE: this is not from JM */
        /* idr_flag || (!used_for_ref) || adaptive_ref_pic_marking_mode_flag */
        /* FIXME: to handle DPB overflow? */
        ltst_sliding_window_memory_management(p_Vid, listD, p_Vid->active_sps->num_ref_frames, ptPic);
	}
#else
    D_DEBUG("dpb: num_ref_frames: %d [st: %d, lt: %d] (ref %d)\n", p_Vid->active_sps->num_ref_frames, listD->st_ref_size, listD->lt_ref_size, ptPic->used_for_ref);
    D_DEBUG("dpb size %d (%d)\n", listD->used_size, listD->max_size);
	if (listD->used_size == listD->max_size)
	{
		D_DEBUG("remove one picture buffer\n");
		remove_non_ref_picture_from_dpb(p_Vid, listD);
	}
#endif
	insert_picture_in_dpb(p_Vid, listD->fs[listD->used_size], ptPic);

	if (ptPic->structure != FRAME){
        /* record the last stored field picture */
		listD->last_picture = listD->fs[listD->used_size];
	}else
		listD->last_picture = NULL;
	listD->used_size++;

	update_stref_list(listD);
	update_ltref_list(listD);
		
    ptPic->valid = 0;


    if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) 
    {
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "after store:\n");
        dump_dpb_info(p_Vid, 0);

        if (p_Vid->release_buffer_num > 0) {
            dump_release_list(p_Vid, D_BUF_REF);
        }
        if (listD->lt_ref_size > 0) {
            dump_lt_ref_list(p_Vid, D_BUF_REF);
        }
    }

	if (ptPic->structure == FRAME)
		return FRAME_DONE;
	return FIELD_DONE;
}					


static int compare_pic_by_pic_num_desc(const void *arg1, const void *arg2)
{
	if ( (*(StorablePicture**)arg1)->pic_num < (*(StorablePicture**)arg2)->pic_num)
		return 1;
	if ( (*(StorablePicture**)arg1)->pic_num > (*(StorablePicture**)arg2)->pic_num)
		return -1;
	else
		return 0;
}

static int compare_pic_by_lt_pic_num_asc(const void *arg1, const void *arg2)
{
	if ( (*(StorablePicture**)arg1)->long_term_pic_num < (*(StorablePicture**)arg2)->long_term_pic_num)
		return -1;
	if ( (*(StorablePicture**)arg1)->long_term_pic_num > (*(StorablePicture**)arg2)->long_term_pic_num)
		return 1;
	else
		return 0;
}

static int compare_pic_by_frame_num_desc(const void *arg1, const void *arg2)
{
	if ( (*(FrameStore**)arg1)->frame_num_wrap < (*(FrameStore**)arg2)->frame_num_wrap)
		return 1;
	if ( (*(FrameStore**)arg1)->frame_num_wrap > (*(FrameStore**)arg2)->frame_num_wrap)
		return -1;
	else 
		return 0;
}

static int compare_fs_by_lt_frame_num_asc(const void *arg1, const void *arg2)
{
	if ( (*(FrameStore**)arg1)->long_term_frame_idx < (*(FrameStore**)arg2)->long_term_frame_idx)
		return -1;
	if ( (*(FrameStore**)arg1)->long_term_frame_idx > (*(FrameStore**)arg2)->long_term_frame_idx)
		return 1;
	else
		return 0;
}

int compare_fs_by_poc_desc(const void *arg1, const void *arg2)
{
	if ( (*(FrameStore**)arg1)->poc < (*(FrameStore**)arg2)->poc)
		return 1;
	if ( (*(FrameStore**)arg1)->poc > (*(FrameStore**)arg2)->poc)
		return -1;
	else
		return 0;
}

static int compare_pic_by_poc_desc(const void *arg1, const void *arg2)
{
	if ( (*(StorablePicture**)arg1)->poc < (*(StorablePicture**)arg2)->poc)
		return 1;
	if ( (*(StorablePicture**)arg1)->poc > (*(StorablePicture**)arg2)->poc)
		return -1;
	else
		return 0;
}

static int compare_fs_by_poc_asc(const void *arg1, const void *arg2)
{
	if ( (*(FrameStore**)arg1)->poc < (*(FrameStore**)arg2)->poc)
		return -1;
	if ( (*(FrameStore**)arg1)->poc > (*(FrameStore**)arg2)->poc)
		return 1;
	else
		return 0;
}

static int compare_pic_by_poc_asc(const void *arg1, const void *arg2)
{
	if ( (*(StorablePicture**)arg1)->poc < (*(StorablePicture**)arg2)->poc)
		return -1;
	if ( (*(StorablePicture**)arg1)->poc > (*(StorablePicture**)arg2)->poc)
		return 1;
	else
		return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Generates an alternating field list from a given FrameStore list
 *
 ************************************************************************
 */
static void gen_pic_list_from_frame_list(PictureStructure currStructure, FrameStore **inList, uint32_t list_idx, StorablePicture **outList, uint8_t *list_size, int long_term)
{
	uint32_t top_idx = 0;
	uint32_t bot_idx = 0;

	int (*is_ref)(StorablePicture *s);

	if (long_term)
		is_ref = is_long_ref;
	else
		is_ref = is_short_ref;

	if (currStructure == TOP_FIELD){
		while ((top_idx<list_idx) || (bot_idx<list_idx)){
			for ( ; top_idx<list_idx; top_idx++){
				if (inList[top_idx]->is_used & 1){
					if (is_ref(inList[top_idx]->topField)){
						outList[*list_size] = inList[top_idx]->topField;
						(*list_size)++;
						top_idx++;
						break;
					}
				}
			}
			for ( ; bot_idx<list_idx; bot_idx++){
				if (inList[bot_idx]->is_used & 2){
					if (is_ref(inList[bot_idx]->btmField)){
						outList[*list_size] = inList[bot_idx]->btmField;
						(*list_size)++;
						bot_idx++;
						break;
					}
				}
			}
		}	// end of while ((top_idx<list_idx) || (bot_idx<list_idx))
	}
	if (currStructure == BOTTOM_FIELD)
	{
		while ((top_idx<list_idx) || (bot_idx<list_idx)){
			for ( ; bot_idx<list_idx; bot_idx++){
				if (inList[bot_idx]->is_used & 2){
					if (is_ref(inList[bot_idx]->btmField)){
						outList[*list_size] = inList[bot_idx]->btmField;
						(*list_size)++;
						bot_idx++;
						break;
					}
				}
			}
			for ( ; top_idx<list_idx; top_idx++){
				if (inList[top_idx]->is_used & 1){
					if (is_ref(inList[top_idx]->topField)){
						outList[*list_size] = inList[top_idx]->topField;
						(*list_size)++;
						top_idx++;
						break;
					}
				}
			}
		}
	}
}


/*!
 ************************************************************************
 * \brief
 *    Initialize p_Vid->refListLX[0] and list 1 depending on current slice type
 *
 ************************************************************************
 */
void init_lists(void *ptVid)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
	SliceHeader *sh = p_Vid->slice_hdr;
	DecodedPictureBuffer *listD = p_Vid->listD;
	FrameStore *list_l0[MAX_LIST_SIZE], *list_l1[MAX_LIST_SIZE], *list_lt[MAX_LIST_SIZE];
	StorablePicture *tmp;

	int i;
 	uint32_t list0idx, listltidx, list0idx_tmp;
	int add_top, add_bot, diff;
	unsigned int MaxFrameNum = 1 << (p_Vid->active_sps->log2_max_frame_num);

	for (i = 0; i < 6; i++)
		p_Vid->list_size[i] = 0;

	// set picture number
	if (p_Vid->structure == FRAME){
		for (i = 0; i<listD->st_ref_size; i++){
			if (listD->fs_stRef[i]->is_used == 3){
			//if (listD->fs_stRef[i]->is_ref == 3){
				if ((listD->fs_stRef[i]->frame->used_for_ref) && (!listD->fs_stRef[i]->frame->is_long_term)){
					listD->fs_stRef[i]->frame_num_wrap = listD->fs_stRef[i]->frame_num;
					if (listD->fs_stRef[i]->frame_num > p_Vid->frame_num)
						listD->fs_stRef[i]->frame_num_wrap -= MaxFrameNum;
                    listD->fs_stRef[i]->frame->pic_num = listD->fs_stRef[i]->frame_num_wrap;
				}
			}
		}	// long term
		for (i = 0; i<listD->lt_ref_size; i++)
			if (listD->fs_ltRef[i]->is_used == 3)
				listD->fs_ltRef[i]->frame->long_term_pic_num = listD->fs_ltRef[i]->frame->long_term_frame_idx;
	}
	else{	// field
		if (p_Vid->structure == TOP_FIELD){
			add_top = 1;
			add_bot = 0;
		}
		else {
			add_top = 0;
			add_bot = 1;
		}
		for (i = 0; i<listD->st_ref_size; i++){
			if (listD->fs_stRef[i]->is_ref){
				listD->fs_stRef[i]->frame_num_wrap = listD->fs_stRef[i]->frame_num;
				if (listD->fs_stRef[i]->frame_num > p_Vid->frame_num)
					listD->fs_stRef[i]->frame_num_wrap -= MaxFrameNum;

				if (listD->fs_stRef[i]->is_ref & 1)
					listD->fs_stRef[i]->topField->pic_num = listD->fs_stRef[i]->frame_num_wrap*2 + add_top;
				if (listD->fs_stRef[i]->is_ref & 2)
					listD->fs_stRef[i]->btmField->pic_num = listD->fs_stRef[i]->frame_num_wrap*2 + add_bot;
			}
		}
		for (i = 0; i<listD->lt_ref_size; i++){
			if (listD->fs_ltRef[i]->is_ref){
				if (listD->fs_ltRef[i]->is_long & 1)
					listD->fs_ltRef[i]->topField->long_term_pic_num = listD->fs_ltRef[i]->topField->long_term_frame_idx*2 + add_top;
				if (listD->fs_ltRef[i]->is_long & 2)
					listD->fs_ltRef[i]->btmField->long_term_pic_num = listD->fs_ltRef[i]->btmField->long_term_frame_idx*2 + add_bot;
			}
		}
	}
	if (sh->slice_type == I_SLICE){
		p_Vid->list_size[0] = p_Vid->list_size[1] = 0;
		return;
	}

	if (sh->slice_type == P_SLICE)
	{
		if (p_Vid->structure == FRAME){
			list0idx = 0;
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_used == 3){
				//if (listD->fs_stRef[i]->is_ref == 3){
					if ((listD->fs_stRef[i]->frame->used_for_ref) && (!listD->fs_stRef[i]->frame->is_long_term))
                        p_Vid->refListLX[0][list0idx++] = listD->fs_stRef[i]->frame;
				}
			}
			q_sort((void*)p_Vid->refListLX[0], list0idx, (int)sizeof(StorablePicture*), compare_pic_by_pic_num_desc);
			p_Vid->list_size[0] = list0idx;
			// long term
			for (i = 0; i<listD->lt_ref_size; i++){
				if (listD->fs_ltRef[i]->is_used == 3){
				//if (listD->fs_ltRef[i]->is_long == 3){
					if (listD->fs_ltRef[i]->frame->is_long_term)
						p_Vid->refListLX[0][list0idx++] = listD->fs_ltRef[i]->frame;
				}
			}
			q_sort((void*)&(p_Vid->refListLX[0][p_Vid->list_size[0]]), list0idx-p_Vid->list_size[0], sizeof(StorablePicture*), compare_pic_by_lt_pic_num_asc);
			p_Vid->list_size[0] = list0idx;
		}
		else {	// field
			list0idx = 0;
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_ref)
					list_l0[list0idx++] = listD->fs_stRef[i];
			}
			q_sort((void*)list_l0, list0idx, sizeof(FrameStore*), compare_pic_by_frame_num_desc);
			p_Vid->list_size[0] = 0;
			gen_pic_list_from_frame_list(p_Vid->structure, list_l0, list0idx, p_Vid->refListLX[0], &p_Vid->list_size[0], 0);

			listltidx = 0;
			for (i = 0; i<listD->lt_ref_size; i++)
				list_lt[listltidx++] = listD->fs_ltRef[i];
			q_sort((void*)list_lt, listltidx, sizeof(FrameStore*), compare_fs_by_lt_frame_num_asc);
			gen_pic_list_from_frame_list(p_Vid->structure, list_lt, listltidx, p_Vid->refListLX[0], &p_Vid->list_size[0], 1);
		}
	}
	else { /* B slice */
		if (p_Vid->structure == FRAME){
			list0idx = 0;
			// short term
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_used == 3){
				//if (listD->fs_stRef[i]->is_ref == 3){
					if ((listD->fs_stRef[i]->frame->used_for_ref) && (!listD->fs_stRef[i]->frame->is_long_term)){
						if (listD->fs_stRef[i]->frame->poc <= p_Vid->ThisPOC)
							p_Vid->refListLX[0][list0idx++] = listD->fs_stRef[i]->frame;
					}

				}
			}
			// poc <= currPOC (desc)
			q_sort((void*)p_Vid->refListLX[0], list0idx, sizeof(StorablePicture*), compare_pic_by_poc_desc);
			list0idx_tmp = list0idx;
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_used == 3){
				//if (listD->fs_stRef[i]->is_ref == 3){
					if ((listD->fs_stRef[i]->frame->used_for_ref) && (!listD->fs_stRef[i]->frame->is_long_term)){
						if (listD->fs_stRef[i]->frame->poc > p_Vid->ThisPOC)
							p_Vid->refListLX[0][list0idx++] = listD->fs_stRef[i]->frame;
					}
				}
			}
			// poc > currPOC (asc)
			q_sort((void*)&(p_Vid->refListLX[0][list0idx_tmp]), list0idx-list0idx_tmp, sizeof(StorablePicture*), compare_pic_by_poc_asc);
			// set list l1 from list l0
			for (i = 0; i<list0idx_tmp; i++)
				p_Vid->refListLX[1][list0idx - list0idx_tmp + i] = p_Vid->refListLX[0][i];
			for (i = list0idx_tmp; i<list0idx; i++)
				p_Vid->refListLX[1][i - list0idx_tmp] = p_Vid->refListLX[0][i];

			p_Vid->list_size[0] = p_Vid->list_size[1] = list0idx;
			// long term
			for (i = 0; i<listD->lt_ref_size; i++){
				if (listD->fs_ltRef[i]->is_used == 3){
				//if (listD->fs_ltRef[i]->is_long == 3){
					if (listD->fs_ltRef[i]->frame->is_long_term)
						p_Vid->refListLX[0][list0idx++] = listD->fs_ltRef[i]->frame;
				}
			}
			q_sort((void*)&(p_Vid->refListLX[0][p_Vid->list_size[0]]), list0idx-p_Vid->list_size[0], sizeof(StorablePicture*), compare_pic_by_lt_pic_num_asc);
			for (i = p_Vid->list_size[0]; i<list0idx; i++)
				p_Vid->refListLX[1][i] = p_Vid->refListLX[0][i];
			p_Vid->list_size[0] = p_Vid->list_size[1] = list0idx;
		}
		else	// field
		{
			list0idx = 0;
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_used){
					if (listD->fs_stRef[i]->poc <= p_Vid->ThisPOC)
						list_l0[list0idx++] = listD->fs_stRef[i];
				}
			}
			// poc <= currPOC (desc)
			q_sort((void*)list_l0, list0idx, sizeof(FrameStore*), compare_fs_by_poc_desc);
			list0idx_tmp = list0idx;
			for (i = 0; i<listD->st_ref_size; i++){
				if (listD->fs_stRef[i]->is_used){
					if (listD->fs_stRef[i]->poc > p_Vid->ThisPOC)
						list_l0[list0idx++] = listD->fs_stRef[i];
				}
			}
			// poc > currPOC (asc)
			q_sort((void*)&(list_l0[list0idx_tmp]), list0idx-list0idx_tmp, sizeof(FrameStore*), compare_fs_by_poc_asc);
			// set list l1 from list l0
			for (i = 0; i<list0idx_tmp; i++)
				list_l1[list0idx - list0idx_tmp + i] = list_l0[i];
			for (i = list0idx_tmp; i<list0idx; i++)
				list_l1[i - list0idx_tmp] = list_l0[i];
			p_Vid->list_size[0] = p_Vid->list_size[1] = 0;
			gen_pic_list_from_frame_list(p_Vid->structure, list_l0, list0idx, p_Vid->refListLX[0], &p_Vid->list_size[0], 0);
			gen_pic_list_from_frame_list(p_Vid->structure, list_l1, list0idx, p_Vid->refListLX[1], &p_Vid->list_size[1], 0);

			// long term
			listltidx = 0;
			for (i = 0; i<listD->lt_ref_size; i++)
				list_lt[listltidx++] = listD->fs_ltRef[i];

			q_sort((void*)list_lt, listltidx, sizeof(FrameStore*), compare_fs_by_lt_frame_num_asc);
			gen_pic_list_from_frame_list(p_Vid->structure, list_lt, listltidx, p_Vid->refListLX[0], &p_Vid->list_size[0], 1);
			gen_pic_list_from_frame_list(p_Vid->structure, list_lt, listltidx, p_Vid->refListLX[1], &p_Vid->list_size[1], 1);
		}
	}
	if ((p_Vid->list_size[0] == p_Vid->list_size[1]) && (p_Vid->list_size[1]>1)){
		// check if lists are identical, if yes swap first two elements of p_Vid->refListLX
		diff = 0;
		for (i = 0; i<p_Vid->list_size[0]; i++){
			if (p_Vid->refListLX[0][i] != p_Vid->refListLX[1][i]){
				diff = 1;
				break;
			}
		}
		if (!diff){
			tmp = p_Vid->refListLX[1][0];
			p_Vid->refListLX[1][0] = p_Vid->refListLX[1][1];
			p_Vid->refListLX[1][1] = tmp;
		}
	}
	p_Vid->list_size[0] = iMin(p_Vid->list_size[0], (sh->num_ref_idx_l0_active_minus1+1));
	p_Vid->list_size[1] = iMin(p_Vid->list_size[1], (sh->num_ref_idx_l1_active_minus1+1));
/*
	// set the unused list entries to NULL
	for (i = p_Vid->list_size[0]; i < MAX_LIST_SIZE; i++)
		p_Vid->refListLX[0][i] = NULL;
	for (i = p_Vid->list_size[1]; i < MAX_LIST_SIZE; i++)
		p_Vid->refListLX[1][i] = NULL;
*/

}


/*!
 ************************************************************************
 * \brief
 *    Initialize listX[2..5] from lists 0 and 1
 *    listX[2]: list0 for current_field==top
 *    listX[3]: list1 for current_field==top
 *    listX[4]: list0 for current_field==bottom
 *    listX[5]: list1 for current_field==bottom
 *
 ************************************************************************
 */
void init_mbaff_list(void *ptVid)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
	int i, j;

	for (i = 2; i<6; i++)
	{
		for (j = 0; j<MAX_LIST_SIZE; j++)
			p_Vid->refListLX[i][j] = NULL;
		p_Vid->list_size[i] = 0;
	}

	for (i = 0; i<p_Vid->list_size[0]; i++)
	{
		p_Vid->refListLX[2][2*i]   = p_Vid->refListLX[0][i]->sp_top;
		p_Vid->refListLX[2][2*i+1] = p_Vid->refListLX[0][i]->sp_btm;
		p_Vid->refListLX[4][2*i]   = p_Vid->refListLX[0][i]->sp_btm;
		p_Vid->refListLX[4][2*i+1] = p_Vid->refListLX[0][i]->sp_top;
	}
	p_Vid->list_size[2] = p_Vid->list_size[4] = p_Vid->list_size[0]*2;

	for (i = 0; i<p_Vid->list_size[1]; i++)
	{
		p_Vid->refListLX[3][2*i]   = p_Vid->refListLX[1][i]->sp_top;
		p_Vid->refListLX[3][2*i+1] = p_Vid->refListLX[1][i]->sp_btm;
		p_Vid->refListLX[5][2*i]   = p_Vid->refListLX[1][i]->sp_btm;
		p_Vid->refListLX[5][2*i+1] = p_Vid->refListLX[1][i]->sp_top;
	}
	p_Vid->list_size[3] = p_Vid->list_size[5] = p_Vid->list_size[1]*2;
}


/*!
************************************************************************
* \brief
*    Returns short term pic with given picNum
*
************************************************************************
*/
static StorablePicture *get_short_term_pic(VideoParams *p_Vid, int picNum)
{
	DecodedPictureBuffer *listD = p_Vid->listD;
	int i;

	for (i = 0; i<listD->st_ref_size; i++){
		if (p_Vid->structure == FRAME){
			if (listD->fs_stRef[i]->is_ref == 3)
				if ((!listD->fs_stRef[i]->frame->is_long_term) && (listD->fs_stRef[i]->frame->pic_num == picNum))
					return listD->fs_stRef[i]->frame;
		}
		else{
			if (listD->fs_stRef[i]->is_ref & 1)
				if ((!listD->fs_stRef[i]->topField->is_long_term) && (listD->fs_stRef[i]->topField->pic_num == picNum))
					return listD->fs_stRef[i]->topField;
			if (listD->fs_stRef[i]->is_ref & 2)
				if ((!listD->fs_stRef[i]->btmField->is_long_term) && (listD->fs_stRef[i]->btmField->pic_num == picNum))
					return listD->fs_stRef[i]->btmField;
		}
	}
	return NULL;
}


/*!
 ************************************************************************
 * \brief
 *    Returns long term pic with given LongtermPicNum
 *
 ************************************************************************
 */
static StorablePicture* get_long_term_pic(VideoParams *p_Vid, int LongtermPicNum)
{
	DecodedPictureBuffer *listD = p_Vid->listD;
	int i;

	for (i = 0; i<listD->lt_ref_size; i++){
		if (p_Vid->structure == FRAME){
			if (listD->fs_ltRef[i]->is_ref == 3)
				if ((listD->fs_ltRef[i]->frame->is_long_term) && (listD->fs_ltRef[i]->frame->long_term_pic_num == LongtermPicNum))
					return listD->fs_ltRef[i]->frame;
		}
		else {
			if (listD->fs_ltRef[i]->is_ref & 1)
				if ((listD->fs_ltRef[i]->topField->is_long_term) && (listD->fs_ltRef[i]->topField->long_term_pic_num == LongtermPicNum))
					return listD->fs_ltRef[i]->topField;
			if (listD->fs_ltRef[i]->is_ref & 2)
				if ((listD->fs_ltRef[i]->btmField->is_long_term) && (listD->fs_ltRef[i]->btmField->long_term_pic_num == LongtermPicNum))
					return listD->fs_ltRef[i]->btmField;
		}
	}
	return NULL;
}


/*!
 ************************************************************************
 * \brief
 *    Reordering process for short-term reference pictures
 *
 ************************************************************************
 */
static void reorder_short_term(VideoParams *p_Vid, StorablePicture **RefPicListX, int num_ref_idx_lX_active_minus1, int picNumLX, int *refIdxLX)
{
	int cIdx, nIdx;
    StorablePicture *picLX;

    if ((picLX = get_short_term_pic(p_Vid, picNumLX))==NULL)
		return;

	for (cIdx = num_ref_idx_lX_active_minus1 + 1; cIdx>*refIdxLX; cIdx--)
		RefPicListX[cIdx] = RefPicListX[cIdx-1];
	RefPicListX[(*refIdxLX)++] = picLX;
	nIdx = *refIdxLX;
	for (cIdx = *refIdxLX; cIdx<=num_ref_idx_lX_active_minus1 + 1; cIdx++)
		if (RefPicListX[cIdx])
			if (RefPicListX[cIdx]->is_long_term || (RefPicListX[cIdx]->pic_num != picNumLX))
				RefPicListX[nIdx++] = RefPicListX[cIdx];
}


/*!
 ************************************************************************
 * \brief
 *    Reordering process for long-term reference pictures
 *
 ************************************************************************
 */
static void reorder_long_term(VideoParams *p_Vid, StorablePicture **RefPicListX, int num_ref_idx_lX_active_minus1, int LongTermPicNum, int *refIdxLX)
{
	int cIdx,	nIdx;
	StorablePicture *picLX;

	if ((picLX = get_long_term_pic(p_Vid, LongTermPicNum))==NULL)
		return;

	for (cIdx = num_ref_idx_lX_active_minus1 + 1; cIdx>*refIdxLX; cIdx--)
		RefPicListX[cIdx] = RefPicListX[cIdx-1];

	RefPicListX[(*refIdxLX)++] = picLX;
	nIdx = *refIdxLX;
	for(cIdx = *refIdxLX; cIdx<=num_ref_idx_lX_active_minus1 + 1; cIdx++)
		if (RefPicListX[cIdx])
			if ((!RefPicListX[cIdx]->is_long_term) || (RefPicListX[cIdx]->long_term_pic_num != LongTermPicNum))
				RefPicListX[nIdx++] = RefPicListX[cIdx];
}


/*!
 ************************************************************************
 * \brief
 *    Reordering process for reference picture lists
 *
 ************************************************************************
 */
static void reorder_ref_pic_list(VideoParams *p_Vid, StorablePicture **list, uint8_t *list_size, int num_ref_idx_lX_active_minus1, ReorderRefPicList *reorderingList)
{
	int i;
	int MaxPicNum, currPicNum, picNumLXNoWrap, picNumLXPred, picNumLX;
	int refIdxLX = 0;

	MaxPicNum = 1<<(p_Vid->active_sps->log2_max_frame_num);

	if (p_Vid->structure == FRAME)
		currPicNum = p_Vid->frame_num;
	else{
		MaxPicNum *= 2;
		currPicNum = 2*p_Vid->frame_num + 1;
	}

	// initial
	picNumLXPred = currPicNum;
	
	for (i = 0; reorderingList[i].reordering_of_pic_nums_idc!=3; i++)
	{
		//if (reorderingList[i].reordering_of_pic_nums_idc>3)
		//	error
		if (reorderingList[i].reordering_of_pic_nums_idc<2){
			if (reorderingList[i].reordering_of_pic_nums_idc == 0){
				if ((picNumLXPred - (reorderingList[i].abs_diff_pic_num_minus1 + 1)) < 0)
					picNumLXNoWrap = picNumLXPred - (reorderingList[i].abs_diff_pic_num_minus1 + 1) + MaxPicNum;
				else
					picNumLXNoWrap = picNumLXPred - (reorderingList[i].abs_diff_pic_num_minus1 + 1);
			}
			else {	// reordering_of_pic_nums_idc = 1
				if ((picNumLXPred + (reorderingList[i].abs_diff_pic_num_minus1 + 1)) >= MaxPicNum)
					picNumLXNoWrap = picNumLXPred + (reorderingList[i].abs_diff_pic_num_minus1 + 1) - MaxPicNum;
				else
					picNumLXNoWrap = picNumLXPred + (reorderingList[i].abs_diff_pic_num_minus1 + 1);
			}
			picNumLXPred = picNumLXNoWrap;

			if (picNumLXNoWrap > currPicNum)
				picNumLX = picNumLXNoWrap - MaxPicNum;
			else
				picNumLX = picNumLXNoWrap;

			reorder_short_term(p_Vid, list, num_ref_idx_lX_active_minus1, picNumLX, &refIdxLX);
		}
		else if (reorderingList[i].reordering_of_pic_nums_idc == 2)	// reordering_of_pic_nums_idc = 2
			reorder_long_term(p_Vid, list, num_ref_idx_lX_active_minus1, reorderingList[i].long_term_pic_num, &refIdxLX);
	}
	*list_size = num_ref_idx_lX_active_minus1 + 1;
}


/*
 * from reorder_lists of JM
 */
void reorder_list(void *ptVid, void *ptSliceHdr)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
	SliceHeader *sh = (SliceHeader*)ptSliceHdr;

	if ((sh->slice_type != I_SLICE) && (sh->slice_type != SI_SLICE)){
		if (sh->ref_pic_list_reordering_flag_l0)
			reorder_ref_pic_list(p_Vid, p_Vid->refListLX[0], &p_Vid->list_size[0], sh->num_ref_idx_l0_active_minus1, sh->reordering_of_ref_pic_list[0]);
#if 0
		p_Vid->list_size[0] = sh->num_ref_idx_l0_active_minus1 + 1;
#else
		p_Vid->list_size[0] = iMin(p_Vid->list_size[0], sh->num_ref_idx_l0_active_minus1 + 1);
//printk("act l0: %d, list0: %d\n", sh->num_ref_idx_l0_active_minus1 + 1, p_Vid->list_size[0]);
#endif
	}

	if (sh->slice_type == B_SLICE){
		if (sh->ref_pic_list_reordering_flag_l1)
			reorder_ref_pic_list(p_Vid, p_Vid->refListLX[1], &p_Vid->list_size[1], sh->num_ref_idx_l1_active_minus1, sh->reordering_of_ref_pic_list[1]);
#if 0
		p_Vid->list_size[1] = sh->num_ref_idx_l1_active_minus1 + 1;
#else
		p_Vid->list_size[1] = iMin(p_Vid->list_size[1], sh->num_ref_idx_l1_active_minus1 + 1);
//printk("act l1: %d, list1: %d\n", sh->num_ref_idx_l1_active_minus1 + 1, p_Vid->list_size[1]);
#endif
	}
}

#if 0
int replace_damgae_list(void *ptVid)
{
    VideoParams *p_Vid = (VideoParams *)ptVid;
   	int i, j;
	StorablePicture *sp = NULL;

	//if reference list is damaged picture copy nearest reference
    if (p_Vid->slice_hdr->slice_type != I_SLICE){
        if (p_Vid->list_size[0] == 0)
            return H264D_PARSING_ERROR;
        for (i = 0; i<p_Vid->list_size[0]; i++) {
    	    if (p_Vid->refListLX[0][i]->is_damaged) {
    	        if (sp == NULL) {
    	            for (j = 0; j<p_Vid->list_size[0]; j++) {
    	                if (!p_Vid->refListLX[0][j]->is_damaged) {
    	                    sp = p_Vid->refListLX[0][j];
    	                    break;
    	                }
    	            }
    	            if (sp == NULL)
    	                return H264D_PARSING_ERROR;
                }
                p_Vid->refListLX[0][i] = sp;
    	    }
            else 
                sp = p_Vid->refListLX[0][i];
    	}
    }
	if (p_Vid->slice_hdr->slice_type == B_SLICE){
	    if (p_Vid->list_size[1] == 0)
	        return H264D_PARSING_ERROR;
		for (i = 0; i<p_Vid->list_size[1]; i++) {
		    if (p_Vid->refListLX[1][i]->is_damaged) {
		        if (sp == NULL) {
		            for (j = 0; j<p_Vid->list_size[1]; j++) {
		                if (!p_Vid->refListLX[1][j]->is_damaged) {
		                    sp = p_Vid->refListLX[1][j];
		                    break;
		                }
		            }
		            if (sp == NULL)
		                return H264D_PARSING_ERROR;
                }
                p_Vid->refListLX[1][i] = sp;
		    }
		    else 
		        sp = p_Vid->refListLX[1][i];
		}
	}
	
	return H264D_OK;
}
#endif


/*
 * TBD
 */
int fill_empty_list(void *ptVid)
{
    VideoParams *p_Vid = (VideoParams *)ptVid;
   	int i, j;
	StorablePicture *sp = NULL;

	//if reference list is NULL copy nearest reference
    if (p_Vid->slice_hdr->slice_type != I_SLICE){
        if (p_Vid->list_size[0] == 0){
            LOG_PRINT(LOG_ERROR, "reference list_size[0] == 0\n");
            return H264D_PARSING_ERROR;
        }
        for (i = 0; i<p_Vid->list_size[0]; i++) {
    	    if (p_Vid->refListLX[0][i] == NULL || p_Vid->refListLX[0][i]->valid == 0) {
    	        if (sp == NULL) {
    	            for (j = 0; j<p_Vid->list_size[0]; j++) {
    	                if (p_Vid->refListLX[0][j] != NULL && p_Vid->refListLX[0][i]->valid) {
    	                    sp = p_Vid->refListLX[0][j];
    	                    break;
    	                }
    	            }
    	            if (sp == NULL){
                        LOG_PRINT(LOG_ERROR, "failed to find the first reference in ref list\n");
    	                return H264D_PARSING_ERROR;
                    }
                }
                p_Vid->refListLX[0][i] = sp;
    	    }
    	    else 
    	        sp = p_Vid->refListLX[0][i];
    	}
    }
	if (p_Vid->slice_hdr->slice_type == B_SLICE){
	    if (p_Vid->list_size[1] == 0){
            LOG_PRINT(LOG_ERROR, "reference list_size[1] == 0\n");
	        return H264D_PARSING_ERROR;
        }
		for (i = 0; i<p_Vid->list_size[1]; i++) {
		    if (p_Vid->refListLX[1][i] == NULL || p_Vid->refListLX[1][i]->valid == 0) {
		        if (sp == NULL) {
		            for (j = 0; j<p_Vid->list_size[1]; j++) {
		                if (p_Vid->refListLX[1][j] != NULL && p_Vid->refListLX[1][i]->valid) {
		                    sp = p_Vid->refListLX[1][j];
		                    break;
		                }
		            }
		            if (sp == NULL){
                        LOG_PRINT(LOG_ERROR, "failed to find the second reference in ref list\n");
		                return H264D_PARSING_ERROR;
                    }
                }
                p_Vid->refListLX[1][i] = sp;
		    }
		    else 
		        sp = p_Vid->refListLX[1][i];
		}
	}
	
	return H264D_OK;
}


/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark short term picture unused
 ************************************************************************
 */
static int mm_unmark_short_term_for_reference(DecodedPictureBuffer *listD, StorablePicture *pic, int diff)
{
	uint32_t picNumX;
	int i;

	picNumX = (pic->structure==FRAME ? pic->frame_num : (2*pic->frame_num + 1)) - diff;

	for (i = 0; i<listD->st_ref_size; i++){
		if (pic->structure == FRAME){
			if ((listD->fs_stRef[i]->is_ref == 3) && (listD->fs_stRef[i]->is_long==0)){
				if (listD->fs_stRef[i]->frame->pic_num == picNumX){
					unmark_for_reference(listD->fs_stRef[i]);
					if (listD->fs_stRef[i]->is_output)
						return 1;
					return 0;
				}
			}
		}
		else {	// field
			if ((listD->fs_stRef[i]->is_ref & 1) && (!(listD->fs_stRef[i]->is_long & 1))){
				if (listD->fs_stRef[i]->topField->pic_num == picNumX){
					listD->fs_stRef[i]->topField->used_for_ref = 0;
					listD->fs_stRef[i]->is_ref &= 2;
					if (listD->fs_stRef[i]->is_used == 3)
						listD->fs_stRef[i]->frame->used_for_ref = 0;
					if (listD->fs_stRef[i]->is_ref == 0 && listD->fs_stRef[i]->is_output)
						return 1;
					return 0;
				}
			}
			if ((listD->fs_stRef[i]->is_ref & 2) && (!(listD->fs_stRef[i]->is_long & 2))){
				if (listD->fs_stRef[i]->btmField->pic_num == picNumX){
					listD->fs_stRef[i]->btmField->used_for_ref = 0;
					listD->fs_stRef[i]->is_ref &= 1;
					if (listD->fs_stRef[i]->is_used == 3)
						listD->fs_stRef[i]->frame->used_for_ref = 0;
					if (listD->fs_stRef[i]->is_ref == 0 && listD->fs_stRef[i]->is_output)
						return 1;
					return 0;
				}
			}
		}
	}
	return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Adaptive Memory Management: Mark long term picture unused
 ************************************************************************
 */
static int mm_unmark_long_term_for_reference(DecodedPictureBuffer *listD, StorablePicture *pic, int long_term_pic_num)
{
	int i;

	for (i = 0; i<listD->lt_ref_size; i++){
		if (pic->structure == FRAME){
			if ((listD->fs_ltRef[i]->is_ref == 3) && (listD->fs_ltRef[i]->is_long == 3)){
				if (listD->fs_ltRef[i]->frame->long_term_pic_num == long_term_pic_num) {
					unmark_for_long_term_reference(listD->fs_ltRef[i]);
					if (listD->fs_ltRef[i]->is_output)
						return 1;						
					return 0;
				}
			}
		}
		else {	// field
			if ((listD->fs_ltRef[i]->is_ref & 1) && ((listD->fs_ltRef[i]->is_long & 1))){
				if (listD->fs_ltRef[i]->topField->long_term_pic_num == long_term_pic_num){
					listD->fs_ltRef[i]->topField->used_for_ref = 0;
					listD->fs_ltRef[i]->topField->is_long_term = 0;
					listD->fs_ltRef[i]->is_ref &= 2;
					listD->fs_ltRef[i]->is_long &= 2;
					if (listD->fs_ltRef[i]->is_used == 3){
						listD->fs_ltRef[i]->frame->used_for_ref = 0;
						listD->fs_ltRef[i]->frame->is_long_term = 0;
					}
					if (listD->fs_ltRef[i]->is_ref == 0 && listD->fs_ltRef[i]->is_output)
						return 1;
					return 0;
				}
			}
			if ((listD->fs_ltRef[i]->is_ref & 2) && ((listD->fs_ltRef[i]->is_long & 2))){
				if (listD->fs_ltRef[i]->btmField->long_term_pic_num == long_term_pic_num)
				{
					listD->fs_ltRef[i]->btmField->used_for_ref = 0;
					listD->fs_ltRef[i]->btmField->is_long_term = 0;
					listD->fs_ltRef[i]->is_ref &= 1;
					listD->fs_ltRef[i]->is_long &= 1;
					if (listD->fs_ltRef[i]->is_used == 3){
						listD->fs_ltRef[i]->frame->used_for_ref = 0;
						listD->fs_ltRef[i]->frame->is_long_term = 0;
					}
					if (listD->fs_ltRef[i]->is_ref == 0 && listD->fs_ltRef[i]->is_output)
						return 1;
					return 0;
				}
			}
		}
	}
	return 0;
}


/*!
 ************************************************************************
 * \brief
 *    Assign a long term frame index to a short term picture
 ************************************************************************
 */
static int mm_assign_long_term_frame_idx(VideoParams *p_Vid, StorablePicture *pic, int diff, int long_term_frame_idx)
{
	DecodedPictureBuffer *listD = p_Vid->listD;
	uint32_t picNumX = (pic->structure==FRAME ? pic->frame_num : (2*pic->frame_num + 1)) - diff;
	int ret = 0;

	if (pic->structure == FRAME) {
		ret = unmark_long_term_frame_for_reference_by_frame_idx(listD, long_term_frame_idx);
	}
	else{
		uint32_t i;
		PictureStructure structure = FRAME;

		for (i = 0; i<listD->st_ref_size; i++){
			if (listD->fs_stRef[i]->is_ref & 1){
				if (listD->fs_stRef[i]->topField->pic_num == picNumX){
					structure = TOP_FIELD;
					break;
				}
			}
			if (listD->fs_stRef[i]->is_ref & 2){
				if (listD->fs_stRef[i]->btmField->pic_num == picNumX){
					structure = BOTTOM_FIELD;
					break;
				}
			}
		}
		if (structure == FRAME)
		    return H264D_PARSING_ERROR;
		//if (structure==FRAME)
		//	error ("field for long term marking not found",200);
#if 0//def DUMP_BUFFER_INFO
    if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)) {
        int i;
        if (listD->lt_ref_size > 0) {
            for (i = 0; i<listD->lt_ref_size; i++)
                DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "poc%d long%d\t", listD->fs_ltRef[i]->poc, listD->fs_ltRef[i]->long_term_frame_idx);
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "\n");
        }
    }
#endif
		ret = unmark_long_term_field_for_reference_by_frame_idx(p_Vid, structure, long_term_frame_idx, 0, 0, picNumX);
	}
	mark_pic_long_term(listD, pic, long_term_frame_idx, picNumX);
	return ret;
}


/*!
 ************************************************************************
 * \brief
 *    Set new max long_term_frame_idx
 ************************************************************************
 */
static int mm_update_max_long_term_frame_idx(DecodedPictureBuffer *listD, int max_long_term_frame_idx_plus1)
{
	uint32_t i;
	int num = 0;

	listD->max_long_term_pic_idx = max_long_term_frame_idx_plus1 - 1;
	for (i = 0; i < listD->lt_ref_size; i++)
		if (((int)listD->fs_ltRef[i]->long_term_frame_idx) > listD->max_long_term_pic_idx) {
			unmark_for_long_term_reference(listD->fs_ltRef[i]);
			num++;
		}
	return num;
}


/*!
 ************************************************************************
 * \brief
 *    Mark all long term reference pictures unused for reference
 ************************************************************************
 */
static int mm_unmark_all_short_term_for_reference (DecodedPictureBuffer *listD)
{
	uint32_t i;
	int num = 0;
	for (i = 0; i<listD->st_ref_size; i++)
		unmark_for_reference(listD->fs_stRef[i]);
	return num;
}

#if 0
/*!
 ************************************************************************
 * \brief
 *    Mark all long term reference pictures unused for reference
 ************************************************************************
 */
static int mm_unmark_all_long_term_for_reference (DecodedPictureBuffer *listD)
{
	return mm_update_max_long_term_frame_idx(listD, 0);
}
#endif

/*!
 ************************************************************************
 * \brief
 *    Mark the current picture used for long term reference
 ************************************************************************
 */
static int mm_mark_current_picture_long_term(VideoParams *p_Vid, DecodedPictureBuffer *listD, StorablePicture *pic, int long_term_frame_idx)
{
	int ret;
	if (pic->structure == FRAME)
		ret = unmark_long_term_frame_for_reference_by_frame_idx(listD, long_term_frame_idx);
	else
		ret = unmark_long_term_field_for_reference_by_frame_idx(p_Vid, pic->structure, long_term_frame_idx, 1, pic->pic_num, 0);

	pic->is_long_term = 1;
	pic->long_term_frame_idx = long_term_frame_idx;
	
	return ret;
}


/*!
 ************************************************************************
 * \brief
 *    Update the list of frame stores that contain reference frames/fields
 *
 ************************************************************************
 */
static void update_stref_list(DecodedPictureBuffer *listD)
{
	int i, j;
	
	for (i = 0, j = 0; i<listD->used_size; i++) {
		if (is_short_term_reference(listD->fs[i]))
			listD->fs_stRef[j++] = listD->fs[i];
    }

    listD->st_ref_size = j;
	//for (i = j; i<listD->max_size; i++)
	//	listD->fs_stRef[i] = NULL;
}


/*!
 ************************************************************************
 * \brief
 *    Update the list of frame stores that contain long-term reference
 *    frames/fields
 *
 ************************************************************************
 */
static void update_ltref_list(DecodedPictureBuffer *listD)
{
	int i, j;
	for (i = 0, j = 0; i<listD->used_size; i++) {
		if (is_long_term_reference(listD->fs[i]))
			listD->fs_ltRef[j++] = listD->fs[i];
    }

	listD->lt_ref_size = j;
	//for (i = j; i<listD->max_size; i++)
	//	listD->fs_ltRef[i] = NULL;
}


/*!
 ************************************************************************
 * \brief
 *    In JM: All stored pictures are output. Should be called to empty the buffer
 *    In this driver: All stored pictures are marked unused. remove pictures that aleady outputed
 *    diff: 1. this version do not output picture 
 *          2. this version do not remove un-outputed picture (picture may remain in dpb after flush)
 ************************************************************************
 */
void flush_dpb(void *ptVid)
{
	uint32_t idx = 0;
	VideoParams *p_Vid = (VideoParams *)ptVid;
	DecodedPictureBuffer *listD = p_Vid->listD;

    if(SHOULD_DUMP(D_BUF_DETAIL, p_Vid->dec_handle)){
        DUMP_MSG(D_BUF_DETAIL, p_Vid->dec_handle, "bf flush_dpb\n");
        dump_dpb(p_Vid->dec_handle, 0);
    }
    
	/* mark all frames unused */
	while (idx < listD->used_size) {
        LOG_PRINT(LOG_INFO, "flush_dpb %d: poc:%d is_ref:%d is_output:%d\n", 
            idx, listD->fs[idx]->poc, listD->fs[idx]->is_ref, listD->fs[idx]->is_output);

		unmark_for_reference(listD->fs[idx]);
		if (listD->fs[idx]->is_output){
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove from dpb at flush_dpb:%d\n",  listD->fs[idx]->phy_index);
			remove_picture_from_dpb(p_Vid, listD, idx, 1);
        }else{
			idx++;
        }
	}

    if(SHOULD_DUMP(D_BUF_DETAIL, p_Vid->dec_handle)){
        DUMP_MSG(D_BUF_DETAIL, p_Vid->dec_handle, "af flush_dpb\n");
        dump_dpb(p_Vid->dec_handle, 0);
    }

}


/*!
 ************************************************************************
 * \brief
 *    Perform Memory management for idr pictures
 *
 ************************************************************************
 */
static int idr_memory_management(VideoParams *p_Vid, StorablePicture *pic)
{
	DecodedPictureBuffer *listD = p_Vid->listD;
	uint32_t i;
	if (p_Vid->slice_hdr->no_output_of_prior_pics_flag)
	{
	    /* drop and do not output buffers in dpb according to spec. */
        /* release the buffer by calling remove_picture_from_dpb() with is_rec = 1 */
/*
        if (listD->last_picture) {  // avoid to release current buffer
    #ifdef LINUX
    printk("idr but oppsite field exist\n");
    #endif
	        return H264D_PARSING_ERROR;
	    }
*/
#if 1
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "idr remove all\n");
        
		output_all_picture(p_Vid); /* NOTE: still output all here, because un-outputed buffer may cause errors when max_buf_num != 0.  */
        while (listD->used_size > 0) {
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove from dpb at idr:%d\n",  listD->fs[0]->phy_index);
            //if (listD->last_picture && listD->last_picture->phy_index == listD->fs[0]->phy_index);
            //    remove_picture_from_dpb(p_Vid, listD, 0, 0);
            //else
			    remove_picture_from_dpb(p_Vid, listD, 0, 1);
		}
#else
		for (i = 0; i<listD->used_size; i++) {
			unmark_for_reference(listD->fs[i]);
			if (listD->fs[i]->is_output)
				remove_picture_from_dpb(p_Vid, listD, i, 1);
		}
#endif
		for (i = 0; i<listD->st_ref_size; i++)
			listD->fs_stRef[i] = NULL;
		for (i = 0; i<listD->lt_ref_size; i++)
			listD->fs_ltRef[i] = NULL;
		listD->used_size = 0;
	}
	else {
#if 1
        /* FIXME: should call output_all_picture() before flush_dpb() to relase all picture? */
		output_all_picture(p_Vid);
		flush_dpb(p_Vid);
		DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "idr: flush_dpb & output all\n");
#else
		flush_dpb(p_Vid);
		DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "idr: flush_dpb & output all\n");
		output_all_picture(p_Vid);
#endif
	}
	listD->last_picture = NULL;
	update_stref_list(listD);
	update_ltref_list(listD);
	if (p_Vid->slice_hdr->long_term_reference_flag){
		listD->max_long_term_pic_idx = 0;
		pic->is_long_term = 1;
		pic->long_term_frame_idx = 0;
	}
	else {
		listD->max_long_term_pic_idx = -1;
		pic->is_long_term = 0;
	}
	
	return H264D_OK;
}



/*!
 ************************************************************************
 * \brief
 *    Perform Adaptive memory control decoded reference picture marking process
 ************************************************************************
 */
static int adaptive_memory_management(VideoParams *p_Vid, StorablePicture *pic)
{
	int mmco_idx, num, k;
	int ret = H264D_OK;

	p_Vid->last_has_mmco_5 = 0;

	for (mmco_idx = 0; p_Vid->drpmb[mmco_idx].memory_management_control_operation != 0; mmco_idx++){
		switch (p_Vid->drpmb[mmco_idx].memory_management_control_operation)
		{
		case 1:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "unmark short term\n");
			if (mm_unmark_short_term_for_reference(p_Vid->listD, pic,  p_Vid->drpmb[mmco_idx].difference_of_pic_nums_minus1+1))
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			update_stref_list(p_Vid->listD);
			break;
		case 2:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "unmark long term\n");
			if (mm_unmark_long_term_for_reference(p_Vid->listD, pic, p_Vid->drpmb[mmco_idx].long_term_pic_num)) {
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			}
			update_ltref_list(p_Vid->listD);
			break;
		case 3:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "set long term %d\n", p_Vid->drpmb[mmco_idx].long_term_frame_idx);
			if ((ret = mm_assign_long_term_frame_idx(p_Vid, pic, p_Vid->drpmb[mmco_idx].difference_of_pic_nums_minus1+1, p_Vid->drpmb[mmco_idx].long_term_frame_idx)) < 0) {
			    ret = H264D_PARSING_ERROR;

                if(SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)){
                    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "assign long term error: mmco5 %d\n", p_Vid->last_has_mmco_5);
                    dump_dpb_info(p_Vid, 0);
                }
			    goto exit_adaptive_mmco;
			}
            if (ret)
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			update_stref_list(p_Vid->listD);
			update_ltref_list(p_Vid->listD);
			break;
		case 4:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "update max long term idx\n");
			num = mm_update_max_long_term_frame_idx(p_Vid->listD, p_Vid->drpmb[mmco_idx].max_long_term_frame_idx_plus1);
			// remove non-reference long term picture
			for (k = 0; k<num; k++)
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			update_ltref_list(p_Vid->listD);
			break;
		case 5:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "mmco5\n");
			num = mm_unmark_all_short_term_for_reference(p_Vid->listD);
			//mm_unmark_all_long_term_for_reference(p_Vid->listD);
			num += mm_update_max_long_term_frame_idx(p_Vid->listD, 0);
			for (k = 0; k<num; k++)
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			p_Vid->listD->st_ref_size = 0;
			p_Vid->listD->lt_ref_size = 0;
			for (k = 0; k<p_Vid->listD->max_size-1; k++) 
				p_Vid->listD->fs_stRef[k] = p_Vid->listD->fs_ltRef[k] = NULL;
			p_Vid->last_has_mmco_5 = 1;
			break;
		case 6:
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "mark current long term\n");
			if (mm_mark_current_picture_long_term(p_Vid, p_Vid->listD, pic, p_Vid->drpmb[mmco_idx].long_term_frame_idx))
				remove_non_ref_picture_from_dpb(p_Vid, p_Vid->listD);
			// check number of reference frame
			break;
		default:
			// error
			break;
		}
	}
exit_adaptive_mmco:
	if (p_Vid->last_has_mmco_5){
	    if (p_Vid->listD->last_picture)
	        ret = H264D_PARSING_ERROR;
		pic->pic_num = pic->frame_num = 0;

		switch (pic->structure)
		{
		case TOP_FIELD:
			pic->poc = pic->toppoc = p_Vid->toppoc = 0;
			break;
		case BOTTOM_FIELD:
			pic->poc = pic->botpoc = p_Vid->bottompoc = 0;
			break;
		case FRAME:
			pic->toppoc -= pic->poc;
			pic->botpoc -= pic->poc;

			p_Vid->toppoc = pic->toppoc;
			p_Vid->bottompoc = pic->botpoc;

			pic->poc = iMin(pic->toppoc, pic->botpoc);
			p_Vid->framepoc = pic->poc;
			break;
		}
		p_Vid->ThisPOC = pic->poc;
        
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "mmco 5\n");
        
		output_all_picture(p_Vid);
		flush_dpb(p_Vid);
	}
    check_partial_long_term(p_Vid, p_Vid->listD);
    
    if (SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle))
    {
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "adaptive mm:");
        dump_release_list(p_Vid, D_BUF_REF);
    }

	update_stref_list(p_Vid->listD);
	update_ltref_list(p_Vid->listD);

    return ret;
}


/*!
 ************************************************************************
 * \brief
 *    Returns the size of the dpb depending on level and picture size
 ************************************************************************
 */
uint32_t getDpbSize(SeqParameterSet *sps)
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
    
    /* if VUI syntax element exist, use it instead */
	if (sps->vui_parameters_present_flag && sps->vui_seq_parameters.bitstream_restriction_flag){
	    //if ((int)sps->vui_seq_parameters.max_dec_frame_buffering > size)
			//error ("max_dec_frame_buffering larger than MaxDpbSize", 500);
		size = iMax(1, sps->vui_seq_parameters.max_dec_frame_buffering);
	}
	return size;
}


/*
 * Mark the buffer output is done and no longer used (by application)
 * Return RELEASE_UNUSED_BUFFER if it is not referenced
 */
#if USE_FEED_BACK
int buffer_used_done(void *ptVid, DecodedPictureBuffer *listD, int idx)
{
    VideoParams *p_Vid = (VideoParams *)ptVid;
	int i;
	for (i = 0; i < listD->used_size; i++){
		if (listD->fs[i]->phy_index == idx) {
			listD->fs[i]->is_output = 1;
			if (listD->fs[i]->is_ref == 0) {
                DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove unref from dpb at used done:%d\n",  fs->phy_index);
				remove_picture_from_dpb(p_Vid, listD, i, 0);
				return RELEASE_UNUSED_BUFFER;
			}
			return OUTPUT_USED_BUFFER;
		}
	}
	return 0;
}
#endif


/*!
 ************************************************************************
 * \brief
 *      If a gap in frame_num is found, try to fill the gap
 * \param p_Vid
 *
 ************************************************************************
 */
int fill_frame_num_gap(void *ptVid)
{
	VideoParams *p_Vid = (VideoParams*)ptVid;
#if 0
    StorablePicture gap_pic;
	StorablePicture *pic = &gap_pic;
#else
	StorablePicture *pic = p_Vid->gap_pic;
#endif
	int CurrFrameNum;
	int UnusedShortTermFrameNum;
	int tmp1 = p_Vid->slice_hdr->delta_pic_order_cnt[0];
	int tmp2 = p_Vid->slice_hdr->delta_pic_order_cnt[1];
	uint32_t MaxFrameNum = (1 << p_Vid->active_sps->log2_max_frame_num);
	//int old_poc = INT_MAX;
	
	p_Vid->slice_hdr->delta_pic_order_cnt[0] = p_Vid->slice_hdr->delta_pic_order_cnt[1] = 0;
	UnusedShortTermFrameNum = (p_Vid->prev_frame_num + 1) % MaxFrameNum;
	CurrFrameNum = p_Vid->frame_num;

	while (CurrFrameNum != UnusedShortTermFrameNum)
	{
		alloc_storable_picture(pic, FRAME);
		pic->coded_frame = 1;
		pic->pic_num = UnusedShortTermFrameNum;
		pic->frame_num = UnusedShortTermFrameNum;
		pic->non_existing = 1;
		pic->is_output = 1;
		pic->used_for_ref = 1;
		//pic->adaptive_ref_pic_marking_mode_flag = 0;

		p_Vid->frame_num = UnusedShortTermFrameNum;
		if (p_Vid->active_sps->pic_order_cnt_type != 0){
			decode_poc(p_Vid);
        }
        //if (p_Vid->framepoc == old_poc)
        //    return H264D_PARSING_ERROR;
        //old_poc = p_Vid->framepoc;
		pic->toppoc = p_Vid->toppoc;
		pic->botpoc = p_Vid->bottompoc;
		pic->framepoc = p_Vid->framepoc;
		pic->poc = p_Vid->framepoc;
		pic->phy_index = -1;
		// ==============================================================
        pic->refpic_addr = p_Vid->pRecBuffer->dec_yuv_buf_virt;
        pic->refpic_addr_phy = p_Vid->pRecBuffer->dec_yuv_buf_phy;
        pic->mbinfo_addr = p_Vid->pRecBuffer->dec_mbinfo_buf_virt;
        pic->mbinfo_addr_phy = p_Vid->pRecBuffer->dec_mbinfo_buf_phy;
        // ==============================================================
		pic->is_damaged = 1;
		if (store_picture_in_dpb(p_Vid, pic, 0) < 0)
		    break;
		//pic = p_Vid->gap_pic;
		pic->valid = 0;
		p_Vid->prev_frame_num = UnusedShortTermFrameNum;
		UnusedShortTermFrameNum = (UnusedShortTermFrameNum + 1) % MaxFrameNum;
	}
	p_Vid->slice_hdr->delta_pic_order_cnt[0] = tmp1;
	p_Vid->slice_hdr->delta_pic_order_cnt[1] = tmp2;
	p_Vid->frame_num = CurrFrameNum;
	return H264D_OK;
}


/*!
 ************************************************************************
 * \brief
 *    find the smallest POC of un-outputed frame in the DPB.
 *    from get_smallest_poc() of JM
 *
 ************************************************************************
 */
static void get_smallest_output_poc(DecodedPictureBuffer *listD, int *poc, int *pos)
{
	int i;

	*pos = -1;
	*poc = INT_MAX;
	if (listD->used_size < 1)
		return;

	for (i = 0; i<listD->used_size; i++){
	#if 1	// 2010.07.28
		if (listD->fs[i]->is_used) {
	#else
		if (listD->fs[i]->is_used == 3) {
	#endif
			if (((int)listD->fs[i]->poc < (*poc)) && (!listD->fs[i]->is_return_output)){
				*poc = listD->fs[i]->poc;
				*pos = i;
			}
		}
	}
}


static void set_output_frame(FrameInfo *fr_info, int poc, FrameStore *fs)
{
    fr_info->i16POC = poc;
    fr_info->i8BufferIndex = fs->phy_index;
#ifndef VG_INTERFACE
    fr_info->u8Structure = (fs->is_used == 3 ? 0 : fs->is_used);
    fr_info->bDamaged = fs->is_damaged;
#endif
}


/*
 * Get all decoded buffer index list from DPB, in increasing POC order
 * (and mark them as outputed)
 */
int output_all_picture(void *ptVid)
{
	VideoParams *p_Vid = (VideoParams *)ptVid;
	DecodedPictureBuffer *listD = p_Vid->listD;
	FrameStore *fs;
	int pos, poc;

	while (1) {
        get_smallest_output_poc(listD, &poc, &pos);
        if(pos == -1){
            break;
        }
        
		fs = listD->fs[pos];
		p_Vid->output_poc = poc;
		fs->is_return_output = 1;
		if (fs->frame->non_existing) {
			continue;
		}
        set_output_frame(&p_Vid->output_frame[p_Vid->output_frame_num], poc, fs);
        p_Vid->output_frame_num++;
        p_Vid->unoutput_buffer_num--;
#if !USE_FEED_BACK
        fs->is_output = 1;
        if(fs->is_ref == 0){
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove unref from dpb at output:%d\n",  fs->phy_index);
            remove_picture_from_dpb(p_Vid, listD, pos, 1);
        }
#endif

	}
	p_Vid->output_poc = INT_MIN;

	return p_Vid->output_frame_num;
}


#if CHECK_USED_BUF_NUM
/*
 * Get the number of existing frame in DPB
 */
static int get_existing_fr_num(DecodedPictureBuffer *listD)
{
    int existing_cnt = 0;
    int i;
	FrameStore *fs;

    for(i = 0; i < listD->used_size; i++){
        fs = listD->fs[i];

        if(fs->frame->non_existing){
			continue;
        }
        existing_cnt++;
    }

    return existing_cnt;
}
#endif

#if CHECK_UNOUTPUT_BUF_NUM
/*
 * Get the number of frame that is not yet output in DPB
 */
static int get_unoutput_fr_num(DecodedPictureBuffer *listD)
{
    int unoutput_cnt = 0;
    int i;
	FrameStore *fs;

    for(i = 0; i < listD->used_size; i++){
        fs = listD->fs[i];

        if(fs->frame->non_existing){
			continue;
        }
        if(fs->is_return_output)
			continue;
        unoutput_cnt++;
    }

    return unoutput_cnt;
}
#endif


/*
 * output frames and mark them as outputed
 */
int fill_output_list(void *ptVid)
{
    VideoParams *p_Vid = ptVid;
	DecodedPictureBuffer *listD = p_Vid->listD;
	FrameStore *fs;
	int poc, pos;
#if USE_FEED_BACK
	int release_num = 0;
#endif
#if USE_OUTPUT_BUF_NUM
    extern int output_buf_num;
#endif

#if CHECK_UNOUTPUT_BUF_NUM
    {
        int unoutput_num = 0;
        unoutput_num = get_unoutput_fr_num(listD);
        if(unoutput_num != p_Vid->unoutput_buffer_num){
            printk("unoutput_num != p_Vid->unoutput_buffer_num: %d %d fr:%d", unoutput_num, p_Vid->unoutput_buffer_num, p_Vid->dec_handle->u32DecodedFrameNumber);
            while(1);
        }else{
            //printk("unoutput_num: %d\n", unoutput_num);
        }
    }
#endif
#if CHECK_USED_BUF_NUM
    {
        int existing_num = get_existing_fr_num(listD);
        if(existing_num != p_Vid->used_buffer_num){
            printk("existing_num != p_Vid->used_buffer_num: %d %d fr:%d", existing_num, p_Vid->used_buffer_num, p_Vid->dec_handle->u32DecodedFrameNumber);
            while(1);
        }else{
            //printk("existing_num: %d\n", existing_num);
        }
    }
#endif

    /* special case: only output the last inserted frame */
#if USE_OUTPUT_BUF_NUM
    if (p_Vid->max_buffered_num == NO_BUFFERED || output_buf_num == 0) 
#else
    if (p_Vid->max_buffered_num == NO_BUFFERED) 
#endif
    {
        fs = listD->fs[p_Vid->listD->used_size-1];
        set_output_frame(&p_Vid->output_frame[p_Vid->output_frame_num], fs->poc, fs);
        p_Vid->output_frame_num++;
        p_Vid->unoutput_buffer_num--;

        p_Vid->output_poc = fs->frame->poc;
        fs->is_return_output = 1;
#if !USE_FEED_BACK
        fs->is_output = 1;
        if(fs->is_ref == 0){
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove unref from dpb at output:%d\n",  fs->phy_index);
            remove_picture_from_dpb(p_Vid, p_Vid->listD, p_Vid->listD->used_size-1, 1);
        }
#endif
        return p_Vid->output_frame_num;
    }

    /* p_Vid->max_buffered_num == -1 or > 0 or output_buf_num != 0 */


#if OUTPUT_POC_MATCH_STEP
    /* count poc step: to be removed */
	if (p_Vid->even_counter >= 0) {
		if (listD->used_size > 0) {
			if (listD->fs[listD->used_size-1]->poc & 1)
				p_Vid->even_counter = -1;
			else {
				if (p_Vid->poc_step == 1) {
					p_Vid->even_counter++;
					if (p_Vid->even_counter > p_Vid->max_buffered_num)
						p_Vid->poc_step = 2;
				}
			}
		}
	}
    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "return output: max %d, used %d unoutput:%d, curr out poc %d, step %d (this poc %d)\n", 
            p_Vid->max_buffered_num, p_Vid->used_buffer_num, p_Vid->unoutput_buffer_num, 
            p_Vid->output_poc, p_Vid->poc_step, p_Vid->ThisPOC);
#else
    DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "return output: max %d, used %d unoutput:%d, curr out poc %d, (this poc %d)\n", 
            p_Vid->max_buffered_num, p_Vid->used_buffer_num, p_Vid->unoutput_buffer_num, 
            p_Vid->output_poc, p_Vid->ThisPOC);
#endif


	while (1) 
    {
        unsigned int used_buf_num = p_Vid->used_buffer_num;
        unsigned int output_flg = 0;
        
#if USE_FEED_BACK
        used_buf_num -= release_num;
#endif

		get_smallest_output_poc(listD, &poc, &pos);
        if(pos == -1){ /* no more frame can be outputed */
            break;
        }

        /* output frame if the number of existing and un-released frame in DPB > max buffer number */
        if (used_buf_num > p_Vid->max_buffered_num) {
            output_flg |= 1;
        }

#if OUTPUT_POC_MATCH_STEP
        /* output frame if the next POC matches the poc_step rule */
        if (poc == (p_Vid->output_poc + p_Vid->poc_step)) {
            output_flg |= 2;
        }
#endif

#if USE_OUTPUT_BUF_NUM
        /* output frame if the number of existing and un-outputed frame in DPB > output buffer number */
        if(p_Vid->unoutput_buffer_num > output_buf_num){
            output_flg |= 4;
        }
#endif
        if(!output_flg){
            break;
        }
        DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "output flg:%01X\n",  output_flg);

        
		fs = listD->fs[pos];
		p_Vid->output_poc = poc;
		fs->is_return_output = 1;
		if (fs->frame->non_existing) {
			continue;
		}
        set_output_frame(&p_Vid->output_frame[p_Vid->output_frame_num], poc, fs);
        p_Vid->output_frame_num++;
        p_Vid->unoutput_buffer_num--;

#if USE_FEED_BACK
        if (listD->fs[pos]->is_ref == 0)    /* the buffer can be release */
            release_num++;
#else
        fs->is_output = 1;
        if(fs->is_ref == 0){
            DUMP_MSG(D_BUF_REF, p_Vid->dec_handle, "remove unref from dpb at output:%d\n",  fs->phy_index);
            remove_picture_from_dpb(p_Vid, listD, pos, 1);
        }
#endif
	}


    if(SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle))
    {
        dump_dpb_info(p_Vid, 0);
    }

	return p_Vid->output_frame_num;
}

