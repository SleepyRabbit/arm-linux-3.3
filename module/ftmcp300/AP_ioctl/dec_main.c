#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <unistd.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <linux/version.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "h264d_ioctl.h"

#define GOLDEN_COMPARE
//#define USE_FEED_BACK       0

#define GOLDEN_ERROR        -20
#define OUTPUT_LIST_ERROR   -50
#define COMPARE_ERROR       -100
#define COMPARE_ERROR_SCALE -30
#define PAGE_ALIGN(n) (((n+0xFFF)/0x1000)*0x1000) // align n to a multiple of page size (4K)

typedef struct {
    char bsFilename[256];
    FILE *bsFile;
    char goldenFilename[256];
    FILE *goldenFile;
    int check_sum;
    char confFilename[256];
    FILE *confFile;
    char resultFilename[256];
    FILE *resFile;
    char scaleFilename[256];
    FILE *scaleFile;
    char patternName[256];
	char devName[256];
    int actual_frame_num;
    int width;
    int height;
    int MaxWidth;
    int MaxHeight;
    int save_yuv;
    int loop_flg;
    int err_handling;
} ConfigInfo;

typedef struct dec_info_t {
    int max_width;
    int max_height;
    int buffer_size;
    int ref_buffer_size;
    int scale_buffer_size;
    int framebuffer_rgb_stride;
    int framebuffer_rgb_height;
    int h264_dec_fd;
    int interlace;
    int scale_type;
    int scale_x;
    int scale_y;
    int save_scale;
    int unsupport_b;
    char *dev_name;
    unsigned char * mmap_addr;
    int max_buffered_num;
    
#ifdef GOLDEN_COMPARE   
    unsigned char *golden_buffer;
#endif
    FILE *sFile;
    FILE *yuvFile;
    // statistic/flags
    int curr_fr;
    int dec_pass_number;
    int vcache_enable;
    int multislice;
    int err_flg;
    int show_error_sts;
} dec_info;


int decoder_exit(dec_info * avctx)
{
    close(avctx->h264_dec_fd);
    if (avctx->mmap_addr){
        munmap(avctx->mmap_addr, avctx->buffer_size * MAX_DEC_BUFFER_NUM);
    }
    avctx->h264_dec_fd = -1;
    avctx->mmap_addr = NULL;
    //memset(avctx, 0, sizeof(*avctx));
    return 0;
}


int decoder_init(dec_info * avctx)
{
    FAVC_DEC_PARAM tDecParam;
    avctx->h264_dec_fd = open(avctx->dev_name, O_RDWR);
    if (avctx->h264_dec_fd < 0){
        printf("Fail to open %s\n", avctx->dev_name);
        return -1;
    }
    memset(&tDecParam, 0, sizeof(FAVC_DEC_PARAM));
    tDecParam.u32API_version = H264D_VER;
    tDecParam.u16MaxWidth = avctx->max_width;
    tDecParam.u16MaxHeight = avctx->max_height;


    tDecParam.u8MaxBufferedNum = avctx->max_buffered_num;
#if 1
    if (avctx->max_buffered_num == 0){
        tDecParam.u8MaxBufferedNum = 16;
    }
#endif
    //tDecParam.u8MaxBufferedNum = NO_BUFFERED;
    //tDecParam.u8MaxBufferedNum = 3; // for CONFIG_RESOLUTION_4K2K only

    tDecParam.bChromaInterlace = avctx->interlace;
    tDecParam.bUnsupportBSlice = avctx->unsupport_b;
    if (tDecParam.bUnsupportBSlice)
        tDecParam.u8MaxBufferedNum = NO_BUFFERED;
    if (avctx->scale_type < 0)
        tDecParam.fr_param.bScaleEnable = 0;
    else {
        tDecParam.fr_param.bScaleEnable = 1;
        tDecParam.fr_param.u8ScaleRow = avctx->scale_x;
        tDecParam.fr_param.u8ScaleColumn = avctx->scale_y;
        tDecParam.fr_param.u8ScaleType = avctx->scale_type;
    }

    if (ioctl(avctx->h264_dec_fd, H264_IOCTL_DECODE_INIT, &tDecParam) < 0) {
        printf("Error to set H264_IOCTL_DECODE_INIT\n");
        return -1;
    }

    avctx->mmap_addr = (unsigned char*)mmap(0, (avctx->buffer_size*MAX_DEC_BUFFER_NUM),
            PROT_READ/* | PROT_WRITE*/, MAP_SHARED, avctx->h264_dec_fd, 0);

    if ((int)avctx->mmap_addr == -1){
        printf("Fail to mmap frame buffer\n");
        exit(0);
    }


    return 0;
}

