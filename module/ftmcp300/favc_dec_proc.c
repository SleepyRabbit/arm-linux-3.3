/** 
 * @file Favc_dec_proc.c  
 * Copyright (C) 2014 GM Corp. (http://www.grain-media.com) 
 * 
 * $Revision: 1.01 $ 
 * $Date: 2014/05/28 13:48 $ 
 * 
 * ChangeLog: 
 *  $Log: Favc_dec_proc.c $ 
 *  Revision 1.01  2014/05/28 13:48  tire_l 
 *  1. add fs proc 
 * */

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
#include <linux/seq_file.h>

#include "dec_driver/define.h"
#include "dec_driver/h264_reg.h"
#include "dec_driver/global.h"
#include "favc_dec_entity.h"
#include "favc_dec_vg.h"
#include "favc_dec_dbg.h"
#include "favc_dec_rev.h"
#include "h264d_ioctl.h"
#include "favcd_param.h"

#if USE_VG_WORKQ
extern vp_workqueue_t *favcd_cb_wq;
#endif // USE_VG_WORKQ

#define ENTITY_PROC_NAME "videograph/h264d"


static struct proc_dir_entry *h264d_entry_proc = NULL;
static struct proc_dir_entry *profproc = NULL;
static struct proc_dir_entry *paramproc = NULL;
static struct proc_dir_entry *dumpproc = NULL;
static struct proc_dir_entry *infoproc = NULL;
static struct proc_dir_entry *vg_infoproc = NULL;
static struct proc_dir_entry *cbproc = NULL;
static struct proc_dir_entry *prepproc = NULL;
static struct proc_dir_entry *utilproc = NULL;
static struct proc_dir_entry *eng_reg_proc = NULL;
static struct proc_dir_entry *propertyproc = NULL;
static struct proc_dir_entry *jobprofproc = NULL;
static struct proc_dir_entry *jobproc = NULL;
static struct proc_dir_entry *dbgmodeproc = NULL;
static struct proc_dir_entry *levelproc = NULL;
static struct proc_dir_entry *buf_num_proc = NULL;
static struct proc_dir_entry *cfg_proc = NULL;
static struct proc_dir_entry *fsproc = NULL;
static struct proc_dir_entry *fs_chproc = NULL;
static struct proc_dir_entry *fs_fsproc = NULL;


#define ENABLE_PORC_LOG 0
#if ENABLE_PORC_LOG
static struct proc_dir_entry *log_proc = NULL;
#endif // ENABLE_PORC_LOG


extern char favcd_ver_str[128];
extern unsigned int callback_period;
extern unsigned int prepare_period;
extern unsigned int utilization_period;
extern struct utilization_record_t eng_util[ENTITY_ENGINES];

extern int mcp300_max_buf_num;
extern int max_pp_buf_num;
extern unsigned int mcp300_codec_padding_size;
extern unsigned int mcp300_sw_reset;
extern unsigned int vc_cfg;
extern unsigned int mcp300_alloc_mem_from_vg;
extern unsigned int mcp300_b_frame;
extern int engine_num;
extern int output_mbinfo;
extern int save_bs_cnt;
extern int save_bs_idx;
extern int save_output_cnt;
extern int save_output_idx;
extern int use_ioremap_wc;
extern int save_err_bs_flg;
extern unsigned int save_mbinfo;
extern unsigned int save_mbinfo_cnt;
extern unsigned int save_mbinfo_idx;
extern int clear_mbinfo;
extern struct favcd_data_t *private_data;
extern spinlock_t favc_dec_lock;
extern struct property_record_t *property_record;
extern DecoderEngInfo favcd_engine_info[ENTITY_ENGINES];
extern struct list_head  favcd_global_head;
extern struct list_head *favcd_minor_head;
extern struct pp_prof_data pp_prof_data;
extern struct cb_prof_data cb_prof_data;
extern struct isr_prof_data isr_prof_data[ENTITY_ENGINES];
extern struct job_prof_data job_prof_data;
extern int chn_num;
extern char *file_path;
extern char file_path_buf[128];
extern unsigned int lose_pic_handle_flags; //"lose picture handling flags: bit 0: whether to print error message. bit 1: whether to return error");

#if REV_PLAY
extern struct list_head *favcd_gop_minor_head; // gop job group head of each channel
#endif


static unsigned int eng_reg_engine = 0;
static unsigned int property_minor = 0;
static int job_minor = -1; /* value < 0 means all*/



#define TEST_GET_MBINFO
#ifdef  TEST_GET_MBINFO

unsigned char mbinfo_buf[PAGE_SIZE];

int fs_ch = 0;

void swap_byte(unsigned char *ptr, int n)
{
	unsigned char buf[64];
	int i;

	for(i = 0; i < n; i++){
		buf[i] = ptr[n - 1 - i];
	}
	memcpy(ptr, buf, n);
}

void print_mv16x16(u32 *buf)
{
    unsigned int *ptr;
    int i;

    ptr = (unsigned int *)buf;

    for(i = 0; i < 8; i++){
        unsigned int four_bytes;
        int ref_idx;
        int mvy_l;
        int mvx_l;
        
        four_bytes = *(ptr + i);
        swap_byte((unsigned char *)&four_bytes, 4);

        ref_idx = (four_bytes >> 26) & ((1 << 7) - 1);
        // sign extend
        ref_idx = ref_idx << (32 - 6);
        ref_idx = ref_idx >> (32 - 6);

        mvy_l = (four_bytes >> 14) & ((1 << 12) - 1);
        // sign extend
        mvy_l = mvy_l << (32 - 12);
        mvy_l = mvy_l >> (32 - 12);

        mvx_l = (four_bytes >> 0) & ((1 << 14) - 1);
        // sign extend
        mvx_l = mvx_l << (32 - 14);
        mvx_l = mvx_l >> (32 - 14);
        printk("[%5d]%08X mvx:%8d mvy:%8d ref:%8d", i, four_bytes, mvx_l, mvy_l, ref_idx);
        if(ref_idx < 0){
            printk(" invalid");
        }
        printk("\n");
    }
}

void print_mbinfo(unsigned char *buf)
{
    int i;
   	unsigned int four_bytes;
    unsigned char *ptr;
    
    for(i = 0; i < 256; i += 8){
        swap_byte(buf + i, 8);
    }
    
    four_bytes = *(((unsigned int *)buf) + 24);
    printk("%08X mb type:%8d\n", four_bytes, four_bytes & 0x3f);

    ptr = buf + 128;
    for(i = 0; i < 128; i += 4){
        int ref_idx;
        int mvy_l;
        int mvx_l;
        
        four_bytes = *(unsigned int *)(ptr + i);

        ref_idx = (four_bytes >> 26) & ((1 << 7) - 1);
        // sign extend
        ref_idx = ref_idx << (32 - 6);
        ref_idx = ref_idx >> (32 - 6);
        
        mvy_l = (four_bytes >> 14) & ((1 << 12) - 1);
        // sign extend
        mvy_l = mvy_l << (32 - 12);
        mvy_l = mvy_l >> (32 - 12);
        
        mvx_l = (four_bytes >> 0) & ((1 << 14) - 1);
        // sign extend
        mvx_l = mvx_l << (32 - 14);
        mvx_l = mvx_l >> (32 - 14);
        printk("[%5d]%08X mvx:%8d mvy:%8d ref:%8d", i, four_bytes, mvx_l, mvy_l, ref_idx);
        if(ref_idx < 0){
            printk(" invalid");
        }
        printk("\n");

    }
    
}

#endif /* TEST_GET_MBINFO */


