#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>    /* HDIO_GETGEO */
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <mach/fmem.h> /* fmem_dcache_sync() */

#include "favc_dev.h"
#include "h264d_ioctl.h"
#include "dec_driver/H264V_dec.h"
#include "dec_driver/portab.h"
#include "dec_driver/define.h"
#include "dec_driver/global.h"


#define FAVC_DECODER_MINOR  30
#define ENABLE_CLEAN_DCACHE 0

#define MAX_DEC_NUM         2 /* MAX number of decoder can be opened at a time */
#define DEC_IDX_MASK_n(n)  (0x1 << n)
#define DEC_IDX_FULL       ((1 << MAX_DEC_NUM) - 1) // a n-bit mask, where n is MAX_DEC_NUM. NOTE: MAX_DEC_NUM must be less then 32
static int dec_idx = 0; /* an n-bit flags to indicate which dec_data has been allocated */
#if USE_MULTICHAN_LINK_LIST
#define FIX_USE_CNT 2
static int dec_use_cnt = FIX_USE_CNT;
#endif


typedef struct {
    int           is_used[MAX_DEC_BUFFER_NUM];
    unsigned int  start_va[MAX_DEC_BUFFER_NUM];
    unsigned int  start_pa[MAX_DEC_BUFFER_NUM];
    unsigned int  mbinfo_va[MAX_DEC_BUFFER_NUM];
    unsigned int  mbinfo_pa[MAX_DEC_BUFFER_NUM];
    unsigned int  scale_va[MAX_DEC_BUFFER_NUM];
    unsigned int  scale_pa[MAX_DEC_BUFFER_NUM];
} BufferAddrList;

struct dec_private {
    int            dev;              /* an ID for each decoder open. range from 0 to (MAX_DEC_NUM - 1). also a index to dec_data */
    int            engine_idx;
    int            used_buffer_num;
    DecoderParams *dec_handle;       /* a pointer to the encapsulated data structure used in low level driver (decoder.c) */
    unsigned int   Y_sz;             /* Y plane size in bytes. caculated from the following field: u16MaxWidth * u16MaxHeight */
    u16            u16MaxWidth;
    u16            u16MaxHeight;
    BufferAddrList dec_buf;
    enum buf_type_flags buffer_type; // It consists of 2 flags. bit 1: scale enable, bit 0: support B slice (has mbinfo)
    struct vm_area_struct *vma;
    struct semaphore dec_mutex;      /* mutex for channel data */
};

static struct dec_private dec_data[MAX_DEC_NUM] = {{0}};

extern unsigned int mcp300_max_width;
extern unsigned int mcp300_max_height;
extern unsigned int mcp300_engine_en;
unsigned int mcp300_bs_virt[MAX_DEC_NUM] = {0};
unsigned int mcp300_bs_phy[MAX_DEC_NUM] = {0};
unsigned int mcp300_bs_length[MAX_DEC_NUM] = {0};

#define PER_ENGINE_MBINFO 0 /* alloated but not used */
#if PER_ENGINE_MBINFO
unsigned int mcp300_mbinfo_virt[FTMCP300_NUM] = {0};
unsigned int mcp300_mbinfo_phy[FTMCP300_NUM] = {0};
unsigned int mcp300_mbinfo_length[FTMCP300_NUM] = {0};
#endif

DecoderEngInfo favcd_engine_info[FTMCP300_NUM];
#if USE_LINK_LIST
struct codec_link_list *codec_ll[FTMCP300_NUM] = { NULL };
unsigned int codec_ll_va[FTMCP300_NUM];
unsigned int codec_ll_pa[FTMCP300_NUM];
#endif



#if(FTMCP300_NUM == 2)
#define SWITCH_ENGINE 0 // define 1 to switch engines at run time
#else
#define SWITCH_ENGINE 0 // define 1 to switch engines at run time
#endif


struct semaphore favc_dec_mutex;
struct semaphore favc_eng_mutex[FTMCP300_NUM];
wait_queue_head_t mcp300_wait_queue;
#if USE_MULTICHAN_LINK_LIST
wait_queue_head_t mcp300_ll_wait_queue;
wait_queue_head_t mcp300_job_wait_queue;
#endif
#if (LINUX_VERSION_CODE == KERNEL_VERSION(3,3,0))
static DEFINE_SPINLOCK(avcdec_request_lock); // SPIN_LOCK_UNLOCKED has been deprecated since 2.6.19 and will get removed in 2.6.39
#else
static spinlock_t avcdec_request_lock = SPIN_LOCK_UNLOCKED;
#endif


#define WAIT_EVENT  (p_Dec->u32IntSts)
static int favc_decoder_wait_interrupt(struct dec_private *decp)
{
    DecoderParams *p_Dec = decp->dec_handle;
    extern unsigned int sw_timeout_delay;
    
    wait_event_timeout(mcp300_wait_queue, WAIT_EVENT, msecs_to_jiffies(sw_timeout_delay));  /* fixed timout */
    if (!WAIT_EVENT) {
        return H264D_ERR_SW_TIMEOUT;
    }
    return H264D_OK;
}


int favc_decoder_decode_one_frame(struct dec_private *decp, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    DecoderParams *p_Dec = decp->dec_handle;
    int ret;

    ret = H264Dec_OneFrameStart(p_Dec, ptFrame);
    if(ret != H264D_OK)
        return ret;

    while(1){
        ret = favc_decoder_wait_interrupt(decp);
        if(ret != H264D_OK){ /* software timeout */
            F_DEBUG("software timeout error: ch:%d event:%02X\n", p_Dec->chn_idx, WAIT_EVENT);
            H264Dec_OneFrameErrorHandler(p_Dec, ptFrame);
            break;
        }
        ret = H264Dec_OneFrameNext(p_Dec, ptFrame);
        if(ret == FRAME_DONE) /* one frame is decoded successfully */
            return FRAME_DONE;

        if(ret < 0){ /* some error occurred */
            break;
        }

        /* continue at SLICE_DONE or FIELD_DONE */
        //if(ret == H264D_OK)
        if(ret == SLICE_DONE || ret == FIELD_DONE || ret == H264D_OK)
        {
            continue;
        }

        F_DEBUG("unexpected H264Dec_OneFrameNext() return:%d ch:%d\n", ret, p_Dec->chn_idx);
    }

    return ret;
}


/*
 * open driver function
 */
int favc_decoder_open(struct inode *inode, struct file *filp)
{
    int idx;
    int minor;
    int engine_idx;
    struct dec_private *decp;

    if ((dec_idx & DEC_IDX_FULL) == DEC_IDX_FULL) {
        F_DEBUG("Decoder Device Service Full,0x%x!\n", dec_idx);
        return -EFAULT;
    }

    /* select engine according to the minor num of opened dev node */
    minor = iminor(inode);
    engine_idx = minor - FAVC_DECODER_MINOR;
    if((mcp300_engine_en & (1 << engine_idx)) == 0){
        F_DEBUG("engine is not enabled: %d (enabled engine bit mask:%x)", engine_idx, mcp300_engine_en);
        return -EFAULT;
    }


    /* find an unused channel and mark it as used */
    down(&favc_dec_mutex);
    for (idx = 0; idx < MAX_DEC_NUM; idx++) {
        if ((dec_idx & DEC_IDX_MASK_n(idx)) == 0) {
            dec_idx |= DEC_IDX_MASK_n(idx);
#if USE_MULTICHAN_LINK_LIST
#if FIX_USE_CNT == 0
            down(&favc_eng_mutex[engine_idx]); /* lock engine */
            dec_use_cnt++;
            up(&favc_eng_mutex[engine_idx]); /* lock engine */
#endif
#endif
            break;
        }
    }
    up(&favc_dec_mutex);
    
    decp = (struct dec_private *)&dec_data[idx];

    decp->dev = idx;
    decp->engine_idx = engine_idx;
    decp->vma = NULL;
    filp->private_data = decp;
    
    return 0; /* success */
}


