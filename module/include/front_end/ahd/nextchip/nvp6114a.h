#ifndef _FE_AHD_NVP6114A_H_
#define _FE_AHD_NVP6114A_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  NVP6114A Device Definition
 *************************************************************************************/
#define NVP6114A_DEV_MAX                 4
#define NVP6114A_DEV_CH_MAX              4       ///< VIN#0~3
#define NVP6114A_DEV_AUDIO_CH_MAX        8       ///< AIN#0~7

typedef enum {
    NVP6114A_DEV_VPORT_A = 0,
    NVP6114A_DEV_VPORT_B,
    NVP6114A_DEV_VPORT_MAX
} NVP6114A_DEV_VPORT_T;

typedef enum {
    NVP6114A_VMODE_1CH_BYPASS = 0,       ///< NTSC/PAL, 960H/AHD 720P/AFHD 1080P 
    NVP6114A_VMODE_NTSC_960H_2CH,        ///< 37.125MHz D1   2 channel (dual edge)
    NVP6114A_VMODE_NTSC_960H_4CH,        ///< 148.5MHz  D1   4 channel
    NVP6114A_VMODE_NTSC_720P_2CH,        ///< 148.5MHz  720P 2 channel (dual edge)
    NVP6114A_VMODE_PAL_960H_2CH,         ///< 74.25MHz  D1   2 channel (dual edge)
    NVP6114A_VMODE_PAL_960H_4CH,         ///< 148.5MHz  D1   4 channel
    NVP6114A_VMODE_PAL_720P_2CH,         ///< 148.5MHz  720P 2 channel (dual edge)

    /* 960H/720P Hybrid */
    NVP6114A_VMODE_NTSC_960H_720P_2CH,   ///< VPORTA=>960H 2CH, VPORTB=>720P 2CH (dual edge)
    NVP6114A_VMODE_NTSC_720P_960H_2CH,   ///< VPORTA=>720P 2CH, VPORTB=>960H 2CH (dual edge)
    NVP6114A_VMODE_PAL_960H_720P_2CH,    ///< VPORTA=>960H 2CH, VPORTB=>720P 2CH (dual edge)
    NVP6114A_VMODE_PAL_720P_960H_2CH,    ///< VPORTA=>720P 2CH, VPORTB=>960H 2CH (dual edge)

    /* CIF AHD/AFHD */
    NVP6114A_VMODE_NTSC_CIF_AHD_2CH,     ///< 74.25MHz 
    NVP6114A_VMODE_NTSC_CIF_AHD_4CH,     ///< 148.5MHz
    NVP6114A_VMODE_PAL_CIF_AHD_2CH,      ///< 74.25MHz
    NVP6114A_VMODE_PAL_CIF_AHD_4CH,      ///< 148.5MHz
    NVP6114A_VMODE_NTSC_CIF_AFHD_2CH,    ///< 148.5MHz 960*1080 2 channel (dual edge)
    NVP6114A_VMODE_PAL_CIF_AFHD_2CH,     ///< 148.5MHz 960*1080 2 channel (dual edge)

    /* AFHD-CIF/720P Hybrid */
    NVP6114A_VMODE_NTSC_CIF_AFHD_720P_2CH,   ///< VPORTA=>960*1080 2CH, VPORTB=>720P 2CH (dual edge)
    NVP6114A_VMODE_NTSC_720P_CIF_AFHD_2CH,   ///< VPORTA=>720P 2CH,     VPORTB=>960*1080 2CH (dual edge)
    NVP6114A_VMODE_PAL_CIF_AFHD_720P_2CH,    ///< VPORTA=>960*1080 2CH, VPORTB=>720P 2CH (dual edge)
    NVP6114A_VMODE_PAL_720P_CIF_AFHD_2CH,    ///< VPORTA=>720P 2CH,     VPORTB=>960*1080 2CH (dual edge)

    NVP6114A_VMODE_MAX
} NVP6114A_VMODE_T;

