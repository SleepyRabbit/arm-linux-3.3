#ifndef _DEFINE_H_
    #define _DEFINE_H_
    
    #define iClip3(low, high, val) ((val)<(low) ? (low) : ((val)>(high) ? (high) : (val)))
    #define iAbs(a) ((a)>0 ? (a) : -(a))
    #define iMin(a,b) (((a)<(b)) ? (a) : (b))
    #define iMax(a,b) (((a)>(b)) ? (a) : (b))
    #define MAX_DPB_SIZE  34
    #define MAX_LIST_SIZE 33
    #define MAX_MMCO_NUM  32
    #define MAX_DSF_NUM   32
    #define MAX_REF_FRAME_NUM  MAX_DPB_SIZE

/*
    #define TLU_OFFSET 0x30400
    #define H264_OFF_WTABLE 0xE00
    
    #define H264_OFF_SYSM 0x10c00
    #define H264_OFF_POCM 0x20000
    #define H264_OFF_DSFM 0x20280
    #define H264_OFF_CTXM 0x20400
    #define H264_OFF_SFMM 0x40000
    
    #define H264_OFF_SYSM_LEFTTOP_MB 0x600
    #define H264_OFF_SYSM_LEFTBOT_MB 0x700
    #define H264_OFF_SYSM_LEFT_MB 0x700
    #define H264_OFF_SYSM_TOP_FIELDFLAG 0x880
    #define H264_OFF_SYSM_TOPLEFT_FIELDFLAG 0xB80
*/
    #define BASELINE         66      //!< YUV 4:2:0/8  "Baseline"
    #define MAIN             77      //!< YUV 4:2:0/8  "Main"
    #define EXTENDED         88      //!< YUV 4:2:0/8  "Extended"
    #define FREXT_HP        100      //!< YUV 4:2:0/8 "High"
    #define FREXT_Hi10P     110      //!< YUV 4:2:0/10 "High 10"
    #define FREXT_Hi422     122      //!< YUV 4:2:2/10 "High 4:2:2"
    #define FREXT_Hi444     144      //!< YUV 4:4:4/14 "High 4:4:4"

#ifndef LINUX
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
#ifndef LOG_ERROR 
/* copy from decoder/decoder.h */
#define LOG_ERROR     0
#define LOG_WARNING   1
#define LOG_INFO      2
#endif
#define LOG_TRACE     3
#define LOG_DEBUG     4

/* global variables that controls the log function behavior */    
extern int log_level;
extern int dbg_mode;

#define MIN_DBG_MODE 0
#define MAX_DBG_MODE 4

/* define the effects of dbg_mode */
#define TREAT_WARN_AS_ERR(dbg_mode)       (dbg_mode >= 1) /* whether to treat some kind of warning as error */
#define PRINT_ERR_TO_CONSOLE(dbg_mode)    (dbg_mode >= 0) /* whether to print ioctl/VG driver error to console */
#define PRINT_LL_ERR_TO_CONSOLE(dbg_mode) (dbg_mode >= 2) /* whether to print low level error to console */
#define PRINT_ERR_TO_MASTER(dbg_mode)     (dbg_mode >= 0) /* whether to print error to master */
#define DAMNIT_AT_ERR(dbg_mode)           (dbg_mode >= 4) /* whether to call damnit at error */
#define SAVE_BS_AT_ERR(dbg_mode)          (dbg_mode >= 3) /* whether to save bitstream at error */
#define PRINT_ERR_IN_ISR(dbg_mode)        (dbg_mode >= 1) /* whether to print job error in ISR */


    enum intr_flags {
        INT_SLICE_DONE     = (1 << 0), /* bit0: slice done */
        INT_BS_EMPTY       = (1 << 1), /* bit1: runder run 0 (bitstream empty) */
        INT_BS_LOW         = (1 << 2), /* bit2: runder run 1 (bitstream under thershold) */ /* unused */
        INT_HW_TIMEOUT     = (1 << 3), /* bit3: hardware time out (time of decode one mb is longer than threshold) */
        INT_FRAME_DONE     = (1 << 4), /* bit4: frame done */ /* unused */
        INT_DMA_R_ERR      = (1 << 5), /* bit5: dma read error */
        INT_DMA_W_ERR      = (1 << 6), /* bit6: dma write error */
        INT_LINK_LIST_ERR  = (1 << 7), /* bit7: linked list error */
    };

   	//#define INTERRUPT_FLAG (BIT0|BIT1|BIT2|BIT3|BIT5|BIT6) // for streamming
   	//#define INTERRUPT_FLAG (BIT0|BIT1|BIT3|BIT5|BIT6|BIT7)
    #define INTERRUPT_FLAG         ( INT_SLICE_DONE \
                                   | INT_BS_EMPTY   \
                                   | INT_HW_TIMEOUT \
                                   | INT_DMA_R_ERR  \
                                   | INT_DMA_W_ERR  \
                                   | INT_LINK_LIST_ERR)

    #define TIMEOUT_CLOCK 200000 /* HW timeout */
    //#define TIMEOUT_CLOCK 20000000
    //#define TIMEOUT_CLOCK 0xFFFFFFFF

    #define MAX_BUF_CNT_VAL 0xFFFE /* the max value can be set to bitstream system buffer count register (offset 0x10) 
                                    * NOTE: a value of 0xFFFF indicates infinite bitstream
                                    */
    
    #define PADDING_SIZE        64
    /* Padding 64 bytes are required to avoid the bitstream empty occurred before slice done.
       The old comments seems not true with current driver:
       "Allegro_Inter_CABAC_02_A_HD_10.1 need padding 128 byte (+2) to avoid hw bitstream empty" */

    #define MAX_LEVEL_IDC       51 /* MAX supported level. 
                                      According to datasheet, it should be 41 due to the per-engine performance.
                                      Define this to 51 when testing with 4K2K patterns */

    #define MAX_SPS_NUM 32
    #define MAX_PPS_NUM 256
    #define LL_NUM      2

