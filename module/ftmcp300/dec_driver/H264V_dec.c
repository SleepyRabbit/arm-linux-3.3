#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>

#include "H264V_dec.h"
#include "global.h"



/*
 * Initialize and set the private data structure (ptDecHandle) according to parameter (ptrDEC)
 * NOTE: Only data structure are reset. No HW reset.
 */
int H264Dec_Init(FAVC_DEC_PARAM *ptParam, void *ptDecHandle)
{
    int ret;
    
    PERF_MSG("decoder_reset_param >\n");
    ret = decoder_reset_param(ptParam, ptDecHandle);
    PERF_MSG("decoder_reset_param <\n");
    if (ret < 0)
        return ret;

    return ret;
}

/*
 * Set decoder flags
 */
int H264Dec_SetFlags(void * ptDecHandle, enum decoder_flag flag)
{
    return decoder_set_flags(ptDecHandle, flag);
}

/*
 * Set scale options
 * NOTE: Only data structure are set. No HW register programming.
 */
int H264Dec_SetScale(FAVC_DEC_FR_PARAM *ptParam, void *ptDecHandle)
{
    int ret;
    ret = decoder_set_scale(ptParam, ptDecHandle);
    return ret;
}


/* 
 * Return a proper padding size for bitstream buffer.
 * Reason of padding bytes:
 * 1. round up to multiple of 64 byte (HW requirement)
 * 2. Padding extra padding_size bytes to avoid 'bitstream empty interrupt' occurrs too early (before 'slicce done' interrupt)
 */
static unsigned int H264Dec_GetPaddingSize(uint32_t size)
{
    unsigned int align_size;
    unsigned int padding_size;
    extern unsigned int bs_padding_size;

    padding_size = bs_padding_size;

	/* round up to (size + padding_size + align_size) a multiple-of-64 */
    align_size = 0;
    if ((size + padding_size) & 0x3F)
        align_size = (~(size + padding_size) & 0x3F) + 1;
    
#if 0
    printk("org bs size = %d, pad size = %d, align size = %d, total = %d\n", 
        size, padding_size, align_size, size + padding_size + align_size);
#endif

    return padding_size + align_size;
}


/* 
 * set bitstream address and size 
 */
int H264Dec_SetBitstreamBuf(void * ptDecHandle, uint32_t phy_addr, uint32_t vir_addr, uint32_t size)
{
    DecoderParams *dec = (DecoderParams *)ptDecHandle;
    unsigned int padded_size;

    padded_size = size + H264Dec_GetPaddingSize(size);

    dec->u32BSCurrentLength = padded_size; /* padded size */
    dec->u32BSLength = size;
    dec->pu8Bitstream_phy = (uint8_t *)phy_addr;
    dec->pu8Bitstream_virt = (uint8_t *)vir_addr;
    if (decoder_set_bs_buffer(dec) < 0)
        return -1;
    return 0;
}


/*
 * Bind DecHandle to a specified engine
 */
int H264Dec_BindEngine(void *ptDecHandle, void *ptEngInfo)
{
    int ret;
    ret = decoder_bind_engine(ptDecHandle, ptEngInfo);
    return ret;
}


/*
 * Unbind engine for DecHandle
 */
int H264Dec_UnbindEngine(void *ptDecHandle)
{
    int ret;
    ret = decoder_unbind_engine(ptDecHandle);
    return ret;
}


/*
 * Start the first slice decoding of decoing a frame
 */
int H264Dec_OneFrameStart(void * ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    return decode_one_frame_start(ptDecHandle, ptFrame);
}


/*
 * Handle intrrupt of the previous slice decoding and start the next slice decoding until a frame is done
 */
int H264Dec_OneFrameNext(void * ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    return decode_one_frame_next(ptDecHandle, ptFrame);
}


/*
 * Handling error occurred during decoding a frame
 */
void H264Dec_OneFrameErrorHandler(void *ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    decode_one_frame_err_handle((DecoderParams *)ptDecHandle, ptFrame);
}


