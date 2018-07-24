#ifdef LINUX
	#include <linux/kernel.h>
#endif
#include "global.h"
#include "h264_reg.h"


extern void dump_list(DecoderParams *p_Dec, int dump_mem);
//extern void dump_vcache_register(void *ptDecHandle, int level);


void dump_param_register(DecoderParams *p_Dec, unsigned int base_addr, int level)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)base_addr;
    VideoParams *p_Vid = NULL;
    uint32_t tmp = 0;

    if(p_Dec){
        p_Vid = p_Dec->p_Vid;
        tmp = DECODER_PARAM5((p_Vid->PicSizeInMbs - p_Vid->active_pps->entropy_coding_mode_flag), p_Vid->slice_hdr->currMbAddr);
        ptReg_io = (H264_reg_io *)p_Dec->pu32BaseAddr;
    }
    
	LOG_PRINT(level, "PARM0 : %08x\n", ptReg_io->DEC_PARAM0);
	LOG_PRINT(level, "PARM1 : %08x\n", ptReg_io->DEC_PARAM1);
	LOG_PRINT(level, "PARM3 : %08x\n", ptReg_io->DEC_PARAM3);
	LOG_PRINT(level, "PARM4 : %08x\n", ptReg_io->DEC_PARAM4);
	LOG_PRINT(level, "PARM5 : %08x\n", ptReg_io->DEC_PARAM5);
    if(p_Dec){
    	LOG_PRINT(level, "PARAM5 : %08x init\n", tmp);
    }
	LOG_PRINT(level, "PARM6 : %08x\n", ptReg_io->DEC_PARAM6);
	LOG_PRINT(level, "PARM7 : %08x\n", ptReg_io->DEC_PARAM7);
	LOG_PRINT(level, "PARM30: %08x\n", ptReg_io->DEC_PARM30);
	LOG_PRINT(level, "PARM31: %08x\n", ptReg_io->DEC_PARM31);
    if(p_Vid){
    	LOG_PRINT(level, "PARAM31: %08x init\n", (SHIFT(p_Vid->bottompoc, 16, 16) | SHIFT(p_Vid->col_index, 8, 0)));
    }
}


#if !(SLICE_DMA_DISABLE)
static void vld_sysinfo_recover(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	VideoParams *p_Vid = p_Dec->p_Vid;
	uint32_t tmp, mb_cnt, sys_idx, sys_idx_new;
	int mb_addr, mb_x;

	tmp = 0;
	while (!(((tmp&BIT2)>>2) & ((tmp&BIT3)>>3)))
		tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)
// --------------------------------------
// move mb_info of left_mb
// --------------------------------------
	if (p_Vid->MbaffFrameFlag){
		mb_addr = p_Vid->slice_hdr->currMbAddr - 2;
		mb_cnt = 2;		// number of movied mbs
		sys_idx = 6;
		mb_x = (mb_addr>>1)%p_Vid->PicWidthInMbs;
	}
	else {
		mb_addr = p_Vid->slice_hdr->currMbAddr - 1;
		mb_cnt = 1;
		sys_idx = 7;
		mb_x = mb_addr%p_Vid->PicWidthInMbs;
	}
	//if (mb_addr >= 0){
	if (mb_addr >= 0 && mb_x != p_Vid->active_sps->pic_width_in_mbs_minus1) {
		ptReg_io->DEC_PARAM7 = (0x01<<19) | (sys_idx<<16) | 1;	// sys_mem index
//printk("mb_addr > 0, vld parm7: %08x\n", (0x01<<19) | (sys_idx<<16) | 1);
		// cpu_sel = 1, idx0 = sys_idx, en_gate_clk = 1

		tmp = (32<<19);			// offset
		tmp |= (mb_cnt<<5)<<10;	// length (32 dword/pre-mb) (cur-mb is 32 dword/pre-mb)
		tmp |= (32<<4) | 6;		// block width, id of sys mem
		ptReg_io->CPUCMD0 = tmp;
//printk("mb_addr > 0, vld cmd0: %08x\n", tmp);
		tmp = ((uint32_t)p_Vid->dec_pic->mbinfo_addr_phy);// + p_Vid->frame_size_mb/2;
		tmp += (mb_addr<<8);
		tmp |= 1;
		ptReg_io->CPUCMD1 = tmp;
//printk("mbinfo %08x\n", (uint32_t)p_Vid->dec_pic->mbinfo_addr_phy);
//printk("mb_addr > 0, vld cmd1: %08x\n", tmp);
	}
// --------------------------------------
// move mb_info of top_mb
// --------------------------------------
	if (p_Vid->MbaffFrameFlag){
		mb_addr = p_Vid->slice_hdr->currMbAddr - 2 - 2*p_Vid->PicWidthInMbs;
		if (mb_addr < 0){
			if (mb_addr == -4){	// last mb of first row
				mb_cnt = 2;
				sys_idx_new = 2;
			}
			else if (mb_addr == -2){	// first mb of second row
				mb_cnt = 4;
				sys_idx_new = 4;
			}
			else {
				mb_cnt = 0;
				sys_idx_new = 0;
			}
			mb_addr = 0;
			sys_idx = 0;
		}
		else {
			mb_cnt = 6;
			sys_idx = 6;
			sys_idx_new = 4;
		}
	}
	else {
		mb_addr = p_Vid->slice_hdr->currMbAddr - 1 - p_Vid->PicWidthInMbs;
		if (mb_addr<0){
			if (mb_addr == -2){
				mb_cnt = 1;
				sys_idx_new = 1;
			}
			else if (mb_addr == -1){
				mb_cnt = 2;
				sys_idx_new = 2;
			}
			else {
				mb_cnt = 0;
				sys_idx_new = 0;
			}
			mb_addr = 0;
			sys_idx = 0;
		}
		else {
			mb_cnt = 3;
			sys_idx = 7;
			sys_idx_new = 2;
		}
	}
	if (mb_cnt != 0){
		tmp = 0;
		while (!(((tmp&BIT2)>>2) & ((tmp&BIT3)>>3)))
			tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)

		ptReg_io->DEC_PARAM7 = (sys_idx<<20) | 1;	// idx1
//printk("mb_cnt != 0, vld parm7: %08x\n", (sys_idx<<20) | 1);
		tmp = (32<<19);			// offset
		tmp |= (mb_cnt<<4)<<10;	// 16 dword/pre-mb (upper mb is 16 dowrd/per-mb)
		tmp |= (16<<4) | 6;		// block width
		ptReg_io->CPUCMD0 = tmp;
//printk("mb_cnt != 0, vld cmd0: %08x\n", tmp);
		tmp = ((uint32_t)p_Vid->dec_pic->mbinfo_addr_phy);// + p_Vid->frame_size_mb/2;
		tmp += (mb_addr<<8);
		tmp |= 1;
		ptReg_io->CPUCMD1 = tmp;
