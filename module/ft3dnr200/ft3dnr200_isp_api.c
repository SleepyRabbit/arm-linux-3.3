#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/interrupt.h>

#include <linux/ioport.h>
#include <linux/platform_device.h>

#include <ft3dnr200_isp_api.h>
#include "ft3dnr200.h"

#ifdef VIDEOGRAPH_INC
#include <video_entity.h>   //include video entity manager
#include <log.h>    //include log system "printm","damnit"...
#define MODULE_NAME         "DN"    //<<<<<<<<<< Modify your module name (two bytes) >>>>>>>>>>>>>>
#endif

#define TMNR_MOD_INC                (priv.isp_param.tmnr_id)++
#define MRNR_MOD_INC                (priv.isp_param.mrnr_id)++
#define SP_MOD_INC                  (priv.isp_param.sp_id)++

#define SET_ISP_CTRL(VAR,VAL)       priv.isp_param.ctrl.VAR=(VAL)
#define SET_ISP_TMNR(VAR,VAL)       priv.isp_param.tmnr.VAR=(VAL)
#define SET_ISP_MRNR(VAR,VAL)       priv.isp_param.mrnr.VAR=(VAL)
#define SET_ISP_SP(VAR,VAL)         priv.isp_param.sp.VAR=(VAL)
#define SET_ISP_SP_IDX(IDX,VAR,VAL) priv.isp_param.sp.VAR[IDX]=(VAL)

#define GET_ISP_CTRL(VAR,VAL)       (VAL)=priv.isp_param.ctrl.VAR

/*
    TMNR export APIs
*/
void api_set_tmnr_en(bool temporal_en)
{
    DRV_SEMA_LOCK;
    //temproal_en  : enable temporal noise reduction
    SET_ISP_CTRL(temporal_en,temporal_en);
    TMNR_MOD_INC;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_en);

void api_set_tmnr_learn_en(bool tnr_learn_en)
{
    DRV_SEMA_LOCK;
    //tnr_learn_en : enable temporal strength learning
    SET_ISP_CTRL(tnr_learn_en,tnr_learn_en);
    TMNR_MOD_INC;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_learn_en);

void api_set_tmnr_param(int Y_var, int Cb_var, int Cr_var, int motion_var,
                             int trade_thres, int suppress_strength, int K)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(Y_var,Y_var);
    SET_ISP_TMNR(Cb_var,Cb_var);
    SET_ISP_TMNR(Cr_var,Cr_var);
    SET_ISP_TMNR(motion_var,motion_var);
    SET_ISP_TMNR(trade_thres,trade_thres);
    SET_ISP_TMNR(suppress_strength,suppress_strength);
    SET_ISP_TMNR(K,K);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x8C] &= ~0x0FFFFFFF;
    *(u32 *)&priv.curr_reg[0x8C] |= ((priv.isp_param.tmnr.Y_var) | (priv.isp_param.tmnr.Cb_var << 8) |
                                     (priv.isp_param.tmnr.Cr_var << 16) | (priv.isp_param.tmnr.K << 24));
    *(u32 *)&priv.curr_reg[0x90] &= ~0x0000FFFF;
    *(u32 *)&priv.curr_reg[0x90] |= ((priv.isp_param.tmnr.trade_thres) | (priv.isp_param.tmnr.suppress_strength << 8));
    *(u32 *)&priv.curr_reg[0x94] &= ~0x0000FF00;
    *(u32 *)&priv.curr_reg[0x94] |= (priv.isp_param.tmnr.motion_var << 8);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_param);

void api_set_tmnr_NF(int NF)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(NF,NF);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x90] &= ~0xFF000000;
    *(u32 *)&priv.curr_reg[0x90] |= (priv.isp_param.tmnr.NF << 24);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_NF);

void api_set_tmnr_var_offset(int var_offset)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(var_offset,var_offset);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x94] &= ~0x0000000F;
    *(u32 *)&priv.curr_reg[0x94] |= priv.isp_param.tmnr.var_offset;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_var_offset);

void api_set_tmnr_auto_recover_en(bool auto_recover_en)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(auto_recover,auto_recover_en);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x8C] &= ~(1 << 30);
    *(u32 *)&priv.curr_reg[0x8C] |= (priv.isp_param.tmnr.auto_recover << 30);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_auto_recover_en);

void api_set_tmnr_rapid_recover_en(bool rapid_recover_en)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(rapid_recover,rapid_recover_en);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x8C] &= ~(1 << 31);
    *(u32 *)&priv.curr_reg[0x8C] |= (priv.isp_param.tmnr.rapid_recover << 31);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_rapid_recover_en);

