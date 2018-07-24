#define ENABLE_DEFINE_CHECK /* to enable define checking in define.h. NOTE: must be somewhere before define.h is included */

#ifdef LINUX
    #include <linux/kernel.h>
    #include <linux/module.h>
    #include <linux/sched.h>
    #include <linux/delay.h>
    #include <linux/init.h>
    #include <linux/string.h>
#else
    #include <string.h>
    #include <stdio.h>
#endif
#include "global.h"
#include "params.h"
#include "portab.h"
#include "h264_reg.h"
#include "quant.h"
#include "refbuffer.h"
#if USE_SW_PARSER
#include "bitstream.h"
#endif /* USE_SW_PARSER */
#include "vlc.h"
#include "tlu.h"
#include "H264V_dec.h"

#include "define.h"


/* constant value */
static const char slice_type_name[3] = {'P', 'B', 'I'};
static const char structure_name[3][10] = {"FRAME", "TOP", "BOTTOM"};

static void return_release_list(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid);
static void return_output_list(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid);
static void return_output_frame_info(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid);


#if ENTER_DBG_MODE_AT_ERROR
#define __SET_DBG_MODE(val) do{ \
    extern int dbg_mode;        \
    dbg_mode = val;             \
}while(0)
#else
#define __SET_DBG_MODE(val) 
#endif

void __dump_mem(void *va, unsigned int len)
{
    int i;
    unsigned char *ptr = (unsigned char *)va;
    
    for(i = 0; i < len; i++){
        if((i % 16 == 0))
            printk("[%04X]", i);
        printk(" %02X", ptr[i]);
        if((i % 16 == 15))
            printk("\n");
    }
    if((i % 16 != 0))
        printk("\n");
}


/*
 * Print out the current bitstream loaded into SRAM
 */
void decoder_print_current_bitstream(unsigned int reg_base_addr, unsigned int len)
{
    int i;
    unsigned char *ptr = (unsigned char *)(reg_base_addr + TLU_BS_OFFSET);

    if(len > 256){
        len = 256;
    }
    
    for(i = 0; i < len; i+=4){
        if((i % 16 == 0)){
            LOG_PRINT(LOG_ERROR, "[%04X]", i);
        }
        
        /* reverse the byte order within each 4-byte word
           to make them looks the same as data in external memory */
        LOG_PRINT_NPFX(LOG_ERROR, " %02X %02X %02X %02X", ptr[i+3], ptr[i+2], ptr[i+1], ptr[i]);
        if((i % 16 == 12))
            LOG_PRINT_NPFX(LOG_ERROR, "\n");
    }
    
    if((i % 16 != 0))
        LOG_PRINT_NPFX(LOG_ERROR, "\n");
}


// ============================================================================
//   function of dump information
// ============================================================================
void dump_list(DecoderParams *p_Dec, int dump_mem)
{
    int i;
    VideoParams *p_Vid = p_Dec->p_Vid;

    printk("l0(%d): ", p_Vid->list_size[0]);
    for (i = 0; i<p_Vid->list_size[0]; i++) {
        if (p_Vid->refListLX[0][i] == NULL || p_Vid->refListLX[0][i]->valid == 0)
            printk("l0[%d] NULL ", i);
        else {
            printk("\tl0[%d]: %d[%s] ", i, p_Vid->refListLX[0][i]->poc, structure_name[p_Vid->refListLX[0][i]->structure]);
            if (dump_mem)
                printk("(%08x)\n", (uint32_t)p_Vid->refListLX[0][i]->refpic_addr_phy);
            else
                printk("\n");                
        }
    }
    printk("\n");
    
    printk("l1(%d): ", p_Vid->list_size[1]);
    for (i = 0; i<p_Vid->list_size[1]; i++) {
        if (p_Vid->refListLX[1][i] == NULL)
            printk("l1[%d] NULL ", i);
        else {
            printk("\tl1[%d]: %d[%s] ", i, p_Vid->refListLX[1][i]->poc, structure_name[p_Vid->refListLX[1][i]->structure]);
            if (dump_mem)
                printk("(%08x)\n", (uint32_t)p_Vid->refListLX[1][i]->refpic_addr_phy);
            else
                printk("\n"); 
        }
    }
    printk("\n");
/*
    {
        int k;
        for (k = 2; k<6; k++) {
            if (p_Vid->list_size[k] > 0) {
                printk("l%d(%d): ", k, p_Vid->list_size[k]);
                for (i = 0; i<p_Vid->list_size[k]; i++) {
                    if (p_Vid->refListLX[k][i] == NULL)
                        printk("l%d[%d] NULL ", k, i);
                    else
                        printk("%d[%d] ", p_Vid->refListLX[k][i]->poc, p_Vid->refListLX[k][i]->structure);
                }
                printk("\n");
            }
        }
    }
*/
}


//#define LOG_DPB LOG_INFO
#define LOG_DPB LOG_ERROR /* for tracing error */
void dump_dpb_info(void *ptVid, int mem_info)
{
    VideoParams *p_Vid = (VideoParams *)ptVid;
    DecodedPictureBuffer *listD;
    StorablePicture *sp;
    int i;
    LOG_PRINT(LOG_DPB, "dump_dpb_info enter\n");

    // null pointer checking
    if(!ptVid){
        LOG_PRINT(LOG_DPB, "null p_Vid\n");
        return;
    }
    listD = p_Vid->listD;
    if(!listD){
        LOG_PRINT(LOG_DPB, "null listD\n");
        return;
    }
    
    if(!p_Vid->active_sps){
        LOG_PRINT(LOG_DPB, "null active_sps\n");
        return;
    }

    LOG_PRINT(LOG_DPB, "dpb size %d (ref frame = %d, st:%d + lt:%d)\n", listD->used_size, p_Vid->active_sps->num_ref_frames, listD->st_ref_size, listD->lt_ref_size);
    for (i = 0; i < listD->used_size; i++) {
        if (listD->fs[i]->is_used == 3)
            sp = listD->fs[i]->frame;
        else if (listD->fs[i]->is_used == 1)
            sp = listD->fs[i]->topField;
        else if (listD->fs[i]->is_used == 2)
            sp = listD->fs[i]->btmField;
        else {
            LOG_PRINT_NPFX(LOG_DPB, "[%d]error picture structure %d\n", i, listD->fs[i]->is_used);
            continue;
        }
        if (mem_info)
            LOG_PRINT_NPFX(LOG_DPB, "poc %d (idx %d): rec %x, mbinfo %x\n", sp->poc, listD->fs[i]->phy_index, (uint32_t)sp->refpic_addr_phy, (uint32_t)sp->mbinfo_addr_phy);
        else {
            LOG_PRINT_NPFX(LOG_DPB, "{ poc %d (idx %d[us %d]) ", sp->poc, listD->fs[i]->phy_index, listD->fs[i]->is_used);
            if (listD->fs[i]->is_long)
                LOG_PRINT_NPFX(LOG_DPB, "lt_ref%d(%d) ", listD->fs[i]->is_long, listD->fs[i]->is_ref);
            else
                LOG_PRINT_NPFX(LOG_DPB, "st_ref%d ", listD->fs[i]->is_ref);
            LOG_PRINT_NPFX(LOG_DPB, "rout %d ", listD->fs[i]->is_return_output);
            if (listD->fs[i]->is_output)
                LOG_PRINT_NPFX(LOG_DPB, "out");
            LOG_PRINT_NPFX(LOG_DPB, "}");
            if ((i % 4) == 3)
                LOG_PRINT_NPFX(LOG_DPB, "\n");
        }
    }
    if ((i % 4) != 0)
        LOG_PRINT_NPFX(LOG_DPB, "\n");
    if (listD->last_picture)
        LOG_PRINT_NPFX(LOG_DPB, "\tlast picture exist\n");
    else 
        LOG_PRINT_NPFX(LOG_DPB, "\tno last picture\n");

#if OUTPUT_POC_MATCH_STEP
    LOG_PRINT_NPFX(LOG_DPB, "\tmax %d, used %d, curr out poc %d, step %d (this poc %d)\n", 
        p_Vid->max_buffered_num, p_Vid->used_buffer_num, p_Vid->output_poc, p_Vid->poc_step, p_Vid->ThisPOC);
#else
    LOG_PRINT_NPFX(LOG_DPB, "\tmax %d, used %d, curr out poc %d, (this poc %d)\n", 
        p_Vid->max_buffered_num, p_Vid->used_buffer_num, p_Vid->output_poc, p_Vid->ThisPOC);
#endif
}


void dump_dpb(DecoderParams *p_Dec, int mem_info)
{
    dump_dpb_info(p_Dec->p_Vid, mem_info);
}


void dump_output_list(VideoParams *p_Vid, enum dump_cond cond)
{
    int i;
    DUMP_MSG(cond, p_Vid->dec_handle, "output list[%d]:", p_Vid->output_frame_num);
    for(i = 0; i < p_Vid->output_frame_num; i++){
        DUMP_MSG(cond, p_Vid->dec_handle, " %d", p_Vid->output_frame[i].i16POC);
    }
    DUMP_MSG(cond, p_Vid->dec_handle, "\n");
}


void dump_release_list(VideoParams *p_Vid, enum dump_cond cond)
{
    int i;
    DUMP_MSG(cond, p_Vid->dec_handle, "release list[%d]:", p_Vid->release_buffer_num);
    for(i = 0; i < p_Vid->release_buffer_num; i++){
        DUMP_MSG(cond, p_Vid->dec_handle, " %d", p_Vid->release_buffer[i]);
    }
    DUMP_MSG(cond, p_Vid->dec_handle, "\n");
}


void dump_lt_ref_list(VideoParams *p_Vid, enum dump_cond cond)
{
    int i;
    DecodedPictureBuffer *listD = p_Vid->listD;
    DUMP_MSG(cond, p_Vid->dec_handle, "lt ref list[%d]:", listD->lt_ref_size);
    for (i = 0; i < listD->lt_ref_size; i++)
        DUMP_MSG(cond, p_Vid->dec_handle, "poc%d long%d\t", listD->fs_ltRef[i]->poc, listD->fs_ltRef[i]->long_term_frame_idx);
    DUMP_MSG(cond, p_Vid->dec_handle, "\n");
}


void dump_mem_register(DecoderParams *p_Dec)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys*)((uint32_t)(p_Dec->pu32BaseAddr) + H264_OFF_SYSM);
    uint32_t i;

    LOG_PRINT(LOG_DEBUG, "current base w:  %08x (@00h)\n", ptReg_sys->CUR_FRAME_BASE_W);
    LOG_PRINT(LOG_DEBUG, "current MB info: %08x (@04h)\n", ptReg_sys->CUR_MB_INFO_BASE1);
	LOG_PRINT(LOG_DEBUG, "current intra:   %08x (@08h)\n", ptReg_sys->CUR_INTRA_BASE1);
	LOG_PRINT(LOG_DEBUG, "scale base:      %08x (@14h)\n", ptReg_sys->SCALE_BASE);
	LOG_PRINT(LOG_DEBUG, "current base r:  %08x (@20h)\n", ptReg_sys->CUR_FRAME_BASE_R);
	LOG_PRINT(LOG_DEBUG, "current intra:   %08x (@24h)\n", ptReg_sys->CUR_INTRA_BASE2);
	LOG_PRINT(LOG_DEBUG, "current mb info: %08x (@28h)\n", ptReg_sys->CUR_MB_INFO_BASE2);
	LOG_PRINT(LOG_DEBUG, "col ref mb info: %08x (@2ch)\n", ptReg_sys->COL_REFPIC_MB_INFO);
	for (i = 0; i < p_Dec->p_Vid->list_size[0]; i++)
        LOG_PRINT(LOG_DEBUG, "list 0 ref %2d:   %08x (@%xh)\n", i, ptReg_sys->REFLIST0[i], 4*i+0x100);
    for (i = 0; i < p_Dec->p_Vid->list_size[1]; i++)
	    LOG_PRINT(LOG_DEBUG, "list 1 ref %2d:   %08x (@%xh)\n", i, ptReg_sys->REFLIST1[i], 4*i+0x180);
/*
    printk("current mb info1: %08x\n", ptReg_sys->CUR_MB_INFO_BASE1);
    printk("current mb info2: %08x\n", ptReg_sys->CUR_MB_INFO_BASE2);
    printk("current intra base1: %08x\n", ptReg_sys->CUR_INTRA_BASE1);
    printk("current intra base2: %08x\n", ptReg_sys->CUR_INTRA_BASE2);

    printk("ref l0: ");
    for (i = 0; i<p_Dec->p_Vid->list_size[0]; i++)
        printk("(%d) %08x\t", i, ptReg_sys->REFLIST0[i]);
    printk("\nref l1: ");
    for (i = 0; i<p_Dec->p_Vid->list_size[1]; i++)
        printk("(%d) %08x\t", i, ptReg_sys->REFLIST1[i]);
    printk("\n");
    if (p_Dec->bScaleEnable)
        printk("scale addr: %08x\n", ptReg_sys->SCALE_BASE);
*/
}


void dump_curr_mb(DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    LOG_PRINT(LOG_DEBUG, "mb cnt: 0x%x\n", ptReg_io->DEC_PARAM5 >> 16);
}


void dump_vcache_register(unsigned int vc_base_addr, int level)
{
#if CONFIG_ENABLE_VCACHE
//    DecoderParams *p_Dec = (DecoderParams *)ptDecHandle;
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)(vc_base_addr);

    LOG_PRINT(level, "vcache\n");
    LOG_PRINT(level, "0x00: %08x\n", ptReg_vcache->V_ENC);
    LOG_PRINT(level, "0x04: %08x\n", ptReg_vcache->DECODE);        // @04
    LOG_PRINT(level, "0x08: %08x\n", ptReg_vcache->I_SEARCH_RANGE);// @08
    LOG_PRINT(level, "0x0C: %08x\n", ptReg_vcache->O_SEARCH_RANGE);// @0C
    LOG_PRINT(level, "0x10: %08x\n", ptReg_vcache->CHN_CTL);       // @10
    LOG_PRINT(level, "0x20: %08x\n", ptReg_vcache->REF0_PARM);     // @20
    LOG_PRINT(level, "0x24: %08x\n", ptReg_vcache->REF1_PARM);     // @24
    LOG_PRINT(level, "0x28: %08x\n", ptReg_vcache->REF2_PARM);     // @28
    LOG_PRINT(level, "0x2C: %08x\n", ptReg_vcache->REF3_PARM);     // @2C
    LOG_PRINT(level, "0x30: %08x\n", ptReg_vcache->LOCAL_BASE);    // @30
    LOG_PRINT(level, "0x38: %08x\n", ptReg_vcache->DIS_ILF_PARM);  // @38
    LOG_PRINT(level, "0x40: %08x\n", ptReg_vcache->SYS_INFO);      // @40
    LOG_PRINT(level, "0x44: %08x\n", ptReg_vcache->RESERVED44);
    LOG_PRINT(level, "0x48: %08x\n", ptReg_vcache->CUR_PARM);
#endif
}

void dump_ref_list_addr(unsigned int base_addr, int level)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys *)(base_addr + H264_OFF_SYSM);
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)(base_addr);
    int i;
    unsigned int list0_size;
    unsigned int list1_size;

    list0_size = ((ptReg_io->DEC_PARAM4 >> 17) & 0x1F) + 1;
    list1_size = ((ptReg_io->DEC_PARAM4 >> 22) & 0x1F) + 1;

    LOG_PRINT(level, "REFLIST0 size: %d\n", list0_size);
    for(i = 0; i < 32; i++){
        LOG_PRINT(level, "[%2d]: %08x %s\n", i, ptReg_sys->REFLIST0[i], (i >= list0_size)?"inv":"");
    }

    LOG_PRINT(level, "REFLIST1 size: %d\n", list1_size);
    for(i = 0; i < 32; i++){
        LOG_PRINT(level, "[%2d]: %08x %s\n", i, ptReg_sys->REFLIST1[i], (i >= list1_size)?"inv":"");
    }
}


void dump_all_register(unsigned int base_addr, unsigned int vc_base_addr, int level)
{
    //DecoderParams *p_Dec = (DecoderParams *)ptDecHandle;
    //volatile H264_reg_io *ptReg_io = (H264_reg_io *)(p_Dec->pu32BaseAddr);
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)(base_addr);
#if 0
    uint32_t tmp = ((p_Dec->bLinkListError<< 7) |
                    (p_Dec->bDmaRWError << 5) | 
                    (p_Dec->bFrameDone << 4) | 
                    (p_Dec->bTimeout << 3) | 
                    (p_Dec->bBSLow << 2) | 
                    (p_Dec->bBSEmpty << 1) | 
                     p_Dec->bSliceDone);
#endif

    LOG_PRINT(level, "Bit CRL    : %08x (@00h)\n", ptReg_io->BITCTL);
    LOG_PRINT(level, "Bit read   : %08x (@08h)\n", ptReg_io->BITREAD);
    LOG_PRINT(level, "Bit shift  : %08x (@0ch)\n", ptReg_io->BITSHIFT);
    LOG_PRINT(level, "Bit buf cnt: %08x (@10h)\n", ptReg_io->BITSYSBUFCNT);
    LOG_PRINT(level, "Bit addr   : %08x (@14h)\n", ptReg_io->BITSYSBUFADR);
    LOG_PRINT(level, "CPUCMD0    : %08x (@18h)\n", ptReg_io->CPUCMD0);
    LOG_PRINT(level, "CPUCMD1    : %08x (@1Ch)\n", ptReg_io->CPUCMD1);
    LOG_PRINT(level, "mb cnt     : 0x%04x (@34h)\n", ptReg_io->DEC_PARAM5 >> 16);
    //LOG_PRINT(level, "Dec Parm4: %08x\n", ptReg_io->DEC_PARAM4);
    LOG_PRINT(level, "Dec CMD0   : %08x (@40h)\n", ptReg_io->DEC_CMD0);
    LOG_PRINT(level, "Dec CMD1   : %08x (@44h)\n", ptReg_io->DEC_CMD1);
    LOG_PRINT(level, "Dec CMD2   : %08x (@48h)\n", ptReg_io->DEC_CMD2);
    LOG_PRINT(level, "Dec STS0   : %08x (@50h)\n", ptReg_io->DEC_STS0);
    LOG_PRINT(level, "Dec INTSTS : %08x (@54h)\n", ptReg_io->DEC_INTSTS);
    LOG_PRINT(level, "RESERVED0  : %08x (@58h)\n", ptReg_io->RESERVED0);
//    LOG_PRINT(level, "Curr IRQ: %08x\n", tmp);
#if CONFIG_ENABLE_VCACHE
    dump_vcache_register(vc_base_addr, level);
#endif

}


/*
 * set bitstream address and length to HW register
 * - skip byte_offset and makes the address 8-byte aligned
 * - adjust length according to the byte_offset and the alogned offset
 */
int set_hw_bs_ptr(DecoderParams *p_Dec, unsigned char *bs_addr_va, unsigned int bs_addr_phy, unsigned int bs_len, unsigned int byte_offset, unsigned int bit_offset)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    unsigned int curr_addr;
    unsigned int curr_addr_8_aligned;
    int byte_offset_aligned;
    int curr_bit_shift;
    int byte_offset_remain;
    int byte_offset_remain_no_emu;
    unsigned int bs_len_adj;
    unsigned int bs_len_adj_aligned;
    const unsigned int MAX_BUF_SIZE = MAX_BUF_CNT_VAL << 6; //4194240 = (4 * 1024 * 1024) - 64;

    /*
     * bs_addr    curr_addr_8_aligned curr_addr
     * |---------------------------|-----|-----------------------|
     * | byte_offset                     |
     * | byte_offset_aligned       |
     *                             |     | byte_offset_remain
     * | bs_len                                                  | 
     *   bs_len_adj                |                             | 
     * NOTE: 
     * - bs_addr may not be an 8-byte-aligned address
     * - byte_offset_aligned may be negative
     */
     
    curr_addr = bs_addr_phy + byte_offset;
    curr_addr_8_aligned = curr_addr & (~7); /* round down the offset to multiple of 8 bytes */
    byte_offset_aligned = curr_addr_8_aligned - bs_addr_phy;
    bs_len_adj = bs_len - byte_offset_aligned;
    
    bs_len_adj_aligned = (bs_len_adj + 63) >> 6; /* round up to multiple of 64 and right shift 6 bits */
    

#if USE_RELOAD_BS
    if(bs_len_adj_aligned > MAX_BUF_CNT_VAL){
        LOG_PRINT(LOG_WARNING, "bitstream length too large (> 4MB):%d (%d)\n", bs_len_adj, bs_len_adj_aligned);
        p_Dec->u8ReloadFlg = 1;
        p_Dec->u32NextReloadBitstream_phy = curr_addr_8_aligned + MAX_BUF_SIZE;
        p_Dec->u32NextReloadBitstream_len = (bs_len_adj_aligned - MAX_BUF_CNT_VAL) << 6;
        bs_len_adj_aligned = MAX_BUF_CNT_VAL;
    }else{
        p_Dec->u8ReloadFlg = 0;
    }
#else
    if(bs_len_adj_aligned > MAX_BUF_CNT_VAL){
        LOG_PRINT(LOG_ERROR, "bitstream length too large (> 4MB):%d (%d)\n", bs_len_adj, bs_len_adj_aligned);
    }