/*
 * map physical address of reconstructed buffer and scale buffer to virtual address in user space
 * buf_idx is the index of the reconstructed buffer of decp->dec_buf array
 */
int mmap_remap(struct dec_private *decp, int buf_idx)
{
    int addr, buffer_size, ref_buf_size;
    unsigned long pfn;
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif
    struct vm_area_struct *vma = decp->vma;

    /* caculate buffer size to determine address offset */
    ref_buf_size = buffer_size = PAGE_ALIGN(decp->Y_sz * 2);

	/*
	 * if scaling is enabled, buffer_size == ref_buf_size + scale_buf_size. 
	 * otherwise, buffer_size == ref_buf_size
	 */
    if (decp->buffer_type & FR_BUF_SCALE){
        buffer_size += PAGE_ALIGN(decp->Y_sz / 2);
    }
    
    pfn = decp->dec_buf.start_pa[buf_idx] >> PAGE_SHIFT;
    addr = vma->vm_start + buf_idx * buffer_size;

    /* make the user virtual address non-cacheable. 
       NOTE: If it is cacheable, cache invalidate for the user virtual address 
       must be taken care in driver */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); 
    if (remap_pfn_range(vma, addr, pfn, ref_buf_size, vma->vm_page_prot))
        return -EAGAIN;
    
    DUMP_MSG(D_BUF_IOCTL, dec_handle, "ref remap %08x (%08x size %x)\n", addr, decp->dec_buf.start_pa[buf_idx], ref_buf_size); 

    /* if scaling is enabled, do the mapping for scale buffer */
    if (decp->buffer_type & FR_BUF_SCALE) {
        int scale_buf_size = PAGE_ALIGN(decp->Y_sz/2);
        pfn = decp->dec_buf.scale_pa[buf_idx] >> PAGE_SHIFT;
        addr = vma->vm_start + buf_idx * buffer_size + ref_buf_size;
        
        /* make the user virtual address non-cacheable. 
           NOTE: If it is cacheable, cache invalidate for the user virtual address 
           must be taken care in driver */
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        if (remap_pfn_range(vma, addr, pfn, scale_buf_size, vma->vm_page_prot))
            return -EAGAIN;

        DUMP_MSG(D_BUF_IOCTL, dec_handle, "scale remap %08x (%08x size %x)\n", addr, decp->dec_buf.scale_pa[buf_idx], scale_buf_size); 
    }

    return 0;
}


/*
 * get an unused buffer index from memory pool, allocate frame buffer and map it to user space
 * pBuf: for storing the allocated buffer
 * return value: buf_idx
 */
int get_unused_buffer(struct dec_private *decp, FAVC_DEC_BUFFER *pBuf)
{
    MCP300_Buffer buf_info;
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif
    int i, idx;
    
    
    if(SHOULD_DUMP(D_BUF_IOCTL, dec_handle))
    {
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "used buf(%d):", decp->used_buffer_num);
        for (i = 0; i<MAX_DEC_BUFFER_NUM; i++) {
            DUMP_MSG(D_BUF_IOCTL, dec_handle, " %d", decp->dec_buf.is_used[i]);
            if ((i%4)==3)
                DUMP_MSG(D_BUF_IOCTL, dec_handle, " |");
        }
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
    }
    
    idx = -1;
    /* find an unused buffer */
    for (i = 0; i<MAX_DEC_BUFFER_NUM; i++) {
        if (!decp->dec_buf.is_used[i]) {
            idx = i;
            break;
        }
    }

    if (idx < 0) { /* can not find an unused buffer */
        H264Dec_ClearDPB(decp->dec_handle);       
        for (i = 0; i<MAX_DEC_BUFFER_NUM; i++)
            decp->dec_buf.is_used[i] = 0;
        F_DEBUG("reconstruct buffer is full (release all buffer)\n");
        return -1;
    }

    if (decp->dec_buf.start_va[idx] == 0){
        if (allocate_frame_buffer(&buf_info, decp->Y_sz, decp->buffer_type) < 0){
            F_DEBUG("allocate frame buffer failed\n");
            return -1;
        }
        
        decp->dec_buf.start_va[idx] = buf_info.ref_va;
        decp->dec_buf.start_pa[idx] = buf_info.ref_pa;
        decp->dec_buf.mbinfo_va[idx] = buf_info.mbinfo_va;
        decp->dec_buf.mbinfo_pa[idx] = buf_info.mbinfo_pa;
        decp->dec_buf.scale_va[idx] = buf_info.scale_va;
        decp->dec_buf.scale_pa[idx] = buf_info.scale_pa;

        if (mmap_remap(decp, idx) < 0){
            release_frame_buffer(&buf_info);
            F_DEBUG("mmap_remap failed\n");
            return -1;
        }
    }

    decp->used_buffer_num++;
    decp->dec_buf.is_used[idx] = 1; // mark buffer as used
    pBuf->buffer_index = idx;

    pBuf->dec_yuv_buf_virt = (unsigned char*)decp->dec_buf.start_va[idx];
    pBuf->dec_yuv_buf_phy = (unsigned char*)decp->dec_buf.start_pa[idx];
    pBuf->dec_mbinfo_buf_virt = (unsigned char*)decp->dec_buf.mbinfo_va[idx];
    pBuf->dec_mbinfo_buf_phy = (unsigned char*)decp->dec_buf.mbinfo_pa[idx];
    pBuf->dec_scale_buf_virt = (unsigned char*)decp->dec_buf.scale_va[idx];
    pBuf->dec_scale_buf_phy = (unsigned char*)decp->dec_buf.scale_pa[idx];

    //printk("allocate buffer: used buffer = %d\n", decp->used_buffer_num);
    
    DUMP_MSG(D_BUF_IOCTL, dec_handle, "ref addr: %x(%x), mbinfo: %x(%x)", (unsigned int)pBuf->dec_yuv_buf_virt, (unsigned int)pBuf->dec_yuv_buf_phy, 
        (unsigned int)pBuf->dec_mbinfo_buf_virt, (unsigned int)pBuf->dec_mbinfo_buf_phy);
    if (decp->buffer_type & FR_BUF_SCALE)
        DUMP_MSG(D_BUF_IOCTL, dec_handle, ", sacle: %x(%x)\n", (unsigned int)pBuf->dec_scale_buf_virt, (unsigned int)pBuf->dec_scale_buf_phy);
    else
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
    
    //memset(pBuf->dec_yuv_buf_virt, 0, decp->Y_sz*3);
    
    return idx;
}



/* 
 * check if decoder parameters are valid 
 */
