#ifndef _H264V_ENC_H_
#define _H264V_ENC_H_

#include "../h264e_ioctl.h"
#ifdef RC_ENABLE
#include "favce_param.h"

typedef int (*RC_GET_QUANT_ptr)(void *ptHandler, struct rc_frame_info_t *rc_data);
typedef int (*RC_UPDTAE_ptr)(void *ptHandler, struct rc_frame_info_t *rc_data);
#endif

typedef struct h264_didn_param_t
{
    // control DiDn
    unsigned char bUpdateDiDnParam;

    // spatial de-noise
    unsigned char u8SpDnLineDetStr;         // spatial de-noise line detection strength
    /* line detection level: 0~2, if larger -> easy to detect line -> denoise less */
    unsigned char u8EdgeStrength;           // edge detection strength
    /* edge strength threshold: if larger -> edge condiction hard -> denoise more */
	unsigned char u8DnMinFilterLevel;       // spatial de-noise filter adjust parameter min. value
	unsigned char u8DnMaxFilterLevel;       // spatial de-noise filter adjust parameter max. value
	/* normal edge strength */
	unsigned char bSeparateHrVtDn;          // enable separate horizontal/vertical spatial de-noise
	                                        // 0: square, 1: adaptive
	unsigned char u8SpDnThd;	            // separate horz/vert spatial de-noise threshold
	/* Hz Vt sparate denoise condiction */

    // temporal de-noise
	unsigned char u8DnVarY;                 // de-noise variance for Y
	unsigned char u8DnVarCb;                // de-noise variance for Cb
	unsigned char u8DnVarCr;                // de-noise variance for Cr
	unsigned int  u32DnVarPixelUpperbound;	// frame adaptive de-noise variance pixel upper bound threshold
                                            // pixel large than this value
	unsigned int  u32DnVarPixelLowerbound;	// frame adaptive de-noise variance pixel lower bound threshold
                                            // pixel large than this value and lower than upbound
    unsigned char u8DnVarLumaMBThd;         // frame adaptive de-noise variance Luma MB threshold
    unsigned char u8DnVarChromaMBThd;       // frame adaptive de-noise variance Chroma MB threshold

    // spatial de-interlace
    unsigned char u8ELADeAmbiguousUpperbound;   // ELA(edge line average) de-ambiguous upper bound threshold
    unsigned char u8ELADeAmbiguousLowerbound;   // ELA de-ambiguous lower bound threshold
	unsigned char bELACornerEnable;             // enable ELA corner detection

    // temporal de-interlace
    unsigned char bELAResult;               // set all de-interlaced pixels to ELA result
    unsigned char bTopMotion;               // enable top motion detection
    unsigned char bBottomMotion;            // enable bot motion detection
    unsigned char bStrongMotion;            // enable strong motion detection
    unsigned char u8StrongMotionThd;        // strong motion detection threshold
    unsigned char bAdaptiveSetMBMotion;     // enable adaptively setting all MB motion
	unsigned char u8MBAllMotionThd;	        // all MB motion threshold
                                            // if larger than this value, MB motion is active
    unsigned char bStaticMBMask;            // enable static MB mask to set masked pixel static
    unsigned char u8StaticMBMaskThd;        // static MB mask threshold
    unsigned char bExtendMotion;            // enable extend motion MB
    unsigned char u8ExtendMotionThd;        // extend MB threshold
    unsigned char bLineMask;                // enable line MB mask to keep OSD shape
    unsigned char u8LineMaskThd;            // line MB mask threshold
    unsigned char u8LineMaskAdmissionThd;   // line MB mask admission threshold
/*
    // Param 11 (0x2C)
    unsigned char u8DnVarY;             // [14:20]  0x08
    unsigned char u8DnVarCb;            // [7:13]   0x08
    unsigned char u8DnVarCr;            // [0:6]    0x08

    // Param 12 (0x30)
    unsigned int u32DnVarPixelUpperbound;   // [10:19]  0x08
    unsigned int u32DnVarPixelLowerbound;   // [0:9]    0x08

    // Param 13 (0x34)
    unsigned char u8DnVarLumaMBThd;     // [6:13]   0x40
    unsigned int u8DnVarChromaMBThd;   // [0:5]    0x10

    // Param 14 (0x38)
    unsigned char bELAResult;           // [29]     0x00
    unsigned char bTopMotion;           // [28]     0x01
    unsigned char bBottomMotion;        // [27]     0x01
    unsigned char bStrongMotion;        // [26]     0x01
    unsigned char bAdaptiveSetMBMotion; // [25]     0x01
    unsigned char bStaticMBMask;        // [24]     0x01
    unsigned char bExtendMotion;        // [23]     0x01
    unsigned char bLineMask;            // [22]     0x01
    unsigned char u8EdgeStrength;       // [14:21]  0x28
    unsigned char u8DnMinFilterLevel;   // [12:13]  0x00
    unsigned char u8DnMaxFilterLevel;   // [9:11]   0x00
    unsigned char bSeparateHrVtDn;      // [8]      0x01
    unsigned char u8SpDnThd;            // [0:7]    0x08

    // Param 15 (0x3C)
    unsigned char u8MBAllMotionThd;     // [24:31]  0x20
    unsigned char u8StaticMBMaskThd;    // [16:23]  0x0A
    unsigned char u8ExtendMotionThd;    // [8:15]   0x08
    unsigned char u8LineMaskThd;        // [0:7]    0x06

    // Param 16 (0x40)
    unsigned char u8StrongMotionThd;            // [24:31]  0x28
    unsigned char u8ELADeAmbiguousUpperbound;   // [16:23]  0x3C
    unsigned char u8ELADeAmbiguousLowerbound;   // [8:15]   0x14
    unsigned char bELACornerEnable;             // [0]      0x01

    // Param 17 (0x44)
    unsigned char u8LineMaskAdmissionThd;   // [22:29]  0x06
    unsigned char u8SpDnLineDetStr;         // [20:21]  0x00

*/
} FAVC_ENC_DIDN_PARAM;