//printk("mbinfo %08x\n", (uint32_t)p_Vid->dec_pic->mbinfo_addr_phy);
//printk("mb_cnt != 0, vld cmd1: %08x\n", tmp);
	}
	tmp = 0;
	while (!(((tmp&BIT2)>>2) & ((tmp&BIT3)>>3)))
		tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)

	ptReg_io->DEC_PARAM7 = (sys_idx_new<<20);	// idx1
//printk("vld parm7: %08x\n", (sys_idx_new<<20));
}
#endif


/*
 * To get first_ref_pic_l1_top and reflistL1_0_long for setting ptReg_io->DEC_PARAM4 (0x30)
 */
static int compute_col_top(VideoParams *p_Vid)
{
	StorablePicture *fs = p_Vid->refListLX[1][0];
	StorablePicture *sp_top = fs, *sp_btm = fs;

	if (p_Vid->MbaffFrameFlag){
		sp_top = p_Vid->refListLX[3][0];
		sp_btm = p_Vid->refListLX[5][0];
	}
	else {
		if (p_Vid->field_pic_flag){
			if ((p_Vid->structure != fs->structure) && (fs->coded_frame)){
				if (p_Vid->structure == TOP_FIELD)
					sp_top = sp_btm = p_Vid->refListLX[1][0]->sp_top;
				else
					sp_top = sp_btm = p_Vid->refListLX[1][0]->sp_btm;
			}
		}
	}
	if (sp_top == NULL || sp_top->valid == 0) {
	    LOG_PRINT(LOG_ERROR, "col top field invalid\n");
	    return H264D_PARSING_ERROR;
	}
	if (sp_btm == NULL || sp_btm->valid == 0) {
	    LOG_PRINT(LOG_ERROR, "col bottom field invalid\n");
	    return H264D_PARSING_ERROR;
	}
	p_Vid->first_ref_pic_l1_top = (iAbs(p_Vid->dec_pic->poc - sp_btm->poc) > iAbs(p_Vid->dec_pic->poc - sp_top->poc));

	if (fs->is_long_term)
		p_Vid->reflistL1_0_long = 1;
	else 
		p_Vid->reflistL1_0_long = 0;
    return H264D_OK;
}


#if ENABLE_VCACHE_SYSINFO
/*
 * from task rewind_sysinfo in vcache_task.v
 */
void rewind_sysinfo(DecoderParams *p_Dec, int first_mb_in_slice, int pic_width_in_mbs)
{
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)(p_Dec->pu32VcacheBaseAddr);
    uint32_t cnt0, cnt1;
    
#if USE_LINK_LIST
    /* NOTE 1: purpose: to avoid using the value from register as branch condition
     * NOTE 2: ptReg_vcache->RESERVED44[7:0] == (mbx + 2) % pic_width_in_mbs_minus1 == (mbx + 3) % pic_width_in_mbs
     * if ptReg_vcache->RESERVED44[7:0] == 0, then mbx must be (pic_width_in_mbs_minus1 - 2 == pic_width_in_mbs - 3)
     */
    uint32_t mbx;
    mbx = first_mb_in_slice % pic_width_in_mbs;
    if ((mbx == pic_width_in_mbs - 3)) 
    {
        cnt0 = pic_width_in_mbs - 1;
        cnt1 = pic_width_in_mbs - 1;
        AND_REG(p_Dec, ((MCP300_VC << 7) + 0x44), ptReg_vcache->RESERVED44, 0xFFF80F00);
        ORR_REG(p_Dec, ((MCP300_VC << 7) + 0x44), ptReg_vcache->RESERVED44, ((cnt0 & 0x7F) << 12) | (cnt1 & 0xFF));
    }
    else {
        SUB_REG(p_Dec, ((MCP300_VC << 7) + 0x44), ptReg_vcache->RESERVED44, 7, 12, 1); /* ptReg_vcache->RESERVED44 = (((ptReg_vcache->RESERVED44 >> 12) & 0x7F) - 1) << 12 */
        SUB_REG(p_Dec, ((MCP300_VC << 7) + 0x44), ptReg_vcache->RESERVED44, 8, 0, 1); /* ptReg_vcache->RESERVED44 = (ptReg_vcache->RESERVED44 & 0xFF) - 1 */
    }
#else
    uint32_t tmp;
    tmp = ptReg_vcache->RESERVED44;
    if ((tmp&0xFF) == 0) 
    {
        cnt0 = pic_width_in_mbs - 1;
        cnt1 = pic_width_in_mbs - 1;
    }
    else {
        cnt0 = ((tmp>>12)&0x7F) - 1;
        cnt1 = (tmp&0xFF) - 1;
    }
    ptReg_vcache->RESERVED44 = (tmp&0xFFF80F00) | ((cnt0&0x7F)<<12) | (cnt1&0xFF);
#endif
}
#endif


#if USE_LINK_LIST
/*
 * Start of functions for SW linked list command (SW_LL_FUNC_PARAM2) used in set_dec_param7_sw_ll
 * Why?: operations that can not be implemneted in HW linked command are wrapped into a function
 *       The function pointer to these functions and their parameters will be packed into SW linked 
 *       list commands (SW_LL_FUNC_PARAM2) and executed by CPU.
 */
