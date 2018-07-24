#ifndef _GLOBAL_H_
#define _GLOBAL_H_
#include "params.h"
#include "nalu.h"
#include "slicehdr.h"
#include "refbuffer.h"
#include "define.h"
#include "../h264d_ioctl.h"
#include "H264V_dec.h"
#include "bitstream.h"
#include "favc_dec_ll.h"


//! Buffer structure for decoded reference picture marking commands
struct DecRefPicMarking_s
{
	uint32_t difference_of_pic_nums_minus1;
	uint32_t long_term_pic_num;
	uint32_t long_term_frame_idx;
	uint8_t  memory_management_control_operation;
	uint8_t  max_long_term_frame_idx_plus1;
};


struct video_parameter_t
{
    // start of reserved fields at __reset_video_parameter()
    DecoderParams        *dec_handle;
    SeqParameterSet      *SPS[MAX_SPS_NUM];
    PicParameterSet      *PPS[MAX_PPS_NUM];
    SliceHeader          *slice_hdr;
    DecodedPictureBuffer *listD; /* DPB */
    StorablePicture      *dec_pic;
    StorablePicture      *gap_pic; /* a picture buffer used in fill_frame_num_gap() */
    // end of reserved fields at __reset_video_parameter()

    NALUnit               nalu; // first non-reserved field. NOTE: do not change its order (see __reset_video_parameter() )
    uint8_t               first_idr_found; /* for checking if the first coded slice is IDR slice */
    uint8_t               skip_till_idr; /* for error handling: skip til the next IDR slice */
    SeqParameterSet      *active_sps;
    PicParameterSet      *active_pps;


    int8_t               *ScalingList[8];
    /* NOTE: scaling list size is 8 if sps->chroma_format_idc != 3 (YUV444) */
    // 0: Sl_4x4_Intra_Y    1: Sl_4x4_Intra_Cb    2: Sl_4x4_Intra_Cr
    // 3: Sl_4x4_Inter_Y    4: Sl_4x4_Inter_Cb    5: Sl_4x4_Inter_Cr
    // 6: Sl_8x8_Intra_Y    7: Sl_8x8_Inter_Y

    // reference frame
    StorablePicture *refListLX[6][MAX_LIST_SIZE];
    uint8_t          list_size[6];
    StorablePicture *col_pic;
    
    int              dist_scale_factor[6][MAX_LIST_SIZE];
    
    /* vcache parameter */
    uint32_t         old_ref_addr_pa[2];
    byte             vreg_change; /* vcache register change flag: bit0-REF0 changed, bit1-REF1 changed (need to polling register until change done) */

    // memory management control
    DecRefPicMarking drpmb[MAX_MMCO_NUM];

    uint32_t frame_num;         /* frame_num for this frame */
    uint32_t prev_frame_num;    /* store the frame_num in the last decoded slice. For detecting gap in frame_num. */
    uint32_t primary_pic_type;  // for access_unit_delimiter
    
    /* POC calculation */
    uint32_t prevFrameNum;
    int32_t  toppoc;      /* poc of top field    (TopFieldOrderCnt) */
    int32_t  bottompoc;   /* poc of bottom field (BottomFieldOrderCnt) */
    int32_t  framepoc;    /* poc of this frame */
    int32_t  ThisPOC;
    
    /* for POC mode 0 */
    int32_t  prevPicOrderCntMsb;
    int32_t  prevPicOrderCntLsb;
    int32_t  PicOrderCntMsb;
    /* for POC mode 1 */
    int32_t  FrameNumOffset;
    int32_t  prevFrameNumOffset;

    // video format
    //uint32_t PicHeightInMapUnits; // PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1
    //uint32_t PicSizeInMapUnits;     // PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits
    uint32_t FrameHeightInMbs;      // FrameHeightInMbs = ( 2 - frame_mbs_only_flag ) * PicHeightInMapUnits
    //uint32_t PicHeightInMbs;        // PicHeightInMbs = FrameHeightInMbs / ( 1 + field_pic_flag )
    uint32_t PicSizeInMbs;          // PicSizeInMbs = PicWidthInMbs * PicHeightInMbs
    //uint32_t FrameSizeInMbs;
    uint32_t PicWidthInMbs;         // PicWidthInMbs = pic_width_in_mbs_minus1 + 1
    //uint32_t idr_pic_id;
    
    uint32_t frame_width;
    uint32_t frame_height;  // only 4:2:2 (420 will transform to 422)
    PictureStructure structure;    /* structure of current slice is frame/top/bottom. get from slice header */

