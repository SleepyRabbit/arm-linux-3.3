#ifdef LINUX
	#include <linux/kernel.h>
	#include <linux/module.h>
	#include <linux/string.h>
	#include <asm/io.h>
    #include <linux/pci.h>
#else
	#include <string.h>
#endif
#if defined(FPGA_NON_OS) | defined(WIN32TEST)
    #include <stdio.h>
#endif

#include "mcp280_reg.h"
#include "global.h"
#include "define.h"
#include "H264V_enc.h"
#include "vlc.h"
#include "../debug.h"
#ifndef LINUX
	#include "mem_fun.h"
#endif
#ifdef VG_INTERFACE
    #include "log.h"
#endif
#ifdef AES_ENCRYPT
    #include "encrypt.h"
#endif

#define DDR_REG_BASE        FPGA_EXT2_VA_BASE

char slice_type_char[3] = {'P', 'B', 'I'};
char frame_structure[3][10] = {"frame", "top", "bottom"};
//int enc_index;
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	int update_param_reset_register = 0;
#endif
unsigned int gTimeoutThreshold = TIMEOUT_THRESHOLD;

extern unsigned int use_ioremap_wc;
#ifdef DISABLE_WRITE_OUT_REC
    extern unsigned int h264e_dis_wo_buf;
#endif
#ifdef DYNAMIC_GOP_ENABLE
    extern unsigned int iframe_disable_roi;
#endif

extern void dump_parm_register(EncoderParams *p_Enc);
extern void dump_cmd_register(EncoderParams *p_Enc);
extern void dump_mem_register(EncoderParams *p_Enc);
extern void dump_vcache(EncoderParams *p_Enc);
extern void dump_mcnr_table(void *ptEnc);

//#define C_MODEL_CONSTRAINT

void dump_all_register(EncoderParams *p_Enc, unsigned int flag)
{
    if (flag & DUMP_PARAM)
        dump_parm_register(p_Enc);
    if (flag & DUMP_CMD)
        dump_cmd_register(p_Enc);
    if (flag & DUMP_MEM)
        dump_mem_register(p_Enc);
    if (flag & DUMP_VCACHE)
        dump_vcache(p_Enc);
    if (flag & DUMP_QUANT_TABLE)
        dump_quant_matrix(p_Enc);
#ifdef MCNR_ENABLE
    if (flag & DUMP_MCNR_TABLE)
        dump_mcnr_table(p_Enc);
#endif
}

void set_bs_param(EncoderParams *p_Enc, FAVC_ENC_IOCTL_FRAME *pFrame)
{
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	ptReg_cmd->CMD2 = BIT11;	// clear bitstream
#ifndef ADDRESS_UINT32
    ptReg_mem->BS_ADDR = (uint32_t)pFrame->pu8BSBuffer_phy;
	ptReg_mem->BS_BUF_SIZE = pFrame->u32BSBufferSize;
    p_Enc->u32BSBuffer_virt = (uint32_t)pFrame->pu8BSBuffer_virt;
    p_Enc->u32BSBuffer_phy = (uint32_t)pFrame->pu8BSBuffer_phy;
	p_Enc->u32BSBufferSize = pFrame->u32BSBufferSize;
    p_Enc->u32MaxBSLength = pFrame->u32BSBufferSize;
#else	// ADDRESS_UINT32
    ptReg_mem->BS_ADDR = pFrame->u32BSBuffer_phy;
	ptReg_mem->BS_BUF_SIZE = pFrame->u32BSBufferSize;
    p_Enc->u32BSBuffer_virt = pFrame->u32BSBuffer_virt;
    p_Enc->u32BSBuffer_phy = pFrame->u32BSBuffer_phy;
	p_Enc->u32BSBufferSize = pFrame->u32BSBufferSize;
    p_Enc->u32MaxBSLength = pFrame->u32BSBufferSize;
#endif
	//ptReg_mem->SWAP_DMA = 1;	// set at slice trigger
    clear_bs_length(p_Enc, ptReg_cmd);
    p_Enc->u32ResBSLength = 0;
    p_Enc->u32TopBSLength = 0;
    //p_Enc->u32OutputedBSLength = 0;
    //p_Enc->u32CostBSLength = 0;
}
/*
void reset_bs_param(EncoderParams *p_Enc, int bs_length)
{
	volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);

    ptReg_mem->BS_ADDR = (uint32_t)p_Enc->pu8BSBuffer_phy;
    ptReg_mem->BS_BUF_SIZE = p_Enc->u32MaxBSLength;

    //p_Enc->u32OutputedBSLength += bs_length;
	//ptReg_mem->SWAP_DMA = 1;	// set at slice trigger
}
*/
#ifdef HANDLE_CABAC_ZERO_WORD
#define PADDING_SIZE    8
static void set_bs_dword_align(EncoderParams *p_Enc)
{
    //volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
    uint32_t bs_offset;
    int offset;
    offset = p_Enc->u32ResBSLength & (PADDING_SIZE-1);
    if (offset)
        offset = PADDING_SIZE - offset;
    else
        offset = 0;

    bs_offset = p_Enc->u32HeaderBSLength + p_Enc->u32ResBSLength;
    // set zero
    memset((void *)p_Enc->u32BSBuffer_virt + bs_offset, 0, offset);
    #ifdef LINUX
        //dma_map_single(NULL, (void *)(p_Enc->u32BSBuffer_virt + bs_offset), offset, DMA_TO_DEVICE);
    #endif
    ptReg_mem->BS_ADDR = p_Enc->u32BSBuffer_phy + bs_offset + offset;
    p_Enc->u32TopBSLength = p_Enc->u32ResBSLength + offset;
}
#endif