void sw_ll_macro_cmd1(uint8_t *top_mb_field_flag_ptr, uint32_t base_addr)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io *)base_addr;
	volatile H264_reg_sys *ptReg_sys;
	uint32_t tmp;
    int idx;
    
    tmp = ptReg_io->DEC_PARAM7;
    idx = (((tmp>>13)&0x07) + 1) & 0x07;
    ptReg_sys = (H264_reg_sys*)((((uint32_t)base_addr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
    tmp = ptReg_sys->REG60;
    *top_mb_field_flag_ptr = ((tmp>>6) & BIT0);
}

void sw_ll_macro_cmd2(uint8_t *top_mb_field_flag_ptr, uint32_t base_addr)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io *)base_addr;
	volatile H264_reg_sys *ptReg_sys;
	uint32_t tmp;
    int idx;
    
    tmp = ptReg_io->DEC_PARAM7;
    idx = (((tmp>>13)&0x07) + 1) & 0x07;
    ptReg_sys = (H264_reg_sys*)((((uint32_t)base_addr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
    tmp = ptReg_sys->REG60;
    *top_mb_field_flag_ptr = ((tmp>>6) & BIT0);

    tmp = ptReg_io->DEC_PARAM7;
    idx = (((tmp>>13)&0x07) - 1) & 0x07;
    ptReg_sys = (H264_reg_sys*)((((uint32_t)base_addr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
    tmp = ptReg_sys->REG60;
    *top_mb_field_flag_ptr |= ((tmp>>6) & BIT0)<<1;
}

void sw_ll_macro_cmd3(volatile uint32_t *reg, uint8_t *top_mb_field_flag_ptr)
{
    *reg |= ((*top_mb_field_flag_ptr & 0x03) << 4);
}

void sw_ll_macro_cmd4(uint8_t *prev_mb_fieldflag_ptr, uint32_t base_addr)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io *)base_addr;
	volatile H264_reg_sys *ptReg_sys;
	uint32_t tmp;
    int idx;

    idx = (((ptReg_io->DEC_PARAM7>>16)&0x07) - 2)&0x07;
    ptReg_sys = (H264_reg_sys*)((((uint32_t)base_addr + H264_OFF_SYSM)&0xFFFF0000) + (idx<<8));
    tmp = ptReg_sys->REG60;
    *prev_mb_fieldflag_ptr = ((tmp>>6)&BIT0);
}

void sw_ll_macro_cmd5(volatile uint32_t *reg, uint8_t *prev_mb_fieldflag_ptr)
{
    *reg |= (((uint32_t)*prev_mb_fieldflag_ptr) << 31);
}

/*
 * End of functions for SW linked list command (SW_LL_FUNC_PARAM2) used in set_dec_param7_sw_ll
 */


/*
 * SW linked list version of set_dec_param7()
 */
static void set_dec_param7_sw_ll(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	VideoParams *p_Vid = p_Dec->p_Vid;
	int mb_cnt, dma_act, sys_idx_new;
    uint8_t top_mb_field_flag_nz_flg = 0;

    if (p_Vid->MbaffFrameFlag){
        mb_cnt = p_Vid->slice_hdr->currMbAddr - 2;
        // read top/top_left mb_fieldflag
        mb_cnt = mb_cnt - 2 * p_Vid->PicWidthInMbs;
        if (mb_cnt < 0){
// ============= vld sysinfo recover : add code =============
            if (mb_cnt == -6) {
                dma_act = 1;
                sys_idx_new = 0;
            }
            else if (mb_cnt == -4) {
                sys_idx_new = 2;
                dma_act = 1;                
            }
            else if (mb_cnt == -2) {    // first mb in second row
// ============= vld sysinfo recover : end code =============
                SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd1, &p_Vid->top_mb_field_flag, (uint32_t)p_Dec->pu32BaseAddr);
                top_mb_field_flag_nz_flg = 1;
                //sw_ll_macro_cmd1(&p_Vid->top_mb_field_flag, (uint32_t)p_Dec->pu32BaseAddr);
#if 0
                tmp = ptReg_io->DEC_PARAM7;
                idx = (((tmp>>13)&0x07) + 1) & 0x07;
                ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
                tmp = ptReg_sys->REG60;
                p_Vid->top_mb_field_flag = ((tmp>>6) & BIT0);
#endif
// ============= vld sysinfo recover : add code =============
                sys_idx_new = 4;
                dma_act = 1;
// ============= vld sysinfo recover : end code =============
            }
            else {
                p_Vid->top_mb_field_flag = 0;
// ============= vld sysinfo recover : add code =============
                sys_idx_new = 0;
                dma_act = 0;
// ============= vld sysinfo recover : end code =============
            }
        }
        else {
            SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd2, &p_Vid->top_mb_field_flag, (uint32_t)p_Dec->pu32BaseAddr);
            top_mb_field_flag_nz_flg = 1;
            //sw_ll_macro_cmd2(&p_Vid->top_mb_field_flag, (uint32_t)p_Dec->pu32BaseAddr);
#if 0
            tmp = ptReg_io->DEC_PARAM7;
            idx = (((tmp>>13)&0x07) + 1) & 0x07;
            ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
            tmp = ptReg_sys->REG60;
            p_Vid->top_mb_field_flag = ((tmp>>6) & BIT0);

            tmp = ptReg_io->DEC_PARAM7;
            idx = (((tmp>>13)&0x07) - 1) & 0x07;
            ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
            tmp = ptReg_sys->REG60;
            p_Vid->top_mb_field_flag |= ((tmp>>6) & BIT0)<<1;
#endif

// ============= vld sysinfo recover : add code =============
            sys_idx_new = 4;
            dma_act = 1;
// ============= vld sysinfo recover : end code =============
        }
    } else { // MbaffFrameFlag = 0
		mb_cnt = p_Vid->slice_hdr->currMbAddr - 1;
		p_Vid->top_mb_field_flag = 0;
		
		mb_cnt = mb_cnt - p_Vid->PicWidthInMbs;
		if (mb_cnt < 0){
            if (mb_cnt == -3) {
                dma_act = 1;
                sys_idx_new = 0;
            } else if (mb_cnt == -2) {
                sys_idx_new = 1;
                dma_act = 1;
            } else if (mb_cnt == -1) {	// first mb in second row
                sys_idx_new = 2;
                dma_act = 1;
			} else {
                sys_idx_new = 0;
                dma_act = 0;
			}
		}else {
		    sys_idx_new = 2;
            dma_act = 1;
		}
	}

#if ENABLE_VCACHE_SYSINFO
    if(p_Dec->vcache_en_flgs & VCACHE_SYSINFO_EN){
        if(dma_act){
            rewind_sysinfo(p_Dec, p_Vid->slice_hdr->currMbAddr, p_Vid->PicWidthInMbs);
        }
    }
#endif

#if USE_LINK_LIST
#if 0
    tmp1 = ptReg_io->DEC_PARAM7;
    //printk("set_dec_param7:%08X\n", tmp1);
    if (!dma_act) {
        dma_cnt = 0x00;
        //AND_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, ~(0xF << 20));
    } else {
        dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
        //SUB_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, 4, 20, 1);
    }
#if SLICE_DMA_DISABLE
	if (p_Vid->first_slice) {
	    mem_idx = 0x08000000 | ((dma_cnt & 0x0F) << 20);
	} else {
        mem_idx = (tmp1 & 0xFF0FE000) | ((dma_cnt & 0x0F) << 20);
    }
    tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#endif
#endif

#if 0
    ptReg_io->DEC_PARAM7 = 0;
    printk("1 %08X\n", ptReg_io->DEC_PARAM7);
    ptReg_io->DEC_PARAM7 = 0xFFFFFFFF;
    printk("2 %08X\n", ptReg_io->DEC_PARAM7);
    ptReg_io->DEC_PARAM7 = 0;
    printk("3 %08X\n", ptReg_io->DEC_PARAM7);
    while(1);
#endif

#if 1
    //printk("1 ptReg_io->DEC_PARAM7: %08X\n", tmp1);
    if (!dma_act) {
        //dma_cnt = 0x00;
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, ~(0xF << 20));
        //printk("2 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
    } else {
        //dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
        SUB_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 4, 20, 1);
        //printk("3 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
    }
	if (p_Vid->first_slice) {
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0x08F00000);
        //printk("4 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        //ORR_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        if(top_mb_field_flag_nz_flg){
#if SW_LINK_LIST
            SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd3, &ptReg_io->DEC_PARAM7, &p_Vid->top_mb_field_flag);
#else
            printk("ll do not support mbaff top_mb_field_flag:%d", p_Vid->top_mb_field_flag);
#endif
        }
        //sw_ll_macro_cmd3(&ptReg_io->DEC_PARAM7, &p_Vid->top_mb_field_flag);
        //printk("5 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0x08F81FFF); /* clear bit 28~31, 24~26, 13~18 */
	    //mem_idx = 0x08000000 | ((dma_cnt & 0x0F) << 20);
        //tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
	} else {
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0xFFFFE000);
        //printk("6 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        //ORR_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        if(top_mb_field_flag_nz_flg){
