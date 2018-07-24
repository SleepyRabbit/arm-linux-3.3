#ifndef _H264_REG_H_
#define _H264_REG_H_

#include "portab.h"
#include "define.h"
#include "favc_dec_ll.h"

#define TLU_BS_OFFSET 0x30000 /* 256-btytes SRAM for storing current bitstream */
#define TLU_OFFSET    0x30400
//#define H264_OFF_WTABLE 0xE00

#define H264_OFF_SYSM 0x10c00
#define H264_OFF_POCM 0x20000
#define H264_OFF_DSFM 0x20280
//#define H264_OFF_CTXM 0x20400
#define H264_OFF_SFMM 0x40000

//#define H264_OFF_SYSM_LEFTTOP_MB    0x600
//#define H264_OFF_SYSM_LEFTBOT_MB    0x700
//#define H264_OFF_SYSM_LEFT_MB       0x700
#define H264_OFF_SYSM_TOP_MB        0x800
//#define H264_OFF_SYSM_TOP_FIELDFLAG 0x880
//#define H264_OFF_SYSM_TOPLEFT_FIELDFLAG 0xB80

typedef struct H264Reg_io
{
	uint32_t BITCTL;        // @00
	uint32_t TLUOPER;       // @04
	uint32_t BITREAD;       // @08
	uint32_t BITSHIFT;      // @0C
	uint32_t BITSYSBUFCNT;  // @10
	uint32_t BITSYSBUFADR;  // @14
	uint32_t CPUCMD0;       // @18
	uint32_t CPUCMD1;       // @1C

	uint32_t DEC_PARAM0;    // @20
	uint32_t DEC_PARAM1;    // @24
	uint32_t DEC_PARAM2;    // @28
	uint32_t DEC_PARAM3;    // @2C
	uint32_t DEC_PARAM4;    // @30
	uint32_t DEC_PARAM5;    // @34
	uint32_t DEC_PARAM6;    // @38
	uint32_t DEC_PARAM7;    // @3C
	uint32_t DEC_CMD0;      // @40
	uint32_t DEC_CMD1;      // @44
	uint32_t DEC_CMD2;      // @48
	uint32_t DEC_CMD3;      // @4C
	uint32_t DEC_STS0;      // @50
	uint32_t DEC_INTSTS;    // @54

	uint32_t RESERVED0;     // @58
	uint32_t RESERVED1;     // @5C
	uint32_t EGC_UE;        // @60 NOTE: read this register will shift HW's internal bitstream read pointer. Therefore, dump this register at decoding may cause error.
	uint32_t EGC_SE;        // @64 NOTE: read this register will shift HW's internal bitstream read pointer. Therefore, dump this register at decoding may cause error.
	uint32_t RESERVED2;     // @68 NOTE: read this register will shift HW's internal bitstream read pointer. Therefore, dump this register at decoding may cause error.
	uint32_t RESERVED3;     // @6C
	uint32_t RESERVED4;     // @70
	uint32_t RESERVED5;     // @74
	uint32_t DEC_PARM30;    // @78
	uint32_t DEC_PARM31;    // @7C
}H264_reg_io;

