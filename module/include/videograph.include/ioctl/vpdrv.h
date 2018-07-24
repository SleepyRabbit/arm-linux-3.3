/**
* @file vpdrv.h
*  vpdrv header file
* Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
*
* $Revision: 1.35 $
* $Date: 2016/08/19 06:54:54 $
*
* ChangeLog:
*  $Log: vpdrv.h,v $
*  Revision 1.35  2016/08/19 06:54:54  schumy_c
*  Add dup_src_select in vpe feature property.
*  In 1080p_extend, use s/w duplicate for cvbs instead of h/w write-back.
*
*  Revision 1.34  2016/08/12 02:22:58  klchu
*  fix compile error for V2.
*
*  Revision 1.33  2016/08/12 02:09:56  klchu
*  modify vp_osd to vp_osd_v1.
*
*  Revision 1.32  2016/08/10 09:24:10  ivan
*  update to add v1 ioctl
*
*  Revision 1.31  2016/08/08 05:39:42  ivan
*  update for GM8135 IPCAM support
*
*  Revision 1.30  2016/08/04 11:40:51  foster_h
*  del platform duplicate_LCD info for AP
*
*  Revision 1.29  2016/08/04 10:15:24  schumy_c
*  Support 4k2k+1080p two frame mode
*
*  Revision 1.28  2016/07/18 09:49:43  waynewei
*  Fixed for crop scaling-up done by vpe.
*
*  Revision 1.27  2016/07/01 07:53:35  foster_h
*  modify comment for [7:0]latency drop enable to [3:0]latency drop enable
*
*  Revision 1.26  2016/07/01 07:25:42  foster_h
*  disable lcd latency drop function, when all playback display.
*
*  Revision 1.25  2016/06/30 05:44:16  schumy_c
*  Add vg_graph feature to support secondary LCD.(including duplicated or separated content)
*
*  Revision 1.24  2016/06/28 03:42:11  waynewei
*  1.Remove target_disp_rate from platform_info due to it would be updated to 0 after auto_apply.
*  2.Add global variable for target display rate handling.
*
*  Revision 1.23  2016/06/28 02:00:58  waynewei
*  Add fdt with jpeg.
*
*  Revision 1.22  2016/06/23 02:59:12  ivan
*  add vpe prediction enable fature
*
*  Revision 1.21  2016/06/02 07:37:26  klchu
*  modify decode keyframe for new vpe.
*
*  Revision 1.20  2016/05/19 02:35:50  foster_h
*  support image_quality attr
*
*  Revision 1.19  2016/05/18 08:58:55  waynewei
*  Remove api_get_cas_scaling.
*
*  Revision 1.18  2016/05/18 03:51:39  schumy_c
*  Add encode type to sysinfo.
*  Support only PCM in default.
*
*  Revision 1.17  2016/05/16 07:38:06  foster_h
*  rename ivs pool name
*
*  Revision 1.16  2016/05/16 03:21:11  foster_h
*  move duplicate pool_buffer setting to gmlib
*
*  Revision 1.15  2016/05/10 03:23:50  ivan
*  update fdt to add cas_scl_offset and update more comments
*
*  Revision 1.14  2016/05/03 08:43:27  waynewei
*  Fixed for the mechanism of dynamic_src_xy_dim when cap->vpe(di) (dynamic should be disabled).
*
*  Revision 1.13  2016/04/28 08:33:07  schumy_c
*  Support virtual desktop for playback crop.
*
*  Revision 1.12  2016/04/18 03:05:19  foster_h
*  update LCD300 and LCD200 proc file about fb0_win
*
*  Revision 1.11  2016/04/15 02:27:27  foster_h
*  Rec vpe_scaling with no enc_scl_out protect.
*
*  Revision 1.10  2016/04/14 08:36:08  zhilong
*  Add engine_nu to vpd_vpe_1_entity_t and vpd_vpe_n_entity_t for indicate vpe engine
*
*  Revision 1.9  2016/04/13 06:12:13  foster_h
*  modify capture quality scaling mode
*
*  Revision 1.8  2016/04/12 06:23:47  foster_h
*  remove platform chip/cpu id
*
*  Revision 1.7  2016/04/12 04:28:18  foster_h
*  remove cpuid and duplicat param
*
*  Revision 1.6  2016/04/12 01:17:33  foster_h
*  remove cpuid
*
*  Revision 1.5  2016/04/08 08:59:19  foster_h
*  modify lcd input_res
*
*  Revision 1.4  2016/04/05 08:17:06  foster_h
*  modify entity driver proc file parsing
*
*  Revision 1.3  2016/03/30 01:52:26  schumy_c
*  Remove old audio ioctl.
*
*  Revision 1.2  2016/03/29 07:47:48  schumy_c
*  Add cap ext.ctrl property.
*  Add description for properties.
*
*  Revision 1.1.1.1  2016/03/25 10:39:11  ivan
*
*/

#ifndef __VPDRV_H__
#define __VPDRV_H__

#include "../VERSION"
#define VPD_VERSION_CODE   VG_VERSION

#define VPD_MAX_POLL_EVENT   2      //must same with 
#define VPD_POLL_READ        1      //event read ID
#define VPD_POLL_WRITE       2      //event write ID 

#define VPD_GET_POLL_IDX(event, eventIdx) \
    do { \
      switch(event) { \
          case VPD_POLL_READ: (eventIdx) = 0; break; \
          case VPD_POLL_WRITE: (eventIdx) = 1; break; \
          default: \
              dump_stack(); \
              printk("[err]%s:%d event=%d error!\n", __FUNCTION__, __LINE__, (event)); \
              printm("VP","[err]%s:%d event=%d error!\n", __FUNCTION__, __LINE__, (event)); \
              damnit("VP"); \
              break; \
      } \
    } while(0) 


#define VPLIB_NULL_BYTE     0xFE
#define VPLIB_NULL_VALUE    0xFEFEFEFE

#define VPD_RUN_STATE                       0
#define VPD_STOP_STATE                      -22

#define VPD_FLAG_NEW_FRAME_RATE  (1 << 3) ///< Indicate the bitstream of new frame rate setting
#define VPD_FLAG_NEW_GOP         (1 << 4) ///< Indicate the bitstream of new GOP setting
#define VPD_FLAG_NEW_DIM         (1 << 6) ///< Indicate the bitstream of new resolution
#define VPD_FLAG_NEW_BITRATE     (1 << 7) ///< Indicate the bitstream of new bitrate setting

#define VPD_MAX_BUFFER_LEN                      (8192)//for large vpdSclEntity_t size 
#define VPD_MAX_PCH_COUNT                       (128/*GM_MAX_BIND_NUM_PER_GROUP*/ + 1) //FIXME: +1 is that suppose pch=0 is reserved
#define VPD_MAX_STRING_LEN                      32
#define VPD_MAX_ENTITY_NUM                      10
#define VPD_MAX_CPU_NUM_PER_CHIP                2
#define VPD_MAX_CHIP_NUM                        4

#define VPD_MAX_GRAPH_NUM                       136

#define VPD_MAX_GROUP_NUM                       VPD_MAX_GRAPH_NUM  // must same with GM_MAX_GROUP_NUM
#define VPD_MAX_BIND_NUM_PER_GROUP              128  // must same with GM_MAX_BIND_NUM_PER_GROUP
#define VPD_MAX_DUP_LINE_ENTITY_NUM             6   // must same with GM_MAX_DUP_LINE_ENTITY_NUM
#define VPD_MAX_LINE_ENTITY_NUM                 16  // must same with GM_MAX_LINE_ENTITY_NUM

#define VPD_GRAPH_LIST_MAX_ARRAY                (VPD_MAX_GRAPH_NUM+31)/32
#define VPD_MAX_GRAPH_APPLY_NUM                 32  //original 8 
#define VPD_MAX_BUFFER_INFO_NUM                 64
#define VPD_MAX_VIDEO_BUFFER_NUM                128
#define VPD_MAX_CAPTURE_PATH_NUM                4
#define VPD_MAX_CAPTURE_CHANNEL_NUM             128
#define VPD_MAX_LCD_NUM                         6
#define VPD_MAX_LCD_NUM_PER_CHIP                3
#define VPD_MAX_AU_GRAB_NUM                     40
#define VPD_MAX_AU_RENDER_NUM                   32
#define VPD_SEPC_MAX_ITEMS                      10
#define VPD_SPEC_MAX_RESOLUTION                 10
#define VPD_CASCADE_CH_IDX                      32
#define VPD_3DI_WIDTH_MIN                       64
#define VPD_3DI_HEIGHT_MIN                      64
#define VPD_SCL_WIDTH_MIN                       64
#define VPD_SCL_HEIGHT_MIN                      64
#define VPD_MAX_USR_FUNC                        6
#define VPD_CrCbY_to_YCrYCb(x)                  ((((x) & 0xff) << 24) | ((x) & 0xff0000) | \
                                                (((x) & 0xff00) >> 8) | (((x) & 0xff) << 8))
