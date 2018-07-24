#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

#include "enc_driver/define.h"

#define SUPPORT_8M
#ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
    //#define STRICT_SUITABLE_BUFFER
#endif
#ifdef USE_GMLIB_CFG
    //#define DIFF_REF_DDR_MAPPING
    //#define NEW_GMLIB_CFG
#endif

#define ALIGN16_UP(x)   (((x + 15) >> 4) << 4)
#define MAX_LINE_CHAR   256
#ifdef PARSING_GMLIB_BY_VG
    #define MAX_LAYOUT_NUM  1
#else
    #define MAX_LAYOUT_NUM  5
#endif
//#define MAX_RESOLUTION  18
// Tuba 20140407: add new resolution nDH 640 x 360
// Tuba 20141223: add new resolution 6M, 7M
#ifdef NEW_RESOLUTION
    #define MAX_RESOLUTION  22
#else
    #define MAX_RESOLUTION  19
#endif
#define SPEC_FILENAME   "spec.cfg"
#define PARAM_FILENAME  "param.cfg"
#define GMLIB_FILENAME  "gmlib.cfg"

#define MEM_ERROR           -2
#define NO_SUITABLE_BUF     -3
#define OVER_SPEC           -4
#define NOT_OVER_SPEC_ERROR -5

#define CFG_NOT_EXIST       -2
#define LAYOUT_NOT_EXIST    -3
#define LAYOUT_PARAM_ERROR  -4
#define PARAM_NOT_EXIST     -5

typedef enum {
    RES_8M = 0,     // 4096 x 2160  : 8847360
#ifdef NEW_RESOLUTION
    RES_7M,         // 3264 x 2176  : 7102464
    RES_6M,         // 3072 x 2048  : 6291456
#endif
    RES_5M,         // 2560 x 1920  : 4659200
    RES_4M,         // 2304 x 1728  : 3981312
    RES_3M,         // 2048 x 1536  : 3145728
    RES_2M,         // 1920 x 1088  : 2088960
    RES_1080P,      // 1920 x 1088  : 2088960
    RES_1_3M,       // 1280 x 1024  : 1310720
    RES_1_2M,       // 1280 x 960   : 1228800
    RES_1080I,      // 1920 x 544   : 1044480
    RES_1M,         // 1280 x 800   : 1024000
    RES_720P,       // 1280 x 720   : 921600
    RES_960H,       // 960 x 576    : 552960
    RES_720I,       // 1280 x 368   : 471040
    RES_D1,         // 720 x 576    : 414720
    RES_VGA,        // 640 x 480    : 307200
    RES_HD1,        // 720 x 368    : 264960
    RES_nHD,        // 640 x 360    : 230400
    RES_2CIF,       // 368 x 596    : 219328
    RES_CIF,        // 368 x 288    : 105984
    RES_QCIF,       // 176 x 144    : 25344
} ResoluationIdx;

struct res_base_info_t {
    int index;
    char name[6];
    int width;
    int height;
};

struct res_info_t {
    int index;
/*
    char name[5];
    int width;
    int height;
*/
    int max_chn;
    int max_fps;
};

struct ref_item_t {
    unsigned int addr_va;
    unsigned int addr_pa;
    unsigned int size;
    ResoluationIdx res_type;    // resolution type
    int buf_idx;                // buffer index (unique)
    struct ref_item_t *prev;
    struct ref_item_t *next;
};

struct ref_pool_t {
    ResoluationIdx res_type;
    int max_chn;
    int max_fps;
    int size;
    int buf_num;
    int avail_num;
    int used_num;
    int reg_num;
    struct ref_item_t *avail;
    struct ref_item_t *alloc;
    int ddr_idx;
};

struct layout_info_t {
    struct ref_pool_t ref_pool[MAX_RESOLUTION];
    int max_channel;
    int index;
    int buf_num;
};

struct channel_ref_buffer_t {
    int chn;
    int used_num;
    int max_num;
    int res_type;
    struct ref_item_t *alloc_buf[MAX_NUM_REF_FRAMES + 1];
    int head;
    int tail;
    int suitable_res_type;
};

extern int init_mem_pool(void);
extern int clean_mem_pool(void);
extern int register_ref_pool(unsigned int chn, int width, int height);
extern int deregister_ref_pool(unsigned int chn);
extern int allocate_pool_buffer(unsigned int *addr_va, unsigned int *addr_pa, unsigned int res_idx, int chn);
extern int release_pool_buffer(unsigned int addr_va, int chn);
extern int check_reg_buf_suitable(unsigned char *checker);
extern int mark_suitable_buf(int chn, unsigned char *checker, int width, int height);
extern int parse_param_cfg(int *ddr_idx, int *compress_rate);
extern int get_ref_buffer_size(unsigned int res_type);
extern int dump_ref_pool(char *str);
extern int dump_chn_pool(char *str);
extern int dump_chn_buffer(char *str);
extern int dump_pool_num(char *str);
#ifdef PROC_SEQ_OUTPUT
    extern int dump_one_chn_info(struct seq_file *m, int chn);
#endif

#endif
