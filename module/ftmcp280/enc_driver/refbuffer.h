#ifndef _REFBUFFER_H_
#define _REFBUFFER_H_

#include "portab.h"
#include "define.h"

typedef struct storable_picture_t
{
#ifndef ADDRESS_UINT32
	uint8_t *srcpic_addr_virt;
	uint8_t *srcpic_addr_phy;
	uint8_t *recpic_luma_virt;
	uint8_t *recpic_luma_phy;
	uint8_t *recpic_chroma_virt;
	uint8_t *recpic_chroma_phy;
#else
	uint32_t srcpic_addr_virt;
	uint32_t srcpic_addr_phy;
    uint32_t recpic_luma_virt;
    uint32_t recpic_luma_phy;
    uint32_t recpic_chroma_virt;
    uint32_t recpic_chroma_phy;
#endif
	int32_t frame_num;
	//int32_t pic_num;				// P-slice, short-term, frame

	int32_t poc;					// B-slice, short-term, frame or field

	uint8_t  used_for_ref;
	uint8_t  slice_type;
	int8_t   phy_index;
	uint32_t poc_lsb;
	byte     idr_flag;
	PictureStructure structure;     //  0: frame, 1: top, 2: bottom

    uint32_t ave_qp;
    uint32_t satd;
} StorablePicture;


typedef struct frame_store_t
{
	uint32_t frame_num;
	int32_t poc;	

	StorablePicture *frame;
	StorablePicture *topField;
	StorablePicture *btmField;

	uint8_t phy_index;
	byte is_used;
	byte is_ref;
} FrameStore;

typedef struct decoded_picture_buffer_t
{
	FrameStore *fs[MAX_DPB_SIZE];
	FrameStore *last_picture;

	uint8_t used_size;
	uint8_t max_size;
}DecodedPictureBuffer;

int init_lists(void *vid, int slice_type);
int store_picture_in_dpb(void *vid, StorablePicture **pic);
int flush_all_picture_in_dbp(void *vid);
int dump_list(void *param);


#endif
