#ifndef _VLC_H_
#define _VLC_H_

#include "portab.h"
#include "h264_reg.h"

/*
 * return the current read pointer in bit offset of a byte
 */
static __inline uint32_t bit_offset(volatile H264_reg_io *ptReg_io)
{
    return (ptReg_io->BITSHIFT & 0x07);
}

/*
 * similir to read_u, but did not shif bitstream
 */
static __inline uint32_t show_bits(volatile H264_reg_io *ptReg_io, uint32_t len)
{
	uint32_t val;
    val = (ptReg_io->BITREAD) >> (32 - len);
    //PERF_MSG("s:%08X\n", val);
	return val;
}

/*
 * shit bitstream len bits
 */
static __inline void flush_bits(volatile H264_reg_io *ptReg_io, uint8_t len)
{
	ptReg_io->BITSHIFT = (len - 1);
}


/*
 * shift bitstream to the next byte aligned offest
 */
static __inline void byte_align(volatile H264_reg_io *ptReg_io)
{
	uint32_t len;

	len = (ptReg_io->BITSHIFT & 0x07);

#if 0
    printk("byte_align len:%d\n", len);
    if(len){
        while(1);
    }
#endif
	
    if (len != 0)
		ptReg_io->BITSHIFT = (7 - len); /* flush_bits(8 - len), set BITSHIFT to (8 - len) - 1 */
/*
	if (len!=0)
		ptReg_io->BITSHIFT = (len-1);
*/
}


static __inline uint32_t read_ue(volatile H264_reg_io *ptReg_io)
{
	uint32_t val;
    val = ptReg_io->EGC_UE;
    //PERF_MSG("ue:%08X\n", val);
	return val;
}

static __inline int read_se(volatile H264_reg_io *ptReg_io)
{
	uint32_t val;
    val = ptReg_io->EGC_SE;
    //PERF_MSG("se:%08X\n", val);
	return val;
}

static __inline uint32_t read_u(volatile H264_reg_io *ptReg_io, uint8_t len)
{
	uint32_t val;

	val = ptReg_io->BITREAD >> (32 - len);
	ptReg_io->BITSHIFT = (len - 1);
    //PERF_MSG("u:%08X\n", val);
	return val;
}
#endif