#if USE_FEED_BACK
/*
 * Feedback the outputed buffer list to low level driver, and get the released buffer list
 */
int H264Dec_FeedBackUsedBuffer(void *ptDecHandle, FAVC_DEC_FEEDBACK *ptFeedback)
{
    /*
     * The function dose two things:
     * 1. pass the list of buffers that no longer used by application to lowwer level via ptFeedback->tRetBuf
     * 2. get the list of buffers that can be releasd (not used by the applicaion and the lowwer level driver)
     *    by the decoder driver via ptFeedback->u8ReleaseBuffer/u8ReleaseBufferNum
     */
    return decoder_feedback_buffer_used_done(ptDecHandle, ptFeedback);
}
#endif


/*
 * Flush un-outputed buffer in low level driver and get released buffer list
 */
int H264Dec_OutputAllPicture(void *ptDecHandle, FAVC_DEC_FRAME_IOCTL *ptFrame, int get_release_flg)
{
    return decoder_output_all_picture(ptDecHandle, ptFrame, get_release_flg);
}


#ifndef VG_INTERFACE
#if USE_LINK_LIST && USE_MULTICHAN_LINK_LIST
int H264Dec_OneFrame_Multichan_top(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle)
{
    int ret;
    DecoderParams *p_Dec = (DecoderParams *)ptDecHandle;
    
    decoder_clear_irq_flags(p_Dec);
    
    ret = decode_one_frame_ll_top(ptFrame, ptDecHandle);
    if(ret){
        printk("decode_one_frame_ll_top ch:%d ret:%d\n", p_Dec->chn_idx, ret);
        return ret;
    }
    
    return 0;
}

//#define DBG_MULTI_CHAN(fmt, args...)  printk(fmt, ## args)
#define DBG_MULTI_CHAN(args...)

int H264Dec_OneFrame_Multichan_trigger(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle)
{
    int ret;
    DecoderParams *p_Dec = (DecoderParams *)ptDecHandle;
    
    ret = decoder_trigger_ll_jobs(ptDecHandle);
    if(ret){
        printk("decoder_trigger_ll_jobs ch:%d ret:%d\n", p_Dec->chn_idx, ret);
        return ret;
    }

    if(p_Dec->codec_ll[0].sw_exec_flg == 0){
        DBG_MULTI_CHAN("H264Dec_OneFrame_Multichan_trigger wait ll %p ch:%d\n", p_Dec, p_Dec->chn_idx);
        decode_one_frame_block(p_Dec, ptFrame);
        DBG_MULTI_CHAN("H264Dec_OneFrame_Multichan_trigger wait ll %p done ch:%d\n", p_Dec, p_Dec->chn_idx);
    }
    decoder_init_ll_jobs(ptDecHandle);
    return 0;
}


int H264Dec_OneFrame_Multichan_bottom(FAVC_DEC_FRAME_IOCTL *ptFrame, void * ptDecHandle)
{
    int ret;

    ret = decode_one_frame_ll_bottom(ptFrame, ptDecHandle);
    if(ret < 0){
        printk("decode_one_frame_ll_bottom ret:%d\n", ret);
        return ret;
    }
    DBG_MULTI_CHAN("H264Dec_OneFrame_Multichan_bottom ch:%d ret:%d\n", p_Dec->chn_idx, ret);
    return 0;
}
#endif // USE_LINK_LIST
#endif // !VG_INTERFACE

/*
 * Allocate memory for ptDecHandle
 */
void *H264Dec_Create(const FAVCD_DEC_INIT_PARAM *ptParam, int ndev)
{
    return decoder_create(ptParam, ndev);
}

/*
 * Release memory for ptDecHandle
 */
void H264Dec_Release(void *ptDecHandle)
{
    decoder_destroy(ptDecHandle);
}

#ifndef VG_INTERFACE
/*
 * Reinit DPB
 */
void H264Dec_ClearDPB(void *ptDecHandle)
{
    init_dpb(ptDecHandle, 0);
}
#endif


