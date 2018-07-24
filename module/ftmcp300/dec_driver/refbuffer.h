#ifndef _REFBUFFER_H_
#define _REFBUFFER_H_

#include "define.h"
#include "portab.h"
#include "params.h"
#include "global.h"

//! definition a picture (field or frame)

struct storable_picture_t
{
	uint8_t *refpic_addr;
	uint8_t *refpic_addr_phy;
	uint8_t *mbinfo_addr;
	uint8_t *mbinfo_addr_phy;
	int32_t frame_num;
	int32_t pic_num;				// P-slice, short-term, frame
	int32_t long_term_frame_idx;	// P-slice or B-slice, long-term, field
	int32_t	long_term_pic_num;		// P-slice or B-slice, long-term, frame

	int32_t toppoc;
	int32_t botpoc;
	int32_t framepoc;
	int32_t poc;

	StorablePicture *sp_top;     // for mb aff, if frame for referencing the top field
	StorablePicture *sp_btm;     // for mb aff, if frame for referencing the bottom field
	StorablePicture *sp_frame;   // for mb aff, if field for referencing the combined frame

	uint8_t valid;

	byte used_for_ref; /* set to nal_reference_idc from NAL unit in init_picture() */
	byte non_existing;
	byte is_output;   /* 1 if this picture is outputed by driver (the only case is the picture is added at fill_frame_num_gap()) */
	byte coded_frame; /* 1 if this picture is a frame, not a field */
	byte bottom_field_first;
	byte is_long_term;
	uint8_t slice_type;
	int8_t  phy_index; /* buffer index. -1 for non-existing frames */
	byte is_damaged;
	PictureStructure structure;
};


//! Frame Stores for Decoded Picture Buffer
typedef struct frame_store_t
{
    // start of reserved fields in reset_frame_store
	StorablePicture *frame;
	StorablePicture *topField;
	StorablePicture *btmField;
    // end of reserved fields in reset_frame_store
    
	uint32_t frame_num; // first non-reserved field. NOTE: do not change its order (see __reset_frame_store() )
	uint32_t long_term_frame_idx;
	int32_t frame_num_wrap;
	int32_t poc;

	uint8_t valid;
	byte is_used;     //!<  0=empty; 1=top; 2=bottom; 3=both fields (or frame)
	byte is_long;     //!<  0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
	byte is_ref;      //!<  0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
	byte is_orig_ref; //!<  original marking by nal_ref_idc: 0=not used for ref; 1=top used; 2=bottom used; 3=both fields (or frame) used
	byte is_output;   // a flag to indicate whether this frame is not used by application (1: means it is no longer used by application user)
	//byte non_existing;
	byte is_return_output; /* whether this frame is outputed by driver */
	int8_t phy_index; /* buffer index */
	uint8_t is_damaged;
}FrameStore;


struct decoded_picture_buffer_t
{
    // start of reserved fields in reset_dpb
	FrameStore *fs[MAX_DPB_SIZE];
    // end of reserved fields in reset_dpb

    // first non-reserved field. NOTE: do not change its order (see reset_dpb() )
	FrameStore *fs_stRef[MAX_DPB_SIZE]; /* pointers point to fs */
	FrameStore *fs_ltRef[MAX_DPB_SIZE]; /* pointers point to fs */
	FrameStore *last_picture;           /* last inserted/stored field (not frame) picture */

	uint8_t lt_ref_size; /* number of valid elements in fs_ltRef array */
	uint8_t st_ref_size; /* number of valid elements in fs_stRef array */
	uint8_t used_size;   /* number of valid elements in fs array */
	int8_t  max_long_term_pic_idx;
	uint8_t max_size;
} ;


void init_lists(void *ptVid);
void reorder_list(void *ptVid, void *ptSliceHdr);
void init_mbaff_list(void *ptVid);
int store_picture_in_dpb(void *ptVid, StorablePicture *ptPic, byte adaptive_ref_pic_marking_mode_flag);
void alloc_storable_picture(StorablePicture *ptPic, PictureStructure structure);
int fill_frame_num_gap(void *ptVid);
uint32_t getDpbSize(SeqParameterSet *sps);
void flush_dpb(void *ptVid);
void init_dpb(void *ptDecHandle, int is_rec);
void remove_picture_from_dpb(void *p_Vid, DecodedPictureBuffer *listD, int pos, int is_rec);


int fill_output_list(void *ptVid);

#if USE_FEED_BACK
int buffer_used_done(void *ptVid, DecodedPictureBuffer *listD, int idx);
#endif
int output_all_picture(void *ptVid);
int fill_empty_list(void *ptVid);
int replace_damgae_list(void *ptVid);


#endif /* _REFBUFFER_H_ */