#if 0
typedef struct H264Reg_ctx{
	uint32_t MB_SKIP;			// @00
	uint32_t FIELD_FLAG;		// @01
	uint32_t MB_TYPE[2];		// @02,03
	uint32_t SUB_MB_TYPE;		// @04
	uint32_t TRANSFORM_SIZE_8;	// @05
	uint32_t CBP_LUMA;			// @06
	uint32_t CBP_CHROMA[2];		// @07,08
	uint32_t MB_QP_DELTA;		// @09
	uint32_t INTRA_4x4;			// @10
	uint32_t INTRA_CHROMA;		// @11

	uint32_t RESERVED1[4];

	uint32_t SIGN_COEF1[4];		// @16,17,18,19
	uint32_t LAST_SIGN_COEF1[4];// @20,21,22,23
	uint32_t SIGN_COEF2[4];		// @24,25,26,27
	uint32_t LAST_SIGN_COEF2[4];// @28,29,30,31
	uint32_t SIGN_COEF3[4];		// @32,33,34,35
	uint32_t LAST_SIGN_COEF3[4];// @36,37,38,39
	uint32_t SIGN_COEF4;		// @40
	uint32_t RESERVED2[3];
	uint32_t LAST_SIGN_COEF4;	// @44
	uint32_t RESERVED3[3];
	uint32_t SIGN_COEF5[4];		// @48,49,50,51
	uint32_t LAST_SIGN_COEF5[4];// @52,53,54,55
	uint32_t SIGN_COEF6[4];		// @56,57,58,59
	uint32_t LAST_SIGN_COEF6[3];// @60,61,62

	uint32_t RESERVED4;

	uint32_t REF_IDX[2];		// @64,65
	uint32_t MVD_X[2];			// @66,67
	uint32_t MVD_Y[2];			// @68,69

	uint32_t RESERVED5[10];

	uint32_t FIELD_SIGN_COEF1[4];		// @80,81,82,83
	uint32_t FIELD_LAST_SIGN_COEF1[4];	// @84,85,86,87
	uint32_t FIELD_SIGN_COEF2[4];		// @88,89,90,91
	uint32_t FIELD_LAST_SIGN_COEF2[4];	// @92,93,94,95
	uint32_t FIELD_SIGN_COEF3[4];		// @96,97,98,99
	uint32_t FIELD_LAST_SIGN_COEF3[4];	// @100,101,102,103
	uint32_t FIELD_SIGN_COEF4;			// @104
	uint32_t RESERVED6[3];
	uint32_t FIELD_LAST_SIGN_COEF4;		// @108
	uint32_t RESERVED7[3];
	uint32_t FIELD_SIGN_COEF5[4];		// @112,113,114,115
	uint32_t FIELD_LAST_SIGN_COEF5[4];	// @116,117,118,119
	uint32_t FIELD_SIGN_COEF6[4];		// @120,121,122,123
	uint32_t FIELD_LAST_SIGN_COEF6[3];	// @124,125,126

	uint32_t RESERVED8[9];

	uint32_t CODED_BLOCK1;		// @136
	uint32_t COEF_ABS_LEVEL1[3];// @137,138,139
	uint32_t CODED_BLOCK2;		// @140
	uint32_t COEF_ABS_LEVEL2[3];// @141,142,143
	uint32_t CODED_BLOCK3;		// @144
	uint32_t COEF_ABS_LEVEL3[3];// @145,146,147
	uint32_t CODED_BLOCK4;		// @148
	uint32_t COEF_ABS_LEVEL4[3];// @149,150,151
	uint32_t CODED_BLOCK5;		// @152
	uint32_t COEF_ABS_LEVEL5[3];// @153,154,155
	uint32_t RESERVED9;
	uint32_t COEF_ABS_LEVEL6[3];// @157,158,159
}H264_reg_ctx;
#endif


typedef struct H264Reg_scale{
	uint32_t INTRA4_Y[4];
	uint32_t INTRA4_CB[4];
	uint32_t INTRA4_CR[4];
	uint32_t INTER4_Y[4];
	uint32_t INTER4_CB[4];
	uint32_t INTER4_CR[4];
	uint32_t INTRA8[16];
	uint32_t INTER8[16];
}H264_reg_scale;

typedef struct H264Reg_refpoc{
	uint32_t REFLIST0[32];
	uint32_t REFLIST1[32];
}H264_reg_refpoc;


typedef struct H264Reg_sys
{
    /* DMA write channel addresses */
	uint32_t CUR_FRAME_BASE_W;	// @00
	uint32_t CUR_MB_INFO_BASE1;	// @04
	uint32_t CUR_INTRA_BASE1;	// @08
	uint32_t RESERVED1[2];		// @0C~10
	uint32_t SCALE_BASE;        // @0x14
	uint32_t RESERVED2;         // @0x18
	uint32_t REG1C;				// @1C

    /* DMA read channel addresses */
	uint32_t CUR_FRAME_BASE_R;	// @20
	uint32_t CUR_INTRA_BASE2;	// @24
	uint32_t CUR_MB_INFO_BASE2;	// @28
	uint32_t COL_REFPIC_MB_INFO;// @2C
	uint32_t RESERVED3[12];		// @30~5C
	
	uint32_t REG60;				// @60
	uint32_t RESERVED4[39];		// @64~FC
	uint32_t REFLIST0[32];		// @100~179
	uint32_t REFLIST1[32];		// @180~1FF
	uint32_t WEIGHTL0[64];		// @200~300
	uint32_t WEIGHTL1[64];		// @300~400
}H264_reg_sys;