void api_set_tmnr_Motion_th(int Motion_top_lft_th,int Motion_top_nrm_th,
                                  int Motion_top_rgt_th,int Motion_nrm_lft_th,
                                  int Motion_nrm_nrm_th,int Motion_nrm_rgt_th,
                                  int Motion_bot_lft_th,int Motion_bot_nrm_th,
                                  int Motion_bot_rgt_th)
{
    DRV_SEMA_LOCK;
    SET_ISP_TMNR(Motion_top_lft_th,Motion_top_lft_th);
    SET_ISP_TMNR(Motion_top_nrm_th,Motion_top_nrm_th);
    SET_ISP_TMNR(Motion_top_rgt_th,Motion_top_rgt_th);
    SET_ISP_TMNR(Motion_nrm_lft_th,Motion_nrm_lft_th);
    SET_ISP_TMNR(Motion_nrm_nrm_th,Motion_nrm_nrm_th);
    SET_ISP_TMNR(Motion_nrm_rgt_th,Motion_nrm_rgt_th);
    SET_ISP_TMNR(Motion_bot_lft_th,Motion_bot_lft_th);
    SET_ISP_TMNR(Motion_bot_nrm_th,Motion_bot_nrm_th);
    SET_ISP_TMNR(Motion_bot_rgt_th,Motion_bot_rgt_th);
    TMNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x98] = priv.isp_param.tmnr.Motion_top_lft_th |
                                   (priv.isp_param.tmnr.Motion_top_nrm_th << 16);
    *(u32 *)&priv.curr_reg[0x9C] = priv.isp_param.tmnr.Motion_top_rgt_th |
                                   (priv.isp_param.tmnr.Motion_nrm_lft_th << 16);
    *(u32 *)&priv.curr_reg[0xA0] = priv.isp_param.tmnr.Motion_nrm_nrm_th |
                                   (priv.isp_param.tmnr.Motion_nrm_rgt_th << 16);
    *(u32 *)&priv.curr_reg[0xA4] = priv.isp_param.tmnr.Motion_bot_lft_th |
                                   (priv.isp_param.tmnr.Motion_bot_nrm_th << 16);
    *(u32 *)&priv.curr_reg[0xA8] = priv.isp_param.tmnr.Motion_bot_rgt_th;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_tmnr_Motion_th);

bool api_get_tmnr_en(void)
{
    bool val;
    DRV_SEMA_LOCK;
    GET_ISP_CTRL(temporal_en,val);
    DRV_SEMA_UNLOCK;
    return val;
}
EXPORT_SYMBOL(api_get_tmnr_en);

int api_get_tmnr_param(tmnr_param_t *ptmnr_param)
{
    if (!ptmnr_param)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memset(ptmnr_param,0,sizeof(tmnr_param_t));
    memcpy(ptmnr_param,&priv.isp_param.tmnr,sizeof(tmnr_param_t));
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_get_tmnr_param);

/*
    MRNR export APIs
*/
void api_set_mrnr_en(bool enable)
{
    DRV_SEMA_LOCK;
    // set spatial_en enable/disable
    SET_ISP_CTRL(spatial_en,enable);
    MRNR_MOD_INC;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_mrnr_en);