#endif

    /* reset bitstream (stop hw bs -> fill -> start hw bs) */

    PERF_MSG("wait_bs >\n");
    WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x40), ptReg_io->DEC_CMD0, BIT5); /* bit_stop: stop bitstream operation */
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x14), ptReg_io->BITSYSBUFADR, curr_addr_8_aligned);
    WRITE_REG(p_Dec, ((MCP300_REG << 7) + 0x10), ptReg_io->BITSYSBUFCNT, bs_len_adj_aligned);
    WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x40), ptReg_io->DEC_CMD0, BIT4); /* bit_go: enable bitstream operation */

    byte_offset_remain = byte_offset - byte_offset_aligned;
    
    /* count the number of 0x000003XX in the remain bytes and cut them off from the remain bytes */
    if(byte_offset_remain){
        byte_offset_remain_no_emu = byte_offset_remain - count_emulation_prevent_byte(bs_addr_va, byte_offset_aligned, byte_offset);
    }else{
        byte_offset_remain_no_emu = 0;
    }
    curr_bit_shift = byte_offset_remain_no_emu * 8 + bit_offset;

#if 0
    if((byte_offset_aligned < 0) || (bs_addr_phy & ~7))
    {
        printk("              bs_addr_phy:%08X\n", bs_addr_phy);
        printk("                curr_addr:%08X\n", curr_addr);
        printk("      curr_addr_8_aligned:%08X\n", curr_addr_8_aligned);
        printk("       u32BSCurrentLength:%d\n", p_Dec->u32BSCurrentLength);
        printk("              byte_offset:%d\n", byte_offset);
        printk("      byte_offset_aligned:%d\n", byte_offset_aligned);
        printk("       byte_offset_remain:%d\n", byte_offset_remain);
        printk("byte_offset_remain_no_emu:%d\n", byte_offset_remain);
        printk("                   bs_len:%d\n", bs_len);
        printk("               bs_len_adj:%d\n", bs_len_adj);
        printk("           curr_bit_shift:%d\n", curr_bit_shift);
        //p_Dec->pfnDamnit("neg curr_addr_8_aligned\n");
    }
#endif
    
    /* wait for bitstream ready */
    POLL_REG_ONE(p_Dec, ((MCP300_REG << 7) + 0x00), ptReg_io->BITCTL, BIT0, 
    {
        E_DEBUG("bit ready polling timeout in set_hw_bs_ptr: %d %d\n", POLLING_ITERATION_TIMEOUT, cnt);
        return H264D_ERR_POLLING_TIMEOUT;
    }); /* return H264D_ERR_POLLING_TIMEOUT at timeout */
    PERF_MSG("wait_bs <\n");

#if 0
    printk("bs_buf\n");
    __dump_mem(bs_addr_va + (curr_addr_8_aligned - bs_addr_phy), 256);

    printk("tlu_buf\n");
    decoder_print_current_bitstream((uint32_t)p_Dec->pu32BaseAddr, 256);
    return H264D_ERR_POLLING_TIMEOUT;
#endif
    

    while(curr_bit_shift > 0){
        if(curr_bit_shift <= 32){
            /* shift curr_bit_shift bits */
            WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x0C), ptReg_io->BITSHIFT, curr_bit_shift - 1); 
            curr_bit_shift = 0;
        }else{
            /* shift 32 bits */
            WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x0C), ptReg_io->BITSHIFT, 31);
            curr_bit_shift -= 32;
        }
    }

#if 0
    printk("bs_buf\n");
    __dump_mem(bs_addr_va + (curr_addr_8_aligned - bs_addr_phy), 256);

    printk("tlu_buf\n");
    __dump_curr_bs((uint32_t)p_Dec->pu32BaseAddr, 256);
#endif

    
    return 0;
}


#if USE_RELOAD_BS
/*
 * Load remaining bitstream after the bistream empty interrupt is received
 * NOTE: 
 *   1. HW enters idle stats when bitsteam empty, and slice decoding will be restarted once the bitstream is loaded.
 *   2. dec_go(set bit0 of offset 0x40) is not required 
 */
static int __load_more_bitstream(DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    unsigned int curr_addr_8_aligned;
    unsigned int bs_len_adj_aligned;
    const unsigned int MAX_BUF_SIZE = MAX_BUF_CNT_VAL << 6; //4194240 = (4 * 1024 * 1024) - 64;
    
    if(p_Dec->u8ReloadFlg == 0){
        LOG_PRINT(LOG_ERROR, "load_more_bitstream is called with zero re-load flag: %d\n", p_Dec->u8ReloadFlg);
        return H264D_ERR_HW_BS_EMPTY;
    }

    curr_addr_8_aligned = p_Dec->u32NextReloadBitstream_phy;
    bs_len_adj_aligned = p_Dec->u32NextReloadBitstream_len >> 6;

    if(bs_len_adj_aligned > MAX_BUF_CNT_VAL){
        LOG_PRINT(LOG_WARNING, "bitstream length too large (> 4MB):%d (%d)\n", bs_len_adj_aligned << 6, bs_len_adj_aligned);
        p_Dec->u8ReloadFlg = 1;
        p_Dec->u32NextReloadBitstream_phy = curr_addr_8_aligned + MAX_BUF_SIZE;
        p_Dec->u32NextReloadBitstream_len = (bs_len_adj_aligned - MAX_BUF_CNT_VAL) << 6;
        bs_len_adj_aligned = MAX_BUF_CNT_VAL;
    }

    WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x10), ptReg_io->BITSYSBUFCNT, bs_len_adj_aligned);

    //printk("__load_more_bitstream(): %d\n", bs_len_adj_aligned);
    
    return H264D_OK;
}
#endif


/*
 * set bitstream buffer address/size
 * NOTE: When HW parser is used, load bitstream right away
 */
int decoder_set_bs_buffer(DecoderParams *p_Dec)
{

#if USE_SW_PARSER
    init_bitstream(&p_Dec->stream, p_Dec->pu8Bitstream_virt, p_Dec->u32BSLength);
#else
    if(set_hw_bs_ptr(p_Dec, p_Dec->pu8Bitstream_virt, (unsigned int)p_Dec->pu8Bitstream_phy, p_Dec->u32BSCurrentLength, 0, 0)){
        return H264D_ERR_POLLING_TIMEOUT;
    }
#endif /* USE_SW_PARSER */

    return 0;
}



#if USE_SW_PARSER
/*
 * shift hardware bitstream pointer to the read pointer of sw parser
 */
static int __sync_hw_bs_ptr(DecoderParams *p_Dec)
{
    int byte_offset, bit_offset;

    byte_offset = sw_byte_offset(&p_Dec->stream);
    if(sw_bs_emtpy(&p_Dec->stream)){
        /* avoid to use sw_bit_offset() when sw_bs_emtpy() */
        bit_offset = 0;
    }else{
        bit_offset = sw_bit_offset(&p_Dec->stream);
    }

    return set_hw_bs_ptr(p_Dec, p_Dec->pu8Bitstream_virt, (unsigned int)p_Dec->pu8Bitstream_phy, 
        p_Dec->u32BSCurrentLength, byte_offset, bit_offset);
}
#endif


#if USE_FEED_BACK
/*
 * handle buffer user not used (be outputed)
 */
int feedback_buffer_used_done(DecoderParams *p_Dec, FAVC_DEC_FEEDBACK *ptFeedback)
{
    int idx, ret = 0;
    VideoParams *p_Vid = p_Dec->p_Vid;

    if(SHOULD_DUMP(D_BUF_DETAIL, p_Vid->dec_handle))
    {
        DUMP_MSG(D_BUF_DETAIL, p_Vid->dec_handle, "bf decoder_feedback_buffer_used_done\n");
        dump_dpb(p_Vid->dec_handle, 0);
    }

    ptFeedback->u8ReleaseBufferNum = 0;
    for (idx = 0; idx < ptFeedback->tRetBuf.i32FeedbackNum; idx++) {
        if (buffer_used_done(p_Vid, p_Vid->listD, ptFeedback->tRetBuf.i32FeedbackList[idx]) == RELEASE_UNUSED_BUFFER) {
            ptFeedback->u8ReleaseBuffer[ptFeedback->u8ReleaseBufferNum++] = ptFeedback->tRetBuf.i32FeedbackList[idx];
            ret = 1;
        }
    }

    if(SHOULD_DUMP(D_BUF_DETAIL, p_Vid->dec_handle))
    {
        DUMP_MSG(D_BUF_DETAIL, p_Vid->dec_handle, "af decoder_feedback_buffer_used_done\n");
        dump_dpb(p_Vid->dec_handle, 0);
    }

    return ret;
}
#endif

/*
 * output all buffer not outputed & flush all
 */
int decoder_output_all_picture(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame, int get_release_flg)
{
    VideoParams *p_Vid;

    if(p_Dec == NULL){
        LOG_PRINT(LOG_ERROR, "dec_handle null(decoder_output_all_picture)\n");
        return -1;
    }
    
    p_Vid = p_Dec->p_Vid;
    p_Vid->output_frame_num = 0; /* clear output buffer list */
    p_Vid->release_buffer_num = 0; /* clear release buffer list */

    output_all_picture(p_Vid);
    return_output_list(ptFrame, p_Vid);
    

    flush_dpb(p_Vid);
    if(get_release_flg){
        return_release_list(ptFrame, p_Vid);
    }

    if(SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)){
        dump_output_list(p_Vid, D_BUF_REF);
        dump_release_list(p_Vid, D_BUF_REF);
    }

#ifndef VG_INTERFACE
    /* NOTE: required by ioctl ap */
    /* set output frame width/height and other info */
    return_output_frame_info(ptFrame, p_Vid);
#endif

    return p_Vid->output_frame_num;
}


/*!
 ************************************************************************
 * \brief
 *    Initializes the parameters for a new picture
 *    init picture: set poc, structure, frame number, reconstruct address
 ************************************************************************
 */
static int init_picture(DecoderParams *p_Dec, VideoParams *p_Vid)
{
    StorablePicture *pic = p_Vid->dec_pic;
    uint32_t ret = H264D_OK;
    uint32_t MaxFrameNum = (1 << p_Vid->active_sps->log2_max_frame_num);
    
    LOG_PRINT(LOG_INFO, "init_picture: %d\n", MaxFrameNum);

    if ((p_Vid->frame_num != p_Vid->prev_frame_num) && 
        (p_Vid->frame_num != ((p_Vid->prev_frame_num+1) % MaxFrameNum))) {
        if (p_Vid->active_sps->gaps_in_frame_num_value_allowed_flag == 1) {
            if (fill_frame_num_gap(p_Vid) < 0){
                LOG_PRINT(LOG_ERROR, "fill_frame_num_gap error\n");
                return H264D_PARSING_ERROR;
            }
        }
        
        if (p_Vid->active_sps->gaps_in_frame_num_value_allowed_flag == 0){
            p_Dec->u16FrNumSkipError++;
            if((p_Dec->u8LosePicHandleFlags & LOSS_PIC_PRINT_ERR) || TREAT_WARN_AS_ERR(dbg_mode)){
                LOG_PRINT(LOG_ERROR, "an unintentional loss of pictures occurs: %d->%d\n", p_Vid->prev_frame_num, p_Vid->frame_num);
            }else{
                LOG_PRINT(LOG_WARNING, "warning: an unintentional loss of pictures occurs: %d->%d\n", p_Vid->prev_frame_num, p_Vid->frame_num);
            }

            if((p_Dec->u8LosePicHandleFlags & LOSS_PIC_RETURN_ERR) || TREAT_WARN_AS_ERR(dbg_mode)){
                return H264D_LOSS_PIC;
            }
        }
    }
    
    /* set bottom first flag (used for mbinfo). mbinfo is top bottom plane or bottom top plane */
    if (((p_Vid->listD->last_picture == NULL) && (p_Vid->structure == BOTTOM_FIELD)) ||
        ((p_Vid->listD->last_picture != NULL) && (p_Vid->structure == TOP_FIELD))) {
        p_Vid->bottom_field_first = 1;
    } else {
        p_Vid->bottom_field_first = 0;
    }

    if (p_Vid->nalu.nal_ref_idc)
        p_Vid->prev_frame_num = p_Vid->frame_num;
    
    decode_poc(p_Vid);
    
    if (p_Vid->ThisPOC > MAX_HW_POC) {
        LOG_PRINT(LOG_ERROR, "hardware does not support poc larger than %d (%d > %d)\n", MAX_HW_POC, p_Vid->ThisPOC, MAX_HW_POC);
        return H264D_ERR_HW_UNSUPPORT;
    }

    alloc_storable_picture(pic, p_Vid->structure);

    pic->coded_frame = (p_Vid->structure==FRAME);
    pic->toppoc = p_Vid->toppoc;
    pic->botpoc = p_Vid->bottompoc;
    pic->framepoc = p_Vid->framepoc;
    if (pic->structure == FRAME)
        pic->poc = p_Vid->framepoc;
    else
        pic->poc = (pic->structure == TOP_FIELD ? pic->toppoc : pic->botpoc);
    pic->used_for_ref = (p_Vid->nalu.nal_ref_idc != 0);
    pic->pic_num = p_Vid->frame_num;
    pic->frame_num = p_Vid->frame_num;
    pic->bottom_field_first = p_Vid->bottom_field_first;
    pic->slice_type = p_Vid->slice_hdr->slice_type;
    pic->non_existing = 0;
    pic->is_output = 0;
    pic->is_damaged = 0;
    pic->valid = 1;
    
    /* consider non-paired field */
    /* if non-paired field, last_picture will not be NULL */
    if (p_Vid->structure == FRAME){
        pic->phy_index = p_Vid->pRecBuffer->buffer_index;
        pic->refpic_addr = p_Vid->pRecBuffer->dec_yuv_buf_virt;
        pic->refpic_addr_phy = p_Vid->pRecBuffer->dec_yuv_buf_phy;
        pic->mbinfo_addr = p_Vid->pRecBuffer->dec_mbinfo_buf_virt;
        pic->mbinfo_addr_phy = p_Vid->pRecBuffer->dec_mbinfo_buf_phy;
    } else {
        if (p_Vid->new_frame) {
            pic->phy_index = p_Vid->pRecBuffer->buffer_index;
            pic->refpic_addr = p_Vid->pRecBuffer->dec_yuv_buf_virt;
            pic->refpic_addr_phy = p_Vid->pRecBuffer->dec_yuv_buf_phy;
            pic->mbinfo_addr = p_Vid->pRecBuffer->dec_mbinfo_buf_virt;
            pic->mbinfo_addr_phy = p_Vid->pRecBuffer->dec_mbinfo_buf_phy;
        } else {
            pic->phy_index = p_Vid->pRecBuffer->buffer_index;
            pic->refpic_addr = p_Vid->pRecBuffer->dec_yuv_buf_virt;
            pic->refpic_addr_phy = p_Vid->pRecBuffer->dec_yuv_buf_phy;
            pic->mbinfo_addr = p_Vid->pRecBuffer->dec_mbinfo_buf_virt + p_Vid->frame_width*p_Vid->frame_height/2;
            pic->mbinfo_addr_phy = p_Vid->pRecBuffer->dec_mbinfo_buf_phy + p_Vid->frame_width*p_Vid->frame_height/2;
        }
    }

    return ret;
}


/*
 * ============================================================================
 *   store picture & reset picture
 * ============================================================================
 * \return
 *    < 0 (H264D_PARSING_ERROR): error occurred and the picture is NOT inserted
 *    FRAME_DONE: a frame is inserted
 *    FIELD_DONE: a field is inserted
 */
static int exit_picture(DecoderParams *p_Dec, VideoParams *p_Vid, StorablePicture *pic, byte is_damaged)
{
    int ret = 0;
    if (pic->valid == 0) {
        DUMP_MSG(D_ERR, p_Dec, "exit_picture err: storing invalid picture ch:%d fr:%d tr:%d\n", 
            p_Vid->dec_handle->chn_idx, p_Vid->dec_handle->u32DecodedFrameNumber,
            p_Vid->dec_handle->u32DecodeTriggerNumber);
        return H264D_PARSING_ERROR;
    }
    pic->is_damaged = is_damaged;
    ret = store_picture_in_dpb(p_Vid, pic, p_Vid->slice_hdr->adaptive_ref_pic_marking_mode_flag);

    pic->valid = 0;
    pic->sp_frame = NULL;
    pic->sp_top = NULL;
    pic->sp_btm = NULL; 

    if (p_Vid->last_has_mmco_5)
        p_Vid->prev_frame_num = 0;

    return ret;
}


/*
 * set dist_scale_factor
 */
static void set_dsf(DecoderParams *p_Dec)
{
    volatile H264_reg_dsf *ptReg_dsf = (H264_reg_dsf*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_DSFM);
    VideoParams *p_Vid = p_Dec->p_Vid;
    uint32_t i, j;
    int tx, tb, td;

    if (p_Vid->slice_hdr->direct_spatial_mv_pred_flag == 0){
        for (j = 0; j<2 + (p_Vid->MbaffFrameFlag*4); j+=2){
            for (i = 0; i<p_Vid->list_size[j]; i++){
                if (p_Vid->refListLX[j][i] == NULL || p_Vid->refListLX[j][i]->valid == 0 ||
                    p_Vid->refListLX[1+j][0] == NULL || p_Vid->refListLX[1+j][0]->valid == 0) {
                    if (j == 0){
                        //ptReg_dsf->DSF_L0[i] = 0;
                        WRITE_REG(p_Dec, ((MCP300_DSF0 << 7) + i * 4), ptReg_dsf->DSF_L0[i], 0);
                    }else if (j==2){
                        //ptReg_dsf->DSF_L3[i] = 0;
                        WRITE_REG(p_Dec, ((MCP300_DSF1 << 7) + i * 4), ptReg_dsf->DSF_L3[i], 0);
                    }else{
                        //ptReg_dsf->DSF_L5[i] = 0;
                        WRITE_REG(p_Dec, ((MCP300_DSF2 << 7) + i * 4), ptReg_dsf->DSF_L5[i], 0);
                    }
                    continue;
                }
                if (j == 0)
                    tb = iClip3(-128, 127, p_Vid->dec_pic->poc - p_Vid->refListLX[j][i]->poc);
                else if (j == 2) {
                    tb = iClip3(-128, 127, p_Vid->dec_pic->toppoc - p_Vid->refListLX[j][i]->poc);
                }
                else 
                    tb = iClip3(-128, 127, p_Vid->dec_pic->botpoc - p_Vid->refListLX[j][i]->poc);

                td = iClip3(-128, 127, p_Vid->refListLX[1+j][0]->poc - p_Vid->refListLX[j][i]->poc);

                if (td != 0){
                    tx = (16384 + iAbs(td/2))/td;
                    p_Vid->dist_scale_factor[j][i] = iClip3(-1024, 1023, (tb * tx + 32)>>6);
                    p_Vid->dist_scale_factor[j][i] &= 0xFFF;
                }
                else {
                    p_Vid->dist_scale_factor[j][i] = 9999;
                    p_Vid->dist_scale_factor[j][i] = 0x1000;
                }
                if (p_Vid->refListLX[j][i]->is_long_term)
                    p_Vid->dist_scale_factor[j][i] |= 0x2000;

                if (j == 0){
                    //ptReg_dsf->DSF_L0[i] = p_Vid->dist_scale_factor[j][i];
                    WRITE_REG(p_Dec, ((MCP300_DSF0 << 7) + i * 4), ptReg_dsf->DSF_L0[i], p_Vid->dist_scale_factor[j][i]);
                }else if (j == 2){
                    //ptReg_dsf->DSF_L3[i] = p_Vid->dist_scale_factor[j][i];
                    WRITE_REG(p_Dec, ((MCP300_DSF1 << 7) + i * 4), ptReg_dsf->DSF_L3[i], p_Vid->dist_scale_factor[j][i]);
                }else{
                    //ptReg_dsf->DSF_L5[i] = p_Vid->dist_scale_factor[j][i];
                    WRITE_REG(p_Dec, ((MCP300_DSF2 << 7) + i * 4), ptReg_dsf->DSF_L5[i], p_Vid->dist_scale_factor[j][i]);
                }
            }
            for (i = p_Vid->list_size[j]; i<MAX_DSF_NUM; i++){
                if (j == 0){
                    //ptReg_dsf->DSF_L0[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF0 << 7) + i * 4), ptReg_dsf->DSF_L0[i], 0);
                }else if (j == 2){
                    //ptReg_dsf->DSF_L3[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF1 << 7) + i * 4), ptReg_dsf->DSF_L3[i], 0);
                }else{
                    //ptReg_dsf->DSF_L5[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF2 << 7) + i * 4), ptReg_dsf->DSF_L5[i], 0);
                }
            }
        }
    }
    else {
        for (j = 0; j<2 + (p_Vid->MbaffFrameFlag*4); j+=2){
            for (i = 0; i < MAX_DSF_NUM; i++){
                if (j == 0){
                    //ptReg_dsf->DSF_L0[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF0 << 7) + i * 4), ptReg_dsf->DSF_L0[i], 0);
                }else if (j == 2){
                    //ptReg_dsf->DSF_L3[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF1 << 7) + i * 4), ptReg_dsf->DSF_L3[i], 0);
                }else{
                    //ptReg_dsf->DSF_L5[i] = 0;
                    WRITE_REG(p_Dec, ((MCP300_DSF2 << 7) + i * 4), ptReg_dsf->DSF_L5[i], 0);
                }
            }
        }
    }
}


/*
 * From get_colindex in h264_vld_task.v
 */