#define VPD_MAX_DATAIN_CHANNEL                  32

#define VPD_INT_MSB(value)              (((unsigned int)(value)>>16) & 0x0000FFFF)  
#define VPD_INT_LSB(value)              (((unsigned int)(value)) & 0x0000FFFF)

#define VPD_GET_LINK_MAGIC(value)       VPD_INT_MSB(value)
#define VPD_GET_LINK_LEN(value)         VPD_INT_LSB(value)

#define VPD_GET_CHIP_ID(value)          (((unsigned int)(value) >> 24) & 0x000000FF)
#define VPD_GET_CPU_ID(value)           (((unsigned int)(value) >> 16) & 0x000000FF)
#define VPD_GET_GRAPH_ID(value)         VPD_INT_LSB(value)

#define VPD_SET_GRAPH_ID(id, value)     ((id) & 0xFFFF0000) | ((value) & 0x0000FFFF)

#define VPD_SET_ID_FOR_CHIP_CPU_GRAPH(chipid, cpuid, graphid) \
    (((((unsigned int)(chipid)) << 24) & 0xFF000000) | ((((unsigned int)(cpuid)) << 16) & 0x00FF0000)|\
        (((unsigned int)(graphid)) & 0x0000FFFF))



/* refer to structure vpd_graph_t */
/* entityMagic(4) + nextEntityOffs(4) + sizeof(vpd_entity_t) + entityStructSize */
#define	VPD_ENTITY_MAGIC_LEN    sizeof(unsigned int) /* VPD_ENTITY_MAGIC_NUM */
#define	VPD_ENTITY_OFFSET_LEN   sizeof(unsigned int) 
#define	VPD_ENTITY_HEAD_LEN     (VPD_ENTITY_MAGIC_LEN + VPD_ENTITY_OFFSET_LEN) 

#define VPD_GET_ENTITY_HEAD_ADDR_BY_ENTITY(entity_addr) \
    (void *)((void *)(entity_addr) - VPD_ENTITY_HEAD_LEN)
#define VPD_GET_ENTITY_HEAD_ADDR(addr, offset)       (void *)((void *)(addr) + (offset))
#define VPD_GET_ENTITY_ENTRY_ADDR(head_addr)         \
    (void *)((void *)(head_addr) + VPD_ENTITY_HEAD_LEN)
#define VPD_GET_ENTITY_MAGIC_VAL(head_addr)          *(unsigned int *)(head_addr)
#define VPD_GET_NEXT_ENTITY_OFFSET_VAL(head_addr)    \
    *(unsigned int *)((void *)(head_addr) + VPD_ENTITY_MAGIC_LEN)
#define VPD_GET_NEXT_ENTITY_OFFSET_ADDR(head_addr)   \
    (void *)((void *)(head_addr) + VPD_ENTITY_MAGIC_LEN)
#define VPD_SET_ENTITY_MAGIC(head_addr)     *(unsigned int *)(head_addr) = VPD_ENTITY_MAGIC_NUM
#define VPD_SET_NEXT_ENTITY_OFFSET(head_addr, offs) \
    *(unsigned int *)((head_addr) + VPD_ENTITY_MAGIC_LEN) = (unsigned int) (offs)
#define VPD_CALC_ENTITY_OFFSET(addr, entity_addr) \
    ((void *)(entity_addr) - (void *)(addr) - VPD_ENTITY_HEAD_LEN) 

/* refer to structure vpd_graph_t */
/* lineMagic(4) + nextlineOffset(4) + line_idx(4) + link_nr(4) */
/* + {inEntityOffset(4) + outEntityOffset(4)}  + ... + {inEntityOffset(4) + outEntityOffset(4)} */
#define	VPD_LINE_MAGIC_LEN    sizeof(unsigned int) /* VPD_ENTITY_MAGIC_NUM */
#define	VPD_LINE_OFFSET_LEN   sizeof(unsigned int) 
#define	VPD_LINE_IDX_LEN      sizeof(unsigned int) 
#define	VPD_LINK_NUM_LEN      sizeof(unsigned int) 
#define	VPD_LINE_HEAD_LEN     \
            (VPD_LINE_MAGIC_LEN + VPD_LINE_OFFSET_LEN + VPD_LINE_IDX_LEN + VPD_LINK_NUM_LEN)
#define	VPD_LINK_ENTRY_LEN    sizeof(unsigned int)            /* in/out */
#define	VPD_LINK_LEN          (2 * VPD_LINK_ENTRY_LEN)   /* in + out */
#define	VPD_LINE_LEN(link_nr)  VPD_LINE_HEAD_LEN + (VPD_LINK_LEN * (link_nr));          

#define VPD_GET_LINE_HEAD_ADDR(addr, offset)     (void *)((void *)(addr) + (offset))
#define VPD_GET_LINE_MAGIC_VAL(head_addr)        *(unsigned int *)(void *)(head_addr)
#define VPD_GET_LINE_MAGIC_ADDR(head_addr)       (void *)(void *)(head_addr)
#define VPD_GET_NEXT_LINE_OFFSET_VAL(head_addr) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN)
#define VPD_GET_NEXT_LINE_OFFSET_ADDR(head_addr) \
            (void *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN)
#define VPD_GET_LINE_IDX_VAL(head_addr) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN + VPD_LINE_OFFSET_LEN)
#define VPD_GET_LINK_NUM_VAL(head_addr) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN + VPD_LINE_OFFSET_LEN + \
                               VPD_LINE_IDX_LEN)
#define VPD_GET_LINK_IN_VAL(head_addr, linkidx) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_HEAD_LEN + ((linkidx) * VPD_LINK_LEN))
#define VPD_GET_LINK_OUT_VAL(head_addr, linkidx) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_HEAD_LEN + \
                       ((linkidx) * VPD_LINK_LEN) + VPD_LINK_ENTRY_LEN) 

#define VPD_SET_LINE_MAGIC(head_addr) \
            *(unsigned int *)(head_addr) = VPD_LINE_MAGIC_NUM
#define VPD_SET_NEXT_LINE_OFFSET(head_addr, offs) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN) = (unsigned int) (offs)
#define VPD_SET_LINE_IDX_VAL(head_addr, line_idx) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN + VPD_LINE_OFFSET_LEN) = (line_idx)
#define VPD_SET_LINK_NUM_VAL(head_addr, link_nr) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_MAGIC_LEN + VPD_LINE_OFFSET_LEN + \
                              VPD_LINE_IDX_LEN) = (link_nr)
#define VPD_SET_LINK_IN_VAL(head_addr, linkidx, in_offs) \
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_HEAD_LEN + ((linkidx) * VPD_LINK_LEN)) = \
                (unsigned int)(in_offs)
#define VPD_SET_LINK_OUT_VAL(head_addr, linkidx, out_offs)\
            *(unsigned int *)((void *)(head_addr) + VPD_LINE_HEAD_LEN + ((linkidx) * VPD_LINK_LEN) + \
                VPD_LINK_ENTRY_LEN) = (unsigned int)(out_offs)

#define	VPD_LINK_MAGIC_NUM                  0x4c494e4b  /* char: "LINK" */
#define	VPD_LINE_MAGIC_NUM                  0x4c494e45  /* char: "LINE" */
#define	VPD_ENTITY_MAGIC_NUM                0x454e5459  /* char: "ENTY" */

#define VPD_REC_CAP_GROUP                   0X1    /* FIXME: schumy added */
#define VPD_LV_CAP_GROUP                    0X2    /* FIXME: schumy added */
#define VPD_DISP_SCL_GROUP                  0X3    /* FIXME: schumy added */
#define VPD_AU_GRAB_GROUP                   0X4    /* FIXME: schumy added */

