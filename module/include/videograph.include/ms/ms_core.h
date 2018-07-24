/**
 * @file ms_core.h
 *  ms core header
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.6 $
 * $Date: 2016/05/26 02:21:14 $
 *
 * ChangeLog:
 *  $Log: ms_core.h,v $
 *  Revision 1.6  2016/05/26 02:21:14  zhilong
 *   Add ms_fixed_buf_cancel_wait api
 *
 *  Revision 1.5  2016/05/23 08:14:13  ivan
 *  add gs ver_no to ms layout change and notify
 *
 *  Revision 1.4  2016/04/12 01:31:26  ivan
 *  update to add header comment
 *
 *
 */
#ifndef __MS_CORE_H__
#define __MS_CORE_H__
#ifndef WIN32
    #include <linux/types.h>
    #include <linux/version.h>
    #include <linux/wait.h>
    #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
        #include <linux/kernel.h>
    #else
        #include <linux/printk.h>
    #endif
#else // define WIN32
    #include "linux_fnc.h"
#endif

#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,14))
#include <frammap/frammap.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
#include <frammap/frammap_if.h>
#endif
#define MS_MAX_DDR                          DDR_ID_MAX //set max ddr number from frammap
#define MS_MAX_NAME_LENGTH                  64
/* address alignment of all buf must be at 1K boundary */
#define MS_BUF_ALIGNMENT                    256
#define MS_BUF_ALIGNMENT_MASK               (~(MS_BUF_ALIGNMENT - 1))
typedef unsigned long long user_token_t;
#define MS_USER_TOKEN(job_id,buf_id) \
    (user_token_t)((((unsigned long long)job_id)<<32)|((unsigned long long)buf_id))
#define MS_TOKEN_JOB_ID(user_token)  (unsigned int)(((user_token)>>32)&0xFFFFFFFF)
#define MS_TOKEN_BUF_ID(user_token) (unsigned int)((user_token)&0xFFFFFFFF)
#define MS_TOKEN_IS_EMPTY(user_token) ((user_token)==0)
#define MS_GET_ADDR(header)     ((void *)(*(unsigned int *)header))
#define MS_GET_DDR_ID(header)   (*(unsigned int *)((unsigned int )header + sizeof(unsigned int)))
#define MS_FAKED_VA (0xf0f0f0f0)
enum MS_FREE_TAG {
    MS_FROM_GS = (1 << 0), 
    MS_FROM_EM = (1 << 1)
};

typedef enum MS_CHECK_FIXED_BUF_RESULT {
    MS_OK = 1, 
    MS_LARGE_FAIL = -1,
    MS_SMALL_FAIL = -2
}ms_check_result_t;

/** 
 * buffer manager family API 
 */
int ms_mgr_create(void);
void ms_mgr_destroy(void);
/** 
 * buffer sea family API 
 */
int ms_sea_create(int ddr_no, u32 size);
int ms_sea_destroy(int ddr_no);
u32 ms_sea_size(int ddr_no);
/** 
 * video graph buffer family API 
 */
struct ms_pool;
typedef struct ms_pool ms_pool_t;

enum MS_BUF_TYPE {
    MS_FIXED_BUF = 0x78, MS_VARLEN_BUF = 0x87,
};

enum MS_ALLOC_CXT {
    MS_ALLOC_FIXED = 0x00000001, MS_ALLOC_VARLEN = 0x00000002,
};

typedef struct {
    const char *name;
    int ddr_no;
    enum MS_BUF_TYPE type;
    u32 size;
}
ms_pool_attribute_t;

#define FLAG_ALGO_MODE_PASS         (1 << 0)
#define FLAG_INIT_PATTERN_BLACK     (1 << 1)
#define FLAG_FORBIDED_FREE_POOL     (1 << 2)
#define FLAG_CANFROM_FREE_POOL      (1 << 3)
#define FLAG_PASS_CHECK_BID         (1 << 4)
#define FLAG_ALLOC_VIR_ADDR         (1 << 5)

int ms_pool_create(ms_pool_t **pool, const ms_pool_attribute_t *attr, int flag);
int ms_hybrid_pool_create(int ms_pool_point_array[], int pool_nr, ms_pool_attribute_t *attr_array, int flag_array[]);

int ms_pool_destroy(ms_pool_t **pool);

typedef struct {
    u32 size;
    u32 nr;
} ms_fixed_buf_des_t;

typedef struct {
    ms_fixed_buf_des_t *fbds;
} ms_fixed_pool_layout_t;

typedef struct {
    /* total size of slot */
    u32 size;
    /* max size to alloc onc varlen buffer. Surely, this should be less than size */
    u32 window_size;
    /* valid slot_id must be all different, can ranging from 0 ~ (4G-1)*/
    u32 slot_id;
} ms_varlen_buf_des_t;