static int dec_pre_check(FAVC_DEC_PARAM *ppub)
{
	/* check if API version matches */
    if((ppub->u32API_version & 0xFFFFFF00) != (H264D_VER & 0xFFFFFF00)) 
    {
        F_DEBUG("Fail API Version v%d.%d.%d (Current Driver v%d.%d.%d)\n",
                H264D_VER_MAJOR_N(ppub->u32API_version),
                H264D_VER_MINOR_N(ppub->u32API_version),
                H264D_VER_MINOR2_N(ppub->u32API_version),
                H264D_VER_MAJOR, H264D_VER_MINOR, H264D_VER_MINOR2);
        F_DEBUG("Please upgrade your H264 driver and re-compile AP.\n");
        return H264D_ERR_API;
    }
    
	/* check if the video width/height exceed the limit set in driver */
    if ((ppub->u16MaxWidth * ppub->u16MaxHeight) > (mcp300_max_width * mcp300_max_height)) {
        F_DEBUG("input max size over capacity (%dx%d > %dx%d)\n", 
            ppub->u16MaxWidth, ppub->u16MaxHeight, mcp300_max_width, mcp300_max_height);
        return H264D_SIZE_OVERFLOW;
    }
    
    return H264D_OK;
}


/*
 * Prepare the decoder parameter (pParam) to low level driver and reset driver's private data (decp)
 * according to decoder parameter from application (pParam) and module parameters
 */
static int reset_dec_private(struct dec_private *decp, FAVC_DEC_PARAM *pParam, unsigned int engine_idx)
{
    enum buf_type_flags new_buffer_type;
    extern unsigned int vc_cfg;


    /* overwirte vcache config */
    pParam->bVCCfg = vc_cfg;

	/* if max width/height is not specified, use default in driver instead */
    if ((pParam->u16MaxWidth == 0) || (pParam->u16MaxHeight == 0)) {
        pParam->u16MaxWidth = mcp300_max_width;
        pParam->u16MaxHeight = mcp300_max_height;
    }

    if(decp->used_buffer_num != 0){
        F_DEBUG("init decoder without cleaning up buffer pool is do not allowed:%d\n", decp->used_buffer_num);
        return -EFAULT;
    }

	/* initialize buffer pool in decoder private data */
    decp->used_buffer_num = 0;
    memset(&decp->dec_buf, 0, sizeof(decp->dec_buf));

    new_buffer_type = 0;
    new_buffer_type |= pParam->fr_param.bScaleEnable?FR_BUF_SCALE:0;
    new_buffer_type |= pParam->bUnsupportBSlice?0:FR_BUF_MBINFO;

    decp->Y_sz = pParam->u16MaxWidth * pParam->u16MaxHeight;
    decp->buffer_type = new_buffer_type;
    decp->u16MaxWidth = pParam->u16MaxWidth;
    decp->u16MaxHeight = pParam->u16MaxHeight;

    if (pParam->bUnsupportBSlice) {
        /* allocate per-engine mbinfo buffer */
        /* if not support B, display order = output order */
        /* NOTE: using per-engine mbinfo buffer is not implemented */
        F_DEBUG("unsupport b\n");
#if PER_ENGINE_MBINFO
        mcp300_mbinfo_length[engine_idx] = PAGE_ALIGN(mcp300_max_width * mcp300_max_height);
        if (mcp300_mbinfo_virt[engine_idx] == 0) {
            if (allocate_dec_buffer(&mcp300_mbinfo_virt[engine_idx], &mcp300_mbinfo_phy[engine_idx], mcp300_mbinfo_length[engine_idx]) < 0) {
                release_dec_buffer(mcp300_mbinfo_virt[engine_idx], mcp300_mbinfo_phy[engine_idx]);
                return H264D_ERR_MEMORY;
            }
        }
#endif
    }
#if 0
    printk("buffer type = %d\n", decp->buffer_type);
    printk("bScaleEnable:%d\n", pParam->bScaleEnable);
    printk("bUnsupportBSlice:%d\n", pParam->bUnsupportBSlice);
#endif
    

    return 0;
}



/*
 * free buffers specified in the release_list array
 */
static int release_used_up_buffer(struct dec_private *decp, unsigned char *release_list, unsigned char release_num)
{
    int idx;
    int ret = 0;
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif

    if (release_num > MAX_DEC_BUFFER_NUM || release_num == 0)
        return 0;
        
    for (idx = 0; idx < release_num; idx++) {
        if(decp->dec_buf.is_used[release_list[idx]])
            decp->used_buffer_num--;
        decp->dec_buf.is_used[release_list[idx]] = 0;
        //printk("ioctl release used up buffer: used buffer = %d\n", decp->used_buffer_num);
    }
    
    if(SHOULD_DUMP(D_BUF_IOCTL, dec_handle))
    {
        int i;
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "release done buf:");
        for (i = 0; i < MAX_DEC_BUFFER_NUM; i++) {
            DUMP_MSG(D_BUF_IOCTL, dec_handle, " %d", decp->dec_buf.is_used[i]);
            if ((i%4)==3)
                DUMP_MSG(D_BUF_IOCTL, dec_handle, " |");
        }
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
    }

    return ret;
}

#if ENABLE_CLEAN_DCACHE
static void _clean_dcache(void *va, unsigned int size)
{
#if 1
    fmem_dcache_sync(va, size, DMA_TO_DEVICE);
#else
    /* clean dcache if the virtual address is kernel logical address (so it must be cacheable and needed to be clean) */
    if(virt_addr_valid(va)){
        dma_addr_t dma_addr;
        dma_addr = dma_map_single(NULL, va, size, DMA_TO_DEVICE);
        /* get access right back */
        dma_unmap_single(NULL, dma_addr, size, DMA_TO_DEVICE);
    }
#endif
}
#endif

/*
 * set bitstream buffer address and size to low level driver
 */
static int SetBitstream(struct dec_private *decp, FAVC_DEC_FRAME *pFrame)
{

    if (H264Dec_SetBitstreamBuf(decp->dec_handle, mcp300_bs_phy[decp->dev], mcp300_bs_virt[decp->dev], pFrame->u32BSLength)<0) {
        F_DEBUG("copy move and refilll bs error\n");
        return -1;
    }

    return 0;
}

/*
 * copy bitstream from user space to kernel space
 * (pad EOS bytes at the end of bitstream according to module parameter)
 */
static int FillBitstream(struct dec_private *decp, FAVC_DEC_FRAME *pFrame)
{
    const unsigned char NaluEndOfStream[5] = {0x00, 0x00, 0x00, 0x01, 0x0B};
    unsigned int padded_size;
    unsigned int padding_bytes;
    extern unsigned int pad_bytes;

    if(pad_bytes <= sizeof(NaluEndOfStream)){
        padding_bytes = pad_bytes;
    }else{
        padding_bytes = sizeof(NaluEndOfStream);
    }

    padded_size = pFrame->u32BSLength + padding_bytes;
    if (padded_size > mcp300_bs_length[decp->dev]) {
        F_DEBUG("bitstream size over limit, 0x%x (0x%x + 0x%x)> 0x%x\n", padded_size, pFrame->u32BSLength, pad_bytes, mcp300_bs_length[decp->dev]);
        return -1;
    }

    
    if(copy_from_user((void *)mcp300_bs_virt[decp->dev], (void *)pFrame->u32BSBuf_virt, pFrame->u32BSLength)){
        F_DEBUG("copy from user error in FillBuffer\n");
        return -1;
    }
    memcpy((void *)(mcp300_bs_virt[decp->dev] + pFrame->u32BSLength), NaluEndOfStream, padding_bytes);

#if ENABLE_CLEAN_DCACHE /* if dec_data->in_addr_va is always NCNB, this can be disabled */
    _clean_dcache((void *)mcp300_bs_virt[decp->dev], padded_size);
#endif /* ENABLE_CLEAN_DCACHE */
        

    return 0;
}