static int get_colindex(VideoParams *p_Vid)
{
    p_Vid->col_pic = p_Vid->refListLX[1][0];
/*
printk("col pic %x->", (uint32_t)p_Vid->col_pic);
if (p_Vid->col_pic)
    printk("col valid %d->", p_Vid->col_pic->valid);
*/
    if (p_Vid->col_pic == NULL || p_Vid->col_pic->valid == 0)
        return H264D_PARSING_ERROR;

    if (p_Vid->col_pic->coded_frame || p_Vid->MbaffFrameFlag) 
        p_Vid->col_pic = p_Vid->col_pic->sp_frame;
    else if (!p_Vid->field_pic_flag){
        StorablePicture *top_pic, *btm_pic;
        int top_dis, btm_dis;
        top_pic = p_Vid->col_pic->sp_top;
        btm_pic = p_Vid->col_pic->sp_btm;
/*
printk("top pic %x->", (uint32_t)top_pic);
printk("bot pic %x->", (uint32_t)btm_pic);
if (top_pic)
    printk("top valid %d->", top_pic->valid);
if (btm_pic)
    printk("bot valid %d->", btm_pic->valid);
*/
        if (top_pic == NULL || top_pic->valid == 0 || btm_pic == NULL || btm_pic->valid == 0) 
            return H264D_PARSING_ERROR;

        top_dis = iAbs(top_pic->poc - p_Vid->ThisPOC);
        btm_dis = iAbs(btm_pic->poc - p_Vid->ThisPOC);
        if (top_dis < btm_dis)
            p_Vid->col_pic = top_pic;
        else
            p_Vid->col_pic = btm_pic;
    }
    if (p_Vid->col_pic == NULL || p_Vid->col_pic->valid == 0)
        return H264D_PARSING_ERROR;

    p_Vid->col_idr_flag = (p_Vid->col_pic->slice_type == I_SLICE);
    p_Vid->col_fieldflag = !p_Vid->col_pic->coded_frame;
    p_Vid->col_index = p_Vid->col_pic->phy_index;
    
    return H264D_OK;
}


/*
 * From set_poc_to_pred_table in h264_vld_task.v
 * return value: 
 *    < 0: H264D_PARSING_ERROR
 *   == 0: H264D_OK
 */
static int set_poc_to_pred_table(DecoderParams *p_Dec, VideoParams *p_Vid)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM);
    StorablePicture *sp;
    int inverse;
    uint32_t i;

    for (i = 0; i<p_Vid->list_size[0]; i++){
        sp = p_Vid->refListLX[0][i];
        if (sp == NULL || sp->valid == 0)
            return H264D_PARSING_ERROR;
        
        inverse = (sp->botpoc < sp->toppoc ? 1 : 0);
        if (sp->structure == BOTTOM_FIELD){
            //ptReg_sys->WEIGHTL0[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }else if (sp->structure == TOP_FIELD){
            //ptReg_sys->WEIGHTL0[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }else if (inverse == 1){
            //ptReg_sys->WEIGHTL0[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->toppoc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->toppoc&0xFFFF)));
        }else{
            //ptReg_sys->WEIGHTL0[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }
        if (!p_Vid->field_pic_flag){
            if (sp->structure == BOTTOM_FIELD){
                //ptReg_sys->WEIGHTL0[i*2+1] = ((sp->is_long_term<<16) | (sp->toppoc&0xFFFF));
                WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL0[i*2+1], ((sp->is_long_term<<16) | (sp->toppoc&0xFFFF)));
            }else{
                //ptReg_sys->WEIGHTL0[i*2+1] = ((sp->is_long_term<<16) | (sp->botpoc&0xFFFF));
                WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL0[i*2+1], ((sp->is_long_term<<16) | (sp->botpoc&0xFFFF)));
            }
        }
    }

    for (i = 0; i<p_Vid->list_size[1]; i++){
        sp = p_Vid->refListLX[1][i];
        if (sp == NULL || sp->valid == 0)
            return H264D_PARSING_ERROR;
        
        inverse = (sp->botpoc < sp->toppoc ? 1 : 0);
        if (sp->structure == BOTTOM_FIELD){
            //ptReg_sys->WEIGHTL1[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }else if (sp->structure == TOP_FIELD){
            //ptReg_sys->WEIGHTL1[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }else if (inverse == 1){
            //ptReg_sys->WEIGHTL1[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->toppoc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->toppoc&0xFFFF)));
        }else{
            //ptReg_sys->WEIGHTL1[i*2] = ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], ((inverse<<17) | (sp->is_long_term<<16) | (sp->poc&0xFFFF)));
        }
        if (!p_Vid->field_pic_flag){
            if (sp->structure == BOTTOM_FIELD){
                //ptReg_sys->WEIGHTL1[i*2+1] = ((sp->is_long_term<<16) | (sp->toppoc&0xFFFF));
                WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL1[i*2+1], ((sp->is_long_term<<16) | (sp->toppoc&0xFFFF)));
            }else{
                //ptReg_sys->WEIGHTL1[i*2+1] = ((sp->is_long_term<<16) | (sp->botpoc&0xFFFF));
                WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL1[i*2+1], ((sp->is_long_term<<16) | (sp->botpoc&0xFFFF)));
            }
        }
    }
    return H264D_OK;
}



void set_pred_weight_table(DecoderParams *p_Dec, SliceHeader *sh)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM);
    int i;

    for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++) {
        //ptReg_sys->WEIGHTL0[i*2] = (((chroma_l0_flag&0x01)<<17) | ((luma_l0_flag&0x01)<<16) | ((lo_0&0xFF)<<8) | (lw_0&0xFF));
        WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], sh->weightl0[i*2]);
        //ptReg_sys->WEIGHTL0[i*2 + 1] = CONCAT_4_8BITS_VALUES(co_0_cr, cw_0_cr, co_0_cb, cw_0_cb);
        WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL0[i*2 + 1], sh->weightl0[i*2 + 1]);
    }
    if (sh->slice_type == B_SLICE){
        for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++){
            //ptReg_sys->WEIGHTL1[i*2] = (((chroma_l1_flag&0x01)<<17) | ((luma_l1_flag&0x01)<<16) | ((lo_1&0xFF)<<8) | (lw_1&0xFF));
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], sh->weightl1[i*2]);
            //ptReg_sys->WEIGHTL1[i*2 + 1] = CONCAT_4_8BITS_VALUES(co_1_cr, cw_1_cr, co_1_cb, cw_1_cb);
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL1[i*2 + 1], sh->weightl1[i*2 + 1]);
        }
    }
}


void set_pred_weight_table_default(DecoderParams *p_Dec, SliceHeader *sh)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM);
    int i;
    for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++){
        //ptReg_sys->WEIGHTL0[i*2] = 0x00000000;
        WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL0[i*2], 0x00000000);
        //ptReg_sys->WEIGHTL0[i*2 + 1] = 0x00000000;
        WRITE_REG(p_Dec, ((MCP300_WP0_L0 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL0[i*2 + 1], 0x00000000);
    }
    if (sh->slice_type == B_SLICE){
        for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++){
            //ptReg_sys->WEIGHTL1[i*2] = 0x00000000;
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + i * 2 * 4), ptReg_sys->WEIGHTL1[i*2], 0x00000000);
            //ptReg_sys->WEIGHTL1[i*2 + 1] = 0x00000000;
            WRITE_REG(p_Dec, ((MCP300_WP0_L1 << 7) + (i * 2 + 1) * 4), ptReg_sys->WEIGHTL1[i*2 + 1], 0x00000000);
        }
    }
}

/*
 * clear the MCP300 SRAM storing POC table
 */
static void __clear_sram_ref_pic_poc(uint32_t base_addr)
{
    volatile H264_reg_refpoc *ptReg_refpoc = (H264_reg_refpoc*)(base_addr + H264_OFF_POCM);
    uint32_t i;
    for(i = 0; i < sizeof(ptReg_refpoc->REFLIST0)/sizeof(ptReg_refpoc->REFLIST0[0]); i++){
        ptReg_refpoc->REFLIST0[i] = 0;
    }
    for(i = 0; i < sizeof(ptReg_refpoc->REFLIST1)/sizeof(ptReg_refpoc->REFLIST1[0]); i++){
        ptReg_refpoc->REFLIST1[i] = 0;
    }
}


/*
 * From get_poclist in h264_vld_task.v
 * Set poc of ref list 0/1 to register
 */
static int set_ref_pic_poc(DecoderParams *p_Dec)
{
    volatile H264_reg_refpoc *ptReg_refpoc = (H264_reg_refpoc*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_POCM);
    VideoParams *p_Vid = p_Dec->p_Vid;
    StorablePicture *sp;
    uint32_t i;
    int poc1, poc2, inverse;
    if (p_Vid->slice_hdr->slice_type == B_SLICE) {
        if (get_colindex(p_Vid) < 0)
            return H264D_PARSING_ERROR;
    }    
    else {
        p_Vid->col_fieldflag = 0;
        p_Vid->col_idr_flag = 0;
        p_Vid->col_index = 0;
        p_Vid->col_pic = NULL;
    }
    for (i = 0; i < p_Vid->list_size[0]; i++){
        sp = p_Vid->refListLX[0][i];
        if (sp == NULL || sp->valid == 0)
            return H264D_PARSING_ERROR;

        inverse = (sp->botpoc < sp->toppoc ? 1 : 0);
        if (sp->structure == BOTTOM_FIELD){ // poc = bottom poc
            poc1 = sp->poc*2 + 1;
            poc2 = sp->toppoc*2;
        }
        else if (sp->structure == TOP_FIELD){ // poc = top poc
            poc1 = sp->poc*2;
            poc2 = sp->botpoc*2 + 1;
        }
        else if (inverse == 1){ // poc = bottom poc
            poc1 = sp->toppoc*2;
            poc2 = sp->botpoc*2 + 1;
        }
        else {  // poc = top poc
            poc1 = sp->poc*2;
            poc2 = sp->botpoc*2 + 1;
        }
        if (p_Vid->field_pic_flag && p_Vid->col_fieldflag)
            poc2 = poc1;
        //ptReg_refpoc->REFLIST0[i] = CONCAT_2_16BITS_VALUES(poc2, poc1);
        WRITE_REG(p_Dec, ((MCP300_POC0 << 7) + i * 4), ptReg_refpoc->REFLIST0[i], CONCAT_2_16BITS_VALUES(poc2, poc1));
    }
    if (p_Vid->slice_hdr->slice_type == B_SLICE){
        for (i = 0; i < p_Vid->list_size[1]; i++){
            sp = p_Vid->refListLX[1][i];
            if (sp == NULL || sp->valid == 0)
                return H264D_PARSING_ERROR;

            inverse = (sp->botpoc < sp->toppoc ? 1 : 0);
            if (sp->structure == BOTTOM_FIELD){
                poc1 = sp->poc*2 + 1;
                poc2 = sp->toppoc*2;
            }
            else if (sp->structure == TOP_FIELD){   // poc = top poc
                poc1 = sp->poc*2;
                poc2 = sp->botpoc*2 + 1;
            }
            else if (inverse == 1){ // poc = bottom poc
                poc1 = sp->toppoc*2;
                poc2 = sp->botpoc*2 + 1;
            }
            else {  // poc = top poc
                poc1 = sp->poc*2;
                poc2 = sp->botpoc*2 + 1;
            }
            if (p_Vid->field_pic_flag && p_Vid->col_fieldflag)
                poc2 = poc1;
            //ptReg_refpoc->REFLIST1[i] = CONCAT_2_16BITS_VALUES(poc2, poc1);
            WRITE_REG(p_Dec, ((MCP300_POC1 << 7) + i * 4), ptReg_refpoc->REFLIST1[i], CONCAT_2_16BITS_VALUES(poc2, poc1));
        }
    }
    return H264D_OK;
}


/*
 * clear MCP300 SRAM storing addresses (to avoid the DMA error at decoding error bitstream)
 * NOTE: Clear it in case that it stores values from previous decoding.
 *       Wrong value in these SRAM may result in incorrect decoding memory access (DMA read/write error)
 *       at decoding error bitstream
 */
static void __clear_sram_data(uint32_t base_addr)
{
    unsigned int sram_addr;
    
    sram_addr = base_addr + H264_OFF_SYSM;
    memset((void *)sram_addr, 0, 0x10C40 - 0x10C00); // 0x10C00 ~ 0x10C40 (DMA read/write channel base address)

    sram_addr = base_addr + 0x10D00;
    memset((void *)sram_addr, 0, 0x10E00 - 0x10D00); // 0x10D00 ~ 0x10E00 (list0/1 ref base)

#if 0 /* this is not necessary?? */
    sram_addr = base_addr + 0x20D88; // 0x20400 + 0x988
    memset((void *)sram_addr, 0, 0x20E88 - 0x20D88); // 0x20D88 ~ 0x20E88 (MV0 ~ MV31)
#endif

}



#if CONFIG_ENABLE_VCACHE
/* 
 * from set_vcache in vcache_task.v 
 */
static void set_vcache(DecoderParams *p_Dec)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)(p_Dec->pu32VcacheBaseAddr);
    uint32_t curr_ref_addr;
    enum vcache_en_flag vcache_en_flgs = 0;
    extern unsigned int vcache_one_ref;
    extern unsigned int max_ref1_width;

    /* SYSINFO and ILF */
    if(p_Vid->first_slice) {
        /* setting is changed only at the first slice */
        if(p_Vid->frame_width <= VCACHE_MAX_WIDTH){

            /* ILF */
            if((p_Dec->vcache_en_cfgs & VCACHE_ILF_EN) &&
               (0 == p_Vid->MbaffFrameFlag) && 
               (0 == p_Vid->field_pic_flag))
            {
                vcache_en_flgs |= VCACHE_ILF_EN;
            }

            /* SYSINFO */
            if((p_Dec->vcache_en_cfgs & VCACHE_SYSINFO_EN) &&
               (0 == p_Vid->MbaffFrameFlag))
            {
                vcache_en_flgs |= VCACHE_SYSINFO_EN;
            }
        }
        
        if(vcache_en_flgs & VCACHE_ILF_EN){
            WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x38), ptReg_vcache->DIS_ILF_PARM, (BIT1|BIT0));
            p_Dec->eng_info->vcache_ilf_enable = p_Vid->vcache_ilf_enable = 1;
        }else{
            WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x38), ptReg_vcache->DIS_ILF_PARM, 0);
            p_Dec->eng_info->vcache_ilf_enable = p_Vid->vcache_ilf_enable = 0;
        }
        
        if(vcache_en_flgs & VCACHE_SYSINFO_EN){
            uint32_t sys_info;
            if(p_Dec->bHasMBinfo){
                sys_info = 0x00000269;
            }else{
                sys_info = 0x00000261;
            }
            WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x40), ptReg_vcache->SYS_INFO, sys_info);
        }else{
            WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x40), ptReg_vcache->SYS_INFO, 0x00000260);
        }
    }

    /* REF */
    p_Vid->vreg_change = 0;
    if(p_Dec->vcache_en_cfgs & VCACHE_REF_EN) {
        if (0 == p_Vid->MbaffFrameFlag && 0 == p_Vid->field_pic_flag 
            && I_SLICE != p_Vid->slice_hdr->slice_type
            && p_Vid->PicWidthInMbs >= 11 && p_Vid->frame_width <= VCACHE_MAX_WIDTH) 
        {
            if (p_Vid->first_slice) {
                vcache_en_flgs |= VCACHE_REF_EN;
                
                /* only the first slice can enable vcache 
                 * if first slice does not enable vcache, this frame does not use vcache
                 */
                //ptReg_vcache->V_ENC = 1;
                //ptReg_vcache->DECODE = 0x01;

                /* search range: required only when VCACHE REF is enabled */
                WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x08), ptReg_vcache->I_SEARCH_RANGE, 0x04080408); /* input search range */
                if (p_Vid->PicWidthInMbs <= 11){
                    /* to support CIF decoding, shrink the search range and make the HW access DRAM instead of SRAM */
                    WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x0C), ptReg_vcache->O_SEARCH_RANGE, 0x02080208); /* output search range */
                }else{
                    WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x0C), ptReg_vcache->O_SEARCH_RANGE, 0x04080408); /* output search range */
                }


                /* reference 0 */
                WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, (((uint32_t)p_Vid->refListLX[0][0]->refpic_addr_phy & 0xFFFFFF00) | 0x99));
                p_Vid->old_ref_addr_pa[0] = (uint32_t)p_Vid->refListLX[0][0]->refpic_addr_phy;
                LOG_PRINT(LOG_INFO, "first vcache enable: L0 ref 0 (%08x)", (uint32_t)p_Vid->refListLX[0][0]->refpic_addr_phy);
                
                /* reference 1 */
                if (p_Vid->slice_hdr->slice_type == B_SLICE && p_Vid->frame_width <= max_ref1_width) {
                    WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, (((uint32_t)p_Vid->refListLX[1][0]->refpic_addr_phy & 0xFFFFFF00) | 0xa9));
                    p_Vid->old_ref_addr_pa[1] = ((uint32_t)p_Vid->refListLX[1][0]->refpic_addr_phy & 0xFFFFFF00);
                    LOG_PRINT_NPFX(LOG_INFO, ", L1 ref 0 (%08x)", (uint32_t)p_Vid->refListLX[1][0]->refpic_addr_phy);
                }
                else if (p_Vid->list_size[0] > 1 && p_Vid->frame_width <= max_ref1_width) {
                    WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, (((uint32_t)p_Vid->refListLX[0][1]->refpic_addr_phy & 0xFFFFFF00) | 0xa9));
                    p_Vid->old_ref_addr_pa[1] = ((uint32_t)p_Vid->refListLX[0][1]->refpic_addr_phy & 0xFFFFFF00);
                    LOG_PRINT_NPFX(LOG_INFO, ", L0 ref 1 (%08x)", (uint32_t)p_Vid->refListLX[0][1]->refpic_addr_phy);
                } else {
                    p_Vid->old_ref_addr_pa[1] = 0;
                    WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, 0);
                }

                if(vcache_one_ref){
                    if(p_Vid->old_ref_addr_pa[1] != 0){
                        uint32_t local_base;
                        local_base = p_Vid->frame_width * 2 * 10;  // frame width * 2 (422) * 80 (search range) / 8 (dword)
                        if (local_base * 2 > 0x10000){
                            local_base = 0x10000;
                        }
                        LOG_PRINT_NPFX(LOG_INFO, "local base = 0x%x", local_base);
                        WRITE_REG(p_Dec, ((MCP300_VC << 7) + 0x30), ptReg_vcache->LOCAL_BASE, local_base);
                    }
                }else{
                    /* always use the default value 0x10000 for LOCAL_BASE */
                }
                
                
#if CHK_VC_REF_EN_AT_SLICE1 == 0
                p_Vid->vreg_change = 3;
#endif
                LOG_PRINT_NPFX(LOG_INFO, "\n");
            }
            else
            {
                /* not the first slice of a frame */
                /* if ref 0 changed, disable it */
                if (0 != p_Vid->old_ref_addr_pa[0] && p_Vid->old_ref_addr_pa[0] != (uint32_t)p_Vid->refListLX[0][0]->refpic_addr_phy) {
                    AND_REG(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, 0xFFFFFFFE);
                    p_Vid->old_ref_addr_pa[0] = 0;
                    p_Vid->vreg_change |= 1;
                    LOG_PRINT(LOG_INFO, "change ref 0 (%08x)\n", (uint32_t)p_Vid->refListLX[0][0]->refpic_addr_phy);
                }
                
                /* check if ref 1 changed */

                /* get the addr for ref 1 */
                curr_ref_addr = 0;
                if (p_Vid->slice_hdr->slice_type == B_SLICE) {
                    curr_ref_addr = (uint32_t)p_Vid->refListLX[1][0]->refpic_addr_phy;
                } else if (p_Vid->list_size[0] > 1) {
                    curr_ref_addr = (uint32_t)p_Vid->refListLX[0][1]->refpic_addr_phy;
                } else if (0 != p_Vid->old_ref_addr_pa[1]) {
                    curr_ref_addr = 0;
                    LOG_PRINT(LOG_INFO, "disable ref 1\n");
                }   /* otherwise, reg ref 1 is not enabled before */

                /* if ref 1 changed, disable it */
                if (0 != p_Vid->old_ref_addr_pa[1] && p_Vid->old_ref_addr_pa[1] != curr_ref_addr) {
                    AND_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, 0xFFFFFFFE);
                    p_Vid->old_ref_addr_pa[1] = 0;
                    p_Vid->vreg_change |= 2;
                    LOG_PRINT(LOG_INFO, "change ref 1 (%08x)\n", curr_ref_addr);
                }
                LOG_PRINT(LOG_INFO, "vcache ref 0: %08x, ref1: %08x\n", ptReg_vcache->REF0_PARM, ptReg_vcache->REF1_PARM);
            }
        }
        else
        {
            /* disable VCACHE REF
             * 1. mbaff
             * 2. field
             * 3. I slice
             * 4. frame width > VCACHE_MAX_WIDTH or frame width < 176 
             */
#if 0
            if (0 != p_Vid->old_ref_addr_pa[0] || 0 != p_Vid->old_ref_addr_pa[1]) {
                p_Vid->vreg_change = 1;
                p_Vid->old_ref_addr_pa[0] = p_Vid->old_ref_addr_pa[1] = 0;
            }
#else
            if (0 != p_Vid->old_ref_addr_pa[0]) {
                p_Vid->vreg_change |= 1;
                p_Vid->old_ref_addr_pa[0] = 0;
            }
            if (0 != p_Vid->old_ref_addr_pa[1]) {
                p_Vid->vreg_change |= 2;
                p_Vid->old_ref_addr_pa[1] = 0;
            }
#endif
            AND_REG(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, 0xFFFFFFFE); /* clear bit 0: ref0_en */
            AND_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, 0xFFFFFFFE); /* clear bit 0: ref1_en */
            LOG_PRINT(LOG_INFO, "vcache disable\n");
        }
    }
    else
    {
        /* disable VCACHE REF */
#if 0
        if (0 != p_Vid->old_ref_addr_pa[0] || 0 != p_Vid->old_ref_addr_pa[1]) {
            p_Vid->vreg_change = 1;
            p_Vid->old_ref_addr_pa[0] = p_Vid->old_ref_addr_pa[1] = 0;
        }
#else
        if (0 != p_Vid->old_ref_addr_pa[0]) {
            p_Vid->vreg_change |= 1;
            p_Vid->old_ref_addr_pa[0] = 0;
        }
        if (0 != p_Vid->old_ref_addr_pa[1]) {
            p_Vid->vreg_change |= 2;
            p_Vid->old_ref_addr_pa[1] = 0;
        }
#endif
        AND_REG(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, 0xFFFFFFFE); /* clear bit 0: ref0_en */
        AND_REG(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, 0xFFFFFFFE); /* clear bit 0: ref1_en */
    }
    
    if(p_Vid->first_slice) {
        p_Dec->vcache_en_flgs = vcache_en_flgs;
    }

    //return 0;
}
#endif /* CONFIG_ENABLE_VCACHE */


