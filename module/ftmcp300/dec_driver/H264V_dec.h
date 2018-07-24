#ifndef _H264TYPE_DEC_H_
#define _H264TYPE_DEC_H_

#include "portab.h"
#include "../h264d_ioctl.h"


/* vcache enable flags */
enum vcache_en_flag {
    VCACHE_ILF_EN      = (1 << 0),
    VCACHE_SYSINFO_EN  = (1 << 1),
    VCACHE_REF_EN      = (1 << 2),
};
    

/* function pointers passing to low level driver */
typedef struct dec_init_param
{
    MALLOC_PTR_dec pfnMalloc;
    FREE_PTR_dec   pfnFree;
    DAMNIT_PTR_dec pfnDamnit;
    MARK_ENGINE_START_PTR_dec pfnMarkEngStart;
} FAVCD_DEC_INIT_PARAM;


typedef struct dec_buffer_t
{
	unsigned char *dec_yuv_buf_virt;
	unsigned char *dec_yuv_buf_phy;
	unsigned char *dec_mbinfo_buf_virt;
	unsigned char *dec_mbinfo_buf_phy;
	unsigned char *dec_scale_buf_virt;
	unsigned char *dec_scale_buf_phy;
	int buffer_index;
} FAVC_DEC_BUFFER;

typedef struct 
{
    /* input to low level driver */
	FAVC_DEC_BUFFER *ptReconstBuf;

    /* output from low level driver */
	FAVC_DEC_FRAME apFrame;
    //unsigned char bFirstTrigger;  /* the first trigger of decoding a frame */

    /* for error handling */
    unsigned char bPicStored;        /* picture has been stored into DPB */
    unsigned char u8PicTriggerCnt;   /* count of triggering decoder for the current picture (clear to 0 after storing picture) */
    unsigned char u8FrameTriggerCnt;  /* count of triggering decoder for this frame */

	unsigned char u8ReleaseBufferNum;		// number of buffers in the array (u8ReleaseBuffer)
	unsigned char u8ReleaseBuffer[MAX_DEC_BUFFER_NUM];	// buffers that are not used by decoder/driver. It contains the index of the buffer of decp->dec_buf arrays
} FAVC_DEC_FRAME_IOCTL;

#if USE_FEED_BACK
typedef struct 
{
	FAVC_DEC_RET_BUF tRetBuf;
	unsigned char u8ReleaseBufferNum;
	unsigned char u8ReleaseBuffer[MAX_DEC_BUFFER_NUM];	// release feedback buffer if it is not a reference frame
} FAVC_DEC_FEEDBACK;
#endif

enum decoder_flag {
    DEC_WAIT_IDR                = 0x01, /* wait until the first IDR after reset */
    DEC_SKIP_TIL_IDR_DUE_TO_ERR = 0x02, /* some error occured, skip til the next IDR */
};

/* set parameter */
int H264Dec_Init(FAVC_DEC_PARAM *ptParam, void *ptDecHandle);
int H264Dec_SetFlags(void * ptDecHandle, enum decoder_flag flag);
int H264Dec_SetScale(FAVC_DEC_FR_PARAM *ptParam, void *ptDecHandle);
int H264Dec_SetBitstreamBuf(void * ptDecHandle, uint32_t phy_addr, uint32_t vir_addr, uint32_t size);

/* bind/unbind */
int H264Dec_BindEngine(void *ptDecHandle, void *ptEngInfo);
int H264Dec_UnbindEngine(void *ptDecHandle);

/* decode one frame */
int H264Dec_OneFrameStart(void * ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame);
int H264Dec_OneFrameNext(void * ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame);
void H264Dec_OneFrameErrorHandler(void *ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame);
#if USE_FEED_BACK
int H264Dec_FeedBackUsedBuffer(void * ptDecHandle, FAVC_DEC_FEEDBACK *fbBuffer);
#endif

/* flush output buffer in DPB */
int H264Dec_OutputAllPicture(void *ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame, int get_release_flg);
#ifndef VG_INTERFACE
void H264Dec_ClearDPB(void *ptDecHandle);
#endif

/* create/destory */
void *H264Dec_Create(const FAVCD_DEC_INIT_PARAM *ptParam, int ndev);
void H264Dec_Release(void *ptDecHandle);

/* ISR helper functions */
#if USE_LINK_LIST
int H264Dec_receive_irq_ll(void *ptEngInfo, void *ptCodecLL);
#endif
int H264Dec_DispatchIRQ(void *ptDecHandle, void *ptEngInfo);
int H264Dec_ReceiveIRQ(void *ptEngInfo);
//void H264Dec_ClearIRQ(void *ptDecHandle);

/* reset */
void H264Dec_Reset(void *ptDecHandle);
int H264Dec_ResetEgnine(void *ptEngInfo);

/* debug */
void H264Dec_DumpReg(void *ptDecHandle, int level);


#if 0//ndef VG_INTERFACE
int H264Dec_OneFrame(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle);
#if USE_MULTICHAN_LINK_LIST
int H264Dec_OneFrame_Multichan_top(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle);
int H264Dec_OneFrame_Multichan_bottom(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle);
int H264Dec_OneFrame_Multichan_trigger(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle);
#endif
#endif


#endif
