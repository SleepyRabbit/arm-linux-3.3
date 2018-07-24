/**
 * @file video_entity.c
 *  The video entity
 * Copyright (C) 2011 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.18 $
 * $Date: 2013/07/09 07:42:29 $
 *
 * ChangeLog:
 *  $Log: em_test.c,v $
 *  Revision 1.18  2013/07/09 07:42:29  ivan
 *  update print jid log with unsigned int
 *
 *  Revision 1.17  2013/03/26 08:06:18  waynewei
 *  <em_test.c>
 *  1.fixed for test 6 issue in 32_engine of capture
 *  2.fixed for forgeting to free memeory after exit
 *  3.fixed for typo
 *  4.fixed for support more than 32CH (capture has 33CH)
 *  <update for script>
 *  1.add cap_md=0
 *  <cap_md>
 *  1.fixed mb from 16x16 to 32x32(to support 1920x1080)
 *  <vposd>
 *  1.fixed for cpature's bug fixed
 *  2.according to Jerson's suggestion to prevent from set the same value to alpha_t
 *
 *  Revision 1.16  2013/03/19 08:07:21  waynewei
 *  1.fixed bug to add kfree for job_fd
 *
 *  Revision 1.15  2013/03/19 02:14:21  waynewei
 *  1.add split_ch
 *  2.exit to stop all vcap_fd in test_environment
 *
 *  Revision 1.14  2013/02/27 07:47:49  ivan
 *  add test 7 to check driver utilization implement fail
 *
 *  Revision 1.13  2013/02/27 05:59:36  ivan
 *  no need to realloc in test mode
 *
 *  Revision 1.12  2013/01/08 09:11:37  klchu
 *  fixed typo.
 *
 *  Revision 1.11  2013/01/08 05:44:19  ivan
 *  fix to use msecs_to_jiffies instead of tick calculation
 *
 *  Revision 1.10  2012/12/19 08:28:29  waynewei
 *  fixed for support more than 16 engines in em_test.c
 *
 *  Revision 1.9  2012/11/26 03:11:09  ivan
 *  modify sync with gs module from job id to jid
 *
 *  Revision 1.8  2012/11/19 08:01:03  ivan
 *  add job priority setting
 *
 *  Revision 1.7  2012/11/08 08:02:06  waynewei
 *  Fixed free_fd issue during em_test due to error usage of array size
 *
 *  Revision 1.6  2012/11/07 08:33:56  waynewei
 *  1.fixed for entity test with NULL pointer access during copy_property
 *  2.Fixed for build entity error at /usr/src/arm-linux-3.3/module
 *
 *  Revision 1.5  2012/10/29 05:21:27  ivan
 *  update to new version of EM
 *
 *  Revision 1.3  2012/10/17 02:00:43  ivan
 *  Update for linux 3.3 compatible
 *
 *  Revision 1.2  2012/10/12 09:39:23  ivan
 *  take off Carriage Return for linux system
 *
 *  Revision 1.1.1.1  2012/10/12 08:35:52  ivan
 *
 *
 */
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
#include <linux/list.h>

#include "em_api.h"
#include "video_entity.h"
#include "em_param.h"
#include "test_api.h"
/*
Case 1 Basic Test: Put 10 jobs + stop + record
Case 2 Basic Test: Put 10 jobs + stop + Put 10 jobs + stop
Case 3 Basic Burnin Test: Continue Put jobs (HW util/CPU util/ISR Lock/Perf)
Case 4 Multi-jobs Test: Put 5 jobs + Put 5 jobs + stop + record
Case 5 Multi-jobs Test: Put 5 jobs + stop + Put 5 jobs
Case 6 Multi-jobs Burnin Test: Put 5 jobs... (HW util/CPU util/ISR Lock/Perf)
 */

struct video_test_job_t {
    struct video_entity_job_t job;
    struct list_head list;
    int index;
};
static DEFINE_SPINLOCK(ioctl_lock);
struct list_head standby_head, ongoing_head, finish_head;

//#define EM_TEST_DBG(fmt, args...) printk(fmt, ## args)
#define EM_TEST_DBG(fmt, args...)

unsigned int pause_msec = 100;
module_param(pause_msec, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pause_msec, "pause_msec");

