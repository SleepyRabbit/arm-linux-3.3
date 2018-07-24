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
#include <linux/string.h>
#include <linux/seq_file.h>

#include "log.h"
#ifdef SUPPORT_VG2
#include "file_parser.h"
#endif
#include "frammap_if.h"
#include "mem_pool.h"
#include "favc_enc_vg.h"
#include "favc_enc_entity.h"
#include "h264e_ioctl.h"

extern int log_level;

#if 0
#define dmp_ref(fmt, args...) { \
    if (str)    \
        len += sprintf(str+len, fmt "\n", ## args);  \
    else \
        printk(fmt "\n", ## args); }
#else
#define dmp_ref(fmt, args...) { \
    if (str)    \
        len += sprintf(str+len, fmt "\n", ## args);  \
    else \
        printk(fmt "\n", ## args); }
        //printm(MODULE_NAME, fmt "\n", ## args); }
#endif

static const struct res_base_info_t res_base_info[MAX_RESOLUTION] = {
    { RES_8M,    "8M",    ALIGN16_UP(4096), ALIGN16_UP(2160)},  // 4096 x 2160  : 8847360
#ifdef NEW_RESOLUTION
    { RES_7M,    "7M",    ALIGN16_UP(3264), ALIGN16_UP(2176)},  // 2364 x 2176  : 7102464    
    { RES_6M,    "6M",    ALIGN16_UP(3072), ALIGN16_UP(2048)},  // 3072 x 2048  : 6291456
#endif
    { RES_5M,    "5M",    ALIGN16_UP(2592), ALIGN16_UP(1944)},  // 2596 x 1952  : 5067392
    //{ RES_4M,    "4M",    ALIGN16_UP(2304), ALIGN16_UP(1728)},  // 2304 x 1728  : 3981312
    { RES_4M,    "4M",    ALIGN16_UP(2688), ALIGN16_UP(1520)},  // 2688 x 1520  : 4085760
    { RES_3M,    "3M",    ALIGN16_UP(2048), ALIGN16_UP(1536)},  // 2048 x 1536  : 3145728
    { RES_2M,    "2M",    ALIGN16_UP(1920), ALIGN16_UP(1080)},  // 1920 x 1088  : 2088960
    { RES_1080P, "1080P", ALIGN16_UP(1920), ALIGN16_UP(1080)},  // 1920 x 1088  : 2088960
    { RES_1_3M,  "1.3M",  ALIGN16_UP(1280), ALIGN16_UP(1024)},  // 1280 x 1024  : 1310720
    { RES_1_2M,  "1.2M",  ALIGN16_UP(1280), ALIGN16_UP(960)},   // 1280 x 960   : 1228800
    { RES_1080I, "1080I", ALIGN16_UP(1920), ALIGN16_UP(540)},   // 1920 x 544   : 1044480
    { RES_1M,    "1M",    ALIGN16_UP(1280), ALIGN16_UP(800)},   // 1280 x 800   : 1024000
    { RES_720P,  "720P",  ALIGN16_UP(1280), ALIGN16_UP(720)},   // 1280 x 720   : 921600
    { RES_960H,  "960H",  ALIGN16_UP(960),  ALIGN16_UP(576)},   // 960  x 576   : 552960
    { RES_720I,  "720I",  ALIGN16_UP(1280), ALIGN16_UP(360)},   // 1280 x 368   : 471040
    { RES_D1,    "D1",    ALIGN16_UP(720),  ALIGN16_UP(576)},   // 720  x 576   : 414720
    { RES_VGA,   "VGA",   ALIGN16_UP(640),  ALIGN16_UP(480)},   // 640  x 480   : 307200
    { RES_HD1,   "HD1",   ALIGN16_UP(720),  ALIGN16_UP(360)},   // 720  x 368   : 264960
    { RES_nHD,   "nHD",   ALIGN16_UP(640),  ALIGN16_UP(360)},   // 640  x 368   : 235520
    { RES_2CIF,  "2CIF",  ALIGN16_UP(360),  ALIGN16_UP(596)},   // 368  x 596   : 219328
#ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
    { RES_CIF,   "CIF",   ALIGN16_UP(384),  ALIGN16_UP(288)},   // 384  x 288   : 110592
#else
    { RES_CIF,   "CIF",   ALIGN16_UP(360),  ALIGN16_UP(288)},   // 368  x 288   : 105984
#endif
    { RES_QCIF,  "QCIF",  ALIGN16_UP(180),  ALIGN16_UP(144)}    // 192  x 144   : 27648
};
static const char rc_mode_name[5][5] = {"", "CBR", "VBR", "ECBR", "EVBR"};

static spinlock_t favce_ref_lock;
static int max_ref_buf_num = 0;
static struct ref_item_t *ref_item = NULL;
static struct layout_info_t layout_info[MAX_LAYOUT_NUM];
static struct ref_pool_t ref_pool[MAX_RESOLUTION];
//static struct channel_ref_buffer_t chn_pool[MAX_CHN_NUM];
static struct channel_ref_buffer_t *chn_pool = NULL;
#ifdef DIFF_REF_DDR_MAPPING
    static struct buffer_info_t favce_ref_buffer[2] = {{0, 0, 0}, {0, 0, 0}};
#else
    static struct buffer_info_t favce_ref_buffer = {0, 0, 0};
#endif
static int cur_layout = -1;

extern unsigned int h264e_max_width;
extern unsigned int h264e_max_height;
extern unsigned int h264e_max_b_frame;
extern unsigned int h264e_max_chn;
extern unsigned int h264e_snapshot_chn;
extern char *config_path;
extern struct favce_data_t *private_data;
#ifdef SAME_REC_REF_BUFFER
    extern unsigned int h264e_one_ref_buf;
#endif
#ifdef TIGHT_BUFFER_ALLOCATION
    extern unsigned int h264e_tight_buf;
#endif

static void mem_error(void)
{
    dump_ref_pool(NULL);
    dump_chn_pool(NULL);
    damnit(MODULE_NAME);
}

static int getline(char *line, int size, struct file *fd, unsigned long long *offset)
{
    char ch;
    int lineSize = 0, ret;

    memset(line, 0, size);
    while ((ret = (int)vfs_read(fd, &ch, 1, offset)) == 1) {
        if (ch == 0xD)
            continue;
        if (lineSize >= MAX_LINE_CHAR) {
            printk("Line buf is not enough %d! (%s)\n", MAX_LINE_CHAR, line);
            break;
        }
        line[lineSize++] = ch;
        if (ch == 0xA)
            break;
    }
    return lineSize;
}

static int readline(struct file *fd, int size, char *line, unsigned long long *offset)
{
    int ret = 0;
    do {
        ret = getline(line, size, fd, offset);
    } while (((line[0] == 0xa)) && (ret != 0));
    return ret;
}

static int str_cmp(char *str0, char *str1, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (str0[i] != str1[i])
            return -1;
    }
    return 0;
}

#ifdef USE_GMLIB_CFG
static int get_res_idx(char *res_name)
{
    int i;
    for (i = 0; i < MAX_RESOLUTION; i++) {
        if (0 == str_cmp(res_name, (char *)res_base_info[i].name, strlen(res_base_info[i].name)))
            return i;
    }
    return -1;
}

static int get_config_res_name(char *name, char *line, int offset, int size)
{
    int i = 0;
    while (',' == line[offset] || ' ' == line[offset]) {
    	if (offset >= size)
    		return -1;
    	offset++;
    }
    while ('/' != line[offset]) {
    	name[i] = line[offset];
    	i++;
    	offset++;
    }
    name[i] = '\0';
    return offset;
}

#ifdef NEW_GMLIB_CFG
int parse_layout_gmlib_cfg(void)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int idx, l = 0;
    int i, line_size;
    char res_name[10];
    int ret = 0;
    int chn_cnt;
    struct ref_pool_t chn_info;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, GMLIB_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd))
        return CFG_NOT_EXIST;
    while (readline(fd, sizeof(line), line, &offset)) {
        if (strstr(line, "[ENCODE_STREAMS]")) {
            while (readline(fd, sizeof(line), line, &offset)) {
                if ('\n' == line[0] || '\0' == line[0] || '[' == line[0]) {
                    break;
                }
                line_size = strlen(line);
                if (';' == line[0])
                    continue;
                i = 0;
                while ('=' != line[i])  i++;
                i++;
                while (' ' == line[i])  i++;

                chn_cnt = 0;
                while (1) {
                    i = get_config_res_name(res_name, line, i, line_size);
                    if (i >= line_size)
                        break;
                    sscanf(&line[i], "/%d/%d/%d", &chn_info.max_chn, &chn_info.max_fps, &chn_info.ddr_idx);
                    idx = get_res_idx(res_name);
                    if (idx >= 0 && (0 == chn_info.ddr_idx || 1 == chn_info.ddr_idx)) {
                        layout_info[l].ref_pool[idx].max_chn = chn_info.max_chn;
                        layout_info[l].ref_pool[idx].max_fps = chn_info.max_fps;
                        layout_info[l].ref_pool[idx].ddr_idx = chn_info.ddr_idx;
                        chn_cnt += chn_info.max_chn;
                    }
                    else {
                        //favce_err("wrong input: res name %s, ddr_idx %d\n", res_name, chn_info.ddr_idx);
                        favce_err("wrong input: %s", line);
                        ret = LAYOUT_PARAM_ERROR;
                        break;
                    }
                    while (',' != line[i] && i < line_size)  i++;
                    if (i == line_size)
                        break;

                }
                layout_info[l].max_channel = chn_cnt;
                l++;
            }
            break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);
    if (ret < 0)
        return ret;
    return l;
}

int parse_ddr_idx_gmlib_cfg(void)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    //int idx;
    int i, line_size;
    char res_name[10];
    int ddr_idx = -1;
    //int checker = 0;
    struct ref_pool_t chn_info;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, GMLIB_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd))
        return CFG_NOT_EXIST;
    while (readline(fd, sizeof(line), line, &offset)) {
        if (strstr(line, "[ENCODE_STREAMS]")) {
            while (readline(fd, sizeof(line), line, &offset)) {
                if ('\n' == line[0] || '\0' == line[0] || '[' == line[0]) {
                    break;
                }
                line_size = strlen(line);
                if (';' == line[0])
                    continue;
                i = 0;
                while ('=' != line[i])  i++;
                i++;
                while (' ' == line[i])  i++;

                i = get_config_res_name(res_name, line, i, line_size);
                if (i >= line_size)
                    break;
                sscanf(&line[i], "/%d/%d/%d", &chn_info.max_chn, &chn_info.max_fps, &ddr_idx);
                break;

            }
            break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);
    return ddr_idx;
}