void configure()
{
    printf("dec_main -i [filename of input bitstream file]\n");
    printf("         -g [filename of golden file]\n");
    printf("         -gs [filename of golden check sum file]\n");
    printf("         -c [filename of bitstream length for each frame]\n");
    printf("         -r [filename of result file]\n");
    printf("         -e [filename of golden comparision error]\n");
    printf("         -n [pattern name]\n");
    printf("         -d [device name]\n");
    printf("         -f [actual frame num]\n");
    printf("         -w [frame width]\n");
    printf("         -h [frame height]\n");
    printf("         -W [max width]\n");
    printf("         -H [max height]\n");
    printf("         -s [scale golden file] [bit2:scale type bit1:scale_x_4/2 bit0:scale_y_4/2 bit3:save scale yuv]\n");
    printf("         -p [bit0:interlace, bit1:unsupport_b]\n");
    printf("         -S [save yuv enable]\n");
    printf("         -l [infinite-loop enable]\n");
    printf("         -E [error handling method: 0-ignore Non-paired-filed error 1-reinit decoder at error 2-ignore minor error]\n");
}

static unsigned int compute_check_sum(unsigned char *buf, int size, unsigned int *xor_sum)
{
    int i;
    unsigned int sum = 0, x_sum = 0;
    for (i = 0; i<size; i++) {
        sum += buf[i];
        x_sum ^= buf[i];
    }
    *xor_sum = x_sum;
    
    return sum;
}

static int buffer_compare(unsigned char *buf1, unsigned char *buf2, int size)
{
    int i;
    for (i = 0; i<size; i++) {
        if (buf1[i] != buf2[i])
            return 1;
    }
    return 0;
}

int golden_compare(unsigned char *res_buf, int width, int height, FILE *gFile, char *pattern_name, unsigned char *golden_buffer, int structure, int scale_flag, int check_sum)
{
    int i, j;
    unsigned char *ptRes, *ptGld;
    FILE *errFile = NULL;
    unsigned int sum, xor_sum, g_sum, g_xsum;
    char name[100];

    if (!structure) {   // frame
        if (check_sum) {
            sum = compute_check_sum(res_buf, width*height*2, &xor_sum);
            fscanf(gFile, "%u\t%u\n", &g_sum, &g_xsum);
            if (sum != g_sum || xor_sum != g_xsum) {
                printf("sum: res %08x, golden %08x\n", sum, g_sum);
                printf("xor sum: res %08x, golden %08x\n", xor_sum, g_xsum);
                return COMPARE_ERROR;
            }
            return 0;
        }
        // if check sum, return directly
        if (fread(golden_buffer, 1, width*height*2, gFile) != width*height*2) {
            printf("fail to load golden file\n");
            return GOLDEN_ERROR;
        }
        if (buffer_compare(golden_buffer, res_buf, width*height*2)!=0) {
            /* compare error */
            
            /* write decoded buffer to file */
            if (scale_flag)
                sprintf(name, "error_%s_scale_res.yuv", pattern_name);
            else
                sprintf(name, "error_%s_res.yuv", pattern_name);
            if ((errFile = fopen(name, "wb")) == NULL) {
                printf("fail to open file %s\n", name);
                return GOLDEN_ERROR;
            }
            fwrite(res_buf, 1, width*height*2, errFile);
            fclose(errFile);

            /* write golden buffer to file */
            if (scale_flag)
                sprintf(name, "error_%s_scale_gold.yuv", pattern_name);
            else
                sprintf(name, "error_%s_gold.yuv", pattern_name);
            if ((errFile = fopen(name, "wb")) == NULL) {
                printf("fail to open file %s\n", name);
                return GOLDEN_ERROR;
            }
            fwrite(golden_buffer, 1, width*height*2, errFile);
            fclose(errFile);
            
            return COMPARE_ERROR;
        }
    }
    else {
        if (fread(golden_buffer, 1, width*height, gFile) != width*height) {
            printf("fail to load golden file\n");
            return GOLDEN_ERROR;
        }

        ptGld = golden_buffer;
        if (structure == 1) // top
            ptRes = res_buf;
        else    // bottom
            ptRes = res_buf + width*2;
        for (i = 0; i<height/2; i++) {
            if (buffer_compare(ptRes, ptGld, width*2) != 0) {
                /* compare error */

                /* write a line of decoded buffer to file */
                sprintf(name, "error_%s_res.yuv", pattern_name);
                if ((errFile = fopen(name, "wb")) == NULL) {
                    printf("fail to open file %s\n", name);
                    return GOLDEN_ERROR;
                }
                if (structure == 1) // top
                    ptRes = res_buf;
                else
                    ptRes = res_buf + width*2;
                for (j = 0; j<height/2; j++) {
                    fwrite(ptRes, 1, width*2, errFile);
                    ptRes += width*4;
                }                   
                fclose(errFile);

                /* write a line of golden buffer to file */
                sprintf(name, "error_%s_gold.yuv", pattern_name);
                if ((errFile = fopen(name, "wb")) == NULL) {
                    printf("fail to open file %s\n", name);
                    return GOLDEN_ERROR;
                }
                fwrite(golden_buffer, 1, width*height, errFile);
                fclose(errFile);
                
                return COMPARE_ERROR;
            }
            ptRes += width*4;
            ptGld += width*2;
        }
    }

    return 0;
}