unsigned int pause_period = 0; /* in number of jobs */
module_param(pause_period, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(pause_period, "pause_period");

unsigned int test_stop = 0; /* whether to call stop in test case 1 */
module_param(test_stop, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(test_stop, "test_stop");

int stop_job_num = -1; /* call stop after putting N job in test case 1 */
module_param(stop_job_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(stop_job_num, "stop_job_num");


int em_dbg = 1; /* whether to print message in em_test */
module_param(em_dbg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(em_dbg, "em_dbg");


int list_count(struct list_head *head)
{
    int ret = 0;
    unsigned long flags;
    struct video_test_job_t *testc;

    spin_lock_irqsave(&ioctl_lock, flags);
    list_for_each_entry(testc, head, list)
    ret++;
    spin_unlock_irqrestore(&ioctl_lock, flags);
    return ret;
}

/*
 *  Note: this function only applies to the user space address
 */
unsigned int dvr_user_va_to_pa(unsigned int addr)
{
    pmd_t *pmdp;
    pte_t *ptep;
    unsigned int paddr, vaddr;

    pmdp = pmd_offset(pud_offset(pgd_offset(current->mm, addr), addr), addr);
    if (unlikely(pmd_none(*pmdp)))
        return 0;

    ptep = pte_offset_kernel(pmdp, addr);
    if (unlikely(pte_none(*ptep)))
        return 0;

    vaddr = (unsigned int)page_address(pte_page(*ptep)) + (addr & (PAGE_SIZE - 1));
    paddr = __pa(vaddr);

    return paddr;
}


int test_callback(void *param)
{
    unsigned long flags;
    struct video_entity_job_t *job = param;
    struct video_test_job_t *test_job;

    do {
        test_job = job->em_user_priv;
        EM_TEST_DBG(KERN_DEBUG "     callback idx %d jid(%u) status %d (0x%x)\n",
                    test_job->index, job->id, job->status, (int)job);
        spin_lock_irqsave(&ioctl_lock, flags);
        list_del(&test_job->list);
        list_add_tail(&test_job->list, &finish_head);
        EM_TEST_DBG(KERN_DEBUG "     put jid(%u) to finish, value %d\n",
                    test_job->index, job->out_prop[0].value);
        spin_unlock_irqrestore(&ioctl_lock, flags);
    } while ((job = job->next_job));

    return 0;
}


void clear_prop(struct video_entity_job_t *job)
{
    struct video_entity_job_t *job_next = job;
    do {
        struct video_property_t *in_property = (struct video_property_t *)job_next->in_prop;
        struct video_property_t *out_property = (struct video_property_t *)job_next->out_prop;
        in_property[0].id = ID_NULL;
        out_property[0].id = ID_NULL;
    } while ((job_next = job_next->next_job) != 0);
}

#define MAX_ENGINE_TICKS    msecs_to_jiffies(60)
#define MIN_ENGINE_TICKS    msecs_to_jiffies(10)
#define TEST_MINORS         64
#define TEST_ENGINES        32
int em_test(unsigned long arg)
{
    unsigned long flags;
    int i = 0, job_number;
    int job_id = 0;
    int ret = 0;
    int count;
    unsigned int job_fd[TEST_MINORS];
    unsigned int j_cnt=0;
    char *em_tbl;
    int idx;


    /* copy from user */
    struct test_case_t *test_cases = 0;
    struct job_case_t *job_cases = 0;

    /* prepare by ioctl */
    struct video_test_job_t *test_job[MAX_JOBS], *testn, *testc;
    struct video_entity_job_t *job;
    struct video_bufinfo_t in_binfo, out_binfo;

#define PREALLC_MEM 
#ifdef PREALLC_MEM
    static struct test_case_t *test_cases_pre_alloc = NULL;
    static struct video_test_job_t *test_job_pre_alloc[MAX_JOBS] = {NULL};
    static char *em_tbl_pre_alloc = NULL;
#endif

    if(em_dbg){
        printk("em_test >>\n");
    }

#ifdef PREALLC_MEM
    if(test_cases_pre_alloc == NULL){
        test_cases_pre_alloc = kmalloc(sizeof(struct test_case_t), GFP_KERNEL);
        if(test_cases_pre_alloc == NULL){
            printk("test_cases_pre_alloc allocate failed\n");
            while(1);
        }
    }

    if(test_job_pre_alloc[0] == NULL){
        for(i = 0; i < MAX_JOBS; i++){
            test_job_pre_alloc[i] = kzalloc(sizeof(struct video_test_job_t), GFP_KERNEL);
            if(test_job_pre_alloc[i] == NULL){
                printk("test_job_pre_alloc allocate failed\n");
                while(1);
            }
        }
    }

    if(em_tbl_pre_alloc == NULL){
        em_tbl_pre_alloc = kmalloc(TEST_MINORS*TEST_ENGINES, GFP_KERNEL);
        if(em_tbl_pre_alloc == NULL){
            printk("em_tbl_pre_alloc allocate failed\n");
            while(1);
        }
    }
#endif //PREALLC_MEM

#ifdef PREALLC_MEM
    em_tbl = em_tbl_pre_alloc;
#else
    em_tbl = kmalloc(TEST_MINORS*TEST_ENGINES, GFP_KERNEL);
#endif

    if (em_tbl == 0)
        return -1;
    memset(em_tbl, 0xFF, (TEST_MINORS*TEST_ENGINES));    

    memset(job_fd, 0, sizeof(unsigned int) * TEST_MINORS);
#ifdef PREALLC_MEM
    test_cases = test_cases_pre_alloc;
#else
    test_cases = kmalloc(sizeof(struct test_case_t), GFP_KERNEL);
#endif
    if (test_cases == 0)
        return -1;
    INIT_LIST_HEAD(&standby_head);
    INIT_LIST_HEAD(&ongoing_head);
    INIT_LIST_HEAD(&finish_head);
    if (copy_from_user((void *)test_cases, (void *)arg, sizeof(struct test_case_t)))
        return -1;
    job_cases = test_cases->job_cases;

    printk("Assume Engine MAX time=%lu MIN time=%lu (ticks)\n", MAX_ENGINE_TICKS, MIN_ENGINE_TICKS);
    i = 0;
    while (job_cases[i].enable && (i < MAX_JOBS)) {
#ifdef PREALLC_MEM
        test_job[i] = test_job_pre_alloc[i];
#else
        test_job[i] = kzalloc(sizeof(struct video_test_job_t), GFP_KERNEL);
#endif
        test_job[i]->index = i;
        INIT_LIST_HEAD(&test_job[i]->list);

        idx = MAKE_IDX(TEST_MINORS, job_cases[i].engine, job_cases[i].minor);
        if(idx >= TEST_MINORS*TEST_ENGINES){
            printk("[EM_TEST][ERROR]MAKE_IDX(%d) from engine/minor is too big to be the index(%d) of em_tbl.\n",idx,(TEST_MINORS*TEST_ENGINES));
            return -1;
        }
        if(em_tbl[idx] == 0xFF){            
            job_fd[j_cnt] = em_alloc_fd(test_cases->type, job_cases[i].engine,
                                                     job_cases[i].minor);
            if (job_fd[j_cnt] == 0) {
                printk("Error allocate fd type 0x%x engine %d minor %d\n",
                       test_cases->type, job_cases[i].engine, job_cases[i].minor);
                //kfree(test_cases);
                //kfree(em_tbl);
                ret = -1;
                goto return_entry;
            }
            em_tbl[idx] = j_cnt;
            j_cnt++;            
        }

        idx = em_tbl[idx];

        job = &test_job[i]->job;
        job_cases[i].id = job->ver = job->id = ++job_id;
        job->fd = job_fd[idx];
        job->priority = 0;
        job->next_job = 0;
        job->status = JOB_STATUS_ONGOING;
        em_set_property(job->fd, job->ver, &(job_cases[i].in_prop));
        em_get_bufinfo(job->fd, job->ver, &in_binfo, &out_binfo);

        in_binfo.buf[0].addr_pa = dvr_user_va_to_pa(job_cases[i].in_mmap_va[0]);
        in_binfo.buf[1].addr_pa = dvr_user_va_to_pa(job_cases[i].in_mmap_va[1]);
        in_binfo.buf[0].addr_va = (unsigned int)__va(in_binfo.buf[0].addr_pa);
        in_binfo.buf[1].addr_va = (unsigned int)__va(in_binfo.buf[1].addr_pa);
        in_binfo.buf[0].addr_prop = 0;
        in_binfo.buf[1].addr_prop = 0;
//        in_binfo.is_keep = 0;
        memcpy(&(job->in_buf), &in_binfo, sizeof(struct video_bufinfo_t));

        out_binfo.buf[0].addr_pa = dvr_user_va_to_pa(job_cases[i].out_mmap_va[0]);
        out_binfo.buf[1].addr_pa = dvr_user_va_to_pa(job_cases[i].out_mmap_va[1]);
        out_binfo.buf[0].addr_va = (unsigned int)__va(out_binfo.buf[0].addr_pa);
        out_binfo.buf[1].addr_va = (unsigned int)__va(out_binfo.buf[1].addr_pa);
        out_binfo.buf[0].addr_prop = 0;
        out_binfo.buf[1].addr_prop = 0;
        out_binfo.buf[0].need_realloc_val = 0;
        out_binfo.buf[1].need_realloc_val = 0;
//        out_binfo.is_keep = 0;
        memcpy(&(job->out_buf), &out_binfo, sizeof(struct video_bufinfo_t));
        job->src_callback = test_callback;
        job->em_user_priv = test_job[i];

        spin_lock_irqsave(&ioctl_lock, flags);
        list_add_tail(&(test_job[i]->list), &ongoing_head);
        spin_unlock_irqrestore(&ioctl_lock, flags);
        i++;
    }
    job_number = i;

    switch(test_cases->case_item) {
        case 1: //Case 1 Basic Test: Put 10 jobs + stop + record
        {
            int jiffies_stop = 0;
            int jiffies_start = 0;
#define TIMEOUT_FOR_CASE1
#ifdef TIMEOUT_FOR_CASE1
            int loop_cnt = 0;
#endif //TIMEOUT_FOR_CASE1
            #define MAX_USED_MINOR 32
            int minor_used[MAX_USED_MINOR];
            for(i = 0; i < MAX_USED_MINOR; i++){
                minor_used[i] = 0;
            }
            for (i = 0; i < job_number; i++) {
                int minor = job_cases[i].minor;
                if(job_cases[i].minor >= MAX_USED_MINOR){
                    printk("job's minor %d > %d\n", minor, MAX_USED_MINOR);
                    continue;
                }
                if(minor_used[minor] == 0){
                    minor_used[minor] = 1;
                    printk("minor %d is used\n", minor);
                }
            }
            
            for (i = 0; i < job_number; i++) {
                job = &test_job[i]->job;
                EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                clear_prop(job);
#if 1 /* add delay before put jobs */
                if(pause_period != 0){
                    if(i != 0 && (i % pause_period) == 0){
                        set_current_state(TASK_INTERRUPTIBLE);
                        schedule_timeout(pause_msec);
                    }
                }
#endif
                em_putjob(job);
#if 1 
                /* call stop after putting N jobs */
                if(test_stop){
                    if((stop_job_num < 0 && i == job_number - 1) || (i == stop_job_num)){
                        //call stop for all used channel
                        int j;
                        for(j = 0; j < MAX_USED_MINOR; j++){
                            if(minor_used[j]){
                                em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, j));
                            }
                        }
                    }
                }
#endif
            }

            jiffies_start = jiffies;
            while ((count = list_count(&ongoing_head)) > 0) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(MAX_ENGINE_TICKS);

#define TIMEOUT_CNT 200
#ifdef TIMEOUT_FOR_CASE1
                //Julian: added for detecting decode hang
                if(loop_cnt >= TIMEOUT_CNT){
                    EM_TEST_DBG(KERN_DEBUG "    stop job after waiting %d times (dur:%ld)\n", loop_cnt, jiffies - jiffies_start);
                    //em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));
                    //call stop for all used channel
                    for(i = 0; i < MAX_USED_MINOR; i++){
                        if(minor_used[i]){
                            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, i));
                        }
                    }
                    break;
                }
                if((loop_cnt % 50) == 0){
                    printk("wait ongoing job count(%d) == 0 (%d < %d)\n", count, loop_cnt, TIMEOUT_CNT);
                }
                loop_cnt++;
#endif //TIMEOUT_FOR_CASE1
            }
#if 1 
			// Julian: call stop if job is on-going, and wait until all jobs are done
            if (count == 1) {     //maybe keep job
                EM_TEST_DBG(KERN_DEBUG "    stop job\n");
                //call stop for all used channel
                for(i = 0; i < MAX_USED_MINOR; i++){
                    if(minor_used[i]){
                        em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, i));
                    }
                }
            }

			jiffies_stop = jiffies;
			loop_cnt = 0;
			while ((count = list_count(&ongoing_head)) > 0) { 
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(MAX_ENGINE_TICKS);
				loop_cnt++;
				if(loop_cnt > TIMEOUT_CNT){
					printk("Error to call stop,after %lu ticks,ongoing jobs still %d (dur:%lu)\n",
						   MAX_ENGINE_TICKS, count, jiffies - jiffies_stop);
					printk("test case 1 stop error\n");
					return -1;
				}
                if((loop_cnt % 50) == 0){
                    printk("wait ongoing job count(%d) == 0 (%d < %d)\n", count, loop_cnt, TIMEOUT_CNT);
                }
			}
			printk("time to wait until the last job is done: %ld\n", jiffies - jiffies_stop);