int api_set_mrnr_param(mrnr_param_t *pMRNRth)
{
    if (!pMRNRth)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    // set MRNR threshold parameters
    memcpy(&priv.isp_param.mrnr, pMRNRth, sizeof(mrnr_param_t));
    MRNR_MOD_INC;

    *(u32 *)&priv.curr_reg[0x10] = priv.isp_param.mrnr.Y_L_edg_th[0][0] | (priv.isp_param.mrnr.Y_L_edg_th[0][1] << 16);
    *(u32 *)&priv.curr_reg[0x14] = priv.isp_param.mrnr.Y_L_edg_th[0][2] | (priv.isp_param.mrnr.Y_L_edg_th[0][3] << 16);
    *(u32 *)&priv.curr_reg[0x18] = priv.isp_param.mrnr.Y_L_edg_th[0][4] | (priv.isp_param.mrnr.Y_L_edg_th[0][5] << 16);
    *(u32 *)&priv.curr_reg[0x1c] = priv.isp_param.mrnr.Y_L_edg_th[0][6] | (priv.isp_param.mrnr.Y_L_edg_th[0][7] << 16);
    *(u32 *)&priv.curr_reg[0x20] = priv.isp_param.mrnr.Y_L_edg_th[1][0] | (priv.isp_param.mrnr.Y_L_edg_th[1][1] << 16);
    *(u32 *)&priv.curr_reg[0x24] = priv.isp_param.mrnr.Y_L_edg_th[1][2] | (priv.isp_param.mrnr.Y_L_edg_th[1][3] << 16);
    *(u32 *)&priv.curr_reg[0x28] = priv.isp_param.mrnr.Y_L_edg_th[1][4] | (priv.isp_param.mrnr.Y_L_edg_th[1][5] << 16);
    *(u32 *)&priv.curr_reg[0x2c] = priv.isp_param.mrnr.Y_L_edg_th[1][6] | (priv.isp_param.mrnr.Y_L_edg_th[1][7] << 16);
    *(u32 *)&priv.curr_reg[0x30] = priv.isp_param.mrnr.Y_L_edg_th[2][0] | (priv.isp_param.mrnr.Y_L_edg_th[2][1] << 16);
    *(u32 *)&priv.curr_reg[0x34] = priv.isp_param.mrnr.Y_L_edg_th[2][2] | (priv.isp_param.mrnr.Y_L_edg_th[2][3] << 16);
    *(u32 *)&priv.curr_reg[0x38] = priv.isp_param.mrnr.Y_L_edg_th[2][4] | (priv.isp_param.mrnr.Y_L_edg_th[2][5] << 16);
    *(u32 *)&priv.curr_reg[0x3c] = priv.isp_param.mrnr.Y_L_edg_th[2][6] | (priv.isp_param.mrnr.Y_L_edg_th[2][7] << 16);
    *(u32 *)&priv.curr_reg[0x40] = priv.isp_param.mrnr.Y_L_edg_th[3][0] | (priv.isp_param.mrnr.Y_L_edg_th[3][1] << 16);
    *(u32 *)&priv.curr_reg[0x44] = priv.isp_param.mrnr.Y_L_edg_th[3][2] | (priv.isp_param.mrnr.Y_L_edg_th[3][3] << 16);
    *(u32 *)&priv.curr_reg[0x48] = priv.isp_param.mrnr.Y_L_edg_th[3][4] | (priv.isp_param.mrnr.Y_L_edg_th[3][5] << 16);
    *(u32 *)&priv.curr_reg[0x4c]  = priv.isp_param.mrnr.Y_L_edg_th[3][6] | (priv.isp_param.mrnr.Y_L_edg_th[3][7] << 16);
    *(u32 *)&priv.curr_reg[0x50] = priv.isp_param.mrnr.Cb_L_edg_th[0] | (priv.isp_param.mrnr.Cb_L_edg_th[1] << 16);
    *(u32 *)&priv.curr_reg[0x54] = priv.isp_param.mrnr.Cb_L_edg_th[2] | (priv.isp_param.mrnr.Cb_L_edg_th[3] << 16);
    *(u32 *)&priv.curr_reg[0x58] = priv.isp_param.mrnr.Cr_L_edg_th[0] | (priv.isp_param.mrnr.Cr_L_edg_th[1] << 16);
    *(u32 *)&priv.curr_reg[0x5c] = priv.isp_param.mrnr.Cr_L_edg_th[2] | (priv.isp_param.mrnr.Cr_L_edg_th[3] << 16);
    *(u32 *)&priv.curr_reg[0x60] = priv.isp_param.mrnr.Y_L_smo_th[0][0] | (priv.isp_param.mrnr.Y_L_smo_th[0][1] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[0][2] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[0][3] << 24);
    *(u32 *)&priv.curr_reg[0x64] = priv.isp_param.mrnr.Y_L_smo_th[0][4] | (priv.isp_param.mrnr.Y_L_smo_th[0][5] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[0][6] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[0][7] << 24);
    *(u32 *)&priv.curr_reg[0x68] = priv.isp_param.mrnr.Y_L_smo_th[1][0] | (priv.isp_param.mrnr.Y_L_smo_th[1][1] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[1][2] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[1][3] << 24);
    *(u32 *)&priv.curr_reg[0x6c] = priv.isp_param.mrnr.Y_L_smo_th[1][4] | (priv.isp_param.mrnr.Y_L_smo_th[1][5] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[1][6] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[1][7] << 24);
    *(u32 *)&priv.curr_reg[0x70] = priv.isp_param.mrnr.Y_L_smo_th[2][0] | (priv.isp_param.mrnr.Y_L_smo_th[2][1] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[2][2] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[2][3] << 24);
    *(u32 *)&priv.curr_reg[0x74] = priv.isp_param.mrnr.Y_L_smo_th[2][4] | (priv.isp_param.mrnr.Y_L_smo_th[2][5] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[2][6] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[2][7] << 24);
    *(u32 *)&priv.curr_reg[0x78] = priv.isp_param.mrnr.Y_L_smo_th[3][0] | (priv.isp_param.mrnr.Y_L_smo_th[3][1] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[3][2] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[3][3] << 24);
    *(u32 *)&priv.curr_reg[0x7c] = priv.isp_param.mrnr.Y_L_smo_th[3][4] | (priv.isp_param.mrnr.Y_L_smo_th[3][5] << 8) |
                                   (priv.isp_param.mrnr.Y_L_smo_th[3][6] << 16) | (priv.isp_param.mrnr.Y_L_smo_th[3][7] << 24);
    *(u32 *)&priv.curr_reg[0x80] = priv.isp_param.mrnr.Cb_L_smo_th[0] | (priv.isp_param.mrnr.Cb_L_smo_th[1] << 8) |
                                   (priv.isp_param.mrnr.Cb_L_smo_th[2] << 16) | (priv.isp_param.mrnr.Cb_L_smo_th[3] << 24);
    *(u32 *)&priv.curr_reg[0x84] = priv.isp_param.mrnr.Cr_L_smo_th[0] | (priv.isp_param.mrnr.Cr_L_smo_th[1] << 8) |
                                   (priv.isp_param.mrnr.Cr_L_smo_th[2] << 16) | (priv.isp_param.mrnr.Cr_L_smo_th[3] << 24);
    *(u32 *)&priv.curr_reg[0x88] = priv.isp_param.mrnr.Y_L_nr_str[0] | (priv.isp_param.mrnr.Y_L_nr_str[1] << 4) |
                                   (priv.isp_param.mrnr.Y_L_nr_str[2] << 8) | (priv.isp_param.mrnr.Y_L_nr_str[3] << 12) |
                                   (priv.isp_param.mrnr.C_L_nr_str[0] << 16) | (priv.isp_param.mrnr.C_L_nr_str[1] << 20) |
                                   (priv.isp_param.mrnr.C_L_nr_str[2] << 24) | (priv.isp_param.mrnr.C_L_nr_str[3] << 28);
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_set_mrnr_param);

bool api_get_mnnr_en(void)
{
    bool val;
    DRV_SEMA_LOCK;
    GET_ISP_CTRL(spatial_en,val);
    DRV_SEMA_UNLOCK;
    return val;
}
EXPORT_SYMBOL(api_get_mnnr_en);

int api_get_mrnr_param(mrnr_param_t *pMRNRth)
{
    if (!pMRNRth)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memset(pMRNRth,0,sizeof(mrnr_param_t));
    memcpy(pMRNRth,&priv.isp_param.mrnr,sizeof(mrnr_param_t));
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_get_mrnr_param);

