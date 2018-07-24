/* favc_dec_ll.c */

#include <linux/module.h>
#include <linux/kernel.h>
#include "define.h"
#include "favc_dec_ll.h"



/*
 * a table for translating linked list register offset to original offset (get_reg_offset())
 */
static const unsigned int reg_base[] =
{
    0x00000, 
    0x10c00, 
    0x10d00, 
    0x10d80, 
    0x10e00, 
    0x10e80, 
    0x10f00, 
    0x10f80, 
    0x20000, 
    0x20080, 
    0x20280, 
    0x20300, 
    0x20380, 
    0x40000, 
    0x40080, 
    0x00000, 
};


/*
 * translate the linked list register offest to oringinal register offset
 */
unsigned int get_reg_offset(unsigned int ll_reg)
{
    
    unsigned int reg_offset;
    unsigned int reg_idx = (ll_reg >> 7) & 0xFF;
    unsigned int reg_sub_offset = (ll_reg & ((1 << 7) - 1));

    /*
     * original reg offset = reg_base[ll_reg[15:7]] + ll_reg[6:0]
     */

#if 0
    if(reg_idx > 15){
        printk("get_reg_addr error\n");
        return 0;
    }
#endif
    reg_offset = reg_base[reg_idx];
    reg_offset += reg_sub_offset;
    
    return reg_offset;
}



#if USE_LINK_LIST

#define DBG_SW_EXEC_CMD   0
#define USE_WRITE_OUT_CMD 1

#define BIT_FIELD(VAL, N_BIT, SHIFT) ((uint32_t)(((VAL) >> SHIFT) & N_BIT_MASK(N_BIT)))


/*
 * init a linked list job
 */
void ll_job_init(struct link_list_job *job, unsigned int job_id, unsigned int cmd_pa)
{
    memset(job, 0, sizeof(*job));
    job->job_id = job_id;
    job->cmd_pa = cmd_pa;
    ll_add_cmd(job, job_header_cmd(job_id, 0, 0)); /* add a dummy job header */

#if SW_LINK_LIST == 0 && USE_WRITE_OUT_CMD == 1
    /*
     * Add a write out command for each jobs
     * NOTE: 
     *    1. 'write out command' is the only command that is NOT executed in order.
     *       HW linked list controller will execute this command *AFTER* the interrupt of this job is received. 
     *       The read out values are used for error handling.
     *
     *    2. the wirte out address must be 8-byte aligned
     */
    ll_add_cmd(job, write_out_cmd((MCP300_REG + 0x00) >> 2, 22, cmd_pa + (unsigned int)&job->status[0] - (unsigned int)job->cmd));
    memset(job->status, 0xFF, sizeof(job->status));
#endif
}


/*
 * reinit a linked list job
 */
void ll_job_reinit(struct link_list_job *job)
{
    //ll_job_init(job, job->job_id);
    job->cmd_num = 1; /* job header is kept */
#if (SW_LINK_LIST == 0) && (USE_WRITE_OUT_CMD == 1)
    job->cmd_num = 2; /* write out command is kept */
#endif
    job->max_cmd_num = 0;
    job->exe_cmd_idx = 0;
    job->has_sw_cmd = 0;
}


/*
 * init a linked list
 */
void ll_init(struct codec_link_list *codec_ll, unsigned int init_id)
{
    int i;
    //printk("ll_init\n");
    //printk("ll_pa:%08X\n", codec_ll->ll_pa);
    
    codec_ll->job_num = 0;
    codec_ll->curr_job = &codec_ll->jobs[0];
    for(i = 0; i < LL_MAX_JOBS; i++){
        //printk("i:%d\n", i);
        ll_job_init(&codec_ll->jobs[i], i + init_id + 1, codec_ll->ll_pa + (unsigned int)&codec_ll->jobs[i] - (unsigned int)codec_ll);
        //printk("cmd_pa:%08X\n", codec_ll->jobs[i].cmd_pa);
    }

#if 0
    ll_add_cmd(codec_ll->curr_job, single_write_cmd(1, 0xFFFF0000));
    ll_add_cmd(codec_ll->curr_job, bitwise_and_cmd(0x30, 0x12345678));
    ll_add_cmd(codec_ll->curr_job, bitwise_or_cmd(0x34, 0x12345678));
    ll_add_cmd(codec_ll->curr_job, poll_zero_cmd(0x38, 0x12340000));
    ll_add_cmd(codec_ll->curr_job, poll_one_cmd(0x40, 0x00005678));

    //ll_print_job(&codec_ll->jobs[0]);
    
    ll_add_job(codec_ll, NULL);
    ll_add_cmd(codec_ll->curr_job, single_write_cmd(1, 0xFFFFEEEE));
    ll_add_job(codec_ll, NULL);

    ll_print_ll(codec_ll);
#endif
}