#else	//Julian: call stop if job is on-going, and wiat MAX_ENGINE_TICKS. If the last job is not done, panic!!
            if (count == 1) {     //maybe keep job
                EM_TEST_DBG(KERN_DEBUG "    stop job\n");
                //em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));
                //call stop for all used channel
                for(i = 0; i < MAX_USED_MINOR; i++){
                    if(minor_used[i]){
                        em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, i));
                    }
                }
                jiffies_stop = jiffies;
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(MAX_ENGINE_TICKS);
            }

            if ((count = list_count(&ongoing_head)) > 0) {
                printk("Error to call stop,after %dticks,ongoing jobs still %d (dur:%lu)\n",
                       MAX_ENGINE_TICKS, count, jiffies - jiffies_stop);
                EM_PANIC_M("test case 1 stop error\n");
                return -1;
            }
#endif
#if 0
            /* check if job's buffer is kept */
            {
                int err_flg = 0;
                int err_cnt = 0;
                for (i = 0; i < job_number; i++) {
                    job = &test_job[i]->job;
                    printk("job %d: out buf is_keep = %d\n", job->id, job->out_buf.is_keep);
                    if(job->out_buf.is_keep){
                        printk("job %d: out buf is_keep != 0\n", job->id);
                        err_flg = 1;
                        err_cnt++;
                    }
                }

                if(err_flg){
                    printk("error: %d output buf of jobs is not released\n", err_cnt);
                }
            }