#define VPD_FLAG_OUTBUF_MAX_CNT             (0x1 << 0)
#define VPD_FLAG_INBUF_MAX_CNT              (0x1 << 1)
#define VPD_FLAG_HIGH_PRIORITY              (0x1 << 2)
#define VPD_FLAG_STOP_WHEN_APPLY            (0x1 << 3)
#define VPD_FLAG_PASS_EVEN_FAIL             (0x1 << 4)
#define VPD_FLAG_FLOW_BY_TICK               (0x1 << 5)
#define VPD_FLAG_ONE_BY_TICK                (0x1 << 6)
#define VPD_FLAG_NO_PUTJOB                  (0x1 << 7)
#define VPD_FLAG_REFERENCE_CLOCK            (0x1 << 8)
#define VPD_FLAG_NON_BLOCK                  (0x1 << 9)
#define VPD_FLAG_GROUP                      (0x1 << 10)
#define VPD_FLAG_DONT_CARE_CB_FAIL          (0x1 << 11)
#define VPD_FLAG_DIR_CB                     (0x1 << 12)
#define VPD_FLAG_ENODE_ENABLE               (0x1 << 13)
#define VPD_FLAG_SYNC_TUNING                (0x1 << 14)
#define VPD_FLAG_JOB_SEQUENCE_FIRST         (0x1 << 15)
#define VPD_FLAG_JOB_SEQUENCE_LAST          (0x1 << 16)
#define VPD_FLAG_LOW_FRAMERATE              (0x1 << 17)
#define SET_ENTITY_FEATURE(flag, value) \
    do{ \
        flag |= value; \
    }while(0)
//
typedef enum {
    GRAPH_ENCODE=1, 
    GRAPH_DISPLAY,
    GRAPH_AU_GRAB,
    GRAPH_AU_RENDER,
    GRAPH_AU_LIVESOUND,
    GRAPH_FILE_ENC
} vpd_graph_type_t;

typedef enum {
    VPD_NONE_ENTITY_TYPE=0, 
    VPD_CAP_ENTITY_TYPE, 
    VPD_DEC_ENTITY_TYPE,   
    VPD_VPE_1_ENTITY_TYPE,
    VPD_VPE_N_ENTITY_TYPE,
    VPD_H264E_ENTITY_TYPE,      //5
    VPD_MPEG4E_ENTITY_TYPE,
    VPD_MJPEGE_ENTITY_TYPE,  
    VPD_DISP_ENTITY_TYPE,  
    VPD_DIN_ENTITY_TYPE,    
    VPD_DOUT_ENTITY_TYPE,       //10
    VPD_AU_GRAB_ENTITY_TYPE,
    VPD_AU_RENDER_ENTITY_TYPE,
    VPD_OSG_ENTITY_TYPE,
    VPD_IVS_ENTITY_TYPE,
	VPD_3DNR_ENTITY_TYPE,
	VPD_TOTAL_ENTITY_CNT    /* Last one, Don't remove it */
} vpd_entity_type_t;

typedef enum {
    VPD_DISP0_IN_POOL=0,    /*  0:GM_LCD0 */
    VPD_DISP1_IN_POOL,      /*  1:GM_LCD1 */
    VPD_DISP2_IN_POOL,      /*  2:GM_LCD2 */
    VPD_DISP3_IN_POOL,      /*  3:GM_LCD3, channel zero */
    VPD_DISP4_IN_POOL,      /*  4:GM_LCD4, channel zero */
    VPD_DISP5_IN_POOL,      /*  5:GM_LCD5, channel zero */
    VPD_DISP0_CAP_OUT_POOL, /*  6:GM_LCD0 */
    VPD_DISP1_CAP_OUT_POOL, /*  7:GM_LCD1 */
    VPD_DISP2_CAP_OUT_POOL, /*  8:GM_LCD2 */
    VPD_DISP3_CAP_OUT_POOL, /*  9:GM_LCD3, channel zero */
    VPD_DISP4_CAP_OUT_POOL, /* 10:GM_LCD4, channel zero */
    VPD_DISP5_CAP_OUT_POOL, /* 11:GM_LCD5, channel zero */
    VPD_DISP0_DEC_IN_POOL,  /* 12:GM_LCD0 */
    VPD_DISP1_DEC_IN_POOL,  /* 13:GM_LCD1 */
    VPD_DISP2_DEC_IN_POOL,  /* 14:GM_LCD2 */
    VPD_DISP3_DEC_IN_POOL,  /* 15:GM_LCD3, channel zero */
    VPD_DISP4_DEC_IN_POOL,  /* 16:GM_LCD4, channel zero */
    VPD_DISP5_DEC_IN_POOL,  /* 17:GM_LCD5, channel zero */
    VPD_DISP0_DEC_OUT_POOL, /* 18:GM_LCD0 */
    VPD_DISP1_DEC_OUT_POOL, /* 19:GM_LCD1 */
    VPD_DISP2_DEC_OUT_POOL, /* 20:GM_LCD2 */
    VPD_DISP3_DEC_OUT_POOL, /* 21:GM_LCD3, channel zero */
    VPD_DISP4_DEC_OUT_POOL, /* 22:GM_LCD4, channel zero */
    VPD_DISP5_DEC_OUT_POOL, /* 23:GM_LCD5, channel zero */
    VPD_DISP0_DEC_OUT_RATIO_POOL,   /* 24:GM_LCD0 */
    VPD_DISP1_DEC_OUT_RATIO_POOL,   /* 25:GM_LCD1 */
    VPD_DISP2_DEC_OUT_RATIO_POOL,   /* 26:GM_LCD2 */
    VPD_DISP3_DEC_OUT_RATIO_POOL,   /* 27:GM_LCD3, channel zero */
    VPD_DISP4_DEC_OUT_RATIO_POOL,   /* 28:GM_LCD4, channel zero */
    VPD_DISP5_DEC_OUT_RATIO_POOL,   /* 29:GM_LCD5, channel zero */
    VPD_DISP0_ENC_SCL_OUT_POOL,     /* 30:GM_LCD0 */
    VPD_DISP1_ENC_SCL_OUT_POOL,     /* 31:GM_LCD1 */
    VPD_DISP2_ENC_SCL_OUT_POOL,     /* 32:GM_LCD2 */
    VPD_DISP3_ENC_SCL_OUT_POOL,     /* 33:GM_LCD3, channel zero */
    VPD_DISP4_ENC_SCL_OUT_POOL,     /* 34:GM_LCD4, channel zero */
    VPD_DISP5_ENC_SCL_OUT_POOL,     /* 35:GM_LCD5, channel zero */
    VPD_DISP0_ENC_OUT_POOL,         /* 36:GM_LCD0 */   
    VPD_DISP1_ENC_OUT_POOL,         /* 37:GM_LCD1 */
    VPD_DISP2_ENC_OUT_POOL,         /* 38:GM_LCD2 */
    VPD_DISP3_ENC_OUT_POOL,         /* 39:GM_LCD3, channel zero */
    VPD_DISP4_ENC_OUT_POOL,         /* 40:GM_LCD4, channel zero */
    VPD_DISP5_ENC_OUT_POOL,         /* 41:GM_LCD5, channel zero */
    VPD_ENC_CAP_OUT_POOL,           /* 42: */
    VPD_ENC_IVS_POOL,           /* 43: */
    VPD_ENC_SCL_OUT_POOL,           /* 44: */
    VPD_ENC_OUT_POOL,               /* 45: */
    VPD_AU_ENC_AU_GRAB_OUT_POOL,    /* 46: */
    VPD_AU_DEC_AU_RENDER_IN_POOL,   /* 47: */
    VPD_END_POOL      /* Last one, Don't remove it */
} vpbuf_pool_type_t;

typedef enum {
    VPD_ACTIVE_BY_APPLY,
    VPD_ACTIVE_IMMEDIATELY,
} vpd_clr_win_mode_t;

typedef enum {
    VPD_SNAPSHOT_ENC_TYPE,
    VPD_SNAPSHOT_DEC_TYPE,
    VPD_SNAPSHOT_DISP_WINDOW_TYPE,
} vpd_snapshot_type_t;

typedef enum {
    VPD_METHOD_FRAME = 0,
    VPD_METHOD_FRAME_2FRAMES,
    VPD_METHOD_2FRAMES,
    VPD_METHOD_2FRAMES_2FRAMES,
    VPD_METHOD_FRAME_FRAME
} vpd_disp_group_method_t;

typedef struct {
    int line_idx;   // mapping to line index of graph
    int id;         //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15) 
    int entity_fd;
    int reserve[2]; // NOTE: vpdChannelFD total len = GM_FD_MAX_PRIVATE_DATA 
} vpd_channel_fd_t; // NOTE: relative to gm_pollfd_t

typedef enum { 
    VPD_INTERLACED,
    VPD_PROGRESSIVE
//    VPD_RGB888,
//    VPD_ISP,
} vpd_video_scan_method_t;



/**************************************************************************************************/
/* common definition */
/**************************************************************************************************/
typedef struct { 
    unsigned int x;
    unsigned int y;
} pos_t;

typedef struct { 
    unsigned int w;
    unsigned int h;
} dim_t;
#define IS_ZERO_DIM(p_dim)     ((p_dim)->w == 0 || (p_dim)->h == 0)

typedef struct { 
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
} rect_t;
#define IS_ZERO_RECT(p_rect)    ((p_rect)->w == 0 || (p_rect)->h == 0)

