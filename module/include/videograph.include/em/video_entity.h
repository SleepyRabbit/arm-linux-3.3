/**
 * @file video_entity.h (wrapper)
 *  EM video entity related header (wrapper V1)
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/08/09 02:55:26 $
 *
 * ChangeLog:
 *  $Log: video_entity.h,v $
 *  Revision 1.2  2016/08/09 02:55:26  ivan
 *  update for v1 wrapper
 *
 */
/* Header file for Video Entity Driver */
#ifndef _VIDEO_ENTITY_H_
#define _VIDEO_ENTITY_H_

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/rbtree.h>

#include "common.h"
#include "job.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Property Definition
///////////////////////////////////////////////////////////////////////////////////////////////////
enum ve_wellknown_property_id {
    ID_NULL                 =0,

    /* source ID */
    ID_SRC_FMT              =1,     //image format, refer to "enum buf_type_t"
    ID_SRC_XY               =2,     //construction of (x << 16 | y)
    ID_SRC_BG_DIM           =3,     //construction of (width << 16 | height)
    ID_SRC_DIM              =4,     //construction of (width << 16 | height)
                                    //0 or "not defined" means use same value of ID_SRC_BG_DIM
    ID_SRC_INTERLACE        =5,     //indicate source is interlace or progressive frame
                                    //0 or "not defined" means progressive frame
    ID_SRC_TYPE             =6,     //0: Generic 1: ISP
    ID_ORG_DIM              =7,     //cap org input dim

    /* destionation ID */
    ID_DST_FMT              =11,    //image format, refer to "enum buf_type_t"
    ID_DST_XY               =12,    //construction of (x<<16|y)
    ID_DST_BG_DIM           =13,    //construction of (width<<16|height)
    ID_DST_DIM              =14,    //construction of (width<<16|height)
                                    //0 means use same value of ID_DST_BG_DIM

    /* CAP and SCL */
    ID_DST_CROP_XY          =15,    //construction of (x << 16 | y)
    ID_DST_CROP_DIM         =16,    //construction of (width << 16 | height)
    /* win ID */
    ID_WIN_XY               =17,    //construction of (x << 16 | y)
    ID_WIN_DIM              =18,    //construction of (width << 16 | height)
                                    //0 means use same value of ID_DST_BG_DIM

    /* frame rate */
    ID_FPS_RATIO            =20,    //construction of frame rate (denomerator<<16|numerator)
    ID_TARGET_FRAME_RATE    =21,    //target frame rate value
    ID_SRC_FRAME_RATE       =22,    //source frame rate value
    ID_RUNTIME_FRAME_RATE   =23,    //calculated real frame rate value
    ID_FAST_FORWARD         =24,    //skip frame encoding for decode
    ID_REF_FRAME            =25,    //the frame is reference or not, by return of encode

    /* crop re-calculate */
    ID_CROP_XY              =26,    //for crop re-calculate, em internal-use only
    ID_CROP_DIM             =27,    //for crop re-calculate, em internal-use only
    ID_MAX_DIM              =28,    //for crop re-calculate, em internal-use only
    ID_IS_CROP_ENABLE       =29,    //for crop re-calculate, em internal-use only

    /* functional input & result  */
    ID_DI_RESULT            =30,    //Indicate Deinterlaced function result
                                    //0:Nothing 1:Deinterlace 2:Copy line 3:denoise 4:Copy Frame
    ID_SUB_YUV              =31,    //Indicate if sub ratio YUV exist
                                    //0:Not Exist 1: Exist
    ID_SCL_FEATURE          =32,    //Indicate Scaler Function
                                    //0:Nothing 1:Scaling 2:Progress corection
    ID_SCL_RESULT           =33,    //0:Nothing 1:choose 1st src yuv  2:choose 2nd src yuv (RATIO_FRAME)
    ID_DVR_FEATURE          =34,    //0:liveview 1:recording 
    ID_GRAPH_RATIO          =35,    //graph ratio for internal usage
    ID_IDENTIFIER           =36,    //identify who i am when other parameters can't do it
                                    // (used for audio dataout)
    ID_CHANGE_SRC_INTERLACE =37,    //tell 3di change ID_SRC_INTERLACE = 0, after finish 3DI
    ID_DIDN_MODE            =38,    //deInterlace and denoise 
    ID_CLOCKWISE            =39,
    