#if CONFIG_ENABLE_VCACHE
typedef struct H264Reg_vcache_t
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
    uint32_t LOCAL_BASE;    // @30
    uint32_t RESERVED34;    // @34
    uint32_t DIS_ILF_PARM;  // @38
    uint32_t RESERVED3C;    // @3C
    uint32_t SYS_INFO;      // @40
    uint32_t RESERVED44;    // @44
    uint32_t CUR_PARM;      // @48
} H264_reg_vcache;
#endif

typedef struct H264Reg_dsf{
	uint32_t DSF_L0[32];    // @0x20280
	uint32_t DSF_L3[32];    // @0x20300
	uint32_t DSF_L5[32];    // @0x20380
}H264_reg_dsf;

#if 0
	// long_start_code[22](nalu),                 seq_scaling_matrix_present_flag[21](sps)
	// chroma_format_idc[20-19](sps),             frame_mbs_only_flag[18](sps)
	// mb_adaptive_frame_field_flag[17](sps),     direct_8x8_inference_flag[16](sps)
	// pic_height_in_map_units_minus1[15-8](sps), pic_width_in_mbs_minus1[7-0](sps)
#define DECODER_PARAM0(a,b,c,d,e,f,g,h) ((((uint32_t)(a)&0x01)<<22) | (((uint32_t)(b)&0x01)<<21) | \
	(((uint32_t)(c)&0x03)<<19) | (((uint32_t)(d)&0x01)<<18) | \
	(((uint32_t)(e)&0x01)<<17) | (((uint32_t)(f)&0x01)<<16) | \
	(((uint32_t)(g)&0xFF)<<8) | ((uint32_t)(h)&0xFF))
	
	// weighted_bipred_idc[15-14](pps),          constrained_intra_pred_flag[13](pps)
	// pic_scaling_matrix_present_flag[12](pps), second_chroma_qp_index_offset[11-7](pps)
	// chroma_qp_index_offset[6-2](pps),         entropy_coding_mode_flag[1](pps)
	// transform_8x8_mode_flag[0](pps)
#define DECODER_PARAM1(a,b,c,d,e,f,g) ((((uint32_t)(a)&0x03)<<14) | (((uint32_t)(b)&0x01)<<13) | \
	(((uint32_t)(c)&0x01)<<12) | (((uint32_t)(d)&0x1F)<<7) | \
	(((uint32_t)(e)&0x1F)<<2) | (((uint32_t)(f)&0x01)<<1) | \
	((uint32_t)(g)&0x01))

	// qpv[17:12], qpu[11:6], qpy[5:0]
#define DECODER_PARAM3(a,b,c) ((((uint32_t)(a)&0x3F)<<12) | (((uint32_t)(b)&0x3F)<<6) | ((uint32_t)(c)&0x3F))

	// left_mb_filed_flag[31], top/bottom field first flag(0 : top first 1 : bottom first)[30]
	// new_frame[29], 0b01[29-28], num_ref_idx_l1_active_minus1_slice[26-22], 
	// num_ref_idx_l0_active_minus1_slice[21-17], disable_deblocking_filter_idc[16-15], 
	// direct_spatial_mv_pred_flag[14], bottom_field_flag[13], first_ref_pic_l1_top[12], 
	// col_pic_is_long[11], col_intra_flag[10], col_field_pic_flag[9], field_pic_flag[8], 
	// slice_qp[7-2], slice_type[1-0]
#define DECODER_PARAM4(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o) ((((uint32_t)(a)&0x01)<<31) | (((uint32_t)(b)&0x01)<<30) | \
	(((uint32_t)(c)&0x01)<<29) | (0x01<<27) | (((uint32_t)(d)&0x1F)<<22) | (((uint32_t)(e)&0x1F)<<17) | \
	(((uint32_t)(f)&0x03)<<15) | (((uint32_t)(g)&0x01)<<14) | (((uint32_t)(h)&0x01)<<13) | \
	(((uint32_t)(i)&0x01)<<12) | (((uint32_t)(j)&0x01)<<11) | (((uint32_t)(k)&0x01)<<10) | \
	(((uint32_t)(l)&0x01)<<9) | (((uint32_t)(m)&0x01)<<8) | (((uint32_t)(n)&0x3F)<<2) | ((uint32_t)(o)&0x03))
