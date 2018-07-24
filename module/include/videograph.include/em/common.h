/**
 * @file common.h (wrapper)
 *  EM common file header (wrapper V1)
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/08/09 02:55:26 $
 *
 * ChangeLog:
 *  $Log: common.h,v $
 *  Revision 1.2  2016/08/09 02:55:26  ivan
 *  update for v1 wrapper
 *
 */
#ifndef _EM_COMMON_H_
#define _EM_COMMON_H_
/*
Bitmap:
|---type(8)----+--reserved(4)-+--chip(4)--+----engine(8)----+---minor(8)---|
*/
#define EM_TYPE_OFFSET      24
#define EM_TYPE_BITMAP      0xFF000000
#define EM_TYPE_MAX_VALUE   (EM_TYPE_BITMAP >> EM_TYPE_OFFSET)

#define EM_CHIP_OFFSET      16
#define EM_CHIP_BITMAP      0x000F0000
#define EM_CHIP_MAX_VALUE   (EM_CHIP_BITMAP >> EM_CHIP_OFFSET)

#define EM_ENGINE_OFFSET    8
#define EM_ENGINE_BITMAP    0x0000FF00
#define EM_ENGINE_MAX_VALUE (EM_ENGINE_BITMAP >> EM_ENGINE_OFFSET)

#define EM_MINOR_OFFSET     0
#define EM_MINOR_BITMAP     0x000000FF
#define EM_MINOR_MAX_VALUE  (EM_MINOR_BITMAP >> EM_MINOR_OFFSET)

#define ENTITY_FD(type, chip, engine, minor) \
    ((type << EM_TYPE_OFFSET) | (chip << EM_CHIP_OFFSET) | (engine << EM_ENGINE_OFFSET) | minor)

#define ENTITY_FD_TYPE(fd)      ((fd & EM_TYPE_BITMAP) >> EM_TYPE_OFFSET)
#define ENTITY_FD_CHIP(fd)      ((fd & EM_CHIP_BITMAP) >> EM_CHIP_OFFSET)
#define ENTITY_FD_ENGINE(fd)    ((fd & EM_ENGINE_BITMAP) >> EM_ENGINE_OFFSET)
#define ENTITY_FD_MINOR(fd)     ((fd & EM_MINOR_BITMAP) >> EM_MINOR_OFFSET)

#define MAX_WELL_KNOWN_ID_NUM   100
#define MAX_PROPERTYS           368                 //32CH
#define MAX_PROCESS_PROPERTYS   10                  //max of preprocess/postprocess numbers

#define MAX_HUGE_PROPERTIES         512 /* job_in/buf_out */
#define MAX_SMALL_PROPERTIES        33  /* user_in */
#define MAX_TINY_PROPERTIES         17  /* job_in/buf_out/job_out/process_in */

#define MAX_PROCESS_PROPERTIES      MAX_TINY_PROPERTIES     //max of preprocess/postprocess numbers
#define MAX_USER_PROPERTIES         MAX_SMALL_PROPERTIES

#define EM_MAX_BUF_COUNT            2

struct video_property_t {
    unsigned int    pch:8;
    unsigned int    id:24;
    unsigned int    value;
};

struct video_properties_t {
    unsigned int max_p_num;
    struct video_property_t *p;
};

#define MAKE_IDX(minors, engine, minor)     ((engine * minors) + minor)
#define IDX_ENGINE(idx, minors)             (idx / minors)
#define IDX_MINOR(idx, minors)              (idx % minors)

/* defination of property value of src_xy, dst_xy... */
#define EM_PARAM_X(value)           (((unsigned int)(value) >> 16) & 0x0000FFFF)
#define EM_PARAM_Y(value)           (((unsigned int)(value)) & 0x0000FFFF)
#define EM_PARAM_XY(x, y)           (((((unsigned int)x) << 16) & 0xFFFF0000) | \
                                    (((unsigned int)y) & 0x0000FFFF))

/* defination of property value of src_dim, dst_dim... */
#define EM_PARAM_WIDTH(value)       EM_PARAM_X(value)
#define EM_PARAM_HEIGHT(value)      EM_PARAM_Y(value)
#define EM_PARAM_DIM(width, height) EM_PARAM_XY(width, height)

