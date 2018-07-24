#ifndef __GM2D_KERNELMODULE_H__
#define __GM2D_KERNELMODULE_H__

#define GM2D_MMIOALLOC	0x1000
#define GM2D_MAGIC		0x84465203 	
#define GM2D_SCALER_BASE_SHIFT	14		///< Scaler fixed point arithmetic precision

// Driver constants
#define GM2D_CWORDS		4080		///< Length of each command list buffer in words
#define GM2D_WAIT_IDLE		_IO('T', 1)
#define GM2D_START_BLOCK	_IO('T', 3)
#define TSI_ACCELERATOR		0x54736930


// Think2D register file
#define GM2D_TARGET_MODE	0x00
#define GM2D_TARGET_BLEND	0x04
#define GM2D_TARGET_BAR		0x08
#define GM2D_TARGET_STRIDE	0x0c
#define GM2D_TARGET_RESOLXY	0x10
#define GM2D_DST_COLORKEY	0x14
#define GM2D_CLIPMIN		0x18
#define GM2D_CLIPMAX		0x1c
#define GM2D_DRAW_CMD		0x20
#define GM2D_DRAW_STARTXY	0x24
#define GM2D_DRAW_ENDXY		0x28
#define GM2D_DRAW_COLOR		0x2c
#define GM2D_BLIT_CMD		0x30
#define GM2D_BLIT_SRCADDR	0x34
#define GM2D_BLIT_SRCRESOL	0x38 
#define GM2D_BLIT_SRCSTRIDE	0x3c 
#define GM2D_BLIT_SRCCOLORKEY	0x40 
#define GM2D_BLIT_FG_COLOR	    0x44 
#define GM2D_BLIT_DSTADDR	    0x48 
#define GM2D_BLIT_DSTYXSIZE	0x4c 
#define GM2D_BLIT_SCALE_YFN	0x50
#define GM2D_BLIT_SCALE_XFN	0x54 
#define GM2D_SHADERBASE		0x58 
#define GM2D_SHADERCTRL		0x5c 
#define GM2D_IDREG		0xec 
#define GM2D_CMDADDR		0xf0 
#define GM2D_CMDSIZE		0xf4 
#define GM2D_INTERRUPT		0xf8 
#define GM2D_STATUS		0xfc
#define GM2D_REGFILE_SIZE	0x100

// If command lists are enabled, must be used with every drawing/blitting operation
#define GM2D_CMDLIST_WAIT	0x80000000

// Think2D modes
#define GM2D_RGBX8888		0x00
#define GM2D_RGBA8888		0x01
#define GM2D_XRGB8888		0x02
#define GM2D_ARGB8888		0x03
#define GM2D_RGBA5650		0x04
#define GM2D_RGBA5551		0x05
#define GM2D_RGBA4444		0x06
#define GM2D_RGBA0800		0x07
#define GM2D_RGBA0008		0x08
#define GM2D_L8			    0x09
#define GM2D_RGBA3320		0x0A
#define GM2D_LUT8		    0x0B
#define GM2D_BW1			0x0C
#define GM2D_UYVY		    0x0D

// Think2D scaling constants
#define GM2D_NOSCALE		0x00
#define GM2D_UPSCALE		0x01
#define GM2D_DNSCALE		0x02

// Think2D rotation constants
#define GM2D_DEG000		0x00
#define GM2D_DEG090		0x01
#define GM2D_DEG180		0x02
#define GM2D_DEG270		0x03
#define GM2D_MIR_X		0x04
#define GM2D_MIR_Y		0x05

// Think2D predefined blending modes
#define GM2D_SRCCOPY		((DSBF_ZERO << 16) | DSBF_ONE)
#define GM2D_SRCALPHA		((DSBF_INVSRCALPHA << 16) | DSBF_SRCALPHA)

/**
 * \brief Data shared among hardware, kernel driver and Think2D driver.
 */
struct gm2d_shared
{
    unsigned int hw_running;	///< Set while hardware is running
	unsigned int sw_buffer; 	///< Indicates which buffer is used by the driver to prepare the command list
	unsigned int sw_ready;		///< Number of ready commands in current software buffer

	unsigned int sw_preparing;	///< Number of words under preparation

	unsigned int num_starts;	///< Number of hardware starts (statistics)
	unsigned int num_interrupts;	///< Number of processed Think2D interrupts (statistics)
	unsigned int num_waits; 	///< Number of ioctl waits (statistics)
	unsigned int *sw_now;			///< First word in buffer under preparation
	unsigned int cbuffer[2][GM2D_CWORDS]; ///< Command list double buffer
};

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

#endif