#endif

	// total_mb_cnt[31:16], first_mb_in_slice[15:0](slice currMbAddr)
#define DECODER_PARAM5(a,b) ((((uint32_t)(a)&0xFFFF)<<16) | ((uint32_t)(b)&0xFFFF))
#if 0
	// col_bot_first[31], mc_Clog2wd[30-28], mc_Log2wd[27-25}, slice_beta_offset_div2[24-21], 
	// slice_alpha_c0_offset_div2[20-17], cpu_mby[16-9], cpu_mbx[8-0]
#define DECODER_PARAM6(a,b,c,d,e,f,g) ((((uint32_t)(a)&0x01)<<31) | (((uint32_t)(b)&0x07)<<28) | \
	(((uint32_t)(c)&0x07)<<25) | (((uint32_t)(d)&0x0F)<<21) | (((uint32_t)(e)&0x0F)<<17) | \
	(((uint32_t)(f)&0xFF)<<9) | ((uint32_t)(g)&0x01FF))

	// idx1[22:20], cpu_sel[19], idx0[18-16], left_mb_cbp1[15-12], left_mb_cbp0[11-8], 
	// left_mb_intra1[7], left_mb_intra0[6], mb_topfieldflag[5:4], en_gate_clk[0]
#define DECODER_PARAM7(a,b,c,d,e,f,g,h,i) ((((uint32_t)(a)&0x07)<<20) | (((uint32_t)(b)&0x01)<<19) | \
	(((uint32_t)(c)&0x07)<<16) | (((uint32_t)(d)&0x0F)<<12) | (((uint32_t)(e)&0x0F)<<8 | \
	(((uint32_t)(f)&0x01)<<7) | (((uint32_t)(g)&0x01)<<6) | (((uint32_t)(h)&0x03)<<4) | \
	((uint32_t)(i)&0x01))

	//frame_num[31:16], cur_poc[16:0]
#define DECODER_PARAM30(a,b) ((((uint32_t)(a)&0xFFFF)<<16) | ((uint32_t)(b)&0xFFFF))

	// cur_bot_poc[31:16], col_index[4:0]
#define DECODER_PARAM31(a,b) ((((uint32_t)(a)&0xFFFF)<<16) | ((uint32_t)(b)&0x0F))
#endif

#define CONCAT_2_16BITS_VALUES(a,b)    ((((a)&0xFFFF)<<16) | ((b)&0xFFFF))
#define CONCAT_4_8BITS_VALUES(a,b,c,d) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((c)&0xFF)<<8) | ((d)&0xFF))
#define CONCAT_4_7BITS_VALUES(a,b,c,d) ((((a)&0x7F)<<21) | (((b)&0x7F)<<14) | (((c)&0x7F)<<7) | ((d)&0x7F))
#define CONCAT_4_1BIT_VALUES(a,b,c,d)  ((((a)&0x01)<<3) | (((b)&0x01)<<2) | (((c)&0x01)<<1) | ((d)&0x01))

#define NBIT_MASK(n) ((1 << n) - 1)

#define SHIFT(val, bit_size, base)   (((val)&NBIT_MASK(bit_size))<<(base))
#define SHIFT_1(val, base)           (((val)&0x01)<<(base))


#if USE_LINK_LIST

#if SW_LINK_LIST == 1
    #error SW_LINK_LIST == 1 is removed
#endif

/* SW_LINK_LIST == 0 or 2 */

#define SHIFT_LOW(ll_reg) ((ll_reg) >> 2)

#define WRITE_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "%08X=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, single_write_cmd(SHIFT_LOW(ll_reg), v));  \
    }while(0)

#define WRITE_REG_SYNC(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);        \
        PERF_MSG_N(REG_DUMP, "%08X=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, single_write_cmd(SHIFT_LOW(ll_reg), v)); \
    }while(0)

#define ORR_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "%08X|=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_or_cmd(SHIFT_LOW(ll_reg), v));  \
    }while(0)
    
