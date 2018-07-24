/* Baseline: (1) only I & P
             (2) no field coding & CABAC
             (3) chroma_format_idc = 1(not present)
             (4) scaling list not present
             (5) transform_8x8_mode_flag not present
             (6) second chroma qp offset not present
             (7) level <= 15
   Main : (1) I, P, B
          (2) chroma_format_idc not present (=1)
          (3) scaling list not present
          (4) transform_8x8_mode_flag not present
          (5) second chroma qp offset not present
          (6) level <= 15
   High: (1) I, P, B
         (2) chroma_format_idc = 0 or 1
*/
/*
* 1.1.5: fix assign engien 2 error
* 1.1.6: add engine number to proc (vg_info)
* 1.1.7: update multislice parameter assignment
* 0.1.8: when max_b_frame = 0, not use callback list
* 0.1.9: add mutex to protect work schedule & add spin lock to protect stop job (not allocate memory)
* 0.1.10: 2 engines
* 0.1.11: add out buffer checker & remove streaming code
* 0.1.12: fix release wrong reference frame
* 0.1.13: add re-allocate ref. buffer
* 0.1.14: add didn input (temporal deinterlace & spatial deinteralce)
* 0.1.15: deinterlace & spatial denoise enable, delay callback job not reserve buffer
* 0.1.16: add src_interlace & di_result to disbale deinterlace
* 0.1.17: remove redundant code & update delta qp to be interger (2013/04/08)
*         branch (.1) add debug message (callback fail & postprocess) (2013/04/09)
* 0.1.18: (1) add di constraint (when field coding, disable di)
*         (2) fix bug: when stop job, stop reallocate buffer
* 0.1.19: fix bug: add spin lock to stop job, because it can not be interrupted when change job status (callback fail)
* 0.1.20: fix bug: release all reference when stop job @ reallocated state
* 0.1.21: update proc dump job & property, add driver name to error message
* 0.1.22: add rate control (2013/04/18)
* 0.1.23: when temporal di enable, enable some mode to inprove image quality (2013/04/18)
* 0.1.24: (1) add workqueue to record bitstream
*         (2) CABAC/CAVLC error, set software reset (2013/05/03)
* 0.1.25: update parameter (allocate refference buffer error) (2013/05/10)
*         (1) force intra & release all reference buffer
*         (2) when reinit callback previous job
* 0.1.26: remove post-process when reorder job (2013/05/13)
* 0.1.27: update record bitstream function (2013/05/15)
* 0.1.28: trigger job when reorder job @ workqueue (2013/05/17)
* 0.1.29: modify rate control input parameter to let bitrate stable (to fit c-model) (2013/05/21)
* 0.1.30: (1) release all reference buffer when allocate reference buffer error (2013/05/27)
*         (2) protect shared parameter (2013/05/28)
* 0.1.31: (1) protect packing header
*         (2) EVBR use max bitrate (2013/05/29)
* 0.1.32: add src_fmt TYPE_YUV422_FIELDS (the same as TYPE_YUV422_FRAME)
* 0.1.33: add callback protection
* 0.1.34: allocate sysinfo/mvinfo buffer from DDR1 (2013/06/14)
* 0.1.35: add proc to control encode mode when di enable (2013/06/18)
* 0.1.36: add watermark update
* 0.1.37: protect job list & remove redundant code
*         branch: (1) reference buffer management (2013/07/08)
*                 (2) re-encode whern bitstream overflow (2013/07/08)
* 0.1.38: add AES_ENCRYPT header
* 0.1.39: add didn parameter setting
* 0.1.40: fix bug: reference buffer management and over spec checker (2013/07/22)
* 0.1.41: fill min_quant automatically (2013/07/22)
* 0.1.42: 1. check proeprty update when property does not input
*         2. compute min quant by compress rate @ driver init
* 0.1.43: update min_quant & max_quant assignment (2013/07/24)
*         use property when proeprty input, use default value (by compress rate) when no property input
* 0.1.44: 1. update debug message
*         2. reference buffer deadlock: check release one buffer & mark finish when job done
* 0.1.45: refernece buffer dealock: allocate more than one buffer to allow over spec temporally
*         DDR id of memory allocated depend on param.cfg (2013/08/22)
* 0.1.46: add module parameter h264e_max_chn to define the maximial number of channel (2013/08/28)
* 0.1.47: add snapshot channel, using max resolution, user assign slice type (2013/08/28)
* 0.1.48: add buffer register protection (2013/09/03)
* 0.1.49: remove DIDN ID (defined by VG) & add control to disable register cnt (2013/09/04)
* 0.1.50: add defition of GM8287 (2013/09/05)
* 0.1.51: add memory counter (2013/09/05)
* 0.1.52: fix bug: assign wrong address when bottom field (2013/09/06)
* 0.1.53: handle temporally over spec (2013/10/02)
* 0.1.54: fix bug: hw timeout (2013/10/04)
* 0.1.55: parsing reference buffer of multi-declaration in spec.cfg and use larger buffer declaration (2013/10/16)
* 0.1.56: increase index of idr for skip detection (2013/10/22)
* 0.1.57: add definition of GM8139 (2013/10/24)
* 0.1.58: add mcnr proc & baseline profile setting
* 0.1.59: re-allocate job out buffer by checking out buffer address (2013/10/30)
* 0.1.60: add module parameter: pwm period
* 0.1.61: enable/disable adaptive search range by resolution (2013/11/05)
* 0.1.62: modify buffer management to support 8M/5M/3M (2013/11/18)
* 0.1.63: add definition of VGA to buffer pool management (2013/11/19)
* 0.1.64: fix bug: divide definition of two engine & one engine (2013/11/25)
* 0.1.65: relaxed buffer size computation (width x height < buffer size) (2013/11/25)
* 0.1.66: enable re-encoder when bitstream overflow (2013/11/28)
* 0.1.67: force first frame be I frame (assign slice type) (2013/11/29)
* 0.1.68: update re-encoder qp factor (qp-bitrate factor) (2013/12/2)
* 0.1.69: 1. sync encode parameter & proc default configure
*         2. update mv info size (motion buffer write out unit 64 byte)
*         3. add module parameter to control swap yc
* 0.1.70: 1. fix bug: release buffer pool ckecker use wrong index
*         2. add control to output to console directly
* 0.1.71: 1. align max width/height to multiple of 16
*         2. fix bug: check buffer value before free buffer 
*         3. modify default value of encoder parameter
* 0.1.71.1: branch: bitstream buffer cacheable, before HW go, invalid cache
* 0.1.72: 1. fix bug: AES slice length error when CABAC
*         2. invalidate dcache when encoder go
* 0.1.73: fix bug: when parameter change, first frame must be I frame
* 0.1.74: fix bug: single engine register workqueue function of reorder error
* 0.1.75: 1. update sps profile level setting (level by actually encoded resolution) (2014/01/17)
*         2. add q matrix setting
*         3. move dcache invalidate before callback (2014/01/21)
* 0.1.76: 1. add module parameter to specify config path (spec.cfg/param.cfg) (2014/01/27)
*         2. add property "profile" (2014/01/29)
*         3. (not enable) modify job_item allocateion, use job_item pool, not allocate dynamicly
* 0.1.76.1: (enable) modify job_item allocateion, use job_item pool, not allocate dynamicly
* 0.1.77: 1. modify job_item allocateion, use job_item pool, not allocate dynamicly
*         2. vcache constraint: enable reference one cache ont non-cache
*                               add vcache ref2 enable checker to turn on/off out of order
*         3. set maximal width/height automatically by spec.cfg
* 0.1.78: 1. update error message, check all error/warning message provide enough message (parameter setting warning alarm)
*         2. software reset only initialization & something wrong
*         3. update sei aes encrypt code prevent 00 00 00/01/02/03
*         4. automatically modify wrong roi x/y
*         5. dump register by VG
*         6. fix bug: use wrong didn setting (2014/03/17)
* 0.1.79: add module parameter to enable ioreamp ncb
* 0.1.80: add property "fast_forward" to encode non-reference P
* 0.1.81: add check sum function
* 0.1.82: 1. fix bug: fast forward error
*         2. update definition of timer
*         3. modify default value of min_quant (reasonable value)
* 0.1.83: 1. add new definition of resolution nHD (640x360)
*         2. force update GOP when stop job
* 0.1.84: update fast forward & reallocate warning message
* 0.1.85: fix bug: when reference  buffers are used up, force release other channel
* 0.1.86: 1. update function of dump register
*         2. use list_for_each_entry_saft @ reorder function
* 0.1.87: 1. fix bug: allocate job item must be protected
*         2. add command of parameter
* 0.1.88: 1. update vui error
*         2. change definition of 5M: 2592 x 1944
* 0.1.89: modify default configure of IPcam, between quality mode and performance mode
* 0.1.90: add proc to set level idc
* 0.1.91: 1. software reset: initialization & sonething wrong
*         2. dynamic assign engine (for engine balance)
* 0.1.92: 1. output slice offset
*         2. add proc to dump channel status
* 0.1.93: re-cllback when callback array is overflow
* 0.1.94: 1. set reference & reconstructed using the same buffer (reduce memory allocated)
*         2. add tight buffer parameter to reduce memory usage
*         3. add buffer address checker
* 0.1.95: fix bug: when no fps ratio, set be 1-by-1 to avoid divided by zero
* 0.1.96: 1. handle vcache off & use one reference buffer (width < 208)
*         2. avoid re-encode when one ref buf enable or t.dn enable (because source & reference is broken)
* 0.1.97: fix bug: when change engine, must trigger other engine
* 0.1.98: fix bug: re-allocate buffer must check vcache enable or not
* 0.1.99: 1. fix bug: when timeout, not output slice offset because it is NULL pointer
*         2. when watermark enable disable coefficient/mcnr, and recover when watermark disable
* 0.1.100: fix bug: protect engine assignment & engine list to be the same (pujob)
* 0.1.101: support new configure, gmlib.cfg (2014/07/24)
* 0.1.102: 1. support multiple configure setting (2014/08/14)
*          2. support mb layer rate control to limit bitrate (2014/08/14)
*          3. check common buffer allocation, when something wrong, insert driver fail (2014/08/18)
* 0.1.103: fix bug: parsing gmlib error
* 0.1.104: platform8136 driver
* 0.1.105: using kthread to replace workqueue
* 0.1.106: 1. support src_fmt TYPE_YUV422_2FRAMES_2FRAMES, TYPE_YUV422_FRAME_2FRAMES, TYPE_YUV422_FRAME_FRAME
*          2. return fixed bs length when reencode fail
* 0.1.107: support roi of two frame
* 0.1.108: reset di_result & src_interlace to be zero when property not exist
* 0.1.109: add new property "qp_offset" & "profile"
* 0.1.110: force engine balance
* 0.1.111: 1. fixed condition of force balance
*          2. add property clockwise to swap width and height (for rotation)
*          3. add protection of job state
* 0.1.112: fix bug of wrong checker of preprocess & postprocess
* 0.1.113: fix bug: check condition of force engine balance error
* 0.1.114: add new resolution of 6M/7M
* 0.1.115: set maximal channel id to vg
* 0.1.116: polling src_interlace & di_result when property update to update didn mode (2015/01/21)
* 0.1.117: fix bug: check qp offset update error
* 0.1.118: 1. add out property: di_result
*          2. when ip_offset & roi_delta_qp from proeprty both be zero, using parameter setting by proc
* 0.1.119: add new definition of resolution: 1.2M
* 0.1.120: remove level & dpb check
* 0.1.121: enable one rec buffer when fastforward enable
* 0.1.122: not write out mvinfo & l1colmvinfo
* 0.1.123: add moodule parameter h264e_dis_wo_buf to caontrol write out rec frame
* 0.1.124: fix bug: interrupt flag error
* 0.1.125: update definition of resolution 4M = 2688x1520
* 0.1.126: 1. add proc to conrtol HW timeout clock
*          2. prevent more than one property of src_interlace (zero channel)
*          3. fix bug of setting profile by proc
*          4. add handler of roi qp (fixed qp)
* 1.2.127: support dynamic gop & smart ROI
* 1.2.128: support enable one rec buffer when fastforward enable (8287)
* 2.0.0: videograph2
* 2.0.1: reduce uasge of memory
* 2.0.2: fix bug: allocate job item error
*/