int favc_dec_print_used_buf(struct dec_private *decp)
{
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif
    
    if(SHOULD_DUMP(D_BUF_IOCTL, dec_handle))
    {
        int i;
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "dec_frame buffer: ");
        for (i = 0; i<MAX_DEC_BUFFER_NUM; i++) {
            DUMP_MSG(D_BUF_IOCTL, dec_handle, "%d ", decp->dec_buf.is_used[i]);
            if ((i%4) == 3)
                DUMP_MSG(D_BUF_IOCTL, dec_handle, "| ");
        }
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
    }

    return 0;
}


int favc_dec_print_buf(struct dec_private *decp, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif
    
    if(SHOULD_DUMP(D_BUF_IOCTL, dec_handle))
    {
        FAVC_DEC_FRAME *ptAPFrame = &ptFrame->apFrame;
        int i;
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "dec_frame release buffer[%d]: ", ptFrame->u8ReleaseBufferNum);
        for (i = 0; i < ptFrame->u8ReleaseBufferNum; i++)
            DUMP_MSG(D_BUF_IOCTL, dec_handle, "%d ", ptFrame->u8ReleaseBuffer[i]);
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
        
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "output buffer[%d]: ", ptAPFrame->u8OutputPicNum);
        for (i = 0; i < ptAPFrame->u8OutputPicNum; i++)
            DUMP_MSG(D_BUF_IOCTL, dec_handle, "%d(%d) ", ptAPFrame->mOutputFrame[i].i16POC, ptAPFrame->mOutputFrame[i].i8BufferIndex);
        DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
        return 0;
    }
    return 0;
}


int favc_dec_switch_engine(struct dec_private *decp)
{
    if(decp->engine_idx == 0){
        decp->engine_idx = 1;
    }else{
        decp->engine_idx = 0;
    }
    F_DEBUG("-----------switch_engine %d--------------\n", decp->engine_idx);

    return 0;
}


/*
 * set dump message start/end frame number
 */
void favc_dec_set_dump_cond(DecoderParams *dec_handle)
{
    extern int dump_start;
    extern int dump_end;
    extern unsigned int dump_cond;
    dec_handle->u32DumpFrStart = dump_start;
    dec_handle->u32DumpFrEnd = dump_end;
    dec_handle->u32DumpCond = dump_cond;
    return;
}

/*
 * start of ioctl functions
 */
int favc_dec_ioctl_init(struct dec_private *decp, unsigned long arg)
{
    FAVC_DEC_PARAM tDecParam; /* version, height, width */
    int ret;

    if(copy_from_user((void *)&tDecParam, (void *)arg, sizeof(FAVC_DEC_PARAM))) {
        return -EFAULT;
    }
    if ((ret = dec_pre_check(&tDecParam)) < 0) {
        F_DEBUG("dec_pre_check error: %d\n", ret);
        return -EFAULT;
    }
    if ((ret = reset_dec_private(decp, &tDecParam, decp->engine_idx)) < 0) {
        F_DEBUG("reset_dec_private error: %d\n", ret);
        return -EFAULT;
    }
    if ((ret = H264Dec_Init(&tDecParam, decp->dec_handle)) < 0) {
        F_DEBUG("H264Dec_Init error: %d\n", ret);
        return -EFAULT;
    }

    favc_dec_set_dump_cond(decp->dec_handle);

    return 0;
}


