/**
 * @file graph_service_if.h
 *  graph service interface header
 * Copyright (C) 2016 GM Corp. (http://www.grain-media.com)
 *
 * $Revision: 1.2 $
 * $Date: 2016/03/31 02:11:31 $
 *
 * ChangeLog:
 *  $Log: graph_service_if.h,v $
 *  Revision 1.2  2016/03/31 02:11:31  ivan
 *  add to reference ddr_id of ms pool info
 *
 *  Revision 1.1.1.1  2016/03/25 10:39:11  ivan
 *
 */
#ifndef __GRAPH_SERVICE_IF_H__
#define __GRAPH_SERVICE_IF_H__


#ifdef __GRAPH_SERVICE_C__
#define GS_EXT
#else
#define GS_EXT extern
#endif

#include <linux/types.h>
#define GS_MAX_HYBRID_NUM           8
#define GS_MAX_HYBRID_POOL_MEMBERS  8

#define GS_MAX_NAME_LEN         31
#define MAX_PRIVATE_POOL_COUNT  3
#define DONT_CARE               0xFF5417FF
#define ENTITY_LOWEST_PRIORITY  0xFA5417AF


#define REMOVE_OLD_VER          0
#define KEEP_OLD_VER            1

#define POOL_FIXED_SIZE         0
#define POOL_VAR_SIZE           1

#define AS_INPUT                0
#define AS_OUTPUT               1

#define METHOD_BY_TIMESTAMP     0
#define METHOD_BY_RATIO         1

#define GRAPH_VER_IDLE          0
#define GRAPH_VER_START         1

#define MAX_BUF_COUNT           0
#define MIN_BUF_COUNT           1

#define UPPER_STREAM            0
#define DOWN_STREAM             1

typedef enum entity_mode_tag {
    MODE_NONE = 0,
    MODE_FLOW_BY_TICK = 1,              /* value1: TRUE, FALSE */
    MODE_ENODE_STATUS = 2,              /* value1: ENODE_ENABLE, ENODE_NO_PJ, ENODE_DIRECT_CB */
    MODE_ACCEPT_DIFF_VER_INPUT = 3,     /* value1: TRUE, FALSE */
    MODE_DO_NOT_CARE_CALLBACK_FAIL = 4, /* value1: TRUE, FALSE */
    MODE_ENTITY_NON_BLOCKING = 5,       /* value1: TRUE, FALSE */
    MODE_DIR_STOP_WHEN_APPLY = 6,       /* value1: TRUE, FALSE */
    MODE_JOB_FLOW_PRIORITY = 8,         /* value1: 0(low) ~ 1(high) */
    MODE_TICK_ONE_JOB = 9,              /* value1: TRUE, FALSE */
    MODE_LOW_FRAMERATE = 10,                  /* value1: TRUE, FALSE */
    MODE_JOB_SEQUENCE                  /* value1: 0(last), 1(first) */
} entity_mode_t;

// Define priority for 'MODE_JOB_FLOW_PRIORITY'
#define PRIO_NORMAL     0
#define PRIO_HIGH       1

#define POOL_OPTION_CAN_FROM_FREE_AT_APPLY   (1<<0)

//entity node status
#define ENODE_ENABLE        0 // define entity node status, normal operation
#define ENODE_NO_PJ         1 // entity node disable and does not put job
#define ENODE_DIRECT_CB     2 // entity node disable and direct call back
#define ENODE_DIRECT_CB_WITH_BUF    3 // aquire buffer but not to putjob

#ifndef TRUE
#define TRUE (1==1)
#endif
#ifndef FALSE
#define FALSE (1==0)
#endif

typedef int  (*fn_graph_notify_t)(int status, int graph_ver_id, void *private_data);


typedef   unsigned int      ENTITY_FD;
typedef   void*             GRAPH_HDL;
typedef   void*             BUFFER_HDL;
typedef   void*             APPLY_LIST_HDL;
typedef   void*             BUFFER_POOL_HDL;
typedef   void*             POOL_LAYOUT_HDL;

typedef struct fragment_tag {
    int  is_used;
    unsigned int ddr_id;
    unsigned long start_va;
    unsigned long start_pa;
} fragment_t;

typedef struct mspool_info_tag {
    BUFFER_POOL_HDL pool;
    unsigned int ddr_vch;
    unsigned int size;
    int type;
    char *name;
    int option;
    int flag;
    int priv_date;  /* vpd used, pool_type */
} mspool_info_t;

GS_EXT int    gs_init(void);
GS_EXT int    gs_exit(void);
GS_EXT GRAPH_HDL  gs_create_graph(fn_graph_notify_t fn_graph_notify, void *private_data, char *name);
GS_EXT int    gs_destroy_graph(GRAPH_HDL graph);
GS_EXT int    gs_change_graph_name(GRAPH_HDL graph, char *name);
GS_EXT int    gs_new_graph_version(GRAPH_HDL graph, int ver_no);
GS_EXT int    gs_del_graph_version(int graph_ver_id);
GS_EXT int    gs_get_ver_id(GRAPH_HDL graph, int ver_no);
GS_EXT int    gs_get_graph_ver_no(int graph_ver_id, GRAPH_HDL *graph, int *ver_no);
GS_EXT int    gs_add_link(int graph_ver_id, ENTITY_FD from_entity, ENTITY_FD to_entity,
                          BUFFER_POOL_HDL buf_pool, BUFFER_POOL_HDL buf_pool2, void *p_buffer_info,
                          char *buf_name, BUFFER_HDL *ret_bnode_hdl);