unsigned char detail_config = 0;
static int favcd_proc_config_show(struct seq_file *sfile, void *v)
{
    int i;
    extern int irq_no[ENTITY_ENGINES];
    extern unsigned int vcache_one_ref;
    extern unsigned int max_ref1_width;
    extern unsigned int max_level_idc;
    extern const unsigned int property_map_size;
    
    seq_printf(sfile, "%s\n", favcd_ver_str);
    seq_printf(sfile, "engine number : %d\n", ENTITY_ENGINES);
    seq_printf(sfile, "channel number: %d\n", chn_num);

    if(detail_config == 0) /* for hinding the following message */
        return 0;
    
    for(i = 0; i < ENTITY_ENGINES; i++){
        seq_printf(sfile, "IRQ[%d]:%s %d pa:%08lX\n", i, irq_name[i], irq_no[i], favcd_base_address_pa[i]);
    }
    
    seq_printf(sfile, "codec padding size:%d\n", mcp300_codec_padding_size);
    seq_printf(sfile, "mcp300_sw_reset:%d\n", mcp300_sw_reset);
    seq_printf(sfile, "mcp300_alloc_mem_from_vg:%d\n", mcp300_alloc_mem_from_vg);
    seq_printf(sfile, "mcp300_b_frame:%d\n", mcp300_b_frame);
    seq_printf(sfile, "output_mbinfo:%d\n", output_mbinfo);
    seq_printf(sfile, "max_pp_buf_num:%d\n", max_pp_buf_num);
    seq_printf(sfile, "vcache workaround:%d\n", mcp300_sw_reset);
    

    seq_printf(sfile, "vcache enabled:%d\n", CONFIG_ENABLE_VCACHE);
    seq_printf(sfile, "vcache block enabled:%d\n", VCACHE_BLOCK_ENABLE);
    seq_printf(sfile, "vcache ilf:%d\n", ENABLE_VCACHE_ILF);
    seq_printf(sfile, "vcache sysinfo:%d\n", ENABLE_VCACHE_SYSINFO);
    seq_printf(sfile, "vcache ref:%d\n", ENABLE_VCACHE_REF);
    seq_printf(sfile, "vcache cfg:%d\n", vc_cfg);
    seq_printf(sfile, "vcache_one_ref: %d\n", vcache_one_ref);
    seq_printf(sfile, "max_ref1_width: %d\n", max_ref1_width);

    seq_printf(sfile, "SLICE_WRITE_BACK:%d\n", SLICE_WRITE_BACK);
    seq_printf(sfile, "SLICE_DMA_DISABLE:%d\n", SLICE_DMA_DISABLE);

    seq_printf(sfile, "HZ:%d FAVCD_HZ:%d\n", HZ, FAVCD_HZ);
    seq_printf(sfile, "engine_num:%d\n", engine_num);
    
    seq_printf(sfile, "USE_VG_WORKQ: %d\n", USE_VG_WORKQ);

    seq_printf(sfile, "CHK_ISR_DURATION:%d\n", CHK_ISR_DURATION);
    seq_printf(sfile, "dbg_mode:%d\n", dbg_mode);
    seq_printf(sfile, "use_ioremap_wc:%d\n", use_ioremap_wc);
    seq_printf(sfile, "WAIT_FR_DONE_CTRL:%d\n", WAIT_FR_DONE_CTRL);
    seq_printf(sfile, "BLOCK_READ_DISABLE:%d\n", BLOCK_READ_DISABLE);
    seq_printf(sfile, "max_level_idc:%d\n", max_level_idc);

    {
        extern unsigned int alloc_cnt;
        extern unsigned int free_cnt;
        seq_printf(sfile, "alloc_cnt:%d\n", alloc_cnt);
        seq_printf(sfile, "free_cnt:%d\n", free_cnt);
    }
    {
        extern unsigned int fkmalloc_cnt;
        extern unsigned int fkfree_cnt;
        extern unsigned int fkmalloc_size;
        seq_printf(sfile, "fkmalloc_cnt:%d\n", fkmalloc_cnt);
        seq_printf(sfile, "fkfree_cnt:%d\n", fkfree_cnt);
        seq_printf(sfile, "fkmalloc_size:%d\n", fkmalloc_size);
    }
#if ENABLE_VIDEO_RESERVE
    {
        extern unsigned int resv_out_cnt;
        extern unsigned int free_out_cnt;
        seq_printf(sfile, "resv_out_cnt:%d\n", resv_out_cnt);
        seq_printf(sfile, "free_out_cnt:%d\n", free_out_cnt);
    }
#endif

    seq_printf(sfile, "clear_mbinfo:%d\n", clear_mbinfo);
    seq_printf(sfile, "USE_SW_PARSER:%d\n", USE_SW_PARSER);
    seq_printf(sfile, "USE_LINK_LIST:%d\n", USE_LINK_LIST);
    seq_printf(sfile, "POLLING_ITERATION_TIMEOUT:%d\n", POLLING_ITERATION_TIMEOUT);
    seq_printf(sfile, "EXC_HDR_PARSING_FROM_UTIL:%d\n", EXC_HDR_PARSING_FROM_UTIL);
    seq_printf(sfile, "OUTPUT_POC_MATCH_STEP:%d\n", OUTPUT_POC_MATCH_STEP);
    seq_printf(sfile, "USE_OUTPUT_BUF_NUM:%d\n", USE_OUTPUT_BUF_NUM);
    seq_printf(sfile, "EN_DUMP_MSG:%d\n", EN_DUMP_MSG);
    seq_printf(sfile, "LIMIT_VG_BUF_NUM:%d\n", LIMIT_VG_BUF_NUM);
    seq_printf(sfile, "TEST_SW_TIMEOUT:%d\n", TEST_SW_TIMEOUT);
    seq_printf(sfile, "USE_PROFILING:%d\n", USE_PROFILING);
    seq_printf(sfile, "CHECK_UNOUTPUT:%d\n", CHECK_UNOUTPUT);
    seq_printf(sfile, "DISABLE_IRQ_IN_ISR:%d\n", DISABLE_IRQ_IN_ISR);
    seq_printf(sfile, "EN_WARN_ERR_MSG_ONLY:%d\n", EN_WARN_ERR_MSG_ONLY);
    seq_printf(sfile, "CHK_VC_REF_EN_AT_SLICE1:%d\n", CHK_VC_REF_EN_AT_SLICE1);
    seq_printf(sfile, "REV_PLAY:%d\n", REV_PLAY);

    seq_printf(sfile, "property_map_size:%d\n", property_map_size);
    seq_printf(sfile, "MAX_RECORD:%d\n", MAX_RECORD);

#if 0    
    seq_printf(sfile, "sizeof(FAVC_DEC_FRAME_IOCTL):%d\n", sizeof(FAVC_DEC_FRAME_IOCTL));
    seq_printf(sfile, "sizeof(struct favcd_job_item_t):%d\n", sizeof(struct favcd_job_item_t));
#endif
#if 1
    seq_printf(sfile, "sizeof(SliceHeader):%d\n", sizeof(SliceHeader));
    seq_printf(sfile, "sizeof(SeqParameterSet):%d\n", sizeof(SeqParameterSet));
    seq_printf(sfile, "sizeof(PicParameterSet):%d\n", sizeof(PicParameterSet));
    seq_printf(sfile, "sizeof(StorablePicture):%d\n", sizeof(StorablePicture));
#endif
    
    return 0;
}


static ssize_t favcd_proc_config_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    char value_str[16] = {'\0'};
    int len = count;
    int val;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &val);

    detail_config = val;
    printk("\ndetail config = %d (0:disabled 1:enabled)\n", detail_config);

    return count;
}



