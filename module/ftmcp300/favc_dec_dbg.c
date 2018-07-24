#include <linux/slab.h> /* for kamalloc/kfree */
#include "dec_driver/global.h" /* for DecoderParams */
#include "favc_dec_vg.h"
#include "favc_dec_dbg.h"

#if USE_WRITE_FILE
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h> /* for get_fs, set_fs */
#endif

#if CHECK_ALLOC_FREE_JOB_ITEM
#warning CHECK_ALLOC_FREE_JOB_ITEM is enabled

struct alloc_record {
    int cnt;
    unsigned int ptr;
    int free_flg;
};

#define ALLOC_REC_SIZE 8
#define ALLOC_MARK 0x5a5a0000
#define FREE_MARK  0x6b6b1234

static struct alloc_record alloc_rec[ALLOC_REC_SIZE] = {{0,0,0}};
static int alloc_rec_cnt = 0;

void favcd_print_alloc_rec(void)
{
    int i = 0;
    
    for(i = 0; i < ALLOC_REC_SIZE; i++){
        printk("cnt:%5d ptr:0x%08X free_flg:%d\n", alloc_rec[i].cnt, alloc_rec[i].ptr, alloc_rec[i].free_flg);
    }
}


void favcd_alloc_check(unsigned int *iptr)
{
    extern unsigned int alloc_cnt;
    extern unsigned int free_cnt;
    
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].ptr = (unsigned int)iptr;
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].cnt = alloc_rec_cnt;
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].free_flg = 0;
    alloc_rec_cnt++;

    *iptr = ALLOC_MARK; /* mark as allocated and not initialized */
    DEBUG_M(LOG_TRACE, 0, 0, "__alloc_job_item memory: %08X\n", (unsigned int)iptr);

    if(alloc_cnt - free_cnt > 1000){
        DEBUG_M(LOG_ERROR, 0, 0, "__alloc_job_item alloc_cnt:%d free_cnt:%d\n", alloc_cnt, free_cnt);
        favcd_print_alloc_rec();
        FAVCD_DAMNIT("alloc/free cnt difference > 1000\n");
    }
}


void favcd_free_check(unsigned int *iptr)
{
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].ptr = (unsigned int)iptr;
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].cnt = alloc_rec_cnt;
    alloc_rec[alloc_rec_cnt % ALLOC_REC_SIZE].free_flg = 1;
    alloc_rec_cnt++;

    if(*iptr == FREE_MARK){ /* check if it is freed */
        DEBUG_M(LOG_TRACE, 0, 0, "__free_job_item free a memory already freed: %08X\n", (unsigned int)iptr);
        favcd_print_alloc_rec();
        FAVCD_DAMNIT("free an already freed memory\n");
    }

    *iptr = FREE_MARK; /* mark as freed */
}

#endif /* CHECK_ALLOC_FREE_JOB_ITEM */


#if USE_WRITE_FILE

int favcd_write_file(char *path, unsigned char *buf, int size, int is_new_flg)
{
    unsigned long long offset = 0;
    struct file *filp;
    int ret = 0;
    mm_segment_t fs;


    fs = get_fs();
    set_fs(KERNEL_DS);

    if(is_new_flg){
        filp = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    }else{
        filp = filp_open(path, O_WRONLY | O_APPEND, 0777);
    }

    if (IS_ERR(filp)) {
        printk("Error to open %s\n", path);
        goto returnit;
    }
    ret = vfs_write(filp, (unsigned char *)buf, size, &offset);
    //printk("ret: %d size:%d offset:%llu\n", ret, size, offset);
    
    filp_close(filp, NULL);

    set_fs(fs);
    LOG_PRINT(LOG_INFO, "save buffer %08X in file %s (size = %d)\n", (unsigned int)buf, path, size);
    return 0;
returnit:
    return -1;

}

