#ifndef __GM2D_GFX_H__
#define __GM2D_GFX_H__

/*
 * A rectangle specified by a point and a dimension.
 */
typedef struct {
     int            x;   /* X coordinate of its top-left point */
     int            y;   /* Y coordinate of its top-left point */
     int            w;   /* width of it */
     int            h;   /* height of it */
} GM2DRectangle;

/*
 * A region specified by two points.
 *
 * The defined region includes both endpoints.
 */
typedef struct {
     int            x1;  /* X coordinate of top-left point */
     int            y1;  /* Y coordinate of top-left point */
     int            x2;  /* X coordinate of lower-right point */
     int            y2;  /* Y coordinate of lower-right point */
} GM2DRegion;

typedef enum {
     GM2D_FILLINGRECT           = 0x00000001,/*GM2D ,GM2D*/
     GM2D_DRAWLINE              = 0x00000002,/*GM2D ,GM2D*/
     GM2D_DRAWRECT              = 0x00000004,/*GM2D ,GM2D*/
     GM2D_BLITTING              = 0x00000008,/*GM2D ,GM2D*/
     GM2D_STRETCHBLITTING       = 0x00000010,/*GM2D*/
} GM2D_AccelFlags_T;

typedef enum {
     GM2D_CLIPPING_FUNC           = 0x00000001,
     GM2D_ROP_FUNC                = 0x00000002,
     GM2D_POTERDUFF_FUNC          = 0x00000004,
     GM2D_SOURCEALPHA_FUNC        = 0x00000008,
     GM2D_DIFFSURFACE_TRANS_FUNC  = 0x00000010,
     GM2D_DRAWING_FUNC            = 0x00000020,
     GM2D_BLITTER_FUNC            = 0x00000040,
} GM2D_CapabilitiesFlags_T;
typedef enum {
     GM2D_COLOR_SET             = 0x00000001, /*GM2D ,GM2D*/
     GM2D_COLORBLEND_SET        = 0x00000002, /*GM2D*/
     GM2D_FG_COLORIZE_SET       = 0x00000004, /*GM2D*/
     GM2D_DSTINATION_SET        = 0x00000008, /*GM2D ,GM2D*/
     GM2D_SOURCE_SET            = 0x00000008, /*GM2D ,GM2D*/
     GM2D_CLIP_SET              = 0x00000010, /*GM2D*/
     GM2D_DEG090_SET            = 0x00000020, /*GM2D*/
     GM2D_DEG180_SET            = 0x00000040, /*GM2D*/
     GM2D_DEG270_SET            = 0x00000080, /*GM2D*/
     GM2D_FLIP_HORIZONTAL_SET   = 0x00000100, /*GM2D*/
     GM2D_FLIP_VERTICAL_SET     = 0x00000200, /*GM2D*/
     GM2D_SRC_COLORKEY_SET      = 0x00000400, /*GM2D*/
     GM2D_DST_COLORKEY_SET      = 0x00000800, /*GM2D*/
     GM2D_LINE_STYLE_SET        = 0x00000400, /*GM2D*/
     GM2D_ROP_METHOD_SET        = 0x00000800, /*GM2D*/
     GM2D_FORCE_FG_ALPHA        = 0x00001000, /*GM2D*/
} GM2D_SettingFlags_T;

typedef enum {
	GM2D_RGB_5650  ,	//16bits, DSPF_RGB16
	GM2D_ARGB_1555 ,	//16bits, DSPF_ARGB1555
	GM2D_RGBA_5551 ,	//16bits, DSPF_ARGB1555
	GM2D_RGBA_4444  ,
	GM2D_RGB_888   ,	//32bits, DSPF_RGB32
	GM2D_ARGB_8888 ,	//32bits, DSPF_ARGB
	GM2D_RGBA_8888 , 	//32bits, DSPF_ARGB
	GM2D_XRGB_8888 	//32bits, DSPF_ARGB
} GM2D_BPP_T;

typedef struct _GM2D_GFX_Setting_Data{
    unsigned int   color;         /*for GM2D_COLOR_SET , GM2D_FG_COLORIZE_SET*/
    unsigned short src_blend;     /*for GM2D_COLORBLEND_SET*/
    unsigned short dst_blend;     /*for GM2D_COLORBLEND_SET*/
    GM2DRegion     clip;          /*GM2D_CLIP_SET*/
    unsigned int   src_color;     /*GM2D_SRC_COLORKEY_SET*/
    unsigned int   dst_color;     /*GM2D_DST_COLORKEY_SET*/
    unsigned int   line_style;    /*GM2D_LINE_STYLE_SET*/
    unsigned int   rop_method ;   /*GM2D_FORCE_FG_ALPHA*/
}GM2D_GFX_Setting_Data;

typedef struct _GM2D_GraphicsDeviceFuncs {

    void (*EngineReset)(void *device );  
    /*
     * emit any buffered commands, i.e. trigger processing
     */
    int (*EmitCommands)  ( void *device );    /*GM2D ,GM2D*/
     /*
      * Check if the function 'accel' can be accelerated and parameter can be set.
      */
    int (*CheckState)( void *device);         /*GM2D ,GM2D*/

     /*
      * This function will follow device->accel_do to set parameter and value.  
      */
    int (*SetState)  ( void *device);         /*GM2D ,GM2D*/
    int (*GetSupportFeature)    ( void *device );
    /*
     * drawing functions
     */
    int (*FillRectangle) ( void *device,GM2DRectangle *rect );  /*GM2D ,GM2D*/

    int (*DrawRectangle) ( void *device,GM2DRectangle *rect );  /*GM2D ,GM2D*/

    int (*DrawLine)      ( void *device,GM2DRegion *line );     /*GM2D ,GM2D*/ 
    
    /*
     * blitting functions
     */
    int (*Blit)         ( void *device , GM2DRectangle *rect, int dx, int dy );  /*GM2D ,GM2D*/

 
    int (*StretchBlit)  ( void *device , GM2DRectangle *srect, GM2DRectangle *drect );  /*GM2D*/         

}GM2D_GraphicsDeviceFuncs;