/*
    Sharpness export APIs
*/
int api_set_sp_param(sp_param_t *psp_param)
{
    sp_param_t *sp = NULL;

    if (!psp_param)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memcpy(&priv.isp_param.sp,psp_param,sizeof(sp_param_t));
    SP_MOD_INC;

    sp = &priv.isp_param.sp;

    *(u32 *)&priv.curr_reg[0xD8] =
        sp->hpf_use_lpf[0] | (sp->hpf_use_lpf[1] << 1) | (sp->hpf_use_lpf[2] << 2) |
        (sp->hpf_use_lpf[3] << 3) | (sp->hpf_use_lpf[4] << 4) | (sp->hpf_use_lpf[5] << 5) |
        (sp->hpf_enable[0] << 8) | (sp->hpf_enable[1] << 9) | (sp->hpf_enable[2] << 10) |
        (sp->hpf_enable[3] << 11) | (sp->hpf_enable[4] << 12) | (sp->hpf_enable[5] << 13) |
        (sp->edg_wt_enable << 16) | (sp->nl_gain_enable << 17);
    *(u32 *)&priv.curr_reg[0xDC] = (sp->hpf0_5x5_coeff[0] & 0xFFFF) | (sp->hpf0_5x5_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0xE0] = (sp->hpf0_5x5_coeff[2] & 0xFFFF) | (sp->hpf0_5x5_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0xE4] = (sp->hpf0_5x5_coeff[4] & 0xFFFF) | (sp->hpf0_5x5_coeff[5] << 16);
    *(u32 *)&priv.curr_reg[0xE8] = (sp->hpf0_5x5_coeff[6] & 0xFFFF) | (sp->hpf0_5x5_coeff[7] << 16);
    *(u32 *)&priv.curr_reg[0xEC] = (sp->hpf0_5x5_coeff[8] & 0xFFFF) | (sp->hpf0_5x5_coeff[9] << 16);
    *(u32 *)&priv.curr_reg[0xF0] = (sp->hpf0_5x5_coeff[10] & 0xFFFF) | (sp->hpf0_5x5_coeff[11] << 16);
    *(u32 *)&priv.curr_reg[0xF4] = (sp->hpf0_5x5_coeff[12] & 0xFFFF) | (sp->hpf0_5x5_coeff[13] << 16);
    *(u32 *)&priv.curr_reg[0xF8] = (sp->hpf0_5x5_coeff[14] & 0xFFFF) | (sp->hpf0_5x5_coeff[15] << 16);
    *(u32 *)&priv.curr_reg[0xFC] = (sp->hpf0_5x5_coeff[16] & 0xFFFF) | (sp->hpf0_5x5_coeff[17] << 16);
    *(u32 *)&priv.curr_reg[0x100] = (sp->hpf0_5x5_coeff[18] & 0xFFFF) | (sp->hpf0_5x5_coeff[19] << 16);
    *(u32 *)&priv.curr_reg[0x104] = (sp->hpf0_5x5_coeff[20] & 0xFFFF) | (sp->hpf0_5x5_coeff[21] << 16);
    *(u32 *)&priv.curr_reg[0x108] = (sp->hpf0_5x5_coeff[22] & 0xFFFF) | (sp->hpf0_5x5_coeff[23] << 16);
    *(u32 *)&priv.curr_reg[0x10C] = (sp->hpf0_5x5_coeff[24] & 0xFFFF);
    *(u32 *)&priv.curr_reg[0x110] = (sp->hpf1_3x3_coeff[0] & 0xFFFF) | (sp->hpf1_3x3_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x114] = (sp->hpf1_3x3_coeff[2] & 0xFFFF) | (sp->hpf1_3x3_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x118] = (sp->hpf1_3x3_coeff[4] & 0xFFFF) | (sp->hpf1_3x3_coeff[5] << 16);
    *(u32 *)&priv.curr_reg[0x11C] = (sp->hpf1_3x3_coeff[6] & 0xFFFF) | (sp->hpf1_3x3_coeff[7] << 16);
    *(u32 *)&priv.curr_reg[0x120] = (sp->hpf1_3x3_coeff[8] & 0xFFFF);
    *(u32 *)&priv.curr_reg[0x124] = (sp->hpf2_1x5_coeff[0] & 0xFFFF) | (sp->hpf2_1x5_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x128] = (sp->hpf2_1x5_coeff[2] & 0xFFFF) | (sp->hpf2_1x5_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x12C] = (sp->hpf2_1x5_coeff[4] & 0xFFFF);
    *(u32 *)&priv.curr_reg[0x130] = (sp->hpf3_1x5_coeff[0] & 0xFFFF) | (sp->hpf3_1x5_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x134] = (sp->hpf3_1x5_coeff[2] & 0xFFFF) | (sp->hpf3_1x5_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x138] = (sp->hpf3_1x5_coeff[4] | 0xFFFF);
    *(u32 *)&priv.curr_reg[0x13C] = (sp->hpf4_5x1_coeff[0] & 0xFFFF) | (sp->hpf4_5x1_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x140] = (sp->hpf4_5x1_coeff[2] & 0xFFFF) | (sp->hpf4_5x1_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x144] = (sp->hpf4_5x1_coeff[4] & 0xFFFF);
    *(u32 *)&priv.curr_reg[0x148] = (sp->hpf5_5x1_coeff[0] & 0xFFFF) | (sp->hpf5_5x1_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x14C] = (sp->hpf5_5x1_coeff[2] & 0xFFFF) | (sp->hpf5_5x1_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x150] = (sp->hpf5_5x1_coeff[4] & 0xFFFF);
    *(u32 *)&priv.curr_reg[0x154] = (sp->hpf_gain[0]) | (sp->hpf_bright_clip[0] << 16) | (sp->hpf_dark_clip[0] << 24);
    *(u32 *)&priv.curr_reg[0x158] = (sp->hpf_shift[0]) | (sp->hpf_coring_th[0] << 8) | (sp->hpf_peak_gain[0] << 16);
    *(u32 *)&priv.curr_reg[0x15C] = (sp->hpf_gain[1]) | (sp->hpf_bright_clip[1] << 16) | (sp->hpf_dark_clip[1] << 24);
    *(u32 *)&priv.curr_reg[0x160] = (sp->hpf_shift[1]) | (sp->hpf_coring_th[1] << 8) | (sp->hpf_peak_gain[1] << 16);
    *(u32 *)&priv.curr_reg[0x164] = (sp->hpf_gain[2]) | (sp->hpf_bright_clip[2] << 16) | (sp->hpf_dark_clip[2] << 24);
    *(u32 *)&priv.curr_reg[0x168] = (sp->hpf_shift[2]) | (sp->hpf_coring_th[2] << 8) | (sp->hpf_peak_gain[2] << 16);
    *(u32 *)&priv.curr_reg[0x16C] = (sp->hpf_gain[3]) | (sp->hpf_bright_clip[3] << 16) | (sp->hpf_dark_clip[3] << 24);
    *(u32 *)&priv.curr_reg[0x170] = (sp->hpf_shift[3]) | (sp->hpf_coring_th[3] << 8) | (sp->hpf_peak_gain[3] << 16);
    *(u32 *)&priv.curr_reg[0x174] = (sp->hpf_gain[4]) | (sp->hpf_bright_clip[4] << 16) | (sp->hpf_dark_clip[4] << 24);
    *(u32 *)&priv.curr_reg[0x178] = (sp->hpf_shift[4]) | (sp->hpf_coring_th[4] << 8) | (sp->hpf_peak_gain[4] << 16);
    *(u32 *)&priv.curr_reg[0x17C] = (sp->hpf_gain[5]) | (sp->hpf_bright_clip[5] << 16) | (sp->hpf_dark_clip[5] << 24);
    *(u32 *)&priv.curr_reg[0x180] = (sp->hpf_shift[5]) | (sp->hpf_coring_th[5] << 8) | (sp->hpf_peak_gain[5] << 16);
    *(u32 *)&priv.curr_reg[0x184] = (sp->edg_wt_curve.index[0]) | (sp->edg_wt_curve.index[1] << 8) |
          (sp->edg_wt_curve.index[2] << 16) | (sp->edg_wt_curve.index[3] << 24);
    *(u32 *)&priv.curr_reg[0x188] = (sp->edg_wt_curve.value[0]) | (sp->edg_wt_curve.value[1] << 8) |
          (sp->edg_wt_curve.value[2] << 16) | (sp->edg_wt_curve.value[3] << 24);
    *(u32 *)&priv.curr_reg[0x18C] = (sp->edg_wt_curve.slope[0]) | (sp->edg_wt_curve.slope[1] << 16);
    *(u32 *)&priv.curr_reg[0x190] = (sp->edg_wt_curve.slope[2]) | (sp->edg_wt_curve.slope[3] << 16);
    *(u32 *)&priv.curr_reg[0x194] = (sp->edg_wt_curve.slope[4]);
    *(u32 *)&priv.curr_reg[0x198] = (sp->rec_bright_clip) | (sp->rec_dark_clip << 8) | (sp->rec_peak_gain << 16);
    *(u32 *)&priv.curr_reg[0x19C] = (sp->gau_5x5_coeff[0] & 0xFFFF) | (sp->gau_5x5_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x1A0] = (sp->gau_5x5_coeff[2] & 0xFFFF) | (sp->gau_5x5_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x1A4] = (sp->gau_5x5_coeff[4] & 0xFFFF) | (sp->gau_5x5_coeff[5] << 16);
    *(u32 *)&priv.curr_reg[0x1A8] = (sp->gau_5x5_coeff[6] & 0xFFFF) | (sp->gau_5x5_coeff[7] << 16);
    *(u32 *)&priv.curr_reg[0x1AC] = (sp->gau_5x5_coeff[8] & 0xFFFF) | (sp->gau_5x5_coeff[9] << 16);
    *(u32 *)&priv.curr_reg[0x1B0] = (sp->gau_5x5_coeff[10] & 0xFFFF) | (sp->gau_5x5_coeff[11] << 16);
    *(u32 *)&priv.curr_reg[0x1B4] = (sp->gau_5x5_coeff[12] & 0xFFFF) | (sp->gau_5x5_coeff[13] << 16);
    *(u32 *)&priv.curr_reg[0x1B8] = (sp->gau_5x5_coeff[14] & 0xFFFF) | (sp->gau_5x5_coeff[15] << 16);
    *(u32 *)&priv.curr_reg[0x1BC] = (sp->gau_5x5_coeff[16] & 0xFFFF) | (sp->gau_5x5_coeff[17] << 16);
    *(u32 *)&priv.curr_reg[0x1C0] = (sp->gau_5x5_coeff[18] & 0xFFFF) | (sp->gau_5x5_coeff[19] << 16);
    *(u32 *)&priv.curr_reg[0x1C4] = (sp->gau_5x5_coeff[20] & 0xFFFF) | (sp->gau_5x5_coeff[21] << 16);
    *(u32 *)&priv.curr_reg[0x1C8] = (sp->gau_5x5_coeff[22] & 0xFFFF) | (sp->gau_5x5_coeff[23] << 16);
    *(u32 *)&priv.curr_reg[0x1CC] = (sp->gau_5x5_coeff[24] & 0xFFFF) | (sp->gau_shift << 16);
    *(u32 *)&priv.curr_reg[0x1D0] = (sp->nl_gain_curve.index[0]) | (sp->nl_gain_curve.index[1] << 8) |
          (sp->nl_gain_curve.index[2] << 16) | (sp->nl_gain_curve.index[3] << 24);
    *(u32 *)&priv.curr_reg[0x1D4] = (sp->nl_gain_curve.value[0]) | (sp->nl_gain_curve.value[1] << 8) |
          (sp->nl_gain_curve.value[2] << 16) | (sp->nl_gain_curve.value[3] << 24);
    *(u32 *)&priv.curr_reg[0x1D8] = (sp->nl_gain_curve.slope[0]) | (sp->nl_gain_curve.slope[1] << 16);
    *(u32 *)&priv.curr_reg[0x1DC] = (sp->nl_gain_curve.slope[2]) | (sp->nl_gain_curve.slope[3] << 16);
    *(u32 *)&priv.curr_reg[0x1E0] = (sp->nl_gain_curve.slope[4]);
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_set_sp_param);