#define USE_KMALLOC_KFREE 0
int favcd_copy_file(char *dst_path, char *src_path)
{
    unsigned long long offset = 0;
    struct file *filp;
    int ret = 0;
    mm_segment_t fs;
    int first_flg = 1;
#if USE_KMALLOC_KFREE
    char buf_samll[128];
    char *buf;
    int buf_size = 4096;

    buf = kmalloc(buf_size, GFP_KERNEL);
    if(buf == NULL){
        buf = buf_samll;
        buf_size = sizeof(buf_samll);
    }
#else
    /* avoid dynamic memory allocation */
    #define COPY_BUF_SIZE 4096
    static unsigned char buf[COPY_BUF_SIZE];
    int buf_size = COPY_BUF_SIZE;
#endif


    fs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(src_path, O_RDONLY, 0777);
    if (IS_ERR(filp)) {
        printk("Error to open %s\n", src_path);
        goto returnit;
    }
    
    do {
        ret = vfs_read(filp, (unsigned char *)buf, buf_size, &offset);
        //printk("ret: %d size:%d offset:%llu\n", ret, size, offset);
        if(ret <= 0){
            break;
        }
        favcd_write_file(dst_path, buf, ret, first_flg);
        first_flg = 0;
    }while(ret > 0);
    
    filp_close(filp, NULL);

    set_fs(fs);

#if USE_KMALLOC_KFREE
    if(buf != buf_samll){
        kfree(buf);
    }
#endif
    
    LOG_PRINT(LOG_INFO, "copy file from %s to %s\n", src_path, dst_path);
    return 0;
    
returnit:
#if USE_KMALLOC_KFREE
    if(buf != buf_samll){
        kfree(buf);
    }
#endif
    return -1;
}

#endif // USE_WRITE_FILE

/*
 *  save input bitstream of all jobs to file
 */
void favcd_save_job_bitstream_to_file(void)
{
    extern struct list_head  favcd_global_head;
    extern char *file_path;
    struct favcd_job_item_t *job_item;
    char path[64];
    
    DRV_LOCK_G(); /* lock for global list */
    list_for_each_entry (job_item, &favcd_global_head, global_list) {
        if(job_item->bs_dump_flg == 0){
            if(job_item->is_idr_flg){
                snprintf(path, sizeof(path), "%serr_bs%08d_%02d_I.264", file_path, job_item->job_id, job_item->minor);
            }else{
                snprintf(path, sizeof(path), "%serr_bs%08d_%02d.264", file_path, job_item->job_id, job_item->minor);
            }
            favcd_write_file(path, (uint8_t *)job_item->job->in_buf.buf[0].addr_va, job_item->in_size, 1);
            job_item->bs_dump_flg = 1;
        }
    }
    DRV_UNLOCK_G(); /* unlock for global list */
}


/*
 *  save input bitstream to file
 */
void favcd_save_bitstream_to_file(struct favcd_job_item_t *job_item)
{
    static int save_bs_idx = 0; /* index of the saving bitstream */
    extern int save_bs_cnt;
    extern char *file_path;
    
    if(save_bs_cnt != 0){
        char path[64];
        if(save_bs_idx != save_bs_cnt){
            snprintf(path, sizeof(path), "%sbs%08d_%02d.264", file_path, save_bs_idx, job_item->minor);
            favcd_write_file(path, (uint8_t *)job_item->job->in_buf.buf[0].addr_va, job_item->in_size, 1);
            save_bs_idx++;
        }else{
            save_bs_idx = save_bs_cnt = 0;
        }
    }
}


/*
 *  save mbinfo to file
 */
void favcd_save_mbinfo_to_file(struct favcd_job_item_t *job_item)
{
    struct favcd_data_t *dec_data;
    static int save_mbinfo_idx = 0;
    extern int save_mbinfo_cnt;
    extern char *file_path;
    
    /* save mbinfo */
    if(save_mbinfo_cnt){
        char path[64];
        int buf_idx;
        int mbinfo_size;
        
        buf_idx = job_item->buf_idx;
        dec_data = favcd_get_dec_data(job_item->minor);
        if(buf_idx != -1 && job_item->mbinfo_dump_flg == 0){
            job_item->mbinfo_dump_flg = 1;
            snprintf(path, sizeof(path), "%smbinfo_%08d_%02d.mbinfo", file_path, save_mbinfo_idx, job_item->minor);
            mbinfo_size = (job_item->dst_dim >> 16) * (job_item->dst_dim & 0xFFFF);
            //__favcd_invalidate_dcache((void *)dec_data->dec_buf[buf_idx].mbinfo_va, mbinfo_size);
            favcd_write_file(path, (uint8_t *)dec_data->dec_buf[buf_idx].mbinfo_va, mbinfo_size, 1);
    
            save_mbinfo_idx++;
            if(save_mbinfo_idx == save_mbinfo_cnt){
                save_mbinfo_cnt = 0;
                save_mbinfo_idx = 0;
            }
        }
    }
}


