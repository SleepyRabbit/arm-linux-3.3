#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/kallsyms.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#include "enc_driver/define.h"
#ifdef VG_INTERFACE
    #include "favc_enc_vg.h"
    #include "favc_enc_entity.h"
    #include "h264e_ioctl.h"
    #define H264E_PROC_DIR "videograph/h264e"
#else
    #include "favc_dev.h"
    #define H264E_PROC_DIR "h264e"
#endif
#ifdef REF_POOL_MANAGEMENT
    #include "mem_pool.h"
#endif

#define print_config(m, fmt, args...) { \
    if (m)                              \
        seq_printf(m, fmt, ## args);    \
    else                                \
        printk(fmt, ##args); }

/* proc defition */
static struct proc_dir_entry *h264e_entry_proc = NULL;
static struct proc_dir_entry *loglevel_proc = NULL;
static struct proc_dir_entry *info_proc = NULL;
#ifdef REF_POOL_MANAGEMENT
    static struct proc_dir_entry *ref_info_proc = NULL;
#endif
#ifdef PROC_SEQ_OUTPUT
    static struct proc_dir_entry *chn_info_proc = NULL;
#endif
#ifdef VG_INTERFACE
    static struct proc_dir_entry *cb_proc = NULL;
    static struct proc_dir_entry *util_proc = NULL;
    static struct proc_dir_entry *property_proc = NULL;
    static struct proc_dir_entry *job_proc = NULL;
    static struct proc_dir_entry *parm_proc = NULL;
    static struct proc_dir_entry *vg_info_proc = NULL;
    static struct proc_dir_entry *didn_proc = NULL;
    #ifdef TWO_ENGINE
        static struct proc_dir_entry *chn_proc = NULL;
    #endif
    #ifdef MCNR_ENABLE
        static struct proc_dir_entry *mcnr_proc = NULL;
    #endif
    static struct proc_dir_entry *qmatrix_proc = NULL;
    #ifdef USE_CONFIG_FILE
        static struct proc_dir_entry *config_proc = NULL;
    #endif
#endif
static struct proc_dir_entry *dbg_proc = NULL;
#ifdef MEM_CNT_LOG
    static struct proc_dir_entry *mem_cnt_proc = NULL;
#endif

#ifdef RECORD_BS
    static struct workqueue_struct *favc_enc_log_wq = NULL;
    static DECLARE_DELAYED_WORK(process_log_work, 0);
#endif

#ifdef VG_INTERFACE
    static unsigned int property_minor;
    static unsigned int job_minor;
#endif
int force_dump_register = 0;

#ifdef VG_INTERFACE
    extern unsigned int h264e_max_chn;
    extern unsigned int callback_period;
    extern unsigned int callback_period_jiffies;
    extern unsigned int utilization_period;
    #ifdef TWO_ENGINE
        extern unsigned int utilization_start[MAX_ENGINE_NUM], utilization_record[MAX_ENGINE_NUM];
    #else
        extern unsigned int utilization_start, utilization_record;
    #endif
    #ifdef RC_ENABLE
        extern int dump_bitrate;
    #endif
    extern int realloc_cnt_limit;
    extern int over_spec_limit;
    extern int default_configure;
    //extern int enable_quality_mode; // enable mode when di enable
    extern int adapt_search_range;  // Tuba 20131105: enable/disable adaptive search range by resolution
    //extern int gEntropyCoding;
    extern int gDidnMode;
    /*
	extern int gROIQPType; // 0: disable, 1: delta qp , 2: fixed qp
    extern int gROIDeltaQP;
	extern int gROIFixedQP;
	*/
    extern int forceQP;
    //extern int procForceUpdate[MAX_CHN_NUM];
    extern int *procForceUpdate;
    extern int procForceDirectDump;
    extern unsigned int h264e_max_b_frame;
    extern FAVC_ENC_PARAM gEncParam;
    extern FAVC_ENC_DIDN_PARAM gDiDnParam;
    extern FAVC_ENC_SCALE_LIST gScalingList;
    #ifdef USE_CONFIG_FILE
        extern unsigned int h264e_user_config;
        #ifdef ENABLE_USER_CONFIG
        extern struct favce_param_data_t gEncConfigure[MAX_CFG_NUM+1];  // idx 0 is not legal (0 means not use)
        #endif
        extern char *config_path;
    #endif
    #ifdef TWO_ENGINE
        extern int *chn_2_engine;
    #endif
    #ifdef FIX_TWO_ENGINE
        extern int procAssignEngine;
    #endif
    #ifdef DYNAMIC_ENGINE
        extern int engine_cost[MAX_ENGINE_NUM];
        #ifdef CHECK_ENGINE_BALANCE
            extern unsigned int gHWUtlThd;
            extern unsigned int gHWUtlDiffThd;
            extern unsigned int gForceBalanceMaxNum;
            extern unsigned int gUnbalanceCounter;
        #endif
    #endif
    extern int gIPOffset;
    extern int gPBOffset;
    extern int gQPStep;
    extern int gMinQuant;
    extern int gMaxQuant;
    extern unsigned int gLevelIdc;  // Tuba 20140520: let level idc setting can be modify by user

    #ifdef REORDER_JOB
        extern int sleep_time;
    #endif
    extern unsigned int video_gettime_us(void);
    extern int dump_property_value(char *str, unsigned int chn);
    extern int dump_job_info(char *str, unsigned int chn);
    extern int set_configure_value(FAVC_ENC_PARAM *pParam);
#endif  // VG_INTERFACE
extern int log_level;

#ifdef RECORD_BS
    extern struct buffer_info_t mcp280_rec_bs_buffer[2];
    //int cur_rec_buf_idx = 0;
    int rec_idx = 0;
    int save_idx = 0;
    int record_chn = -1;
    int rec_size[2] = {0, 0};
    int dump_log_busy[2] = {0, 0};
    int dump_size;
#endif
#ifdef OVERFLOW_RETURN_VALID_VALUE
    extern int gOverflowReturnLength;
#endif
#ifdef OVERFLOW_REENCODE
    extern int g_bitrate_factor;
#endif

extern int mcp280_ddr_id;
#ifdef MEM_CNT_LOG
    extern int allocate_cnt;
#endif
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	extern int update_param_reset_register;
#endif
#ifdef ENABLE_MB_RC
    extern unsigned int gMBQPLimit;
    extern unsigned int gMBRCBasicUnit;
#endif
#ifdef DYNAMIC_GOP_ENABLE
    extern unsigned int h264e_dynamic_gop;
    extern unsigned int gop_factor;
    extern unsigned int measure_static_event;
    extern unsigned int iframe_disable_roi;
#endif
extern unsigned int gTimeoutThreshold;

extern int favce_msg(char *str);

#ifdef VG_INTERFACE
#define UINT32  0
#define INT32   1
#define UINT8   2
#define INT8    3
#define HEX8    4

typedef struct mapping_st
{
    char *tokenName;
    void *value;
    int type;
    // 0: uint32_t, 1: int32_t, 2: uint8_t, 3: int8_t, 4: char array 
    // 5: double, 6: 4 integer, 7: int array
    int lb;
    int ub;
    char *note;
    int mask;	// valid (from config file or from property)
} MapInfo;

static const MapInfo syntax[] = 
{
#ifdef USE_CONFIG_FILE
    {"DefaultCfg",              &default_configure,                 INT32, CFG_MIDDLE, CFG_FILE, "", 0},
#else
    {"DefaultCfg",              &default_configure,                 INT32, CFG_MIDDLE, CFG_USER, "", 0},
#endif
    {"SymbolMode",              &gEncParam.bEntropyCoding,          UINT8,   0,   1, "0: CAVLC, 1: CABAC", 1},
    {"ROIQPType",               &gEncParam.u8RoiQPType,             UINT8,   0,   2, "0: disable, 1: delta QP, 2: fixed QP", 1},
    {"ROIDeltaQP",              &gEncParam.i8ROIDeltaQP,            INT8,  -20,  20, "ROI QP = cur QP + ROIDeltaQP", 1},   // change to signed value
    {"ROIFixedQP",              &gEncParam.u8ROIFixQP,              UINT8,   1,  51, "ROI QP = ROIFixedQP", 1},
    {"ResendSPSPPS",            &gEncParam.u8ResendSPSPPS,          UINT8,   0,   2, "Packing SPSPPS. 0: only first frame, 1: each I frame, 2: each frame", 1},
    {"CbQPOffset",              &gEncParam.i8ChromaQPOffset0,       INT8,  -12,  12, "Cr QP = Y QP + CrQPOffset", 1},
    {"CrQPOffset",              &gEncParam.i8ChromaQPOffset1,       INT8,  -12,  12, "Cb QP = Y QP + CrQPOffset", 1},
    {"DFDisableIdc",            &gEncParam.i8DisableDBFIdc,         INT8,    0,   2, "Deblocking filter. 0: enable, 1: disable, 2: not cross slice boundary", 1},
    {"DFAlpha",                 &gEncParam.i8DBFAlphaOffset,        INT8,  -12,  12, "Deblocking parameter", 1},
    {"DFBeta",                  &gEncParam.i8DBFBetaOffset,         INT8,  -12,  12, "Deblocking parameter", 1},
    {"DiDnMode",                &gDidnMode,                         INT32,  -1,  15, "DiDn mode. -1: decided by property, Bit0: sp di, Bit1: tp di, Bit2: sp dn, Bit3: tp dn", 0},
    {"PRef0SearchRangeX",       &gEncParam.u32HorizontalSR[0],      UINT32, 16,  64, "Search range of horizontal", 1},
    {"PRef0SearchRangeY",       &gEncParam.u32VerticalSR[0],        UINT32, 16,  32, "Search range of vertical", 1},
    {"DisableCoeff",            &gEncParam.bDisableCoefThd,         UINT8,   0,   1, "coefficient threshold. 0: enable, 1: disable", 1},
    {"LumaCoeffThd",            &gEncParam.u8LumaCoefThd,           UINT8,   0,   7, "parameter of coefficient threshold of luma", 1},
    {"ChromaCoeffThd",          &gEncParam.u8ChromaCoefThd,         UINT8,   0,   7, "parameter of coefficient threshold of chroma", 1},
    {"DeltaQPWeight",           &gEncParam.u8MBQPWeight,            UINT8,   4,   5, "Delta QP. 4: enable, 5: disable", 1},
    {"DeltaQPStrength",         &gEncParam.u8QPDeltaStrength,       UINT8,   0,  31, "parameter evaluate delta qp", 1},
    {"DeltaQPThd",              &gEncParam.u32QPDeltaThd,           UINT32,  0,1023, "parameter to evaluate delta qp", 1},
    {"MaxDeltaQP",              &gEncParam.u8MaxDeltaQP,            UINT8,   0,  12, "the maximal vlaue of delta qp", 1},
    {"Transform8x8",            &gEncParam.bTransform8x8Mode,       UINT8,   0,   1, "Transform 8x8. 0: disable, 1: enable", 1},
    {"InterDefaultTransformSize",&gEncParam.bInterDefaultTransSize, UINT8,   0,   1, "transform to evaluete inter cost. 0: 4x4, 1: 8x8", 1},
    {"DisablePInterPartition",  &gEncParam.u8PInterPredModeDisable, HEX8,    0,  15, "P pred. Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16", 1},
    {"DisableBInterPartition",  &gEncParam.u8BInterPredModeDisable, HEX8,    0,  15, "", 1},
    {"DisableIntra8x8",         &gEncParam.bIntra8x8Disable,        UINT8,   0,   1, "Intra 8x8 pred. 0: enable, 1: disable", 1},
    {"IntraMode",               &gEncParam.bIntra4x4Mode,           UINT8,   0,   1, "Intra 4x4 pred. 0: 5 mode, 1: 9 mode", 1},
    {"FastIntra4x4",            &gEncParam.bFastIntra4x4,           UINT8,   0,   1, "Intra fast alg. 0: disbale, 1: enable", 1},
    {"DisableIntra16x16Plane",  &gEncParam.bIntra16x16PlaneDisable, UINT8,   0,   1, "Intra 16x16 plane pred. 0: enable, 1: disbale", 1},
    {"DisableIntraInInter",     &gEncParam.bIntraDisablePB,         UINT8,   0,   1, "Intra pred @ P frame. 0: enable, 1: disable", 1},
    {"DisableIntra4x4",         &gEncParam.bIntra4x4DisableI,       UINT8,   0,   1, "Intra pred @ I frame. 0: enable, 1: disable", 1},
    {"DisableIntra16x16",       &gEncParam.bIntra16x16DisableI,     UINT8,   0,   1, "Intra pred @ I frmae. 0: enable, 1: disable", 1},
    {"IPOffset",                &gIPOffset,                         INT32, -51,  51, "I frame QP = P frame QP - IPOffset", 1},
    {"PBOffset",                &gPBOffset,                         INT32, -51,  51, "B frame QP = P frame QP + PBOffset", 0},
    {"QPStep",                  &gQPStep,                           INT32,   1,  12, "", 0},
    {"MinQuant",                &gMinQuant,                         INT32,   1,  51, "Default value of min quant", 0},
    {"MaxQuant",                &gMaxQuant,                         INT32,   1,  51, "Default value of max quant", 0},
    {"IntraCostRatio",          &gEncParam.u8IntraCostRatio,        UINT8,   0,  15, "Adjust possible of intra pred @ P frame", 1},
    {"ForceMV0Thd",             &gEncParam.u32ForceMV0Thd,          UINT32,  0,65535, "Force use PMV", 1},
    {"CABACInitMode",           &gEncParam.u8CabacInitIDC,          UINT8,   0,   1, "CABAC init mode", 1},
    {"CostEarlyTerminate",      &gEncParam.u32FMECostThd,           UINT32,  0,65535, "Early terminate by inter cost, 0: disbale", 1},
    {"PCycleEarlyTerminate",    &gEncParam.u32FMECycThd[P_SLICE],   UINT32,  0x20, 0xFFF, "Early terminate by cycle of P frame, 4095: disable", 1},
    {"BCycleEarlyTerminate",    &gEncParam.u32FMECycThd[B_SLICE],   UINT32,  0x20, 0xFFF, "Early terminate by cycle of B frame, 4095: disable", 1},
    {"ScalingListEnable",       &gEncParam.bScalingMatrixFlag,      UINT8,   0,   1, "Scaling matrix. 0: disable, 1: enable", 1},
#ifdef MCNR_ENABLE
    {"MCNREnable",              &gEncParam.bMCNREnable,             UINT8,   0,   1, "MCNR. 0: disbale, 1: enable", 1},
    {"MCNRShift",               &gEncParam.u8MCNRShift,             UINT8,   0,   3, "MCNR parameter", 1},
    {"MCNRMVThd",               &gEncParam.u32MCNRMVThd,           UINT32,   0,1023, "MCNR parameter", 1},
    {"MCNRVarThd",              &gEncParam.u32MCNRVarThd,          UINT32,   0, 255, "MCNR parameter", 1},
#endif
    {"Profile",                 &gEncParam.u32Profile,             UINT32,  66, 100, "default profile. 66: Baseline, 77: Main, 100: High", 0},
    {"LevelIdc",                &gLevelIdc,                        UINT32,   0,  51, "level idc. 0: using default setting, others: level_idc = LevelIdc/10", 0},
    {NULL,                      NULL,                               0,       0,   0}
#if 0
    {"ReferenceNumber",         &gParam.u8NumRefFrames,         UINT8,      1},
    {"SADOutputFormat",         &gParam.bSADSource,             UINT8,      1},
    {"BL0SearchRangeX",         &gParam.u32HorizontalBSR[0],    UINT32,     1},
    {"BL0SearchRangeY",         &gParam.u32VerticalBSR[0],      UINT32,     1},
    {"BL1SearchRangeX",         &gParam.u32HorizontalBSR[1],    UINT32,     1},
    {"BL1SearchRangeY",         &gParam.u32VerticalBSR[1],      UINT32,     1},
    {"PRef1SearchRangeX",       &gParam.u32HorizontalSR[1],     UINT32,     1},
    {"PRef1SearchRangeY",       &gParam.u32VerticalSR[1],       UINT32,     1},
    {"PRef2SearchRangeX",       &gParam.u32HorizontalSR[2],     UINT32,     1},
    {"PRef2SearchRangeY",       &gParam.u32VerticalSR[2],       UINT32,     1},
    {"MBQPBasicUnit",           &gParam.u32RCBasicUnit,         UINT32,     1},
    {"MBQPBitRate",             &gParam.u32MBRCBitrate,         UINT32,     1},
    {"RCPMaxMBQP",              &gParam.u8RCMaxQP[P_SLICE],     UINT8,      1},
    {"RCPMinMBQP",              &gParam.u8RCMinQP[P_SLICE],     UINT8,      1},
    {"RCBMaxMBQP",              &gParam.u8RCMaxQP[B_SLICE],     UINT8,      1},
    {"RCBMinMBQP",              &gParam.u8RCMinQP[B_SLICE],     UINT8,      1},
    {"RCIMaxMBQP",              &gParam.u8RCMaxQP[I_SLICE],     UINT8,      1},
    {"RCIMinMBQP",              &gParam.u8RCMinQP[I_SLICE],     UINT8,      1},
    {"DirectModeType",          &gParam.bDirectSpatialMVPred,   UINT8,      1},
#endif
};

static const MapInfo didn_syntax[] = 
{
    //unsigned char bUpdateDiDnParam;
    {"DiDnMode",            &gDidnMode,                     INT32,  -1,  15},
    /******************** spatial de-noise ********************/
    {"DnLineDetStr",        &gDiDnParam.u8SpDnLineDetStr,   UINT8,  0,  3},     // spatial de-noise line detection strength
    {"EdgeStr",             &gDiDnParam.u8EdgeStrength,     UINT8,  0,  255},   // edge detection strength
    {"DnMinFilterLv",       &gDiDnParam.u8DnMinFilterLevel, UINT8,  0,  3},     // spatial de-noise filter adjust parameter min. value
    {"DnMaxFilterLv",       &gDiDnParam.u8DnMaxFilterLevel, UINT8,  0,  7},     // spatial de-noise filter adjust parameter max. value
    {"DnSepHVEnable",       &gDiDnParam.bSeparateHrVtDn,    UINT8,  0,  1},     // enable separate horizontal/vertical spatial de-noise
    {"DnSepHVThd",          &gDiDnParam.u8SpDnThd,          UINT8,  0,  255},   // separate horz/vert spatial de-noise threshold
    /******************** temporal de-noise ********************/
#ifdef SUPPORT_FULL_TDN
    {"DnVarY",              &gDiDnParam.u8DnVarY,           UINT8,  0,  255},   // de-noise variance for Y
    {"DnVarCb",             &gDiDnParam.u8DnVarCb,          UINT8,  0,  255},   // de-noise variance for Cb
    {"DnVarCr",             &gDiDnParam.u8DnVarCr,          UINT8,  0,  255},   // de-noise variance for Cr
    // range: {2,4,6,8,10,12,14,16,20}
    // Temporal y/cb/cr denoise strength, the larger variance the stronger de-noise effect.
#else
    {"DnVarY",              &gDiDnParam.u8DnVarY,           UINT8,  0,  2},   // de-noise variance for Y
    {"DnVarCb",             &gDiDnParam.u8DnVarCb,          UINT8,  0,  2},   // de-noise variance for Cb
    {"DnVarCr",             &gDiDnParam.u8DnVarCr,          UINT8,  0,  2},   // de-noise variance for Cr
#endif
    {"DnVarPixUpBound",     &gDiDnParam.u32DnVarPixelUpperbound,    UINT32, 0,  1023},  // frame adaptive de-noise variance pixel upper bound threshold
    {"DnVarPixLowBound",    &gDiDnParam.u32DnVarPixelLowerbound,	UINT32, 0,  1023},  // frame adaptive de-noise variance pixel lower bound threshold
    {"DnVarLumaMBThd",      &gDiDnParam.u8DnVarLumaMBThd,   UINT8,  0,  255},   // frame adaptive de-noise variance Luma MB threshold
    {"DnVarChromaMBThd",    &gDiDnParam.u8DnVarChromaMBThd, UINT8,  0,  63},    // frame adaptive de-noise variance Chroma MB threshold
    /******************** spatial de-interlace ********************/
    {"ELADeAmbUpBound",     &gDiDnParam.u8ELADeAmbiguousUpperbound, UINT8,  0,  255},   // ELA(edge line average) de-ambiguous upper bound threshold
    {"ELADeAmbLowBound",    &gDiDnParam.u8ELADeAmbiguousLowerbound, UINT8,  0,  255},   // ELA de-ambiguous lower bound threshold
    {"ElaCornerEn",         &gDiDnParam.bELACornerEnable,   UINT8,  0,  1},     // enable ELA corner detection
    /******************** temporal de-interlace ********************/
    {"DiELAResult",         &gDiDnParam.bELAResult,         UINT8,  0,  1},     // set all de-interlaced pixels to ELA result
    // Set all pixels as motion
    {"DiTopMotionEn",       &gDiDnParam.bTopMotion,         UINT8,  0,  1},     // enable top motion detection
    // Top field motion detection function switch, 1 means on
    {"DiBotMotionEn",       &gDiDnParam.bBottomMotion,      UINT8,  0,  1},     // enable bot motion detection
    // Bottom field motion detection function switch, 1 means on
    {"DiStrongMotionEn",    &gDiDnParam.bStrongMotion,      UINT8,  0,  1},     // enable strong motion detection
    // Strong motion detection function switch, 1 means on
    {"DiStrongMotionThd",   &gDiDnParam.u8StrongMotionThd,  UINT8,  0,  255},   // strong motion detection threshold
    // Strong motion threshold of motion detection
    {"DiAdpSetMBMotionEn",  &gDiDnParam.bAdaptiveSetMBMotion,   UINT8,  0,  1}, // enable adaptively setting all MB motion
    // Motion block function switch, 1 means on
    // Motion block functon sets all pixels in the block as motion to improve human visual perception quality 
    // larger threshold, less motion MB, less ELA
    {"DiAdpSetMBMotionThd", &gDiDnParam.u8MBAllMotionThd,   UINT8,  0,  255},   // all MB motion threshold
    // Motion block function threshold
    {"DiStaticMBMaskEn",    &gDiDnParam.bStaticMBMask,      UINT8,  0,  1},     // enable static MB mask to set masked pixel static
    // Static block function switch, 1 menas on
    // Static block function detects static pixels around edge and keeps edge continuity
    {"DiStaticMBMaskThd",   &gDiDnParam.u8StaticMBMaskThd,  UINT8,  0,  255},   // static MB mask threshold
    // Static block function threshold
    {"DiExtendMotionEn",    &gDiDnParam.bExtendMotion,      UINT8,  0,  1},     // enable extend motion MB
    // Extended motion block function switch, 1 means on
    // Extended motion block function extends motion blocks to ease crawling effort under fine or periodic structure
    // larger threshold, less extended motion, less ELA
    {"DiExtendMotionThd",   &gDiDnParam.u8ExtendMotionThd,  UINT8,  0,  255},   // extend MB threshold
    // Extended motion block function threshold
    {"DiLineMaskEn",        &gDiDnParam.bLineMask,          UINT8,  0,  1},     // enable line MB mask to keep OSD shape
    // Line block function switch, 1 means on
    // Line block function is aimed to maintain artificail image structure
    {"DiLinMaskThd",        &gDiDnParam.u8LineMaskThd,      UINT8,  0,  255},   // line MB mask threshold
    // Line block function threshold
    {"DiLinMaskAdmThd",     &gDiDnParam.u8LineMaskAdmissionThd, UINT8,  0,  255},   // line MB mask admission threshold
    {NULL,                  NULL,                           0,       0,   0}
};

#if 0
static const int SCALE_LIST_INTRA4X4_Y[16] = {17,17,16,16,17,16,15,15,16,15,15,15,16,15,15,15};
static const int SCALE_LIST_INTRA4X4_Cb[16] = {6,12,19,26,12,19,26,31,19,26,31,35,26,31,35,39};
static const int SCALE_LIST_INTRA4X4_Cr[16] = {6,12,19,26,12,19,26,31,19,26,31,35,26,31,35,40};
static const int SCALE_LIST_INTER4x4_Y[16] = {9,13,18,21,13,18,21,24,18,21,24,27,21,24,27,30};
static const int SCALE_LIST_INTER4X4_Cb[16] = {0,13,18,21,13,18,21,24,18,21,24,27,21,24,27,30};
static const int SCALE_LIST_INTER4X4_Cr[16] = {9,13,18,21,13,18,21,24,18,21,24,27,21,24,27,27};
static const int SCALE_LIST_INTRA8x8[64] = {
    16,16,17,16,16,16,16,16, 16,17,16,16,16,15,15,15,
    17,16,16,16,15,15,15,15, 16,16,16,15,15,15,15,15,
    16,16,15,15,15,15,15,15, 16,15,15,15,15,15,15,16, 
    16,15,15,15,15,15,16,16, 16,15,15,15,15,16,16,16};
static const int SCALE_LIST_INTER8x8[64] = {
     0,16,17,16,16,16,16,16, 16,17,16,16,16,15,15,15,
    17,16,16,16,15,15,15,15, 16,16,16,15,15,15,15,15,
    16,16,15,15,15,15,15,15, 16,15,15,15,15,15,15,16,
    16,15,15,15,15,15,16,16, 16,15,15,15,15,16,16,16};
#else
static const int SCALE_LIST_INTRA4X4_Y[16] = {7,16,22,24,16,22,24,28,18,22,27,33,22,24,32,47};
static const int SCALE_LIST_INTRA4X4_Cb[16] = {7,16,22,24,16,22,24,28,18,22,27,33,22,24,32,47};
static const int SCALE_LIST_INTRA4X4_Cr[16] = {7,16,22,24,16,22,24,28,18,22,27,33,22,24,32,47};
static const int SCALE_LIST_INTER4x4_Y[16] = {13,15,17,18,15,17,18,20,17,18,21,22,18,20,22,25};
static const int SCALE_LIST_INTER4X4_Cb[16] = {13,15,17,18,15,17,18,20,17,18,21,22,18,20,22,25};
static const int SCALE_LIST_INTER4X4_Cr[16] = {13,15,17,18,15,17,18,20,17,18,21,22,18,20,22,25};
static const int SCALE_LIST_INTRA8x8[64] = {
    7,13,16,18,22,22,24,28, 13,13,18,20,22,24,28,31,
    16,18,22,22,24,28,28,32, 18,18,22,22,24,28,31,33,
    18,22,22,24,27,29,33,40, 22,22,24,27,29,33,40,48,
    22,22,24,28,32,38,47,57, 22,24,29,32,38,47,57,69};
static const int SCALE_LIST_INTER8x8[64] = {
    13,14,15,16,17,17,18,19, 14,15,16,17,17,18,19,20,
    15,16,17,17,18,19,20,21, 16,17,17,18,19,20,21,22,
    17,17,18,19,21,22,22,23, 17,18,19,20,22,22,23,25,
    18,19,20,22,22,23,25,26, 19,20,21,22,23,25,26,27};
#endif

#ifdef MCNR_ENABLE
static const uint8_t MCNR_TH_TABLE_LSAD[52] = {15,14,13,13,12,12,11,11,10,10,9,9,8,8,7,7,6,6,5,5,4,4,4,3,3,3,2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const uint8_t MCNR_TH_TABLE_HSAD[52] = {14,13,12,12,11,11,10,10,9,9,8,8,7,7,6,6,5,5,4,4,3,3,3,2,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#endif

static void set_default_enc_param(FAVC_ENC_PARAM *pParam, FAVC_ENC_DIDN_PARAM *pDiDnParam, FAVC_ENC_SCALE_LIST *pScalingList)
{
    if (NULL == pParam)
        return;
    pParam->u32Profile              = FREXT_HP;  // high profile
    pParam->u8CabacInitIDC          = 0;
    pParam->bSADSource              = 0;    // auto check (didn)
    pParam->bScalingMatrixFlag      = 0;
    pParam->bIntTopBtm              = 1;

    pParam->u32WKReinitPeriod       = 1;    // when CABAC, only be 1
    // Tuba 20130704: decide frame number bit & poc bit by b frame #
    if (h264e_max_b_frame <= 2) {
        pParam->u8Log2MaxFrameNum   = 4;
        pParam->u8Log2MaxPOCLsb     = 4;
    }
    else {
        pParam->u8Log2MaxFrameNum   = 5;
        pParam->u8Log2MaxPOCLsb     = 5;
    }

    // limited & constraint
    pParam->u8NumRefFrames          = 1;
    pParam->u8ResendSPSPPS          = 1;
    /*  0: output sps and pps only first IDR frame
     *  1: outptu sps and pps each I frame
     *  2: output sps and pps each frame */
#ifdef GM8139
    pParam->bEntropyCoding          = 1;
#elif defined(GM8136)
    pParam->bEntropyCoding          = 1;
#else
    pParam->bEntropyCoding          = 0;
#endif
    pParam->u8DiDnMode              = 0;
    pParam->i8ChromaQPOffset0       = 0;    // Tuba 20131216: Cb qp offset default value 0
    pParam->i8ChromaQPOffset1       = 0;    // Tuba 20131216: Cr qp offset default value 0
    pParam->i8DisableDBFIdc         = 0;
    pParam->i8DBFAlphaOffset        = 0;    // Tuba 20131216: alpha default value 0
    pParam->i8DBFBetaOffset         = 0;    // Tuba 20131216: beta default value 0
    pParam->u32HorizontalBSR[0]     = 32;
    pParam->u32VerticalBSR[0]       = 16;
    pParam->u32HorizontalBSR[1]     = 32;
    pParam->u32VerticalBSR[1]       = 16;
    pParam->u32HorizontalSR[0]      = 32;
    pParam->u32VerticalSR[0]        = 16;
    pParam->u32HorizontalSR[1]      = 32;
    pParam->u32VerticalSR[1]        = 16;
    pParam->u32HorizontalSR[2]      = 32;
    pParam->u32VerticalSR[2]        = 16;
    pParam->bDisableCoefThd         = 0;
    pParam->u8LumaCoefThd           = 4;
    pParam->u8ChromaCoefThd         = 4;
    pParam->u32FMECostThd           = 0;
    //pParam->u32FMECycThd[P_SLICE]   = 205;
    //pParam->u32FMECycThd[B_SLICE]   = 104;
    pParam->u32FMECycThd[P_SLICE]   = 0xFFF;
    pParam->u32FMECycThd[B_SLICE]   = 0xFFF;
#ifdef GM8139
    #ifdef ENABLE_MB_RC
    pParam->u8MBQPWeight            = 5;
    #else
    pParam->u8MBQPWeight            = 4;
    #endif
#elif defined(GM8136)
    #ifdef ENABLE_MB_RC
    pParam->u8MBQPWeight            = 5;
    #else
    pParam->u8MBQPWeight            = 4;
    #endif
#else
    pParam->u8MBQPWeight            = 5;
#endif
    pParam->u8QPDeltaStrength       = 19;
    pParam->u32QPDeltaThd           = 231;
    pParam->u8MaxDeltaQP            = 5;
    pParam->bInterDefaultTransSize  = 0;
    pParam->bTransform8x8Mode       = 0;
    pParam->u8PInterPredModeDisable = 0x06; // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
    pParam->u8BInterPredModeDisable = 0x0E; // Bit0: 8x8, Bit1: 8x16, Bit2: 16x8, Bit3: 16x16
    pParam->bIntra4x4Mode           = 0;    // 0: 5 mode, 1: 9mode
    pParam->bFastIntra4x4           = 1;
    pParam->bIntra8x8Disable        = 1;
    pParam->bIntra16x16PlaneDisable = 1;
    pParam->bDirectSpatialMVPred    = 0;
    pParam->bIntraDisablePB         = 0;
    pParam->bIntra4x4DisableI       = 0;
    pParam->bIntra16x16DisableI     = 0;
    pParam->u8RoiQPType             = 1;
    //pParam->u8ROIDeltaQP            = (unsigned char)(-4);
    pParam->i8ROIDeltaQP            = -4;
    pParam->u8ROIFixQP              = 20;
    pParam->u8IntraCostRatio        = 0;
    pParam->u32ForceMV0Thd          = 0;

    // didn parameter reset
    if (pDiDnParam) {
    #ifdef SUPPORT_FULL_TDN
        pDiDnParam->u8DnVarY = 0x08;
        pDiDnParam->u8DnVarCb = 0x08;
        pDiDnParam->u8DnVarCr = 0x08;
    #else
        pDiDnParam->u8DnVarY = 0x02;
        pDiDnParam->u8DnVarCb = 0x02;
        pDiDnParam->u8DnVarCr = 0x02;
    #endif
        pDiDnParam->u32DnVarPixelUpperbound = 0x08;
        pDiDnParam->u32DnVarPixelLowerbound = 0x08;
        pDiDnParam->u8DnVarLumaMBThd = 0x40;
        pDiDnParam->u8DnVarChromaMBThd = 0x10;
        pDiDnParam->bELAResult = 0x00;
        pDiDnParam->bTopMotion = 0x01;
        pDiDnParam->bBottomMotion = 0x01;
        pDiDnParam->bStrongMotion = 0x01;
        pDiDnParam->bAdaptiveSetMBMotion = 0x01;
        pDiDnParam->bStaticMBMask = 0x01;
        pDiDnParam->bExtendMotion = 0x01;
        pDiDnParam->bLineMask = 0x01;
        pDiDnParam->u8EdgeStrength = 0x28;
        pDiDnParam->u8DnMinFilterLevel = 0x00;
        pDiDnParam->u8DnMaxFilterLevel = 0x00;
        pDiDnParam->bSeparateHrVtDn = 0x01;
        pDiDnParam->u8SpDnThd = 0x08;
        pDiDnParam->u8MBAllMotionThd = 0x20;
        pDiDnParam->u8StaticMBMaskThd = 0x0A;
        pDiDnParam->u8ExtendMotionThd = 0x08;
        pDiDnParam->u8LineMaskThd = 0x06;
        pDiDnParam->u8StrongMotionThd = 0x28;
        pDiDnParam->u8ELADeAmbiguousUpperbound = 0x3C;
        pDiDnParam->u8ELADeAmbiguousLowerbound = 0x14;
        pDiDnParam->bELACornerEnable = 0x01;
        pDiDnParam->u8LineMaskAdmissionThd = 0x06;
        pDiDnParam->u8SpDnLineDetStr = 0x00;
    }

    // q matrix initialization
    if (pScalingList) {
        int i;
        for (i = 0; i < 8; i++)
            pScalingList->bScalingListFlag[i] = 1;
        memcpy(pScalingList->i32ScaleList4x4[0], SCALE_LIST_INTRA4X4_Y, sizeof(pScalingList->i32ScaleList4x4[0]));
        memcpy(pScalingList->i32ScaleList4x4[1], SCALE_LIST_INTRA4X4_Cb, sizeof(pScalingList->i32ScaleList4x4[1]));
        memcpy(pScalingList->i32ScaleList4x4[2], SCALE_LIST_INTRA4X4_Cr, sizeof(pScalingList->i32ScaleList4x4[2]));
        memcpy(pScalingList->i32ScaleList4x4[3], SCALE_LIST_INTER4x4_Y, sizeof(pScalingList->i32ScaleList4x4[3]));
        memcpy(pScalingList->i32ScaleList4x4[4], SCALE_LIST_INTER4X4_Cb, sizeof(pScalingList->i32ScaleList4x4[4]));
        memcpy(pScalingList->i32ScaleList4x4[5], SCALE_LIST_INTER4X4_Cr, sizeof(pScalingList->i32ScaleList4x4[5]));
        memcpy(pScalingList->i32ScaleList8x8[0], SCALE_LIST_INTRA8x8, sizeof(pScalingList->i32ScaleList8x8[0]));
        memcpy(pScalingList->i32ScaleList8x8[1], SCALE_LIST_INTER8x8, sizeof(pScalingList->i32ScaleList8x8[1]));
    }    

    // mcnr parameter initialization
#ifdef MCNR_ENABLE
    #ifdef GM8139
        pParam->bMCNREnable = 1;
    #elif defined(GM8136)
        pParam->bMCNREnable = 1;
    #else
        pParam->bMCNREnable = 0;
    #endif
    pParam->u8MCNRShift = 2;
    pParam->u32MCNRMVThd = 4;
    pParam->u32MCNRVarThd = 10;
    memcpy(pParam->mMCNRTable.u8LSAD, MCNR_TH_TABLE_LSAD, sizeof(pParam->mMCNRTable.u8LSAD));
    memcpy(pParam->mMCNRTable.u8HSAD, MCNR_TH_TABLE_HSAD, sizeof(pParam->mMCNRTable.u8HSAD));
#endif
    set_configure_value(pParam);
}

#ifdef USE_CONFIG_FILE
#ifdef ENABLE_USER_CONFIG
static int param_getline(char *line, int size, struct file *fd, unsigned long long *offset)
{
    char ch;
    int lineSize = 0, ret;

    memset(line, 0, size);
    while ((ret = (int)vfs_read(fd, &ch, 1, offset)) == 1) {
        if (lineSize >= MAX_LINE_CHAR) {
            printk("Line buf is not enough %d! (%s)\n", MAX_LINE_CHAR, line);
            break;
        }
        line[lineSize++] = ch;
        if (ch == 0xA)
            break;
    }
    return lineSize;
}

static int param_readline(struct file *fd, int size, char *line, unsigned long long *offset)
{
    int ret = 0;
    do {
        ret = param_getline(line, size, fd, offset);
    } while (((line[0] == 0xa)||(line[0] == '#')) && (ret != 0));
    line[ret] = '\0';
    return ret;
}

static int find_syntax_idx(char *syntax_name)
{
    int idx;
    for (idx = 0; syntax[idx].tokenName != NULL; idx++) {
        if (strcmp(syntax[idx].tokenName, syntax_name) == 0)
            return idx;
    }
    return -1;
}

static int parsing_syntax(char *line, int len)
{
    int i = 0, idx;
    char *ptr;
    int value;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;

    // skip blank
    while (line[i] == ' ')
        i++;
    // syntax start
    ptr = &line[i];
    while (line[i] != ' ' && line[i] != '=')
        i++;
    line[i] = '\0';
    // syntax end
    idx = find_syntax_idx(ptr);
    if (idx < 0) {
        favce_err("can not find syntax \"%s\"\n", ptr);
        return -1;
    }
    // syntax value
    i++;
    // skip blank
    while (line[i] == ' ' || line[i] == '=')
        i++;
    if (line[i] == '0' && line[i+1] == 'x')
        sscanf(&line[i+2], "%x", &value);
    else {
        sscanf(&line[i], "%d", &value);
    }

    if (value < syntax[idx].lb || value > syntax[idx].ub) {
        favce_err("%s(%d) is out of range! (%d ~ %d)\n", syntax[idx].tokenName, value, syntax[idx].lb, syntax[idx].ub);
        return -1;
    }

    switch (syntax[idx].type) {
        case UINT32:
            u32Val = (unsigned int *)syntax[idx].value;
            *u32Val = value;
            break;
        case INT32:
            i32Val = (int *)syntax[idx].value;
            *i32Val = value;
            break;
        case UINT8:
        case HEX8:
            u8Val = (unsigned char *)syntax[idx].value;
            *u8Val = value;
            break;
        case INT8:
            i8Val = (char *)syntax[idx].value;
            *i8Val = value;
            break;
        default:
            break;
    }
    if (0 == idx)
        return 1;
    return 0;
}
#endif  // ENABLE_USER_CONFIG

void dump_enc_config(struct seq_file *m, int config_idx, struct favce_param_data_t *cfg_data)
{
    int i = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;
    FAVC_ENC_PARAM bakParam;
    //FAVC_ENC_DIDN_PARAM bakDiDnParam;
    int bak_ip_offset;

    if (cfg_data && cfg_data->valid) {
        memcpy(&bakParam, &gEncParam, sizeof(FAVC_ENC_PARAM));
        //memcpy(&bakDiDnParam, &gDiDnParam, sizeof(FAVC_ENC_DIDN_PARAM));
        bak_ip_offset = gIPOffset;
        memcpy(&gEncParam, &cfg_data->enc_param, sizeof(FAVC_ENC_PARAM));
        gIPOffset = cfg_data->ip_offset;
    }
    else
        return;
    print_config(m, "***************************\n");
    print_config(m, "*       configure %d       *\n", config_idx);
    print_config(m, "***************************\n");
    //printk("configure %d\n", config_idx);
    //printk("    parameter name        value                note\n");
    //printk("=======================  =======  ==============================\n");
    print_config(m, "    parameter name        value\n");
    print_config(m, "=======================  =======\n");
    while (syntax[i].tokenName) {
        if (0 == syntax[i].mask) {
            i++;
            continue;
        }
        print_config(m, "%-25s  ", syntax[i].tokenName);
        switch (syntax[i].type) {
            case UINT32:
                u32Val = (unsigned int*)syntax[i].value;
                print_config(m, "%4u", *u32Val);
                break;
            case INT32:
                i32Val = (int*)syntax[i].value;
                print_config(m, "%4d", *i32Val);
                break;
            case UINT8:
                u8Val = (unsigned char*)syntax[i].value;
                print_config(m, "%4u", *u8Val);
                break;
            case HEX8:
                u8Val = (unsigned char*)syntax[i].value;
                print_config(m, "0x%02x", *u8Val);
                break;
            case INT8:
                i8Val = (char *)syntax[i].value;
                print_config(m, "%4d", *i8Val);
                break;
            default:
                break;
        }
        print_config(m, "\n");
        i++;
    }
    print_config(m, "\n");
    memcpy(&gEncParam, &bakParam, sizeof(FAVC_ENC_PARAM));
    gIPOffset = bak_ip_offset;
}

static void reset_default_config(void)
{
    int i;
    set_default_enc_param(&gEncParam, NULL, NULL);
    for (i = 0; i < MAX_CFG_NUM; i++) {
    #ifdef ENABLE_USER_CONFIG
        memcpy(&gEncConfigure[i].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
        gEncConfigure[i].ip_offset = gIPOffset;
        gEncConfigure[i].valid = 0;
    #endif
    }
}

// parsing configure
static int parsing_config_file(void)
{
#ifdef ENABLE_USER_CONFIG
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int len;
    int old_cfg_idx = -1;
    int ret = 0;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, CONFIG_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        favce_err("open %s error\n", filename);
        return -1;
    }

    set_default_enc_param(&gEncParam, NULL, NULL);
    while ((len = param_readline(fd, sizeof(line), line, &offset)) > 0) {
        // parsing token & value
        ret = parsing_syntax(line, len);
        if (ret < 0)
            continue;
        else if (1 == ret) {
            if (old_cfg_idx > 0 && old_cfg_idx <= MAX_CFG_NUM) {    // not allow idx 0
                // copy configure
                memcpy(&gEncConfigure[old_cfg_idx].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
                memcpy(&gEncConfigure[old_cfg_idx].didn_param, &gDiDnParam, sizeof(FAVC_ENC_DIDN_PARAM));
                gEncConfigure[old_cfg_idx].ip_offset = gIPOffset;
                gEncConfigure[old_cfg_idx].valid = 1;
                set_default_enc_param(&gEncParam, &gDiDnParam, NULL);  // reset global variable
                //dump_enc_config(NULL, old_cfg_idx, &gEncConfigure[old_cfg_idx].enc_param);
            }
            old_cfg_idx = default_configure;
        }
    }
    //pCfgEncParam[old_cfg_idx] = &gCfgEncParam[old_cfg_idx];
    memcpy(&gEncConfigure[old_cfg_idx].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
    memcpy(&gEncConfigure[old_cfg_idx].didn_param, &gDiDnParam, sizeof(FAVC_ENC_DIDN_PARAM));
    gEncConfigure[old_cfg_idx].ip_offset = gIPOffset;
    gEncConfigure[old_cfg_idx].valid = 1;
    //dump_enc_config(NULL, old_cfg_idx, &gEncConfigure[old_cfg_idx].enc_param);

    filp_close(fd, NULL);
    set_fs(fs);
#endif
    return 0;
}

#ifdef PROC_SEQ_OUTPUT
static void *proc_config_start(struct seq_file *m, loff_t *pos)
{
    //loff_t n = *pos;
    int idx = (int)(*pos);
    if (0 == idx) {
        //seq_printf(m, " chn    resolution   buf.type  gop   mode         fps         bitrate    max.br    init.q  min.q  max.q  qp\n");
        //seq_printf(m, "=====  ============  ========  ===  ======  ===============  =========  =========  ======  =====  =====  ==\n");
    }
    if (idx <= MAX_CFG_NUM) {
        int idx_p1 = idx+1;
        return (void *)idx_p1;
    }
    return 0;
}
static int proc_config_show(struct seq_file *m, void *p)
{
#ifdef ENABLE_USER_CONFIG
    int idx = (int)p;
    if (gEncConfigure[idx].valid)
        dump_enc_config(m, idx, &gEncConfigure[idx]);
#endif
    return 0;
}
static void *proc_config_next(struct seq_file *m, void *p, loff_t *pos)
{
    int chn = (int)p;
    (*pos)++;
    if (chn+1 <= MAX_CFG_NUM) 
        return (void *)(chn+1);
    return 0;
}
static void proc_config_stop(struct seq_file *m, void *p)
{
}
static struct seq_operations proc_config_seq_ops = {
    .start = proc_config_start,
    .next  = proc_config_next,
    .stop  = proc_config_stop,
    .show  = proc_config_show,
};
static int proc_config_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &proc_config_seq_ops);
}
static struct file_operations proc_config_operations = {
        .open = proc_config_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};
#endif  // PROC_SEQ_OUTPUT

#endif  // USE_CONFIG_FILE

#endif  // VG_INTERFACE

#ifdef RECORD_BS
#define REC_PATH "/mnt/nfs"
int rec_file_idx = 0;
static void En_log_File(void *param)
{
	int ret;
	struct file * fd1 = NULL;
	mm_segment_t fs;
	unsigned char fn[0x80];
	unsigned long long offset = 0;

    //dump_log_busy = 1;    

	fs = get_fs();
	set_fs(KERNEL_DS);

    sprintf(fn, "%s/chn%d_rec_%03d.264", REC_PATH, record_chn, rec_file_idx++);
	fd1 = filp_open(fn, O_WRONLY|O_CREAT, 0777);
	if (IS_ERR(fd1)) {
		printk ("Error to open %s", fn);
		return;
	}

	//ret = vfs_write(fd1, (unsigned char*)rec_buffer.addr_va, rec_length, &offset);
	ret = vfs_write(fd1, (unsigned char*)mcp280_rec_bs_buffer[save_idx].addr_va, rec_size[save_idx], &offset);
	if(ret <= 0)
		printk ("Error to write En_log_File %s\n", fn);
	filp_close(fd1, NULL);
	set_fs(fs);
    printk("log %s\n", fn);
    rec_size[save_idx] = 0;
    dump_log_busy[save_idx] = 0;
}

#define BS_OFFSET   0x2A
int dump_bitstream(void *ptr, int size, int chn)
{
    //int thd;
    if (chn == record_chn) {
        if (dump_log_busy[rec_idx] || 0 == mcp280_rec_bs_buffer[rec_idx].addr_va)
            return 0;
        //printk("dump %d\n", size);
        if (rec_size[rec_idx] + size < mcp280_rec_bs_buffer[rec_idx].size) {
            memcpy((void *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx]), ptr, size);
            rec_size[rec_idx] += size;
        }
        else {
            // dump bitstream
            dump_log_busy[rec_idx] = 1;
            save_idx = rec_idx;
            rec_idx ^= 0x01;
            PREPARE_DELAYED_WORK(&process_log_work, (void *)En_log_File);
            queue_delayed_work(favc_enc_log_wq, &process_log_work, 1);
            memcpy((void *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx]), ptr, size);
            rec_size[rec_idx] += size;
        }
    }
    else if (record_chn == 0xFF) {
        if (dump_log_busy[rec_idx] || 0 == mcp280_rec_bs_buffer[rec_idx].addr_va)
            return 0;
        if (rec_size[rec_idx] + dump_size + 4 < mcp280_rec_bs_buffer[rec_idx].size) {
            unsigned char *buf = (unsigned char *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx]);
            buf[0] = chn;
            buf[1] = size&0xFF;
            buf[2] = (size>>8)&0xFF;
            buf[3] = (size>>16)&0xFF;
            memcpy((void *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx] + 4), ptr+ BS_OFFSET, dump_size);
            rec_size[rec_idx] += dump_size + 4;
        }
        else {
            // dump bitstream
            unsigned char *buf;
            dump_log_busy[rec_idx] = 1;
            save_idx = rec_idx;
            rec_idx ^= 0x01;
            PREPARE_DELAYED_WORK(&process_log_work, (void *)En_log_File);
            queue_delayed_work(favc_enc_log_wq, &process_log_work, 1);

            buf = (unsigned char *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx]);
            buf[0] = chn;
            buf[1] = size&0xFF;
            buf[2] = (size>>8)&0xFF;
            buf[3] = (size>>16)&0xFF;
            memcpy((void *)(mcp280_rec_bs_buffer[rec_idx].addr_va + rec_size[rec_idx] + 4), ptr + BS_OFFSET, dump_size);
            rec_size[rec_idx] += dump_size + 4;
        }
    }
    return 0;
}