typedef enum { 
    VPD_BITSTREAM          = 0x01000000,
    VPD_YUV420             = 0x04200000,
    VPD_YUV422             = 0x04220000,
    VPD_RGB565             = 0x05650000,
    VPD_RGB1555            = 0x15550000,
    VPD_RGB8888            = 0x88880000,
    VPD_IVS_CASCADE_SCALING= 0xFFFF0000,
} vpd_buf_type_t;

typedef struct { 
    int is_dpcm : 4;
    int is_mb   : 4;
} vpd_buf_type_ex_t;


typedef enum {
    VPD_DI_OFF = 0,
    VPD_DI_TEMPORAL = 1,
    VPD_DI_SPATIAL = 2,
    VPD_DI_TEMPORAL_SPATIAL = 3,
} vpd_di_mode_t;

typedef enum {
    VPD_DN_OFF = 0,
    VPD_DN_TEMPORAL = 1,
    VPD_DN_SPATIAL = 2,
    VPD_DN_TEMPORAL_SPATIAL = 3,
} vpd_dn_mode_t;

typedef enum {
    VPD_SHARPNESS_OFF = 0,
    VPD_SHARPNESS_ON = 1,
} vpd_sharpness_mode_t;

typedef enum {
    VPD_VPE_BACKGROUND = 0,
    VPD_VPE_PIP_LAYER = 1
} vpd_vpe_feature_t;

typedef enum {
    VPD_VRC_CBR,    ///< Constant Bitrate
    VPD_VRC_VBR,    ///< Variable Bitrate
    VPD_VRC_ECBR,   ///< Enhanced Constant Bitrate
    VPD_VRC_EVBR,   ///< Enhanced Variable Bitrate
} vpd_vrc_mode_t;  /* video codec rate control mode */

typedef enum {
    VPD_SLICE_TYPE_P_FRAME,
    VPD_SLICE_TYPE_B_FRAME,
    VPD_SLICE_TYPE_I_FRAME,
    VPD_SLICE_TYPE_AUDIO_BS,
    VPD_SLICE_TYPE_RAW,
} vpd_slice_type_t;  /* video codec rate control mode */


typedef enum {
    VPD_CAP_METHOD_FRAME = 0,   // 'even/odd interlacing frame' or 'whole progressive frame'
    VPD_CAP_METHOD_2FRAME,      // 'half-even follows half-odd frame'
    VPD_CAP_METHOD_FIELD,
    VPD_CAP_METHOD_2FIELD
} vpd_cap_method_t;

typedef enum {
    VPD_FLIP_NONE = 0,
    VPD_FLIP_VERTICAL,
    VPD_FLIP_HORIZONTAL,
    VPD_FLIP_VERTICAL_AND_HORIZONTAL
} vpd_flip_t;

typedef enum {
    VPD_ROTATE_NONE = 0,
    VPD_ROTATE_LEFT,
    VPD_ROTATE_RIGHT,
    VPD_ROTATE_180,         //rotate 180 degree
} vpd_rotate_t;

typedef enum { 
    VPD_AU_NONE = 0,
    VPD_AU_PCM,
    VPD_AU_ACC,
    VPD_AU_ADPCM,
    VPD_AU_G711_ALAW,
    VPD_AU_G711_ULAW,
} vpd_au_enc_type_t;


typedef enum { 
    VPD_NORMAL_ENC = 0,  
    VPD_CHANNEL_ZERO_ENC,
    VPD_DISP_TO_ENC,
} vpd_enc_feature_t;

typedef enum { 
    VPD_DATAIN_AUTO = 0,  
    VPD_DATAIN_420,
    VPD_DATAIN_422,
} vpd_datain_fmt_t;

typedef enum { 
    VPD_TICK_ONE_JOB = 1,
} vpd_vpe_tick_job_t;

/**************************************************************************************************/
/* entity data from lib */
/**************************************************************************************************/

typedef struct {
    unsigned int pch;

    unsigned int cap_vch;
    unsigned int path;

    unsigned int buf_offset;
    unsigned int buf_size;
    vpd_buf_type_t dst_fmt;
    vpd_buf_type_ex_t dst_fmt_ex;
    
    dim_t        src_bg_dim;
    dim_t        dst_bg_dim;
    rect_t       dst_rect;
    rect_t       crop_rect;   //cap can crop by "ID_DST_CROP_XY/DIM" or "ID_SRC_XY/DIM"
    
    struct {
        // if user set 10fps, then fill--> target_fps=10, ratio=VPLIB_NULL_VALUE
        //    in the EM, set ID_FPS_RATIO=30:10 which 30 is obtained from platform_info
        //    in this case, EM needs to runtime update ID_FPS_RATIO if receive "framerate change" notification
        // if user set 7:3, then fill--> target_fps=VPLIB_NULL_VALUE, ratio=7:3
        //    in the EM, set ID_FPS_RATIO=7:3
        unsigned int    target_fps;
        unsigned int    ratio;
    } frame_rate;

    vpd_cap_method_t method;
    int dma_number;
    int auto_aspect_ratio;
    int frame_retrieve_when_overrun;
    vpd_flip_t flip_mode;
    struct {
        int is_right_extend;
        int pallete_idx;
    } ext_ctrl;
} vpd_cap_entity_t;

typedef struct {
    int attr_vch;
    int pch_count;
    int engine_nu;
    int predict_enable;
    struct {
        unsigned int pch;
        //source
        vpd_buf_type_t src_fmt;
        vpd_buf_type_ex_t src_fmt_ex;
        int is_dynamic_src_rect; //true for playback, then 'src_rect' is virtual and need recalculate
        dim_t src_bg_dim;
        rect_t crop_rect;
        dim_t crop_bg_dim;
        rect_t crop_rect2;
        dim_t crop_bg_dim2;
        rect_t src_rect;
        rect_t src_rect2;
        unsigned int buf_offset;
        unsigned int buf_size;

        //destination
        vpd_buf_type_t dst_fmt;
        vpd_buf_type_ex_t dst_fmt_ex;
        dim_t dst_bg_dim;
        dim_t dup_dst_bg_dim;
        int dup_src_region;
        rect_t dst_rect;
        rect_t dst_rect2;
        rect_t hole;

        unsigned int draw_sequence; //0(background, last draw) 1(pip layer, first draw)
        vpd_di_mode_t di_mode;
        vpd_dn_mode_t dn_mode;
        vpd_sharpness_mode_t shrp_mode;
        unsigned int spnr; // spatial denoise, range:0(low) ~ 100(high)
        unsigned int tmnr; // temporal denoise, range:0(low) ~ 64(high)
        unsigned int sharpness; // sharpness, range:0(low) ~ 100(high)

        vpd_rotate_t rotate_mode;
        unsigned int aspect_ratio_palette;
        unsigned int cas_scl_ratio; //for face detection, cascade scaling
        unsigned int cas_scl_offset; //for face detection, cascade scaling
    } pch_param;
} vpd_vpe_1_entity_t;

typedef struct {
    int attr_vch;
    int pch_count;
    int engine_nu;
    int predict_enable;
    struct {
        unsigned int pch;
        //source
        vpd_buf_type_t src_fmt;
        vpd_buf_type_ex_t src_fmt_ex;
        dim_t src_bg_dim;
        
        rect_t crop_rect;
        dim_t crop_bg_dim;
        rect_t crop_rect2;
        dim_t crop_bg_dim2;
        rect_t src_rect;
        rect_t src_rect2;
        unsigned int buf_offset;
        unsigned int buf_size;

        //destination
        vpd_buf_type_t dst_fmt;
        vpd_buf_type_ex_t dst_fmt_ex;
        dim_t dst_bg_dim;
        dim_t dup_dst_bg_dim;
        int dup_src_region;
        rect_t dst_rect;
        rect_t dst_rect2;
        rect_t hole;

        unsigned int draw_sequence; //0(background, last draw) 1(pip layer, first draw)
        vpd_di_mode_t di_mode;
        vpd_dn_mode_t dn_mode;
        vpd_sharpness_mode_t shrp_mode;
        unsigned int spnr; // spatial denoise, range:0(low) ~ 100(high)
        unsigned int tmnr; // temporal denoise, range:0(low) ~ 64(high)
        unsigned int sharpness; // sharpness, range:0(low) ~ 100(high)

        vpd_rotate_t rotate_mode;
        unsigned int aspect_ratio_palette;
        unsigned int cas_scl_ratio; //for face detection, cascade scaling
        unsigned int cas_scl_offset; //for face detection, cascade scaling
    } pch_param[VPD_MAX_PCH_COUNT];
} vpd_vpe_n_entity_t;

typedef struct {
    unsigned int reserved:30;
    unsigned int direction:2;
} vpd_osg_param_1_t;

