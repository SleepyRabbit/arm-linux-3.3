#ifndef _GM2D_OSG_IF_H_
#define _GM2D_OSG_IF_H_

typedef struct __gm2d_osg_blit{
    unsigned int       dst_paddr;
    unsigned short     dst_w;
    unsigned short     dst_h;
    unsigned int       src_paddr;
    unsigned short     src_w;
    unsigned short     src_h;
    unsigned short     dx;
    unsigned short     dy;
    unsigned int       src_colorkey;
}gm2d_osg_blit_t;

typedef struct __gm2d_osg_mask{
    unsigned int       dst_paddr;
    unsigned short     dst_w;
    unsigned short     dst_h;
    unsigned short     dx1;
    unsigned short     dy1;
    unsigned short     dx2;
    unsigned short     dy2;
    unsigned int       color;
    unsigned int       type;
    unsigned int       border_sz;
}gm2d_osg_mask_t;

typedef enum {
    GM2D_0SG_BORDER_TURE   = 1,
    GM2D_0SG_BORDER_HORROW = 2   
} GM2D_OSG_BORDER_TP;

#if defined(CONFIG_PLATFORM_GM8136)

typedef struct __gm2d_lcd_blit{
    unsigned int       dst_bg_paddr;/*dst bg start address*/
	unsigned short     dst_bg_w;    /*dst bg width*/
	unsigned short     dst_bg_h;    /*dst bg height*/
	unsigned short     dst_off_x;    /*dst blit start x offset by bg*/
    unsigned short     dst_off_y;    /*dst blit start y offset by bg*/
    unsigned short     dst_off_w;    /*dst blit width*/
    unsigned short     dst_off_h;    /*dst blit height*/
	unsigned short     dst_dx;      /*dst blit x */
    unsigned short     dst_dy;      /*dst blit y*/
    unsigned int       src_bg_paddr; /*src bg start address*/
	unsigned short     src_bg_w;     /*src bg width*/
	unsigned short     src_bg_h;     /*src bg height*/
	unsigned short     src_clp_x;     /*src clip start x*/
    unsigned short     src_clp_y;     /*src clip start y*/
    unsigned short     src_clp_w;     /*src clip width*/
    unsigned short     src_clp_h;     /*src clip height*/
}gm2d_lcd_blit_t;

int gm2d_lcd_do_blit(gm2d_lcd_blit_t* blit_pam,int fire);

#endif

int gm2d_osg_do_blit(gm2d_osg_blit_t* blit_pam,int fire);

int gm2d_osg_do_mask(gm2d_osg_mask_t* mask_pam,int fire);

int gm2d_osg_remainder_fire(void);

#endif