void api_set_sp_en(bool sp_en)
{
    DRV_SEMA_LOCK;
    SET_ISP_CTRL(ee_en,sp_en);
    SP_MOD_INC;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_en);

void api_set_sp_hpf_en(int idx, bool to_set)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_enable,to_set);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0xD8] &= ~(1 << (idx + 8));
    *(u32 *)&priv.curr_reg[0xD8] |= (priv.isp_param.sp.hpf_enable[idx] << (idx + 8));
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_en);

void api_set_sp_hpf_use_lpf(int idx, bool to_set)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_use_lpf,to_set);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0xD8] &= ~(1 << idx);
    *(u32 *)&priv.curr_reg[0xD8] |= (priv.isp_param.sp.hpf_use_lpf[idx] << idx);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_use_lpf);


void api_set_sp_nl_gain_en(bool to_set)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP(nl_gain_enable,to_set);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0xD8] &= ~(1 << 17);
    *(u32 *)&priv.curr_reg[0xD8] |= (priv.isp_param.sp.nl_gain_enable << 17);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_nl_gain_en);


void api_set_sp_edg_wt_en(bool to_set)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP(edg_wt_enable,to_set);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0xD8] &= ~(1 << 16);
    *(u32 *)&priv.curr_reg[0xD8] |= (priv.isp_param.sp.edg_wt_enable << 16);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_edg_wt_en);


