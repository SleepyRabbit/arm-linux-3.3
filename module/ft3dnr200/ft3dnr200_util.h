#ifndef __FT3DNR200_UTIL_H__
#define __FT3DNR200_UTIL_H__

#define BITMASK_BASE(idx)   (idx >> 5)
#define BITMASK_IDX(idx)    (idx & 0x1F)

#define REFCNT_INC(x)       atomic_inc(&(x)->ref_cnt)
#define REFCNT_DEC(x)       do {if (atomic_read(&(x)->ref_cnt)) atomic_dec(&(x)->ref_cnt);} while(0)

#define JOBCNT_ADD(x, y)    atomic_add(y, &(x)->job_cnt)
#define JOBCNT_SUB(x, y)    do {if (atomic_read(&(x)->job_cnt)) atomic_sub(y, &(x)->job_cnt);} while(0)

#define BLKCNT_INC(x)       atomic_inc(&(x)->blk_cnt)
//#define BLKCNT_ADD(x, y)    atomic_add(y, &(x)->blk_cnt)
#define BLKCNT_SUB(x, y)    do {if (atomic_read(&(x)->blk_cnt)) atomic_sub(y, &(x)->blk_cnt);} while(0)

#define LL_ADDR(x)          (x * 0x100)

#define MASK_REF_INC(x, y, z)  atomic_inc(&(x)->mask_ref_cnt[y][z])
#define MASK_REF_DEC(x, y, z)  atomic_dec(&(x)->mask_ref_cnt[y][z])

#define OSD_REF_INC(x, y, z)   atomic_inc(&(x)->osd_ref_cnt[y][z])
#define OSD_REF_DEC(x, y, z)   atomic_dec(&(x)->osd_ref_cnt[y][z])

#define MEMCNT_ADD(x, y)    atomic_add(y, &(x)->mem_cnt)
#define MEMCNT_SUB(x, y)    do {if (atomic_read(&(x)->mem_cnt)) atomic_sub(y, &(x)->mem_cnt);} while(0)

typedef struct mrnr_op {
    int G1_Y_Freq[4];
    int G1_Y_Tone[4];
    int G1_C[4];
    int G2_Y;
    int G2_C;
    int Y_L_Nr_Str[4];
    int C_L_Nr_Str[4];
    unsigned int Y_noise_lvl[32];
    unsigned int Cb_noise_lvl[4];
    unsigned int Cr_noise_lvl[4];
} mrnr_op_t;

static inline int ft3dnr_get_tmnr_motion_th_val(int type, int nrWidth, int nrHeight, int mult)
{
    int val = 0;

    switch (type) {
        case MOTION_TH_TL:
        case MOTION_TH_NL:
            val = (32 * 100 * mult) >> 7;
            break;
        case MOTION_TH_TN:
        case MOTION_TH_NN:
            val = (32 * 128 * mult) >> 7;
            break;
        case MOTION_TH_TR:
        case MOTION_TH_NR:
        {
            s32 nr_widthm100;
            s32 rgt_res;
            nr_widthm100 = nrWidth - 100;
            rgt_res = ((nr_widthm100 % 128) == 0) ? 128 : (nr_widthm100 % 128);
            val = (32 * rgt_res * mult) >> 7;
            break;
        }
        case MOTION_TH_BL:
        {
            s32 bot_res;
            bot_res = (nrHeight % 32 == 0) ? 32 : (nrHeight % 32);
            val = (100 * bot_res * mult) >> 7;
            break;
        }
        case MOTION_TH_BN:
        {
            s32 bot_res;
            bot_res = (nrHeight % 32 == 0) ? 32 : (nrHeight % 32);
            val = (128 * bot_res * mult) >> 7;
            break;
        }
        case MOTION_TH_BR:
        {
            s32 nr_widthm100;
            s32 bot_res;
            s32 rgt_res;
            nr_widthm100 = nrWidth - 100;
            rgt_res = ((nr_widthm100 % 128) == 0) ? 128 : (nr_widthm100 % 128);
            bot_res = (nrHeight % 32 == 0) ? 32 : (nrHeight % 32);
            val = (rgt_res * bot_res * mult) >> 7;
            break;
        }
        default:
            break;
    }

    return val;
}