typedef enum {
    NVP6114A_VFMT_UNKNOWN = 0,
    //NVP6114A_VFMT_720H25,       // SD   720H  PAL
    //NVP6114A_VFMT_720H30,       // SD   720H  NTSC
    NVP6114A_VFMT_960H25,       // SD   960H  PAL
    NVP6114A_VFMT_960H30,       // SD   960H  NTSC
    NVP6114A_VFMT_720P25,       // AHD  720P  25FPS
    NVP6114A_VFMT_720P30,       // AHD  720P  30FPS
    NVP6114A_VFMT_720P50,       // AHD  720P  50FPS
    NVP6114A_VFMT_720P60,       // AHD  720P  60FPS
    NVP6114A_VFMT_1080P25,      // AFHD 1080P 25FPS
    NVP6114A_VFMT_1080P30,      // AFHD 1080P 30FPS
    NVP6114A_VFMT_AHD_CIF_P25,  // AHD  CIF   25FPS
    NVP6114A_VFMT_AHD_CIF_P30,  // AHD  CIF   30FPS
    NVP6114A_VFMT_AFHD_CIF_P25, // AFHD CIF   25FPS
    NVP6114A_VFMT_AFHD_CIF_P30, // AFHD CIF   30FPS
    NVP6114A_VFMT_MAX
} NVP6114A_VFMT_T;

struct nvp6114a_video_fmt_t {
    NVP6114A_VFMT_T fmt_idx;      ///< video format index
    unsigned int   width;         ///< video source width
    unsigned int   height;        ///< video source height
    unsigned int   prog;          ///< 0:interlace 1:progressive
    unsigned int   frame_rate;    ///< currect frame rate
};

typedef enum {
    AUDIO_SAMPLERATE_8K = 0,
    AUDIO_SAMPLERATE_16K,
}NVP6114A_SAMPLERATE_T;

typedef enum {
    AUDIO_BITS_16B = 0,
    AUDIO_BITS_8B,
}NVP6114A_SAMPLESIZE_T;

typedef struct {
    NVP6114A_SAMPLERATE_T sample_rate;
    NVP6114A_SAMPLESIZE_T sample_size;
    int ch_num;
} NVP6114A_AUDIO_INIT_T;

/*************************************************************************************
 *   NVP6114A IOCTL
 *************************************************************************************/
struct nvp6114a_ioc_data_t {
    int ch;         ///< access channel number ==> vin index
    int data;       ///< read/write data value
};

struct nvp6114a_ioc_vfmt_t {
    int          vin_ch;        ///< nvp6114a vin channel number
    unsigned int width;         ///< nvp6114a channel video width
    unsigned int height;        ///< nvp6114a channel video height
    unsigned int prog;          ///< 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< currect frame rate
};

#define NVP6114A_IOC_MAGIC       'n'

#define NVP6114A_GET_NOVID       _IOR(NVP6114A_IOC_MAGIC,   1, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_MODE        _IOR(NVP6114A_IOC_MAGIC,   2, int)
#define NVP6114A_SET_MODE        _IOWR(NVP6114A_IOC_MAGIC,  3, int)

#define NVP6114A_GET_CONTRAST    _IOR(NVP6114A_IOC_MAGIC,   4, struct nvp6114a_ioc_data_t)
#define NVP6114A_SET_CONTRAST    _IOWR(NVP6114A_IOC_MAGIC,  5, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_BRIGHTNESS  _IOR(NVP6114A_IOC_MAGIC,   6, struct nvp6114a_ioc_data_t)
#define NVP6114A_SET_BRIGHTNESS  _IOWR(NVP6114A_IOC_MAGIC,  7, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_SATURATION  _IOR(NVP6114A_IOC_MAGIC,   8, struct nvp6114a_ioc_data_t)
#define NVP6114A_SET_SATURATION  _IOWR(NVP6114A_IOC_MAGIC,  9, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_HUE         _IOR(NVP6114A_IOC_MAGIC,  10, struct nvp6114a_ioc_data_t)
#define NVP6114A_SET_HUE         _IOWR(NVP6114A_IOC_MAGIC, 11, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_SHARPNESS   _IOR(NVP6114A_IOC_MAGIC,  12, struct nvp6114a_ioc_data_t)
#define NVP6114A_SET_SHARPNESS   _IOWR(NVP6114A_IOC_MAGIC, 13, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_INPUT_VFMT  _IOR(NVP6114A_IOC_MAGIC,  14, struct nvp6114a_ioc_data_t)

