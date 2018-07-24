#include <linux/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <poll.h>
#include <asm/ioctl.h>

#if (LINUX_VERSION_CODE == KERNEL_VERSION(3,3,0))
#include <frammap/frammap_if.h>
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,14))
#include <frammap/frm_api.h>
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,28))
#include <frammap/frammap_if.h>
#endif

#include <test_api.h>

//#define TEST_NUM        10
//#define TEST_NUM        40
//#define TEST_NUM        1000
#define TEST_NUM        MAX_JOBS
#define RECORD_AS_POC_ORDER
#define MAX_FILE_NUM    32
#define MAX_FILENAME_LEN 256
#define H264D_PADDING_SIZE 4 // number of padded bytes after input bitstream data. added to make the test behavior same as VG datain

#define REV_PLAY 1

//#define DBG_PRINT(format, arg...)    printf(format, ## arg)
#define DBG_PRINT(format, arg...) 

static unsigned int in_buf_ddr=1,in_buf_type=0,in_size=0;
static unsigned int out_buf_ddr=1,out_buf_type=0,out_size=0;

struct video_cfg_t {
    int enable;
    int round_set;  //set from config file
    int type;
    int engine;
    int minor;
    int pat_idx;
    int sub_yuv_ratio;
    struct video_param_t param[MAX_PROPERTYS];
};

struct video_cfg_t cfg[TEST_NUM];
static int mem_fd[2],em_fd=0;


static unsigned int __get_buf_checksum(const char *buf, unsigned int len)
{
    int i;
    unsigned int sum = 0;
    for(i = 0; i < len; i++){
        sum += buf[i];
    }

    return sum;
}

static int __print_buf(const char *buf, unsigned int len)
{
    int i;
    unsigned int sum;
    printf("idata[%08X]: ", (unsigned int)buf);
    for(i = 0; i < 16; i++){
        printf("%02X ", buf[i]);
    }
    sum = __get_buf_checksum(buf, len);
    printf("sum:0x%08X ", sum);
    printf("len:%d\n", len);

    return 0;
}


/*
 * retval: max channel number
 */
int prepare_input_buffer_by_job_cases(struct job_case_t *job_cases, 
    unsigned int start, int fr_num, unsigned int buffer_sz, 
    char fn[MAX_FILE_NUM][MAX_FILENAME_LEN], unsigned int bs_len[MAX_JOBS], int fn_num)
{
    int i;
    int max_chn_num = -1;
    unsigned char *address;
    int len, ret = 0, idx;
    unsigned char full_fn[256];
    FILE *bsFile[MAX_FILE_NUM] = {NULL};
    FILE *lenFile[MAX_FILE_NUM] = {NULL};

    if(fn_num > MAX_FILE_NUM){
        printf("Too much file\n");
        return -1;
    }

    for(i = 0; i < fn_num; i++) {
        snprintf(full_fn, sizeof(full_fn), "%s.264", fn[i]);
        if ((bsFile[i] = fopen(full_fn, "rb")) == NULL) {
            printf("Fail to open file %s\n", full_fn);
            ret = -1;
            goto exit_prepare_buffer;
        }
        printf("open file[%d]:%s\n", i, full_fn);
        
        snprintf(full_fn, sizeof(full_fn), "%s.txt", fn[i]);
        if ((lenFile[i] = fopen(full_fn, "rb")) == NULL) {
            printf("Fail to open file %s\n", full_fn);
            ret = -1;
            goto exit_prepare_buffer;
        }
        printf("open txt [%d]:%s\n", i, full_fn);
    }
    
    for(i = 0; i < fr_num; i++) {
        int pat_idx = job_cases[i].minor;
        if(pat_idx > max_chn_num){
            max_chn_num = pat_idx;
        }
        
        if (fscanf(lenFile[pat_idx], "%d", &len) == EOF) {
            printf("load length file error\n");
            ret = -1;
            goto exit_prepare_buffer;
        }
        bs_len[i] = len;
        
        address = ((unsigned char *)start)+ (i*buffer_sz);
        // clear input buffer to zero to avoid padding in driver
        memset(address, 0, buffer_sz);
        
        if (fread(address, 1, len, bsFile[pat_idx]) != len) {
            printf("load bitstream error\n");
            ret = -1;
            goto exit_prepare_buffer;
        }

        #if 0
        DBG_PRINT("%d:%d %02X %02X %02X %02X %02X %02X (len:%d)\n", i, i % fn_num,
            address[0], address[1], address[2], 
            address[3], address[4], address[5], len);
        #else
        //printf("%d:%d ", i, pat_idx);
        //__print_buf(address, len);
        #endif
    }

exit_prepare_buffer:
    for(i = 0; i < MAX_FILE_NUM; i++) {
        if (bsFile[i])
            fclose(bsFile[i]);
        if (lenFile[i])
            fclose(lenFile[i]);
    }

    fflush(stdout);

    ret = max_chn_num;
    
    return ret;
}


int is_idr(unsigned char *buf, unsigned int len)
{
    int is_idr_flg = 0;
    unsigned char start_code[4] = {0x0, 0x0, 0x0, 0x1};

    if(memcmp(buf, start_code, 4) == 0 && buf[4] == 0x67){
        is_idr_flg = 1;
    }
    return is_idr_flg;
}


int get_property_id(struct video_param_t param[MAX_PROPERTYS], const char *str)
{
    int i;
    for(i = 0; i < MAX_PROPERTYS; i++){
        if(strcmp(param[i].name, str) == 0){
            return param[i].id;
        }
    }
    return 0;
}


int update_property(struct video_property_t prop[MAX_PROPERTYS], unsigned int id, unsigned int val)
{
    //printf("update_property: id:%d val:%d\n", id, val);
    int i;
    for(i = 0; i < MAX_PROPERTYS; i++){
        if(prop[i].id == id){
            prop[i].value = val;
            return 1;
        }
    }

    return 0;
}

