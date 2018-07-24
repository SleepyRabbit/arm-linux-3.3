#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/kfifo.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/synclink.h>
#include <linux/time.h>

#include "ft3dnr200.h"

#ifdef VIDEOGRAPH_INC
#include "ft3dnr200_vg.h"
#include <log.h>    //include log system "printm","damnit"...
#include <video_entity.h>   //include video entity manager
#define MODULE_NAME  "DN"    //<<<<<<<<<< Modify your module name (two bytes) >>>>>>>>>>>>>>
#endif

#define THDNR_SET_SIGNED_REG(TG,R1,R2,REG) \
        (TG)=0;   \
        (TG)=(R1);  \
        (TG)&=0xffff; \
        (TG)|=((R2)<<16); \
        *(volatile unsigned int *)(base+REG) = (TG)
/*
   Example:
    int tmp=0;
    THDNR_SET_SIGNED_REG(tmp,param->Hpf0_5x5_Coeff00,param->Hpf0_5x5_Coeff01);
    *(volatile unsigned int *)(base + 0xdc) = tmp;
*/

#define SET_LL_CMD_BOT(REG,VAL)  *(volatile u32 *)(REG)=(VAL)
#define SET_LL_CMD_TOP(REG,VAL)  *(volatile u32 *)(REG+0x4)=(VAL)

void ft3dnr_init_tmnr_global(u32 base, tmnr_param_t *tmnr, ft3dnr_dim_t *dim)
{
    u32 tmp = 0;
    int record_setting = 0;

    if (dim == NULL)
        record_setting = 1; // just record setting at first init, otherwise record at isp_api called

#if 0
    tmp = tmnr->Y_var | (tmnr->Cb_var << 8) | (tmnr->Cr_var << 16) | (tmnr->K << 24) |
          (tmnr->auto_recover << 30) | (tmnr->rapid_recover << 31);
    *(volatile unsigned int *)(base + 0x008C) = tmp;

    tmp = tmnr->trade_thres | (tmnr->suppress_strength << 8) | (tmnr->NF << 24);
    *(volatile unsigned int *)(base + 0x0090) = tmp;

    tmp = tmnr->var_offset | (tmnr->motion_var << 8);
    *(volatile unsigned int *)(base + 0x0094) = tmp;
#endif

    tmp = tmnr->Motion_top_lft_th | (tmnr->Motion_top_nrm_th << 16);
    *(volatile unsigned int *)(base + 0x0098) = tmp;
    if (record_setting) *(u32 *)&priv.curr_reg[0x98] = tmp;

    tmp = tmnr->Motion_top_rgt_th | (tmnr->Motion_nrm_lft_th << 16);
    *(volatile unsigned int *)(base + 0x009C) = tmp;
    if (record_setting) *(u32 *)&priv.curr_reg[0x9C] = tmp;

    tmp = tmnr->Motion_nrm_nrm_th | (tmnr->Motion_nrm_rgt_th << 16);
    *(volatile unsigned int *)(base + 0x00A0) = tmp;
    if (record_setting) *(u32 *)&priv.curr_reg[0xA0] = tmp;

    tmp = tmnr->Motion_bot_lft_th | (tmnr->Motion_bot_nrm_th << 16);
    *(volatile unsigned int *)(base + 0x00A4) = tmp;
    if (record_setting) *(u32 *)&priv.curr_reg[0xA4] = tmp;

    tmp = tmnr->Motion_bot_rgt_th;
    *(volatile unsigned int *)(base + 0x00A8) = tmp;
    if (record_setting) *(u32 *)&priv.curr_reg[0xA8] = tmp;
}

