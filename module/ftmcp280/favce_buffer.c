#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/kallsyms.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "log.h"
#include "frammap_if.h"
#include "favc_enc_vg.h"
#include "favc_enc_entity.h"
#include "favce_buffer.h"
#include "h264e_ioctl.h"
#include "mem_pool.h"

int mcp280_ddr_id = 0;

extern int log_level;


#ifdef TWO_ENGINE
    extern struct buffer_info_t mcp280_sysinfo_buffer[MAX_ENGINE_NUM];
    extern struct buffer_info_t mcp280_mvinfo_buffer[MAX_ENGINE_NUM];
    extern struct buffer_info_t mcp280_l1_col_buffer[MAX_ENGINE_NUM];
#else
    extern struct buffer_info_t mcp280_sysinfo_buffer;
    extern struct buffer_info_t mcp280_mvinfo_buffer;
    extern struct buffer_info_t mcp280_l1_col_buffer;
#endif

#ifdef VG_INTERFACE
    extern struct favce_data_t  *private_data;
    extern unsigned int         h264e_max_b_frame;
    extern unsigned int         h264e_max_chn;
#endif
#ifdef DISABLE_WRITE_OUT_REC
    extern unsigned int h264e_dis_wo_buf;
#endif

int allocate_frammap_buffer(struct buffer_info_t *buf, int size)
{
    struct frammap_buf_info buf_info;
    memset(&buf_info, 0, sizeof(struct frammap_buf_info));
    buf_info.size = size;
    buf_info.align = 32;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    buf_info.name = "h264e";
#endif
    buf_info.alloc_type = ALLOC_NONCACHABLE;

    if (1 == mcp280_ddr_id) {
        if (frm_get_buf_ddr(DDR_ID1, &buf_info) < 0)
            return -1;
    }
    else if (0 == mcp280_ddr_id) {
        if (frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info) < 0)
            return -1;
    }
	else
        return -1;
    buf->addr_va = (uint32_t)buf_info.va_addr;
    buf->addr_pa = (uint32_t)buf_info.phy_addr;
    buf->size = size;
    if (0 == buf->addr_va)
        return -1;
    DEBUG_M(LOG_INFO, "allocate buffer(frammap) va 0x%08x, pa 0x%08x, size %d\n", buf->addr_va, buf->addr_pa, size);
    //printk("allocate buffer(frammap) va 0x%08x, pa 0x%08x, size %d\n", buf->addr_va, buf->addr_pa, size);
    return 0;
}

int release_frammap_buffer(struct buffer_info_t *buf)
{
    if (buf->addr_va) {
        DEBUG_M(LOG_INFO, "free buffer(frammap) pa 0x%08x, va 0x%08x, size %d\n", buf->addr_va, buf->addr_pa, buf->size);
        frm_free_buf_ddr((void *)buf->addr_va);
        memset(buf, 0, sizeof(struct buffer_info_t));
    }
    return 0;
}

int allocate_gm_buffer(struct buffer_info_t *buf, int size, void *data)
{
    buf->addr_va = (unsigned int)kmalloc(size, GFP_ATOMIC);
    if (0 == buf->addr_va)
        return -1;
    buf->addr_pa = frm_va2pa(buf->addr_va);
    if (0xFFFFFFFF == buf->addr_pa)
        return -1;
    buf->size = size;

    DEBUG_M(LOG_INFO, "allocate buffer 0x%x (0x%d), size %d\n", buf->addr_va, buf->addr_pa, size);
    return 0;
}

int free_gm_buffer(struct buffer_info_t *buf)
{
    if (buf->addr_va) {
        DEBUG_M(LOG_INFO, "free buffer 0x%x (0x%x)\n", buf->addr_va, buf->addr_pa);
        kfree((void *)buf->addr_va);
        buf->addr_va = 0;
        buf->addr_pa = 0;
        buf->size = 0;
    }
    return 0;
}

unsigned int get_common_buffer_size(unsigned int flag)
{
    unsigned int size = 0;
    int chn;
#ifdef TWO_ENGINE
    int engine;
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        if (flag & SYSO_BUF_MASK)
            size += mcp280_sysinfo_buffer[engine].size;
        if (flag & MBINFO_BUF_MASK)
            size += mcp280_mvinfo_buffer[engine].size;
        if (!h264e_max_b_frame) {
            if (flag & L1_BUF_MASK)
                size += mcp280_l1_col_buffer[engine].size;
        }
    }
#else
    if (flag & SYSO_BUF_MASK)
        size += mcp280_sysinfo_buffer.size;
    if (flag & MBINFO_BUF_MASK)
        size += mcp280_mvinfo_buffer.size;
    if (!h264e_max_b_frame) {
        if (flag & L1_BUF_MASK)
            size += mcp280_l1_col_buffer.size;
    }
#endif

    if (h264e_max_b_frame) {
        if (flag & L1_BUF_MASK) {
            struct favce_data_t *enc_data;
            for (chn = 0; chn < h264e_max_chn; chn++) {
                enc_data = private_data + chn;
                size += enc_data->L1ColMVInfo_buffer.size;
            }
        }
    }
    return size;
}