#define NVP6114A_GET_VOL         _IOR(NVP6114A_IOC_MAGIC,  16, int)
#define NVP6114A_SET_VOL         _IOWR(NVP6114A_IOC_MAGIC, 17, int)

#define NVP6114A_GET_OUT_CH      _IOR(NVP6114A_IOC_MAGIC,  18, int)
#define NVP6114A_SET_OUT_CH      _IOWR(NVP6114A_IOC_MAGIC, 19, int)

/*************************************************************************************
 *  NVP6114A API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int nvp6114a_get_device_num(void);

int nvp6114a_vin_to_ch(int id, int vin_idx);
int nvp6114a_ch_to_vin(int id, int ch_idx);
int nvp6114a_get_vch_id(int id, NVP6114A_DEV_VPORT_T vport, int vport_seq);
int nvp6114a_vin_to_vch(int id, int vin_idx);
u8  nvp6114a_i2c_read(u8 id, u8 reg);
int nvp6114a_i2c_write(u8 id, u8 reg, u8 data);
int nvp6114a_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt);

int nvp6114a_video_get_output_format(int id, int ch, struct nvp6114a_video_fmt_t *vfmt, int vmode);
int nvp6114a_video_get_contrast(int id, int ch);
int nvp6114a_video_get_brightness(int id, int ch);
int nvp6114a_video_get_saturation(int id, int ch);
int nvp6114a_video_get_hue(int id, int ch);
int nvp6114a_video_get_sharpness(int id, int ch);

int nvp6114a_video_init(int id, int vmode);
int nvp6114a_video_set_equalizer(int id, int ch, NVP6114A_VFMT_T vfmt_idx);
int nvp6114a_video_set_output_format(int id, int ch, NVP6114A_VFMT_T vfmt_idx, int vmode);
int nvp6114a_video_set_output_mode(int id, int vmode);
int nvp6114a_video_set_contrast(int id, int ch, u8 value);
int nvp6114a_video_set_brightness(int id, int ch, u8 value);
int nvp6114a_video_set_saturation(int id, int ch, u8 value);
int nvp6114a_video_set_hue(int id, int ch, u8 value);
int nvp6114a_video_set_sharpness(int id, int ch, u8 value);

int nvp6114a_status_get_novid(int id);
int nvp6114a_status_get_video_input_format(int id, int ch);

int nvp6114a_ain_to_ch(int id, int vin_idx);
int nvp6114a_ch_to_ain(int id, int ch_idx);

int nvp6114a_audio_set_mode(int id, NVP6114A_VMODE_T mode, NVP6114A_SAMPLESIZE_T samplesize, NVP6114A_SAMPLERATE_T samplerate);
int nvp6114a_audio_set_mute(int id, int on);
int nvp6114a_audio_get_mute(int id);
int nvp6114a_audio_set_volume(int id, int volume);
int nvp6114a_audio_get_output_ch(int id);
int nvp6114a_audio_set_output_ch(int id, int ch);

int  nvp6114a_notify_vlos_register(int id, int (*nt_func)(int, int, int));
int  nvp6114a_notify_vfmt_register(int id, int (*nt_func)(int, int, struct nvp6114a_video_fmt_t *));
void nvp6114a_notify_vlos_deregister(int id);
void nvp6114a_notify_vfmt_deregister(int id);
#endif

#endif  /* _FE_AHD_NVP6114A_H_ */