//#define DBG_MULTI_CHAN_WQ(fmt, args...)  printk(fmt, ## args)
#define DBG_MULTI_CHAN_WQ(args...)
int favc_dec_ioctl_dec_frame(struct dec_private *decp, unsigned long arg)
{
    FAVC_DEC_FRAME_IOCTL tFrame;
    FAVC_DEC_FRAME *ptAPFrame = &tFrame.apFrame;    // bitstream, output
    FAVC_DEC_BUFFER tRecBuf;    // reconstructed buffer
    int ret;
    DecoderParams *dec_handle = decp->dec_handle;
    extern unsigned int mcp300_sw_reset;

#if USE_MULTICHAN_LINK_LIST
    static int dec_cnt = 0;
    static int trig_cnt = 0;
    int curr_dec_cnt;
    int trigger_flg;
#endif

    tRecBuf.buffer_index = -1; /* mark it as unused */
    tFrame.u8ReleaseBufferNum = 0;
    tFrame.bPicStored = 0;
    tFrame.apFrame.u8OutputPicNum = 0;


    if(copy_from_user((void *)ptAPFrame, (void *)arg, sizeof(FAVC_DEC_FRAME))) {
        return -EFAULT;
    }
    if (decp->dec_handle == NULL) {
        F_DEBUG("decoder is not initialized\n");
        ret = -EFAULT;
        ptAPFrame->i32ErrorMessage = H264D_ERR_DECODER_NULL;
        goto decode_frame_exit;
        //goto decoder_ioctl_exit;
    }

    if (FillBitstream(decp, ptAPFrame) < 0) {
        F_DEBUG("fill bitstream error\n");
        ptAPFrame->i32ErrorMessage = H264D_BS_ERROR;
        ret = -EFAULT;
        goto decode_frame_exit;
    }

    if (get_unused_buffer(decp, &tRecBuf) < 0) {
        F_DEBUG("allocate reconstructed buffer error\n");
        ptAPFrame->i32ErrorMessage = H264D_ERR_BUFFER_FULL;
        ret = -EFAULT;
        goto decode_frame_exit;
    }

#if SWITCH_ENGINE
    favc_dec_switch_engine(decp); /* switch engine */
#endif /* SWITCH_ENGINE */


#if USE_MULTICHAN_LINK_LIST == 0
    down(&favc_eng_mutex[decp->engine_idx]); /* lock engine */
    H264Dec_BindEngine(decp->dec_handle, &favcd_engine_info[decp->engine_idx]);
    if(mcp300_sw_reset){
        H264Dec_Reset(decp->dec_handle);
    }
    
    if (SetBitstream(decp, ptAPFrame) < 0) {
        F_DEBUG("fill bitstream error\n");
        ptAPFrame->i32ErrorMessage = H264D_BS_ERROR;
        ret = -EFAULT;
        goto unlock_engine;
    }
    
    DUMP_MSG(D_BUF_IOCTL, dec_handle, "assign buffer: %d\n", tRecBuf.buffer_index);
        
    tFrame.ptReconstBuf = &tRecBuf;

#if SWITCH_PMU_EACH_FR
    printk("reset mcp300 engine for ch:%d\n", decp->dev);
    pf_mcp300_clk_off();
    pf_mcp300_clk_on();
#endif

    ret = favc_decoder_decode_one_frame(decp, &tFrame);

unlock_engine:    
    H264Dec_UnbindEngine(decp->dec_handle);
    up(&favc_eng_mutex[decp->engine_idx]); /* unlock engine */
    
#else /* USE_MULTICHAN_LINK_LIST==1 */

    H264Dec_BindEngine(decp->dec_handle, &favcd_engine_info[decp->engine_idx]);
    if(mcp300_sw_reset){
        H264Dec_Reset(decp->dec_handle);
    }

    if (SetBitstream(decp, ptAPFrame) < 0) {
        F_DEBUG("fill bitstream error\n");
        ptAPFrame->i32ErrorMessage = H264D_BS_ERROR;
        ret = -EFAULT;
        goto unbind_engine;
    }
    
    DUMP_MSG(D_BUF_IOCTL, dec_handle, "assign buffer: %d\n", tRecBuf.idx);

    tFrame.ptReconstBuf = &tRecBuf;

    down(&favc_eng_mutex[decp->engine_idx]); /* lock engine */
    ret = H264Dec_OneFrame_Multichan_top(&tFrame, decp->dec_handle);
    
    trigger_flg = (dec_cnt == trig_cnt)?1:0;
    dec_cnt++;
    curr_dec_cnt = dec_cnt;
    if(curr_dec_cnt - trig_cnt == dec_use_cnt){
        wake_up(&mcp300_job_wait_queue);
    }
    up(&favc_eng_mutex[decp->engine_idx]); /* unlock engine */

    /* wait until job number == N */
    DBG_MULTI_CHAN_WQ("dev:%d wait 1 dec_cnt:%d trig_cnt:%d\n", decp->dev, curr_dec_cnt, trig_cnt);
    wait_event_timeout(mcp300_job_wait_queue, (curr_dec_cnt - trig_cnt == dec_use_cnt) || (curr_dec_cnt == trig_cnt), msecs_to_jiffies(3000));  /* fixed timout */
    DBG_MULTI_CHAN_WQ("dev:%d wait 1 dec_cnt:%d trig_cnt:%d done\n", decp->dev, curr_dec_cnt, trig_cnt);

    if(ret == 0){
        down(&favc_eng_mutex[decp->engine_idx]); /* lock engine */
        if(trigger_flg){
            DBG_MULTI_CHAN_WQ("dev:%d trig 2\n", decp->dev);
            H264Dec_OneFrame_Multichan_trigger(&tFrame, decp->dec_handle);
            DBG_MULTI_CHAN_WQ("dev:%d trig 2 done\n", decp->dev);
        }
        trig_cnt++;
        wake_up(&mcp300_ll_wait_queue);
        wake_up(&mcp300_job_wait_queue);
        up(&favc_eng_mutex[decp->engine_idx]); /* unlock engine */

        /* wait until current job is triggered */
        DBG_MULTI_CHAN_WQ("dev:%d wait 2 dec_cnt:%d trig_cnt:%d\n", decp->dev, curr_dec_cnt, trig_cnt);
        wait_event_timeout(mcp300_ll_wait_queue, (curr_dec_cnt <= trig_cnt), msecs_to_jiffies(3000));  /* fixed timout */
        DBG_MULTI_CHAN_WQ("dev:%d wait 2 dec_cnt:%d trig_cnt:%d done\n", decp->dev, curr_dec_cnt, trig_cnt);
        ret = H264Dec_OneFrame_Multichan_bottom(&tFrame, decp->dec_handle);
    }
    
unbind_engine:
    H264Dec_UnbindEngine(decp->dec_handle);
#endif

    if(ret < 0){
        ptAPFrame->i32ErrorMessage = ret;
        ret = -EFAULT; 
    }
    
decode_frame_exit:

    /* append buffer to release list if it is allocated and not stored into DPB */
    if(tRecBuf.buffer_index != -1 && tFrame.bPicStored == 0){
        tFrame.u8ReleaseBuffer[tFrame.u8ReleaseBufferNum++] = tRecBuf.buffer_index;
    }
    
    favc_dec_print_buf(decp, &tFrame); // print debug message
    release_used_up_buffer(decp, tFrame.u8ReleaseBuffer, tFrame.u8ReleaseBufferNum);
    favc_dec_print_used_buf(decp); // print debug message

    ptAPFrame->decoded_frame_num = dec_handle->u32DecodedFrameNumber;
    if(copy_to_user((void *)arg, (void *)ptAPFrame, sizeof(FAVC_DEC_FRAME))){
        F_DEBUG("copy to user error\n");
        return -EFAULT;
    }

    return ret;
}

#if USE_FEED_BACK
int favc_dec_ioctl_release_buffer(struct dec_private *decp, unsigned long arg)
{
    FAVC_DEC_FEEDBACK tFeedback;
    FAVC_DEC_RET_BUF *ptRetBuffer = &tFeedback.tRetBuf; // return buffer used done
    int ret = 0;
#if EN_DUMP_MSG
    DecoderParams *dec_handle = decp->dec_handle;
#endif

    if(copy_from_user((void*)ptRetBuffer, (void*)arg, sizeof(FAVC_DEC_RET_BUF))) {
        return -EFAULT;
    }
    
    if (H264Dec_FeedBackUsedBuffer(decp->dec_handle, &tFeedback)) {
        /* size of release buffer list from low level driver > 0 */
        if (SHOULD_DUMP(D_BUF_IOCTL, dec_handle))
        {
            int i;
            DUMP_MSG(D_BUF_IOCTL, dec_handle, "feedback release buffer[%d]: ", tFeedback.u8ReleaseBufferNum);
            for (i = 0; i < tFeedback.u8ReleaseBufferNum; i++)
                DUMP_MSG(D_BUF_IOCTL, dec_handle, "%d ", tFeedback.u8ReleaseBuffer[i]);
            DUMP_MSG(D_BUF_IOCTL, dec_handle, "\n");
        }
        
        ret = release_used_up_buffer(decp, tFeedback.u8ReleaseBuffer, tFeedback.u8ReleaseBufferNum);
        favc_dec_print_used_buf(decp); // print debug message
    }
    ptRetBuffer->i32ErrorMessage = ret;
    if(copy_to_user((void *)arg, (void *)ptRetBuffer, sizeof(FAVC_DEC_RET_BUF))){
        F_DEBUG("copy to user error\n");
        return -EFAULT; 
    }
    
    return 0;
}
#endif

int favc_dec_ioctl_output_all(struct dec_private *decp, unsigned long arg)
{
    FAVC_DEC_FRAME_IOCTL tAllReleaseFrame;
    FAVC_DEC_FRAME *ptAllFrame = &tAllReleaseFrame.apFrame;
    ptAllFrame->i8Valid = 0; /* to avoid a false warning from low level driver */

    H264Dec_OutputAllPicture(decp->dec_handle, &tAllReleaseFrame, 1);

    release_used_up_buffer(decp, tAllReleaseFrame.u8ReleaseBuffer, tAllReleaseFrame.u8ReleaseBufferNum);
    if(copy_to_user((void *)arg,(void *)ptAllFrame, sizeof(*ptAllFrame))){
        F_DEBUG("copy to user error\n");
        return -EFAULT; 
    }
    
    return 0;
}

/*
 * end of ioctl functions
 */