static int favcd_proc_prof_show(struct seq_file *sfile, void *v)
{
    int i, j, idx;
    unsigned long flags;

    unsigned int pp_period_total = 0;
    unsigned int pp_dur_total = 0;
#if 0
    unsigned int pp_parse_dur_total = 0;
    unsigned int pp_rel_dur_total = 0;
    unsigned int pp_job_dur_total = 0;
#endif

    unsigned int cb_period_total = 0;
    unsigned int cb_dur_total = 0;
    char *head_str1;
    char *head_str2;
    char *head_str3;

    head_str1 = "                    trig-               parse par relea rel job   job stop";
    head_str2 = "idx period    trig  start  dur   start  dur   cnt dur   cnt dur   cnt cnt";
    head_str3 = "== ======== ======== ==== ==== ======== ===== === ===== === ===== === ===";


    seq_printf(sfile, "\n[ prepare job profiling ]\n");
    down(&pp_prof_data.sem);
    
    if(pp_prof_data.pp_cnt < PP_PROF_RECORD_NUM){
        idx = 0;
    }else{
        idx = pp_prof_data.pp_cnt % PP_PROF_RECORD_NUM;
    }

    for(i = 0; i < PP_PROF_RECORD_NUM; i++){
        if(i >= pp_prof_data.pp_cnt){
            break;
        }

        if(i % 32 == 0){
            seq_printf(sfile, "%s\n", head_str3);
            seq_printf(sfile, "%s\n", head_str1);
            seq_printf(sfile, "%s\n", head_str2);
            seq_printf(sfile, "%s\n", head_str3);
        }
        seq_printf(sfile, "%2d", i);
        if(pp_prof_data.pp_prof_rec[idx].pp_period & (1 << 31)){ /* detect incorrect period due to timestamp overflow */
            seq_printf(sfile, " %8s", "NA");
        }else{
            seq_printf(sfile, " %8u", pp_prof_data.pp_prof_rec[idx].pp_period);
        }
        seq_printf(sfile, " %8u", pp_prof_data.pp_prof_rec[idx].pp_trig);
        if(pp_prof_data.pp_prof_rec[idx].pp_trig > pp_prof_data.pp_prof_rec[idx].pp_start){
            /* NOTE: the record of trig-start is invalid. 
                     WHY? pp_trig is set while the pp_start is still the previous value */
            seq_printf(sfile, "   NA");
        }else{
            seq_printf(sfile, " %4u", pp_prof_data.pp_prof_rec[idx].pp_start - pp_prof_data.pp_prof_rec[idx].pp_trig);
        }
        seq_printf(sfile, " %4u", pp_prof_data.pp_prof_rec[idx].pp_dur);
        seq_printf(sfile, " %8u", pp_prof_data.pp_prof_rec[idx].pp_start);
        seq_printf(sfile, " %5u", pp_prof_data.pp_prof_rec[idx].pp_parse_dur);
        seq_printf(sfile, " %3u", pp_prof_data.pp_prof_rec[idx].pp_parse_cnt);
        seq_printf(sfile, " %5u", pp_prof_data.pp_prof_rec[idx].pp_rel_dur);
        seq_printf(sfile, " %3u", pp_prof_data.pp_prof_rec[idx].pp_rel_cnt);
        seq_printf(sfile, " %4u", pp_prof_data.pp_prof_rec[idx].pp_job_dur);
        seq_printf(sfile, " %3u", pp_prof_data.pp_prof_rec[idx].pp_job_cnt);
        seq_printf(sfile, " %3u\n", pp_prof_data.pp_prof_rec[idx].pp_stop_cnt);
        
        pp_period_total += pp_prof_data.pp_prof_rec[idx].pp_period;
        pp_dur_total += pp_prof_data.pp_prof_rec[idx].pp_dur;
        
        #if 0
        pp_parse_dur_total += pp_prof_data.pp_prof_rec[idx].pp_parse_dur;
        pp_rel_dur_total += pp_prof_data.pp_prof_rec[idx].pp_rel_dur;
        pp_job_dur_total += pp_prof_data.pp_prof_rec[idx].pp_job_dur;
        #endif

        
        idx++;
        if(idx >= PP_PROF_RECORD_NUM){
            idx = 0;
        }
    }
    seq_printf(sfile, "   pp cnt:%u\n", pp_prof_data.pp_cnt);
    seq_printf(sfile, "  job cnt:%u\n", pp_prof_data.pp_job_cnt);
    seq_printf(sfile, " trig cnt:%u\n", pp_prof_data.pp_trig_cnt);
    seq_printf(sfile, "waste cnt:%u\n", pp_prof_data.pp_waste_cnt);
    seq_printf(sfile, "prepare record:\n");

    up(&pp_prof_data.sem);
    if(pp_period_total != 0){
        seq_printf(sfile, "pp:%2u.%02u%% %u/%u\n", pp_dur_total * 100 / pp_period_total, (pp_dur_total * 10000 / pp_period_total) % 100,  pp_dur_total, pp_period_total);
    }else{
        seq_printf(sfile, "pp:NA\n");
    }
    

    seq_printf(sfile, "\n[ callback profiling ]\n");
    
    down(&cb_prof_data.sem);
    head_str1 = "                    trig-  cbs           job job stop";
    head_str2 = "idx period    trig  start  dur   start   dur cnt cnt";
    head_str3 = "== ======== ======== ==== ==== ======== ==== === ===";

    if(cb_prof_data.cb_sch_cnt < CB_PROF_RECORD_NUM){
        idx = 0;
    }else{
        idx = cb_prof_data.cb_sch_cnt % CB_PROF_RECORD_NUM;
    }

    for(i = 0; i < CB_PROF_RECORD_NUM; i++){
        if(i >= cb_prof_data.cb_sch_cnt){
            break;
        }

        if(i % 32 == 0){
            seq_printf(sfile, "%s\n", head_str3);
            seq_printf(sfile, "%s\n", head_str1);
            seq_printf(sfile, "%s\n", head_str2);
            seq_printf(sfile, "%s\n", head_str3);
        }
        seq_printf(sfile, "%2d", i);
        if(cb_prof_data.cb_prof_rec[idx].cb_period & (1 << 31)){ /* detect incorrect period due to timestamp overflow */
            seq_printf(sfile, " %8s", "NA");
        }else{
            seq_printf(sfile, " %8u", cb_prof_data.cb_prof_rec[idx].cb_period);
        }
        seq_printf(sfile, " %8u", cb_prof_data.cb_prof_rec[idx].cb_trig);
        if(cb_prof_data.cb_prof_rec[idx].cb_trig > cb_prof_data.cb_prof_rec[idx].cb_start){
            /* NOTE: the record of trig-start is invalid. 
                     WHY? cb_trig is set while the cb_start is still the previous value */
            seq_printf(sfile, "   NA"); 
        }else{
            seq_printf(sfile, " %4u", cb_prof_data.cb_prof_rec[idx].cb_start - cb_prof_data.cb_prof_rec[idx].cb_trig);
        }
        seq_printf(sfile, " %4u", cb_prof_data.cb_prof_rec[idx].cb_sch_dur);
        seq_printf(sfile, " %8u", cb_prof_data.cb_prof_rec[idx].cb_start);
        seq_printf(sfile, " %4u", cb_prof_data.cb_prof_rec[idx].cb_job_dur);
        seq_printf(sfile, " %3u", cb_prof_data.cb_prof_rec[idx].cb_job_cnt);
        seq_printf(sfile, " %3u\n", cb_prof_data.cb_prof_rec[idx].cb_stop_cnt);
        
        cb_period_total += cb_prof_data.cb_prof_rec[idx].cb_period;
        cb_dur_total += cb_prof_data.cb_prof_rec[idx].cb_sch_dur;
        
        
        idx++;
        if(idx >= CB_PROF_RECORD_NUM){
            idx = 0;
        }
    }
    seq_printf(sfile, "  sch cnt:%u\n", cb_prof_data.cb_sch_cnt);
    seq_printf(sfile, "  job cnt:%u\n", cb_prof_data.cb_job_cnt);
    seq_printf(sfile, " trig cnt:%u\n", cb_prof_data.cb_trig_cnt);
    seq_printf(sfile, "waste cnt:%u\n", cb_prof_data.cb_waste_cnt);
    seq_printf(sfile, "callback record:\n");
    up(&cb_prof_data.sem);
    
    if(cb_period_total != 0){
        seq_printf(sfile, "cb:%2u.%02u%% %u/%u\n", cb_dur_total * 100 / cb_period_total, (cb_dur_total * 10000 / cb_period_total) % 100,  cb_dur_total, cb_period_total);
    }else{
        seq_printf(sfile, "cb:NA\n");
    }


    seq_printf(sfile, "\n[ ISR profiling ]\n");
    favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for isr_prof_data */
    for(i = 0; i < ENTITY_ENGINES; i++){
        unsigned int isr_period_total = 0;
        unsigned int isr_dur_total = 0;
        
        head_str1 = "                                                      job";
        head_str2 = "idx period   dur   start  rec dec mak buf trg wrk sta cnt";
        head_str3 = "== ======== ==== ======== === === === === === === === ===";
        
        if(isr_prof_data[i].isr_cnt < ISR_PROF_RECORD_NUM){
            idx = 0;
        }else{
            idx = isr_prof_data[i].isr_cnt % ISR_PROF_RECORD_NUM;
        }
        for(j = 0; j < ISR_PROF_RECORD_NUM; j++){
            
            if(j >= isr_prof_data[i].isr_cnt){
                break;
            }
            
            if(j % 32 == 0){
                seq_printf(sfile, "%s\n", head_str3);
                seq_printf(sfile, "%s\n", head_str1);
                seq_printf(sfile, "%s\n", head_str2);
                seq_printf(sfile, "%s\n", head_str3);
            }
            seq_printf(sfile, "%2d", j);
            if(isr_prof_data[i].isr_prof_rec[idx].isr_period & (1 << 31)){ /* detect incorrect period due to timestamp overflow */
                seq_printf(sfile, " %8s", "NA");
            }else{
                seq_printf(sfile, " %8u", isr_prof_data[i].isr_prof_rec[idx].isr_period);
            }
            seq_printf(sfile, " %4u", isr_prof_data[i].isr_prof_rec[idx].isr_dur);
            seq_printf(sfile, " %8u", isr_prof_data[i].isr_prof_rec[idx].isr_start);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_rec_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_dec_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_mark_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_buf_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_trig_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_work_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_start_job_dur);
            seq_printf(sfile, " %3u", isr_prof_data[i].isr_prof_rec[idx].isr_start_job_cnt);
            seq_printf(sfile, "\n");

            isr_period_total += isr_prof_data[i].isr_prof_rec[idx].isr_period;
            isr_dur_total += isr_prof_data[i].isr_prof_rec[idx].isr_dur;
            
            idx++;
            if(idx >= ISR_PROF_RECORD_NUM){
                idx = 0;
            }
        }

        seq_printf(sfile, "engine:%d isr cnt:%8u max isr dur:%3u min isr dur:%3u\n", i,
            isr_prof_data[i].isr_cnt,
            isr_prof_data[i].max_isr_dur,
            isr_prof_data[i].min_isr_dur);

        if(isr_period_total != 0){
            seq_printf(sfile, "isr:%2u.%02u%% %u/%u\n", isr_dur_total * 100 / isr_period_total, (isr_dur_total * 10000 / isr_period_total) % 100,  isr_dur_total, isr_period_total);
        }else{
            seq_printf(sfile, "isr:NA\n");
        }

        seq_printf(sfile, "\n");
    }
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);/* unlock for isr_prof_data */
    
    
    return 0;
}


static ssize_t favcd_proc_prof_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
#if 0
//    int opt, val;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

#if 0    
    sscanf(value_str, "%d", &level);

    log_level = level;
    printk("\nconfig level =%d (0:emerge 1:error 2:debug)\n", log_level);
#endif
#endif

    return count;
}


/* 
 * start of type/structure for param proc node 
 */
#define UINT32  0
#define INT32   1
#define UINT8   2
#define INT8    3
#define HEX8    4
#define STR_T   5

typedef struct mapping_st
{
    char *tokenName;
    void *value;
    int type;
    /* 0: uint32_t, 1: int32_t, 2: uint8_t, 3: int8_t, 4: hex, 5: char array 
     * unimplemented: 5: double, 6: 4 integer, 7: int array
     */
    int lb;
    int ub;
    char *note;
} MapInfo;


static const MapInfo syntax[] = 
{
    {"SaveBsCount",             &save_bs_cnt,            INT32, 0, 300, "number of input bitstreams to be saved (in job number). rest to 0 after saving."},
    {"SaveMBinfoCount",         &save_mbinfo_cnt,       UINT32, 0, 300, "number of mbinfo to be saved (in job number). rest to 0 after saving."},
    {"SaveOutputCount",         &save_output_cnt,       UINT32, 0, 300, "number of output to be saved (in job number). rest to 0 after saving."},
    {"DmpFilePath",            file_path_buf,          STR_T, 0, sizeof(file_path_buf), "path for saving file"},
    {"LosePicFlag",             &lose_pic_handle_flags, UINT32, 0, 3,   "lose picture handling flags: bit 0: whether to print error message. bit 1: whether to return error"},
    {NULL,                      NULL,                        0, 0, 0, NULL}
};

/* 
 * end of type/structure for param proc node 
 */