/*
 *  save output yuv to file
 */
void favcd_save_output_to_file(struct favcd_job_item_t *job_item)
{
    struct favcd_data_t *dec_data;
    static int save_output_idx = 0;
    extern int save_output_cnt;
    extern char *file_path;
    
    /* save output */
    if(save_output_cnt){
        char path[64];
        int buf_idx;
        int output_size;
        
        buf_idx = job_item->buf_idx;
        dec_data = favcd_get_dec_data(job_item->minor);
        if(buf_idx != -1 && job_item->output_dump_flg == 0){
            job_item->output_dump_flg = 1;
            snprintf(path, sizeof(path), "%soutput_%08d_%02d.yuv", file_path, save_output_idx, job_item->minor);
            //output_size = (job_item->dst_dim >> 16) * (job_item->dst_dim & 0xFFFF) * 2;
            output_size = job_item->Y_sz * 2;
            //__favcd_invalidate_dcache((void *)dec_data->dec_buf[buf_idx].mbinfo_va, mbinfo_size);
            favcd_write_file(path, (uint8_t *)dec_data->dec_buf[buf_idx].start_va, output_size, 1);
    
            save_output_idx++;
            if(save_output_idx == save_output_cnt){
                save_output_cnt = 0;
                save_output_idx = 0;
            }
        }
    }
}


/*
 * save bitstreams data and size from IDR
 */
void favcd_record_bitstream_to_file(struct favcd_job_item_t *job_item)
{
#if RECORDING_BS
    extern char *file_path;
    struct favcd_data_t *dec_data;
    char path[128];
    char str[128];
    int len;

    dec_data = favcd_get_dec_data(job_item->minor);

    if(dec_data->rec_en == 0)
        return;
    
    snprintf(path, sizeof(path), "%srec_bs_%02d.264", file_path, job_item->minor);
    favcd_write_file(path, (char *)job_item->job->in_buf.buf[0].addr_va, job_item->in_size, job_item->is_idr_flg);
    snprintf(path, sizeof(path), "%srec_bs_%02d.txt", file_path, job_item->minor);
    len = snprintf(str, sizeof(str), "%u #job_id:%d seq:%d idr:%u errno:%d\n", job_item->in_size, job_item->job_id, job_item->seq_num, job_item->is_idr_flg, job_item->err_num);
    favcd_write_file(path, str, len, job_item->is_idr_flg);
#endif
}


/*
 * copy recorded bitstreams data and size
 */
void favcd_copy_recorded_file(struct favcd_job_item_t *job_item)
{
#if RECORDING_BS
    extern char *file_path;
    struct favcd_data_t *dec_data;
    char src_path[128];
    char dst_path[128];
    unsigned int err_cnt;

    dec_data = favcd_get_dec_data(job_item->minor);
    
    if(dec_data->rec_en == 0)
        return;

    if(job_item->err_num == H264D_OK || job_item->err_num == H264D_SKIP_TILL_IDR){
        return;
    }

    err_cnt = dec_data->err_cnt;

    snprintf(src_path, sizeof(src_path), "%srec_bs_%02d.264", file_path, job_item->minor);
    snprintf(dst_path, sizeof(dst_path), "%srec_bs_%02d_%u.264", file_path, job_item->minor, err_cnt);
    favcd_copy_file(dst_path, src_path);
    snprintf(src_path, sizeof(src_path), "%srec_bs_%02d.txt", file_path, job_item->minor);
    snprintf(dst_path, sizeof(dst_path), "%srec_bs_%02d_%u.txt", file_path, job_item->minor, err_cnt);
    favcd_copy_file(dst_path, src_path);

    dec_data->err_cnt++;
#endif    
}



/*
 * functions of list access
 */

#ifdef DEBUG_LIST
#define DBLIST_PRINT(fmt, arg...) do{   \
        DEBUG_M(-1, 0, 0, fmt, ## arg); \
        /*DEBUG_M(LOG_WARNING, 0, 0, fmt, ## arg);*/ \
        /*printk(fmt, ## arg);*/ \
    } while(0)
#else
#define DBLIST_PRINT(fmt, arg...)
#endif //DEBUG_LIST