#define ORR_REG_SYNC(dec, ll_reg, reg, val) do{ \
        volatile unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "%08X|=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_or_cmd(SHIFT_LOW(ll_reg), v));  \
    }while(0)

#define AND_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "%08X&=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_and_cmd(SHIFT_LOW(ll_reg), v));  \
    }while(0)

#define AND_REG_SYNC(dec, ll_reg, reg, val) do{ \
        volatile unsigned int v = (val);        \
        PERF_MSG_N(REG_DUMP, "%08X&=%08X #"#reg"="#val"\n", (unsigned int)&(reg), v); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_and_cmd(SHIFT_LOW(ll_reg), v));  \
    }while(0)

#define SUB_REG(dec, ll_reg, reg, mask_bits, mask_shift, sub) do{ \
        unsigned int v = (sub);                                   \
        unsigned int zero_mask = N_BIT_MASK(mask_bits) << mask_shift;             \
        PERF_MSG_N(REG_DUMP, "SUB REG %08X & %08X - %d #"#reg"="#sub" offset:%08X\n", get_reg_offset(ll_reg), zero_mask, v, (unsigned int)&(reg)); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_sub_cmd(SHIFT_LOW(ll_reg), zero_mask, v));  \
    }while(0)

#define SUB_REG_SYNC(dec, ll_reg, reg, mask_bits, mask_shift, sub) do{ \
        volatile unsigned int v = (sub);                               \
        unsigned int zero_mask = N_BIT_MASK(mask_bits) << mask_shift;  \
        PERF_MSG_N(REG_DUMP, "SUB REG %08X & %08X - %d #"#reg"="#sub" offset:%08X\n", get_reg_offset(ll_reg), zero_mask, v, (unsigned int)&(reg)); \
        ll_add_cmd(dec->codec_ll->curr_job, bitwise_sub_cmd(SHIFT_LOW(ll_reg), zero_mask, v));  \
    }while(0)
    