typedef struct {
    int attr_vch;
    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t src_bg_dim;
    vpd_osg_param_1_t osg_param_1;
}vpd_osg_entity_t;

#define VPD_MAX_FDT_PARAM_NUM 32

typedef struct {
    unsigned int reserved:5;
    unsigned int ratio:7;
    unsigned int first_width:10; 
    unsigned int first_height:10;
} vpd_fdt_global_1_t;  //face detection global parameter

typedef struct {
    unsigned int jpeg_quality; ///< 1(bad quality) ~ 100(best quality)
} vpd_fdt_global_2_t;  //face detection global parameter

typedef struct {
    int attr_vch;
    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t src_bg_dim;
    unsigned int cas_scl_offset;
    vpd_fdt_global_1_t fdt_global_1;
    vpd_fdt_global_2_t fdt_global_2;
} vpd_ivs_entity_t;

typedef struct {
    int lcd_vch;

    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t   src_bg_dim;
    rect_t  src_rect;
    rect_t  disp1_rect;
    unsigned int src_frame_rate;
    unsigned int feature; ///< feature: [4:0]latency drop enable,  0(disable)  1(enable)
} vpd_disp_entity_t;

typedef struct {
    int     pch;
    int     attr_vch;
    pos_t   dst_pos;
    dim_t   dst_bg_dim;
//    dim_t   max_dec_dim;    // 'max_recon_dim' in old code
    vpd_buf_type_t dst_fmt;
    vpd_buf_type_ex_t dst_fmt_ex;
    unsigned int yuv_width_threshold;
    unsigned int sub_yuv_ratio;   
    
} vpd_dec_entity_t;

#define MAX_ROI_QP_COUNT    8
typedef struct {
    int pch;
    //source
    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t   src_bg_dim;
    rect_t  src_rect;

    //rate-control
    struct {
        vpd_vrc_mode_t mode;
        unsigned int src_frame_rate;
        unsigned int fps_ratio;
        unsigned int init_quant;
        unsigned int max_quant;
        unsigned int min_quant;
        unsigned int bitrate;
        unsigned int bitrate_max;
    } rc;

    //others
    unsigned int idr_interval;
    unsigned int b_frame_num;
    unsigned int fast_forward_value;
    unsigned int enable_mv_data;
    unsigned int enable_field_coding;
    unsigned int enable_gray_coding;

    unsigned int watermark_pattern;
    unsigned int vui_param_value;
    unsigned int vui_sar_value;

    vpd_slice_type_t slice_type;
    unsigned int slice_number;
    vpd_di_mode_t di_mode;
    vpd_dn_mode_t dn_mode;
    vpd_sharpness_mode_t shrp_mode;
    unsigned int spnr; // spatial denoise, range:0(low) ~ 100(high)
    unsigned int tmnr; // temporal denoise, range:0(low) ~ 64(high)
    unsigned int sharpness; // sharpness, range:0(low) ~ 100(high)

    struct {
        unsigned int type_value;    //66(Baseline) 77(Main) 88(Extended) ... see h264 define
        unsigned int level_value;
        unsigned int config_value;
        unsigned int symbol_value;  // original name is 'coding'
    } profile;
    unsigned int checksum_type_value;
    vpd_rotate_t rotate_mode;

    unsigned int roi_qp_offset;
    rect_t roi_qp_region_in_mb[MAX_ROI_QP_COUNT];   //the unit is in "macro block", not "pixel"
    unsigned int roi_root_time; //ap set roi_root_time for roi latency checked by cat ./property
} vpd_h264e_entity_t;

typedef struct {
    int pch;
    //source
    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t   src_bg_dim;
    rect_t  src_rect;

    //rate-control
    struct {
        vpd_vrc_mode_t mode;
        unsigned int src_frame_rate;
        unsigned int fps_ratio;
        unsigned int init_quant;
        unsigned int max_quant;
        unsigned int min_quant;
        unsigned int bitrate;
        unsigned int bitrate_max;
    } rc;
    unsigned int idr_interval;
    vpd_slice_type_t slice_type;
    unsigned int checksum_type_value;

} vpd_mpeg4e_entity_t;

typedef struct {
    int pch;
    //source
    vpd_buf_type_t src_fmt;
    vpd_buf_type_ex_t src_fmt_ex;
    dim_t   src_bg_dim;
    rect_t  src_rect;

    //rate-control
    struct {
        vpd_vrc_mode_t mode;
        unsigned int src_frame_rate;
        unsigned int fps_ratio;
        unsigned int quality;
        unsigned int bitrate;
        unsigned int bitrate_max;
    } rc;
    unsigned int restart_interval;

} vpd_mjpege_entity_t;

typedef struct {
    unsigned int id;
    int is_raw_out;
    unsigned int raw_out_size;
    unsigned int src_frame_rate;
    unsigned int fps_ratio;
} vpd_dout_entity_t;

typedef struct {
    int pch;
    vpd_datain_fmt_t fmt;
/*    int buf_width;
    int buf_height;
    int buf_fps;
    int buf_format;
    int is_timestamp_by_ap;*/
    int attr_vch;
    int is_audio;
} vpd_din_entity_t;


typedef struct {
    unsigned int au_group_no;
    unsigned int sample_rate;
    unsigned int sample_bits;
    unsigned int tracks;
    unsigned int ch_bmp;
    unsigned int ch_bmp2;   //for channel index 32~64
    unsigned int frame_samples;
    int pch_count;
    struct {
        unsigned int pch;
        unsigned int vch;
        vpd_au_enc_type_t enc_type;
        unsigned int bitrate;
        unsigned int buf_offset;
        unsigned int buf_size;
    } pch_param[VPD_MAX_PCH_COUNT];
} vpd_au_grab_entity_t;

typedef struct  {
    unsigned int ch_bmp;
    unsigned int ch_bmp2;
    unsigned int resample_ratio;
    vpd_au_enc_type_t encode_type;
    int block_size;
    int sync_with_lcd_vch;
    int sample_rate;
    int sample_bits;
    unsigned int tracks;

} vpd_au_render_entity_t;


/**************************************************************************************************/
/* Features */
/**************************************************************************************************/
/* clear window */
typedef struct {
    int in_width;
    int in_height;
    vpd_buf_type_t in_fmt;  //todo:update name to 'in_buf_type'
    vpd_buf_type_ex_t in_fmt_ex;
    unsigned char *in_buf; 
    int lcd_vch;
    int out_x;
    int out_y;
    int out_width;
    int out_height;
    vpd_clr_win_mode_t  mode;
    unsigned int attr_vch;
    vpd_channel_fd_t channel_fd;  // clear_bitstream only
    int reserved[3];
}__attribute__((packed, aligned(8))) vpd_clr_win_t;

/* CVBS display vision */
typedef struct {
    int lcd_vch;
    int id;
    int sn;
    int out_x;
    int out_y;
    int out_width;
    int out_height;
    int reserved[4];
}__attribute__((packed, aligned(8))) vpd_cvbs_vision_t;

/* snapshot */
#define MODE_INTERFERENCE  0x2694
#define MAX_MULTI_SNAPSHOT_COUNT    2
typedef struct {
    unsigned int frame_timestamp;
    unsigned int count;
    struct {
        unsigned int size;
        unsigned int offset;
    } info[MAX_MULTI_SNAPSHOT_COUNT];
}__attribute__((packed, aligned(8))) snapshot_size_offset_array_t;

typedef struct {
    vpd_channel_fd_t channel_fd;  // associate with library struct gm_pollfd_t:int fd_private[4]
    vpd_snapshot_type_t type;
    char *bs_buf;       /* buffer point, driver will put bitstream in this buf */
    int bs_len;         /* buffer length, to tell driver the length of buf. */
    int timeout_ms;
    int image_quality;
    int src_x;
    int src_y;
    int src_width;
    int src_height;
    int bs_width;
    int bs_height;
    int bs_size;
    int bs_offset;
    int crop_xy;
    int crop_dim;
    unsigned int timestamp;
    int reserved[4];
} __attribute__((packed, aligned(8))) vpd_snapshot_t;


typedef struct {
   int  lcd_vch;
   char *bs_buf;
   int  bs_len;
   int  image_quality;
   int  timeout_ms;
   unsigned int bs_width;
   unsigned int bs_height;
   unsigned int bs_type;    ///< 0:jpeg  1:h264 I
   unsigned int slice_type;
   int crop_xy;
   int crop_dim;
   unsigned int timestamp;
   int reserved[4];
} __attribute__((packed, aligned(8))) vpd_disp_snapshot_t;