#if CHK_LIST_ACCESS
#ifdef DEBUG_LIST
//#define LOG_LIST LOG_WARNING
#define LOG_LIST LOG_ERROR
void __favcd_printm_list(struct list_head *head)
{
    struct list_head *curr;
    struct list_head *prev;
    int list_len = 0;

    prev = NULL;
    curr = head;
    DEBUG_M(LOG_LIST, 0, 0, "list(%08X):\n", (unsigned int)head);
    do{
        if(list_len >= 100){
            DEBUG_M(LOG_LIST, 0, 0, "list too long\n");
            break;
        }
        if(curr == prev){
            DEBUG_M(LOG_LIST, 0, 0, "prev == curr(%08X)\n", (unsigned int)curr);
            break;
        }
        DEBUG_M(LOG_LIST, 0, 0, "[%d]p:%08X c:%08X n:%08X\n", list_len, (unsigned int)curr->prev, 
            (unsigned int)curr, (unsigned int)curr->next);

        prev = curr;
        curr = curr->next;
        list_len++;
    }while(curr != head);
    DEBUG_M(LOG_LIST, 0, 0, "\n");
}

#else // !DEBUG_LIST 
// disable __favcd_printm_list
#define __favcd_printm_list(head)
#endif // !DEBUG_LIST 

void favcd_print_list(struct list_head *head)
{
    struct list_head *curr;
    struct list_head *prev;
    int list_len = 0;

    prev = NULL;
    curr = head;
    //printk("list(%08X):\n", (unsigned int)head);
    do{
        if(curr == NULL){
            //printk("curr ptr is null\n");
            break;
        }
        if(list_len >= 100){
            printk("list too long\n");
            break;
        }
        if(curr == prev){
            printk("prev == curr(%08X)\n", (unsigned int)curr);
            break;
        }
        printk("[%d]p:%08X c:%08X n:%08X\n", list_len, (unsigned int)curr->prev, 
            (unsigned int)curr, (unsigned int)curr->next);

        prev = curr;
        curr = curr->next;
        list_len++;
    }while(curr != head);
    //printk("\n");
}

void __favcd_check_list_error(struct list_head *head, int caller_id, const char *head_name, const char *func_name)
{
    struct list_head *prev = head->prev;

    if(prev == LIST_POISON2){
        printk("list deleted before: head:%08X prev:%08X caller_id:%d head:%s func:%s\n", (unsigned int)head, (unsigned int)prev, caller_id, head_name, func_name);
    }

    if(prev->next != head){
        printk("list error found: head:%08X prev:%08X caller_id:%d head:%s func:%s\n", (unsigned int)head, (unsigned int)prev, caller_id, head_name, func_name);
        favcd_print_list(head);
        favcd_print_alloc_rec();
        FAVCD_DAMNIT("list error found: head:%08X prev:%08X caller_id:%d head:%s func:%s\n", (unsigned int)head, (unsigned int)prev, caller_id, head_name, func_name);
        //while(1);
    }
}
#else
#define __favcd_check_list_error(head, caller_id, head_name, func_name)
#endif // CHK_LIST_ACCESS

/*
 * helper functions for list access
 */
#if CHK_LIST_ACCESS
void __favcd_list_add_tail_helper(struct list_head *entry, struct list_head *head, const char *head_name, const char *func_name)
{
    DBLIST_PRINT("add list: e:%08X h:%08X %s in %s\n", (unsigned int)entry, (unsigned int)head, head_name, func_name);
    __favcd_check_list_error(head, 1, head_name, func_name); /* check for an common error */
    //__favcd_printm_list(head);
    list_add_tail(entry, head);
    //__favcd_printm_list(head);
    
    DBLIST_PRINT("add list: e:%08X h:%08X %s in %s done\n", (unsigned int)entry, (unsigned int)head, head_name, func_name);
    __favcd_check_list_error(head, 2, head_name, func_name);
}

void __favcd_list_del_helper(struct list_head *entry, const char *entry_name, const char *func_name)
{
    DBLIST_PRINT("del list: e:%08X %s in %s\n", (unsigned int)entry, entry_name, func_name);

    __favcd_check_list_error(entry, 3, entry_name, func_name);
    //__favcd_printm_list(entry);
    list_del(entry);
    
    //__favcd_check_list_error(entry, 4, entry_name, func_name);

    DBLIST_PRINT("del list: e:%08X %s in %s done\n", (unsigned int)entry, entry_name, func_name);
}