#if SW_LINK_LIST
            SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd3, &ptReg_io->DEC_PARAM7, &p_Vid->top_mb_field_flag);
#else
            printk("ll do not support mbaff top_mb_field_flag:%d", p_Vid->top_mb_field_flag);
#endif
        }
        //sw_ll_macro_cmd3(&ptReg_io->DEC_PARAM7, &p_Vid->top_mb_field_flag);
        //printk("7 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        //mem_idx = (tmp1 & 0xFF0FE000) | ((dma_cnt & 0x0F) << 20);
        //tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
    }
#endif

#if 0
    if(ptReg_io->DEC_PARAM7 != tmp){
        printk("ptReg_io->DEC_PARAM7: %08X %08X diff\n", ptReg_io->DEC_PARAM7, tmp);
        while(1);
    }
#endif
    //WRITE_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, tmp);

#else
    tmp1 = ptReg_io->DEC_PARAM7;
    if (!dma_act) {
        dma_cnt = 0x00;
    } else {
        dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
    }
        
	if (p_Vid->first_slice) {
	    mem_idx = 0x08000000 | ((dma_cnt&0x0F)<<20);
	} else {
        mem_idx = (tmp1&0xFF0FE000) | ((dma_cnt&0x0F)<<20);
    }

#if SLICE_DMA_DISABLE
    tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#else
    tmp = 0x08000000 | (sys_idx_new<<20) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#endif

    ptReg_io->DEC_PARAM7 = tmp;
#endif


}

/*
 * Hardware linked list version of set_dec_param7()
 */
static void set_dec_param7_hw_ll(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	VideoParams *p_Vid = p_Dec->p_Vid;
	//uint32_t tmp1, tmp, mem_idx, dma_cnt;
	int mb_cnt, dma_act, sys_idx_new;

	if (p_Vid->MbaffFrameFlag){
	    LOG_PRINT(LOG_ERROR, "mbaff is not supported in link list mode\n");
        panic("%s, mbaff!!\n", __func__);
    } else { // MbaffFrameFlag = 0
		mb_cnt = p_Vid->slice_hdr->currMbAddr - 1;
		p_Vid->top_mb_field_flag = 0;
		
		mb_cnt = mb_cnt - p_Vid->PicWidthInMbs;
		if (mb_cnt < 0){
            if (mb_cnt == -3) {
                dma_act = 1;
                sys_idx_new = 0;
            } else if (mb_cnt == -2) {
                sys_idx_new = 1;
                dma_act = 1;
            } else if (mb_cnt == -1) {	// first mb in second row
                sys_idx_new = 2;
                dma_act = 1;
			} else {
                sys_idx_new = 0;
                dma_act = 0;
			}
		}else {
		    sys_idx_new = 2;
            dma_act = 1;
		}
	}


#if ENABLE_VCACHE_SYSINFO
    if(p_Dec->vcache_en_flgs & VCACHE_SYSINFO_EN){
        if(dma_act){
            rewind_sysinfo(p_Dec, p_Vid->slice_hdr->currMbAddr, p_Vid->PicWidthInMbs);
        }
    }
#endif

#if USE_LINK_LIST
#if 0
    tmp1 = ptReg_io->DEC_PARAM7;
    //printk("set_dec_param7:%08X\n", tmp1);
    if (!dma_act) {
        dma_cnt = 0x00;
        //AND_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, ~(0xF << 20));
    } else {
        dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
        //SUB_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, 4, 20, 1);
    }
#if SLICE_DMA_DISABLE
	if (p_Vid->first_slice) {
	    mem_idx = 0x08000000 | ((dma_cnt & 0x0F) << 20);
	} else {
        mem_idx = (tmp1 & 0xFF0FE000) | ((dma_cnt & 0x0F) << 20);
    }
    tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#endif
#endif

#if 0
    ptReg_io->DEC_PARAM7 = 0;
    printk("1 %08X\n", ptReg_io->DEC_PARAM7);
    ptReg_io->DEC_PARAM7 = 0xFFFFFFFF;
    printk("2 %08X\n", ptReg_io->DEC_PARAM7);
    ptReg_io->DEC_PARAM7 = 0;
    printk("3 %08X\n", ptReg_io->DEC_PARAM7);
    while(1);
#endif

#if 1
    //printk("1 ptReg_io->DEC_PARAM7: %08X\n", tmp1);
    if (!dma_act) {
        //dma_cnt = 0x00;
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, ~(0xF << 20));
        //printk("2 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
    } else {
        //dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
        SUB_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 4, 20, 1);
        //printk("3 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
    }
	if (p_Vid->first_slice) {
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0x08F00000);
        //printk("4 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        //printk("5 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0x08F81FFF); /* clear bit 28~31, 24~26, 13~18 */
	    //mem_idx = 0x08000000 | ((dma_cnt & 0x0F) << 20);
        //tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
	} else {
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, 0xFFFFE000);
        //printk("6 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x3C), ptReg_io->DEC_PARAM7, (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        //printk("7 ptReg_io->DEC_PARAM7: %08X\n", ptReg_io->DEC_PARAM7);
        //mem_idx = (tmp1 & 0xFF0FE000) | ((dma_cnt & 0x0F) << 20);
        //tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
    }
#endif

#if 0
    if(ptReg_io->DEC_PARAM7 != tmp){
        printk("ptReg_io->DEC_PARAM7: %08X %08X diff\n", ptReg_io->DEC_PARAM7, tmp);
        while(1);
    }
#endif
    //WRITE_REG(p_Dec, 0, ptReg_io->DEC_PARAM7, tmp);

#else
    tmp1 = ptReg_io->DEC_PARAM7;
    if (!dma_act) {
        dma_cnt = 0x00;
    } else {
        dma_cnt = ((tmp1 >> 20) & 0x07) - 1;
    }
        
	if (p_Vid->first_slice) {
	    mem_idx = 0x08000000 | ((dma_cnt&0x0F)<<20);
	} else {
        mem_idx = (tmp1&0xFF0FE000) | ((dma_cnt&0x0F)<<20);
    }

#if SLICE_DMA_DISABLE
    tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#else
    tmp = 0x08000000 | (sys_idx_new<<20) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
#endif

    ptReg_io->DEC_PARAM7 = tmp;
#endif


}

#endif


#if USE_LINK_LIST == 0
/*
 * Setting ptReg_io->DEC_PARAM7 (offset 0x3C)
 */
