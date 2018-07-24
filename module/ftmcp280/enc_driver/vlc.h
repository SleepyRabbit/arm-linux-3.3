#ifndef _VLC_H_
    #define _VLC_H_

    #include "portab.h"
    #include "mcp280_reg.h"
    
    #define VLC_WAIT_ITERATION  0x100
    
    #define AUTO_PAKING_HEADER
#ifndef AUTO_PAKING_HEADER
	void u_v_prev(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value, int32_t len);
	void u_v(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value, int32_t len);
	void se_v(volatile H264Reg_Cmd *ptReg_cmd, int32_t value);
	void ue_v(volatile H264Reg_Cmd *ptReg_cmd, uint32_t value);
    void rbsp_trailing_bits(volatile H264Reg_Cmd *ptReg_cmd);
#else
	void u_v_prev(volatile H264Reg_Mem *ptReg_mem, uint32_t value, int32_t len);
	void u_v(volatile H264Reg_Mem *ptReg_mem, uint32_t value, int32_t len);
	void se_v(volatile H264Reg_Mem *ptReg_mem, int32_t value);
	void ue_v(volatile H264Reg_Mem *ptReg_mem, uint32_t value);
	void rbsp_trailing_bits(volatile H264Reg_Mem *ptReg_mem);
	int paking_last_syntax(volatile H264Reg_Mem *ptReg_mem, volatile H264Reg_Cmd *ptReg_cmd);
#endif
	int close_bitstream(volatile H264Reg_Cmd *ptReg_cmd);
	void clear_bs_length(void *ptEncHandle, volatile H264Reg_Cmd *ptReg_cmd);

#endif

