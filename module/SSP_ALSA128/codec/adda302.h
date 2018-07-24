/*
 * $Author: sainthuang $
 * $Date: 2015/06/03 06:27:38 $
 *
 * $Log: adda302.h,v $
 * Revision 1.3  2015/06/03 06:27:38  sainthuang
 * *** empty log message ***
 *
 * Revision 1.2  2015/06/03 05:52:24  sainthuang
 * remove redundent code
 *
 * Revision 1.1  2015/04/20 09:36:54  sainthuang
 * add drvier of adda302 for ALSA
 *
 *
 */

#ifndef __ADDA302_H__
#define __ADDA302_H__

#define DEBUG_ADDA302			0
#define SSP_SLAVE_MODE			1

//debug helper
#if DEBUG_ADDA302
#define DEBUG_ADDA302_PRINT(FMT, ARGS...)    printk(FMT, ##ARGS)
#else
#define DEBUG_ADDA302_PRINT(FMT, ARGS...)
#endif

enum adda302_fs_frequency {
    ADDA302_FS_8K,
    ADDA302_FS_16K,
    ADDA302_FS_22_05K,
    ADDA302_FS_32K,
    ADDA302_FS_44_1K,
    ADDA302_FS_48K,
};

enum adda302_chip_select {
    ADDA302_FIRST_CHIP,
};

enum adda302_port_select {
    ADDA302_AD,
    ADDA302_DA,
};

enum adda302_i2s_select {
    ADDA302_FROM_PAD,
    ADDA302_FROM_SSP,
};

typedef struct adda302_priv {
    int                 adda302_fd;
    u32                 adda302_reg;
} adda302_priv_t;

void adda302_set_da_fs(const enum adda302_fs_frequency fs);
void adda302_set_ad_fs(const enum adda302_fs_frequency fs);

#endif //end of __ADDA300_H__