#if 0
    // for streaming input
    #define DEC_END_STREAM 0x80
    #define DEC_RELOAD_HEADER_BS_PARSING_DONE   0x40
    #define DEC_RELOAD_SLICE_BS 0x10
    #define DEC_RELOAD_HEADER_BS 0x20
    #define DEC_NORMAL 0x00
#endif

    /* decoder states */
    enum decoder_state {
        FRAME_DONE       = 0x08,
        NON_PAIRED_FIELD = 0x04,
        FIELD_DONE       = 0x02,
        SLICE_DONE       = 0x01,
        NORMAL_STATE     = 0x00,
    };
    
    //#define RESET_TIMEOUT_ITERATION 0x1000
    #define POLLING_ITERATION_TIMEOUT 0x100000

    typedef enum{
    	H264D_OK = 0,                   // Operation succeed
    	H264D_ERR_MEMORY = -1,          // alloc memory error (serious error)
    	H264D_ERR_API = -2,             // API version error
    	H264D_PARSING_ERROR = -3,
    	H264D_BS_ERROR = -4,            // bitstream overflow
    	H264D_SIZE_OVERFLOW = -5,       // frame size is larger than max frame size
    	H264D_ERR_VLD_ERROR = -6,       // vld error
    	H264D_ERR_BS_EMPTY = -7,        // no bitstream
        H264D_ERR_DECODER_NULL = -8,
        H264D_ERR_HW_UNSUPPORT = -9,    // hardware unsupport
        H264D_BS_LOW = -10,
    	H264D_ERR_BUFFER_FULL = -11,
    	H264D_NON_PAIRED_FIELD_ERROR = -12,
    	H264D_PARAMETER_ERROR = -13,
        H264D_NO_START_CODE = -14,
        H264D_ERR_DRV_UNSUPPORT = -15,   // driver unsupport 
    
    	H264D_ERR_TIMEOUT = -16,         // hardware timeout
    	H264D_ERR_SW_TIMEOUT = -17,      // software timeout
    	H264D_ERR_TIMEOUT_VLD = -18,
    	H264D_ERR_TIMEOUT_EMPTY = -19,
        H264D_ERR_POLLING_TIMEOUT = -20,
        H264D_ERR_HW_BS_EMPTY = -21,
    	H264D_ERR_DMA = -22,
        H264D_ERR_UNKNOWN = -23,
        H264D_MANAGE_BUF_ERROR = -24,    
        H264D_ERR_BUF_NUM_NOT_ENOUGH = -25,   // reference buffer numbers not enough 

        H264D_NO_PPS = -26,
        H264D_NO_SPS = -27,
        H264D_NO_IDR = -28,
        H264D_LOSS_PIC = -29,
        H264D_LINKED_LIST_ERROR = -30,
        H264D_SKIP_TILL_IDR = -31, /**/
        
    
    	H264D_NON_PAIRED_FIELD = -40,
    	H264D_RELOAD_HEADER = -41,
    	H264D_RELOAD_SLICE = -42,
    	H264D_END_STREAM = -43,
    	H264D_SWRESET_TEST = -44,
    	H264D_ERR_STATE = -45,

    	H264D_ERR_INTERRUPT = -46,
    	
    	H264D_PARSING_WARNING = 10,
    	H264D_F_UNSUPPORT = 11      // unsupport
    } H264D_RET;

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
    	FRAME        = 0,
    	TOP_FIELD    = 1,
    	BOTTOM_FIELD = 2
    } PictureStructure;
    

    // software reset
    //#define TEST_SOFTWARE_RESET

    /* Vcache */
    #define CONFIG_ENABLE_VCACHE	1
    #define VCACHE_BLOCK_ENABLE     0 /* it is recommanded to be disabled to have better performance */
    /* default seting VCACHE_BLOCK_ENABLE = 0 */
    #if CONFIG_ENABLE_VCACHE
        #define ENABLE_VCACHE_ILF       1
        #define ENABLE_VCACHE_SYSINFO   1
        #define ENABLE_VCACHE_REF       1
    #else
        #define ENABLE_VCACHE_ILF       0
        #define ENABLE_VCACHE_SYSINFO   0
        #define ENABLE_VCACHE_REF       0
    #endif