void api_set_sp_gau_coeff(s16 *coeff, u8 shift)
{
    DRV_SEMA_LOCK;
    memcpy(priv.isp_param.sp.gau_5x5_coeff,coeff,sizeof(s16)*25);
    SET_ISP_SP(gau_shift,shift);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x19C] = (priv.isp_param.sp.gau_5x5_coeff[0] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[1] << 16);
    *(u32 *)&priv.curr_reg[0x1A0] = (priv.isp_param.sp.gau_5x5_coeff[2] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[3] << 16);
    *(u32 *)&priv.curr_reg[0x1A4] = (priv.isp_param.sp.gau_5x5_coeff[4] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[5] << 16);
    *(u32 *)&priv.curr_reg[0x1A8] = (priv.isp_param.sp.gau_5x5_coeff[6] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[7] << 16);
    *(u32 *)&priv.curr_reg[0x1AC] = (priv.isp_param.sp.gau_5x5_coeff[8] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[9] << 16);
    *(u32 *)&priv.curr_reg[0x1B0] = (priv.isp_param.sp.gau_5x5_coeff[10] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[11] << 16);
    *(u32 *)&priv.curr_reg[0x1B4] = (priv.isp_param.sp.gau_5x5_coeff[12] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[13] << 16);
    *(u32 *)&priv.curr_reg[0x1B8] = (priv.isp_param.sp.gau_5x5_coeff[14] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[15] << 16);
    *(u32 *)&priv.curr_reg[0x1BC] = (priv.isp_param.sp.gau_5x5_coeff[16] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[17] << 16);
    *(u32 *)&priv.curr_reg[0x1C0] = (priv.isp_param.sp.gau_5x5_coeff[18] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[19] << 16);
    *(u32 *)&priv.curr_reg[0x1C4] = (priv.isp_param.sp.gau_5x5_coeff[20] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[21] << 16);
    *(u32 *)&priv.curr_reg[0x1C8] = (priv.isp_param.sp.gau_5x5_coeff[22] & 0xFFFF) | (priv.isp_param.sp.gau_5x5_coeff[23] << 16);
    *(u32 *)&priv.curr_reg[0x1CC] = (priv.isp_param.sp.gau_5x5_coeff[24] & 0xFFFF) | (priv.isp_param.sp.gau_shift << 16);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_gau_coeff);