    // flag
    byte field_pic_flag; /* 1 - field coding, 0 - frame coding */
    byte MbaffFrameFlag; /* 1 if (sps->mb_adaptive_frame_field_flag && !p_Vid->field_pic_flag) */
    byte idr_flag;       /* 1 if the current slice is IDR */
    byte left_mb_field_flag;
    byte dec_field;
    byte bottom_field_first; /* mbinfo is top bottom plane or bottom top plane */
    byte first_slice; /* whether the current slice is the first slice of a picture (frame or field) */
    byte new_frame; /* frame or top field (set reconstructed buffer) */

    //byte prev_mb_fieldflag;
    byte first_ref_pic_l1_top;
    byte reflistL1_0_long;
    byte col_fieldflag; /* 1 if col picture is a field, not a frame */
    byte col_idr_flag;
    byte top_mb_field_flag;
    byte prev_mb_fieldflag;
    
    uint8_t last_has_mmco_5;
    uint8_t last_pic_bottom_field;

    // quantization parameter
    int8_t QPu;
    int8_t QPv;
    
    uint8_t col_index;
    //uint8_t left_mb_cbp[2];
    //uint8_t left_mb_intra[2];

    /* output list */
    uint32_t output_poc; /* the poc of last outputed picture */
    int max_buffered_num; /* set by upper level driver/AP */
    /* The maximum of buffer number. If number of buffers driver used exceed max_buffered_num,
     * driver will output at least one reconstructed buffers until the number of used buffer 
     * is lower than max_buffered_num after AP output these buffer. Buffer will not be release 
     * when the buffer is not be outputed or it is a reference buffer.
     *   0: output directly (display order = decode order)
     *  -1: output until buffer is full (the display order is always right, but the buffer 
     *      will overflow if user don't release buffer before decode next frame
     *  otherwise: it will clip to max buffer number(depend on pattern level) */

#if OUTPUT_POC_MATCH_STEP
    /* for detecting the behavior of increment by 2 of poc */
    int poc_step;
    int even_counter;
#endif

    FAVC_DEC_BUFFER *pRecBuffer;
    int used_buffer_num; /* number of existing buffer in DPB (in field coding case, it increased at the first field inserted) */
    int unoutput_buffer_num; /* number of existing buffer in DPB that is not yet outputed (in field coding case, it increased at the first field inserted) */

    s16     last_stored_poc;

    /* release buffer list */
    uint8_t release_buffer_num;
    uint8_t release_buffer[MAX_DEC_BUFFER_NUM];

    /* output frame list */
    uint8_t   output_frame_num;
    FrameInfo output_frame[MAX_DEC_BUFFER_NUM];
    
    byte vcache_ilf_enable;

};


typedef struct decoder_engine_info_t
{
    uint16_t  engine;                /* engine index */
    int16_t   chn_idx;               /* bound channel index */
    int16_t   irq_num;

    uint32_t  u32IntSts;
    
    uint32_t *pu32BaseAddr;          /* MCP300 register virtual base address */
    uint32_t *pu32VcacheBaseAddr;    /* VCACHE register virtual base address */

    uint32_t  intra_vaddr;           /* intra buffer virtual address */
    uint32_t  intra_paddr;           /* intra buffer phy address */
    uint32_t  intra_length;          /* intra buffer size */

#if USE_LINK_LIST
    struct codec_link_list *codec_ll[LL_NUM];
    uint8_t   ll_en;                 /* 1 if linked list is used */
#endif

#if 1
    /* flags for deciding whether to wait frame done */
    uint8_t   vcache_ilf_enable;
    uint8_t   prev_pic_done;     /* Whether the previous decoding picture done occurred */
#endif

    /* for debug only */
    uint32_t  u32phy_addr;           /* MCP300 register phyical base address */
    uint32_t  u32vc_phy_addr;        /* VCACHE register phyical base address */


} DecoderEngInfo;


struct decoder_parameter_t
{
    /* start of reserved fields at __reset_parameter() */
    VideoParams   *p_Vid;
    int16_t        chn_idx;
    uint8_t        u8LosePicHandleFlags;

    MALLOC_PTR_dec pfnMalloc;
    FREE_PTR_dec   pfnFree;
    DAMNIT_PTR_dec pfnDamnit;
    MARK_ENGINE_START_PTR_dec pfnMarkEngStart;
    
    enum vcache_en_flag vcache_en_cfgs; /* enabled vcache configs for this channel */
    enum vcache_en_flag vcache_en_flgs; /* enabled vcache flags for the current slice */

    uint32_t u32DumpFrStart;
    uint32_t u32DumpFrEnd;
    uint32_t u32DumpCond;

