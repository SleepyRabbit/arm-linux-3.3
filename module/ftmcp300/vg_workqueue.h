/**
* @file vg_workqueue.h
*  Rewrite linux workqueue. Implement it by fifo instead of list.
* Copyright (C) 2011 GM Corp. (http://www.grain-media.com)
*
* $Revision: 1.12 $
* $Date: 2013/01/23 05:59:16 $
*
* ChangeLog:
*  $Log: vg_workqueue.h,v $
*  Revision 1.12  2013/01/23 05:59:16  schumy_c
*  Add workqueue full check function.
*
*  Revision 1.11  2013/01/08 05:53:00  ivan
*  fix to use msecs_to_jiffies instead of tick calculation
*
*  Revision 1.10  2013/01/08 05:44:46  ivan
*  fix to use msecs_to_jiffies instead of tick calculation
*
*  Revision 1.9  2012/12/12 08:41:46  schumy_c
*  Add flush function for vg_workqueue.
*
*  Revision 1.8  2012/12/11 03:31:45  ivan
*  transfer to UNIX format by dos2unix
*
*  Revision 1.7  2012/12/04 07:51:29  waynewei
*  fixed for build fail with wrong include file
*
*  Revision 1.6  2012/12/03 07:41:27  schumy_c
*  Remove panic when fifo overflow.
*
*  Revision 1.5  2012/12/03 06:50:51  schumy_c
*  Change 'vg_workqueue.h' path location.
*
*  Revision 1.4  2012/11/30 09:57:36  schumy_c
*  Add vg_workqueue nice function.
*
*  Revision 1.3  2012/11/30 02:39:05  schumy_c
*  Change tab to space.
*
*  Revision 1.2  2012/11/29 10:00:35  waynewei
*  fixed for build error in arm-linux-3.3
*  1.kfifo interface difference
*  2.driver_init function duplicate
*
*  Revision 1.1  2012/11/27 02:34:11  schumy_c
*  Change file location.
*
*  Revision 1.3  2012/11/26 06:58:03  schumy_c
*  Set function inline.
*
*  Revision 1.2  2012/11/26 06:56:12  schumy_c
*  Change file define to fit its name.
*
*  Revision 1.1  2012/11/26 06:50:18  schumy_c
*  Add vg_workqueue and use it to replace original linux workqueue.
*
*/

#ifndef __VG_WORKQUEUE_H__
#define __VG_WORKQUEUE_H__

#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#define VG_WQ_FIFO_MAX_CNT  128

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
#define kfifo_put_in    kfifo_in
#define kfifo_get_out   kfifo_out
#else   // for GM8181
#define kfifo_put_in    kfifo_put
#define kfifo_get_out   kfifo_get
#endif


struct vg_workqueue_struct {
    struct task_struct *task;
    int task_nice;
    const char *name;
    struct kfifo *workfifo;
    int workfifo_maxlen;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
#else
    spinlock_t lock;
#endif

};

struct vg_work_struct {
    void (*func)(void *);
    void *wq_data;
    struct timer_list timer;
};

#define PREPARE_VG_WORK(_work, _func)       \
    do {                                    \
        (_work)->func = _func;              \
    } while (0);

#define INIT_VG_WORK(_work, _func)          \
    do {                                    \
        PREPARE_VG_WORK((_work), (_func));  \
        init_timer(&(_work)->timer);        \
    } while (0);

/**************************************************************************************************/
/**************************************************************************************************/

static inline void run_workfifo(struct vg_workqueue_struct *wq)
{
    int len;
    struct vg_work_struct  *vg_work;

    while (1) {
        len = kfifo_get_out(wq->workfifo, (unsigned char *)&vg_work, sizeof(struct vg_work_struct *));
        if (len != sizeof(struct vg_work_struct *))
            break;
        vg_work->func(vg_work);
    }
    return;
}

static inline int vg_worker_thread(void *p_data)
{
    struct vg_workqueue_struct *wq = (struct vg_workqueue_struct *) p_data;


    while (!kthread_should_stop()) {
        set_user_nice(current, -20); 
        //set_user_nice(current, wq->task_nice);
        schedule();
        __set_current_state(TASK_RUNNING);
        run_workfifo(wq);
        set_current_state(TASK_INTERRUPTIBLE);
    }
    __set_current_state(TASK_RUNNING);
    return 0;
}

