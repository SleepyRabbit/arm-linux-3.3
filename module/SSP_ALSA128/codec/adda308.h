/*
 * $Author: sainthuang $
 * $Date: 2015/06/03 06:05:50 $
 *
 * $Log: adda308.h,v $
 * Revision 1.2  2015/06/03 06:05:50  sainthuang
 * remove redundent code
 *
 * Revision 1.1  2015/06/03 05:26:36  sainthuang
 * porting adda308 and gm8136 for alsa
 *
 *
 *
 *
 */

#ifndef __ADDA308_H__
#define __ADDA308_H__

#define DEBUG_ADDA308			0
#define SSP_SLAVE_MODE			1

//debug helper
#if DEBUG_ADDA308
#define DEBUG_ADDA308_PRINT(FMT, ARGS...)    printk(FMT, ##ARGS)
#else
#define DEBUG_ADDA308_PRINT(FMT, ARGS...)
#endif

enum adda308_fs_frequency {
	ADDA302_FS_8K,
	ADDA302_FS_16K,
	ADDA302_FS_22_05K,
	ADDA302_FS_32K,
	ADDA302_FS_44_1K,
	ADDA302_FS_48K,
};

enum adda308_chip_select {
    ADDA308_FIRST_CHIP,
};

enum adda308_port_select {
    ADDA308_AD,
    ADDA308_DA,
};

enum adda308_i2s_select {
    ADDA308_FROM_PAD,
    ADDA308_FROM_SSP,
};

typedef struct adda308_priv {
    int                 adda308_fd;
    u32                 adda308_reg;
} adda302_priv_t;

void adda302_set_da_fs(const enum adda308_fs_frequency fs);
void adda302_set_ad_fs(const enum adda308_fs_frequency fs);

#endif //end of __ADDA300_H__