typedef struct h264_encoder_parameter_ioctl_t 
{
    FAVC_ENC_PARAM apParam;
    unsigned int u32BaseAddr;
    unsigned int u32VcacheBaseAddr;
	unsigned int u32CacheAlign;
    MALLOC_PTR_enc pfnMalloc;
	FREE_PTR_enc   pfnFree;
    unsigned char u8DnResultFormat;     // 0: 2D top, bottom separate, 1: 1D top, bottom separate, 2: top, bottom interleave
	                                    // only effective when temporal de-noise is enable
	unsigned char u8GammaLevel;         // gamma value, range from 0 to 3 inclusive
	                                    // gamma_level = 0:1.5, 1:2.0, 2:2.5, 3:4.0
	FAVC_ENC_DIDN_PARAM *ptDiDnParam;   // if NULL, set default value
} FAVC_ENC_IOCTL_PARAM;

typedef struct h264_encoder_frame_parameter_ioctl_t
{
    FAVC_ENC_FRAME apFrame;
    int new_frame;
    SourceBuffer *mSourceBuffer;
#if ENABLE_TEMPORL_DIDN_WITH_B
    SourceBuffer *mPreviousBuffer;
#endif
	ReconBuffer *mReconBuffer;
    unsigned int u32PrevSrcBuffer_phy;
#ifndef ADDRESS_UINT32
    unsigned char *pu8BSBuffer_virt;
    unsigned char *pu8BSBuffer_phy;
#else
    uint32_t u32BSBuffer_virt;
    uint32_t u32BSBuffer_phy;
#endif
    unsigned int u32BSBufferSize;

    unsigned char u8ReleaseIdx;	// release reconstructed buffer	0, 1, 2, 3, all 0x0F, no relaese 0xFF, 
	//unsigned char u8CurrBSBufIdx;
    // driver maintain (not output to AP)
#ifndef ADDRESS_UINT32
    unsigned char *pu8DiDnRef0_virt;
    unsigned char *pu8DiDnRef0_phy;
    unsigned char *pu8DiDnRef1_virt;
    unsigned char *pu8DiDnRef1_phy;
    unsigned char *pu8SysInfoAddr_virt;
    unsigned char *pu8SysInfoAddr_phy;
    unsigned char *pu8MVInfoAddr_virt;
    unsigned char *pu8MVInfoAddr_phy;
    unsigned char *pu8L1ColMVInfoAddr_virt;
    unsigned char *pu8L1ColMVInfoAddr_phy;
#else
    uint32_t u32DiDnRef0_virt;
    uint32_t u32DiDnRef0_phy;
    uint32_t u32DiDnRef1_virt;
    uint32_t u32DiDnRef1_phy;
    uint32_t u32SysInfoAddr_virt;
    uint32_t u32SysInfoAddr_phy;
    uint32_t u32MVInfoAddr_virt;
    uint32_t u32MVInfoAddr_phy;
    uint32_t u32L1ColMVInfoAddr_virt;
    uint32_t u32L1ColMVInfoAddr_phy;
#endif

    unsigned int u32ROI_x;
    unsigned int u32ROI_y;
} FAVC_ENC_IOCTL_FRAME;

