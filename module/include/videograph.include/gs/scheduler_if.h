/**
 * @file scheduler_if.h
 *  scheduler interface header
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/05/23 08:14:13 $
 *
 * ChangeLog:
 *  $Log: scheduler_if.h,v $
 *  Revision 1.2  2016/05/23 08:14:13  ivan
 *  add gs ver_no to ms layout change and notify
 *
 *  Revision 1.1.1.1  2016/03/25 10:39:11  ivan
 *
 *
 *
 */
#ifndef __SCHEDULER_IF_H__
#define __SCHEDULER_IF_H__

#ifdef __SCHEDULRE_C__
#define SCH_IF_EXT
#else
#define SCH_IF_EXT extern
#endif

#include "graph_service_if.h"

#define SCH_MAX_NAME_LEN         31

SCH_IF_EXT int sch_init(void);
SCH_IF_EXT int sch_exit(void);
SCH_IF_EXT int sch_job_callback(void *data);
SCH_IF_EXT void sch_print_all_structure_values(int log_where, char *message, struct seq_file *s, int apply);

#define LOG_TYPE_ENTITY             0
#define LOG_TYPE_BUFFER             1
#define LOG_TYPE_JOB                2
#define LOG_TYPE_RUNBUFFER          3
#define LOG_TYPE_GRAPH_OVERVIEW     4
#define LOG_TYPE_TIME_SYNC_HEAD_TS  6
#define LOG_TYPE_ENODE_DATA         7
#define LOG_TYPE_BNODE_DATA         8
#define LOG_TYPE_SCH_LIST           9
#define LOG_TYPE_VG_GRAPH           10

SCH_IF_EXT void sch_print_log(int log_where, int item, struct seq_file *s);

#define IN      0
#define OUT     1
#define BOTH    2
SCH_IF_EXT int sch_dump_buffer(char *p_entity_name, int in_out);
SCH_IF_EXT int sch_dump_buffer_to_log(char *p_entity_name, int in_out, int on_off);

#define CONFIG_JOB_TIMER_DELAY      0
#define CONFIG_TICK_TIMER_DELAY     1
#define CONFIG_GROUP_TIMER_DELAY    2
#define CONFIG_WAITBUF_TIMER_DELAY  3
#define CONFIG_WAITBUF_TIMEOUT      4
SCH_IF_EXT int sch_get_config(int config_type);
SCH_IF_EXT int sch_set_config(int config_type, int value);
SCH_IF_EXT int sch_change_pool_layout(void *pool_hdl[], int num, int ver);
SCH_IF_EXT int sch_check_job_schduler(ENTITY_FD entity_fd);
SCH_IF_EXT int sch_check_and_add_timesync_buffer(ENTITY_FD entity_fd);
SCH_IF_EXT int sch_get_upstream_entity_job_count(ENTITY_FD entity_fd);
SCH_IF_EXT int sch_get_entity_job_count(ENTITY_FD entity_fd);
SCH_IF_EXT int sch_get_entity_buffer_count(ENTITY_FD entity_fd, int in_out);

#define BUF_READY_TO_USE    0
#define BUF_ONGOING         1
SCH_IF_EXT int sch_check_buffer_existence(ENTITY_FD entity_fd, int in_out, int which);

#endif /* __SCHEDULER_IF_H__ */