/* cap get raw data */
typedef enum {
    VPD_CAP_OBJECT = 0xFEFE0001,         ///< Capture object type
    VPD_ENCODER_OBJECT = 0xFEFE0002,     ///< Encoder object type
    VPD_WIN_OBJECT = 0xFEFE0003,         ///< Window object type
    VPD_FILE_OBJECT = 0xFEFE0004,        ///< File object type
    VPD_AUDIO_GRAB_OBJECT = 0xFEFE0005,     ///< Audio grabber object type
    VPD_AUDIO_ENCODER_OBJECT = 0xFEFE0006,  ///< Audio encoder object type
    VPD_AUDIO_RENDER_OBJECT = 0xFEFE0007,   ///< Audio renderer object type
} vpd_obj_type_t;

typedef struct {
    int                 vch;
    int                 path;
	rect_t src_crop;
	dim_t out_dim;
	vpd_rotate_t direction;
    char                *yuv_buf;
    unsigned int        yuv_buf_len;
    int                 timeout_ms;
    vpd_channel_fd_t    channel_fd;
    vpd_obj_type_t obj_type;
}__attribute__((packed, aligned(8))) vpd_region_rawdata_t;

/* get cap source */
typedef struct {
    int             cap_vch;        //user channel number
    char            *yuv_buf;       //allocatd buffer by AP
    unsigned int    yuv_buf_len;    //the length of allocated buffer
    unsigned int    width;          //output the real width from system
    unsigned int    height;         //output the real height from system
    int             timeout_ms;     //timeout vlaue
}__attribute__((packed, aligned(8))) vpd_cap_source_t;

/* get cascade scaling */
typedef struct {
    int             cap_vch;        //user channel number
    unsigned int    ratio;          //value range:80~91, it means 80%~91%
    unsigned int    first_width;    //the width of first frame, input by AP
    unsigned int    first_height;   //the height of first frame, input by AP
    char            *yuv_buf;       //allocatd buffer by AP
    unsigned int    yuv_buf_len;    //the length of allocated buffer
    int             timeout_ms;     //timeout value
}__attribute__((packed, aligned(8))) vpd_cas_scaling_t;

/* Keyframe */
typedef struct {
   int  bs_width;
   int  bs_height;
   char *bs_buf;
   int  bs_len;
   int  yuv_width;
   int  yuv_height;
   char *yuv_buf;
   int  yuv_buf_len;
   int  timeout_ms;
}__attribute__((packed, aligned(8))) vpd_decode_keyframe_t;


/**************************************************************************************************/
/* System information */
/**************************************************************************************************/
typedef struct {
    unsigned int version;   // VPD_VERSION_CODE 0x0001 --> "00.01"
    char compiler_date[16];
    char compiler_time[16];
} vpd_version_t;

typedef struct {
    char         name[48]; 
    unsigned int id;
    unsigned int width;
    unsigned int height;
} vpd_lcd_res_info_t;

typedef struct {
    /* 1st U32 */
    int valid:2;
    int start_ddr_no:8;
    int end_ddr_no:8;
    unsigned int reserved1:14; ///< Reserved bits
    /* 2nd U32 */
    int start_cap_vch:9;
    int end_cap_vch:9;
    int reserved2:14; ///< Reserved bits
    /* 3nd U32 */
    int start_file_vch:9;
    int end_file_vch:9;
    int reserved3:14; ///< Reserved bits
    /* others */
    int reserved[4];         ///< Reserved words
} vpd_chip_info_t;

typedef struct {
    /* 1st U32_int integer */
    int          lcdid:4;    //lcdid < 0 : the entry is invalid
    unsigned int chipid:2;
    unsigned int is3di:1;
    unsigned int fps:9;
    unsigned int lcd_type:8;
    unsigned int timestamp_by_ap:1;
    unsigned int pool_type:7; 
    /* 2nd U32_int integer */
    unsigned int method:4;  /* vpdDispGroupMethod_t */ //TODO:8220 remove this....
    unsigned int max_disp_rate:16;
    unsigned int channel_zero:1;        /*1:support channel zero 0:not support*/
    unsigned int lcd_vch:3;
    int src_duplicate_vch:4;   //duplicate from which lcd_vch
    unsigned int src_duplicate_region:2;   //duplicate region
    int reserved2:2; /* reserved */
    /* others */
    vpd_lcd_res_info_t fb0_win;
    vpd_lcd_res_info_t desk_res;
    vpd_lcd_res_info_t output_type;
    unsigned int       fb_vaddr;
    int active:16;
    unsigned int max_disp_width:16;
    unsigned int max_disp_height:16;
    char reserved[14];         ///< Reserved words
} vpd_lcd_sys_info_t;

#define VPD_CAP_SD_OUT_MODE 0
#define VPD_CAP_TC_OUT_MODE 1


typedef struct {
    /* 1st U32 */
    unsigned int scl_dn_w_ratio:5;        /* scaler down: horizontal (width) ratio max */
    unsigned int scl_dn_h_ratio:5;        /* scaler down: vertical (height) ratio max */
    unsigned int scl_dn_w_qty_ratio:5;    /* scaler down: horizontal (width) quality ratio max */
    unsigned int scl_dn_h_qty_ratio:5;    /* scaler down: vertical (height) quality ratio max */
    unsigned int reserved1: 12;
    /* 2st U32 */
    int reserved2:32;         ///< Reserved words
} vpd_cap_ability_info_t;

typedef struct {
    /* 1st U32 */
    int vcapch:9;   // vcapch < 0 : the entry is invalid, capture internal ch number
    unsigned int chipid:2;
    unsigned int num_of_path:4;
    unsigned int scan_method:3;
    unsigned int fps:9;
    unsigned int reserved1:5;
    /* 2nd U32 */
    int ch:9; // ch < 0 : the entry is invalid, front_end channel
    unsigned int vlos_sts:2;
    unsigned int cap_vpe_scaling:1;
    unsigned int reserved2:20;
    /* 3nd U32 */
    unsigned int width:16;
    unsigned int height:16;
    /* others */
    unsigned int engine_minor;
    vpd_cap_ability_info_t ability; 
    int reserved[4];         ///< Reserved words
} vpd_cap_sys_info_t;

typedef struct { 
    int attr_vch;            ///< from file attribute
    unsigned int fd;         ///< it is a fd of datain.
    unsigned int scl1_fd;
   int reserved[10];         ///< Reserved words
} vpd_datain_info_t;

#define VPD_SAMPLE_RATE_8K          (1 << 0)    /*  8000 Hz */
#define VPD_SAMPLE_RATE_11K         (1 << 1)    /* 11025 Hz */
#define VPD_SAMPLE_RATE_16K         (1 << 2)    /* 16000 Hz */
#define VPD_SAMPLE_RATE_22K         (1 << 3)    /* 22050 Hz */
#define VPD_SAMPLE_RATE_32K         (1 << 4)    /* 32000 Hz */
#define VPD_SAMPLE_RATE_44K         (1 << 5)    /* 44100 Hz */
#define VPD_SAMPLE_RATE_48K         (1 << 6)    /* 48000 Hz */

#define VPD_SAMPLE_SIZE_8BIT        (1 << 0)
#define VPD_SAMPLE_SIZE_16BIT       (1 << 1)

#define VPD_CHANNELS_1CH            (1 << 0)    /* mono   */
#define VPD_CHANNELS_2CH            (1 << 1)    /* stereo */

#define VPD_TYPE_PCM                (1 << 0)
#define VPD_TYPE_AAC                (1 << 1)
#define VPD_TYPE_ADPCM              (1 << 2)
#define VPD_TYPE_G711_ALAW          (1 << 3)
#define VPD_TYPE_G711_ULAW          (1 << 4)


#define STRING_LEN     31
typedef struct {
    /* 1st U32_int integer */
    int ch:10;
    unsigned int chipid:2;
    unsigned int reserved:20;
    unsigned int frame_samples;
    unsigned int sample_rate_type_bmp;
    unsigned int sample_size_type_bmp;
    unsigned int channels_type_bmp;
    unsigned int encode_type_bmp;
    unsigned int ssp;
    unsigned int group;
    char description[STRING_LEN + 1];
    /* others */
    unsigned int engine_minor;
    int reserved2[4];         ///< Reserved words
} vpd_au_grab_sys_info_t;

typedef struct {
    /* 1st U32_int integer */
    int ch:10;
    unsigned int chipid:2;
    unsigned int reserved:20;
    /* 2nd U32_int integer */
    unsigned int frame_samples;
    unsigned int sample_rate_type_bmp;
    unsigned int sample_size_type_bmp;
    unsigned int channels_type_bmp;
    unsigned int encode_type_bmp;
    unsigned int ssp;
    unsigned int group;
    char description[STRING_LEN + 1];
    /* others */
    unsigned int engine_minor;
    int reserved2[4];         ///< Reserved words
} vpd_au_render_sys_info_t;