/*
 * From set_meminfo in h264_vld_task.v
 */
static int set_meminfo(DecoderParams *p_Dec)
{
    volatile H264_reg_sys *ptReg_sys = (H264_reg_sys*)((uint32_t)p_Dec->pu32BaseAddr + H264_OFF_SYSM);
    VideoParams *p_Vid = p_Dec->p_Vid;
    uint32_t i;

    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x20), ptReg_sys->CUR_FRAME_BASE_R, ((uint32_t)p_Vid->dec_pic->refpic_addr_phy));
    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x00), ptReg_sys->CUR_FRAME_BASE_W, ((uint32_t)p_Vid->dec_pic->refpic_addr_phy));

    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x04), ptReg_sys->CUR_MB_INFO_BASE1, ((uint32_t)p_Vid->dec_pic->mbinfo_addr_phy));
    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x28), ptReg_sys->CUR_MB_INFO_BASE2, ((uint32_t)p_Vid->dec_pic->mbinfo_addr_phy));

    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x08), ptReg_sys->CUR_INTRA_BASE1, ((uint32_t)p_Dec->eng_info->intra_paddr));
    WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x24), ptReg_sys->CUR_INTRA_BASE2, ((uint32_t)p_Dec->eng_info->intra_paddr));
    
    //printk("mbinfo_addr_phy: %08X\n", ((uint32_t)p_Vid->dec_pic->mbinfo_addr_phy));
    //printk("pu8IntraBuffer_phy: %08X\n", ((uint32_t)p_Dec->pu8IntraBuffer_phy));

    /* set list 0 address and structure */
    for (i = 0; i < p_Vid->list_size[0]; i++) {
        WRITE_REG(p_Dec, ((MCP300_REF_L0 << 7) + i * 4), ptReg_sys->REFLIST0[i], (((uint32_t)p_Vid->refListLX[0][i]->refpic_addr_phy) | ((uint32_t)p_Vid->refListLX[0][i]->structure)));
        //printk("L0 ref %d: %x\n", i, ptReg_sys->REFLIST0[i]);
    }
    
    if (p_Vid->slice_hdr->slice_type == B_SLICE){
        /* set list 1 address and structure */
        for (i = 0; i < p_Vid->list_size[1]; i++) {
            WRITE_REG(p_Dec, ((MCP300_REF_L1 << 7) + i * 4), ptReg_sys->REFLIST1[i], (((uint32_t)p_Vid->refListLX[1][i]->refpic_addr_phy) | ((uint32_t)p_Vid->refListLX[1][i]->structure)));
            //printk("L1 ref %d: %x\n", i, ptReg_sys->REFLIST1[i]);
        }
        /* set mbinfo address */
        WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x2C), ptReg_sys->COL_REFPIC_MB_INFO, ((uint32_t)p_Vid->col_pic->mbinfo_addr_phy));
    }

    /* set scale output address */
    if (p_Dec->bScaleEnable) {
        WRITE_REG(p_Dec, ((MCP300_DMA << 7) + 0x14), ptReg_sys->SCALE_BASE, (uint32_t)p_Vid->pRecBuffer->dec_scale_buf_phy);
    }

#if CONFIG_ENABLE_VCACHE
    set_vcache(p_Dec);
#endif
    
    //dump_mem_register(p_Dec);
    
    return 0;
}

#if CONFIG_ENABLE_VCACHE
/*
 * wait until the changing of VCACHE REF takes effect
 * NOTE: It is requeired only when the VCACHE REF enable bits 
 *       are different from the previous slice in the multi-slice case
 */
static int __wait_vcache_set_done(DecoderParams *p_Dec)
{
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)(p_Dec->pu32VcacheBaseAddr);
    VideoParams *p_Vid = p_Dec->p_Vid;
    byte vcache_enable[2];

    /* wait vcache register setting done */
#if 0
    vcache_enable[0] = (p_Vid->old_ref_addr_pa[0]!=0 ? 1:0);
    vcache_enable[1] = (p_Vid->old_ref_addr_pa[1]!=0 ? 1:0);
    if (p_Vid->vreg_change) {
        int i, cnt = 0;
        while ((ptReg_vcache->REF0_PARM&0x01) != vcache_enable[0] || 
               (ptReg_vcache->REF1_PARM&0x01) != vcache_enable[1]) { /* polling one and polling zero */
            if (cnt > POLLING_ITERATION_TIMEOUT) {
            #ifdef LINUX
                printk("vcache setting timeout\n");
                dump_vcache_register(p_Dec, LOG_DEBUG);
                printk("old ref 0: %08x, old ref 1: %08x\n", p_Vid->old_ref_addr_pa[0], p_Vid->old_ref_addr_pa[1]);
            #endif
                return H264D_ERR_POLLING_TIMEOUT;
            }
            for (i = 0; i<100; i++);
            cnt++;
        }
    }
#else
    if (p_Vid->vreg_change & 1) { /* poll REF0 en bits only when it is changed */
        vcache_enable[0] = (p_Vid->old_ref_addr_pa[0]!=0 ? 1:0);
        if(vcache_enable[0]){
            POLL_REG_ONE(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, BIT0, 
            {
                LOG_PRINT(LOG_ERROR, "vcache setting timeout poll one for ref0_parm\n");
                dump_vcache_register((unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_DEBUG);
                LOG_PRINT(LOG_ERROR, "old ref 0: %08x, old ref 1: %08x\n", p_Vid->old_ref_addr_pa[0], p_Vid->old_ref_addr_pa[1]);
                return H264D_ERR_POLLING_TIMEOUT;
            });
        }else{
            POLL_REG_ZERO(p_Dec, ((MCP300_VC << 7) + 0x20), ptReg_vcache->REF0_PARM, ~BIT0, 
            {
                LOG_PRINT(LOG_ERROR, "vcache setting timeout poll zero for ref0_parm\n");
                dump_vcache_register((unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_DEBUG);
                LOG_PRINT(LOG_ERROR, "old ref 0: %08x, old ref 1: %08x\n", p_Vid->old_ref_addr_pa[0], p_Vid->old_ref_addr_pa[1]);
                return H264D_ERR_POLLING_TIMEOUT;
            });
        }
    }
        
    if (p_Vid->vreg_change & 2) { /* poll REF1 en bits only when it is changed */
        vcache_enable[1] = (p_Vid->old_ref_addr_pa[1]!=0 ? 1:0);
        if(vcache_enable[1]){
            POLL_REG_ONE(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, BIT0, 
            {
                LOG_PRINT(LOG_ERROR, "vcache setting timeout poll one for ref1_parm\n");
                dump_vcache_register((unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_DEBUG);
                LOG_PRINT(LOG_ERROR, "old ref 0: %08x, old ref 1: %08x\n", p_Vid->old_ref_addr_pa[0], p_Vid->old_ref_addr_pa[1]);
                return H264D_ERR_POLLING_TIMEOUT;
            });
        }else{
            POLL_REG_ZERO(p_Dec, ((MCP300_VC << 7) + 0x24), ptReg_vcache->REF1_PARM, ~BIT0, 
            {
                LOG_PRINT(LOG_ERROR, "vcache setting timeout poll zero for ref1_parm\n");
                dump_vcache_register((unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_DEBUG);
                LOG_PRINT(LOG_ERROR, "old ref 0: %08x, old ref 1: %08x\n", p_Vid->old_ref_addr_pa[0], p_Vid->old_ref_addr_pa[1]);
                return H264D_ERR_POLLING_TIMEOUT;
            });
        }
    }
#endif
    LOG_PRINT(LOG_DEBUG, "wait VC change reg done\n");
    p_Vid->vreg_change = 0;

    return H264D_OK;
}
#endif


/*
 * Clear HW's SRAM (required only once after power on)
 */
void decoder_eng_init_sram(DecoderEngInfo *eng_info)
{
    unsigned int base_addr = (unsigned int)eng_info->pu32BaseAddr;
    extern int clear_sram;
    
    if(clear_sram){
        __clear_sram_data(base_addr);
    }

    /* init lookup table: init it one time at module init is enough */
    memcpy((void *)(base_addr + TLU_OFFSET), look_up_table, sizeof(look_up_table));
}


/*
 * wait vcache write channel done 
 * NOTE: 1. This is required when (CONFIG_ENABLE_VCACHE and USE_LINK_LIST)
 *       2. It must be the first linked list command of a new frame
 */
#define RET_AT_TIMEOUT 1
static __inline int __wait_vcache_wch_done_ll(DecoderParams *p_Dec)
{
#if CONFIG_ENABLE_VCACHE && (USE_LINK_LIST == 1) && (WAIT_FR_DONE_CTRL != 0)
	volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)p_Dec->pu32VcacheBaseAddr;

    if(p_Dec->p_Vid->first_slice == 0) { /* skip this if not the first slice */
        return 0;
    }

#if WAIT_FR_DONE_CTRL == 1
    if(p_Dec->eng_info->vcache_ilf_enable && p_Dec->eng_info->prev_pic_done)
#elif WAIT_FR_DONE_CTRL == 2 /* always enabled */
#else
    #error invalid WAIT_FR_DONE_CTRL value
#endif /* WAIT_FR_DONE_CTRL == 1 */
    {
        POLL_REG_ONE(p_Dec, ((MCP300_VC << 7) + 0x04), ptReg_vcache->DECODE, BIT31,
        {
            LOG_PRINT(LOG_ERROR, "VCACHE wch done polling timeout\n");
#if TRACE_WAIT_FR_DONE_ERR // for checking if this error occurred
            LOG_PRINT(LOG_ERROR, "decoded frame:%d trigger:%d\n", p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
            while(1);
#endif
#if RET_AT_TIMEOUT
            return H264D_ERR_POLLING_TIMEOUT;
#endif
        });
    }

#endif /* CONFIG_ENABLE_VCACHE && (USE_LINK_LIST == 1) && (WAIT_FR_DONE_CTRL != 0) */

    return 0;
}


/*
 * decoder_eng_reset: Reset HW engine (also called "software reset")
 * NOTE: most of the mcp300/vcache registers will be reset to default value
 */
int decoder_eng_reset(DecoderEngInfo *eng_info)
{
    int i, cnt;
    int ret = 0;
    uint32_t tmp;
    volatile H264_reg_io *ptReg_io = (volatile H264_reg_io *)eng_info->pu32BaseAddr;
#if CONFIG_ENABLE_VCACHE
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)eng_info->pu32VcacheBaseAddr;
#endif
    unsigned long poll_start_jiffies;
    //extern unsigned int video_gettime_us(void); //functions defined in kernel
    extern int sw_reset_timeout_ret;
    
    /* reset mcp300 DMA command */
    LOG_PRINT(LOG_DEBUG, "MCP300 DMA reset start\n");
    ptReg_io->DEC_CMD0 = BIT5;  /* stop bitstream operation */
    ptReg_io->CPUCMD1 |= BIT2;  /* initiate DMA shut done procedure */
    cnt = 0;
    
    poll_start_jiffies = video_gettime_us();
    while (!(ptReg_io->DEC_STS0 & BIT26)) { /* wait DMA shutdown done */ /* polling one */
        if (cnt > POLLING_ITERATION_TIMEOUT) {
            __SET_DBG_MODE(1);
            LOG_PRINT(LOG_ERROR, "MCP300 DMA reset polling timeout:%lu cnt:%d\n", video_gettime_us() - poll_start_jiffies, cnt);
            LOG_PRINT(LOG_ERROR, "DEC_CMD0     :%08X\n", ptReg_io->DEC_CMD0);
            LOG_PRINT(LOG_ERROR, "CPUCMD1      :%08X\n", ptReg_io->CPUCMD1);
            LOG_PRINT(LOG_ERROR, "BITSYSBUFCNT :%08X\n", ptReg_io->BITSYSBUFCNT);
            dump_all_register((uint32_t)ptReg_io, (uint32_t)eng_info->pu32VcacheBaseAddr, LOG_ERROR);
            dump_param_register(NULL, (uint32_t)ptReg_io, LOG_ERROR);
            dump_ref_list_addr((uint32_t)ptReg_io, LOG_ERROR);

            LOG_PRINT(LOG_ERROR, "DEC_INTSTS   :%08X\n", ptReg_io->DEC_INTSTS);
            ptReg_io->DEC_INTSTS = ptReg_io->DEC_INTSTS; /* clear interrupt status */
            LOG_PRINT(LOG_ERROR, "DEC_INTSTS   :%08X cleared\n", ptReg_io->DEC_INTSTS);

            if(sw_reset_timeout_ret){
                return H264D_ERR_POLLING_TIMEOUT;
            }else{
                ret = H264D_ERR_POLLING_TIMEOUT;
                break;
            }
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
    LOG_PRINT(LOG_DEBUG, "MCP300 DMA reset stop\n");


#if CONFIG_ENABLE_VCACHE
    /* reset vcache DMA command */
	LOG_PRINT(LOG_DEBUG, "vcache DMA reset start\n");
    ptReg_vcache->V_ENC |= BIT4; /* disable dma */
    cnt = 0;
    while (!(ptReg_vcache->DECODE & BIT1)) { /* wait DMA shutdown done */ /* polling one */
        if (cnt > POLLING_ITERATION_TIMEOUT) {
            __SET_DBG_MODE(1);
    		LOG_PRINT(LOG_ERROR, "vcache DMA reset polling timeout\n");
            if(sw_reset_timeout_ret){
                return H264D_ERR_POLLING_TIMEOUT;
            }else{
                ret = H264D_ERR_POLLING_TIMEOUT;
                break;
            }
        }
        for (i = 0; i<100; i++);
        cnt++;
    }
	LOG_PRINT(LOG_DEBUG, "vcache DMA reset stop\n");
#endif /* CONFIG_ENABLE_VCACHE */

#if CONFIG_ENABLE_VCACHE
    /* reset vcache */
    ptReg_vcache->V_ENC |= BIT8; /* vcache sfotware reset */
    tmp = ptReg_vcache->V_ENC;
    tmp = ptReg_vcache->V_ENC;
	LOG_PRINT(LOG_DEBUG, "vcache reset done\n");
#endif /* CONFIG_ENABLE_VCACHE */

    /* reset MCP300 */
    ptReg_io->DEC_CMD0 = BIT15; /* mcp300 software reset */
    for (i = 0; i < 3; i++) /* dummy commands to delay 8 HW cycles */
        tmp = ptReg_io->DEC_CMD0;
	LOG_PRINT(LOG_DEBUG, "MCP300 software reset done\n");
    
    return ret;
}


/*
 * Init decoder HW register and its SRAM
 */
void decoder_eng_init_reg(DecoderEngInfo *eng_info, int has_mb_info)
{
    volatile H264_reg_io *ptReg_io = (volatile H264_reg_io *)eng_info->pu32BaseAddr;
#if CONFIG_ENABLE_VCACHE
    volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)eng_info->pu32VcacheBaseAddr;
#endif
    uint32_t tmp;
    uint32_t intr_mask;

#if CONFIG_ENABLE_VCACHE
    /* set initial value for vcache register */
    ptReg_vcache->V_ENC = CONFIG_ENABLE_VCACHE | (VCACHE_BLOCK_ENABLE << 1);
    ptReg_vcache->LOCAL_BASE = 0x00010000;
    ptReg_vcache->DECODE = 0x01;
    
    ptReg_vcache->REF2_PARM = 0;
    ptReg_vcache->REF3_PARM = 0;
    
    ptReg_vcache->DIS_ILF_PARM = 0;      /* VCACHE ILF disabled by default */
    ptReg_vcache->SYS_INFO = 0x00000260; /* VCACHE SYS_INFO disabled by default */
#endif
    
    /* set initial value for mcp300 register */
    ptReg_io->DEC_CMD2 = TIMEOUT_CLOCK; /* for timout interrupt */
    intr_mask = (INTERRUPT_FLAG << 8);
    tmp = (0x1 << 16) | intr_mask | 0x02;
    ptReg_io->DEC_CMD1 = tmp;


    /* clear SRAM for storing ref pic poc to make the dumpped mbinfo data of the first I-frame the same */
    if(has_mb_info) {
        __clear_sram_ref_pic_poc((uint32_t)ptReg_io); 
    }

	LOG_PRINT(LOG_DEBUG, "set init reg done\n");
    
}


/*
 * decoder_reset:
 * Do sw reset and re-init register/SRAM for the engine used by this channel
 */
int decoder_reset(DecoderParams *p_Dec)
{
    int ret;

#if TRACE_SW_RST
    volatile H264_reg_io *ptReg_io = (volatile H264_reg_io *)p_Dec->pu32BaseAddr;
    unsigned int dec_intsts = ptReg_io->DEC_INTSTS;
    //__SET_DBG_MODE(1);
	LOG_PRINT(LOG_ERROR, "init_params enter\n");
#endif

    ret = decoder_eng_reset(p_Dec->eng_info);
    if(ret){
#if TRACE_SW_RST
        dump_list(p_Dec, 1);
        LOG_PRINT(LOG_ERROR, "decoder_reset return: %d\n", ret);
        LOG_PRINT(LOG_ERROR, "DEC_INTSTS: %08X bf reset\n", dec_intsts);
        LOG_PRINT(LOG_ERROR, "DEC_INTSTS: %08X last\n", p_Dec->u32IntStsPrev);
        //LOG_PRINT(LOG_ERROR, "in_interrupt(): %d\n", in_interrupt());
        LOG_PRINT(LOG_ERROR, "chn: %d\n", p_Dec->chn_idx);
        LOG_PRINT(LOG_ERROR, "u32DecodedFrameNumber: %d\n", p_Dec->u32DecodedFrameNumber);
        LOG_PRINT(LOG_ERROR, "u32DecodeTriggerNumber: %d\n", p_Dec->u32DecodeTriggerNumber);
        if(p_Dec->pfnDamnit){
            p_Dec->pfnDamnit("eng_reset error");
        }
#endif
        return ret;
    }

    decoder_eng_init_reg(p_Dec->eng_info, p_Dec->bHasMBinfo);

    //dump_all_register(p_Dec);
    
#if TRACE_SW_RST
    LOG_PRINT(LOG_ERROR, "decoder_reset return\n");
#endif
    return H264D_OK;
}



#if USE_SW_PARSER == 0
/*
 * Check bitsteam empty flag (both in register and received interrupt flags)
 */
int decoder_bitstream_empty(DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)(p_Dec->pu32BaseAddr);
    int ret;
    
    /*
     * if bitstream buffer emtpy occurred in ISR, 
     * (p_Dec->u32IntSts & INT_BS_EMPTY) may not be set, so DEC_INTSTS is also checked
     */
    ret = (p_Dec->u32IntSts & INT_BS_EMPTY) | (ptReg_io->DEC_INTSTS & BIT1);
    //ret = (ptReg_io->DEC_INTSTS & BIT1);
    
    return ret;
}
#endif


/*
 * allocate memory for DecoderParams
 */
void *decoder_create(const FAVCD_DEC_INIT_PARAM *ptParam, int ndev)
{
    DecoderParams *p_Dec;
    VideoParams *p_Vid;
    uint32_t i;

    MALLOC_PTR_dec pfnMalloc = ptParam->pfnMalloc;
    FREE_PTR_dec   pfnFree = ptParam->pfnFree;

    if(pfnMalloc == NULL || pfnFree == NULL){
        E_DEBUG("null allocate/free function pointer\n");
        return NULL;
    }

    if ((p_Dec = (DecoderParams*)pfnMalloc(sizeof(DecoderParams)))==NULL){
        return NULL;
    }
    memset(p_Dec, 0, sizeof(*p_Dec));

    /* copy function pointers */
    p_Dec->pfnMalloc = pfnMalloc;
    p_Dec->pfnDamnit = ptParam->pfnDamnit;
    p_Dec->pfnFree = pfnFree; /* register free functon pointer */
    p_Dec->pfnMarkEngStart = ptParam->pfnMarkEngStart;
    
    p_Dec->chn_idx = ndev;
    p_Dec->vcache_en_cfgs = (ENABLE_VCACHE_REF << 2)|(ENABLE_VCACHE_SYSINFO << 1)|ENABLE_VCACHE_ILF;
    
    if ((p_Vid = (VideoParams*)pfnMalloc(sizeof(VideoParams)))==NULL)
        goto decoder_create_err;
    memset(p_Vid, 0, sizeof(*p_Vid));
    
    p_Dec->p_Vid = p_Vid;
    p_Vid->dec_handle = p_Dec;

    
    for (i = 0; i < MAX_SPS_NUM; i++){
        if(i != 0){
            p_Vid->SPS[i] = NULL;
            continue;
        }
        p_Vid->SPS[i] = pfnMalloc(sizeof(*p_Vid->SPS[i]));
        if(p_Vid->SPS[i] == NULL){
            goto decoder_create_err;
        }
        memset(p_Vid->SPS[i], 0, sizeof(*p_Vid->SPS[i]));
        //printk("allocate SPS[%d]:%08X\n", i, p_Vid->SPS[i]);
    }

    
    for (i = 0; i < MAX_PPS_NUM; i++){
        if(i != 0){
            p_Vid->PPS[i] = NULL;
            continue;
        }
        p_Vid->PPS[i] = pfnMalloc(sizeof(*p_Vid->PPS[i]));
        if(p_Vid->PPS[i] == NULL){
            goto decoder_create_err;
        }
        memset(p_Vid->PPS[i], 0, sizeof(*p_Vid->PPS[i]));
        //printk("allocate PPS[%d]:%08X\n", i, p_Vid->PPS[i]);
    }
    
    if ((p_Vid->slice_hdr = (SliceHeader*)pfnMalloc(sizeof(SliceHeader)))==NULL)
        goto decoder_create_err;
    
    if ((p_Vid->listD = (DecodedPictureBuffer*)pfnMalloc(sizeof(DecodedPictureBuffer)))==NULL)
        goto decoder_create_err;
    memset(p_Vid->listD, 0, sizeof(*p_Vid->listD));




    for (i = 0; i<MAX_DPB_SIZE; i++){
        if ((p_Vid->listD->fs[i] = (FrameStore*)pfnMalloc(sizeof(FrameStore)))==NULL)
            goto decoder_create_err;
        memset(p_Vid->listD->fs[i], 0, sizeof(*p_Vid->listD->fs[i]));

        if ((p_Vid->listD->fs[i]->frame = (StorablePicture*)pfnMalloc(sizeof(StorablePicture)))==NULL)
            goto decoder_create_err;
        if ((p_Vid->listD->fs[i]->topField = (StorablePicture*)pfnMalloc(sizeof(StorablePicture)))==NULL)
            goto decoder_create_err;
        if ((p_Vid->listD->fs[i]->btmField = (StorablePicture*)pfnMalloc(sizeof(StorablePicture)))==NULL)
            goto decoder_create_err;
    }

    if ((p_Vid->dec_pic = (StorablePicture*)pfnMalloc(sizeof(StorablePicture)))==NULL)
        goto decoder_create_err;
    if ((p_Vid->gap_pic = (StorablePicture*)pfnMalloc(sizeof(StorablePicture)))==NULL)
        goto decoder_create_err;

    //p_Dec->u32DecodedFrameNumber = 0;
    return p_Dec;
    
decoder_create_err:
    decoder_destroy(p_Dec);

    return NULL;

}

/*
 * Start of functions for reseting DecoderParams
 */
 
static void __reset_frame_store(FrameStore *fs)
{
    unsigned int offset;
    unsigned int size;

    offset = ((unsigned int)&fs->frame_num - (unsigned int)fs);
    size = sizeof(*fs) - offset;

    //printk("__reset_frame_store: offset:%d size:%d\n", offset, size);
    memset(&fs->frame_num, 0, size); // clear non-reserved fields to zero

    memset(fs->frame, 0, sizeof(StorablePicture));
    memset(fs->topField, 0, sizeof(StorablePicture));
    memset(fs->btmField, 0, sizeof(StorablePicture));

}


static void __reset_dpb(DecodedPictureBuffer *listD)
{
    unsigned int offset;
    unsigned int size;
    int i;

    offset = ((unsigned int)&listD->fs_stRef - (unsigned int)listD);
    size = sizeof(*listD) - offset;

    //printk("__reset_dpb: offset:%d size:%d\n", offset, size);
    memset(&listD->fs_stRef, 0, size); // clear non-reserved fields to zero

    for(i = 0; i < MAX_DPB_SIZE; i++){
        __reset_frame_store(listD->fs[i]);
    }
}


static void __reset_video_parameter(VideoParams *p_Vid)
{
    unsigned int offset;
    unsigned int size;
    int i;

    offset = ((unsigned int)&p_Vid->nalu - (unsigned int)p_Vid);
    size = sizeof(*p_Vid) - offset;

    //printk("__reset_video_parameter: offset:%d size:%d\n", offset, size);

    memset(&p_Vid->nalu, 0, size); // clear non-reserved fields to zero

    // clear allocated memory to zero in reserved fieleds
    for (i = 0; i< MAX_SPS_NUM; i++) {
        if (p_Vid->SPS[i])
            memset(p_Vid->SPS[i], 0, sizeof(SeqParameterSet));
    }
    for (i = 0; i < MAX_PPS_NUM; i++) {
        if (p_Vid->PPS[i])
            memset(p_Vid->PPS[i], 0, sizeof(PicParameterSet));
    }

    memset(p_Vid->dec_pic, 0, sizeof(StorablePicture));
    memset(p_Vid->gap_pic, 0, sizeof(StorablePicture));
    memset(p_Vid->slice_hdr, 0, sizeof(SliceHeader));

    //memset(p_Vid->listD, 0, sizeof(DecodedPictureBuffer));

    __reset_dpb(p_Vid->listD);


}


/*
 * Initialize fields in DecoderParams, including the allocated memory
 */
static void __reset_parameter(DecoderParams *p_Dec)
{
    unsigned int offset;
    unsigned int size;

    offset = ((unsigned int)&p_Dec->u16MaxWidth - (unsigned int)p_Dec);
    size = sizeof(*p_Dec) - offset;

    //printk("__reset_parameter: offset:%d size:%d\n", offset, size);

    memset(&p_Dec->u16MaxWidth, 0, size); // clear non-reserved fields to zero

    __reset_video_parameter(p_Dec->p_Vid);

}

/*
 * End of functions for reseting DecoderParams
 */

/*
 * set decoder flags
 */
int decoder_set_flags(DecoderParams *p_Dec, enum decoder_flag flag)
{
    switch(flag){
        case DEC_WAIT_IDR:
            p_Dec->p_Vid->first_idr_found = 0;
            break;
        case DEC_SKIP_TIL_IDR_DUE_TO_ERR: /* some error occured, skip til the next IDR */
            p_Dec->p_Vid->skip_till_idr = 1;
            p_Dec->p_Vid->first_idr_found = 1;
            break;
        default:
            return 1;
    }
    return 0;
}

/*
 * skip bitstreams until the next IDR
 * NOTE: print proper message according to flags
 */
int decoder_skip_til_idr(DecoderParams *p_Dec)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    
    if((p_Vid->first_idr_found == 0) || (p_Vid->skip_till_idr == 1)){
        if(p_Vid->idr_flag){
            p_Vid->first_idr_found = 1;
            p_Vid->skip_till_idr = 0;
    
            if(p_Dec->u16SkipTillIDRLast != 0){
                E_DEBUG("ch %d skip %d frame until IDR slice since the last error\n", p_Dec->chn_idx, p_Dec->u16SkipTillIDRLast);
                p_Dec->u16SkipTillIDRLast = 0;
            }
        }else{
            if((p_Vid->first_idr_found == 0)){
                E_DEBUG("first coded slice is not an IDR slice\n");
                DUMP_MSG(D_ERR, p_Dec, "first coded slice is not an IDR slice\n");
                p_Dec->u16WaitIDR++;
                return H264D_NO_IDR;
            }
            if((p_Vid->skip_till_idr == 1)){
                W_DEBUG("skip coded slice till an IDR slice\n");
                DUMP_MSG(D_ERR, p_Dec, "skip coded slice till an IDR slice\n");
                p_Dec->u16SkipTillIDR++;
                p_Dec->u16SkipTillIDRLast++;
                return H264D_SKIP_TILL_IDR;
            }
        }
    }

    return 0;
}



/*
 * set scale options
 */
int decoder_set_scale(FAVC_DEC_FR_PARAM *ptrDEC, DecoderParams *p_Dec)
{
    /* check options */
    if (ptrDEC->bScaleEnable && 
        ((ptrDEC->u8ScaleRow != 2 && ptrDEC->u8ScaleRow != 4) || 
         (ptrDEC->u8ScaleColumn != 2 && ptrDEC->u8ScaleColumn != 4))) {
        W_DEBUG("scale row and column must be 2 or 4 (scale row = %d, scale column = %d)\n", 
            ptrDEC->u8ScaleRow, ptrDEC->u8ScaleColumn);
        return H264D_PARAMETER_ERROR;
    }

    /* set options */
    p_Dec->bScaleEnable = (ptrDEC->bScaleEnable?1:0);
    p_Dec->u8ScaleType = (ptrDEC->u8ScaleType?1:0);
    p_Dec->u8ScaleSize = (((ptrDEC->u8ScaleRow == 4 ? 1:0) << 1) | 
                           (ptrDEC->u8ScaleColumn == 4 ? 1:0));
    p_Dec->u16ScaleYuvWidthThrd = ptrDEC->u16ScaleYuvWidthThrd;
    
    return H264D_OK;
}


/*
 * bind a engine to a channel
 */
int decoder_bind_engine(DecoderParams *p_Dec, DecoderEngInfo *eng_info)
{
    DUMP_MSG(D_ENG, p_Dec, "bind ch %d to engine %d\n", p_Dec->chn_idx, eng_info->engine);

#if 1
    //FIXME: redundant. should be removed?
    /* set register address */
    p_Dec->pu32BaseAddr = (void *)eng_info->pu32BaseAddr;
    p_Dec->pu32VcacheBaseAddr = (void *)eng_info->pu32VcacheBaseAddr;
#endif


#if USE_LINK_LIST
    decoder_set_ll(p_Dec, eng_info->codec_ll[0]);
#endif

    p_Dec->eng_info = eng_info;

#if USE_MULTICHAN_LINK_LIST == 0
    /* NOTE: must check if the engine is used by other channel first, 
              otherwise the ISR or SW timeout handler may get the wrong chn_idx */
    if(eng_info->chn_idx != -1){
        LOG_PRINT(LOG_ERROR, "decoder_bind_engine for chn %d failed: engine %d is already used chn:%d\n", p_Dec->chn_idx, eng_info->engine, eng_info->chn_idx);
    }
#endif
    eng_info->chn_idx = p_Dec->chn_idx;

    return 0;
}


/*
 * unbind a channel form a engine
 */
int decoder_unbind_engine(DecoderParams *p_Dec)
{
    DecoderEngInfo *eng_info = p_Dec->eng_info;

    if(eng_info == NULL){
        LOG_PRINT(LOG_ERROR, "decoder_unbind_engine for chn %d failed: null eng_info\n", p_Dec->chn_idx);
        return -1;
    }

    DUMP_MSG(D_ENG, p_Dec, "unbind ch %d to engine %d (prev bound to:%d)\n", p_Dec->chn_idx, eng_info->engine, eng_info->chn_idx);


#if USE_MULTICHAN_LINK_LIST == 0
    eng_info->chn_idx = -1;
#endif
    p_Dec->eng_info = NULL;

    return 0;
}


#if USE_LINK_LIST
/* 
 * Set link list to be used
 */
int decoder_set_ll(DecoderParams *p_Dec, struct codec_link_list *codec_ll)
{
    p_Dec->codec_ll = codec_ll;

    return 0;
}
#endif


/*
 * Initialize and set the private data structure (ptDecHandle) according to parameter (ptrDEC)
 */
int decoder_reset_param(FAVC_DEC_PARAM *ptrDEC, DecoderParams *p_Dec)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    int ret;

	/* initialize DecHandle */
    __reset_parameter(p_Dec);

	/* set DecoderParams fields accroding to parameter */
    p_Vid->max_buffered_num = ptrDEC->u8MaxBufferedNum;
    p_Dec->bChromaInterlace = ptrDEC->bChromaInterlace;
    p_Dec->bUnsupportBSlice = ptrDEC->bUnsupportBSlice;
    p_Dec->bOutputMBinfo = ptrDEC->bOutputMBinfo;
    p_Dec->vcache_en_cfgs = ptrDEC->bVCCfg;
    p_Dec->u8LosePicHandleFlags = ptrDEC->u8LosePicHandleFlags;
    if(p_Dec->vcache_en_cfgs & VCACHE_SYSINFO_EN){
        p_Dec->bHasMBinfo = (!p_Dec->bUnsupportBSlice) || p_Dec->bOutputMBinfo;
    }else{
        p_Dec->bHasMBinfo = 1; /* always need mbinfo buffer if VCACHE SYSINFO is disabled */
    }

    /* set scale options */
    ret = decoder_set_scale(&ptrDEC->fr_param, p_Dec);
    if(ret != H264D_OK){
        return ret;
    }

    LOG_PRINT(LOG_DEBUG, "bHasMBinfo:%d bOutputMBinfo:%d bUnsupportBSlice:%d bScaleEnable:%d u8ScaleType:%d u8ScaleSize:%d\n", 
        p_Dec->bHasMBinfo,
        p_Dec->bOutputMBinfo,
        p_Dec->bUnsupportBSlice,
        p_Dec->bScaleEnable,
        p_Dec->u8ScaleType,
        p_Dec->u8ScaleSize);

    p_Vid->output_poc = INT_MIN;
    
#if OUTPUT_POC_MATCH_STEP
    p_Vid->poc_step = 1;
    p_Vid->even_counter = 0;
#endif

    p_Dec->u16MaxWidth = ptrDEC->u16MaxWidth;
    p_Dec->u16MaxHeight = ptrDEC->u16MaxHeight;

    p_Dec->u16FrameWidth = 0;   // not set
    p_Dec->u16FrameHeight = 0;

    p_Vid->old_ref_addr_pa[0] = 0xFFFFFFFF;
    p_Vid->old_ref_addr_pa[1] = 0xFFFFFFFF;

    p_Dec->multislice = 0;

    return H264D_OK;
}