#endif

static int proc_info_write_mode(struct file *file, const char *buffer,unsigned long count, void *data)
{
    return 0;
}
static int proc_info_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;
    *eof = 1;
    len = favce_msg(page);
    return len;
}

static int proc_log_level_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len;
    len = sprintf(page, "Log level = %d (%d: emergy, %d: error, %d: warning, %d: debug, %d: info)\n", 
        log_level, LOG_EMERGY, LOG_ERROR, LOG_WARNING, LOG_DEBUG, LOG_INFO);
    *eof = 1;
    return len;
}
static int proc_log_level_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char str[0x10];
    unsigned long len = count;

    if (len >= sizeof(str))
        len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%d", &log_level);
    return count;
}
#ifdef MEM_CNT_LOG
static int proc_mem_cnt_read(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;
    len = sprintf(page, "memory cnt: %d\n", allocate_cnt);
    *eof = 1;
    return len;
}
static int proc_mem_cnt_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    return count;
}
#endif

#ifdef VG_INTERFACE
static int proc_cb_write_mode(struct file *file, const char *buffer,unsigned long count, void *data)
{
    char str[0x10];
    unsigned long len = count;

    if (len >= sizeof(str))
        len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;

    sscanf(str, "%d", &callback_period);
    callback_period_jiffies = msecs_to_jiffies(callback_period);
    return count;
}
static int proc_cb_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    *eof = 1;
    return sprintf(page, "Callback Period = %d (msecs)\n", callback_period);
}