int parse_compress_gmlib_cfg(void)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int i;
    int compress_rate = -1;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, GMLIB_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd))
        return CFG_NOT_EXIST;
    while (readline(fd, sizeof(line), line, &offset)) {
        if (strstr(line, "min_compressed_ratio")) {
            i = 0;
            while ('=' != line[i])  i++;
            i++;
            sscanf(&line[i], "%d", &compress_rate);
            break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);
    if (compress_rate < 0) {
        favce_err("min_compress_ratio is not exist\n");
        return PARAM_NOT_EXIST;
    }
    return compress_rate;
}

#else   // NEW_GMLIB_CFG
int parse_gmlib_cfg(int *ddr_idx, int *compress_rate, int parse_layout)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int idx, l = 0;
    int i, line_size;
    char res_name[10];
    int ret = 0;
    int checker = 0;
    struct ref_pool_t chn_info;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, GMLIB_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        //favce_err("open %s error\n", filename);
        //damnit(MODULE_NAME);
        return -1;
    }
    if (parse_layout)
        checker |= 0x03;
    if (ddr_idx)
        checker |= 0x01;
    if (compress_rate)
        checker |= 0x02;
    while (readline(fd, sizeof(line), line, &offset)) {
        if (1 == parse_layout || NULL != ddr_idx) {
            if (strstr(line, "[ENCODE_STREAMS]")) {
                while (readline(fd, sizeof(line), line, &offset)) {
                    if ('\n' == line[0] || '\0' == line[0] || '[' == line[0]) {
                        break;
                    }
                    line_size = strlen(line);
                    if (';' == line[0])
                        continue;
                    i = 0;
                    while ('=' != line[i])  i++;
                    i++;
                    while (' ' == line[i])  i++;
                    if (parse_layout) {
                        int chn_cnt = 0;
                        while (1) {
                            i = get_config_res_name(res_name, line, i, line_size);
                            if (i >= line_size)
                                break;
                            sscanf(&line[i], "/%d/%d/%d", &chn_info.max_chn, &chn_info.max_fps, &chn_info.ddr_idx);
                            idx = get_res_idx(res_name);
                            if (idx >= 0 && (0 == chn_info.ddr_idx || 1 == chn_info.ddr_idx)) {
                                layout_info[l].ref_pool[idx].max_chn = chn_info.max_chn;
                                layout_info[l].ref_pool[idx].max_fps = chn_info.max_fps;
                                layout_info[l].ref_pool[idx].ddr_idx = chn_info.ddr_idx;
                                chn_cnt += chn_info.max_chn;
                            }
                            else {
                                favce_err("wrong input: res name %s, ddr_idx %d\n", res_name, chn_info.ddr_idx);
                            }
                            while (',' != line[i] && i < line_size)  i++;
                            if (i == line_size)
                                break;
                        }
                        layout_info[l].max_channel = chn_cnt;
                        l++;
                    }
                    else if (ddr_idx) {
                        i = get_config_res_name(res_name, line, i, line_size);
                        if (i >= line_size)
                            break;
                        sscanf(&line[i], "/%d/%d/%d", &chn_info.max_chn, &chn_info.max_fps, ddr_idx);
                        ret |= 0x01;
                        break;
                    }
                }
                if (parse_layout)
                    break;
                if (ret == checker)
                    break;
            }
        }
        if (compress_rate && strstr(line, "min_compressed_ratio")) {
            i = 0;
            while ('=' != line[i])  i++;
            i++;
            sscanf(&line[i], "%d", compress_rate);
            ret |= 0x02;
            if (ret == checker)
                break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);
    return l;
}
#endif  // NEW_GMLIB_CFG
#endif

static char *str_tok = NULL;
static char *find_blank_token(char *str)
{
    if (NULL == str) {
        while (*str_tok != '\0' && *str_tok != '\n') {
            if (*str_tok == ' ') 
                break;
            str_tok++;
        }
    }
    else
        str_tok = str;
    if (NULL == str_tok)
        return str_tok;
    while (*str_tok != '\0' && *str_tok != '\n') {
        if (*str_tok == ' ') {
            str_tok++;
            continue;
        }
        return str_tok;
    }
    return NULL;
}

int parse_spec_cfg(void)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    char *ptr;
    int res_idx_order[MAX_RESOLUTION];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int i, idx = 0, res_idx, l = 0;
    int res_num = 0;
    int ret;
#ifdef USE_GMLIB_CFG
    #ifdef NEW_GMLIB_CFG
    if ((l = parse_layout_gmlib_cfg()) > 0)
        return l;
    if (CFG_NOT_EXIST != l) {
        favce_err("parsing layout from gmlib.cfg error\n");
        return l;
    }
    #else
    if ((l = parse_gmlib_cfg(NULL, NULL, 1)) > 0) {
        return l;
    }
    #endif
    l = 0;
    memset(layout_info, 0, sizeof(struct layout_info_t) * MAX_LAYOUT_NUM);
