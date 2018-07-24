/**
 * @file vpdrv_v1.h
 *  vpdrv header file (V1)
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.10 $
 * $Date: 2016/08/24 10:25:16 $
 *
 * ChangeLog:
 *  $Log: vpdrv_v1.h,v $
 *  Revision 1.10  2016/08/24 10:25:16  waynewei
 *  Support osg for GM8135.
 *
 *  Revision 1.9  2016/08/12 05:51:18  zhilong
 *  Add set capture tamper cmd
 *
 *  Revision 1.8  2016/08/12 02:09:56  klchu
 *  modify vp_osd to vp_osd_v1.
 *
 *  Revision 1.7  2016/08/12 01:54:28  klchu
 *  add OSD font & OSD mask for V1_IPCAM.
 *
 *  Revision 1.6  2016/08/11 14:27:03  waynewei
 *  Add set_palette_v1 for v2.
 *
 *  Revision 1.5  2016/08/11 06:28:06  ivan
 *  Rearrange v1 support command list
 *
 *  Revision 1.4  2016/08/10 11:10:24  klchu
 *  add capture flip function for V1_IP_CAM.
 *
 *  Revision 1.3  2016/08/10 10:53:27  ivan
 *  take off audio v1 command
 *
 *  Revision 1.2  2016/08/10 09:23:03  ivan
 *  udpate type error
 *
 *  Revision 1.1  2016/08/10 09:19:13  ivan
 *  update to add v1 ioctl
 *
 */

#ifndef __VPDRV_V1_H__
#define __VPDRV_V1_H__

#define VPD_IOC_MAGIC_V1  'V'

#define VPD_MAX_OSD_FONT_WINDOW                 8
#define VPD_MAX_OSD_MASK_WINDOW                 8
#define VPD_MAX_OSD_MARK_WINDOW                 4
#define VPD_MAX_OSD_PALETTE_IDX_NUM             16
#define VPD_MAX_OSD_MARK_IMG_NUM                4
#define VPD_MAX_OSD_MARK_IMG_SIZE               (2048*8)
#define VPD_MAX_OSD_FONT_STRING_LEN             256
#define VPD_MAX_OSD_SUB_STREAM_SCALER           4   //need_porting, we don't have this hw_info, how to get
#define VPD_MAX_OSG_MARK_WINDOW                 64  // for each CH


#define VPD_MAX_OSD_MARK_IMG_SIZE               (2048*8)
#define VPD_MAX_OSG_MARK_WINDOW                 64  // for each CH

typedef struct vpdChannelFd {
    int lineidx; // mapping to line index of graph
    int id;     //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15) 
    int reserve[2]; // NOTE: vpdChannelFD total len = GM_FD_MAX_PRIVATE_DATA 
} vpdChannelFd_t;// NOTE: relative to gm_pollfd_t

/***************************** set palette *****************************/
#define VPD_MAX_OSD_PALETTE_IDX_NUM             16

typedef struct {
    int id;
    int palette_table[VPD_MAX_OSD_PALETTE_IDX_NUM];
}__attribute__((packed, aligned(8))) vpd_osd_palette_table_t;


/*****************************  cap motion detection (Multi) *****************************/
typedef struct {
    int     id;                 ///<id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int     cap_vch;            ///<user channel number
    int     motion_id;
    int     motion_value;
}__attribute__((packed, aligned(8))) vpd_cap_motion_t;

typedef struct vpd_get_copy_cap_md {
    vpdChannelFd_t channelfd;   
    char *md_buf;                                   ///< buffer point of cap_md, set by AP
    int md_buf_len;                                 ///< length of md_buf, for check buf length whether it is enough. set the value by AP.
    int ch_num;                                     ///< capture physical channel number, set by pif from bindfd
    int is_valid;
    int md_len;                                     ///< real cap_md info size
    int md_dim_width;                               ///< MD Region width
    int md_dim_height;                              ///< MD Region height
    int md_mb_width;                                ///< MD Macro block width
    int md_mb_height;                               ///< MD Macro block height
    unsigned int md_buf_va;                         ///< MD buffer virtual addr
    int priv[2];                                    /* internal use */
}__attribute__((packed, aligned(8))) vpd_get_copy_cap_md_t;