/* 
 * Free all allocated memroy in dec_handle
 */
void decoder_destroy(DecoderParams *p_Dec)
{
    VideoParams *p_Vid;
    uint32_t i;
    FREE_PTR_dec pfnFree;

    if (p_Dec){
        pfnFree = p_Dec->pfnFree;
		if(pfnFree == NULL){
		    E_DEBUG("invalid free function pointer\n");
            return;
		}
        p_Vid = p_Dec->p_Vid;
        if (p_Vid){
            if (p_Vid->dec_pic)
                pfnFree(p_Vid->dec_pic);
            if (p_Vid->gap_pic)
                pfnFree(p_Vid->gap_pic);

            if (p_Vid->listD){
                for (i = 0; i<MAX_DPB_SIZE; i++){
                    if (p_Vid->listD->fs[i]->frame)
                        pfnFree(p_Vid->listD->fs[i]->frame);
                    if (p_Vid->listD->fs[i]->topField)
                        pfnFree(p_Vid->listD->fs[i]->topField);
                    if (p_Vid->listD->fs[i]->btmField)
                        pfnFree(p_Vid->listD->fs[i]->btmField);

                    if (p_Vid->listD->fs[i])
                        pfnFree(p_Vid->listD->fs[i]);
                }
                pfnFree(p_Vid->listD);
            }

            if (p_Vid->slice_hdr){
                pfnFree(p_Vid->slice_hdr);
            }
            for (i = 0; i<MAX_SPS_NUM; i++)
                if (p_Vid->SPS[i])
                    pfnFree(p_Vid->SPS[i]);
            for (i = 0; i<MAX_PPS_NUM; i++)
                if (p_Vid->PPS[i])
                    pfnFree(p_Vid->PPS[i]);
            pfnFree(p_Vid);
        }
        pfnFree(p_Dec);
    }
}



static __inline int __chk_unhandled_irq(DecoderParams *p_Dec)
{
#if CHK_UNHANDLED_IRQ
    unsigned int flags;
    flags = p_Dec->u32IntSts;

    if(flags){
        printk("IRQ unhandled: {\n");
        if(flags & INT_LINK_LIST_ERR)
            printk("linked list err\n");
        if(flags & INT_DMA_W_ERR)
            printk("DMA write err\n");
        if(flags & INT_DMA_R_ERR)
            printk("DMA read err\n");
        if(flags & INT_FRAME_DONE)
            printk("frame done\n");
        if(flags & INT_HW_TIMEOUT)
            printk("hw timeout\n");
        if(flags & INT_BS_LOW)
            printk("bs low\n");
        if(flags & INT_BS_EMPTY)
            printk("bs empty\n");
        if(flags & INT_SLICE_DONE)
            printk("slice done\n");
        printk("}\n");
    }
#endif
    return 0;
}


/*
 * clear all received interrupt flags
 */
void decoder_clear_irq_flags(DecoderParams *p_Dec)
{
    //printk("decoder_clear_irq_flags:%08X\n", p_Dec->u32IntSts);
    p_Dec->u32IntSts = 0;
}


/*
 *  Handling received interrupt flags
 *  BIT0: slice done
 *  BIT1: bitstream buffer empty
 *  BIT2: bitstream buffer lower then thd
 *  BIT3: timeout
 *  BIT4: frame done
 *  BIT5: dma read error
 *  BIT6: dma write error   
 * \return
 *    < 0: some errors occurred
 *         H264D_LINKED_LIST_ERROR
 *         H264D_ERR_VLD_ERROR
 *         H264D_ERR_HW_BS_EMPTY
 *         H264D_ERR_POLLING_TIMEOUT (timeout at sw reset)
 *         H264D_ERR_TIMEOUT_EMPTY (timeout and bitstream empty)
 *         H264D_ERR_TIMEOUT_VLD (timeout and vld error)
 *         H264D_ERR_TIMEOUT (timeout)
 *         H264D_ERR_UNKNOWN (unknown)
 *         
 *    ==0: H264D_OK (no eror, or something that can be ignored. Decoding process is still ongoing, no need to trigger anain)
 *    > 0: 
 *         SLICE_DONE: one slice is done
 */
static int __handle_received_interrupt(DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    int ret;


    if (p_Dec->u32IntSts & (INT_DMA_R_ERR|INT_DMA_W_ERR)) {
        /* NOTE: no need to handle or report DMA error,
                 since the DMA error does not affect the normal decoding 
                 (according to the burn-in result) */
        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "h264 dec DMA read/write error: %d\n", (p_Dec->u32IntSts & (INT_DMA_R_ERR|INT_DMA_W_ERR)) >> 5);
#if PRINT_DMA_ERROR
        LOG_PRINT(LOG_ERROR, "chn: %d\n", p_Dec->chn_idx);
        LOG_PRINT(LOG_ERROR, "engine: %d\n", p_Dec->eng_info->engine);
        LOG_PRINT(LOG_ERROR, "u32DecodedFrameNumber: %d\n", p_Dec->u32DecodedFrameNumber);
        LOG_PRINT(LOG_ERROR, "u32DecodeTriggerNumber: %d\n", p_Dec->u32DecodeTriggerNumber);
        LOG_PRINT(LOG_ERROR, "jiffies: %lu\n", jiffies);
#endif
        __SET_DBG_MODE(0);

        if(p_Dec->u32IntSts & INT_DMA_R_ERR)
            p_Dec->u16DmaRError++;
        if(p_Dec->u32IntSts & INT_DMA_W_ERR)
            p_Dec->u16DmaWError++;

        p_Dec->u32IntSts &= ~(INT_DMA_R_ERR|INT_DMA_W_ERR);
        if(p_Dec->u32IntSts == 0){ /* return if no other interrupt */
            return H264D_OK;
        }
    }

    
#if USE_LINK_LIST
    if (p_Dec->u32IntSts & INT_LINK_LIST_ERR) {
        p_Dec->u32IntSts &= ~INT_LINK_LIST_ERR;
        p_Dec->u16LinkListError++;
        
        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "h264 dec linked list error: %08X\n", ptReg_io->RESERVED0);
        LOG_PRINT(LOG_ERROR, "exec_cmd_cnt: %d\n", (ptReg_io->RESERVED0 >> 16) & ((1 << 13) - 1));
        LOG_PRINT(LOG_ERROR, "job_id: %d\n", (ptReg_io->RESERVED0 >> 5) & ((1 << 11) - 1));
        LOG_PRINT(LOG_ERROR, "err st:%s %s %s\n", 
            (ptReg_io->RESERVED0 & 1)?"[command format incorrect]":"", 
            (ptReg_io->RESERVED0 & 2)?"[timeout]":"", 
            (ptReg_io->RESERVED0 & 4)?"[done]":"" 
        );
        dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_ERROR);
        
        __SET_DBG_MODE(0);
        
        decoder_reset(p_Dec);
        __chk_unhandled_irq(p_Dec);
        return H264D_LINKED_LIST_ERROR;
    }
