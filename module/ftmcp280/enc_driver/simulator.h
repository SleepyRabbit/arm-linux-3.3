#ifndef __SIMULATOR_H__
	#define __SIMULATOR_H__
	#include "portab.h"


	#define DEBUG 0		// 0:no debug output
									// 1:Generate coeff.bin --> cost 100cycle/MB
									// 2:Generate iq.bin --> cost 100cycle/MB
									// 3:Generate coeff.bin & iq.bin --> cost 100cycle/MB
									// 4:Generate coeff.bin, iq.bin & mb.txt --> cost 27000cycle/MB

	#define SIMD_LOG
	#define DMA_LOG

	#ifdef RTL
	#undef DEBUG
	#define DEBUG 0
	#endif

	#if (DEBUG == 0)
	#undef SIMD_LOG
	#undef DMA_LOG
	#endif

	#if (DEBUG == 0)
		#define DPRINT_mb(...) 		// define to nothing in release mode
		#define DWRITE_coeff(...) 	// define to nothing in release mode
		#define DWRITE_iq(...) 			// define to nothing in release mode
		#define DPRINT_v3(...) 			// define to nothing in release mode
	#elif (DEBUG == 1)
		#define DPRINT_mb(...)
		#define DWRITE_coeff(...) vpeWrite(file_coeff, __VA_ARGS__)
		#define DWRITE_iq(...) 
		#define DPRINT_v3(...)
	#elif (DEBUG == 2)
		#define DPRINT_mb(...)
		#define DWRITE_coeff(...)
		#define DWRITE_iq(...) vpeWrite(file_iq, __VA_ARGS__)
		#define DPRINT_v3(...)
	#elif (DEBUG == 3)
		#define DPRINT_mb(...)
		#define DWRITE_coeff(...) vpeWrite(file_coeff, __VA_ARGS__)
		#define DWRITE_iq(...) vpeWrite(file_iq, __VA_ARGS__)
		#define DPRINT_v3(...)
	#elif (DEBUG >= 4)
		#define DPRINT_mb(...) 	vpePrintf(file_mb, __VA_ARGS__)
		#define DWRITE_coeff(...) vpeWrite(file_coeff, __VA_ARGS__)
		#define DWRITE_iq(...) vpeWrite(file_iq, __VA_ARGS__)
		#define DPRINT_v3(...) 		my_fprint( __VA_ARGS__)
	#endif

	#ifdef SIMD_LOG
		#define DWRITE_simd(...) vpeWrite(file_simd, __VA_ARGS__)
	#else
		#define DWRITE_simd(...)
	#endif
	#ifdef DMA_LOG
		#define DWRITE_dma(...) vpeWrite(file_dma, __VA_ARGS__)
	#else
		#define DWRITE_dma(...)
	#endif

	// do not use this function.
	extern void my_fprints(int handler, const char *fmt, ...);

	// the following for standard out (print to console)
	#ifdef RTL
	#define my_fprint(...)
	#else
	#define my_fprint(...) my_fprints(1, __VA_ARGS__)
	#endif
	// the followings for file operation
	#define vpePrintf(h, ...) my_fprints(h, __VA_ARGS__)
	extern int vpeOpen(char *filename, char * mode);
	extern int vpeRead(int handler, void * buf, uint32_t size, int swap_endian);
	extern int vpeWrite(int handler, void * buf, uint32_t size);
	extern void vpeClose(int handler);

	//extern int OpenReadBS(void * buf, int size);
	//extern int OpenFrameFile(void);
	extern int LoadFrame(void *buf, int size);
	//extern int OpenReadBS(void * buf, int size);
	//extern int OpenReadBS2(void * buf);
	//extern int LoadImage(void *buf, uint32_t size);
	//extern int ReloadBS(uint32_t bit_buf_cnt);
	extern int OpenSrcGld(void);
	//extern int output_error_msg(void);
	extern int CompareGolden(void * buf, int byte_size);
	extern int SaveBuffer(char *filename, void *buf, int byte_size);
	//extern int OpenConfigure(void);
	extern int LoadConfigure(void *buf, int word_num);
	extern void vpePass(void);
	extern void vpeFail(void);
	extern void vpeFinish(void);
	extern void vpeSet(uint32_t val);
	//extern int vpeComp(uint8_t * buffer, uint8_t * bufferg, int size);
	#ifdef SIMULATOR_GOLBAL
		#define SIMU_EXT		// nother
	#else
		#define SIMU_EXT		extern
	#endif

	#if !(defined(HOST) && defined(RISC0))
		SIMU_EXT int file_mb;
		SIMU_EXT int file_coeff;
	#endif

	#ifdef RISC0
		SIMU_EXT int file_iq;
		#ifdef SIMD_LOG
			SIMU_EXT int file_simd;
		#endif
		#ifdef DMA_LOG
			SIMU_EXT int file_dma;
		#endif
	#endif
#endif /*__SIMULATOR_H__*/
