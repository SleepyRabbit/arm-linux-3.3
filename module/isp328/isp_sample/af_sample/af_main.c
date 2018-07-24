
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "ioctl_isp328.h"

//============================================================================
// global
//============================================================================
static int isp_fd;
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))


static int cal_f_value(af_sta_t *af_sta)
{
        /* 3x3 block array
        |-----------|
        | 0 | 1 | 2 |
        |---|---|---|
        | 3 | 4 | 5 |
        |---|---|---|
        | 6 | 7 | 8 |
        |-----------| */
    return af_sta->af_result_w[4];
}

static int get_choose_int(void)
{
    char buf[256];
    int val, error;

    error = scanf("%d", &val);

    if(error != 1){
        printf("Invalid option. Try again.\n");
        clearerr(stdin);
        fgets(buf, sizeof(buf), stdin);
        val = -1;
    }

    return val;
}

static int af_set_enable(void)
{
    int ret, af_en;

    ret = ioctl(isp_fd, AF_IOC_GET_ENABLE, &af_en);
    if(ret < 0){
        return ret;
    }
    printf("AF: %s\n", (af_en ? "ENABLE" : "DISABLE"));

    do{
        printf("AF (0:Disable, 1:Enable) >> ");
        af_en = get_choose_int();
    }while (af_en < 0);

    return ioctl(isp_fd, AF_IOC_SET_ENABLE, &af_en);
}

static int af_set_mode(void)
{
    int ret, af_mode;

    ret = ioctl(isp_fd, AF_IOC_GET_SHOT_MODE, &af_mode);
    if(ret < 0){
        return ret;
    }
    printf("AF mode: %s\n", (af_mode ? "Continous" : "One Shot"));

    do{
        printf("AF mode (0:One Shot, 1:Continous) >> ");
        af_mode = get_choose_int();
    }while (af_mode < 0);

    return ioctl(isp_fd, AF_IOC_SET_SHOT_MODE, &af_mode);
}

static int af_set_method(void)
{
    int ret, af_method;

    ret = ioctl(isp_fd, AF_IOC_GET_AF_METHOD, &af_method);
    if(ret < 0){
        printf("AF_IOC_GET_AF_METHOD fail!\n");
        return ret;
    }
    printf("AF method: %d\n", af_method);

    do{
        printf("AF method (0:Step, 1:Poly, 2: test motor, 3: global search >> ");
        af_method = get_choose_int();
    }while (af_method < 0);

    return ioctl(isp_fd, AF_IOC_SET_AF_METHOD, &af_method);
}