#endif

    
    if (p_Dec->u32IntSts & INT_SLICE_DONE) {
        p_Dec->u32IntSts &= ~INT_SLICE_DONE;
        p_Dec->u16SliceDone++;
        
        if (ptReg_io->DEC_STS0 & 0x0FC0) {
            /* one of the typical error path */
            LOG_PRINT(LOG_ERROR, "h264 dec vld error: %x\n", ptReg_io->DEC_STS0);
            LOG_PRINT(LOG_ERROR, "decoded frame number: %d trigger number: %d\n", p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
            p_Dec->error_sts = ptReg_io->DEC_STS0;
            p_Dec->buf_cnt = ptReg_io->BITSYSBUFCNT * 64;
            p_Dec->buf_offset = ptReg_io->BITSYSBUFADR - (unsigned int)p_Dec->pu8Bitstream_phy;
            p_Dec->u16VldError++;
#if DBG_SW_PARSER
            sw_parser_dump(&p_Dec->stream);
#endif
            dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_ERROR);
            dump_param_register(p_Dec, (unsigned int)p_Dec->pu32BaseAddr, LOG_ERROR);

            decoder_reset(p_Dec);
            __chk_unhandled_irq(p_Dec);
            return H264D_ERR_VLD_ERROR;
        }
        /* the normal case return */
        __chk_unhandled_irq(p_Dec);
        return SLICE_DONE;
    }

    if (p_Dec->u32IntSts & INT_BS_EMPTY) {
        p_Dec->u32IntSts &= ~INT_BS_EMPTY;
        p_Dec->u16BSEmpty++;

#if USE_RELOAD_BS
        if(p_Dec->u8ReloadFlg){ /* ignore bitsteam empty when reload bitstream is expected */
            ret = __load_more_bitstream(p_Dec);
            if(ret == H264D_OK){
                return H264D_OK;
            }
        }
#endif        

        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "h264 dec bitstream empty\n");
        LOG_PRINT(LOG_ERROR, "current mb cnt = %d\n", ptReg_io->DEC_PARAM5);
        
        decoder_reset(p_Dec);
        __chk_unhandled_irq(p_Dec);
        __SET_DBG_MODE(0);
        return H264D_ERR_HW_BS_EMPTY;
    }
    
    if (p_Dec->u32IntSts & INT_BS_LOW) {
        p_Dec->u32IntSts &= ~INT_BS_LOW;
        p_Dec->u16BSLow++;

        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "h264 dec bitstream empty\n");
        __chk_unhandled_irq(p_Dec);
        __SET_DBG_MODE(0);
        return H264D_ERR_BS_EMPTY;
    }
    
    if (p_Dec->u32IntSts & INT_HW_TIMEOUT) {
        p_Dec->u32IntSts &= ~INT_HW_TIMEOUT;
        p_Dec->u16Timeout++;
        
        __SET_DBG_MODE(1);
        p_Dec->error_sts = ptReg_io->DEC_STS0;
        
        /* one of the typical error path */
        if (!(ptReg_io->BITCTL & BIT0)) {
            p_Dec->u16BSEmpty++;
            
            LOG_PRINT(LOG_ERROR, "h264 dec hardware time out & bitstream empty: %08X\n", p_Dec->error_sts);
            if ((ret = decoder_reset(p_Dec)) < 0){
                __chk_unhandled_irq(p_Dec);
                return ret;
            }
            __chk_unhandled_irq(p_Dec);
            return H264D_ERR_TIMEOUT_EMPTY;
        }

        /* VLD error */
        if (p_Dec->error_sts & 0x0FC0) {
            p_Dec->u16VldError++;
            LOG_PRINT(LOG_ERROR, "h264 dec hardware time out & vld error: %08X\n", p_Dec->error_sts);
            dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_ERROR);
            dump_param_register(p_Dec, (unsigned int)p_Dec->pu32BaseAddr, LOG_ERROR);
            if ((ret = decoder_reset(p_Dec)) < 0){
                __chk_unhandled_irq(p_Dec);
                return ret;
            }
            __chk_unhandled_irq(p_Dec);
            return H264D_ERR_TIMEOUT_VLD;
        }

        /* hardware timeout without other errors */
        LOG_PRINT(LOG_ERROR, "h264 dec hardware time out: %08X\n", p_Dec->error_sts);
        dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_ERROR);
        dump_param_register(p_Dec, (unsigned int)p_Dec->pu32BaseAddr, LOG_ERROR);
        if ((ret = decoder_reset(p_Dec)) < 0){
            __chk_unhandled_irq(p_Dec);
            return ret;
        }
        __chk_unhandled_irq(p_Dec);
        return H264D_ERR_TIMEOUT;
    }
    
    LOG_PRINT(LOG_ERROR, "unknown error happened: %x %x\n", p_Dec->u32IntSts, p_Dec->u32IntStsPrev);
    
    
    return H264D_ERR_UNKNOWN;
}


/*!
 ************************************************************************
 * \brief
 *    Reads new slice from bit_stream
 ************************************************************************
 */
static int read_new_slice(DecoderParams *p_Dec, unsigned char no_more_bs)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    int ret;

    //W_DEBUG("read_new_slice enter\n");

    for (;;)
    {
        LOG_PRINT(LOG_TRACE, "get_next_nalu calling\n");
        p_Vid->nalu.nal_unit_type = 0; /* clear it */
        
        PERF_MSG("GET_NEXT_NALU >\n");
        ret = GET_NEXT_NALU(p_Dec, &p_Vid->nalu, no_more_bs);
        PERF_MSG("GET_NEXT_NALU <\n");
        LOG_PRINT(LOG_TRACE, "get_next_nalu return %d\n", ret);
#if USE_SW_PARSER
        LOG_PRINT(LOG_TRACE, "curr byte: %d  bs size: %d\n", p_Dec->stream.cur_byte - p_Dec->stream.streamBuffer, p_Dec->stream.totalLength);
#endif

        if (ret < 0){
            DUMP_MSG(D_NPF, p_Dec, "curr state: %d ret:%d nalu type:%d\n", p_Dec->u8CurrState, ret, p_Vid->nalu.nal_unit_type);
            /* detect NON_PAIRED_FIELD */
            if (p_Dec->u8CurrState == FIELD_DONE) {
                if(ret == H264D_ERR_BS_EMPTY && p_Vid->nalu.nal_unit_type == NALU_TYPE_EOS_MARK)
                    return H264D_NON_PAIRED_FIELD;
            }

            if(ret == H264D_ERR_BS_EMPTY){
                return H264D_NO_START_CODE;
            }
#if DBG_SW_PARSER
            printk("cannot get next nalu\n");
            sw_parser_dump(&p_Dec->stream);
#endif
            return ret;
        }
        LOG_PRINT(LOG_INFO, "NAL type:%d ref:%d\n", p_Vid->nalu.nal_unit_type, p_Vid->nalu.nal_ref_idc);
#if DBG_SW_PARSER
        printk("NAL type:%d\n", p_Vid->nalu.nal_unit_type);
        sw_parser_dump(&p_Dec->stream);
#endif
        
        switch (p_Vid->nalu.nal_unit_type)
        {
        case NALU_TYPE_SLICE:
        case NALU_TYPE_IDR:
            p_Vid->idr_flag = (p_Vid->nalu.nal_unit_type == NALU_TYPE_IDR);
            if (p_Vid->idr_flag){
                DUMP_MSG(D_NALU, p_Dec, "-- IDR slice %d --\n", p_Vid->nalu.nal_ref_idc);
            }else{
                DUMP_MSG(D_NALU, p_Dec, "-- slice %d --\n", p_Vid->nalu.nal_ref_idc);
            }
            
            /* check whether to skip til IDR */
            ret = decoder_skip_til_idr(p_Dec);
            if(ret < 0){
                return ret;
            }

            if (p_Vid->idr_flag && p_Vid->listD->last_picture) {
            #if 1//def DUMP_ERROR_INFO
                E_DEBUG("idr slice but there is composited field\n");
            #endif
                //return H264D_PARSING_ERROR;
                return H264D_NON_PAIRED_FIELD_ERROR;
            }


            PERF_MSG("read_slice_header >\n");
            ret = read_slice_header(p_Dec, p_Vid, p_Vid->slice_hdr);
            PERF_MSG("read_slice_header <\n");
            if (ret < 0) {
                #if DBG_SW_PARSER
                printk("read_slice_header() return error:%d\n", ret);
                #endif
                E_DEBUG("read slice header error\n");
                if (p_Vid->listD->last_picture && p_Vid->idr_flag)
                    return H264D_NON_PAIRED_FIELD_ERROR;
                return ret;
            }
            
            if(BS_EMPTY(p_Dec)){
#if DBG_SW_PARSER
                printk("bs empty after read_slice_header()\n");
                sw_parser_dump(&p_Dec->stream);
#endif
                return H264D_ERR_BS_EMPTY;
            }
            
            if (p_Dec->u32IntSts & INT_BS_LOW)
                return H264D_BS_LOW;

#if USE_LINK_LIST && (SW_LINK_LIST == 0)
            if(p_Vid->MbaffFrameFlag){
                E_DEBUG("Mbaff is not supported in HW linked list mode\n");
                return H264D_ERR_HW_UNSUPPORT;
            }
#endif
            return H264D_OK;
            break;
        case NALU_TYPE_SPS:
            DUMP_MSG(D_NALU, p_Dec, "-- SPS --\n");
            PERF_MSG("readSPS >\n");
            ret = readSPS(p_Dec, p_Vid);
            PERF_MSG("readSPS <\n");
            if (ret < 0)
                return ret;
            break;
        case NALU_TYPE_PPS:
            DUMP_MSG(D_NALU, p_Dec, "-- PPS --\n");
            PERF_MSG("readPPS >\n");
            ret = readPPS(p_Dec, p_Vid);
            PERF_MSG("readPPS <\n");
            if (ret < 0)
                return ret;
            break;
        case NALU_TYPE_DPA:
        case NALU_TYPE_DPB:
        case NALU_TYPE_DPC:
            //E_DEBUG("error\n");
            LOG_PRINT(LOG_WARNING, "unsupport nalu type %d, skip this nalu\n", p_Vid->nalu.nal_unit_type);
            //return H264D_F_UNSUPPORT;
            break;
        case NALU_TYPE_SEI:
            DUMP_MSG(D_NALU, p_Dec, "-- SEI --\n");
            if ((ret = readSEI(p_Dec, p_Vid))<0)
                return ret;
            break;
        case NALU_TYPE_AUD:
            if ((ret = access_unit_delimiter(p_Dec, p_Vid))<0)
                return ret;
            break;
        case NALU_TYPE_EOSEQ:
            //LOG_PRINT(LOG_INFO, "end of sequence\n");
            break;
        case NALU_TYPE_EOSTREAM:
#if USE_SW_PARSER == 0
            if (p_Dec->u8CurrState == FIELD_DONE) {
                return H264D_NON_PAIRED_FIELD;
            }
            return H264D_ERR_BS_EMPTY;
            if (no_more_bs)
                return H264D_END_STREAM;
#endif
            break;
        case NALU_TYPE_FILL:
            //E_DEBUG("error\n");
            LOG_PRINT(LOG_WARNING, "unsupport nalu type %d, skip this nalu\n", p_Vid->nalu.nal_unit_type);                
            //return H264D_F_UNSUPPORT;
            break;
        case NALU_TYPE_SPSE:
            //E_DEBUG("error\n");
            LOG_PRINT(LOG_WARNING, "unsupport nalu type %d, skip this nalu\n", p_Vid->nalu.nal_unit_type);
            break;
        default:
            //E_DEBUG("error\n");
            LOG_PRINT(LOG_WARNING, "unsupport nalu type %d, skip this nalu\n", p_Vid->nalu.nal_unit_type);
            break;
        }
        //if (p_Dec->bBSEmpty)
        if(BS_EMPTY(p_Dec))
            return H264D_ERR_BS_EMPTY;
    }
    return H264D_OK;
}


/*
 * copy release buffer list from p_Vid->release_buffer to ptFrame->u8ReleaseBuffer
 */
static void return_release_list(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid)
{
    memcpy(ptFrame->u8ReleaseBuffer, p_Vid->release_buffer, sizeof(p_Vid->release_buffer[0]) * p_Vid->release_buffer_num);
    ptFrame->u8ReleaseBufferNum = p_Vid->release_buffer_num;
}


/*
 * copy output buffer list from p_Vid->output_frame to ptFrame->apFrame.mOutputFrame
 */
static void return_output_list(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid)
{
    memcpy(ptFrame->apFrame.mOutputFrame, p_Vid->output_frame, sizeof(FrameInfo) * p_Vid->output_frame_num);
    ptFrame->apFrame.u8OutputPicNum = p_Vid->output_frame_num;
}


/*
 * set output frame info
 */
static void return_output_frame_info(FAVC_DEC_FRAME_IOCTL *ptFrame, VideoParams *p_Vid)
{
    DecoderParams *p_Dec = p_Vid->dec_handle;
    FAVC_DEC_FRAME *apFrame = &ptFrame->apFrame;

    /* get actual decoded frame width/height */
    apFrame->u16FrameWidth = p_Vid->frame_width;
    apFrame->u16FrameHeight = p_Vid->frame_height;

    LOG_PRINT(LOG_TRACE, "(%d)return_output_frame_info: %d %d\n", p_Vid->dec_handle->chn_idx, apFrame->u16FrameWidth, apFrame->u16FrameHeight);

    /* get crop size */
    apFrame->u16CroppingLeft = p_Dec->u16CroppingLeft;
    apFrame->u16CroppingRight = p_Dec->u16CroppingRight;
    apFrame->u16CroppingTop = p_Dec->u16CroppingTop;
    apFrame->u16CroppingBottom = p_Dec->u16CroppingBottom;
    
    /* get frame rate info */
    if(p_Vid->active_sps && p_Vid->active_sps->vui_parameters_present_flag &&
        p_Vid->active_sps->vui_seq_parameters.timing_info_present_flag){
        apFrame->u32TimeScale = p_Vid->active_sps->vui_seq_parameters.time_scale;
        apFrame->u32UnitsInTick = p_Vid->active_sps->vui_seq_parameters.num_units_in_tick;
    }else{
        /* set to a default value */
        apFrame->u32TimeScale = 60;
        apFrame->u32UnitsInTick = 1;
    }

    apFrame->bFieldCoding = p_Vid->field_pic_flag;
    apFrame->bHasScaledFrame = p_Dec->bScaleEnabled; /* get the flag set in decode_slice(); */
    
    apFrame->i16POC = p_Vid->last_stored_poc; /* get the last stored poc */

#ifndef VG_INTERFACE 
    /* for ioctl driver only */
    apFrame->multislice = p_Dec->multislice;
    apFrame->vcache_enable = p_Dec->vcache_en_flgs;
    apFrame->error_sts = p_Dec->error_sts;
    apFrame->current_slice_type = p_Dec->current_slice_type;
#endif

    apFrame->bPicStored = ptFrame->bPicStored;

    if(apFrame->i8Valid){
        LOG_PRINT(LOG_WARNING, "some error occurred in this frame before:%d\n", apFrame->i8Valid);
    }else{
        apFrame->i8Valid = 1;
    }
    
}


#if USE_SW_PARSER

enum frame_done_status {
    FDS_HAS_FRAME_DONE = (1 << 0),
    FDS_BS_EMPTY =       (1 << 1),
};
/*
 * Use software parser to see if there should be a frame done at the last trigger
 */
enum frame_done_status decode_peek_frame_done(DecoderParams *p_Dec)
{
    int ret;
    NALUnit nalu;
    Bitstream stream_backup;
    enum frame_done_status fr_done_sts = 0;
	uint32_t first_mb_in_slice = 0;
    //uint8_t idr_flag;

    /* backup sw parser status */
    stream_backup = p_Dec->stream;

    /* predict the last triggered decoding will get "frame done" if:
     * 1. the next slice is a new pictutre (mb index is 0), or
     * 2. parser reaches the end of buffers
     */
    for (;;)
    {
        //unsigned int nalu_start, nalu_end;
        //LOG_PRINT(LOG_TRACE, "get_next_nalu calling\n");
        //sw_parser_dump(&p_Dec->stream);
        //nalu_start = video_gettime_us();
        ret = sw_get_next_nalu(p_Dec, &nalu, 1);
        //nalu_end = video_gettime_us();
        //LOG_PRINT(-1, "sw_get_next_nalu: %d\n", nalu_end - nalu_start);
        //sw_parser_dump(&p_Dec->stream);
        
        LOG_PRINT(LOG_TRACE, "get_next_nalu return %d\n", ret);

        if(ret == H264D_ERR_BS_EMPTY){
            fr_done_sts = FDS_HAS_FRAME_DONE;
            goto loop_end;
        }
        
#if 0//DBG_SW_PARSER
        printk("NAL type:%d\n", nalu.nal_unit_type);
        sw_parser_dump(&p_Dec->stream);
#endif
        
        switch (nalu.nal_unit_type)
        {
            case NALU_TYPE_SLICE:
            case NALU_TYPE_IDR:
                //idr_flag = (nalu.nal_unit_type == NALU_TYPE_IDR);
                first_mb_in_slice = READ_UE(&p_Dec->stream);
                if(first_mb_in_slice == 0){
                    fr_done_sts = FDS_HAS_FRAME_DONE;
                }else{
                    fr_done_sts = 0;
                }
                goto loop_end;
                break;
            default: /* skip NALU other then coded slice */
                break;
        }
    }

loop_end:
    
    if(fr_done_sts == FDS_HAS_FRAME_DONE){
        DUMP_MSG(D_LL, p_Dec, "has frame done at: ch:%d fr:%d sl:%d\n",  p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber, p_Dec->u32SliceIndex);
    }
    
    if(sw_bs_emtpy(&p_Dec->stream)){
        fr_done_sts |= FDS_BS_EMPTY;
    }

    /* restore sw parser status */
    p_Dec->stream = stream_backup;
    
    return fr_done_sts;
}
#endif


static __inline int __test_sw_reset(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
#ifdef TEST_SOFTWARE_RESET
    if (ptFrame->apFrame.random) {
        VideoParams *p_Vid = p_Dec->p_Vid;
        int k;
        for (k = 0; k<ptFrame->apFrame.random; k++);
        if (decoder_reset(p_Dec) < 0)
            return H264D_ERR_POLLING_TIMEOUT;
        decoder_clear_irq_flags(p_Dec);
        p_Vid->first_slice = 1;
        printk("R");
        return H264D_SWRESET_TEST;
    }
#endif
    return 0;
}


static __inline void __test_hw_timeout(DecoderParams *p_Dec)
{
#if TEST_HW_TIMEOUT /* for testing HW timeout */
#warning TEST_HW_TIMEOUT is enabled
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    extern unsigned int hw_timeout_delay;
    if(hw_timeout_delay){
        ptReg_io->DEC_CMD2 = hw_timeout_delay;
    }else{
        ptReg_io->DEC_CMD2 = TIMEOUT_CLOCK;     /* for timout interrupt */
    }
#endif
}


/*
 * add delay before trigger decdoer (for HW verification only)
 */
static __inline void __delay_before_trigger(void)
{
#if DELAY_BEFORE_TRIGGER
#warning DELAY_BEFORE_TRIGGER is enabled
    extern unsigned int delay_en;
    unsigned int delay_start, delay_end;
    volatile int i;
    if(delay_en & (1 << p_Dec->chn_idx)){
        delay_start = jiffies;
        printk("delay before decoding ch:%d ", p_Dec->chn_idx);
        //for(i = 0; i < 20000; i++);
        for(i = 0; i < 20000000; i++);
        delay_end = jiffies;
        printk("dur: %d\n", delay_end - delay_start);
    }
    LOG_PRINT(LOG_TRACE, "before set dec_go: %d\n", i);
#endif
}

/*
 * decoder_trig_preprocess: do somthing before trigger decoder
 * return value:
 * == 0: trigger decoder
 * == 1: do no trigger
 */
static __inline int __decoder_trig_preprocess(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
#if USE_LINK_LIST == 0
    __delay_before_trigger();
    
#if EXC_HDR_PARSING_FROM_UTIL
    if(p_Dec->pfnMarkEngStart){
       p_Dec->pfnMarkEngStart(p_Dec->eng_info->engine); 
    }
#endif

#if TEST_SW_TIMEOUT
{
    extern unsigned int sw_timeout;
    if(sw_timeout == 0){
        return 0;
    }else{
        sw_timeout--;
        return 1;
    }
}
#endif
#endif /* USE_LINK_LIST == 0 */
    return 0;
}

/*
 * start decoding process
 * return value:
 * == 0: decoding process is started
 * == 1: decoding process is NOT started
 *  < 0: some error occurred (in __test_sw_reset())
 */
static __inline int decoder_trig(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    int ret;
    int not_start_flg;
    LOG_PRINT(LOG_TRACE, "before set dec_go\n");

    not_start_flg = __decoder_trig_preprocess(ptFrame, p_Dec);
    if(not_start_flg == 0){
        WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x40), ptReg_io->DEC_CMD0, BIT0); // set dec_go
    }

#if USE_LINK_LIST
    ll_add_job(p_Dec->codec_ll, NULL, p_Dec);
    not_start_flg = 1;
#endif /* USE_LINK_LIST */
    
    PERF_MSG("trig > %d\n", p_Dec->eng_info->engine);
    LOG_PRINT(LOG_TRACE, "after set dec_go\n");

    ret = __test_sw_reset(ptFrame, p_Dec);
    if(ret < 0){
        return ret;
    }
    
    return not_start_flg;
}


static __inline void __delay_before_prog_reg(void)
{
#if DELAY_BEFORE_W_REG && (USE_LINK_LIST == 0)
    extern unsigned int delay_en;
    unsigned int delay_start, delay_end;
    volatile int i;
    
    if(delay_en & (1 << p_Dec->chn_idx)){
        delay_start = jiffies;
        printk("delay before w reg for ch:%d ", p_Dec->chn_idx);
        //for(i = 0; i < 20000; i++);
        for(i = 0; i < 20000000; i++);
        delay_end = jiffies;
        printk("dur: %d\n", delay_end - delay_start);
    }
#endif
}

/*
 * programming register for decoding a slice
 */
