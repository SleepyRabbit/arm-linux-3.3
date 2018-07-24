#ifndef _MCP280_REG_H_
#define _MCP280_REG_H_

#include "portab.h"

#define MCP280_OFF_CMD  0x0080
#define MCP280_OFF_MEM  0x0100
#define MCP280_OFF_TABLE    0x200
#define MCP280_OFF_QUANT    0x500
#define MCP280_OFF_MCNR     0x800
#define MCP280_OFF_VCACHE   0x80000

typedef struct h264_reg_parm
{
	uint32_t PARM0; 	// @0x00
	uint32_t PARM1; 	// @0x04
	uint32_t PARM2; 	// @0x08
	uint32_t PARM3; 	// @0x0C
	uint32_t PARM4; 	// @0x10
	uint32_t PARM5; 	// @0x14
	uint32_t PARM6; 	// @0x18
	uint32_t PARM7; 	// @0x1C
	uint32_t PARM8; 	// @0x20
	uint32_t PARM9; 	// @0x24
	uint32_t PARM10;	// @0x28
	uint32_t PARM11;	// @0x2C
	uint32_t PARM12;	// @0x30
	uint32_t PARM13;	// @0x34
	uint32_t PARM14;	// @0x38
	uint32_t PARM15;	// @0x3C
	uint32_t PARM16;	// @0x40
	uint32_t PARM17;	// @0x44
	uint32_t PARM18;	// @0x48
	uint32_t PARM19;	// @0x4C
	uint32_t PARM20;	// @0x50
	uint32_t PARM21;    // @0x54
	uint32_t PARM22;    // @0x58
	uint32_t PARM23;    // @0x5C
	uint32_t PARM24;    // @0x60
	uint32_t PARM25;    // @0x64
	uint32_t PARM26;    // @0x68
	uint32_t PARM27;    // @0x6C
	uint32_t PARM28;    // @0x70
	uint32_t PARM29;    // @0x74
	uint32_t PARM30;    // @0x78
} H264Reg_Parm;

typedef struct h264_reg_cmd
{
	uint32_t CMD0;		// @0x80    software reset, decode go
	uint32_t CMD1;		// @0x84    header code
	uint32_t CMD2;		// @0x88    se, ue, u
	uint32_t RESERVED[13];
	uint32_t STS0;		// @0xC0    status
	uint32_t STS1;		// @0xC4    bitstream length
	uint32_t STS2;		// @0xC8    interrupt
	uint32_t STS3;		// @0xCC    frame cost
	uint32_t STS4;		// @0xD0    mb qp sum
	uint32_t STS5;		// @0xD4
	uint32_t STS6;		// @0xD8
	uint32_t STS7;		// @0xDC
	uint32_t STS8;		// @0xE0
	uint32_t STS9;      // @0xE4
	uint32_t STS10;     // @0xE8
	uint32_t STS11;     // @0xEC    MCNR SAD
    uint32_t RESERVED3; // @0xF0
	uint32_t RESERVED4; // @0xF4
	uint32_t DBG0;      // @0xF8
    uint32_t DBG1;      // @0xFC
} H264Reg_Cmd;

typedef struct h264_meminfo
{
	uint32_t DIDN_CUR_TOP;	// 0x100
	uint32_t DIDN_CUR_BTM;	// 0x104
	uint32_t ME_REF[6]; 	// 0x108~0x11C	 ref0_L, ref0_C, ref1_L, ref1_C, ref2_L, ref2_C

	uint32_t REC_LUMA;			// 0x120
	uint32_t REC_CHROMA;		// 0x124
	uint32_t DIDN_REF_TOP;		// 0x128
	uint32_t DIDN_REF_BTM;		// 0x12C
	uint32_t DIDN_RESULT_TOP;	// 0x130
	uint32_t DIDN_RESULT_BTM;	// 0x134

	uint32_t SYSINFO;		// 0x138
	uint32_t MOTION_INFO;	// 0x13C
	uint32_t L1_COL_MV;		// 0x140
	uint32_t BS_BUF_SIZE;	// 0x144
	uint32_t BS_ADDR;		// 0x148
	uint32_t ILF_LUMA;		// 0x14C
	uint32_t ILF_CHROMA;	// 0x150

	uint32_t RESERVED1[11];
	uint32_t SWAP_DMA;		// 0x180
	
	uint32_t RESERVED2[31];

	uint32_t DIDN_TABLE[16];	// @0x200~0x23F
	uint32_t RESERVED3[112];
	uint32_t SLICE_HDR[64];		// @0x400~0x4FF
} H264Reg_Mem;

