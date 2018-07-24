#ifndef __FT3DNR200_DMA_H__
#define __FT3DNR200_DMA_H__

enum {
    THDNR_DMA_SRC_ADDR      = 0x01E4,
    THDNR_DMA_EE_ADDR       = 0x01EC,
    THDNR_DMA_DES_ADDR      = 0x01F0,
    THDNR_DMA_REF_R_ADDR    = 0x01F4,
    THDNR_DMA_VAR_ADDR      = 0x01F8,
    THDNR_DMA_MOT_ADDR      = 0x01FC,
    THDNR_DMA_REF_W_ADDR    = 0x0200
};

typedef enum {
    WC_WAIT_VALUE = 0,
    RC_WAIT_VALUE,
    DMA_PARAM_MAX
} RW_CMD_WAIT_VAL_ID_T;

typedef struct dma_buf_addr {
    unsigned int src_addr;
    unsigned int ee_addr;
    unsigned int des_addr;
    unsigned int ref_r_addr;
    unsigned int var_addr;
    unsigned int mot_addr;
    unsigned int ref_w_addr;
} dma_buf_addr_t;

typedef struct rw_wait_value {
    int wcmd_wait;
    int rcmd_wait;
} rw_wait_value_t;

int ft3dnr_dma_proc_init(struct proc_dir_entry *entity_proc);
void ft3dnr_dma_proc_remove(struct proc_dir_entry *entity_proc);

#endif
