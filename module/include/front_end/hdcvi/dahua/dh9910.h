#ifndef _FE_HDCVI_DH9910_H_
#define _FE_HDCVI_DH9910_H_

#include <linux/ioctl.h>

/*************************************************************************************
 *  DH9910 Device Definition
 *************************************************************************************/
#define DH9910_DEV_MAX                 4
#define DH9910_DEV_CH_MAX              4

typedef enum {
    DH9910_DEV_VOUT_0 = 0,
    DH9910_DEV_VOUT_1,
    DH9910_DEV_VOUT_2,
    DH9910_DEV_VOUT_3,
    DH9910_DEV_VOUT_MAX
} DH9910_DEV_VOUT_T;

typedef enum {
    DH9910_VOUT_FORMAT_BT656_DUAL_HEADER = 0,   ///< BT656 8BIT Dual Header
    DH9910_VOUT_FORMAT_BT1120,                  ///< BT1120 16BIT
    DH9910_VOUT_FORMAT_BT656,                   ///< BT656 8BIT Single Header
    DH9910_VOUT_FORMAT_MAX
} DH9910_VOUT_FORMAT_T;

typedef enum {
    DH9910_VOUT_MODE_1CH_BYPASS = 0,            ///< ch#0 rising edge
    DH9910_VOUT_MODE_2CH_DUAL_EDGE,             ///< ch#0 rising edge, ch#1 falling edge
    DH9910_VOUT_MODE_MAX
} DH9910_VOUT_MODE_T;

typedef enum {
    DH9910_AUDIO_SAMPLERATE_8K = 0,
    DH9910_AUDIO_SAMPLERATE_16K,
} DH9910_AUDIO_SAMPLERATE_T;

typedef enum {
    DH9910_AUDIO_SAMPLESIZE_8BITS = 0,
    DH9910_AUDIO_SAMPLESIZE_16BITS,
} DH9910_AUDIO_SAMPLESIZE_T;

typedef enum {
    DH9910_VFMT_720P25 = 0, ///< progressive, 25P
    DH9910_VFMT_720P30,     ///< progressive, 30P
    DH9910_VFMT_720P50,     ///< progressive, 50P
    DH9910_VFMT_720P60,     ///< progressive, 60P
    DH9910_VFMT_1080P25,    ///< progressive, 25P
    DH9910_VFMT_1080P30,    ///< progressive, 30P
    DH9910_VFMT_960H25,     ///< Interlace,   50I => 25P
    DH9910_VFMT_960H30,     ///< Interlace,   60I => 30P
    DH9910_VFMT_MAX
} DH9910_VFMT_T;

typedef enum {
    DH9910_PTZ_PTOTOCOL_DHSD1 = 0,
    DH9910_PTZ_PTOTOCOL_MAX
} DH9910_PTZ_PTOTOCOL_T;

typedef enum {
    DH9910_PTZ_BAUD_1200 = 0,
    DH9910_PTZ_BAUD_2400,
    DH9910_PTZ_BAUD_4800,
    DH9910_PTZ_BAUD_9600,
    DH9910_PTZ_BAUD_19200,
    DH9910_PTZ_BAUD_38400,
    DH9910_PTZ_BAUD_MAX
} DH9910_PTZ_BAUD_T;

typedef enum {
    DH9910_CABLE_TYPE_COAXIAL = 0,
    DH9910_CABLE_TYPE_UTP_10OHM,
    DH9910_CABLE_TYPE_UTP_17OHM,
    DH9910_CABLE_TYPE_UTP_25OHM,
    DH9910_CABLE_TYPE_UTP_35OHM,
    DH9910_CABLE_TYPE_MAX
} DH9910_CABLE_TYPE_T;

struct dh9910_video_fmt_t {
    DH9910_VFMT_T fmt_idx;       ///< video format index
    unsigned int  width;         ///< video source width
    unsigned int  height;        ///< video source height
    unsigned int  prog;          ///< 0:interlace 1:progressive
    unsigned int  frame_rate;    ///< currect frame rate
};

struct dh9910_ptz_t {
    DH9910_PTZ_PTOTOCOL_T   protocol;   ///< PTZ protocol, 0: DH_SD1
    DH9910_PTZ_BAUD_T       baud_rate;  ///< PTZ RS485 transmit baud rate, default:9600
    int                     parity_chk; ///< PTZ RS485 transmit parity check, 0:disable, 1:enable
};

/*************************************************************************************
 *  DH9910 COMM Definition
 *************************************************************************************/
typedef enum {
    DH9910_COMM_CMD_UNKNOWN = 0,
    DH9910_COMM_CMD_ACK_OK,
    DH9910_COMM_CMD_ACK_FAIL,
    DH9910_COMM_CMD_GET_VIDEO_STATUS,
    DH9910_COMM_CMD_SET_COLOR,
    DH9910_COMM_CMD_GET_COLOR,
    DH9910_COMM_CMD_CLEAR_EQ,
    DH9910_COMM_CMD_SEND_RS485,
    DH9910_COMM_CMD_SET_VIDEO_POS,
    DH9910_COMM_CMD_GET_VIDEO_POS,
    DH9910_COMM_CMD_SET_CABLE_TYPE,
    DH9910_COMM_CMD_SET_AUDIO_VOL,
    DH9910_COMM_CMD_GET_PTZ_CFG,
    DH9910_COMM_CMD_MAX
} DH9910_COMM_CMD_T;