static int af_re_trigger(void)
{
    int ret, busy=1, timeout=0;

    ret = ioctl(isp_fd, AF_IOC_SET_RE_TRIGGER, NULL);
    if(ret < 0){
        return ret;
    }
    printf("AF Re-Trigger\n");
    printf("Wait for converge ...\n");

    do{
        printf(".");
        ioctl(isp_fd, AF_IOC_CHECK_BUSY, &busy);
        sleep(1);
    }while (busy == 1 && ++timeout < 20);

    if(timeout >= 20){
        printf("\nAF timeout!\n");
    }else{
        printf("\nAF converged\n");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret, option;
    int min_pos, max_pos, curr_pos, tar_pos, tar_step, check_busy;
    int peak_fv, peak_pos;
    unsigned int FV;
    focus_range_t focus_range;
	zoom_info_t zoom_info;
    af_sta_t af_sta;

    // open MCU device
    //TODO: revise device name
    isp_fd = open("/dev/isp328", O_RDWR);
    if(isp_fd < 0){
        printf("Open ISP fail!\n");
        return -1;
    }

    // get focus position range
    ret = ioctl(isp_fd, LENS_IOC_GET_FOCUS_RANGE, &focus_range);
    if(ret < 0){
        printf("AF_GET_FOCUS_RANGE fail!");
        goto _end;
    }
    min_pos = focus_range.min;
    max_pos = focus_range.max;
    printf("AF range: %d, %d\n",min_pos, max_pos);

    while (1){
            printf("----------------------------------------\n");
            printf(" 1. Zoom get info\n");
            printf(" 2. Focus pos\n");
            printf(" 3. Zoom pos\n");
            printf(" 4. Zoom step\n");
            printf(" 5. Focus home\n");
            printf(" 6. Zoom home\n");
            printf(" 7. Check Busy\n");
            printf(" 8. Get FV\n");
            printf(" 9. AF enable\n");
            printf(" 10. AF re-triger\n");
            printf(" 11. AF set mode\n");
            printf(" 12. AF set method\n");
            printf(" 0. Quit\n");
            printf("----------------------------------------\n");

        do{
            printf(">> ");
            option = get_choose_int();
        }while (option < 0);

        switch (option){
            case 1:
                ret = ioctl(isp_fd, LENS_IOC_GET_ZOOM_INFO, &zoom_info);
                if(ret < 0){
                    printf("LENS_IOC_GET_ZOOM_INFO fail!");
                    goto _end;
                }
                printf("Zoom stage count: %d, current focal length(x10): %d\n", zoom_info.zoom_stage_cnt, zoom_info.curr_zoom_x10);
                break;

            case 2:
                // get focus position range
                ret = ioctl(isp_fd, LENS_IOC_GET_FOCUS_RANGE, &focus_range);
                if(ret < 0){
                    printf("AF_GET_FOCUS_RANGE fail!");
                    goto _end;
                }
                min_pos = focus_range.min;
                max_pos = focus_range.max;
                printf("AF range: %d, %d\n", min_pos, max_pos);

                ret = ioctl(isp_fd, LENS_IOC_GET_FOCUS_POS, &curr_pos);
                if(ret < 0){
                    printf("LENS_IOC_GET_FOCUS_POS fail!");
                    break;
                }
                printf("current focus pos = %d\n", curr_pos);

                do{
                    printf("new focus pos >> ");
                    tar_pos = get_choose_int();
                }while (0);

                ioctl(isp_fd, LENS_IOC_SET_FOCUS_POS, &tar_pos);
                break;

            case 3:
                ret = ioctl(isp_fd, LENS_IOC_GET_ZOOM_POS, &curr_pos);
                if(ret < 0){
                    printf("LENS_IOC_GET_ZOOM_POS fail!");
                    break;
                }
                printf("current zoom pos = %d\n", curr_pos);

                do{
                    printf("new zoom pos (0~100) >> ");
                    tar_pos = get_choose_int();
                }while (0);

                if ((tar_pos >= 0) && (tar_pos <= 100)) {
                    ioctl(isp_fd, LENS_IOC_SET_ZOOM_POS, &tar_pos);
                    sleep(1);
                    af_re_trigger();
                } else {
                    printf("out of setting range!\n");
                }
                break;

            case 4:
                ret = ioctl(isp_fd, LENS_IOC_GET_ZOOM_POS, &curr_pos);
                if(ret < 0){
                    printf("LENS_IOC_GET_ZOOM_POS fail!");
                    break;
                }
                printf("current zoom pos = %d\n", curr_pos);

                do{
                    printf("set zoom step >> ");
                    tar_step = get_choose_int();
                }while (0);

                if ((tar_step >= -100) && (tar_step <= 100)) {
                    ioctl(isp_fd, LENS_IOC_SET_ZOOM_MOVE, &tar_step);
                    sleep(1);
                    af_re_trigger();
                } else {
                    printf("out of setting range!\n");
                }
                break;

            case 5:
                ioctl(isp_fd, LENS_IOC_SET_FOCUS_INIT, NULL);
                break;

            case 6:
                ioctl(isp_fd, LENS_IOC_SET_ZOOM_INIT, NULL);
                break;

            case 7:
                ret = ioctl(isp_fd, LENS_IOC_GET_CHECK_BUSY, &check_busy);
                if(ret < 0){
                    printf("LENS_IOC_GET_CHECK_BUSY fail!\n");
                    break;
                }
                if(check_busy){
                    printf("LENS_is busy\n");
                }else{
                    printf("LENS_is ready\n");
                }
                break;

            case 8:
                ret = ioctl(isp_fd, ISP_IOC_GET_AF_STA, &af_sta);
                if(ret < 0){
                    printf("ISP_IOC_GET_AF_STA fail!");
                    break;
                }
                FV = cal_f_value(&af_sta);
                if(FV >= peak_fv){
                    peak_fv = FV;
                    peak_pos = curr_pos;
                }

                ret = ioctl(isp_fd, LENS_IOC_GET_FOCUS_POS, &curr_pos);
                if(ret < 0){
                    printf("LENS_IOC_GET_FOCUS_POS fail!");
                    break;
                }
                printf("(pos, FV)= (%d,%d)\n", curr_pos, FV);
                break;

            case 9:
                af_set_enable();
                break;

            case 10:
                af_re_trigger();
                break;

            case 11:
                af_set_mode();
                break;

            case 12:
                af_set_method();
                break;

            case 0:
            default:
                return 0;
            }
        }

_end:
    close(isp_fd);

    return 0;
}

