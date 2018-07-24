#ifndef _FE_TP2824_H_
#define _FE_TP2824_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  TP2824 Device Definition
 *************************************************************************************/
#define TP2824_DEV_MAX                 4
#define TP2824_DEV_CH_MAX              4
#define TP2824_DEV_AUDIO_CH_MAX        4       ///< AIN#0~3

#define TP2824_DEV_ID               0x24    ///< TP2824

typedef enum {
    TP2824_DEV_VOUT0 = 0,              ///< VD1
    TP2824_DEV_VOUT1,                  ///< VD2
    TP2824_DEV_VOUT2,                  ///< VD3
    TP2824_DEV_VOUT3,                  ///< VD4
    TP2824_DEV_VOUT_MAX
} TP2824_DEV_VOUT_T;

typedef enum {
    TP2824_VOUT_FORMAT_BT656 = 0,
    TP2824_VOUT_FORMAT_BT1120,
    TP2824_VOUT_FORMAT_MAX
} TP2824_VOUT_FORMAT_T;

typedef enum {
    TP2824_VOUT_MODE_1CH_BYPASS = 0,            ///< ch#0 rising edge
    TP2824_VOUT_MODE_2CH_DUAL_EDGE,             ///< ch#0 rising edge, ch#1 falling edge, only tp2824
    TP2824_VOUT_MODE_MAX
} TP2824_VOUT_MODE_T;

typedef enum {
    TP2824_VFMT_720P60 = 0,
    TP2824_VFMT_720P50,
    TP2824_VFMT_1080P30,
    TP2824_VFMT_1080P25,
    TP2824_VFMT_720P30,
    TP2824_VFMT_720P25,
    TP2824_VFMT_960H30,
    TP2824_VFMT_960H25,
    TP2824_VFMT_HALF_1080P30,
    TP2824_VFMT_HALF_1080P25,
    TP2824_VFMT_HALF_720P30,
    TP2824_VFMT_HALF_720P25,
    TP2824_VFMT_MAX
} TP2824_VFMT_T;

struct tp2824_video_fmt_t {
    TP2824_VFMT_T fmt_idx;       ///< video format index
    unsigned int  width;         ///< video source width
    unsigned int  height;        ///< video source height
    unsigned int  prog;          ///< 0:interlace 1:progressive
    unsigned int  frame_rate;    ///< currect frame rate
};

typedef enum {
    AUDIO_SAMPLERATE_8K = 0,
    AUDIO_SAMPLERATE_16K,
}TP2824_SAMPLERATE_T;

typedef enum {
    AUDIO_BITS_16B = 0,
    AUDIO_BITS_8B,
}TP2824_SAMPLESIZE_T;

typedef struct {
    TP2824_SAMPLERATE_T sample_rate;
    TP2824_SAMPLESIZE_T sample_size;
    int ch_num;
} TP2824_AUDIO_INIT_T;

/*************************************************************************************
 *   TP2824 IOCTL
 *************************************************************************************/
struct tp2824_ioc_data_t {
    int vin_ch;                 ///< access tp2824 vin channel number
    int data;                   ///< read/write data value
};

struct tp2824_ioc_vfmt_t {
    int          vin_ch;        ///< tp2824 vin channel number
    unsigned int width;         ///< tp2824 channel video width
    unsigned int height;        ///< tp2824 channel video height
    unsigned int prog;          ///< 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< currect frame rate
};

#define  TP2824_IOC_MAGIC       't'

#define  TP2824_GET_NOVID       _IOR( TP2824_IOC_MAGIC,   1, struct tp2824_ioc_data_t)
#define  TP2824_GET_VIDEO_FMT   _IOR( TP2824_IOC_MAGIC,   2, struct tp2824_ioc_vfmt_t)

#define  TP2824_GET_VOL         _IOR( TP2824_IOC_MAGIC,  3, int)
#define  TP2824_SET_VOL         _IOWR( TP2824_IOC_MAGIC, 4, int)
/*************************************************************************************
 *   TP2824 API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int  tp2824_ptz_init(int id);
int  tp2824_get_dev_id(int id);
int  tp2824_get_revision(int id);
int  tp2824_get_dev_id(int id);
int  tp2824_get_device_num(void);
int  tp2824_get_vch_id(int id, TP2824_DEV_VOUT_T vout, int seq_id);
int  tp2824_get_vout_id(int id, int vin_idx);
int  tp2824_get_vout_seq_id(int id, int vin_idx);
int  tp2824_get_vout_format(int id);
int  tp2824_get_vout_mode(int id);
int  tp2824_vin_to_vch(int id, int vin_idx);

u8   tp2824_i2c_read(u8 id, u8 reg);
int  tp2824_i2c_read_ext(u8 id, u8 reg);
int  tp2824_i2c_write(u8 id, u8 reg, u8 data);
int  tp2824_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt);

int  tp2824_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2824_video_fmt_t *));
void tp2824_notify_vfmt_deregister(int id);
int  tp2824_notify_vlos_register(int id, int (*nt_func)(int, int, int));
void tp2824_notify_vlos_deregister(int id);

int  tp2824_status_get_video_input_format(int id, int ch, struct tp2824_video_fmt_t *vfmt);
int  tp2824_status_get_video_loss(int id, int ch);

int  tp2824_video_init(int id, TP2824_VOUT_FORMAT_T vout_fmt, TP2824_VOUT_MODE_T vout_mode);
int  tp2824_video_get_video_output_format(int id, int ch, struct tp2824_video_fmt_t *vfmt);
int  tp2824_video_set_video_output_format(int id, int ch, TP2824_VFMT_T vfmt, TP2824_VOUT_FORMAT_T vout_fmt);
int  tp2824_video_set_pattern_output(int id, int ch, TP2824_VFMT_T vfmt, TP2824_VOUT_FORMAT_T vout_fmt, int enable);

int  tp2824_audio_set_mode(int id, TP2824_VOUT_MODE_T mode, TP2824_SAMPLESIZE_T samplesize, TP2824_SAMPLERATE_T samplerate);
int  tp2824_audio_set_mute(int id, int on);
int  tp2824_audio_get_mute(int id);
int  tp2824_audio_set_volume(int id, int volume);
int  tp2824_audio_get_volume(int id);
#endif

#endif  /* _FE_TP2824_H_ */