#endif
    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, SPEC_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        favce_err("open %s error\n", filename);
        damnit(MODULE_NAME);
        return -1;
    }
    for (i = 0; i < MAX_RESOLUTION; i++)
        res_idx_order[i] = -1;

    while ((ret = readline(fd, sizeof(line), line, &offset))) {
        if (strstr(line, "[ENC STREAMS]")) {
            if (0 == readline(fd, sizeof(line), line, &offset))    // resolution type
                break;
            // read resolution idx
            ptr = find_blank_token(line);
            while (ptr) {
                // find resolution idx
                for (i = 0; i < MAX_RESOLUTION; i++) {
                    if (str_cmp(ptr, (char *)res_base_info[i].name, (int)strlen(res_base_info[i].name)) == 0) {
                        res_idx_order[idx] = i;//res_base_info[i].index;
                        idx++;
                        break;
                    }
                }
                ptr = find_blank_token(NULL);
            }
            res_num = idx;
    
            while (readline(fd, sizeof(line), line, &offset)) {
                if (strstr(line, "CH_")) {
                    sscanf(line, "CH_%d", &layout_info[l].max_channel);
                    idx = 0;
                    ptr = find_blank_token(line);
                    ptr = find_blank_token(NULL);
                    while (ptr) {
                        res_idx = res_idx_order[idx];
                        if (res_idx < 0)
                            break;
                        sscanf(ptr, "%d/%d", &layout_info[l].ref_pool[res_idx].max_chn, 
                            &layout_info[l].ref_pool[res_idx].max_fps);
                        ptr = find_blank_token(NULL);
                        idx++;
                    }
                    l++;
                }
                else
                    break;
            }
            break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);

    return l;
}

int parse_param_cfg(int *ddr_idx, int *compress_rate)
{
    char line[MAX_LINE_CHAR];
    char filename[0x80];
    mm_segment_t fs;
    struct file *fd = NULL;
    unsigned long long offset = 0;
    int ret = 0;
    int checker = 0;

#ifdef USE_GMLIB_CFG
    #ifdef NEW_GMLIB_CFG
    int value;
    if (ddr_idx) {
        value = parse_ddr_idx_gmlib_cfg();
        if (value >= 0)
            *ddr_idx = value;
        else if (CFG_NOT_EXIST != value) {
            favce_err("parsing ddr_idx from gmlib error\n");
            return value;
        }
    }
    if (compress_rate) {
        value = parse_compress_gmlib_cfg();
        if (value > 0)
            *compress_rate = value;
        else if (CFG_NOT_EXIST != value) {
            favce_err("parsing min_compress_ratio from gmlib error\n");
            return value;
        }
    }
    #else
    if (parse_gmlib_cfg(ddr_idx, compress_rate, 0) >= 0)
        return 1;
    #endif
#endif
    if (ddr_idx)
        checker |= 0x01;
    if (compress_rate)
        checker |= 0x02;

    fs = get_fs();
    set_fs(get_ds());
    sprintf(filename, "%s/%s", config_path, PARAM_FILENAME);
    fd = filp_open(filename, O_RDONLY, S_IRWXU);
    if (IS_ERR(fd)) {
        favce_err("open %s error\n", filename);
        damnit(MODULE_NAME);
        return -1;
    }
    while ((ret = readline(fd, sizeof(line), line, &offset))) {
        if (ddr_idx && strstr(line, "enc_refer_ddr")) {
            sscanf(line, "enc_refer_ddr=%d", ddr_idx);
            ret |= 0x01;
            if (ret == checker)
                break;
        }
        if (compress_rate && strstr(line, "min_compressed_ratio")) {
            sscanf(line, "min_compressed_ratio=%d", compress_rate);
            ret |= 0x02;
            if (ret == checker)
                break;
        }
    }
    filp_close(fd, NULL);
    set_fs(fs);

    return 0;
}

int dump_ref_pool(char *str)
{
    int res;
    int len = 0;
    int cnt = 0;
    struct ref_item_t *buf;
    unsigned long flags;
    
    dmp_ref("Reference Pool");
    spin_lock_irqsave(&favce_ref_lock, flags);
    dmp_ref("Avail:");
    dmp_ref(" id    addr_virt      addr_phy       size");
    dmp_ref("====  ============  ============  ==========");
    for (res = 0; res < MAX_RESOLUTION; res++) {
        if (ref_pool[res].buf_num > 0) {
            buf = ref_pool[res].avail;
            cnt = 0;
            while (buf) {
                dmp_ref("%3d    0x%08x    0x%08x    %8d", buf->buf_idx, buf->addr_va, buf->addr_pa, buf->size);
                buf = buf->next;
            }
        }
    }
    dmp_ref("Allocated:");
    dmp_ref(" id    addr_virt      addr_phy       size");
    dmp_ref("====  ============  ============  ==========");
    for (res = 0; res < MAX_RESOLUTION; res++) {
        if (ref_pool[res].buf_num > 0) {
            buf = ref_pool[res].alloc;
            cnt = 0;
            while (buf) {
                dmp_ref("%3d    0x%08x    0x%08x    %8d", buf->buf_idx, buf->addr_va, buf->addr_pa, buf->size);
                buf = buf->next;
            }
        }
    }
    spin_unlock_irqrestore(&favce_ref_lock, flags);
    return len;
}

int dump_chn_pool(char *str)
{
    int i, chn;
    struct ref_item_t *ref;
    int len = 0, cnt;
    unsigned long flags;
    char suit_res_name[6], res_name[6];

    dmp_ref("Channel used pool");
    dmp_ref(" chn     res     s.res     addr_virt      addr_phy       size");
    dmp_ref("=====  =======  =======   ============  ============  ==========");
    spin_lock_irqsave(&favce_ref_lock, flags);
    for (chn = 0; chn < h264e_max_chn; chn++) {
        cnt = 0;
        if (chn_pool[chn].res_type >= 0 && chn_pool[chn].res_type < MAX_RESOLUTION)
            strcpy(res_name, res_base_info[chn_pool[chn].res_type].name);
        else
            strcpy(res_name, "NULL");
        if (chn_pool[chn].suitable_res_type >= 0 && chn_pool[chn].suitable_res_type < MAX_RESOLUTION)
            strcpy(suit_res_name, res_base_info[chn_pool[chn].suitable_res_type].name);
        else
            strcpy(suit_res_name, "NULL");
        for (i = 0; i < MAX_NUM_REF_FRAMES + 1; i++) {
            if (chn_pool[chn].alloc_buf[i]) {
                ref = chn_pool[chn].alloc_buf[i];
                dmp_ref(" %3d    %5s    %5s     0x%08x    0x%08x    %8d", chn, 
                    res_name, suit_res_name, ref->addr_va, ref->addr_pa, ref->size);
                cnt++;
            }
        }
        if (0 == cnt && chn_pool[chn].suitable_res_type >= 0)
            dmp_ref(" %3d    %5s    %5s     ----------    ----------      -----", chn, res_name, suit_res_name);
    }
    spin_unlock_irqrestore(&favce_ref_lock, flags);
    return len;
}

#ifdef PROC_SEQ_OUTPUT
int dump_one_chn_info(struct seq_file *m, int chn)
{
    struct favce_data_t *enc_data;

    if (chn < 0 || chn >= h264e_max_chn)
        return -1;

    enc_data = private_data + chn;

    if (enc_data->res_pool_type< 0)
        return -1;

    seq_printf(m, " %3d    %4dx%4d      %-5s   %3d", chn, enc_data->buf_width, enc_data->buf_height, 
            res_base_info[enc_data->res_pool_type].name, enc_data->idr_interval);
    seq_printf(m, "   %4s   %7d/%-7d  %7d    %7d      %2d      %2d     %2d",
        rc_mode_name[enc_data->rc_mode], enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio),
        EM_PARAM_N(enc_data->fps_ratio), enc_data->bitrate, enc_data->max_bitrate, 
        enc_data->init_quant, enc_data->min_quant, enc_data->max_quant);
    seq_printf(m, "   %2d\n", enc_data->cur_qp);
    return 0;
}
#endif

