#ifndef _NALU_H_
#define _NALU_H_

#include "portab.h"

#define NALU_TYPE_EOS_MARK 32 /* a special mark (non-exist value in normal bitstream) for nal_unit_type:
                                 (nal_unit_type == NALU_TYPE_EOS_MARK && retval == H264D_ERR_BS_EMPTY) 
                                 indicates the start code is not found and the end of bitstream buffer is reached */
typedef struct nalu_t
{
	byte nal_ref_idc;
	byte nal_unit_type;
	//byte forbidden_zero_bit;
	//byte is_long_start_code;
}NALUnit;

int get_next_nalu(void *ptDecHandle, NALUnit *nalu, unsigned char no_more_bs);
int sw_get_next_nalu(void *ptDecHandle, NALUnit *nalu, unsigned char no_more_bs);

#endif