static int favcd_proc_param_show(struct seq_file *sfile, void *v)
{
    int i = 0;
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;
    char *str_ptr;
    
    seq_printf(sfile, "Usage: echo [parameter name] [value] > /proc/videograph/h264e/param\n");
    seq_printf(sfile, "\n");
    seq_printf(sfile, "                                range\n");
    seq_printf(sfile, "    parameter name       value   /size             note\n");
    seq_printf(sfile, "======================= ======= ====== ==============================\n");
    while (syntax[i].tokenName) {
        seq_printf(sfile, "%-23s ", syntax[i].tokenName);
        switch (syntax[i].type) {
            case UINT32:
                u32Val = (unsigned int*)syntax[i].value;
                seq_printf(sfile, "%7u", *u32Val);
                seq_printf(sfile, " %d~%3d", syntax[i].lb, syntax[i].ub);
                break;
            case INT32:
                i32Val = (int*)syntax[i].value;
                seq_printf(sfile, "%7d", *i32Val);
                seq_printf(sfile, " %d~%3d", syntax[i].lb, syntax[i].ub);
                break;
            case UINT8:
                u8Val = (unsigned char*)syntax[i].value;
                seq_printf(sfile, "%7u", *u8Val);
                seq_printf(sfile, " %d~%3d", syntax[i].lb, syntax[i].ub);
                break;
            case HEX8:
                u8Val = (unsigned char*)syntax[i].value;
                seq_printf(sfile, "0x%07X", *u8Val);
                seq_printf(sfile, " %d~%3d", syntax[i].lb, syntax[i].ub);
                break;
            case INT8:
                i8Val = (char *)syntax[i].value;
                seq_printf(sfile, "%7d", *i8Val);
                seq_printf(sfile, " %d~%3d", syntax[i].lb, syntax[i].ub);
                break;
            case STR_T:
                str_ptr = (char *)syntax[i].value;
                seq_printf(sfile, "\"%s\"", str_ptr);
                seq_printf(sfile, " %6d", syntax[i].ub);
                break;
            default:
                break;
        }
        if (syntax[i].note)
            seq_printf(sfile, "   %s", syntax[i].note);
        seq_printf(sfile, "\n");
        i++;
    }

    
    return 0;
}



static ssize_t favcd_proc_param_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int i;
    int len = count;
    int value;
    char cmd_str[40] = {0};
    char str[80];
    int idx;
    
    unsigned int *u32Val;
    int *i32Val;
    unsigned char *u8Val;
    signed char *i8Val;
    char *str_ptr;
    char *param_str_ptr;
    int param_str_size;
    int str_buf_size;

    int change_variable = 0;

    
    if (copy_from_user(str, buffer, len))
        return -EFAULT;

    if(len > sizeof(str) - 1){
        len = sizeof(str) - 1; 
    }
    str[len] = '\0';

    memset(cmd_str, sizeof(cmd_str), 0);
    
    sscanf(str, "%s %d\n", cmd_str, &value);

    idx = 0;
    while (syntax[idx].tokenName) {
        if (strcmp(syntax[idx].tokenName, cmd_str) == 0){
            break;
        }
        idx++;
    }

    printk("idx:%d\n", idx);
    
    if (syntax[idx].tokenName == NULL) {
        printk("unknown \"%s\"", cmd_str);
        goto err_ret;
    }

    /* check range/size */
    switch (syntax[idx].type) {
        case UINT32:
        case INT32:
        case UINT8:
        case INT8:
            if (value < syntax[idx].lb || value > syntax[idx].ub){
                printk("%s(%d) is out of range! (%d ~ %d)\n", syntax[idx].tokenName, value, syntax[idx].lb, syntax[idx].ub);
                goto err_ret;
            }
            break;
            
        case STR_T:
            /* check latter */
            break;
    }

    /* get value */
    switch (syntax[idx].type) {
        case UINT32:
            u32Val = (unsigned int *)syntax[idx].value;
            if (*u32Val != value)
                change_variable = 1;
            *u32Val = value;
            break;
        case INT32:
            i32Val = (int *)syntax[idx].value;
            if (*i32Val != value)
                change_variable = 1;
            *i32Val = value;
            break;
        case UINT8:
        case HEX8:
            u8Val = (unsigned char *)syntax[idx].value;
            if (*u8Val != value)
                change_variable = 1;
            *u8Val = value;
            break;
        case INT8:
            i8Val = (char *)syntax[idx].value;
            if (*i8Val != value)
                change_variable = 1;
            *i8Val = value;
            break;
        case STR_T:
            str_ptr = (char *)syntax[idx].value;
            str_buf_size = syntax[idx].ub;

            param_str_ptr = str + strlen(syntax[idx].tokenName) + 1;
            param_str_size = strlen(param_str_ptr);

            if(param_str_size >= str_buf_size){
                printk("%s(%d) is out of range! (%d)\n", syntax[idx].tokenName, param_str_size, str_buf_size);
                goto err_ret;
            }

            if(strncmp(str_ptr, param_str_ptr, param_str_size) != 0){
                change_variable = 1;
            }
            
            strncpy(str_ptr, param_str_ptr, param_str_size);
            str_ptr[param_str_size] = 0;
            
            /* remove the trailing char: '\n' */
            for(i = 0; i < param_str_size; i++){
                if(str_ptr[i] == '\n'){
                    str_ptr[i] = 0;
                }
                if(str_ptr[i] == 0){
                    break;
                }
            }
            break;
        default:
            break;
    }
    
err_ret:
    return count;
}

enum {
    F_MIN_FUNC_IDX = 100,
    F_GET_MBINFO = F_MIN_FUNC_IDX,
    F_STOP,
    F_DAMNIT,
    F_CLK_ON,
    F_CLK_OFF,
    F_RST_ENG,
    F_MAX_FUNC_IDX,
};

static void __dispatch_func(int func_idx, int val, int val2, int usage)
{
    
    switch(func_idx){
#ifdef  TEST_GET_MBINFO
        case F_GET_MBINFO:
            if(usage){
                printk("[%d]favcd_get_mbinfo [ch] [fmt]\n", F_GET_MBINFO);
                break;
            }
            printk("dump mbinfo of minor:%d\n", val);
            {
                int ret;
                int minor = val;
                int size = PAGE_SIZE;
                int fmt = val2;
                
                ret = favcd_get_mbinfo(minor, fmt, (u32 *)mbinfo_buf, size);
                printk("ret: %d\n", ret);
                if(ret == 0){
                    switch(fmt){
                        case FAVCD_MBINFO_FMT_RAW:
                            print_mbinfo(mbinfo_buf);
                            break;
                        case FAVCD_MBINFO_FMT_16X16_BLK:
                            print_mv16x16((u32 *)mbinfo_buf);
                            break;
                    }
                }
            }
            break;
#endif /* TEST_GET_MBINFO */
        case F_STOP: // call stop
            if(usage){
                printk("[%d]stop [ch]\n", F_STOP);
                break;
            }
            favcd_stop(NULL, 0, val);
            break;
        case F_DAMNIT:
            if(usage){
                printk("[%d]FAVCD_DAMNIT\n", F_DAMNIT);
                break;
            }
            FAVCD_DAMNIT("damnit triggered by proc");
            break;
        case F_CLK_ON:
            if(usage){
                printk("[%d]pf_mcp300_clk_on\n", F_CLK_ON);
                break;
            }
            printk("calling pf_mcp300_clk_on()\n");
            pf_mcp300_clk_on();
            break;
        case F_CLK_OFF:
            if(usage){
                printk("[%d]pf_mcp300_clk_off\n", F_CLK_OFF);
                break;
            }
            printk("calling pf_mcp300_clk_off()\n");
            pf_mcp300_clk_off();
            break;
        case F_RST_ENG:
            if(usage){
                printk("[%d]H264Dec_ResetEgnine [engine]\n", F_RST_ENG);
                break;
            }
            printk("calling H264Dec_ResetEgnine() for engine: %d\n", val);
            if(val < ENTITY_ENGINES){
                H264Dec_ResetEgnine(&favcd_engine_info[val]);
            }
            break;
        default:
            printk("unknown func_idx: %d (valid func_idx:%d~%d)\n", func_idx, F_MIN_FUNC_IDX, F_MAX_FUNC_IDX);
            break;
    }

}



static int favcd_proc_dbgmode_show(struct seq_file *sfile, void *v)
{
    int i;
    int rec_ch_cnt = 0;
    extern char file_path_buf[128];
    extern char *file_path;
    seq_printf(sfile, "Usage: echo [dbg_cmd] > /proc/videograph/h264d/dbgmode\n");
    seq_printf(sfile, "       [dbg_cmd] can be:\n");
    seq_printf(sfile, "           [value]: debug mode value\n");
    seq_printf(sfile, "           path [rec_file_path]: set path of the recording file\n");
    seq_printf(sfile, "           rec [1(enable)/0(disable)] [ch_start] [ch_end]: enable/disable bitstream recording for ch_start~ch_end\n");
    seq_printf(sfile, "           func [func_idx] [param1] [param2]: call functions with param1, param2 \n");
    seq_printf(sfile, "           usage: print usage of all functions \n");
    seq_printf(sfile, "\n");
    seq_printf(sfile, "    Debug mode: %d (0:disabled >0:enabled)\n", dbg_mode);
    seq_printf(sfile, "Recording path: %s\n", file_path);
    seq_printf(sfile, "Recording chan: ");
    for(i = 0; i < chn_num; i++){
        if(private_data[i].rec_en){
            seq_printf(sfile, "%d ", i);
            rec_ch_cnt++;
        }
    }
    seq_printf(sfile, "(total %d channel recording enabled)\n", rec_ch_cnt);

    if(detail_config == 0) /* for hinding the following message */
        return 0;

    seq_printf(sfile, "%25s ", "dbg_mode ctrl behavior");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", i);
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "TREAT_WARN_AS_ERR");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", TREAT_WARN_AS_ERR(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "PRINT_ERR_TO_CONSOLE");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", PRINT_ERR_TO_CONSOLE(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "PRINT_LL_ERR_TO_CONSOLE");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", PRINT_LL_ERR_TO_CONSOLE(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "PRINT_ERR_TO_MASTER");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", PRINT_ERR_TO_MASTER(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "DAMNIT_AT_ERR");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", DAMNIT_AT_ERR(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "SAVE_BS_AT_ERR");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", SAVE_BS_AT_ERR(i));
    }
    seq_printf(sfile, "\n");

    seq_printf(sfile, "%25s ", "PRINT_ERR_IN_ISR");
    for(i = MIN_DBG_MODE; i <= MAX_DBG_MODE; i++){
        seq_printf(sfile, " %d", PRINT_ERR_IN_ISR(i));
    }
    seq_printf(sfile, "\n");
    
    return 0;
}


static ssize_t favcd_proc_dbgmode_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int level;
    char value_str[128] = {'\0'};
    int len = count;
    extern char file_path_buf[128];
    extern char *file_path;
    int i;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    