int favce_alloc_general_buffer(int max_width, int max_height)
{
    int sysinfo_size, mvinfo_size, l1colmvinfo_size;
#ifdef VG_INTERFACE
    struct favce_data_t *enc_data;
    int chn;
#endif
#ifdef TWO_ENGINE
    int engine;
#endif

#ifndef PARSING_GMLIB_BY_VG
    if (parse_param_cfg(&mcp280_ddr_id, NULL) < 0)
        return -1;
#endif
    // Tuba 20140818: add ddr checker
    if (0 != mcp280_ddr_id && 1 != mcp280_ddr_id) {
        favce_err("wrong ddr idx %d\n", mcp280_ddr_id);
        return -1;
    }
    sysinfo_size = max_width * 3;
    mvinfo_size = max_width * max_height / 32;
    l1colmvinfo_size = max_width * max_height / 16;
#ifdef TWO_ENGINE
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        if (allocate_frammap_buffer(&mcp280_sysinfo_buffer[engine], sysinfo_size) < 0) {
            DEBUG_E(LOG_ERROR, "{engine%d} allocate sysinfo buffer error, size %d\n", engine, sysinfo_size);
            goto allocate_general_buffer_fail;
        }
    #ifdef DISABLE_WRITE_OUT_REC
        if (1 == h264e_dis_wo_buf)
            continue;
    #endif
        if (allocate_frammap_buffer(&mcp280_mvinfo_buffer[engine], mvinfo_size) < 0) {
            DEBUG_E(LOG_ERROR, "{engine%d} allocate mvinfo buffer error, size %d\n", engine, mvinfo_size);
            goto allocate_general_buffer_fail;
        }
        if (!h264e_max_b_frame) {
            if (allocate_frammap_buffer(&mcp280_l1_col_buffer[engine], l1colmvinfo_size) < 0) {
                DEBUG_E(LOG_ERROR, "{engine%d} allocate l1mvinfo buffer error, size %d\n", engine, l1colmvinfo_size);
                goto allocate_general_buffer_fail;
            }
        }
    }
#else
    if (allocate_frammap_buffer(&mcp280_sysinfo_buffer, sysinfo_size) < 0) {
        DEBUG_E(LOG_ERROR, "allocate sysinfo buffer error, size %d\n", sysinfo_size);
        goto allocate_general_buffer_fail;
    }
    #ifdef DISABLE_WRITE_OUT_REC
    if (1 == h264e_dis_wo_buf)
        goto skip_allocate_mv_l1;
    #endif
    if (allocate_frammap_buffer(&mcp280_mvinfo_buffer, mvinfo_size) < 0) {
        DEBUG_E(LOG_ERROR, "allocate mvinfo buffer error, size %d\n", mvinfo_size);
        goto allocate_general_buffer_fail;
    }
    if (!h264e_max_b_frame) {
        if (allocate_frammap_buffer(&mcp280_l1_col_buffer, l1colmvinfo_size) < 0) {
            DEBUG_E(LOG_ERROR, "allocate l1mvinfo buffer error, size %d\n", l1colmvinfo_size);
            goto allocate_general_buffer_fail;
        }
    }
    #ifdef DISABLE_WRITE_OUT_REC
    skip_allocate_mv_l1:
    #endif
#endif

    if (h264e_max_b_frame) {
        for (chn = 0; chn < h264e_max_chn; chn++) {
        #ifdef VG_INTERFACE
            enc_data = private_data + chn;
            if (allocate_frammap_buffer(&enc_data->L1ColMVInfo_buffer, l1colmvinfo_size) < 0) {
                DEBUG_E(LOG_ERROR, "{chn%d} allocate l1mvinfo buffer error, size %d\n", enc_data->chn, l1colmvinfo_size);
                goto allocate_general_buffer_fail;
            }
        #else
        #endif
        }
    }
    return 0;

allocate_general_buffer_fail:
    //DEBUG_E(LOG_ERROR, "allocate buffer error\n");
#ifdef TWO_ENGINE
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        release_frammap_buffer(&mcp280_sysinfo_buffer[engine]);
        release_frammap_buffer(&mcp280_mvinfo_buffer[engine]);
        if (!h264e_max_b_frame)
            release_frammap_buffer(&mcp280_l1_col_buffer[engine]);
    }
#else
    release_frammap_buffer(&mcp280_sysinfo_buffer);
    release_frammap_buffer(&mcp280_mvinfo_buffer);
    if (!h264e_max_b_frame)
        release_frammap_buffer(&mcp280_l1_col_buffer);
#endif
    if (h264e_max_b_frame) {
        for (chn = 0; chn < h264e_max_chn; chn++) {
        #ifdef VG_INTERFACE
            enc_data = private_data + chn;
            release_frammap_buffer(&enc_data->L1ColMVInfo_buffer);
        #else
        #endif
        }
    }
    return -1;
}
int favce_release_general_buffer(void)
{
#ifdef VG_INTERFACE
    struct favce_data_t *enc_data;
    int chn;
#endif
#ifdef TWO_ENGINE
    int engine;
#endif

#ifdef TWO_ENGINE
    for (engine = 0; engine < MAX_ENGINE_NUM; engine++) {
        release_frammap_buffer(&mcp280_sysinfo_buffer[engine]);
        release_frammap_buffer(&mcp280_mvinfo_buffer[engine]);
        if (!h264e_max_b_frame)
            release_frammap_buffer(&mcp280_l1_col_buffer[engine]);
    }
#else
    release_frammap_buffer(&mcp280_sysinfo_buffer);
    release_frammap_buffer(&mcp280_mvinfo_buffer);
    if (!h264e_max_b_frame)
        release_frammap_buffer(&mcp280_l1_col_buffer);
#endif
    if (h264e_max_b_frame) {
        for (chn = 0; chn < h264e_max_chn; chn++) {
        #ifdef VG_INTERFACE
            enc_data = private_data + chn;
            release_frammap_buffer(&enc_data->L1ColMVInfo_buffer);
        #else
        #endif
        }
    }
    return 0;
}