static void set_dec_param7(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	volatile H264_reg_sys *ptReg_sys;
	VideoParams *p_Vid = p_Dec->p_Vid;
	uint32_t tmp1, tmp, mem_idx, dma_cnt;
	int mb_cnt, dma_act, sys_idx_new, idx;

	tmp1 = ptReg_io->DEC_PARAM7;
	if (p_Vid->MbaffFrameFlag){
		mb_cnt = p_Vid->slice_hdr->currMbAddr - 2;
		/*
		if (mb_cnt<0){
			p_Vid->left_mb_cbp[0] = p_Vid->left_mb_cbp[1] = 0;
			p_Vid->left_mb_intra[0] = p_Vid->left_mb_intra[1] = 0;
		}
		else {
			// left_top_mb
			ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_LEFTTOP_MB);
			tmp = ptReg_sys->REG60;
			p_Vid->left_mb_intra[0] = ((tmp>>14) & BIT0);
			if ((tmp>>15) & BIT0)
				p_Vid->left_mb_cbp[0] = CONCAT_4_1BIT_VALUES((tmp>>11), (tmp>>11), (tmp>>9), (tmp>>9));
			else {
				tmp = ptReg_sys->REG1C;
				p_Vid->left_mb_cbp[0] = CONCAT_4_1BIT_VALUES((tmp>>3), (tmp>>7), (tmp>>23), (tmp>>15));
			}
			// left_bottom_mb
			ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_LEFTBOT_MB);
			tmp = ptReg_sys->REG60;
			p_Vid->left_mb_intra[1] = (tmp>>14) & BIT0;
			if ((tmp>>15) & BIT0)
				p_Vid->left_mb_cbp[1] = CONCAT_4_1BIT_VALUES((tmp>>11), (tmp>>11), (tmp>>9), (tmp>>9));
			else {
				tmp = ptReg_sys->REG1C;
				p_Vid->left_mb_cbp[1] = CONCAT_4_1BIT_VALUES((tmp>>3), (tmp>>7), (tmp>>23), (tmp>>15));
			}
		}
		*/
		// read top/top_left mb_fieldflag
		mb_cnt = mb_cnt - 2*p_Vid->PicWidthInMbs;
//printk("mbaff: %d\n", mb_cnt);
		if (mb_cnt<0){
// ============= vld sysinfo recover : add code =============
            if (mb_cnt == -6) {
                dma_act = 1;
                sys_idx_new = 0;
            }
            else if (mb_cnt == -4) {
                sys_idx_new = 2;
                dma_act = 1;                
            }
            else if (mb_cnt == -2) {	// first mb in second row
// ============= vld sysinfo recover : end code =============
			//if (mb_cnt == -2){	// first mb in second row
			#if SLICE_DMA_DISABLE
			    tmp = ptReg_io->DEC_PARAM7;
			    idx = (((tmp>>13)&0x07) + 1)&0x07;
			#else
			    idx = 1;
			#endif
			    ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
				//ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_FIELDFLAG);
			    //printk("parm7 sys mbaff: %08x (%08x): %08x\n", (uint32_t)(&ptReg_sys->REG60), (uint32_t)p_Dec->pu32BaseAddr, ptReg_sys->REG60);
				tmp = ptReg_sys->REG60;
				p_Vid->top_mb_field_flag = ((tmp>>6) & BIT0);
// ============= vld sysinfo recover : add code =============
                sys_idx_new = 4;
                dma_act = 1;
// ============= vld sysinfo recover : end code =============
			}
			else {
				p_Vid->top_mb_field_flag = 0;
// ============= vld sysinfo recover : add code =============
                sys_idx_new = 0;
                dma_act = 0;
// ============= vld sysinfo recover : end code =============
			}
		}
		else {
        #if SLICE_DMA_DISABLE
		    tmp = ptReg_io->DEC_PARAM7;
		    idx = (((tmp>>13)&0x07) + 1)&0x07;
		#else
		    idx = 1;
		#endif
		    ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
			//ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOPLEFT_FIELDFLAG);
			//printk("parm7 sys0: %08x (%08x): %08x\n", (uint32_t)(&ptReg_sys->REG60), (uint32_t)p_Dec->pu32BaseAddr, ptReg_sys->REG60);
			tmp = ptReg_sys->REG60;
			//p_Vid->top_mb_field_flag = ((tmp>>6) & BIT0)<<1;
			p_Vid->top_mb_field_flag = ((tmp>>6) & BIT0);

        #if SLICE_DMA_DISABLE
		    tmp = ptReg_io->DEC_PARAM7;
		    idx = (((tmp>>13)&0x07) - 1)&0x07;
		#else
		    idx = 7;
		#endif
		    ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_MB + (idx<<7));
			//ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_TOP_FIELDFLAG);
			//printk("parm7 sys1: %08x (%08x): %08x\n", (uint32_t)(&ptReg_sys->REG60), (uint32_t)p_Dec->pu32BaseAddr, ptReg_sys->REG60);
			tmp = ptReg_sys->REG60;
			//p_Vid->top_mb_field_flag |= ((tmp>>6) & BIT0);
			p_Vid->top_mb_field_flag |= ((tmp>>6) & BIT0)<<1;

// ============= vld sysinfo recover : add code =============
            sys_idx_new = 4;
            dma_act = 1;
// ============= vld sysinfo recover : end code =============
		}
	}
	else { // MbaffFrameFlag = 0
		mb_cnt = p_Vid->slice_hdr->currMbAddr - 1;
		/*
		if (mb_cnt<0){
			p_Vid->left_mb_cbp[0] = p_Vid->left_mb_cbp[1] = 0;
			p_Vid->left_mb_intra[0] = p_Vid->left_mb_intra[1] = 0;
		}
		else {
			ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_LEFT_MB);
			tmp = ptReg_sys->REG60;
			p_Vid->left_mb_intra[0] = (tmp>>14) & BIT0;
			p_Vid->left_mb_intra[1] = 0;
			if ((tmp>>15) & BIT0)
				p_Vid->left_mb_cbp[0] = CONCAT_4_1BIT_VALUES((tmp>>11), (tmp>>11), (tmp>>9), (tmp>>9));
			else {
				tmp = ptReg_sys->REG1C;
				p_Vid->left_mb_cbp[0] = CONCAT_4_1BIT_VALUES((tmp>>3), (tmp>>7), (tmp>>23), (tmp>>15));
			}
			p_Vid->left_mb_cbp[1] = 0;
		}
		*/
		p_Vid->top_mb_field_flag = 0;
		
// ============= vld sysinfo recover : add code =============
		//mb_cnt = mb_cnt - 2*p_Vid->PicWidthInMbs;
		mb_cnt = mb_cnt - p_Vid->PicWidthInMbs;
//printk("mb_cnt = %d\n", mb_cnt);
		if (mb_cnt<0){
            if (mb_cnt == -3) {
                dma_act = 1;
                sys_idx_new = 0;
            }
            else if (mb_cnt == -2) {
                sys_idx_new = 1;
                dma_act = 1;
                
            }
            else if (mb_cnt == -1) {	// first mb in second row
                sys_idx_new = 2;
                dma_act = 1;
			}
			else {
                sys_idx_new = 0;
                dma_act = 0;
			}
		}
		else {
		    sys_idx_new = 2;
            dma_act = 1;
		}
// ============= vld sysinfo recover : end code =============
	}