int dump_chn_buffer(char *str)
{
    int len = 0;
    int chn;
    struct favce_data_t *enc_data;

#if 0
    dmp_ref(" chn    resolution   buf.type");
    dmp_ref("=====  ============  ========");
    for (chn = 0; chn < h264e_max_chn; chn++) {
        enc_data = private_data + chn;
        if (enc_data->res_pool_type >= 0) {
            dmp_ref(" %3d    %4dx%4d      %s", chn, enc_data->buf_width, enc_data->buf_height, 
                res_base_info[enc_data->res_pool_type].name);
        }
    }
#else
    len += sprintf(str+len, " chn    resolution   buf.type  gop   mode         fps         bitrate    max.br    init.q  min.q  max.q  qp\n");
    len += sprintf(str+len, "=====  ============  ========  ===  ======  ===============  =========  =========  ======  =====  =====  ==\n");
    
    for (chn = 0; chn < h264e_max_chn; chn++) {
        enc_data = private_data + chn;
    
        if (enc_data->res_pool_type >= 0) {
            len += sprintf(str+len, " %3d    %4dx%4d      %-5s   %3d", chn, enc_data->buf_width, enc_data->buf_height, 
                res_base_info[enc_data->res_pool_type].name, enc_data->idr_interval);
        }
        else
            continue;
        len += sprintf(str+len, "   %4s   %7d/%-7d  %7d    %7d      %2d      %2d     %2d",
            rc_mode_name[enc_data->rc_mode], enc_data->src_frame_rate * EM_PARAM_M(enc_data->fps_ratio),
            EM_PARAM_N(enc_data->fps_ratio), enc_data->bitrate, enc_data->max_bitrate, 
            enc_data->init_quant, enc_data->min_quant, enc_data->max_quant);
        len += sprintf(str+len, "   %2d", enc_data->cur_qp);
        len += sprintf(str+len, "\n");
    }
#endif
    return len;
}

int dump_pool_num(char *str)
{
    int res;
    int size, cnt;
    int total_size = 0;
    int len = 0;
    struct ref_item_t *buf;
    int mvinfo_size, sysinfo_size, l1col_size;

#ifdef DIFF_REF_DDR_MAPPING
    if (favce_ref_buffer[0].size > 0) {
        dmp_ref("allocate reference buffer va0x%x/pa0x%x, size %d", 
            favce_ref_buffer[0].addr_va, favce_ref_buffer[0].addr_pa, favce_ref_buffer[0].size);
    }
    if (favce_ref_buffer[1].size > 0) {
        dmp_ref("allocate reference buffer va0x%x/pa0x%x, size %d", 
            favce_ref_buffer[1].addr_va, favce_ref_buffer[1].addr_pa, favce_ref_buffer[1].size);
    }
#else
    dmp_ref("allocate reference buffer va0x%x/pa0x%x, size %d", 
        favce_ref_buffer.addr_va, favce_ref_buffer.addr_pa, favce_ref_buffer.size);
#endif
    for (res = 0; res < MAX_RESOLUTION; res++) {
        if (ref_pool[res].buf_num > 0) {
            size = 0;
            cnt = 0;
            buf = ref_pool[res].avail;
            while (buf) {
                size += buf->size;
                cnt++;
                buf = buf->next;                
            }
            buf = ref_pool[res].alloc;
            while (buf) {
                size += buf->size;
                cnt++;
                buf = buf->next;                
            }
            total_size += size;
        #ifdef DIFF_REF_DDR_MAPPING
            dmp_ref("\t%s: unit %d, num %d, size %d (ddr%d)", res_base_info[res].name, 
                res_base_info[res].width * res_base_info[res].height * 3 / 2, cnt, size, ref_pool[res].ddr_idx);
        #else
            dmp_ref("\t%s: unit %d, num %d, size %d", res_base_info[res].name, res_base_info[res].width * res_base_info[res].height * 3 / 2, cnt, size);
        #endif
        }
    }
    sysinfo_size = get_common_buffer_size(SYSO_BUF_MASK);
    mvinfo_size = get_common_buffer_size(MBINFO_BUF_MASK);
    l1col_size = get_common_buffer_size(L1_BUF_MASK);
    dmp_ref("\tsys info size %d, mvinfo size %d, l1col size %d", sysinfo_size, mvinfo_size, l1col_size);
    total_size += (sysinfo_size + mvinfo_size + l1col_size);
    dmp_ref("\t\ttotal size %d byte (%d.%dM)", total_size, total_size / 1048576, (total_size%1048576)*1000/1048576);
    return len;
}

int dump_layout_info(char *str)
{
    int l, i, len = 0;
    int str_len = 0;
    char line[0x100];
    for (l = 0; l < MAX_LAYOUT_NUM; l++) {
        if (layout_info[l].max_channel > 0) {
            dmp_ref("========== layout %d ==========", l);
            str_len = 0;
            for (i = 0; i < MAX_RESOLUTION; i++)
                str_len += sprintf(line + str_len, "%6s", res_base_info[i].name);
            dmp_ref("%s", line);
            str_len = 0;
            for (i = 0; i < MAX_RESOLUTION; i++)
                str_len += sprintf(line + str_len, "%6d", layout_info[l].ref_pool[i].max_chn);
            dmp_ref("%s", line);
        }
    }
    return len;
}

static int insert_buf_to_pool(struct ref_item_t **head, struct ref_item_t *buf)
{
    if (NULL == (*head)) {
        (*head) = buf;
    }
    else {
        (*head)->prev = buf;
        buf->next = (*head);
        buf->prev = NULL;
        (*head) = buf;
    }
    return 0;
}

static int remove_buf_from_pool(struct ref_item_t **head, struct ref_item_t *buf)
{
    struct ref_item_t *next, *prev;
    next = buf->next;
    prev = buf->prev;
    if (next) {
        next->prev = prev;
        // be head
        if (NULL == prev)
            (*head) = next;
    }
    if (prev)
        prev->next = next;
    if (NULL == next && NULL == prev)
        (*head) = NULL;
    buf->next = buf->prev = NULL;
    return 0;
}

static int put_buf_to_chn_pool(struct channel_ref_buffer_t *pool, struct ref_item_t *ref)
{
    int i, idx = pool->head;
    // select one to buf: FIFO
    //for (i = 0; i < MAX_NUM_REF_FRAMES + 1; i++) {
    for (i = 0; i < pool->max_num; i++) {
        if (NULL == pool->alloc_buf[idx]) {
            pool->alloc_buf[idx] = ref;
            pool->head = (pool->head + 1)%pool->max_num;
            pool->used_num++;
            return 0;
        }
        idx = (idx + 1)%pool->max_num;
    }
    
    // not find empty buffer
    favce_err("put buf to pool error\n");
    mem_error();

    return -1;
}

static struct ref_item_t *remove_buf_from_chn_pool(struct channel_ref_buffer_t *pool, unsigned int addr_va)
{
    int i, idx = pool->tail;
    struct ref_item_t *ref;
    for (i = 0; i < pool->max_num; i++) {
        if (pool->alloc_buf[idx] && pool->alloc_buf[idx]->addr_va == addr_va) {
            ref = pool->alloc_buf[idx];
            pool->alloc_buf[idx] = NULL;
            pool->used_num--;
            pool->tail = (pool->tail + 1)%pool->max_num;
            return ref;
        }
        idx = (idx + 1)%pool->max_num;
    }
    
    // not find this buffer
    mem_error();
    return NULL;
}

static int reset_current_layout(int layout_idx)
{
    int i;
    if (layout_info[layout_idx].max_channel == 0) {
        favce_err("not such layout %d\n", layout_idx);
        return -1;
    }
    if (cur_layout < 0 || cur_layout != layout_idx) {
        for (i = 0; i < MAX_RESOLUTION; i++) {
            // check all buffer are avail
            if (cur_layout >= 0) {
                if (ref_pool[i].alloc) {
                    favce_err("layout change but there are some buffers not released!\n");
                    mem_error();
                }
            }
            memcpy(&ref_pool[i], &layout_info[layout_idx].ref_pool[i], sizeof(struct ref_pool_t));
        }
    }
    cur_layout = layout_idx;
#ifdef AUTOMATIC_SET_MAX_RES
    for (i = 0; i < MAX_RESOLUTION; i++) {
        if (layout_info[layout_idx].ref_pool[i].max_chn >= 1) {
            if (0 == h264e_max_width)
                h264e_max_width = res_base_info[i].width;
            if (0 == h264e_max_height)
                h264e_max_height = res_base_info[i].height;
            break;
        }
   }
#endif
    return 0;
}