#define VPD_MD_USER_PRIVATE_DATA   10
typedef struct vpdMultiCapMd {
    int user_private[VPD_MD_USER_PRIVATE_DATA];     ///< Library user data, don't use it!, 
                                                    ///< size need same with gm_multi_cap_md_t 
                                                    ///< + bindfd + retval
    vpd_get_copy_cap_md_t cap_md_vpd;
} vpd_multi_cap_md_t;

/*****************************  cap tamper  *****************************/
typedef struct {
    int     id;                 ///<id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int     cap_vch;            ///<user channel number
    unsigned short tamper_sensitive_b;
    unsigned short tamper_threshold;
    unsigned short tamper_sensitive_h;
}__attribute__((packed, aligned(8))) vpd_cap_tamper_t;

typedef struct {
    int     id;                 ///<id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int     cap_vch;            ///<user channel number
    int     motion_id;
    int     motion_value;
}__attribute__((packed, aligned(8))) vpdCapMotion_t;

typedef struct vpd_cap_md_info {
    unsigned int nr_cap_md;
    vpd_multi_cap_md_t *multi_cap_md;
} __attribute__((packed, aligned(8))) vpd_cap_md_info_t;

/*****************************  cap flip  *****************************/
typedef struct {
    int     id;                 ///<id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int     cap_vch;            ///<user channel number
    vpd_flip_t flip;
}__attribute__((packed, aligned(8))) vpd_cap_flip_t;

/*****************************  OSD/OSG  *****************************/
typedef enum vpdOsdMethod {
    VPD_OSD_METHOD_CAP,
    VPD_OSD_METHOD_SCL,
    VPD_OSD_METHOD_TOTAL_CNT, /* Last one, Don't remove it */
} vpdOsdMethod_t;

typedef enum {
    VPD_OSD_ALIGN_NONE = 0,
    VPD_OSD_ALIGN_TOP_LEFT,
    VPD_OSD_ALIGN_TOP_CENTER,
    VPD_OSD_ALIGN_TOP_RIGHT,
    VPD_OSD_ALIGN_BOTTOM_LEFT,
    VPD_OSD_ALIGN_BOTTOM_CENTER,
    VPD_OSD_ALIGN_BOTTOM_RIGHT,
    VPD_OSD_ALIGN_CENTER,
    VPD_OSD_ALIGN_MAX,
} vpdOsdAlignType_t;

typedef enum {
    VPD_OSD_FONT_ZOOM_NONE = 0,        ///< disable zoom
    VPD_OSD_FONT_ZOOM_2X,              ///< horizontal and vertical zoom in 2x
    VPD_OSD_FONT_ZOOM_3X,              ///< horizontal and vertical zoom in 3x
    VPD_OSD_FONT_ZOOM_4X,              ///< horizontal and vertical zoom in 4x
    VPD_OSD_FONT_ZOOM_ONE_HALF,        ///< horizontal and vertical zoom out 2x

    VPD_OSD_FONT_ZOOM_H2X_V1X = 8,     ///< horizontal zoom in 2x
    VPD_OSD_FONT_ZOOM_H4X_V1X,         ///< horizontal zoom in 4x
    VPD_OSD_FONT_ZOOM_H4X_V2X,         ///< horizontal zoom in 4x and vertical zoom in 2x

    VPD_OSD_FONT_ZOOM_H1X_V2X = 12,    ///< vertical zoom in 2x
    VPD_OSD_FONT_ZOOM_H1X_V4X,         ///< vertical zoom in 4x
    VPD_OSD_FONT_ZOOM_H2X_V4X,         ///< horizontal zoom in 2x and vertical zoom in 4x

    VPD_OSD_FONT_ZOOM_2X_WITH_EDGE = 20,    ///< horizontal and vertical zoom in 2x with edge mode (2-pixel)
    VPD_OSD_FONT_ZOOM_4X_WITH_EDGE,    ///< horizontal and vertical zoom in 4x with edge mode (2-pixel)

    VPD_OSD_FONT_ZOOM_MAX
} vpdOsdFontZoom_t;

typedef struct {
    int vch;                           ///< user channel
    int ch;                            ///< capture's vcapch, it is a physical channel.
    int path;
    int is_group_job;
    int winidx;
    vpdOsdMethod_t method;
    int fd; //fd would be got from vplib.c in ioctl layer.
} vpdOsdMg_input_t;