void __favcd_list_init_head_helper(struct list_head *entry, const char *entry_name, const char *func_name)
{
    DBLIST_PRINT("ini list: e:%08X %s in %s\n", (unsigned int)entry, entry_name, func_name);
    INIT_LIST_HEAD(entry);
    DBLIST_PRINT("ini list: e:%08X %s in %s done\n", (unsigned int)entry, entry_name, func_name);
}
#endif // CHK_LIST_ACCESS


unsigned int favcd_get_buf_checksum(const char *buf, unsigned int len)
{
    int i;
    unsigned int sum = 0;
    for(i = 0; i < len; i++){
        sum += buf[i];
    }

    return sum;
}


/* 
 * functions for dump memory 
 */

int favcd_print_buf(const char *buf, unsigned int len)
{
    int i, j;
    unsigned int sum;
    for(j = 0; j < len; j += 16){
        printk("\nidata[%08X]: ", (unsigned int)buf + j);
        for(i = 0; i < 16; i++){
            printk("%02X ", buf[i + j]);
        }
    }
    printk("\n");
    sum = favcd_get_buf_checksum(buf, len);
    printk("sum:0x%08X ", sum);
    printk("len:%d\n", len);

    return 0;
}

void favcd_dump_mem(void *va, unsigned int len)
{
    int i;
    unsigned char *ptr = (unsigned char *)va;
    
    for(i = 0; i < len; i++){
        if((i % 16 == 0))
            DEBUG_M_NPFX(LOG_ERROR, 0, 0, "[%04X]", i);
        DEBUG_M_NPFX(LOG_ERROR, 0, 0, " %02X", ptr[i]);
        if((i % 16 == 15))
            DEBUG_M_NPFX(LOG_ERROR, 0, 0, "\n");
    }
    if((i % 16 != 0))
        DEBUG_M_NPFX(LOG_ERROR, 0, 0, "\n");
}

/*
 * print bitstream buffer of a job
 */
void favcd_dump_job_bs(struct favcd_job_item_t *job_item)
{
    struct video_entity_job_t *job;
    int i, len;
    unsigned char *va;
    struct favcd_data_t *dec_data;
    DecoderParams *dec_handle;
    const int max_print_size = 128;

    dec_data = favcd_get_dec_data(job_item->minor);
    dec_handle = dec_data->dec_handle;

    job = (struct video_entity_job_t *)job_item->job;
    DEBUG_M(LOG_ERROR, 0, 0, "{%d/%02d} ID:%04d st:%s(%d) p/s/e time:0x%04x/0x%04x/0x%04x size:%8d bg(w/h):%dx%d fmt:%03X width_thrd:%d sub_yuv_en:%d ratio:%d\n",
        job_item->engine, job_item->minor, job->id, 
        favcd_job_status_long_str(job_item->status), job_item->status, 
        job_item->puttime & 0xffff, job_item->starttime & 0xffff, (int)job_item->finishtime & 0xffff,
        job_item->bs_size, (job_item->dst_bg_dim >> 16), (job_item->dst_bg_dim &0xFFFF),
        job_item->dst_fmt, job_item->yuv_width_thrd, job_item->sub_yuv_en, job_item->sub_yuv_ratio);
    
    
    len = job_item->bs_size;
    if(len >= max_print_size){
        len = max_print_size;
    }
    
    va = (unsigned char *)job->in_buf.buf[0].addr_va;
    DEBUG_M(LOG_ERROR, 0, 0, "input bs: va:%08X pa:%08X\n", job->in_buf.buf[0].addr_va, job->in_buf.buf[0].addr_pa);
    favcd_dump_mem(va, len);


    i = dec_handle->buf_offset;
#if 0
    DEBUG_M(LOG_ERROR, 0, 0, "input bs: offset:%d count:%d", dec_handle->buf_offset, dec_handle->buf_cnt);
    for(; i < job_item->bs_size && i < dec_handle->buf_cnt + 32; i++){
        if((i % 16 == 0)){
            DEBUG_M_PFX(LOG_ERROR, 0, 0, "\n[%04d]", i);
        }
        DEBUG_M_PFX(LOG_ERROR, 0, 0, " %02X", va[i]);
    }
    DEBUG_M_PFX(LOG_ERROR, 0, 0, "\n");
#endif

    if(i + max_print_size < job_item->bs_size){ /* print the last N bytes */
        i = job_item->bs_size - max_print_size;
        len = max_print_size;
    }else if(i < job_item->bs_size){ /* remain bytes < N, print the rest of them  */
        len = job_item->bs_size - i;
    }else{ /* i >= in_size */
        len = 0;
    }

    if(len){
        DEBUG_M(LOG_ERROR, 0, 0, "input bs(last %d bytes):\n", len);
        favcd_dump_mem(va + i, len);
    }

    
#if 0    
    va = (unsigned char *)job->out_buf.buf[0].addr_va;
    DEBUG_M(LOG_ERROR, 0, 0, "output yuv:");
    favcd_dump_mem(va, len);
#endif
}


