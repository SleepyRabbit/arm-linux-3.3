#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include "params.h"
#include "refbuffer.h"
#include "slice_header.h"
#include "quant.h"
#include "H264V_enc.h"
#ifdef RTL_SIMULATION
	#include "simulator.h"
#endif

#ifdef LINUX
    //#define DUMP_FPGA_MSG
    // DUMP_REGISTER: bit0: parameter register, bit1: status, bit2: memory address
    //#define DUMP_REGISTER 0
#endif

//#define DUMP_MESSAGE
#ifdef RTL_SIMULATION
	#define showValue(value)	vpeSet(value)
	#define showPass()	vpePass()
	#define showFail()	vpeFail()
#elif defined(FPGA_NON_OS)
	#include "stdio.h"
	#define showValue(value)    printf("%08x\n", value);
	#define showPass()  printf("Pass\n");
	#define showFail()  printf("Fail\n");
#else
    #ifdef DUMP_MESSAGE
		#define showValue(value)	printk("%08x\n", value);
		#define showPass()	printk("Pass\n");
		#define showFail()	printk("Fail\n");
    #else
        #define showValue(value)
        #define showPass()
        #define showFail()
	#endif
#endif

#define DUMP_PARAM  0x01
#define DUMP_CMD    0x02
#define DUMP_MEM    0x04
#define DUMP_VCACHE 0x08
#define DUMP_QUANT_TABLE    0x10
#define DUMP_MCNR_TABLE     0x20
#define DUMP_ALL    0xFF
#define DUMP_VG     0x100