typedef struct {
    /* 1st U32_int integer */
    int b_frame_nr:4;
    unsigned int reserved:28;
    int reserved2[2];         ///< Reserved words
} vpd_dec_sys_info_t;   

typedef struct {
    /* 1st U32_int integer */
    int b_frame_nr:4;
    unsigned int reserved:28;
    int reserved2[2];         ///< Reserved words
} vpd_enc_sys_info_t;   

typedef struct  {    
    char res_type[VPD_SPEC_MAX_RESOLUTION][7];
    int max_channels[VPD_SPEC_MAX_RESOLUTION];
    int max_fps[VPD_SPEC_MAX_RESOLUTION];
}vpd_res_item_t;

typedef struct {
    /* 1st U32_int integer */
    unsigned int exist_enc_scl_out_pool:1;  /* enc_scl_out_pool, 1: existed, 0: not existed */
    unsigned int reserved1:31;
    /* others reserved */
    int reserved[4];         ///< Reserved words
} vpd_spec_info_t;

typedef struct {
    struct {
        unsigned int dpcm:1;
        unsigned int yuv420:1;
        unsigned int reserved:30;
    } enc;
    struct {
        unsigned int cap_dpcm:1;
        unsigned int lcd_dpcm:1;
        unsigned int reserved:30;
    } disp[VPD_MAX_LCD_NUM];
} vpd_buffer_fmt_t;

typedef struct {
    unsigned int graph_type;
    char graph_name[48];
    vpd_chip_info_t         chip_info[VPD_MAX_CHIP_NUM];
    vpd_cap_sys_info_t      cap_info[VPD_MAX_CAPTURE_CHANNEL_NUM];
    vpd_lcd_sys_info_t      lcd_info[VPD_MAX_LCD_NUM];
    vpd_au_grab_sys_info_t  au_grab_info[VPD_MAX_AU_GRAB_NUM];
    vpd_au_render_sys_info_t au_render_info[VPD_MAX_AU_RENDER_NUM];
    vpd_dec_sys_info_t      dec_info;
    vpd_enc_sys_info_t      enc_info;
    vpd_spec_info_t         spec_info;
    int transcode_lcd_num;
    vpd_buffer_fmt_t buffer;
} __attribute__((packed, aligned(8))) vpd_sys_info_t;



/**************************************************************************************************/
/* vpd internal */
/**************************************************************************************************/

typedef struct {
    unsigned int length;   
    unsigned char *str;
} vpd_send_log_t;

typedef struct {
    int fd;
    int type;   
} vpd_entity_notify_t;

typedef struct {
    unsigned int dbg_level;   
    unsigned int dbg_mode;
} vpd_gmlib_dbg_t;


typedef struct {
    unsigned int in_offs; 
    unsigned int out_offs; 
} vpd_link_entity_offs_t;

typedef struct {
    unsigned int line_magic;
    unsigned int next_lineoffs;
    unsigned int line_idx;
    unsigned int link_nr;
    vpd_link_entity_offs_t link[VPD_MAX_LINE_ENTITY_NUM];
} vpd_line_t;

typedef struct {
    vpd_entity_type_t type;   /* entity type, set by gmlib */
    int feature;
    int group_id;
    int duplicate_pool;
    vpbuf_pool_type_t pool_type;
    char pool_name[VPD_MAX_STRING_LEN];    
    int pool_width;      //for video only
    int pool_height;     //for video only
    int pool_fps;        //for video only
    int pool_vch;        // entity vch, use to get ddr no.
    int gs_flow_rate;
    int src_fmt;
    int sn;     /* sequence number, set by gmlib */
    void *e;    /* entity, set by gmlib */
    void *priv; /* entity private data (vplibEntityPriv_t), for vpd use */  
    unsigned int ap_bindfd; // application bindfd
    int extra_buffers;
    int reserve[1];
} vpd_entity_t;

typedef struct {
    vpd_entity_t *in; 
    vpd_entity_t *out; 
} vpd_link_entity_addr_t;

typedef struct {
    void *line_head;
    unsigned int line_idx;
    unsigned int link_nr;
    vpd_link_entity_addr_t link[VPD_MAX_LINE_ENTITY_NUM];
} vpd_get_line_info_t;

typedef struct {
    int valid;      // set by vpd: to check this graph is in use. 
                    // set by gmlib: bit_31(0x80000000) for gmlib force_graph_exit(abnormal graph)
                    //               bit_00 for check this graph is in use.
                    // gmlib and vpd set/maintain the valid by himself. 
    char name[20];
    int len;        // set by gmlib, total graph data size
    int id;         // set by gmlib, return a real used graphid, id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15) 
    int buf_size;   // buffer size for this group, dynamically alloc and grow up.
    vpd_graph_type_t type;  // set by gmlib
    int entity_nr;
    int line_nr;
    void *group;        // group address, gmlib use only
    void *gs_priv;      // set by vpd, GS info private data (vplibGs_t), static memory
    void *entity_priv;  // set by vpd, entity private data (vplibEntityPriv_t), dynamic alloc memory
    unsigned int entity_offs; // set by gmlib, entity data offset
    unsigned int line_offs;   // set by gmlib, line data offset
    /*  
    {entityMagic(4) + nextEntityOffs(4) + sizeof(vpd_entity_t) + entityStructSize} 
    {entityMagic(4) + nextEntityOffs(4) + sizeof(vpd_entity_t) + entityStructSize} 
        :
    lineMagic(4) + nextlineOffset(4) + line_idx(4) + link_nr(4)
    + {inEntityOffset(4) + outEntityOffset(4)}  + ... + {inEntityOffset(4) + outEntityOffset(4)} 
    lineMagic(4) + nextlineOffset(4) + line_idx(4) + link_nr(4)
    + {inEntityOffset(4) + outEntityOffset(4)}  + ... + {inEntityOffset(4) + outEntityOffset(4)} 
        :
    */
}__attribute__((packed, aligned(8))) vpd_graph_t;

typedef struct {
    int len;
    int id;  //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15) 
    vpd_channel_fd_t channel_fd;  // associate with library struct gm_pollfd_t:int fd_private[4]
    vpd_entity_t entity; 
    unsigned int entity_offs; // set by gmlib, entity data offset
}__attribute__((packed, aligned(8))) vpd_update_entity_t;

typedef struct {
    int len;
    int id;  //id=chipid(bit24_31)+cpuid(bit16_23)+graphid(bit-0_15) 
    vpd_channel_fd_t channel_fd;  // associate with library struct gm_pollfd_t:int fd_private[4]
    vpd_entity_type_t type;
    int get_fd;
}__attribute__((packed, aligned(8))) vpd_get_fd_t;

typedef struct {
    int id;
    int *g_array;
}__attribute__((packed, aligned(8))) vpd_query_graph_t;

typedef struct {
    int array[VPD_GRAPH_LIST_MAX_ARRAY];              ///< graph list
} vpd_graph_list_t;

typedef struct {
    unsigned int event;
    unsigned int bs_len;
    unsigned int extra_len;
    unsigned int keyframe;  // 1: keyframe, 0: non-keyframe
} vpd_poll_ret_events_t;

typedef struct {
    void *fd;
    unsigned int event;
    vpd_poll_ret_events_t revent;
    /* fd private data */  
    vpd_channel_fd_t channel_fd;  // associate with library struct gm_pollfd_t:int fd_private[4]
} vpd_poll_obj_t;

typedef struct {
    int timeout_ms;   // -1 means forever
    unsigned int nr_poll;
    vpd_poll_obj_t *poll_objs;
} vpd_poll_t;

typedef struct {
    /* data_in and data_out entity FD */
    unsigned int fd;
} vpd_graph_poll_din_obj_t;

typedef struct {
    unsigned int va;        // kernel virtual address of this bs
    unsigned int size;      // max allowable length of bs to be copied
    unsigned int used_size; // in the end, how much memory used. should be set by user
    // NOTE: Be careful, this long long timestamp must 4-bytes alignment. for other toolchain. 
    unsigned long long timestamp __attribute__((packed, aligned(4))); 
} vpd_din_bs_t; 

typedef struct {
    /* for datain users to check and set */
    vpd_din_bs_t bs;
    /* internal use */
    void    *priv;
} vpd_din_buf_t;

typedef struct {
    unsigned int extra_va;  //put "audio 2nd bitstream" or "video mv data"
    unsigned int extra_len;
    unsigned int bs_va;     // kernel virtual address of this bs
    unsigned int bs_len;
    unsigned int new_bs;    // new bitstream flag
    unsigned int bistream_type;
    unsigned char frame_type;
    unsigned char ref_frame;
    unsigned char reserved[2];
    unsigned int checksum;
    unsigned int timestamp;// user's bs timestamp must be set to this field
    unsigned int slice_offset[3]; //multi-slice offset 1~3 (first offset is 0) 
    unsigned int reserved2;
} vpd_dout_bs_t; 