/*
 * Re-init a linked list
 */
void ll_reinit(struct codec_link_list *codec_ll)
{
    int i;
    //printk("ll_reinit\n");
    codec_ll->job_num = 0;
    codec_ll->curr_job = &codec_ll->jobs[0];
    for(i = 0; i < LL_MAX_JOBS; i++){
        ll_job_reinit(&codec_ll->jobs[i]);
    }
    
    //ll_init(codec_ll, codec_ll->jobs[0].job_id);
}


/*
 * Print linked list jobs
 */
void ll_print_ll(struct codec_link_list *codec_ll)
{
    int i;

    if(codec_ll == NULL){
        printk("codec_ll is NULL\n");
        return;
    }
    
    printk("codec ll job num:%d\n", codec_ll->job_num);
    for(i = 0; i < codec_ll->job_num; i++){
        ll_print_job(&codec_ll->jobs[i]);
    }
}

/*
 * Print linked list command of a job
 */
void ll_print_job(struct link_list_job *job)
{
    int i;
    int ret;
    printk("job id: %d cmd num:%d va:%08X pa:%08X st_pa:%08X\n", job->job_id, job->cmd_num, (unsigned int)job->cmd, job->cmd_pa, 
        job->cmd_pa + (unsigned int)&job->status[0] - (unsigned int)job->cmd);
    for(i = 0; i < job->cmd_num; i++){
        ret = ll_print_cmd(&job->cmd[i]);
        if(ret){
            i += ret;
        }
    }
}

#if SW_LINK_LIST
/*
 * set MCP300/VCACHE register's base address (for SW linked list only)
 */
void ll_set_base_addr(struct codec_link_list *codec_ll, unsigned int base_addr, unsigned int vc_base_addr)
{
    codec_ll->base_va = base_addr;
    codec_ll->vc_base_va = vc_base_addr;
}

#if SW_LINK_LIST == 2
/*
 * Execute all commands of a job by SW
 */
int ll_sw_exec_job2(struct codec_link_list *codec_ll, struct link_list_job *job)
{
    int i;
    int ret = 0;
#if DBG_SW_EXEC_CMD
    printk("sw exec job id: %d cmd num:%d\n", job->job_id, job->cmd_num);
#endif
    for(i = 0; i < job->cmd_num; i++){
        job->exe_cmd_idx = i;
        ret = ll_sw_exec_cmd2(codec_ll, &job->cmd[i]);
        if(ret < 0){
            printk("command %d err:%d\n", i, ret);
            break;
        }
        if(ret > 0){
            i += ret; /* skip param for SW cmd */
            ret = 0;
        }
    }

    return ret;
}

/*
 * Execute all jobs in linked list by SW
 */
int ll_sw_exec2(struct codec_link_list *codec_ll)
{
    int i;
    int ret = 0;
#if DBG_SW_EXEC_CMD
    printk("sw exec: job num:%d\n", codec_ll->job_num);
#endif
    for(i = 0; i < codec_ll->job_num; i++){
         codec_ll->exe_job_idx = i;
         ret = ll_sw_exec_job2(codec_ll, &codec_ll->jobs[i]);
         if(ret){
             printk("job %d err:%d\n", i, ret);
             break;
         }
    }
    return ret;
}
#endif /* SW_LINK_LIST == 2 */
#endif /* SW_LINK_LIST */

/*
 * print a linked list command
 */