static int filter_over_res_layout(int max_width, int max_height)
{
    int l;//, i;
    for (l = 0; l < MAX_LAYOUT_NUM; l++) {
        if (layout_info[l].max_channel <= 0)
            continue;
        /*
        // merge 2M & 1080P
        layout_info[l].ref_pool[RES_1080P].max_chn += layout_info[l].ref_pool[RES_2M].max_chn;
        layout_info[l].ref_pool[RES_2M].max_chn = 0;
        */
    #ifdef ONE_REF_BUF_VCACHE_OFF_SMALL
        if (h264e_one_ref_buf)
            layout_info[l].ref_pool[RES_CIF].max_chn += layout_info[l].ref_pool[RES_QCIF].max_chn;
    #endif
        /*
        if (0 == max_width * max_height)
            continue;
        for (i = 0; i < MAX_RESOLUTION; i++) {
            if (layout_info[l].ref_pool[i].max_chn > 0) {
                if (res_base_info[i].width*res_base_info[i].height > max_width * max_height) {
                    layout_info[l].ref_pool[i].max_chn = 0;
                    printk("filter resolution %d x %x, max %d x %d\n", 
                        res_base_info[i].width, res_base_info[i].height, max_width, max_height);
                }
            }
        }
        */
    }
    return 0;
}

static int add_snapshot_channel(int ss_chn, int ddr_idx)
{
    int l, res_type;
#ifdef AUTOMATIC_SET_MAX_RES
    if (0 == h264e_max_width || 0 == h264e_max_height) {
        for (l = 0; l < MAX_LAYOUT_NUM; l++) {
            if (layout_info[l].max_channel <= 0)
                continue;
            for (res_type = 0; res_type < MAX_RESOLUTION; res_type++) {
                if (layout_info[l].ref_pool[res_type].max_chn > 0) {
                    layout_info[l].ref_pool[res_type].max_chn += ss_chn;
                    break;
                }
            }
        }
    }
    else {
        for (res_type = MAX_RESOLUTION - 1; res_type >= 0; res_type--) {
            if (res_base_info[res_type].width >= h264e_max_width && res_base_info[res_type].height >= h264e_max_height)
                break;
        }
        if (res_base_info[res_type].width < h264e_max_width && res_base_info[res_type].height < h264e_max_height) {
            favce_err("resolution is over max resolution, %d x %d > %d x %d\n", 
                h264e_max_width, h264e_max_height, res_base_info[res_type].width, res_base_info[res_type].height);
            return -1;
        }
        for (l = 0; l < MAX_LAYOUT_NUM; l++) {
            if (layout_info[l].max_channel > 0) {
                if (0 == layout_info[l].ref_pool[res_type].max_chn)
                    layout_info[l].ref_pool[res_type].ddr_idx = ddr_idx;
                layout_info[l].ref_pool[res_type].max_chn += ss_chn;
            }
        }
    }
#else
    for (res_type = MAX_RESOLUTION - 1; res_type >= 0; res_type--) {
        if (res_base_info[res_type].width >= h264e_max_width && res_base_info[res_type].height >= h264e_max_height)
            break;
    }
    if (res_base_info[res_type].width < h264e_max_width && res_base_info[res_type].height < h264e_max_height) {
        favce_err("resolution is over max resolution, %d x %d > %d x %d\n", 
            h264e_max_width, h264e_max_height, res_base_info[res_type].width, res_base_info[res_type].height);
        return -1;
    }
    for (l = 0; l < MAX_LAYOUT_NUM; l++) {
        if (layout_info[l].max_channel > 0) {
            if (0 == layout_info[l].ref_pool[res_type].max_chn)
                layout_info[l].ref_pool[res_type].ddr_idx = ddr_idx;
            layout_info[l].ref_pool[res_type].max_chn += ss_chn;
        }
    }
#endif
    return 0;
}

static int get_pool_buf_num(struct ref_pool_t *pool)
{
    int buf_num = 0;
    if (0 == pool->max_chn)
        return buf_num;
#ifdef SAME_REC_REF_BUFFER
    if (h264e_one_ref_buf)
        buf_num = pool->max_chn;
    else {  // must be disable b
        if (pool->max_chn > 1)
            buf_num = pool->max_chn + MAX_ENGINE_NUM;
        else
            buf_num = pool->max_chn + 1;
    }
#elif defined(DISABLE_B_FRAME)
    if (pool->max_chn > 1)
        buf_num = pool->max_chn + MAX_ENGINE_NUM;
    else
        buf_num = pool->max_chn + 1;
#else
    if (h264e_max_b_frame) {
        if (pool->max_chn > 1)
            buf_num = pool->max_chn * 2 + MAX_ENGINE_NUM;
        else
            buf_num = pool->max_chn * 2 + 1;
    }
    else {
        if (pool->max_chn > 1)
            buf_num = pool->max_chn + MAX_ENGINE_NUM;
        else
            buf_num = pool->max_chn + 1;
    }
#endif
    return buf_num;
}

static int allocate_ref_ddr(struct buffer_info_t *ref_ddr, int ref_size, int ddr_idx)
{
    struct frammap_buf_info buf_info;

    memset(&buf_info, 0, sizeof(struct frammap_buf_info));
    buf_info.size = ref_size;//total_ref_size;
    buf_info.align = 32;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    buf_info.name = "h264e";
#endif
    buf_info.alloc_type = ALLOC_NONCACHABLE;
    if (1 == ddr_idx) {
        if (frm_get_buf_ddr(DDR_ID1, &buf_info) < 0) {
            favce_err("allocate reference buffer pool from DDR1 error\n");
            return -1;
        }
    }
    else if (0 == ddr_idx) {
        if (frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info) < 0) {
            favce_err("allocate reference buffer pool from DDR0 error\n");
            return -1;
        }
    }
    if (0 == buf_info.va_addr) {
        favce_err("allocate reference buffer pool error\n");
        return -1;
    }
    ref_ddr->addr_va = (unsigned int)buf_info.va_addr;
    ref_ddr->addr_pa = (unsigned int)buf_info.phy_addr;
    ref_ddr->size = buf_info.size;
    return 0;
}

int get_ref_buffer_size(unsigned int res_type)
{
    if (res_type >= MAX_RESOLUTION) {
        favce_err("unknown resolution idx %d\n", res_type);
        return 0;
    }
    return res_base_info[res_type].width*res_base_info[res_type].height;
}

#ifdef PARSING_GMLIB_BY_VG
static void set_max_parameter(struct gmcfg_pool_info_t *pool)
{
    int i;
    int max_w = 0, max_h = 0, max_res = 0;
    int chn_count = 0;
    
    for (i = 0; i < pool->element_count; i++) {
        DEBUG_M(LOG_DEBUG, "%s, chn %d, ddr %d/%d, %d x %d\n", pool->element[i].name, pool->element[i].channels, 
            pool->element[i].ddr_no, pool->element[i].ddr_no_2, pool->element[i].width, pool->element[i].height);
        if (pool->element[i].channels < 0 || pool->element[i].ddr_no < 0 || pool->element[i].ddr_no >= DDR_ID_MAX) {
            pool->element[i].channels = 0;
            pool->element[i].ddr_no = 0;
            pool->element[i].width = 0;
            pool->element[i].height = 0;
        }
    #ifdef SUPPORT_REF_TWO_DDR
        if (pool->element[i].ddr_no_2 <= 0 || pool->element[i].ddr_no_2 >= DDR_ID_MAX
            || pool->element[i].ddr_no == pool->element[i].ddr_no_2)
            pool->element[i].ddr_no_2 = -1;
    #endif
        // set min width for vcache off buffer (2 buffer)
        if (pool->element[i].width < (VCACHE_SUPPORT_MIN_WIDTH-16)*2)
            pool->element[i].width = (VCACHE_SUPPORT_MIN_WIDTH-16)*2;
        chn_count += pool->element[i].channels;
        if (max_res < pool->element[i].width*pool->element[i].height) {
            max_res = pool->element[i].width*pool->element[i].height;
            max_w = pool->element[i].width;
            max_h = pool->element[i].height;
        }
    }
    if (0 == h264e_max_width || 0 == h264e_max_height) {
        h264e_max_width = max_w;
        h264e_max_height = max_h;
    }
    if (0 == h264e_max_chn)
        h264e_max_chn = chn_count;
}