/*
 * print buffer pool
 */
void favcd_print_dec_buf(struct favcd_data_t *dec_data)
{
    int i;
    struct favcd_job_item_t *job_item;

    DEBUG_M(LOG_TRACE, 0, 0, "favcd_print_dec_buf: %d\n", dec_data->used_buffer_num);
    
    for(i = 0; i < MAX_DEC_BUFFER_NUM; i++){
        if(dec_data->dec_buf[i].is_used){
            job_item = dec_data->dec_buf[i].job_item;
            if(job_item){
                DEBUG_M(LOG_TRACE, 0, 0, "[%d]: rel: %d va:%08X job:%08X id:%d st:%s\n", i, 
                    dec_data->dec_buf[i].is_released, dec_data->dec_buf[i].start_va, 
                    (unsigned int)job_item, job_item->job_id, favcd_job_status_str(job_item->status));
            }else{
                DEBUG_M(LOG_TRACE, 0, 0, "[%d]: rel: %d va:%08X\n", i, 
                    dec_data->dec_buf[i].is_released, dec_data->dec_buf[i].start_va);
            }
        }
    }
}


/* print charactors at put job, callback */
void favcd_trace_job(char c)
{
    int cnt = 0;
    static int put_job_cnt = 0;
    static int callback_cnt = 0;

#if 0    
    extern int trace_job;
    if(trace_job == 0){
        return;
    }
#endif
    
    switch(c){
        case 'C': cnt = callback_cnt++;
            break;
        case 'P': cnt = put_job_cnt++;
            break;
        default:
            break;
    }
    printk("%c%d", c, cnt % 10);
}


/* generate error to bitstream buffer */
void favcd_gen_err_bitstream(struct favcd_job_item_t *job_item)
{
    unsigned int in_addr_va = job_item->job->in_buf.buf[0].addr_va;
    unsigned int size = job_item->bs_size;
    unsigned int rand_val;
    extern unsigned int err_bs_type;

    switch(err_bs_type){
        case 0:
            memcpy((void *)in_addr_va, (void *)(in_addr_va + size / 2), size / 2);
            break;
        case 1:
            memcpy((void *)(in_addr_va + size / 2), (void *)(in_addr_va), size / 2);
            break;
        case 2:
            memset((void *)in_addr_va, 0, size);
            break;
        case 3:
            rand_val = size;
            memcpy((void *)(in_addr_va + size), &rand_val, 4);
            break;
        default:
            break;
    }
}


/* Get job status string */
const char *favcd_job_status_str(unsigned int status)
{
    const char *str;
    switch(status){
        case DRIVER_STATUS_STANDBY:  str = "1S";  break;
        case DRIVER_STATUS_READY:    str = "2R";  break;
        case DRIVER_STATUS_ONGOING:  str = "3O";  break;
        case DRIVER_STATUS_WAIT_POC: str = "4W";  break;
        case DRIVER_STATUS_KEEP:     str = "5K";  break;
        case DRIVER_STATUS_FINISH:   str = "6D";  break;
        case DRIVER_STATUS_FAIL:     str = "7F";  break;
        default:                     str = "0U";  break;
    }
    return str;
}

/* Get job status string, long version */
const char *favcd_job_status_long_str(unsigned int status)
{
    
    const char *str;
    switch(status){
        case DRIVER_STATUS_STANDBY:  str = "STANDBY";  break;
        case DRIVER_STATUS_READY:    str = "  READY";  break;
        case DRIVER_STATUS_ONGOING:  str = "ONGOING";  break;
        case DRIVER_STATUS_WAIT_POC: str = "WAITPOC";  break;
        case DRIVER_STATUS_KEEP:     str = "   KEEP";  break;
        case DRIVER_STATUS_FINISH:   str = " FINISH";  break;
        case DRIVER_STATUS_FAIL:     str = "   FAIL";  break;
        default:                     str = "UNKNOWN";  break;
    }
    return str;
}

