#ifndef _FAVCE_BUFFER_H_
#define _FAVCE_BUFFER_H_

typedef enum {
    BS_BUF_MASK     = 0x01,
    REF_BUF_MASK    = 0x02,
    DIDN_BUF_MASK   = 0x04,
    SYSO_BUF_MASK   = 0x08,
    MBINFO_BUF_MASK = 0x10,
    L1_BUF_MASK     = 0x20
} BufferTypeMask;

struct buffer_info_t
{
    unsigned int addr_pa;
    unsigned int addr_va;
    unsigned int size;
};

extern int allocate_frammap_buffer(struct buffer_info_t *buf, int size);
extern int release_frammap_buffer(struct buffer_info_t *buf);
extern int allocate_gm_buffer(struct buffer_info_t *buf, int size, void *data);
extern int free_gm_buffer(struct buffer_info_t *buf);
extern int favce_alloc_general_buffer(int max_width, int max_height);
extern int favce_release_general_buffer(void);
extern unsigned int get_common_buffer_size(unsigned int flag);

#endif