typedef enum {
    VPD_OSD_PRIORITY_MARK_ON_OSD = 0,   ///< Mark above OSD window
    VPD_OSD_PRIORITY_OSD_ON_MARK,       ///< Mark below OSD window
    VPD_OSD_PRIORITY_MAX
} vpdOsdPriority_t;

typedef enum {
    VPD_OSD_FONT_SMOOTH_LEVEL_WEAK = 0,     ///< weak smoothing effect
    VPD_OSD_FONT_SMOOTH_LEVEL_STRONG,       ///< strong smoothing effect
    VPD_OSD_FONT_SMOOTH_LEVEL_MAX
} vpdOsdSmoothLevel_t;

typedef struct {
    int                         onoff;  ///< OSD font smooth enable/disable 0 : enable, 1 : disable
    vpdOsdSmoothLevel_t         level;  ///< OSD font smooth level
} vpdOsdSmooth_t;

typedef enum {
    VPD_OSD_MARQUEE_MODE_NONE = 0,         ///< no marueee effect
    VPD_OSD_MARQUEE_MODE_HLINE,            ///< one horizontal line marquee effect
    VPD_OSD_MARQUEE_MODE_VLINE,            ///< one vertical line marquee effect
    VPD_OSD_MARQUEE_MODE_HFLIP,            ///< one horizontal line flip effect
    VPD_OSD_MARQUEE_MODE_MAX
} vpdOsdMarqueeMode_t;

typedef enum {
    VPD_OSD_MARQUEE_LENGTH_8192 = 0,       ///< 8192 step
    VPD_OSD_MARQUEE_LENGTH_4096,           ///< 4096 step
    VPD_OSD_MARQUEE_LENGTH_2048,           ///< 2048 step
    VPD_OSD_MARQUEE_LENGTH_1024,           ///< 1024 step
    VPD_OSD_MARQUEE_LENGTH_512,            ///< 512  step
    VPD_OSD_MARQUEE_LENGTH_256,            ///< 256  step
    VPD_OSD_MARQUEE_LENGTH_128,            ///< 128  step
    VPD_OSD_MARQUEE_LENGTH_64,             ///< 64   step
    VPD_OSD_MARQUEE_LENGTH_32,             ///< 32   step
    VPD_OSD_MARQUEE_LENGTH_16,             ///< 16   step
    VPD_OSD_MARQUEE_LENGTH_8,              ///< 8    step
    VPD_OSD_MARQUEE_LENGTH_4,              ///< 4    step
    VPD_OSD_MARQUEE_LENGTH_MAX
} vpdOsdMarqueeLength_t;

typedef enum {
    VPD_FONT_ALPHA_0 = 0,               ///< alpha 0%
    VPD_FONT_ALPHA_25,                  ///< alpha 25%
    VPD_FONT_ALPHA_37_5,                ///< alpha 37.5%
    VPD_FONT_ALPHA_50,                  ///< alpha 50%
    VPD_FONT_ALPHA_62_5,                ///< alpha 62.5%
    VPD_FONT_ALPHA_75,                  ///< alpha 75%
    VPD_FONT_ALPHA_87_5,                ///< alpha 87.5%
    VPD_FONT_ALPHA_100,                 ///< alpha 100%
    VPD_FONT_ALPHA_MAX
} vpdFontAlpha_t;

typedef struct {
    vpdOsdMarqueeMode_t          mode;
    vpdOsdMarqueeLength_t        length; ///< OSD marquee length control
    int                          speed;  ///< OSD marquee speed  control, 0~3, 0:fastest, 3:slowest
} vpdOsdMarqueeParam_t;

typedef enum {
    VPD_OSD_BORDER_TYPE_WIN = 0,    ///< treat border as background
    VPD_OSD_BORDER_TYPE_FONT,              ///< treat border as font
    VPD_OSD_BORDER_TYPE_MAX
} vpdOsdBorderType_t;

typedef struct {
    int                     enable;    ///1:enable,0:disable
    int                     width;     ///< OSD window border width, 0~7, border width = 4x(n+1) pixels
    vpdOsdBorderType_t      type;      ///< OSD window border transparency type
    int                     color;     ///< OSD window border color, palette index 0~15
} vpdOsdBorderParam_t;