#ifdef __KERNEL__
    /* defines for VCACHE ref buffer size */
    #define VCACHE_ONE_REFERENCE_8210 0 /* define 0 to avoid changing VCACHE 0x30 (LOCAL_BASE) register on 8210 */
    #define MAX_REF1_WIDTH_8210    1920

    #define VCACHE_ONE_REFERENCE_8287 1 /* 1 if vcache has only one reference memory */
    #define MAX_REF1_WIDTH_8287     960 /* max width to enable vcache ref 1 */

    #define VCACHE_ONE_REFERENCE_A369 1 /* 1 if vcache has only one reference memory */
    #define MAX_REF1_WIDTH_A369     960 /* max width to enable vcache ref 1 */

#if defined(GM8210)
    #define VCACHE_ONE_REFERENCE VCACHE_ONE_REFERENCE_8210 /* define 0 to avoid changing VCACHE 0x30 (LOCAL_BASE) register on 8210 */
    #define MAX_REF1_WIDTH       MAX_REF1_WIDTH_8210
#elif defined(GM8287)
    #define VCACHE_ONE_REFERENCE VCACHE_ONE_REFERENCE_8287 /* 1 if vcache has only one reference memory */
    #define MAX_REF1_WIDTH       MAX_REF1_WIDTH_8287 /* max width to enable vcache ref 1 */
#elif defined(A369) 
    #define VCACHE_ONE_REFERENCE VCACHE_ONE_REFERENCE_A369 /* 1 if vcache has only one reference memory */
    #define MAX_REF1_WIDTH       MAX_REF1_WIDTH_A369 /* max width to enable vcache ref 1 */
#else
    #warning need to confirm with HW designer about this for the new platform
    #define VCACHE_ONE_REFERENCE 1 /* 1 if vcache has only one reference memory */
    #define MAX_REF1_WIDTH     960 /* max width to enable vcache ref 1 */
#endif

#endif /* __KERNEL__ */