static inline void conv_mrnr_op_to_mrnr_param(mrnr_op_t *pmrnr_op, mrnr_param_t *pmrnr_param)
{
    int i, j;

    // convert to register
    for (i = 0; i < 4; i++) {
        // Y channel thresholds
        for (j = 0; j < 8; j++) {
            // "+8192" is for rounding operation; ">>14" is normalize for fixed-point operation; "1023" is the maximum value of this register
            pmrnr_param->Y_L_edg_th[i][j] = min((unsigned int)(((pmrnr_op->Y_noise_lvl[i*8+j]) * pmrnr_op->G1_Y_Freq[i] * pmrnr_op->G1_Y_Tone[j>>1] * 9 + 8192)>>14), (unsigned int)1023);
            // "+512" is for rounding operation; ">>10" is normalize for fixed-point operation; "255" is the maximum value of this register
            pmrnr_param->Y_L_smo_th[i][j] = min((unsigned int)(((pmrnr_op->Y_noise_lvl[i*8+j]) * pmrnr_op->G2_Y + 512)>>10), (unsigned int)255);
        }

        // Cb channel thresholds
        // "+512" is for rounding operation; ">>10" is normalize for fixed-point operation; "1023" is the maximum value of this register
        pmrnr_param->Cb_L_edg_th[i] = min((unsigned int)(((pmrnr_op->Cb_noise_lvl[i]) * pmrnr_op->G1_C[i] * 9 + 512)>>10), (unsigned int)1023);
        // "+512" is for rounding operation; ">>10" is normalize for fixed-point operation; "255" is the maximum value of this register
        pmrnr_param->Cb_L_smo_th[i] = min((unsigned int)(((pmrnr_op->Cb_noise_lvl[i]) * pmrnr_op->G2_C + 512)>>10), (unsigned int)255);

        // Cr channel thresholds
        // "+512" is for rounding operation; ">>10" is normalize for fixed-point operation; "1023" is the maximum value of this register
        pmrnr_param->Cr_L_edg_th[i] = min((unsigned int)(((pmrnr_op->Cr_noise_lvl[i]) * pmrnr_op->G1_C[i] * 9 + 512)>>10), (unsigned int)1023);
        // "+512" is for rounding operation; ">>10" is normalize for fixed-point operation; "255" is the maximum value of this register
        pmrnr_param->Cr_L_smo_th[i] = min((unsigned int)(((pmrnr_op->Cr_noise_lvl[i]) * pmrnr_op->G2_C + 512)>>10), (unsigned int)255);

        // Y and CbCr NR strength
        pmrnr_param->Y_L_nr_str[i] = min(pmrnr_op->Y_L_Nr_Str[i], 15);
        pmrnr_param->C_L_nr_str[i] = min(pmrnr_op->C_L_Nr_Str[i], 15);
    }
}

