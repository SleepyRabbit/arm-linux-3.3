#ifdef LINUX
    #include <linux/kernel.h>
#endif
#include "vlc.h"
#include "define.h"
#include "global.h"

int paking_idx;

extern unsigned int use_ioremap_wc;

#ifndef AUTO_PAKING_HEADER
void u_v_prev(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value, int32_t len)
{
	ptReg_cmd->CMD1 = value;
	ptReg_cmd->CMD2 = BIT6 | (len&0x3F);
	ptReg_cmd->RESERVED[0] = 0;
}
void u_v(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value, int32_t len)
{
	ptReg_cmd->CMD1 = value;
	ptReg_cmd->CMD2 = BIT8 | BIT6 | (len&0x3F);
	ptReg_cmd->RESERVED[0] = 0;
}
void se_v(volatile H264Reg_Cmd *ptReg_cmd, int32_t value)
{
	uint32_t tmp = iAbs(value)*2 - (value>0);
	ptReg_cmd->CMD1 = tmp;
	ptReg_cmd->CMD2 = BIT8 | BIT7 | BIT6;
	ptReg_cmd->RESERVED[0] = 0;
}
void ue_v(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value)
{
	ptReg_cmd->CMD1 = value;
	ptReg_cmd->CMD2 = BIT8 | BIT7 | BIT6;	
	ptReg_cmd->RESERVED[0] = 0;
}
void rbsp_trailing_bits(volatile H264Reg_Cmd *ptReg_cmd)
{
	ptReg_cmd->CMD2 = BIT9 | BIT8;
}
#else
void u_v_prev(volatile H264Reg_Mem *ptReg_mem, uint32_t value, int32_t len)
{
    ptReg_mem->SLICE_HDR[paking_idx] = (len&0x3F);
    ptReg_mem->SLICE_HDR[paking_idx+1] = value;
    paking_idx += 2;
}
void u_v(volatile H264Reg_Mem *ptReg_mem, uint32_t value, int32_t len)
{
    ptReg_mem->SLICE_HDR[paking_idx] = BIT6 | (len&0x3F);
    ptReg_mem->SLICE_HDR[paking_idx+1] = value;
    paking_idx += 2;
}
void se_v(volatile H264Reg_Mem *ptReg_mem, int32_t value)
{
    uint32_t tmp = iAbs(value)*2 - (value>0);
    ptReg_mem->SLICE_HDR[paking_idx] = BIT6|BIT7;
    ptReg_mem->SLICE_HDR[paking_idx+1] = tmp;
    paking_idx += 2;
}
void ue_v(volatile H264Reg_Mem *ptReg_mem, uint32_t value)
{
    ptReg_mem->SLICE_HDR[paking_idx] = BIT6|BIT7;
    ptReg_mem->SLICE_HDR[paking_idx+1] = value;
    paking_idx += 2;
}
void rbsp_trailing_bits(volatile H264Reg_Mem *ptReg_mem)
{
	ptReg_mem->SLICE_HDR[paking_idx-2] |= BIT8;
}
int paking_last_syntax(volatile H264Reg_Mem *ptReg_mem, volatile H264Reg_Cmd *ptReg_cmd)
{
    int counter = 0;
    uint32_t tmp;
    ptReg_mem->SLICE_HDR[paking_idx-2] |= BIT9;
    ptReg_cmd->CMD2 = BIT12;    // auto paking header go

    if (use_ioremap_wc)
        tmp = ptReg_cmd->CMD2;
    while (!(ptReg_cmd->STS0 & BIT1)) {
        counter++;
        if (counter > VLC_WAIT_ITERATION)
            return H264E_ERR_POLLING_TIMEOUT;
    }
    return H264E_OK;
}
#endif

int close_bitstream(volatile H264Reg_Cmd *ptReg_cmd)
{
    uint32_t counter = 0;
    uint32_t tmp;
	ptReg_cmd->CMD2 = BIT10;
    if (use_ioremap_wc)
        tmp = ptReg_cmd->CMD2;
	while (!(ptReg_cmd->STS0 & BIT1)) {
	    counter++;
        if (counter > MAX_POLLING_ITERATION)
            return H264E_ERR_POLLING_TIMEOUT;
	}
	return H264E_OK;
}

void clear_bs_length(void *ptEncHandle, volatile H264Reg_Cmd *ptReg_cmd)
{
    uint32_t tmp;
    //((EncoderParams *)ptEncHandle)->u32OutputedBSLength = 0;
    ptReg_cmd->CMD2 = BIT11;
    if (use_ioremap_wc)
        tmp = ptReg_cmd->CMD2;
}

