/**
fake capture engine
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

#include "favc_enc_vg.h"
#include "favc_enc_entity.h"
#ifdef REF_POOL_MANAGEMENT
#include "mem_pool.h"
#endif

spinlock_t favce_lock;

#ifdef TWO_ENGINE
    static int engine_busy[MAX_ENGINE_NUM];
#else
    static int engine_busy;
#endif
//void *engine_data[MAX_ENGINE_NUM];
struct rc_entity_t *rc_dev = NULL;

//1:sucessful(engine not busy,set it busy)
//0:engine is busy
int test_and_set_engine_busy(int engine)
{
    int ret = 0;
    unsigned long flags;

    spin_lock_irqsave(&favce_lock, flags);
#ifdef TWO_ENGINE
    if (engine_busy[engine] == 0) {
        engine_busy[engine] = 1;
        ret = 1;
    }
#else
    if (engine_busy == 0) {
        engine_busy = 1;
        ret = 1;
    }
#endif
    spin_unlock_irqrestore(&favce_lock, flags);
    return ret;
}

int is_engine_idle(int engine)
{
    unsigned long flags;
    spin_lock_irqsave(&favce_lock, flags);
#ifdef TWO_ENGINE
    if (engine_busy[engine]) {
        spin_unlock_irqrestore(&favce_lock, flags);
        return 0;
    }
#else
    if (engine_busy) {
        spin_unlock_irqrestore(&favce_lock, flags);
        return 0;
    }
#endif
    spin_unlock_irqrestore(&favce_lock, flags);
    return 1;
}

void set_engine_busy(int engine)
{
    unsigned long flags;
    spin_lock_irqsave(&favce_lock, flags);
#ifdef TWO_ENGINE
    engine_busy[engine] = 1;
#else
    engine_busy = 1;
#endif
    spin_unlock_irqrestore(&favce_lock, flags);
}

void set_engine_idle(int engine)
{
    unsigned long flags;
    spin_lock_irqsave(&favce_lock, flags);
#ifdef TWO_ENGINE
    engine_busy[engine] = 0;
#else
    engine_busy = 0;
#endif
    spin_unlock_irqrestore(&favce_lock, flags);
}

int rc_register(struct rc_entity_t *entity)
{
    rc_dev = entity;
    return 0;
}

int rc_deregister(void)
{
    rc_dev = NULL;
    return 0;
}

static int __init favce_init(void)
{
#ifdef TWO_ENGINE
    int i;
#endif
    int ret = 0;

    spin_lock_init(&favce_lock);
#ifdef TWO_ENGINE
    for (i = 0; i < MAX_ENGINE_NUM; i++)
        engine_busy[i] = 0;
#else
    engine_busy = 0;
#endif

    ret = favce_vg_init();
    if (ret < 0)
        return ret;
    return ret;
}

static void __exit favce_clearnup(void)
{
    favce_vg_close();
}

EXPORT_SYMBOL(rc_register);
EXPORT_SYMBOL(rc_deregister);

module_init(favce_init);
module_exit(favce_clearnup);

MODULE_AUTHOR("Grain Media Corp.");
MODULE_LICENSE("GPL");