    /* bitstream */
    ID_BITSTREAM_SIZE       =40,    //bitstream size
    ID_BITRATE              =41,    //bitrate value
    ID_IDR_INTERVAL         =43,    //IP interval
    ID_B_FRM_NUM            =44,    //B frame number
    ID_SLICE_TYPE           =45,    //0:P frame  1:B frame   2:I frame   5:Audio
    ID_QUANT                =46,    //quant value
    ID_INIT_QUANT           =47,    //initial quant value
    ID_MAX_QUANT            =48,    //max quant value
    ID_MIN_QUANT            =49,    //min quant value
    
    ID_MAX_RECON_DIM        =50,    //max recon dimentation, construction of (x << 16|y)
    ID_MAX_REFER_DIM        =51,    //max refer dimentation, construction of (x << 16|y)
    ID_MOTION_ENABLE        =52,    // motion detection enable
    ID_BITSTREAM_OFFSET     =53,    //It's the offset of motion_vector for getting the position of real bit-stream.
    ID_CHECKSUM             =54,    //checksum type

    /* aspect ratio & border */
    ID_AUTO_ASPECT_RATIO    =55,    //construction of (palette_idx << 16 | enable)
    ID_AUTO_BORDER          =56,    //construction of (palette_idx << 16 | border_width << 8 | enable)
    /* aspect ratio & border can pass to scaler*/
    ID_AUTO_ASPECT_RATIO_SCL    =57,    //construction of (palette_idx << 16 | enable)
    ID_AUTO_BORDER_SCL          =58,    //construction of (palette_idx << 16 | border_width << 8 | enable)
    
    ID_AU_SAMPLE_RATE       =60,    //audio sample rate
    ID_AU_CHANNEL_TYPE      =61,    //audio channel type
    ID_AU_ENC_TYPE          =62,    //audio encode type
    ID_AU_RESAMPLE_RATIO    =63,    //audio resample ratio
    ID_AU_SAMPLE_SIZE       =64,    //audio sample size
    ID_AU_DATA_LENGTH_0     =65,    //audio data length of encode type 0
    ID_AU_DATA_LENGTH_1     =66,    //audio data length of encode type 1
    ID_AU_DATA_OFFSET_0     =67,    //audio data offset of encode type 0
    ID_AU_DATA_OFFSET_1     =68,    //audio data offset of encode type 1
    ID_AU_CHANNEL_BMP       =69,    //bitmap of active audio channel
    ID_AU_BLOCK_SIZE        =70,    //audio block size
    ID_AU_BLOCK_COUNT       =71,    //audio block count

    ID_SRC2_XY              =72,    //construction of (x << 16 | y)
    ID_SRC2_DIM             =73,    //construction of (width << 16 | height)
    ID_DST2_CROP_XY         =74,    //construction of (x << 16 | y)
    ID_DST2_CROP_DIM        =75,    //construction of (width << 16 | height)    
    ID_DST2_BG_DIM          =76,    //construction of (width<<16|height)
    ID_SRC2_BG_DIM          =77,    //construction of (width<<16|height)
    ID_DST2_XY              =78,    //construction of (x<<16|y)
    ID_DST2_DIM             =79,    //construction of (width<<16|height)
    ID_DST2_FD              =80,    //special for dst2_bg_dim usage

    /* win ID */
    ID_WIN2_BG_DIM          =81,
    ID_WIN2_XY              =82,    //construction of (x << 16 | y)
    ID_WIN2_DIM             =83,    //construction of (width << 16 | height)
    ID_CAS_SCL_RATIO        =84,    //cas_scl_ratio

    /* DEC PB */
    ID_DEC_SPEED            =85,    //PB speed
    ID_DEC_DIRECT           =86,    //PB forward/backward
    ID_DEC_GOP              =87,    //PB GOP
    ID_DEC_OUT_GROUP_SIZE   =88,    //PB output group size

    /* Audio properties (part-II)*/
    ID_AU_FRAME_SAMPLES     =90,    //audio frame samples

    /* CVBS offset */
    ID_DST2_BUF_OFFSET      =91,    //CVBS offset for continued buffer

    /* Multi slice offset 1~3 (first offset is 0)*/
    ID_SLICE_OFFSET1        =92,
    ID_SLICE_OFFSET2        =93,
    ID_SLICE_OFFSET3        =94,

    /* Speicial prpoerty */
    ID_BUF_CLEAN            =95,
    ID_VG_SERIAL            =96,
    ID_SPEICIAL_FUNC        =97,  //bit0: 1FRAME_60I_FUNC
    ID_SOURCE_FIELD         =98,  // 0: top field, 1: bottom field