#ifndef VG_INTERFACE
int encode_one_frame(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame, unsigned char is_first_frame, int queue_frame);
#endif
int encoder_create(void **pptHandle, FAVC_ENC_IOCTL_PARAM *pParam, int dev);
int encoder_reset(void *ptEncHandle, FAVC_ENC_IOCTL_PARAM *pParam, int dev);
int encoder_init_vui(void *ptHandle, FAVC_ENC_VUI_PARAM *pVUI);
int encoder_release(void *ptHandle);
int encode_one_frame_trigger(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame, unsigned char is_first_frame, int queue_frame);
int encode_one_frame_sync(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame);
int encode_one_slice_trigger(void *ptEncHandle);
int encode_one_slice_sync(void *ptEncHandle);
int encoder_receive_irq(void *ptHandle);
int encoder_abandon_rest(void *ptHandle);
int get_motion_info_length(void *ptHandle);
#ifdef ENABLE_FAST_FORWARD
    int encoder_reset_gop_param(void *ptHandle, int idr_period, int b_frame, int fast_forward);
#else
    int encoder_reset_gop_param(void *ptHandle, int idr_period, int b_frame);
#endif
int encoder_reset_wk_param(void *ptHandle, int wk_enable, int wk_init_interval, unsigned int wk_pattern);
int encoder_get_slice_type(void *ptHandle);
#ifdef RC_ENABLE
    int encoder_rc_set_quant(void *ptHandle, RC_GET_QUANT_ptr pfnRCGetQuant, void *rc_handler, int force_qp);
    int encoder_rc_update(void *ptHandle, RC_UPDTAE_ptr pfnRCUpdate, void *rc_handler);
#endif
int encode_get_current_xy(void *ptHandle, int *mb_x, int *mb_y, int *encoded_field_done);
int encode_update_fixed_qp(void *ptHandle, int fixed_qp_I, int fixed_qp_P, int fixed_qp_B);
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
    int encoder_reset_register(unsigned int base_addr, unsigned int vcache_base_addr);
#endif
// for test 
int encode_busy_waiting(void *ptEncHandle);
#ifdef SAME_REC_REF_BUFFER
    int encoder_check_vcache_enable(void *ptHandle);
#endif
#ifdef DYNAMIC_GOP_ENABLE
    int encoder_check_iframe(void *ptHandle, int idr_period);
#endif

/*
#ifdef LINUX
    #define W_DEBUG(fmt, args...) printk("(%s)warning: " fmt, __FUNCTION__, ## args)
#else
    #define W_DEBUG(...)
#endif
*/
#ifdef LINUX
    #define E_DEBUG(fmt, args...) printk("(%s)error: " fmt, __FUNCTION__, ## args)
#else
    #define E_DEBUG(...)
#endif


#endif