/* defination of property value of fps_ratio = M/N */
#define EM_PARAM_M(value)           EM_PARAM_X(value)
#define EM_PARAM_N(value)           EM_PARAM_Y(value)
#define EM_PARAM_RATIO(m, n)        EM_PARAM_XY(m, n)

enum mm_type_t {
    TYPE_VAR_MEM = 0x2001,
    TYPE_FIX_MEM,
};

enum buf_type_t {
    TYPE_YUV420_PACKED = 0x101,
    TYPE_YUV420_FRAME = 0x102,
    TYPE_YUV422_FIELDS = 0x103,         //YUV generated only from TOP field
    TYPE_YUV422_FRAME = 0x104,          //YUV generated from odd and even interlaced
    TYPE_YUV422_RATIO_FRAME = 0x105,    //YUV for TYPE_YUV422_FRAME + (TYPE_YUV422_FRAME / ratio)
    TYPE_YUV422_2FRAMES = 0x106,        //YUV for TYPE_YUV422_FRAME x2
    TYPE_YUV400_FRAME = 0x107,          // frame: monochrome
    TYPE_YUV422_2FRAMES_2FRAMES = 0x108,  //YUV for TYPE_YUV422_FRAME x2 + TYPE_YUV422_FRAME x 2
    TYPE_YUV422_FRAME_2FRAMES = 0x109,    //YUV for TYPE_YUV422_FRAME + TYPE_YUV422_FRAME x 2
    TYPE_YUV422_FRAME_FRAME = 0x10A,    //YUV for TYPE_YUV422_FRAME + TYPE_YUV422_FRAME

    TYPE_YUV422_H264_2D_FRAME = 0x201,
    TYPE_YUV422_MPEG4_2D_FRAME,

    TYPE_DECODE_BITSTREAM = 0x300, //H264, MPEG, JPEG bitstream
    TYPE_BITSTREAM_H264 = 0x301,
    TYPE_BITSTREAM_MPEG4 = 0x302,
    TYPE_BITSTREAM_JPEG = 0x303,

    TYPE_AUDIO_BITSTREAM = 0x401,

    TYPE_RGB565 = 0x501,
    TYPE_YUV422_CASCADE_SCALING_FRAME = 0x502,
    TYPE_RGB555 = 0x503,
};

enum mm_vrc_mode_t {
    EM_VRC_CBR = 1,
    EM_VRC_VBR,
    EM_VRC_ECBR,
    EM_VRC_EVBR
};

enum slice_type_t {
    EM_SLICE_TYPE_P = 0,
    EM_SLICE_TYPE_I = 2,
    EM_SLICE_TYPE_B = 1,
    EM_SLICE_TYPE_AUDIO = 5,
    EM_SLICE_TYPE_RAW = 6,
};

/* the bitmap definition for audio capability */
#define EM_AU_CAPABILITY_SAMPLE_RATE_8K          (1 << 0)    /*  8000 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_11K         (1 << 1)    /* 11025 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_16K         (1 << 2)    /* 16000 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_22K         (1 << 3)    /* 22050 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_32K         (1 << 4)    /* 32000 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_44K         (1 << 5)    /* 44100 Hz */
#define EM_AU_CAPABILITY_SAMPLE_RATE_48K         (1 << 6)    /* 48000 Hz */

#define EM_AU_CAPABILITY_SAMPLE_SIZE_8BIT        (1 << 0)
#define EM_AU_CAPABILITY_SAMPLE_SIZE_16BIT       (1 << 1)

#define EM_AU_CAPABILITY_CHANNELS_1CH            (1 << 0)    /* mono   */
#define EM_AU_CAPABILITY_CHANNELS_2CH            (1 << 1)    /* stereo */

#define EM_AU_CAPABILITY_TYPE_PCM                (1 << 0)
#define EM_AU_CAPABILITY_TYPE_AAC                (1 << 1)
#define EM_AU_CAPABILITY_TYPE_ADPCM              (1 << 2)
#define EM_AU_CAPABILITY_TYPE_G711_ALAW          (1 << 3)
#define EM_AU_CAPABILITY_TYPE_G711_ULAW          (1 << 4)

enum audio_channel_type_t {
    EM_AUDIO_MONO = 1,
    EM_AUDIO_STEREO = 2,
};

enum audio_encode_type_t {
    EM_AUDIO_NONE = 0,
    EM_AUDIO_PCM = 1,
    EM_AUDIO_AAC = 2,
    EM_AUDIO_ADPCM = 3,
    EM_AUDIO_G711_ALAW = 4,
    EM_AUDIO_G711_ULAW = 5
};