typedef struct video_parameters_t
{
	//SeqParameterSet sps;
	SeqParameterSet *active_sps;
	//PicParameterSet pps;
	PicParameterSet *active_pps;
	uint8_t  resend_spspps;	// 0: only first frmae, 1: all I frames, 2: all frames
	byte     write_spspps_flag;
	byte     field_pic_flag;		// set in preare sps
	//byte     new_frame;
	
	// chroma format
	uint32_t gop_idr_cnt;	// if cnt == 0, idr frame
	uint32_t gop_b_remain_cnt;	// if cnt == 0, I or P frame
	uint32_t gop_poc_base;
#ifdef ENABLE_FAST_FORWARD
    uint32_t gop_non_ref_p_cnt;
#endif
	uint32_t ref_l0_num;
	byte     last_ref_idc;
    byte     previous_idr_flag; // Tuba 20120216: add last idr to update b frame poc
    int      last_idr_poc;      // Tuba 20120216: add last idr to update b frame poc

	uint32_t encoded_frame_num;
	uint32_t total_mb_number;
	uint32_t sum_mb_qp;
#ifdef STRICT_IDR_IDX
    uint32_t idr_cnt;
#endif

	StorablePicture *dec_picture;

	// image information
	uint32_t frame_num;
	//uint32_t prev_frame_num;
	//PictureStructure  structure;
	uint32_t frame_width;	// encode width
	uint32_t frame_height;	// encode height
	uint32_t source_height;
	uint32_t source_width;
	uint32_t mb_row;
	uint32_t slice_number;
    uint32_t cur_slice_idx;
    uint32_t slice_offset[MAX_SLICE_NUM];
	SliceHeader *currSlice;
	uint32_t slice_line_number;

	uint32_t  idr_period;
	uint32_t  number_Bframe;
#ifdef ENABLE_FAST_FORWARD
    uint32_t  fast_forward;
#endif
	//ASSIGN_FRAME_TYPE assign_frame_type;
    int force_intra;
    int force_slice_type;
	//uint8_t  encode_intra;

	// reference list
	DecodedPictureBuffer *p_Dpb;
	StorablePicture *ref_list[2][MAX_LIST_SIZE];
	uint8_t  list_size[2];

	// release buffer
	uint8_t  release_buffer_idx;
	//uint32_t release_buffer_idx[MAX_DPB_SIZE];
	//uint32_t release_num;

	// ROI
	byte     roi_flag;
	uint32_t roi_x;		// if all zero => roi = frame
	uint32_t roi_y;
	uint32_t roi_width;
	uint32_t roi_height;

	// search range
	uint32_t vt_sr[3][MAX_NUM_REF_FRAMES];
	uint32_t hz_sr[3][MAX_NUM_REF_FRAMES];
//	uint32_t vertB_sr;
//	uint32_t horzB_sr;
	uint8_t  max_hzsr_idx[MAX_SLICE_TYPE];
	uint8_t  max_vtsr_idx[MAX_SLICE_TYPE];
	
	// qp
	int8_t   currQP;
	uint8_t  delta_qp_strength;
	int32_t  delta_qp_thd;
	uint8_t  max_delta_qp;
	uint8_t  initial_qp;
//#ifdef FIXED_QP
	int8_t   fixed_qp[3];
//#endif
	int8_t   rc_qpy;

	// intra/inter mode
	byte     intra_4x4_mode_number;	// 0: 5 prediction modes, 1: 9 prediction modes
	byte     disable_coef_threshold;	// 0: enable, 1: disable
	byte     ori_dis_coef_thd;      // Tuba 20140714: backup when wateramrk enable
	uint8_t  luma_coef_thd;
	uint8_t  chroma_coef_thd;
	uint32_t fme_cyc_thd[2];
	uint32_t fme_cost_thd;
	uint8_t  inter_default_trans_size;	// Inter hadamard transform size. 0: 4x4, 1: 8x8
	uint8_t  inter_pred_mode_dis[3];
		// bit 0: 8x8, bit 1: 8x16, bit 2: 16x8, bit 3: 16x16 (1: enable, 0: disable)
    // mb qp update
	uint8_t  mb_qp_weight;
#ifdef ENABLE_MB_RC
    uint8_t  mb_bak_weight;
#endif
	uint32_t rc_init2;
	uint32_t rc_basicunit;	// should be multiple of row mb. 0: not use
	int32_t  bitbudget_frame;
	int32_t  bitbudget_bu;
	int32_t  basicunit_num;
	uint32_t rc_qp_step;
	uint32_t ave_frame_qp;
	uint32_t rc_max_qp[3];
	uint32_t rc_min_qp[3];
	uint32_t bit_rate;
	uint32_t slice_bit_rate[3];
	//uint32_t frame_rate;	
	uint32_t num_unit_in_tick;
	uint32_t time_unit;
	
	// didn
	uint32_t didn_mode;
	//byte     itl_mode;
	SOURCE_MODE src_mode;
	uint8_t  gamma_level;
	byte     src_img_type;	// 0: top/btm separate, 1: interleave
	byte     didn_param_enable;
	FAVC_ENC_DIDN_PARAM didn_param;
	uint8_t  dn_result_format;
	byte     sad_source;
	
	// watermark
	byte     wk_enable;
    uint32_t wk_init_pattern;
	uint32_t wk_pattern;
    uint32_t wk_reinit_period;
    uint32_t wk_frame_number;

	ReconBuffer *curr_rec_buffer;

	// quant
	QuantParam mQuant;
	// scaling list 
	byte     no_quant_matrix_flag;
	
	// cabac 
	uint8_t  cabac_init_idc;
	uint32_t bitlength; // bitstream length beside start code (CABAC count bit number)
	uint32_t bincount;  // from STS9
	
	uint32_t total_cost;
#ifdef RC_ENABLE
    uint32_t last_cost;
    uint8_t  slice_type;    // for rate control (remember original slice type)
#endif
	//uint32_t bitstream_length;

	byte     intra_8x8_disable;
	byte     fast_intra_4x4;
	byte     intra_16x16_plane_disbale;
	
	byte     p_ref1_field_temporal;
	int32_t  delta_mb_qp_sum;
	int32_t  delta_mb_qp_sum_clamp;
	uint32_t nonz_delta_qp_cnt;
	//uint8_t  original_delta_qp_strength;
	
    byte     disable_intra_in_pb;
    uint8_t  disable_intra_in_i;    // 0x00, 0x03: intra4x4 and intra16x16 both enable
                                    // 0x01: intra16x16 enable, intra4x4 disable
                                    // 0x02: intra16x16 disbale, intra4x4 enable
    uint8_t  intra_cost_ratio;      // intra_cost*(1+intra_cost_ratio/32)
    uint32_t force_mv0_thd;         // when vaiance lower than thd => force mv0
                                    
	byte     int_top_btm;
	byte     vcache_enable;
    byte     vcache_ref2_enable;    // Tuba 20140211: add vcache ref2 enable checker to turn on/off out of order
	
	uint8_t  roi_qp_type;   // 0: disable, 1: delta qp , 2: fixed qp
	int8_t   roi_qp;
	FAVC_ROI_QP_REGION roi_qp_region[ROI_REGION_NUM];
#ifdef OVERFLOW_REENCODE
    uint32_t record_sts0;
#endif
    // mcnr
    byte     mcnr_en;
    byte     ori_mcnr_en;   // Tuba 20140714: backup ehwn watermark enable
    uint8_t  mcnr_sad_shift;
    uint16_t mcnr_mv_thd;
    uint16_t mcnr_var_thd;
    uint32_t mcnr_sad;
} VideoParams;