void dump_argu(ConfigInfo *conf_info)
{
    printf("bs filename %s\n", conf_info->bsFilename);
#ifdef GOLDEN_COMPARE
    printf("golden filename %s\n", conf_info->goldenFilename);
#endif
    printf("config filename %s\n", conf_info->confFilename);
    printf("result filename %s\n", conf_info->resultFilename);
    printf("pattern name %s\n", conf_info->patternName);
    printf("device name %s\n", conf_info->devName);
    printf("frame num %d\n", conf_info->actual_frame_num);
    printf("frame width %d\n", conf_info->width);
    printf("frame height %d\n", conf_info->height);
    printf("max width %d\n", conf_info->MaxWidth);
    printf("max height %d\n", conf_info->MaxHeight);
}

int parsing_argv(int argc, char *argv[], ConfigInfo *conf_info, dec_info *active_dec)
{
    int i, tmp;
    if (argc < 2)
        return -1;

    conf_info->save_yuv = 0;
    //conf_info->save_yuv = 1; /* set default value to 1 */

    active_dec->scale_type = -1;
    for (i = 1; i<argc; i+=2){
        if (argv[i][0] == '-') {
            switch(argv[i][1]) {
            case 'i':
                strcpy(conf_info->bsFilename, argv[i+1]);
                if ((conf_info->bsFile = fopen(conf_info->bsFilename, "rb"))==NULL){
                    printf("Can not open H.264 file %s\n", conf_info->bsFilename); 
                    return -1;
                }
                break;
            case 'g':
            #ifdef GOLDEN_COMPARE
                if (argv[i][2] == 's') {
                    conf_info->check_sum = 1;
                    strcpy(conf_info->goldenFilename, argv[i+1]);
                    if ((conf_info->goldenFile = fopen(conf_info->goldenFilename, "r"))==NULL){
                        printf("Can not open golden file %s\n", conf_info->goldenFilename);
                        return -1;
                    }
                }
                else {
                    conf_info->check_sum = 0;
                    strcpy(conf_info->goldenFilename, argv[i+1]);
                    if ((conf_info->goldenFile = fopen(conf_info->goldenFilename, "rb"))==NULL){
                        printf("Can not open golden file %s\n", conf_info->goldenFilename);
                        return -1;
                    }
                }
            #endif
                break;
            case 'c':
                strcpy(conf_info->confFilename, argv[i+1]);
                if ((conf_info->confFile = fopen(conf_info->confFilename, "r"))==NULL){
                    printf("Can not open config file %s\n", conf_info->confFilename);
                    return -1;
                }
                break;
            case 'r':
                strcpy(conf_info->resultFilename, argv[i+1]);
                if (strlen(conf_info->resultFilename) == 0) {
                    printf("Length of result file name is zero\n");
                    return -1;
                }
                break;
            case 'n':
                strcpy(conf_info->patternName, argv[i+1]);
                break;
            case 'd':
                strcpy(conf_info->devName, argv[i+1]);
                break;
            case 'f':
                conf_info->actual_frame_num = atoi(argv[i+1]);
                break;
            case 'w':
                active_dec->framebuffer_rgb_stride = atoi(argv[i+1]);
                if (active_dec->framebuffer_rgb_stride <= 0) {
                    printf("Illegal height (%d)\n", active_dec->framebuffer_rgb_stride);
                    return -1;
                }
                break;
            case 'h':
                active_dec->framebuffer_rgb_height = atoi(argv[i+1]);
                if (active_dec->framebuffer_rgb_height <= 0) {
                    printf("Illegal height (%d)\n", active_dec->framebuffer_rgb_height);
                    return -1;
                }
                break;
            case 'W':
                active_dec->max_width = atoi(argv[i+1]);
                if (active_dec->max_width <= 0) {
                    printf("Illegal width (%d)\n", active_dec->max_width);
                    return -1;
                }
                break;
            case 'H':
                active_dec->max_height = atoi(argv[i+1]);
                if (active_dec->max_height <= 0) {
                    printf("Illegal height (%d)\n", active_dec->max_height);
                    return -1;
                }
                break;
            case 's':
                strcpy(conf_info->scaleFilename, argv[i+1]);
                if ((conf_info->scaleFile = fopen(conf_info->scaleFilename, "rb")) == NULL) {
                    printf("Can not open scale golden file %s\n", conf_info->scaleFilename);
                    return -1;
                }
                tmp = atoi(argv[i+2]);
                active_dec->save_scale = (tmp>>3)&1;
                active_dec->scale_type = (tmp>>2)&0x01;
                active_dec->scale_x = (tmp&0x02)?4:2;
                active_dec->scale_y = (tmp&0x01)?4:2;
                i++;
                break;
            case 'p':
                tmp = atoi(argv[i+1]);
                active_dec->interlace = tmp&0x01;
                active_dec->unsupport_b = (tmp>>1)&0x01;
                break;
            case 'b':
                active_dec->max_buffered_num = atoi(argv[i+1]);
                break;
            case 'S':
                conf_info->save_yuv = atoi(argv[i+1]);
                break;
            case 'l':
                conf_info->loop_flg = atoi(argv[i+1]);
                break;
            case 'E':
                conf_info->err_handling = atoi(argv[i+1]);
                break;
            default:
                printf("unknow flag %s\n", argv[i]);
                break;
            }
        }
        else
            printf("unknow flag %s\n", argv[i]);
    }
    if (conf_info->bsFile == NULL) {
        printf("unknow bitstream file\n");
        return -1;
    }
    if (conf_info->confFile == NULL) {
        printf("unknow configure file\n");
        return -1;
    }
    if (active_dec->scale_type > 0 && conf_info->scaleFile == NULL) {
        printf("unknown scale file\n");
        //return -1;
    }
    
    active_dec->ref_buffer_size = PAGE_ALIGN(active_dec->max_width*active_dec->max_height*2);
    if (active_dec->scale_type >= 0)
        active_dec->scale_buffer_size = PAGE_ALIGN(active_dec->max_width*active_dec->max_height/2);
    else
        active_dec->scale_buffer_size = 0;
    active_dec->buffer_size = active_dec->ref_buffer_size + active_dec->scale_buffer_size;
    
    active_dec->dev_name = conf_info->devName;
    
    return 0;
}