#define POLL_REG_ONE(dec, ll_reg, reg, zero_mask, err_st) do{ \
        PERF_MSG_N(REG_DUMP, "POLL %08X BIT %08X one#"#reg"="#zero_mask"\n", (unsigned int)&(reg), zero_mask);      \
        ll_add_cmd(dec->codec_ll->curr_job, poll_one_cmd(SHIFT_LOW(ll_reg), zero_mask, "poll one:"#reg"="#zero_mask));  \
    }while(0)

#define POLL_REG_ZERO(dec, ll_reg, reg, one_mask, err_st) do{ \
        PERF_MSG_N(REG_DUMP, "POLL %08X BIT %08X zero#"#reg"="#one_mask"\n", (unsigned int)&(reg), one_mask);       \
        ll_add_cmd(dec->codec_ll->curr_job, poll_zero_cmd(SHIFT_LOW(ll_reg), one_mask, "poll zero:"#reg"="#one_mask));  \
    }while(0)

#if SW_LINK_LIST == 0
#define SW_LL_FUNC_PARAM2(dec, func, param1, param2) do{ \
        printk("ll error: %s %s line:%d\n", __FILE__, __FUNCTION__, __LINE__);   \
        while(1);               \
    }while(0)
#else /* SW_LINK_LIST == 2 */
#define SW_LL_FUNC_PARAM2(dec, func, param1, param2) do{ \
        /*printk("ll func: %s %s line:%d\n", __FILE__, __FUNCTION__, __LINE__);*/ \
        ll_add_cmd(dec->codec_ll->curr_job, sw_func_cmd1((void *)func, (unsigned int)param1));  \
        ll_add_cmd(dec->codec_ll->curr_job, sw_func_cmd2((void *)func, (unsigned int)param2));  \
    }while(0)
#endif /* SW_LINK_LIST == 2 */

#else /* USE_LINK_LIST == 0 */

#define WRITE_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);              \
        PERF_MSG_N(REG_DUMP, "REG %08X=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) = v; \
    }while(0)

#define WRITE_REG_SYNC(dec, ll_reg, reg, val) do{ \
        volatile unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "REG %08X=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) = v; \
        v = (reg); /* read back to make the last write take effect */ \
    }while(0)

#define ORR_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "REG %08X|=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) |= v; \
    }while(0)

#define ORR_REG_SYNC(dec, ll_reg, reg, val) do{ \
        volatile unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "REG %08X|=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) |= v; \
        v = (reg); /* read back to make the last write take effect */ \
    }while(0)

#define AND_REG(dec, ll_reg, reg, val) do{ \
        unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "REG %08X&=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) &= v; \
    }while(0)

#define AND_REG_SYNC(dec, ll_reg, reg, val) do{ \
        volatile unsigned int v = (val);     \
        PERF_MSG_N(REG_DUMP, "REG %08X&=%08X #"#reg"="#val" offset:%08X\n", get_reg_offset(ll_reg), (unsigned int)v, (unsigned int)&(reg)); \
        (reg) &= v; \
        v = (reg); /* read back to make the last write take effect */ \
    }while(0)


#define SUB_REG(dec, ll_reg, reg, mask_bits, mask_shift, sub) do{ \
        unsigned int v = (sub);                                  \
        unsigned int org_reg = (reg);                            \
        unsigned int zero_mask = N_BIT_MASK(mask_bits) << mask_shift;             \
        /*unsigned int tmp0 = ((org_reg >> mask_shift) & N_BIT_MASK(mask_bits)) - v; */\
        unsigned int tmp0 = (org_reg - (v << mask_shift)) & zero_mask; \
        unsigned int tmp1 = (org_reg & zero_mask);                                 \
        PERF_MSG_N(REG_DUMP, "REG %08X & %08X - %d #"#reg"="#sub" offset:%08X\n", get_reg_offset(ll_reg), zero_mask, v, (unsigned int)&(reg)); \
        (reg) = tmp0 | tmp1;                          \
    }while(0)

#define SUB_REG_SYNC(dec, ll_reg, reg, mask_bits, mask_shift, sub) do{ \
        volatile unsigned int v = (sub);                              \
        unsigned int org_reg = (reg);                                 \
        unsigned int zero_mask = N_BIT_MASK(mask_bits) << mask_shift;             \
        /*unsigned int tmp0 = ((org_reg >> mask_shift) & N_BIT_MASK(mask_bits)) - v; */\
        unsigned int tmp0 = (org_reg - (v << mask_shift)) & zero_mask; \
        unsigned int tmp1 = (org_reg & zero_mask);                                 \
        PERF_MSG_N(REG_DUMP, "REG %08X & %08X - %d #"#reg"="#sub" offset:%08X\n", get_reg_offset(ll_reg), zero_mask, v, (unsigned int)&(reg)); \
        (reg) = tmp0 | tmp1;                          \
        v = (reg); /* read back to make the last write take effect */ \
    }while(0)

#define POLL_REG_ONE(dec, ll_reg, reg, zero_mask, err_st) do{ \
        unsigned int v = (zero_mask);    \
        unsigned int cnt = 0;            \
        PERF_MSG_N(REG_DUMP, "REG POLL %08X BIT %08X one#"#reg"="#zero_mask" offset:%08X\n", get_reg_offset(ll_reg), v, (unsigned int)&(reg));  \
        while((reg & v) != v && cnt < POLLING_ITERATION_TIMEOUT){ cnt++;}     \
        if((reg & v) != v) { err_st; }   \
    }while(0)

#define POLL_REG_ZERO(dec, ll_reg, reg, one_mask, err_st) do{ \
        unsigned int v = (one_mask);     \
        unsigned int cnt = 0;            \
        PERF_MSG_N(REG_DUMP, "REG POLL %08X BIT %08X zero#"#reg"="#one_mask" offset:%08X\n", get_reg_offset(ll_reg), v, (unsigned int)&(reg));  \
        while((reg & ~v) != 0 && cnt < POLLING_ITERATION_TIMEOUT){ cnt++; }         \
        if((reg & ~v) != 0) { err_st; }          \
        /*if((~(reg | v)) != 0) { err_st; }*/          \
    }while(0)

#define SW_LL_FUNC_PARAM2(dec, func, param1, param2)  do{ \
            func(param1, param2);                             \
        }while(0)

#endif /* USE_LINK_LIST == 0 */

#endif /* _H264_REG_H_ */