static inline void ft3dnr_nr_set_strength(int l_nr_str)
{
    int i;
    mrnr_op_t mrnr_op;

    mrnr_op.G1_Y_Freq[0] = 30;
    mrnr_op.G1_Y_Freq[1] = 22;
    mrnr_op.G1_Y_Freq[2] = 19;
    mrnr_op.G1_Y_Freq[3] = 16;
    mrnr_op.G1_Y_Tone[0] = 13;
    mrnr_op.G1_Y_Tone[1] = 19;
    mrnr_op.G1_Y_Tone[2] = 19;
    mrnr_op.G1_Y_Tone[3] = 13;
    mrnr_op.G1_C[0] = 26;
    mrnr_op.G1_C[1] = 26;
    mrnr_op.G1_C[2] = 26;
    mrnr_op.G1_C[3] = 26;
    mrnr_op.G2_Y = 144;
    mrnr_op.G2_C = 144;
    mrnr_op.Y_L_Nr_Str[0] = 8;
    mrnr_op.Y_L_Nr_Str[1] = 8;
    mrnr_op.Y_L_Nr_Str[2] = 8;
    mrnr_op.Y_L_Nr_Str[3] = 8;
    mrnr_op.C_L_Nr_Str[0] = 5;
    mrnr_op.C_L_Nr_Str[1] = 5;
    mrnr_op.C_L_Nr_Str[2] = 5;
    mrnr_op.C_L_Nr_Str[3] = 5;
    mrnr_op.Y_noise_lvl[0] = 202;
    mrnr_op.Y_noise_lvl[1] = 269;
    mrnr_op.Y_noise_lvl[2] = 266;
    mrnr_op.Y_noise_lvl[3] = 233;
    mrnr_op.Y_noise_lvl[4] = 207;
    mrnr_op.Y_noise_lvl[5] = 171;
    mrnr_op.Y_noise_lvl[6] = 147;
    mrnr_op.Y_noise_lvl[7] = 134;
    mrnr_op.Y_noise_lvl[8] = 151;
    mrnr_op.Y_noise_lvl[9] = 201;
    mrnr_op.Y_noise_lvl[10] = 199;
    mrnr_op.Y_noise_lvl[11] = 174;
    mrnr_op.Y_noise_lvl[12] = 155;
    mrnr_op.Y_noise_lvl[13] = 128;
    mrnr_op.Y_noise_lvl[14] = 110;
    mrnr_op.Y_noise_lvl[15] = 100;
    mrnr_op.Y_noise_lvl[16] = 101;
    mrnr_op.Y_noise_lvl[17] = 134;
    mrnr_op.Y_noise_lvl[18] = 133;
    mrnr_op.Y_noise_lvl[19] = 116;
    mrnr_op.Y_noise_lvl[20] = 103;
    mrnr_op.Y_noise_lvl[21] = 85;
    mrnr_op.Y_noise_lvl[22] = 73;
    mrnr_op.Y_noise_lvl[23] = 67;
    mrnr_op.Y_noise_lvl[24] = 50;
    mrnr_op.Y_noise_lvl[25] = 67;
    mrnr_op.Y_noise_lvl[26] = 66;
    mrnr_op.Y_noise_lvl[27] = 58;
    mrnr_op.Y_noise_lvl[28] = 51;
    mrnr_op.Y_noise_lvl[29] = 42;
    mrnr_op.Y_noise_lvl[30] = 36;
    mrnr_op.Y_noise_lvl[31] = 33;
    mrnr_op.Cb_noise_lvl[0] = 300;
    mrnr_op.Cb_noise_lvl[1] = 225;
    mrnr_op.Cb_noise_lvl[2] = 150;
    mrnr_op.Cb_noise_lvl[3] = 75;
    mrnr_op.Cr_noise_lvl[0] = 300;
    mrnr_op.Cr_noise_lvl[1] = 225;
    mrnr_op.Cr_noise_lvl[2] = 150;
    mrnr_op.Cr_noise_lvl[3] = 75;

    for (i = 0; i < 32; i++) {
        mrnr_op.Y_noise_lvl[i] = mrnr_op.Y_noise_lvl[i] * l_nr_str / 128;
    }

    for (i = 0; i < 4; i++) {
        mrnr_op.Cb_noise_lvl[i] = mrnr_op.Cb_noise_lvl[i] * l_nr_str / 128;
        mrnr_op.Cr_noise_lvl[i] = mrnr_op.Cr_noise_lvl[i] * l_nr_str / 128;
    }

    conv_mrnr_op_to_mrnr_param(&mrnr_op, &priv.default_param.mrnr);
}

static inline void ft3dnr_sp_set_strength(u16 val)
{
    int i;
    u16 hpf_gain[6] = {80, 48, 0, 0, 0, 0}, gain;

    for (i = 0; i < 2; i++) {
        if (val >= 128)
            gain = hpf_gain[i] + (val - 128);
        else
            gain = (hpf_gain[i] * val) >> 7;
        hpf_gain[i] = clamp(gain, (u16)0, (u16)1023);
        priv.default_param.sp.hpf_gain[i] = hpf_gain[i];
    }
}

#endif