int ll_print_cmd(union link_list_cmd *ll_cmd_ptr)
{
    unsigned int cmd_type;
    unsigned int sub_type;
    unsigned int ll_offset;
    unsigned int reg_offset;
    union link_list_cmd ll_cmd = *ll_cmd_ptr;
    int ret = 0;
    cmd_type = BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 60);
    sub_type = BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 56);

    printk("[%016llX] ", ll_cmd.cmd);

    switch(cmd_type){
        case LCT_JOB_HEADER:
            printk("    LL_JOB_HDR:%d id:%d nxt_cmd_num:%d last:%d next_job_addr:%08X\n", cmd_type, 
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 11, 49),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 14, 35),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 1, 32),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0));
            break;
        case LCT_REG_ACCESS:
            ll_offset = BIT_FIELD(ll_cmd.hw_cmd.cmd, 12, 32);
            switch(sub_type){
                case LCST_SINGLE_WRITE:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk(" LL_REG_ACCESS:%d  SINGLE_WRITE:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                case LCST_BITWISE_AND:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk(" LL_REG_ACCESS:%d   BITWISE_AND:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                case LCST_BITWISE_OR:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk(" LL_REG_ACCESS:%d    BITWISE_OR:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                case LCST_SUB:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk(" LL_REG_ACCESS:%d           SUB:%d offset:%08X sub:%d zero_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 44),
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                case LCST_VLC_AUTO_PACK:
                    printk(" LL_REG_ACCESS:%d VLC_AUTO_PACK:%d skip_syntax_num:%d syntax_idx:%d\n", cmd_type, sub_type,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 5, 32),
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 5, 0));
                    break;
                default:
                    printk(" LL_REG_ACCESS:%d unknown:%d %016llX\n", cmd_type, sub_type, ll_cmd.hw_cmd.cmd);
                    break;
            }
            break;
        case LCT_REG_POLLING:
            ll_offset = BIT_FIELD(ll_cmd.hw_cmd.cmd, 12, 32);
            switch(sub_type){
                case LCST_POLLING_ZERO:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk("LL_REG_POLLING:%d  POLLING_ZERO:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                case LCST_POLLING_ONE:
                    reg_offset = get_reg_offset(ll_offset << 2);
                    printk("LL_REG_POLLING:%d   POLLING_ONE:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                    break;
                default:
                    printk("LL_REG_POLLING:%d unknown:%d %016llX\n", cmd_type, sub_type, ll_cmd.hw_cmd.cmd);
                    break;
            }
            break;
        case LCT_WRITE_OUT:
            ll_offset = BIT_FIELD(ll_cmd.hw_cmd.cmd, 12, 32);
            reg_offset = get_reg_offset(ll_offset << 2);
            printk("LL_REG_WRITE_OUT:%d  offset:%08X addr:%08X num:%d reg:%08X%s\n", cmd_type,
                ll_offset,
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 8, 48),
                reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
            break;
        case LCT_SW_CMD:
#if SW_LINK_LIST == 2
            {
                void   (*func)(uint32_t, uint32_t);
                unsigned int param1;
                unsigned int param2;
                unsigned int tmp;
                param1 = BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0);
                tmp = BIT_FIELD(ll_cmd.hw_cmd.cmd, 16, 32) << 16;
                ll_cmd = *(ll_cmd_ptr + 1);
                tmp |= BIT_FIELD(ll_cmd.hw_cmd.cmd, 16, 32);
                param2 = BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0);
                func = (void *)tmp;
                printk("LCT_SW_CMD:%d   func:%08X param1:%08X param2:%08X\n", cmd_type, (unsigned int)func, param1, param2);
                ret = 1;
            }
#endif
            break;
        default:
            printk("unknown LL_CMD_TYPE:%d %016llX\n", cmd_type, ll_cmd.hw_cmd.cmd);
            break;
    }
    return ret;
}

#if SW_LINK_LIST
#if SW_LINK_LIST == 2
/*
 * execute one linked list command by SW
 * NOTE: LCT_SW_CMD takes two command slot (2*64 bytes).
 */