/* Get error message string */
const char *favcd_err_str(int err_no)
{
    const char *str = NULL;

    switch(err_no)
    {
        case H264D_OK:
            str = "OK";
            break;
        case H264D_NO_IDR:
            str = "non IDR bitstream at the first frame";
            break;
        case H264D_SKIP_TILL_IDR:
            str = "skip till IDR coded slice";
            break;
        case H264D_NO_PPS:
            str = "no PPS";
            break;
        case H264D_NO_SPS:
            str = "no SPS";
            break;
        case H264D_PARSING_ERROR:
            str = "parsing error (syntax error)";
            break;
        case H264D_ERR_VLD_ERROR:
            str = "decode vld error";
            break;
        case H264D_LOSS_PIC:
            str = "loss pic error";
            break;
        case H264D_ERR_BS_EMPTY:
            str = "no valid bitstream in input buffer (bitstream empty)";
            break;
        case H264D_ERR_HW_BS_EMPTY:
            str = "HW detected bitstream empty";
            break;
        case H264D_ERR_TIMEOUT:
            str = "hardware timeout error";
            break;
        case H264D_ERR_MEMORY:
            str = "allocate memory error";
            break;
        case H264D_ERR_API:
            str = "API version error";
            break;
        case H264D_BS_ERROR:
            str = "load bit stream error (larger than bit stream buffer or copy error)";
            break;
        case H264D_SIZE_OVERFLOW:
            str = "frame size is larger than max frame size from input property dst_bg_dim";
            break;
        case H264D_ERR_SW_TIMEOUT:
            str = "software timeout";
            break;
        case H264D_ERR_HW_UNSUPPORT:
            str = "hardware unsupport";
            break;
        case H264D_ERR_DRV_UNSUPPORT:
            str = "driver unsupport(b-frame)";
            break;
        case H264D_ERR_BUFFER_FULL:
            str = "reconstructed buffer is full";
            break;
        case H264D_NO_START_CODE:
            str = "no valid bitstream in input buffer (no start code)";
            break;
        case H264D_ERR_TIMEOUT_VLD:
            str = "hardware timeout and vld error";
            break;
        case H264D_ERR_DMA:
            str = "DMA read/write error";
            break;
        case H264D_ERR_TIMEOUT_EMPTY:
            str = "timeout because of bit stream empty";
            break;
        case H264D_ERR_DECODER_NULL:
            str = "input decoder is null";
            break;
        case H264D_ERR_BUF_NUM_NOT_ENOUGH:
            str = "buffer number not enough";
            break;
        case H264D_ERR_POLLING_TIMEOUT:
            str = "polling timeout";
            break;
        default:
            str = "unknown";
            break;
    }

    return str;
}



/* 
 * functions for debug message
 */
 
/*
 * Return whether the error number should be print to master
 */
const int favcd_err_to_master(int err_no)
{
    switch(err_no)
    {
        /* list of error numbers that will be reported to master */
        case H264D_NO_IDR:
        case H264D_ERR_BUF_NUM_NOT_ENOUGH:
#if 0
        case H264D_SKIP_TILL_IDR:
        case H264D_NO_PPS:
        case H264D_NO_SPS:
        case H264D_PARSING_ERROR:
        case H264D_ERR_VLD_ERROR:
        case H264D_LOSS_PIC:
        case H264D_ERR_BS_EMPTY:
        case H264D_ERR_TIMEOUT:
        case H264D_ERR_MEMORY:
        case H264D_ERR_API:
        case H264D_BS_ERROR:
        case H264D_SIZE_OVERFLOW:
        case H264D_ERR_SW_TIMEOUT:
        case H264D_ERR_HW_UNSUPPORT:
        case H264D_ERR_BUFFER_FULL:
        case H264D_ERR_TIMEOUT_VLD:
        case H264D_ERR_DMA:
        case H264D_ERR_TIMEOUT_EMPTY:
        case H264D_ERR_DECODER_NULL:
        case H264D_ERR_POLLING_TIMEOUT:
#endif
            return 1;
        default:
            return 0;
    }
    return 0;
}

/*
 * Return whether the error number should be treated as warning instead of error
 */