//printk("mbaff %d, first %d, dma act = %d, mb_cnt = %d\n", p_Vid->MbaffFrameFlag, p_Vid->first_slice, dma_act, mb_cnt);

	if (!dma_act)
	    dma_cnt = 0x00;
    else {
        dma_cnt = ((tmp1>>20)&0x07) - 1;
#if ENABLE_VCACHE_SYSINFO
        if(p_Dec->vcache_en_flgs & VCACHE_SYSINFO_EN){
            rewind_sysinfo(p_Dec, p_Vid->slice_hdr->currMbAddr, p_Vid->PicWidthInMbs);
        }
#endif
    }
        
	if (p_Vid->first_slice) {
	    mem_idx = 0x08000000 | ((dma_cnt&0x0F)<<20);
    /*
    #if SLICE_DMA_DISABLE
        printk("dis");
    #else
        printk("en");
    #endif
    #if SLICE_WRITE_BACK
        printk("wb");
    #endif
    */
	}
    else
        mem_idx = (tmp1&0xFF0FE000) | ((dma_cnt&0x0F)<<20);

#if 0
	tmp = ((tmp1&0xFFFF0000) | ((p_Vid->left_mb_cbp[1]&0x0F)<<12) | ((p_Vid->left_mb_cbp[0]&0x0F)<<8) | 
		((p_Vid->left_mb_intra[1]&0x01)<<7) | ((p_Vid->left_mb_intra[0]&0x01)<<6) | ((p_Vid->top_mb_field_flag&0x03)<<4) |
		((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
#else

#if SLICE_DMA_DISABLE
    tmp = mem_idx | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
//printk("parm7: %08x -> %08x\n", tmp1, tmp);
#else
//printk("sys idx = %d\n", sys_idx_new);
    tmp = 0x08000000 | (sys_idx_new<<20) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1);
//printk("parm7: %08x -> %08x\n", tmp1, tmp);
#endif
    //printk("%08x -> %08x\n", tmp1, tmp);
/*
    #if ILF_RECOVER_ENABLE
        tmp = (0x08000000 | (tmp1&0x00FF0000) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) |((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
//printk("set parm7: %08x\n", tmp);
    #else
        if (p_Vid->first_slice)
            tmp = (0x08000000 | (tmp1&0x00FF0000) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) |((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
        else
            tmp = ((tmp1&0xFFFF0000) | ((p_Vid->top_mb_field_flag&0x03)<<4) | (SLICE_WRITE_BACK<<3) | ((p_Vid->slice_hdr->cabac_init_idc&0x03)<<1));
//printk("set parm7: %08x\n", tmp);
    #endif
*/

#endif
    ptReg_io->DEC_PARAM7 = tmp;

	//return tmp;
}
#endif


#if !(SLICE_DMA_DISABLE)
/*
 * from task ilf_sysinfo_recover in h264d_vld_task.v
 */
static void ilf_sysinfo_recover(DecoderParams *p_Dec)
{
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	VideoParams *p_Vid = p_Dec->p_Vid;
	uint32_t tmp, tmp1, mb_cnt, mb_x, mb_y;
	int mb_addr;

	if (p_Vid->slice_hdr->disable_deblocking_filter_idc==0){
        /* ilf enabled */
		if (p_Vid->MbaffFrameFlag){
			mb_addr = p_Vid->slice_hdr->currMbAddr - 2;
			mb_cnt = 2;
			mb_x = (mb_addr>>1)%p_Vid->PicWidthInMbs;
			mb_y = (mb_addr>>1)/p_Vid->PicWidthInMbs;
			mb_y = mb_y<<1;
		}
		else if (p_Vid->field_pic_flag){
			mb_addr = p_Vid->slice_hdr->currMbAddr - 1;
			mb_cnt = 2;
			mb_x = mb_addr%p_Vid->PicWidthInMbs;
			mb_y = mb_addr/p_Vid->PicWidthInMbs;
			mb_y = mb_y<<1;
		}
		else {
			mb_addr = p_Vid->slice_hdr->currMbAddr - 1;
			mb_cnt = 1;
			mb_x = mb_addr%p_Vid->PicWidthInMbs;
			mb_y = mb_addr/p_Vid->PicWidthInMbs;
		}
        
		/* NOTE: For muliti-slice only. 
		 *       In one slice case, currMbAddr == 0 --> mb_addr < 0 */
        
		if (mb_addr >= 0 && mb_x != p_Vid->active_sps->pic_width_in_mbs_minus1){
			tmp1 = ptReg_io->DEC_PARAM7;
			tmp = 0;

			while (!(((tmp&BIT2)>>2) & ((tmp&BIT3)>>3))) /* polling one */
				tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)

			ptReg_io->DEC_PARAM7 = tmp1 | 1;	// open mixer clock

			tmp = ((mb_cnt<<5)<<10) | (2<<4) | ((p_Vid->PicWidthInMbs<<2)<<19);
			ptReg_io->CPUCMD0 = tmp;
//printk("ilf cmd0: %08x\n", tmp);
			tmp = (uint32_t)p_Vid->dec_pic->refpic_addr_phy;
			tmp = tmp + mb_y * (p_Vid->PicWidthInMbs << 9);	// (Width<<4)<<5 (32 dowrd/per-mb)
			tmp = tmp + (mb_x << 5) + 16;
			tmp = tmp | 1;
			ptReg_io->CPUCMD1 = tmp;
//printk("ref %08x\n", (uint32_t)p_Vid->dec_pic->refpic_addr_phy);
//printk("ilf cmd1: %08x\n", tmp);
			tmp = 0;
			while (!(((tmp&BIT2)>>2) & ((tmp&BIT3)>>3))) /* polling one */
				tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)

			ptReg_io->DEC_CMD0 = 2;	// ilf mb-demix go

			tmp = 0;
			while (!((tmp&BIT1)>>1)) /* polling one */
				tmp = ptReg_io->DEC_STS0;		// check dma idle (0x50)

//printk("ilf parm7: %08x\n", tmp1);
			ptReg_io->DEC_PARAM7 = tmp1;
		}
	}
}
#endif

/*
 * Setting parameter registers for decoding current slice
 */
