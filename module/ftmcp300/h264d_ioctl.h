/*
2014/09/25 00208C00, 1. support k thread 
                     2. record fail list channel to reduece CPU utilization
                     3. free buffer in callback_scheduler function
2010/08/10 fix bug: bottom first flag & support NPF
2010/10/15 streaming input
version 1.01
    streaming input
*/

#ifndef _IOCTL_H264D_H_
    #define _IOCTL_H264D_H_
    
    #include <linux/types.h>
    #include "dec_driver/define.h"
    
    #define H264D_VER           0x00208C00
    //                            HIIFFFBB
    //                            H huge change
    //                             II interface change
    //                               FFF functional modified or bug fixed
    //                                  BB branch for customer request

    #define H264D_VER_MAJOR_N(N)     (((N)>>28)&0x0f)  // huge change
    #define H264D_VER_MINOR_N(N)     (((N)>>20)&0xff)  // inerface change
    #define H264D_VER_MINOR2_N(N)    (((N)>>8)&0xfff)   // functional modified or bug fixed
    #define H264D_VER_BRANCH_N(N)    ((N)&0x0ff)        // branch for customer request


    #define H264D_VER_MAJOR     H264D_VER_MAJOR_N(H264D_VER)   // huge change
    #define H264D_VER_MINOR     H264D_VER_MINOR_N(H264D_VER)   // inerface change
    #define H264D_VER_MINOR2    H264D_VER_MINOR2_N(H264D_VER)  // functional modified or bug fixed
    #define H264D_VER_BRANCH    H264D_VER_BRANCH_N(H264D_VER)  // branch for customer request

    /* for converting H264D_VER to string */
    #define STR(x)          #x
    #define STR2(x)         STR(x)
    #define H264D_VER_STR   STR2(H264D_VER)

    #define H264D_IOC_MAGIC  'd'
    #define H264_IOCTL_DECODE_INIT           _IOW(H264D_IOC_MAGIC,  0, FAVC_DEC_PARAM)   //#define H264_IOCTL_DECODE_INIT              0x4170
    #define H264_IOCTL_DECODE_FRAME          _IOWR(H264D_IOC_MAGIC, 1, FAVC_DEC_FRAME)   //#define H264_IOCTL_DECODE_FRAME             0x4172
    #define H264_IOCTL_DECODE_RELEASE_BUFFER _IOWR(H264D_IOC_MAGIC, 2, FAVC_DEC_RET_BUF) //#define H264_IOCTL_DECODE_RELEASE_BUFFER    0x4177
    #define H264_IOCTL_DECODE_OUTPUT_ALL     _IOR(H264D_IOC_MAGIC,  3, FAVC_DEC_FRAME)   //#define H264_IOCTL_DECODE_OUTPUT_ALL        0x4178

    #define MAX_DEC_BUFFER_NUM 17

    /* values for u8MaxBufferedNum */
    #define NO_BUFFERED   0
    #define MAX_BUFFERED -1

    
    typedef struct FrameInfo_t{
        __s16  i16POC;
        __s8   i8BufferIndex;   
#ifndef VG_INTERFACE
        __u8   u8Structure;      /* 0:frame  1:top  2:bottom */
        __u8   bDamaged;
#endif
    } FrameInfo;

    typedef struct {
        /* output */
        __u16  u16FrameWidth;     /* output frame width (non-cropped) */
        __u16  u16FrameHeight;    /* output frame height (non-cropped) */

        __u16  u16CroppingLeft;
        __u16  u16CroppingRight;
        __u16  u16CroppingTop;
        __u16  u16CroppingBottom;
        
        __u32  u32TimeScale;
        __u32  u32UnitsInTick;
        __u8   bHasScaledFrame;
        __u8   bFieldCoding;
        __s16  i16POC;
        //__u32  u32Seq;

        __u8      u8OutputPicNum;            /* number of output buffers */
        FrameInfo mOutputFrame[MAX_DEC_BUFFER_NUM];
        /* Reconstructed buffer will not be released until it is not a reference frame and it is outputted.
         * If user does not return used-done buffers, the reconstructed buffer may full and driver returns 
         * fail. Output list is sorted by POC in increasing order. When the number of used buffers is over 
         * max_buffered_num defined in ¡§H264_DEC_PARAM¡¨, the driver will add at least one frame that is not 
         * a reference frame to output list. If the POC of rest non-output frame and the last outputted POC 
         * is continuous, the driver will add this frame to output list and repeat it until no continuous 
         * POC of frame. */

        __s8 i8Valid; /* 0: invalid(unset). 1: valid. <0: set with error */
        __u8 bPicStored; /* 1: picture has been added into dpb, so it will be returned by low level drivers via release/output lists */
        __u8 bIsIDR; /* 1: the decoded picture is an IDR frame */

#ifndef VG_INTERFACE
        __u32 u32BSBuf_virt;     // virtual address of input bitstream, set by application
        __u32 u32BSLength;       // bitstream length, set by application

        __s32 i32ErrorMessage;
        /* error message. 
         *  -1: alloc memory error
         *  -2: API version error
         *  -3: parsing error (syntax error)
         *  -4: load bit stream error (larger than bit stream buffer or copy error)
         *  -5: frame size is larger than max frame size (over reconstructed buffer)
         *  -6: decode vld error
         *  -7: bit stream empty
         *  -8: hardware timeout 
         *  -9: software timeout 
         *  -10: hardware unsupport
         *  -11: reconstructed buffer is full
         *  -12: hardware timeout and vld error
         *  -13: DMA read/write error
         *  -14: timeout because of bit stream empty
         *  -15: input decoder is null              */

        //__u8 bIsMap[MAX_DEC_BUFFER_NUM];   // whether the buffer be mapped (used for unmap)
        /* ========================== for debug ========================== */
        //for debug
        __u32 error_sts;
        __u8  vcache_enable;
        //__u8 vcache_ilf;
        //__u8 vcache_sysinfo;
        __u8 multislice;
        __u8 current_slice_type;
        __u32 decoded_frame_num;
#ifdef TEST_SOFTWARE_RESET
        __s32 random;     // software reset testing
#endif
#endif /* !VG_INTERFACE */
    } FAVC_DEC_FRAME;

    typedef struct {
        __u8 bScaleEnable;
        /* output scaled reconstructed picture
         * scaled reconstructed picture is not support multi-slice and MBAFF pattern 
         * if field coding, bChromaInterlace must be 1 
         * if frame coding, bChromaInterlace must be 0  */
        __u8 u8ScaleRow;          // scaled row (2 or 4)
        __u8 u8ScaleColumn;       // scaled column (2 or 4)
        __u8 u8ScaleType;         // scaled type (0 or 1)
        /* 0: frame, top field and bottom field use the same scaled algorithm
         * 1: frame and top field use the same algorithm, but bottom field use
         *    different algorithm  */
        __u16  u16ScaleYuvWidthThrd;
        /* if ((bScaleEnable == 1) && (output_yuv_width > u16ScaleYuvWidthThrd))
         *     enable scaled output
         * else
         *     disable scaled output
         */
    } FAVC_DEC_FR_PARAM; /* decode parameters that may changed for each frame */


    typedef struct {
        __u16 u16MaxWidth;    
		/* It indicates the video Max. Width. The image of decoded frame 
		 * from bit stream should be never larger than this value */
        __u16 u16MaxHeight;
		/* It indicates the video Max. Height. The image of decoded frame 
		 * from bit stream should be never larger than this value */
        __u8  u8MaxBufferedNum;
        /* The maximum of buffer number. If number of buffers driver used exceed max_buffered_num,
         * driver will output at least one reconstructed buffers until the number of used buffer 
         * is lower than max_buffered_num after AP output these buffer. Buffer will not be release 
         * when the buffer is not be outputed or it is a reference buffer.
         *   0: output directly (display order = decode order)
         *  -1: output until buffer is full (the display order is always right, but the buffer 
         *      will overflow if user don't release buffer before decode next frame
         *  otherwise: it will clip to max buffer number(depend on pattern level) */
        __u8  bChromaInterlace;
        /* how reconstructed buffer pad chroma value (transform from 420 to 422)
         *    chroma interlace = 0
         *   Frame           Field
         * Y0   C0        TopY0 TopC0
         * Y1   C0        BotY0 TopC0
         * Y2   C1        TopY1 BotC1
         * Y3   C1        BotY1 BotC1
         *
         *    chroma interlace = 1
         *   Frame           Field
         * Y0   C0        TopY0 TopC0
         * Y1   C1        BotY0 BotC0
         * Y2   C0        TopY1 TopC1
         * Y3   C1        BotY1 BotC1 */
         __u8 bOutputMBinfo;       // output MB info buffer
         __u8 bUnsupportBSlice;    // unsupport b slice (shared MB info buffer)
         /* It indicates whether driver support B slice decoding 
		  * 0: support b slice
		  * 1: not support b slice, it costs less buffer */
		  
         __u8 bVCCfg;

#define LOSS_PIC_PRINT_ERR  (1 << 0)
#define LOSS_PIC_RETURN_ERR (1 << 1)
         __u8 u8LosePicHandleFlags; /* Flags for specifying how lossing pic is handled by driver */

        FAVC_DEC_FR_PARAM fr_param; /* parameters that may be changed for each frame */
             
#ifndef VG_INTERFACE
         __u32 u32API_version; /* It indicates the version of driver modules */
#endif
    } FAVC_DEC_PARAM;

    typedef struct {
        __s32 i32FeedbackList[MAX_DEC_BUFFER_NUM];  
        /* user return the buffers that are outputed or used up */
        __s32 i32FeedbackNum;
        
#ifndef VG_INTERFACE
        __s32 i32ErrorMessage;
#endif

    } FAVC_DEC_RET_BUF;
    
#endif /* _IOCTL_H264D_H_ */