#if RECORDING_BS
    if(strncmp(value_str, "rec ", 4) == 0){ /* "rec [1|0] ch_start ch_end" */
        int ch_start, ch_end;
        int rec_en = 0;
        ch_start = ch_end = -1;
        sscanf(value_str, "rec %d %d %d", &rec_en, &ch_start, &ch_end);

        if(ch_start > ch_end || ch_start < 0 || ch_end < 0 || ch_start >= chn_num || ch_end >= chn_num){
            printk("invalid chn value: %d %d\n", ch_start, ch_end);
        }

        for(i = ch_start; i <= ch_end; i++){
            private_data[i].rec_en = rec_en;
        }
    }
#endif
    else if(strncmp(value_str, "path ", 5) == 0){    /* "path [dump file path]" */
        strncpy(file_path_buf, value_str + 5, sizeof(file_path_buf));
        
        file_path_buf[sizeof(file_path_buf) - 1] = 0;
        
        /* remove the trailing char: '\n' */
        for(i = 0; i < sizeof(file_path_buf); i++){
            if(file_path_buf[i] == '\n'){
                file_path_buf[i] = 0;
            }
            if(file_path_buf[i] == 0){
                break;
            }
        }
        file_path = file_path_buf;
        printk("\nDump file path = [%s]\n", file_path);
    }
    else if(strncmp(value_str, "func ", 5) == 0){    /* "func [func_idx] [param1] [param2]" */
        int func_idx;
        int param1;
        int param2;
        sscanf(value_str, "func %d %d %d", &func_idx, &param1, &param2);
        __dispatch_func(func_idx, param1, param2, 0);
    }
    else if(strncmp(value_str, "usage", 5) == 0){    /* usage */
        int func_idx;
        for(func_idx = F_MIN_FUNC_IDX; func_idx < F_MAX_FUNC_IDX; func_idx++){
            __dispatch_func(func_idx, 0, 0, 1);
        }
        
    }else{
        sscanf(value_str, "%d", &level);
        dbg_mode = level;
        printk("\nDebug mode = %d (0:disabled >0:enabled)\n", dbg_mode);
    }

    return count;
}


static int favcd_proc_level_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nLog level = %d (0:error 1:warning 2:info 3:trace 4:debug)\n", log_level);
    return 0;
}


static ssize_t favcd_proc_level_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int level;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;
    
    sscanf(value_str, "%d", &level);

    log_level = level;
    printk("\nLog level =%d (0:error 1:warning 2:info 3:trace 4:debug)\n", log_level);

    return count;
}


static int favcd_proc_prep_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nPrepare Period =%d (msecs)\n", prepare_period);
    return 0;
}


static ssize_t favcd_proc_prep_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_set;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &mode_set);
    prepare_period = mode_set;
    printk("\nPrepare Period =%d (msecs)\n", prepare_period);

    return count;
}


static int favcd_proc_cb_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "\nCallback Period =%d (msecs)\n", callback_period);
    return 0;
}


static ssize_t favcd_proc_cb_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_set;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &mode_set);
    callback_period = mode_set;
    printk("\nCallback Period =%d (ticks)\n", callback_period);

    return count;
}


static int favcd_proc_util_show(struct seq_file *sfile, void *v)
{
    int i;

    if(utilization_period == 0){
        seq_printf(sfile, "HW utilization measurement is disabled. set measurment period to non-zero to enable it.\n");
        return 0;
    }

    for (i = 0; i < ENTITY_ENGINES; i++) {
        if (eng_util[i].utilization_record == 0){
            seq_printf(sfile, "Engine%d HW Utilization Period=%d(sec) Utilization=N/A\n", i, utilization_period);
        }else{
            seq_printf(sfile, "Engine%d HW Utilization Period=%d(sec) Utilization=%d\n", i, utilization_period, eng_util[i].utilization_record);
        }
    }
    return 0;
}


static ssize_t favcd_proc_util_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int val;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &val);
    utilization_period = val;
    printk("\nUtilization Period =%d(sec)\n", utilization_period);

    return count;
}


static int favcd_proc_vg_info_show(struct seq_file *sfile, void *v)
{
    seq_printf(sfile, "#H264 decoder information:\n");
    seq_printf(sfile, "B_FRAME_NR: %d\n", mcp300_b_frame);
    seq_printf(sfile, "ENGINE_NR: %d\n", ENTITY_ENGINES);
    return 0;
}


static ssize_t favcd_proc_vg_info_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return count;
}


static int favcd_proc_buf_num_show(struct seq_file *sfile, void *v)
{
    
    seq_printf(sfile, "\nrefernece buffer number = %d\n", mcp300_max_buf_num);
    return 0;
}


static ssize_t favcd_proc_buf_num_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int buf_num;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &buf_num);
    mcp300_max_buf_num = buf_num;
    printk("\nrefernece buffer number = %d\n", mcp300_max_buf_num);

    return count;
}

static int favcd_proc_jobprof_show(struct seq_file *sfile, void *v)
{
    int i;
    int idx;
    struct job_prof_record *job_prof_ptr;
    char *head_str1;
    char *head_str2;
    char *head_str3;

    head_str1 = "        put callback en      put-  start-       put    put  put-  put-  put-   put-                                       scale/";
    head_str2 = "idx     seq      seq g ch callback finish        ts period  prep ready start finish err poc   DST W/H    NC W/H    BG W/H field IDR    size";
    head_str3 = "==  ======= ======== = == ======= ======= ========= ====== ===== ===== ===== ====== === === =========  ======== ========= == == === =======";

    /* find index of min job_prof_ptr->cb_seq_num */
    idx = 0;
    for(i = 0; i < JOB_PROF_RECORD_NUM; i++){
        if(job_prof_data.job_prof_record[idx].cb_seq_num > job_prof_data.job_prof_record[i].cb_seq_num){
            idx = i;
        }
    }
    
    for(i = 0; i < JOB_PROF_RECORD_NUM; i++){
        job_prof_ptr = &job_prof_data.job_prof_record[idx];

        if(i >= job_prof_data.job_cnt){
            break;
        }
        if(i % 32 == 0){
            seq_printf(sfile, "%s\n", head_str3);
            seq_printf(sfile, "%s\n", head_str1);
            seq_printf(sfile, "%s\n", head_str2);
            seq_printf(sfile, "%s\n", head_str3);
        }
        seq_printf(sfile, 
            "%2d %8d %8d %1d %2d %7d %7d  %8d %6d %5d %5d %5d %6d %3d %3d %4d/%4d %4d/%4d %4d/%4d %2d %2d %3s %7d\n", 
            i, 
            job_prof_ptr->seq_num,
            job_prof_ptr->cb_seq_num,
            job_prof_ptr->engine,
            job_prof_ptr->minor,
            (job_prof_ptr->callback_time == 0)?0:(job_prof_ptr->callback_time - job_prof_ptr->put_time),
            (job_prof_ptr->finish_time == 0)?0:(job_prof_ptr->finish_time - job_prof_ptr->start_time),
            job_prof_ptr->put_time,
            job_prof_ptr->put_period,
            (job_prof_ptr->prepare_time == 0)?0:(job_prof_ptr->prepare_time - job_prof_ptr->put_time),
            (job_prof_ptr->ready_time == 0)?0:(job_prof_ptr->ready_time - job_prof_ptr->put_time),
            (job_prof_ptr->start_time == 0)?0:(job_prof_ptr->start_time - job_prof_ptr->put_time),
            (job_prof_ptr->finish_time == 0)?0:(job_prof_ptr->finish_time - job_prof_ptr->put_time),
            job_prof_ptr->err_num, 
            job_prof_ptr->poc, 
            (job_prof_ptr->dst_dim >> 16) & 0xFFFF, 
            (job_prof_ptr->dst_dim) & 0xFFFF, 
            (job_prof_ptr->dst_dim_non_cropped >> 16) & 0xFFFF, 
            (job_prof_ptr->dst_dim_non_cropped) & 0xFFFF, 
            (job_prof_ptr->dst_bg_dim >> 16) & 0xFFFF, 
            (job_prof_ptr->dst_bg_dim) & 0xFFFF, 
            (job_prof_ptr->flags & JOB_ITEM_FLAGS_SUB_YUV_EN)?1:0, 
            (job_prof_ptr->flags & JOB_ITEM_FLAGS_SRC_INTERLACE_EN)?1:0, 
            job_prof_ptr->is_idr_flg?"IDR":" ",
            job_prof_ptr->in_size);

        idx = (idx + 1) % JOB_PROF_RECORD_NUM;
    }
    seq_printf(sfile, "job cnt: %d\n", job_prof_data.job_cnt);

    return 0;
}


static ssize_t favcd_proc_jobprof_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return count;
}


static int favcd_proc_job_show(struct seq_file *sfile, void *v)
{
    struct video_entity_job_t *job;
    struct favcd_job_item_t *job_item;
    int total_job_cnt = 0;
    int ch_job_cnt = 0;
    
    seq_printf(sfile, "\nMinor=%d (<0 means all) System ticks=0x%x\n", job_minor, (int)jiffies&0xffff);

    seq_printf(sfile, "Engine Minor  Job_ID   Status   Puttime    start   end\n");
    seq_printf(sfile, "===========================================================\n");

    DRV_LOCK_G(); /* lock for global list */
    list_for_each_entry(job_item, &favcd_global_head, global_list) {
        job = (struct video_entity_job_t *)job_item->job;
        total_job_cnt++;
        if(job_minor >= 0 && job_item->minor != job_minor){
            continue; /* skip */
        }
        seq_printf(sfile, "%d      %02d    %07d   %s   0x%04x   0x%04x  0x%04x\n",
            job_item->engine, job_item->minor, job->id, 
            favcd_job_status_long_str(job_item->status), job_item->puttime & 0xffff,
            job_item->starttime & 0xffff, (int)job_item->finishtime & 0xffff);
        ch_job_cnt++;
    }
    DRV_UNLOCK_G(); /* unlock for global list */

    if(job_minor >= 0){
        seq_printf(sfile, "ch %d job number: %d\n", job_minor, ch_job_cnt);
    }
    seq_printf(sfile, "total job number: %d\n", total_job_cnt);
    
    seq_printf(sfile, "Usage: echo <chn> > /proc/videograph/h264d/job\n");
    seq_printf(sfile, "       <chn>: channel index. (<0 means all channel)\n");

    return 0;
}


