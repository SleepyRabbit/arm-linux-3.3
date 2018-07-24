#ifndef _GM2D_LIB_H_
#define _GM2D_LIB_H_

#define GM2D_MMIOALLOC	0x1000
#define GM2D_MAGIC		0x84465203 	


#define GM2D_SCALER_BASE_SHIFT	14		

/* Driver constants */
#if 1
#define GM2D_CWORDS		4080		
#else
//#define GM2D_CWORDS		0x100		///< Length of each command list buffer in words
#define GM2D_CWORDS		512		///< Length of each command list buffer in words
#endif
#define GM2D_WAIT_IDLE		_IO('T', 1)
#define GM2D_START_BLOCK		_IO('T', 3)
#define GM2D_RESET		    _IO('T', 5) 
#define GM2D_PRINT_PHY       _IOR('T', 7, unsigned int) 

#define GM2D_TARGET_MODE		    0x00
#define GM2D_TARGET_BLEND	    0x04
#define GM2D_TARGET_BAR		    0x08
#define GM2D_TARGET_STRIDE	    0x0c
#define GM2D_TARGET_RESOLXY	    0x10
#define GM2D_DST_COLORKEY	    0x14
#define GM2D_CLIPMIN		        0x18
#define GM2D_CLIPMAX		        0x1c
#define GM2D_DRAW_CMD		    0x20
#define GM2D_DRAW_STARTXY	    0x24
#define GM2D_DRAW_ENDXY		    0x28
#define GM2D_DRAW_COLOR		    0x2c
#define GM2D_BLIT_CMD		    0x30
#define GM2D_BLIT_SRCADDR	    0x34
#define GM2D_BLIT_SRCRESOL	    0x38 
#define GM2D_BLIT_SRCSTRIDE	    0x3c 
#define GM2D_BLIT_SRCCOLORKEY	0x40 
#define GM2D_BLIT_FG_COLOR	    0x44 
#define GM2D_BLIT_DSTADDR	    0x48 
#define GM2D_BLIT_DSTYXSIZE	    0x4c 
#define GM2D_BLIT_SCALE_YFN	    0x50
#define GM2D_BLIT_SCALE_XFN	    0x54 
#define GM2D_SHADERBASE		    0x58 
#define GM2D_SHADERCTRL		    0x5c 
#define GM2D_IDREG		        0xec 
#define GM2D_CMDADDR		        0xf0 
#define GM2D_CMDSIZE		        0xf4 
#define GM2D_INTERRUPT		    0xf8 
#define GM2D_STATUS		        0xfc
#define GM2D_REGFILE_SIZE	    0x100

/* If command lists are enabled, must be used with every drawing/blitting operation */
#define GM2D_CMDLIST_WAIT	0x80000000

/* GM2D modes */
typedef enum{
    GM2D_RGBX8888, //0x00
    GM2D_RGBA8888, //0x01
    GM2D_XRGB8888, //0x02
    GM2D_ARGB8888, //0x03
    GM2D_RGBA5650, //0x04
    GM2D_RGBA5551, //0x05
    GM2D_RGBA4444, //0x06
    GM2D_RGBA0800, //0x07
    GM2D_RGBA0008, //0x08
    GM2D_L8,       //0x09
    GM2D_RGBA3320, //0x0A
    GM2D_LUT8,     //0x0B
    GM2D_BW1,      //0x0C
    GM2D_UYVY,     //0x0D
    GM2D_DUMMY_MODE
}GM2D_MODE_T;

/* GM2D scaling constants */
typedef enum{
    GM2D_NOSCALE, //0x00
    GM2D_UPSCALE, //0x01
    GM2D_DNSCALE, //0x02
    GM2D_DUMMY_SCALING
}GM2D_SCALING_T;

/* GM2D rotation constants */
typedef enum{
    GM2D_DEG000, //0x00
    GM2D_DEG090, //0x01
    GM2D_DEG180, //0x02
    GM2D_DEG270, //0x03
    GM2D_MIR_X,  //0x04
    GM2D_MIR_Y,   //0x05
    GM2D_DUMMY_ROTATION
}GM2D_ROTATION_T;

/* GM2D predefined blending modes */
#define GM2D_SRCCOPY		((DSBF_ZERO << 16) | DSBF_ONE)
#define GM2D_SRCALPHA	((DSBF_INVSRCALPHA << 16) | DSBF_SRCALPHA)

/**
 * Returns the bytes per pixel for a mode.
 *
 * @param mode Surface pixel format
 */
inline unsigned int GM2D_modesize(unsigned int mode)
{
    switch (mode)
    {
        case GM2D_RGBA0800:
        case GM2D_RGBA0008:
        case GM2D_L8:
        case GM2D_RGBA3320:
            return 1;

        case GM2D_RGBA5650:
        case GM2D_RGBA5551:
        case GM2D_RGBA4444:
            return 2;

        case GM2D_RGBX8888:
        case GM2D_RGBA8888:
        case GM2D_XRGB8888:
        case GM2D_ARGB8888:
        default:
            return 4;
    }
}

/*
 * brief Data shared among hardware, kernel driver and GM2D driver.
 */
struct gm2d_shared
{
    unsigned int hw_running;	///< Set while hardware is running
    unsigned int sw_buffer; 	///< Indicates which buffer is used by the driver to prepare the command list
    unsigned int sw_ready;		///< Number of ready commands in current software buffer