#ifndef _H264E_IOCTL_H_
	#define _H264E_IOCTL_H_

	#include "enc_driver/define.h"
	#include "enc_driver/portab.h"
    #define H264E_VER           0x02000200
	#define H264E_VER_MAJOR     ((H264E_VER>>24)&0xFF)  // huge change
	#define H264E_VER_MINOR     ((H264E_VER>>16)&0xFF)  // interface change 
	#define H264E_VER_MINOR2    ((H264E_VER>>8)&0xFF)   // functional modified or buf fixed
    #define H264E_VER_BRANCH    (H264E_VER&0xFF)        // branch for customer request

	#define FAVC_IOCTL_ENCODE_INIT              0x4201
	//#define FAVC_IOCTL_DECODE_REINIT            0x4171
	#define FAVC_IOCTL_ENCODE_FRAME             0x4203
    #define FAVC_IOCTL_RC_INIT                  0x4204
	#define FAVC_IOCTL_ENCODE_B_FRAME           0x4205
    #define FAVC_IOCTL_ENCODE_VUI               0x4206
    #define FAVC_IOCTL_GET_MOTIONINFO           0x5207
    #define FAVC_IOCTL_RE_ENCODE_FRAME          0x4209

	#define H264_ENCODER_DEV  "/dev/favcenc"

    
	typedef enum {
		FrameTypeDefault = 0,
		FrameTypeIDR = 1,
		FrameTypeIntra = 2,
		FrameTypeP = 3,
		FrameTypeNonRefB = 6
	} ASSIGN_FRAME_TYPE;
	
	typedef struct roi_qp_region_s
    {
        byte     enable;
        uint32_t roi_x;
        uint32_t roi_y;
        uint32_t roi_width;
        uint32_t roi_height;
    } FAVC_ROI_QP_REGION;
	
	typedef struct reconstructed_buffer
    {
    #ifdef ADDRESS_UINT32
        uint32_t rec_luma_virt;
    	uint32_t rec_luma_phy;
    	uint32_t rec_chroma_virt;
    	uint32_t rec_chroma_phy;
    #else
    	uint8_t *rec_luma_virt;
    	uint8_t *rec_luma_phy;
    	uint8_t *rec_chroma_virt;
    	uint8_t *rec_chroma_phy;
    #endif        
    	uint8_t is_used;
    	uint8_t rec_buf_idx;
    } ReconBuffer;

    typedef struct input_image_bufer
    {
    #ifndef ADDRESS_UINT32
    	uint8_t  *src_buffer_virt;
    	uint8_t  *src_buffer_phy;
    #else
        uint32_t src_buffer_virt;
    	uint32_t src_buffer_phy;
    #endif
    	uint8_t  is_used;
    	uint8_t  src_buf_idx;
    	int32_t  ThisPOC;
    	//uint32_t frame_num;
    	uint32_t lsb;
    	//int32_t  poc;
    	//byte     idr_flag;
    	FAVC_ROI_QP_REGION sROIQPRegion[8];
    } SourceBuffer;
    
	typedef struct h264_scaling_list_t
	{
		int i32ScaleList4x4[6][16];
		int i32ScaleList8x8[2][64];
		int bScalingListFlag[8];
	} FAVC_ENC_SCALE_LIST;

    typedef struct h264_mcnr_table_t
    {
        unsigned char u8LSAD[52];
        unsigned char u8HSAD[52];
    } FAVC_ENC_MCNR_TABLE;

	typedef struct h264_encoder_vui_param_t
	{
		unsigned char u8AspectRatioIdc;
		/*	sample aspect ratio of the luma samples
		 *	0: not used   1: 1:1 (square)  2: 12:11     3: 10:11
		 *	4: 16:11      5: 40:33         6: 24:11     7: 20:11
		 *	8: 32:11      9: 80:33         10: 18:11    11: 15:11
		 *	12: 64:33     13:160:99
		 *	14~254: reserved
		 *	255: Ectended_SAR, it will use u32SarWidth and u32SarHeight be sample ratio
		 *	if not used, u8AspectRatioIdc = 0 */
		unsigned int u32SarWidth;
		unsigned int u32SarHeight;

		unsigned char u8VideoFormat;
		/*	0:component   1: PAL   2: NTSC   3: SECAM   4: MAC   5: unspected video format
		 *	if not used, u8VideoFormat = 5 */
        unsigned char bVideoFullRangeFlag;  // if not present, it is zero
		unsigned char u8ColourPrimaries;
		/*	the chromaticity coordinates of the source primaroes
		 *	if not used, u8ColourPrimaries = 2 */
		unsigned char u8TransferCharacteristics;
		/*	the opto-electronic transfer characteristic of the source pictures
		 *	if not used, u8TransferCharacteristis = 2 */
		unsigned char u8MatrixCoefficients;
		/*	the martix coefficients used in deriving luma and chroma signals from green, blue, and red primaries
		 *	if not used, u8MatrixCoefficients = 2 */
		unsigned char u8ChromaSampleLocTypeTopField;
		unsigned char u8ChromaSampleLocTypeBottomField;
		/*	the location of chroma samples for top field and bottom field
		 *	rnage from 0 to 5, inclusive */
		unsigned char bTimingInfoPresentFlag;
		unsigned int u32NumUnitsInTick;
        unsigned int u32TimeScale;
	} FAVC_ENC_VUI_PARAM;

	typedef struct h264_encoder_frame_parameter_t
	{
		unsigned char *pu8BSBuffer_virt;	// from AP
		unsigned char *pu8BSBuffer_phy;
		unsigned int u32APMaxBSLength;	// length of bitstream buffer from AP
		unsigned int u32ResBSLength;	// return encoded bitstream length

		unsigned char u8SrcBufferIdx;

		unsigned int u32UsedUpSourceBuffer;
		// used up source buffer idx, all 0xFF, no release 0xFFFFFFFF (return to AP)

		unsigned char u8CurrentQP;
        int i32ForceSliceType;
        /* 0: force P
         * 1: force B
         * 2: force I
         * otherwise: auto  */
		unsigned char bForceIntra;
		unsigned char u8FrameSliceType;	// slice type of encoded frame (return to AP)
		unsigned int u32Cost;
		
		unsigned char u8NonEncodedBFrame;
		// the number of non-encoded b frame which source is 
		// loaded in source buffer need to be encoded
		unsigned char bBufferedBFrame;  // current frame need to be buffered (B frame)
		
		//unsigned char u8ROIQPType;  // 0: disable, 1: delta qp , 2: fixed qp
		signed char i8ROIQP;
		FAVC_ROI_QP_REGION sROIQPRegion[8];

        unsigned int frame_total_qp;
		
		//int rewind;
		
		int i32ErrorMessage;
		//int random;
		//int retest;
    #ifdef ENABLE_FAST_FORWARD
        int bRefFrame;
    #endif
    #ifdef OUTPUT_SLICE_OFFSET
        unsigned int u32SliceOffset[MAX_SLICE_NUM];
        unsigned int u32SliceNumber;
    #endif
	} FAVC_ENC_FRAME;

	typedef struct h264_encoder_parameter_t
	{
	#ifndef VG_INTERFACE
		unsigned int u32API_version;
		unsigned int u32MaxWidth;
		unsigned int u32MaxHeight;
		unsigned int u32APMaxBSLength;      // length of bitstream buffer from AP
		unsigned char bAbandonOverflow;     // if encoded bitstream larger than AP bitstream buffer, throw away the rest of bitstream
    #endif

    /*****************************
     *       GOP setting 
     *****************************/
		//unsigned int u32IntraPeriod;    // intra period (unit frame)
		unsigned int u32IDRPeriod;      // IDR period (unit frame), priority of IDR is higher than intra
		unsigned char u8BFrameNumber;   // how many B frames between two P or I frames (GOP length - 1), range: 0~MAX_B_NUMBER (16)
		/*  GOP: I B B ... P B B ... P ... I (idr)
		 *         <------> u8BFrameNumber
		 *       <------------------------> u8IDRPeriod */
    #ifdef ENABLE_FAST_FORWARD
		unsigned int u32FastForward;    // 0/2/4    
	#endif

    /*****************************
     *     image inforamtion
     *****************************/
        unsigned int u32Profile;        // BASELINE, MAIN, FREXT_HP
        unsigned int u32LevelIdc;
		unsigned int u32SourceWidth;    // source frame height
		unsigned int u32SourceHeight;   // source frame width
		// ROI
		unsigned int u32ROI_X;          // must be multiple of 16
		unsigned int u32ROI_Y;          // must be multiple of 16
		unsigned int u32ROI_Width;      // must be multiple of 16 and bigger than or equal to 64 (4 MB)
		unsigned int u32ROI_Height;     // if field coding, must be multiple of 32, frame must be multiple of 16
		                                // if u32ROI_Width and u32ROI_Height equal to zero, they will set to source image size
		unsigned char u8NumRefFrames;   // range: 0~3
		unsigned char bFieldCoding;     // field coding or frame coding
		unsigned char bEntropyCoding;   // 0: CAVLC, 1: CABAC
		unsigned char u8CabacInitIDC;   // cabac init number
		//    unsigned char u8InitialQP;      // pps initial qp
		unsigned char bChromaFormat;    // 0: mono, 1: 4:2:2
		unsigned char bSrcImageType;    
		/*  0: top/bottom fields separate, 1: top/bottom fields interleave
		 *	                        itl_mode 
		 *	                |    0    |      1
		 *	             ---+---------+-------------
		 *	src_img_type  0 |  frame  | frame/field
		 *	              - +---------+-------------
		 *	              1 |  frame  | frame/field
		 *	                     ^        ^
		 *	         progressive mode   interleave mode        
		 *	itl: no de-interlace */
		unsigned int u32SliceMBRowNumber;  // the number of MB line of one slice contians, if 0, single slice
		//unsigned int u32MaxMBSliceNum;      // max number of MBs in one slice (should be align to mb number of one row)
		unsigned char bSADSource;   // 0: from ME SAD, 1: from DiDn
		                            // only effective when temporal de-noise or temporal de-interlace enable
		                            
        unsigned char bIntTopBtm;
        unsigned int u32CroppingLeft;
        unsigned int u32CroppingRight;
        unsigned int u32CroppingTop;
        unsigned int u32CroppingBottom;

    /***************************** 
     *        scaling list
     *****************************/
		unsigned char bScalingMatrixFlag;
		//FAVC_ENC_SCALE_LIST mScaleList;     // scaling list parameters
		FAVC_ENC_SCALE_LIST *pScaleList;     // scaling list parameters

    /***************************** 
     *  sps and pps information
     *****************************/
		unsigned char u8Log2MaxFrameNum;
		unsigned char u8Log2MaxPOCLsb;
		unsigned char u8ResendSPSPPS;   // from AP
		/*	0: output sps and pps only first IDR frame
		 *	1: outptu sps and pps each I frame
		 *	2: output sps and pps each frame */

    /*****************************
     *      quant parameter
     *****************************/
	//#ifdef FIXED_QP
		signed char i8FixedQP[3];
	//#endif
		signed char i8ChromaQPOffset0;      // Cb qp offset
		signed char i8ChromaQPOffset1;      // Cr qp offset

    /*****************************
     *    deblocking parameter
     *****************************/
		signed char i8DisableDBFIdc;        // if value<0, deblocking disable not present
		signed char i8DBFAlphaOffset;     // deblocking alpha
		signed char i8DBFBetaOffset;      // deblocking beta

    /*****************************
     *      didn parameter
     *****************************/
		unsigned char u8DiDnMode;
		/*	        [3]                 [2]                   [1]                     [0]
		 *	temporal de-noise    spatial de-noise    temporal de-interlace    spatial de-interlace
		 *	if u8BFrameNumber > 0, temporal de-noise and temporal de-interlace must be disable */

    /*****************************
     *        search range
     *****************************/
		// search range
		unsigned int u32HorizontalBSR[2];                   // horizational search range of B slice
		unsigned int u32VerticalBSR[2];                     // vertical search range of B slice
		unsigned int u32HorizontalSR[MAX_NUM_REF_FRAMES];   // horzational search range of P slices
		unsigned int u32VerticalSR[MAX_NUM_REF_FRAMES];     // vertical search range of P slices

    /*****************************
     * coefficient threshold and eraly terminate
     *****************************/
		unsigned char bDisableCoefThd;  // coefficient threshold disable
		unsigned char u8LumaCoefThd;    // luma threshold value, range 0~7. if <0, disable
		                                // if number of non-zero transform coefficient < this value => set all coefficient be zero
		unsigned char u8ChromaCoefThd;  // chroma threshold value, range 0~7. if <0, disable
		unsigned int u32FMECostThd;     // fast motion estimation by cost threshold
		                                // range 0~0xFFFF, if 0 disbale
		unsigned int u32FMECycThd[2];   // fast motion estimation by cycle threshold (for P and B)
		                                // range 0x20~0xFFF, 0xFFF disbale


	/*****************************
     *         watermark
     *****************************/
		unsigned char bWatermarkCtrl;       // 0: watermark & CRC disable, 1: intra MB & CRC enable
		unsigned int u32WatermarkPattern;	// wateramrk initial pattern
		/*	watermark only enable when intra MB, and confficient filter by threshold 
		 *	(i8LumaCoefThd and i8ChromaCoefThd) will be disable */
		unsigned int u32WKReinitPeriod;

	/*****************************
     *          delta qp
     *****************************/
		unsigned char u8MBQPWeight;	
		/*
		 *	>4: each MB of this slice uses the same QP
		 *	 4: MB QP decided by image variance
		 *	 0: MB QP decided by MB level rate control
		 *	 1~3: weighted of image variance & rate control
		 *	      (weight*qp_by_variance + (~weight)*qp_by_bits)/4 */
		unsigned char u8QPDeltaStrength;
		unsigned int u32QPDeltaThd; // the threshold can not be zero (diff from c-model)
		/*	delta qp = delta_strength*image_variance - delta_threshold
		 *	modify delta_strength and delta_threshold to let almost delta qps be zeros */	
		unsigned char u8MaxDeltaQP;
		// rate control
		unsigned int u32RCBasicUnit;        // 0: basic unit of mb level qp update
		unsigned int u32MBRCBitrate;
		unsigned char u8RCMaxQP[3];
		unsigned char u8RCMinQP[3];
		// frame rate
		unsigned int u32num_unit_in_tick;   // time unit of one frame
		unsigned int u32time_unit;          // time unit of one second 
    	// frame rate = time_unit/num_unit_in_tick, it can handle float value of frame rate

    /*****************************
     *  transform and mb partition
     *****************************/
		unsigned char bTransform8x8Mode;    // enable 8x8 transform
		unsigned char bInterDefaultTransSize;   // Inter hadamard transform size. 0: 4x4, 1: 8x8
		unsigned char u8PInterPredModeDisable;
        unsigned char u8BInterPredModeDisable;
		/*	 [3]     [2]    [1]    [0]
		 *	16x16   16x8   8x16    8x8
		 *	if all disable  (0x0F)
		 *		P slice: enable 16x16
		 *		B slice: enable 8x8 */
		unsigned char bIntra8x8Disable;
		unsigned char bIntra4x4Mode;    // 0: 5 pred modes, 1: 9 pred modes
		unsigned char bFastIntra4x4;    // open loop intra 4x4
		unsigned char bIntra16x16PlaneDisable;
		unsigned char bDirectSpatialMVPred;     // direct mode MV prediction. 0: temporal, 1: spatial
		unsigned char bIntraDisablePB;
		unsigned char bIntra4x4DisableI;
		unsigned char bIntra16x16DisableI;
		
		unsigned char u8IntraCostRatio; // from 0 to 15
		unsigned int u32ForceMV0Thd;    // 16 bits

    /*****************************
     *       roi qp region
     *****************************/
		unsigned char u8RoiQPType;  // 0: disable, 1: delta qp , 2: fixed qp
		//unsigned char u8ROIQP;
		//unsigned char u8ROIDeltaQP;
		signed char i8ROIDeltaQP;
        unsigned char u8ROIFixQP;
    #ifdef MCNR_ENABLE
    /*****************************
     *           MCNR 
     *****************************/
        unsigned char bMCNREnable;
        unsigned char u8MCNRShift;
        unsigned int u32MCNRMVThd;
        unsigned int u32MCNRVarThd;
        FAVC_ENC_MCNR_TABLE mMCNRTable;
    #endif
    } FAVC_ENC_PARAM;

#endif
