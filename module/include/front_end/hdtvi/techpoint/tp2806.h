#ifndef _FE_TP2806_H_
#define _FE_TP2806_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  TP2806 Device Definition
 *************************************************************************************/
#define TP2806_DEV_MAX                 4
#define TP2806_DEV_CH_MAX              4

#define TP2806_DEV_ID                  4    ///< TP2806

typedef enum {
    TP2806_DEV_VOUT0 = 0,              ///< VD1
    TP2806_DEV_VOUT1,                  ///< VD2
    TP2806_DEV_VOUT2,                  ///< VD3
    TP2806_DEV_VOUT3,                  ///< VD4
    TP2806_DEV_VOUT_MAX
} TP2806_DEV_VOUT_T;

typedef enum {
    TP2806_VOUT_FORMAT_BT656 = 0,
    TP2806_VOUT_FORMAT_BT1120,
    TP2806_VOUT_FORMAT_MAX
} TP2806_VOUT_FORMAT_T;

typedef enum {
    TP2806_VOUT_MODE_1CH_BYPASS = 0,            ///< ch#0 rising edge
    TP2806_VOUT_MODE_2CH_DUAL_EDGE,             ///< ch#0 rising edge, ch#1 falling edge, only tp2806
    TP2806_VOUT_MODE_MAX
} TP2806_VOUT_MODE_T;

typedef enum {
    TP2806_VFMT_720P60 = 0,
    TP2806_VFMT_720P50,
    TP2806_VFMT_1080P30,  ///< Unsupported
    TP2806_VFMT_1080P25,  ///< Unsupported
    TP2806_VFMT_720P30,
    TP2806_VFMT_720P25,
    TP2806_VFMT_MAX
} TP2806_VFMT_T;

struct tp2806_video_fmt_t {
    TP2806_VFMT_T fmt_idx;       ///< video format index
    unsigned int  width;         ///< video source width
    unsigned int  height;        ///< video source height
    unsigned int  prog;          ///< 0:interlace 1:progressive
    unsigned int  frame_rate;    ///< currect frame rate
};

/*************************************************************************************
 *   TP2806 IOCTL
 *************************************************************************************/
struct tp2806_ioc_data_t {
    int vin_ch;                 ///< access tp2806 vin channel number
    int data;                   ///< read/write data value
};

struct tp2806_ioc_vfmt_t {
    int          vin_ch;        ///< tp2806 vin channel number
    unsigned int width;         ///< tp2806 channel video width
    unsigned int height;        ///< tp2806 channel video height
    unsigned int prog;          ///< 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< currect frame rate
};

#define  TP2806_IOC_MAGIC       't'

#define  TP2806_GET_NOVID       _IOR( TP2806_IOC_MAGIC,   1, struct tp2806_ioc_data_t)
#define  TP2806_GET_VIDEO_FMT   _IOR( TP2806_IOC_MAGIC,   2, struct tp2806_ioc_vfmt_t)

/*************************************************************************************
 *   TP2806 API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int  tp2806_get_dev_id(int id);
int  tp2806_get_revision(int id);
int  tp2806_get_dev_id(int id);
int  tp2806_get_device_num(void);
int  tp2806_get_vch_id(int id, TP2806_DEV_VOUT_T vout, int seq_id);
int  tp2806_get_vout_id(int id, int vin_idx);
int  tp2806_get_vout_seq_id(int id, int vin_idx);
int  tp2806_get_vout_format(int id);
int  tp2806_get_vout_mode(int id);
int  tp2806_vin_to_vch(int id, int vin_idx);

u8   tp2806_i2c_read(u8 id, u8 reg);
int  tp2806_i2c_read_ext(u8 id, u8 reg);
int  tp2806_i2c_write(u8 id, u8 reg, u8 data);
int  tp2806_i2c_array_write(u8 id, u8 addr, u8 *parray, u32 array_cnt);

int  tp2806_notify_vfmt_register(int id, int (*nt_func)(int, int, struct tp2806_video_fmt_t *));
void tp2806_notify_vfmt_deregister(int id);
int  tp2806_notify_vlos_register(int id, int (*nt_func)(int, int, int));
void tp2806_notify_vlos_deregister(int id);

int  tp2806_status_get_video_input_format(int id, int ch, struct tp2806_video_fmt_t *vfmt);
int  tp2806_status_get_video_loss(int id, int ch);

int  tp2806_video_init(int id, TP2806_VOUT_FORMAT_T vout_fmt, TP2806_VOUT_MODE_T vout_mode);
int  tp2806_video_get_video_output_format(int id, int ch, struct tp2806_video_fmt_t *vfmt);
int  tp2806_video_set_video_output_format(int id, int ch, TP2806_VFMT_T vfmt, TP2806_VOUT_FORMAT_T vout_fmt);

#endif

#endif  /* _FE_TP2806_H_ */