/*
 * ioctl driver function
 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
long favc_decoder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
int favc_decoder_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
    int ret = 0;
    struct dec_private *decp = (struct dec_private *)filp->private_data;
    int dev = decp->dev;
    
    DUMP_MSG(D_BUF_IOCTL, decp->dec_handle, "h264_decoder_ioctl:cmd=0x%x\n", cmd);
    
    if (((1 << dev) & dec_idx) == 0)    {
        F_DEBUG("No Support index %d for 0x%x\n", dev, dec_idx);
        return -EFAULT;
    }

    down(&decp->dec_mutex);
    switch(cmd) {
        case H264_IOCTL_DECODE_INIT:
            ret = favc_dec_ioctl_init(decp, arg);
            break;
        case H264_IOCTL_DECODE_FRAME:
            ret = favc_dec_ioctl_dec_frame(decp, arg);
            break;
        case H264_IOCTL_DECODE_RELEASE_BUFFER:
#if USE_FEED_BACK
            ret = favc_dec_ioctl_release_buffer(decp, arg);
#endif
            break;
        case H264_IOCTL_DECODE_OUTPUT_ALL:
            ret = favc_dec_ioctl_output_all(decp, arg);
            break;
        default:
            F_DEBUG("undefined ioctl cmd %x\n", cmd);
            ret = -EFAULT;
            break;
    }
    up(&decp->dec_mutex);
    
    return ret;
}


/*
 * mmap driver function
 * NOTE: the acutal mapping is defered to favc_dec_ioctl_dec_frame()
 */
int favc_decoder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct dec_private * decp = (struct dec_private *)filp->private_data;
    decp->vma = vma;

    return 0;
}


/*
 * release all used frame buffer in pool
 */
void favcd_decoder_release_used_frame(struct dec_private * decp)
{
    int i;
    for (i = 0; i< MAX_DEC_BUFFER_NUM; i++) {
        if (decp->dec_buf.start_va[i]) {
            MCP300_Buffer Buf;
            Buf.ref_va = decp->dec_buf.start_va[i];
            Buf.mbinfo_va = decp->dec_buf.mbinfo_va[i];
            Buf.scale_va = decp->dec_buf.scale_va[i];
            Buf.ref_pa = decp->dec_buf.start_pa[i];
            Buf.mbinfo_pa = decp->dec_buf.mbinfo_pa[i];
            Buf.scale_pa = decp->dec_buf.scale_pa[i];
            release_frame_buffer(&Buf);
            
            decp->dec_buf.start_va[i] = 0;
            decp->dec_buf.start_pa[i] = 0;
            decp->dec_buf.mbinfo_va[i] = 0;
            decp->dec_buf.mbinfo_pa[i] = 0;
            decp->dec_buf.scale_va[i] = 0;
            decp->dec_buf.scale_pa[i] = 0;
            if(decp->dec_buf.is_used[i]){
                decp->used_buffer_num--;
            }
            decp->dec_buf.is_used[i] = 0;
            DUMP_MSG(D_BUF_IOCTL, decp->dec_handle, "ioctl release buffer: %d used buffer = %d\n", i, decp->used_buffer_num);
        }
    }
}


/*
 * release driver function
 */
int favc_decoder_release(struct inode *inode, struct file *filp)
{
    struct dec_private * decp = (struct dec_private *)filp->private_data;
    int dev = decp->dev;
    
    down(&favc_dec_mutex);
    down(&decp->dec_mutex);
    filp->private_data = 0;

    favcd_decoder_release_used_frame(decp);

    dec_idx &= (~(DEC_IDX_MASK_n(dev)) );   
#if USE_MULTICHAN_LINK_LIST
#if FIX_USE_CNT == 0
    down(&favc_eng_mutex[decp->engine_idx]); /* lock engine */
    dec_use_cnt--;
    up(&favc_eng_mutex[decp->engine_idx]); /* lock engine */
#endif
#endif
    up(&decp->dec_mutex);
    up(&favc_dec_mutex);
    
    return 0;
}


static struct file_operations favc_decoder_fops = {
    .owner = THIS_MODULE,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
    .unlocked_ioctl = favc_decoder_ioctl,
#else
    .ioctl = favc_decoder_ioctl,
#endif
    .mmap = favc_decoder_mmap,
    .open = favc_decoder_open,
    .release = favc_decoder_release,
};
    
static struct miscdevice favc_decoder_dev[FTMCP300_NUM] = {
    {
        .minor = FAVC_DECODER_MINOR,
        .name = "favcdec0",
        .fops = &favc_decoder_fops,
    },
#if (FTMCP300_NUM == 2)
    {
        .minor = FAVC_DECODER_MINOR + 1,
        .name = "favcdec1",
        .fops = &favc_decoder_fops,
    },
#endif
};

#ifdef A369
static int is_mcp300_intr(void)
{
    u32 ahb_int_sts;
    ahb_int_sts = ioread32(AHBB_FTAHBB020_3_VA_BASE+0x414);
    if(ahb_int_sts & BIT1){
        return 1;
    }
    return 0;
}

static void clear_mcp300_intr(void)
{
    /* Clear EXTAHB3 Interrupt */
    iowrite32(BIT1, AHBB_FTAHBB020_3_VA_BASE+0x41C);
}

#define IS_MCP300_INTR()    is_mcp300_intr()
#define CLEAR_MCP300_INTR() clear_mcp300_intr()
#else 
/* not required for non-A369 platform */
#define IS_MCP300_INTR()    (1)
#define CLEAR_MCP300_INTR() do{ }while(0)
#endif


/*
 * interrupt handler
 */
static irqreturn_t mcp300d_int_hd(int irq, void *dev_id)
{
    //unsigned long flags;
    unsigned int engine_idx = (unsigned int)dev_id;
#if USE_LINK_LIST
    struct codec_link_list *codec_ll;
#else
    DecoderParams *dec_handle;
    struct dec_private *decp;
    unsigned int chn_idx;
#endif


    DUMP_MSG_G(D_ISR, "mcp300 %d ISR %d >\n", engine_idx, irq);
    
    if(IS_MCP300_INTR() == 0){
        return IRQ_NONE;
    }
    
    //favcd_spin_lock_irqsave( &avcdec_request_lock, flags);

    if(engine_idx >= FTMCP300_NUM){
        F_DEBUG("invalid engine index:%d\n", engine_idx);
        goto IRQ_RET;
    }

#if USE_LINK_LIST
    if(favcd_engine_info[engine_idx].ll_en == 0){
        F_DEBUG("engine %d ll_en == 0 (%d) false mcp300 interrupt\n", engine_idx, favcd_engine_info[engine_idx].ll_en);
        goto IRQ_RET;
    }

    codec_ll = favcd_engine_info[engine_idx].codec_ll[0];
    
    H264Dec_receive_irq_ll(&favcd_engine_info[engine_idx], codec_ll);
    //DUMP_MSG_G(D_ISR, "mcp300 %d ISR %d exe_job_idx:%d>\n", engine_idx, irq, codec_ll->exe_job_idx);
    wake_up(&mcp300_wait_queue);
    
#else
    chn_idx = favcd_engine_info[engine_idx].chn_idx;
    if(chn_idx >= MAX_DEC_NUM){
        F_DEBUG("invalid chn index:%d\n", chn_idx);
        goto IRQ_RET;
    }
    decp = &dec_data[chn_idx];
    dec_handle = (DecoderParams *)decp->dec_handle;

    if(dec_handle == NULL){
        F_DEBUG("null chn pointer. false mcp300 interrupt\n");
        goto IRQ_RET;
    }

    H264Dec_ReceiveIRQ(&favcd_engine_info[engine_idx]);
    
    if (H264Dec_DispatchIRQ(dec_handle, &favcd_engine_info[engine_idx]) != 0){
        F_DEBUG("isr but not slice done\n");
        goto IRQ_RET;
    }
    
    DUMP_MSG_G(D_ISR, "mcp300 %d ISR wake up ch: %d\n", engine_idx, dec_handle->chn_idx);
    wake_up(&mcp300_wait_queue);
#endif


IRQ_RET:
    CLEAR_MCP300_INTR();

    //favcd_spin_unlock_irqrestore( &avcdec_request_lock, flags);

    DUMP_MSG_G(D_ISR, "mcp300 %d ISR %d <\n", engine_idx, irq);

    return IRQ_HANDLED;
}