static int proc_util_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char str[0x10];
    unsigned long len = count;

    if (len >= sizeof(str))
        len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;

    sscanf(str, "%d", &utilization_period);
    return count;
}
static int proc_util_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
#ifdef TWO_ENGINE
    int i;
#endif
    int len = 0;

    *eof = 1;
#ifdef TWO_ENGINE
    for (i = 0; i < MAX_ENGINE_NUM; i++) {
        if (utilization_start[i] == 0)
            break;
        len += sprintf(page+len, "Engine%d HW Utilization Period=%d(sec) Utilization=%d\n",
            i, utilization_period, utilization_record[i]);
    }
#else
    if (utilization_start != 0) {
        len += sprintf(page+len, "HW Utilization Period=%d(sec) Utilization=%d\n",
            utilization_period, utilization_record);
    }
#endif

    return len;
}

static int proc_property_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char str[0x10];
    unsigned long len = count;

    if (len >= sizeof(str))
        len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;

    sscanf(str, "%d", &property_minor);
    return count;
}
static int proc_property_read_mode(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    len += sprintf(page+len, "usage: echo [chn] > /proc/%s/property\n", H264E_PROC_DIR);
    if (property_minor < h264e_max_chn)
        len += dump_property_value(page+len, property_minor);
    else {
        len += sprintf(page+len, "channel %d over max channel number %d\n", property_minor, h264e_max_chn);
    }
    *eof = 1;
    return len;
}