const int favcd_is_warning(int err_no)
{
    extern unsigned int lose_pic_handle_flags;

    switch(err_no)
    {
        /* list of error numbers that will be treated as warning (instead of error) */
        case H264D_NO_IDR:
        case H264D_SKIP_TILL_IDR:
#if 0
        case H264D_SKIP_TILL_IDR:
        case H264D_NO_PPS:
        case H264D_NO_SPS:
        case H264D_PARSING_ERROR:
        case H264D_ERR_VLD_ERROR:
        case H264D_LOSS_PIC:
        case H264D_ERR_BS_EMPTY:
        case H264D_ERR_TIMEOUT:
        case H264D_ERR_MEMORY:
        case H264D_ERR_API:
        case H264D_BS_ERROR:
        case H264D_SIZE_OVERFLOW:
        case H264D_ERR_SW_TIMEOUT:
        case H264D_ERR_HW_UNSUPPORT:
        case H264D_ERR_BUFFER_FULL:
        case H264D_ERR_TIMEOUT_VLD:
        case H264D_ERR_DMA:
        case H264D_ERR_TIMEOUT_EMPTY:
        case H264D_ERR_DECODER_NULL:
        case H264D_ERR_POLLING_TIMEOUT:
#endif
            return 1;
        case H264D_LOSS_PIC:
            if((lose_pic_handle_flags & LOSS_PIC_PRINT_ERR) == LOSS_PIC_PRINT_ERR || TREAT_WARN_AS_ERR(dbg_mode))
                return 0; /* treat LOSS_PIC as error */
            else
                return 1;
        default:
            return 0;
    }
    return 0;
}


const char *favcd_err_pos_str(enum err_position err_pos)
{
    const char *str;
    
    switch(err_pos){
        case ERR_NONE:
            str = "NONE";
            break;
        case ERR_TRIG:
            str = "TRIG";
            break;
        case ERR_ISR:
            str = "ISR";
            break;
        case ERR_SW_TIMEOUT:
            str = "TIMEOUT";
            break;
        default:
            str = "UNKNOWN";
            break;
    }
    return str;
}

/*
 * print error message for job item
 */
void favcd_print_job_error(struct favcd_job_item_t *job_item)
{
    unsigned int job_has_err;
    if(job_item->err_num >= 0 || favcd_is_warning(job_item->err_num)){
        job_has_err = 0;
    }else{
        job_has_err = 1;
    }
    if(job_has_err){
        DEBUG_M(LOG_ERROR, job_item->engine, job_item->minor, "(%d,%d) job %d seq: %d size: %d %s %s: %d(%s) buf:%d stored:%d read ptr:%d\n"
            , job_item->engine, job_item->minor, 
            job_item->job_id, job_item->seq_num, job_item->bs_size, 
            favcd_err_pos_str(job_item->err_pos), job_has_err?"error":"warning", job_item->err_num,
            favcd_err_str(job_item->err_num), job_item->buf_idx, job_item->stored_flg, job_item->parser_offset);
    }
}


/* 
 * dump log handler 
 */

int pannic_flg = 0; /* indicate whether the current log dumping is caused by panic */
/*
 * handler function for printing message at damnit/dump log
 */
int favcd_log_printout_handler(int data)
{
    struct favcd_job_item_t *job_item;
    unsigned long flags;
    extern int print_out_verbose;
    extern int save_err_bs_flg;
    extern struct list_head  favcd_global_head;
    extern char favcd_ver_str[128];

    if(pannic_flg){
        printm(MODULE_NAME, "PANIC!! PrintOut Start\n");
    }
    printm(MODULE_NAME, "%s\n", favcd_ver_str);

    if(save_err_bs_flg || print_out_verbose){
        favcd_spin_lock_irqsave(&favc_dec_lock, flags); /* lock for job status */
        list_for_each_entry(job_item, &favcd_global_head, global_list) {
            favcd_dump_job_bs(job_item);
        }
        favcd_spin_unlock_irqrestore(&favc_dec_lock, flags); /* unlock for job status */
    }else{
        printm(MODULE_NAME, "verbose message is disabled. (enable it by setting parameter print_out_verbose to 1)\n");
    }

    if(pannic_flg){
        printm(MODULE_NAME, "PANIC!! PrintOut End\n");
    }
    pannic_flg = 0;
    return 0;
}


/*
 * handler function for printing message at damnit
 */
int favcd_panic_printout_handler(int data)
{
    pannic_flg = 1;
    /* let favcd_log_printout_handler() to print log latter */
    return 0;
}