#endif

            list_for_each_entry_safe(testc, testn, &finish_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);
                job_cases[i].status = job->status;
                memcpy(job_cases[i].out_prop, job->out_prop,
                       sizeof(struct video_property_t) * MAX_PROPERTYS);
                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &standby_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);
                em_destroy_property(job->fd, job->ver);
                #ifndef PREALLC_MEM
                kfree(test_job[i]);
                #endif
            }
            if (copy_to_user((void *)arg, (void *)test_cases, sizeof(struct test_case_t)))
                return -1;
            break;
        }
        case 2: //Case 2 Basic Test: Put 8 jobs + stop + Put 2 jobs + stop (two rounds)
        {
            int loop_cnt;
            int jiffies_start;
            printk("(1)Put %d jobs\n", job_number);
            for (i = 0; i < job_number - 2; i++) {
                job = &(test_job[i]->job);
                EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                clear_prop(job);
                em_putjob(&test_job[i]->job);
            }
            while ((count = list_count(&finish_head)) < 1) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
            }

            EM_TEST_DBG(KERN_DEBUG "    stop job while finish %d\n", count);
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));

            job = &test_job[job_number - 2]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            job = &test_job[job_number - 1]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            loop_cnt = 0;
            jiffies_start = jiffies; // to get total time required to wait job finished
        wait_test2_1:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MAX_ENGINE_TICKS * 2);

            if ((count = list_count(&finish_head)) < job_number - 1) {
                printk("Error to wait job finished, count=%d (should be %d)\n",
                       count, job_number - 1);
                ret = -1;
                loop_cnt++;
                if(loop_cnt > 10) {
                    printk("test case 2 stop error: job unfinished\n");
                    while(1);
                }
                goto wait_test2_1;
            }
            printk("Wait job finished, count=%d (duration %ld)\n", count, jiffies - jiffies_start);

            EM_TEST_DBG(KERN_DEBUG "    stop job while finish %d\n", count);
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));

        wait_test2_2:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MAX_ENGINE_TICKS);     /* wait for stop */
            if ((count = list_count(&ongoing_head)) > 0) {     //count==1 means keep job situation
                printk("Error to call stop,after %luticks,%d jobs still run.\n",
                       MAX_ENGINE_TICKS * 2, count);
                ret = -1;
                goto wait_test2_2;
            }

            list_for_each_entry_safe(testc, testn, &finish_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);
                printk("    jid(%d) Status %d at 0x%x\n", job->id, job->status, (int)job);
                if ((i == job_number - 1) &&
                    ((job->status != JOB_STATUS_FINISH) && (job->status != JOB_STATUS_FAIL))) {
                    printk("Error jid(%d) status, should be FINISH/FAIL\n", job->id);
                    ret = -1;
                } else if ((i == job_number - 2) && (job->status != JOB_STATUS_FINISH)) {
                    printk("Error jid(%d) status, should be FINISH\n", job->id);
                    ret = -1;
                } else if ((i == job_number - 3) && (job->status != JOB_STATUS_FAIL)) {
                    printk("Error jid(%d) status, should be FAIL\n", job->id);
                    ret = -1;
                }
                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &standby_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);
                em_destroy_property(job->fd, job->ver);
            }

            list_for_each_entry_safe(testc, testn, &standby_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);

                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &ongoing_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);

                job_cases[i].id = job->ver = job->id = ++job_id;
                job->status = JOB_STATUS_ONGOING;
                em_set_property(job->fd, job->ver, &(job_cases[i].in_prop));
            }

            printk("(2)Put %d jobs\n", job_number);
            for (i = 0; i < job_number - 2; i++) {
                job = &(test_job[i]->job);
                EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                clear_prop(job);
                em_putjob(job);
            }
            while ((count = list_count(&finish_head)) < 1) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
            }

            EM_TEST_DBG(KERN_DEBUG "    stop job while finish %d\n", count);
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));

            job = &test_job[job_number - 2]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            job = &test_job[job_number - 1]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            loop_cnt = 0;
        wait_test2_3:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MAX_ENGINE_TICKS * 2);

            if ((count = list_count(&finish_head)) < job_number - 1) {
                printk("Error to wait job finished, count=%d (should be %d)\n",
                       count, job_number - 1);
                ret = -1;
                loop_cnt++;
                if(loop_cnt > 10) {
                    printk("test case 2 stop error: job unfinished\n");
                    while(1);
                }
                goto wait_test2_3;
            }

            EM_TEST_DBG(KERN_DEBUG "    stop job while finish %d\n", count);
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));

        wait_test2_4:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MAX_ENGINE_TICKS);     /* wait for stop */

            if ((count = list_count(&ongoing_head)) > 0) {     //count==1 means keep job situation
                printk("Error to call stop,after %luticks,%d jobs still run.\n",
                       MAX_ENGINE_TICKS * 2, count);
                ret = -1;
                goto wait_test2_4;
            }

            list_for_each_entry_safe(testc, testn, &finish_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);
                printk("    jid(%d) Status %d at 0x%x\n", job->id, job->status, (int)&job->status);

                if ((i == job_number - 1) &&
                    ((job->status != JOB_STATUS_FINISH) && (job->status != JOB_STATUS_FAIL))) {
                    printk("Error jid(%d) status, should be FINISH/FAIL\n", job->id);
                    ret = -1;
                } else if ((i == job_number - 2) && (job->status != JOB_STATUS_FINISH)) {
                    printk("Error jid(%d) status, should be FINISH\n", job->id);
                    ret = -1;
                } else if ((i == job_number - 3) && (job->status != JOB_STATUS_FAIL)) {
                    printk("Error jid(%d) status, should be FAIL\n", job->id);
                    ret = -1;
                }
                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &standby_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);
                em_destroy_property(job->fd, job->ver);
                #ifndef PREALLC_MEM
                kfree(test_job[i]);
                #endif
            }
            break;
        }
        case 3: //Case 3 Basic Burnin Test: Continue Put jobs (HW util/CPU util/ISR Lock/Perf)
        {
            int count = 0;
            int msecs = test_cases->burnin_secs * 1000;
            unsigned int start_jiffies, end_jiffies;
            unsigned int end_mark = 0, print_diff = 0, diff = 0;
#define KEEP_ORG_ORDER
#ifdef KEEP_ORG_ORDER
            unsigned int last_job_id = 0;
#endif // KEEP_ORG_ORDER

#define STOP_FOR_USED_CHANNEL
#ifdef STOP_FOR_USED_CHANNEL
#define MAX_USED_MINOR 32
            int minor_used[MAX_USED_MINOR];
            for(i = 0; i < MAX_USED_MINOR; i++){
                minor_used[i] = 0;
            }
            for (i = 0; i < job_number; i++) {
                int minor = job_cases[i].minor;
                if(job_cases[i].minor >= MAX_USED_MINOR){
                    printk("job's minor %d > %d\n", minor, MAX_USED_MINOR);
                    continue;
                }
                if(minor_used[minor] == 0){
                    minor_used[minor] = 1;
                    printk("minor %d is used\n", minor);
                }
            }
#endif //STOP_FOR_USED_CHANNEL


            end_jiffies = start_jiffies = jiffies;
            for (i = 0; i < job_number; i++) {
                job = &(test_job[i]->job);
                EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                clear_prop(job);
                em_putjob(job);
            }

            while (1) {
                //from finish_head to standby head
                list_for_each_entry_safe(testc, testn, &finish_head, list) {
#ifdef KEEP_ORG_ORDER
                    if(testc->job.id != last_job_id + 1)
                        continue;
                    last_job_id = testc->job.id;
#endif // KEEP_ORG_ORDER
                    count++;
                    i = testc->index;
                    job = &(test_job[i]->job);

                    spin_lock_irqsave(&ioctl_lock, flags);
                    list_del(&testc->list);
                    list_add_tail(&testc->list, &standby_head);
                    spin_unlock_irqrestore(&ioctl_lock, flags);
                    em_destroy_property(job->fd, job->ver);
                }

                //get from standby head to ongoing head
                if (end_mark == 0) {
                    list_for_each_entry_safe(testc, testn, &standby_head, list) {
                        i = testc->index;
                        job = &(test_job[i]->job);

                        spin_lock_irqsave(&ioctl_lock, flags);
                        list_del(&testc->list);
                        list_add_tail(&testc->list, &ongoing_head);
                        spin_unlock_irqrestore(&ioctl_lock, flags);

                        job_cases[i].id = job->ver = job->id = ++job_id;
                        job->status = JOB_STATUS_ONGOING;

                        em_set_property(job->fd, job->ver, &(job_cases[i].in_prop));

                        EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                        clear_prop(job);
                        em_putjob(job);
                    }
                } else if (list_empty(&finish_head) && list_empty(&ongoing_head))
                    break;

                end_jiffies = jiffies;
                if (end_jiffies > start_jiffies) {
                    diff = end_jiffies - start_jiffies;
                    if ((msecs > 0) && (diff > msecs)) {
#ifdef STOP_FOR_USED_CHANNEL
                        for(i = 0; i < MAX_USED_MINOR; i++) {
                            if(minor_used[i]){
                                em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, i));
                            }
                        }
#else //STOP_FOR_USED_CHANNEL
                        em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine,
                                             job_cases[0].minor));