typedef struct {
    /* for dataout users to check and set */
    vpd_dout_bs_t bs;
    /* internal use */
    void    *priv; /* internal use */
} vpd_dout_buf_t;

typedef struct {
    vpd_channel_fd_t channel_fd;
    unsigned int bs_len;    // length of bitstream from AP
    char *bs_buf;           // buffer point of bitstream from AP
    unsigned int time_align_by_user; // time align value in micro-second, 0 means disable
    int timeout_ms;         // timeout in ms, -1 means no poll 
    vpd_din_buf_t dinBuf;   // data_in handle
    unsigned int reserved[8];
} vpd_put_copy_din_t;

#define VPD_DEC_USER_PRIVATE_DATA         10
typedef struct {
    // NOTE: check 8bytes align for (* vpd_din_bs_t)->timestamp
    int user_private[VPD_DEC_USER_PRIVATE_DATA];   ///< Library user data, don't use it!, 
                                                   ///< size user part of gm_dec_multi_bitstream_t 
                                                   ///<  *bindfd + *buf + length + retval = 4 (int)
    vpd_put_copy_din_t din;
} vpd_multi_din_t;

typedef struct {
    // NOTE: check 8bytes align for (* vpd_din_bs_t)->timestamp
    unsigned int nr_bs;
    int timeout_ms;
    vpd_multi_din_t *multi_bs;
} vpd_put_copy_multi_bs_t;

typedef struct {
    vpd_channel_fd_t channel_fd;
    unsigned int bs_buf_len;    // length of bs_buf, for check buf length whether it is enough. set the value by AP.
    char *bs_buf;               // buffer point of bitstream, set by AP
    unsigned int extra_buf_len; // length of mv_buf(video)/2nd_bs(audio), for check buf length whether it is enough. set the value by AP.
    char *extra_buf;            // extra_buffer point of bitstream, set by AP
    vpd_dout_buf_t dout_buf;
    /* internal use */
    int    priv[2]; /* internal use */
    unsigned int reserved[4];
} vpd_get_copy_dout_t;

#define VPD_ENC_USER_PRIVATE_DATA         27
typedef struct {
    int user_private[VPD_ENC_USER_PRIVATE_DATA];   ///< Library user data, don't use it!, 
                                                   ///< size need same with gm_enc_bitstream_t 
                                                   ///< + bindfd + retval
    vpd_get_copy_dout_t dout;
} vpd_multi_dout_t;

typedef struct {
    unsigned int nr_bs;
    vpd_multi_dout_t *multi_bs;
} vpd_get_copy_multi_bs_t;

typedef struct {
    /* must be set by user */
    vpd_graph_poll_din_obj_t which;
    unsigned char *bs_buf;
    unsigned int bs_length;
} vpd_graph_put_copy_din_t;

typedef struct {
    int which;
} __attribute__((packed, aligned(8))) vpd_env_update_t;

typedef struct {
    int bindfd;
    int entity_type;
    int fd;
    int reserved[5];    
} __attribute__((packed, aligned(8))) vpd_fd_from_bindfd_t;


//#define SEND_NOTIFY_BY_SIGNAL

#define INFO_TYPE_UNKNOWN               0
#define INFO_TYPE_PERF_LOG              1
#define INFO_TYPE_GMLIB_DBGMODE         3
#define INFO_TYPE_GMLIB_DBGLEVEL        4
#define INFO_TYPE_SIGNAL_LOSS           5
#define INFO_TYPE_SIGNAL_PRESENT        6
#define INFO_TYPE_FRAMERATE_CHANGE      7
#define INFO_TYPE_HW_CONFIG_CHANGE      8
#define INFO_TYPE_TAMPER_ALARM          9
#define INFO_TYPE_TAMPER_ALARM_RELEASE  10
#define INFO_TYPE_AU_BUF_UNDERRUN       11
#define INFO_TYPE_AU_BUF_OVERRUN        12
#define INFO_TYPE_DATA                  100

typedef struct {
    int info_type;
    unsigned long sig_jiffies;      //the time when this sig is generated
    unsigned long query_jiffies;    //the time when user get it from user space
    union {
        struct {
            unsigned int observe_time;
        } perf_log_info;
        struct {
            unsigned int dbg_mode;
        } gmlib_dbg_mode;
        struct {
            unsigned int dbg_level;   
        } gmlib_dbg_level;
        struct {
            vpd_entity_type_t entity_type;
            unsigned int gmlib_vch;
        } general_notify, signal_loss_info, signal_preset_info, framerate_change_info, hw_config_change;
        struct {
            unsigned int length;
        } gmlib_data;
    };
    int reserved[4];         ///< Reserved words
} vpd_sig_info_t;


/**************************************************************************************************/
/* ioctl */
/**************************************************************************************************/

#define VPD_IOC_MAGIC  'V' 
#define VPD_SET_GRAPH                           _IOWR(VPD_IOC_MAGIC, 1, vpd_graph_t)
#define VPD_APPLY                               _IOW(VPD_IOC_MAGIC, 2, vpd_graph_list_t)
#define VPD_POLL                                _IOWR(VPD_IOC_MAGIC, 5, vpd_poll_t)
#define VPD_GET_COPY_MULTI_DOUT                 _IOWR(VPD_IOC_MAGIC, 6, vpd_get_copy_multi_bs_t) 
#define VPD_PUT_COPY_MULTI_DIN                  _IOWR(VPD_IOC_MAGIC, 7, vpd_put_copy_multi_bs_t) 
#define VPD_UPDATE_ENTITY_SETTING               _IOWR(VPD_IOC_MAGIC, 8, vpd_update_entity_t) 

#define VPD_REQUEST_KEYFRAME                    _IOW(VPD_IOC_MAGIC, 20, vpd_channel_fd_t)
#define VPD_REQUEST_SNAPSHOT                    _IOWR(VPD_IOC_MAGIC, 21, vpd_snapshot_t)
#define VPD_REQUEST_DISP_SNAPSHOT               _IOWR(VPD_IOC_MAGIC, 22, vpd_disp_snapshot_t)
#define VPD_DECODE_KEYFRAME                     _IOW(VPD_IOC_MAGIC, 23, vpd_decode_keyframe_t)
#define VPD_REGION_RAWDATA                      _IOWR(VPD_IOC_MAGIC, 24, vpd_region_rawdata_t)
#define VPD_GET_CAP_SOURCE                      _IOWR(VPD_IOC_MAGIC, 25, vpd_cap_source_t)

#define VPD_CLEAR_WINDOW                        _IOW(VPD_IOC_MAGIC, 40, vpd_clr_win_t) 
#define VPD_ADJUST_CVBS_VISION                  _IOW(VPD_IOC_MAGIC, 41, vpd_cvbs_vision_t) 
#define VPD_CLEAR_BITSTREAM                     _IOW(VPD_IOC_MAGIC, 42, vpd_clr_win_t) 

#define VPD_GET_GMLIB_DBG_MODE                  _IOR(VPD_IOC_MAGIC, 50, vpd_gmlib_dbg_t)
#define VPD_SEND_LOGMSG                         _IOW(VPD_IOC_MAGIC, 51, vpd_send_log_t)
#define VPD_GET_PLATFORM_INFO                   _IOR(VPD_IOC_MAGIC, 52, vpd_sys_info_t) 
#define VPD_UPDATE_PLATFORM_INFO                _IOW(VPD_IOC_MAGIC, 53, vpd_sys_info_t) 
#define VPD_GET_PLATFORM_VERSION                _IOR(VPD_IOC_MAGIC, 54, vpd_version_t) 
#define VPD_GET_SIG_INFO                        _IOR(VPD_IOC_MAGIC, 55, vpd_sig_info_t) 
#define VPD_ENV_UPDATE                          _IOR(VPD_IOC_MAGIC, 56, vpd_env_update_t)
#define VPD_GET_SIG_NR                          _IOWR(VPD_IOC_MAGIC, 57, int) 
#define VPD_SET_SIG_NR                          _IOWR(VPD_IOC_MAGIC, 58, int) 

#define VPD_QUERY_GRAPH                         _IOR(VPD_IOC_MAGIC, 71, vpd_query_graph_t)
#define VPD_GET_FD                              _IOWR(VPD_IOC_MAGIC, 75, vpd_get_fd_t) 

#ifdef CONFIG_V1_IPCAM
#include "vpdrv_v1.h"
#else
#define vpd_init_v1(void)
#define vpd_release_v1(void)
#endif

#endif /* __VPDRV_H__ */