static __inline int decoder_prog_reg(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    int ret;

#if 0 //USE_LINK_LIST /* check if any linked list command is set */
#if SW_LINK_LIST == 0 && USE_WRITE_OUT_CMD == 1
    if(p_Dec->codec_ll->curr_job->cmd_num != 2)
#else
    if(p_Dec->codec_ll->curr_job->cmd_num != 1)
#endif
    {
        E_DEBUG("cmd num != 0(%d)\n", p_Dec->codec_ll->curr_job->cmd_num);
        ll_print_ll(p_Dec->codec_ll);
        printk("curr_job:\n");
        ll_print_job(p_Dec->codec_ll->curr_job);
        while(1);
    }
#endif

    __delay_before_prog_reg();


    /* start of register setting */
#if USE_LINK_LIST
    if(p_Dec->codec_ll->job_num != 0){
        WRITE_REG_SYNC(p_Dec, ((MCP300_REG << 7) + 0x54), ptReg_io->DEC_INTSTS, 0x01); /* to clear slice done interrupt for the previous job */
    }
#endif
    __wait_vcache_wch_done_ll(p_Dec);


    PERF_MSG("set_poc >\n");
    if((p_Vid->active_pps->weighted_pred_flag && p_Vid->slice_hdr->slice_type==P_SLICE) ||
       (p_Vid->active_pps->weighted_bipred_idc==1 && p_Vid->slice_hdr->slice_type==B_SLICE)) {
        set_pred_weight_table(p_Dec, p_Vid->slice_hdr);
    } else if ((p_Vid->active_pps->weighted_bipred_idc == 2) && (p_Vid->slice_hdr->slice_type == B_SLICE)) {
        if ((ret = set_poc_to_pred_table(p_Dec, p_Vid)) < 0)
            return ret;
    } else {
        set_pred_weight_table_default(p_Dec, p_Vid->slice_hdr);
    }
    
    ret = set_ref_pic_poc(p_Dec);
    PERF_MSG("set_poc < %d\n", ret);
    if (ret < 0)
        return ret;

    PERF_MSG("meminfo >\n");
    ret = set_meminfo(p_Dec);
    PERF_MSG("meminfo <\n");
    if (ret < 0){
        LOG_PRINT(LOG_ERROR, "set_meminfo error\n");
        return H264D_ERR_POLLING_TIMEOUT;;
    }

    /* check if required buffers are missing */
    if(p_Vid->first_slice){
        if(p_Vid->dec_pic->mbinfo_addr_phy == NULL) {
            if((p_Dec->vcache_en_flgs & VCACHE_SYSINFO_EN) == 0){
                E_DEBUG("mbinfo buffer is required when vcache sysinfo is disabled\n");
                p_Dec->pfnDamnit("mbinfo buffer is required when vcache sysinfo is disabled\n");
                return H264D_ERR_MEMORY;;
            }else if(p_Dec->bHasMBinfo){
                /* SYSINFO_EN */
                E_DEBUG("mbinfo buffer is required when vcache sysinfo and hasMBinfo is enabled\n");
                p_Dec->pfnDamnit("mbinfo buffer is required when vcache sysinfo and hasMBinfo is enabled\n");
                return H264D_ERR_MEMORY;;
            }
        }
    }


    if (p_Vid->slice_hdr->slice_type == B_SLICE){
        set_dsf(p_Dec);
    }

    PERF_MSG("assign_quant_params >\n");
    assign_quant_params(p_Dec, p_Vid);
    PERF_MSG("assign_quant_params <\n");

    LOG_PRINT(LOG_INFO, "decode_slice\n");
    PERF_MSG("dec_slice >\n");
    ret = decode_slice(p_Dec);
    PERF_MSG("dec_slice <\n");
    if (ret < 0){
        E_DEBUG("decode_slice return error\n");
        return ret;
    }
    LOG_PRINT(LOG_INFO, "decode_slice return\n");
    


    p_Dec->current_slice_type = slice_type_name[p_Vid->slice_hdr->slice_type];

    LOG_PRINT(LOG_INFO, "dec go (frame %c): mb %d [%d], poc %d\n", slice_type_name[p_Vid->slice_hdr->slice_type], p_Vid->slice_hdr->first_mb_in_slice, ptReg_io->BITSYSBUFCNT, p_Vid->ThisPOC);
    

#if CONFIG_ENABLE_VCACHE
    PERF_MSG("wait_vc >\n");
    ret = __wait_vcache_set_done(p_Dec);
    PERF_MSG("wait_vc <\n");
    if (ret < 0){
        LOG_PRINT(LOG_ERROR, "wait_vcache_set_done error\n");
        return H264D_ERR_POLLING_TIMEOUT;
    }
#endif

    // for debuging
    dump_all_register((unsigned int)p_Dec->pu32BaseAddr, (unsigned int)p_Dec->pu32VcacheBaseAddr, LOG_DEBUG); 
    dump_mem_register(p_Dec);


#if USE_SW_PARSER
    __sync_hw_bs_ptr(p_Dec);
#endif

    __test_hw_timeout(p_Dec); /* for testing HW timeout */

    return 0;
}


static __inline int decoder_parse_header(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    DecodedPictureBuffer *listD = p_Vid->listD;
    int ret;

    PERF_MSG("read_new_slice >\n");
    ret = read_new_slice(p_Dec, 0);
    PERF_MSG("read_new_slice <\n");
    
    ptFrame->apFrame.bIsIDR |= p_Vid->idr_flag;
    if (ret < 0) {
        if(ret == H264D_NON_PAIRED_FIELD){
            LOG_PRINT(LOG_INFO, "read_new_slice return H264D_NON_PAIRED_FIELD: %d frame:%d trigger:%d\n", ret, p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
        }else if(ret == H264D_ERR_BS_EMPTY){
            LOG_PRINT(LOG_ERROR, "read_new_slice encounter bitstream empty: frame:%d trigger:%d\n", p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
        }else if(ret == H264D_SKIP_TILL_IDR){
            LOG_PRINT(LOG_ERROR, "read_new_slice skip til IDR ch:%d  frame:%d\n", p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber);
        }else{
            LOG_PRINT(LOG_ERROR, "read_new_slice return error: %d ch:%d frame:%d trigger:%d\n", ret, p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
        }
#if DBG_SW_PARSER
        sw_parser_dump(&p_Dec->stream);
#endif
        return ret;
    }
    LOG_PRINT(LOG_INFO, "read_new_slice return: %d\n", ret);

    LOG_PRINT(LOG_INFO, "decoder_parse_header fr num:%d idr:%d slice type:%d(%c)\n", 
        p_Dec->u32DecodedFrameNumber, p_Vid->idr_flag,
        p_Vid->slice_hdr->slice_type, slice_type_name[p_Vid->slice_hdr->slice_type]);

    /* decode one frame does not allow there are two non-paired fields in one frame */
    if (listD->last_picture) {
        if (((p_Vid->structure == TOP_FIELD) && (listD->last_picture->is_used == 2)) || 
            ((p_Vid->structure == BOTTOM_FIELD) && (listD->last_picture->is_used == 1))) 
        {
            if (listD->last_picture->frame_num == p_Vid->frame_num){
                if (((p_Vid->nalu.nal_ref_idc != 0) && (listD->last_picture->is_orig_ref == 0)) || 
                    ((p_Vid->nalu.nal_ref_idc == 0) && (listD->last_picture->is_orig_ref != 0))){
                    /* "used for reference" is not the same for two fields */
                    return H264D_PARSING_ERROR;
                }
            }else{  
                /* frame_num is different in two fields */
                return H264D_PARSING_ERROR;
            }
        }else{
             /* two top or two bottom fields */
            return H264D_PARSING_ERROR;
        }
    }

    DUMP_MSG(D_BUF_SLICE, p_Dec, "first: %d\n", p_Vid->slice_hdr->first_mb_in_slice);

    if (p_Vid->first_slice == 1) {
        if (p_Vid->new_frame == 1) {
            p_Vid->pRecBuffer = ptFrame->ptReconstBuf;
        }
        
        if(SHOULD_DUMP(D_BUF_LIST, p_Dec)){
            DUMP_MSG(D_BUF_LIST, p_Dec, "bf init_picture\n");
            dump_dpb(p_Dec, 0);
        }
        
        PERF_MSG("init_picture >\n");
        ret = init_picture(p_Dec, p_Vid);
        PERF_MSG("init_picture <\n");

        if(SHOULD_DUMP(D_BUF_LIST, p_Dec)){
            DUMP_MSG(D_BUF_LIST, p_Dec, "af init_picture\n");
            dump_dpb(p_Dec, 0);
        }

        if (ret < 0) {
            E_DEBUG("initial picture fail\n");            
            return ret;
        }
//printk("current frame poc %d[%d]\n", p_Vid->ThisPOC, p_Vid->dec_pic->phy_index);
    } else {
        p_Dec->multislice = 1;
    }

    if(SHOULD_DUMP(D_BUF_LIST, p_Dec)){
        DUMP_MSG(D_BUF_LIST, p_Dec, "bf init_lists\n");
        dump_dpb(p_Dec, 0);
    }
    
    PERF_MSG("list_i >\n");
    init_lists(p_Vid);
    PERF_MSG("list_i <\n");

    if(SHOULD_DUMP(D_BUF_LIST, p_Dec)){
        DUMP_MSG(D_BUF_LIST, p_Dec, "bf reorder_list\n");
        dump_dpb(p_Dec, 0);
    }

    PERF_MSG("list_r >\n");
    reorder_list(p_Vid, p_Vid->slice_hdr);
    PERF_MSG("list_r <\n");

    PERF_MSG("list_f >\n");
#if 1
    ret = fill_empty_list(p_Vid);
#else
    ret = replace_damgae_list(p_Vid);
#endif
    PERF_MSG("list_f <\n");

    if (ret < 0) {
        E_DEBUG("reference list error (reference list empty)\n");
        return ret;
    }

    //printk("\t======================== poc %d, start mb %d ========================\n", p_Vid->ThisPOC, p_Vid->slice_hdr->first_mb_in_slice);
    if(SHOULD_DUMP(D_BUF_LIST, p_Dec)){
        dump_list(p_Dec, 1);
    }
    
    if (p_Vid->structure == FRAME) {
        PERF_MSG("mbaff >\n");
        init_mbaff_list(p_Vid);
        PERF_MSG("mbaff <\n");
    }

    return 0;

}


/*
 * output frame infomation: output/release list, frame info
 */
static void decoder_output_frame(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    fill_output_list(p_Vid);
    return_output_list(ptFrame, p_Vid);
    return_release_list(ptFrame, p_Vid);
    
    if(SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)){
        dump_output_list(p_Vid, D_BUF_REF);
        dump_release_list(p_Vid, D_BUF_REF);
    }
    
    return_output_frame_info(ptFrame, p_Vid);
}

/*
 * handle picture (either a frame or a field) done and update decoder state
 * \return
 *    < 0 (H264D_PARSING_ERROR): error occurred and the picture is NOT inserted
 *    FRAME_DONE: a frame is inserted
 *    FIELD_DONE: a field is inserted
 */
static int decoder_picture_done(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    VideoParams *p_Vid = p_Dec->p_Vid;
    int ret;
    
    ret = exit_picture(p_Dec, p_Vid, p_Vid->dec_pic, 0);
    if (ret == FRAME_DONE) {
        LOG_PRINT(LOG_INFO, "frame done (poc %d: phy %d) %s\n", p_Vid->listD->fs[p_Vid->listD->used_size-1]->poc, p_Vid->listD->fs[p_Vid->listD->used_size-1]->frame->phy_index, p_Vid->idr_flag?"IDR":"");

        /* reset decoder state */
        p_Dec->u8CurrState = NORMAL_STATE;

        ptFrame->bPicStored = 1;
        ptFrame->u8PicTriggerCnt = 0;

        if (SHOULD_DUMP(D_BUF_INFO, p_Dec))
        {
            DUMP_MSG(D_BUF_INFO, p_Dec, "decoder_picture_done calling dump_dpb at FRAME_DONE\n");
            dump_dpb(p_Dec, 0);
        }

    }else if(ret == FIELD_DONE) {
        LOG_PRINT(LOG_INFO, "%s field done (poc %d: phy %d)\n", structure_name[p_Vid->structure], p_Vid->listD->fs[p_Vid->listD->used_size-1]->poc, p_Vid->listD->fs[p_Vid->listD->used_size-1]->phy_index);
        
        /* reset decoder state */
        p_Dec->u8CurrState = FIELD_DONE;
        
        /* set variables for the new picture */
        p_Vid->first_slice = 1;

        ptFrame->bPicStored = 1;
        ptFrame->u8PicTriggerCnt = 0;
    }

    return ret;
}


/*
 * get hardware frame done flag
 */
static __inline int __is_picture_done(DecoderParams *p_Dec)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)((uint32_t)p_Dec->pu32BaseAddr);
    if(ptReg_io->DEC_STS0 & BIT25){
        return 1;
    }
    return 0;
}


/*
 * wait until VCACHE write channel done
 */
static __inline int __wait_vcache_wch_done(DecoderParams *p_Dec)
{
#if CONFIG_ENABLE_VCACHE && (USE_LINK_LIST == 0)
    //printk("ret H264D_ERR_POLLING_TIMEOUT\n");
    //return H264D_ERR_POLLING_TIMEOUT; // for testing error handling
    
    if (p_Dec->eng_info->vcache_ilf_enable) /* wait vcache wch done only when VCACHE ILF is enabled */
    //if(1) /* always wait vcache wch done at frame done interrupt */
    {
        volatile H264_reg_vcache *ptReg_vcache = (H264_reg_vcache *)p_Dec->pu32VcacheBaseAddr;
        int cnt = 0;
        //printk("wait vc frame done\n");
        while (!(ptReg_vcache->DECODE & BIT31)) { /* polling one */
            if (cnt > POLLING_ITERATION_TIMEOUT) {
                LOG_PRINT(LOG_ERROR, "VCACHE wch done polling timeout in ISR\n");
                return H264D_ERR_POLLING_TIMEOUT;
            }
            cnt++;
        }
    }
#endif
    return 0;
}


#if USE_LINK_LIST
unsigned int decoder_get_ll_job_id(DecoderEngInfo *eng_info)
{
    unsigned int id;
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)eng_info->pu32BaseAddr;
    id = (ptReg_io->RESERVED0 >> 5) & NBIT_MASK(11);

    return id;
}
#endif


static int __check_uncleared_irq(DecoderEngInfo *eng_info)
{
#if CHK_UNCLEARED_IRQ
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)(eng_info->pu32BaseAddr);
    
    if(ptReg_io->DEC_INTSTS != 0){
        uint32_t int_flgs;
        
        /* uncleared IRQ, may be caused by two adjacent interrupts */
        /* NOTE: 
         * 1. if HW timout is too small, this may happen 
         * 2. this is for detecting un-expected HW behavior */
         
        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "uncleared irq: DEC_INTSTS:%08X int_flgs:%08X MASK:%08X\n", ptReg_io->DEC_INTSTS, int_flgs, INTERRUPT_FLAG);
        __SET_DBG_MODE(0);

        int_flgs = ptReg_io->DEC_INTSTS;
        ptReg_io->DEC_INTSTS = int_flgs; /* clear HW interrupt again */
        eng_info->u32IntSts |= int_flgs;

        if(ptReg_io->DEC_INTSTS != 0){
            printk("uncleared irq 2: DEC_INTSTS:%08X int_flgs:%08X MASK:%08X\n", ptReg_io->DEC_INTSTS, int_flgs, INTERRUPT_FLAG);
        }
#if 0
    {
        uint32_t int_flgs2;
        int cnt = 0;
        while(ptReg_io->DEC_INTSTS){
            /* dummy reads */
            int_flgs2 = ptReg_io->DEC_INTSTS;
            ptReg_io->DEC_INTSTS = int_flgs2;
            cnt++;

            //if(cnt % 100 == 0)
            {
                printk("uncleared irq 3: int_flgs:%08X DEC_INTSTS:%08X cnt:%d\n", int_flgs2, ptReg_io->DEC_INTSTS, cnt);
            }
        }
    }
        printk("uncleared irq 4: int_flgs:%08X DEC_INTSTS:%08X cnt:%d\n", int_flgs2, ptReg_io->DEC_INTSTS, cnt);
#endif
    }

#endif
    return 0;
}


/*
 * Receive HW interrupt flags into engine info and clear HW interrupt flags
 *  interrupt:
 *      Bit0: slice done: no timeout, under run0, under run1
 *      Bit1: under run0: only in parsing header
 *      Bit2: under run1: from receive to handle, may timeout, slice done happen
 *      Bit3: timeout: no other interrupt */
int decoder_eng_receive_irq(DecoderEngInfo *eng_info)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io*)(eng_info->pu32BaseAddr);
    uint32_t int_flgs;
    volatile uint32_t tmp;
    
    int_flgs = ptReg_io->DEC_INTSTS;
    ptReg_io->DEC_INTSTS = int_flgs; /* write 1 back to clear interrupt flags */
    tmp = ptReg_io->DEC_INTSTS; /* to flush last register write in write buffer */

    /* record interrupt flags */
    eng_info->u32IntSts = int_flgs;

    LOG_PRINT(LOG_INFO, "int_flgs:%08X MASK:%08X\n", int_flgs, INTERRUPT_FLAG);
    DUMP_MSG_G(D_ISR, "int_flgs:%08X MASK:%08X\n", int_flgs, INTERRUPT_FLAG);

    __check_uncleared_irq(eng_info);

    if (eng_info->u32IntSts & ~INTERRUPT_FLAG) {
        /* unexpected interrupt occurred */
        __SET_DBG_MODE(1);
        LOG_PRINT(LOG_ERROR, "unexpected irq: DEC_INTSTS:%08X int_flgs:%08X MASK:%08X\n", ptReg_io->DEC_INTSTS, int_flgs, INTERRUPT_FLAG);
        __SET_DBG_MODE(0);
        //return -1;
    }

#if USE_LINK_LIST
    if (int_flgs & (INT_LINK_LIST_ERR|INT_HW_TIMEOUT)){

        LOG_PRINT(LOG_ERROR, "h264 dec linked list or timeout error: 0x%08X\n", ptReg_io->RESERVED0);
        //LOG_PRINT(LOG_ERROR, "ch: %d fr: %d tr: %d\n", p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
        
        LOG_PRINT(LOG_ERROR, "exec_cmd_cnt: %d\n", (ptReg_io->RESERVED0 >> 16) & ((1 << 13) - 1));
        LOG_PRINT(LOG_ERROR, "job_id: %d\n", (ptReg_io->RESERVED0 >> 5) & ((1 << 11) - 1));
        LOG_PRINT(LOG_ERROR, "err st:%s %s %s\n", 
            (ptReg_io->RESERVED0 & 1)?"[command format incorrect]":"", 
            (ptReg_io->RESERVED0 & 2)?"[timeout]":"", 
            (ptReg_io->RESERVED0 & 4)?"[done]":"" 
        );
        LOG_PRINT(LOG_ERROR, "ll address:%08X\n", ptReg_io->CPUCMD1);
        //ll_print_ll(p_Dec->codec_ll);
        ll_print_ll(eng_info->codec_ll[0]);
    }
#endif

    /* normal return */
    return H264D_OK;
}


int decoder_eng_dispatch_irq(DecoderParams *p_Dec, DecoderEngInfo *eng_info)
{
    uint32_t int_flgs;
    int_flgs = eng_info->u32IntSts;
    
    p_Dec->u32IntStsPrev = p_Dec->u32IntSts = int_flgs;

    if (int_flgs & ~INTERRUPT_FLAG) {
        return H264D_ERR_INTERRUPT;
    }

#if USE_LINK_LIST
    if (int_flgs & (INT_LINK_LIST_ERR|INT_HW_TIMEOUT)){
        LOG_PRINT(LOG_ERROR, "h264 dec linked list or timeout error ch: %d fr: %d tr: %d\n", p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber, p_Dec->u32DecodeTriggerNumber);
    }
#endif
    
    return H264D_OK;
}


static int __decode_one_frame_preprocess(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    p_Dec->multislice = 0;
    
    p_Dec->p_Vid->first_slice = 1;
    p_Dec->p_Vid->new_frame = 1;
    p_Dec->p_Vid->release_buffer_num = 0; /* clear release buffer list */
    p_Dec->p_Vid->output_frame_num = 0;   /* clear output buffer list */ // for set IDR and output all picture

    ptFrame->apFrame.bIsIDR = 0;
    ptFrame->apFrame.i8Valid = 0;
    ptFrame->apFrame.bHasScaledFrame = 0;
    ptFrame->apFrame.bPicStored = 0;
    ptFrame->apFrame.u8OutputPicNum = 0;
    ptFrame->u8ReleaseBufferNum = 0;
    
    ptFrame->bPicStored = 0;
    ptFrame->u8PicTriggerCnt = 0;
    ptFrame->u8FrameTriggerCnt = 0;

#if CLEAR_RECON_FR
#warning CLEAR_RECON_FR is enabled
{
    extern unsigned int clear_en;
    if(clear_en & (1 << p_Dec->chn_idx)){
        unsigned int clear_size;
        //clear_size = p_Dec->u16MaxWidth * p_Dec->u16MaxHeight * 2;
        clear_size = p_Dec->u16MaxWidth * 8 * 2;
        //printk("clear output buf for ch: %d size:%d\n", p_Dec->chn_idx, clear_size);
        memset(ptFrame->ptReconstBuf->dec_yuv_buf_virt, 0, clear_size);
    }
}
#endif

    return 0;
}

static void __decode_one_frame_post_process(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
}


/*
 * 1. parse header until the next slice data
 * 2. programming register
 * 3. start decoding process
 */
int decode_one_frame_trigger(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    int ret;
    int not_start_flg;
    VideoParams *p_Vid = p_Dec->p_Vid;
    p_Dec->u32DecodeTriggerNumber++;
    
    LOG_PRINT(LOG_INFO, "decode_one_frame_trigger enter:%d\n", p_Dec->u32DecodeTriggerNumber);

    if (p_Vid->first_slice)
        p_Dec->u32SliceIndex = 0;
    else
        p_Dec->u32SliceIndex++;

    DUMP_MSG(D_BUF_SLICE, p_Dec, "trigger slice %d [%x]\n", p_Dec->u32SliceIndex, p_Dec->u8CurrState);
    if(SHOULD_DUMP(D_BUF_SLICE, p_Dec)){
        dump_dpb(p_Dec, 0);
    }

    /* parse header until start of the next slice data */
    ret = decoder_parse_header(ptFrame, p_Dec);
    if(ret < 0){
        return ret;
    }

    /* start of register setting */
    ret = decoder_prog_reg(ptFrame, p_Dec);
    if(ret < 0){
        return ret;
    }

    /* trigger HW decoding process */
    not_start_flg = decoder_trig(ptFrame, p_Dec);
    if(not_start_flg < 0){
        return not_start_flg;
    }
    
    p_Vid->first_slice = 0;
    p_Vid->new_frame = 0;
    ptFrame->u8PicTriggerCnt++;
    ptFrame->u8FrameTriggerCnt++;

    return ret;
}


