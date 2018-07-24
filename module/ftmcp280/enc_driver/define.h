#ifndef _DEFINE_H_
#define _DEFINE_H_

// maximum resolution definition
/*
#define RESOLUTION_4K4K     0
#define RESOLUTION_4K2K     0
#define RESOLUTION_1080P    1
*/
/*
#if RESOLUTION_4K4K
    #define SOFTWARE_TIMEOUT    8000
#elif RESOLUTION_4K2K
    #define SOFTWARE_TIMEOUT    5000
#else
*/
    #define SOFTWARE_TIMEOUT    3000
//#endif    

#define IS_FREXT_PROFILE(profile_idc) (profile_idc>=FREXT_HP || profile_idc == FREXT_CAVLC444)
#define iClip3(low, hight, value)	((value)<(low)?(low):((value)>(hight)?(hight):(value)))
#define iCeil(val, div)	((((val)+(div)-1)/(div))*(div))
#define iRound(val, div)	(((val)/(div))*(div))
#define iCeilDiv(val, div)	(((val)+(div)-1)/(div))
#define iAbs(a) ((a)>0 ? (a) : -(a))
#define iMin(a,b) ((a)<(b) ? (a) : (b))
#define iMax(a,b) ((a)>(b) ? (a) : (b))
//#define DIV_400(a)  (((a)+0x3FF)&0xFFFFFC00)
//#define MAX_DPB_SIZE 3
//#define MAX_LIST_SIZE 3
#define MAX_MMCO_NUM 32
#define MAX_DSF_NUM 32

//#define MAX_ENC_NUM     2
/*
#if RESOLUTION_1080P
    #define MAX_ENC_SRC_BUF 15
#elif RESOLUTION_4K2K
    #define MAX_ENC_SRC_BUF 4
#elif RESOLUTION_4K4K
    #define MAX_ENC_SRC_BUF 2
#endif
*/
#define SUPPORT_4K2K
#ifndef SUPPORT_4K2K
#define MAX_ENC_SRC_BUF 4
#define MAX_ENC_REF     4
#define MAX_DPB_SIZE    3
#define MAX_LIST_SIZE   3
#else
#define MAX_ENC_SRC_BUF 1
#define MAX_ENC_REF     1
#define MAX_DPB_SIZE    1
#define MAX_LIST_SIZE   1
#endif


//#define MAX_DEC_BUFFER 16
#define MAX_B_NUMBER        3		// max queue buffer
#define MAX_NUM_REF_FRAMES  3
#define MAX_SLICE_TYPE      3
#define RELEASE_ALL_BUFFER  0xFF
#define NO_RELEASE_BUFFER   0x0F
#define MAX_SLICE_NUM       0x20
#define MAX_OUTPUT_SLICE_NUM    4
// used up source buffer
#define USED_UP_ALL_SRC_BUFFER 0xFF
#define NO_USED_UP_SRC_BUFER 0xFFFFFFFF

#define BASELINE         66      //!< YUV 4:2:0/8  "Baseline"
#define MAIN             77      //!< YUV 4:2:0/8  "Main"
#define EXTENDED         88      //!< YUV 4:2:0/8  "Extended"
#define FREXT_HP        100      //!< YUV 4:2:0/8 "High"
#define FREXT_Hi10P     110      //!< YUV 4:2:0/10 "High 10"
#define FREXT_Hi422     122      //!< YUV 4:2:2/10 "High 4:2:2"
#define FREXT_Hi444     144      //!< YUV 4:4:4/14 "High 4:4:4"

#define FORMAT400   0
#define FORMAT420   1
#define FORMAT422   2
#define FORMAT444   3

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#ifndef UINT_MAX
#define UINT_MAX 2147483647
#endif
#ifndef INT_MIN
#define INT_MIN (-2147483647 -1)
#endif
#ifndef BYTE_MAX
#define BYTE_MAX 255
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef NULL
#define NULL 0
#endif

// level of log message
#define LOG_EMERGY  0
#define LOG_ERROR   1
#define LOG_WARNING 2
//#define LOG_MIDDLE  3
#define LOG_DEBUG   3
#define LOG_INFO    4
//#define LOG_TEST    6
#define LOG_DIRECT  0x100