typedef enum {
    VPD_MARK_DIM_16 = 2,       ///< 16  pixel/line
    VPD_MARK_DIM_32,           ///< 32  pixel/line
    VPD_MARK_DIM_64,           ///< 64  pixel/line
    VPD_MARK_DIM_128,          ///< 128 pixel/line
    VPD_MARK_DIM_256,          ///< 256 pixel/line
    VPD_MARK_DIM_512,          ///< 512 pixel/line
    VPD_MARK_DIM_MAX
} vpdMarkDim_t;

typedef struct {
    int  mark_exist;
    int  mark_yuv_buf_len;
    vpdMarkDim_t mark_width;
    vpdMarkDim_t mark_height;
    int  osg_tp_color;
    unsigned short osg_mark_idx;
} vpdOsdImgParam_t;

typedef struct {
    int  id;
    int  is_force_reset;
    int  ch_attr_vch;
    int  path;
    int  reserved[8];
}__attribute__((packed, aligned(8))) vpd_osg_mark_cmd_t;

typedef struct {
    int id;
    vpdOsdImgParam_t mark_img[VPD_MAX_OSD_MARK_IMG_NUM];
    char *yuv_buf_total;
    int yuv_buf_total_len;
}__attribute__((packed, aligned(8))) vpd_osd_mark_img_table_t;

typedef struct {
    int id;                     //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int linkIdx;
    vpdOsdMethod_t method;
    int cap_vch;                // (ch_num >=0) --> captureOsd, (ch_num =-1) --> scalarOsd.
    vpdOsdMg_input_t mg_input;  // osd management
    int win_idx;
    int enabled;
    vpdOsdAlignType_t align_type;
    unsigned int x;             // x_coordinate of OSD window
    unsigned int y;             // y_coordinate of OSD window
    unsigned int h_words;       // the horizontal number of words of OSD window
    unsigned int v_words;       // the vertical number of words of OSD window
    unsigned int h_space;       //The vertical space between charater and charater
    unsigned int v_space;       //The horizontal space between charater and charater

    unsigned int    font_index_len;    // the max number of words of  OSD window
    unsigned short  *font_index;      // string

    vpdFontAlpha_t font_alpha;
    vpdFontAlpha_t win_alpha;
    int win_palette_idx;
    int font_palette_idx;
    vpdOsdPriority_t priority;
    vpdOsdSmooth_t smooth;
    vpdOsdMarqueeParam_t marquee;
    vpdOsdBorderParam_t border;
    vpdOsdFontZoom_t font_zoom;
} __attribute__((packed, aligned(8))) vpd_osd_font_t;

#define VPD_OSD_FONT_MAX_ROW 18
typedef struct {
    int   id;
    int   font_idx;                                 ///< font index, 0 ~ (osd_char_max - 1)
    unsigned short  bitmap[VPD_OSD_FONT_MAX_ROW];   ///< GM8210 only 18 row (12 bits data + 4bits reserved)
}__attribute__((packed, aligned(8))) vpd_osd_font_update_t;

typedef enum {
    VPD_MASK_ALPHA_0 = 0,               ///< alpha 0%
    VPD_MASK_ALPHA_25,                  ///< alpha 25%
    VPD_MASK_ALPHA_37_5,                ///< alpha 37.5%
    VPD_MASK_ALPHA_50,                  ///< alpha 50%
    VPD_MASK_ALPHA_62_5,                ///< alpha 62.5%
    VPD_MASK_ALPHA_75,                  ///< alpha 75%
    VPD_MASK_ALPHA_87_5,                ///< alpha 87.5%
    VPD_MASK_ALPHA_100,                 ///< alpha 100%
    VPD_MASK_ALPHA_MAX
} vpdMaskAlpha_t;

typedef enum {
    VPD_MASK_BORDER_TYPE_HOLLOW = 0,
    VPD_MASK_BORDER_TYPE_TRUE
} vpdMaskBorderType_t;

typedef struct {
    int                      width;      ///< Mask window border width when hollow on, 0~15, border width = 2x(n+1) pixels
    vpdMaskBorderType_t      type;       ///< Mask window hollow/true
} vpdMaskBorder_t;