/*
 * 1. handle the received interrupt. return if not slice done
 * 2. if picture done, store decoded picture (frame or field) into DPB and update decoder state
 * 3. if FRAME_DONE, set output frame data
 * return value:
 *    < 0: some error occured
 *    ==0: H264D_OK (no eror, or just something that can be ignored) --> do not trigger 
 *    > 0: 
 *         SLICE_DONE: one slice is done   --> may trigger
 *         FRAME_DONE: a frame is inserted --> may trigger
 *         FIELD_DONE: a field is inserted --> may trigger
 */
int decode_one_frame_sync(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    int ret = SLICE_DONE;

    p_Dec->eng_info->vcache_ilf_enable = p_Dec->vcache_en_flgs & VCACHE_ILF_EN;
    
#if USE_MULTICHAN_LINK_LIST == 0
    ret = __handle_received_interrupt(p_Dec);

    if(ret != SLICE_DONE){ /* ret <= 0 */
        LOG_PRINT(LOG_ERROR, "current bitstream in SRAM:\n");
        decoder_print_current_bitstream((unsigned int)p_Dec->pu32BaseAddr, 256);
        p_Dec->eng_info->prev_pic_done = 0;
        return ret;
    }
#endif

    /* slice done */
    rbsp_slice_trailing_bits(p_Dec);

#if USE_LINK_LIST == 0
    if(__is_picture_done(p_Dec) == 0){
        return ret;
    }
#endif

    /* handling picture done: one picture (frame or field) is decoded */
    p_Dec->u16PicDone++;

    if(__wait_vcache_wch_done(p_Dec)){
        p_Dec->eng_info->prev_pic_done = 0;
        return H264D_ERR_POLLING_TIMEOUT;
    }
    
    p_Dec->eng_info->prev_pic_done = 1;

    ret = decoder_picture_done(p_Dec, ptFrame);
    if(ret < 0){ /* some error occured */
        return ret;
    }
    
    /* FIELD_DONE or FRAME_DONE */
    if(ret == FRAME_DONE){
        p_Dec->u16FrameDone++;
        decoder_output_frame(p_Dec, ptFrame);
    }
    return ret;
}


/*
 * Start the first slice decoding of decoing a frame
 */
int decode_one_frame_start(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    int ret = H264D_OK;
   
    p_Dec->u32DecodedFrameNumber++;
    LOG_PRINT(LOG_INFO, "=================== decode one frame %d (ch:%d st:%x buf:%d) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState, ptFrame->ptReconstBuf->buffer_index);
    DUMP_MSG(D_BUF_FR, p_Dec, "=================== decode one frame %d (ch:%d st:%x buf:%d) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState, ptFrame->ptReconstBuf->buffer_index);

    /* init status at the first trigger */
    __decode_one_frame_preprocess(p_Dec, ptFrame);

    PERF_MSG_N(PROF_DEC, "dec_one_trig start >\n");
    ret = decode_one_frame_trigger(p_Dec, ptFrame);
    PERF_MSG_N(PROF_DEC, "dec_one_trig start <\n");
    
    if(ret){
        PERF_MSG_N(PROF_DEC, "dec_one_err start >\n");
        decode_one_frame_err_handle(p_Dec, ptFrame);
        PERF_MSG_N(PROF_DEC, "dec_one_err start <\n");
    }
    
    return ret;
}


/*
 * Handle interrupt of the previous slice decoding and start the next slice decoding until a frame is done
 */
int decode_one_frame_next(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    int ret = H264D_OK;

    PERF_MSG_N(PROF_DEC, "dec_one_sync next >\n");
    ret = decode_one_frame_sync(p_Dec, ptFrame);
    PERF_MSG_N(PROF_DEC, "dec_one_sync next <\n");
    LOG_PRINT(LOG_INFO, "decode_one_frame_sync() ret:%d\n", ret);

    if(ret == H264D_OK){ /* ret == 0: decoding process is on-going. Do nothing. */
        return H264D_OK;
    }

    if(ret == FRAME_DONE){ /* one frame is decoded */
        __decode_one_frame_post_process(p_Dec, ptFrame);
        return FRAME_DONE;
    }

    if(ret < 0){ /* some error occurred */
        goto err_ret;
    }

    /* ret > 0: FIELD_DONE or SLICE_DONE */
    PERF_MSG_N(PROF_DEC, "dec_one_trig next >\n");
    ret = decode_one_frame_trigger(p_Dec, ptFrame);
    PERF_MSG_N(PROF_DEC, "dec_one_trig next <\n");
    if (ret != H264D_OK) {
        goto err_ret;
    }

    return H264D_OK;
    
err_ret:
    PERF_MSG_N(PROF_DEC, "dec_one_err next >\n");
    decode_one_frame_err_handle(p_Dec, ptFrame);
    PERF_MSG_N(PROF_DEC, "dec_one_err next <\n");
    return ret;
}


/*
 * handle decode frame error according to decoder state
 */
void decode_one_frame_err_handle(DecoderParams *p_Dec, FAVC_DEC_FRAME_IOCTL *ptFrame)
{
    VideoParams *p_Vid = p_Dec->p_Vid;


    DUMP_MSG(D_ERR, p_Dec, "decode_one_frame_err_handle: ch:%d fr:%d sl:%d pic_trig:%d stored:%d buf_idx:%d\n", 
        p_Dec->chn_idx, p_Dec->u32DecodedFrameNumber, p_Dec->u32SliceIndex,
        ptFrame->u8PicTriggerCnt, ptFrame->bPicStored, ptFrame->ptReconstBuf->buffer_index);


    __chk_unhandled_irq(p_Dec);

    /* clear received but un-handled IRQ */
    decoder_clear_irq_flags(p_Dec);
    
    /* reset hw register */
    decoder_reset(p_Dec);
    
    /* NPF handling? not needed */

    /* whether to store picture into DPB?: if decode triggered for the picture at least once */
    if(ptFrame->u8PicTriggerCnt){
        int ret = exit_picture(p_Dec, p_Vid, p_Vid->dec_pic, 1);
        if((ret == FRAME_DONE) || (ret == FIELD_DONE)){
            ptFrame->bPicStored = 1;
        }
    }

    /* whether to fill output list?: do it if picture has been stored successfully */
    if(ptFrame->bPicStored){
        fill_output_list(p_Vid);
    }
    return_output_list(ptFrame, p_Vid);
    
    return_release_list(ptFrame, p_Vid);
    
    if(SHOULD_DUMP(D_BUF_REF, p_Vid->dec_handle)){
        dump_output_list(p_Vid, D_BUF_REF);
        dump_release_list(p_Vid, D_BUF_REF);
    }

    return_output_frame_info(ptFrame, p_Vid);
    ptFrame->apFrame.i8Valid = -1; /* set output invalid */


    /* clear last_picture, since the decoding of this frame will not continue */
    p_Vid->listD->last_picture = NULL;

    /* clear decoder state */
    p_Dec->u8CurrState = NORMAL_STATE;

}


#if USE_LINK_LIST
#ifndef VG_INTERFACE

int decoder_init_ll_jobs(DecoderParams *p_Dec)
{
    ll_reinit(p_Dec->codec_ll);
    return 0;
}

#if 0
/*
 * trigger the execution of a linked list
 */
int decoder_trigger_ll_jobs(DecoderParams *p_Dec)
{
    /* exec jobs and wait interrupts */
    int i;
    uint64_t cmd;
    unsigned int cmd_num = 0;
    unsigned int cmd_pa = 0;
    unsigned int job_id;
    unsigned int has_sw_cmd = 0;
    int ret;
    struct codec_link_list *codec_ll = p_Dec->codec_ll;
    DecoderParams *dec_handle;
    
    DUMP_MSG(D_LL, p_Dec, "decoder_trigger_ll_jobs job_num:%d\n", p_Dec->codec_ll->job_num);

#if (SW_LINK_LIST == 0) || (SW_LINK_LIST == 2)
    /* set job header command in linked list */
    for(i = 0; i < codec_ll->job_num; i++){
        codec_ll->exe_job_idx = i;

        has_sw_cmd |= codec_ll->jobs[i].has_sw_cmd;

        if(i != codec_ll->job_num - 1){
            cmd_num = codec_ll->jobs[i + 1].cmd_num;
            cmd_pa = codec_ll->jobs[i + 1].cmd_pa;
            cmd = codec_ll->jobs[i].cmd[0].cmd;
            job_id = (cmd >> 49) & NBIT_MASK(11);
            codec_ll->jobs[i].cmd[0] = job_header_cmd(job_id, cmd_num, cmd_pa);
        }else{
            /* set last bit by setting null cmd address */
            cmd = codec_ll->jobs[i].cmd[0].cmd;
            job_id = (cmd >> 49) & NBIT_MASK(11);
            codec_ll->jobs[i].cmd[0] = job_header_cmd(job_id, 0, 0);
        }
    }
#endif

#if SW_LINK_LIST == 0
    codec_ll->sw_exec_flg = 0;
    return decoder_trigger_ll_job(p_Dec, 0);
#endif

#if USE_SW_LINK_LIST_ONLY
    #warning always use SW command
    has_sw_cmd = 1;
#endif
    //printk("job_num:%d\n", codec_ll->job_num);
#if SW_LINK_LIST == 2
#ifdef A369 /* only A369 has HW linked list */
    if(has_sw_cmd == 0){
        //printk("no sw_cmd\n");
        codec_ll->sw_exec_flg = 0;
        return decoder_trigger_ll_job(p_Dec, 0);
    }
#else
    #warning SW_LINK_LIST==2 on platform other then A369
#endif
#endif

    codec_ll->sw_exec_flg = 1;

    //printk("decoder_trigger_ll_jobs %d\n", codec_ll->job_num);

    for(i = 0; i < codec_ll->job_num; i++){
        codec_ll->exe_job_idx = i;
        
        //ll_print_job(&p_Dec->codec_ll->jobs[i]);
        
#if SW_LINK_LIST == 2
        dec_handle = codec_ll->jobs[i].dec_handle;
        ret = ll_sw_exec_job2(codec_ll, &codec_ll->jobs[i]);
        //printk("ll_sw_exec_job %d ch: %d\n", i, dec_handle->chn_idx);
        if(ret){
            printk("ll_sw_exec_job2() error:%d\n", ret);
            break;
        }

#if USE_MULTICHAN_LINK_LIST == 0
        if(i == codec_ll->job_num - 1)
            break;
#endif /* USE_MULTICHAN_LINK_LIST */
#endif
        /* wait interrupt */
        ret = decoder_block(dec_handle);
        //printk("WAIT_EVENT:%d\n", WAIT_EVENT);
        if(ret != H264D_OK){
            printk("decoder_block error:%d\n", ret);
            break;
        }
        
        decoder_clear_irq_flags(dec_handle);
    }

    return 0;
}
#endif
#endif

/*
 * trigger HW execution of a linked list
 */
int decoder_trigger_ll_job(DecoderParams *p_Dec, int job_idx)
{
    volatile H264_reg_io *ptReg_io = (H264_reg_io *)((uint32_t)p_Dec->pu32BaseAddr);
    unsigned int tmp;
#if 0
    printk("hw exec job id: %d cmd num: %d\n", p_Dec->codec_ll->jobs[job_idx].job_id, p_Dec->codec_ll->jobs[job_idx].cmd_num);
    ll_print_job(&p_Dec->codec_ll->jobs[job_idx]);
#endif

    /* set command number */
    tmp = p_Dec->codec_ll->jobs[job_idx].cmd_num;
    ptReg_io->CPUCMD0 = SHIFT(tmp, 14, 4);
    ptReg_io->DEC_CMD1 |= (1 << 15);

    tmp = (unsigned int)p_Dec->codec_ll->jobs[job_idx].cmd_pa;
    if(tmp & 7){
        printk("command address is not multiple of 8\n");
        return -1;
    }

    /* set command address */
    ptReg_io->CPUCMD1 = tmp;
    
#if 0
    printk("cmd address: %08X CPUCMD0:%08X CPUCMD1:%08X DEC_CMD1:%08X va:%08X base:%08X 0x58:%08X\n", 
        tmp, ptReg_io->CPUCMD0, ptReg_io->CPUCMD1, ptReg_io->DEC_CMD1, 
        (unsigned int)&p_Dec->codec_ll->jobs[job_idx], (unsigned int)p_Dec->pu32BaseAddr, ptReg_io->RESERVED0);
#endif

    /* set ll fire */
    ptReg_io->DEC_CMD0 = BIT2;
#if 0
    printk("set ll go %08X\n", ptReg_io->DEC_CMD0);
    printk("cmd address: %08X CPUCMD0:%08X CPUCMD1:%08X DEC_CMD1:%08X va:%08X base:%08X 0x58:%08X(af ll go)\n", tmp, ptReg_io->CPUCMD0, ptReg_io->CPUCMD1, ptReg_io->DEC_CMD1, 
        (unsigned int)&p_Dec->codec_ll->jobs[job_idx], (unsigned int)p_Dec->pu32BaseAddr, ptReg_io->RESERVED0);
#endif
#if 0
    while(1){
        schedule();
    }
#endif
    //ll_print_job(&p_Dec->codec_ll->jobs[job_idx]);
    
    return 0;
}
#endif /* USE_LINK_LIST */


#if 0//ndef VG_INTERFACE
#if USE_LINK_LIST
int decode_one_frame_ll(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    int ret = H264D_OK;
    int has_frame_done;
    int bs_empty;

   
    p_Dec->u32DecodedFrameNumber++;
    LOG_PRINT(LOG_INFO, "=================== decode one frame %d (ch:%d st:%x) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState);
    DUMP_MSG(D_BUF_FR, p_Dec, "=================== decode one frame %d (ch:%d st:%x) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState);

    while(1)
    {
        
        // ================== trigger ================= //
        //
        LOG_PRINT(LOG_INFO, "decode_one_frame_trigger() calling\n");
        ret = decode_one_frame_trigger(p_Dec, ptFrame);
        LOG_PRINT(LOG_INFO, "decode_one_frame_trigger() return %d\n", ret);
        if (ret != H264D_OK) {
#if USE_NEW_ERR_HANDLE
            decode_one_frame_err_handle(p_Dec, ptFrame);
#endif
            return ret;
        }

        /* check if it is the last slice of a field */
        LOG_PRINT(LOG_INFO, "decode_peek_frame_done() calling\n");
        ret = decode_peek_frame_done(p_Dec);
        LOG_PRINT(LOG_INFO, "decode_peek_frame_done() return %d\n", ret);
        //printk("has_frame_done:%d\n", has_frame_done);

        has_frame_done = ret & 1;
        bs_empty = (ret >> 1) & 1;
        
        if(has_frame_done == 0 && bs_empty == 0){ 
            /* prepare jobs for the next slice */
            continue;
        }
        
#if 1
        if(has_frame_done == 1 && bs_empty == 0){
            /* has frame done but no bitstream empty: field coding case */
            ret = decoder_picture_done(p_Dec, ptFrame);
#if USE_NEW_ERR_HANDLE
            if(ret == FRAME_DONE)
                return ret;
            
            if((ret == FIELD_DONE)){
                continue;
            }
            decode_one_frame_err_handle(p_Dec, ptFrame);
            return ret;
#else

            ret = decode_sync_error_handle(p_Dec, ptFrame, ret);
            if (ret != H264D_OK) {
                //printk("decode_sync_error_handle return: %d\n", ret);
                return ret;
            }
            continue;
#endif
        }
#endif

        if(bs_empty){
            break;
        }
    }

    DUMP_MSG(D_LL, p_Dec, "decoder_trigger_ll_jobs for ch %d\n", p_Dec->chn_idx);
    decoder_trigger_ll_jobs(p_Dec);

    // =================== block =================== //
    //
    LOG_PRINT(LOG_INFO, "decode_one_frame_block() calling\n");
    ret = decode_one_frame_block(p_Dec, ptFrame);
    LOG_PRINT(LOG_INFO, "decode_one_frame_block() return %d\n", ret);
    if (ret != H264D_OK) {
#if USE_NEW_ERR_HANDLE
        decode_one_frame_err_handle(p_Dec, ptFrame);
#endif
        return ret;
    }
    DUMP_MSG(D_LL, p_Dec, "ll_reinit for ch %d\n", p_Dec->chn_idx);
    ll_reinit(p_Dec->codec_ll);
    
    // =================== sync =================== //
    //
    LOG_PRINT(LOG_INFO, "decode_one_frame_sync() calling\n");
    ret = decode_one_frame_sync(p_Dec, ptFrame);
    //printk("decode_one_frame_sync:%d\n", ret);
    LOG_PRINT(LOG_INFO, "decode_one_frame_sync() return %d\n", ret);
    if (ret != H264D_OK) {
#if USE_NEW_ERR_HANDLE
        decode_one_frame_err_handle(p_Dec, ptFrame);
#endif
        return ret;
    }
    
#if USE_NEW_ERR_HANDLE == 0
    if(has_frame_done && bs_empty){
        LOG_PRINT(LOG_INFO, "frame_done and bs_empty. curr stat:%d\n", p_Dec->u8CurrState);
        ret = decode_trigger_error_handle(p_Dec, ptFrame, H264D_NON_PAIRED_FIELD);
        return ret;
    }
#endif

    return H264D_PARSING_ERROR;
}

int decode_one_frame_ll_top(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    int ret = H264D_OK;
    int has_frame_done;
    int bs_empty;

   
    p_Dec->u32DecodedFrameNumber++;
    LOG_PRINT(LOG_INFO, "=================== decode one frame %d (ch:%d st:%x) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState);
    DUMP_MSG(D_BUF_FR, p_Dec, "=================== decode one frame %d (ch:%d st:%x) ===================\n", p_Dec->u32DecodedFrameNumber, p_Dec->chn_idx, p_Dec->u8CurrState);

    while(1)
    {
        
        //printk("decode_one_frame_ll:%d\n", i++);
        // ================== trigger ================= //
        //
        LOG_PRINT(LOG_INFO, "decode_one_frame_trigger() calling\n");
        ret = decode_one_frame_trigger(p_Dec, ptFrame);
        LOG_PRINT(LOG_INFO, "decode_one_frame_trigger() return %d\n", ret);
        if (ret != H264D_OK) {
            //("decode_one_frame_trigger return: %d\n", ret);
#if USE_NEW_ERR_HANDLE
            decode_one_frame_err_handle(p_Dec, ptFrame);
#endif
            return ret;
        }

        /* check if it is the last slice of a field */
        LOG_PRINT(LOG_INFO, "decode_peek_frame_done() calling\n");
        ret = decode_peek_frame_done(p_Dec);
        LOG_PRINT(LOG_INFO, "decode_peek_frame_done() return %d\n", ret);
        //printk("has_frame_done:%d\n", has_frame_done);

        has_frame_done = ret & 1;
        bs_empty = (ret >> 1) & 1;
        
        if(has_frame_done == 0 && bs_empty == 0){ 
            /* prepare jobs for the next slice */
            continue;
        }
        
#if 1
        if(has_frame_done == 1 && bs_empty == 0){
            /* has frame done but no bitstream empty: field coding case */
            ret = decoder_picture_done(p_Dec, ptFrame);
#if USE_NEW_ERR_HANDLE
            if(ret == FRAME_DONE)
                return ret;
            
            if((ret == FIELD_DONE)){
                continue;
            }
            decode_one_frame_err_handle(p_Dec, ptFrame);
            return ret;
#else
            ret = decode_sync_error_handle(p_Dec, ptFrame, ret);
            if (ret != H264D_OK) {
                //printk("decode_sync_error_handle return: %d\n", ret);
                return ret;
            }
            continue;
#endif
        }
#endif

        if(bs_empty){
            break;
        }
    }

    return H264D_OK;
}

int decode_one_frame_ll_bottom(FAVC_DEC_FRAME_IOCTL *ptFrame, DecoderParams *p_Dec)
{
    int ret;

    //decoder_trigger_ll_jobs(p_Dec);
    
    // =================== block =================== //
    //
#if 0
    LOG_PRINT(LOG_INFO, "decode_one_frame_block() calling\n");
    ret = decode_one_frame_block(p_Dec, ptFrame);
    LOG_PRINT(LOG_INFO, "decode_one_frame_block() return %d\n", ret);
    if (ret != H264D_OK) {
        return ret;
    }
#endif
    //ll_reinit(p_Dec->codec_ll);
    
    // ==================== sync =================== //
    //
    LOG_PRINT(LOG_INFO, "decode_one_frame_sync() calling\n");
    ret = decode_one_frame_sync(p_Dec, ptFrame);
    //printk("decode_one_frame_sync:%d\n", ret);
    LOG_PRINT(LOG_INFO, "decode_one_frame_sync() return %d\n", ret);
    if (ret != H264D_OK) {
        return ret;
    }
    
    LOG_PRINT(LOG_INFO, "frame_done and bs_empty. curr stat:%d\n", p_Dec->u8CurrState);
    ret = decode_trigger_error_handle(p_Dec, ptFrame, H264D_NON_PAIRED_FIELD);
    return ret;
}


#endif

#endif // !VG_INTERFACE