#if 0 /* use PMU functions instead */
void enable_clk_and_stop_reset(void)
{
    volatile unsigned int *scu_reg_base;
    volatile unsigned int *scu_reg_rst;
    volatile unsigned int *scu_reg_clk;
    volatile unsigned int *scu_reg_clk2;
    volatile unsigned int *addr;
    int i;
    
//    scu_reg_base = ioremap_nocache(SCU_BASE_ADDRESS, PAGE_ALIGN(0x100000));
//    scu_reg_base = ioremap_nocache(SCU_BASE_ADDRESS, PAGE_ALIGN(0x1000));
    scu_reg_base = ioremap_nocache(PMU_FTPMU010_PA_BASE, PMU_FTPMU010_VA_SIZE);
    scu_reg_rst = scu_reg_base + (0xa0 >> 2);
    scu_reg_clk = scu_reg_base + (0xb0 >> 2);
    scu_reg_clk2 = scu_reg_base + (0xb4 >> 2);

    printk("scu_reg_base addr:0x%08X\n", (unsigned int)scu_reg_base);
    printk("scu_reg_rst  addr:0x%08X val:%08X\n", (unsigned int)scu_reg_rst, *scu_reg_rst);
    printk("scu_reg_clk  addr:0x%08X val:%08X\n", (unsigned int)scu_reg_clk, *scu_reg_clk);
    printk("scu_reg_clk2 addr:0x%08X val:%08X\n", (unsigned int)scu_reg_clk2, *scu_reg_clk2);

    *scu_reg_rst |= (1 << 3);
    *scu_reg_clk &= ~(1 << 24);
    *scu_reg_clk2 &= ~(1 << 29);

    *scu_reg_clk &= ~(1 << 2);
    *scu_reg_clk2 &= ~(1 << 7);
    
    printk("scu_reg_rst  addr:0x%08X val:%08X\n", (unsigned int)scu_reg_rst, *scu_reg_rst);
    printk("scu_reg_clk  addr:0x%08X val:%08X\n", (unsigned int)scu_reg_clk, *scu_reg_clk);

    addr = scu_reg_rst;
    printk("scu + 0xa0:\n");
    for(i = 0; i < 20; i++){
        printk("addr:0x%08X:%08X\n", (unsigned int)(addr + i), *(unsigned int *)(addr + i));
    }
    
}
#endif

#define MEM_TEST 1
#if MEM_TEST
/*
 * testing memory on FPGA board of A369
 */
int __init mem_test(unsigned char *ptr, unsigned int size)
{
    extern unsigned int memtest;
    int j, k;
    int err_flg = 0;

    if(memtest == 0) /* skip memory test */
        return 0;
    
    for(j = 0; j < 8; j++) {
        unsigned char val = (1 << j);
        memset(ptr, val, size);
        printk("round %d testing\n", j);

        for(k = 0; k < size; k++){
            if(ptr[k] != val){
                err_flg = 1;
                printk("mem test error at round %d, byte %d: %d != %d\n", j, k, ptr[k], val);
            }
        }
    }
    if(err_flg == 0){
        printk("pass!!\n");
    }else{
        printk("error!!\n");
        return 1;
    }
    //while(1);
    return 0;
}
#endif

void favcd_damnit(char * str)
{
#if 1
    printk("favcd_damnit at: %s\n", str);
#endif
    panic("favcd_damnit");
}

void __init __print_config(void)
{
    extern unsigned int vc_cfg;
    extern unsigned int mcp300_sw_reset;
    extern unsigned int vcache_one_ref;
    extern unsigned int max_ref1_width;
    extern unsigned int max_level_idc;
    
    printk("vcache workaround: %d\n", mcp300_sw_reset);
    printk("slice_write_back enable: %d\n", SLICE_WRITE_BACK);
    printk("slice dma disable: %d\n", SLICE_DMA_DISABLE);
    printk("USE_SW_PARSER: %d\n", USE_SW_PARSER);
    #ifdef A369
    printk("====== important =======\n");
    #endif
    printk("USE_LINK_LIST: %d\n", USE_LINK_LIST);
    printk("USE_MULTICHAN_LINK_LIST: %d\n", USE_MULTICHAN_LINK_LIST);
    printk("SW_LINK_LIST: %d\n", SW_LINK_LIST);
    printk("USE_SW_LINK_LIST_ONLY: %d\n", USE_SW_LINK_LIST_ONLY);
    printk("WAIT_FR_DONE_CTRL:%d\n", WAIT_FR_DONE_CTRL);
    #ifdef A369
    printk("====== important =======\n");
    #endif

    printk("config:\n");
    printk("vcache enabled: %d\n", CONFIG_ENABLE_VCACHE);
    printk("vcache block enabled: %d\n", VCACHE_BLOCK_ENABLE);
    printk("vcache ilf: %d\n", ENABLE_VCACHE_ILF);
    printk("vcache sysinfo: %d\n", ENABLE_VCACHE_SYSINFO);
    printk("vcache ref: %d\n", ENABLE_VCACHE_REF);
    printk("vcache cfg: %d\n", vc_cfg);
    printk("vcache_one_ref: %d\n", vcache_one_ref);
    printk("max_ref1_width: %d\n", max_ref1_width);
    printk("max_level_idc:%d\n", max_level_idc);

#if 1
    printk("sizeof(SliceHeader):%d\n", sizeof(SliceHeader));
    printk("sizeof(SeqParameterSet):%d\n", sizeof(SeqParameterSet));
    printk("sizeof(PicParameterSet):%d\n", sizeof(PicParameterSet));
    printk("sizeof(StorablePicture):%d\n", sizeof(StorablePicture));
#endif



}


/*
 * cleanup at module unloading
 */
int h264d_cleanup_ioctl(void)
{
    int i;
    extern unsigned int mcp300_dev_reg;
    for(i = 0; i < FTMCP300_NUM; i++){
        if(mcp300_engine_en & (1 << i)){
            if (favcd_engine_info[i].irq_num){
                free_irq(favcd_engine_info[i].irq_num, (void *)i);
            }
            if (mcp300_dev_reg & (1 << i)){
                misc_deregister(&favc_decoder_dev[i]);
            }

            if (favcd_engine_info[i].intra_vaddr)
                release_dec_buffer(favcd_engine_info[i].intra_vaddr, favcd_engine_info[i].intra_paddr);
#if PER_ENGINE_MBINFO
            if (mcp300_mbinfo_virt[i])
                release_dec_buffer(mcp300_mbinfo_virt[i], mcp300_mbinfo_phy[i]);
#endif
        }
    }

    pf_mcp300_clk_off();
    pf_mcp300_clk_exit();

    for (i = 0; i < FTMCP300_NUM; i++) {
        if (favcd_engine_info[i].pu32BaseAddr) {
            iounmap((void __iomem *)favcd_engine_info[i].pu32BaseAddr);
        }
        if (favcd_engine_info[i].pu32VcacheBaseAddr) {
            iounmap((void __iomem *)favcd_engine_info[i].pu32VcacheBaseAddr);
        }
#if USE_LINK_LIST
        if (codec_ll_va[i]) {
            release_dec_buffer(codec_ll_va[i], codec_ll_pa[i]);
        }
#endif
    }
    
    for(i = 0; i < MAX_DEC_NUM; i++){
        if (mcp300_bs_virt[i])
            release_dec_buffer(mcp300_bs_virt[i], mcp300_bs_phy[i]);
        if (dec_data[i].dec_handle != NULL){
            H264Dec_Release(dec_data[i].dec_handle);
        }
    }


    return 0;
}