int decode_slice(DecoderParams *p_Dec)
{
	VideoParams *p_Vid = p_Dec->p_Vid;
	SeqParameterSet *sps = p_Vid->active_sps;
	PicParameterSet *pps = p_Vid->active_pps;
	SliceHeader *sh = p_Vid->slice_hdr;
	volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
	//volatile H264_reg_sys *ptReg_sys4;
	uint32_t pos_x, pos_y, height;
	int8_t l0_size_m1, l1_size_m1;
#if USE_LINK_LIST == 0
	volatile H264_reg_sys *ptReg_sys;// = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + H264_OFF_SYSM_LEFTTOP_MB);
	//byte prev_mb_fieldflag = 0;
	int idx;
    uint32_t tmp;
#else
    uint8_t prev_mb_fieldflag_nz_flg = 0;
#endif
	uint32_t tmp_reg_value;//, gld_reg_value;
	
	//if (pps->entropy_coding_mode_flag)
	//	init_ctx_table(p_Dec, p_Vid);

    p_Vid->prev_mb_fieldflag = 0;

	p_Vid->first_ref_pic_l1_top = 0;
	p_Vid->reflistL1_0_long = 0;

	p_Dec->bScaleEnabled = 0; /* set a default value in case error return. It will be overwrite latter in noraml path */

	if (sh->slice_type == B_SLICE) {
		if (compute_col_top(p_Vid) < 0)
		    return H264D_PARSING_ERROR;
	}

#if !(SLICE_DMA_DISABLE)
	vld_sysinfo_recover(p_Dec);
#endif

	if (sps->frame_mbs_only_flag | p_Vid->field_pic_flag)
		height = sps->pic_height_in_map_units_minus1;
	else
		height = sps->pic_height_in_map_units_minus1*2 + 1;

//    tmp_reg_value = (SHIFT_1(p_Vid->nalu.is_long_start_code, 22) | SHIFT_1(sps->seq_scaling_matrix_present_flag, 21)
    tmp_reg_value = (SHIFT_1(sps->seq_scaling_matrix_present_flag, 21)
        | SHIFT(sps->chroma_format_idc, 2, 19) | SHIFT_1(sps->frame_mbs_only_flag, 18)
        | SHIFT_1(sps->mb_adaptive_frame_field_flag, 17) | SHIFT_1(sps->direct_8x8_inference_flag, 16)
        | SHIFT(height, 8, 8) | SHIFT(sps->pic_width_in_mbs_minus1, 8, 0));
    if(!p_Dec->bHasMBinfo){
        tmp_reg_value |= 1 << 23; // no_bframe = 1
    }
    //ptReg_io->DEC_PARAM0 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x20), ptReg_io->DEC_PARAM0, tmp_reg_value);

{
    extern int blk_read_dis;
    //tmp_reg_value = (SHIFT_1(1, 21) | SHIFT_1(0x01, 20) | SHIFT(pps->weighted_bipred_idc, 2, 14)
    tmp_reg_value = (SHIFT_1(blk_read_dis, 21) | SHIFT_1(0x01, 20) | SHIFT(pps->weighted_bipred_idc, 2, 14)
        | SHIFT_1(pps->constrained_intra_pred_flag, 13) | SHIFT_1(pps->pic_scaling_matrix_present_flag, 12)
        | SHIFT(pps->second_chroma_qp_index_offset, 5, 7) | SHIFT(pps->chroma_qp_index_offset, 5, 2)
        | SHIFT_1(pps->entropy_coding_mode_flag, 1) | SHIFT_1(pps->transform_8x8_mode_flag, 0));
}
    //ptReg_io->DEC_PARAM1 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x24), ptReg_io->DEC_PARAM1, tmp_reg_value);

    LOG_PRINT(LOG_INFO, "bScaleEnable:%d u16FrameWidthCropped:%d u16ScaleYuvWidthThrd:%d\n", 
        p_Dec->bScaleEnable, p_Dec->u16FrameWidthCropped, p_Dec->u16ScaleYuvWidthThrd);

    if(p_Dec->bChromaInterlace == 3){ /* set chroma interlace as HW required */
        if (p_Vid->structure == FRAME){
            AND_REG(p_Dec, ((MCP300_REG << 7) + 0x44), ptReg_io->DEC_CMD1, ~0x60);
        } else if ((p_Vid->structure == TOP_FIELD || p_Vid->structure == BOTTOM_FIELD)) {
            ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x44), ptReg_io->DEC_CMD1, 0x60);
        }
    }else if(p_Dec->bChromaInterlace == 1){
        ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x44), ptReg_io->DEC_CMD1, 0x60);
    }else if(p_Dec->bChromaInterlace == 0){
        AND_REG(p_Dec, ((MCP300_REG << 7) + 0x44), ptReg_io->DEC_CMD1, ~0x60);
    }else{
        LOG_PRINT(LOG_ERROR, "invalid bChromaInterlace: %d\n", p_Dec->bChromaInterlace);
    }

    /* set scale options */
    if (p_Dec->bScaleEnable && (p_Dec->u16FrameWidthCropped > p_Dec->u16ScaleYuvWidthThrd)) {
        // check: frame => chroma interlace = 0, field => chroma interlace = 1
        // scaling reconstructed picture does not support Mbaff & multi-slice
        if (p_Vid->MbaffFrameFlag || sh->first_mb_in_slice != 0) {
            LOG_PRINT(LOG_WARNING, "not support scaled reconstructed picture when mbaff enable or multi-slice (%d,%d)\n", p_Vid->MbaffFrameFlag, sh->first_mb_in_slice);
        } else if(p_Vid->pRecBuffer->dec_scale_buf_phy == NULL) {
            LOG_PRINT(LOG_WARNING, "disable scaled reconstructed picture due to null address\n");
        } else {
            if(p_Dec->bChromaInterlace == 3){ /* set chroma interlace as HW required */
                ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x24), ptReg_io->DEC_PARAM1, (SHIFT(p_Dec->u8ScaleSize, 2, 18) | SHIFT_1(p_Dec->u8ScaleType, 17) | SHIFT_1(0x01, 16)));
                p_Dec->bScaleEnabled = 1;
            }else{
                if (p_Vid->structure == FRAME && p_Dec->bChromaInterlace == 1){
                    LOG_PRINT(LOG_ERROR, "for scaled frame, chroma interlace must be 0\n");
                } else if ((p_Vid->structure == TOP_FIELD || p_Vid->structure == BOTTOM_FIELD) && !p_Dec->bChromaInterlace) {
                    LOG_PRINT(LOG_ERROR, "for scaled field, chroma interlace must be 1\n");
                } else {
                    //ptReg_io->DEC_PARAM1 |= (SHIFT(p_Dec->u8ScaleSize, 2, 18) | SHIFT_1(p_Dec->u8ScaleType, 17) | SHIFT_1(0x01, 16));
                    ORR_REG(p_Dec, ((MCP300_REG << 7) + 0x24), ptReg_io->DEC_PARAM1, (SHIFT(p_Dec->u8ScaleSize, 2, 18) | SHIFT_1(p_Dec->u8ScaleType, 17) | SHIFT_1(0x01, 16)));
    				p_Dec->bScaleEnabled = 1;
                }
            }
        }
    }

    tmp_reg_value = (SHIFT(p_Vid->QPv, 6, 12) | SHIFT(p_Vid->QPu, 6, 6) | SHIFT(sh->SliceQPy, 6, 0));
    //ptReg_io->DEC_PARAM3 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x2C), ptReg_io->DEC_PARAM3, tmp_reg_value);
    

	pos_x = sh->first_mb_in_slice % p_Vid->PicWidthInMbs;
	if (p_Vid->MbaffFrameFlag && (sh->first_mb_in_slice!=0)){
		if (pos_x != 0){
#if USE_LINK_LIST
            //sw_ll_macro_cmd4(&p_Vid->prev_mb_fieldflag, (uint32_t)ptReg_io);
            SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd4, &p_Vid->prev_mb_fieldflag, (uint32_t)ptReg_io);
            prev_mb_fieldflag_nz_flg = 1;
#else
        #if SLICE_DMA_DISABLE
            idx = (((ptReg_io->DEC_PARAM7>>16)&0x07) - 2)&0x07;
        #else
            idx = 0x06;
        #endif
            ptReg_sys = (H264_reg_sys*)((((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM)&0xFFFF0000) + (idx<<8));
            //printk("sys %08x (%08x): %08x\n", (uint32_t)(&ptReg_sys->REG60), (uint32_t)p_Dec->pu32BaseAddr, ptReg_sys->REG60);
            tmp = ptReg_sys->REG60;
			//tmp = ptReg_sys->REG60;
			p_Vid->prev_mb_fieldflag = ((tmp>>6)&BIT0);
#endif
		}
		else
			p_Vid->prev_mb_fieldflag = 0;
	}
    l0_size_m1 = iMax(0, p_Vid->list_size[0]-1);
	l1_size_m1 = iMax(0, p_Vid->list_size[1]-1);