int add_property(struct video_property_t prop[MAX_PROPERTYS], unsigned int id, unsigned int val)
{
    int i;
    for(i = 0; i < MAX_PROPERTYS - 1; i++){
        if(prop[i].id == 0){
            prop[i].id = id;
            prop[i].value = val;
            prop[i+1].id = 0;
            return 1;
        }
    }

    return 0;
}

/*
 * revert all jobs (for reverse play back only)
 */
void revert_job_cases(struct job_case_t *job_cases, int fr_num)
{
    int i, j;
    struct job_case_t job_case_tmp;
    for(i = 0, j = fr_num - 1; i < j; i++, j--){
        job_case_tmp = job_cases[i];
        job_cases[i] = job_cases[j];
        job_cases[j] = job_case_tmp;
    }
}

#define MAX_GOP_SIZE 100
struct gop_bs_info {
    unsigned int gop_size;
    unsigned int bs_len[MAX_GOP_SIZE];
};

/*
 * gourp input buffers for jobs of the same GOP (for reverse play back only)
 */
int group_input_buffer_of_GOP(struct job_case_t *job_cases, 
    unsigned int start, int fr_num, unsigned int buffer_sz, 
    unsigned int bs_len[MAX_JOBS], int fn_num)
{
    int i, j;
    unsigned char *src_addr;
    unsigned char *dst_addr;
    int len, ret = 0, idx;
    int gop_cnt[MAX_FILE_NUM] = {0};
    unsigned int gop_first_job[MAX_FILE_NUM] = {0};
    unsigned int gop_last_job[MAX_FILE_NUM] = {0};
    unsigned int gop_bs_sz[MAX_FILE_NUM] = {0};
    unsigned int gop_bs_cnt[MAX_FILE_NUM] = {0};
    int pat_idx;
    struct gop_bs_info gop_bs_info[MAX_FILE_NUM];

    // property ID
    int bitstream_size_id;
    int direct_id;

    printf("group_input_buffer_of_GOP\n");

    memset(gop_bs_info, 0, sizeof(gop_bs_info));

    bitstream_size_id = get_property_id(cfg[0].param, "bitstream_size");
    direct_id = get_property_id(cfg[0].param, "direct");

    printf("bitstream_size_id:%d\n", bitstream_size_id);
    printf("direct_id:%d\n", direct_id);

    if(fn_num > MAX_FILE_NUM){
        printf("Too much file\n");
        return -1;
    }

    for(i = 0; i < MAX_FILE_NUM; i++){
        gop_cnt[i] = -1;
        gop_bs_cnt[i] = 0;
    }

    for(i = 0; i < fr_num; i++) {
        int is_idr_flg;
        pat_idx = job_cases[i].minor;

        src_addr = ((unsigned char *)start) + (i * buffer_sz);
        
        /* check if it is an IDR */
        is_idr_flg = is_idr(src_addr, bs_len[i]);
        if(is_idr_flg){
            if(gop_cnt[pat_idx] != -1){ 
                /* end of the previous GOP */
                for(j = gop_first_job[pat_idx]; j <= gop_last_job[pat_idx]; j++){
                    if(job_cases[i].minor == pat_idx){
                        update_property(job_cases[j].in_prop, bitstream_size_id, gop_bs_sz[pat_idx] + H264D_PADDING_SIZE);
                        update_property(job_cases[j].in_prop, direct_id, 1);
                        
                        memcpy((unsigned char *)job_cases[j].in_mmap_va[0] + gop_bs_sz[pat_idx], 
                            &gop_bs_info[pat_idx], 
                            4 * (gop_bs_info[pat_idx].gop_size + 1));
                    }
                }

                printf("[%d]gop[%d]: start job %d last: %d bs_cnt:%d bs_sz:%d va:%08x gop_size:%d\n", 
                    pat_idx, gop_cnt[pat_idx], gop_first_job[pat_idx], 
                    gop_last_job[pat_idx], gop_bs_cnt[pat_idx],
                    gop_bs_sz[pat_idx], job_cases[gop_last_job[pat_idx]].in_mmap_va[0], 
                    gop_bs_info[pat_idx].gop_size);
                
            }
            printf("fr %d is IDR: %08x\n", i, job_cases[i].in_mmap_va[0]);
            gop_cnt[pat_idx]++;
            gop_first_job[pat_idx] = i;
            gop_last_job[pat_idx] = i;
            gop_bs_sz[pat_idx] = bs_len[i];
            gop_bs_cnt[pat_idx] = 1;

            gop_bs_info[pat_idx].bs_len[0] = bs_len[i];
            gop_bs_info[pat_idx].gop_size = 1;

            continue;
        }else{
            gop_last_job[pat_idx] = i;
            // set bitstream address of a GOP to the same address
            job_cases[i].in_mmap_va[0] = job_cases[gop_first_job[pat_idx]].in_mmap_va[0];


            // append bitstream size to input buffer 1
            if(gop_bs_info[pat_idx].gop_size >= MAX_GOP_SIZE){
                printf("gop_size too large: > %d\n", MAX_GOP_SIZE);
                return -1;
            }
            gop_bs_info[pat_idx].bs_len[gop_bs_info[pat_idx].gop_size] = bs_len[i];
            gop_bs_info[pat_idx].gop_size++;

            printf("fr %d is not an IDR: %08x\n", i, job_cases[i].in_mmap_va[0]);
        }


        /* append bitstream data */
        //dst_addr = ((unsigned char *)start) + (gop_first_job[pat_idx] * buffer_sz) + gop_bs_sz[pat_idx];
        dst_addr = (unsigned char *)job_cases[i].in_mmap_va[0] + gop_bs_sz[pat_idx];
        
        memcpy(dst_addr, src_addr, bs_len[i]);
        gop_bs_sz[pat_idx] += bs_len[i];
        gop_bs_cnt[pat_idx]++;
    }

    for(pat_idx = 0; pat_idx < MAX_FILE_NUM; pat_idx++){
        if(gop_cnt[pat_idx] != -1){
            /* end of the previous GOP */
            
            for(j = gop_first_job[pat_idx]; j <= gop_last_job[pat_idx]; j++){
                if(job_cases[i].minor == pat_idx){
                    update_property(job_cases[j].in_prop, bitstream_size_id, gop_bs_sz[pat_idx] + H264D_PADDING_SIZE);
                    update_property(job_cases[j].in_prop, direct_id, 1);

                    memcpy((unsigned char *)job_cases[j].in_mmap_va[0] + gop_bs_sz[pat_idx], 
                        &gop_bs_info[pat_idx], 
                        4 * (gop_bs_info[pat_idx].gop_size + 1));
                }
            }
            printf("[%d]gop[%d]: start job %d last: %d bs_cnt:%d bs_sz:%d va:%08x gop_size:%d-\n", 
                pat_idx, gop_cnt[pat_idx], gop_first_job[pat_idx], 
                gop_last_job[pat_idx], gop_bs_cnt[pat_idx],
                gop_bs_sz[pat_idx], job_cases[gop_last_job[pat_idx]].in_mmap_va[0], 
                gop_bs_info[pat_idx].gop_size);
        }
    }

    return ret;
}