static void sort_reference_buffer_layout(struct gmcfg_pool_info_t *pool, struct layout_info_t *pLayout)
{
    int i;//, j;
    //struct gmcfg_pool_element_t tmp_element;
    int idx = 0;
    int chn_cnt = 0;

    // sort
    /*
    for (i = 0; i < pool->element_count - 1; i++) {
        for (j = 0; j < pool->element_count - i - 1; j++) {
            if (pool->element[j].width*pool->element[j].height > pool->element[j+1].width*pool->element[j+1].height) {
                memcpy(&tmp_element, &pool->element[j], sizeof(struct gmcfg_pool_element_t));
                memcpy(&pool->element[j], &pool->element[j+1], sizeof(struct gmcfg_pool_element_t));
                memcpy(&pool->element[j+1], &tmp_element, sizeof(struct gmcfg_pool_element_t));
            }
        }
    }
    */

    //idx = 0;
    for (i = 0; i < pool->element_count; i++) {
        if (0 == pool->element[i].channels)
            continue;
        idx = get_res_idx(pool->element[i].name);
        if (idx >= 0) {
            pLayout->ref_pool[idx].max_chn = pool->element[i].channels;
            pLayout->ref_pool[idx].max_fps = 30;
            pLayout->ref_pool[idx].ddr_idx = pool->element[i].ddr_no;
            chn_cnt += pool->element[i].channels;
        }
        else {
            favce_err("wrong input: res name %s, ddr_idx %d\n", pool->element[i].name, pool->element[i].ddr_no);
        }
    }
    pLayout->max_channel = chn_cnt;
    //pLayout->element_count = idx;
}
#endif

int init_mem_pool(void)
{
    int res, l, i, idx = 0;
    int layout_num, buf_num, max_layout_idx = 0;
    int ddr_idx = 0;
    unsigned int total_ref_size = 0;
    unsigned int max_ref_size = 0;
#ifdef DIFF_REF_DDR_MAPPING
    unsigned int ref_ddr_size[2] = {0, 0};
    unsigned int max_ref_ddr_size[2] = {0, 0};
#endif
    //unsigned int base_va, base_pa;
    //struct frammap_buf_info buf_info;
    struct ref_pool_t *pool;
#ifdef PARSING_GMLIB_BY_VG
    struct gmcfg_pool_info_t gm_pool;
#else
    int gmlib_exist = 0;
#endif
    
    spin_lock_init(&favce_ref_lock);

    chn_pool = kzalloc(sizeof(struct channel_ref_buffer_t) * h264e_max_chn, GFP_KERNEL);
    if (NULL == chn_pool) {
        favce_err("Fail to allocate chn_pool!\n");
        goto init_mem_pool_fail;
    }

    // 0. reset global parameter
    memset(layout_info, 0, sizeof(struct layout_info_t) * MAX_LAYOUT_NUM);
    //memset(ref_pool, 0, sizeof(struct ref_pool_t) * MAX_RESOLUTION);
    //memset(chn_pool, 0, sizeof(struct channel_ref_buffer_t) * h264e_max_chn);
#ifdef PARSING_GMLIB_BY_VG
    // 1. get buffer info
    gmcfg_get_enc_in_pool_info(&gm_pool);

    // 1.1 automatically set resolution & channel
    set_max_parameter(&gm_pool);

    // 2. sort buffer size
    sort_reference_buffer_layout(&gm_pool, &layout_info[0]);
    layout_num = 1;
#else
    // 1. load spec configure
    if (0 == (layout_num = parse_spec_cfg())) {
        favce_err("parsing buffer configure error\n");
        goto init_mem_pool_fail;
    }
    //dump_layout_info(NULL);

    // 2. load param (ddr index)
    if ((gmlib_exist = parse_param_cfg(&ddr_idx, NULL)) < 0) {
        favce_err("parsing ddr index error\n");
        goto init_mem_pool_fail;
    }
#endif
    filter_over_res_layout(((h264e_max_width+15)/16)*16, ((h264e_max_width+15)/16)*16);

    // 2-1. add two extra buffer of max reoslution
#ifdef TIGHT_BUFFER_ALLOCATION
    if (0 == h264e_tight_buf) 
#endif
    {
        for (l = 0; l < layout_num; l++) {
            for (i = 0; i < MAX_RESOLUTION; i++) {
                // Tuba 20131118: extra buffer when channel larger than 1
                if (layout_info[l].ref_pool[i].max_chn > 1) {
                    layout_info[l].ref_pool[i].max_chn += MAX_ENGINE_NUM;
                    break;
                }
            }
        }
    }

    // 2-2. add snapshot channel
    if (add_snapshot_channel(h264e_snapshot_chn, ddr_idx) < 0)
        goto init_mem_pool_fail;

    // 3. compute number of reference buffer
    max_ref_buf_num = 0;
    for (l = 0; l < layout_num; l++) {
        layout_info[l].buf_num = 0;
        layout_info[l].index = l;
        total_ref_size = 0;
        // can not reset ref pool because max_chn contains info
        for (i = 0; i < MAX_RESOLUTION; i++) {
            if (layout_info[l].ref_pool[i].max_chn) {
                buf_num = get_pool_buf_num(&layout_info[l].ref_pool[i]);
                total_ref_size += res_base_info[i].width * res_base_info[i].height * 3 / 2 * buf_num; 
                max_ref_buf_num += buf_num;
                layout_info[l].buf_num += buf_num;
            #ifdef DIFF_REF_DDR_MAPPING
                if (0 == gmlib_exist)
                    layout_info[l].ref_pool[i].ddr_idx = ddr_idx;
                ref_ddr_size[layout_info[l].ref_pool[i].ddr_idx] += res_base_info[i].width * res_base_info[i].height * 3 / 2 * buf_num; 
            #endif
            }
        }
        if (total_ref_size > max_ref_size) {
            max_ref_size = total_ref_size;
            max_layout_idx = l;
        #ifdef DIFF_REF_DDR_MAPPING
            max_ref_ddr_size[0] = ref_ddr_size[0];
            max_ref_ddr_size[1] = ref_ddr_size[1];
        #endif
        }
    }

    ref_item = kzalloc(sizeof(struct ref_item_t) * max_ref_buf_num, GFP_KERNEL);
    if (NULL == ref_item) {
        favce_err("allocate reference item error\n");
        goto init_mem_pool_fail;
    }        
    memset(ref_item, 0 , sizeof(struct ref_item_t) * max_ref_buf_num);
    memset(ref_pool, 0 , sizeof(ref_pool));
    //memset(chn_pool, 0 , sizeof(chn_pool));

    // 4. allocate buffer
#if 0
    memset(&buf_info, 0, sizeof(struct frammap_buf_info));
    buf_info.size = max_ref_size;//total_ref_size;
    buf_info.align = 32;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
    buf_info.name = "h264e";
#endif
    buf_info.alloc_type = ALLOC_NONCACHABLE;
    if (1 == ddr_idx) {
        if (frm_get_buf_ddr(DDR_ID1, &buf_info) < 0) {
            favce_err("allocate reference buffer pool from DDR1 error\n");
            goto init_mem_pool_fail;
        }
    }
    else {
        if (frm_get_buf_ddr(DDR_ID_SYSTEM, &buf_info) < 0) {
            favce_err("allocate reference buffer pool from DDR0 error\n");
            goto init_mem_pool_fail;
        }
    }
    if (0 == buf_info.va_addr) {
        favce_err("allocate reference buffer pool error\n");
        goto init_mem_pool_fail;
    }
    favce_ref_buffer.addr_va = (unsigned int)buf_info.va_addr;
    favce_ref_buffer.addr_pa = (unsigned int)buf_info.phy_addr;
    favce_ref_buffer.size = buf_info.size;
#else
    #ifndef DIFF_REF_DDR_MAPPING
    if (allocate_ref_ddr(&favce_ref_buffer, max_ref_size, ddr_idx) < 0)
        return -1;
    #else
    if (max_ref_ddr_size[0])
        allocate_ref_ddr(&favce_ref_buffer[0], max_ref_ddr_size[0], 0);
    if (max_ref_ddr_size[1])
        allocate_ref_ddr(&favce_ref_buffer[1], max_ref_ddr_size[1], 1);
    #endif
#endif
    //dump_pool_num(ddr_idx);
    
    // 5. buffer assignment
    for (l = 0; l < layout_num; l++) {
    #ifdef DIFF_REF_DDR_MAPPING
        unsigned int base_va[2], base_pa[2];
        base_va[0] = favce_ref_buffer[0].addr_va;
        base_pa[0] = favce_ref_buffer[0].addr_pa;
        base_va[1] = favce_ref_buffer[1].addr_va;
        base_pa[1] = favce_ref_buffer[1].addr_pa;
    #else
        unsigned int base_va, base_pa;
        base_va = favce_ref_buffer.addr_va;
        base_pa = favce_ref_buffer.addr_pa;
    #endif
        for (res = 0; res < MAX_RESOLUTION; res++) {
            pool = &layout_info[l].ref_pool[res];
            pool->res_type = res;
            pool->buf_num = 
            pool->avail_num = 
            pool->used_num = 0;
            pool->avail = 
            pool->alloc = NULL;
            if (pool->max_chn) {
                unsigned int size = res_base_info[res].width * res_base_info[res].height * 3 / 2;
                buf_num = get_pool_buf_num(pool);
                for (i = 0; i < buf_num; i++) {
                #ifdef DIFF_REF_DDR_MAPPING
                    ref_item[idx].addr_va = base_va[pool->ddr_idx];
                    ref_item[idx].addr_pa = base_pa[pool->ddr_idx];
                #else
                    ref_item[idx].addr_va = base_va;
                    ref_item[idx].addr_pa = base_pa;
                #endif
                    ref_item[idx].size = size;
                    ref_item[idx].res_type = res;
                    ref_item[idx].buf_idx = idx;
                    insert_buf_to_pool(&pool->avail, &ref_item[idx]);
                    pool->buf_num++;
                    pool->avail_num++;
                    //ref_pool[res].buf_num++;
                    //ref_pool[res].avail_num++;
                    idx++;
                #ifdef DIFF_REF_DDR_MAPPING
                    base_va[pool->ddr_idx] += size;
                    base_pa[pool->ddr_idx] += size;
                    if (base_va[pool->ddr_idx] > favce_ref_buffer[pool->ddr_idx].addr_va + favce_ref_buffer[pool->ddr_idx].size) {
                        //dump_pool_num(ddr_idx);
                        favce_err("allocated buffer size & pool size do not match\n");
                        mem_error();
                        clean_mem_pool();
                        goto init_mem_pool_fail;
                    }
                #else
                    base_va += size;
                    base_pa += size;
                    if (base_va > favce_ref_buffer.addr_va + favce_ref_buffer.size) {
                        //dump_pool_num(ddr_idx);
                        favce_err("allocated buffer size & pool size do not match\n");
                        mem_error();
                        clean_mem_pool();
                        goto init_mem_pool_fail;
                    }
                #endif
                }
            }
        }        
    }
    reset_current_layout(max_layout_idx);
    //dump_pool_num(ddr_idx);
    //dump_ref_pool(NULL);

    // 6. channel pool init
    for (i = 0; i < h264e_max_chn; i++) {
        chn_pool[i].chn = i;
        chn_pool[i].res_type = -1;
    #ifdef SAME_REC_REF_BUFFER
        if (h264e_one_ref_buf)
            chn_pool[i].max_num = 1;
        else
            chn_pool[i].max_num = 1 + 1;
    #else
        if (h264e_max_b_frame)
            chn_pool[i].max_num = 2 + 1;
        else
            chn_pool[i].max_num = 1 + 1;
    #endif
        chn_pool[i].suitable_res_type = -1;
    }
    
    return 0;

init_mem_pool_fail:
    if (chn_pool)
        kfree(chn_pool);
    chn_pool = NULL;
    if (ref_item)
        kfree(ref_item);
    ref_item = NULL;
#ifdef DIFF_REF_DDR_MAPPING
    if (favce_ref_buffer[0].addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer[0].addr_va);
    if (favce_ref_buffer[1].addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer[1].addr_va);
#else
    if (favce_ref_buffer.addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer.addr_va);
#endif
    //damnit(MODULE_NAME);
    return -1;
}