    /* counters of IRQ */
    uint16_t u16SliceDone;
    uint16_t u16BSEmpty;
    uint16_t u16BSLow;
    uint16_t u16Timeout;
    uint16_t u16PicDone;
    uint16_t u16DmaRError;
    uint16_t u16DmaWError;
    uint16_t u16LinkListError;
    uint16_t u16VldError;

    uint16_t u16FrameDone;

    /* counters of typical errors */
    uint16_t u16FrNumSkipError;
    uint16_t u16SkipTillIDR; /* total skipped frame */
    uint16_t u16SkipTillIDRLast; /* skipped frame number from the last error */
    uint16_t u16WaitIDR;

    /* counters of decoding action */
    uint32_t u32DecodedFrameNumber;  /* number of start coding */
    uint32_t u32DecodeTriggerNumber; /* number of decode triggering */

    DecoderEngInfo *eng_info;
    /* end of reserved fields at __reset_parameter() */
    
    /* first non-reserved field. NOTE: do not change its order (see __reset_parameter() ) */
    uint16_t u16MaxWidth;       /* max width */
    uint16_t u16MaxHeight;      /* max height */
    uint16_t u16FrameWidth;     /* actual frame width (un-crpped) */
    uint16_t u16FrameHeight;    /* actual frame height (un-crpped) */

#if 1
    uint32_t *pu32BaseAddr;          /* MCP300 register base address */
    uint32_t *pu32VcacheBaseAddr;    /* VCACHE register base address */
#endif

#if USE_SW_PARSER
    Bitstream stream;
#endif

#if USE_LINK_LIST
    struct codec_link_list *codec_ll;// link list
#endif


    /* cropping info */
    uint16_t u16CroppingLeft;
    uint16_t u16CroppingRight;
    uint16_t u16CroppingTop;
    uint16_t u16CroppingBottom;
    uint16_t u16FrameWidthCropped;  /* actual frame width - crop_left - crop_right */
    uint16_t u16FrameHeightCropped; /* actual frame height - crop_top - crop_bottom */

    byte     multislice; /* 1 if current frame is triggered with first_slice == 0 */

    /* recorded ISR flags */
    enum intr_flags u32IntSts;
    enum intr_flags u32IntStsPrev;


    uint8_t  u8CurrState;   /* NORMAL_STATE or FIELD_DONE, for detecting NON_PAIRED_FIELD */
    byte     bChromaInterlace;
    byte     bUnsupportBSlice;
    byte     bOutputMBinfo;
    byte     bHasMBinfo;
    
    /* scale releated optioins */
    byte     bScaleEnable;  /* whether decoder user want to enable scale output */
    byte     bScaleEnabled; /* whether the scaling output is enabled in the current slice 
                             * (in some cases, such as multi-slices, scale output can not be enabled) */
    uint8_t  u8ScaleSize;
    uint8_t  u8ScaleType;
    uint16_t u16ScaleYuvWidthThrd;

    uint32_t u32BSLength; /* non-padded bitstream length in byte */
    uint32_t u32BSCurrentLength;

    uint8_t *pu8Bitstream_phy;
    uint8_t *pu8Bitstream_virt;

#if USE_RELOAD_BS
    uint32_t u32NextReloadBitstream_phy;
    uint32_t u32NextReloadBitstream_len;
    uint8_t  u8ReloadFlg;
#endif
    
    /* for debug */
    uint32_t u32SliceIndex;
    unsigned int error_sts;
    unsigned int buf_cnt;
    unsigned int buf_offset;
    unsigned char current_slice_type;
};