#define VCACHE_MAX_WIDTH  1920 /* max width to enable VCACHE SYSINFO/ILF/REF */




    /***********************************************
    * slice_write_back = 0 (write through)
    *   SLICE_DMA_DISABLE on/off
    *   (prefer SLICE_DMA_DISABLE on)
    * slice_write_back = 1
    *   SLICE_DMA_DISABLE on
    *   (vcache on)
    ***********************************************/
    #if (ENABLE_VCACHE_ILF|ENABLE_VCACHE_SYSINFO)
        #define SLICE_WRITE_BACK        1
    #else
        #define SLICE_WRITE_BACK        1
        //#define SLICE_WRITE_BACK        0
    #endif

    #if SLICE_WRITE_BACK
        #define SLICE_DMA_DISABLE   1
    #else
        #define SLICE_DMA_DISABLE   0
        //#define SLICE_DMA_DISABLE   1
    #endif

    /**********************************************/


    /* SW/HW parser */
    #define USE_SW_PARSER           1 /* 0: use HW parser, 1: use SW parser */
    #define DBG_SW_PARSER           0
    
    #define SW_RESET_TIMEOUT_RET    1
    #define CLEAR_SRAM_AT_INIT      1

    /* for VG driver only */
    #define USE_WRITE_FILE            1 /* whether to use write file functions in VG driver */
    #define CHECK_ALLOC_FREE_JOB_ITEM 0 /* whether to do extra checking at allocating/freeing memory for job_item */
    #define CHK_LIST_ACCESS           0 /* check comman error at accessing list */
    #define RECORDING_BS              1 /* enable recording bitstream */
    #define LIMIT_VG_BUF_NUM          1 /* limit per channel bufer number according to mcp300_max_buf_num */
    #define CHECK_UNOUTPUT            0 /* FIXME: this seems to cause bug (might be race on callback_list at stopping) */

    /* for reverse playback */
    #define REV_PLAY                  0 /* reverse play back enable */
    #define REALLOC_BUF_FOR_GOP_JOB   1 /* FIXME: must be 1 */
    #define GOP_JOB_STOP_HANDLING     1 /* FIXME: not yet completed */

    /* for profiling */
    #define EXC_HDR_PARSING_FROM_UTIL  0 /* 1: exclude header parsing at measuring HW utilization 0: include header parsing at measuring HW utilization */
    #define USE_PROFILING              0

    /* output conditions */
    #define OUTPUT_POC_MATCH_STEP   0 /* to be removed */
    #define USE_OUTPUT_BUF_NUM      1
    
    #define USE_FEED_BACK           0 /* whether to use feed back output API: should be disabled for VG */


    #define USE_RELOAD_BS           1 /* whether to support reload bitstream (to handle bitstream size >= 4MB) */
    #define DISABLE_IRQ_IN_ISR      1 /* disable IRQ in ISR (for speeding up) */
    
    /* for verification only */
#define ENABLE_VERIFY_DEFINE        0
#if ENABLE_VERIFY_DEFINE
    #define CLEAR_RECON_FR          0
    #define SWITCH_PMU_EACH_FR      0
    #define DELAY_BEFORE_W_REG      0
    #define DELAY_BEFORE_TRIGGER    0
    #define BLOCK_READ_DISABLE      1 /* set it to 1 by default */
    #define CHK_UNCLEARED_IRQ       1
    #define TEST_SW_TIMEOUT         1
    #define TEST_HW_TIMEOUT         1
    #define CHK_VC_REF_EN_AT_SLICE1 1 /* 1-check after the second slice. 0-check for all slice */
#else
    #define CLEAR_RECON_FR          0
    #define SWITCH_PMU_EACH_FR      0
    #define DELAY_BEFORE_W_REG      0
    #define DELAY_BEFORE_TRIGGER    0
    #define BLOCK_READ_DISABLE      1 /* set it to 1 by default */
    #define CHK_UNCLEARED_IRQ       0
    #define TEST_SW_TIMEOUT         0
    #define TEST_HW_TIMEOUT         0
    #define CHK_VC_REF_EN_AT_SLICE1 0 /* 1-check after the second slice. 0-check for all slice */
#endif

    /* for linked list */
    #define USE_LINK_LIST           0 /* enable linked list */
    #define USE_SW_LINK_LIST_ONLY   0
    #define USE_MULTICHAN_LINK_LIST 0 /* 1: put multi-channel frame decoding job into one linked-list. 0: only decoding one frame in linked list */

    #define WAIT_FR_DONE_CTRL       2 /* 0: disabled, 1: enabled when ILF enabled and not error, 2: always enabled */
    #define TRACE_WAIT_FR_DONE_ERR  1

    #define HALT_AT_ERR             0 /* for checking if error path is executed */
    
    /* for debug only */
#define ENABLE_DBG_DEFINE           0
#if ENABLE_DBG_DEFINE
	#define TRACE_SW_RST            1
    #define PRINT_DMA_ERROR         1
    #define ENTER_DBG_MODE_AT_ERROR 1
    #define CHK_UNHANDLED_IRQ       1
    #define CHECK_USED_BUF_NUM      1
    #define CHECK_UNOUTPUT_BUF_NUM  1
    #define TRACE_SEM               1
    #define EN_DUMP_MSG             1
    #define CHK_ISR_DURATION        1       // define this to non-zero to enable ISR duration checking/profiling
    #define EN_WARN_ERR_MSG_ONLY    0
    #define DISABLE_ALL_MSG         0
    #define DISABLE_QSORT           0
    #define CLR_SLICE_HDR           1
    #define EN_MINOR_PROF           1 /* Enable minor profiling. Disabled for speedup */
    #define DBG_REV_PLAY            1