static ssize_t favcd_proc_job_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int job_minor_val = 0;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &job_minor_val);
    job_minor = job_minor_val;

    printk("\nchannel=%d (<0 means all)\n", job_minor);

    return count;
}

struct reg_dump_range {
    unsigned int offset;
    unsigned int size;
};

static int favcd_proc_eng_reg_show(struct seq_file *sfile, void *v)
{
    int i = 0, j;
    unsigned long flags;
    unsigned int address, size;
    unsigned int phy_addr;
    unsigned int offset;
    int engine = eng_reg_engine;
    struct reg_dump_range base_dump_range[] = {
            {   .offset = 0,                     .size = 0x80,  },     /* offset 0x00000 */
            {   .offset = H264_OFF_SYSM,         .size = 0x80,  },     /* offset 0x10c00 */
            {   .offset = H264_OFF_SYSM + 0x100, .size = 0x30,  },     /* offset 0x10d00 */
            {   .offset = H264_OFF_POCM,         .size = 4 * 32 * 2, },/* offset 0x20000 */
        };
    struct reg_dump_range vc_dump_range[] = {
            {   .offset = 0,                     .size = 0x44,  },     /* offset 0x00 */
        };

    favcd_spin_lock_irqsave(&favc_dec_lock, flags);
    
    /* dump register */
    seq_printf(sfile, "register of engine %d:\n", engine);

    /* MCP300 */
    for(j = 0; j < ARRAY_SIZE(base_dump_range); j++){
        offset = base_dump_range[j].offset;
        size = base_dump_range[j].size;
        phy_addr = (unsigned int)favcd_engine_info[engine].u32phy_addr + offset;
        address = (unsigned int)favcd_engine_info[engine].pu32BaseAddr + offset;
        
        seq_printf(sfile, "offset: 0x%08X ~ 0x%08X\n", offset, offset + size);
        for (i = 0; i < size; i = i + 0x10) {
            if(offset == 0 && i == 0x60){ 
                /* to skip reading some registers that may cause decoding error */
                seq_printf(sfile, "0x%08x  0x-------- 0x-------- 0x-------- 0x%08x\n", (phy_addr + i), 
                    *(unsigned int *)(address + i + 0xc));
            }else{
                seq_printf(sfile, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x\n", (phy_addr + i), 
                    *(unsigned int *)(address + i),     *(unsigned int *)(address + i + 4), 
                    *(unsigned int *)(address + i + 8), *(unsigned int *)(address + i + 0xc));
            }
        }
        seq_printf(sfile, "\n");
    }


    /* vcache */
    for(j = 0; j < ARRAY_SIZE(vc_dump_range); j++){
        offset = base_dump_range[j].offset;
        size = base_dump_range[j].size;
        phy_addr = (unsigned int)favcd_engine_info[engine].u32vc_phy_addr + offset;
        address = (unsigned int)favcd_engine_info[engine].pu32VcacheBaseAddr + offset;
        
        seq_printf(sfile, "VC offset: 0x%08X ~ 0x%08X\n", offset, offset + size);
        for (i = 0; i < size; i = i + 0x10) {
            seq_printf(sfile, "0x%08x  0x%08x 0x%08x 0x%08x 0x%08x\n", (phy_addr + i), 
                *(unsigned int *)(address + i),     *(unsigned int *)(address + i + 4), 
                *(unsigned int *)(address + i + 8), *(unsigned int *)(address + i + 0xc));
        }
        seq_printf(sfile, "\n");
    }
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);
    
    seq_printf(sfile, "Usage: echo <eng_idx> > /proc/videograph/h264d/eng_regs\n");

    return 0;
}

static ssize_t favcd_proc_eng_reg_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_engine = 0;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &mode_engine);

    if(mode_engine < 0 || mode_engine >= ENTITY_ENGINES){
        printk("engine number out of ranges: %d", mode_engine);
        return count;
    }

    eng_reg_engine = mode_engine;
    printk("\nLookup engine=%d\n", eng_reg_engine);

    return count;
}

static int favcd_proc_fs_fs_show(struct seq_file *sfile, void *v)
{
    int idx;
	struct favcd_data_t *dec_data;
	DecoderParams *dec_parm;

	idx = fs_ch ;
	dec_data = private_data + idx;
		
	if(dec_data == NULL){
		seq_printf(sfile, "\n");
	}
	
	dec_parm = dec_data->dec_handle;
	if(dec_parm != NULL){
		seq_printf(sfile, "%d\n", dec_parm->u16FrNumSkipError);	
	}	
	
    return 0;
}

static ssize_t favcd_proc_fs_fs_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	int clear_flg;	
	int len = count;
	char value_str[16] = {'\0'};
    
	struct favcd_data_t *dec_data;
	DecoderParams *dec_parm;

    if (len > sizeof(value_str) - 1){
    	len = sizeof(value_str) - 1; 
    }

    if (copy_from_user(value_str, buffer, len))
    	return -EFAULT;

    sscanf(value_str, "%d",&clear_flg);

	if (clear_flg == 0) {
		dec_data = private_data + fs_ch;
		if(dec_data != NULL){
			dec_parm = dec_data->dec_handle;
			if(dec_parm != NULL){
				dec_parm->u16FrNumSkipError = 0;	
			}	
		}
	}
	
    return count;
}

static int favcd_proc_fs_ch_show(struct seq_file *sfile, void *v)
{
	seq_printf(sfile, "chn: %2d \n", fs_ch);	
    return 0;
}

static ssize_t favcd_proc_fs_ch_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	int temp_fs_ch;
	int len = count;
	char value_str[16] = {'\0'};
    
    if (len > sizeof(value_str) - 1){
    	len = sizeof(value_str) - 1; 
    }

    if (copy_from_user(value_str, buffer, len))
    	return -EFAULT;

    sscanf(value_str, "%d",&temp_fs_ch);

	if ((temp_fs_ch < 0) || (temp_fs_ch > 63)) {
		printk("channel number error %d\n", temp_fs_ch);
		return -EFAULT;
	}

	fs_ch = temp_fs_ch ;
	
    return count;
}


static int favcd_proc_property_show(struct seq_file *sfile, void *v)
{
    int i = 0;
    struct property_map_t *map;
    unsigned long flags;
    int idx = MAKE_IDX(chn_num, 0, property_minor);
    int prop_dir = 0;

    favcd_spin_lock_irqsave(&favc_dec_lock, flags);

    seq_printf(sfile, "\n%s ch %d job %d err_num:%d (%s)\n", property_record[idx].entity->name,
        property_minor, property_record[idx].job_id, property_record[idx].err_num, favcd_err_str(property_record[idx].err_num));
    seq_printf(sfile, "=============================================================\n");
    seq_printf(sfile, "ID       Name(string)     Value(hex) Value(dec)  Readme\n");
    seq_printf(sfile, "[input property]\n");

    for(i = 0; i < MAX_RECORD; i++){
        unsigned int id, value;
        id = property_record[idx].property[i].id;
        if (id == ID_NULL){
            if(prop_dir == 0){ /* end of input property */
                seq_printf(sfile, "[output property]\n");
                prop_dir = 1;
                continue;
            }
            /* end of output property */
            break;
        }

        value = property_record[idx].property[i].value;
        map = favcd_get_propertymap(id);
        if (map) {
            if(id == ID_DST_BG_DIM || id == ID_DST_DIM || id == ID_DST_XY){
                seq_printf(sfile, "%-3d  %19s  %08X    %4dx%4d  %s\n", id, map->name, value, (value >> 16), (value & 0xFFFF), map->readme);
            }else{
                seq_printf(sfile, "%-3d  %19s  %08X     %8d  %s\n", id, map->name, value, value, map->readme);
            }
        }
        
    }
    
    favcd_spin_unlock_irqrestore(&favc_dec_lock, flags);

    seq_printf(sfile, "Usage: echo <chn_idx> > /proc/videograph/h264d/property\n");

    return 0;
}


static ssize_t favcd_proc_property_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    int mode_engine = 0, mode_minor = 0;
    char value_str[16] = {'\0'};
    int len = count;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%d", &mode_minor);



    if(mode_minor < 0 || mode_minor >= chn_num){
        printk("minor number out of ranges: %d", mode_engine);
        return count;
    }
    
    property_minor = mode_minor;
    printk("\nLookup minor=%d\n", property_minor);

    return count;
}


static char *get_dump_cond_str(enum dump_cond cond)
{
    char *str;
    switch(cond){
        case D_BUF_FR:     str = "FR"; break;
        case D_BUF_REF:    str = "REF"; break;
        case D_BUF_INFO:   str = "INFO"; break;
        case D_BUF_IOCTL:  str = "IOCTL"; break;
        case D_BUF_LIST:   str = "LIST"; break;
        case D_BUF_SLICE:  str = "SLICE"; break;
        case D_BUF_DETAIL: str = "DETAIL"; break;
        case D_BUF_ERR:    str = "BUF_ERR"; break;
        case D_PARSE:      str = "PARSE"; break;
        case D_NPF:        str = "NPF"; break;
        case D_ERR:        str = "ERR"; break;
        case D_SW_PARSE:   str = "SW_PARSE"; break;
        case D_NALU:       str = "NALU"; break;
        case D_ISR:        str = "ISR"; break;
        case D_LL:         str = "LL"; break;
        case D_VG_BUF:     str = "VG_BUF"; break;
        case D_ENG:        str = "ENG"; break;
        case D_RES:        str = "RES"; break;
        case D_REC_BS:     str = "REC_BS"; break;
        case D_REV:        str = "REV"; break;
        case D_VG:         str = "VG"; break;
        default:           str = "UNKNOWN"; break;
    }
    return str;
}