void init_cfg(void)
{
    int i,j;
    struct video_param_t *param;
    
    for(i=0;i<TEST_NUM;i++) {
        memset(&cfg[i],0,sizeof(struct video_cfg_t));
        param=cfg[i].param;
        for(j=0;j<MAX_PROPERTYS;j++) {
            strcpy(param[j].name,"");
        }
    }
}


int read_param_line(char *line, int size, FILE *fd, unsigned long long *offset)
{
    int ret;
    int i = 0;
    while((ret=(int)fgets(line,size,fd))!=0) {
        DBG_PRINT("read_param_line: [%s]\n", line);
        if(strchr(line,(int)';'))
            continue;
        if(strchr(line,(int)'=')){
            return 0;
        }
        if(strchr(line,(int)'\n'))
            continue;
    }
    DBG_PRINT("read_param_line: error");
    return -1;
}



char filename[100];

/* get input property for each jobs from N.cfg */
int parse_cfg(int job_num)
{
    int i=1,j=0,k,len;
    FILE *fd;
    char *next_str;
    struct video_param_t *param;
    int value,type;
    char line[1024];
    unsigned long long offset;

    int bs_size_id_idx = -1;
    
    init_cfg();

    //for(i=0;i<TEST_NUM;i++) 
    for(i = 0;i < job_num; i++) 
    {
        sprintf(filename,"%d.cfg",i);
        fd=fopen(filename,"r");
        if (fd <= 0){
            printf("failed to open cfg file: %s\n", filename);
            break;
        }
        DBG_PRINT("open cfg file: %s\n", filename);
        cfg[i].enable=1;
        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            break;
        if((line[10]=='0')&&(line[11]=='x'))
            sscanf(line, "round_set=0x%x\n", &cfg[i].round_set);
        else
            sscanf(line, "round_set=%d\n", &cfg[i].round_set);
        
        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            break;            
        if((line[5]=='0')&&(line[6]=='x'))
            sscanf(line,"type=0x%x\n",&cfg[i].type);
        else
            sscanf(line,"type=%d\n",&cfg[i].type);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            break;
        if((line[7]=='0')&&(line[8]=='x'))
            sscanf(line,"engine=0x%x\n",&cfg[i].engine);
        else
            sscanf(line,"engine=%d\n",&cfg[i].engine);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            break;
        if((line[6]=='0')&&(line[7]=='x'))
            sscanf(line,"minor=0x%x\n",&cfg[i].minor);
        else
            sscanf(line,"minor=%d\n",&cfg[i].minor);
        cfg[i].pat_idx = cfg[i].minor;
        DBG_PRINT("minor:[%d]:%d\n", i, cfg[i].minor);
        DBG_PRINT("line:[%s]\n", line);
        DBG_PRINT("pat_idx:[%d]:%d\n", i, cfg[i].pat_idx);

        fflush(stdout);

        param=cfg[i].param;
        j=0;
        while(1) {
            if(fscanf(fd,"%s",param[j].name)==EOF)
                break;
            next_str=strstr(param[j].name,"=");
            *next_str=0;
            next_str++;
            if((next_str[0]=='0') && (next_str[1]=='x'))
                sscanf(next_str,"0x%x",&param[j].value);
            else
                sscanf(next_str,"%d",&param[j].value);
            param[j].type=cfg[i].type;

            if(strcmp(param[j].name, "bitstream_size") == 0){
                param[j].value += H264D_PADDING_SIZE; //padded size;
                bs_size_id_idx = j;
            }

            if(strcmp(param[j].name, "sub_yuv_ratio") == 0){
                cfg[i].sub_yuv_ratio = param[j].value;
            }
            j++;
        }

        fclose(fd);

#if REV_PLAY 
        strcpy(param[j].name,"direct");
        param[j].value = 0;
        j++;
#endif

        ioctl(em_fd, IOCTL_QUERY_ID_SET, param); //wait all done

#if 0 //dump param
        for (k = 0; k < j; k++) {
            printf("  (%d)ID:%d Name:%s Value:0x%08x\n", k, param[k].id, param[k].name, param[k].value);
        }
#endif
        fflush(stdout);
    }
    if(i==0)
        return -1;
    return 0;
}

void build_property(struct video_property_t *property,struct video_param_t *param)
{
    int i;
    for(i=0;i<MAX_PROPERTYS;i++) {
        property[i].id=param[i].id;
        property[i].value=param[i].value;
        if(param[i].id==0)
            break;
    }
}