int copy_file(char *outname, char *inname)
{
    char tmp[0x100];
    sprintf(tmp, "cp %s %s", inname, outname);
    printf("copy noisy pattern\n");
    system(tmp);
    return 0;        
}

int decode(dec_info *avctx, ConfigInfo *conf_info, FAVC_DEC_FRAME *tFrame, unsigned char *bs_buf, int bs_size)
{
#if USE_FEED_BACK
    FAVC_DEC_RET_BUF tRetBuffer;
#endif
    unsigned char *curr_pic;
    int ret;
    int out_idx;

    tFrame->u32BSBuf_virt = (unsigned int)bs_buf;
    tFrame->u32BSLength = bs_size;
    
    // decode one frame
    ret = ioctl(avctx->h264_dec_fd, H264_IOCTL_DECODE_FRAME, tFrame);
    printf("D");
    fflush(stdout);
    
    avctx->vcache_enable |= tFrame->vcache_enable;
    avctx->multislice |= tFrame->multislice;
    
    /* error handling */
    if(ret < 0){
        if(conf_info->err_handling == 0){ /* ignore H264D_NON_PAIRED_FIELD error */
            if(tFrame->i32ErrorMessage == H264D_NON_PAIRED_FIELD){
                printf("N(%d)", tFrame->i32ErrorMessage);
                ret = 0;
            }
        }else if(conf_info->err_handling == 1){ /* reinit decoder at errors */
            decoder_exit(avctx);
            decoder_init(avctx);
            printf("R(%d)", tFrame->i32ErrorMessage);
            ret = 0;
        }else if(conf_info->err_handling == 2){ /* ignore minor errors */
            int minor_err_flg = 0;
            switch(tFrame->i32ErrorMessage){
                /* list of errors to be ignored */
                case H264D_PARSING_ERROR:
                case H264D_NO_IDR:
                case H264D_ERR_VLD_ERROR:
                case H264D_ERR_BS_EMPTY:
                case H264D_ERR_HW_BS_EMPTY:
                case H264D_ERR_TIMEOUT_EMPTY:
                case H264D_NO_START_CODE:
                    minor_err_flg = 1;
                    break;
                default:
                    break;
            }

            if(minor_err_flg){
                printf("S(%d)", tFrame->i32ErrorMessage);
                ret = 0;
            }
        }
    }

    if (ret < 0) {
        printf("Error to set H264_IOCTL_DECODE_FRAME (%d)\n", tFrame->i32ErrorMessage);
        avctx->show_error_sts = 1;
        avctx->err_flg = 1;
        return 1;
    }
    
#if USE_FEED_BACK
    tRetBuffer.i32FeedbackNum = 0;
#endif

    for (out_idx = 0; out_idx < tFrame->u8OutputPicNum; out_idx++) {
        if (avctx->yuvFile)
        {
            curr_pic = avctx->mmap_addr + avctx->buffer_size * tFrame->mOutputFrame[out_idx].i8BufferIndex;
            fwrite(curr_pic, 1, tFrame->u16FrameWidth * tFrame->u16FrameHeight * 2, avctx->yuvFile);
            fflush(avctx->yuvFile);
            printf("save frame poc %d\n", tFrame->mOutputFrame[out_idx].i16POC);
        }
#ifdef GOLDEN_COMPARE
        if (conf_info->goldenFile)
        {
            if (tFrame->mOutputFrame[out_idx].i8BufferIndex < 0) {
                printf("output list idx (%d) < 0\n", tFrame->mOutputFrame[out_idx].i8BufferIndex);
                tFrame->i32ErrorMessage = OUTPUT_LIST_ERROR;
                return 1;
            }
            curr_pic = avctx->mmap_addr + avctx->buffer_size * tFrame->mOutputFrame[out_idx].i8BufferIndex;
            //printf("compare addr %x (poc %d[%d]) (%d/%d)\n", (unsigned int)curr_pic, tFrame.mOutputFrame[out_idx].i16POC, 
            //  tFrame.mOutputFrame[out_idx].u8Structure, avctx->dec_pass_number, conf_info.actual_frame_num);
            printf("G");
            if (golden_compare(curr_pic, tFrame->u16FrameWidth, tFrame->u16FrameHeight, conf_info->goldenFile, conf_info->patternName, 
                    avctx->golden_buffer, tFrame->mOutputFrame[out_idx].u8Structure, 0, conf_info->check_sum) < 0) {
                tFrame->i32ErrorMessage = COMPARE_ERROR;
                printf("compare reconstructed buffer error: st:%d\n", tFrame->mOutputFrame[out_idx].u8Structure);
                avctx->err_flg = 1;
                return 1;
            }
        }
#endif // GOLDEN_COMPARE

#if USE_FEED_BACK
        tRetBuffer.i32FeedbackList[tRetBuffer.i32FeedbackNum++] = tFrame->mOutputFrame[out_idx].i8BufferIndex;
#endif
        avctx->dec_pass_number++;

#ifdef GOLDEN_COMPARE
        if (conf_info->scaleFile) {
            curr_pic = avctx->mmap_addr + avctx->buffer_size*tFrame->mOutputFrame[out_idx].i8BufferIndex + avctx->ref_buffer_size;
            printf("S");
            //printf("scale addr %x (%d/%d)\n", (unsigned int)curr_pic, avctx->dec_pass_number, conf_info.actual_frame_num);
            if (golden_compare(curr_pic, tFrame->u16FrameWidth/avctx->scale_x, tFrame->u16FrameHeight/avctx->scale_y, conf_info->scaleFile, 
                    conf_info->patternName, avctx->golden_buffer, tFrame->mOutputFrame[out_idx].u8Structure, 1, 0) < 0) {
                tFrame->i32ErrorMessage = COMPARE_ERROR_SCALE;
                printf("compare scaled reconstructed buffer error: st:%d\n", tFrame->mOutputFrame[out_idx].u8Structure);
                avctx->err_flg = 1;
                return 1;
            }
            if (avctx->sFile)
                fwrite(curr_pic, 1, tFrame->u16FrameWidth/avctx->scale_x*tFrame->u16FrameHeight/avctx->scale_y*2, avctx->sFile);
        }
#endif // GOLDEN_COMPARE
        fflush(stdout);
    }

#if USE_FEED_BACK
    ioctl(avctx->h264_dec_fd, H264_IOCTL_DECODE_RELEASE_BUFFER, &tRetBuffer);
    if (tRetBuffer.i32ErrorMessage < 0) {
        printf("release buffer error\n");
        avctx->show_error_sts = 1;
        avctx->err_flg = 1;
        return 1;
    }
#endif
    
    return 0;
}