#if USE_LINK_LIST
    tmp_reg_value = (SHIFT_1(p_Vid->bottom_field_first, 30)
#ifdef RTL_SIMULATION
        | SHIFT_1(p_Vid->first_slice, 29)
#endif
        | SHIFT_1(1, 27) | SHIFT(l1_size_m1, 5, 22) | SHIFT(l0_size_m1, 5, 17) 
        | SHIFT(sh->disable_deblocking_filter_idc, 2, 15) | SHIFT_1(sh->direct_spatial_mv_pred_flag, 14) 
        | SHIFT_1(sh->bottom_field_flag, 13) | SHIFT_1(p_Vid->first_ref_pic_l1_top, 12) | SHIFT_1(p_Vid->reflistL1_0_long, 11)
        | SHIFT_1(p_Vid->col_fieldflag, 9) | SHIFT_1(p_Vid->field_pic_flag, 8) | SHIFT(sh->SliceQPy, 6, 2) 
        | SHIFT(sh->slice_type, 2, 0));

    //ptReg_io->DEC_PARAM4 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x30), ptReg_io->DEC_PARAM4, tmp_reg_value);
    //sw_ll_macro_cmd5(&ptReg_io->DEC_PARAM4, &p_Vid->prev_mb_fieldflag); 
    if(prev_mb_fieldflag_nz_flg){
#if SW_LINK_LIST
        SW_LL_FUNC_PARAM2(p_Dec, sw_ll_macro_cmd5, &ptReg_io->DEC_PARAM4, &p_Vid->prev_mb_fieldflag);
#else
        printk("linked list do not support mb field: %d\n", p_Vid->prev_mb_fieldflag);
#endif
    }

#else // ! USE_LINK_LIST
    tmp_reg_value = (SHIFT_1(p_Vid->prev_mb_fieldflag, 31) | SHIFT_1(p_Vid->bottom_field_first, 30)
#ifdef RTL_SIMULATION
        | SHIFT_1(p_Vid->first_slice, 29)
#endif
        | SHIFT_1(1, 27) | SHIFT(l1_size_m1, 5, 22) | SHIFT(l0_size_m1, 5, 17) 
        | SHIFT(sh->disable_deblocking_filter_idc, 2, 15) | SHIFT_1(sh->direct_spatial_mv_pred_flag, 14) 
        | SHIFT_1(sh->bottom_field_flag, 13) | SHIFT_1(p_Vid->first_ref_pic_l1_top, 12) | SHIFT_1(p_Vid->reflistL1_0_long, 11)
        | SHIFT_1(p_Vid->col_fieldflag, 9) | SHIFT_1(p_Vid->field_pic_flag, 8) | SHIFT(sh->SliceQPy, 6, 2) 
        | SHIFT(sh->slice_type, 2, 0));

    //ptReg_io->DEC_PARAM4 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x30), ptReg_io->DEC_PARAM4, tmp_reg_value);
#endif
	tmp_reg_value = (SHIFT((p_Vid->PicSizeInMbs - pps->entropy_coding_mode_flag), 16, 16) | SHIFT(sh->currMbAddr, 16, 0));
	//ptReg_io->DEC_PARAM5 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x34), ptReg_io->DEC_PARAM5, tmp_reg_value);


	pos_x <<= p_Vid->MbaffFrameFlag;
	pos_y = sh->first_mb_in_slice / p_Vid->PicWidthInMbs;
	if (p_Vid->col_pic != NULL) {
        tmp_reg_value = (SHIFT_1(p_Vid->col_pic->bottom_field_first, 31) | SHIFT(sh->chroma_log2_weight_denom, 3, 28)
            | SHIFT(sh->luma_log2_weight_denom, 3, 25) | SHIFT(sh->slice_beta_offset_div2, 4, 21) 
            | SHIFT(sh->slice_alpha_c0_offset_div2, 4, 17) | SHIFT(pos_y, 8, 9) | SHIFT(pos_x, 9, 0));
    }
	else {
        tmp_reg_value = (SHIFT_1(0, 31) | SHIFT(sh->chroma_log2_weight_denom, 3, 28)
            | SHIFT(sh->luma_log2_weight_denom, 3, 25) | SHIFT(sh->slice_beta_offset_div2, 4, 21) 
            | SHIFT(sh->slice_alpha_c0_offset_div2, 4, 17) | SHIFT(pos_y, 8, 9) | SHIFT(pos_x, 9, 0));
	}
    //ptReg_io->DEC_PARAM6 = tmp_reg_value;
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x38), ptReg_io->DEC_PARAM6, tmp_reg_value);

    // move ilf_sysinfo_recover before set param7
	if (pos_x != 0) {
	    p_Dec->multislice = 0x10;
    #if !(SLICE_DMA_DISABLE)
	    ilf_sysinfo_recover(p_Dec);
	    //printk("i");
    #endif
	}

#if USE_LINK_LIST
    //set_dec_param7_hw_ll(p_Dec);
    set_dec_param7_sw_ll(p_Dec);
#else
	set_dec_param7(p_Dec);
#endif

	if (!p_Vid->field_pic_flag) {
        tmp_reg_value = (SHIFT(p_Vid->frame_num, 16, 16) | SHIFT(p_Vid->toppoc, 16, 0));
    } else {
        tmp_reg_value = (SHIFT(p_Vid->frame_num, 16, 16) | SHIFT(p_Vid->ThisPOC, 16, 0));
    }
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x78), ptReg_io->DEC_PARM30, tmp_reg_value);

    tmp_reg_value = (SHIFT(p_Vid->bottompoc, 16, 16) | SHIFT(p_Vid->col_index, 8, 0));
    //ptReg_io->DEC_PARM31 = tmp_reg_value;
    WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x7C), ptReg_io->DEC_PARM31, tmp_reg_value);
	
    DUMP_MSG(D_PARSE, p_Dec, "first mb %d (structure %d)\n", sh->first_mb_in_slice, p_Vid->structure);
    dump_param_register(p_Dec, (uint32_t)p_Dec->pu32BaseAddr, LOG_TRACE);

    return H264D_OK;
}

