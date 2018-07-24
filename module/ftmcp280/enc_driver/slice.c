#ifdef LINUX
	#include <linux/kernel.h>
	#include <linux/module.h>
	#include <linux/sched.h>
	#include <linux/delay.h>
	#include <linux/init.h>
	#include <linux/string.h>
    #include <asm/io.h>
    #include <linux/pci.h>
    #include <asm/cacheflush.h>
    #include <mach/fmem.h>
#else
	#include <string.h>
#endif
#include "define.h"
#include "slice_header.h"
#include "mcp280_reg.h"
#include "global.h"
#include "quant.h"
#include "vlc.h"
#include "../debug.h"
#ifdef FPGA_NON_OS
#include <stdio.h>
#endif
#ifdef VG_INTERFACE
	#include "log.h"
#endif
#ifdef AES_ENCRYPT
	#include "encrypt.h"
#endif

#ifdef LINUX
#define mFa526CleanInvalidateDCacheAll()
#else
// Clean & Invalidate all D-cache
#define mFa526CleanInvalidateDCacheAll()  \
__asm {                         \
        MCR p15, 0, 0, c7, c14, 0     \
}
#endif
#ifndef RTL_SIMULATION
//#define DUMP_REGISTER
#endif

#ifndef VG_INTERFACE
    int enc_index;
#else
    extern unsigned int h264e_yuv_swap;
    #ifdef DISABLE_WRITE_OUT_REC
    extern unsigned int h264e_dis_wo_buf;
    #endif
#endif
extern char slice_type_char[3];
#ifdef ENABLE_MB_RC
    extern unsigned int gMBQPLimit;
#endif
extern int force_dump_register;

#ifdef VG_INTERFACE
void dump_register(uint32_t addr, int size)
{
    uint32_t *ptr = (uint32_t *)addr;
    int i;
    /*
    for (i = 0; i < size; i = i + 0x20) {
        //DEBUG_M(LOG_ERROR, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", (addr + i), 
        //    *(ptr+i), *(ptr+i+0x4), *(ptr+i+0x8), *(ptr+i+0xC), *(ptr+i+0x10), *(ptr+i+0x14), *(ptr+i+0x18), *(ptr+i+0x1C));
        DEBUG_E(LOG_ERROR, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", (addr + i), 
            *(ptr+i), *(ptr+i+0x4), *(ptr+i+0x8), *(ptr+i+0xC), *(ptr+i+0x10), *(ptr+i+0x14), *(ptr+i+0x18), *(ptr+i+0x1C));
    }
    */
    for (i = 0; i < size/4; i += 8) {
        DEBUG_M(LOG_ERROR, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", (addr + 4*i), 
            *(ptr+i), *(ptr+i+1), *(ptr+i+2), *(ptr+i+3), *(ptr+i+4), *(ptr+i+5), *(ptr+i+6), *(ptr+i+7));
    }
}
#endif

void dump_parm_register(EncoderParams *p_Enc)
{
    if (p_Enc) {
    #ifdef VG_INTERFACE
        dump_register(p_Enc->u32BaseAddr, 0x7C);
    #endif
    #ifdef DUMP_REGISTER_NAME
        {
            uint32_t *pReg = (uint32_t *)p_Enc->u32BaseAddr;
            int i;

            printk("parameter register:\n");
            for (i = 0; i<=30; i++) {
                printk("PARM%02d: %08x(@%02xh)\n", i, *pReg, i*4);
                pReg++;
            }
        }
    #endif
    }
}

void dump_cmd_register(EncoderParams *p_Enc)
{
    if (p_Enc) {
    #ifdef VG_INTERFACE
        dump_register(p_Enc->u32BaseAddr + 0xC0, 0x40);
    #endif
    #ifdef DUMP_REGISTER_NAME
        {
            uint32_t *pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0x80);
            int i;

            printk("command & status register:\n");
            for (i = 0; i<=2; i++) {
                printk("CMD%d: %08x(@%02xh)\n", i, *pReg, i*4 + 0x80);
                pReg++;
            }
            
            pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0xC0);
            for (i = 0; i<=11; i++) {
                printk("STS%d: %08x(@%02xh)\n", i, *pReg, i*4 + 0xC0);
                pReg++;
            }
            pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0xF8);
            printk("DBG0: %08x(@%02xh)\n", *pReg, 0xF8);
            pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0xFC);
            printk("DBG1: %08x(@%02xh)\n", *pReg, 0xFC);
            pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0x1C0);
            printk("DBG2: %08x(@%02xh)\n", *pReg, 0x1C0);
            //pReg = (uint32_t *)(p_Enc->u32BaseAddr + 0x144);
            //printk("BS_BUF_SIZE: %08x(@%02xh)\n", *pReg, 0x144);
        }
    #endif
    }
}

void dump_mem_register(EncoderParams *p_Enc)
{
	if (p_Enc) {
    #ifdef VG_INTERFACE
    	dump_register(p_Enc->u32BaseAddr + MCP280_OFF_MEM, 0x84);
    #endif
    #ifdef DUMP_REGISTER_NAME
        {
            volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
            VideoParams *p_Vid = p_Enc->p_Vid;
            int ref_idx;

            printk("memory info register:\n");

            printk("cur top:        %08x (@100h)\n", ptReg_mem->DIDN_CUR_TOP);
            printk("cur btm:        %08x (@104h)\n", ptReg_mem->DIDN_CUR_BTM);
            printk("didn ref top:   %08x (@128h)\n", ptReg_mem->DIDN_REF_TOP);
            printk("didn ref btm:   %08x (@12ch)\n", ptReg_mem->DIDN_REF_BTM);
            printk("didn res top:   %08x (@130h)\n", ptReg_mem->DIDN_RESULT_TOP);
            printk("didn res btm:   %08x (@134h)\n", ptReg_mem->DIDN_RESULT_BTM);
            printk("rec luma:       %08x (@120h)\n", ptReg_mem->REC_LUMA);
            printk("rec chroma:     %08x (@124h)\n", ptReg_mem->REC_CHROMA);
            printk("ilf luma:       %08x (@14ch)\n", ptReg_mem->ILF_LUMA);
            printk("ilf chroma:     %08x (@150h)\n", ptReg_mem->ILF_CHROMA);

            if (p_Vid->dec_picture->slice_type != I_SLICE) {
                for (ref_idx = 0; ref_idx<p_Vid->list_size[0]; ref_idx++) {
                    printk("ref%d l0 luma:  %08x (@%xh)\n", ref_idx, ptReg_mem->ME_REF[ref_idx*2], 0x108+ref_idx*4);
                    printk("ref%d l0 chroma:%08x (@%xh)\n", ref_idx, ptReg_mem->ME_REF[ref_idx*2+1], 0x10c+ref_idx*4);
                }
            }
            if (p_Vid->dec_picture->slice_type == B_SLICE) {
                printk("ref0 l1 luma:   %08x (@110h)\n", ptReg_mem->ME_REF[2]);
                printk("ref0 l1 chroma: %08x (@114h)\n", ptReg_mem->ME_REF[3]);
            }

            // mv info & l1 collacted mv
            printk("sysinfo:        %08x (@138h)\n", ptReg_mem->SYSINFO);
            printk("mvinfo:         %08x (@13ch)\n", ptReg_mem->MOTION_INFO);
            printk("mvl1col:        %08x (@140h)\n", ptReg_mem->L1_COL_MV);
            printk("bs buf size:    %08x (@144h)\n", ptReg_mem->BS_BUF_SIZE);
            printk("bs addr:        %08x (@148h)\n", ptReg_mem->BS_ADDR);
            printk("swap:           %08x (@180h)\n", ptReg_mem->SWAP_DMA);
        }
    #endif
    }
}

void dump_vcache(EncoderParams *p_Enc)
{
    if (p_Enc) {
    #ifdef VG_INTERFACE
        dump_register(p_Enc->u32VcacheAddr, 0x4C);
    #endif
    #ifdef DUMP_REGISTER_NAME
        {
            volatile H264Reg_Vcache *ptReg_vcache = (H264Reg_Vcache *)(p_Enc->u32VcacheAddr);
            printk("vcache register:\n");
        
            printk("V_ENC:      %08x (@00h)\n", ptReg_vcache->V_ENC);
            printk("DECODE:     %08x (@04h)\n", ptReg_vcache->DECODE);
            printk("InSR:       %08x (@08h)\n", ptReg_vcache->I_SEARCH_RANGE);
            printk("OutSR:      %08x (@0Ch)\n", ptReg_vcache->O_SEARCH_RANGE);
            printk("CHN_CTL:    %08x (@10h)\n", ptReg_vcache->CHN_CTL);
            printk("ref0:       %08x (@20h)\n", ptReg_vcache->REF0_PARM);
            printk("ref1:       %08x (@24h)\n", ptReg_vcache->REF1_PARM);
            printk("ref2:       %08x (@28h)\n", ptReg_vcache->REF2_PARM);
            printk("ref3:       %08x (@2Ch)\n", ptReg_vcache->REF3_PARM);
            printk("LoadBase:   %08x (@30h)\n", ptReg_vcache->LOAD_BASE);
            printk("LoadBase2:  %08x (@34h)\n", ptReg_vcache->LOAD_BASE2);
            printk("disilf:     %08x (@38h)\n", ptReg_vcache->DIS_ILF_PARM);
            printk("LoadBase3:  %08x (@3Ch)\n", ptReg_vcache->LOAD_BASE3);
            printk("sysinfo:    %08x (@40h)\n", ptReg_vcache->SYS_INFO);
            printk("curp:       %08x (@48h)\n", ptReg_vcache->CUR_PARM);
        }
    #endif
    }
}

void dump_mcnr_table(void *ptEnc)
{
#ifdef LINUX
    EncoderParams *p_Enc = (EncoderParams *)ptEnc;
    volatile H264Reg_MCNR *ptReg_mcnr = (H264Reg_MCNR *)(p_Enc->u32BaseAddr + MCP280_OFF_MCNR);
    int i;
    for (i = 0; i < 13; i++)
        printk("mcnr low sad thd: %08x (@%03x)\n", ptReg_mcnr->MCNR_LSAD[i], MCP280_OFF_MCNR + i*4);
        //printk("mcnr low sad thd: %08x (@%03x)\n", ptReg_mcnr->MCNR_LSAD[i], (unsigned int)(&ptReg_mcnr->MCNR_LSAD[i]));
    for (i = 0; i < 13; i++)
        printk("mcnr high sad thd: %08x (@%03x)\n", ptReg_mcnr->MCNR_HSAD[i], MCP280_OFF_MCNR + 0x40 + i*4);
        //printk("mcnr high sad thd: %08x (@%03x)\n", ptReg_mcnr->MCNR_HSAD[i], (unsigned int)(&ptReg_mcnr->MCNR_HSAD[i]));
#endif
}


