#ifndef _FE_TP2834_H_
#define _FE_TP2834_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  TP2834 Device Definition
 *************************************************************************************/
#define TP2834_DEV_MAX                 4
#define TP2834_DEV_CH_MAX              4
#define TP2834_DEV_AUDIO_CH_MAX        4    ///< AIN#0~3

#define TP2834_DEV_ID               0x24    ///< TP2834

typedef enum {
    TP2834_DEV_VOUT0 = 0,              ///< VD1
    TP2834_DEV_VOUT1,                  ///< VD2
    TP2834_DEV_VOUT2,                  ///< VD3
    TP2834_DEV_VOUT3,                  ///< VD4
    TP2834_DEV_VOUT_MAX
} TP2834_DEV_VOUT_T;

typedef enum {
    TP2834_VOUT_FORMAT_BT656 = 0,
    TP2834_VOUT_FORMAT_BT1120,
    TP2834_VOUT_FORMAT_MAX
} TP2834_VOUT_FORMAT_T;

typedef enum {
    TP2834_VOUT_MODE_1CH_BYPASS = 0,            ///< ch#0 rising edge
    TP2834_VOUT_MODE_2CH_DUAL_EDGE,             ///< ch#0 rising edge, ch#1 falling edge, only tp2834
    TP2834_VOUT_MODE_MAX
} TP2834_VOUT_MODE_T;

typedef enum {
    TP2834_VFMT_720P60 = 0,
    TP2834_VFMT_720P50,
    TP2834_VFMT_1080P30,
    TP2834_VFMT_1080P25,
    TP2834_VFMT_720P30,
    TP2834_VFMT_720P25,
    TP2834_VFMT_960H30,
    TP2834_VFMT_960H25,
    TP2834_VFMT_HALF_1080P30,
    TP2834_VFMT_HALF_1080P25,
    TP2834_VFMT_HALF_720P30,
    TP2834_VFMT_HALF_720P25,
    TP2834_VFMT_AHD_1080P30,
    TP2834_VFMT_AHD_1080P25,
    TP2834_VFMT_AHD_720P30,
    TP2834_VFMT_AHD_720P25,
    TP2834_VFMT_MAX
} TP2834_VFMT_T;

struct tp2834_video_fmt_t {
    TP2834_VFMT_T fmt_idx;       ///< video format index
    unsigned int  width;         ///< video source width
    unsigned int  height;        ///< video source height
    unsigned int  prog;          ///< 0:interlace 1:progressive
    unsigned int  frame_rate;    ///< currect frame rate
};

typedef enum {
    AUDIO_SAMPLERATE_8K = 0,
    AUDIO_SAMPLERATE_16K,
}TP2834_SAMPLERATE_T;

typedef enum {
    AUDIO_BITS_16B = 0,
    AUDIO_BITS_8B,
}TP2834_SAMPLESIZE_T;

typedef struct {
    TP2834_SAMPLERATE_T sample_rate;
    TP2834_SAMPLESIZE_T sample_size;
    int ch_num;
} TP2834_AUDIO_INIT_T;

/*************************************************************************************
 *   TP2834 IOCTL
 *************************************************************************************/
struct tp2834_ioc_data_t {
    int vin_ch;                 ///< access tp2834 vin channel number
    int data;                   ///< read/write data value
};

struct tp2834_ioc_vfmt_t {
    int          vin_ch;        ///< tp2834 vin channel number
    unsigned int width;         ///< tp2834 channel video width
    unsigned int height;        ///< tp2834 channel video height
    unsigned int prog;          ///< 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< currect frame rate
};

#define  TP2834_IOC_MAGIC       't'

#define  TP2834_GET_NOVID       _IOR( TP2834_IOC_MAGIC,   1, struct tp2834_ioc_data_t)
#define  TP2834_GET_VIDEO_FMT   _IOR( TP2834_IOC_MAGIC,   2, struct tp2834_ioc_vfmt_t)

#define  TP2834_GET_VOL         _IOR( TP2834_IOC_MAGIC,  3, int)
#define  TP2834_SET_VOL         _IOWR( TP2834_IOC_MAGIC, 4, int)
/*************************************************************************************
 *   TP2834 API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int  tp2834_ptz_init(int id);
int  tp2834_get_dev_id(int id);
int  tp2834_get_revision(int id);
int  tp2834_get_dev_id(int id);
int  tp2834_get_device_num(void);
int  tp2834_get_vch_id(int id, TP2834_DEV_VOUT_T vout, int seq_id);
int  tp2834_get_vout_id(int id, int vin_idx);
int  tp2834_get_vout_seq_id(int id, int vin_idx);
int  tp2834_get_vout_format(int id);
int  tp2834_get_vout_mode(int id);
int  tp2834_vin_to_vch(int id, int vin_idx);

u8   tp2834_i2c_read(u8 id, u8 reg);
int  tp2834_i2c_read_ext(u8 id, u8 reg);
int  tp2834_i2c_write(u8 id, u8 reg, u8 data);
int  tp2834_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt);

int  tp2834_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2834_video_fmt_t *));
void tp2834_notify_vfmt_deregister(int id);
int  tp2834_notify_vlos_register(int id, int (*nt_func)(int, int, int));
void tp2834_notify_vlos_deregister(int id);

int  tp2834_status_get_video_input_format(int id, int ch, struct tp2834_video_fmt_t *vfmt);
int  tp2834_status_get_video_loss(int id, int ch);

int  tp2834_video_init(int id, TP2834_VOUT_FORMAT_T vout_fmt, TP2834_VOUT_MODE_T vout_mode);
int  tp2834_video_get_video_output_format(int id, int ch, struct tp2834_video_fmt_t *vfmt);
int  tp2834_video_set_video_output_format(int id, int ch, TP2834_VFMT_T vfmt, TP2834_VOUT_FORMAT_T vout_fmt);
int  tp2834_video_set_pattern_output(int id, int ch, TP2834_VFMT_T vfmt, TP2834_VOUT_FORMAT_T vout_fmt, int enable);

int  tp2834_audio_set_mode(int id, TP2834_VOUT_MODE_T mode, TP2834_SAMPLESIZE_T samplesize, TP2834_SAMPLERATE_T samplerate);
int  tp2834_audio_set_mute(int id, int on);
int  tp2834_audio_get_mute(int id);
int  tp2834_audio_set_volume(int id, int volume);
int  tp2834_audio_get_volume(int id);
#endif

#endif  /* _FE_TP2834_H_ */
