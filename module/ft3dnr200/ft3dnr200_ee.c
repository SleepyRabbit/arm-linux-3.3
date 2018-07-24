#include <linux/version.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "ft3dnr200.h"
#include "ft3dnr200_util.h"

static struct proc_dir_entry *eeproc;
static struct proc_dir_entry *ee_paramproc, *ee_spstrproc;

static u16 sp_str = DEF_EE_SP_STR;

static int ee_proc_param_show(struct seq_file *sfile, void *v)
{
    u32 base = priv.engine.ft3dnr_reg;
    u32 tmp = 0;

    tmp = *(volatile unsigned int *)(base + 0xD8);
    seq_printf(sfile, "Hpf0_5x5_UseLpf = %d\n", tmp & 0x1);
    seq_printf(sfile, "Hpf1_3x3_UseLpf = %d\n", (tmp >> 1) & 0x1);
    seq_printf(sfile, "Hpf2_1x5_UseLpf = %d\n", (tmp >> 2) & 0x1);
    seq_printf(sfile, "Hpf3_1x5_UseLpf = %d\n", (tmp >> 3) & 0x1);
    seq_printf(sfile, "Hpf4_5x1_UseLpf = %d\n", (tmp >> 4) & 0x1);
    seq_printf(sfile, "Hpf5_5x1_UseLpf = %d\n", (tmp >> 5) & 0x1);
    seq_printf(sfile, "Hpf0_5x5_Enable = %d\n", (tmp >> 8) & 0x1);
    seq_printf(sfile, "Hpf1_3x3_Enable = %d\n", (tmp >> 9) & 0x1);
    seq_printf(sfile, "Hpf2_1x5_Enable = %d\n", (tmp >> 10) & 0x1);
    seq_printf(sfile, "Hpf3_1x5_Enable = %d\n", (tmp >> 11) & 0x1);
    seq_printf(sfile, "Hpf4_5x1_Enable = %d\n", (tmp >> 12) & 0x1);
    seq_printf(sfile, "Hpf5_5x1_Enable = %d\n", (tmp >> 13) & 0x1);
    seq_printf(sfile, "Edg_Wt_Enable = %d\n", (tmp >> 16) & 0x1);
    seq_printf(sfile, "NL_Gain_Enable = %d\n", (tmp >> 17) & 0x1);

    tmp = *(volatile unsigned int *)(base + 0xDC);
    seq_printf(sfile, "Hpf0_5x5_Coeff00 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff01 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xE0);
    seq_printf(sfile, "Hpf0_5x5_Coeff02 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff03 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xE4);
    seq_printf(sfile, "Hpf0_5x5_Coeff04 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff10 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xE8);
    seq_printf(sfile, "Hpf0_5x5_Coeff11 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff12 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xEC);
    seq_printf(sfile, "Hpf0_5x5_Coeff13 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff14 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xF0);
    seq_printf(sfile, "Hpf0_5x5_Coeff20 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff21 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xF4);
    seq_printf(sfile, "Hpf0_5x5_Coeff22 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff23 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xF8);
    seq_printf(sfile, "Hpf0_5x5_Coeff24 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff30 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0xFC);
    seq_printf(sfile, "Hpf0_5x5_Coeff31 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff32 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x100);
    seq_printf(sfile, "Hpf0_5x5_Coeff33 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff34 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x104);
    seq_printf(sfile, "Hpf0_5x5_Coeff40 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff41 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x108);
    seq_printf(sfile, "Hpf0_5x5_Coeff42 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf0_5x5_Coeff43 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x10C);
    seq_printf(sfile, "Hpf0_5x5_Coeff44 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x110);
    seq_printf(sfile, "Hpf1_3x3_Coeff00 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf1_3x3_Coeff01 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x114);
    seq_printf(sfile, "Hpf1_3x3_Coeff02 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf1_3x3_Coeff10 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x118);
    seq_printf(sfile, "Hpf1_3x3_Coeff11 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf1_3x3_Coeff12 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x11C);
    seq_printf(sfile, "Hpf1_3x3_Coeff20 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf1_3x3_Coeff21 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x120);
    seq_printf(sfile, "Hpf1_3x3_Coeff22 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x124);
    seq_printf(sfile, "Hpf2_1x5_Coeff0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf2_1x5_Coeff1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x128);
    seq_printf(sfile, "Hpf2_1x5_Coeff2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf2_1x5_Coeff3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x12C);
    seq_printf(sfile, "Hpf2_1x5_Coeff4 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x130);
    seq_printf(sfile, "Hpf3_1x5_Coeff0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf3_1x5_Coeff1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x134);
    seq_printf(sfile, "Hpf3_1x5_Coeff2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf3_1x5_Coeff3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x138);
    seq_printf(sfile, "Hpf3_1x5_Coeff4 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x13C);
    seq_printf(sfile, "Hpf4_5x1_Coeff0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf4_5x1_Coeff1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x140);
    seq_printf(sfile, "Hpf4_5x1_Coeff2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf4_5x1_Coeff3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x144);
    seq_printf(sfile, "Hpf4_5x1_Coeff4 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x148);
    seq_printf(sfile, "Hpf5_5x1_Coeff0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf5_5x1_Coeff1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x14C);
    seq_printf(sfile, "Hpf5_5x1_Coeff2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Hpf5_5x1_Coeff3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x150);
    seq_printf(sfile, "Hpf5_5x1_Coeff4 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x154);
    seq_printf(sfile, "Hpf0_5x5_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf0_5x5_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf0_5x5_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x158);
    seq_printf(sfile, "Hpf0_5x5_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf0_5x5_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf0_5x5_PeakGain = %d\n", (tmp >> 16) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x15C);
    seq_printf(sfile, "Hpf1_3x3_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf1_3x3_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf1_3x3_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x160);
    seq_printf(sfile, "Hpf1_3x3_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf1_3x3_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf1_3x3_PeakGain = %d\n", (tmp >> 16) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x164);
    seq_printf(sfile, "Hpf2_1x5_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf2_1x5_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf2_1x5_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x168);
    seq_printf(sfile, "Hpf2_1x5_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf2_1x5_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf2_1x5_PeakGain = %d\n", (tmp >> 16) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x16C);
    seq_printf(sfile, "Hpf3_1x5_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf3_1x5_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf3_1x5_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x170);
    seq_printf(sfile, "Hpf3_1x5_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf3_1x5_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf3_1x5_PeakGain = %d\n", (tmp >> 16) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x174);
    seq_printf(sfile, "Hpf4_5x1_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf4_5x1_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf4_5x1_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x178);
    seq_printf(sfile, "Hpf4_5x1_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf4_5x1_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf4_5x1_PeakGain = %d\n", (tmp >> 16) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x17C);
    seq_printf(sfile, "Hpf5_5x1_Gain = %d\n", tmp & 0x3FF);
    seq_printf(sfile, "Hpf5_5x1_BrightClipGain = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Hpf5_5x1_DarkClipGain = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x180);
    seq_printf(sfile, "Hpf5_5x1_Shift = %d\n", tmp & 0xF);
    seq_printf(sfile, "Hpf5_5x1_CoringTh = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Hpf5_5x1_PeakGain = %d\n", (tmp >> 16) & 0xFF);

    tmp = *(volatile unsigned int *)(base + 0x184);
    seq_printf(sfile, "Edg_Wt_Idx0 = %d\n", tmp & 0xFF);
    seq_printf(sfile, "Edg_Wt_Idx1 = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Edg_Wt_Idx2 = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Edg_Wt_Idx3 = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x188);
    seq_printf(sfile, "Edg_Wt_Val0 = %d\n", tmp & 0xFF);
    seq_printf(sfile, "Edg_Wt_Val1 = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Edg_Wt_Val2 = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "Edg_Wt_Val3 = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x18C);
    seq_printf(sfile, "Edg_Wt_Slope0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Edg_Wt_Slope1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x190);
    seq_printf(sfile, "Edg_Wt_Slope2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Edg_Wt_Slope3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x194);
    seq_printf(sfile, "Edg_Wt_Slope4 = %d\n", (s16) (tmp & 0xFFFF));

    tmp = *(volatile unsigned int *)(base + 0x198);
    seq_printf(sfile, "Rec_BrightClipGain = %d\n", tmp & 0xFF);
    seq_printf(sfile, "Rec_DarkClipGain = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "Rec_PeakGain = %d\n", (tmp >> 16) & 0xFF);

    tmp = *(volatile unsigned int *)(base + 0x19C);
    seq_printf(sfile, "Gau_5x5_Coeff00 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff01 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1A0);
    seq_printf(sfile, "Gau_5x5_Coeff02 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff03 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1A4);
    seq_printf(sfile, "Gau_5x5_Coeff04 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff10 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1A8);
    seq_printf(sfile, "Gau_5x5_Coeff11 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff12 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1AC);
    seq_printf(sfile, "Gau_5x5_Coeff13 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff14 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1B0);
    seq_printf(sfile, "Gau_5x5_Coeff20 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff21 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1B4);
    seq_printf(sfile, "Gau_5x5_Coeff22 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff23 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1B8);
    seq_printf(sfile, "Gau_5x5_Coeff24 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff30 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1BC);
    seq_printf(sfile, "Gau_5x5_Coeff31 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff32 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1C0);
    seq_printf(sfile, "Gau_5x5_Coeff33 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff34 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1C4);
    seq_printf(sfile, "Gau_5x5_Coeff40 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff41 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1C8);
    seq_printf(sfile, "Gau_5x5_Coeff42 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Coeff43 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1CC);
    seq_printf(sfile, "Gau_5x5_Coeff44 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "Gau_5x5_Shift = %d\n", (tmp >> 16) & 0xF);

    tmp = *(volatile unsigned int *)(base + 0x1D0);
    seq_printf(sfile, "NL_Gain_Idx0 = %d\n", tmp & 0xFF);
    seq_printf(sfile, "NL_Gain_Idx1 = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "NL_Gain_Idx2 = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "NL_Gain_Idx3 = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x1D4);
    seq_printf(sfile, "NL_Gain_Val0 = %d\n", tmp & 0xFF);
    seq_printf(sfile, "NL_Gain_Val1 = %d\n", (tmp >> 8) & 0xFF);
    seq_printf(sfile, "NL_Gain_Val2 = %d\n", (tmp >> 16) & 0xFF);
    seq_printf(sfile, "NL_Gain_Val3 = %d\n", (tmp >> 24) & 0xFF);
    tmp = *(volatile unsigned int *)(base + 0x1D8);
    seq_printf(sfile, "NL_Gain_Slope0 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "NL_Gain_Slope1 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1DC);
    seq_printf(sfile, "NL_Gain_Slope2 = %d\n", (s16) (tmp & 0xFFFF));
    seq_printf(sfile, "NL_Gain_Slope3 = %d\n", (s16) ((tmp >> 16) & 0xFFFF));
    tmp = *(volatile unsigned int *)(base + 0x1E0);
    seq_printf(sfile, "NL_Gain_Slope4 = %d\n", (s16) (tmp & 0xFFFF));

    return 0;
}

static int ee_proc_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, ee_proc_param_show, PDE(inode)->data);
}

static int ee_proc_spstr_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "sp_str = %d\n", sp_str);

    return 0;
}

static int ee_proc_spstr_open(struct inode *inode, struct file *file)
{
    return single_open(file, ee_proc_spstr_show, PDE(inode)->data);
}

static ssize_t ee_proc_spstr_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    u32 value = 0;
    char value_str[16] = {'\0'};

    if (copy_from_user(value_str, buffer, count))
        return -EFAULT;

    value_str[count] = '\0';
    sscanf(value_str, "%d\n", &value);
    sp_str = (u16)clamp(value, (u32)0, (u32)255);

    DRV_SEMA_LOCK;
    ft3dnr_sp_set_strength(sp_str);
    DRV_SEMA_UNLOCK;

    return count;
}

static struct file_operations ee_proc_param_ops = {
    .owner  = THIS_MODULE,
    .open   = ee_proc_param_open,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations ee_proc_spstr_ops = {
    .owner  = THIS_MODULE,
    .open   = ee_proc_spstr_open,
    .write  = ee_proc_spstr_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

int ft3dnr_ee_proc_init(struct proc_dir_entry *entity_proc)
{
    int ret = 0;

    eeproc = create_proc_entry("ee", S_IFDIR|S_IRUGO|S_IXUGO, entity_proc);
    if (eeproc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        ret = -EFAULT;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    eeproc->owner = THIS_MODULE;
#endif

    /* the tmnr parameter content of controller register */
    ee_paramproc = create_proc_entry("param", S_IRUGO|S_IXUGO, eeproc);
    if (ee_paramproc == NULL) {
        printk("error to create %s/ee/param proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ee_paramproc->proc_fops = &ee_proc_param_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ee_paramproc->owner = THIS_MODULE;
#endif

    /* the parameter interface of sp_str setting */
    ee_spstrproc = create_proc_entry("sp_str", S_IRUGO|S_IXUGO, eeproc);
    if (ee_spstrproc == NULL) {
        printk("error to create %s/ee/sp_str proc\n", FT3DNR_PROC_NAME);
        ret = -EFAULT;
    }
    ee_spstrproc->proc_fops = &ee_proc_spstr_ops;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    ee_spstrproc->owner = THIS_MODULE;
#endif

    return 0;
}

void ft3dnr_ee_proc_remove(struct proc_dir_entry *entity_proc)
{
    if (ee_spstrproc)
        remove_proc_entry(ee_spstrproc->name, eeproc);
    if (ee_paramproc)
        remove_proc_entry(ee_paramproc->name, eeproc);

    if (eeproc)
        remove_proc_entry(eeproc->name, eeproc->parent);
}
