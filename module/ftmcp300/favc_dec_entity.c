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

#include "favc_dec_vg.h"
#include "favc_dec_entity.h"

spinlock_t favcd_lock; // lock for engine_busy
static int engine_busy[ENTITY_ENGINES];

//1:sucessful(engine not busy,set it busy)
//0:engine is busy
int test_and_set_engine_busy(int engine)
{
    int ret = 0;
    unsigned long flags;

    favcd_spin_lock_irqsave(&favcd_lock, flags);
    if (engine_busy[engine] == 0) {
        engine_busy[engine] = 1;
        ret = 1;
    }
    favcd_spin_unlock_irqrestore(&favcd_lock, flags);
    return ret;
}

//1:engine is idle
//0:engine is busy
int test_engine_idle(int engine)
{
    if (engine_busy[engine] == 0){
        return 1;
    }
    return 0;
}

#if 0
void set_engine_busy(int engine)
{
    engine_busy[engine] = 1;
}
#endif

void set_engine_idle(int engine)
{
    engine_busy[engine] = 0;
}



static int __init favcd_init(void)
{
    int i;
    int retval;
    
    spin_lock_init(&favcd_lock);

    for (i = 0; i < ENTITY_ENGINES; i++)
        engine_busy[i] = 0;
    
    retval = favcd_vg_init();
    
    return retval;
}

static void __exit favcd_cleanup(void)
{
    favcd_vg_cleanup();
}

module_init(favcd_init);
module_exit(favcd_cleanup);

MODULE_AUTHOR("Grain Media Corp.");
#if USE_VG_WORKQ
MODULE_LICENSE("GM");
#else
MODULE_LICENSE("GPL");
#endif
MODULE_VERSION(H264D_VER_STR);