int decoder_output_all(dec_info *avctx, ConfigInfo *conf_info, FAVC_DEC_FRAME *tFrame)
{
    FAVC_DEC_FRAME tAllFrame;
    int out_idx;
    unsigned char *curr_pic;
#if USE_FEED_BACK
    FAVC_DEC_RET_BUF tRetBuffer;
    tRetBuffer.i32FeedbackNum = 0;
#endif

    ioctl(avctx->h264_dec_fd, H264_IOCTL_DECODE_OUTPUT_ALL, &tAllFrame);

    for (out_idx = 0; out_idx < tAllFrame.u8OutputPicNum; out_idx++) {
        if (avctx->yuvFile)
        {
            curr_pic = avctx->mmap_addr + avctx->buffer_size * tAllFrame.mOutputFrame[out_idx].i8BufferIndex;
            fwrite(curr_pic, 1, tFrame->u16FrameWidth*tFrame->u16FrameHeight*2, avctx->yuvFile);
            fflush(avctx->yuvFile);
            printf("final save frame poc %d\n", tAllFrame.mOutputFrame[out_idx].i16POC);
        }
#ifdef GOLDEN_COMPARE
        if (conf_info->goldenFile) {
            if (tAllFrame.mOutputFrame[out_idx].i8BufferIndex < 0) {
                printf("output list idx (%d) < 0\n", tAllFrame.mOutputFrame[out_idx].i8BufferIndex);
                if (tFrame->i32ErrorMessage >= 0)
                    tFrame->i32ErrorMessage = OUTPUT_LIST_ERROR;
                return 1;
            }
            curr_pic = avctx->mmap_addr + avctx->buffer_size  *tAllFrame.mOutputFrame[out_idx].i8BufferIndex;
            //printf("compare addr %x (poc %d[%d]) (%d/%d)\n", (unsigned int)curr_pic, tAllFrame.mOutputFrame[out_idx].i16POC, 
            //  tAllFrame.mOutputFrame[out_idx].u8Structure, avctx->dec_pass_number, conf_info->actual_frame_num);
            printf("G");
            if (golden_compare(curr_pic, tAllFrame.u16FrameWidth, tAllFrame.u16FrameHeight, conf_info->goldenFile, conf_info->patternName, 
                    avctx->golden_buffer, tAllFrame.mOutputFrame[out_idx].u8Structure, 0, conf_info->check_sum) < 0) {
                printf("compare reconstructed buffer error: st:%d\n", tFrame->mOutputFrame[out_idx].u8Structure);
                if (tFrame->i32ErrorMessage >= 0)
                    tFrame->i32ErrorMessage = COMPARE_ERROR;
                break;
            }
        }
#endif // GOLDEN_COMPARE
#if USE_FEED_BACK
        tRetBuffer.i32FeedbackList[tRetBuffer.i32FeedbackNum++] = tAllFrame.mOutputFrame[out_idx].i8BufferIndex;
#endif
        avctx->dec_pass_number++;
    
#ifdef GOLDEN_COMPARE        
        if (conf_info->scaleFile) {
            curr_pic = avctx->mmap_addr + avctx->buffer_size*tAllFrame.mOutputFrame[out_idx].i8BufferIndex + avctx->ref_buffer_size;
            //printf("scale addr %x (%d/%d)\n", (unsigned int)curr_pic, avctx->dec_pass_number, conf_info->actual_frame_num);
            printf("S");
            if (golden_compare(curr_pic, tAllFrame.u16FrameWidth/avctx->scale_x, tAllFrame.u16FrameHeight/avctx->scale_y, conf_info->scaleFile, 
                    conf_info->patternName, avctx->golden_buffer, tAllFrame.mOutputFrame[out_idx].u8Structure, 1, 0) < 0) {
                printf("compare scaled reconstructed buffer error: st:%d\n", tFrame->mOutputFrame[out_idx].u8Structure);
                tFrame->i32ErrorMessage = COMPARE_ERROR_SCALE;
                break;
            }
            if (avctx->sFile)
                fwrite(curr_pic, 1, tFrame->u16FrameWidth/avctx->scale_x*tFrame->u16FrameHeight/avctx->scale_y*2, avctx->sFile);
        }
#endif // GOLDEN_COMPARE
    }

#if USE_FEED_BACK
    ioctl(avctx->h264_dec_fd, H264_IOCTL_DECODE_RELEASE_BUFFER, &tRetBuffer);
    if (tRetBuffer.i32ErrorMessage < 0) {
        avctx->show_error_sts = 1;
        printf("release buffer error %d\n", tRetBuffer.i32ErrorMessage);
        return 1;
    }
#endif
    
    return 0;
}