FILE *record_fd[MAX_FILE_NUM] = {NULL};
int rec_dst_size[MAX_FILE_NUM] = {0};
int dst_size_chg_cnt[MAX_FILE_NUM] = {0};
FILE *record_fd2[MAX_FILE_NUM] = {NULL}; // for the 2nd frame in TYPE_YUV422_RATIO_FRAME
void do_record(int type, unsigned int addr_va[], int size, int size2, int fn_idx)
{
    int res_chg_flg = 0;
    if(fn_idx < 0 || fn_idx > MAX_FILE_NUM)
    {
        printf("incorrect fn_idx:%d", fn_idx);
        fflush(stdout);
        return;
    }
    //printf("%d %d %d\n", fn_idx, rec_dst_size[fn_idx], size);

    if(size == 0){
        printf("skip recording due to zero dst size\n");
        fflush(stdout);
        return;
    }
    
    if(record_fd[fn_idx]==0) {
        rec_dst_size[fn_idx] = size;
        if(type==TYPE_YUV422_2FRAMES) {
            sprintf(filename,"output_%02d.yuv", fn_idx);
        } else if(type==TYPE_YUV422_RATIO_FRAME) {
            sprintf(filename,"output_422_%02d.yuv", fn_idx);
        } else if(type==TYPE_YUV422_FRAME) {
            sprintf(filename,"output_422_%02d.yuv", fn_idx);
        } else if(type==TYPE_BITSTREAM_H264) {
            sprintf(filename,"output_%02d.yuv", fn_idx);
        } else if(type==TYPE_BITSTREAM_MPEG4) {
            sprintf(filename,"output_%02d.m4v", fn_idx);
        } else if(type==TYPE_BITSTREAM_JPEG) {
            sprintf(filename,"output_%02d.jpg", fn_idx);
        } else {
            sprintf(filename,"test_%02d.yuv", fn_idx);
        }
        record_fd[fn_idx]=fopen(filename,"wb");
        if(record_fd[fn_idx] == NULL){
            printf("failed to open file: %s\n", filename);
            fflush(stdout);
        }
    }else if(rec_dst_size[fn_idx] != size){
        printf("output resolution changed: %d -> %d\n", rec_dst_size[fn_idx], size);
        // open another file at resolution changing
        rec_dst_size[fn_idx] = size;
        dst_size_chg_cnt[fn_idx]++;
        res_chg_flg = 1;
        
        if(type==TYPE_YUV422_2FRAMES) {
            sprintf(filename,"output_%02d_%02d.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else if(type==TYPE_YUV422_RATIO_FRAME) {
            sprintf(filename,"output_422_%02d_%02d.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else if(type==TYPE_YUV422_FRAME) {
            sprintf(filename,"output_422_%02d_%02d.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else if(type==TYPE_BITSTREAM_H264) {
            sprintf(filename,"output_%02d_%02d.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else if(type==TYPE_BITSTREAM_MPEG4) {
            sprintf(filename,"output_%02d_%02d.m4v", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else if(type==TYPE_BITSTREAM_JPEG) {
            sprintf(filename,"output_%02d_%02d.jpg", fn_idx, dst_size_chg_cnt[fn_idx]);
        } else {
            sprintf(filename,"test_%02d_%02d.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
        }
        fclose(record_fd[fn_idx]);
        record_fd[fn_idx]=fopen(filename,"wb");
        if(record_fd[fn_idx] == NULL){
            printf("failed to open file: %s\n", filename);
            fflush(stdout);
        }
    }

    //printf("%s\n", filename);
    if(record_fd[fn_idx]) {

        fwrite((void *)addr_va[0], 1, size, record_fd[fn_idx]);
        //printf("Record to %s Done!\n",filename);
        //fflush(record_fd);
        DBG_PRINT("[%02d]write buffer size %d %s\n", fn_idx, size, filename);
        fflush(stdout);
        //getchar();
    }
    
    if(type==TYPE_YUV422_RATIO_FRAME) { // for storing scaled YUV, the 2nd frame
        if(record_fd2[fn_idx]==0) {
            sprintf(filename, "output_422_%02d_scaled.yuv", fn_idx);
            record_fd2[fn_idx]=fopen(filename,"wb");
            if(record_fd2[fn_idx] == NULL){
                printf("failed to open file: %s\n", filename);
                fflush(stdout);
            }
        }else if(res_chg_flg){
            sprintf(filename, "output_422_%02d_%02d_scaled.yuv", fn_idx, dst_size_chg_cnt[fn_idx]);
            fclose(record_fd2[fn_idx]);
            record_fd2[fn_idx]=fopen(filename,"wb");
            if(record_fd2[fn_idx] == NULL){
                printf("failed to open file: %s\n", filename);
                fflush(stdout);
            }
        }
        if(record_fd2[fn_idx]) {
            fwrite((void *)addr_va[1], 1, size2, record_fd2[fn_idx]);
            //printf("Record to %s Done!\n",filename);
            //fflush(record_fd);
            DBG_PRINT("[%02d]write buffer size %d\n", fn_idx, size2);
            fflush(stdout);
        }
    }

}


void print_info(void)
{
    printf("Case 1 Basic Test: Put 10 jobs + stop + record\n");
    printf("Case 2 Basic Test: Put 10 jobs + stop + Put 10 jobs + stop\n");
    printf("Case 3 Basic Burnin Test: Continue Put jobs (HW util/CPU util/ISR Lock/Perf)\n");
    printf("Case 4 Multi-jobs Test: Put 5 jobs + Put 5 jobs + stop + record\n");
    printf("Case 5 Multi-jobs Test: Put 5 jobs + stop + Put 5 jobs\n");
    printf("Case 6 Multi-jobs Burnin Test: Put 5 jobs... (HW util/CPU util/ISR Lock/Perf)\n");
    fflush(stdout);
}

struct poc_record {
    unsigned int idx;
    unsigned int chn; //channel idx or pattern idx
    unsigned int poc;
    unsigned int seq;
    unsigned int has_subyuv;
    unsigned int dst_size;
    unsigned int dst_scaled_size;
};

#ifdef RECORD_AS_POC_ORDER
struct poc_record poc_recs[TEST_NUM];
int rec_num = 0;
#endif // RECORD_AS_POC_ORDER

int push_poc_record(struct poc_record rec)
{
    if(rec_num == TEST_NUM){
        return -1;
    }
    poc_recs[rec_num++] = rec;
    return 0;
}

int print_poc_record(void)
{
    int i;
    for(i = 0; i < rec_num; i++){
        printf("[%d][c:%d] idx:%d poc:%d seq:%d\n", i,  poc_recs[i].chn, 
            poc_recs[i].idx, poc_recs[i].poc, poc_recs[i].seq);
    }
    return 0;
}

int check_poc_record_chn_seq_order(void)
{
    int i;
    int err_seq_cnt = 0;
    for(i = 0; i < rec_num; i++){
        printf("[%d][c:%d] idx:%d poc:%d seq:%d\n", i,  poc_recs[i].chn, 
            poc_recs[i].idx, poc_recs[i].poc, poc_recs[i].seq);
        if(i > 0) {
            if((poc_recs[i - 1].chn > poc_recs[i].chn) ||
               (poc_recs[i - 1].chn == poc_recs[i].chn && poc_recs[i - 1].seq >= poc_recs[i].seq)){
                printf("chn/seq order incorrect at index: %d\n", i);
                err_seq_cnt++;
            }
        }
    }
    return err_seq_cnt;
}

#if 0 //unused sorting functions
int sort_poc_record_by_chn(struct poc_record *poc_recs, int rec_num)
{
    int i, j;
    struct poc_record tmp;

    for(i = 1; i < rec_num; i++){
        for(j = i; j > 0; j--){ // find min poc rec
            if(poc_recs[j].chn >= poc_recs[j - 1].chn){ 
                continue;
            } else {
                //swap
                tmp = poc_recs[j];
                poc_recs[j] = poc_recs[j - 1];
                poc_recs[j - 1] = tmp;
            }
        }
    }
    return 0;
}


int sort_poc_record_by_poc(struct poc_record *poc_recs, int rec_num)
{
    int i, j;
    struct poc_record tmp;

    for(i = 1; i < rec_num; i++){
        for(j = i; j > 0; j--){ // find min poc rec
            if(poc_recs[j].poc >= poc_recs[j - 1].poc){ 
                continue;
            } else {
                //swap
                tmp = poc_recs[j];
                poc_recs[j] = poc_recs[j - 1];
                poc_recs[j - 1] = tmp;
            }
        }
    }
    return 0;
}

int sort_poc_record_by_seq(struct poc_record *poc_recs, int rec_num)
{
    int i, j;
    struct poc_record tmp;

    for(i = 1; i < rec_num; i++){
        for(j = i; j > 0; j--){ // find min poc rec
            if(poc_recs[j].seq >= poc_recs[j - 1].seq){ 
                continue;
            } else {
                //swap
                tmp = poc_recs[j];
                poc_recs[j] = poc_recs[j - 1];
                poc_recs[j - 1] = tmp;
            }
        }
    }
    return 0;
}
#endif

// sort the record as the order of channel, sequence
int sort_poc_record_by_chn_seq(struct poc_record *poc_recs, int rec_num)
{
    int i, j;
    struct poc_record tmp;
    
    for(i = 1; i < rec_num; i++){
        for(j = i; j > 0; j--){ // find min poc rec
            if( (poc_recs[j].chn > poc_recs[j - 1].chn) ||
                (poc_recs[j].chn == poc_recs[j - 1].chn &&
                 poc_recs[j].seq >= poc_recs[j - 1].seq)){ 
                continue;
            } else {
                //swap
                tmp = poc_recs[j];
                poc_recs[j] = poc_recs[j - 1];
                poc_recs[j - 1] = tmp;
            }
        }
    }
}




int main(int argc,char *argv[])
{
    int i,j,k,ret;
    unsigned int total_sz=0;
    FILE *fd;
    frmmap_meminfo_t mem_info;
    struct test_case_t test_cases;
    //static struct test_case_t test_cases; // static to avoid stack overflow
    struct job_case_t *job_cases;
    unsigned int in_user_va = 0, out_user_va = 0;
	void *mmap_addr;
    unsigned char mmap_err_flg = 0;
    int test_idx=1,burnin_seconds=0;
    char line[1024];
    unsigned long long offset;
    unsigned int rec_en = 0;
    unsigned int valid_job_num = 0;
    char fn[MAX_FILE_NUM][MAX_FILENAME_LEN] = {};
    unsigned int dst_size[MAX_FILE_NUM] = {0};
    unsigned int dst_scale[MAX_FILE_NUM] = {0};
    unsigned int bs_len[MAX_JOBS] = {0};
    int fn_num = 0;
    int fr_num = TEST_NUM;
    int test_retval = -1;
    //int ib_ddr = -1;
    int ib_ddr = 0;
    //int ob_ddr = -1;
    int ob_ddr = 0;
    int max_chn_num;
    int rev_mode = 0;
    int verbose_flg = 1;

    for(i = 0; i < MAX_FILE_NUM; i++){
        dst_scale[i] = 1;
    }
    
    if(argc < 2) {
        printf("Usage: %s [case index] [seconds] [-rec] [-p pat_name...] [-f frame_num] [-ib ddr] [-ob ddr] [-rev] [-v]\n",argv[0]);
        print_info();
        return -1;
    }
    test_idx = atoi(argv[1]);
    printf("Test Case %d startup\n",test_idx);
    
    if(argc > 2)
        burnin_seconds=atoi(argv[2]);


    // parse argv
    for(i = 1; i < argc; i++){
        if(strcmp(argv[i], "-rec") == 0){
            rec_en = 1;
        }else if(strcmp(argv[i], "-rev") == 0){
            rev_mode = 1;
        }else if(strcmp(argv[i], "-v") == 0){
            verbose_flg = 1;
        }else if(strcmp(argv[i], "-p") == 0){
            ++i;
            if(i < argc){
                strncpy(fn[fn_num], argv[i], sizeof(fn));
                fn_num++;
            }
        }else if(strcmp(argv[i], "-ib") == 0){
            ++i;
            if(i < argc){
                ib_ddr = atoi(argv[i]);
            }
        }else if(strcmp(argv[i], "-ob") == 0){
            ++i;
            if(i < argc){
                ob_ddr = atoi(argv[i]);
            }
        }else if(strcmp(argv[i], "-f") == 0){
            ++i;
            if(i < argc){
                fr_num = atoi(argv[i]);
                if(fr_num > TEST_NUM){
                    printf("frame num exceed TEST_NUM(%d)\n", TEST_NUM);
                    return -1;
                }
            }
        }
        
    }

    if(fn_num == 0){
        printf("no patterns are specified\n");
        return -1;
    }

    #if 1
    if(verbose_flg){
        printf("== options ==\n");
        printf("H264D_PADDING_SIZE:%d\n", H264D_PADDING_SIZE);
        printf("test idx: %d\n", test_idx);
        printf("burnin_seconds: %d\n", burnin_seconds);
        printf("recording output to file: %s\n", rec_en?"enabled":"disabled");
        printf("frame num: %d\n", fr_num);
        printf("file num: %d\n", fn_num);
        for(i = 0; i < fn_num; i++){
            printf("pattern[%d]: %s\n", i, fn[i]);
        }
        printf("rev_mode:%d\n", rev_mode);
        printf("==============\n");
        fflush(stdout);
    }
    #endif
    
    em_fd=open("/dev/em",O_RDWR);
    if(em_fd==0) {
        perror("open /dev/em failed");
        return -1;
    }
    mem_fd[0] = open("/dev/frammap0", O_RDWR);
    if(mem_fd[0] == 0) {
        perror("open /dev/frammap0 failed");
        return -1;
    }
    ioctl(mem_fd[0],FRM_IOGBUFINFO, &mem_info);
    printf("Available memory size is %d bytes.\n", mem_info.aval_bytes);
    
    mem_fd[1] = open("/dev/frammap1", O_RDWR);
    if(mem_fd == 0) {
        perror("open /dev/frammap1 failed");
        return -1;
    }
    ioctl(mem_fd[1],FRM_IOGBUFINFO, &mem_info);
    printf("Available memory size is %d bytes.\n", mem_info.aval_bytes);

    /*
     * get parameters from buf.cfg
     */
    fd=fopen("buf.cfg","r");
    if(fd) {
        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[10]=='0')&&(line[11]=='x'))
            sscanf(line,"in_buf_ddr=0x%x\n",&in_buf_ddr);
        else
            sscanf(line,"in_buf_ddr=%d\n",&in_buf_ddr);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[12]=='0')&&(line[13]=='x'))
            sscanf(line,"in_buf_type=0x%x\n",&in_buf_type);
        else
            sscanf(line,"in_buf_type=0x%x\n",&in_buf_type);
            
        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[18]=='0')&&(line[19]=='x'))
            sscanf(line,"max_in_frame_size=0x%x\n",&in_size);
        else
            sscanf(line,"max_in_frame_size=%d\n",&in_size);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[12]=='0')&&(line[13]=='x'))
            sscanf(line,"out_buf_ddr=0x%x\n",&out_buf_ddr);
        else
            sscanf(line,"out_buf_ddr=%d\n",&out_buf_ddr);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[13]=='0')&&(line[14]=='x'))
            sscanf(line,"out_buf_type=0x%x\n",&out_buf_type);
        else
            sscanf(line,"out_buf_type=0x%x\n",&out_buf_type);

        if (read_param_line(line, sizeof(line), fd, &offset) < 0)
            return 0;
        if((line[19]=='0')&&(line[20]=='x'))
            sscanf(line,"max_out_frame_size=0x%x\n",&out_size);
        else
            sscanf(line,"max_out_frame_size=%d\n",&out_size);
            
        printf("Prepare to test with buffer in size 0x%x out size 0x%x\n",in_size,out_size);fflush(stdout);
        fclose(fd);
    } else {
        perror("Error open buf.cfg\n");
        close(em_fd);
        close(mem_fd);
        return -1;
    }


    /*
     * get input/output buffer address from mmap
     */
    if(in_buf_type==TYPE_YUV422_2FRAMES)
        in_size=in_size*2;
    if(out_buf_type == TYPE_YUV422_2FRAMES || out_buf_type == TYPE_YUV422_RATIO_FRAME)
        out_size=out_size*2;

    if(ib_ddr != -1){
        in_buf_ddr = ib_ddr;
    }
    if(ob_ddr != -1){
        out_buf_ddr = ob_ddr;
    }

    if(in_buf_ddr!=out_buf_ddr) {
        if (in_size) {
            mmap_addr = mmap(0, in_size * fr_num, PROT_READ | PROT_WRITE, MAP_SHARED, 
                mem_fd[in_buf_ddr], 0);

            if (mmap_addr == MAP_FAILED) {
                mmap_err_flg = 1;
                perror("Error to mmap in_user_va\n");
                return -1;
            }
            in_user_va = (unsigned int)mmap_addr;
        }
        if (out_size) {
            mmap_addr = mmap(0, out_size * fr_num, PROT_READ | PROT_WRITE, MAP_SHARED, 
                mem_fd[out_buf_ddr], 0);
            if (mmap_addr == MAP_FAILED) {
                mmap_err_flg = 1;
                perror("Error to mmap out_user_va\n");
                return -1;
            }
            out_user_va = (unsigned int)mmap_addr;
        }
    } else {
        total_sz=(in_size+out_size)*fr_num;
        mmap_addr = mmap(0, total_sz,PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd[in_buf_ddr], 0);

#if 1
        // for checking mmap errors. can be removed latter
        printf("in_size: %d\n", in_size);
        printf("out_size: %d\n", out_size);
        printf("TEST_NUM: %d\n", TEST_NUM);
        printf("fr_num: %d\n", fr_num);
        printf("total_sz: %d\n", total_sz);
        printf("in_buf_ddr: %d\n", in_buf_ddr);
        printf("mem_fd[in_buf_ddr]: %d\n", mem_fd[in_buf_ddr]);
        printf("mmap_addr: 0x%08X\n", mmap_addr);
#endif
		
        if (mmap_addr == MAP_FAILED) {
            mmap_err_flg = 1;
            perror("Error to mmap in_user_va/out_user_va\n");
            return -1;
        }
        in_user_va = (unsigned int)mmap_addr;
        out_user_va=in_user_va+(in_size*fr_num);
    }

    printf("MMAP in_va=0x%x/0x%x out_va=0x%x/0x%x\n",
        in_user_va,(in_size*fr_num),out_user_va,(out_size*fr_num));fflush(stdout);
    memset((void *)in_user_va,0,in_size*fr_num);
    memset((void *)out_user_va,0,out_size*fr_num);

    if(mmap_err_flg) {
        perror("Error mmap");
        return -1;
    }