enum notify_type {
    VG_NO_NEW_JOB = 0,
    VG_HW_DELAYED,
    VG_FRAME_DROP,
    VG_SIGNAL_LOSS,
    VG_SIGNAL_PRESENT,
    VG_FRAMERATE_CHANGE,
    VG_HW_CONFIG_CHANGE,
    VG_TAMPER_ALARM,
    VG_TAMPER_ALARM_RELEASE,
    VG_AUDIO_BUFFER_UNDERRUN,
    VG_AUDIO_BUFFER_OVERRUN,
    VG_NOTIFY_TYPE_COUNT
};

enum checksum_type {
    EM_CHECKSUM_NONE = 0x0,
    EM_CHECKSUM_ALL_CRC = 0x101,
    EM_CHECKSUM_ALL_SUM = 0x0102,
    EM_CHECKSUM_ALL_4_BYTE = 0x103,
    EM_CHECKSUM_ONLY_I_CRC = 0x201,
    EM_CHECKSUM_ONLY_I_SUM = 0x0202,
    EM_CHECKSUM_ONLY_I_4_BYTE = 0x203
};

struct video_buf_t {
    unsigned int addr_va;
    unsigned int addr_pa;
    unsigned int ddr_id;    //indicate DDR virtual channel index 0,1,2,3...

    struct buf_properties_t {   //video_properties_t
        unsigned int max_p_num;
        void *p;
    } buf_prop; //used to record buffer property, same with video_properties_t
    int need_realloc_val;
    unsigned int size;
    unsigned short width;
    unsigned short height;
    unsigned int behavior_bitmap;
    unsigned int vari_win_size;      //For VG internal use only
    unsigned int buf_header_addr;
};

struct video_bufinfo_t {
    int                 mtype;  //memory type
    int                 btype;  //buffer type
    int                 count;  //buffer count provide by EM(get_binfo)-->GS-->EM(putjob)    
    struct video_buf_t buf[EM_MAX_BUF_COUNT];
    int fd;             //for reallocate fd
};


//buf_idx=> 0:buf[0], 1:buf[1]
//btype=>TYPE_YUV422_FRAME/TYPE_YUV420_FRAME
//mtype=>TYPE_FIX_MEM
//width=> type integer
//height=> type integer
struct video_buf_info_items_t {
    int buf_idx;
    int btype;
    int mtype;
    int width;
    int height;
};

enum entity_type_t {
    /*template*/
    TYPE_TEMPLATE = 0x0,
    /*capture*/
    TYPE_CAPTURE = 0x10,
    /*decode*/
    TYPE_DECODER = 0x20, //only have one decode entity for H264/MPEG4/JPEG
    /*encode*/
    TYPE_ENCODER = 0x30,
    TYPE_H264_ENC = 0x31,
    TYPE_MPEG4_ENC = 0x32,
    TYPE_JPEG_ENC = 0x33,
    /*vpe*/
    TYPE_VPE = 0x40,
    TYPE_OSG = 0x41,
    /*display*/
    TYPE_DISPLAY0 = 0x60,
    TYPE_DISPLAY1 = 0x61,
    TYPE_DISPLAY2 = 0x62,    
    /*datain/dataout/databox*/
    TYPE_DATAIN = 0x70,
    TYPE_DATAOUT = 0x71,
    /* audio */
    TYPE_AUDIO_DEC = 0x80,
    TYPE_AUDIO_ENC = 0x81,
    TYPE_AU_RENDER = 0x80,  // the alias of TYPE_AUDIO_DEC
    TYPE_AU_GRAB = 0x81,    // the alias of TYPE_AUDIO_ENC
    /* ivs */
    TYPE_IVS = 0x90,
	TYPE_3DNR = 0x91,
	TYPE_ROTATION = 0x91,
};


/* need sync with em/common.h  & gm_lib/src/pif.h  & ioctl/vplib.h  */
#define EM_PROPERTY_NULL_BYTE   0xFE
#define EM_PROPERTY_NULL_VALUE ((EM_PROPERTY_NULL_BYTE << 24) | (EM_PROPERTY_NULL_BYTE << 16) | \
                            (EM_PROPERTY_NULL_BYTE << 8) | (EM_PROPERTY_NULL_BYTE))
#endif

