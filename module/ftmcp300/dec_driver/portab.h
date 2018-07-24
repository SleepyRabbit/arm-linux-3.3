#ifndef _PORTAB_H_
#define _PORTAB_H_
/*****************************************************************************
 *  Common things
 ****************************************************************************/

/*****************************************************************************
 *  Types used in Faraday sources
 ****************************************************************************/

/*----------------------------------------------------------------------------
 | Standard Unix include file (sorry, we put all unix into "linux" case)
 *---------------------------------------------------------------------------*/


	//#define bool int
	//#define boolean int
	//#define boolean unsigned char
	#define int8_t   signed char
	#define uint8_t  unsigned char
	#define byte     unsigned char
	#define int16_t  short
	#define uint16_t unsigned short
	#define int32_t  int
	#define uint32_t unsigned int
#if 1
	#define int64_t  long long
	#define uint64_t unsigned long long
#else
	#define int64_t  __int64
	#define uint64_t unsigned __int64
#endif

	#define BIT0			0x01
	#define BIT1			0x02
	#define BIT2			0x04
	#define BIT3			0x08
	#define BIT4			0x10
	#define BIT5			0x20
	#define BIT6			0x40
	#define BIT7			0x80
	#define BIT8			0x0100
	#define BIT9			0x0200
	#define BIT10			0x0400
	#define BIT11			0x0800
	#define BIT12			0x1000
	#define BIT13			0x2000
	#define BIT14			0x4000
	#define BIT15			0x8000
	#define BIT16			0x010000
	#define BIT17			0x020000
	#define BIT18			0x040000
	#define BIT19			0x080000
	#define BIT20			0x100000
	#define BIT21			0x200000
	#define BIT22			0x400000
	#define BIT23			0x800000
	#define BIT24			0x01000000
	#define BIT25			0x02000000
	#define BIT26			0x04000000
	#define BIT27			0x08000000
	#define BIT28			0x10000000
	#define BIT29			0x20000000
	#define BIT30			0x40000000
	#define BIT31			0x80000000
/*****************************************************************************
 *  Some things that are only architecture dependant
 ****************************************************************************/
	//#define CACHE_LINE  16
	//#define ptr_t uint32_t
	//#define BS_IS_BIG_ENDIAN


/*****************************************************************************
 *  Types in MCP300 driver
 ****************************************************************************/
	
	typedef void *(* DMA_MALLOC_PTR_dec)(uint32_t size, uint8_t align_size,
							uint8_t reserved_size, void ** phy_ptr);
	typedef void (* DMA_FREE_PTR_dec)(void * virt_ptr, void * phy_ptr);
    typedef void *(* MALLOC_PTR_dec)(uint32_t size);
	typedef void (* FREE_PTR_dec)(void * virt_ptr);
	typedef void (* DAMNIT_PTR_dec)(char * str);
    typedef void (* MARK_ENGINE_START_PTR_dec)(int engine);
    
    typedef struct decoder_parameter_t DecoderParams;
    typedef struct video_parameter_t VideoParams;
    typedef struct seq_parameter_set_rbsp_t SeqParameterSet;
    typedef struct pic_parameter_set_rbsp_t PicParameterSet;
    typedef struct decoded_picture_buffer_t DecodedPictureBuffer;
    typedef struct storable_picture_t StorablePicture;
    typedef struct DecRefPicMarking_s DecRefPicMarking;


#endif