#else
    #define TRACE_SW_RST            0
    #define PRINT_DMA_ERROR         0
    #define ENTER_DBG_MODE_AT_ERROR 0
    #define CHK_UNHANDLED_IRQ       0
    #define CHECK_USED_BUF_NUM      0
    #define CHECK_UNOUTPUT_BUF_NUM  0
    #define TRACE_SEM               0
    #define EN_DUMP_MSG             0
    #define CHK_ISR_DURATION        1       // define this to non-zero to enable ISR duration checking/profiling
    #define EN_WARN_ERR_MSG_ONLY    1
    #define DISABLE_ALL_MSG         0
    #define DISABLE_QSORT           0
    #define CLR_SLICE_HDR           1
    #define EN_MINOR_PROF           1 /* Enable minor profiling. Disabled for speedup */
    #define DBG_REV_PLAY            0
#endif




#ifdef ENABLE_DEFINE_CHECK

#if REV_PLAY
    #warning REV_PLAY is not fully tested
#endif
/* show error / warnings for incorrect defines */
#if USE_LINK_LIST == 1 && USE_SW_PARSER == 0
    #error must use sw parser when link list enabled
#endif

#if USE_LINK_LIST == 1 //&& WAIT_FR_DONE_CTRL != 0
    #warning wait frame done is not properly handled
#endif

#if USE_MULTICHAN_LINK_LIST && WAIT_FR_DONE_CTRL != 2
    #error wait frame done is not properly handled. must be enabled and cannot handle decode error yet
#endif

#if ENABLE_VERIFY_DEFINE
    #warning this should be enabled only for verification and test
#endif

#if ENABLE_DBG_DEFINE
    #warning this should be enabled only for debug and test
#endif
#if TRACE_SW_RST
    #warning TRACE_SW_RST is enabled. this may cause printing uncessary debug message
#endif
#if PRINT_DMA_ERROR
    #warning PRINT_DMA_ERROR is enabled
#endif
#if ENTER_DBG_MODE_AT_ERROR
    #warning ENTER_DBG_MODE_AT_ERROR is enabled
#endif
#if CHK_UNHANDLED_IRQ
    #warning CHK_UNHANDLED_IRQ is enabled
#endif
#if HALT_AT_ERR
    #warning HALT_AT_ERR is enabled
#endif

#if CHECK_ALLOC_FREE_JOB_ITEM
    #warning CHECK_ALLOC_FREE_JOB_ITEM is enabled, which cause higher CPU loading
#endif

#if CHK_LIST_ACCESS
    #warning CHK_LIST_ACCESS is enabled, which cause higher CPU loading
#endif

#endif /* ENABLE_DEFINE_CHECK */



/* 
 * start of DUMP_MSG macros
 */

enum dump_cond {
    D_BUF_FR     = 0, 
    D_BUF_REF    = 1, 
    D_BUF_INFO   = 2, 
    D_BUF_IOCTL  = 3, 
    D_BUF_LIST   = 4, 
    D_BUF_SLICE  = 5, 
    D_BUF_DETAIL = 6, 
    D_BUF_ERR    = 7, 
    D_PARSE      = 8, 
    D_NPF        = 9, 
    D_ERR        = 10, 
    D_SW_PARSE   = 11, 
    D_NALU       = 12, 
    D_ISR        = 13, 
    D_LL         = 14, 
    D_VG_BUF     = 15,
    D_ENG        = 16,
    D_RES        = 17,
    D_REC_BS     = 18,
    D_REV        = 19,
    D_VG         = 20,
    D_MAX_DUMP_COND
};


#if EN_DUMP_MSG
    /* dump message under the conditions specified in each channel's data struction */
    #define SHOULD_DUMP(cond, dec) \
        ((dec->u32DumpCond & (1 << cond)) && (dec->u32DecodedFrameNumber >= dec->u32DumpFrStart && dec->u32DecodedFrameNumber < dec->u32DumpFrEnd))
        
    #define DUMP_MSG(cond, dec, fmt, args...) do { \
        if(SHOULD_DUMP(cond, ((DecoderParams *)dec))) { \
           printk(fmt, ## args);             \
        }                                    \
    }while(0)
    
    /* dump message under the conditions specified by a global variable */
    extern unsigned int dump_cond;
    #define SHOULD_DUMP_G(cond) (dump_cond & (1 << cond))
    #define DUMP_MSG_G(cond, fmt, args...) do {  \
            if(SHOULD_DUMP_G(cond)) {            \
               printk(fmt, ## args);             \
            }                                    \
        }while(0)


