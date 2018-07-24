#ifndef _FAVC_DEC_LL_H_
#define _FAVC_DEC_LL_H_
#include "portab.h"

#define SW_LINK_LIST     2 /*0: HW linked list, 1: SW linked list, 2: MIXED */
#define DBG_SW_LINK_LIST 0 /* can be enbled only if SW_LINK_LIST == 1 */
//#define SW_POLLING_ITERATION_TIMEOUT 0x100000
//#define SW_POLLING_ITERATION_TIMEOUT 0x1000
#define SW_POLLING_ITERATION_TIMEOUT 0x100000

/* register base index used in linked list comand */
#define MCP300_REG     0 /* 0x00000 */
#define MCP300_DMA     1 /* 0x10c00 */
#define MCP300_REF_L0  2 /* 0x10d00 */
#define MCP300_REF_L1  3 /* 0x10d80 */
#define MCP300_WP0_L0  4 /* 0x10e00 */
#define MCP300_WP1_L0  5 /* 0x10e80 */
#define MCP300_WP0_L1  6 /* 0x10f00 */
#define MCP300_WP1_L1  7 /* 0x10f80 */
#define MCP300_POC0    8 /* 0x20000 */
#define MCP300_POC1    9 /* 0x20080 */
#define MCP300_DSF0   10 /* 0x20280 */
#define MCP300_DSF1   11 /* 0x20300 */
#define MCP300_DSF2   12 /* 0x20380 */
#define MCP300_SL0    13 /* 0x40000 */
#define MCP300_SL1    14 /* 0x40080 */
#define MCP300_VC     15 /* vcache address */

/* the mapped register base address from each register base index */
#define MCP300_REG_OFFSET     0x00000
#define MCP300_DMA_OFFSET     0x10c00
#define MCP300_REF_L0_OFFSET  0x10d00
#define MCP300_REF_L1_OFFSET  0x10d80
#define MCP300_WP0_L0_OFFSET  0x10e00
#define MCP300_WP1_L0_OFFSET  0x10e80
#define MCP300_WP0_L1_OFFSET  0x10f00
#define MCP300_WP1_L1_OFFSET  0x10f80
#define MCP300_POC0_OFFSET    0x20000
#define MCP300_POC1_OFFSET    0x20080
#define MCP300_DSF0_OFFSET    0x20280
#define MCP300_DSF1_OFFSET    0x20300
#define MCP300_DSF2_OFFSET    0x20380
#define MCP300_SL0_OFFSET     0x40000
#define MCP300_SL1_OFFSET     0x40080
#define MCP300_VC_OFFSET      0x00000

#define REG_ADDR(idx, offset)   (unsigned int *)((unsigned int)(idx << 7) + offset)

unsigned int get_reg_offset(unsigned int ll_reg);

struct link_list_hw_cmd {
    uint64_t cmd;
#if DBG_SW_LINK_LIST
    char              *str;
#endif
};


union link_list_cmd {
    uint64_t                cmd;
    struct link_list_hw_cmd hw_cmd;
};


struct link_list_cmd_meta {
#if DBG_SW_LINK_LIST
    char              *str;
#endif
};


#define N_BIT_MASK(n)     ((1LL << n) - 1)
//#define LL_MAX_JOB_CMDS    512
#define LL_MAX_JOB_CMDS    1280 /* for Allegro_NPF_CAVLC_00_5x6_10.5.264 */
#define LL_WORD_OFST_BITS  12 /* for HW link list */
//#define LL_WORD_OFST_BITS  24 /* for SW link list */


enum ll_cmd_type {
    LCT_JOB_HEADER = 1,
    LCT_REG_ACCESS = 2,
    LCT_REG_POLLING = 3,
    LCT_WRITE_OUT = 4,
    LCT_SW_CMD = 5, /* for SW linked list only. invalid for HW linked list */
};

enum ll_cmd_sub_type{
    /* sub type of LCT_REG_ACCESS */
    LCST_SINGLE_WRITE = 0, 
    LCST_BITWISE_AND = 1, 
    LCST_BITWISE_OR = 2, 
    LCST_SUB = 3, 
    LCST_VLC_AUTO_PACK = 4, 
    
    /* sub type of LCT_REG_POLLING */
    LCST_POLLING_ZERO = 0, 
    LCST_POLLING_ONE = 1, 
};



struct link_list_job {
    /* NOTE: this must be 8-byte aligned */
    union link_list_cmd   cmd[LL_MAX_JOB_CMDS]; /* linked list command */
    uint32_t              status[22];  /* for storing offset 0x00~0x54 0x50, 0x10, 0x14. NOTE: this must be 8-byte aligned */
    
    uint32_t              cmd_pa;      /* physical address of the first command */
    uint16_t              job_id;
    uint16_t              cmd_num;
    uint16_t              max_cmd_num; /* for debug only */
    uint16_t              exe_cmd_idx; /* the last executed command index. for error handling */
    uint8_t               has_sw_cmd;  /* whether this job contains SW linked list command */
    DecoderParams        *dec_handle;  /* per-channel data structure associate to this linked list job */
#if 0
    uint8_t               reserved1;   /* to make the next field 8-byte aligned */
    uint8_t               reserved2;   /* to make the next field 8-byte aligned */
    uint8_t               reserved3;   /* to make the next field 8-byte aligned */
#endif
};