typedef struct {
    int id;                             //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    int linkIdx;
    vpdOsdMethod_t method;
    int cap_vch;                        // (ch_num >=0) --> captureOsd, (ch_num = obj) --> scalarOsd.
    vpdOsdMg_input_t mg_input;          // osd management
    int mask_idx;                       // the mask window index
    int enabled;
    unsigned int x;                     // left position of mask window
    unsigned int y;                     // top position of mask window
    unsigned int width;                 // the dimension width of mask window
    unsigned int height;                // the dimension height of mask window
    vpdMaskAlpha_t alpha;
    int palette_idx;                    // the mask color index that assign by FIOSDS_PALTCOLOR
    vpdMaskBorder_t border;
    vpdOsdAlignType_t align_type;
    int is_all_path;
}__attribute__((packed, aligned(8))) vpd_osd_mask_t;

typedef enum {
    VPD_MARK_ALPHA_0 = 0,               ///< alpha 0%
    VPD_MARK_ALPHA_25,                  ///< alpha 25%
    VPD_MARK_ALPHA_37_5,                ///< alpha 37.5%
    VPD_MARK_ALPHA_50,                  ///< alpha 50%
    VPD_MARK_ALPHA_62_5,                ///< alpha 62.5%
    VPD_MARK_ALPHA_75,                  ///< alpha 75%
    VPD_MARK_ALPHA_87_5,                ///< alpha 87.5%
    VPD_MARK_ALPHA_100,                 ///< alpha 100%
    VPD_MARK_ALPHA_MAX
} vpdMarkAlpha_t;

typedef enum {
    VPD_MARK_ZOOM_1X = 0,      ///< zoom in lx
    VPD_MARK_ZOOM_2X,          ///< zoom in 2X
    VPD_MARK_ZOOM_4X,          ///< zoom in 4x
    VPD_MARK_ZOOM_MAX
} vpdMarkZoom_t;

typedef struct {
    int fd;
    int id;                     //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15)
    vpdOsdMethod_t method;
    int attr_vch;                // capture_object: 0 ~ 127, file_object: 128 ~
    vpdOsdMg_input_t mg_input;  // osd management
    int mark_idx;               // the mask window index
    int enabled;
    unsigned int x;             // left position of mask window
    unsigned int y;             // top position of mask window
    vpdMarkAlpha_t alpha;
    vpdMarkZoom_t zoom;
    vpdOsdAlignType_t align_type;
    unsigned short osg_mark_idx;
}__attribute__((packed, aligned(8))) vpd_osd_mark_t;


#define VPD_SET_OSD_FONT_V1             _IOWR(VPD_IOC_MAGIC_V1, 230, vpd_osd_font_t) 
#define VPD_SET_OSD_NEW_FONT_V1         _IOWR(VPD_IOC_MAGIC_V1, 231, vpd_osd_font_update_t)
#define VPD_SET_OSD_MASK_V1             _IOWR(VPD_IOC_MAGIC_V1, 232, vpd_osd_mask_t)
#define VPD_SET_OSD_PALETTE_TABLE_V1    _IOWR(VPD_IOC_MAGIC_V1, 233, vpd_osd_palette_table_t)
#define VPD_SET_OSD_MARK_IMG_V1         _IOWR(VPD_IOC_MAGIC_V1, 234, vpd_osd_mark_img_table_t)
#define VPD_SET_OSG_MARK_IMG_V1         _IOWR(VPD_IOC_MAGIC_V1, 235, vpd_osd_mark_img_table_t)
#define VPD_SET_OSD_MARK_V1             _IOWR(VPD_IOC_MAGIC_V1, 236, vpd_osd_mark_t) 

#define VPD_SET_CAP_MOTION_V1           _IOWR(VPD_IOC_MAGIC_V1, 240, vpd_cap_motion_t)
#define VPD_GET_COPY_MULTI_CAP_MD_V1    _IOWR(VPD_IOC_MAGIC_V1, 241, vpd_cap_md_info_t) 

#define VPD_SET_CAP_FLIP_V1             _IOWR(VPD_IOC_MAGIC_V1, 250, vpd_cap_flip_t)
#define VPD_SET_CAP_TAMPER_V1           _IOWR(VPD_IOC_MAGIC_V1, 260, vpd_cap_tamper_t)

void vpd_init_v1(void);
void vpd_release_v1(void);

#endif
