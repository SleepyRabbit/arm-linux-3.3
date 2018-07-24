#ifndef _PORTAB_H_
#define _PORTAB_H_
/*****************************************************************************
 *  Common things
 ****************************************************************************/
#if 0
	#ifdef LINUX
		#define __int64		long long
		#define __align8 	__attribute__ ((aligned (8)))
		#define __align16 	__attribute__ ((aligned (16)))
		#define FUN_MEMSET(x,y,z)	memset(x,y,z)
		#define FUN_ABS(x)		abs(x)
	#else		/// CodeWarrior
		#define __align16 	__align(16)
		#include <string.h>
		#define FUN_MEMSET(x,y,z)	memset(x,y,z)
		#include <stdlib.h>
		#define FUN_ABS(x)		abs(x)
	#endif
#endif

	/* Debug level masks */
	#define DPRINTF_ERROR		0x00000001
	#define DPRINTF_STARTCODE	0x00000002
	#define DPRINTF_HEADER		0x00000004
	#define DPRINTF_TIMECODE	0x00000008
	#define DPRINTF_MB			0x00000010
	#define DPRINTF_COEFF		0x00000020
	#define DPRINTF_MV			0x00000040
	#define DPRINTF_DEBUG		0x80000000

	/* debug level for this library */
	#define DPRINTF_LEVEL		0

	/* Buffer size for non C99 compliant compilers (msvc) */
	#define DPRINTF_BUF_SZ  	1024

/*****************************************************************************
 *  Types used in Faraday sources
 ****************************************************************************/

/*----------------------------------------------------------------------------
 | Standard Unix include file (sorry, we put all unix into "linux" case)
 *---------------------------------------------------------------------------*/


	//#    define boolean int
	#define boolean unsigned char
	#define int8_t   signed char
	#define uint8_t  unsigned char
	#define byte     unsigned char
	#define int16_t  short
	#define uint16_t unsigned short
	#define int32_t  int
	#define uint32_t unsigned int
#if 0
	#define int64_t  long long
	#define uint64_t unsigned long long
#else
	#define int64_t  __int64
	#define uint64_t unsigned __int64
#endif
	#define bool int
//	#define NULL 0

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
	
	#define MASK1			0x00000001
	#define MASK2			0x00000003
	#define MASK3			0x00000007
	#define MASK4			0x0000000F
	#define MASK5			0x0000001F
	#define MASK6			0x0000003F
	#define MASK7			0x0000007F
	#define MASK8			0x000000FF
	#define MASK9			0x000001FF
	#define MASK10			0x000003FF
	#define MASK11			0x000007FF
	#define MASK12			0x00000FFF
	#define MASK13			0x00001FFF
	#define MASK14			0x00003FFF
	#define MASK15			0x00007FFF
	#define MASK16			0x0000FFFF
	#define MASK17			0x0001FFFF
	#define MASK18			0x0003FFFF
	#define MASK19			0x0007FFFF
	#define MASK20			0x000FFFFF
	#define MASK21			0x001FFFFF
	#define MASK22			0x003FFFFF
	#define MASK23			0x007FFFFF
	#define MASK24			0x00FFFFFF
	#define MASK25			0x01FFFFFF
	#define MASK26			0x03FFFFFF
	#define MASK27			0x07FFFFFF
	#define MASK28			0x0FFFFFFF
	#define MASK29			0x1FFFFFFF
	#define MASK30			0x3FFFFFFF
	#define MASK31			0x7FFFFFFF
	#define MASK32			0xFFFFFFFF
/*****************************************************************************
 *  Some things that are only architecture dependant
 ****************************************************************************/
	#define CACHE_LINE  16
	#define ptr_t uint32_t
	//#define BS_IS_BIG_ENDIAN
	#define ALIGN_WORD(addr) ((((addr)+3)>>2)<<2)
	typedef void *(* DMA_MALLOC_PTR_enc)(uint32_t size, uint8_t align_size,
							uint8_t reserved_size, void ** phy_ptr);
	typedef void (* DMA_FREE_PTR_enec)(void * virt_ptr, void * phy_ptr);

	typedef void *(* MALLOC_PTR_enc)(uint32_t size, uint8_t align_size,
								uint8_t reserved_size);
	typedef void (* FREE_PTR_enc)(void * virt_ptr);

	union VECTOR1
	{
		uint32_t u32num;
		struct
		{
			int16_t s16y;
			int16_t s16x;
		} vec;
	};


/*----------------------------------------------------------------------------
 | gcc SPARC specific macros/functions
 *---------------------------------------------------------------------------*/
	#define BSWAP(a) \
                ((a) = (((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | \
                       (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))


#endif