void write_test_result(dec_info *avctx, ConfigInfo *conf_info, FAVC_DEC_FRAME *tFrame)
{
#ifdef GOLDEN_COMPARE
    if (conf_info->goldenFile) {
        if (avctx->dec_pass_number == conf_info->actual_frame_num) {
			printf(": passed\n");
            printf("%s: pass %d/%d\n", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num);
        }
        else {
			printf(": falied\n");
			avctx->err_flg = 1;
            printf("%s: fail %d/%d\t%d\n", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num, tFrame->i32ErrorMessage);
        }
        if ((conf_info->resFile = fopen(conf_info->resultFilename, "a")) != NULL) {
            if (avctx->dec_pass_number == conf_info->actual_frame_num) {
                fprintf(conf_info->resFile, "%s\tpass %d/%d", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num);
                if (avctx->vcache_enable)
                    fprintf(conf_info->resFile, " <vcache enable>");
                if (avctx->multislice) {
                    if (avctx->multislice & 0x10)
                        fprintf(conf_info->resFile, " =>multislice");
                    else
                        fprintf(conf_info->resFile, " =>multislice [mb row]");
                }
                fprintf(conf_info->resFile, "\n");
            }
            else {
                fprintf(conf_info->resFile, "%s\tfail %d/%d\t%d", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num, tFrame->i32ErrorMessage);
                if (avctx->vcache_enable)
                    fprintf(conf_info->resFile, " <vcache enable>");
                if (avctx->multislice) {
                    if (avctx->multislice & 0x10)
                        fprintf(conf_info->resFile, " =>multislice");
                    else
                        fprintf(conf_info->resFile, " =>multislice [mb row]");
                }
                fprintf(conf_info->resFile, "\n");
                if (avctx->show_error_sts)
                    fprintf(conf_info->resFile, "\tslice %c: sts %08x\n", tFrame->current_slice_type==0?'n':tFrame->current_slice_type, tFrame->error_sts);
            }
        }
    }
    else
#endif // GOLDEN_COMPARE
    {
        if (tFrame->i32ErrorMessage>=0){
            printf("%s: pass %d/%d\n", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num);
        } else {
            printf("%s: fail %d/%d(%d)\n", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num, tFrame->i32ErrorMessage);
            avctx->err_flg = 1;
        }
        if ((conf_info->resFile = fopen(conf_info->resultFilename, "a")) != NULL) {
            if (tFrame->i32ErrorMessage<0)
                fprintf(conf_info->resFile, "%s\tfail %d/%d(%d)\n", conf_info->patternName, avctx->curr_fr, tFrame->i32ErrorMessage, conf_info->actual_frame_num);
            else
                fprintf(conf_info->resFile, "%s: pass %d/%d\n", conf_info->patternName, avctx->dec_pass_number, conf_info->actual_frame_num);
        }
        else
            printf("can't open %s\n", conf_info->resultFilename);
    }

}