/*
 * module initialization
 */
int __init h264d_init_ioctl(void)
{
    int i;
    extern unsigned int vc_cfg;
    extern unsigned int mcp300_dev_reg;
    
    const FAVCD_DEC_INIT_PARAM stParam = 
    {
        pfnMalloc: fkmalloc,
        pfnFree:   fkfree,
        pfnDamnit: favcd_damnit,
        pfnMarkEngStart: NULL,
    };

    memset(favcd_engine_info, 0, sizeof(favcd_engine_info));

    /* initial per-channel data structure */
    for(i = 0; i < MAX_DEC_NUM; i++){
        memset(&dec_data[i], 0, sizeof(struct dec_private));
        dec_data[i].dec_handle = H264Dec_Create(&stParam, i);
        if(dec_data[i].dec_handle == NULL){
            goto dec_init_drv_error;
        }
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
        sema_init(&dec_data[i].dec_mutex, 1);
#else
        init_MUTEX(&dec_data[i].dec_mutex);
#endif

    }
    
    /* register driver minor number/operation function pointers*/
    for(i = 0; i < FTMCP300_NUM; i++){
        if(mcp300_engine_en & (1 << i)){
            if(misc_register(&favc_decoder_dev[i]) < 0){
                printk("h264_decoder_dev misc-register fail");
                goto dec_init_drv_error;
            }
            mcp300_dev_reg |= (1 << i);
        }
    }


    /* init mutex */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
    sema_init(&favc_dec_mutex, 1);
#else
    init_MUTEX(&favc_dec_mutex);
#endif

    for(i = 0; i < FTMCP300_NUM; i++){
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
        sema_init(&favc_eng_mutex[i], 1);
#else
        init_MUTEX(&favc_eng_mutex[i]);
#endif
    }
    
    /* init spin lock */
    spin_lock_init(&avcdec_request_lock);

    /* init waitqueue */
    init_waitqueue_head(&mcp300_wait_queue);
#if USE_MULTICHAN_LINK_LIST
    init_waitqueue_head(&mcp300_ll_wait_queue);
    init_waitqueue_head(&mcp300_job_wait_queue);
#endif    

    /* enable clock */
    pf_mcp300_clk_on();

    for(i = 0; i < MAX_DEC_NUM; i++){
        mcp300_bs_length[i] = PAGE_ALIGN(mcp300_max_width * mcp300_max_height);
        if (allocate_dec_buffer(&mcp300_bs_virt[i], &mcp300_bs_phy[i], mcp300_bs_length[i]) < 0) {
            F_DEBUG("allocate bitstream buffer error (size:%d max width:%d, max height:%d)\n", 
                mcp300_bs_length[i], mcp300_max_width, mcp300_max_height);
            goto dec_init_drv_error;
        }
    
        printk("bitsteram buffer: %x (%x) length %d\n", mcp300_bs_virt[i], mcp300_bs_phy[i], mcp300_bs_length[i]);
    }

    /* do memory test */
    if(mem_test((unsigned char *)mcp300_bs_virt[0], mcp300_bs_length[0])){
        goto dec_init_drv_error;
    }


    for(i = 0; i < FTMCP300_NUM; i++){
        if(mcp300_engine_en & (1 << i)){
            /* register interupt handler */
            if (request_irq(irq_no[i], mcp300d_int_hd, 0, irq_name[i], (void *)i) != 0) {   
                F_DEBUG("MCP300 Dec interrupt request failed, IRQ=%d\n", irq_no[i]);
                goto dec_init_drv_error;
            }
            favcd_engine_info[i].irq_num = irq_no[i];

            /* allocate intra buffer */
            favcd_engine_info[i].intra_length = mcp300_max_width * 2; /* one MB row * 32 byte */
            if (allocate_dec_buffer(&favcd_engine_info[i].intra_vaddr, &favcd_engine_info[i].intra_paddr, favcd_engine_info[i].intra_length) < 0) {
                release_dec_buffer(favcd_engine_info[i].intra_vaddr, favcd_engine_info[i].intra_paddr);
                goto dec_init_drv_error;
            }
            printk("intra buffer: %x (%x) length %d\n", favcd_engine_info[i].intra_vaddr, favcd_engine_info[i].intra_paddr, favcd_engine_info[i].intra_length);

            /* get kernel virtual address for accessing MCP300 register */
            favcd_engine_info[i].pu32BaseAddr = ioremap_nocache(favcd_base_address_pa[i], favcd_base_address_size[i]);
            if (unlikely(favcd_engine_info[i].pu32BaseAddr == NULL)){
                F_DEBUG("MCP300 Dec no virtual address\n");
                goto dec_init_drv_error;
            }

            /* get kernel virtual address for accessing MCP300 VCACHE register */
            favcd_engine_info[i].pu32VcacheBaseAddr = ioremap_nocache(favcd_vcache_address_pa[i], favcd_vcache_address_size[i]);
            if (unlikely(favcd_engine_info[i].pu32VcacheBaseAddr == NULL)){
                F_DEBUG("MCP300 Dec no virtual address\n");
                goto dec_init_drv_error;
            }

        
#if USE_LINK_LIST
            /* allocate linked list buffer */
            if(allocate_dec_buffer(&codec_ll_va[i], &codec_ll_pa[i], sizeof(*codec_ll[i]))){
                F_DEBUG("allocate memory for linked list failed\n");
                goto dec_init_drv_error;
            }

            printk("ll va:%08X pa:%08X\n", (unsigned int)codec_ll_va[i], (unsigned int)codec_ll_pa[i]);
            
            codec_ll[i] = (void *)codec_ll_va[i];
            if(codec_ll[i] == NULL){
                F_DEBUG("allocate memory for linked list failed\n");
                goto dec_init_drv_error;
            }
            codec_ll[i]->ll_pa = codec_ll_pa[i];

            ll_init(codec_ll[i], i * 200);
#if SW_LINK_LIST == 2
            ll_set_base_addr(codec_ll[i], (unsigned int)favcd_engine_info[i].pu32BaseAddr, (unsigned int)favcd_engine_info[i].pu32VcacheBaseAddr);
#endif
#endif /* USE_LINK_LIST */

            /* init engine info */
            favcd_engine_info[i].engine = i;
            favcd_engine_info[i].chn_idx = -1;
#if USE_LINK_LIST
            favcd_engine_info[i].ll_en = 1;
            favcd_engine_info[i].codec_ll[0] = codec_ll[i];
            favcd_engine_info[i].codec_ll[1] = NULL;
#endif
            favcd_engine_info[i].vcache_ilf_enable = 0;

            H264Dec_ResetEgnine(&favcd_engine_info[i]);

        }
    }


    /* print configuration of lowwer level driver */
    __print_config();

    return 0;


dec_init_drv_error:
    h264d_cleanup_ioctl();
    F_DEBUG("init error\n");
    return -1;
}