static int proc_job_write_mode(struct file *file, const char *buffer,unsigned long count, void *data)
{
    char str[0x10];
    unsigned long len = count;

    if (len >= sizeof(str))
        len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;

    sscanf(str, "%d", &job_minor);
    return count;
}
static int proc_job_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;
    len += sprintf(page+len, "usage: echo [chn] > /proc/%s/job ([chn] = 999: means dump all job)\n", H264E_PROC_DIR);
    len += sprintf(page+len, "current [chn] = %d\n", job_minor);
    len += dump_job_info(page+len, job_minor);
    *eof = 1;
    return len;
}

#ifdef PROC_SEQ_OUTPUT
static void *proc_chn_info_start(struct seq_file *m, loff_t *pos)
{
    //loff_t n = *pos;
    int chn = (int)(*pos);
    if (0 == chn) {
        seq_printf(m, " chn    resolution   buf.type  gop   mode         fps         bitrate    max.br    init.q  min.q  max.q  qp\n");
        seq_printf(m, "=====  ============  ========  ===  ======  ===============  =========  =========  ======  =====  =====  ==\n");
    }
    if (chn < h264e_max_chn) {
        int chn_p1 = chn+1;
        return (void *)chn_p1;
    }
    return 0;
}
static int proc_chn_info_show(struct seq_file *m, void *p)
{
    int chn_p1 = (int)p;
    dump_one_chn_info(m, chn_p1-1);
    return 0;
}
static void *proc_chn_info_next(struct seq_file *m, void *p, loff_t *pos)
{
    int chn = (int)p;
    (*pos)++;
    if (chn+1 < h264e_max_chn) 
        return (void *)(chn+1);
    return 0;
}
static void proc_chn_info_stop(struct seq_file *m, void *p)
{
}
static struct seq_operations proc_chn_info_seq_ops = {
    .start = proc_chn_info_start,
    .next  = proc_chn_info_next,
    .stop  = proc_chn_info_stop,
    .show  = proc_chn_info_show,
};
static int proc_chn_info_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &proc_chn_info_seq_ops);
}
static struct file_operations proc_chn_info_operations = {
        .open = proc_chn_info_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};