int main(int argc, char *argv[])
{
    FAVC_DEC_FRAME tFrame;
    dec_info active_dec;
    ConfigInfo conf_info;

    int len;
    int max_bs_size, bs_size, frame_num = 0;
    unsigned char *bs_buf = NULL;
    int round = 0;

    memset(&active_dec, 0, sizeof(active_dec));
    memset(&conf_info, 0, sizeof(ConfigInfo));
    memset(&tFrame, 0, sizeof(FAVC_DEC_FRAME));

    if (parsing_argv(argc, argv, &conf_info, &active_dec) < 0) {
        configure();
        active_dec.err_flg = 1;
        goto dec_exit2;
    }


    if (decoder_init(&active_dec) < 0) {
        printf("init decoder fail\n");
        active_dec.err_flg = 1;
        goto dec_exit2;
    }


    /* allocate bitstream buffer */
    max_bs_size = active_dec.max_width * active_dec.max_height;
    //max_bs_size += (max_bs_size / 4) * 3;
    max_bs_size += max_bs_size;
    printf("max_bs_size:%d\n", max_bs_size);
    if ((bs_buf = (unsigned char*)malloc(max_bs_size)) == NULL) {
        printf("allocate bitstream buffer error\n");
        active_dec.err_flg = 1;
        goto dec_exit2;
    }

    
#ifdef GOLDEN_COMPARE
    /* allocate one frame buffer for golden compare */
    if (conf_info.goldenFile || conf_info.scaleFile) {
        if ((active_dec.golden_buffer = (unsigned char *)malloc(active_dec.max_width*active_dec.max_height*2)) == NULL) {
            printf("allocate golden buffer error\n");
            active_dec.err_flg = 1;
            goto dec_exit2;
        }
    }            
#endif

    /* open file for saving scaled output frame */
    if (conf_info.scaleFile && active_dec.save_scale) {
        char temp_name[100];
        sprintf(temp_name, "scale_yuv_%s.yuv", conf_info.patternName);
        if ((active_dec.sFile = fopen(temp_name, "wb")) == NULL)
            printf("can not open output scale file\n");
        printf("save scale result %s\n", temp_name);
    }

    /* open file for saving output frame */
    if(conf_info.save_yuv){
        active_dec.yuvFile = fopen("test.yuv", "wb");
        printf("save yuv result file test.yuv\n");
    }
    tFrame.i32ErrorMessage = 0;
    
    printf("decode one frame: %s:", conf_info.patternName);
#ifdef GOLDEN_COMPARE
    if (conf_info.check_sum)
        printf(" [golden compare check sum]");
    else
        printf(" [golden compare]");
#endif
    printf("\n");
    fflush(stdout);   

loop_forever:
    if(conf_info.loop_flg)
        printf("running forever: round %d\n", round++);

    /* get frame number from the first line of conf file */
    fscanf(conf_info.confFile, "%d", &frame_num);
    frame_num = conf_info.actual_frame_num; /* use argument to overwrite the frame number from conf */
    printf("frame number = %d / %d\n", frame_num, conf_info.actual_frame_num);
    fflush(stdout);
    //frame_num = iMin(30, frame_num);

    /* main decode loop */
    for (active_dec.curr_fr = 0; active_dec.curr_fr < frame_num; active_dec.curr_fr++){
        
        fscanf(conf_info.confFile, "%d", &bs_size);
        //printf("bs length %d\n", bs_size);
        
        if(bs_size > max_bs_size){
            printf("bitstream length > max_bs_size (%d/%d)\n", bs_size, max_bs_size);
            active_dec.err_flg = 1;
            goto dec_exit;
        }
        len = fread((void *)bs_buf, 1, bs_size, conf_info.bsFile);
        
        if (len != bs_size) {
            printf("Load bitstream length uncorrectly (%d/%d)\n", bs_size, len);
            active_dec.err_flg = 1;
            goto dec_exit;
        }

        if(decode(&active_dec, &conf_info, &tFrame, bs_buf, bs_size)){
            goto dec_exit1;
        }
    }
    
dec_exit:
    if(decoder_output_all(&active_dec, &conf_info, &tFrame)){
        goto dec_exit1;
    }

    if(conf_info.loop_flg){
        fseek(conf_info.bsFile, 0L, SEEK_SET);
        if(conf_info.goldenFile)
            fseek(conf_info.goldenFile, 0L, SEEK_SET);
        fseek(conf_info.confFile, 0L, SEEK_SET);
        goto loop_forever;
    }

dec_exit1:
    write_test_result(&active_dec, &conf_info, &tFrame);
    
dec_exit2:
    if (active_dec.h264_dec_fd)
        decoder_exit(&active_dec);
    
    if (bs_buf)                     free(bs_buf);
    if (conf_info.bsFile)           fclose(conf_info.bsFile);
#ifdef GOLDEN_COMPARE
    if (active_dec.golden_buffer)   free(active_dec.golden_buffer);
#endif // GOLDEN_COMPARE
    if (active_dec.sFile)           fclose(active_dec.sFile);
    if (active_dec.yuvFile)         fclose(active_dec.yuvFile);
    if (conf_info.goldenFile)       fclose(conf_info.goldenFile);
    if (conf_info.confFile)         fclose(conf_info.confFile);
    if (conf_info.resFile)          fclose(conf_info.resFile);
    if (conf_info.scaleFile)        fclose(conf_info.scaleFile);

    printf("decode end %s\n", conf_info.patternName);
    if(active_dec.err_flg)
        return 1;
    return 0;
}