int clean_mem_pool(void)
{
#ifdef DIFF_REF_DDR_MAPPING
    if (favce_ref_buffer[0].addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer[0].addr_va);
    if (favce_ref_buffer[1].addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer[1].addr_va);
#else
    if (favce_ref_buffer.addr_va)
        frm_free_buf_ddr((void *)favce_ref_buffer.addr_va);
#endif
    if (ref_item)
        kfree(ref_item);
    if (chn_pool)
        kfree(chn_pool);
    return 0;
}

/* 
 * accumulate from smaller buffer to larger buffer & check if suitable buffer over spec
*/
static int check_over_spec(void)
{
    int i, count = 0;
    int chn_res_cnt = 0;
    int res_cnt[MAX_RESOLUTION];

    memset(res_cnt, 0, sizeof(res_cnt));
    for (i = 0; i < h264e_max_chn; i++) {
        if (chn_pool[i].suitable_res_type >= 0)
            res_cnt[chn_pool[i].suitable_res_type]++;
    }
    for (i = 0; i < MAX_RESOLUTION; i++) {
        chn_res_cnt += res_cnt[i];
        count += ref_pool[i].max_chn;
        if (chn_res_cnt > count) {
            //favce_wrn("allocated buffer over spec\n");
            //dump_ref_pool(NULL);
            //dump_chn_pool(NULL);
            return -1;
        }
    }
    return 0;
}

static int get_suitable_resolution(int width, int height)
{
    int i;
    for (i = MAX_RESOLUTION - 1; i >= 0; i--) {
    #ifdef STRICT_SUITABLE_BUFFER
        if (ref_pool[i].max_chn > 0 && res_base_info[i].width >= width && res_base_info[i].height >= height)
            return i;
    #else
        if (ref_pool[i].max_chn > 0 && res_base_info[i].width * res_base_info[i].height >= width * height)
            return i;
    #endif  
    }
    return -1;
}
/*
 * Return -1: can not find suitable buffer pool
 */