/* wrapper types/macros for HW/SW parser */
#if USE_SW_PARSER
/* SW parser functions/types */
typedef struct bitstream_t        *PARSER_HDL_PTR;
#define GET_PARSER_HDL(p_Dec)      (PARSER_HDL_PTR)&p_Dec->stream
#define SHOW_BITS(hdl, nbits)      sw_show_bits(hdl, nbits)
#define SKIP_BITS(hdl, nbits)      sw_skip_bits(hdl, nbits)
#define SKIP_BYTES(hdl, nbtyes)    sw_skip_byte(hdl, nbtyes)
#define BYTE_ALIGN(hdl)            sw_byte_align(hdl)
#define READ_UE(hdl)               sw_read_ue(hdl)
#define READ_SE(hdl)               sw_read_se(hdl)
#define READ_U(hdl, nbits)         sw_read_bits(hdl, nbits)
#define BIT_OFFSET(hdl)            sw_bit_offset(hdl)
#define BS_EMPTY(p_Dec)            sw_bs_emtpy(&p_Dec->stream)
#define GET_NEXT_NALU(p_Dec, nalu, no_more_bs)        sw_get_next_nalu(p_Dec, nalu, no_more_bs)
#else
/* HW parser functions/types */
typedef struct H264Reg_io        *PARSER_HDL_PTR;
#define GET_PARSER_HDL(p_Dec)     (PARSER_HDL_PTR)(((DecoderParams *)p_Dec)->pu32BaseAddr)
#define SHOW_BITS(hdl, nbits)     show_bits(hdl, nbits)
#define SKIP_BITS(hdl, nbits)     flush_bits(hdl, nbits)
#define SKIP_BYTES(hdl, nbtyes)   flush_bits(hdl, nbtyes*8)
#define BYTE_ALIGN(hdl)           byte_align(hdl)
#define READ_UE(hdl)              read_ue(hdl)
#define READ_SE(hdl)              read_se(hdl)
#define READ_U(hdl, nbits)        read_u(hdl, nbits)
#define BIT_OFFSET(hdl)           bit_offset(hdl)
#define BS_EMPTY(p_Dec)           decoder_bitstream_empty(p_Dec)
#define GET_NEXT_NALU(p_Dec, nalu, no_more_bs)        get_next_nalu(p_Dec, nalu, no_more_bs)
#endif

/* create/destory */
void *decoder_create(const FAVCD_DEC_INIT_PARAM *ptParam, int ndev);
void decoder_destroy(DecoderParams *p_Dec);

/* bind/unbind */
int decoder_bind_engine(DecoderParams *p_Dec, DecoderEngInfo *eng_info);
int decoder_unbind_engine(DecoderParams *p_Dec);

/* reset */
int decoder_eng_reset(DecoderEngInfo *eng_info);
void decoder_eng_init_reg(DecoderEngInfo *eng_info, int has_mb_info);
void decoder_eng_init_sram(DecoderEngInfo *eng_info);
int decoder_reset(DecoderParams *p_Dec);

/* set param */
int decoder_set_scale(FAVC_DEC_FR_PARAM *ptrDEC, DecoderParams *p_Dec);
int decoder_set_flags(DecoderParams *p_Dec, enum decoder_flag flag);
int decoder_reset_param(FAVC_DEC_PARAM *ptrDEC, DecoderParams *p_Dec);

/* HW bitstream parser */
int decoder_bitstream_empty(DecoderParams *p_Dec);

/* decode one frame */
int decode_one_frame_start(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame);
int decode_one_frame_next(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame);
void decode_one_frame_err_handle(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame);
#if USE_LINK_LIST
int decode_one_frame_ll(FAVC_DEC_FRAME_IOCTL *ptFrame, void *ptDecHandle);
int decode_one_frame_ll_top(FAVC_DEC_FRAME_IOCTL *ptFrame, void *ptDecHandle);
int decode_one_frame_ll_bottom(FAVC_DEC_FRAME_IOCTL *ptFrame, void *ptDecHandle);
#endif
int decode_slice(DecoderParams *p_Dec);

/* load bitstream */
int decoder_set_bs_buffer(DecoderParams *p_Dec);

/* DPB management */
int decoder_output_all_picture(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame, int get_release_flg);
#if USE_FEED_BACK
int decoder_feedback_buffer_used_done(DecoderParams *p_Dec, FAVC_DEC_FEEDBACK *ptRetBuffer);
#endif

/* IRQ */
int decoder_eng_receive_irq(DecoderEngInfo *eng_info);
int decoder_eng_dispatch_irq(DecoderParams *p_Dec, DecoderEngInfo *eng_info);
void decoder_clear_irq_flags(DecoderParams *p_Dec);

/* Linked List */
#if USE_LINK_LIST
int decoder_set_ll(DecoderParams *p_Dec, struct codec_link_list *codec_ll);
unsigned int decoder_get_ll_job_id(DecoderEngInfo *eng_info);
int decoder_trigger_ll_job(DecoderParams *p_Dec, int job_idx);
int decoder_trigger_ll_jobs(DecoderParams *p_Dec);
int decoder_init_ll_jobs(DecoderParams *p_Dec);
#endif

/* debug register */
void dump_all_register(unsigned int base_addr, unsigned int vc_base_addr, int level);
void dump_param_register(DecoderParams *p_Dec, unsigned int base_addr, int level);

/* debug DPB */
void dump_dpb(DecoderParams *p_Dec, int mem_info);
void dump_lt_ref_list(VideoParams *p_Vid, enum dump_cond cond);
void dump_output_list(VideoParams *p_Vid, enum dump_cond cond);
void dump_release_list(VideoParams *p_Vid, enum dump_cond cond);

#endif /* _GLOBAL_H_ */