#define DH9910_COMM_MSG_MAGIC           0x9910
#define DH9910_COMM_MSG_BUF_MAX         384

struct dh9910_comm_header_t {
    unsigned int magic;                 ///< DH9910_COMM_MAGIC
    unsigned int cmd_id;                ///< command index
    unsigned int data_len;              ///< data buffer length
    unsigned int dev_id;                ///< access device index
    unsigned int dev_ch;                ///< access device channel
    unsigned int reserved[2];           ///< reserve byte
};

struct dh9910_comm_send_rs485_t {
    unsigned char buf_len;
    unsigned char cmd_buf[256];
};

/*************************************************************************************
 *  DH9910 IOCTL
 *************************************************************************************/
struct dh9910_ioc_data_t {
    int cvi_ch;                 ///< access cvi channel number
    int data;                   ///< read/write data value
};

struct dh9910_ioc_vfmt_t {
    int          cvi_ch;        ///< cvi channel number
    unsigned int width;         ///< cvi channel video width
    unsigned int height;        ///< cvi channel video height
    unsigned int prog;          ///< cvi channel 0:interlace, 1: progressive
    unsigned int frame_rate;    ///< cvi channel frame rate
};

struct dh9910_ioc_vcolor_t {
    int           cvi_ch;       ///< cvi channel number
    unsigned char brightness;   ///< 0 ~ 100, default 50
    unsigned char contrast;     ///< 0 ~ 100, default 50
    unsigned char saturation;   ///< 0 ~ 100, default 50
    unsigned char hue;          ///< 0 ~ 100, default 50
    unsigned char gain;         ///< not support
    unsigned char white_balance;///< not support
    unsigned char sharpness;    ///< 0 ~ 15, default 1
    unsigned char reserved;
};

struct dh9910_ioc_vpos_t {
    int cvi_ch;                 ///< cvi channel number
    int h_offset;
    int v_offset;
    int reserved[2];
};

struct dh9910_ioc_cable_t {
    int cvi_ch;                 ///< cvi channel number
    DH9910_CABLE_TYPE_T cab_type;
};

struct dh9910_ioc_rs485_tx_t {
    int           cvi_ch;       ///< cvi channel number
    unsigned char buf_len;      ///< command buffer length
    unsigned char cmd_buf[256]; ///< command data
};

struct dh9910_ioc_audio_vol_t {
    int           cvi_ch;       ///< cvi channel number
    unsigned char volume;
};

#define DH9910_IOC_MAGIC            'd'

#define DH9910_GET_NOVID            _IOR(DH9910_IOC_MAGIC,   1, struct dh9910_ioc_data_t)
#define DH9910_GET_VIDEO_FMT        _IOR(DH9910_IOC_MAGIC,   2, struct dh9910_ioc_vfmt_t)

#define DH9910_GET_VIDEO_COLOR      _IOR(DH9910_IOC_MAGIC,   3, struct dh9910_ioc_vcolor_t)
#define DH9910_SET_VIDEO_COLOR      _IOWR(DH9910_IOC_MAGIC,  4, struct dh9910_ioc_vcolor_t)

#define DH9910_GET_VIDEO_POS        _IOR(DH9910_IOC_MAGIC,   5, struct dh9910_ioc_vpos_t)
#define DH9910_SET_VIDEO_POS        _IOWR(DH9910_IOC_MAGIC,  6, struct dh9910_ioc_vpos_t)

#define DH9910_SET_CABLE_TYPE       _IOWR(DH9910_IOC_MAGIC,  7, struct dh9910_ioc_cable_t)
#define DH9910_RS485_TX             _IOWR(DH9910_IOC_MAGIC,  8, struct dh9910_ioc_rs485_tx_t)
#define DH9910_CLEAR_EQ             _IOWR(DH9910_IOC_MAGIC,  9, int)    ///< parameter is cvi channel number

#define DH9910_SET_AUDIO_VOL        _IOWR(DH9910_IOC_MAGIC, 10, struct dh9910_ioc_audio_vol_t)

/*************************************************************************************
 *  DH9910 API Export Function Prototype
 *************************************************************************************/
#ifdef __KERNEL__
int  dh9910_get_device_num(void);
int  dh9910_get_vch_id_ext(int id, DH9910_DEV_VOUT_T vout, int cvi_ch);
int  dh9910_get_vout_id(int id, int vin_idx);
int  dh9910_get_vout_seq_id(int id, int vin_idx);
int  dh9910_get_vout_format(int id);
int  dh9910_get_vout_mode(int id);
int  dh9910_vin_to_vch(int id, int vin_idx);

int  dh9910_notify_vfmt_register(int id, int (*nt_func)(int, int, struct dh9910_video_fmt_t *));
void dh9910_notify_vfmt_deregister(int id);
int  dh9910_notify_vlos_register(int id, int (*nt_func)(int, int, int));
void dh9910_notify_vlos_deregister(int id);
#endif

#endif  /* _FE_HDCVI_DH9910_H_ */