#endif

static int proc_parm_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0, i = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;

    len += sprintf(page+len, "usage: echo [parameter name] [value(dec)] > /proc/videograph/h264e/param\n");
    len += sprintf(page+len, "default setting, cur = %d (%d: middle, %d: perf, %d: quality, %d: user defintion)\n", 
        default_configure, CFG_MIDDLE, CFG_PERFORMANCE, CFG_QUALITY, CFG_USER);
    //len += sprintf(page+len, "======= parameter setting =======\n");
    len += sprintf(page+len, "    parameter name        value                note\n");
    len += sprintf(page+len, "=======================  =======  ==============================\n");
    while (syntax[i].tokenName) {
        len += sprintf(page+len, "%-25s  ", syntax[i].tokenName);
        switch (syntax[i].type) {
            case UINT32:
                u32Val = (unsigned int*)syntax[i].value;
                len += sprintf(page+len, "%4u", *u32Val);
                break;
            case INT32:
                i32Val = (int*)syntax[i].value;
                len += sprintf(page+len, "%4d", *i32Val);
                break;
            case UINT8:
                u8Val = (unsigned char*)syntax[i].value;
                len += sprintf(page+len, "%4u", *u8Val);
                break;
            case HEX8:
                u8Val = (unsigned char*)syntax[i].value;
                len += sprintf(page+len, "0x%02x", *u8Val);
                break;
            case INT8:
                i8Val = (char *)syntax[i].value;
                len += sprintf(page+len, "%4d", *i8Val);
                break;
            default:
                break;
        }
        if (syntax[i].note)
            len += sprintf(page+len, "    %s", syntax[i].note);
        len += sprintf(page+len, "\n");
        i++;
    }
    *eof = 1;
    return len;
}
static int proc_parm_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    // input parameter
    int len = count;
    int value;
    char cmd_str[40];
    char str[80];
    int idx = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;
    int change_variable = 0;

    if (len >= sizeof(str))
       len = sizeof(str) - 1;
    
    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%s %d\n", cmd_str, &value);

    while (syntax[idx].tokenName) {
        if (strcmp(syntax[idx].tokenName, cmd_str) == 0)
            break;
        idx++;
    }
    if (syntax[idx].tokenName) {
        if (value < syntax[idx].lb || value > syntax[idx].ub)
            printk("%s(%d) is out of range! (%d ~ %d)\n", syntax[idx].tokenName, value, syntax[idx].lb, syntax[idx].ub);
        else {
            switch (syntax[idx].type) {
                case UINT32:
                    u32Val = (unsigned int *)syntax[idx].value;
                    if (*u32Val != value)
                        change_variable = 1;
                    *u32Val = value;
                    break;
                case INT32:
                    i32Val = (int *)syntax[idx].value;
                    if (*i32Val != value)
                        change_variable = 1;
                    *i32Val = value;
                    break;
                case UINT8:
                case HEX8:
                    u8Val = (unsigned char *)syntax[idx].value;
                    if (*u8Val != value)
                        change_variable = 1;
                    *u8Val = value;
                    break;
                case INT8:
                    i8Val = (char *)syntax[idx].value;
                    if (*i8Val != value)
                        change_variable = 1;
                    *i8Val = value;
                    break;
                default:
                    break;
            }
        }
    }
    else
        printk("unknown \"%s\"", cmd_str);
    if (change_variable) {
        int i;
        for (i = 0; i < h264e_max_chn; i++)
            procForceUpdate[i] = 1;
    }
    if (0 == idx) {
        // set default configure value
        set_configure_value(&gEncParam);
    }
    
    return count;
}