static int favcd_proc_dump_show(struct seq_file *sfile, void *v)
{
    int i;
    
    seq_printf(sfile, " chn  start    end   cond   cur fr  chn  start    end   cond   cur fr\n");
    seq_printf(sfile, "==== ====== ====== ======== ====== ==== ====== ====== ======== ======\n");
    for(i = 0; i < chn_num; i++) {
        DecoderParams *dec_parm = (DecoderParams *)private_data[i].dec_handle;
        seq_printf(sfile, "[%2d] %6d-%6d %08X %6d ", i, dec_parm->u32DumpFrStart, dec_parm->u32DumpFrEnd, dec_parm->u32DumpCond, dec_parm->u32DecodedFrameNumber);
        if(i % 2 == 1)
            seq_printf(sfile, "\n");
    }
    
    seq_printf(sfile, "dump cond:\n");
    for(i = 0; i < D_MAX_DUMP_COND; i++) {
        seq_printf(sfile, "[%08X] %-8s", 1 << i, get_dump_cond_str(i));
        if(i % 4 == 3)
            seq_printf(sfile, "\n");
    }
    seq_printf(sfile, "\n");
    seq_printf(sfile, "\nUsage: echo [ch] [start_fr] [end_fr] [cond] [curr_fr] > /proc/videograph/h264d/dump\n");

    return 0;
}

static ssize_t favcd_proc_dump_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    unsigned int chn = 0, dump_start = 0, dump_end = 0, cond = 0;
    int reset_fr_num = -1;
    char value_str[64] = {'\0'};
    int len = count;
    DecoderParams *dec_parm;

    if(len > sizeof(value_str) - 1){
        len = sizeof(value_str) - 1; 
    }

    if(copy_from_user(value_str, buffer, len))
        return -EFAULT;

    sscanf(value_str, "%u %u %u %x %d", &chn, &dump_start, &dump_end, &cond, &reset_fr_num);

    if(chn >= chn_num){
        printk("channel number out of ranges: %d", chn);
        return count;
    }

    printk("\nchn: %d dump range:%d-%d cond:%08X reset fr_num:%d\n", chn, dump_start, dump_end, cond, reset_fr_num);

    dec_parm = (DecoderParams *)private_data[chn].dec_handle;

    dec_parm->u32DumpFrStart = dump_start;
    dec_parm->u32DumpFrEnd = dump_end;
    dec_parm->u32DumpCond = cond;
    if(reset_fr_num >= 0){
        dec_parm->u32DecodedFrameNumber = reset_fr_num;
    }

    return count;
}



static int favcd_proc_info_show(struct seq_file *sfile, void *v)
{
    int idx;
    struct favcd_data_t *dec_data;
    struct favcd_job_item_t *job_item;
    int i;
    DecoderParams *dec_parm;
    VideoParams   *p_Vid;

#if USE_VG_WORKQ
    seq_printf(sfile, "favcd_cb_wq len: %d\n", (unsigned int)get_workqueue_len(favcd_cb_wq));
#endif //USE_VG_WORKQ
    for(idx = 0; idx < chn_num; idx++) {
        dec_data = private_data + idx;

        //seq_printf("\n");
        seq_printf(sfile, "chn: %2d ", (unsigned int)idx);
        seq_printf(sfile, "dec_data: 0x%08X ", (unsigned int)dec_data);

        if(dec_data == NULL){
            seq_printf(sfile, "\n");
            continue;
        }
        seq_printf(sfile, "eng: %2d ", (unsigned int)dec_data->engine);
        seq_printf(sfile, "mir: %2d ", (unsigned int)dec_data->minor);
        seq_printf(sfile, "dec: 0x%08X ", (unsigned int)dec_data->dec_handle);
        seq_printf(sfile, "used_buf_num: %d ", dec_data->used_buffer_num);
        seq_printf(sfile, "ready_job: %d ", dec_data->ready_job_cnt);
#if RECORDING_BS
        seq_printf(sfile, "rec_en: %d ", dec_data->rec_en);
#endif
        seq_printf(sfile, "\n");

        dec_parm = dec_data->dec_handle;
        if(dec_parm != NULL){
            seq_printf(sfile, "vc: %1X/%1X ", dec_parm->vcache_en_flgs, dec_parm->vcache_en_cfgs);
            seq_printf(sfile, "intr cnt: ");
            seq_printf(sfile, "bl:%d be:%d to:%d dr:%d dw:%d ll:%d fd:%d pd:%d sd:%d vl:%d", 
                dec_parm->u16BSLow, dec_parm->u16BSEmpty, dec_parm->u16Timeout,
                dec_parm->u16DmaRError, dec_parm->u16DmaWError, dec_parm->u16LinkListError, 
                dec_parm->u16FrameDone, dec_parm->u16PicDone, dec_parm->u16SliceDone, dec_parm->u16VldError);
            seq_printf(sfile, " cnt: df:%d dt:%d", dec_parm->u32DecodedFrameNumber, dec_parm->u32DecodeTriggerNumber);
            seq_printf(sfile, " err cnt: fs:%d wi:%d si:%d", dec_parm->u16FrNumSkipError, dec_parm->u16WaitIDR, dec_parm->u16SkipTillIDR);

            p_Vid = dec_parm->p_Vid;
            if(p_Vid){
                seq_printf(sfile, " buf used:%d unoutput:%d max:%d", p_Vid->used_buffer_num, p_Vid->unoutput_buffer_num, p_Vid->max_buffered_num);
                if(p_Vid->active_sps){
                    seq_printf(sfile, " num_ref_frames:%d", p_Vid->active_sps->num_ref_frames);
                }
            }
            seq_printf(sfile, "\n");
            
        }

#if REV_PLAY
        if(dec_parm != NULL){
            int gop_cnt = 0;
            struct favcd_gop_job_item_t *gop_job_item;
            list_for_each_entry(gop_job_item, &favcd_gop_minor_head[dec_data->minor], gop_job_list) {
                seq_printf(sfile, "[%2d]gop job id:%08X size:%d job:%d clean:%d output:%d group:%d seq:%d first_job_seq:%d\n", 
                    gop_cnt, gop_job_item->gop_id, gop_job_item->gop_size, gop_job_item->job_cnt, gop_job_item->clean_cnt, gop_job_item->output_cnt, 
                    gop_job_item->output_group_size, gop_job_item->seq_num, gop_job_item->first_job_seq_num);
                gop_cnt++;
            }
        }
#endif

        //seq_printf(sfile, "unit_in_tick: %d ", (unsigned int)dec_data->unit_in_tick);
        //seq_printf(sfile, "time_scale: %d ", (unsigned int)dec_data->time_scale);
        //seq_printf(sfile, "\n");

        for(i = 0; i < MAX_DEC_BUFFER_NUM; i++) {
            if(dec_data->dec_buf[i].is_used == 0)
                continue;
            seq_printf(sfile, "[%2d]used:%d rel:%d ", i, dec_data->dec_buf[i].is_used, dec_data->dec_buf[i].is_released);
            seq_printf(sfile, "st:0x%08X 0x%08X ", dec_data->dec_buf[i].start_va, dec_data->dec_buf[i].start_pa);
            seq_printf(sfile, "mb:0x%08X 0x%08X ", dec_data->dec_buf[i].mbinfo_va, dec_data->dec_buf[i].mbinfo_pa);
            seq_printf(sfile, "sc:0x%08X 0x%08X ", dec_data->dec_buf[i].scale_va, dec_data->dec_buf[i].scale_pa);
            job_item = favcd_get_linked_job_of_buf(dec_data, i);
            if(job_item){
                seq_printf(sfile, "job id:%2d st:%s 0x%08X outbuf: 0x%08X", job_item->job_id, 
                    favcd_job_status_str(job_item->status), (unsigned int)job_item, 
                    (unsigned int)job_item->res_out_buf);
            }
            seq_printf(sfile, "\n");
        }
    }

    return 0;
}


static ssize_t favcd_proc_info_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return count;
}


static int favcd_proc_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_dump_show, PDE(inode)->data);
}

static int favcd_proc_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_info_show, PDE(inode)->data);
}

static int favcd_proc_eng_reg_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_eng_reg_show, PDE(inode)->data);
}

static int favcd_proc_property_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_property_show, PDE(inode)->data);
}

static int favcd_proc_jobprof_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_jobprof_show, PDE(inode)->data);
}

static int favcd_proc_fs_ch_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_fs_ch_show, PDE(inode)->data);
}

static int favcd_proc_fs_fs_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_fs_fs_show, PDE(inode)->data);
}

static int favcd_proc_job_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_job_show, PDE(inode)->data);
}

static int favcd_proc_buf_num_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_buf_num_show, PDE(inode)->data);
}

static int favcd_proc_vg_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_vg_info_show, PDE(inode)->data);
}

static int favcd_proc_util_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_util_show, PDE(inode)->data);
}

static int favcd_proc_cb_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_cb_show, PDE(inode)->data);
}

static int favcd_proc_prep_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_prep_show, PDE(inode)->data);
}

static int favcd_proc_dbgmode_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_dbgmode_show, PDE(inode)->data);
}

static int favcd_proc_level_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_level_show, PDE(inode)->data);
}

static int favcd_proc_config_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_config_show, PDE(inode)->data);
}

static int favcd_proc_prof_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_prof_show, PDE(inode)->data);
}

static int favcd_proc_param_open(struct inode *inode, struct file *file)
{
    return single_open(file, favcd_proc_param_show, PDE(inode)->data);
}