int register_ref_pool(unsigned int chn, int width, int height)
{
    int i, j;
    unsigned long flags;

    if (chn >= h264e_max_chn) {
        favce_err("channel id(%d) over max channel\n", chn);
        damnit(MODULE_NAME);
        return MEM_ERROR;
    }
    chn_pool[chn].suitable_res_type = get_suitable_resolution(width, height);
    // not suitable 
    if (chn_pool[chn].suitable_res_type < 0) {
        favce_err("{chn%d} no suitable buffer for %d x %d\n", chn, width, height);
        mem_error();
        return NO_SUITABLE_BUF;
    }

    if (check_over_spec() < 0) {
        chn_pool[chn].suitable_res_type = -1;
        favce_wrn("{chn%d} allocated buffer over spec, %d x %d\n", chn, width, height);
        //favce_err("{chn%d} allocated buffer over spec, %d x %d\n", chn, width, height);
        //dump_ref_pool(NULL);
        //dump_chn_pool(NULL);
        //mem_error();
        return OVER_SPEC;
    }
    /*
    if (chn_pool[chn].suitable_res_type < 0) {
        favce_err("{chn%d} no suitable resolution %d x %d\n", chn, width, height);
        dump_ref_pool(NULL);
        damnit(MODULE_NAME);
        return -1;
    }
    */
    spin_lock_irqsave(&favce_ref_lock, flags);

    for (i = MAX_RESOLUTION - 1; i >= 0; i--) {
    #ifdef STRICT_SUITABLE_BUFFER
        if (ref_pool[i].max_chn && res_base_info[i].width >= width && res_base_info[i].height >= height)
    #else
        if (ref_pool[i].max_chn && res_base_info[i].width * res_base_info[i].height >= width * height)
    #endif
        {
            // if register number hit max channel => register larger pool
            if (ref_pool[i].reg_num + 1 > ref_pool[i].max_chn) {
                //favce_wrn("{chn%d} pool %s is overflow, use larger pool\n", chn, res_base_info[i].name);
                continue;
            }
            // previous channel is not de-register
            if (chn_pool[chn].res_type >= 0) {
                // check all ref buffers are released
                /*
                if (chn_pool[chn].used_num) {
                    favce_err("chn%d is not free all reference\n", chn);
                    mem_error();
                    spin_unlock_irqrestore(&favce_ref_lock, flags);
                    return -1;
                }
                */
                for (j = 0; j <= MAX_NUM_REF_FRAMES; j++) {
                    if (chn_pool[chn].alloc_buf[j]) {
                        favce_err("chn%d ref buffer %d is not free\n", chn, chn_pool[chn].alloc_buf[j]->buf_idx);
                        mem_error();
                        spin_unlock_irqrestore(&favce_ref_lock, flags);
                        return MEM_ERROR;
                    }
                }
                ref_pool[chn_pool[chn].res_type].reg_num--;                
            }

            chn_pool[chn].res_type = i;
            ref_pool[i].reg_num++;

            favce_info("{chn%d} resolution = %d x %d, register %s (suit %s)\n", chn, width, height, 
                res_base_info[i].name, res_base_info[chn_pool[chn].suitable_res_type].name);
            spin_unlock_irqrestore(&favce_ref_lock, flags);
            return i;
        }
    }
    favce_wrn("{chn%d} can not find suitable pool for resolution %d x %d\n", chn, width, height);
    chn_pool[chn].suitable_res_type = -1;
    spin_unlock_irqrestore(&favce_ref_lock, flags);
    return NOT_OVER_SPEC_ERROR;
}

int deregister_ref_pool(unsigned int chn)
{
    unsigned long flags;
    int j;
    if (chn >= h264e_max_chn) {
        favce_err("deregister: channel id(%d) over max channel\n", chn);
        damnit(MODULE_NAME);
        return -1;
    }
    spin_lock_irqsave(&favce_ref_lock, flags);

    if (chn_pool[chn].res_type >= 0) {
        // check all ref buffers are released
        if (chn_pool[chn].used_num) {
            favce_err("chn%d is not free all reference\n", chn);
            mem_error();
            spin_unlock_irqrestore(&favce_ref_lock, flags);
            return -1;
        }
        for (j = 0; j <= MAX_NUM_REF_FRAMES; j++) {
            if (chn_pool[chn].alloc_buf[j]) {
                favce_err("chn%d ref buffer %d is not free\n", chn, chn_pool[chn].alloc_buf[j]->buf_idx);
                mem_error();
                spin_unlock_irqrestore(&favce_ref_lock, flags);
                return -1;
            }
        }
        favce_info("{chn%d} deregister %s\n", chn, res_base_info[chn_pool[chn].res_type].name);
        ref_pool[chn_pool[chn].res_type].reg_num--;
        chn_pool[chn].res_type = -1;
        chn_pool[chn].suitable_res_type = -1;
    }
    spin_unlock_irqrestore(&favce_ref_lock, flags);
    return 0;
}

/*
 * check buffer pool suitable: register buffer pool & suitable buffer pool
*/
int check_reg_buf_suitable(unsigned char *checker)
{
    int i, ret = 0;
    memset(checker, 0, sizeof(unsigned char)*h264e_max_chn);
    favce_info("check resolution & suitable resolution\n");
    for (i = 0; i < h264e_max_chn; i++) {
        if (chn_pool[i].res_type >= 0 && chn_pool[i].res_type != chn_pool[i].suitable_res_type) {
            checker[i] = 1;
            ret = 1;
        }
    }
    return ret;
}

int mark_suitable_buf(int chn, unsigned char *checker, int width, int height)
{
    int i, ret = 0;
    int suit_res_type;
    if (chn_pool[chn].suitable_res_type < 0)
        return -1;
    memset(checker, 0, sizeof(unsigned char)*h264e_max_chn);
    suit_res_type = get_suitable_resolution(width, height);
    for (i = 0; i < h264e_max_chn; i++) {
        if (chn_pool[i].res_type >= 0 && chn_pool[i].res_type <= suit_res_type) {
            checker[i] = 1;
            ret = 1;
        }
    }
    return ret;
}

int allocate_pool_buffer(unsigned int *addr_va, unsigned int *addr_pa, unsigned int res_idx, int chn)
{
    struct ref_item_t *ref;
    unsigned long flags;
    if (res_idx >= MAX_RESOLUTION) {
        favce_err("{chn%d} allocate unknown resolution idx %d\n", chn, res_idx);
        mem_error();
        //damnit(MODULE_NAME);
        return -1;
    }
    
    // spin lock
    spin_lock_irqsave(&favce_ref_lock, flags);
    
    if (NULL == ref_pool[res_idx].avail) {
        // error
        favce_err("%s: no ref buffer\n", res_base_info[res_idx].name);
        mem_error();
        return -1;
    }
    if (chn_pool[chn].used_num >= chn_pool[chn].max_num) {
        favce_err("{chn%d} allocate buffer %d over max reference number %d\n", chn, chn_pool[chn].used_num + 1, chn_pool[chn].max_num);
        mem_error();
        return -1;
    }
    
    ref = ref_pool[res_idx].avail;
    remove_buf_from_pool(&ref_pool[res_idx].avail, ref);
    ref_pool[res_idx].avail_num--;
    insert_buf_to_pool(&ref_pool[res_idx].alloc, ref);
    ref_pool[res_idx].used_num++;
    put_buf_to_chn_pool(&chn_pool[chn], ref);
    
    *addr_va = ref->addr_va;
    *addr_pa = ref->addr_pa;
    
    spin_unlock_irqrestore(&favce_ref_lock, flags);

    DEBUG_M(LOG_INFO, "{chn%d} allocate ref buf %s: va0x%x, pa0x%x\n", chn, res_base_info[res_idx].name, *addr_va, *addr_pa);
    
    return 0;
}

int release_pool_buffer(unsigned int addr_va, int chn)
{
    struct ref_item_t *ref;
    int res_idx;
    unsigned long flags;

    if (chn >= h264e_max_chn) {
        favce_err("channel id(%d) over max channel\n", chn);
        damnit(MODULE_NAME);
        return -1;
    }
    if (chn_pool[chn].res_type < 0 || chn_pool[chn].res_type >= MAX_RESOLUTION) {
        favce_err("{chn%d} unknown resolution %d\n", chn, chn_pool[chn].res_type);
        mem_error();
        return -1;
    }
    spin_lock_irqsave(&favce_ref_lock, flags);
    res_idx = chn_pool[chn].res_type;
    ref = remove_buf_from_chn_pool(&chn_pool[chn], addr_va);
    remove_buf_from_pool(&ref_pool[res_idx].alloc, ref);
    ref_pool[res_idx].used_num--;
    insert_buf_to_pool(&ref_pool[res_idx].avail, ref);
    ref_pool[res_idx].avail_num++;
//dump_ref_pool(NULL);

    spin_unlock_irqrestore(&favce_ref_lock, flags);

    DEBUG_M(LOG_INFO, "{chn%d} release ref buf %s: va0x%x\n", chn, res_base_info[res_idx].name, addr_va);
    
    return 0;
}