static int proc_didn_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0, i = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;

    if (gDidnMode < 0)
        len += sprintf(page+len, "DiDn mode = -1");
    else        
        len += sprintf(page+len, "DiDn mode = 0x%x", gDidnMode);
    len += sprintf(page+len, " (Bit0: sp di, Bit1: tp di, Bit2: sp dn, Bit3: tp dn)\n");
    //len += sprintf(page+len, "===== didn parameter setting =====\n");
    len += sprintf(page+len, "   didn parameter    value\n");
    len += sprintf(page+len, "===================  =====\n");
    while (didn_syntax[i].tokenName) {
        len += sprintf(page+len, "%-19s   ", didn_syntax[i].tokenName);
        switch (didn_syntax[i].type) {
            case UINT32:
                u32Val = (unsigned int*)didn_syntax[i].value;
                len += sprintf(page+len, "%u\n", *u32Val);
                break;
            case INT32:
                i32Val = (int*)didn_syntax[i].value;
                len += sprintf(page+len, "%d\n", *i32Val);
                break;
            case UINT8:
                u8Val = (unsigned char*)didn_syntax[i].value;
                len += sprintf(page+len, "%u\n", *u8Val);
                break;
            default:
                len += sprintf(page+len, "\n");
                break;
        }
        i++;
    }
    *eof = 1;
    return len;
}
static int proc_didn_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    // input parameter
    int len = count;
    int value;
    char cmd_str[40];
    char str[80];
    int idx = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    int change_variable = 0;

    if (len >= sizeof(str))
       len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%s %d\n", cmd_str, &value);

    while (didn_syntax[idx].tokenName) {
        if (strcmp(didn_syntax[idx].tokenName, cmd_str) == 0)
            break;
        idx++;
    }
    if (didn_syntax[idx].tokenName) {
        if (value < didn_syntax[idx].lb || value > didn_syntax[idx].ub)
            printk("%s(%d) is out of range! (%d ~ %d)\n", didn_syntax[idx].tokenName, value, didn_syntax[idx].lb, didn_syntax[idx].ub);
        else {
            switch (didn_syntax[idx].type) {
                case UINT32:
                    u32Val = (unsigned int *)didn_syntax[idx].value;
                    if (*u32Val != value)
                        change_variable = 1;
                    *u32Val = value;
                    break;
                case INT32:
                    i32Val = (int *)didn_syntax[idx].value;
                    if (*i32Val != value)
                        change_variable = 1;
                    *i32Val = value;
                    break;
                case UINT8:
                    u8Val = (unsigned char *)didn_syntax[idx].value;
                    if (*u8Val != value)
                        change_variable = 1;
                    *u8Val = value;
                    break;
                default:
                    break;
            }
        }
    }
    else
        printk("unknown \"%s\"", cmd_str);
    if (change_variable) {
        int i;
        for (i = 0; i < h264e_max_chn; i++)
            procForceUpdate[i] = 1;
            //gDiDnParam.bUpdateDiDnParam[i] = 1;
        gDiDnParam.bUpdateDiDnParam = 1;
    }
    
    return count;
}

static int proc_qmatrix_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0, idx, i;
    char matrix_name[8][0x20] = {"INTRA4X4 Y", "INTRA4X4 Cb", "INTRA4X4 Cr", 
        "INTER4X4 Y", "INTER4X4 Cb", "INTER4X4 Cr", "INTRA8X8 Y", "INTER8X8 Y"};

    len += sprintf(page+len, "usage: echo [matrix idx] [idx] [value] > /proc/%s/q_matrix\n", H264E_PROC_DIR);
    len += sprintf(page+len, "\tdisable/enable qmatrix: echo [matrix idx] -1 [0/1] > /proc/%s/q_matrix\n", H264E_PROC_DIR);
    
    if (0 == gEncParam.bScalingMatrixFlag)
        len += sprintf(page+len, "Qmatrix: disable\n");
    else {
        len += sprintf(page+len, "Qmatrix:\n");
        for (idx = 0; idx < 8; idx++){
            if (gScalingList.bScalingListFlag[idx]) {
                len += sprintf(page+len, "%d. %s:\n\t", idx, matrix_name[idx]);
                if (idx < 6) {
                    for (i = 0; i < 16; i++) {
                        len += sprintf(page+len, "%2d,", gScalingList.i32ScaleList4x4[idx][i]);
                        if ((i%4) == 3)
                            len += sprintf(page+len, "\n\t");
                    }
                    len += sprintf(page+len, "\n");
                }
                else {
                    for (i = 0; i < 64; i++) {
                        len += sprintf(page+len, "%2d,", gScalingList.i32ScaleList8x8[idx-6][i]);
                        if ((i%8) == 7)
                            len += sprintf(page+len, "\n\t");
                    }
                    len += sprintf(page+len, "\n");
                }
            }
            else {
                len += sprintf(page+len, "%d. %s: disable\n", idx, matrix_name[idx]);
            }
        }
        len += sprintf(page+len, "\n");
    }

    *eof = 1;
    return len;
}
static int proc_qmatrix_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    // input parameter
    int len = count;
    char str[80];
    int matrix_idx, idx, value;

    if (len >= sizeof(str))
       len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%d %d %d\n", &matrix_idx, &idx, &value);

    if (matrix_idx < 0 || matrix_idx >= 8) {
        printk("matrix idx(%d) out of range (0~7)\n", matrix_idx);
        goto qmatrix_write_fail;
    }

    if (idx < 0) {
        goto qmatrix_write_fail;
        //gScalingList.bScalingListFlag[matrix_idx] = (0 == value? 0: 1);
    }
    else {  // idx >= 0
        if (matrix_idx < 6) {
            if (idx >= 16) {
                printk("matrix[%d] idx(%d) out of range (0~15)\n", matrix_idx, idx);
                goto qmatrix_write_fail;
            }
            gScalingList.i32ScaleList4x4[matrix_idx][idx] = value;
        }
        else {
            if (idx >= 64) {
                printk("matrix[%d] idx(%d) out of range (0~63)\n", matrix_idx, idx);
                goto qmatrix_write_fail;
            }
			// Tuba 20140206 fix bug: scaling matrix 8x8 wrong index
            gScalingList.i32ScaleList8x8[matrix_idx-6][idx] = value;
        }
    }
    for (idx = 0; idx < h264e_max_chn; idx++)
        procForceUpdate[idx] = 1;

    return count;
qmatrix_write_fail:
    printk("usage: echo [matrix idx] [idx] [value] > /proc/%s/q_matrix\n", H264E_PROC_DIR);
    printk("\tdisable/enable qmatrix: echo [matrix idx] -1 [0/1] > /proc/%s/q_matrix\n", H264E_PROC_DIR);
    return count;
}


#ifdef MCNR_ENABLE
static int proc_mcnr_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0, i;

    len += sprintf(page+len, "usage: echo [H/L] [idx] [value] > /proc/%s/mcnr\n", H264E_PROC_DIR);
    len += sprintf(page+len, "MCNR LSAD threshold table:\n\t");
    for (i = 0; i < 52; i++) {
        len += sprintf(page+len, "%2d,", gEncParam.mMCNRTable.u8LSAD[i]);
        if ((i%10) == 9)
            len += sprintf(page+len, "\n\t");
    }
    len += sprintf(page+len, "\n");
    len += sprintf(page+len, "MCNR HSAD threshold table:\n\t");
    for (i = 0; i < 52; i++) {
        len += sprintf(page+len, "%2d,", gEncParam.mMCNRTable.u8HSAD[i]);
        if ((i%10) == 9)
            len += sprintf(page+len, "\n\t");
    }
    len += sprintf(page+len, "\n");

    *eof = 1;
    return len;
}
static int proc_mcnr_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    // input parameter
    int len = count;
    char str[80];
    char hl_cmd;
    int idx, value;

    if (len >= sizeof(str))
       len = sizeof(str) - 1;

    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%c %d %d\n", &hl_cmd, &idx, &value);

    if (idx < 0 || idx > 51) {
        printk("idx(%d) out of range (0~51)\n", idx);
        goto mcnr_write_fail;
    }
    if (value < 0 || value > 255) {
        printk("value(%d) out of range (0~255)\n", value);
        goto mcnr_write_fail;
    }
    if ('H' == hl_cmd)
        gEncParam.mMCNRTable.u8LSAD[idx] = value;
    else if ('L' == hl_cmd)
        gEncParam.mMCNRTable.u8HSAD[idx] = value;
    else {
        printk("unknown syntax \"%c\"\n", hl_cmd);
        goto mcnr_write_fail;
    }

    return count;
mcnr_write_fail:
    printk("usgae: usage: echo [H/L] [idx] [value] > /proc/%s/mcnr\n", H264E_PROC_DIR);
    return count;
}
#endif

static int proc_vg_info_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

    len += sprintf(page+len, "#H264 encoder information:\n");
    len += sprintf(page+len, "B_FRAME_NR: %d\n", h264e_max_b_frame);
#ifdef GM8210
    len += sprintf(page+len, "ENGINE_NR: 2\n");
#else
    len += sprintf(page+len, "ENGINE_NR: 1\n");
#endif

    *eof = 1;
    return len;
}
static int proc_vg_info_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    return count;
}