typedef struct {
    ms_varlen_buf_des_t *vbds;
} ms_varlen_pool_layout_t;

typedef struct {
    int ver;
    int ddr_id;
    char  user[40];
	unsigned int buf_header;   	
    unsigned int  size;
    unsigned int  time;
} ms_layout_fragment_t;

typedef struct {
    ms_pool_t * layout_pool;
    void *layout_attr;
} ms_hybrid_pool_info_t;

typedef void (*ms_buf_addr_update)(void *priv, void *new_addr);
int ms_fixed_pool_layout_change(ms_pool_t *pool, ms_fixed_pool_layout_t *layout, int ver);
void *ms_fixed_buf_alloc(ms_pool_t *pool, u32 size, 
	u8 *need_realloc, const user_token_t user, char *name, int can_from_free);
int ms_layout_is_ready(ms_pool_t *pool);
int ms_AllLayout_is_ready(void);
void ms_notify_fromGs(ms_pool_t *ms_pool, int ver);

u32 ms_fix_buf_get_usenr(ms_pool_t *pool, u32 size);
ms_fixed_pool_layout_t *ms_fix_buf_container_of_pool(u32 addr);
/* return the user's private data, user can not store 0 to it, 
0 is used by ms internally. */
int ms_fixed_buf_cancel_wait(ms_pool_t *ms_pool, const user_token_t user);
int ms_fixed_buf_start_to_wait(ms_pool_t *ms_pool, unsigned int size, ms_pool_t *ratio_pool,
                               unsigned int ratio_size,void *priv, const user_token_t user, void *cb);
int ms_varlen_pool_layout_change(ms_pool_t *pool, const ms_varlen_pool_layout_t *layout, int ver);

void *ms_varlen_buf_alloc(ms_pool_t *pool, u32 slot_id, u32 size,
	u8 *need_realloc, const user_token_t user, int can_from_free);

int ms_varlen_get_usedbuf_counter(ms_pool_t *pool, u32 slot_id);
int ms_varlen_buf_complete(ms_pool_t *pool, u32 slot_id, void *buf, u32 used_size);
int ms_reserve_by_user(void *buf);
int ms_buf_is_used(void *buf);
int ms_buf_free(void *buf, enum MS_FREE_TAG free_tag);
int ms_get_fixed_pool_usage(ms_pool_t *pool, ms_layout_fragment_t *pool_usage, int nr);
int ms_check_job_buf_id(ms_pool_t *ms_pool, unsigned int max_bid,  unsigned int min_bid);
int ms_get_fixedpoolinfo_by_name(char *ms_pool_name, unsigned int *pool_pa_start,
                            unsigned int *pool_va_start, unsigned int *pool_size);
/**
 * for those users who need very fast structure allocation. 
 * NOTE: size can't be larger than PAGE_SIZE (usually 4KB)
 */
void *ms_small_fast_alloc_timeout(u32 size, u32 timeout_msec);
void *ms_small_fast_alloc(u32 size);
int ms_small_fast_free(void *buf);

/* for debug use */
/* va to pa, return corrosponding pa which is related to va */
void *ms_phy_address(void *va);
u32 ms_which_buffer_sea(u32 pa);
u32 ms_offset_in_buffer_sea(u32 pa);
u32 ms_buffer_sea_pa_base(u8 ddr_no);
u32 ms_buffer_sea_size(u8 ddr_no);
ms_pool_t * ms_get_pool_by_name(char *pool_name);
int ms_hybrid_pool_layout_change(ms_hybrid_pool_info_t ms_pool_array[], int ms_pool_nr, int ver);
unsigned int ms_set_buf_time(unsigned int buf, unsigned int time_stamp);
void *ms_zalloc(unsigned int size);
void  ms_free(void *p_buf);
void *ms_cache_create(int size, int reserve_count, int can_extend, char *name);
void  ms_cache_config(void *p_gm_cache, int threshold_percent, int extend_percent);
int   ms_cache_destroy(void *p_gm_cache);
void *ms_cache_alloc(void *p_gm_cache);
int   ms_cache_free(void *p_gm_cache, void *buffer);
void* ms_cache_alloc_with_key(void *p_ms_cache, unsigned long long key, int *access_cnt,
                              void (*fn_constructor)(void *user_buffer));
int  ms_cache_free_with_key(void *p_ms_cache, void *buffer, void (*fn_destructor)(void *user_buffer));
ms_check_result_t ms_check_fixed_buf(ms_pool_t *ms_pool, unsigned int size1, ms_pool_t *ms_pool2, unsigned int size2,
                         const user_token_t user, char *name);
ms_check_result_t ms_get_fixed_buf_nr(ms_pool_t *ms_pool, unsigned int size, unsigned int *pool_nr, ms_pool_t *ms_pool2,
                                     unsigned int size2, unsigned int *pool2_nr, const user_token_t user, char *name);
#endif // end of __MS_CORE_H__