static int get_dsf(VideoParams *p_Vid, SliceHeader *currSlice, StorablePicture *pic)
{
	int dsf = 0;
	int td, tx, tb;
	if (pic->slice_type == B_SLICE && currSlice->direct_spatial_mv_pred_flag == 0) {
		tb = iClip3(-128, 127, pic->poc - p_Vid->ref_list[0][0]->poc);
		//td = iClip3(-128, 127, p_Vid->ref_list[1][0]->poc - pic->poc);
		td = iClip3(-128, 127, p_Vid->ref_list[1][0]->poc - p_Vid->ref_list[0][0]->poc);
		if (td != 0) {
			tx = (16384 + iAbs(td/2)) / td;
			dsf = iClip3(-1024, 1023, (tb*tx + 32)>>6);
		}
		else
			return 0;
	}

	return dsf;
}

static void set_meminfo(EncoderParams *p_Enc, VideoParams *p_Vid, SliceHeader *currSlice, int b_rec_out)
{
	volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
    //volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
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
    #ifndef GM8210
        uint32_t load_base;
    #endif
#endif
	StorablePicture *pic = p_Vid->dec_picture;
	int ref_idx;
#ifndef ADDRESS_UINT32
	uint8_t *DnT_ref0_base, *DnB_ref0_base, *DnT_ref1_base, *DnB_ref1_base;
#else
    uint32_t DnT_ref0_base, DnB_ref0_base, DnT_ref1_base, DnB_ref1_base;
#endif
	uint32_t source_frame_size = p_Vid->source_width*p_Vid->source_height*2;
	//uint32_t one_422_frame_size = p_Vid->frame_width*p_Vid->frame_height*2;
	uint32_t one_frame_Y_size = p_Vid->frame_width*p_Vid->frame_height;
	uint32_t roi_skip_size = 0;
	uint32_t curr_src_top, curr_src_bot;

	/* roi: if src_img_type = 0, and bypass or itl mode, and frame coding, encoder
	 *      does not encode roi like Fig2. the encoded roi is the same as Fig1, it 
	 *      treat the src_img_type as 1    */
	if (p_Vid->roi_flag) {
		//if (!p_Vid->src_img_type) {
		if (!p_Vid->src_img_type && p_Vid->int_top_btm) {
			//if (p_Vid->src_mode != ELSE_MODE && p_Vid->field_pic_flag == 0)	// ByPass & ITL mode
			//	roi_skip_size = p_Vid->roi_y*p_Vid->source_width + p_Vid->roi_x*2;
			//else
				roi_skip_size = p_Vid->roi_y*p_Vid->source_width + p_Vid->roi_x*2;	// half height
		}
		else
			roi_skip_size = p_Vid->roi_y*p_Vid->source_width*2 + p_Vid->roi_x*2;
	}
	/*                           Fig1                      Fig2
	 *	                 +-------------------+    +-------------------+
	 *	                 |                   |    |    +---------+    |
	 *	                 |    +---------+    |    |    | ROI(top)|    |
	 *	src_img_type=0   |    |         |    |    |    +---------+    |
	 *	  +-------+      |    |   ROI   |    |    +-------------------+
	 *	  |  top  |      |    |         |    |    |    +---------+    |
	 *	  +-------+      |    +---------+    |    |    | ROI(bot)|    |
	 *	  |  bot  |      |                   |    |    +---------+    |
	 *	  +-------+      +-------------------+    +-------------------+
	 *	frame coding: start_Y_pos -> top field start_Y_pos/2 */
	// source image type
	if (p_Vid->src_img_type == 0) {
	#ifndef ADDRESS_UINT32
		if (p_Vid->src_mode == BYPASS_MODE || p_Vid->src_mode == ITL_MODE) {
		    if (p_Vid->int_top_btm && p_Vid->field_pic_flag == 0)
		        curr_src_top = (uint32_t)(pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width + roi_skip_size);
		    else
		        curr_src_top = (uint32_t)(pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width*2 + roi_skip_size);
		}
		else	// not (bypass or itl mode), it must be frame coding
			curr_src_top = (uint32_t)(pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width + roi_skip_size);
	#else
        if (p_Vid->src_mode == BYPASS_MODE || p_Vid->src_mode == ITL_MODE) {
		    if (p_Vid->int_top_btm && p_Vid->field_pic_flag == 0)
		        curr_src_top = (pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width + roi_skip_size);
		    else
		        curr_src_top = (pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width*2 + roi_skip_size);
		}
		else	// not (bypass or itl mode), it must be frame coding
			curr_src_top = (pic->srcpic_addr_phy + currSlice->start_Y_pos*16*p_Vid->source_width + roi_skip_size);
	#endif
		if (pic->structure == BOTTOM_FIELD)
			curr_src_top += source_frame_size/2;

        if (p_Vid->src_mode != ELSE_MODE && p_Vid->int_top_btm && p_Vid->field_pic_flag == 0) {
            if (currSlice->start_Y_pos == 0 || p_Vid->src_mode == BYPASS_MODE)
                curr_src_bot = curr_src_top + source_frame_size/2;
            else
                curr_src_bot = curr_src_top + source_frame_size/2 - p_Vid->source_width*2;
        }
		else if (p_Vid->src_mode == BYPASS_MODE)
			curr_src_bot = curr_src_top + p_Vid->source_width*2*8;
		else if (p_Vid->src_mode == ITL_MODE)
			curr_src_bot = curr_src_top + p_Vid->source_width*2*7;
		else	// it mut be frame coding
			curr_src_bot = curr_src_top + source_frame_size/2;
	}
	else {  // src_img_type = 1
	#ifndef ADDRESS_UINT32
		if (p_Vid->field_pic_flag)
			curr_src_top = (uint32_t)(pic->srcpic_addr_phy + (pic->structure==BOTTOM_FIELD? 1:0)*p_Vid->source_width*2 + currSlice->start_Y_pos*16*p_Vid->source_width*4 + roi_skip_size);
		else
			curr_src_top = (uint32_t)(pic->srcpic_addr_phy + (pic->structure==BOTTOM_FIELD? source_frame_size/2:0) + currSlice->start_Y_pos*16*p_Vid->source_width*2 + roi_skip_size);
    #else
        if (p_Vid->field_pic_flag)
            curr_src_top = (pic->srcpic_addr_phy + (pic->structure==BOTTOM_FIELD? 1:0)*p_Vid->source_width*2 + currSlice->start_Y_pos*16*p_Vid->source_width*4 + roi_skip_size);
        else
            curr_src_top = (pic->srcpic_addr_phy + (pic->structure==BOTTOM_FIELD? source_frame_size/2:0) + currSlice->start_Y_pos*16*p_Vid->source_width*2 + roi_skip_size);
    #endif

		if (!p_Vid->field_pic_flag) {
			if (p_Vid->src_mode == BYPASS_MODE)
				curr_src_bot = curr_src_top + p_Vid->source_width*2*8;
			else if (p_Vid->src_mode == ITL_MODE)
				curr_src_bot = curr_src_top + p_Vid->source_width*2*7;
			else
				curr_src_bot = curr_src_top + p_Vid->source_width*2;
		}
		else {
			if (p_Vid->src_mode == BYPASS_MODE)
				curr_src_bot = curr_src_top + p_Vid->source_width*2*8*2;
			else if (p_Vid->src_mode == ITL_MODE)
				curr_src_bot = curr_src_top + p_Vid->source_width*2*7*2;
			else
				curr_src_bot = curr_src_top + p_Vid->source_width*2;
		}
	}
	ptReg_mem->DIDN_CUR_TOP = curr_src_top;
	ptReg_mem->DIDN_CUR_BTM = curr_src_bot;
	/*	if temporal de-noise enable, temporal de-interlace use de-noise result and write out de-noise result
	 *	if temporal de-noise disable, de-interlace use source result and not write out result (save bandwidth) */
	if (p_Vid->didn_mode & TEMPORAL_DENOISE) {	// temporal denoise enable
		if (pic->structure != BOTTOM_FIELD) {
		#ifndef ADDRESS_UINT32
			DnT_ref0_base = p_Enc->pu8DiDnRef0_phy;
			DnT_ref1_base = p_Enc->pu8DiDnRef1_phy;
	    #else
		    DnT_ref0_base = p_Enc->u32DiDnRef0_phy;
			DnT_ref1_base = p_Enc->u32DiDnRef1_phy;
		#endif
			if (p_Vid->dn_result_format == 0 || p_Vid->dn_result_format == 1) {	// top/bottom separate			
				DnB_ref0_base = DnT_ref0_base + one_frame_Y_size;
				DnB_ref1_base = DnT_ref1_base + one_frame_Y_size;
			}
			else {
				DnB_ref0_base = DnT_ref0_base + p_Vid->source_width*2;
				DnB_ref1_base = DnT_ref1_base + p_Vid->source_width*2;
			}
		}
		else {
			if (p_Vid->dn_result_format == 0 || p_Vid->dn_result_format == 1) {	// top/bottom separate
			#ifndef ADDRESS_UINT32
				DnT_ref0_base = p_Enc->pu8DiDnRef0_phy + (one_frame_Y_size/2);
				DnT_ref1_base = p_Enc->pu8DiDnRef1_phy + (one_frame_Y_size/2);
			#else
				DnT_ref0_base = p_Enc->u32DiDnRef0_phy + (one_frame_Y_size/2);
				DnT_ref1_base = p_Enc->u32DiDnRef1_phy + (one_frame_Y_size/2);
			#endif
				DnB_ref0_base = DnT_ref0_base + one_frame_Y_size;// + (one_frame_Y_size/2);
				DnB_ref1_base = DnT_ref1_base + one_frame_Y_size;// + (one_frame_Y_size/2);
			}
			else {
			#ifndef ADDRESS_UINT32
				DnT_ref0_base = p_Enc->pu8DiDnRef0_phy + one_frame_Y_size;
				DnT_ref1_base = p_Enc->pu8DiDnRef1_phy + one_frame_Y_size;
			#else
				DnT_ref0_base = p_Enc->u32DiDnRef0_phy + one_frame_Y_size;
				DnT_ref1_base = p_Enc->u32DiDnRef1_phy + one_frame_Y_size;
			#endif
				DnB_ref0_base = DnT_ref0_base + p_Vid->source_width*2 + (one_frame_Y_size/2);
				DnB_ref1_base = DnT_ref1_base + p_Vid->source_width*2 + (one_frame_Y_size/2);
			}
		}
	#ifndef ADDRESS_UINT32
		if (p_Vid->dn_result_format == 0 || p_Vid->dn_result_format == 1) {
			ptReg_mem->DIDN_REF_TOP = (uint32_t)(DnT_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*16);	// YUV planar
			ptReg_mem->DIDN_REF_BTM = (uint32_t)(DnB_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
			ptReg_mem->DIDN_RESULT_TOP = (uint32_t)(DnT_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
			ptReg_mem->DIDN_RESULT_BTM = (uint32_t)(DnB_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
		}
		else {
			ptReg_mem->DIDN_REF_TOP = (uint32_t)(DnT_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*32);	// YUV planar
			ptReg_mem->DIDN_REF_BTM = (uint32_t)(DnB_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
			ptReg_mem->DIDN_RESULT_TOP = (uint32_t)(DnT_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
			ptReg_mem->DIDN_RESULT_BTM = (uint32_t)(DnB_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
		}
	#else
		if (p_Vid->dn_result_format == 0 || p_Vid->dn_result_format == 1) {
			ptReg_mem->DIDN_REF_TOP = (DnT_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*16);	// YUV planar
			ptReg_mem->DIDN_REF_BTM = (DnB_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
			ptReg_mem->DIDN_RESULT_TOP = (DnT_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
			ptReg_mem->DIDN_RESULT_BTM = (DnB_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*16);
		}
		else {
			ptReg_mem->DIDN_REF_TOP = (DnT_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*32);	// YUV planar
			ptReg_mem->DIDN_REF_BTM = (DnB_ref0_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
			ptReg_mem->DIDN_RESULT_TOP = (DnT_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
			ptReg_mem->DIDN_RESULT_BTM = (DnB_ref1_base + currSlice->start_Y_pos*p_Vid->frame_width*32);
		}
	#endif
	}
	else if (p_Vid->didn_mode & TEMPORAL_DEINTERLACE) {	// temporal denoise disable and temporal deinterlace enable
		if (p_Vid->encoded_frame_num != 0) {
		#ifndef ADDRESS_UINT32
			if (p_Vid->src_img_type) {
				ptReg_mem->DIDN_REF_TOP = (uint32_t)(p_Enc->u32PrevSrcBuffer_phy + currSlice->start_Y_pos*p_Vid->source_width*32 + roi_skip_size);
				ptReg_mem->DIDN_REF_BTM = (uint32_t)(p_Enc->u32PrevSrcBuffer_phy + p_Vid->source_width*2 + currSlice->start_Y_pos*p_Vid->source_width*32 + roi_skip_size);
			}
			else {
				ptReg_mem->DIDN_REF_TOP = (uint32_t)(p_Enc->u32PrevSrcBuffer_phy + currSlice->start_Y_pos*p_Vid->source_width*16 + roi_skip_size);	// YUV planar
				ptReg_mem->DIDN_REF_BTM = (uint32_t)(p_Enc->u32PrevSrcBuffer_phy + source_frame_size/2 + currSlice->start_Y_pos*p_Vid->source_width*16 + roi_skip_size);
			}
		#else
			if (p_Vid->src_img_type) {
				ptReg_mem->DIDN_REF_TOP = (p_Enc->u32PrevSrcBuffer_phy + currSlice->start_Y_pos*p_Vid->source_width*32 + roi_skip_size);
				ptReg_mem->DIDN_REF_BTM = (p_Enc->u32PrevSrcBuffer_phy + p_Vid->source_width*2 + currSlice->start_Y_pos*p_Vid->source_width*32 + roi_skip_size);
			}
			else {
				ptReg_mem->DIDN_REF_TOP = (p_Enc->u32PrevSrcBuffer_phy + currSlice->start_Y_pos*p_Vid->source_width*16 + roi_skip_size);	// YUV planar
				ptReg_mem->DIDN_REF_BTM = (p_Enc->u32PrevSrcBuffer_phy + source_frame_size/2 + currSlice->start_Y_pos*p_Vid->source_width*16 + roi_skip_size);
			}
		#endif
		}
	}
#ifdef DISABLE_WRITE_OUT_REC
    if (h264e_dis_wo_buf && 0 == pic->used_for_ref) {
        ptReg_mem->REC_LUMA = UNKNOWN_ADDRESS;
        ptReg_mem->REC_CHROMA = UNKNOWN_ADDRESS;
    }
    else 
#endif
    {
    	ptReg_mem->REC_LUMA = pic->recpic_luma_phy + currSlice->start_Y_pos*p_Vid->frame_width*16;
    	ptReg_mem->REC_CHROMA = pic->recpic_chroma_phy + currSlice->start_Y_pos*p_Vid->frame_width*8;
    }
	// deblock 
	if (b_rec_out || pic->slice_type!=B_SLICE) {
	#ifndef ADDRESS_UINT32
		if (currSlice->start_Y_pos == 0) {
			ptReg_mem->ILF_LUMA = (uint32_t)pic->recpic_luma_phy;
			ptReg_mem->ILF_CHROMA = (uint32_t)pic->recpic_chroma_phy;
		}
		else {
			ptReg_mem->ILF_LUMA = (uint32_t)(pic->recpic_luma_phy + p_Vid->frame_width*(currSlice->start_Y_pos-1)*16);
			ptReg_mem->ILF_CHROMA = (uint32_t)(pic->recpic_chroma_phy + p_Vid->frame_width*(currSlice->start_Y_pos-1)*8);	// 8 lines
		}
	#else
		if (currSlice->start_Y_pos == 0) {
			ptReg_mem->ILF_LUMA = pic->recpic_luma_phy;
			ptReg_mem->ILF_CHROMA = pic->recpic_chroma_phy;
		}
		else {
			ptReg_mem->ILF_LUMA = (pic->recpic_luma_phy + p_Vid->frame_width*(currSlice->start_Y_pos-1)*16);
			ptReg_mem->ILF_CHROMA = (pic->recpic_chroma_phy + p_Vid->frame_width*(currSlice->start_Y_pos-1)*8);	// 8 lines
		}
	#endif
    #if VCACHE_ILF_ENABLE	// enable vcache ilf
        //if (p_Vid->frame_width >= 208 && p_Vid->frame_width <= 1920) {
        if (p_Vid->frame_width >= 208 && p_Vid->frame_width <= VCACHE_MAX_REF0_WIDTH) {
            if (p_Vid->active_sps->chroma_format_idc != FORMAT400)
                ptReg_vcache->DIS_ILF_PARM = 9;
            else
                ptReg_vcache->DIS_ILF_PARM = 1;
        #ifdef DUMP_ENCODER_STATUS
            printk("i");
        #endif
        }
        else {
            ptReg_vcache->DIS_ILF_PARM = 0;
        #ifdef DUMP_ENCODER_STATUS
            printk("d");
        #endif
        }
    #endif
	}
	#if VCACHE_ILF_ENABLE
	else {
        ptReg_vcache->DIS_ILF_PARM = 0;
    #ifdef DUMP_ENCODER_STATUS
        printk("d");
    #endif
    }
    #endif

	// reference frame
#ifndef ADDRESS_UINT32
	if (pic->slice_type != I_SLICE) {
		for (ref_idx = 0; ref_idx<p_Vid->list_size[0]; ref_idx++) {
			ptReg_mem->ME_REF[ref_idx*2] = (uint32_t)(p_Vid->ref_list[0][ref_idx]->recpic_luma_phy);
			ptReg_mem->ME_REF[ref_idx*2+1] = (uint32_t)(p_Vid->ref_list[0][ref_idx]->recpic_chroma_phy);
		}
	}
	if (pic->slice_type == B_SLICE) {
		ptReg_mem->ME_REF[2] = (uint32_t)(p_Vid->ref_list[1][0]->recpic_luma_phy);
		ptReg_mem->ME_REF[3] = (uint32_t)(p_Vid->ref_list[1][0]->recpic_chroma_phy);
	}

	// mv info & l1 collacted mv
	ptReg_mem->SYSINFO = (uint32_t)p_Enc->pu8SysInfoAddr_phy;
	if (pic->structure != BOTTOM_FIELD) {
		ptReg_mem->MOTION_INFO = (uint32_t)(p_Enc->pu8MVInfoAddr_phy[0] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<3));
		ptReg_mem->L1_COL_MV = (uint32_t)(p_Enc->pu8L1ColMVInfoAddr_phy[0] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<4));
	}
	else {
		ptReg_mem->MOTION_INFO = (uint32_t)(p_Enc->pu8MVInfoAddr_phy[1] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<3));
		ptReg_mem->L1_COL_MV = (uint32_t)(p_Enc->pu8L1ColMVInfoAddr_phy[1] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<4));
	}
#else
	if (pic->slice_type != I_SLICE) {
		for (ref_idx = 0; ref_idx<p_Vid->list_size[0]; ref_idx++) {
			ptReg_mem->ME_REF[ref_idx*2] = (p_Vid->ref_list[0][ref_idx]->recpic_luma_phy);
			ptReg_mem->ME_REF[ref_idx*2+1] = (p_Vid->ref_list[0][ref_idx]->recpic_chroma_phy);
		}
	}
	if (pic->slice_type == B_SLICE) {
		ptReg_mem->ME_REF[2] = (p_Vid->ref_list[1][0]->recpic_luma_phy);
		ptReg_mem->ME_REF[3] = (p_Vid->ref_list[1][0]->recpic_chroma_phy);
	}

	// mv info & l1 collacted mv
	ptReg_mem->SYSINFO = p_Enc->u32SysInfoAddr_phy;
	if (pic->structure != BOTTOM_FIELD) {
		ptReg_mem->MOTION_INFO = (p_Enc->u32MVInfoAddr_phy[0] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<3));
		ptReg_mem->L1_COL_MV = (p_Enc->u32L1ColMVInfoAddr_phy[0] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<4));
	}
	else {
		ptReg_mem->MOTION_INFO = (p_Enc->u32MVInfoAddr_phy[1] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<3));
		ptReg_mem->L1_COL_MV = (p_Enc->u32L1ColMVInfoAddr_phy[1] + ((p_Vid->mb_row*currSlice->start_Y_pos)<<4));
	}
#endif	
#if MCP280_VCACHE_ENABLE
    // vcache limitation: min 208x96, max 1920(width)
    //if (pic->slice_type != I_SLICE && p_Vid->frame_width >= 208 && p_Vid->frame_height >= 96 
    //    && p_Vid->frame_width <= 1920 && p_Vid->list_size[0] <= 2) {
    if (pic->slice_type != I_SLICE && p_Vid->frame_width >= 208 && p_Vid->frame_height >= 96 
        && p_Vid->frame_width <= VCACHE_MAX_REF0_WIDTH && p_Vid->list_size[0] <= 2) {
        // ref0 luma        [7:4]: rid, [3:2]:2^mb_width_in_DW, [1]:dont_care, [0]:en
        p_Vid->vcache_enable = 1;
	#ifndef ADDRESS_UINT32
        ptReg_vcache->REF0_PARM = (((uint32_t)p_Vid->ref_list[0][0]->recpic_luma_phy)&0xFFFFFF00) | 0x35;
	#else
        ptReg_vcache->REF0_PARM = ((p_Vid->ref_list[0][0]->recpic_luma_phy)&0xFFFFFF00) | 0x35;
	#endif
        // ref0 chroma
    #if SET_VCACHE_MONO_REF
        if (p_Vid->active_sps->chroma_format_idc != FORMAT400) {
		#ifndef ADDRESS_UINT32
		    ptReg_vcache->REF1_PARM = (((uint32_t)p_Vid->ref_list[0][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x41;
		#else
		    ptReg_vcache->REF1_PARM = ((p_Vid->ref_list[0][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x41;
		#endif
        }
		else
		    ptReg_vcache->REF1_PARM = 0;
		    //ptReg_vcache->REF1_PARM &= 0xFFFFFFFE;
    #else
		#ifndef ADDRESS_UINT32
        ptReg_vcache->REF1_PARM = (((uint32_t)p_Vid->ref_list[0][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x41;
		#else
        ptReg_vcache->REF1_PARM = ((p_Vid->ref_list[0][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x41;
		#endif
    #endif
    #ifdef DUMP_ENCODER_STATUS
        printk("V1");
    #endif
/*
    #ifdef GM8287
        if (p_Vid->frame_width <= VCACHE_MAX_REF_WIDTH)
    #endif
*/
       // check second reference frame (vcahce)
        if (p_Vid->frame_width <= VCACHE_MAX_REF1_WIDTH) {
            if (B_SLICE == pic->slice_type) {
                // ref1 luma
			#ifndef ADDRESS_UINT32
                ptReg_vcache->REF2_PARM = (((uint32_t)p_Vid->ref_list[1][0]->recpic_luma_phy)&0xFFFFFF00) | 0x15;
			#else
                ptReg_vcache->REF2_PARM = ((p_Vid->ref_list[1][0]->recpic_luma_phy)&0xFFFFFF00) | 0x15;
			#endif
                // ref1 chroma
            #if SET_VCACHE_MONO_REF
                if (p_Vid->active_sps->chroma_format_idc != FORMAT400) {
				#ifndef ADDRESS_UINT32
                    ptReg_vcache->REF3_PARM = (((uint32_t)p_Vid->ref_list[1][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#else
                    ptReg_vcache->REF3_PARM = ((p_Vid->ref_list[1][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#endif
                }
                else
                    ptReg_vcache->REF3_PARM = 0;
                    //ptReg_vcache->REF3_PARM &= 0xFFFFFFFE;
            #else
				#ifndef ADDRESS_UINT32
                	ptReg_vcache->REF3_PARM = (((uint32_t)p_Vid->ref_list[1][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#else
                	ptReg_vcache->REF3_PARM = ((p_Vid->ref_list[1][0]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#endif
            #endif
                p_Vid->vcache_ref2_enable = 1;  // Tuba 20120211: support vcache enable 1 ref
            #ifdef DUMP_ENCODER_STATUS
                printk("V2");
            #endif
    		}
    		else if (p_Vid->list_size[0] == 2) {
                // ref1 luma
			#ifndef ADDRESS_UINT32
                ptReg_vcache->REF2_PARM = (((uint32_t)p_Vid->ref_list[0][1]->recpic_luma_phy)&0xFFFFFF00) | 0x15;
			#else
                ptReg_vcache->REF2_PARM = ((p_Vid->ref_list[0][1]->recpic_luma_phy)&0xFFFFFF00) | 0x15;
			#endif
                // ref1 chroma
            #if SET_VCACHE_MONO_REF
                if (p_Vid->active_sps->chroma_format_idc != FORMAT400) {
				#ifndef ADDRESS_UINT32
                    ptReg_vcache->REF3_PARM = (((uint32_t)p_Vid->ref_list[0][1]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#else
                    ptReg_vcache->REF3_PARM = ((p_Vid->ref_list[0][1]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#endif
                }
                else
                    ptReg_vcache->REF3_PARM = 0;
                    //ptReg_vcache->REF3_PARM &= 0xFFFFFFFE;
            #else
				#ifndef ADDRESS_UINT32
                ptReg_vcache->REF3_PARM = (((uint32_t)p_Vid->ref_list[0][1]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#else
                ptReg_vcache->REF3_PARM = ((p_Vid->ref_list[0][1]->recpic_chroma_phy)&0xFFFFFF00) | 0x91;
				#endif
            #endif
                p_Vid->vcache_ref2_enable = 1;  // Tuba 20120211: support vcache enable 1 ref
            #ifdef DUMP_ENCODER_STATUS
                printk("V2");
            #endif
    		}
    		else {
    		    //ptReg_vcache->REF2_PARM &= 0xFFFFFFFE;
    		    //ptReg_vcache->REF3_PARM &= 0xFFFFFFFE;
    		    ptReg_vcache->REF2_PARM = 0;
    		    ptReg_vcache->REF3_PARM = 0;
                p_Vid->vcache_ref2_enable = 0;  // Tuba 20120211: support vcache enable 1 ref
    		}
        }
    //#ifdef GM8287
        else {  // VCACHE_MAX_REF1_WIDTH
            ptReg_vcache->REF2_PARM = 0;
            ptReg_vcache->REF3_PARM = 0;
            p_Vid->vcache_ref2_enable = 0;  // Tuba 20120211: support vcache enable 1 ref
        }
    //#endif

#if 1	// Tuba 20140211: vcache load base constraint
    #ifdef GM8210
        ptReg_vcache->LOAD_BASE = 0x4B00;   // max width: 1920, load base2 & load base 3 not exist
    #else
        if (p_Vid->frame_width > VCACHE_MAX_REF1_WIDTH) { // frame width 960
        #ifdef GM8139
            ptReg_vcache->LOAD_BASE = 0xA000;
        #else   // GM8287/GM8136
            ptReg_vcache->LOAD_BASE = 0x4B00;
        #endif
        }
        else {
            load_base = p_Vid->frame_width * 10;
            ptReg_vcache->LOAD_BASE = load_base;
            ptReg_vcache->LOAD_BASE2 = load_base * 3 / 2;   // 0x30 + frame_width * 40 / 8
            ptReg_vcache->LOAD_BASE3 = load_base * 5 / 2;   // 0x34 + frame_width * 80 / 8
        #ifdef DUMP_ENCODER_STATUS
            printk("L");
        #endif
        }
    #endif
#else
		//ptReg_vcache->LOAD_BASE = 0x00025800;   // 1920*80
    #ifdef GM8287
        //tmp = p_Vid->frame_width * 10; // frame width * 80 (search range) / 8 (dword)
        if (p_Vid->frame_width > VCACHE_MAX_REF1_WIDTH) { // frame width 960
            ptReg_vcache->LOAD_BASE = 0x4B00;
        }
        else {
            load_base = p_Vid->frame_width * 10;
            ptReg_vcache->LOAD_BASE = load_base;
            ptReg_vcache->LOAD_BASE2 = load_base * 3 / 2;   // 0x30 + frame_width * 40 / 8
            ptReg_vcache->LOAD_BASE3 = load_base * 5 / 2;   // 0x34 + frame_width * 80 / 8
        #ifdef DUMP_ENCODER_STATUS
            printk("L");
        #endif
        }
    #elif defined(GM8139)
        ptReg_vcache->LOAD_BASE = 0xA000;
    #else
		ptReg_vcache->LOAD_BASE = 0x4B00;
    #endif
#endif
    }
    else {
        p_Vid->vcache_enable = 0;
        /*
        ptReg_vcache->REF0_PARM &= 0xFFFFFFFE;
		ptReg_vcache->REF1_PARM &= 0xFFFFFFFE;
        ptReg_vcache->REF2_PARM &= 0xFFFFFFFE;
        ptReg_vcache->REF3_PARM &= 0xFFFFFFFE;
        */
        ptReg_vcache->REF0_PARM = 0;
		ptReg_vcache->REF1_PARM = 0;
        ptReg_vcache->REF2_PARM = 0;
        ptReg_vcache->REF3_PARM = 0;
    }
#endif
    //dump_all_register(p_Enc, DUMP_MEM);
}

#ifdef MCNR_ENABLE
//static const int width_threshold[9] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static const int width_threshold[10] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
static int log2_width(int width)
{
    int i;
    for (i = 9; i>0; i--)
        if (width >= width_threshold[i])
            return i;
    return 0;
}
#endif

static void set_roi_qp_register(EncoderParams *p_Enc, VideoParams *p_Vid)
{
    volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
    unsigned int tmp;
    int i;
    FAVC_ROI_QP_REGION *pRegion;

    if (p_Vid->roi_qp_type == DISBALE_ROI_QP) {
    #ifdef MCNR_ENABLE
        // Tuba 20121015 mcnr start
        if (I_SLICE == p_Vid->dec_picture->slice_type)
            ptReg_parm->PARM22 = 0;
        else
            ptReg_parm->PARM22 = SHIFT_1(p_Vid->mcnr_en, 31) | ((p_Vid->mcnr_mv_thd&MASK10)<<21) |
                ((p_Vid->mcnr_sad_shift&MASK2)<<19);

    #ifdef DUMP_ENCODER_STATUS
        if (p_Vid->mcnr_en)
            printk("M");
    #endif        
        // Tuba 20121015 end
    #else
        ptReg_parm->PARM22 = 0;
    #endif
        return;
    }
#if defined(MCNR_ENABLE)&defined(DUMP_ENCODER_STATUS)
    if (p_Vid->mcnr_en)
        printk("M");
#endif

    // parm 22
    tmp = 0;
    for (i = 0; i<8; i++) {
        if (p_Vid->roi_qp_region[i].enable)
            tmp |= (1<<(i+8));
    }
    if (p_Vid->roi_qp_type == ROI_FIXED_QP)
        tmp |= BIT7;

    if (ROI_DELTA_QP == p_Vid->roi_qp_type) {
    #if 0
        tmp |= iClip3(-p_Vid->currQP, 51-p_Vid->currQP, p_Vid->roi_qp)&0x7F;
    #else
        if (p_Vid->roi_qp + p_Vid->currQP > 51) {
            tmp |= (51 - p_Vid->currQP)&0x7F;
        }
        /*
        else if (p_Vid->roi_qp + p_Vid->currQP < 0) {
            tmp |= (-p_Vid->currQP)&0x7F;
        }
        */
        else
            tmp |= (p_Vid->roi_qp & 0x7F);
    #endif
    }
    else {
        tmp |= (p_Vid->roi_qp & 0x7F);
    }
    // Tuba 20121015 mcnr satrt
#ifdef MCNR_ENABLE
    if (I_SLICE == p_Vid->dec_picture->slice_type)
        ptReg_parm->PARM22 = tmp;
    else {
        ptReg_parm->PARM22 = SHIFT_1(p_Vid->mcnr_en, 31) | ((p_Vid->mcnr_mv_thd&MASK10)<<21) |
            ((p_Vid->mcnr_sad_shift&MASK2)<<19) | tmp;
    }
    // Tuba 20121015 end
#else
    ptReg_parm->PARM22 = tmp;
#endif
    pRegion = &p_Vid->roi_qp_region[0];
    ptReg_parm->PARM23 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[1];
    ptReg_parm->PARM24 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[2];
    ptReg_parm->PARM25 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[3];
    ptReg_parm->PARM26 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[4];
    ptReg_parm->PARM27 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[5];
    ptReg_parm->PARM28 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[6];
    ptReg_parm->PARM29 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
    pRegion = &p_Vid->roi_qp_region[7];
    ptReg_parm->PARM30 = ((pRegion->roi_height-1)<<24) | ((pRegion->roi_width-1)<<16) | (pRegion->roi_y<<8) | pRegion->roi_x;
}

int get_max_hzsr(int slice_type, VideoParams *p_Vid)
{
    int i, max_idx = 0;
    int max_sr = 0;
    if (slice_type == P_SLICE && p_Vid->ref_l0_num != p_Vid->list_size[0]) {
        for (i = 0; i<p_Vid->list_size[0]; i++) {
            if (max_sr < p_Vid->hz_sr[P_SLICE][i]) {
                max_sr = p_Vid->hz_sr[P_SLICE][i];
                max_idx = i;
            }
        }
        return max_idx;
    }
    else
        return p_Vid->max_hzsr_idx[slice_type];
}

int get_max_vtsr(int slice_type, VideoParams *p_Vid)
{
    int i, max_idx = 0;
    int max_sr = 0;
    if (slice_type == P_SLICE && p_Vid->ref_l0_num != p_Vid->list_size[0]) {
        for (i = 0; i<p_Vid->list_size[0]; i++) {
            if (max_sr < p_Vid->vt_sr[P_SLICE][i]) {
                max_sr = p_Vid->vt_sr[P_SLICE][i];
                max_idx = i;
            }
        }
        return max_idx;
    }
    else
        return p_Vid->max_vtsr_idx[slice_type];
}

#ifdef HANDLE_SPSPPS_ZERO_WORD
#define PADDING_SIZE    8
static void set_header_bs_dword_align(EncoderParams *p_Enc)
{
    volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
    int offset;
    offset = (ptReg_cmd->STS1&0xFFFFFF) & (PADDING_SIZE-1);
    if (offset)
        offset = PADDING_SIZE - offset;
    else
        offset = 0;
    memset((void *)(p_Enc->u32BSBuffer_virt + p_Enc->u32HeaderBSLength), 0, offset);
    #ifdef LINUX
        //dma_map_single(NULL, (void *)(p_Enc->u32BSBuffer_virt + p_Enc->u32HeaderBSLength), offset, DMA_TO_DEVICE);
    #endif
    ptReg_mem->BS_ADDR = p_Enc->u32BSBuffer_phy + p_Enc->u32HeaderBSLength + offset;
    p_Enc->u32HeaderBSLength += offset;
}
#endif

int encode_one_slice_trigger(void *ptEncHandle)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
	VideoParams *p_Vid = p_Enc->p_Vid;
	SeqParameterSet *sps = p_Vid->active_sps;
	PicParameterSet *pps = p_Vid->active_pps;
	SliceHeader *currSlice = p_Vid->currSlice;
	StorablePicture *pic = p_Vid->dec_picture;
	FAVC_ENC_DIDN_PARAM *pDidn = &p_Vid->didn_param;	
	volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
	volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	//volatile H264Reg_Quant *ptReg_quant = (H264Reg_Quant *)(p_Enc->u32BaseAddr + MCP280_OFF_QUANT);
	int avg_qp, dsf, i, counter = 0;
	byte flag;
	uint8_t field_type = 0;//, list0_size;
	uint8_t slice_type = pic->slice_type;
	byte b_rec_out = 0;
    
// for VG mode
#ifdef VG_INTERFACE
    if (currSlice->slice_index < p_Vid->slice_number - 1) {
        currSlice->last_slice = 0;
        //currSlice->start_Y_pos = start_mb_Y;
        currSlice->end_Y_pos = currSlice->start_Y_pos + p_Vid->slice_line_number - 1;
    }
    else {
        currSlice->last_slice = 1;
        currSlice->end_Y_pos = (p_Vid->field_pic_flag ? p_Vid->frame_height/32 : p_Vid->frame_height/16) - 1;
    }
#endif
// end of for VG mode
    // wait until at least one SRAM buffer empty (previous header may paking large data)
    while (((ptReg_cmd->STS1>>24)&0x03) < 1) {
        counter++;
        if (counter >= MAX_POLLING_ITERATION)
            return H264E_ERR_POLLING_TIMEOUT;
    }
	//enc_index = p_Enc->enc_dev;

	ptReg_parm->PARM0 = (((currSlice->end_Y_pos&MASK8)<<24) | ((currSlice->start_Y_pos&MASK8)<<16) | \
		((sps->pic_width_in_mbs_minus1&MASK8)<<8) | ((p_Vid->source_width/16-1)&MASK8));

	// init_lists will set ref_num
	//flag = (pic->slice_type == B_SLICE ? pic->used_for_ref : 0);
	if (pic->structure != FRAME) {
		if (slice_type == I_SLICE) 
			field_type = (pic->structure == BOTTOM_FIELD);
		else if (slice_type == P_SLICE) {
			for (i = p_Vid->list_size[0]-1; i>=0; i--)
				field_type = (field_type<<1) | (p_Vid->ref_list[0][i]->structure == BOTTOM_FIELD);
			field_type = (field_type<<1) | (pic->structure == BOTTOM_FIELD);
		}
		else
			field_type = ((p_Vid->ref_list[1][0]->structure == BOTTOM_FIELD)<<2) | \
				((p_Vid->ref_list[0][0]->structure == BOTTOM_FIELD)<<1) | (pic->structure == BOTTOM_FIELD);
	}
	else 
		field_type = 0;

	ptReg_parm->PARM1 = (((sps->pic_height_in_map_units_minus1&MASK8)<<24) | ((field_type&MASK4)<<20) | \
		SHIFT_1(b_rec_out, 19) | SHIFT_1(currSlice->last_slice, 18) | SHIFT_1(sps->chroma_format_idc, 17) | \
		SHIFT_1(p_Vid->field_pic_flag, 16) | ((get_max_hzsr(slice_type, p_Vid)&MASK2)<<14) | \
		((slice_type&MASK2)<<12) | ((p_Vid->vt_sr[slice_type][0]&MASK6)<<6) | (p_Vid->hz_sr[slice_type][0]&MASK6));

	ptReg_parm->PARM2 = (((p_Vid->vt_sr[slice_type][2]&MASK6)<<24) | ((p_Vid->hz_sr[slice_type][2]&MASK6)<<16) | \
		((p_Vid->vt_sr[slice_type][1]&MASK6)<<8) | (p_Vid->hz_sr[slice_type][1]&MASK6));

	ptReg_parm->PARM3 = (((p_Vid->currQP&MASK6)<<26) | ((pps->second_chroma_qp_index_offset&MASK5)<<21) | \
		((pps->chroma_qp_index_offset&MASK5)<<16) | SHIFT_1(p_Vid->intra_4x4_mode_number, 15) | \
		SHIFT_1(p_Vid->disable_coef_threshold, 14) | ((p_Vid->luma_coef_thd&MASK3)<<11) | \
		((p_Vid->chroma_coef_thd&MASK3)<<8) | ((currSlice->slice_beta_offset_div2&MASK4)<<4) | \
		(currSlice->slice_alpha_c0_offset_div2&MASK4));
	if (slice_type != I_SLICE)
		ptReg_parm->PARM4 = (((p_Vid->fme_cyc_thd[slice_type]&MASK12)<<20) | (p_Vid->fme_cost_thd&MASK16));
	else
		ptReg_parm->PARM4 = (0xFFF00000 | (p_Vid->fme_cost_thd&MASK16));

/*
	if ((dsf = get_dsf(p_Vid, currSlice, pic)) == -9999) {
	    E_DEBUG("reference list error\n");
		return H264E_ERR_REF;
	}
*/
    dsf = get_dsf(p_Vid, currSlice, pic);
/*
	if (pic->structure == BOTTOM_FIELD && p_Vid->p_ref1_field_temporal)
		list0_size = 1;
	else
		list0_size = iMax(p_Vid->list_size[0]-1, 0);
*/
    if (currSlice->last_slice) {
        if (FRAME == pic->structure || BOTTOM_FIELD == pic->structure)
            flag = 1;
    #ifdef HANDLE_CABAC_ZERO_WORD 
        else if (TOP_FIELD == pic->structure && pps->entropy_coding_mode_flag)
            flag = 1;
    #endif
        else
            flag = 0;                
    }
    else
        flag = 0;

	ptReg_parm->PARM5 = (((flag&MASK1)<<31) | ((dsf&MASK11)<<19) | \
		SHIFT_1(currSlice->direct_spatial_mv_pred_flag, 18) | SHIFT_1(p_Vid->inter_default_trans_size, 17) | \
		SHIFT_1(pps->transform_8x8_mode_flag, 16) | ((iMax(p_Vid->list_size[0]-1,0)&MASK2)<<14) | \
		((p_Vid->inter_pred_mode_dis[pic->slice_type]&MASK4)<<10) | SHIFT_1(p_Vid->fast_intra_4x4, 9) | \
		((p_Vid->dn_result_format&MASK2)<<7) | SHIFT_1(p_Vid->sad_source, 6) | \
		SHIFT_1(p_Vid->no_quant_matrix_flag, 5) | ((p_Vid->mb_qp_weight&MASK3)<<2) | \
		(currSlice->disable_deblocking_filter_idc&MASK2));

/****************************************************************
 *		RC function
 ****************************************************************/

// CABAC register must set after sps/pps because close bitstream
/*
	ptReg_parm->PARM6 = (((p_Vid->rc_basicunit&MASK16)<<16) | ((get_max_vtsr(slice_type, p_Vid)&MASK2)<<10) | \
	    ((p_Vid->disable_intra_in_i&MASK2)<<7) | SHIFT_1(p_Vid->disable_intra_in_pb, 6) | \
	    SHIFT_1(p_Vid->wk_enable, 5) | \
		SHIFT_1(p_Vid->intra_8x8_disable, 4) | SHIFT_1(p_Vid->intra_16x16_plane_disbale, 3) | \
		((p_Vid->cabac_init_idc&MASK2)<<1) | SHIFT_1(pps->entropy_coding_mode_flag, 0));
*/
	ptReg_parm->PARM7 = p_Vid->bitbudget_frame;
	ptReg_parm->PARM8 = p_Vid->bitbudget_bu;

	if (p_Vid->mb_qp_weight<4) {
		if (p_Vid->ave_frame_qp == -1) {
			//avg_qp = p_Vid->currQP;
        #ifndef RC_ENABLE
    		avg_qp = p_Vid->fixed_qp[P_SLICE];
        #else
            avg_qp = p_Vid->currQP;
    	#endif
        }
		else
			avg_qp = p_Vid->ave_frame_qp;
	}
	else
		avg_qp = 0;

#ifdef ENABLE_MB_RC
    if (p_Vid->mb_qp_weight < 4) {
        // avg_qp must be the same as rc_qpy
        /*
        ptReg_parm->PARM9 = (((p_Vid->rc_qpy&MASK6)<<20) | ((p_Vid->rc_qp_step&MASK2)<<18) | \
            ((p_Vid->rc_qpy&MASK6)<<12) | ((iMin(51, p_Vid->currQP+gMBQPLimit)&MASK6)<<6) | \
            (iMax(1, p_Vid->currQP-gMBQPLimit)&MASK6));
        */
        ptReg_parm->PARM9 = (((p_Vid->rc_qpy&MASK6)<<20) | ((p_Vid->rc_qp_step&MASK2)<<18) | \
            ((p_Vid->rc_qpy&MASK6)<<12) | ((iMin(p_Vid->rc_max_qp[slice_type], p_Vid->currQP+gMBQPLimit)&MASK6)<<6) | \
            (iMax(p_Vid->rc_min_qp[slice_type], p_Vid->currQP-gMBQPLimit)&MASK6));
    }
#else
    if (p_Vid->mb_qp_weight < 4) {
    	ptReg_parm->PARM9 = (((p_Vid->rc_qpy&MASK6)<<20) | ((p_Vid->rc_qp_step&MASK2)<<18) | \
    		((avg_qp&MASK6)<<12) | ((p_Vid->rc_max_qp[pic->slice_type]&MASK6)<<6) | \
    		(p_Vid->rc_min_qp[pic->slice_type]&MASK6));
    }
#endif

	flag = (p_Vid->encoded_frame_num == 0);

    ptReg_parm->PARM10 = (((p_Vid->intra_cost_ratio&MASK4)<<16) | SHIFT_1(p_Vid->int_top_btm, 6) | \
        ((p_Vid->didn_mode&MASK4)<<2) | SHIFT_1(p_Vid->src_img_type, 1) | SHIFT_1(flag, 0));

	if (p_Vid->didn_param_enable) {
		// set didn parameter register
		ptReg_parm->PARM11 = (((pDidn->u8DnVarY&MASK7)<<14) | ((pDidn->u8DnVarCb&MASK7)<<7) | (pDidn->u8DnVarCr&MASK7));
		ptReg_parm->PARM12 = (((pDidn->u32DnVarPixelUpperbound&MASK10)<<10) | (pDidn->u32DnVarPixelLowerbound&MASK10));
		ptReg_parm->PARM13 = (((p_Vid->force_mv0_thd&MASK16)<<16) | \
            ((pDidn->u8DnVarLumaMBThd&MASK8)<<6) | (pDidn->u8DnVarChromaMBThd&MASK6));
/* old didn setting
		ptReg_parm->PARM14 = (((pDidn->u8DnMinFilterLevel&MASK2)<<12) | ((pDidn->u8DnMaxFilterLevel&MASK3)<<9) | \
			SHIFT_1(pDidn->bSeparateHrVtDn, 8) | (pDidn->u8SpDnThd&MASK8));
		ptReg_parm->PARM15 = (((pDidn->u8MBAllMotionThd&MASK8)<<24) | ((pDidn->u8TmDiShadowMotionThd&MASK8)<<16) | \
			((pDidn->u8OsdThd&MASK8)<<8) | SHIFT_1(pDidn->bTmDiChromaMotionEnable, 4) | SHIFT_1(pDidn->bAllMotionMethod2Enable, 3) | \
			SHIFT_1(pDidn->bAllMotionMethod1Enable, 2) | SHIFT_1(pDidn->bMotionExtendEnable, 1) | SHIFT_1(pDidn->bOsdEnable, 0));
		ptReg_parm->PARM16 = (((pDidn->u8ELADeAmbiguousUpperbound&MASK8)<<16) | ((pDidn->u8ELADeAmbiguousLowerbound&MASK8)<<8) | \
			SHIFT_1(pDidn->bELACornerEnable, 0));
*/
        ptReg_parm->PARM14 = (SHIFT_1(pDidn->bELAResult, 29) | SHIFT_1(pDidn->bTopMotion, 28) | SHIFT_1(pDidn->bBottomMotion, 27)
            | SHIFT_1(pDidn->bStrongMotion, 26) | SHIFT_1(pDidn->bAdaptiveSetMBMotion, 25) | SHIFT_1(pDidn->bStaticMBMask, 24)
            | SHIFT_1(pDidn->bExtendMotion, 23) | SHIFT_1(pDidn->bLineMask, 22) | ((pDidn->u8EdgeStrength&MASK8)<<14)
            | ((pDidn->u8DnMinFilterLevel&MASK2)<<12) | ((pDidn->u8DnMaxFilterLevel&MASK3)<<9) 
            | SHIFT_1(pDidn->bSeparateHrVtDn, 8) | (pDidn->u8SpDnThd&MASK8));
        ptReg_parm->PARM15 = (((pDidn->u8MBAllMotionThd&MASK8)<<24) | ((pDidn->u8StaticMBMaskThd&MASK8)<<16)
            | ((pDidn->u8ExtendMotionThd&MASK8)<<8) | (pDidn->u8LineMaskThd&MASK8));
		ptReg_parm->PARM16 = (((pDidn->u8StrongMotionThd&MASK8)<<24) | ((pDidn->u8ELADeAmbiguousUpperbound&MASK8)<<16)
            | ((pDidn->u8ELADeAmbiguousLowerbound&MASK8)<<8) | SHIFT_1(pDidn->bELACornerEnable, 0));
        ptReg_parm->PARM17 = (((pDidn->u8LineMaskAdmissionThd&MASK8)<<22) | ((pDidn->u8SpDnLineDetStr&MASK2)<<20)
            | ((p_Vid->max_delta_qp&MASK5)<<15) | ((p_Vid->delta_qp_strength&MASK5)<<10) | (p_Vid->delta_qp_thd&MASK10));
	}
	else {
        if (p_Vid->force_mv0_thd)
    	    ptReg_parm->PARM13 = ((p_Vid->force_mv0_thd&MASK16)<<16) | (ptReg_parm->PARM13&MASK16);
        ptReg_parm->PARM17 = ((ptReg_parm->PARM17&0xFFF00000) | ((p_Vid->max_delta_qp&MASK5)<<15)
            | ((p_Vid->delta_qp_strength&MASK5)<<10) | (p_Vid->delta_qp_thd&MASK10));
	}

    if ((pic->structure == FRAME || pic->structure == TOP_FIELD) && currSlice->first_mb_in_slice == 0 && p_Vid->wk_enable) {
		ptReg_parm->PARM18 = p_Vid->wk_pattern;
    }

	if (p_Vid->mb_qp_weight < 4) {
		ptReg_parm->PARM19 = p_Vid->rc_init2;
    }

	// set buffer address
	set_meminfo(p_Enc, p_Vid, currSlice, b_rec_out);
	set_roi_qp_register(p_Enc, p_Vid);

	//ptReg_mem->SWAP_DMA = 1;	// for didn input
#if 0
#ifdef VG_INTERFACE
    if (p_Vid->vcache_enable) {
        if (p_Vid->vcache_ref2_enable)
            ptReg_mem->SWAP_DMA = h264e_yuv_swap|BIT1|BIT2;  // swap & out of order
        else
            ptReg_mem->SWAP_DMA = h264e_yuv_swap|BIT2;
    }
    else
        ptReg_mem->SWAP_DMA = h264e_yuv_swap;
#else
    if (p_Vid->vcache_enable) {
        if (p_Vid->vcache_ref2_enable)
            ptReg_mem->SWAP_DMA = INPUT_YUYV|BIT1|BIT2;  // swap & out of order
        else
            ptReg_mem->SWAP_DMA = INPUT_YUYV|BIT2;
    }
    else
        ptReg_mem->SWAP_DMA = INPUT_YUYV;
#endif
#else
#ifdef VG_INTERFACE
    if (p_Vid->vcache_enable)
	    ptReg_mem->SWAP_DMA = h264e_yuv_swap|BIT1|BIT2;  // swap & out of order
	else
	    ptReg_mem->SWAP_DMA = h264e_yuv_swap;
#else
	if (p_Vid->vcache_enable)
	    ptReg_mem->SWAP_DMA = INPUT_YUYV|BIT1|BIT2;  // swap & out of order
	else
	    ptReg_mem->SWAP_DMA = INPUT_YUYV;
#endif
#endif
	// set quant
	if (!p_Vid->no_quant_matrix_flag)
		set_quant_matrix(p_Enc, sps, &p_Vid->mQuant);

    // generate slice header
	if (p_Vid->write_spspps_flag) {
        //int header_size = 0;

        if (pps->entropy_coding_mode_flag) {
            ptReg_parm->PARM6 = (((p_Vid->rc_basicunit&MASK16)<<16) | ((get_max_vtsr(slice_type, p_Vid)&MASK2)<<10) | \
        	    ((p_Vid->disable_intra_in_i&MASK2)<<7) | SHIFT_1(p_Vid->disable_intra_in_pb, 6) | \
        	    SHIFT_1(p_Vid->wk_enable, 5) | \
        		SHIFT_1(p_Vid->intra_8x8_disable, 4) | SHIFT_1(p_Vid->intra_16x16_plane_disbale, 3) | \
        		((p_Vid->cabac_init_idc&MASK2)<<1) | SHIFT_1(0, 0));
        }
        else {
            ptReg_parm->PARM6 = (((p_Vid->rc_basicunit&MASK16)<<16) | ((get_max_vtsr(slice_type, p_Vid)&MASK2)<<10) | \
                ((p_Vid->disable_intra_in_i&MASK2)<<7) | SHIFT_1(p_Vid->disable_intra_in_pb, 6) | \
                SHIFT_1(p_Vid->wk_enable, 5) | \
                SHIFT_1(p_Vid->intra_8x8_disable, 4) | SHIFT_1(p_Vid->intra_16x16_plane_disbale, 3) | \
                ((p_Vid->cabac_init_idc&MASK2)<<1) | SHIFT_1(pps->entropy_coding_mode_flag, 0));
        }

		if (write_sps(p_Enc, p_Vid->active_sps) < 0) {
		    E_DEBUG("write sps timeout\n");
        #ifdef VG_INTERFACE
            printm("FE", "write sps timeout\n");
        #endif
		    return H264E_ERR_POLLING_TIMEOUT;
		}
		if (write_pps(p_Enc, p_Vid->active_pps) < 0) {
		    E_DEBUG("write pps timeout\n");
        #ifdef VG_INTERFACE
            printm("FE", "write pps timeout\n");
        #endif
		    return H264E_ERR_POLLING_TIMEOUT;
		}

        p_Vid->slice_offset[0] = ptReg_cmd->STS1&0xFFFFFF;
    #ifdef AES_ENCRYPT
    {
        // 1. record the correct position of sei
        // 2. packing sei nalu
        // 3. update sei data by slice data
        char aes_data[AES_SEI_LENGTH+1];
        int aes_key_len = sizeof(AES_KEY);
        // step 1: record the correct position of sei
        p_Enc->u32SEIStart = p_Vid->slice_offset[0];
        // step 2: packing sei nalu
        memcpy(aes_data, AES_KEY, sizeof(AES_KEY));
        memset((void *)(&aes_data[aes_key_len]), 0xFF, AES_SEI_LENGTH - aes_key_len);
		//write_sei(p_Enc, AES_SEI_TYPE, AES_SEI_LENGTH, aes_data);
		write_sei(p_Enc, AES_SEI_TYPE, AES_SEI_LENGTH, aes_data);
    }
    #endif
        /* // old version 
         * CABAC must close bitsteram (flush bitstream) and clear bitstream 
         * beacuse hardwave must count bitstream of slice not inclusing SPS
         * and PPS (for packing zero)
         * clear bitstream will reset STS1 (bitstream length & set STS10 be 
         * initial value)
         * padding_zero_dword_align: padding zero to dword align & accumulate
         * bitstream length */
        // for CABAC padding zero (minus pps not outputed bitstraem)
    #ifdef HANDLE_SPSPPS_ZERO_WORD
        if (pps->entropy_coding_mode_flag) {
        #ifdef HANDLE_SPSPPS_ZERO_WORD
            if (close_bitstream(ptReg_cmd) < 0)
                return H264E_ERR_POLLING_TIMEOUT;
            p_Enc->u32HeaderBSLength = ptReg_cmd->STS1&0xFFFFFF;
            //p_Enc->u32HeaderBSLength = header_size + (ptReg_cmd->STS1&0xFFFFFF);
            set_header_bs_dword_align(p_Enc);
            clear_bs_length(p_Enc, ptReg_cmd);
        #else
            int offset;
            offset = (ptReg_cmd->STS1>>26)&0x07;
            if (0 == offset)
                p_Vid->bitlength -= 4;
            else
                p_Vid->bitlength -= offset;
        #endif
        }
        else
            p_Enc->u32HeaderBSLength = 0;
    #else
        p_Enc->u32HeaderBSLength = 0;
    #endif
	}

	// must after sps pps header (do not include sps, pps headerS)
#ifdef MCNR_ENABLE
	if (pps->entropy_coding_mode_flag) {
        // Tuba 20121015 mcnr start
    	if (I_SLICE == slice_type)
            ptReg_parm->PARM20 = (p_Vid->field_pic_flag ? p_Vid->total_mb_number/2 : p_Vid->total_mb_number);
        else {
            ptReg_parm->PARM20 = ((log2_width(p_Vid->frame_width)&MASK4)<<24) | ((p_Vid->mcnr_var_thd&MASK8)<<16) |
                (p_Vid->field_pic_flag ? p_Vid->total_mb_number/2 : p_Vid->total_mb_number);
        }
        // Tuba 20121015 end
        ptReg_cmd->STS9 = p_Vid->bincount;
        ptReg_cmd->STS10 = p_Vid->bitlength;
    }
    else {
        if (I_SLICE != slice_type)
            ptReg_parm->PARM20 = ((log2_width(p_Vid->frame_width)&MASK4)<<24) | ((p_Vid->mcnr_var_thd&MASK8)<<16);
    }
#else
    if (pps->entropy_coding_mode_flag) {
        ptReg_parm->PARM20 = (p_Vid->field_pic_flag ? p_Vid->total_mb_number/2 : p_Vid->total_mb_number);
        ptReg_cmd->STS9 = p_Vid->bincount;
        ptReg_cmd->STS10 = p_Vid->bitlength;
    }
#endif
	ptReg_parm->PARM6 = (((p_Vid->rc_basicunit&MASK16)<<16) | ((get_max_vtsr(slice_type, p_Vid)&MASK2)<<10) | \
	    ((p_Vid->disable_intra_in_i&MASK2)<<7) | SHIFT_1(p_Vid->disable_intra_in_pb, 6) | \
	    SHIFT_1(p_Vid->wk_enable, 5) | \
		SHIFT_1(p_Vid->intra_8x8_disable, 4) | SHIFT_1(p_Vid->intra_16x16_plane_disbale, 3) | \
		((p_Vid->cabac_init_idc&MASK2)<<1) | SHIFT_1(pps->entropy_coding_mode_flag, 0));
#ifdef MCNR_ENABLE
    if (I_SLICE != slice_type)
        ptReg_cmd->STS11 = p_Vid->mcnr_sad;
#endif

	if (write_slice_header(p_Enc, p_Vid, currSlice, pic) < 0) {
	    E_DEBUG("write slice header timeout\n");
    #ifdef VG_INTERFACE
        printm("FE", "write slice header timeout\n");
    #endif
	    return H264E_ERR_POLLING_TIMEOUT;
	}

	mFa526CleanInvalidateDCacheAll();
#ifdef HANDLE_BS_CACHEABLE
    fmem_dcache_sync((void *)p_Enc->u32BSBuffer_virt, p_Enc->u32BSBufferSize, DMA_FROM_DEVICE);
#endif

#ifdef DUMP_REGISTER
    dump_all_register(p_Enc, DUMP_ALL&(~DUMP_QUANT_TABLE));
#endif
    if (force_dump_register) {
        dump_all_register(p_Enc, force_dump_register);
        force_dump_register = 0;
    }

	ptReg_cmd->CMD0 = 1;		// encode go
#ifdef REGISTER_NCB
    flag = ptReg_cmd->CMD0;
#endif
#ifndef VG_INTERFACE
	enc_index = p_Enc->enc_dev;
#endif

	return H264E_OK;	
}

#if defined(LINUX)&(!defined(VG_INTERFACE))
extern wait_queue_head_t mcp280_wait_queue;
#endif
int encode_one_slice_error_handle(EncoderParams *p_Enc, uint32_t irq, uint32_t bs_len) 
{
    volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    unsigned int interrupt_flag = INTERRUPT_FLAG;
    /* slice done check firstly, because overflow occur before slice 
     * done when the difference of buffer size and bitstream size is 
     * less than 64    */
    /* priority: dma fail > slice done > overflow > timeout */
#ifdef DISABLE_WRITE_OUT_REC
    if (h264e_dis_wo_buf)
        interrupt_flag = INTERRUPT_FLAG_NO_DMA;
    else
#endif
        interrupt_flag = INTERRUPT_FLAG;
    if (irq & (IRQ_DMA_WRITE_ERR|IRQ_DMA_READ_ERR)) {   // DMA fail
        p_Enc->u32CurrIRQ &= ~0x1F;
        ptReg_cmd->STS2 = ((~interrupt_flag)&0x1F)<<16;
        dump_all_register(p_Enc, DUMP_PARAM|DUMP_CMD|DUMP_MEM|DUMP_VCACHE);
        return H264E_ERR_DMA;
    }
    else if (irq & IRQ_SLICE_DONE) {      // slice done
        if (irq & IRQ_BS_FULL) {
            //if (bs_len - p_Enc->u32OutputedBSLength > p_Enc->u32MaxBSLength) {
            if (bs_len > p_Enc->u32MaxBSLength) {
                p_Enc->u32CurrIRQ &= ~IRQ_BS_FULL;
                ptReg_cmd->STS2 = ((~interrupt_flag)&0x1F)<<16;
                //(*reset_irq) = BIT1;
                return H264E_ERR_BS_OVERFLOW;
            }
        }
        p_Enc->u32CurrIRQ &= ~0x1F;
        ptReg_cmd->STS2 = ((~interrupt_flag)&0x1F)<<16;
        return H264E_OK;
    }
    else if (irq & IRQ_BS_FULL) {      // bitstream buffer overflow
        p_Enc->u32CurrIRQ &= ~IRQ_BS_FULL;
        ptReg_cmd->STS2 = ((~interrupt_flag)&0x1F)<<16;
        #ifdef VG_INTERFACE
            DEBUG_M(LOG_WARNING, "[favce warning] bitstream overflow, size %d > bitstream buffer %d (QP = %d)\n", 
                bs_len, p_Enc->u32MaxBSLength, p_Enc->p_Vid->currQP);
        #else
            printk("[favce warning] bitstream overflow, size %d > bitstream buffer %d (QP = %d)\n", 
                bs_len, p_Enc->u32MaxBSLength, p_Enc->p_Vid->currQP);
        #endif
        return H264E_ERR_BS_OVERFLOW;
    }
    else if (irq & IRQ_HW_TIMEOUT) {      // timeout
        //(*reset_irq) = 0x1F;
        p_Enc->u32CurrIRQ &= ~0x1F;
        ptReg_cmd->STS2 = ((~interrupt_flag)&0x1F)<<16;
        dump_all_register(p_Enc, DUMP_PARAM|DUMP_CMD|DUMP_MEM|DUMP_VCACHE);
        return H264E_ERR_HW_TIMEOUT;
    }
    //DEBUG_M(LOG_ERROR, "{chn%d} unknown irq 0x%x (base 0x%x)\n", p_Enc->enc_dev, irq, p_Enc->u32BaseAddr);
    return H264E_ERR_UNKNOWN;
}

int encode_one_slice_sync(void *ptEncHandle)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
	VideoParams *p_Vid = p_Enc->p_Vid;
	volatile H264Reg_Parm *ptReg_parm = (H264Reg_Parm *)(p_Enc->u32BaseAddr);
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    //uint32_t tmp_irq;
    int ret;

    //dump_cmd_register(p_Enc);
	if ((ret = encode_one_slice_error_handle(p_Enc, p_Enc->u32CurrIRQ, (ptReg_cmd->STS1&0xFFFFFF))) < 0) {
	//if ((ret = encode_one_slice_error_handle(p_Enc, &tmp_irq, (ptReg_cmd->STS1&0xFFFFFF))) < 0) {
        //p_Enc->u32CurrIRQ &= tmp_irq;
	    //reset_irq(p_Enc);
    #ifdef OVERFLOW_REENCODE
        p_Vid->record_sts0 = ptReg_cmd->STS0;
    #endif
	#ifdef NO_RESET_REGISTER_PARAM_UPDATE
		init_register(p_Enc, 1);
	#else
	    init_register(p_Enc);
	#endif
	    return ret;
	}
	//reset_irq(p_Enc);

	p_Vid->write_spspps_flag = 0;
	if (p_Vid->mb_qp_weight < 4) {
	    p_Vid->bitbudget_frame = ptReg_cmd->STS8;	// get the rest budget
    }
	p_Vid->currSlice->first_mb_in_slice += p_Vid->slice_line_number*p_Vid->mb_row;
	p_Vid->currSlice->start_Y_pos += p_Vid->slice_line_number;
	p_Vid->total_cost += ptReg_cmd->STS3;
	p_Vid->sum_mb_qp += ptReg_cmd->STS4;
	if (p_Vid->mb_qp_weight < 4) {
		//p_Vid->initial_qp = ((ptReg_parm->PARM3>>26)&MASK6);
		p_Vid->rc_qpy = (ptReg_parm->PARM9>>20)&MASK6;
		p_Vid->rc_init2 = ptReg_parm->PARM19;
	}
	if (p_Vid->mb_qp_weight>0 && p_Vid->mb_qp_weight<=4) {
		p_Vid->delta_mb_qp_sum += ptReg_cmd->STS7;
		p_Vid->delta_mb_qp_sum_clamp += ptReg_cmd->STS6;
		p_Vid->nonz_delta_qp_cnt += ptReg_cmd->STS5;
	}
#ifdef MCNR_ENABLE
    p_Vid->mcnr_sad = ptReg_cmd->STS11;
#endif
    //p_Enc->u32ResBSLength = (ptReg_cmd->STS1&0xFFFFFF) - p_Enc->u32OutputedBSLength;
    p_Enc->u32ResBSLength = (ptReg_cmd->STS1&0xFFFFFF);

    // if CABAC, count bitstream length of current frame except start code
    if (p_Vid->active_pps->entropy_coding_mode_flag) {
    	p_Vid->bitlength = ptReg_cmd->STS10 - 3;
    	p_Vid->bincount = ptReg_cmd->STS9;
    }
    //dump_all_register(p_Enc, DUMP_ALL&(~DUMP_QUANT_TABLE));
    p_Vid->cur_slice_idx++; // skip first slice (header)
    if (p_Vid->cur_slice_idx < MAX_SLICE_NUM)
        p_Vid->slice_offset[p_Vid->cur_slice_idx] = ptReg_cmd->STS1&0xFFFFFF;
#ifdef VG_INTERFACE
    p_Vid->currSlice->slice_index++;

    if (p_Vid->currSlice->last_slice) {
        favce_info("plane done");
        return PLANE_DONE;
    }
    favce_info("slice done");
    return SLICE_DONE;    
#else
	return H264E_OK;
#endif
}

#ifndef VG_INTERFACE
int encode_block(EncoderParams *p_Enc)
{
#ifdef LINUX
	wait_event_timeout(mcp280_wait_queue, WAIT_EVENT, msecs_to_jiffies(SOFTWARE_TIMEOUT));	// fixed timout
	if (!WAIT_EVENT) {
	    E_DEBUG("software timeout\n");
		return H264E_ERR_SW_TIMEOUT;
	}
#else   // LINUX
	volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);	
    //volatile H264Reg_Mem *ptReg_mem = (H264Reg_Mem *)(p_Enc->u32BaseAddr + MCP280_OFF_MEM);
	uint32_t tmp;
	while (!HW_INTERRUPT) {
	    int i;
	    for (i = 0; i<100; i++);
	    //tmp = ptReg_cmd->STS1;
        //tmp = ptReg_mem->BS_BUF_SIZE;
        tmp = ptReg_cmd->STS2;
	}
    p_Enc->u32CurrIRQ = (ptReg_cmd->STS2&0x1F);
    ptReg_cmd->STS2 &= 0x1F;
#endif
	return H264E_OK;
}

int encode_one_slice(void *ptEncHandle)
{
	EncoderParams *p_Enc = (EncoderParams *)ptEncHandle;
    //volatile H264Reg_Cmd *ptReg_cmd = (H264Reg_Cmd *)(p_Enc->u32BaseAddr + MCP280_OFF_CMD);
    //int tmp_bs;
	int ret;
    /*
    if (p_Enc->u8CurrState != NORMAL_STATE) {
        p_Enc->u8CurrState = NORMAL_STATE;
        goto wait_slice_done;
    }
    */

    // before encode slice, check the bitstream buffer
    // return bitstream to AP when bitstream buffer will be full (packing header does not 
    // polling overflow interrupt)
    /*
    if ((p_Enc->u32MaxBSLength - p_Enc->u32ResBSLength) < REMAIN_BS_THRESHOLD) {
        p_Enc->u8CurrState = OVERFLOW_HEADER_STATE;
        // set the other bs buffer
        //p_Enc->u8OutBSBufIdx = p_Enc->u8CurrBSBufIdx;
        //p_Enc->u8CurrBSBufIdx ^= 1;
        // return bitstream length = p_Vid->bitstream_length
        p_Enc->u32ResBSLength = (ptReg_cmd->STS1&0xFFFFFF) - p_Enc->u32OutputedBSLength;
        p_Enc->u32ResBSLength &= 0xFFFFFFC0;    // 64-byte align

        // trigger but not block
        reset_bs_param(p_Enc, p_Enc->u32ResBSLength);   
        // reset bitsteram buffer
        // 1. fill new bitstream buffer & buffer size
        // 2. add outputed bitstream length
        if ((ret = encode_one_slice_trigger(p_Enc)) < 0)
            return ret;
        return H264E_ERR_BS_OVERFLOW;
    }
    */
	if ((ret = encode_one_slice_trigger(p_Enc)) < 0)
	    return ret;

//wait_slice_done:
//irq_array[irq_number++] = 0x4400;
	if ((ret = encode_block(p_Enc)) < 0)
		return ret;
	if ((ret = encode_one_slice_sync(p_Enc)) < 0) {
        // handle slice bs full
        /*
        if (ret == H264E_ERR_BS_OVERFLOW) {
            p_Enc->u8CurrState = OVERFLOW_SLICE_STATE;
            // return bitstream length = buffer size
            //p_Enc->u8OutBSBufIdx = p_Enc->u8CurrBSBufIdx;
            //p_Enc->u8CurrBSBufIdx ^= 1;

            tmp_bs = ptReg_cmd->STS1&0xFFFFFF; // not use register to compare (remember then compare, because the variable may be different between comparison & assignment)
            p_Enc->u32ResBSLength = iMin(p_Enc->u32MaxBSLength, tmp_bs - p_Enc->u32OutputedBSLength);
            reset_bs_param(p_Enc, p_Enc->u32ResBSLength);
        }
        */
        return ret;
    }
	return H264E_OK;
}
#endif