typedef struct __GM2D_Surface_Data{
    unsigned int surface_vaddr;  /* surface virtual address */
    unsigned int surface_paddr;  /* surface physical address */
    int width;                   /* surface width */
    int height;                  /* surface height */
    GM2D_BPP_T bpp;              /* surface format */
}GM2D_Surface_Data;

typedef struct __GM2D_GfxDrv_Data{
    int gfx_fd;         /*2D engine fd*/
    int fb_fd ;         /*frame buffer engine fd*/
    void * gfx_shared;  /*driver mode command memory*/
    int    gfx_map_sz;  /*driver mode command memory size*/
    void * gfx_local;   /*user mode local command memory*/
    volatile unsigned int  *mmio_base; /*driver io mapping base*/
    GM2D_Surface_Data  fb_data;         /*frame buffer surface data*/ 
    GM2D_Surface_Data  target_sur_data; /*target surface data*/ 
    GM2D_Surface_Data  source_sur_data; /*source surface data*/   
 
}GM2D_GfxDrv_Data;

typedef struct {
     GM2D_CapabilitiesFlags_T   flags;
     GM2D_AccelFlags_T          accel_func;
} GM2D_Capabilities;

typedef struct __GM2D_GfxDevice {
     GM2D_GfxDrv_Data          drv_data;    /*driver data*/ 
     GM2D_Capabilities         caps;        /*will show the 2D engine capability */
     GM2D_GraphicsDeviceFuncs  funcs;       /*2D engine support API*/
     GM2D_AccelFlags_T         accel_do;    /*Tell 2D engine what kind of function need to operate*/
     GM2D_SettingFlags_T       accel_param; /*Tell 2D engine what kind of function parameter need to operate*/
     GM2D_GFX_Setting_Data     accel_setting_data; /*Tell 2D engine function parameter value to set*/   
}GM2D_GfxDevice;

#define GM2D_SUCCESS                0
#define GM2D_FAIL                  -1      

/* line style for ft2d only*/
typedef enum {
     GM2D_STYLE_SOLID             = 1,
     GM2D_STYLE_DASH              ,
     GM2D_STYLE_DOT               ,
     GM2D_STYLE_DASH_DOT          ,
     GM2D_STYLE_DASH_DOT_DOT      ,
     GM2D_INV_STYLE_DASH              ,
     GM2D_INV_STYLE_DOT               ,
     GM2D_INV_STYLE_DASH_DOT          ,
     GM2D_INV_STYLE_DASH_DOT_DOT      ,
     GM2D_MAX_LINESTYLE
} GM2D_LINESTYLE_T;


typedef enum {
     GM2D_ROP_BALCKNESS             = 1,
     GM2D_ROP_NOTSRCERASE              ,
     GM2D_ROP_NOTSRCCOPY               ,
     GM2D_ROP_SRCERASE                 ,
     GM2D_ROP_DSTINVERT                ,
     GM2D_ROP_PATINVERT              ,
     GM2D_ROP_SRCINVERT               ,
     GM2D_ROP_SRCAND          ,
     GM2D_ROP_MERGEPAINT      ,
     GM2D_ROP_MERGECOPY       ,
     GM2D_ROP_SRCCOPY         ,
     GM2D_ROP_SRCPAINT        ,
     GM2D_ROP_PATCOPY         ,
     GM2D_ROP_PATPAINT        , 
     GM2D_ROP_WHITENESS       ,
     GM2D_ROP_MAX
} GM2D_ROPMETHOD_T;

typedef enum {
    GM2D_GM2D_BLEND_UNKNOWN_MODE      = 0x00,
    GM2D_GM2D_BLEND_ZERO_MODE         = 0x01,
    GM2D_GM2D_BLEND_ONE_MODE          = 0x02,
    GM2D_GM2D_BLEND_SRCCOLOR_MODE     = 0x03,
    GM2D_GM2D_BLEND_INVSRCCOLOR_MODE  = 0x04,
    GM2D_GM2D_BLEND_SRCALPHA_MODE     = 0x05,
    GM2D_GM2D_BLEND_INVSRCALPHAR_MODE = 0x06,
    GM2D_GM2D_BLEND_DESTALPHA_MODE    = 0x07,
    GM2D_GM2D_BLEND_INVDESTALPHA_MODE = 0x08,
    GM2D_GM2D_BLEND_DESTCOLOR_MODE    = 0x09,
    GM2D_GM2D_BLEND_INVDESTCOLOR_MODE = 0x0A
}GM2D_GM2D_BLEND_MODE_T;

/*function definition*/
int driver_init_device( GM2D_GfxDevice           *device,
                        GM2D_GraphicsDeviceFuncs *funcs,
                        GM2D_BPP_T rgb_t );
                        
void driver_close_device(GM2D_GfxDevice   *device); 
#endif