void ft3dnr_init_sp_global(u32 base, sp_param_t *sp, ft3dnr_dim_t *dim)
{
    u32 tmp = 0;
    int record_setting = 0;

    if (dim == NULL)
        record_setting = 1; // just record setting at first init, otherwise record at isp_api called

    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[0],sp->hpf0_5x5_coeff[1],0x00DC);
    if (record_setting) *(u32 *)&priv.curr_reg[0xDC] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[2],sp->hpf0_5x5_coeff[3],0x00E0);
    if (record_setting) *(u32 *)&priv.curr_reg[0xE0] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[4],sp->hpf0_5x5_coeff[5],0x00E4);
    if (record_setting) *(u32 *)&priv.curr_reg[0xE4] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[6],sp->hpf0_5x5_coeff[7],0x00E8);
    if (record_setting) *(u32 *)&priv.curr_reg[0xE8] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[8],sp->hpf0_5x5_coeff[9],0x00EC);
    if (record_setting) *(u32 *)&priv.curr_reg[0xEC] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[10],sp->hpf0_5x5_coeff[11],0x00F0);
    if (record_setting) *(u32 *)&priv.curr_reg[0xF0] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[12],sp->hpf0_5x5_coeff[13],0x00F4);
    if (record_setting) *(u32 *)&priv.curr_reg[0xF4] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[14],sp->hpf0_5x5_coeff[15],0x00F8);
    if (record_setting) *(u32 *)&priv.curr_reg[0xF8] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[16],sp->hpf0_5x5_coeff[17],0x00FC);
    if (record_setting) *(u32 *)&priv.curr_reg[0xFC] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[18],sp->hpf0_5x5_coeff[19],0x0100);
    if (record_setting) *(u32 *)&priv.curr_reg[0x100] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[20],sp->hpf0_5x5_coeff[21],0x0104);
    if (record_setting) *(u32 *)&priv.curr_reg[0x104] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[22],sp->hpf0_5x5_coeff[23],0x0108);
    if (record_setting) *(u32 *)&priv.curr_reg[0x108] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[24],0,0x010C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x10C] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[0],sp->hpf1_3x3_coeff[1],0x0110);
    if (record_setting) *(u32 *)&priv.curr_reg[0x110] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[2],sp->hpf1_3x3_coeff[3],0x0114);
    if (record_setting) *(u32 *)&priv.curr_reg[0x114] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[4],sp->hpf1_3x3_coeff[5],0x0118);
    if (record_setting) *(u32 *)&priv.curr_reg[0x118] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[6],sp->hpf1_3x3_coeff[7],0x011C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x11C] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[8],0,0x0120);
    if (record_setting) *(u32 *)&priv.curr_reg[0x120] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[0],sp->hpf2_1x5_coeff[1],0x0124);
    if (record_setting) *(u32 *)&priv.curr_reg[0x124] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[2],sp->hpf2_1x5_coeff[3],0x0128);
    if (record_setting) *(u32 *)&priv.curr_reg[0x128] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[4],0,0x012C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x12C] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[0],sp->hpf3_1x5_coeff[1],0x0130);
    if (record_setting) *(u32 *)&priv.curr_reg[0x130] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[2],sp->hpf3_1x5_coeff[3],0x0134);
    if (record_setting) *(u32 *)&priv.curr_reg[0x134] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[4],0,0x0138);
    if (record_setting) *(u32 *)&priv.curr_reg[0x138] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[0],sp->hpf4_5x1_coeff[1],0x013C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x13C] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[2],sp->hpf4_5x1_coeff[3],0x0140);
    if (record_setting) *(u32 *)&priv.curr_reg[0x140] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[4],0,0x0144);
    if (record_setting) *(u32 *)&priv.curr_reg[0x144] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[0],sp->hpf5_5x1_coeff[1],0x0148);
    if (record_setting) *(u32 *)&priv.curr_reg[0x148] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[2],sp->hpf5_5x1_coeff[3],0x014C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x14C] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[4],0,0x0150);
    if (record_setting) *(u32 *)&priv.curr_reg[0x150] = tmp;

    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[0],sp->gau_5x5_coeff[1],0x019C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x19C] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[2],sp->gau_5x5_coeff[3],0x01A0);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1A0] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[4],sp->gau_5x5_coeff[5],0x01A4);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1A4] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[6],sp->gau_5x5_coeff[7],0x01A8);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1A8] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[8],sp->gau_5x5_coeff[9],0x01AC);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1AC] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[10],sp->gau_5x5_coeff[11],0x01B0);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1B0] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[12],sp->gau_5x5_coeff[13],0x01B4);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1B4] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[14],sp->gau_5x5_coeff[15],0x01B8);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1B8] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[16],sp->gau_5x5_coeff[17],0x01BC);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1BC] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[18],sp->gau_5x5_coeff[19],0x01C0);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1C0] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[20],sp->gau_5x5_coeff[21],0x01C4);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1C4] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[22],sp->gau_5x5_coeff[23],0x01C8);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1C8] = tmp;
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[24],sp->gau_shift,0x01CC);
    if (record_setting) *(u32 *)&priv.curr_reg[0x1CC] = tmp;
}