//#define LL_MAX_JOBS   32
//#define LL_MAX_JOBS   192
//#define LL_MAX_JOBS   256
//#define LL_MAX_JOBS   320
//#define LL_MAX_JOBS   360 /* for passing the special pattern Allegro_Inter_CABAC_10_L41_HD_10.1 */
#define LL_MAX_JOBS   720 /* for passing the special pattern Allegro_Inter_CABAC_10_L41_HD_10.1 */

struct codec_link_list {
    struct link_list_job  jobs[LL_MAX_JOBS];
    struct link_list_job *curr_job;
    uint32_t              ll_pa;       /* start of the physical address of the linked list */
    int                   job_num;     /* number of jobs in jobs array */
#if SW_LINK_LIST
    uint32_t              base_va;     /* va of MCP300 base address of the bound engine */
    uint32_t              vc_base_va;  /* va of VCACHE base address of the bound engine */
#endif
    uint16_t              exe_job_idx; /* the last executed command index. for error handling */
    uint8_t               sw_exec_flg; /* indicate whether the linked list is executed by SW */
};


/*
 * start of functions for packing linked list command
 */
inline static union link_list_cmd job_header_cmd(unsigned int job_id, unsigned int next_job_cmd_num, unsigned int next_job_st_addr)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (1LL << 60) | 
	            (((uint64_t)job_id & N_BIT_MASK(11)) << 49) |
                (((uint64_t)next_job_cmd_num & N_BIT_MASK(14)) << 35) |
                ((uint64_t)(next_job_st_addr?0:1) << 32) | 
                next_job_st_addr;
    return ll_cmd;
}


inline static union link_list_cmd single_write_cmd(unsigned int offst, unsigned int data)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (2LL << 60) | 
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 data;
    return ll_cmd;
}


inline static union link_list_cmd bitwise_and_cmd(unsigned int offst, unsigned int data)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (2LL << 60) | 
                 (1LL << 56) |
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 data;
    return ll_cmd;
}


inline static union link_list_cmd bitwise_or_cmd(unsigned int offst, unsigned int data)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (2LL << 60) | 
                 (2LL << 56) |
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 data;
    return ll_cmd;
}


inline static union link_list_cmd bitwise_sub_cmd(unsigned int offst, unsigned int zero_mask, unsigned int subtrahend)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (2LL << 60) | 
                 (3LL << 56) | 
                 ((uint64_t)(subtrahend & N_BIT_MASK(4)) << 44) |
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 zero_mask;
    return ll_cmd;
}


inline static union link_list_cmd poll_zero_cmd(unsigned int offst, unsigned int one_mask, char *str)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (3LL << 60) | 
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 one_mask;
#if DBG_SW_LINK_LIST
    ll_cmd.hw_cmd.str = str;
#endif
    return ll_cmd;
}


inline static union link_list_cmd poll_one_cmd(unsigned int offst, unsigned int zero_mask, char *str)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (3LL << 60) | 
                 (1LL << 56) |
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 zero_mask;
#if DBG_SW_LINK_LIST
    ll_cmd.hw_cmd.str = str;
#endif
    return ll_cmd;
}


inline static union link_list_cmd write_out_cmd(unsigned int offst, unsigned int num, unsigned int dst_addr)
{
    union link_list_cmd ll_cmd;
    ll_cmd.hw_cmd.cmd = (4LL << 60) | 
                 ((uint64_t)(num & N_BIT_MASK(8)) << 48) |
                 (((uint64_t)offst & N_BIT_MASK(LL_WORD_OFST_BITS)) << 32) |
                 dst_addr;
    return ll_cmd;
}


#if SW_LINK_LIST == 2
inline static union link_list_cmd sw_func_cmd1(void *func, uint32_t param1)
{
    union link_list_cmd ll_cmd;
    uint32_t addr_h;
    addr_h = (uint32_t)func;
    addr_h = addr_h >> 16;
    
    ll_cmd.hw_cmd.cmd = (5LL << 60) | ((uint64_t)addr_h  << 32) | param1;
    return ll_cmd;
}
inline static union link_list_cmd sw_func_cmd2(void *func, uint32_t param2)
{
    union link_list_cmd ll_cmd;
    uint32_t addr_l;
    addr_l = (uint32_t)func;
    addr_l = addr_l & 0xFFFFF;
    ll_cmd.hw_cmd.cmd = (5LL << 60) | ((uint64_t)addr_l  << 32) | param2;
    return ll_cmd;
}
#endif

/*
 * end of functions for packing linked list command
 */



void ll_init(struct codec_link_list *codec_ll, unsigned int init_id);
void ll_reinit(struct codec_link_list *codec_ll);
void ll_job_init(struct link_list_job *job, unsigned int job_id, unsigned int cmd_pa);
void ll_job_reinit(struct link_list_job *job);
int ll_print_cmd(union link_list_cmd *ll_cmd_ptr);
#if SW_LINK_LIST == 2
void ll_set_base_addr(struct codec_link_list *codec_ll, unsigned int base_addr, unsigned int vc_base_addr);
int ll_sw_exec_cmd2(struct codec_link_list *codec_ll, union link_list_cmd *ll_cmd_ptr);
int ll_sw_exec_job2(struct codec_link_list *codec_ll, struct link_list_job *job);
int ll_sw_exec2(struct codec_link_list *codec_ll);
#endif

void ll_print_job(struct link_list_job *job);
void ll_print_ll(struct codec_link_list *codec_ll);

int ll_add_cmd(struct link_list_job *ll_job, union link_list_cmd ll_cmd);
int ll_add_job(struct codec_link_list *codec_ll, struct link_list_job *ll_job, void *dec_handle);

#endif /* _FAVC_DEC_LL_H_ */