#ifdef NO_RESET_REGISTER_PARAM_UPDATE
int encoder_reset_register(unsigned int base_addr, unsigned int vcache_base_addr)
{
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(base_addr + MCP280_OFF_CMD);
	volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(base_addr);
#if MCP280_VCACHE_ENABLE
    volatile H264Reg_Vcache *ptReg_vcache = (H264Reg_Vcache *)(vcache_base_addr);
#endif
	unsigned int tmp;
	int i, cnt = 0;

#if 1
    LOG_PRINT(LOG_INFO, "reset DMA");
    ptReg_cmd->CMD0 = BIT2; // disable DMA
    if (use_ioremap_wc)
        tmp = ptReg_cmd->CMD0;
    while (!(ptReg_cmd->STS0 & BIT3)) {
        if (cnt > MAX_POLLING_ITERATION) {
            favce_err("reset DMA error");
            return H264E_ERR_POLLING_TIMEOUT;
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
#endif
    LOG_PRINT(LOG_INFO, "software reset");
	ptReg_cmd->CMD0 = BIT31;		// reset about 12T
	for (i = 0; i<10; i++)
        tmp = ptReg_cmd->STS0;
    LOG_PRINT(LOG_INFO, "software reset done");
#ifdef DISABLE_WRITE_OUT_REC
    if (h264e_dis_wo_buf)
        ptReg_cmd->STS2 = ((~INTERRUPT_FLAG_NO_DMA)&0x1F)<<16;
    else
#endif
    ptReg_cmd->STS2 = ((~INTERRUPT_FLAG)&0x1F)<<16;
	//ptReg_cmd->STS2 = BIT20;	// enable all interrupt beside timeout
    /* interrupt: Bit 0: slice done
     *            Bit 1: bitstream buffer full
     *            Bit 2: DMA read error
     *            Bit 3: DMA write error
     *            Bit 4: encode MB timeout
     * interrupt mask: Bit 16: slice done mask
     *                 Bit 17: bitsteram buffer full mask
     *  (0: enable     Bit 18: DMA read error mask
     *   1: disable)   Bit 19: DMA write error mask
     *                 Bit 20: encode MB timeout mask
    */
    ptReg_parm->PARM21 = gTimeoutThreshold;

#if MCP280_VCACHE_ENABLE
    LOG_PRINT(LOG_INFO, "vcache reset");
    ptReg_vcache->V_ENC |= BIT4; // disable dma
    if (use_ioremap_wc)
        tmp = ptReg_vcache->V_ENC;
    cnt = 0;
    while (!(ptReg_vcache->DECODE & BIT1)) { // wait dma shut down done
        if (cnt > MAX_POLLING_ITERATION) {
            favce_err("vcache reset polling timeout\n");
            return H264E_ERR_POLLING_TIMEOUT;
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
    ptReg_vcache->V_ENC |= BIT8; // sfotware reset (vcache)
    tmp = ptReg_vcache->V_ENC;
    tmp = ptReg_vcache->V_ENC;
    LOG_PRINT(LOG_INFO, "vcache reset done");

    ptReg_vcache->V_ENC = MCP280_VCACHE_ENABLE | (VCACHE_BLOCK_ENABLE<<1);
    #if !(VCACHE_ILF_ENABLE)
        ptReg_vcache->DIS_ILF_PARM = 0;
    #endif
#endif
    //p_Enc->u32CurrIRQ = 0;

    LOG_PRINT(LOG_INFO, "set interrupt register done");
#ifdef REC_MEM_ADDR
	memset(&rec_mem_reg[0], 0, sizeof(rec_mem_reg));
    memset(&rec_mem_reg[1], 0, sizeof(rec_mem_reg));
#endif
    return H264E_OK;
}
int init_register(EncoderParams *p_Enc, int sw_reset_en)
{
	int ret = H264E_OK;
	if (sw_reset_en)
		ret = encoder_reset_register(p_Enc->u32BaseAddr, p_Enc->u32VcacheAddr);
	p_Enc->u32CurrIRQ = 0;
	return ret;
}

#else
int init_register(EncoderParams *p_Enc)
{
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
#if MCP280_VCACHE_ENABLE
    #ifdef RTL_SIMULATION
        volatile H264Reg_Vcache *ptReg_vcache = (H264Reg_Vcache *)(0x91000000);
    #else
        #ifndef FPGA_8150
            volatile H264Reg_Vcache *ptReg_vcache = (H264Reg_Vcache *)(p_Enc->u32VcacheAddr);
        #else
            volatile H264Reg_Vcache *ptReg_vcache = (H264Reg_Vcache *)(p_Enc->u32BaseAddr + MCP280_OFF_VCACHE);
        #endif
    #endif
#endif
	unsigned int tmp;
	int i, cnt = 0;

#if 1
    LOG_PRINT(LOG_INFO, "reset DMA");
    ptReg_cmd->CMD0 = BIT2; // disable DMA
    if (use_ioremap_wc)
        tmp = ptReg_cmd->CMD0;
    while (!(ptReg_cmd->STS0 & BIT3)) {
        if (cnt > MAX_POLLING_ITERATION) {
            LOG_PRINT(LOG_ERROR, "reset DDR error");
            return H264E_ERR_POLLING_TIMEOUT;
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
#endif
    LOG_PRINT(LOG_INFO, "software reset");
	ptReg_cmd->CMD0 = BIT31;		// reset about 12T
	for (i = 0; i<10; i++)
        tmp = ptReg_cmd->STS0;
    LOG_PRINT(LOG_INFO, "software reset done");
#ifdef DISABLE_WRITE_OUT_REC
    if (h264e_dis_wo_buf)
        ptReg_cmd->STS2 = ((~INTERRUPT_FLAG_NO_DMA)&0x1F)<<16;
    else
#endif
    ptReg_cmd->STS2 = ((~INTERRUPT_FLAG)&0x1F)<<16;
	//ptReg_cmd->STS2 = BIT20;	// enable all interrupt beside timeout
    /* interrupt: Bit 0: slice done
     *            Bit 1: bitstream buffer full
     *            Bit 2: DMA read error
     *            Bit 3: DMA write error
     *            Bit 4: encode MB timeout
     * interrupt mask: Bit 16: slice done mask
     *                 Bit 17: bitsteram buffer full mask
     *  (0: enable     Bit 18: DMA read error mask
     *   1: disable)   Bit 19: DMA write error mask
     *                 Bit 20: encode MB timeout mask
    */
    ptReg_parm->PARM21 = gTimeoutThreshold;

//#if MCP280_VCACHE_ENABLE
#if MCP280_VCACHE_ENABLE
    LOG_PRINT(LOG_INFO, "vcache reset");
    ptReg_vcache->V_ENC |= BIT4; // disable dma
    if (use_ioremap_wc)
        tmp = ptReg_vcache->V_ENC;
    cnt = 0;
    while (!(ptReg_vcache->DECODE & BIT1)) { // wait dma shut down done
        if (cnt > MAX_POLLING_ITERATION) {
            favce_err("vcache reset polling timeout\n");
            return H264E_ERR_POLLING_TIMEOUT;
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
    ptReg_vcache->V_ENC |= BIT8; // sfotware reset (vcache)
    tmp = ptReg_vcache->V_ENC;
    tmp = ptReg_vcache->V_ENC;
    LOG_PRINT(LOG_INFO, "vcache reset done");

    ptReg_vcache->V_ENC = MCP280_VCACHE_ENABLE | (VCACHE_BLOCK_ENABLE<<1);
#if !(VCACHE_ILF_ENABLE)
    ptReg_vcache->DIS_ILF_PARM = 0;
#endif
//#endif    // MCP280_VCACHE_ENABLE
#endif
    p_Enc->u32CurrIRQ = 0;

    LOG_PRINT(LOG_INFO, "set interrupt register done");

    return H264E_OK;
}
#endif

int encoder_abandon_rest(void *ptHandle)
{
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	if (init_register((EncoderParams *)ptHandle, 1) < 0)
		return H264E_ERR_POLLING_TIMEOUT;
#else
    if (init_register((EncoderParams *)ptHandle) < 0)
        return H264E_ERR_POLLING_TIMEOUT;
#endif
    return H264E_OK;
}

int encoder_create(void **pptHandle, FAVC_ENC_IOCTL_PARAM *pParam, int dev)
{
	EncoderParams *p_Enc = NULL;
	VideoParams *p_Vid= NULL;
	SliceHeader *currSlice = NULL;
	DecodedPictureBuffer *p_Dpb = NULL;
	int i;
	
	if ((p_Enc = (EncoderParams *)pParam->pfnMalloc(sizeof(EncoderParams), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	*pptHandle = p_Enc;
	if ((p_Vid = (VideoParams *)pParam->pfnMalloc(sizeof(VideoParams), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	p_Enc->p_Vid = p_Vid;
	if ((currSlice = (SliceHeader *)pParam->pfnMalloc(sizeof(SliceHeader), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	p_Vid->currSlice = currSlice;
	if ((p_Vid->active_sps = (SeqParameterSet *)pParam->pfnMalloc(sizeof(SeqParameterSet), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	if ((p_Vid->active_pps = (PicParameterSet *)pParam->pfnMalloc(sizeof(PicParameterSet), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	if ((p_Vid->dec_picture = (StorablePicture *)pParam->pfnMalloc(sizeof(StorablePicture), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	if ((p_Dpb = (DecodedPictureBuffer *)pParam->pfnMalloc(sizeof(DecodedPictureBuffer), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
		return H264E_ERR_MEMORY;
	p_Vid->p_Dpb = p_Dpb;
	for (i = 0; i<MAX_DPB_SIZE; i++) {
		if ((p_Dpb->fs[i] = (FrameStore *)pParam->pfnMalloc(sizeof(FrameStore), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
			return H264E_ERR_MEMORY;
		if ((p_Dpb->fs[i]->frame = (StorablePicture *)pParam->pfnMalloc(sizeof(StorablePicture), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
			return H264E_ERR_MEMORY;
		if ((p_Dpb->fs[i]->topField = (StorablePicture *)pParam->pfnMalloc(sizeof(StorablePicture), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
			return H264E_ERR_MEMORY;
		if ((p_Dpb->fs[i]->btmField = (StorablePicture *)pParam->pfnMalloc(sizeof(StorablePicture), pParam->u32CacheAlign, pParam->u32CacheAlign)) == NULL)
			return H264E_ERR_MEMORY;
	}
#ifdef STRICT_IDR_IDX
    p_Vid->idr_cnt = 0;
#endif
	return encoder_reset(p_Enc, pParam, dev);

//encoder_create_fail:
//    return H264E_ERR_MEMORY;
}

static int h264e_check_params(FAVC_ENC_PARAM *pParam)
{
    // check range
#ifdef ENABLE_FAST_FORWARD
    if (pParam->u32FastForward) {
        //if (2 == pParam->u32FastForward || 4 == pParam->u32FastForward) {
        if (pParam->u32FastForward >= MIN_FASTFORWARD && pParam->u32FastForward <= MAX_FASTFORWARD) {
            if (pParam->u8BFrameNumber) {
                favce_wrn("fast forward enable, B frame can not be enable\n");
                pParam->u8BFrameNumber = 0;
            }
            if ((pParam->u32IDRPeriod%pParam->u32FastForward) != 0) {
                favce_wrn("IDR interval must be multiple of fast forward(%d), force idr_period = %d\n",
                    pParam->u32FastForward, (pParam->u32IDRPeriod+pParam->u32FastForward-1)/pParam->u32FastForward*pParam->u32FastForward);
                pParam->u32IDRPeriod = (pParam->u32IDRPeriod+pParam->u32FastForward-1)/pParam->u32FastForward*pParam->u32FastForward;
            }                
        }
        else {
            favce_wrn("fast forward can not be %d, disable fast forward\n", pParam->u32FastForward);
            pParam->u32FastForward = 0;
        }
    }
#endif
    if (pParam->u32IDRPeriod == 1) {    // all intra 
        if (pParam->u8BFrameNumber > 0) {
            favce_wrn("all intra coding, GOP has no B frame");
            pParam->u8BFrameNumber = 0;
        }
    }
    else {                                                              // not all intra
        if (pParam->u8NumRefFrames == 0) {
            favce_wrn("not all intra coding, reference frame number must greater than zero\n");
            pParam->u8NumRefFrames = 1;
        }
    }
    // finally, pParam->u8NumRefFrames is always one
    if (pParam->u8NumRefFrames > MAX_NUM_REF_FRAMES) {
        favce_wrn("number of reference frames(%d) can not over %d\n", pParam->u8NumRefFrames, MAX_NUM_REF_FRAMES);
        pParam->u8NumRefFrames = MAX_NUM_REF_FRAMES;
    }
    if (pParam->u8BFrameNumber > MAX_B_NUMBER) {
        favce_wrn("number of b frames(%d) can not over %d\n", pParam->u8BFrameNumber, MAX_B_NUMBER);
        pParam->u8BFrameNumber = MAX_B_NUMBER;
    }
    if (pParam->u32Profile != BASELINE && pParam->u32Profile != MAIN && pParam->u32Profile != FREXT_HP) {
        favce_wrn("unknow profile %d, force high profile\n", pParam->u32Profile);
        pParam->u32Profile = FREXT_HP;
    }
	// check source wdith & heigth not exceed max width & height
	if (pParam->u8Log2MaxFrameNum < 4 || pParam->u8Log2MaxFrameNum > 16) {
	    favce_wrn("log2 max frame number(%d) must between 4 and 16\n", pParam->u8Log2MaxFrameNum);
	    pParam->u8Log2MaxFrameNum = iClip3(4, 16, pParam->u8Log2MaxFrameNum);
	}
	if (pParam->u8Log2MaxPOCLsb < 4 || pParam->u8Log2MaxPOCLsb > 16) {
	    favce_wrn("log2 max poc lsb(%d) must between 4 and 16\n", pParam->u8Log2MaxPOCLsb);
	    pParam->u8Log2MaxPOCLsb = iClip3(4, 16, pParam->u8Log2MaxPOCLsb);
	}
	if (pParam->u8ResendSPSPPS > 3) {
	    favce_wrn("u8ResendSPSPPS is larger than 2, it will resend sps pps every frame\n");
	    pParam->u8ResendSPSPPS = 2;
	}
    if (pParam->i8ChromaQPOffset0 < -12 || pParam->i8ChromaQPOffset0 > 12) {
        favce_wrn("chroma qp offset must between -12 and 12\n");
        pParam->i8ChromaQPOffset0 = iClip3(-12, 12, pParam->i8ChromaQPOffset0);
    }
	if (pParam->i8DisableDBFIdc > 3) {
	    favce_wrn("disable deblocking idc can not exceed 3\n");
	    pParam->i8DisableDBFIdc = -1;
	}
	if (pParam->u8DiDnMode & 0xF0) {
	    favce_wrn("didn mode can not exceed 15\n");
	    pParam->u8DiDnMode &= 0x0F;
	}
	if (pParam->u8PInterPredModeDisable & 0xF0) {
	    favce_wrn("disable P frame inter prediction mode flag can not exceed 15\n");
	    pParam->u8PInterPredModeDisable &= 0x0F;
	}
	if (pParam->u8BInterPredModeDisable & 0xF0) {
	    favce_wrn("disable B frame inter prediction mode flag can not exceed 15\n");
	    pParam->u8BInterPredModeDisable &= 0x0F;
	}

    if (pParam->u32Profile == BASELINE) {
        if (pParam->u8BFrameNumber) {
            favce_wrn("baseline profile can not encode B frames\n");
            pParam->u8BFrameNumber = 0;
        }
        if (pParam->bFieldCoding) {
            favce_wrn("baseline profile can not encode field\n");
            pParam->bFieldCoding = 0;
        }
        if (pParam->bEntropyCoding) {
            favce_wrn("baseline profile can not use CABAC coding\n");
            pParam->bEntropyCoding = 0;
        }
        if (pParam->bChromaFormat == 0) {
            favce_wrn("baseline profile can not encode mono frames\n");
            pParam->bChromaFormat = 1;
        }
        if (pParam->bScalingMatrixFlag) {
            favce_wrn("baseline profile can not set scaling matrix\n");
            pParam->bScalingMatrixFlag = 0;
        }
        if (pParam->i8ChromaQPOffset1) {
            favce_wrn("bsaeline profile can not set second chroma qp offset\n");
            pParam->i8ChromaQPOffset1 = pParam->i8ChromaQPOffset0;
        }
        if (pParam->bTransform8x8Mode) {
            favce_wrn("baseline can not use 8x8 transform\n");
            pParam->bTransform8x8Mode = 0;
        }
    }
    else if (pParam->u32Profile == MAIN) {
        if (pParam->bChromaFormat == 0) {
            favce_wrn("main profile can not encode mono frames\n");
            pParam->bChromaFormat = 1;
        }
        if (pParam->bScalingMatrixFlag) {
            favce_wrn("main profile can not set scaling matrix\n");
            pParam->bScalingMatrixFlag = 0;
        }
        if (pParam->bTransform8x8Mode) {
            favce_wrn("main profile can not use 8x8 transform\n");
            pParam->bTransform8x8Mode = 0;
        }
        if (pParam->i8ChromaQPOffset1) {
            favce_wrn("main profile can not set second chroma qp offset\n");
            pParam->i8ChromaQPOffset1 = pParam->i8ChromaQPOffset0;
        }
    }
    else {
        if (pParam->i8ChromaQPOffset1 < -12 || pParam->i8ChromaQPOffset1 > 12) {
            favce_wrn("second chroma qp offset must between -12 and 12\n");
            pParam->i8ChromaQPOffset1 = iClip3(-12, 12, pParam->i8ChromaQPOffset1);
        }
/*
        if (pParam->bTransform8x8Mode == 0 && pParam->bIntra8x8Disable == 1) {
            favce_wrn("intra 8x8 must be close when transform 8x8 is close\n");
            pParam->bIntra8x8Disable = 1;
        } 
*/
    }
    return 0;
}

static void reset_parameter(EncoderParams *p_Enc)
{
	//VideoParams *p_Vid = p_Enc->p_Vid;
	//DecodedPictureBuffer *p_Dpb = p_Vid->p_Dpb;
	int i;
	
	// store all pointers and reset all parameters
	VideoParams *tmpVid;
	SeqParameterSet *tmpSPS;
	PicParameterSet *tmpPPS;
	StorablePicture *tmpPic;
	SliceHeader *tmpHeader;
	DecodedPictureBuffer *tmpDpb;
	FrameStore *tmpFs[MAX_DPB_SIZE];
	StorablePicture *tmpFrame[MAX_DPB_SIZE];
	StorablePicture *tmpTop[MAX_DPB_SIZE];
	StorablePicture *tmpBottom[MAX_DPB_SIZE];
	
	// store addressese all pointers
	tmpVid = p_Enc->p_Vid;
	tmpSPS = tmpVid->active_sps;
	tmpPPS = tmpVid->active_pps;
	tmpPic = tmpVid->dec_picture;
	tmpHeader = tmpVid->currSlice;
	tmpDpb = tmpVid->p_Dpb;
	for (i = 0; i<MAX_DPB_SIZE; i++) {
		tmpFs[i] = tmpDpb->fs[i];
		tmpFrame[i] = tmpDpb->fs[i]->frame;
		tmpTop[i] = tmpDpb->fs[i]->topField;
		tmpBottom[i] = tmpDpb->fs[i]->btmField;
	}
	
	for (i = 0; i<MAX_DPB_SIZE; i++) {
		memset(tmpFrame[i], 0 ,sizeof(StorablePicture));
		memset(tmpTop[i], 0, sizeof(StorablePicture));
		memset(tmpBottom[i], 0, sizeof(StorablePicture));
		memset(tmpFs[i], 0, sizeof(FrameStore));
	}
	memset(tmpDpb, 0, sizeof(DecodedPictureBuffer));
	memset(tmpSPS, 0, sizeof(SeqParameterSet));
	memset(tmpPPS, 0, sizeof(PicParameterSet));
	memset(tmpPic, 0, sizeof(StorablePicture));
	memset(tmpHeader, 0 ,sizeof(SliceHeader));
	memset(tmpVid, 0, sizeof(VideoParams));
	memset(p_Enc, 0, sizeof(EncoderParams));

	
	p_Enc->p_Vid = tmpVid;
	tmpVid->active_sps = tmpSPS;
	tmpVid->active_pps = tmpPPS;
	tmpVid->dec_picture = tmpPic;
	tmpVid->currSlice = tmpHeader;
	tmpVid->p_Dpb = tmpDpb = tmpDpb;
	for (i = 0 ; i<MAX_DPB_SIZE; i++) {
		tmpDpb->fs[i] = tmpFs[i];
		tmpDpb->fs[i]->frame = tmpFrame[i];
		tmpDpb->fs[i]->topField = tmpTop[i];
		tmpDpb->fs[i]->btmField = tmpBottom[i];
	}
}

int encoder_reset(void *ptEncHandle, FAVC_ENC_IOCTL_PARAM *pParam, int dev)
{
    FAVC_ENC_PARAM *apParam = &pParam->apParam;
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
	VideoParams *p_Vid = p_Enc->p_Vid;
#ifdef MCNR_ENABLE
    volatile H264Reg_MCNR *ptReg_mcnr;
#endif
	uint32_t max_hzsr, max_vtsr, vt_sr, hz_sr;
	uint32_t cost[3] = {0, 0, 0};
	int i;
#ifdef STRICT_IDR_IDX
    int idr_cnt = p_Vid->idr_cnt;
#endif

#ifdef AUTO_PAKING_HEADER
    LOG_PRINT(LOG_INFO, "auto paking");
#endif
	reset_parameter(p_Enc);
	p_Enc->u32BaseAddr = pParam->u32BaseAddr;
    p_Enc->u32VcacheAddr = pParam->u32VcacheBaseAddr;
	p_Enc->pfnMalloc = pParam->pfnMalloc;
	p_Enc->pfnFree = pParam->pfnFree;
	p_Enc->enc_dev = dev;
#ifdef STRICT_IDR_IDX
    p_Vid->idr_cnt = idr_cnt;
#endif
    //p_Enc->u8CurrBSBufIdx = p_Enc->u8OutBSBufIdx = 0;
    //p_Enc->u8CurrState = NORMAL_STATE;
#ifdef NO_RESET_REGISTER_PARAM_UPDATE
	init_register(p_Enc, update_param_reset_register);
#else
	if (init_register(p_Enc) < 0)
	    return H264E_ERR_POLLING_TIMEOUT;
#endif

    h264e_check_params(&pParam->apParam);
	p_Vid->idr_period = apParam->u32IDRPeriod;
#ifdef ENABLE_FAST_FORWARD
    p_Vid->fast_forward = apParam->u32FastForward;
#endif
	//if (apParam->u8BFrameNumber > MAX_B_NUMBER)   // check in check_params
	//	return H264E_ERR_INPUT_PARAM;
	p_Vid->number_Bframe = apParam->u8BFrameNumber;

	// gop initial seting
	p_Vid->gop_idr_cnt = 0;
	p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
	p_Vid->gop_poc_base = 0;
    p_Vid->previous_idr_flag = 0;   // Tuba 20120216: add last idr to update b frame poc

	if (prepare_sps(p_Vid, p_Vid->active_sps, apParam, 0) < 0) {
        E_DEBUG("prepare sps error\n");
        return H264E_ERR_INPUT_PARAM;
    }
	if (prepare_pps(p_Vid, p_Vid->active_pps, apParam, 0) < 0) {
        E_DEBUG("prepare pps error\n");
        return H264E_ERR_INPUT_PARAM;
    }
	p_Vid->resend_spspps = apParam->u8ResendSPSPPS;
    /*
	if (p_Vid->field_pic_flag) {
	#ifndef ADDRESS_UINT32
		p_Enc->pu8MVInfoAddr_virt[1] = p_Enc->pu8MVInfoAddr_virt[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->pu8MVInfoAddr_phy[1] = p_Enc->pu8MVInfoAddr_phy[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->pu8L1ColMVInfoAddr_virt[1] = p_Enc->pu8L1ColMVInfoAddr_virt[0] + p_Vid->total_mb_number*4*4/2;
		p_Enc->pu8L1ColMVInfoAddr_phy[1] = p_Enc->pu8L1ColMVInfoAddr_phy[0] + p_Vid->total_mb_number*4*4/2;
	#else
		p_Enc->u32MVInfoAddr_virt[1] = p_Enc->u32MVInfoAddr_virt[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->u32MVInfoAddr_phy[1] = p_Enc->u32MVInfoAddr_phy[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->u32L1ColMVInfoAddr_virt[1] = p_Enc->u32L1ColMVInfoAddr_virt[0] + p_Vid->total_mb_number*4*4/2;
		p_Enc->u32L1ColMVInfoAddr_phy[1] = p_Enc->u32L1ColMVInfoAddr_phy[0] + p_Vid->total_mb_number*4*4/2;
	#endif
	}
	*/
	if (apParam->u8CabacInitIDC>3) {
	    E_DEBUG("cabac init idc(%d) larger than 3\n", apParam->u8CabacInitIDC);
		return H264E_ERR_INPUT_PARAM;
	}
	p_Vid->cabac_init_idc = apParam->u8CabacInitIDC;
	p_Vid->no_quant_matrix_flag = !(p_Vid->active_sps->seq_scaling_matrix_present_flag | p_Vid->active_pps->pic_scaling_matrix_present_flag);

//#if !(MCP280_VCACHE_ENABLE)
	// search range constraint
	for (i = 0; i<p_Vid->ref_l0_num; i++) {
	    hz_sr = iClip3(2, 25, (apParam->u32HorizontalSR[i]+7)>>3);
	    hz_sr = iMin(hz_sr, (p_Vid->frame_width-16)>>3);
	    vt_sr = iClip3(2, 15, (apParam->u32VerticalSR[i]+7)>>3);
	    vt_sr = iMin(vt_sr, (p_Vid->frame_height-16)>>3);
	    cost[0] += HORZ_MB(hz_sr)*vt_sr*4;
	    cost[1] += (HORZ_MB(hz_sr)*vt_sr/2 + HORZ_MB(hz_sr))*8;
	    cost[2] += (HORZ_MB(hz_sr)+1)*(vt_sr+1)*16;
	}
	if (cost[0] > 320 || cost[1] > 480 || cost[2] > 1760) {
	    E_DEBUG("search range too large\n");
	    return H264E_ERR_INPUT_PARAM;
	}
    /* ================================================================
    // B frame only two reference
	if (p_Vid->number_Bframe) {
	    cost[0] = cost[1] = cost[2] = 0;
	    for (i = 0; i<2; i++) {
    	    hz_sr = iClip3(2, 25,(apParam->u32HorizontalBSR[i]+7)>>3);
    	    hz_sr = iMin(hz_sr, (p_Vid->frame_width-16)>>3);
    	    vt_sr = iClip3(2, 15, (apParam->u32VerticalBSR[i]+7)>>3);
            vt_sr = iMin(vt_sr, (p_Vid->frame_height-16)>>3);
    	    cost[0] += HORZ_MB(hz_sr)*vt_sr*4;
    	    cost[1] += (HORZ_MB(hz_sr)*vt_sr/2 + HORZ_MB(hz_sr))*8;
    	    cost[2] += (HORZ_MB(hz_sr)+1)*(vt_sr+1)*16;
    	}
    	if (cost[0] > 320 || cost[1] > 480 || cost[2] > 1760) {
    	    E_DEBUG("search range too large\n");
    	    return H264E_ERR_INPUT_PARAM;
    	}
	}
    ================================================================ */
//#endif
    // one ref P: horz 200, vert 16
    //            horz 16, vert 120 (bound by DMA)
	max_hzsr = max_vtsr = 0;
    for (i = 0; i<MAX_NUM_REF_FRAMES; i++) {
	//for (i = 0; i<p_Vid->ref_l0_num; i++) {
		hz_sr = iClip3(2, 25, (apParam->u32HorizontalSR[i]+7)>>3);
		hz_sr = iMin(hz_sr, (p_Vid->frame_width-16)>>3);
		vt_sr = iClip3(2, 15, (apParam->u32VerticalSR[i]+7)>>3);
		vt_sr = iMin(vt_sr, (p_Vid->frame_height-16)>>3);
		p_Vid->vt_sr[P_SLICE][i] = p_Vid->vt_sr[I_SLICE][i] = vt_sr;
		p_Vid->hz_sr[P_SLICE][i] = p_Vid->hz_sr[I_SLICE][i] = hz_sr;
		if (max_hzsr < hz_sr) {
			max_hzsr = hz_sr;
			p_Vid->max_hzsr_idx[P_SLICE] = i;
		}
		if (max_vtsr < vt_sr) {
		    max_vtsr = vt_sr;
		    p_Vid->max_vtsr_idx[P_SLICE] = i;
		}
	}

	p_Vid->max_hzsr_idx[I_SLICE] = 0;
	p_Vid->max_vtsr_idx[I_SLICE] = 0;
	//max_hzsr = max_vtsr = 0;
	//for (i = 0; i<2; i++) {
        hz_sr = iClip3(2, 25, (apParam->u32HorizontalBSR[0]+7)>>3);
        p_Vid->hz_sr[B_SLICE][0] = iMin(hz_sr, (p_Vid->frame_width-16)>>3);
        vt_sr = iClip3(2, 15, (apParam->u32VerticalBSR[0]+7)>>3);
        p_Vid->vt_sr[B_SLICE][0] = iMin(vt_sr, (p_Vid->frame_height-16)>>3);
        
        hz_sr = iClip3(2, 25, (apParam->u32HorizontalBSR[1]+7)>>3);
        p_Vid->hz_sr[B_SLICE][1] = iMin(hz_sr, (p_Vid->frame_width-16)>>3);
        vt_sr = iClip3(2, 15, (apParam->u32VerticalBSR[1]+7)>>3);
        p_Vid->vt_sr[B_SLICE][1] = iMin(vt_sr, (p_Vid->frame_height-16)>>3);
        
    	//p_Vid->hz_sr[B_SLICE][i] = hz_sr;
    	//p_Vid->vt_sr[B_SLICE][i] = vt_sr;
    	if (p_Vid->hz_sr[B_SLICE][0] >= p_Vid->hz_sr[B_SLICE][1])
    	    p_Vid->max_hzsr_idx[B_SLICE] = 0;
        else
            p_Vid->max_hzsr_idx[B_SLICE] = 1;
        if (p_Vid->vt_sr[B_SLICE][0] >= p_Vid->vt_sr[B_SLICE][1])
            p_Vid->max_vtsr_idx[B_SLICE] = 0;
        else
            p_Vid->max_vtsr_idx[B_SLICE] = 1;
        /*
    	if (max_hzsr < hz_sr) {
    	    max_hzsr = hz_sr;
    	    p_Vid->max_hzsr_idx[B_SLICE] = i;
    	}
    	if (max_vtsr < vt_sr) {
    	    max_vtsr = vt_sr;
    	    p_Vid->max_vtsr_idx[B_SLICE] = i;
    	}
    	*/
    //}
	p_Vid->intra_4x4_mode_number = apParam->bIntra4x4Mode;
	p_Vid->ori_dis_coef_thd = p_Vid->disable_coef_threshold = apParam->bDisableCoefThd;
	p_Vid->luma_coef_thd = apParam->u8LumaCoefThd;
	p_Vid->chroma_coef_thd = apParam->u8ChromaCoefThd;

	p_Vid->fme_cost_thd = apParam->u32FMECostThd;
	p_Vid->fme_cyc_thd[0] = apParam->u32FMECycThd[0];
	p_Vid->fme_cyc_thd[1] = apParam->u32FMECycThd[1];
	p_Vid->currSlice->direct_spatial_mv_pred_flag = apParam->bDirectSpatialMVPred;
	p_Vid->inter_default_trans_size = apParam->bInterDefaultTransSize;

	p_Vid->inter_pred_mode_dis[P_SLICE] = apParam->u8PInterPredModeDisable;
	p_Vid->inter_pred_mode_dis[B_SLICE] = apParam->u8BInterPredModeDisable;
	p_Vid->inter_pred_mode_dis[I_SLICE] = apParam->u8PInterPredModeDisable;

	p_Vid->mb_qp_weight = apParam->u8MBQPWeight;
    if (p_Vid->mb_qp_weight > 0 && p_Vid->mb_qp_weight < 5) {
        if (apParam->u8MaxDeltaQP == 0) {
            favce_err("max delat qp can not be zero\n");
            return H264E_ERR_INPUT_PARAM;
        }
        if (apParam->u32QPDeltaThd == 0) {
            favce_err("threshold of delta qp by variance can not be zero\n");
            return H264E_ERR_INPUT_PARAM;
        }
    }
#ifdef ENABLE_MB_RC
    // mb_bak_weight must be 4 or 5
    if (4 == p_Vid->mb_qp_weight || 5 == p_Vid->mb_qp_weight)
        p_Vid->mb_bak_weight = p_Vid->mb_qp_weight;
    else
        p_Vid->mb_bak_weight = 5;
#endif
	//p_Vid->original_delta_qp_strength = p_Vid->delta_qp_strength = apParam->u8QPDeltaStrength;
	p_Vid->delta_qp_strength = apParam->u8QPDeltaStrength;
	p_Vid->delta_qp_thd = apParam->u32QPDeltaThd;
	p_Vid->max_delta_qp = apParam->u8MaxDeltaQP;
	p_Vid->src_img_type = apParam->bSrcImageType;
	//row_mb_num = p_Vid->frame_width/16;
	// rate control
	if (p_Vid->mb_qp_weight <= 3 && apParam->u32RCBasicUnit == 0) {
	    E_DEBUG("set mb delta qp but not set basic unit\n");
		return H264E_ERR_INPUT_PARAM;
	}

    if (p_Vid->mb_qp_weight < 4) {
        int slice_mb_number;
        if (0 == apParam->u32SliceMBRowNumber)
            slice_mb_number = p_Vid->field_pic_flag ? p_Vid->total_mb_number/2 : p_Vid->total_mb_number;
        else
            slice_mb_number = apParam->u32SliceMBRowNumber * p_Vid->frame_width/16;

    #ifdef C_MODEL_CONSTRAINT
        if ((apParam->u32RCBasicUnit % (p_Vid->frame_width/16)) != 0) {
            E_DEBUG("basic unit(%d) must be multiple of mb width(%d)\n", 
                apParam->u32RCBasicUnit, p_Vid->frame_width/16);
    		return H264E_ERR_INPUT_PARAM;
        }

        if (apParam->bEntropyCoding) {
            int total_mb = p_Vid->field_pic_flag ? p_Vid->total_mb_number/2 : p_Vid->total_mb_number;
            if (apParam->u32RCBasicUnit < total_mb && 
                (apParam->u32RCBasicUnit % slice_mb_number) != 0) {
                E_DEBUG("CABAC basic unit(%d) must be multiple of slice mb number(%d)\n", 
                    apParam->u32RCBasicUnit, slice_mb_number);
        		return H264E_ERR_INPUT_PARAM;
            }
        }
        else {
            if (apParam->u32RCBasicUnit > 0 && (slice_mb_number%apParam->u32RCBasicUnit) != 0) {
                E_DEBUG("CAVLC basic unit(%d) must be factor of slice mb number(%d)\n", 
                    apParam->u32RCBasicUnit, slice_mb_number);
        		return H264E_ERR_INPUT_PARAM;
            }
        }
    #else
        if (apParam->u32RCBasicUnit > 0 && (slice_mb_number%apParam->u32RCBasicUnit) != 0) {
            E_DEBUG("basic unit(%d) must be factor of slice mb number(%d)\n", apParam->u32RCBasicUnit, slice_mb_number);
            return H264E_ERR_INPUT_PARAM;
        }
    #endif
    }

	p_Vid->rc_basicunit = apParam->u32RCBasicUnit;
	if (p_Vid->rc_basicunit == 0)
		p_Vid->rc_qp_step = 1;
	else
		p_Vid->rc_qp_step = ((p_Vid->total_mb_number/p_Vid->rc_basicunit)>=9 ? 1 : 2);
	p_Vid->bit_rate = apParam->u32MBRCBitrate;
	//p_Vid->frame_rate = pParam->u32FrameRate;
	if (0 == apParam->u32num_unit_in_tick || 0 == apParam->u32time_unit) {
        p_Vid->num_unit_in_tick = 1;
    	p_Vid->time_unit = 30;
    }
    else {
    	p_Vid->num_unit_in_tick = apParam->u32num_unit_in_tick;
    	p_Vid->time_unit = apParam->u32time_unit;
    }
	p_Vid->bitbudget_frame = 0; //pParam->u32MBRCBitrate/pParam->u32FrameRate;
	if (p_Vid->rc_basicunit == 0)
		p_Vid->basicunit_num = 1;
	else
		p_Vid->basicunit_num = iMax(1, p_Vid->total_mb_number/p_Vid->rc_basicunit); // can not be zero
	//p_Vid->bitbudget_bu = p_Vid->bitbudget_frame/p_Vid->basicunit_num;
	for (i = 0; i<3; i++) {
		p_Vid->rc_max_qp[i] = apParam->u8RCMaxQP[i];
		p_Vid->rc_min_qp[i] = apParam->u8RCMinQP[i];
	}

    if (apParam->u32SliceMBRowNumber == 0 || (apParam->u32SliceMBRowNumber > (p_Vid->field_pic_flag ? p_Vid->frame_height/32 : p_Vid->frame_height/16))) {
        p_Vid->slice_line_number = (p_Vid->field_pic_flag ? p_Vid->frame_height/32 : p_Vid->frame_height/16);
        p_Vid->slice_number = 1;
    }
    else {
        p_Vid->slice_line_number = apParam->u32SliceMBRowNumber;
        p_Vid->slice_number = iCeilDiv((p_Vid->field_pic_flag ? p_Vid->frame_height/32 : p_Vid->frame_height/16), p_Vid->slice_line_number);
    }

#ifdef C_MODEL_CONSTRAINT
    if (apParam->bEntropyCoding && apParam->u8MBQPWeight < 4) {
        if (((apParam->u32RCBasicUnit / (p_Vid->frame_width/16)) % p_Vid->slice_line_number) != 0 &&
            apParam->u32RCBasicUnit < p_Vid->total_mb_number) {
            E_DEBUG("basic unit(%d) must be muleiple of slice MB when CABAC coding\n", apParam->u32RCBasicUnit);
            return H264E_ERR_INPUT_PARAM;
        }
    }
#endif

#if !(ENABLE_TEMPORL_DIDN_WITH_B)
	if ((apParam->u8DiDnMode&(TEMPORAL_DENOISE | TEMPORAL_DEINTERLACE)) && apParam->u8BFrameNumber) {
	    E_DEBUG("when temporal denoise/deinterlace on, b frame number must be zero\n");
		return H264E_ERR_INPUT_PARAM;
	}
#endif
	p_Vid->didn_mode = apParam->u8DiDnMode;
	if (p_Vid->didn_mode == 0)
		p_Vid->src_mode = BYPASS_MODE;
	else if ((p_Vid->didn_mode & (SPATIAL_DEINTERLACE | TEMPORAL_DEINTERLACE)) == 0)
		p_Vid->src_mode = ITL_MODE;
	else
		p_Vid->src_mode = ELSE_MODE;
	//p_Vid->itl_mode = ((p_Vid->didn_mode & (SPATIAL_DEINTERLACE | TEMPORAL_DEINTERLACE))==0);	// include bypass mode
	/*	                        itl_mode 
	 *	                |    0    |      1
	 *	             ---+---------+-------------
	 *	src_img_type  0 |  frame  | frame/field
	 *	             ---+---------+-------------
	 *	              1 |  frame  | frame/field
	 *	                     ^        ^
	 *	         progressive mode   interleave mode */
	if (p_Vid->src_mode == ELSE_MODE && p_Vid->field_pic_flag) {    // not itl & not bypass mode
	    E_DEBUG("when not itl & bypass mode, not support field coding\n");
		return H264E_ERR_INPUT_PARAM;
	}

	if (pParam->ptDiDnParam) {
		p_Vid->didn_param_enable = 1;
		memcpy(&p_Vid->didn_param, pParam->ptDiDnParam, sizeof(p_Vid->didn_param));
	}
	else
		p_Vid->didn_param_enable = 0;

	p_Vid->dn_result_format = pParam->u8DnResultFormat;
	p_Vid->sad_source = apParam->bSADSource;
	
	p_Vid->ave_frame_qp = -1;
	p_Vid->wk_enable = apParam->bWatermarkCtrl;
    p_Vid->wk_init_pattern =
	p_Vid->wk_pattern = apParam->u32WatermarkPattern;
    p_Vid->wk_reinit_period = apParam->u32WKReinitPeriod;
    p_Vid->wk_frame_number = 0;
    p_Vid->encoded_frame_num = 0;   // reset fraem number when reinit

    // Tuba 20121015 MCNR start
#ifdef MCNR_ENABLE
    p_Vid->ori_mcnr_en = p_Vid->mcnr_en = apParam->bMCNREnable;
    if (p_Vid->mcnr_en) {
        if (apParam->u8MCNRShift > 3) {
            E_DEBUG("mcnr sad shift must between 0~3 (%d)\n", apParam->u8MCNRShift);
            return H264E_ERR_INPUT_PARAM;
        }
        p_Vid->mcnr_sad_shift = apParam->u8MCNRShift;   // 0~3
        p_Vid->mcnr_mv_thd = apParam->u32MCNRMVThd;     // 10 bits
        p_Vid->mcnr_var_thd = apParam->u32MCNRVarThd;   // 8 bits
        ptReg_mcnr = (H264Reg_MCNR *)(p_Enc->u32BaseAddr + MCP280_OFF_MCNR);
        for (i = 0; i<52; i+=4) {
            ptReg_mcnr->MCNR_LSAD[i/4] = (apParam->mMCNRTable.u8LSAD[i]
                | (apParam->mMCNRTable.u8LSAD[i+1]<<8)
                | (apParam->mMCNRTable.u8LSAD[i+2]<<16)
                | (apParam->mMCNRTable.u8LSAD[i+3]<<24));
        }
        for (i = 0; i<52; i+=4) {
            ptReg_mcnr->MCNR_HSAD[i/4] = (apParam->mMCNRTable.u8HSAD[i]
                | (apParam->mMCNRTable.u8HSAD[i+1]<<8)
                | (apParam->mMCNRTable.u8HSAD[i+2]<<16)
                | (apParam->mMCNRTable.u8HSAD[i+3]<<24));
        }
    }
#endif
    // Tuba 20121015 end

	// if watermark enable, i8LumaCoefThd and i8ChromaCoefThd must be zero (disbale)
	if (p_Vid->wk_enable) {
		p_Vid->disable_coef_threshold = 1;
		// Tuba 20121203: disable mcnr when watermark is enable
		p_Vid->mcnr_en = 0;
	}
//#ifdef FIXED_QP
	p_Vid->fixed_qp[I_SLICE] = apParam->i8FixedQP[I_SLICE];
	p_Vid->fixed_qp[P_SLICE] = apParam->i8FixedQP[P_SLICE];
	p_Vid->fixed_qp[B_SLICE] = apParam->i8FixedQP[B_SLICE];
//#endif

	p_Vid->intra_8x8_disable = apParam->bIntra8x8Disable;
	p_Vid->fast_intra_4x4 = apParam->bFastIntra4x4;
	p_Vid->intra_16x16_plane_disbale = apParam->bIntra16x16PlaneDisable;

    if (p_Vid->roi_flag && apParam->bIntTopBtm == 0 && p_Vid->src_img_type == 0) {
        apParam->bIntTopBtm = 1;
        favce_wrn("can not close top bottom interleave when source is top bottom interleave and roi enable");
    }
    p_Vid->int_top_btm = apParam->bIntTopBtm;
    p_Vid->disable_intra_in_pb = apParam->bIntraDisablePB;
    
    // 0x00, 0x03: intra4x4 and intra16x16 both enable
    // 0x01: intra16x16 enable, intra4x4 disable
    // 0x02: intra16x16 disbale, intra4x4 enable
    if (apParam->bIntra4x4DisableI && apParam->bIntra16x16DisableI) {
        favce_wrn("can not disable both intra4x4 and intra16x16 in I slice");
        p_Vid->disable_intra_in_i = 0x00;
    }
    else {
        if (apParam->bIntra4x4DisableI)
            p_Vid->disable_intra_in_i = 0x01;
        else if (apParam->bIntra16x16DisableI)
            p_Vid->disable_intra_in_i = 0x02;
        else
            p_Vid->disable_intra_in_i = 0x00;
    }

    if (apParam->u8IntraCostRatio > 15) {
        favce_wrn("intra cost ratio(%d) can not over 15\n", apParam->u8IntraCostRatio);
        apParam->u8IntraCostRatio = 0;
    }
    p_Vid->intra_cost_ratio = apParam->u8IntraCostRatio;
    if (apParam->u32ForceMV0Thd > 0xFFFF) {
        favce_wrn("variance threshold(%d) can not over 65535\n", apParam->u32ForceMV0Thd);
        apParam->u32ForceMV0Thd = 0;
    }
    p_Vid->force_mv0_thd = apParam->u32ForceMV0Thd;

	calculate_quant_param(p_Vid->active_sps, &p_Vid->mQuant);

    // roi qp setting
    p_Vid->roi_qp_type = apParam->u8RoiQPType;  // 0: disable, 1: delta qp , 2: fixed qp
    if (ROI_DELTA_QP == p_Vid->roi_qp_type)
        p_Vid->roi_qp = apParam->i8ROIDeltaQP;
        //p_Vid->roi_qp = apParam->u8ROIDeltaQP;
    else if (ROI_FIXED_QP == p_Vid->roi_qp_type)
        p_Vid->roi_qp = apParam->u8ROIFixQP;

    // Tuba 20120518 start: add condition of delta & ROI qp
    if (p_Vid->mb_qp_weight > 0 && p_Vid->mb_qp_weight <= 4) {  // image variance
        if (ROI_DELTA_QP == p_Vid->roi_qp_type) {
            if (p_Vid->roi_qp > 0 && p_Vid->roi_qp + p_Vid->max_delta_qp > 25) {
                p_Vid->roi_qp = 25 - p_Vid->max_delta_qp;  
                favce_wrn("roi delta qp is too large, change to %d\n", p_Vid->roi_qp);
            }
            else if (p_Vid->roi_qp < 0 && p_Vid->roi_qp - p_Vid->max_delta_qp < -26) {
                p_Vid->roi_qp = -26 + p_Vid->max_delta_qp;
                favce_wrn("roi delta qp is too small, change to %d\n", p_Vid->roi_qp);
            }
        }
        else if (ROI_FIXED_QP == p_Vid->roi_qp_type) {
            // add condition @ trigger slice
        #ifdef FIXED_QP
            /*
            int maxQP, minQP, tmpQP;
            maxQP = iMax(p_Vid->fixed_qp[I_SLICE], iMax(p_Vid->fixed_qp[P_SLICE], p_Vid->fixed_qp[B_SLICE]));
            minQP = iMin(p_Vid->fixed_qp[I_SLICE], iMin(p_Vid->fixed_qp[P_SLICE], p_Vid->fixed_qp[B_SLICE]));
            minQP = iMax(minQP - p_Vid->max_delta_qp, 0);
            maxQP = iMin(maxQP + p_Vid->max_delta_qp, 51);
            if (p_Vid->roi_qp > minQP + 25 || p_Vid->roi_qp < maxQP - 26) {
                tmpQP = (maxQP + minQP)/2;
                if (tmpQP > minQP + 25 || tmpQP < maxQP - 26)
                    return H264E_ERR_INPUT_PARAM;
                p_Vid->roi_qp = tmpQP;
                favce_wrn("roi delta qp may out of range, change to %d\n", p_Vid->roi_qp);
            }
            */
        #endif
        }
    }
    // Tuba 20120518 end

    //if (p_Vid->didn_mode & TEMPORAL_DENOISE)    // temporal de-noise
    //    set_gamma_table(ptReg_mem, p_Vid->gamma_level);
	return H264E_OK;
}

int encoder_init_vui(void *ptHandle, FAVC_ENC_VUI_PARAM *pVUI)
{
	EncoderParams *p_Enc = (EncoderParams *)ptHandle;

	if (pVUI) {
		p_Enc->p_Vid->active_sps->vui_parameters_present_flag = 1;
		prepare_vui(&p_Enc->p_Vid->active_sps->vui_seq_parameter, pVUI);
	}
	else
		p_Enc->p_Vid->active_sps->vui_parameters_present_flag = 0;
	return 0;
}

int encoder_release(void *ptHandle)
{
	EncoderParams *p_Enc = (EncoderParams *)ptHandle;
	int i;
	if (p_Enc) {
		if (p_Enc->p_Vid->currSlice)	p_Enc->pfnFree(p_Enc->p_Vid->currSlice);
		if (p_Enc->p_Vid->active_sps)	p_Enc->pfnFree(p_Enc->p_Vid->active_sps);
		if (p_Enc->p_Vid->active_pps)	p_Enc->pfnFree(p_Enc->p_Vid->active_pps);
        if (p_Enc->p_Vid->dec_picture)  p_Enc->pfnFree(p_Enc->p_Vid->dec_picture);
		if (p_Enc->p_Vid->p_Dpb) {
			for (i = 0; i<MAX_DPB_SIZE; i++) {
				if (p_Enc->p_Vid->p_Dpb->fs[i]->frame)	p_Enc->pfnFree(p_Enc->p_Vid->p_Dpb->fs[i]->frame);
				if (p_Enc->p_Vid->p_Dpb->fs[i]->topField)	p_Enc->pfnFree(p_Enc->p_Vid->p_Dpb->fs[i]->topField);
				if (p_Enc->p_Vid->p_Dpb->fs[i]->btmField)	p_Enc->pfnFree(p_Enc->p_Vid->p_Dpb->fs[i]->btmField);
				if (p_Enc->p_Vid->p_Dpb->fs[i])	p_Enc->pfnFree(p_Enc->p_Vid->p_Dpb->fs[i]);
			}
			p_Enc->pfnFree(p_Enc->p_Vid->p_Dpb);
		}
		if (p_Enc->p_Vid)	p_Enc->pfnFree(p_Enc->p_Vid);
		p_Enc->pfnFree(p_Enc);
	}
	return H264E_OK;
}

int get_motion_info_length(void *ptHandle)
{
    VideoParams *p_Vid;
    if (ptHandle == NULL)
        return -1;
    p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
    return p_Vid->total_mb_number*2*4;
}

int encode_update_fixed_qp(void *ptHandle, int fixed_qp_I, int fixed_qp_P, int fixed_qp_B)
{
    VideoParams *p_Vid;
    if (ptHandle == NULL)
        return -1;
    p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
    p_Vid->fixed_qp[I_SLICE] = fixed_qp_I;    
	p_Vid->fixed_qp[P_SLICE] = fixed_qp_P;
	p_Vid->fixed_qp[B_SLICE] = fixed_qp_B;
    return 0;
}

#ifdef ENABLE_FAST_FORWARD
int encoder_reset_gop_param(void *ptHandle, int idr_period, int b_frame, int fast_forward)
#else
int encoder_reset_gop_param(void *ptHandle, int idr_period, int b_frame)
#endif
{
    VideoParams *p_Vid;
    if (NULL == ptHandle)
        return -1;
    p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
    if (idr_period <= 0) {
        favce_err("reset gop parameter error, idr period = %d -> %d", p_Vid->idr_period, idr_period);
        return -1;
    }
    if (b_frame < 0 || b_frame > MAX_B_NUMBER) {
        favce_wrn("reset gop parameter error, b frame = %d -> %d\n", p_Vid->number_Bframe, b_frame);
        return -1;
    }
#ifdef ENABLE_FAST_FORWARD
    if (fast_forward) {
        if (fast_forward >= MIN_FASTFORWARD && fast_forward <= MAX_FASTFORWARD) {
            if (b_frame) {
                favce_wrn("fast forward enable, B frame can not be enable\n");
                b_frame = 0;
            }
            if ((idr_period%fast_forward) != 0) {
                favce_wrn("IDR interval must be multiple of fast forward(%d), force idr_period = %d\n",
                    fast_forward, (idr_period+fast_forward-1)/fast_forward*fast_forward);
                idr_period = (idr_period+fast_forward-1)/fast_forward*fast_forward;
            }                
        }
        else {
            favce_wrn("fast forward can not be %d, disable fast forward\n", fast_forward);
            fast_forward = 0;
        }
    }
    p_Vid->fast_forward = fast_forward;
#endif
    p_Vid->idr_period = idr_period;
    p_Vid->number_Bframe = b_frame;

    // update
    if (b_frame)
        p_Vid->active_sps->num_ref_frames = 2;
    else
        p_Vid->active_sps->num_ref_frames = 1;

    p_Vid->last_idr_poc = p_Vid->gop_poc_base;
	p_Vid->gop_poc_base = 0;
	p_Vid->frame_num = 0;
	p_Vid->gop_idr_cnt = 0;
	p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
	//p_Vid->encoded_frame_num = 0; // remove reset encoded frame number
#ifdef ENABLE_FAST_FORWARD
    p_Vid->gop_non_ref_p_cnt = 0;
#endif

    return 0;
}

int encoder_reset_wk_param(void *ptHandle, int wk_enable, int wk_init_interval, unsigned int wk_pattern)
{
    VideoParams *p_Vid;
    if (NULL == ptHandle)
        return -1;
    p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
	p_Vid->wk_enable = wk_enable;
    p_Vid->wk_init_pattern =
	p_Vid->wk_pattern = wk_pattern;
    p_Vid->wk_reinit_period = wk_init_interval;
    if (p_Vid->wk_enable) {
        p_Vid->disable_coef_threshold = 1;
        p_Vid->mcnr_en = 0;
    }
    else {
        p_Vid->disable_coef_threshold = p_Vid->ori_dis_coef_thd;
        p_Vid->mcnr_en = p_Vid->ori_mcnr_en;
    }

    return 0;
}

static int encoder_set_roi_xy(VideoParams *p_Vid, FAVC_ENC_IOCTL_FRAME *pFrame)
{
    if (!p_Vid->roi_flag)
        return 0;
    // Tuba 20140307: when over range, round x/y
#if 0
    if ((pFrame->u32ROI_x + p_Vid->roi_width > p_Vid->source_width) ||
        (pFrame->u32ROI_y + p_Vid->roi_height > p_Vid->source_height)) {
	    favce_err("roi is exceed source image, width %d + %d > %d, height %d + %d > %d\n", 
            pFrame->u32ROI_x, p_Vid->roi_width, p_Vid->source_width, pFrame->u32ROI_y, p_Vid->roi_height, p_Vid->source_height);
		return H264E_ERR_INPUT_PARAM;
    }
    /* base address must be 8-byte align */
    if (pFrame->u32ROI_x & 0x03) {
        favce_err("x position(%d) of roi must be multiple of 4\n", pFrame->u32ROI_x);
        return H264E_ERR_INPUT_PARAM;
    }
#else
    if (pFrame->u32ROI_x + p_Vid->roi_width > p_Vid->source_width) {
        favce_wrn("roi x exceed source image (%d + %d > %d), force to be %d\n", 
            pFrame->u32ROI_x, p_Vid->roi_width, p_Vid->source_width, p_Vid->source_width - p_Vid->roi_width);
        pFrame->u32ROI_x = p_Vid->source_width - p_Vid->roi_width;
    }
    if (pFrame->u32ROI_y + p_Vid->roi_height > p_Vid->source_height) {
        favce_wrn("roi y exceed source image (%d + %d > %d), force to be %d\n",
            pFrame->u32ROI_y, p_Vid->roi_height, p_Vid->source_height, p_Vid->source_height - p_Vid->roi_height);
        pFrame->u32ROI_y = p_Vid->source_height - p_Vid->roi_height;
    }
    /* base address must be 8-byte align */
    if (pFrame->u32ROI_x & 0x03) {
        favce_wrn("x position(%d) of roi must be multiple of 4, force to be %d\n",
            pFrame->u32ROI_x, ((pFrame->u32ROI_x>>2)<<2));
        pFrame->u32ROI_x = ((pFrame->u32ROI_x>>2)<<2);
    }
#endif

    p_Vid->roi_x = pFrame->u32ROI_x;
    p_Vid->roi_y = pFrame->u32ROI_y;
    return 0;
}

int encoder_get_slice_type(void *ptHandle)
{
    VideoParams *p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
    if (!p_Vid->gop_idr_cnt)
        return I_SLICE;
    else if (!p_Vid->gop_b_remain_cnt)
        return P_SLICE;
    else
        return B_SLICE;
    return -1;
}

int encoder_check_iframe(void *ptHandle, int idr_period)
{
    VideoParams *p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
#if 0
    if (0 == (p_Vid->gop_idr_cnt%idr_period))
        return 1;
#else
    if (p_Vid->idr_period - p_Vid->gop_idr_cnt >= idr_period)
        return 1;
#endif
    return 0;
}

#ifdef SAME_REC_REF_BUFFER
int encoder_check_vcache_enable(void *ptHandle)
{
    VideoParams *p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
#if MCP280_VCACHE_ENABLE
    if (p_Vid->frame_width >= 208 && p_Vid->frame_width <= VCACHE_MAX_REF0_WIDTH) {
        return 1;
    }
#endif
    return 0;
}
#endif

#ifdef RC_ENABLE
int encoder_rc_set_quant(void *ptHandle, RC_GET_QUANT_ptr pfnRCGetQuant, void *rc_handler, int force_qp)
{
    VideoParams *p_Vid = ((EncoderParams *)ptHandle)->p_Vid;
    StorablePicture *pic = p_Vid->dec_picture;
    struct rc_frame_info_t rc_data;

    if (NULL == ptHandle || NULL == pfnRCGetQuant || NULL == rc_handler) {
        p_Vid->rc_qpy = p_Vid->initial_qp = p_Vid->currQP = p_Vid->fixed_qp[pic->slice_type];
        return 0;
    }

    if (force_qp > 0) {
        rc_data.force_qp = force_qp;
    }
    else
        rc_data.force_qp= -1;
    p_Vid->slice_type = 
    rc_data.slice_type = pic->slice_type;
    //rc_data.last_satd = p_Vid->total_cost;
    rc_data.last_satd = p_Vid->last_cost;
    if (B_SLICE == pic->slice_type) {
        rc_data.list[0].avg_qp = p_Vid->ref_list[0][0]->ave_qp;
        rc_data.list[0].poc = p_Vid->ref_list[0][0]->poc;
        rc_data.list[0].slice_type = p_Vid->ref_list[0][0]->slice_type;
        rc_data.list[1].avg_qp = p_Vid->ref_list[1][0]->ave_qp;
        rc_data.list[1].poc = p_Vid->ref_list[1][0]->poc;
        rc_data.list[1].slice_type = p_Vid->ref_list[1][0]->slice_type;
    }
    rc_data.cur_frame.poc = pic->poc;
    p_Vid->rc_qpy = p_Vid->currQP = pfnRCGetQuant(rc_handler, &rc_data);
#ifdef ENABLE_MB_RC
    if (0 != rc_data.frame_size) {
        p_Vid->bitbudget_frame = rc_data.frame_size;
        p_Vid->bitbudget_bu = p_Vid->bitbudget_frame/p_Vid->basicunit_num;
        p_Vid->mb_qp_weight = 0;
    }
    else {
        p_Vid->mb_qp_weight = p_Vid->mb_bak_weight;
        //p_Vid->mb_qp_weight = 5;
    }
#endif
    return p_Vid->currQP;
}

int encoder_rc_update(void *ptHandle, RC_UPDTAE_ptr pfnRCUpdate, void *rc_handler)
{
    EncoderParams *p_Enc = (EncoderParams *)ptHandle;
    VideoParams *p_Vid = p_Enc->p_Vid;
    struct rc_frame_info_t rc_data;
    
    rc_data.slice_type = p_Vid->slice_type;
    p_Vid->last_cost = 
    rc_data.last_satd = p_Vid->total_cost;
    rc_data.frame_size = p_Enc->u32ResBSLength;
    rc_data.avg_qp = p_Vid->currQP;
#ifdef ENABLE_MB_RC
    if (p_Vid->mb_qp_weight < 4 && p_Vid->ave_frame_qp > 0)
        rc_data.avg_qp_act = p_Vid->ave_frame_qp;
    else
#endif
        rc_data.avg_qp_act = p_Vid->currQP;
    pfnRCUpdate(rc_handler, &rc_data);
    return 0;
}
#endif

static int prepare_param(VideoParams *p_Vid, StorablePicture *pic, SourceBuffer *pSrcBuf)
{
	int MaxPicOrderCntLsb = 1<<(p_Vid->active_sps->log2_max_pic_order_cnt_lsb_minus4+4);
	int slice_type;
	//int default_setting = 1;

    if (p_Vid->force_intra) {
        DEBUG_M(LOG_DEBUG, "force I frame I\n");
        p_Vid->last_idr_poc = p_Vid->gop_poc_base;  // Tuba 20120216: add last idr to update b frame poc
		pic->slice_type = I_SLICE;
		pic->idr_flag = 1;
		pic->used_for_ref = 3;
		pic->poc = 0;
		pic->poc_lsb = 0;
		slice_type = pic->slice_type;
    #ifndef DIVIDE_GOP_PARAM_SETTING
		p_Vid->gop_poc_base = 0;
		p_Vid->frame_num = 0;
		p_Vid->gop_idr_cnt = (p_Vid->idr_period ? p_Vid->idr_period : 1);
		p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
    #endif
    }
    else {
        switch (p_Vid->force_slice_type) {
        case I_SLICE:
            DEBUG_M(LOG_DEBUG, "assign slice type: I\n");
            p_Vid->last_idr_poc = p_Vid->gop_poc_base;
			pic->slice_type = I_SLICE;
			pic->idr_flag = 1;
			pic->used_for_ref = 3;
			pic->poc = 0;
			pic->poc_lsb = 0;
			slice_type = pic->slice_type;
            break;
        case P_SLICE:
            DEBUG_M(LOG_DEBUG, "assign slice type: P\n");
            p_Vid->last_idr_poc = 0;    // Tuba 20120216: add last idr to update b frame poc
			pic->slice_type = P_SLICE;
			pic->idr_flag = 0;
			pic->used_for_ref = 2;
			pic->poc = p_Vid->gop_poc_base;		
			pic->poc_lsb = p_Vid->gop_poc_base%MaxPicOrderCntLsb;
			slice_type = pic->slice_type;

            p_Vid->gop_idr_cnt = 1;
            break;
        case B_SLICE:
            DEBUG_M(LOG_DEBUG, "assign slice type: B\n");
            pSrcBuf->ThisPOC = p_Vid->gop_poc_base;
			pSrcBuf->lsb = p_Vid->gop_poc_base%MaxPicOrderCntLsb;
			slice_type = B_SLICE;

            p_Vid->gop_idr_cnt = 1;
            break;
        default:
            DEBUG_M(LOG_DEBUG, "gop idr cnt = %d, gop poc base = %d, gop b remain cnt = %d\n", p_Vid->gop_idr_cnt, p_Vid->gop_poc_base, p_Vid->gop_b_remain_cnt);
    		if (!p_Vid->gop_idr_cnt) {  // I frame
                p_Vid->last_idr_poc = p_Vid->gop_poc_base;
    			pic->slice_type = I_SLICE;
    			pic->idr_flag = 1;
    			pic->used_for_ref = 3;
    			pic->poc = 0;
    			pic->poc_lsb = 0;
    			slice_type = pic->slice_type;
            #ifndef DIVIDE_GOP_PARAM_SETTING
    			p_Vid->gop_poc_base = 0;
    			p_Vid->frame_num = 0;
    			p_Vid->gop_idr_cnt = (p_Vid->idr_period ? p_Vid->idr_period : 1);
    			p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
            #endif
    		}
    		else if (!p_Vid->gop_b_remain_cnt) {    // P frame
                p_Vid->last_idr_poc = 0;    // Tuba 20120216: add last idr to update b frame poc
    			pic->slice_type = P_SLICE;
    			pic->idr_flag = 0;
            #ifdef ENABLE_FAST_FORWARD
				if (p_Vid->fast_forward > 1) {
					if (p_Vid->gop_non_ref_p_cnt) {
                    	pic->used_for_ref = 0;
                 	}
                	else
                    	pic->used_for_ref = 2;
				} else {
					pic->used_for_ref = 2;
				}
            #else
    			pic->used_for_ref = 2;
            #endif
    			pic->poc = p_Vid->gop_poc_base;		
    			pic->poc_lsb = p_Vid->gop_poc_base%MaxPicOrderCntLsb;
    			slice_type = pic->slice_type;
            #ifndef DIVIDE_GOP_PARAM_SETTING
    			p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;// ? p_Vid->number_Bframe : 1);
    	    #endif
    		}
    		else {
    			pSrcBuf->ThisPOC = p_Vid->gop_poc_base;
    			pSrcBuf->lsb = p_Vid->gop_poc_base%MaxPicOrderCntLsb;
    			slice_type = B_SLICE;
            #ifndef DIVIDE_GOP_PARAM_SETTING
    			p_Vid->gop_b_remain_cnt--;
            #endif
    		}
			break;
        }
	}
#ifndef DIVIDE_GOP_PARAM_SETTING
	if (p_Vid->idr_period)
		p_Vid->gop_idr_cnt--;

	p_Vid->gop_poc_base += 2;
#endif
	return slice_type;
}

#ifdef DIVIDE_GOP_PARAM_SETTING
static int update_gop_param(VideoParams *p_Vid)
{
    if (p_Vid->force_intra) {
        p_Vid->gop_poc_base = 0;
        p_Vid->frame_num = 0;
        p_Vid->gop_idr_cnt = (p_Vid->idr_period ? p_Vid->idr_period : 1);
        p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
    #ifdef ENABLE_FAST_FORWARD
        p_Vid->gop_non_ref_p_cnt = 1;
    #endif
    }
    else {
        if (!p_Vid->gop_idr_cnt) {  // I frame
            p_Vid->gop_poc_base = 0;
            p_Vid->frame_num = 0;
            p_Vid->gop_idr_cnt = (p_Vid->idr_period ? p_Vid->idr_period : 1);
            p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;
        #ifdef ENABLE_FAST_FORWARD
            p_Vid->gop_non_ref_p_cnt = 1;
        #endif
        }
        else if (!p_Vid->gop_b_remain_cnt) {    // P frame
            p_Vid->gop_b_remain_cnt = p_Vid->number_Bframe;// ? p_Vid->number_Bframe : 1);
        #ifdef ENABLE_FAST_FORWARD
            p_Vid->gop_non_ref_p_cnt++;
            if (p_Vid->gop_non_ref_p_cnt == p_Vid->fast_forward)
                p_Vid->gop_non_ref_p_cnt = 0;
        #endif
        }
        else {
            p_Vid->gop_b_remain_cnt--;
        }
    }

    if (p_Vid->idr_period)
        p_Vid->gop_idr_cnt--;
    p_Vid->gop_poc_base += 2;

    if (p_Vid->last_ref_idc)
        p_Vid->frame_num++;
    
    return 0;
}
#endif

#ifndef VG_INTERFACE
static int encode_one_plane(EncoderParams *p_Enc, VideoParams *p_Vid, SliceHeader *currSlice, StorablePicture *pic)
{
	int ret = H264E_OK;
    //int slice_idx, start_mb_Y;
	init_lists(p_Vid, pic->slice_type);
//dump_list(p_Vid);

    // not set first_mb_in_slice = 0 & not start @ first slice because of handling bitstream overflw
	//currSlice->first_mb_in_slice = 0;
    for (;currSlice->slice_index < p_Vid->slice_number -1 ; currSlice->slice_index++) {
	//for (slice_idx = 0, start_mb_Y = 0; slice_idx < p_Vid->slice_number-1; slice_idx++, start_mb_Y += p_Vid->slice_line_number) {
        currSlice->last_slice = 0;
        //currSlice->start_Y_pos = start_mb_Y;
        currSlice->end_Y_pos = currSlice->start_Y_pos + p_Vid->slice_line_number - 1;
		ret = encode_one_slice(p_Enc);
		if (ret < 0) {
			return ret;
        }
        //currSlice->start_Y_pos += p_Vid->slice_line_number;
		//currSlice->first_mb_in_slice += p_Vid->slice_line_number*(p_Vid->frame_width/16); // sync
    }
    currSlice->last_slice = 1;
	//currSlice->start_Y_pos = start_mb_Y;
	currSlice->end_Y_pos = (p_Vid->field_pic_flag ? p_Vid->frame_height/32 : p_Vid->frame_height/16) - 1;
	ret = encode_one_slice(p_Enc);
	if (ret < 0) {
		return ret;
	}

	return ret;
}
#endif

static int init_meminfo(EncoderParams *p_Enc, VideoParams *p_Vid, FAVC_ENC_IOCTL_FRAME *pFrame)
{
    FAVC_ENC_FRAME *apFrame = &pFrame->apFrame;

    // Tuba 20140610: check buffer valid
    if (0 == pFrame->u32SysInfoAddr_phy || 0 == pFrame->u32MVInfoAddr_phy || 0 == pFrame->mReconBuffer->rec_luma_phy) {
        favce_err("buffer address can not be zero, sysinfo = 0x%x, mvinfo = 0x%x, rec = 0x%x\n", 
            pFrame->u32SysInfoAddr_phy, pFrame->u32MVInfoAddr_phy, pFrame->mReconBuffer->rec_luma_phy);
        return -1;        
    }
    if ((p_Vid->didn_mode & TEMPORAL_DENOISE) && 0 == pFrame->u32DiDnRef1_phy) {
        favce_err("buffer address can not be zero, didn result = 0x%x\n", pFrame->u32DiDnRef1_phy);
        return -1;
    }
    p_Enc->u32SysInfoAddr_virt = pFrame->u32SysInfoAddr_virt;
    p_Enc->u32SysInfoAddr_phy = pFrame->u32SysInfoAddr_phy;
    p_Enc->u32MVInfoAddr_virt[0] = pFrame->u32MVInfoAddr_virt;
    p_Enc->u32MVInfoAddr_phy[0] = pFrame->u32MVInfoAddr_phy;
    p_Enc->u32L1ColMVInfoAddr_virt[0] = pFrame->u32L1ColMVInfoAddr_virt;
    p_Enc->u32L1ColMVInfoAddr_phy[0] = pFrame->u32L1ColMVInfoAddr_phy;
	if (p_Vid->field_pic_flag) {
		p_Enc->u32MVInfoAddr_virt[1] = p_Enc->u32MVInfoAddr_virt[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->u32MVInfoAddr_phy[1] = p_Enc->u32MVInfoAddr_phy[0] + p_Vid->total_mb_number*2*4/2;
		p_Enc->u32L1ColMVInfoAddr_virt[1] = p_Enc->u32L1ColMVInfoAddr_virt[0] + p_Vid->total_mb_number*4*4/2;
		p_Enc->u32L1ColMVInfoAddr_phy[1] = p_Enc->u32L1ColMVInfoAddr_phy[0] + p_Vid->total_mb_number*4*4/2;
	}

    
#if ENABLE_TEMPORL_DIDN_WITH_B
    if (pFrame->mPreviousBuffer) {
    	p_Enc->u32DiDnRef0_virt = pFrame->mPreviousBuffer->src_buffer_virt;
	    p_Enc->u32DiDnRef0_phy = pFrame->mPreviousBuffer->src_buffer_phy;
    }
    else {
        p_Enc->u32DiDnRef0_virt = pFrame->u32DiDnRef0_virt;
    	p_Enc->u32DiDnRef0_phy = pFrame->u32DiDnRef0_phy;
    }
#else
    p_Enc->u32DiDnRef0_virt = pFrame->u32DiDnRef0_virt;
    p_Enc->u32DiDnRef0_phy = pFrame->u32DiDnRef0_phy;
#endif
    p_Enc->u32DiDnRef1_virt = pFrame->u32DiDnRef1_virt;
    p_Enc->u32DiDnRef1_phy = pFrame->u32DiDnRef1_phy;
	p_Vid->currQP = apFrame->u8CurrentQP;
	//p_Vid->assign_frame_type = apFrame->tAssignFrameType;
	p_Vid->force_intra = (1 == apFrame->bForceIntra ? 1 : 0);
	p_Enc->m_CurrSrcBuffer = pFrame->mSourceBuffer;
	p_Enc->m_CurrRecBuffer = pFrame->mReconBuffer;
    p_Vid->force_slice_type = apFrame->i32ForceSliceType;
	return H264E_OK;
}

static int check_roi_region(FAVC_ROI_QP_REGION *arrRegion, VideoParams *p_Vid)
{
    int i;
    int mb_width, mb_height;
    FAVC_ROI_QP_REGION *pRegion;
    
    if (p_Vid->roi_qp_type == DISBALE_ROI_QP)
        return -1;

    mb_width = p_Vid->frame_width/16;
    if (p_Vid->field_pic_flag)
        mb_height = p_Vid->frame_height/32;
    else
        mb_height = p_Vid->frame_height/16;
    for (i = 0; i<ROI_REGION_NUM; i++) {
        pRegion = &p_Vid->roi_qp_region[i];
        if (arrRegion[i].enable) {
        #ifdef DYNAMIC_GOP_ENABLE
            if (iframe_disable_roi && I_SLICE == p_Vid->dec_picture->slice_type) {
                pRegion->enable = 0;
                continue;
            }
        #endif
            pRegion->roi_x = arrRegion[i].roi_x/16;
            pRegion->roi_width = arrRegion[i].roi_width/16;
            if (pRegion->roi_x + pRegion->roi_width > mb_width || pRegion->roi_width == 0) {
                favce_wrn("roi qp region %d width out of range or width = 0 (%d + %d > %d)\n", i, arrRegion[i].roi_x, arrRegion[i].roi_width, p_Vid->frame_width);
                pRegion->enable = 0;
                continue;
            }
            if (p_Vid->field_pic_flag) {
                pRegion->roi_y = arrRegion[i].roi_y/32;
                pRegion->roi_height = arrRegion[i].roi_height/32;
            }
            else {
                pRegion->roi_y = arrRegion[i].roi_y/16;
                pRegion->roi_height = arrRegion[i].roi_height/16;
            }
            if (pRegion->roi_y + pRegion->roi_height > mb_height || pRegion->roi_height == 0) {
                favce_wrn("roi qp region %d height out of range or height = 0 (%d + %d > %d)\n", i, arrRegion[i].roi_y, arrRegion[i].roi_height, p_Vid->frame_height);
                pRegion->enable = 0;
                continue;
            }
            pRegion->enable = 1;
        }
        else {
            pRegion->enable = 0;
        }
    }

    return 0;
}

static int init_frame_param(EncoderParams *p_Enc, VideoParams *p_Vid, FAVC_ENC_IOCTL_FRAME *pFrame, int queue_frame)
{
	StorablePicture *pic = p_Vid->dec_picture;

	// prepare params
	if (!queue_frame) {
		if (prepare_param(p_Vid, p_Vid->dec_picture, p_Enc->m_CurrSrcBuffer) == B_SLICE) {
            // Tuba 20130326_0: B frame not assign refernece frame
			//pFrame->u8ReleaseIdx = pFrame->mReconBuffer->rec_buf_idx;
			// buffered roi qp setting
			memcpy(pFrame->mSourceBuffer->sROIQPRegion, pFrame->apFrame.sROIQPRegion, sizeof(p_Vid->roi_qp_region));
			showValue(0x10200000);
			return FRAME_WAIT;
		}
	}
	p_Vid->release_buffer_idx = NO_RELEASE_BUFFER;
	p_Vid->total_cost = 0;
	p_Vid->sum_mb_qp = 0;

	pic->srcpic_addr_virt = p_Enc->m_CurrSrcBuffer->src_buffer_virt;
	pic->srcpic_addr_phy = p_Enc->m_CurrSrcBuffer->src_buffer_phy;
	pic->recpic_luma_virt = p_Enc->m_CurrRecBuffer->rec_luma_virt;
	pic->recpic_luma_phy = p_Enc->m_CurrRecBuffer->rec_luma_phy;
	pic->recpic_chroma_virt = p_Enc->m_CurrRecBuffer->rec_chroma_virt;
	pic->recpic_chroma_phy = p_Enc->m_CurrRecBuffer->rec_chroma_phy;
	pic->phy_index = p_Enc->m_CurrRecBuffer->rec_buf_idx;

	if (queue_frame) {
        if (p_Vid->last_idr_poc) {
            unsigned int poc_mask = ~(0xffffffff << (p_Vid->active_sps->log2_max_pic_order_cnt_lsb_minus4+4));
            pic->poc = p_Enc->m_CurrSrcBuffer->ThisPOC - p_Vid->last_idr_poc;
       	    pic->poc_lsb = (pic->poc & poc_mask);
        }
        else {
            pic->poc = p_Enc->m_CurrSrcBuffer->ThisPOC;
       	    pic->poc_lsb = p_Enc->m_CurrSrcBuffer->lsb;
        }
		pic->used_for_ref = 0;
		pic->slice_type = B_SLICE;
		pic->idr_flag = 0;
		// copy roi region
		check_roi_region(p_Enc->m_CurrSrcBuffer->sROIQPRegion, p_Vid);
	}
	else {
        p_Vid->roi_qp = pFrame->apFrame.i8ROIQP;
	    check_roi_region(pFrame->apFrame.sROIQPRegion, p_Vid);
    }
#ifndef ENABLE_MB_RC
	if (p_Vid->mb_qp_weight < 4) {
        p_Vid->bitbudget_frame += p_Vid->bit_rate*p_Vid->num_unit_in_tick/p_Vid->time_unit;
		p_Vid->bitbudget_bu = p_Vid->bitbudget_frame/p_Vid->basicunit_num;
	}
#endif
	return H264E_OK;
}

int encoder_receive_irq(void *ptHandle)
{
	EncoderParams *p_Enc = (EncoderParams *)ptHandle;
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    uint32_t tmp;

#ifdef DISABLE_WRITE_OUT_REC
    if ((1 == h264e_dis_wo_buf && HW_INTERRUPT_NO_DMA) ||
        (0 == h264e_dis_wo_buf && HW_INTERRUPT)) {
        favce_info("receive irq %x", ptReg_cmd->STS2&0x1F);
        if (h264e_dis_wo_buf)
            p_Enc->u32CurrIRQ |= (ptReg_cmd->STS2&INTERRUPT_FLAG_NO_DMA);
        else
            p_Enc->u32CurrIRQ |= (ptReg_cmd->STS2&INTERRUPT_FLAG);
		ptReg_cmd->STS2 &= p_Enc->u32CurrIRQ;
		while((ptReg_cmd->STS2&p_Enc->u32CurrIRQ) != 0) {
    		ptReg_cmd->STS2 &= p_Enc->u32CurrIRQ;
        }
        // disable all interrupt mask
        ptReg_cmd->STS2 = (0x1F<<16);
        if (use_ioremap_wc)
            tmp = ptReg_cmd->STS2;
		return H264E_OK;
    }
#else
	if (HW_INTERRUPT) {
        favce_info("receive irq %x", ptReg_cmd->STS2&0x1F);
		p_Enc->u32CurrIRQ |= (ptReg_cmd->STS2&INTERRUPT_FLAG);
		ptReg_cmd->STS2 &= p_Enc->u32CurrIRQ;
		while((ptReg_cmd->STS2&p_Enc->u32CurrIRQ) != 0) {
    		ptReg_cmd->STS2 &= p_Enc->u32CurrIRQ;
        }
        // disable all interrupt mask
        ptReg_cmd->STS2 = (0x1F<<16);
        if (use_ioremap_wc)
            tmp = ptReg_cmd->STS2;
        //dump_cmd_register(p_Enc);
		return H264E_OK;
	}
#endif
	return H264E_ERR_IRQ;
}

#ifdef OVERFLOW_REENCODE
int encode_get_current_xy(void *ptHandle, int *mb_x, int *mb_y, int *encoded_field_done)
{
    EncoderParams *p_Enc = (EncoderParams *)ptHandle;
    VideoParams *p_Vid = p_Enc->p_Vid;
    *mb_y = ((p_Vid->record_sts0>>24) & 0xFF);
    *mb_x = ((p_Vid->record_sts0>>16) & 0xFF);
    if (p_Enc->p_Vid->p_Dpb->last_picture)
        *encoded_field_done = 1;
    else
        *encoded_field_done = 0;
    return 0;
}
#endif

#ifdef AES_ENCRYPT
static uint32_t find_start_code(uint8_t *bs_start, uint32_t offset, uint8_t type)
{
    uint8_t *start;
    int i;

    start = bs_start + offset;
    if (0xFF == type) {
        for (i = 0; i < 0x100; i++) {   // 64 byte * 4
            if ((0 == start[0] && 0 == start[1] && 0 == start[2] && 1 == start[3]) ||
                (0 == start[0] && 0 == start[1] && 1 == start[2])) {
                break;
            }
            start++;
            offset++;
        }
    }
    else {
        for (i = 0; i < 0x20; i++) {
            if ((0 == start[0] && 0 == start[1] && 0 == start[2] && 1 == start[3] && type == start[4]) ||
                (0 == start[0] && 0 == start[1] && 1 == start[2] && type == start[3])) {
                break;
            }
            start++;
            offset++;
        }
    }
    return offset;
}
#endif

//int encode_one_frame_sync(EncoderParams *p_Enc, VideoParams *p_Vid, FAVC_ENC_IOCTL_FRAME *pFrame, int slice_type)
//{
int encode_one_frame_sync(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame)
{
    EncoderParams *p_Enc = (EncoderParams *)ptHandle;
    VideoParams *p_Vid = p_Enc->p_Vid;
    int slice_type = p_Vid->dec_picture->slice_type;
#ifdef ENABLE_FAST_FORWARD
    int ref_type = p_Vid->dec_picture->used_for_ref;  // Tuba 20141013
#endif
    volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	FAVC_ENC_FRAME *apFrame = &pFrame->apFrame;
	int ret;

	ret = store_picture_in_dpb(p_Vid, &p_Vid->dec_picture);
	if (ret < 0)
		return ret;
	if (ret == FRAME_DONE) {
		if (slice_type == P_SLICE) {
			p_Vid->ave_frame_qp = p_Vid->sum_mb_qp/p_Vid->total_mb_number;
        }

        apFrame->frame_total_qp = p_Vid->sum_mb_qp;

		pFrame->u8ReleaseIdx = p_Vid->release_buffer_idx;

        p_Enc->u32CurrIRQ = 0;  // overflow interrupt may occur when close bitstream
        // CABAC field coding
    #ifdef ENABLE_FAST_FORWARD
        //apFrame->bRefFrame = ((p_Vid->dec_picture->used_for_ref||I_SLICE==slice_type)?1:0);
        apFrame->bRefFrame = (ref_type?1:0); // Tuba 20141013
        //printk("%c%d ", slice_type_char[slice_type], apFrame->bRefFrame);
    #endif
        if (p_Vid->active_pps->entropy_coding_mode_flag && p_Vid->field_pic_flag) {
            //p_Enc->u32ResBSLength = p_Enc->u32HeaderBSLength + (ptReg_cmd->STS1&0xFFFFFF) - p_Enc->u32OutputedBSLength + p_Enc->u32TopBSLength; 
            p_Enc->u32ResBSLength = p_Enc->u32HeaderBSLength + (ptReg_cmd->STS1&0xFFFFFF) + p_Enc->u32TopBSLength; 
        }
        else {
		    //p_Enc->u32ResBSLength = p_Enc->u32HeaderBSLength + (ptReg_cmd->STS1&0xFFFFFF) - p_Enc->u32OutputedBSLength;
		    p_Enc->u32ResBSLength = p_Enc->u32HeaderBSLength + (ptReg_cmd->STS1&0xFFFFFF);
        }
		apFrame->u32ResBSLength = p_Enc->u32ResBSLength;
    #if 0
		apFrame->u32Cost = p_Vid->total_cost;
    #else
        if (I_SLICE == slice_type && p_Vid->intra_cost_ratio)
            apFrame->u32Cost = p_Vid->total_cost * 32 / (32 + p_Vid->intra_cost_ratio);
        else
            apFrame->u32Cost = p_Vid->total_cost;
    #endif
		p_Vid->encoded_frame_num++;
    #ifdef DIVIDE_GOP_PARAM_SETTING
        update_gop_param(p_Vid);
    #endif
		//p_Enc->m_PrevSrcBuffer = p_Enc->m_CurrSrcBuffer;
        //pFrame->u8CurrBSBufIdx = p_Enc->u8OutBSBufIdx = p_Enc->u8CurrBSBufIdx;
		apFrame->u8FrameSliceType = slice_type;

        if (p_Vid->wk_enable) {
            p_Vid->wk_frame_number++;
            if (p_Vid->wk_reinit_period && (p_Vid->wk_frame_number%p_Vid->wk_reinit_period) == 0) {
                p_Vid->wk_pattern = p_Vid->wk_init_pattern;
                p_Vid->wk_frame_number = 0;
            }
            else 
        		p_Vid->wk_pattern = ptReg_parm->PARM18;
        }
    #ifdef AES_ENCRYPT
        /* step 3: update sei data by slice data
         *   1. find sei start point
         *   2. compute slice data start point & length
         *   3. set slice data by slice length
         *   4. update encrypt
         *   5. fill sei data   */
        if (I_SLICE == slice_type) {
            uint32_t sei_offset = p_Vid->slice_offset[0];
            uint32_t first_slice_offset;// = p_Vid->slice_offset[1];
            uint32_t first_slice_end_offset;
            uint8_t *bs_start = (uint8_t *)p_Enc->u32BSBuffer_virt;
            uint8_t *sei_start;
            uint8_t *slice_data;
            uint32_t slice_length;
            uint8_t sei_data[16];
            //uint8_t *sei_start = (uint8_t *)(p_Enc->u32BSBuffer_virt + p_Enc->u32SEIStart);
            EncCtx ctxEnc;
            int i;
        #if 1
            // find sei start point
            sei_offset = find_start_code(bs_start, sei_offset, 0x06);
            sei_start = bs_start + sei_offset;
            // find slice header
            //first_slice_offset = sei_offset + 50;   // may be not equal to 50 (because padding zero)
            first_slice_offset = find_start_code(bs_start, sei_offset + 50, 0xFF);
            slice_data = bs_start + first_slice_offset;

            if (1 == p_Vid->slice_number && 0 == p_Vid->field_pic_flag) {
                first_slice_end_offset = apFrame->u32ResBSLength;
            }
            else {
                first_slice_end_offset = p_Vid->slice_offset[1];
                first_slice_end_offset = find_start_code(bs_start, first_slice_end_offset, 0xFF);
            }
            slice_length = first_slice_end_offset - first_slice_offset;
        #else
            sei_start = (uint8_t *)(p_Enc->u32BSBuffer_virt + p_Enc->u32SEIStart);
            // find sei start point
            for (i = 0; i < 0x10; i++) {
                if (sei_start[0] == 0 && sei_start[1] == 0 && sei_start[2] == 0 && 
                    sei_start[3] == 1 && sei_start[4] == 6) 
                    break;
                sei_start++;
            }
            // compute slice data start point & length
            // 0 <- SPS/PPS -> slice_offset[0] <- slice data -> ... slice_offset[p_Vid->currSlice->slice_number]
            slice_data = (uint8_t *)(sei_start + 50);
            slice_length = apFrame->u32ResBSLength - ((uint32_t)slice_data - p_Enc->u32BSBuffer_virt);
        #endif
            // set slice data by slice length
            sei_data[0] = slice_length & 0xFF;
            if (sei_data[0] <= 5)
                sei_data[0] = 5;
            for (i = sei_data[0]; (i < sei_data[0] + 15) && (i < slice_length); i++) {
                sei_data[i - sei_data[0] + 1] = slice_data[i];
            }
            EncCtxIni(&ctxEnc, AES_KEY);
            Encrypt(&ctxEnc, sei_data, sei_data);

            // Tuba 20140307: add prevent 00 00 00/01/02/03 -> 00 00 05
            if (0 == sei_data[0] && sei_data[1] <= 3)
                sei_data[1] = 5;
            for (i = 2; i < 16; i++) {
                if (0 == sei_data[i-2] && 0 == sei_data[i-1] && sei_data[i] <= 3)
                    sei_data[i] = 5;
           }           
           memcpy(sei_start + 7 + sizeof(AES_KEY), sei_data, 16);
        }
    #endif
    #ifdef OUTPUT_SLICE_OFFSET
        apFrame->u32SliceNumber = p_Vid->cur_slice_idx;
        memcpy(apFrame->u32SliceOffset, p_Vid->slice_offset, sizeof(uint32_t)*p_Vid->cur_slice_idx);
    #endif
	}
#ifdef HANDLE_CABAC_ZERO_WORD
    else if (FIELD_DONE == ret && p_Vid->active_pps->entropy_coding_mode_flag) {    // CABAC
        set_bs_dword_align(p_Enc);
        //p_Enc->u32ResBSLength = ptReg_cmd->STS1&0xFFFFFF;
        clear_bs_length(p_Enc, ptReg_cmd);
        // when clean bitstream: handle bitstream overflow
        {
            volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
            int align_size = iCeil(p_Enc->u32TopBSLength + p_Enc->u32HeaderBSLength, 64);
            ptReg_mem->BS_BUF_SIZE = p_Enc->u32MaxBSLength - align_size;
        }
    }
#endif
	return ret;
}


int encode_one_frame_trigger(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame, unsigned char is_first_frame, int queue_frame)
{
	EncoderParams *p_Enc = (EncoderParams *)ptHandle;
	VideoParams *p_Vid = p_Enc->p_Vid;
	SliceHeader *currSlice = p_Vid->currSlice;
	StorablePicture *pic = p_Vid->dec_picture;
	//volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	int max_frame_num = (1<<(p_Vid->active_sps->log2_max_frame_num_minus4+4));

    favce_info("frame trigger (first frame %d, new frame %d, b frame %d)", is_first_frame, pFrame->new_frame, queue_frame);
	//if (p_Vid->new_frame) {
	if (pFrame->new_frame) {
        pFrame->new_frame = 0;
		if (init_meminfo(p_Enc, p_Vid, pFrame) < 0)
            return H264E_ERR_MEMORY;
		if (init_frame_param(p_Enc, p_Vid, pFrame, queue_frame) == FRAME_WAIT) {
            DEBUG_M(LOG_DEBUG, "store B frame in queue");
			return FRAME_WAIT;
		}
		set_bs_param(p_Enc, pFrame);
		//p_Vid->bitstream_length = 0;
		if ((p_Vid->resend_spspps == 0 && is_first_frame) ||
		    (p_Vid->resend_spspps == 1 && pic->slice_type == I_SLICE) ||
		    (p_Vid->resend_spspps == 2)) {
			p_Vid->write_spspps_flag = 1;
		}
		else
			p_Vid->write_spspps_flag = 0;
		if (pic->idr_flag)
			p_Vid->frame_num = 0;
    #ifndef DIVIDE_GOP_PARAM_SETTING
		else if (p_Vid->last_ref_idc)
			p_Vid->frame_num++;
    #endif
		pic->frame_num = (p_Vid->frame_num%max_frame_num);
		p_Vid->last_ref_idc = (pic->used_for_ref ? 1 : 0);

		if (p_Vid->field_pic_flag)
			pic->structure = TOP_FIELD;
		else
			pic->structure = FRAME;
		p_Vid->delta_mb_qp_sum = 0;
		p_Vid->delta_mb_qp_sum_clamp = 0;
		p_Vid->nonz_delta_qp_cnt = 0;

        p_Enc->u32PrevSrcBuffer_phy = pFrame->u32PrevSrcBuffer_phy;

        p_Vid->cur_slice_idx = 0;   // for slice length
		currSlice->slice_index = 0;
        currSlice->start_Y_pos = 0;
		currSlice->first_mb_in_slice = 0;
    #ifdef MCNR_ENABLE
        p_Vid->mcnr_sad = 0;
    #endif

    #ifndef RC_ENABLE
		p_Vid->currQP = p_Vid->fixed_qp[pic->slice_type];
	#endif
		p_Vid->rc_qpy = p_Vid->initial_qp = p_Vid->currQP;
		p_Vid->rc_init2 = 0;
		if (p_Vid->active_pps->entropy_coding_mode_flag) {
		    p_Vid->bitlength = 0xFFFFFFFC;
		    p_Vid->bincount = 0;
		}
        p_Enc->u32HeaderBSLength = 0;
        // strat of dump information
        favce_info("<<encode %c %s>> poc = %d, frame num = %d, ref = %d, idr = %d", 
            slice_type_char[pic->slice_type], frame_structure[pic->structure], 
            pic->poc, pic->frame_num, pic->used_for_ref, pic->idr_flag);
        // end of dump information
        init_lists(p_Vid, pic->slice_type);
        encoder_set_roi_xy(p_Vid, pFrame);
	}
	else {
		if (p_Vid->field_pic_flag) {
			int luma_frame_size = p_Vid->frame_width * p_Vid->frame_height;
			pic->structure = BOTTOM_FIELD;
			pic->poc++;
			pic->recpic_luma_virt += luma_frame_size/2;
			pic->recpic_luma_phy += luma_frame_size/2;
			pic->recpic_chroma_virt += luma_frame_size/4;
			pic->recpic_chroma_phy += luma_frame_size/4;
			pic->poc_lsb++;
			if (pic->idr_flag) {
				pic->used_for_ref = 2;
				pic->idr_flag = 0;
			}

            currSlice->slice_index = 0;
            currSlice->start_Y_pos = 0;
            currSlice->first_mb_in_slice = 0;
#ifdef MCNR_ENABLE
            p_Vid->mcnr_sad = 0;
#endif
    		if (p_Vid->active_pps->entropy_coding_mode_flag) {
    		    p_Vid->bitlength = 0xFFFFFFFC;
    		    p_Vid->bincount = 0;
    		}
            // strat of dump information
            favce_info("<<encode %c %s>> poc = %d, frame num = %d, ref = %d, idr = %d", 
                slice_type_char[pic->slice_type], frame_structure[pic->structure], 
                pic->poc, pic->frame_num, pic->used_for_ref, pic->idr_flag);
            // end of dump information
            init_lists(p_Vid, pic->slice_type);
		}
	#ifndef RC_ENABLE
		p_Vid->currQP = p_Vid->fixed_qp[pic->slice_type];
	#endif
		p_Vid->rc_qpy = p_Vid->currQP;
		p_Vid->rc_init2 = 0;
	}
	return H264E_OK;
}
#ifndef VG_INTERFACE
int encode_one_frame(void *ptHandle, FAVC_ENC_IOCTL_FRAME *pFrame, unsigned char is_first_frame, int queue_frame)
{
	EncoderParams *p_Enc = (EncoderParams *)ptHandle;
	//volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	VideoParams *p_Vid = p_Enc->p_Vid;
	StorablePicture *pic = p_Vid->dec_picture;
	SliceHeader *currSlice = p_Vid->currSlice;
	int ret;
    DEBUG_M(LOG_DEBUG, "================== encode frame %d (%x) ==================", p_Vid->encoded_frame_num, p_Enc->u32CurrIRQ);

    //p_Vid->new_frame = 1;
	while (1) {
		ret = encode_one_frame_trigger(p_Enc, pFrame, is_first_frame, queue_frame);
		if (ret == FRAME_WAIT) {
			return FRAME_WAIT;
		}
		//slice_type = pic->slice_type;
		//p_Vid->new_frame = 0;

		if ((ret = encode_one_plane(p_Enc, p_Vid, currSlice, pic)) < 0) {
		    return ret;
		}
		//if ((ret = encode_one_frame_sync(p_Enc, p_Vid, pFrame, pic->slice_type)) < 0)
		if ((ret = encode_one_frame_sync(p_Enc, pFrame)) < 0)
		    return ret;
		if (ret == FRAME_DONE) {
			return ret;
		}
	}
	return H264E_OK;
}
#endif