#if 0
    if (prepare_input_buffer(in_user_va, fr_num, in_size, fn, fn_num) < 0) {
        perror("Error to prepare input buffer");
        return -1;
    }
#endif

    /* get input property for each jobs */
    if (parse_cfg(fr_num) < 0) {
        perror("Error to parsing config");
        return -1;
    }



    memset(&test_cases,0,sizeof(test_cases));
    test_cases.case_item=test_idx;
    test_cases.burnin_secs=burnin_seconds;
    test_cases.type=cfg[0].type;
    job_cases=test_cases.job_cases;

#if 0//REV_PLAY==0 
    k=0;
    for(i=0;i<fr_num;i++) {
        if(cfg[k].enable==0) {
            break;
        }
        job_cases[i].enable=1;
        job_cases[i].engine=cfg[k].engine;
        job_cases[i].minor=cfg[k].minor;
        job_cases[i].in_buf_type=in_buf_type;
        job_cases[i].out_buf_type=out_buf_type;

        if(in_buf_type==TYPE_YUV422_2FRAMES) {
            job_cases[i].in_mmap_va[0]=in_user_va+(i*(in_size/2));
            job_cases[i].in_mmap_va[1]=in_user_va+(i*in_size);
        } else
            job_cases[i].in_mmap_va[0]=in_user_va+(i*in_size);

        if(out_buf_type==TYPE_YUV422_RATIO_FRAME || out_buf_type==TYPE_YUV422_2FRAMES) {
            //job_cases[i].out_mmap_va[0]=out_user_va+(i*(out_size/2));
            //job_cases[i].out_mmap_va[1]=out_user_va+(i*out_size);
            job_cases[i].out_mmap_va[0]=out_user_va+(i*out_size);
            job_cases[i].out_mmap_va[1]=out_user_va+(i*out_size + out_size/2);
            dst_scale[job_cases[i].minor] = cfg[k].sub_yuv_ratio;
            //printf("[%d]out_va[0]:0x%08X [1]:0x%08X\n", i, 
            //    job_cases[i].out_mmap_va[0], job_cases[i].out_mmap_va[1]);
        } else
            job_cases[i].out_mmap_va[0]=out_user_va+(i*out_size);

        build_property(job_cases[i].in_prop,cfg[k].param);
        cfg[k].round_set--;
        if(cfg[k].round_set==0) {
            k++;
        }
    }