void ft3dnr_init_global(void)
{
    u32 base = 0, tmp = 0;
    int chanID = 0;
    tmnr_param_t *tmnr = &priv.default_param.tmnr;
    sp_param_t *sp = &priv.default_param.sp;

    base = priv.engine.ft3dnr_reg;

    /* init register memory to 0 */
#if 1
    memset((void *)base,0x0,FT3DNR_REG_MEM_LEN);
#else
    for (i = 0; i < FT3DNR_REG_MEM_LEN; i = i + 4)
        *(volatile unsigned int *)(base + i) = 0;
#endif

    /* init link list memory to 0 */
#if 1
    memset((void *)(base+FT3DNR_LL_MEM_BASE),0x0,FT3DNR_LL_MEM_SIZE);
#else
    for (i = 0; i < FT3DNR_LL_MEM; i = i + 4)
        *(volatile unsigned int *)(base + FT3DNR_LL_MEM_BASE + i) = 0;
#endif

    /* wc/rc wait value */
    tmp = priv.engine.wc_wait_value | (priv.engine.rc_wait_value << 16);
    *(volatile unsigned int *)(base + 0x0204) = tmp;

    /* var addr */
    if (priv.chan_param[chanID].var_buf_p)
        tmp = priv.chan_param[chanID].var_buf_p->paddr;
    else
        tmp = priv.res_cfg[0].var_buf.paddr;
    *(volatile unsigned int *)(base + FT3DNR_VAR_ADDR) = tmp;

    /* mot addr */
    if (priv.chan_param[chanID].mot_buf_p)
        tmp = priv.chan_param[chanID].mot_buf_p->paddr;
    else
        tmp = priv.res_cfg[0].mot_buf.paddr;
    *(volatile unsigned int *)(base + FT3DNR_MOT_ADDR) = tmp;

    /* sp addr */
    tmp = priv.engine.sp_buf.paddr;
    *(volatile unsigned int *)(base + FT3DNR_EE_ADDR) = tmp;

    /* ref addr */
    if (priv.chan_param[chanID].ref_buf_p)
        tmp = priv.chan_param[chanID].ref_buf_p->paddr;
    else
        tmp = priv.res_cfg[0].ref_buf.paddr;
    *(volatile unsigned int *)(base + FT3DNR_REF_R_ADDR) = tmp;
    *(volatile unsigned int *)(base + FT3DNR_REF_W_ADDR) = tmp;

#if 1
    ft3dnr_init_tmnr_global(base, tmnr, NULL);
#else
    tmp = tmnr->Y_var | (tmnr->Cb_var << 8) | (tmnr->Cr_var << 16) | (tmnr->K << 24) |
          (tmnr->auto_recover << 30) | (tmnr->rapid_recover << 31);
    *(volatile unsigned int *)(base + 0x008C) = tmp;

    tmp = tmnr->trade_thres | (tmnr->suppress_strength << 8) | (tmnr->NF << 24);
    *(volatile unsigned int *)(base + 0x0090) = tmp;

    tmp = tmnr->var_offset | (tmnr->motion_var << 8);
    *(volatile unsigned int *)(base + 0x0094) = tmp;

    tmp = tmnr->Motion_top_lft_th | (tmnr->Motion_top_nrm_th << 16);
    *(volatile unsigned int *)(base + 0x0098) = tmp;

    tmp = tmnr->Motion_top_rgt_th | (tmnr->Motion_nrm_lft_th << 16);
    *(volatile unsigned int *)(base + 0x009C) = tmp;

    tmp = tmnr->Motion_nrm_nrm_th | (tmnr->Motion_nrm_rgt_th << 16);
    *(volatile unsigned int *)(base + 0x00A0) = tmp;

    tmp = tmnr->Motion_bot_lft_th | (tmnr->Motion_bot_nrm_th << 16);
    *(volatile unsigned int *)(base + 0x00A4) = tmp;

    tmp = 0;
    tmp = tmnr->Motion_bot_rgt_th;
    *(volatile unsigned int *)(base + 0x00A8) = tmp;
#endif

#if 1
    ft3dnr_init_sp_global(base, sp, NULL);
#else
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[0],sp->hpf0_5x5_coeff[1],0x00DC);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[2],sp->hpf0_5x5_coeff[3],0x00E0);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[4],sp->hpf0_5x5_coeff[5],0x00E4);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[6],sp->hpf0_5x5_coeff[7],0x00E8);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[8],sp->hpf0_5x5_coeff[9],0x00EC);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[10],sp->hpf0_5x5_coeff[11],0x00F0);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[12],sp->hpf0_5x5_coeff[13],0x00F4);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[14],sp->hpf0_5x5_coeff[15],0x00F8);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[16],sp->hpf0_5x5_coeff[17],0x00FC);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[18],sp->hpf0_5x5_coeff[19],0x0100);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[20],sp->hpf0_5x5_coeff[21],0x0104);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[22],sp->hpf0_5x5_coeff[23],0x0108);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf0_5x5_coeff[24],0,0x010C);

    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[0],sp->hpf1_3x3_coeff[1],0x0110);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[2],sp->hpf1_3x3_coeff[3],0x0114);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[4],sp->hpf1_3x3_coeff[5],0x0118);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[6],sp->hpf1_3x3_coeff[7],0x011C);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf1_3x3_coeff[8],0,0x0120);

    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[0],sp->hpf2_1x5_coeff[1],0x0124);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[2],sp->hpf2_1x5_coeff[3],0x0128);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf2_1x5_coeff[4],0,0x012C);

    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[0],sp->hpf3_1x5_coeff[1],0x0130);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[2],sp->hpf3_1x5_coeff[3],0x0134);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf3_1x5_coeff[4],0,0x0138);

    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[0],sp->hpf4_5x1_coeff[1],0x013C);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[2],sp->hpf4_5x1_coeff[3],0x0140);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf4_5x1_coeff[4],0,0x0144);

    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[0],sp->hpf5_5x1_coeff[1],0x0148);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[2],sp->hpf5_5x1_coeff[3],0x014C);
    THDNR_SET_SIGNED_REG(tmp,sp->hpf5_5x1_coeff[4],0,0x0150);

    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[0],sp->gau_5x5_coeff[1],0x019C);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[2],sp->gau_5x5_coeff[3],0x01A0);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[4],sp->gau_5x5_coeff[5],0x01A4);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[6],sp->gau_5x5_coeff[7],0x01A8);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[8],sp->gau_5x5_coeff[9],0x01AC);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[10],sp->gau_5x5_coeff[11],0x01B0);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[12],sp->gau_5x5_coeff[13],0x01B4);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[14],sp->gau_5x5_coeff[15],0x01B8);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[16],sp->gau_5x5_coeff[17],0x01BC);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[18],sp->gau_5x5_coeff[19],0x01C0);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[20],sp->gau_5x5_coeff[21],0x01C4);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[22],sp->gau_5x5_coeff[23],0x01C8);
    THDNR_SET_SIGNED_REG(tmp,sp->gau_5x5_coeff[24],sp->gau_shift,0x01CC);
#endif
}