static inline void delayed_vg_work_timer_fn(unsigned long data)
{
    int len;
    struct vg_work_struct *vg_work;
    struct vg_workqueue_struct *vg_wq;
    vg_work = (struct vg_work_struct *) data;
    vg_wq = vg_work->wq_data;

    len = kfifo_put_in(vg_wq->workfifo, (unsigned char *)&data, sizeof(struct vg_work_struct *));
    if (len != sizeof(data)) {
#if 1
        printk(KERN_DEBUG "vg_workqueue(%s) overflow, max(%d)\n", vg_wq->name, VG_WQ_FIFO_MAX_CNT);
        panic("vg_work fifo overflow. name(%s) fifo max(%d)\n", vg_wq->name, VG_WQ_FIFO_MAX_CNT);
#endif
        goto exit;
    }
    wake_up_process(vg_wq->task); 
exit:
    return;
}

/**************************************************************************************************/
/**************************************************************************************************/
static inline
int is_workqueue_full(struct vg_workqueue_struct *vg_wq)
{
    int ret = 0;
    int fifo_len;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
    fifo_len = kfifo_avail(vg_wq->workfifo);
#else
    fifo_len = vg_wq->workfifo_maxlen - kfifo_len(vg_wq->workfifo);
#endif
    if (fifo_len < sizeof(struct vg_work_struct *))
        ret = 1;
    return ret;
}

static inline
int get_workqueue_len(struct vg_workqueue_struct *vg_wq)
{
    int ret = 0;
    ret =  kfifo_len(vg_wq->workfifo) / sizeof(struct vg_work_struct *);
    return ret;
}


static inline 
void flush_vg_workqueue(struct vg_workqueue_struct *wq)
{
    run_workfifo(wq);
}

static inline
void destroy_vg_workqueue(struct vg_workqueue_struct *wq)
{
	flush_vg_workqueue(wq);

    kfifo_free(wq->workfifo);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
    kfree(wq->workfifo);
#endif    
    kthread_stop(wq->task);
    kfree(wq);
}

static inline
struct vg_workqueue_struct* create_vg_workqueue(char *name)
{
    int destroy = 0;
    struct vg_workqueue_struct *wq;
    struct task_struct *p;

    wq = kzalloc(sizeof(*wq), GFP_KERNEL);
    if (!wq)
        return NULL;
    wq->name = name;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
#else
    spin_lock_init(&wq->lock);
#endif

    p = kthread_create(vg_worker_thread, wq, "%s%s", "gm_", name);
    if (IS_ERR(p)) {
        destroy = 1;
        goto exit;
    }
    wq->workfifo_maxlen = VG_WQ_FIFO_MAX_CNT * sizeof(struct vg_work_struct *);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33))
    wq->workfifo = kzalloc(sizeof(struct kfifo), GFP_KERNEL);
    if(!wq->workfifo || kfifo_alloc(wq->workfifo, wq->workfifo_maxlen, GFP_KERNEL)) {
        if(wq->workfifo == NULL)
            panic("To allocate fifo , but return NULL error!%s:%p\n",wq->name,wq->task);
        else
            panic("Call kfifo_alloc , but return error!%s,task:%p\n",wq->name,wq->task);
        
        goto exit;
    }
#else
    wq->workfifo = kfifo_alloc(wq->workfifo_maxlen, GFP_KERNEL, &wq->lock);
#endif
    if (IS_ERR(wq->workfifo)) {
        destroy = 1;
        goto exit;
    }
    wq->task = p;
    wake_up_process(p);

exit:
    if (destroy) {
        destroy_vg_workqueue(wq);
        wq = NULL;
    }
    return wq;
}

static inline
void set_vg_workqueue_nice(struct vg_workqueue_struct *vg_wq, int nice)
{
    if (nice >= -20 && nice <= 19)
        vg_wq->task_nice = nice;
}

static inline
int start_delayed_vg_work(struct vg_workqueue_struct *vg_wq, struct vg_work_struct *vg_work,
                          unsigned long delay_ms)
{
    int ret = 0;
    struct timer_list *timer = &vg_work->timer;

    if (is_workqueue_full(vg_wq)) {
        ret = -17;
        goto exit;
    }

    if (delay_ms) {
        if (!timer_pending(timer)) {
            vg_work->wq_data = vg_wq;
            timer->function = delayed_vg_work_timer_fn;
            timer->data = (unsigned long) vg_work;
            timer->expires = jiffies + msecs_to_jiffies(delay_ms);
            add_timer(timer);
        }
    } else {
        vg_work->wq_data = vg_wq;
        delayed_vg_work_timer_fn((unsigned long) vg_work);
    }

exit:
    return ret;
}

static inline
int cancel_delayed_vg_work(struct vg_work_struct *vg_work)
{
    return del_timer_sync(&vg_work->timer);
}

#endif /* __VG_WORKQUEUE_H__ */