#else
    /* disable all DUMP_MSG */
    #define SHOULD_DUMP(cond, dec) 0
    #define DUMP_MSG(cond, dec, fmt, args...) do { }while(0)
    #define SHOULD_DUMP_G(cond)    0
    #define DUMP_MSG_G(cond, fmt, args...)    do { }while(0)
#endif

/* 
 * end of DUMP_MSG macros
 */



/* 
 * start of LOG_PRINT macros
 */
#define MODULE_NAME "FD" /* pre-fix string of log messages */

#if DISABLE_ALL_MSG
#define LOG_PRINT_COND(level) (0)
#elif EN_WARN_ERR_MSG_ONLY
#define LOG_PRINT_COND(level) (level <= LOG_WARNING)
#else
#define LOG_PRINT_COND(level) (1)
#endif

#ifdef VG_INTERFACE

    #include "log.h"
    
    #define LOG_PRINT_PFX(prefix, level, fmt, args...) do {\
    if(LOG_PRINT_COND(level)) {                          \
        const char *fmt_str = fmt;                       \
        char *pfx_str = prefix;                          \
        if (log_level >= level){                         \
            printm(pfx_str, fmt_str, ## args);           \
        }                                                \
        if (((log_level >= 0x10) && ((log_level & 0xF) >= level)) || \
            (PRINT_LL_ERR_TO_CONSOLE(dbg_mode) && level == LOG_ERROR)){\
            if(pfx_str != NULL) printk("[%s]", pfx_str); \
            printk(fmt_str, ## args);                    \
        }                                                \
    }\
    }while(0)
#else // !VG_INTERFACE
    #define LOG_PRINT_PFX(prefix, level, fmt, args...) do {  \
        if(LOG_PRINT_COND(level)) {                          \
            const char *fmt_str = fmt;                       \
            const char *pfx_str = prefix;                    \
            if ((log_level >= level) ||                      \
                (PRINT_LL_ERR_TO_CONSOLE(dbg_mode) && level == LOG_ERROR)){\
                if(pfx_str != NULL) printk("[%s]", pfx_str); \
                printk(fmt_str, ## args);                    \
            }                                                \
        }\
        }while(0)
#endif // !VG_INTERFACE

#define LOG_PRINT(level, fmt, args...)      LOG_PRINT_PFX(MODULE_NAME, level, fmt, ## args)
#define LOG_PRINT_NPFX(level, fmt, args...) LOG_PRINT_PFX(NULL, level, fmt, ## args)

/* output error/warning in low level driver */
#define E_DEBUG(fmt, args...) LOG_PRINT(LOG_ERROR,   "error:"fmt, ## args);
#define W_DEBUG(fmt, args...) LOG_PRINT(LOG_WARNING, "warning:"fmt, ## args);


/* 
 * end of LOG_PRINT macros
 */


/* 
 * start of PERF_MSG macros
 */

#ifdef __KERNEL__
/* Get time functions defined in GM's kernel (no header file can be included).
 * The time is ticking even when ISR is disabled */
unsigned int video_gettime_us(void);
unsigned int video_gettime_ms(void);
#endif


/* functions for profiling */
#if USE_PROFILING

#define PROF      -1
#define PROF_PP   -2
#define PROF_WSCH -3
#define PROF_ISR  -4
#define PROF_CB   -5
#define PROF_DEC  -9
#define REG_DUMP  -10

#ifdef VG_INTERFACE
#define PERF_MSG_N(n, fmt, args...) do{ if(log_level == n)    printm(MODULE_NAME, "[%04X][P]" fmt, video_gettime_us() & 0xFFFF, ## args); } while(0)
#define PERF_MSG(fmt, args...)      do{ if(log_level == PROF) printm(MODULE_NAME, "[%04X][P]" fmt, video_gettime_us() & 0xFFFF, ## args); } while(0)
#else
#define PERF_MSG_N(n, fmt, args...) do{ if(log_level == n)    printk(MODULE_NAME "[P]" fmt, ## args); } while(0)
#define PERF_MSG(fmt, args...)      do{ if(log_level == PROF) printk(MODULE_NAME "[P]" fmt, ## args); } while(0) 
#endif
#else /* !USE_PROFILING */
#define PERF_MSG_N(n, fmt, args...)
#define PERF_MSG(fmt, args...)
#endif /* !USE_PROFILING */

/* 
 * end of PERF_MSG macros
 */



#if USE_FEED_BACK
    enum {
        OUTPUT_USED_BUFFER = 1,
        RELEASE_UNUSED_BUFFER = 2,
    };
#endif    


#endif /* _DEFINE_H_ */