void api_set_sp_hpf_coeff(int idx, s16 *coeff, u8 shift)
{
    int arraySize[6] = {25,9,5,5,5,5};

    if ((idx<0)||(idx>5))
        return;

    DRV_SEMA_LOCK;
    switch (idx)
    {
        case 0:
            memcpy(priv.isp_param.sp.hpf0_5x5_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0xDC] = (priv.isp_param.sp.hpf0_5x5_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0xE0] = (priv.isp_param.sp.hpf0_5x5_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0xE4] = (priv.isp_param.sp.hpf0_5x5_coeff[4] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[5] << 16);
            *(u32 *)&priv.curr_reg[0xE8] = (priv.isp_param.sp.hpf0_5x5_coeff[6] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[7] << 16);
            *(u32 *)&priv.curr_reg[0xEC] = (priv.isp_param.sp.hpf0_5x5_coeff[8] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[9] << 16);
            *(u32 *)&priv.curr_reg[0xF0] = (priv.isp_param.sp.hpf0_5x5_coeff[10] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[11] << 16);
            *(u32 *)&priv.curr_reg[0xF4] = (priv.isp_param.sp.hpf0_5x5_coeff[12] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[13] << 16);
            *(u32 *)&priv.curr_reg[0xF8] = (priv.isp_param.sp.hpf0_5x5_coeff[14] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[15] << 16);
            *(u32 *)&priv.curr_reg[0xFC] = (priv.isp_param.sp.hpf0_5x5_coeff[16] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[17] << 16);
            *(u32 *)&priv.curr_reg[0x100] = (priv.isp_param.sp.hpf0_5x5_coeff[18] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[19] << 16);
            *(u32 *)&priv.curr_reg[0x104] = (priv.isp_param.sp.hpf0_5x5_coeff[20] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[21] << 16);
            *(u32 *)&priv.curr_reg[0x108] = (priv.isp_param.sp.hpf0_5x5_coeff[22] & 0xFFFF) | (priv.isp_param.sp.hpf0_5x5_coeff[23] << 16);
            *(u32 *)&priv.curr_reg[0x10C] = (priv.isp_param.sp.hpf0_5x5_coeff[24] & 0xFFFF);
            break;
        case 1:
            memcpy(priv.isp_param.sp.hpf1_3x3_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0x110] = (priv.isp_param.sp.hpf1_3x3_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf1_3x3_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0x114] = (priv.isp_param.sp.hpf1_3x3_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf1_3x3_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0x118] = (priv.isp_param.sp.hpf1_3x3_coeff[4] & 0xFFFF) | (priv.isp_param.sp.hpf1_3x3_coeff[5] << 16);
            *(u32 *)&priv.curr_reg[0x11C] = (priv.isp_param.sp.hpf1_3x3_coeff[6] & 0xFFFF) | (priv.isp_param.sp.hpf1_3x3_coeff[7] << 16);
            *(u32 *)&priv.curr_reg[0x120] = (priv.isp_param.sp.hpf1_3x3_coeff[8] & 0xFFFF);
            break;
        case 2:
            memcpy(priv.isp_param.sp.hpf2_1x5_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0x124] = (priv.isp_param.sp.hpf2_1x5_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf2_1x5_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0x128] = (priv.isp_param.sp.hpf2_1x5_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf2_1x5_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0x12C] = (priv.isp_param.sp.hpf2_1x5_coeff[4] & 0xFFFF);
            break;
        case 3:
            memcpy(priv.isp_param.sp.hpf3_1x5_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0x130] = (priv.isp_param.sp.hpf3_1x5_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf3_1x5_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0x134] = (priv.isp_param.sp.hpf3_1x5_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf3_1x5_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0x138] = (priv.isp_param.sp.hpf3_1x5_coeff[4] | 0xFFFF);
            break;
        case 4:
            memcpy(priv.isp_param.sp.hpf4_5x1_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0x13C] = (priv.isp_param.sp.hpf4_5x1_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf4_5x1_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0x140] = (priv.isp_param.sp.hpf4_5x1_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf4_5x1_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0x144] = (priv.isp_param.sp.hpf4_5x1_coeff[4] & 0xFFFF);
            break;
        case 5:
            memcpy(priv.isp_param.sp.hpf5_5x1_coeff,coeff,sizeof(s16)*arraySize[idx]);

            *(u32 *)&priv.curr_reg[0x148] = (priv.isp_param.sp.hpf5_5x1_coeff[0] & 0xFFFF) | (priv.isp_param.sp.hpf5_5x1_coeff[1] << 16);
            *(u32 *)&priv.curr_reg[0x14C] = (priv.isp_param.sp.hpf5_5x1_coeff[2] & 0xFFFF) | (priv.isp_param.sp.hpf5_5x1_coeff[3] << 16);
            *(u32 *)&priv.curr_reg[0x150] = (priv.isp_param.sp.hpf5_5x1_coeff[4] & 0xFFFF);
            break;
        default:
            break;
    }
    SET_ISP_SP_IDX(idx,hpf_shift,shift);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x158 + idx * 8] &= ~0xF;
    *(u32 *)&priv.curr_reg[0x158 + idx * 8] |= priv.isp_param.sp.hpf_shift[idx];
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_coeff);


void api_set_sp_hpf_gain(int idx, u16 gain)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_gain,gain);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x154 + idx * 8] &= ~0xFFFF;
    *(u32 *)&priv.curr_reg[0x154 + idx * 8] |= (priv.isp_param.sp.hpf_gain[idx] & 0xFFFF);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_gain);


void api_set_sp_hpf_coring_th(int idx, u8 coring_th)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_coring_th,coring_th);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x158 + idx * 8] &= ~(0xFF << 8);
    *(u32 *)&priv.curr_reg[0x158 + idx * 8] |= (priv.isp_param.sp.hpf_coring_th[idx] << 8);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_coring_th);


void api_set_sp_hpf_bright_clip(int idx, u8 bright_clip)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_bright_clip,bright_clip);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x154 + idx * 8] &= ~(0xFF << 16);
    *(u32 *)&priv.curr_reg[0x154 + idx * 8] |= (priv.isp_param.sp.hpf_bright_clip[idx] << 16);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_bright_clip);


void api_set_sp_hpf_dark_clip(int idx, u8 dark_clip)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_dark_clip,dark_clip);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x154 + idx * 8] &= ~(0xFF << 24);
    *(u32 *)&priv.curr_reg[0x154 + idx * 8] |= (priv.isp_param.sp.hpf_dark_clip[idx] << 24);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_dark_clip);