#ifdef TWO_ENGINE
static int proc_chn_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;
    int i, j;

    len += sprintf(page+len, "assign channel and engine map\n");
    len += sprintf(page+len, "usage: \t# echo [chn] [engine] > /proc/%s/chn\n", H264E_PROC_DIR);
    len += sprintf(page+len, " chn    0 1 2 3 4 5 6 7 8 9\n");
    len += sprintf(page+len, "=====  =====================\n");
    for (i = 0; i < h264e_max_chn; i+=10) {
        len += sprintf(page+len, " %3d    ", i);
        for (j = 0; j < 10 && i+j < h264e_max_chn; j++) {
            if (chn_2_engine[i+j] >= 0)
                len += sprintf(page+len, "%d ", chn_2_engine[i+j]);
            else
                len += sprintf(page+len, "- ");
        }
        len += sprintf(page+len,"\n");
    }
#ifdef DYNAMIC_ENGINE
    len += sprintf(page+len, "engine0 cost %d, engine1 cost %d\n", engine_cost[0], engine_cost[1]);
#endif

    *eof = 1;
    return len;
}
static int proc_chn_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int chn, engine;
    char str[80];
    int len = count;
    
#ifdef DYNAMIC_ENGINE
    if (0 == engine_cost[0] && 0 == engine_cost[1]) // must when engine is idle
#endif
    {
        if (len >= sizeof(str))
           len = sizeof(str) - 1;
        
        if (copy_from_user(str, buffer, len))
            return 0;
        str[len] = '\0';
        sscanf(str, "%d %d", &chn, &engine);
        if (-1 == engine) { // reset all channel
            for (chn = 0; chn < h264e_max_chn; chn++) {
                if (chn & 0x02)
                    chn_2_engine[chn] = 0x01;
                else
                    chn_2_engine[chn] = 0x00;
            }
            return count;
        }
    	if (engine < 0 || engine >= MAX_ENGINE_NUM) {
    		printk("no such engine (%d)\n", engine);
    		return count;
    	}
    	if (-1 == chn) {
    		for (chn = 0; chn < h264e_max_chn; chn++)
    			chn_2_engine[chn] = engine;
    	}
    	else if (chn >= 0 && chn < h264e_max_chn) {
            if (0 == engine)
                chn_2_engine[chn] = 0;
            else if (1 == engine)
                chn_2_engine[chn] = 1;
        }
    }
    return count;
}
#endif  // TWO_ENGINE

#endif  // VG_INTERFACE

#ifdef REF_POOL_MANAGEMENT
static int ref_info_flag = 0;
static int proc_ref_info_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

#ifdef PROC_SEQ_OUTPUT
    len += sprintf(page + len, "dump ref buffer flag = %d (0: dump pool number, 1: dump ref pool, 2: dump chn pool)\n", ref_info_flag);
    switch (ref_info_flag) {
    case 0:
        len += dump_pool_num(page+len);
        break;
    case 1: 
        len += dump_ref_pool(page + len);
        break;
    case 2:
        len += dump_chn_pool(page + len);
        break;
    }
#else
    len += sprintf(page + len, "dump ref buffer flag = %d (0: dump channel info, 1: dump pool number, 2: dump ref pool, 3: dump chn pool)\n", ref_info_flag);
    switch (ref_info_flag) {
    case 0:
        len += dump_chn_buffer(page+len);
        break;
    case 1:
        len += dump_pool_num(page+len);
        break;
    case 2: 
        len += dump_ref_pool(page + len);
        break;
    case 3:
        len += dump_chn_pool(page + len);
        break;
    }
#endif
    *eof = 1;
    return len;
}

static int proc_ref_info_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len = count;
    char str[80];

    if (len >= sizeof(str))
       len = sizeof(str) - 1;
    
    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%d", &ref_info_flag);
    return count;
}
#endif
static int proc_dbg_read_mode(char *page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

    len += sprintf(page+len, "usage:\n");
    len += sprintf(page+len, "id  value               comment\n");
    len += sprintf(page+len, "=== ===== =====================================================================\n");
#ifdef VG_INTERFACE
    len += sprintf(page+len, "  1  %3d   default profile setting, BaseLine: %d, Main: %d, High: %d\n", gEncParam.u32Profile, BASELINE, MAIN, FREXT_HP);
    len += sprintf(page+len, " 10  %3d   default setting, %d: middle, %d: perf, %d: quality, %d: user defintion\n",
            default_configure, CFG_MIDDLE, CFG_PERFORMANCE, CFG_QUALITY, CFG_USER);
    len += sprintf(page+len, " 12  %3d   enable/disable adaptive search range by resolution\n", adapt_search_range);
    len += sprintf(page+len, " 20  %3d   force qp, when no rc\n", forceQP);
    #ifdef FIX_TWO_ENGINE
    len += sprintf(page+len, " 21  %3d   force engine\n", procAssignEngine);
    #endif
    len += sprintf(page+len, " 30  ---   force update\n");
    len += sprintf(page+len, " 32  %3d   force direct dump to console\n", procForceDirectDump);
#ifdef CHECK_ENGINE_BALANCE
    len += sprintf(page+len, " 33  %3d   HW utilization busy threshold\n", gHWUtlThd);
    len += sprintf(page+len, " 34  %3d   HW utilization threshold of difference\n", gHWUtlDiffThd);
    len += sprintf(page+len, " 35  %3d   maximal number of force balance (cur %d)\n", gForceBalanceMaxNum, gUnbalanceCounter);
#endif
    len += sprintf(page+len, " 36  %3d   dump register (bit0: param, bit 1: cmd, bit2: mem, bit3: vcache)\n", force_dump_register); 
    len += sprintf(page+len, " 40  %3d   dump_bitrate\n", dump_bitrate);
#endif
#ifdef OVERFLOW_RETURN_VALID_VALUE
    len += sprintf(page+len, " 41  %3d   overflow return valid length\n", gOverflowReturnLength);
#endif
    len += sprintf(page+len, " 42  %3d   HW time out\n", gTimeoutThreshold);
#ifdef REF_POOL_MANAGEMENT
    len += sprintf(page+len, " 50  %3d   realloc limit\n", realloc_cnt_limit);
    len += sprintf(page+len, " 51  %3d   over spec limit\n", over_spec_limit);
#endif
#ifdef REORDER_JOB
    len += sprintf(page+len, " 52  %3d   reallocate sleep time\n", sleep_time);
#endif
#ifdef OVERFLOW_REENCODE
    len += sprintf(page+len, " 53  %3d   bitrate factor of QP (base 256)\n", g_bitrate_factor);
#endif
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	len += sprintf(page+len, " 54  %3d   reset register when update parameter\n", update_param_reset_register);
#endif
#ifdef ENABLE_MB_RC
    len += sprintf(page+len, " 55  %3d   mb layer rc, max delta qp\n", gMBQPLimit);
    len += sprintf(page+len, " 56  %3d   mb layer rc, basic unit (mb row)\n", gMBRCBasicUnit);
#endif
#ifdef DYNAMIC_GOP_ENABLE
    len += sprintf(page+len, " 57  %3d   dynamic gop enable\n", h264e_dynamic_gop);
    len += sprintf(page+len, " 58  %3d   extend gop factor\n", gop_factor);
    len += sprintf(page+len, " 59  %3d   static gop measure\n", measure_static_event);
    len += sprintf(page+len, " 60  %3d   i frame disable roi region\n", iframe_disable_roi);
#endif
#ifdef RECORD_BS
    len += sprintf(page+len, "100  %3d   record chn\n", record_chn);
    len += sprintf(page+len, "101  %3d   record bitstream size\n", dump_size);
    len += sprintf(page+len, "102  ---   flush record bitstream\n");
#endif

    *eof = 1;
    return len;
}

static int proc_dbg_write_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len = count;
    int cmd_idx, value;    
    char str[80];
    int i;
    if (len >= sizeof(str))
        len = sizeof(str) - 1;
    if (copy_from_user(str, buffer, len))
        return 0;
    str[len] = '\0';
    sscanf(str, "%d %d\n", &cmd_idx, &value);
    switch (cmd_idx) {
		// can not remove because some customer use this proc
    #ifdef VG_INTERFACE
        case 1:
            if (BASELINE == value || MAIN == value || value == FREXT_HP) {
                if (gEncParam.u32Profile != value) {
                    for (i = 0; i< h264e_max_chn; i++)
                        procForceUpdate[i] = 1;
                }
                gEncParam.u32Profile = value;
            }
            break;
        case 10:
            if (value >= CFG_MIDDLE && value <= CFG_USER) {
                if (default_configure != value) {
                    for (i = 0; i< h264e_max_chn; i++)
                        procForceUpdate[i] = 1;
                }
                default_configure = value;
                //printk("current setting: %d\n", default_configure);
            }
            break;
        case 12:
            if (1 == value)
                adapt_search_range = 1;
            else
                adapt_search_range = 0;
            break;
        case 20:
            if (value < 0)
                forceQP = -1;
            else if (value >= 1 && value <= 51)
                forceQP = value;
            break;
        #ifdef FIX_TWO_ENGINE
        case 21:
            //printk("force engine %d\n", value);
            if (1 == value)
                procAssignEngine = 1;
            else
                procAssignEngine = 0;
            break;
        #endif
        case 30:   // force update
            for (i = 0; i < h264e_max_chn; i++)
                procForceUpdate[i] = 1;
            printk("proc force update!\n");
            break;
        case 32:
            if (1 == value)
                procForceDirectDump = 1;
            else
                procForceDirectDump = 0;
            break;
    #ifdef CHECK_ENGINE_BALANCE
        case 33:
            if (value >= 0 && value <= 100)
                gHWUtlThd = value;
            break;
        case 34:
            if (value >= 0 && value <= 100)
                gHWUtlDiffThd = value;
            break;
        case 35:
            if (value > 0)
                gForceBalanceMaxNum = value;
            else 
                gForceBalanceMaxNum = 0;
            break;
    #endif
        case 36:
            force_dump_register = value;
            break;
        case 40:
            if (1 == value)
                dump_bitrate = value;
            else
                dump_bitrate = 0;
            break;
    #endif
    #ifdef OVERFLOW_RETURN_VALID_VALUE
        case 41:
            if (value > 0)
                gOverflowReturnLength = value;
            else
                gOverflowReturnLength = 0;
            break;
    #endif
        case 42:
            if (value > 0)
                gTimeoutThreshold = value;
            else
                gTimeoutThreshold = TIMEOUT_THRESHOLD;
            break;
    #ifdef REF_POOL_MANAGEMENT
        case 50:
            if (value <= 0)
                realloc_cnt_limit = -1;
            else
                realloc_cnt_limit = value;
            break;
        case 51:
            if (value <= 0)
                over_spec_limit = -1;
            else
                over_spec_limit = value;
            break;
    #endif
    #ifdef REORDER_JOB
        case 52:
            if (value > 0)
                sleep_time = value;
            else
                sleep_time = 0;
            break;
    #endif
    #ifdef OVERFLOW_REENCODE
        case 53:
            if (value > 0 && value < 256)
                g_bitrate_factor = value;
            break;
    #endif
	#ifdef NO_RESET_REGISTER_PARAM_UPDATE
		case 54:
			if (1 == value)
				update_param_reset_register = 1;
			else
				update_param_reset_register = 0;
			break;
	#endif
    #ifdef ENABLE_MB_RC
        case 55:
            if (value >= 0 && value <= 12)
                gMBQPLimit = value;
            break;
        case 56:
            if (value > 0)
                gMBRCBasicUnit = value;
            else
                gMBRCBasicUnit = 1;
            break;
    #endif
    #ifdef DYNAMIC_GOP_ENABLE
        case 57:
            if (value > 0 && value < 3)
                h264e_dynamic_gop = value;
            else
                h264e_dynamic_gop = 0;
            break;
        case 58:
            if (value > 0)
                gop_factor = value;
            break;
        case 59:
            if (value > 0)
                measure_static_event = value;
            break;
        case 60:
            if (1 == value)
                iframe_disable_roi = 1;
            else
                iframe_disable_roi = 0;
            break;
    #endif
    #ifdef RECORD_BS
        case 100:
            if (value >= 0 && value < h264e_max_chn) {
                record_chn = value;
                rec_size[0] = rec_size[1] = 0;
                printk("record chn %d\n", record_chn);
            }
            else if (0xFF == value) {
                record_chn = value;
                printk("record all channel\n");
            }
            else
                record_chn = -1;
            break;
        case 101:
            if (value <= 0)
                dump_size = 0;
            else
                dump_size = value;
            printk("dump bitstream size %d\n", dump_size);
            break;
        case 102:
            dump_log_busy[rec_idx] = 1;
            save_idx = rec_idx;
            rec_idx ^= 0x01;
            PREPARE_DELAYED_WORK(&process_log_work, (void *)En_log_File);
            queue_delayed_work(favc_enc_log_wq, &process_log_work, 1);
            printk("flush buffer info\n");
            break;
    #endif  // RECORD_BS
        default:
            break;
    }

    return count;
}

