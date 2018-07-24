#ifndef _FE_TP2833_H_
#define _FE_TP2833_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  TP2833 Device Definition
 *************************************************************************************/
#define TP2833_DEV_MAX                 4
#define TP2833_DEV_CH_MAX              4
#define TP2833_DEV_AUDIO_CH_MAX        4    ///< AIN#0~3

#define TP2833_DEV_ID               0x23    ///< TP2833
#define TP2833_DEV_ID_REVC          0x26    ///< TP2833 Rev.C

typedef enum {
    TP2833_DEV_VOUT0 = 0,              ///< VD1
    TP2833_DEV_VOUT1,                  ///< VD2
    TP2833_DEV_VOUT_MAX
} TP2833_DEV_VOUT_T;

typedef enum {
    TP2833_VOUT_FORMAT_BT656 = 0,
    TP2833_VOUT_FORMAT_BT1120,
    TP2833_VOUT_FORMAT_MAX
} TP2833_VOUT_FORMAT_T;

typedef enum {
    TP2833_VOUT_MODE_1CH_BYPASS = 0,            ///< ch#0 rising edge
    TP2833_VOUT_MODE_2CH_DUAL_EDGE,             ///< ch#0 rising edge, ch#1 falling edge, only tp2833
    TP2833_VOUT_MODE_MAX
} TP2833_VOUT_MODE_T;

typedef enum {
    TP2833_VFMT_720P60 = 0,
    TP2833_VFMT_720P50,
    TP2833_VFMT_1080P30,
    TP2833_VFMT_1080P25,
    TP2833_VFMT_720P30,
    TP2833_VFMT_720P25,
    TP2833_VFMT_960H30,
    TP2833_VFMT_960H25,
    TP2833_VFMT_HALF_1080P30,
    TP2833_VFMT_HALF_1080P25,
    TP2833_VFMT_HALF_720P30,
    TP2833_VFMT_HALF_720P25,
    TP2833_VFMT_AHD_1080P30,
    TP2833_VFMT_AHD_1080P25,
    TP2833_VFMT_AHD_720P30,
    TP2833_VFMT_AHD_720P25,
    TP2833_VFMT_CVI_1080P25,
    TP2833_VFMT_CVI_720P25,
    TP2833_VFMT_MAX
} TP2833_VFMT_T;

struct tp2833_video_fmt_t {
    TP2833_VFMT_T fmt_idx;       ///< video format index
    unsigned int  width;         ///< video source width
    unsigned int  height;        ///< video source height
    unsigned int  prog;          ///< 0:interlace 1:progressive
    unsigned int  frame_rate;    ///< currect frame rate
};

typedef enum {
    AUDIO_SAMPLERATE_8K = 0,
    AUDIO_SAMPLERATE_16K,
}TP2833_SAMPLERATE_T;

typedef enum {
    AUDIO_BITS_16B = 0,
    AUDIO_BITS_8B,
}TP2833_SAMPLESIZE_T;

typedef struct {
    TP2833_SAMPLERATE_T sample_rate;
    TP2833_SAMPLESIZE_T sample_size;
    int ch_num;
} TP2833_AUDIO_INIT_T;

/*************************************************************************************
 *   TP2833 IOCTL
 *************************************************************************************/
struct tp2833_ioc_data_t {
    int vin_ch;                 ///< access tp2833 vin channel number
    int data;                   ///< read/write data value
};

struct tp2833_ioc_vfmt_t {
    int          vin_ch;        ///< tp2833 vin channel number
    unsigned int width;         ///< tp2833 channel video width
    unsigned int height;        ///< tp2833 channel video height
    unsigned int prog;          ///< 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< currect frame rate
};

#define  TP2833_IOC_MAGIC       't'

#define  TP2833_GET_NOVID       _IOR( TP2833_IOC_MAGIC,   1, struct tp2833_ioc_data_t)
#define  TP2833_GET_VIDEO_FMT   _IOR( TP2833_IOC_MAGIC,   2, struct tp2833_ioc_vfmt_t)

#define  TP2833_GET_VOL         _IOR( TP2833_IOC_MAGIC,  3, int)
#define  TP2833_SET_VOL         _IOWR( TP2833_IOC_MAGIC, 4, int)
/*************************************************************************************
 *   TP2833 API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int  tp2833_get_dev_id(int id);
int  tp2833_is_rev_c(int id);
int  tp2833_get_revision(int id);
int  tp2833_get_device_num(void);
int  tp2833_get_vch_id(int id, TP2833_DEV_VOUT_T vout, int seq_id);
int  tp2833_get_vout_id(int id, int vin_idx);
int  tp2833_get_vout_seq_id(int id, int vin_idx);
int  tp2833_get_vout_format(int id);
int  tp2833_get_vout_mode(int id);
int  tp2833_vin_to_vch(int id, int vin_idx);

u8   tp2833_i2c_read(u8 id, u8 reg);
int  tp2833_i2c_read_ext(u8 id, u8 reg);
int  tp2833_i2c_write(u8 id, u8 reg, u8 data);
int  tp2833_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt);

int  tp2833_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2833_video_fmt_t *));
void tp2833_notify_vfmt_deregister(int id);
int  tp2833_notify_vlos_register(int id, int (*nt_func)(int, int, int));
void tp2833_notify_vlos_deregister(int id);

int  tp2833_status_get_video_input_format(int id, int ch, struct tp2833_video_fmt_t *vfmt);
int  tp2833_status_get_video_loss(int id, int ch);

int  tp2833_video_init(int id, TP2833_VOUT_FORMAT_T vout_fmt, TP2833_VOUT_MODE_T vout_mode);
int  tp2833_video_get_video_output_format(int id, int ch, struct tp2833_video_fmt_t *vfmt);
int  tp2833_video_set_video_output_format(int id, int ch, TP2833_VFMT_T vfmt, TP2833_VOUT_FORMAT_T vout_fmt);
int  tp2833_video_set_pattern_output(int id, int ch, TP2833_VFMT_T vfmt, TP2833_VOUT_FORMAT_T vout_fmt, int enable);

int  tp2833_audio_set_mode(int id, TP2833_VOUT_MODE_T mode, TP2833_SAMPLESIZE_T samplesize, TP2833_SAMPLERATE_T samplerate);
int  tp2833_audio_set_mute(int id, int on);
int  tp2833_audio_get_mute(int id);
int  tp2833_audio_set_volume(int id, int volume);
int  tp2833_audio_get_volume(int id);
#endif

#endif  /* _FE_TP2833_H_ */
