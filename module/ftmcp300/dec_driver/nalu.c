#ifdef LINUX
	#include <linux/kernel.h>
#else
	#include <stdio.h>
#endif
#include "nalu.h"
#include "portab.h"
#include "vlc.h"
//#if defined(LINUX) & defined(VG_INTERFACE)
//#include "log.h"
//#endif
#include "global.h"
#include "h264_reg.h"


#if USE_SW_PARSER == 0
/*
 * Seek to the start of the next NALU (by searching start code: 0x00, 0x00, 0x01) and get nalu type 
 * NOTE: use HW syntax parser
 * return value:
 *  <0: H264D_ERR_BS_EMPTY
 *      H264D_PARSING_ERROR
 *      H264D_RELOAD_HEADER
 * ==0: H264D_OK
 */
int get_next_nalu(void *ptDecHandle, NALUnit *nalu, unsigned char no_more_bs)
{
	DecoderParams *p_Dec = (DecoderParams*)ptDecHandle;
	volatile H264_reg_io *ptReg_io = (H264_reg_io *)((uint32_t)p_Dec->pu32BaseAddr);
	uint32_t forbidden_zero_bit, tmp, flag, flag_long, flag_short;
	int ret = H264D_OK;
	
	byte_align(ptReg_io);
    flag = ptReg_io->BITSHIFT; /* NOTE: this also read after the last register write */
    
    LOG_PRINT(LOG_TRACE, "get_next_nalu entering loop\n");
    
    while(1){
        if (!no_more_bs && (p_Dec->u32IntSts & INT_BS_LOW)){
            return H264D_RELOAD_HEADER;
        }
        if (decoder_bitstream_empty(p_Dec)){ /* check both bBSEmpty and INTSTS */
			return H264D_ERR_BS_EMPTY;
        }

        /* HW syntax parser discard the 0x03 byte in the (0x00, 0x00, 0x03) byte sequence (when 0x44.4 prev_off == 0 )
         * Here is to avoid the false start code due to the discarded 0x03 byte 
         * false start code (short): 0x00,0x00,0x03,0x01
         *  true start code (short): 0x00,0x00,0x01
         * false start code (long) : 0x00,0x00,0x03,0x00,0x01
         *  true start code (long) : 0x00,0x00,0x00,0x01
         */
        flag_short = flag & (BIT10|BIT11);     /* indicates if a discarded 0x03(emulation_prevention_three_byte) byte occurred within the short start code */
        flag_long = flag & (BIT9|BIT10|BIT11); /* indicates if a discarded 0x03(emulation_prevention_three_byte) byte occurred within the long start code */

        tmp = show_bits(ptReg_io, 32);

        if( (tmp == 0x00000001 && flag_long == 0) || 
            (((tmp>>8)&0xFFFFFF) == 0x000001 && flag_short == 0)){
            /* true start code is found */
            break;
        }
        
		tmp = read_u(ptReg_io, 8); /* shift one byte */
        flag = ptReg_io->BITSHIFT; /* NOTE: this also read after the last register write */
    }
    
    LOG_PRINT(LOG_TRACE, "get_next_nalu loop stop\n");
    
	if (tmp == 0x00000001) {
		//nalu->is_long_start_code = 1;
		tmp = read_u(ptReg_io, 32);
	}
    else {
		//nalu->is_long_start_code = 0;
		tmp = read_u(ptReg_io, 24);
	}
    
    /* get nalu type */
    LOG_PRINT(LOG_TRACE, "nalu: %08x\n", show_bits(ptReg_io, 32));

	tmp = read_u(ptReg_io, 8);
	forbidden_zero_bit = tmp & BIT7;
	if (forbidden_zero_bit){
		nalu->nal_unit_type = 15;
		LOG_PRINT(LOG_ERROR, "forbidden zero bit(%x) should be 0\n", forbidden_zero_bit);
		return H264D_PARSING_ERROR;
	}
	nalu->nal_ref_idc = (byte)((tmp & 0x00000060) >> 5);
	nalu->nal_unit_type = (byte)(tmp & 0x0000001F);

	return ret;
}
#endif


#if USE_SW_PARSER
/*
 * Seek to the start of the next NALU (by searching start code: 0x00, 0x00, 0x01) and get nalu type 
 * NOTE: use SW syntax parser
 * return value:
 *  <0: H264D_ERR_BS_EMPTY
 *      H264D_PARSING_ERROR
 * ==0: H264D_OK
 */
int sw_get_next_nalu(void *ptDecHandle, NALUnit *nalu, unsigned char no_more_bs)
{
	DecoderParams *p_Dec = (DecoderParams*)ptDecHandle;
    PARSER_HDL_PTR p_Par = GET_PARSER_HDL(p_Dec);
	uint32_t forbidden_zero_bit, tmp;
	
    if(sw_seek_to_next_start_code(p_Par)){
        nalu->nal_unit_type = NALU_TYPE_EOS_MARK; /* set it to indicate the start code is not found and the end of bitstream buffer is reached */
        return H264D_ERR_BS_EMPTY;
    }
    
    sw_skip_byte(p_Par, 3); /* skip 0x00, 0x00, 0x01 */
#if DBG_SW_PARSER
    LOG_PRINT(LOG_TRACE, "nalu: %08x\n", SHOW_BITS(p_Par, 32));
#endif

    /* speedup version of sw_read_bits(p_Par, 8), when we know it is byte aligned and no need to handle bs_empty */
    tmp = *p_Par->cur_byte;
    sw_skip_byte(p_Par, 1);

    if(sw_bs_emtpy(p_Par)){
        return H264D_PARSING_ERROR;
    }
    
	forbidden_zero_bit = tmp & BIT7;
	if (forbidden_zero_bit)
    {
		nalu->nal_unit_type = 15;
		LOG_PRINT(LOG_ERROR, "forbidden zero bit(%x) should be 0\n", forbidden_zero_bit);
		return H264D_PARSING_ERROR;
	}
	nalu->nal_ref_idc = (byte)((tmp & 0x00000060) >> 5);
	nalu->nal_unit_type = (byte)(tmp & 0x0000001F);

	return H264D_OK;
}
#endif