void api_set_sp_hpf_peak_gain(int idx, u8 peak_gain)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP_IDX(idx,hpf_peak_gain,peak_gain);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x158 + idx * 8] &= ~(0xFF << 16);
    *(u32 *)&priv.curr_reg[0x158 + idx * 8] |= (priv.isp_param.sp.hpf_peak_gain[idx] << 16);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_hpf_peak_gain);


void api_set_sp_rec_bright_clip(u8 bright_clip)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP(rec_bright_clip,bright_clip);

    *(u32 *)&priv.curr_reg[0x198] &= ~0xFF;
    *(u32 *)&priv.curr_reg[0x198] |= priv.isp_param.sp.rec_bright_clip;
    SP_MOD_INC;
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_rec_bright_clip);


void api_set_sp_rec_dark_clip(u8 dark_clip)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP(rec_dark_clip,dark_clip);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x198] &= ~(0xFF << 8);
    *(u32 *)&priv.curr_reg[0x198] |= (priv.isp_param.sp.rec_dark_clip << 8);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_rec_dark_clip);


void api_set_sp_rec_peak_gain(u8 peak_gain)
{
    DRV_SEMA_LOCK;
    SET_ISP_SP(rec_peak_gain,peak_gain);
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x198] &= ~(0xFF << 16);
    *(u32 *)&priv.curr_reg[0x198] |= (priv.isp_param.sp.rec_peak_gain << 16);
    DRV_SEMA_UNLOCK;
}
EXPORT_SYMBOL(api_set_sp_rec_peak_gain);


int api_set_sp_nl_gain_curve(sp_curve_t *pcurve)
{
    if (!pcurve)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memcpy(&priv.isp_param.sp.nl_gain_curve,pcurve,sizeof(sp_curve_t));
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x1D0] = (priv.isp_param.sp.nl_gain_curve.index[0]) | (priv.isp_param.sp.nl_gain_curve.index[1] << 8) |
          (priv.isp_param.sp.nl_gain_curve.index[2] << 16) | (priv.isp_param.sp.nl_gain_curve.index[3] << 24);
    *(u32 *)&priv.curr_reg[0x1D4] = (priv.isp_param.sp.nl_gain_curve.value[0]) | (priv.isp_param.sp.nl_gain_curve.value[1] << 8) |
          (priv.isp_param.sp.nl_gain_curve.value[2] << 16) | (priv.isp_param.sp.nl_gain_curve.value[3] << 24);
    *(u32 *)&priv.curr_reg[0x1D8] = (priv.isp_param.sp.nl_gain_curve.slope[0]) | (priv.isp_param.sp.nl_gain_curve.slope[1] << 16);
    *(u32 *)&priv.curr_reg[0x1DC] = (priv.isp_param.sp.nl_gain_curve.slope[2]) | (priv.isp_param.sp.nl_gain_curve.slope[3] << 16);
    *(u32 *)&priv.curr_reg[0x1E0] = (priv.isp_param.sp.nl_gain_curve.slope[4]);
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_set_sp_nl_gain_curve);


int api_set_sp_edg_wt_curve(sp_curve_t *pcurve)
{
    if (!pcurve)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memcpy(&priv.isp_param.sp.edg_wt_curve,pcurve,sizeof(sp_curve_t));
    SP_MOD_INC;

    *(u32 *)&priv.curr_reg[0x184] = (priv.isp_param.sp.edg_wt_curve.index[0]) | (priv.isp_param.sp.edg_wt_curve.index[1] << 8) |
          (priv.isp_param.sp.edg_wt_curve.index[2] << 16) | (priv.isp_param.sp.edg_wt_curve.index[3] << 24);
    *(u32 *)&priv.curr_reg[0x188] = (priv.isp_param.sp.edg_wt_curve.value[0]) | (priv.isp_param.sp.edg_wt_curve.value[1] << 8) |
          (priv.isp_param.sp.edg_wt_curve.value[2] << 16) | (priv.isp_param.sp.edg_wt_curve.value[3] << 24);
    *(u32 *)&priv.curr_reg[0x18C] = (priv.isp_param.sp.edg_wt_curve.slope[0]) | (priv.isp_param.sp.edg_wt_curve.slope[1] << 16);
    *(u32 *)&priv.curr_reg[0x190] = (priv.isp_param.sp.edg_wt_curve.slope[2]) | (priv.isp_param.sp.edg_wt_curve.slope[3] << 16);
    *(u32 *)&priv.curr_reg[0x194] = (priv.isp_param.sp.edg_wt_curve.slope[4]);
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_set_sp_edg_wt_curve);

bool api_get_sp_en(void)
{
    bool val;
    DRV_SEMA_LOCK;
    GET_ISP_CTRL(ee_en,val);
    DRV_SEMA_UNLOCK;
    return val;
}
EXPORT_SYMBOL(api_get_sp_en);

int api_get_sp_param(sp_param_t *psp_param)
{
    if (!psp_param)
        return THDNR_ERR_NULL_PTR;

    DRV_SEMA_LOCK;
    memset(psp_param,0,sizeof(sp_param_t));
    memcpy(psp_param,&priv.isp_param.sp,sizeof(sp_param_t));
    DRV_SEMA_UNLOCK;

    return THDNR_ERR_NONE;
}
EXPORT_SYMBOL(api_get_sp_param);

int api_get_isp_set(u8 *curr_set, int len)
{
    int i;

    if (curr_set == NULL)
        return 0;

    DRV_SEMA_LOCK;
    for (i = 0; (i < len) && (i < sizeof(priv.reg_ary)); i++)
        *(curr_set + i) = priv.curr_reg[i];
    DRV_SEMA_UNLOCK;

    return i;
}
EXPORT_SYMBOL(api_get_isp_set);