    ID_WELL_KNOWN_MAX = MAX_WELL_KNOWN_ID_NUM,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Property Opertion
///////////////////////////////////////////////////////////////////////////////////////////////////
/* ID_DIDN_MODE property */
//[3:0]denoise value
#define DN_OFF              0
#define DN_TEMPORAL         1
#define DN_SPATIAL          2
#define DN_TEMPORAL_SPATIAL 3
#define GET_DN_VALUE(didn_prop)    ((didn_prop) & 0x0F)

//[7:4]deinterlace value
#define DI_OFF              0
#define DI_TEMPORAL         1
#define DI_SPATIAL          2
#define DI_TEMPORAL_SPATIAL 3
#define GET_DI_VALUE(didn_prop)    (((didn_prop) & 0xF0) >> 4)

//[11:8]sharpness value
#define SHARPNESS_OFF       0
#define SHARPNESS_ON        1
#define GET_SHARPNESS_VALUE(didn_prop)    (((didn_prop) & 0xF00) >> 8)

#define FORM_DIDN_PROP_VALUE(di_val, dn_val, shrp_val)  ((shrp_val) << 8 | (di_val) << 4 | (dn_val))
#define FORM_IMAGE_QUALITY_PROP_VALUE(sharpness_val, tmnr_val, spnr_val) \
        ((sharpness_val) << 16 | (tmnr_val) << 8 | (spnr_val))

/* ID_FLIP_ROTATE property */
//[3:0]rotate value
#define ROTATE_OFF          0
#define ROTATE_LEFT         1
#define ROTATE_RIGHT        2
#define ROTATE_180          3
#define GET_ROTATE_VALUE(flip_rotate_prop)     ((flip_rotate_prop) & 0x0F)

//[7:4]flip value
#define FLIP_OFF                     0
#define FLIP_VERTICAL                1
#define FLIP_HORIZONTAL              2
#define FLIP_VERTICAL_AND_HORIZONTAL 3
#define GET_FLIP_VALUE(flip_rotate_prop)       (((flip_rotate_prop) & 0xF0) >> 4)

#define FORM_FLIP_ROTATE_PROP_VALUE(flip_val, rotate_val)    ((flip_val) << 4 | (rotate_val))


/* ID_VPE_FEATURE property */
//[7:0]draw sequence
#define VPE_BACKGROUND      0
#define VPE_PIP_LAYER       1
#define GET_DRAW_SEQ_VALUE(vpe_feature_prop)     ((vpe_feature_prop) & 0xFF)

//[11:8]aspect ratio
#define ASPECT_RATIO_OFF    0
#define ASPECT_RATIO_ON     1
#define GET_ASPECT_RATIO_VALUE(vpe_feature_prop) (((vpe_feature_prop) & 0xF00) >> 8)

//[19:12]palette index
#define GET_PALETTE_VALUE(vpe_feature_prop)      (((vpe_feature_prop) & 0xFF000) >> 12)

//[23:20]predict enable
#define PREDICT_OFF         0
#define PREDICT_ON          1

#define FORM_VPE_FEATURE_PROP_VALUE(draw_seq, aspect_ratio, palette, predict)    \
        ((draw_seq) | ((aspect_ratio) << 8) | (((palette) & 0xFF) << 12) | (predict << 20))


/* ID_CAP_FEATURE property */
//[3:0]cap method
#define CAP_FRAME           0
#define CAP_2FRAME          1
#define CAP_FIELD           2
#define CAP_2FIELD          3
#define GET_CAP_METHOD_VALUE(cap_feature_prop)          ((cap_feature_prop) & 0xF)

//[7:4]auto aspect
#define GET_CAP_AUTO_ASPECT_VALUE(cap_feature_prop)     (((cap_feature_prop) & 0xF0) >> 4)

//[11:8]dma
#define GET_CAP_DMA_VALUE(cap_feature_prop)             (((cap_feature_prop) & 0xF00) >> 8)

//[15:12]reserved

//[19:16]reserved

//[23:20]extend control
#define EXT_CTRL_DISABLE    0
#define EXT_CTRL_MANUAL     1
#define EXT_CTRL_AUTO       2
#define GET_EXT_CONTROL_VALUE(cap_feature_prop)         (((cap_feature_prop) & 0xF00000) >> 20)

//[27:24]extend auto region
#define GET_EXT_AUTO_REGION_VALUE(cap_feature_prop)     (((cap_feature_prop) & 0xF000000) >> 24)
#define IS_EXT_LEFT(auto_region_value)                  ((auto_region_value) & 0x1)
#define IS_EXT_RIGHT(auto_region_value)                 ((auto_region_value) & 0x2)
#define IS_EXT_TOP(auto_region_value)                   ((auto_region_value) & 0x4)
#define IS_EXT_BOTTOM(auto_region_value)                ((auto_region_value) & 0x8)
#define FORM_AUTO_REGION_VALUE(is_left, is_right, is_top, is_bottom)  \
            ((!(!is_left)) | ((!(!is_right)) << 1) | ((!(!is_top)) << 2) | ((!(!is_bottom)) << 3))

//[31:28]extend pallete color
#define GET_EXT_PALLETE_COLOR_VALUE(cap_feature_prop)      (((cap_feature_prop) & 0xF0000000) >> 28)

#define FORM_CAP_FEATURE_PROP_VALUE(cap_method, auto_aspect, dma, \
                                    ext_ctrl, ext_auto_region, ext_pallete_idx)   \
      ((cap_method) | ((auto_aspect) << 4) | ((dma) << 8) |  \
      ((ext_ctrl) << 20) | ((ext_auto_region) << 24) | ((ext_pallete_idx) << 28))