typedef struct h264_reg_quant
{
	// quantization matrix
	uint32_t INTRA_8x8_Y[32];	// @0x500~0x57F
	uint32_t INTER_8x8_Y[32];	// @0x580~0x5FF
	uint32_t INTRA_4x4_Y[8];	// @0x600~0x61F
	uint32_t INTRA_4x4_Cb[8];	// @0x620~0x63F
	uint32_t INTRA_4x4_Cr[8];	// @0x640~0x65F
	uint32_t RESERVED[8];
	uint32_t INTER_4x4_Y[8];	// @0x680~0x69F
	uint32_t INTER_4x4_Cb[8];	// @0x6A0~0x6BF
	uint32_t INTER_4x4_Cr[8];	// @0x6C0~0x6DF

	uint32_t RESERVED1[8];
	uint32_t INV_INTRA_8x8_Y[16];	// @0x700~0x73F
	uint32_t INV_INTER_8x8_Y[16];	// @0x740~0x77F
	uint32_t INV_INTRA_4x4_Y[4];	// @0x780~0x78F
	uint32_t INV_INTRA_4x4_Cb[4];	// @0x790~0x79F
	uint32_t INV_INTRA_4x4_Cr[4];	// @0x7A0~0x7AF
	uint32_t RESERVED2[4];
	uint32_t INV_INTER_4x4_Y[4];	// @0x7C0~0x7CF
	uint32_t INV_INTER_4x4_Cb[4];	// @0x7D0~0x7DF
	uint32_t INV_INTER_4x4_Cr[4];	// @0x7E0~0x7EF
} H264Reg_Quant;

typedef struct H264_reg_mcnr
{
    uint32_t MCNR_LSAD[13]; // @ 0x800~0x833
    uint32_t RESERVED[3];
    uint32_t MCNR_HSAD[13]; // @ 0x840~0x873
} H264Reg_MCNR;

typedef struct H264_reg_vcache
{
    uint32_t V_ENC;         // @00
    uint32_t DECODE;        // @04
    uint32_t I_SEARCH_RANGE;// @08
    uint32_t O_SEARCH_RANGE;// @0C
    uint32_t CHN_CTL;       // @10
    uint32_t RESERVED14;    // @14
    uint32_t RESERVED18;    // @18
    uint32_t RESERVED1C;    // @1C
    uint32_t REF0_PARM;     // @20
    uint32_t REF1_PARM;     // @24
    uint32_t REF2_PARM;     // @28
    uint32_t REF3_PARM;     // @2C
    uint32_t LOAD_BASE;     // @30
    uint32_t LOAD_BASE2;    // @34
    uint32_t DIS_ILF_PARM;  // @38
    uint32_t LOAD_BASE3;    // @3C
    uint32_t SYS_INFO;      // @40
    uint32_t RESERVED44;    // @44
    uint32_t CUR_PARM;      // @48
} H264Reg_Vcache;

#define SHIFT(a, start, end)	(((a)&((1<<((end)-(start)+1))-1))<<(end))
#define SHIFT_1(a, end)	(((a)&0x01)<<(end))
#define CONCATE_4_8BITS_VALUES(a,b,c,d)	((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((c)&0xFF)<<8) | ((d)&0xFF))
#define CONCATE_2_16BITS_VALUES(a, b)	((((a)&0xFFFF)<<16) | ((b)&0xFFFF))

#endif