    unsigned int sw_preparing;	///< Number of words under preparation

    unsigned int num_starts;	///< Number of hardware starts (statistics)
    unsigned int num_interrupts;	///< Number of processed GM2D interrupts (statistics)
    unsigned int num_waits; 	///< Number of ioctl waits (statistics)
    unsigned int *sw_now;			///< First word in buffer under preparation
    unsigned int cbuffer[2][GM2D_CWORDS]; ///< Command list double buffer
};

typedef struct{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
}gm2d_color_t;

typedef enum {
    BLEND_UNKNOWN_MODE      = 0x00,
    BLEND_ZERO_MODE         = 0x01,
    BLEND_ONE_MODE          = 0x02,
    BLEND_SRCCOLOR_MODE     = 0x03,
    BLEND_INVSRCCOLOR_MODE  = 0x04,
    BLEND_SRCALPHA_MODE     = 0x05,
    BLEND_INVSRCALPHAR_MODE = 0x06,
    BLEND_DESTALPHA_MODE    = 0x07,
    BLEND_INVDESTALPHA_MODE = 0x08,
    BLEND_DESTCOLOR_MODE    = 0x09,
    BLEND_INVDESTCOLOR_MODE = 0x0A
}GM2D_BLEND_MODE_T;

#define GM2D_CLEAR_MODE     (BLEND_ZERO_MODE << 16 | BLEND_ZERO_MODE)
#define GM2D_SRCCOPY_MODE   (BLEND_ZERO_MODE << 16 | BLEND_ONE_MODE)
#define GM2D_SRCALPHA_MODE  (BLEND_INVSRCALPHAR_MODE << 16 | BLEND_ONE_MODE)

typedef struct{
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
}gm2d_retangle_t;

typedef struct{
    unsigned int x1;
    unsigned int y1;
    unsigned int x2;
    unsigned int y2;
}gm2d_region_t;

/* Information maintained by every user process
 */
typedef struct {
    unsigned int cmd[GM2D_CWORDS];
    unsigned int cmd_cnt;    //how many commands are active
    /* TODO semaphore */
} gm2dge_llst_t;

#define GM2D_BLIT_SRC_COLORKEY          0x01
#define GM2D_BLIT_BLEND_COLORALPHA      0x02
#define GM2D_BLIT_DST_COLORKEY          0x04
#define GM2D_BLIT_COLORIZE              0x08
#define GM2D_BLIT_BLEND_ALPHACHANNEL    0X10


#define GM2D_DRAW_DST_COLORKEY          0x01
#define GM2D_DRAW_DSBLEND               0x02

typedef struct {
    int                blt_src_fd;       /* source surface fd*/
    unsigned int       blt_src_paddr;    /* blt src physical address */
    unsigned int       blt_src_vaddr;    /* blt src virtual address */
    unsigned int       blt_src_size;     /* blt src mmap size */
    unsigned int       blt_src_width;    /* blt src width */
    unsigned int       blt_src_height;   /* blt src height */
    unsigned int       blt_src_stride;   /* blt src stride */
    GM2D_ROTATION_T blt_src_rotation; /* rotation*/
    unsigned int       blt_src_flags;    /* blt blend mode*/
}gm2d_blt_sour_t;

typedef struct {
    int fd;                            /*open /dev/gm2d fd"*/
    int lcd_fd;                        /*open /dev/fb1 fd"*/
    int width;                         /*target framebuff width*/
    int height;                        /*target framebuff height*/
    unsigned int drawing_flags;        /*drawing mode flag*/
    GM2D_BLEND_MODE_T v_dstBlend;   /*destination blend mode*/
    GM2D_BLEND_MODE_T v_srcBlend;   /*source blend mode*/
    unsigned int fb_paddr;             /* frame buffer physical address */
    unsigned int fb_vaddr;             /* frame buffer virtual address */
    gm2d_blt_sour_t src_blt;        /*Source surface struct*/
    GM2D_MODE_T bpp_type;           /*RGB565,RGB888,or ....*/
    gm2dge_llst_t *llst;            /*local allocate memory for command list*/
    void *sharemem;                    /*this memory comes from kernel through mapping*/
    void *io_mem;                      /*gm2dge io register mapping*/
    unsigned int mapped_sz;            /*sharemem mapping size*/
    unsigned int iomem_sz;             /*io_mem mapping size*/
    pthread_mutex_t  mutex;
} gm2dge_desc_t;

#define TARGET_MODE_SET            0x00000001
#define TARGET_BLEND_SET           0x00000002
#define TARGET_BADDR_SET           0x00000004
#define TARGET_STRIDE_SET          0x00000008
#define TARGET_RESOLXY_SET         0x00000010
#define TARGET_DST_COLORKEY_SET    0x00000020
#define TARGET_CLIP_MIN_SET        0x00000040
#define TARGET_CLIP_MAX_SET        0x00000080

struct gm2d_target_surface_t{
    unsigned int command;
    unsigned int target_mode;  
    unsigned int target_blend;
    unsigned int target_baddr;
    unsigned int target_pitch;
    unsigned int target_res_xy;
    unsigned int target_dstcolor_key;
    unsigned int target_clip_min;
    unsigned int target_clip_max;
};

#define GM2D_SUCCESS     0
#define GM2D_FAIL       -1
#endif