int ll_sw_exec_cmd2(struct codec_link_list *codec_ll, union link_list_cmd *ll_cmd_ptr)
{
    unsigned int cmd_type;
    unsigned int sub_type = 0;
    unsigned int ll_offset = 0;
    unsigned int reg_offset = 0;
    volatile unsigned int *ptr = NULL;
    unsigned int data;
    unsigned int cnt;
    union link_list_cmd ll_cmd = *ll_cmd_ptr;
    int ret = 0;
    cmd_type = BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 60);

    if(cmd_type > LCT_JOB_HEADER && cmd_type < LCT_SW_CMD){
        sub_type = BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 56);
        ll_offset = BIT_FIELD(ll_cmd.hw_cmd.cmd, 12, 32);
        reg_offset = get_reg_offset(ll_offset << 2);
        if((ll_offset >> 5) == MCP300_VC){
            ptr = (volatile unsigned int *)(codec_ll->vc_base_va + reg_offset);
        }else{
            ptr = (volatile unsigned int *)(codec_ll->base_va + reg_offset);
        }
    }
    data = BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0);


    //printk("[%016llX] ", ll_cmd.cmd);

    switch(cmd_type){
        case LCT_JOB_HEADER:
#if DBG_SW_EXEC_CMD
            printk("    LL_JOB_HDR:%d id:%d nxt_cmd_num:%d last:%d next_job_addr:%08X\n", cmd_type, 
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 11, 49),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 14, 35),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 1, 32),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0));
#endif
            break;
        case LCT_REG_ACCESS:
            switch(sub_type){
                case LCST_SINGLE_WRITE:
#if DBG_SW_EXEC_CMD
                    printk(" LL_REG_ACCESS:%d  SINGLE_WRITE:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset, data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    *ptr = data;
                    break;
                case LCST_BITWISE_AND:
#if DBG_SW_EXEC_CMD
                    printk(" LL_REG_ACCESS:%d   BITWISE_AND:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset, data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    *ptr &= data;
                    break;
                case LCST_BITWISE_OR:
#if DBG_SW_EXEC_CMD
                    printk(" LL_REG_ACCESS:%d    BITWISE_OR:%d offset:%08X data:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset, data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    *ptr |= data;
                    break;
                case LCST_SUB:
#if DBG_SW_EXEC_CMD
                    printk(" LL_REG_ACCESS:%d           SUB:%d offset:%08X sub:%d zero_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset, 
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 44),
                        data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    {
                        unsigned int mask_shift, sub;
                        unsigned int zero_mask;
                        unsigned int tmp;
                        sub = BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 44);
                        zero_mask = data;
                        for(mask_shift = 0; mask_shift < 32; mask_shift++){
                            if((zero_mask & (1 << mask_shift))){
                                break;
                            }
                        }

                        tmp = (*ptr & zero_mask) >> mask_shift;
                        tmp -= sub;
                        tmp = (tmp << mask_shift) & zero_mask;
                        *ptr = (*ptr & ~zero_mask) | tmp;
                    }

                    break;
#if 0
                case LCST_VLC_AUTO_PACK:
                    printk(" LL_REG_ACCESS:%d VLC_AUTO_PACK:%d skip_syntax_num:%d syntax_idx:%d\n", cmd_type, sub_type,
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 5, 32),
                        BIT_FIELD(ll_cmd.hw_cmd.cmd, 5, 0));
                    break;
#endif
                default:
                    printk(" LL_REG_ACCESS:%d unknown:%d %016llX\n", cmd_type, sub_type, ll_cmd.hw_cmd.cmd);
                    break;
            }
            break;
        case LCT_REG_POLLING:
            switch(sub_type){
                case LCST_POLLING_ZERO:
#if DBG_SW_EXEC_CMD
                    printk("LL_REG_POLLING:%d  POLLING_ZERO:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    data = ~data; /* convert one_mask to zero_mask */
                    cnt = 0;
                    while((*ptr & data) != 0){
                        cnt++;
                        if(cnt >= SW_POLLING_ITERATION_TIMEOUT){
#if DBG_SW_LINK_LIST
                            printk("POLLING_ZERO time out: %s\n", ll_cmd.hw_cmd.str);
#else
                            printk("POLLING_ZERO time out\n");
#endif
                            printk("POLLING_ZERO reg val:%08X  cmd val:%08X cmd:%016llX cnt:%d\n", *ptr, data, ll_cmd.hw_cmd.cmd, cnt);
                            printk("LL_REG_POLLING:%d  POLLING_ZERO:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                                ll_offset,
                                data,
                                reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");

                            ret = -1;
                            break;
                        }
                    }
                    break;
                case LCST_POLLING_ONE:
#if DBG_SW_EXEC_CMD

                    printk("LL_REG_POLLING:%d   POLLING_ONE:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                        ll_offset,
                        data,
                        reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
                    cnt = 0;
                    while((*ptr & data) != data){
                        cnt++;
                        if(cnt >= SW_POLLING_ITERATION_TIMEOUT){
#if DBG_SW_LINK_LIST
                            printk("POLLING_ONE time out: %s\n", ll_cmd.hw_cmd.str);
#else
                            printk("POLLING_ONE time out\n");
#endif
                            printk("POLLING_ONE reg val:%08X  cmd val:%08X cmd:%016llX cnt:%d\n", *ptr, data, ll_cmd.hw_cmd.cmd, cnt);
                            printk("LL_REG_POLLING:%d   POLLING_ONE:%d offset:%08X one_mask:%08X reg:%08X%s\n", cmd_type, sub_type,
                                ll_offset,
                                data,
                                reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
                            ret = -1; // DISABLE polling one time out error
                            break;
                        }
                    }
                    break;
                default:
                    printk("LL_REG_POLLING:%d unknown:%d %016llX\n", cmd_type, sub_type, ll_cmd.hw_cmd.cmd);
                    break;
            }
            break;
        case LCT_WRITE_OUT:
#if DBG_SW_EXEC_CMD
            printk("LL_REG_WRITE_OUT:%d  offset:%08X addr:%08X num:%d reg:%08X%s\n", cmd_type,
                ll_offset,
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0),
                BIT_FIELD(ll_cmd.hw_cmd.cmd, 8, 48),
                reg_offset, ((ll_offset >> 5) == MCP300_VC)?"V":"");
#endif
            break;
        case LCT_SW_CMD:
            {
                void   (*func)(uint32_t, uint32_t);
                unsigned int param1;
                unsigned int param2;
                unsigned int tmp;
                param1 = data;
                tmp = BIT_FIELD(ll_cmd.hw_cmd.cmd, 16, 32) << 16;
                ll_cmd = *(ll_cmd_ptr + 1);
                tmp |= BIT_FIELD(ll_cmd.hw_cmd.cmd, 16, 32);
                param2 = BIT_FIELD(ll_cmd.hw_cmd.cmd, 32, 0);

                func = (void *)tmp;
#if DBG_SW_EXEC_CMD
                printk("LCT_SW_CMD:%d   func:%08X param1:%08X param2:%08X\n", cmd_type, (unsigned int)func, param1, param2);
#endif
                func(param1, param2);
                ret = 1;
            }
            break;
        default:
            printk("unknown LL_CMD_TYPE:%d %016llX\n", cmd_type, ll_cmd.hw_cmd.cmd);
            ret = -1;
            break;
    }

    return ret;
}
#endif /* SW_LINK_LIST == 2 */
#endif /* SW_LINK_LIST */


/*
 * add a linked list command to a job
 */
int ll_add_cmd(struct link_list_job *ll_job, union link_list_cmd ll_cmd)
{
    //printk("ll_add_cmd %d %016llX\n", ll_job->cmd_num, ll_cmd.hw_cmd.cmd);
    if(ll_job->cmd_num >= LL_MAX_JOB_CMDS){
        ll_job->max_cmd_num++;
        printk("link list command overflow: cmd_num:%d/%d max_cmd_num:%d\n", ll_job->cmd_num, LL_MAX_JOB_CMDS, ll_job->max_cmd_num);
        return -1;
    }

    ll_job->cmd[ll_job->cmd_num] = ll_cmd;
    ll_job->cmd_num++;
    ll_job->max_cmd_num++;

#if SW_LINK_LIST == 2
    /* Mark a flag to indicate that the job can be executed by SW only */
    if(BIT_FIELD(ll_cmd.hw_cmd.cmd, 4, 60) == LCT_SW_CMD){
        ll_job->has_sw_cmd = 1;
    }
#endif

    return 0;
}

/*
 * add a job to the linked list
 */
int ll_add_job(struct codec_link_list *codec_ll, struct link_list_job *job, void *dec_handle)
{
    if(codec_ll->job_num >= LL_MAX_JOBS){
        printk("link list job overflow\n");
        return -1;
    }

    if(job){ /* NULL job indicates job already added */
        codec_ll->jobs[codec_ll->job_num] = *job;
    }
    codec_ll->jobs[codec_ll->job_num].dec_handle = dec_handle;
    
    codec_ll->job_num++;
    codec_ll->curr_job++;
    
    if(codec_ll->job_num >= LL_MAX_JOBS){
        printk("link list job full: %d\n", codec_ll->job_num);
        codec_ll->curr_job = NULL;
    }
    
    return 0;
}

#endif /* USE_LINK_LIST */