void ft3dnr_set_lli_mrnr(ft3dnr_job_t *job, u32 ret_addr)
{
    u32 base=0, tmp=0, offset=0;
    int mrnrNum = job->ll_blk.mrnr_num;
    mrnr_param_t *mrnr = NULL;

    /* select mrnr from isp or decoder or sensor */
    if (job->param.src_type == SRC_TYPE_ISP)
        mrnr = &job->param.mrnr;
    else if (job->param.src_type == SRC_TYPE_DECODER || job->param.src_type == SRC_TYPE_SENSOR)
        mrnr = &priv.default_param.mrnr;

    base = priv.engine.ft3dnr_reg;
    offset = FT3DNR_MRNR_MEM_BASE + FT3DNR_MRNR_MEM_BLOCK * mrnrNum;

    // 0x700
    tmp = mrnr->Y_L_edg_th[0][0] | (mrnr->Y_L_edg_th[0][1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000010;

    offset+= 8; // 0x708
    tmp = mrnr->Y_L_edg_th[0][2] | (mrnr->Y_L_edg_th[0][3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000014;

    offset+= 8; // 0x710
    tmp = mrnr->Y_L_edg_th[0][4] | (mrnr->Y_L_edg_th[0][5] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000018;

    offset+= 8; // 0x718
    tmp = mrnr->Y_L_edg_th[0][6] | (mrnr->Y_L_edg_th[0][7] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00001c;

    offset+= 8; // 0x720
    tmp = mrnr->Y_L_edg_th[1][0] | (mrnr->Y_L_edg_th[1][1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000020;

    offset+= 8; // 0x728
    tmp = mrnr->Y_L_edg_th[1][2] | (mrnr->Y_L_edg_th[1][3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000024;

    offset+= 8; // 0x730
    tmp = mrnr->Y_L_edg_th[1][4] | (mrnr->Y_L_edg_th[1][5] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000028;

    offset+= 8; // 0x738
    tmp = mrnr->Y_L_edg_th[1][6] | (mrnr->Y_L_edg_th[1][7] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00002c;

    offset+= 8; // 0x740
    tmp = mrnr->Y_L_edg_th[2][0] | (mrnr->Y_L_edg_th[2][1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000030;

    offset+= 8; // 0x748
    tmp = mrnr->Y_L_edg_th[2][2] | (mrnr->Y_L_edg_th[2][3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000034;

    offset+= 8; // 0x750
    tmp = mrnr->Y_L_edg_th[2][4] | (mrnr->Y_L_edg_th[2][5] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000038;

    offset+= 8; // 0x758
    tmp = mrnr->Y_L_edg_th[2][6] | (mrnr->Y_L_edg_th[2][7] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00003c;

    offset+= 8; // 0x760
    tmp = mrnr->Y_L_edg_th[3][0] | (mrnr->Y_L_edg_th[3][1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000040;

    offset+= 8; // 0x768
    tmp = mrnr->Y_L_edg_th[3][2] | (mrnr->Y_L_edg_th[3][3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000044;

    offset+= 8; // 0x770
    tmp = mrnr->Y_L_edg_th[3][4] | (mrnr->Y_L_edg_th[3][5] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000048;

    offset+= 8; // 0x778
    tmp = mrnr->Y_L_edg_th[3][6] | (mrnr->Y_L_edg_th[3][7] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00004c;

    offset+= 8; // 0x780
    tmp = mrnr->Cb_L_edg_th[0] | (mrnr->Cb_L_edg_th[1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000050;

    offset+= 8; // 0x788
    tmp = mrnr->Cb_L_edg_th[2] | (mrnr->Cb_L_edg_th[3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000054;

    offset+= 8; // 0x790
    tmp = mrnr->Cr_L_edg_th[0] | (mrnr->Cr_L_edg_th[1] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000058;

    offset+= 8; // 0x798
    tmp = mrnr->Cr_L_edg_th[2] | (mrnr->Cr_L_edg_th[3] << 16);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00005c;

    offset+= 8; // 0x7a0
    tmp = mrnr->Y_L_smo_th[0][0] | (mrnr->Y_L_smo_th[0][1] << 8) |
          (mrnr->Y_L_smo_th[0][2] << 16) | (mrnr->Y_L_smo_th[0][3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000060;

    offset+= 8; // 0x7a8
    tmp = mrnr->Y_L_smo_th[0][4] | (mrnr->Y_L_smo_th[0][5] << 8) |
          (mrnr->Y_L_smo_th[0][6] << 16) | (mrnr->Y_L_smo_th[0][7] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000064;

    offset+= 8; // 0x7b0
    tmp = mrnr->Y_L_smo_th[1][0] | (mrnr->Y_L_smo_th[1][1] << 8) |
          (mrnr->Y_L_smo_th[1][2] << 16) | (mrnr->Y_L_smo_th[1][3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000068;

    offset+= 8; // 0x7b8
    tmp = mrnr->Y_L_smo_th[1][4] | (mrnr->Y_L_smo_th[1][5] << 8) |
          (mrnr->Y_L_smo_th[1][6] << 16) | (mrnr->Y_L_smo_th[1][7] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00006c;

    offset+= 8; // 0x7c0
    tmp = mrnr->Y_L_smo_th[2][0] | (mrnr->Y_L_smo_th[2][1] << 8) |
          (mrnr->Y_L_smo_th[2][2] << 16) | (mrnr->Y_L_smo_th[2][3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000070;

    offset+= 8; // 0x7c8
    tmp = mrnr->Y_L_smo_th[2][4] | (mrnr->Y_L_smo_th[2][5] << 8) |
          (mrnr->Y_L_smo_th[2][6] << 16) | (mrnr->Y_L_smo_th[2][7] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000074;

    offset+= 8; // 0x7d0
    tmp = mrnr->Y_L_smo_th[3][0] | (mrnr->Y_L_smo_th[3][1] << 8) |
          (mrnr->Y_L_smo_th[3][2] << 16) | (mrnr->Y_L_smo_th[3][3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000078;

    offset+= 8; // 0x7d8
    tmp = mrnr->Y_L_smo_th[3][4] | (mrnr->Y_L_smo_th[3][5] << 8) |
          (mrnr->Y_L_smo_th[3][6] << 16) | (mrnr->Y_L_smo_th[3][7] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e00007c;

    offset+= 8; // 0x7e0
    tmp = mrnr->Cb_L_smo_th[0] | (mrnr->Cb_L_smo_th[1] << 8) |
          (mrnr->Cb_L_smo_th[2] << 16) | (mrnr->Cb_L_smo_th[3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000080;

    offset+= 8; // 0x7e8
    tmp = mrnr->Cr_L_smo_th[0] | (mrnr->Cr_L_smo_th[1] << 8) |
          (mrnr->Cr_L_smo_th[2] << 16) | (mrnr->Cr_L_smo_th[3] << 24);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000084;

    offset+= 8; // 0x7f0
    tmp = mrnr->Y_L_nr_str[0] | (mrnr->Y_L_nr_str[1] << 4) |
          (mrnr->Y_L_nr_str[2] << 8) | (mrnr->Y_L_nr_str[3] << 12) |
          (mrnr->C_L_nr_str[0] << 16) | (mrnr->C_L_nr_str[1] << 20) |
          (mrnr->C_L_nr_str[2] << 24) | (mrnr->C_L_nr_str[3] << 28);
    *(volatile unsigned int *)(base + offset) = tmp;
    *(volatile unsigned int *)(base + offset + 0x4) = 0x5e000088;

    // jump to next update pointer cmd
    offset += 8;
    tmp = 0x60000000 + FT3DNR_LL_MEM_BASE + ret_addr;
    *(volatile unsigned int *)(base + offset) = 0;
    *(volatile unsigned int *)(base + offset + 0x4) = tmp;
}

void ft3dnr_set_lli_sp(ft3dnr_job_t *job, u32 ret_addr)
{
    u32 base=0, tmp=0, offset=0;
    int spNum = job->ll_blk.sp_num;
    sp_param_t *sp = NULL;

    /* select sp from isp or decoder or sensor */
    if (job->param.src_type == SRC_TYPE_ISP)
        sp = &job->param.sp;
    else if (job->param.src_type == SRC_TYPE_DECODER || job->param.src_type == SRC_TYPE_SENSOR)
        sp = &priv.default_param.sp;

    base = priv.engine.ft3dnr_reg;
    offset = FT3DNR_SP_MEM_BASE + FT3DNR_SP_MEN_BLOCK * spNum;

    tmp = sp->hpf_use_lpf[0] | (sp->hpf_use_lpf[1] << 1) | (sp->hpf_use_lpf[2] << 2) |
          (sp->hpf_use_lpf[3] << 3) | (sp->hpf_use_lpf[4] << 4) | (sp->hpf_use_lpf[5] << 5) |
          (sp->hpf_enable[0] << 8) | (sp->hpf_enable[1] << 9) | (sp->hpf_enable[2] << 10) |
          (sp->hpf_enable[3] << 11) | (sp->hpf_enable[4] << 12) | (sp->hpf_enable[5] << 13) |
          (sp->edg_wt_enable << 16) | (sp->nl_gain_enable << 17);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0000D8);

    // hpf0
    offset += 8;
    tmp = (sp->hpf_gain[0]) | (sp->hpf_bright_clip[0] << 16) | (sp->hpf_dark_clip[0] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000154);

    offset += 8;
    tmp = (sp->hpf_shift[0]) | (sp->hpf_coring_th[0] << 8) | (sp->hpf_peak_gain[0] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000158);

    // hpf1
    offset += 8;
    tmp = (sp->hpf_gain[1]) | (sp->hpf_bright_clip[1] << 16) | (sp->hpf_dark_clip[1] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00015C);

    offset += 8;
    tmp = (sp->hpf_shift[1]) | (sp->hpf_coring_th[1] << 8) | (sp->hpf_peak_gain[1] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000160);

    // hpf2
    offset += 8;
    tmp = (sp->hpf_gain[2]) | (sp->hpf_bright_clip[2] << 16) | (sp->hpf_dark_clip[2] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000164);

    offset += 8;
    tmp = (sp->hpf_shift[2]) | (sp->hpf_coring_th[2] << 8) | (sp->hpf_peak_gain[2] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000168);

    // hpf3
    offset += 8;
    tmp = (sp->hpf_gain[3]) | (sp->hpf_bright_clip[3] << 16) | (sp->hpf_dark_clip[3] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00016C);

    offset += 8;
    tmp = (sp->hpf_shift[3]) | (sp->hpf_coring_th[3] << 8) | (sp->hpf_peak_gain[3] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000170);

    // hpf4
    offset += 8;
    tmp = (sp->hpf_gain[4]) | (sp->hpf_bright_clip[4] << 16) | (sp->hpf_dark_clip[4] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000174);

    offset += 8;
    tmp = (sp->hpf_shift[4]) | (sp->hpf_coring_th[4] << 8) | (sp->hpf_peak_gain[4] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000178);

    // hpf5
    offset += 8;
    tmp = (sp->hpf_gain[5]) | (sp->hpf_bright_clip[5] << 16) | (sp->hpf_dark_clip[5] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00017C);

    offset += 8;
    tmp = (sp->hpf_shift[5]) | (sp->hpf_coring_th[5] << 8) | (sp->hpf_peak_gain[5] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000180);

    // edg wt idx
    offset += 8;
    tmp = (sp->edg_wt_curve.index[0]) | (sp->edg_wt_curve.index[1] << 8) |
          (sp->edg_wt_curve.index[2] << 16) | (sp->edg_wt_curve.index[3] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000184);

    // edg wt val
    offset += 8;
    tmp = (sp->edg_wt_curve.value[0]) | (sp->edg_wt_curve.value[1] << 8) |
          (sp->edg_wt_curve.value[2] << 16) | (sp->edg_wt_curve.value[3] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000188);

    // edg wt slope
    offset += 8;
    tmp = (sp->edg_wt_curve.slope[0]) | (sp->edg_wt_curve.slope[1] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00018C);

    offset += 8;
    tmp = (sp->edg_wt_curve.slope[2]) | (sp->edg_wt_curve.slope[3] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000190);

    offset += 8;
    tmp = 0;
    tmp = (sp->edg_wt_curve.slope[4]);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000194);

    // rec gain
    offset += 8;
    tmp = (sp->rec_bright_clip) | (sp->rec_dark_clip << 8) | (sp->rec_peak_gain << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000198);

    // nl gain idx
    offset += 8;
    tmp = (sp->nl_gain_curve.index[0]) | (sp->nl_gain_curve.index[1] << 8) |
          (sp->nl_gain_curve.index[2] << 16) | (sp->nl_gain_curve.index[3] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001D0);

    // nl gain val
    offset += 8;
    tmp = (sp->nl_gain_curve.value[0]) | (sp->nl_gain_curve.value[1] << 8) |
          (sp->nl_gain_curve.value[2] << 16) | (sp->nl_gain_curve.value[3] << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001D4);

    // nl gain slope
    offset += 8;
    tmp = (sp->nl_gain_curve.slope[0]) | (sp->nl_gain_curve.slope[1] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001D8);

    offset += 8;
    tmp = (sp->nl_gain_curve.slope[2]) | (sp->nl_gain_curve.slope[3] << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001DC);

    offset += 8;
    tmp = 0;
    tmp = (sp->nl_gain_curve.slope[4]);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001E0);

    // jump to next update pointer cmd
    offset += 8;
    tmp = 0x60000000 + FT3DNR_LL_MEM_BASE + ret_addr;
    SET_LL_CMD_BOT(base+offset,0);
    SET_LL_CMD_TOP(base+offset,tmp);

}

extern int tmnr_en;

int ft3dnr_set_lli_blk(ft3dnr_job_t *job, int chain_cnt, int last_job)
{
    u32 base=0, offset=0, tmp=0;
    int blkNum, mrnrNum, spNum;
    ft3dnr_ctrl_t *ctrl=NULL;
    ft3dnr_dim_t *dim=NULL;
    ft3dnr_dma_t *dma=NULL;
    tmnr_param_t *tmnr=NULL;
    int record_setting = 0;

    if (!job)
        return -1;

    blkNum = job->ll_blk.blk_num;
    mrnrNum = job->ll_blk.mrnr_num;
    spNum = job->ll_blk.sp_num;
    ctrl = &job->param.ctrl;
    dim = &job->param.dim;
    dma = &job->param.dma;
#if 0
    tmnr = &job->param.tmnr;
#else
    /* select tmnr from isp or decoder or sensor */
    if (job->param.src_type == SRC_TYPE_ISP)
        tmnr = &job->param.tmnr;
    else if (job->param.src_type == SRC_TYPE_DECODER || job->param.src_type == SRC_TYPE_SENSOR)
        tmnr = &priv.default_param.tmnr;
#endif

    if (dim->src_bg_width == priv.curr_max_dim.src_bg_width)
        record_setting = 1;

    if (job->fops && job->fops->mark_engine_start)
        job->fops->mark_engine_start(job);

    base = priv.engine.ft3dnr_reg + FT3DNR_LL_MEM_BASE;
    offset = FT3DNR_LL_MEM_BLOCK * blkNum;

    // spatial/temporal/edg/learn enable
    offset += 8;
    if (tmnr_en == 0) {
        ctrl->temporal_en = 0;
        ctrl->tnr_learn_en = 0;
        ctrl->tnr_rlt_w = 0;
    }
    tmp = ctrl->spatial_en | (ctrl->temporal_en << 1) | (ctrl->tnr_learn_en << 3) |
          (priv.engine.ycbcr.src_yc_swap << 4) | (priv.engine.ycbcr.src_cbcr_swap << 5) |
          (priv.engine.ycbcr.dst_yc_swap << 6) | (priv.engine.ycbcr.dst_cbcr_swap << 7) |
          (priv.engine.ycbcr.ref_yc_swap << 8) | (priv.engine.ycbcr.ref_cbcr_swap << 9) |
          (ctrl->ee_en << 10) | (ctrl->tnr_rlt_w << 12);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x46000000);
    if (record_setting) *(u32 *)&priv.curr_reg[0x00] = tmp;

    // src width/height
    offset += 8;
    tmp = (dim->src_bg_width | dim->src_bg_height << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000004);
    if (record_setting) *(u32 *)&priv.curr_reg[0x04] = tmp;

    // start_x/start_y
    offset += 8;
    tmp = (dim->src_x | dim->src_y << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000008);
    if (record_setting) *(u32 *)&priv.curr_reg[0x08] = tmp;

    // nr width/height
    offset += 8;
    tmp = (dim->nr_width | dim->nr_height << 16);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00000C);
    if (record_setting) *(u32 *)&priv.curr_reg[0x0C] = tmp;

    // tmnr
    offset += 8;
    tmp = (tmnr->Y_var) | (tmnr->Cb_var << 8) | (tmnr->Cr_var << 16) |
          (tmnr->K << 24) | (tmnr->auto_recover << 30) | (tmnr->rapid_recover << 31);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E00008C);

    offset += 8;
    tmp = (tmnr->trade_thres) | (tmnr->suppress_strength << 8) | (tmnr->NF << 24);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000090);

    offset += 8;
    tmp = (tmnr->var_offset) | (tmnr->motion_var << 8);
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000094);

    // update pointer(jmp) cmd
    offset += 8;
    tmp = 0x60000000 + FT3DNR_MRNR_MEM_BASE + (mrnrNum * FT3DNR_MRNR_MEM_BLOCK);
    SET_LL_CMD_BOT(base+offset,0);
    SET_LL_CMD_TOP(base+offset,tmp);

    offset += 8;
    ft3dnr_set_lli_mrnr(job, offset);

    // update pointer(jmp) cmd
    tmp = 0x60000000 + FT3DNR_SP_MEM_BASE + (spNum * FT3DNR_SP_MEN_BLOCK);
    SET_LL_CMD_BOT(base+offset,0);
    SET_LL_CMD_TOP(base+offset,tmp);

    offset += 8;
    ft3dnr_set_lli_sp(job, offset);

    // src addr
    tmp = dma->src_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001E4);

    // dst addr
    offset += 8;
    tmp = dma->dst_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001F0);

    // ref read addr
    offset += 8;
    tmp = dma->ref_r_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001F4);

    // ref write addr
    offset += 8;
    tmp = dma->ref_w_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E000200);

    // mot addr
    offset += 8;
    tmp = dma->mot_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001FC);

    // var addr
    offset += 8;
    tmp = dma->var_addr;
    SET_LL_CMD_BOT(base+offset,tmp);
    SET_LL_CMD_TOP(base+offset,0x5E0001F8);

    // jump to next cmd (null cmd)
    offset += 8;
    tmp = 0x80000000 + FT3DNR_LL_MEM_BASE + job->ll_blk.next_blk * FT3DNR_LL_MEM_BLOCK;
    SET_LL_CMD_BOT(base+offset,0);
    SET_LL_CMD_TOP(base+offset,tmp);

    // null cmd
    offset = job->ll_blk.next_blk * FT3DNR_LL_MEM_BLOCK;
    SET_LL_CMD_BOT(base+offset,0);
    SET_LL_CMD_TOP(base+offset,0x00000000);

    /* this chain has 2 jobs, second job can write status register */
#if 0
    if (chain_cnt == 2 && last_job) {
        base = priv.engine.ft3dnr_reg + FT3DNR_LL_MEM_BASE + 0x80 * blk;
        *(volatile unsigned int *)(base + 0) = 0x00000000;
        *(volatile unsigned int *)(base + 0x4) = 0x20000000;
    }
#endif
    return 0;
}


void ft3dnr_job_finish(ft3dnr_job_t *job)
{
    if (job->fops && job->fops->post_process)
        job->fops->post_process(job);

#ifdef DRV_CFG_USE_TASKLET
    /* update node status */
    if (job->fops && job->fops->update_status)
        job->fops->update_status(job, JOB_STS_DONE);
#endif

    ft3dnr_job_free(job);
}

void ft3dnr_write_status(void)
{
    u32 llBase=0, offset=0;
    ft3dnr_job_t *job, *ne;

    llBase = ft3dnr_get_ip_base() + FT3DNR_LL_MEM_BASE;

    /* trigger timer */
    mod_timer(&priv.engine.timer, jiffies + priv.engine.timeout);

    list_for_each_entry_safe_reverse(job, ne, &priv.engine.slist, job_list) {
        offset = job->ll_blk.status_addr;
        *(volatile u32 *)(llBase + offset) = 0;
        *(volatile u32 *)(llBase + offset + 0x4) = 0x20000000;
    }
}

int ft3dnr_fire(void)
{
    u32 tmp=0, base=0;

    base = priv.engine.ft3dnr_reg;

    /* trigger timer */
    mod_timer(&priv.engine.timer, jiffies + priv.engine.timeout);

    tmp = *(volatile unsigned int *)(base);

    if (tmp & 0x1000000) {
        printk("[FT3DNR] Fire Bit not clear!\n");
        return -1;
    }
    else {
        tmp |= (1 << 24);
        *(volatile unsigned int *)(base) = tmp;
    }

    return 0;
}

void ft3dnr_dump_reg(void)
{
    int i;
    u32 base=0, size=0;

    base = ft3dnr_get_ip_base();
    size = 0x1E4;

    for (i=0; i<size; i+=0x10) {
        printk("0x%08x  0x%08x 0x%08x 0x%08x 0x%08x\n",(base+i),*(u32 *)(base+i),
                *(u32 *)(base+i+0x4),*(u32 *)(base+i+0x8),*(u32 *)(base+i+0xC));
    }

    printk("\n");
    printk("link list memory content\n");

    base = priv.engine.ft3dnr_reg + FT3DNR_LL_MEM_BASE;
    size = FT3DNR_LL_MEM_SIZE;
    for (i=0; i<size; i+=0x10) {
        printk("0x%08x  0x%08x 0x%08x 0x%08x 0x%08x\n",(base+i),*(unsigned int *)(base+i),
            *(unsigned int *)(base+i+4),*(unsigned int *)(base+i+8),*(unsigned int *)(base+i+0xc));
    }
}

irqreturn_t ft3dnr_interrupt(int irq, void *devid)
{
    int status0=0, status1=0;
    int wjob_cnt=0;
    u32 base=0, ipStat=0;
    ft3dnr_job_t *job, *nextJob;
    //bool dump_job_flow = IS_DUMP_JOB_FLOW;
    unsigned long flags=0;
    ft3dnr_dma_t    *dma;
    job_node_t *node;

    base = ft3dnr_get_ip_base();
    ipStat = *(volatile unsigned int *)(base + 0x0208);

    /* Clear Interrupt Status */
    *(volatile u32 *)(base + 0x0208) = 0x00C8;

#if 0
    if ((ipStat & 0x8) == 0x8)
        printk("[FT3DRN_IRQ] frame done %08X\n", ipStat);
    if ((ipStat & 0x88) == 0x88)
        printk("[FT3DRN_IRQ] link list done %08X\n", ipStat);
#endif

    /* Check Error Status */
    if (ipStat & BIT6)
        printk("[FT3DRN_IRQ] invalid link list cmd!!\n");

    // Find out the completed jobs, and callback it to V.G.
    DRV_SPIN_LOCK(flags);
    list_for_each_entry_safe(job, nextJob, &priv.engine.wlist, job_list) {
        if (job->status == JOB_STS_DONE)
            continue;

        status0 = *(volatile u32 *)(base + FT3DNR_LL_MEM_BASE + job->ll_blk.status_addr);
        status1 = *(volatile u32 *)(base + FT3DNR_LL_MEM_BASE + job->ll_blk.status_addr + 0x4);

        if ((status1 & 0x20000000) != 0x20000000) {// not status command
            printk("[FT3DRN_IRQ] it's not status command [%x] value %x, something wrong !!\n",
                                            job->ll_blk.status_addr, status1);
            ft3dnr_dump_reg();
            damnit(MODULE_NAME);
        }
        else {
            if ((status1 & 0x1) == 0x1) {

                dma = &job->param.dma;
                node = (job_node_t *)job->private;
                DEBUG_M(LOG_WARNING, node->engine, node->minor, "buffer dst=0x%08x src=0x%08x var=0x%08x ee=0x%08x mot=0x%08x ref_r=0x%08x ref_w=0x%08x  job done id=%d\n",
                    dma->dst_addr, dma->src_addr, dma->var_addr, dma->ee_addr, dma->mot_addr, dma->ref_r_addr, dma->ref_w_addr, job->job_id);

                // callback the job
                job->status = JOB_STS_DONE;
                ft3dnr_job_finish(job);
                mod_timer(&priv.engine.timer, jiffies + priv.engine.timeout);
            }
            else { // job not yet do
                //printk(KERN_DEBUG "[FT3DRN_IRQ] job not yet do [%d]\n", job->job_id);
                break;
            }
        }
    }

    list_for_each_entry_safe(job, nextJob, &priv.engine.wlist, job_list) {
        if (job->status != JOB_STS_DONE)
            wjob_cnt++;
    }

    // no jobs in working list, HW idle
    if (wjob_cnt == 0) {
        UNLOCK_ENGINE();
        mark_engine_finish();
        del_timer(&priv.engine.timer);
    }

    DRV_SPIN_UNLOCK(flags);

    return IRQ_HANDLED;
}

#if 1
void ft3dnr_sw_reset(void)
{
    u32 base = 0, tmp = 0;
    int cnt = 10;

    base = priv.engine.ft3dnr_reg;

    /* DMA DISABLE */
    tmp = *(volatile unsigned int *)(base + 0x208);
    tmp |= BIT1;
    *(volatile unsigned int *)(base + 0x208) = tmp;

    /* polling DMA disable status */
    while (((*(volatile unsigned int *)(base + 0x208) & BIT0) != BIT0) && (cnt-- > 0))
        udelay(10);

    if ((*(volatile unsigned int *)(base + 0x208) & BIT0) != BIT0)
        printk("[FT3DNR] DMA shutdown failed!!\n");
    else
        printk("[FT3DNR] DMA shutdown done!!\n");

#if 0
    /* clear dma disable */
    tmp = *(volatile unsigned int *)(base + 0xe0);
    tmp &= ~BIT1;
    *(volatile unsigned int *)(base + 0xe0) = tmp;
#endif

    /* sw reset */
    tmp = *(volatile unsigned int *)(base + 0x208);
    tmp |= BIT2;
    *(volatile unsigned int *)(base + 0x208) = tmp;

    udelay(10);

    tmp = *(volatile unsigned int *)(base + 0x208);
    tmp &= ~BIT2;
    *(volatile unsigned int *)(base + 0x208) = tmp;

    udelay(10);

    if((*(volatile unsigned int *)(base + 0x208) & BIT2) != 0x0)
        printk("[FT3DNR] SW rest failed!!\n");
    else
        printk("[FT3DNR] SW reset done!!\n");

    /* after software reset, 3dnr become normal mode */
    priv.engine.ll_mode = 0;

}
#else
void ft3dnr_sw_reset(void)
{
    u32 base = 0, tmp = 0;
    int cnt = 10;

    base = priv.engine.ft3dnr_reg;

    /* DMA DISABLE */
    tmp = *(volatile unsigned int *)(base + 0xe0);
    tmp |= BIT1;
    *(volatile unsigned int *)(base + 0xe0) = tmp;

    /* polling DMA disable status */
    while (((*(volatile unsigned int *)(base + 0xe0) & BIT0) != BIT0) && (cnt-- > 0))
        udelay(10);

    if ((*(volatile unsigned int *)(base + 0xe0) & BIT0) != BIT0)
        printk("[FT3DNR] DMA disable failed!!\n");
    else
        printk("[FT3DNR] DMA disable done!!\n");

    /* clear dma disable */
    tmp = *(volatile unsigned int *)(base + 0xe0);
    tmp &= ~BIT1;
    *(volatile unsigned int *)(base + 0xe0) = tmp;

    /* sw reset */
    tmp = *(volatile unsigned int *)(base + 0xe0);
    tmp |= BIT2;
    *(volatile unsigned int *)(base + 0xe0) = tmp;

    udelay(10);

    if((*(volatile unsigned int *)(base + 0xe0) & BIT2) != 0x0)
        printk("[FT3DNR] SW rest failed!!\n");
    else
        printk("[FT3DNR] SW reset done!!\n");

    /* after software reset, 3dnr become normal mode */
    priv.engine.ll_mode = 0;
}
#endif

#if 0
void ft3dnr_set_tmnr_rlut(void)
{
    u32 base = 0;
    int i = 0;

    base = priv.engine.ft3dnr_reg;

    for (i = 0; i < 6; i++)
        *(volatile unsigned int *)(base + 0x9c + (i << 2)) = priv.isp_param.rvlut.rlut[i];
}

void ft3dnr_set_tmnr_vlut(void)
{
    u32 base = 0;
    int i = 0;

    base = priv.engine.ft3dnr_reg;

    for (i = 0; i < 6; i++)
        *(volatile unsigned int *)(base + 0xb4 + (i << 2)) = priv.isp_param.rvlut.vlut[i];
}

int ft3dnr_write_register(ft3dnr_job_t *job)
{
    u32 base = 0, tmp = 0;
    ft3dnr_ctrl_t   *ctrl = &job->param.ctrl;
    ft3dnr_dim_t    *dim = &job->param.dim;
    ft3dnr_his_t    *his = &job->param.his;
    ft3dnr_dma_t    *dma = &job->param.dma;

    base = priv.engine.ft3dnr_reg;

    tmp = ctrl->spatial_en | (ctrl->temporal_en << 1) | (ctrl->tnr_edg_en << 2) | (ctrl->tnr_learn_en << 3);
    *(volatile unsigned int *)(base + 0x0) = tmp;
    // src width/height
    tmp = (dim->src_bg_width | dim->src_bg_height << 16);
    *(volatile unsigned int *)(base + 0x4) = tmp;
    // start_x/start_y
    tmp = (dim->src_x | dim->src_y << 16);
    *(volatile unsigned int *)(base + 0x8) = tmp;
    // ori width/height
    tmp = (dim->nr_width | dim->nr_height << 16);
    *(volatile unsigned int *)(base + 0xc) = tmp;
    // norm/right th
    tmp = (his->norm_th | his->right_th << 16);
    *(volatile unsigned int *)(base + 0x90) = tmp;
    // bot/corner th
    tmp = (his->bot_th | his->corner_th << 16);
    *(volatile unsigned int *)(base + 0x94) = tmp;
    // src addr
    tmp = dma->src_addr;
    *(volatile unsigned int *)(base + 0xcc) = tmp;
    // dst addr
    tmp = dma->dst_addr;
    *(volatile unsigned int *)(base + 0xd0) = tmp;
    // ref addr
    tmp = dma->ref_addr;
    *(volatile unsigned int *)(base + 0xd4) = tmp;
    // var addr
    tmp = dma->var_addr;
    *(volatile unsigned int *)(base + 0xd8) = tmp;

    return 0;
}
#endif