/*
 * Dispach the received IRQ of an engine to per-channel data
 */
int H264Dec_DispatchIRQ(void *ptDecHandle, void *ptEngInfo)
{
    return decoder_eng_dispatch_irq(ptDecHandle, ptEngInfo);
}


/*
 * Receive (store and clear) HW's interrupt flags
 */
int H264Dec_ReceiveIRQ(void *ptEngInfo)
{
    return decoder_eng_receive_irq(ptEngInfo);
}


#if 0 // disable H264Dec_ClearIRQ()
/*
 * Clear dispatched IRQ flags in per-channel data
 */
void H264Dec_ClearIRQ(void *ptDecHandle)
{
    decoder_clear_irq_flags(ptDecHandle);
}
#endif

#if USE_LINK_LIST
int H264Dec_receive_irq_ll(void *ptEngInfo, void *ptCodecLL)
{
    DecoderParams *p_Dec;
    DecoderEngInfo *eng_info = ptEngInfo;
    struct codec_link_list *codec_ll = ptCodecLL;

    /* get IRQ flags and dispatch to the corrsponding dec_handle */
    if(codec_ll->sw_exec_flg == 1){
        /* SW linked list, dispatch to one dec_handle */
        p_Dec = codec_ll->jobs[codec_ll->exe_job_idx].dec_handle;
        DUMP_MSG_G(D_LL, "get irq for chn_idx:%d\n", p_Dec->chn_idx);
        //decoder_eng_receive_irq(p_Dec);
        H264Dec_DispatchIRQ(p_Dec, ptEngInfo);
        DUMP_MSG_G(D_LL, "int sts:%08X\n", p_Dec->u32IntSts);
    }else{
        /* HW linked list, dispatch to dec_handle associated to all executed jobs */
        int i;
        int job_idx = -1;
        unsigned int id;

        id = decoder_get_ll_job_id(eng_info);
        DUMP_MSG_G(D_LL, "get job id:%d\n", id);
        for(i = 0; i < codec_ll->job_num; i++){
            if(codec_ll->jobs[i].job_id == id){
                job_idx = i;
                p_Dec = codec_ll->jobs[job_idx].dec_handle;
                DUMP_MSG_G(D_LL, "job found: %d/%d\n", i, codec_ll->job_num);
                //decoder_eng_receive_irq(p_Dec);
                H264Dec_DispatchIRQ(p_Dec, ptEngInfo);
                break;
            }
        }

        /* dispatch implicit IRQ */
        for(i = 0; i < job_idx; i++){
            p_Dec = codec_ll->jobs[i].dec_handle;
            p_Dec->u32IntSts = p_Dec->u32IntStsPrev = INT_SLICE_DONE;
        }

        /* error handling for i > job_idx */
        for(i = job_idx + 1; i < codec_ll->job_num; i++){
            p_Dec = codec_ll->jobs[i].dec_handle;
            p_Dec->u32IntSts = p_Dec->u32IntStsPrev = INT_HW_TIMEOUT;
        }
            
        
    }
    //return decoder_eng_receive_irq(ptDecHandle);
    return 0;
}
#endif


/*
 * Do sw reset and re-init register/SRAM for the engine
 * NOTE: Reset HW only. No SW data structure is reset.
 */
int H264Dec_ResetEgnine(void *ptEngInfo)
{
    DecoderEngInfo *eng_info = ptEngInfo;
    decoder_eng_reset(eng_info);
    decoder_eng_init_reg(eng_info, 1);
    decoder_eng_init_sram(eng_info);

    return 0;
}


/*
 * Do sw reset and re-init register/SRAM for the engine used by this channel
 * NOTE: Reset HW only. No SW data structure is reset.
 */
void H264Dec_Reset(void *ptDecHandle)
{
    decoder_reset(ptDecHandle);
}


/*
 * Dump register of the engine bound to the specified channel data
 */
void H264Dec_DumpReg(void *ptDecHandle, int level)
{
    DecoderParams *p_Dec = ptDecHandle;
    dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, level);
}