extern int log_level;
extern int procForceDirectDump;
#ifdef LINUX
    #if 0
        #define LOG_TEST    3
        #define LOG_PRINT(level, fmt, arg...) { \
            if (level <= log_level) \
                printk("(%s): " fmt "\n", __FUNCTION__, ## arg);    }
    #else
        #define LOG_PRINT(...)
    #endif
#else
    #define LOG_PRINT(...)
#endif


// picture structure
#define FRAME_DONE 0x10
#define FIELD_DONE 0x08
#define PLANE_DONE 0x04
#define SLICE_DONE 0x02
// if the encoded frame is not in encoding order, buffer it
#define FRAME_WAIT 0xFF

#define RELEASE_UNUSED_BUFFER 2
#define OUTPUT_USED_BUFFER 1

#define SPATIAL_DEINTERLACE  0x01
#define TEMPORAL_DEINTERLACE 0x02
#define SPATIAL_DENOISE      0x04
#define TEMPORAL_DENOISE     0x08

#define PARTITION_16x16 0x01
#define PARTITION_16x8  0x02
#define PARTITION_8x16  0x04
#define PARTITION_8x8   0x08

#define MAX_POLLING_ITERATION   0x1000
//#define TIMEOUT_THRESHOLD       0x4000
#define TIMEOUT_THRESHOLD       0x400000    // for random pattern & low qp, default 0x40000


#define SUPPORT_MIN_WIDTH   128
#define SUPPORT_MIN_HEIGHT  96
#define DEFAULT_MAX_WIDTH   720
#define DEFAULT_MAX_HEIGHT  576

#define LOG_DELTA_QP_THD_LEARNRATE_BASE 4
#define DELTA_QP_THD_LEARNRATE_BASE (1<<LOG_DELTA_QP_THD_LEARNRATE_BASE)
#define LOG_DELTA_QP_STR_LEARNRATE_BASE 3
#define DELTA_QP_STR_LEARNRATE_BASE (1<<LOG_DELTA_QP_STR_LEARNRATE_BASE)

#define DISBALE_ROI_QP  0
#define ROI_DELTA_QP    1
#define ROI_FIXED_QP    2
#define ROI_REGION_NUM  8

#ifdef REORDER_JOB
    #define JOB_REORDER             -254
    #define ONLY_ONE_JOB            -252
#endif

typedef enum{
	H264E_OK = 0,                // Operation succeed
	H264E_ERR_MEMORY = -1,       // alloc memory error (serious error)
	H264E_ERR_API = -2,          // API version error
	H264E_SIZE_OVERFLOW = -3,
	H264E_ERR_INPUT_PARAM = -4,
	H264E_ERR_BS_OVERFLOW = -5,
	H264E_ERR_REF = -6,
	H264E_ERR_SRC_BUF = -7,
	H264E_ERR_REF_BUF = -8,
	H264E_ERR_IRQ = -9,
	H264E_ERR_SW_TIMEOUT = -10,
	H264E_ERR_HW_TIMEOUT = -11,
	H264E_ERR_DMA = -12,
	H264E_ERR_NO_BUF_B = -13,
	H264E_ERR_POLLING_TIMEOUT = -14,
	H264E_ERR_CHENNEL = -15,
    H264E_ERR_TRIGGER_SLICE = -16,
	H264E_ERR_UNKNOWN = -30,
	H264E_SW_RESET_TEST = -33
} H264E_RET;

// nal unit type
typedef enum{
	NALU_TYPE_SLICE    = 1,
	NALU_TYPE_DPA      = 2,
	NALU_TYPE_DPB      = 3,
	NALU_TYPE_DPC      = 4,
	NALU_TYPE_IDR      = 5,
	NALU_TYPE_SEI      = 6,
	NALU_TYPE_SPS      = 7,
	NALU_TYPE_PPS      = 8,
	NALU_TYPE_AUD      = 9,
	NALU_TYPE_EOSEQ    = 10,
	NALU_TYPE_EOSTREAM = 11,
	NALU_TYPE_FILL     = 12,
	NALU_TYPE_SPSE     = 13
} NaluType;

// slice type
typedef enum{
	P_SLICE = 0,
	B_SLICE = 1,
	I_SLICE = 2,
	SP_SLICE = 3,
	SI_SLICE = 4,
	NUM_SLICE_TYPES = 5
} SliceType;

typedef enum {
	CF_UNKNOWN = -1,     //!< Unknown color format
	YUV400     =  0,     //!< Monochrome
	YUV420     =  1,     //!< 4:2:0
	YUV422     =  2,     //!< 4:2:2
	YUV444     =  3      //!< 4:4:4
} ColorFormat;

typedef enum {
	FRAME,
	TOP_FIELD,
	BOTTOM_FIELD
} PictureStructure;

typedef enum {
	BYPASS_MODE = 0,
	ITL_MODE = 1,
	ELSE_MODE = 2
} SOURCE_MODE;

typedef enum {
    CFG_MIDDLE = 0,
    CFG_PERFORMANCE = 1,
    CFG_QUALITY = 2,
    CFG_USER = 3,
    CFG_FILE = 4,
} DefaultConfigure;

#if defined(GM8139)|defined(GM8136)|defined(GM8287)
    #define DISABLE_WRITE_OUT_REC
#endif

#define IRQ_SLICE_DONE      BIT0
#define IRQ_BS_FULL         BIT1
#define IRQ_DMA_READ_ERR    BIT2
#define IRQ_DMA_WRITE_ERR   BIT3
#define IRQ_HW_TIMEOUT      BIT4

#define INTERRUPT_FLAG (IRQ_SLICE_DONE|IRQ_BS_FULL|IRQ_DMA_READ_ERR|IRQ_DMA_WRITE_ERR|IRQ_HW_TIMEOUT)
#ifdef DISABLE_WRITE_OUT_REC
    #define INTERRUPT_FLAG_NO_DMA   (IRQ_SLICE_DONE|IRQ_BS_FULL|IRQ_HW_TIMEOUT)
    #if defined(GM8139)|defined(GM8136)
    #define UNKNOWN_ADDRESS 0xC0000000
    #elif defined(GM8287)
    #define UNKNOWN_ADDRESS 0x99000000
    #endif
#endif
/* interrupt: Bit 0: slice done
 *            Bit 1: bitstream buffer full
 *            Bit 2: DMA read error
 *            Bit 3: DMA write error
 *            Bit 4: encode MB timeout  */
 
#define HW_INTERRUPT    ((ptReg_cmd->STS2&INTERRUPT_FLAG)>0)
#define WAIT_EVENT      ((p_Enc->u32CurrIRQ&INTERRUPT_FLAG)>0)
#ifdef DISABLE_WRITE_OUT_REC
    #define HW_INTERRUPT_NO_DMA ((ptReg_cmd->STS2&INTERRUPT_FLAG_NO_DMA)>0)
    #define WAIT_EVENT_NO_DMA   ((p_Enc->u32CurrIRQ&INTERRUPT_FLAG_NO_DMA)>0)
#endif

#define INPUT_YUYV  0
// input YVU: 0: CbYCrY
//            1: YCbYCr
/*******************************************************
*  vcache definition
*******************************************************/
//#if defined(GM8129)|defined(FPGA_PLATFORM)
//#define MCP280_VCACHE_ENABLE    0
//#else
#define MCP280_VCACHE_ENABLE    1
//#endif
#if MCP280_VCACHE_ENABLE
    #define VCACHE_ILF_ENABLE   1
#else
    #define VCACHE_ILF_ENABLE   0
#endif

#define VCACHE_BLOCK_ENABLE     0

#define SET_VCACHE_MONO_REF     1

#ifdef GM8210
    #define VCACHE_MAX_REF0_WIDTH   1920
    #define VCACHE_MAX_REF1_WIDTH   1920
    #define PLATFORM_NAME           "GM8210"
#elif defined(GM8287)
    #define VCACHE_MAX_REF0_WIDTH   1920
    #define VCACHE_MAX_REF1_WIDTH   960
    #define PLATFORM_NAME           "GM8287"
#elif defined(GM8139)
    #define VCACHE_MAX_REF0_WIDTH   4096
    #define VCACHE_MAX_REF1_WIDTH   1920
    #define PLATFORM_NAME           "GM8139"
#elif defined(GM8136)
    #define VCACHE_MAX_REF0_WIDTH   1920
    #define VCACHE_MAX_REF1_WIDTH   960
    #define PLATFORM_NAME           "GM8136"
#endif
//#define VCACHE_MAX_REF_WIDTH    960

/*******************************************************
*  use source be didn reference frame when B is enable 
********************************************************/
#define ENABLE_TEMPORL_DIDN_WITH_B  0

/*******************************************************
*  CABAC packing zero
*******************************************************/
//#define HANDLE_CABAC_ZERO_WORD
//#define HANDLE_SPSPPS_ZERO_WORD

/*******************************************************
*   watermark not use sps/pps
*******************************************************/
#define WATERMARK_WITHOUT_SPSPPS    0

/*******************************************************
*   Not encode when bitstream overflow 
*******************************************************/

/*******************************************************
*   MCNR Enable
*******************************************************/
#ifndef GM8210
    #define MCNR_ENABLE
#endif

#ifdef VG_INTERFACE
    #define MAX_OUT_PROP_NUM    10
#endif

#ifdef GM8210
    #define TWO_ENGINE
    #define MAX_ENGINE_NUM  2
#else
    #define MAX_ENGINE_NUM  1
#endif
//#define DEFAULT_MAX_CHN_NUM 128

#ifdef TWO_ENGINE
    //#define FIX_TWO_ENGINE
    #define DYNAMIC_ENGINE
    #ifdef DYNAMIC_ENGINE
        #define CHANGE_ENGINE           -256
    #endif
#endif

/* not support delta qp bt rate control */
#define REGISTER_NCB
//#define NEW_SEARCH_RANGE
#define OVERFLOW_REENCODE
//#define HANDLE_PANIC_STATUS
//#define HANDLE_BS_CACHEABLE   // cache invalidate before encode go
#define HANDLE_BS_CACHEABLE2    // cache incalidate @ callback function
#define NEW_JOB_ITEM_ALLOCATE
#define AUTOMATIC_SET_MAX_RES
#define ENABLE_FAST_FORWARD
#ifdef ENABLE_FAST_FORWARD
    #define MIN_FASTFORWARD 2
    #define MAX_FASTFORWARD 6
#endif
#define ENABLE_CHECKSUM
#define NO_RESET_REGISTER_PARAM_UPDATE
#define OUTPUT_SLICE_OFFSET
#if MCP280_VCACHE_ENABLE&(defined(GM8139)|defined(GM8136))
    #define SAME_REC_REF_BUFFER
    // muse disable B frame & resolution constraint
    #ifdef SAME_REC_REF_BUFFER
        #define ONE_REF_BUF_VCACHE_OFF_SMALL
    #endif
#endif
#define TIGHT_BUFFER_ALLOCATION
#define PROC_SEQ_OUTPUT
#define AVOID_TDN_REENCODE
#if defined(GM8139)|defined(GM8136)
    #define ACTUALLY_REFERNCE_SIZE
    #define SUPPORT_FULL_TDN
#endif
#define USE_GMLIB_CFG
#define USE_CONFIG_FILE
#ifdef USE_CONFIG_FILE
    #define MAX_CFG_NUM     1
    #define CONFIG_FILENAME "favce_param.cfg"
    //#define ENABLE_USER_CONFIG
#endif
#define USE_KTHREAD
//#define HANDLE_2FRAME
#define OVERFLOW_RETURN_VALID_VALUE
#define LOCK_JOB_STAUS_ENABLE
#ifdef DYNAMIC_ENGINE
    #define CHECK_ENGINE_BALANCE
#endif
#define ENABLE_SWAP_WH
#define NEW_RESOLUTION
#define DYNAMIC_GOP_ENABLE
#ifdef SUPPORT_VG2
	#define DIRECT_CALLBACK
	#define PARSING_GMLIB_BY_VG
#endif
#define VCACHE_SUPPORT_MIN_WIDTH    208

/* local change */
//#define EVALUATE_PERFORMANCE
#define ADDRESS_UINT32
#define DIVIDE_GOP_PARAM_SETTING
//#define DUMP_ENCODER_STATUS
//#define DUMP_REGISTER_NAME
//#define STRICT_IDR_IDX

#endif
