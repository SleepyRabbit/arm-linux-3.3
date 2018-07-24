#ifndef _FAVC_DEV_H_
#define _FAVC_DEV_H_

#include "platform.h"

#define RESOLUTION_4096x2160
#ifdef RESOLUTION_2592x1944
	#define MAX_DEFAULT_WIDTH  2592
	#define MAX_DEFAULT_HEIGHT 1944
#elif defined(RESOLUTION_1600x1200)
	#define MAX_DEFAULT_WIDTH  1600
	#define MAX_DEFAULT_HEIGHT 1200
#elif defined(RESOLUTION_720x576)
	#define MAX_DEFAULT_WIDTH   720
	#define MAX_DEFAULT_HEIGHT  576
#elif defined(RESOLUTION_4096x2160)
    #define MAX_DEFAULT_WIDTH  4096
    #define MAX_DEFAULT_HEIGHT 2160
#else
    #warning No resolution is defined, use 4096x2160
    #define MAX_DEFAULT_WIDTH  4096
    #define MAX_DEFAULT_HEIGHT 2160
#endif

/*************************************************/


enum buf_type_flags {
    FR_BUF_MBINFO = 0x1,
    FR_BUF_SCALE  = 0x2,
};


typedef struct mcp300_buffer_info2_t
{
    unsigned int ref_va;
    unsigned int ref_pa;
    unsigned int mbinfo_va;
    unsigned int mbinfo_pa;
    unsigned int scale_va;
    unsigned int scale_pa;
} MCP300_Buffer;

void *fkmalloc(size_t size);
void fkfree(void *mem_ptr);
int allocate_dec_buffer(unsigned int *addr_virt, unsigned int *addr_phy, unsigned int size);
void release_dec_buffer(unsigned int addr_virt, unsigned int addr_phy);
int release_frame_buffer(MCP300_Buffer *pBuf);
int allocate_frame_buffer(MCP300_Buffer *pBuf, unsigned int Y_sz, enum buf_type_flags buffer_flag);

#if 1
#define F_DEBUG(fmt, args...) printk("error: " fmt, ## args)
#else
#define F_DEBUG(a...)
#endif

#if 0
#define C_DEBUG(fmt, args...) printk("FMCP: " fmt, ## args)
#else
#define C_DEBUG(fmt, args...) 
#endif


#endif // _FAVC_DEV_H_

