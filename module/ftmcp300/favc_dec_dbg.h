
#ifndef _FAVC_DEC_DBG_H_
#define _FAVC_DEC_DBG_H_

#include "dec_driver/define.h"

/* checking job item allocate/free*/
#if CHECK_ALLOC_FREE_JOB_ITEM
void favcd_print_alloc_rec(void);
void favcd_alloc_check(unsigned int *iptr);
void favcd_free_check(unsigned int *iptr);
#else
#define favcd_print_alloc_rec() do{}while(0)
#define favcd_alloc_check(ptr) do{}while(0)
#define favcd_free_check(ptr) do{}while(0)
#endif

/* writing / copying files */
#if USE_WRITE_FILE
int favcd_write_file(char *path, unsigned char *buf, int size, int is_new_flg);
int favcd_copy_file(char *dst_path, char *src_path);
#else
#define favcd_write_file(path, buf, size, is_new_flg) do{}while(0)
#define favcd_copy_file(dst_path, src_path) do{}while(0)
#endif

/* Dump job's data to file */
void favcd_save_job_bitstream_to_file(void);
void favcd_save_bitstream_to_file(struct favcd_job_item_t *job_item);
void favcd_save_mbinfo_to_file(struct favcd_job_item_t *job_item);
void favcd_save_output_to_file(struct favcd_job_item_t *job_item);
void favcd_record_bitstream_to_file(struct favcd_job_item_t *job_item);
void favcd_copy_recorded_file(struct favcd_job_item_t *job_item);



/* 
 * debug list access 
 */
void favcd_print_list(struct list_head *head);
void __favcd_check_list_error(struct list_head *head, int caller_id, const char *head_name, const char *func_name);

/* 
 * helper functions for list access
 * NOTE: Do not call these functions directly. Use the follinw macros instead
 */
void __favcd_list_add_tail_helper(struct list_head *entry, struct list_head *head, const char *head_name, const char *func_name);
void __favcd_list_del_helper(struct list_head *entry, const char *entry_name, const char *func_name);
void __favcd_list_init_head_helper(struct list_head *entry, const char *entry_name, const char *func_name);

#if CHK_LIST_ACCESS
#define __CHK_LIST_ENTRY_INIT(entry)  do{ \
        struct list_head *entry_ptr = (entry);\
        if(entry_ptr->next != entry_ptr){ \
            printk("entry error type 1: e->n(%08X) != e(%08X)(entry:%s func:%s)\n", (unsigned int)entry_ptr->next, (unsigned int)entry_ptr, #entry, __FUNCTION__); \
        }\
        if(entry_ptr->prev != entry_ptr){ \
            printk("entry error type 2: e->p(%08X) != e(%08X)(entry:%s func:%s)\n", (unsigned int)entry_ptr->prev, (unsigned int)entry_ptr, #entry, __FUNCTION__); \
        } \
    } while(0)
#else
#define __CHK_LIST_ENTRY_INIT(entry)  do{ } while(0)
#endif

#if CHK_LIST_ACCESS
#define __CHK_LIST_HEAD(head)  do{ \
        struct list_head *head_ptr = (head);\
        struct list_head *next_ptr = head_ptr->next;\
        if(head_ptr->next != next_ptr){ \
            printk("head error type 1: h->n(%08X->%08X) != n(%08X)(head:%s func:%s)\n", (unsigned int)head_ptr, (unsigned int)head_ptr->next, (unsigned int)next_ptr, #head, __FUNCTION__); \
        }\
        if(head_ptr != next_ptr->prev){ \
            printk("head error type 2: h(%08X) != n->p(%08X->%08X)(head:%s func:%s)\n", (unsigned int)head_ptr, (unsigned int)next_ptr, (unsigned int)next_ptr->prev, #head, __FUNCTION__); \
        } \
    } while(0)
#else
#define __CHK_LIST_HEAD(head)  do{  } while(0)
#endif


/* list wrapper functions for the ease of debugging */
#if CHK_LIST_ACCESS
#define __favcd_list_add_tail(entry, head) do {\
        __CHK_LIST_HEAD(head);\
        __CHK_LIST_ENTRY_INIT(entry);\
        __favcd_list_add_tail_helper(entry, head, #entry, __FUNCTION__);\
        __CHK_LIST_HEAD(head);\
    } while(0)

#define __favcd_list_del(entry) do{\
        __favcd_list_del_helper(entry, #entry, __FUNCTION__); \
    } while(0)

#define __favcd_init_list_head(entry) do{\
        __favcd_list_init_head_helper(entry, #entry, __FUNCTION__); \
    } while(0)
#else
#define __favcd_list_add_tail(entry, head) list_add_tail(entry, head)
#define __favcd_list_del(entry) list_del(entry);
#define __favcd_init_list_head(entry) INIT_LIST_HEAD(entry);
#endif


/* dump memory, memory pool */
int favcd_print_buf(const char *buf, unsigned int len);
void favcd_dump_mem(void *va, unsigned int len);
void favcd_dump_job_bs(struct favcd_job_item_t *job_item);
void favcd_print_dec_buf(struct favcd_data_t *dec_data);


/* print charactors at put job, callback */
void favcd_trace_job(char c);
#define TRACE_JOB(c) do{    if(trace_job) favcd_trace_job(c);  }while(0)


/* generate error bitstream */
void favcd_gen_err_bitstream(struct favcd_job_item_t *job_item);

/* Get error, job status string */
const char *favcd_err_str(int err_no);
const char *favcd_job_status_str(unsigned int status);
const char *favcd_job_status_long_str(unsigned int status);

/* debug/error message */
const int favcd_err_to_master(int err_no);
const int favcd_is_warning(int err_no);
void favcd_print_job_error(struct favcd_job_item_t *job_item);

/* dump log handler */
int favcd_log_printout_handler(int data);
int favcd_panic_printout_handler(int data);


#endif // _FAVC_DEC_DBG_H_