/* ID_CAP_EXTEND_H property */
//[15: 0]extend right pixels
//[31:16]extend left pixels
#define GET_CAP_EXTEND_RIGHT_VALUE(cap_extend_h)    ((cap_extend_h) & 0xFFFF)
#define GET_CAP_EXTEND_LEFT_VALUE(cap_extend_h)     (((cap_extend_h) & 0xFFFF0000) >> 16)
#define FORM_CAP_EXTEND_H(right_pixel, left_pixel)      ((right_pixel) | (left_pixel) << 16)

/* ID_CAP_EXTEND_V property */
//[15: 0]extend top lines
//[31:16]extend bottom lines
#define GET_CAP_EXTEND_TOP_VALUE(cap_extend_v)      ((cap_extend_v) & 0xFFFF)
#define GET_CAP_EXTEND_BOTTOM_VALUE(cap_extend_v)   (((cap_extend_v) & 0xFFFF0000) >> 16)
#define FORM_CAP_EXTEND_V(top_line, bottom_line)    ((top_line) | (bottom_line) << 16)

/* ID_ENC_FEATURE property */
//[3:0]mv data
#define ENC_MV_DATA_OFF         0
#define ENC_MV_DATA_ON          1
#define GET_ENC_MV_DATA_VALUE(enc_feature_prop)         ((enc_feature_prop) & 0xF)

//[7:4]field coding
#define ENC_FIELD_CODING_OFF    0
#define ENC_FIELD_CODING_ON     1
#define GET_ENC_FIELD_CODING_VALUE(enc_feature_prop)    (((enc_feature_prop) & 0xF0) >> 4)

//[11:8]gray coding
#define ENC_GRAY_CODING_OFF     0
#define ENC_GRAY_CODING_ON      1
#define GET_ENC_GRAY_CODING_VALUE(enc_feature_prop)     (((enc_feature_prop) & 0xF00) >> 8)

//[15:12]slice number
#define GET_ENC_SLICE_NUMBER_VALUE(enc_feature_prop)     (((enc_feature_prop) & 0xF000) >> 12)

#define FORM_ENC_FEATURE_PROP_VALUE(mv_data, field_coding, gray_coding, slice_number)   \
        ((mv_data) | ((field_coding) << 4) | ((gray_coding) << 8) | ((slice_number) << 12))

/* ID_QUANT_VALUE property */
//[ 7: 0]init quant
//[15: 8]min quant
//[23:16]max quant
#define GET_INIT_QUANT_VALUE(quant_value_prop)    ((quant_value_prop) & 0xFF)
#define GET_MIN_QUANT_VALUE(quant_value_prop)     (((quant_value_prop) & 0xFF00) >> 8)
#define GET_MAX_QUANT_VALUE(quant_value_prop)     (((quant_value_prop) & 0xFF0000) >> 16)
#define FORM_QUANT_VALUE_PROP_VALUE(init_quant, min_quant, max_quant)      \
        ((init_quant) | ((min_quant) << 8) | ((max_quant) << 16))