#else
    for(i=0;i<fr_num;i++) {
        if(cfg[i].enable==0) { // this is required
            break;
        }
        job_cases[i].enable=1;
        job_cases[i].engine=cfg[i].engine;
        job_cases[i].minor=cfg[i].minor;
        job_cases[i].in_buf_type=in_buf_type;
        job_cases[i].out_buf_type=out_buf_type;

        if(in_buf_type==TYPE_YUV422_2FRAMES) 
        {
            job_cases[i].in_mmap_va[0]=in_user_va+(i*(in_size/2));
            job_cases[i].in_mmap_va[1]=in_user_va+(i*in_size);
        } else {
            job_cases[i].in_mmap_va[0]=in_user_va+(i*in_size);
        }

        if(out_buf_type==TYPE_YUV422_RATIO_FRAME || out_buf_type==TYPE_YUV422_2FRAMES) {
            //job_cases[i].out_mmap_va[0]=out_user_va+(i*(out_size/2));
            //job_cases[i].out_mmap_va[1]=out_user_va+(i*out_size);
            job_cases[i].out_mmap_va[0]=out_user_va+(i*out_size);
            job_cases[i].out_mmap_va[1]=out_user_va+(i*out_size + out_size/2);
            dst_scale[job_cases[i].minor] = cfg[i].sub_yuv_ratio;
            //printf("[%d]out_va[0]:0x%08X [1]:0x%08X\n", i, 
            //    job_cases[i].out_mmap_va[0], job_cases[i].out_mmap_va[1]);
        } else
            job_cases[i].out_mmap_va[0]=out_user_va+(i*out_size);

        build_property(job_cases[i].in_prop, cfg[i].param);
    }