static struct file_operations favcd_proc_dump_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_dump_open,
    .write  = favcd_proc_dump_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_info_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_info_open,
    .write  = favcd_proc_info_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_eng_reg_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_eng_reg_open,
    .write  = favcd_proc_eng_reg_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_property_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_property_open,
    .write  = favcd_proc_property_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_jobprof_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_jobprof_open,
    .write  = favcd_proc_jobprof_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_fs_ch_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_fs_ch_open,
    .write  = favcd_proc_fs_ch_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_fs_fs_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_fs_fs_open,
    .write  = favcd_proc_fs_fs_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_job_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_job_open,
    .write  = favcd_proc_job_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_buf_num_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_buf_num_open,
    .write  = favcd_proc_buf_num_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_vg_info_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_vg_info_open,
    .write  = favcd_proc_vg_info_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_util_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_util_open,
    .write  = favcd_proc_util_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_cb_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_cb_open,
    .write  = favcd_proc_cb_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_prep_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_prep_open,
    .write  = favcd_proc_prep_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_level_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_level_open,
    .write  = favcd_proc_level_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_dbgmode_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_dbgmode_open,
    .write  = favcd_proc_dbgmode_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_config_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_config_open,
    .write  = favcd_proc_config_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_prof_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_prof_open,
    .write  = favcd_proc_prof_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};

static struct file_operations favcd_proc_param_ops = {
    .owner  = THIS_MODULE,
    .open   = favcd_proc_param_open,
    .write  = favcd_proc_param_write,
    .read   = seq_read,
    .llseek = seq_lseek,
    .release= single_release,
};



void favcd_proc_close(void)
{
    if (cfg_proc)
        remove_proc_entry(cfg_proc->name, h264d_entry_proc);
    if (cbproc != 0)
        remove_proc_entry(cbproc->name, h264d_entry_proc);
    if (prepproc != 0)
        remove_proc_entry(prepproc->name, h264d_entry_proc);
    if (utilproc != 0)
        remove_proc_entry(utilproc->name, h264d_entry_proc);
    if (eng_reg_proc != 0)
        remove_proc_entry(eng_reg_proc->name, h264d_entry_proc);
    if (propertyproc != 0)
        remove_proc_entry(propertyproc->name, h264d_entry_proc);
    if (jobprofproc != 0)
        remove_proc_entry(jobprofproc->name, h264d_entry_proc);
	if (fs_chproc!= 0)
        remove_proc_entry(fs_chproc->name, fsproc);
	if (fs_fsproc!= 0)
        remove_proc_entry(fs_fsproc->name, fsproc);
	if (fsproc!= 0)
        remove_proc_entry(fsproc->name, h264d_entry_proc);	
    if (jobproc != 0)
        remove_proc_entry(jobproc->name, h264d_entry_proc);
    if (dbgmodeproc)
        remove_proc_entry(dbgmodeproc->name, h264d_entry_proc);
    if (levelproc != 0)
        remove_proc_entry(levelproc->name, h264d_entry_proc);
    if (buf_num_proc)
        remove_proc_entry(buf_num_proc->name, h264d_entry_proc);
    if (infoproc != 0)
        remove_proc_entry(infoproc->name, h264d_entry_proc);
    if (vg_infoproc)
        remove_proc_entry(vg_infoproc->name, h264d_entry_proc);
    if (profproc)
        remove_proc_entry(profproc->name, h264d_entry_proc);
    if (paramproc)
        remove_proc_entry(paramproc->name, h264d_entry_proc);
    if (dumpproc)
        remove_proc_entry(dumpproc->name, h264d_entry_proc);
    if (h264d_entry_proc != 0)
        remove_proc_entry(ENTITY_PROC_NAME, 0); /* NOTE: can not use h264d_entry_proc->name */

	return;
}


int favcd_proc_init(void)
{
    h264d_entry_proc = create_proc_entry(ENTITY_PROC_NAME, S_IFDIR | S_IRUGO | S_IXUGO, NULL);
    if (h264d_entry_proc == NULL) {
        printk("Error to create driver proc, please insert log.ko first.\n");
        goto fail_init_proc;
    }

	fsproc = create_proc_entry("fs", S_IFDIR|S_IRUGO|S_IXUGO, h264d_entry_proc);    
	if(fsproc == NULL) {        
		printk("error to create %s/fs proc\n", ENTITY_PROC_NAME);  
		goto fail_init_proc;
	}

	fs_chproc = create_proc_entry("ch", S_IRUGO|S_IXUGO, fsproc); 
	if(fs_chproc == NULL) {        
		printk("error to create %s/fs/ch proc\n", ENTITY_PROC_NAME);  
		goto fail_init_proc;
	}
	fs_chproc->proc_fops  = &favcd_proc_fs_ch_ops;
    fs_chproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    fs_chproc->owner      = THIS_MODULE;
#endif

	fs_fsproc = create_proc_entry("fs", S_IRUGO|S_IXUGO, fsproc); 
	if(fs_fsproc == NULL) {        
		printk("error to create %s/fs/fs proc\n", ENTITY_PROC_NAME);  
		goto fail_init_proc;
	}
	fs_fsproc->proc_fops  = &favcd_proc_fs_fs_ops;
    fs_fsproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    fs_fsproc->owner      = THIS_MODULE;
#endif

    paramproc = create_proc_entry("param", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(paramproc == NULL) {
        printk("error to create %s/param proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    paramproc->proc_fops  = &favcd_proc_param_ops;
    paramproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    paramproc->owner      = THIS_MODULE;
#endif

    profproc = create_proc_entry("profiling", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(profproc == NULL) {
        printk("error to create %s/profiling proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    profproc->proc_fops  = &favcd_proc_prof_ops;
    profproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    profproc->owner      = THIS_MODULE;
#endif


    dumpproc = create_proc_entry("dump", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(dumpproc == NULL) {
        printk("error to create %s/dump proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    dumpproc->proc_fops  = &favcd_proc_dump_ops;
    dumpproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dumpproc->owner      = THIS_MODULE;
#endif


    infoproc = create_proc_entry("info", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(infoproc == NULL) {
        printk("error to create %s/info proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    infoproc->proc_fops  = &favcd_proc_info_ops;
    infoproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    infoproc->owner      = THIS_MODULE;
#endif


    vg_infoproc = create_proc_entry("vg_info", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(vg_infoproc == NULL) {
        printk("error to create %s/vg_info proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    vg_infoproc->proc_fops  = &favcd_proc_vg_info_ops;
    vg_infoproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    vg_infoproc->owner      = THIS_MODULE;
#endif


    cbproc = create_proc_entry("callback_period", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(cbproc == NULL) {
        printk("error to create %s/callback_period proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    cbproc->proc_fops  = &favcd_proc_cb_ops;
    cbproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    cbproc->owner      = THIS_MODULE;
#endif


    prepproc = create_proc_entry("prepare_period", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(prepproc == NULL) {
        printk("error to create %s/prepare_period proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    prepproc->proc_fops  = &favcd_proc_prep_ops;
    prepproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    prepproc->owner      = THIS_MODULE;
#endif


    utilproc = create_proc_entry("utilization", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(utilproc == NULL) {
        printk("error to create %s/utilization proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    utilproc->proc_fops  = &favcd_proc_util_ops;
    utilproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    utilproc->owner      = THIS_MODULE;
#endif

    eng_reg_proc = create_proc_entry("eng_reg", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(eng_reg_proc == NULL) {
        printk("error to create %s/eng_reg proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    eng_reg_proc->proc_fops  = &favcd_proc_eng_reg_ops;
    eng_reg_proc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    eng_reg_proc->owner      = THIS_MODULE;
#endif
    

    propertyproc = create_proc_entry("property", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(propertyproc == NULL) {
        printk("error to create %s/property proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    propertyproc->proc_fops  = &favcd_proc_property_ops;
    propertyproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    propertyproc->owner      = THIS_MODULE;
#endif

    jobprofproc = create_proc_entry("job_prof", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(jobprofproc == NULL) {
        printk("error to create %s/job_prof proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    jobprofproc->proc_fops  = &favcd_proc_jobprof_ops;
    jobprofproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    jobprofproc->owner      = THIS_MODULE;
#endif


    jobproc = create_proc_entry("job", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(jobproc == NULL) {
        printk("error to create %s/job proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    jobproc->proc_fops  = &favcd_proc_job_ops;
    jobproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    jobproc->owner      = THIS_MODULE;
#endif

    dbgmodeproc = create_proc_entry("dbg_mode", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(dbgmodeproc == NULL) {
        printk("error to create %s/dbg_mode proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    dbgmodeproc->proc_fops  = &favcd_proc_dbgmode_ops;
    dbgmodeproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    dbgmodeproc->owner      = THIS_MODULE;
#endif

    levelproc = create_proc_entry("log_level", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(levelproc == NULL) {
        printk("error to create %s/log_level proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    levelproc->proc_fops  = &favcd_proc_level_ops;
    levelproc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    levelproc->owner      = THIS_MODULE;
#endif

    buf_num_proc = create_proc_entry("buf_num", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(buf_num_proc == NULL) {
        printk("error to create %s/buf_num proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    buf_num_proc->proc_fops  = &favcd_proc_buf_num_ops;
    buf_num_proc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    buf_num_proc->owner      = THIS_MODULE;
#endif

    cfg_proc = create_proc_entry("config", S_IRUGO|S_IXUGO, h264d_entry_proc);
    if(cfg_proc == NULL) {
        printk("error to create %s/config proc\n", ENTITY_PROC_NAME);
        goto fail_init_proc;
    }
    cfg_proc->proc_fops  = &favcd_proc_config_ops;
    cfg_proc->data       = private_data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,30))
    cfg_proc->owner      = THIS_MODULE;
#endif


    return 0;

fail_init_proc:
    
    favcd_proc_close();
    return -EFAULT;

}