void favce_proc_close(void)
{
#ifdef USE_CONFIG_FILE
    if (config_proc)
        remove_proc_entry(config_proc->name, h264e_entry_proc);
#endif
#ifdef PROC_SEQ_OUTPUT
    if (chn_info_proc)
        remove_proc_entry(chn_info_proc->name, h264e_entry_proc);
#endif
#ifdef REF_POOL_MANAGEMENT
    if (ref_info_proc)
        remove_proc_entry(ref_info_proc->name, h264e_entry_proc);
#endif
    if (dbg_proc)
        remove_proc_entry(dbg_proc->name, h264e_entry_proc);
#ifdef VG_INTERFACE
    #ifdef TWO_ENGINE
        if (chn_proc)
            remove_proc_entry(chn_proc->name, h264e_entry_proc);
    #endif
    if (vg_info_proc)
        remove_proc_entry(vg_info_proc->name, h264e_entry_proc);
    if (qmatrix_proc)
        remove_proc_entry(qmatrix_proc->name, h264e_entry_proc);
    #ifdef MCNR_ENABLE
        if (mcnr_proc)
            remove_proc_entry(mcnr_proc->name, h264e_entry_proc);
    #endif
    if (didn_proc)
        remove_proc_entry(didn_proc->name, h264e_entry_proc);
    if (parm_proc)
        remove_proc_entry(parm_proc->name, h264e_entry_proc);
    if (job_proc)
        remove_proc_entry(job_proc->name, h264e_entry_proc);
    if (property_proc)
        remove_proc_entry(property_proc->name, h264e_entry_proc);
    if (util_proc)
        remove_proc_entry(util_proc->name, h264e_entry_proc);
    if (cb_proc)
        remove_proc_entry(cb_proc->name, h264e_entry_proc);
#endif
    if (info_proc)
        remove_proc_entry(info_proc->name, h264e_entry_proc);
    if (loglevel_proc)
        remove_proc_entry(loglevel_proc->name, h264e_entry_proc);
#ifdef MEM_CNT_LOG
    if (mem_cnt_proc)
        remove_proc_entry(mem_cnt_proc->name, h264e_entry_proc);
#endif
    if (h264e_entry_proc)
        remove_proc_entry(H264E_PROC_DIR, NULL);
#ifdef RECORD_BS
    if (favc_enc_log_wq)
        destroy_workqueue(favc_enc_log_wq);
#endif
    //free_buffer(&rec_buffer);
}

int favce_proc_init(void)
{
    h264e_entry_proc = create_proc_entry(H264E_PROC_DIR, S_IFDIR | S_IRUGO | S_IXUGO, NULL);
    if (NULL == h264e_entry_proc) {
        printk("Error to create %s proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }

    loglevel_proc = create_proc_entry("level", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == loglevel_proc) {
        printk("Error to create %s/level proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    loglevel_proc->read_proc = (read_proc_t *)proc_log_level_read_mode;
    loglevel_proc->write_proc = (write_proc_t *)proc_log_level_write_mode;
    //loglevel_proc->owner = THIS_MODULE;

    info_proc = create_proc_entry("info", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == info_proc) {
        printk("Error to create %s/info proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    info_proc->read_proc = (read_proc_t *)proc_info_read_mode;
    info_proc->write_proc = (write_proc_t *)proc_info_write_mode;
    //info_proc->woner = THIS_MODULE;
#ifdef VG_INTERFACE
    cb_proc = create_proc_entry("callback_period", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (cb_proc == NULL) {
        printk("Error to create %s/callback_period proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    cb_proc->read_proc = (read_proc_t *)proc_cb_read_mode;
    cb_proc->write_proc = (write_proc_t *)proc_cb_write_mode;
    
    util_proc = create_proc_entry("utilization", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (util_proc == NULL) {
        printk("Error to create %s/utilization proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    util_proc->read_proc = (read_proc_t *)proc_util_read_mode;
    util_proc->write_proc = (write_proc_t *)proc_util_write_mode;

    
    property_proc = create_proc_entry("property", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (property_proc == NULL) {
        printk("Error to create %s/property proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    property_proc->read_proc = (read_proc_t *)proc_property_read_mode;
    property_proc->write_proc = (write_proc_t *)proc_property_write_mode;

    job_proc = create_proc_entry("job", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (job_proc == NULL) {
        printk("Error to create %s/job proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    job_proc->read_proc = (read_proc_t *)proc_job_read_mode;
    job_proc->write_proc = (write_proc_t *)proc_job_write_mode;

    parm_proc = create_proc_entry("param", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (parm_proc == NULL) {
        printk("Error to create %s/param proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    parm_proc->read_proc = (read_proc_t *)proc_parm_read_mode;
    parm_proc->write_proc = (write_proc_t *)proc_parm_write_mode;

    didn_proc = create_proc_entry("didn", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (didn_proc == NULL) {
        printk("Error to create %s/didn proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    didn_proc->read_proc = (read_proc_t *)proc_didn_read_mode;
    didn_proc->write_proc = (write_proc_t *)proc_didn_write_mode;

    #ifdef MCNR_ENABLE
        mcnr_proc = create_proc_entry("mcnr", S_IRUGO | S_IXUGO, h264e_entry_proc);
        if (NULL == mcnr_proc) {
            printk("Error to create %s/mcnr proc\n", H264E_PROC_DIR);
            goto fail_init_proc;
        }
        mcnr_proc->read_proc = (read_proc_t *)proc_mcnr_read_mode;
        mcnr_proc->write_proc = (write_proc_t *)proc_mcnr_write_mode;
    #endif

    qmatrix_proc = create_proc_entry("q_matrix", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == qmatrix_proc) {
        printk("Error to create %s/q_matrix proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    qmatrix_proc->read_proc = (read_proc_t *)proc_qmatrix_read_mode;
    qmatrix_proc->write_proc = (write_proc_t *)proc_qmatrix_write_mode;

    vg_info_proc = create_proc_entry("vg_info", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == vg_info_proc) {
        printk("Error to create %s/vg_info proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    vg_info_proc->read_proc = (read_proc_t *)proc_vg_info_read_mode;
    vg_info_proc->write_proc = (write_proc_t *)proc_vg_info_write_mode;

    #ifdef TWO_ENGINE
        chn_proc = create_proc_entry("chn", S_IRUGO | S_IXUGO, h264e_entry_proc);
        if (NULL == chn_proc) {
            printk("Error to create %s/chn proc\n", H264E_PROC_DIR);
            goto fail_init_proc;
        }
        chn_proc->read_proc = (read_proc_t *)proc_chn_read_mode;
        chn_proc->write_proc = (write_proc_t *)proc_chn_write_mode;
    #endif

    #ifdef USE_CONFIG_FILE
        reset_default_config();
        if (h264e_user_config) {
            if (parsing_config_file() >= 0)
                default_configure = CFG_FILE;
            else
                default_configure = CFG_PERFORMANCE;
        }
        else {
            // set default_configure
            default_configure = CFG_PERFORMANCE;
            set_default_enc_param(&gEncParam, NULL, NULL);
        #ifdef ENABLE_USER_CONFIG
            memcpy(&gEncConfigure[1].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
            default_configure = CFG_MIDDLE;
            set_default_enc_param(&gEncParam, NULL, NULL);
            memcpy(&gEncConfigure[2].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
            default_configure = CFG_QUALITY;
            set_default_enc_param(&gEncParam, NULL, NULL);
            memcpy(&gEncConfigure[3].enc_param, &gEncParam, sizeof(FAVC_ENC_PARAM));
            default_configure = CFG_PERFORMANCE;
        #endif
        }
    #endif
    set_default_enc_param(&gEncParam, &gDiDnParam, &gScalingList);
#endif

#ifdef REF_POOL_MANAGEMENT
    ref_info_proc = create_proc_entry("ref_info", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == ref_info_proc) {
        printk("Error to create %s/ref_info proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    ref_info_proc->read_proc = (read_proc_t *)proc_ref_info_read_mode;
    ref_info_proc->write_proc = (write_proc_t *)proc_ref_info_write_mode;
#endif

#ifdef MEM_CNT_LOG
    mem_cnt_proc = create_proc_entry("mem_cnt", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (mem_cnt_proc == NULL) {
        printk("Error to create %s/mem_cnt proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    mem_cnt_proc->read_proc = (read_proc_t *)proc_mem_cnt_read;
    mem_cnt_proc->write_proc = (write_proc_t *)proc_mem_cnt_write;
#endif

    dbg_proc = create_proc_entry("dbg", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (dbg_proc == NULL) {
        printk("Error to create %s/dbg proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    dbg_proc->read_proc = (read_proc_t *)proc_dbg_read_mode;
    dbg_proc->write_proc = (write_proc_t *)proc_dbg_write_mode;

#ifdef PROC_SEQ_OUTPUT
    chn_info_proc = create_proc_entry("chn_info", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (NULL == chn_info_proc) {
        printk("Error to create %s/chn_info\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    #ifdef PROC_SEQ_OUTPUT
    chn_info_proc->proc_fops = &proc_chn_info_operations;
    #endif
#endif

#ifdef USE_CONFIG_FILE
    config_proc = create_proc_entry("config", S_IRUGO | S_IXUGO, h264e_entry_proc);
    if (config_proc == NULL) {
        printk("Error to create %s/config proc\n", H264E_PROC_DIR);
        goto fail_init_proc;
    }
    #ifdef PROC_SEQ_OUTPUT
    config_proc->proc_fops = &proc_config_operations;
    #else
    config_proc->read_proc = (read_proc_t *)proc_config_read_mode;
    config_proc->write_proc = (write_proc_t *)proc_config_write_mode;
    #endif
#endif

#ifdef RECORD_BS
    favc_enc_log_wq = create_workqueue("favc_enc_log");
    if (!favc_enc_log_wq) {
        printk("Error to create log workqueue\n");
        goto fail_init_proc;
    }
    INIT_DELAYED_WORK(&process_log_work, 0);
#endif

    return 0;

fail_init_proc:
    favce_proc_close();
    return -EFAULT;
}


