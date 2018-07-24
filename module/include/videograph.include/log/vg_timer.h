/**
 * @file vg_timer.h
 *  vg timer header header
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2016/08/08 08:01:51 $
 *
 * ChangeLog:
 *  $Log: vg_timer.h,v $
 *  Revision 1.1  2016/08/08 08:01:51  ivan
 *  update v1 wrapper for GM8135
 *
 *  Revision 1.1.1.1  2016/03/25 10:39:11  ivan
 *
 *
 *
 */
#ifndef __VG_TIMER_H__
#define __VG_TIMER_H__

#include <linux/timer.h>


#define MAX_TIMER_NAME_LEN  31
typedef struct vg_timer_tag {
    void (*func)(void *);
    void *data;

    struct task_struct *task;
    struct timer_list  timer;
    int nice;
    unsigned int st_magic;
    int is_thread_job;  //0:init state, 1:will do it at least once, 2:prepare to exit kthread
    spinlock_t thread_lock;
    char name[MAX_TIMER_NAME_LEN + 1];
    struct task_struct *p_destroy_task;
    int free_when_thread_end;
    wait_queue_head_t wait_queue;

} vg_timer_t;


int  vg_timer_init(vg_timer_t *vg_timer, char *name);
int  vg_timer_exit(vg_timer_t *vg_timer);
int  vg_timer_start(vg_timer_t *vg_timer, unsigned long delay_ms,
                           void (*func)(void *data), void *data);
int  vg_timer_cancel(vg_timer_t *vg_timer);
void  vg_timer_set_nice(vg_timer_t *vg_timer, int nice);


#endif /* __VG_TIMER_H__ */

