#ifndef __DEBUG_H__
#define __DEBUG_H__

#define MODULE_NAME         "FE"

    #ifdef VG_INTERFACE
        #include "log.h"
        #if 0
        #define DEBUG_M(level, fmt, args...) { \
            if (log_level == LOG_DIRECT) \
                printk(fmt, ## args); \
            else if (log_level >= level) \
                printm(MODULE_NAME, fmt, ## args); }
        #else
        #define DEBUG_M(level, fmt, args...) {      \
            if (log_level >= level) {               \
                printm(MODULE_NAME, fmt, ## args);  \
                if (procForceDirectDump)            \
                    printk(fmt, ## args);           \
            }   \
            }
        #endif
        #define DEBUG_E(level, fmt, args...) { \
            if (log_level >= level) \
                printm(MODULE_NAME,  fmt, ## args); \
            printk("[favce]" fmt, ##args); }

        #define DEBUG_W(level, fmt, args...) { \
            if (log_level >= level) \
                printm(MODULE_NAME,  fmt, ## args); \
            printk("[favce]" fmt, ##args); }
        
        #define favce_err(fmt, args...) { \
            if (log_level >= LOG_ERROR) \
                printm(MODULE_NAME,  fmt, ## args); \
            printk("[favce]" fmt, ##args); }

        #define favce_wrn(fmt, args...) { \
            if (log_level >= LOG_WARNING) \
                printm(MODULE_NAME,  fmt, ## args); }
        
        #define favce_info(fmt, args...) { \
            if (log_level >= LOG_INFO) \
                printm(MODULE_NAME,  fmt, ## args); }
        #if 0
        #define mcp280_debug(fmt, args...)  printm(MODULE_NAME, fmt, ##args);
        //#define mcp280_debug(fmt, args...)  printk(fmt, ##args);
        #else
        #define mcp280_debug(...)
        #endif
    #else
        #define DEBUG_M(level, fmt, args...) { \
            if (log_level >= level) \
                printk(fmt, ## args); }
        #define DEBUG_E(level, fmt, args...) { \
            printk("[error]" fmt, ##args); }
        
        #define favce_err(fmt, args...) { \
            if (log_level >= LOG_ERROR) \
               printk("[favce]" fmt, ##args); }
        
        #define favce_wrn(fmt, args...) { \
            if (log_level >= LOG_WARNING) \
                printk(fmt, ## args); }
        
        #define favce_info(fmt, args...) { \
            if (log_level >= LOG_INFO) \
                printk(fmt, ## args); }
    #endif

#endif				/* __DEBUG_H__ */
