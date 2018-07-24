/**
 * @file log.h
 *  log header file
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.1 $
 * $Date: 2016/08/08 08:01:51 $
 *
 * ChangeLog:
 *  $Log: log.h,v $
 *  Revision 1.1  2016/08/08 08:01:51  ivan
 *  update v1 wrapper for GM8135
 *
 *  Revision 1.1.1.1  2016/03/25 10:39:11  ivan
 *
 *
 *
 */
#ifndef _LOG_H_
#define _LOG_H_
#include <linux/ioctl.h>
#include <linux/version.h>

//#define MEM_LEAK_TRACE
#ifdef MEM_LEAK_TRACE
#ifndef _LOG_C_
#include "dbg_wrapper.h"
#endif
#endif

#define MAX_STRING_LEN  40

#define IOCTL_PRINTM                0x9969
#define IOCTL_PRINTM_WITH_PANIC     0x9970

int register_panic_notifier(int (*func)(int));
int register_printout_notifier(int (*func)(int));
int register_master_print_notifier(int (*func)(int));
int damnit(char *module);
void printm(char *module,const char *fmt, ...);
void master_print(const char *fmt, ...);
void dumpbuf_pa(int ddr_id1, unsigned int pa1, unsigned int size1, char *filename1,
                int ddr_id2, unsigned int pa2, unsigned int size2, char *filename2,
                char *path);
void register_version(char *);
int unregister_printout_notifier(int (*func)(int));
int unregister_panic_notifier(int (*func)(int));
int dumplog(char *module);
int get_cpu_state(void);

#define VG_ASSERT(condition)\
do{\
    if(unlikely(0 == (condition))){\
        printk("%s %d:assert error!\n", __FUNCTION__, __LINE__);\
        damnit("VG");\
    }\
}while(0)


#endif