GS_EXT int    gs_remove_link(int graph_ver_id, ENTITY_FD from_entity, ENTITY_FD to_entity);
GS_EXT int    gs_verify_link(int graph_ver_id);
GS_EXT int    gs_create_buffer_sea(int ddr_no, int max_size);
GS_EXT int    gs_destroy_buffer_sea(int ddr_no);
GS_EXT int gs_create_hybrid_buffer_pool(mspool_info_t *mspool_info, int no);
GS_EXT int    gs_destroy_buffer_pool(BUFFER_POOL_HDL buf_pool);
GS_EXT u32    gs_which_buffer_sea(u32 va);
GS_EXT u32    gs_offset_in_buffer_sea(u32 va);
GS_EXT u32    gs_buffer_sea_pa_base(u8 ddr_no);
GS_EXT u32    gs_buffer_sea_size(u8 ddr_no);
GS_EXT POOL_LAYOUT_HDL  gs_new_pool_layout(BUFFER_POOL_HDL buf_pool);
GS_EXT int    gs_del_pool_layout(POOL_LAYOUT_HDL layout);
GS_EXT int    gs_reset_pool_layout(POOL_LAYOUT_HDL layout);
GS_EXT int gs_set_ms_extra_layout(BUFFER_POOL_HDL src_pool_hdl, BUFFER_POOL_HDL extra_pool_hdl, 
                             unsigned int size, unsigned int count);
GS_EXT int    gs_change_fix_pool_layout_count(POOL_LAYOUT_HDL pool_layout, unsigned int size, 
                                              unsigned int count, int idx, int hybridid);
GS_EXT int    gs_change_var_pool_layout_count(POOL_LAYOUT_HDL pool_layout, unsigned int size, 
                                              unsigned int win_size, ENTITY_FD entity, int idx,
                                              int hybridid);
GS_EXT int    gs_get_pool_fragment_entry(BUFFER_POOL_HDL buf_pool, fragment_t fragment_ary[],
                                         int *p_count);
GS_EXT int    gs_buffer_is_reserved(void *);
GS_EXT APPLY_LIST_HDL gs_create_apply_list(void);
GS_EXT int    gs_destroy_apply_list(APPLY_LIST_HDL apply_list);
GS_EXT int    gs_reset_apply_list(APPLY_LIST_HDL apply_list);
GS_EXT int    gs_add_graph_to_apply_list(APPLY_LIST_HDL apply_list, int graph_ver_id);
GS_EXT int    gs_apply(APPLY_LIST_HDL apply_list);
GS_EXT int    gs_set_flow_ratio(int graph_ver_id, ENTITY_FD entity, int method, int input_rate,
                                int output_rate);
GS_EXT int    gs_set_head_entity_max_count(int graph_ver_id, ENTITY_FD head_entity, int count);
GS_EXT int    gs_set_tail_entity_min_count(int graph_ver_id, ENTITY_FD tail_entity, int count);
GS_EXT int    gs_set_buffer_count(int graph_ver_id, char *buf_name, int count, int max_min);
GS_EXT int    gs_set_buffer_count_by_buffer_handle(BUFFER_HDL buf_hdl, int count, int max_min);

GS_EXT int    gs_set_reference_clock(int graph_ver_id, ENTITY_FD entity, int fps_or_interval);
GS_EXT int    gs_set_entity_mode(int graph_ver_id, ENTITY_FD entity, int mode, int value1, int value2);
GS_EXT int    gs_set_entity_group_action(int graph_ver_id, ENTITY_FD entity, unsigned int id, int priority);

GS_EXT int    gs_query_entity_fd(ENTITY_FD entity_fd, int up_down, ENTITY_FD *out_fd_ary, int max_count);
GS_EXT int    gs_reset_graph_entities(unsigned int graph_ver_id);
GS_EXT void   gs_print_all_structure_values(void);

#define MODE_GRAPH              0
#define MODE_PERF               1
#define MODE_STAT               2
#define MODE_GRAPH_OLD_STYLE    3
GS_EXT void   gs_print_graph(int log_where, struct seq_file *s, int mode);
GS_EXT void   gs_vg_statistic_print(int log_where, struct seq_file *s,
                                    char *p_graph_name, int period);

/**
  * Error code
  */
#define GS_OK                       ( 0)
#define GS_CANNOT_FIND_GRAPH_VER    (-1)    /* Must be an odd number */
#define GS_INVALID_ARGUMENT         (-2)
#define GS_DUPLICATED_LINK          (-3)
#define GS_LINK_CONFLICT            (-4)
#define GS_INTERNAL_ERROR           (-5)
#define GS_WRONG_GRAPH              (-6)
#define GS_OUT_OF_RANGE             (-7)
#define GS_RESOURCE_LEAK            (-8)
#define GS_OLD_VERSION_NOT_FINISH   (-9)
#define GS_PARTIAL_FDS              (-10)



#endif /* __GRAPH_SERVICE_IF_H__ */