/* ID_AU_ENC_TYPE property */
#define GET_AU_ENC_TYPE_VALUE(au_enc_type_prop)     ((au_enc_type_prop) & 0xFFFF)
#define FORM_AU_ENC_TYPE_PROP_VALUE(au_enc_type, vch)    ((vch) << 16 | (au_enc_type))




///////////////////////////////////////////////////////////////////////////////////////////////////
// Entity FD former
///////////////////////////////////////////////////////////////////////////////////////////////////
#define ENTITY_FD_IDX(fd, engines, minors) \
    ((ENTITY_FD_CHIP(fd) * engines * minors) + (ENTITY_FD_ENGINE(fd) * minors) + ENTITY_FD_MINOR(fd))
#define ENTITY_IDX(chip, engine, minor, engines, minors)   \
                                            ((chip * engines * minors) + (engine * minors) + minor)
#define ENTITY_IDX_CHIP(idx, engines, minors)        (idx / (engines * minors))
#define ENTITY_IDX_ENGINE(idx, engines, minors)      ((idx % (engines * minors)) / minors)
#define ENTITY_IDX_MINOR(idx, engines, minors)       ((idx % (engines * minors)) % minors)


///////////////////////////////////////////////////////////////////////////////////////////////////
// Other stuffs
///////////////////////////////////////////////////////////////////////////////////////////////////

/* video entity */
struct video_entity_ops_t {
    int (*putjob)(void *);  //struct video_job_t *
    int (*stop)(unsigned int);  //entity fd
    int (*queryid)(void *, char *);  //NULL, property string
    int (*querystr)(void *, int, char *); //struct video_entity_t *,id, property string
    int (*getproperty)(void *, int, int, char *);  //NULL,engine,minor,string
    int (*register_clock)(void *);
    //< NULL, func(ms_ticks,is_update,private), private
    int reserved1[4];
};

//for buffer allocate by entity
enum buff_purpose {
    //for decode/encode
    VG_BUFF_DECODE,
    VG_BUFF_ENCODE,
    VG_BUFF_AU_DECODE,
    VG_BUFF_AU_ENCODE
};

#define MAX_NAME_LEN        32
#define MAX_CHANNELS        256

struct video_entity_t {
    /* fill by Video Entity */
    enum entity_type_t          type;       //TYPE_H264E,TYPE_MPEG4E...
    char                        name[MAX_NAME_LEN];

#define MAX_CHIPS       4
    int                         chips;      //chips per device
#define MAX_ENGINES     16
    int                         engines;    //number of device
#define MAX_MINORS      128
    int                         minors;     //channels per device

    struct video_entity_ops_t   *ops;

    /* internal use */
    char                        ch_used[MAX_CHANNELS];
    void                        *func;
    struct list_head            entity_list;
    struct rb_root              root_node;
    int                         reserved2[3];
};

/* video entity */
void video_entity_init(void);
void video_entity_close(void);
int video_entity_register(struct video_entity_t *entity);
int video_entity_deregister(struct video_entity_t *entity);
int video_preprocess(struct video_entity_job_t *ongoing_job, void *param);
int video_postprocess(struct video_entity_job_t *finish_job, void *param);

void *video_reserve_buffer(struct video_entity_job_t *job, int dir);
int video_free_buffer(void *rev_handle);
int video_alloc_buffer(struct video_entity_job_t *job, int dir, struct video_bufinfo_t *buf);
void *video_alloc_buffer_simple(struct video_entity_job_t *job, int size, int parameters,
                                void *phy, int *ddr_no);
void video_free_buffer_simple(void *buffer);
int video_sync_tuning_calculation(void *job, int *src_frame_rate_change);
int video_process_tick(void *clk, unsigned int last_process_jiffies, 
                       struct video_entity_job_t *last_process_job);
int video_entity_notify(struct video_entity_t *entity, int fd, enum notify_type type, int count);
int video_scaling(enum buf_type_t format, unsigned int src_phys_addr, unsigned int src_width,
                  unsigned int src_height, unsigned int dst_phys_addr, unsigned int dst_width,
                  unsigned int dst_height);
int video_scaling2(enum buf_type_t format, unsigned int src_phys_addr, unsigned int src_width,
                  unsigned int src_height,unsigned int src_bg_width, unsigned int src_bg_height,
                  unsigned int dst_phys_addr, unsigned int dst_width, unsigned int dst_height,
                  unsigned int dst_bg_width, unsigned int dst_bg_height);
#endif