#endif

    valid_job_num = i;

    /* prepare input buffer */
    max_chn_num = prepare_input_buffer_by_job_cases(job_cases, in_user_va, valid_job_num, in_size, fn, bs_len, fn_num);
    if (max_chn_num < 0) {
        perror("Error to prepare input buffer");
        return -1;
    }


    printf("valid_job_num:%d\n", valid_job_num);

#if 1
    if(rev_mode){
        group_input_buffer_of_GOP(job_cases, in_user_va, valid_job_num, in_size, bs_len, fn_num);
        revert_job_cases(job_cases, valid_job_num);
    }
#endif

    printf("Press ENTER to start...\n");getchar();
    test_retval = ioctl(em_fd, IOCTL_TEST_CASES, &test_cases);
    if(test_retval < 0) {
        printf("AP IOCTL_TEST_CASES failed!\n");
    }

    DBG_PRINT("IOCTL_TEST_CASES return: %d\n", ret);

    /* result */
    if((test_idx==1)||(test_idx==4)) {
        for(i = 0; i < valid_job_num; i++) {
            struct video_param_t param;
            int ch;
            ch = job_cases[i].minor;
#ifdef RECORD_AS_POC_ORDER
            struct poc_record poc_rec;
            int poc_found_flg;
            poc_rec.idx = i;
            poc_rec.chn = ch;
            poc_rec.poc = -1;
            poc_rec.seq = -1;
#endif
    
            printf("[%d]Job %d Status %d\n", i, job_cases[i].id, job_cases[i].status);
            fflush(stdout);
            /* get output property of a job */
            for(j=0;j<MAX_PROPERTYS;j++) {
                if(job_cases[i].out_prop[j].id==0)
                    break;
                param.id=job_cases[i].out_prop[j].id;
                param.type=test_cases.type;
                param.value=job_cases[i].out_prop[j].value;
                
                ret = ioctl(em_fd, IOCTL_QUERY_STR, &param); //wait all done
                if(ret != 0){
                    printf("IOCTL_QUERY_STR error ret:%d\n", ret);
                }
                
                printf("    %s: 0x%x\n",param.name,param.value);

                // store output property value
                if(strcmp(param.name,"dst_bg_dim")==0){
                    int width;
                    int height;
                    width = param.value >> 16;
                    height = param.value & 0xFFFF;
                    poc_rec.dst_size = dst_size[ch] = width * height * 2;
                    printf("%d:%d w/h/size: %d %d %d %d\n", i, ch, width, height, dst_size[ch], out_size);
                }
                if(strcmp(param.name,"sub_yuv")==0){
                    poc_rec.has_subyuv = param.value;
                }
#ifdef RECORD_AS_POC_ORDER
                if(strcmp(param.name,"poc")==0){
                    poc_rec.poc = param.value;
                    poc_found_flg = 1;
                }
                if(strcmp(param.name,"seq")==0){
                    poc_rec.seq = param.value;
                    poc_found_flg = 1;
                }
#endif //RECORD_AS_POC_ORDER
            }
#ifdef RECORD_AS_POC_ORDER
            if(poc_found_flg){
                push_poc_record(poc_rec);
            }
#endif //RECORD_AS_POC_ORDER            
            fflush(stdout);
            #if 0
            if(rec_en) {
                do_record(out_buf_type,job_cases[i].out_mmap_va, out_size);
            }
            #endif
        }

        if(test_retval < 0){
            printf("### ERROR: IOCTL_TEST_CASES error, skip recording output\n");
        } else if(rec_en) {
#ifdef RECORD_AS_POC_ORDER
            /* record output as the SEQ(POC) order */
            int err_seq_cnt;
            if(rec_num != valid_job_num){
                printf("### ERROR: some jobs has no poc output, can not record as poc order\n");
            }else{
                struct poc_record min_poc_rec;
                printf("rec_num: %d valid_job_num:%d\n", rec_num, valid_job_num);
                printf("bf sorting:\n");
                print_poc_record();
                
                printf("sorting:\n");
                sort_poc_record_by_chn_seq(poc_recs, rec_num);
                
                printf("af sorting:\n");
                print_poc_record();
                err_seq_cnt = check_poc_record_chn_seq_order();
                
                for(i = 0; i < valid_job_num; i++) {
                    int job_idx = poc_recs[i].idx;
                    int ch = poc_recs[i].chn;
                    int scaled_size = 0;
                    if(poc_recs[i].has_subyuv) {
                        scaled_size = dst_size[ch] / (dst_scale[ch] * dst_scale[ch]);
                        poc_recs[i].dst_scaled_size = scaled_size;
                    }
                    printf("[%d][c:%d]outputing idx:%d poc:%d dst_sz: %d scal:%d type:%d\n", i, poc_recs[i].chn, 
                        poc_recs[i].idx, poc_recs[i].poc, poc_recs[i].dst_size, poc_recs[i].dst_scaled_size, out_buf_type);
//                    poc_recs[i].idx, poc_recs[i].poc, dst_size[ch], scaled_size);
//                    do_record(out_buf_type, job_cases[job_idx].out_mmap_va, dst_size[ch], scaled_size, poc_recs[i].chn);
                    do_record(out_buf_type, job_cases[job_idx].out_mmap_va, poc_recs[i].dst_size, poc_recs[i].dst_scaled_size, poc_recs[i].chn);
                }
            }
            if(err_seq_cnt){
                printf("### ERROR: incorrect seq numbers:%d\n", err_seq_cnt);
            }
#else // !RECORD_AS_POC_ORDER
            /* record output as the original job (decoding) order */
            for(i = 0; i < valid_job_num; i++) {
                    do_record(out_buf_type,job_cases[i].out_mmap_va, out_size, out_size, i % fn_num);
            }
#endif // !RECORD_AS_POC_ORDER
        }
    }

    printf("\n###### Test Done ######\n");
    for(i = 0; i < fn_num; i++){
        if(record_fd[i]) {
            printf("\n###### Wait output for File %s writing, please waiting... ######\n", fn[i]);
            fclose(record_fd[i]);
            printf("\n###### Wait output for File %s done... ######\n", fn[i]);
        }
    }
    fflush(stdout);
    munmap((void *)in_user_va,total_sz);
    close(em_fd);
    close(mem_fd[0]);
    close(mem_fd[1]);
}