typedef struct encoder_parameter_t
{
	int enc_dev;
	VideoParams *p_Vid;
	uint32_t u32BaseAddr;
    uint32_t u32VcacheAddr;
	uint32_t u32CurrIRQ;
    //uint32_t u32EncStatus;
	
	MALLOC_PTR_enc pfnMalloc;
	FREE_PTR_enc   pfnFree;

    //uint8_t  u8CurrState;
    //uint8_t  *pu8BSBuffer_virt[2];
    //uint8_t  *pu8BSBuffer_phy[2];
	//uint32_t u32MaxBSLength[2];
#if 0
	uint8_t  *pu8BSBuffer_virt;
    uint8_t  *pu8BSBuffer_phy;
#endif
	uint32_t u32MaxBSLength;
	//uint32_t u8CurrBSBufIdx;
	//uint32_t u8OutBSBufIdx;
	//uint32_t u32OutputedBSLength;
	//uint32_t u32CostBSLength;
	uint32_t u32HeaderBSLength;
	uint32_t u32ResBSLength;
    uint32_t u32TopBSLength;
#ifdef AES_ENCRYPT
    uint32_t u32SEIStart;
#endif

	ReconBuffer  *m_CurrRecBuffer;
	SourceBuffer *m_CurrSrcBuffer;
	uint32_t u32PrevSrcBuffer_phy;
	//SourceBuffer *m_PrevSrcBuffer;
#if ENABLE_TEMPORL_DIDN_WITH_B
	SourceBuffer *m_DidnSrcBuffer;
#endif

#ifndef ADDRESS_UINT32
	uint8_t  *pu8SysInfoAddr_virt;          // 12_words*mb_width (12*4*mb_width = 3*frame_width)
	uint8_t  *pu8SysInfoAddr_phy;
	uint8_t  *pu8MVInfoAddr_virt[2];        // 2_words*mb_number (array for field coding)
	uint8_t  *pu8MVInfoAddr_phy[2];
	uint8_t  *pu8L1ColMVInfoAddr_virt[2];   // 4_words*mb_number
	uint8_t  *pu8L1ColMVInfoAddr_phy[2];
	uint8_t  *pu8DiDnRef0_virt;
	uint8_t  *pu8DiDnRef0_phy;
	uint8_t  *pu8DiDnRef1_virt;
	uint8_t  *pu8DiDnRef1_phy;
#else
    uint32_t u32SysInfoAddr_virt;           // 12_words*mb_width (12*4*mb_width = 3*frame_width)
	uint32_t u32SysInfoAddr_phy;
	uint32_t u32MVInfoAddr_virt[2];         // 2_words*mb_number (array for field coding)
	uint32_t u32MVInfoAddr_phy[2];
	uint32_t u32L1ColMVInfoAddr_virt[2];    // 4_words*mb_number
	uint32_t u32L1ColMVInfoAddr_phy[2];
	uint32_t u32DiDnRef0_virt;
	uint32_t u32DiDnRef0_phy;
	uint32_t u32DiDnRef1_virt;
	uint32_t u32DiDnRef1_phy;
#endif

    uint32_t u32BSBuffer_virt;
    uint32_t u32BSBuffer_phy;
	uint32_t u32BSBufferSize;
} EncoderParams;

void dump_all_register(EncoderParams *p_Enc, unsigned int flag);
void reset_bs_param(EncoderParams *p_Enc, int bs_length);
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	int init_register(EncoderParams *p_Enc, int sw_reset_en);
#else
	int init_register(EncoderParams *p_Enc);
#endif

#endif