#endif //STOP_FOR_USED_CHANNEL
                        end_mark = 1;
                    }
                    if (diff - print_diff > 5000) {
                        printk("Performance %dfps at 0x%x\n",
                               (count * 1000) / (diff - print_diff), (int)end_jiffies & 0xffff);
                        print_diff = diff;
                        count = 0;
                    }
                } else
                    start_jiffies = jiffies;

                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(2);
            }
            break;
        }
        case 4: //Case 4 Multi-jobs Test: Put 10 jobs + stop + record
        {
            for (i = 0; i < job_number / 2; i++) {
                job = &test_job[i]->job;
                if (i < (job_number / 2) - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }
            for (i = job_number / 2; i < job_number; i++) {
                job = &test_job[i]->job;
                if (i < job_number - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }
            job = &test_job[0]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);
            job = &test_job[job_number / 2]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            while ((count = list_count(&ongoing_head)) > 1) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(MAX_ENGINE_TICKS);
            }
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MAX_ENGINE_TICKS);

            if (count == 1) {     //maybe keep job
                EM_TEST_DBG(KERN_DEBUG "    stop job\n");
                em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(MAX_ENGINE_TICKS);
            }

            if ((count = list_count(&ongoing_head)) > 0) {
                EM_TEST_DBG("Error to call stop,after %dticks,ongoing jobs still %d\n",
                            MAX_ENGINE_TICKS, count);
                return -1;
            }

            list_for_each_entry_safe(testc, testn, &finish_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);
                job_cases[i].status = job->status;
                memcpy(job_cases[i].out_prop, job->out_prop,
                       sizeof(struct video_property_t) * MAX_PROPERTYS);
                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &standby_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);
                em_destroy_property(job->fd, job->ver);
                #ifndef PREALLC_MEM
                kfree(test_job[i]);
                #endif
            }
            if (copy_to_user((void *)arg, (void *)test_cases, sizeof(struct test_case_t)))
                return -1;
            break;
        }
        case 5: //Case 5 Multi-jobs Test: Put 5 jobs + stop + Put 5 jobs
        {
            for (i = 0; i < job_number / 2; i++) {
                job = &test_job[i]->job;
                if (i < (job_number / 2) - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }
            for (i = job_number / 2; i < job_number; i++) {
                job = &test_job[i]->job;
                if (i < job_number - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }
            printk("(1)Put %d jobs at 0x%x\n", job_number / 2, (int)jiffies & 0xffff);
            job = &test_job[0]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(MIN_ENGINE_TICKS);

            EM_TEST_DBG(KERN_DEBUG "    stop job\n");
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));
            printk("(2)Put %d jobs\n", job_number / 2);
            job = &test_job[job_number / 2]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            while ((count = list_count(&ongoing_head)) > 1) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(MAX_ENGINE_TICKS);
            }
            EM_TEST_DBG(KERN_DEBUG "    stop job\n");
            em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine, job_cases[0].minor));

            list_for_each_entry_safe(testc, testn, &finish_head, list) {
                i = testc->index;
                job = &(test_job[i]->job);
                printk("    jid(%u) Status %d\n", job->id, job->status);
                if ((i == (job_number / 2) - 1) && (job->status != JOB_STATUS_FAIL)) {
                    printk("Error jid(%u) status, should be FAIL\n", job->id);
                    ret = -1;
                } else if ((i == job_number / 2) && (job->status != JOB_STATUS_FINISH)) {
                    printk("Error jid(%u) status, should be FINISH\n", job->id);
                    ret = -1;
                }

                spin_lock_irqsave(&ioctl_lock, flags);
                list_del(&testc->list);
                list_add_tail(&testc->list, &standby_head);
                spin_unlock_irqrestore(&ioctl_lock, flags);
                em_destroy_property(job->fd, job->ver);
                #ifndef PREALLC_MEM
                kfree(test_job[i]);
                #endif
            }
            break;
        }
        case 6:
        {
            int msecs = test_cases->burnin_secs * 1000;
            int count = 0;
            unsigned int start_jiffies, end_jiffies;
            unsigned int end_mark = 0, print_diff = 0, diff = 0;

            for (i = 0; i < job_number / 2; i++) {
                job = &test_job[i]->job;
                if (i < (job_number / 2) - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }
            for (i = job_number / 2; i < job_number; i++) {
                job = &test_job[i]->job;
                if (i < job_number - 1) {
                    job->next_job = &test_job[i + 1]->job;
                    test_job[i + 1]->job.next_job = 0;
                }
            }

            end_jiffies = start_jiffies = jiffies;
            job = &test_job[0]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%u) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);
            job = &test_job[job_number / 2]->job;
            EM_TEST_DBG(KERN_DEBUG "    put jid(%u) (0x%x)\n", job->id, (int)job);
            clear_prop(job);
            em_putjob(job);

            while (1) {
                //from finish_head to standby head
                list_for_each_entry_safe(testc, testn, &finish_head, list) {
                    count++;
                    i = testc->index;
                    job = &(test_job[i]->job);
                    spin_lock_irqsave(&ioctl_lock, flags);
                    list_del(&testc->list);
                    list_add_tail(&testc->list, &standby_head);
                    spin_unlock_irqrestore(&ioctl_lock, flags);
                    em_destroy_property(job->fd, job->ver);
                }

                //get from standby head to ongoing head
                if (end_mark == 0) {
                    int idx_mask = 0;
                    list_for_each_entry_safe(testc, testn, &standby_head, list) {
                        i = testc->index;
                        idx_mask |= (1 << i);
                        job = &(test_job[i]->job);

                        spin_lock_irqsave(&ioctl_lock, flags);
                        list_del(&testc->list);
                        list_add_tail(&testc->list, &ongoing_head);
                        spin_unlock_irqrestore(&ioctl_lock, flags);

                        job_cases[i].id = job->ver = job->id = ++job_id;
                        job->status = JOB_STATUS_ONGOING;
                        em_set_property(job->fd, job->ver, &(job_cases[i].in_prop));
                    }

                    if (idx_mask & 0x1) {
                        job = &test_job[0]->job;
                        EM_TEST_DBG(KERN_DEBUG "    put jid(%u) (0x%x)\n", job->id, (int)job);
                        clear_prop(job);
                        em_putjob(job);
                    }
                    if (idx_mask & (1 << (job_number / 2))) {
                        job = &test_job[job_number / 2]->job;
                        EM_TEST_DBG(KERN_DEBUG "    put jid(%d) (0x%x)\n", job->id, (int)job);
                        clear_prop(job);
                        em_putjob(job);
                    }

                } else if (list_empty(&finish_head) && list_empty(&ongoing_head))
                    break;

                end_jiffies = jiffies;
                if (end_jiffies > start_jiffies) {
                    diff = end_jiffies - start_jiffies;
                    if ((msecs > 0) && (diff > msecs)) {
                        em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine,
                                             job_cases[0].minor));
                        end_mark = 1;
                    }
                    if (diff - print_diff > 5000) {
                        printk("Performance %dfps at 0x%x\n",
                               (count * 1000) / (diff - print_diff), (int)end_jiffies & 0xffff);
                        print_diff = diff;
                        count = 0;
                    }
                } else
                    start_jiffies = jiffies;

                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(2);
            }
            break;
        }
        case 7: //special to test performance data is correct
        {
            int count = 0;
            int msecs = test_cases->burnin_secs * 1000;
            unsigned int start_jiffies, end_jiffies;
            unsigned int end_mark = 0, print_diff = 0, diff = 0;

            end_jiffies = start_jiffies = jiffies;
            for (i = 0; i < 2; i++) {
                job = &(test_job[i]->job);
                EM_TEST_DBG(KERN_DEBUG "    put jid(%u) (0x%x)\n", job->id, (int)job);
                clear_prop(job);
                em_putjob(job);
            }

            while (1) {
                //from finish_head to standby head
                list_for_each_entry_safe(testc, testn, &finish_head, list) {
                    count++;
                    i = testc->index;
                    job = &(test_job[i]->job);

                    spin_lock_irqsave(&ioctl_lock, flags);
                    list_del(&testc->list);
                    list_add_tail(&testc->list, &standby_head);
                    spin_unlock_irqrestore(&ioctl_lock, flags);
                    em_destroy_property(job->fd, job->ver);
                }

                //get from standby head to ongoing head
                if (end_mark == 0) {
                    list_for_each_entry_safe(testc, testn, &standby_head, list) {
                        i = testc->index;
                        job = &(test_job[i]->job);

                        spin_lock_irqsave(&ioctl_lock, flags);
                        list_del(&testc->list);
                        list_add_tail(&testc->list, &ongoing_head);
                        spin_unlock_irqrestore(&ioctl_lock, flags);

                        job_cases[i].id = job->ver = job->id = ++job_id;
                        job->status = JOB_STATUS_ONGOING;

                        em_set_property(job->fd, job->ver, &(job_cases[i].in_prop));

                        EM_TEST_DBG(KERN_DEBUG "    put jid(%u) (0x%x)\n", job->id, (int)job);
                        clear_prop(job);
                        em_putjob(job);
                    }
                } else if (list_empty(&finish_head) && list_empty(&ongoing_head))
                    break;

                end_jiffies = jiffies;
                if (end_jiffies > start_jiffies) {
                    diff = end_jiffies - start_jiffies;
                    if ((msecs > 0) && (diff > msecs)) {
                        em_stopjob(ENTITY_FD(test_cases->type, job_cases[0].engine,
                                             job_cases[0].minor));
                        end_mark = 1;
                    }
                    if (diff - print_diff > 5000) {
                        printk("Performance %dfps at 0x%x\n",
                               (count * 1000) / (diff - print_diff), (int)end_jiffies & 0xffff);
                        print_diff = diff;
                        count = 0;
                    }
                } else
                    start_jiffies = jiffies;

                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(2);
            }
            break;
        }
        default:
            break;
    }

return_entry:
    for (i = 0; i < TEST_MINORS; i++) {
        if (job_fd[i] != 0) {            
            em_free_fd(job_fd[i]);
            job_fd[i] = 0;
        }
    }
#ifndef PREALLC_MEM
    if(test_cases)
        kfree(test_cases);
    if(em_tbl)
        kfree(em_tbl);
#endif

    if(em_dbg){
        printk("em_test << %d\n", ret);
    }

    return ret;
}


