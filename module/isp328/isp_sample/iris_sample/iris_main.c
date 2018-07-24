
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


int main(int argc, char *argv[])
{
    int ret, option, tmp1, tmp2, tmp3;

    // open MCU device
    isp_fd = open("/dev/isp320", O_RDWR);
    if(isp_fd < 0){
        printf("Open ISP fail!\n");
        return -1;
    }


    while (1){
        printf("--------Select AE mode--------\n");

        // set AE mode
        ret = ioctl(isp_fd, AE_IOC_GET_MODE, &tmp1);
        if(ret < 0){
            printf("AE_IOC_GET_MODE fail!\n");
            goto _end;
        }
        if(tmp1 == 0){
            printf("Current AE mode is AUTO.\n");
        }else if(tmp1 == 1){
            printf("Current AE mode is APERTURE_PRIORITY.\n");
        }else if(tmp1 == 2){
            printf("Current AE mode is SHUTTER_PRIORITY.\n");
        }else{
            printf("Current AE mode is unknown=%d!\n", tmp1);
        }

        do{
            printf("Set AE mode (0:AUTO / 1:APERTURE_PRIORITY / 2:SHUTTER_PRIORITY) >> ");
            option = get_choose_int();

            if((option < 0) || ((option > 2) && (option != 99))){
                printf("Invalid AE mode\n");
            }
        }while((option < 0) || ((option > 2) && (option != 99)));

        if(option == 99){
            goto _BKEAK_LOOP;
        }


        // set min_exp & max_exp
        do{
            ret = ioctl(isp_fd, AE_IOC_GET_MIN_EXP, &tmp1);
            if(ret < 0){
                printf("AE_IOC_GET_MIN_EXP fail!\n");
                break;
            }

            ret = ioctl(isp_fd, AE_IOC_GET_MAX_EXP, &tmp2);
            if(ret < 0){
                printf("AE_IOC_GET_MAX_EXP fail!\n");
                break;
            }
            printf("min_exp = %d, max_exp = %d\n", tmp1, tmp2);

            do{
                printf("new min_exp (1~5000) >> ");
                tmp1 = get_choose_int();

                if((tmp1 <= 0) || (tmp1 > 5000)){
                    printf("Please enter reasonable value again\n");
                }
            }while((tmp1 <= 0) || (tmp1 > 5000));

            do{
                printf("new max_exp (1~5000) >> ");
                tmp2 = get_choose_int();

                if((tmp2 <= 0) || (tmp2 > 5000)){
                    printf("Please enter reasonable value again\n");
                }
            }while ((tmp2 <= 0) || (tmp2 > 5000));

            if(tmp2 >= tmp1){
                ioctl(isp_fd, AE_IOC_SET_MIN_EXP, &tmp1);
                ioctl(isp_fd, AE_IOC_SET_MAX_EXP, &tmp2);
            }else{
                printf("error: min_exp > max_exp!\n");
            }
        }while (tmp2 < tmp1);


        switch(option){
            case 0:    // AUTO: adjust exp & gain (fully open iris) at lower illumination / adjust iris at high illumination
                // set iris_min_exp (>= min_exp)
                ret = ioctl(isp_fd, AE_IOC_GET_IRIS_MIN_EXP, &tmp3);
                if(ret < 0){
                    printf("AE_IOC_GET_IRIS_MIN_EXP fail!\n");
                    break;
                }
                printf("iris_min_exp = %d\n", tmp3);

                do{
                    printf("new iris_min_exp (0~5000) >> ");
                    tmp3 = get_choose_int();

                    if((tmp3 < 0) || (tmp3 > 5000)){
                        printf("Please enter reasonable value again\n");
                    }else if((tmp3 != 0) && (tmp3 < tmp1)){
                        printf("error: iris_min_exp < min_exp\n");
                    }
                }while ((tmp3 < 0) || (tmp3 > 5000) || ((tmp3 != 0) && (tmp3 < tmp1)));

                ioctl(isp_fd, AE_IOC_SET_IRIS_MIN_EXP, &tmp3);
                break;

            case 1:    // APERTURE_PRIORITY: open iris fully, adjust exp(min_exp~max_exp) & gain(min_gain~max_gain) (may be too bright even min_exp & min_gain)
                break;

            case 2:    // SHUTTER_PRIORITY: fix exp & gain to const_exp(min_exp~max_exp) and const_gain(min_gain~max_gain) respectively, adjust iris (may be too dark even max iris)
                // set const_exp
                ret = ioctl(isp_fd, AE_IOC_GET_CONST_EXP, &tmp2);
                if(ret < 0){
                    printf("AE_IOC_GET_CONST_EXP fail!\n");
                    break;
                }
                printf("const_exp = %d\n", tmp2);

                do{
                    printf("new const_exp (1~5000) >> ");
                    tmp2 = get_choose_int();

                    if((tmp2 <= 0) || (tmp2 > 5000)){
                        printf("Please enter reasonable value again\n");
                    }
                }while((tmp2 <= 0) || (tmp2 > 5000));

                ioctl(isp_fd, AE_IOC_SET_CONST_EXP, &tmp2);

                // set const_gain
                ret = ioctl(isp_fd, AE_IOC_GET_CONST_GAIN, &tmp2);
                if(ret < 0){
                    printf("AE_IOC_GET_CONST_GAIN fail!\n");
                    break;
                }
                printf("const_gain = %d\n", tmp2);

                do{
                    printf("new const_gain (0x40[1X]~0x1000[16X*4X]) >> ");
                    tmp2 = get_choose_int();

                    if((tmp2 < 0x40) || (tmp2 > 0x1000)){
                        printf("Please enter reasonable value again\n");
                    }
                }while((tmp2 < 0x40) || (tmp2 > 0x1000));

                ioctl(isp_fd, AE_IOC_SET_CONST_GAIN, &tmp2);
                break;

            default:
                break;
        }


        // set min_gain & max_gain
        do{
            ret = ioctl(isp_fd, AE_IOC_GET_MIN_GAIN, &tmp1);
            if(ret < 0){
                printf("AE_IOC_GET_MIN_GAIN fail!\n");
                break;
            }

            ret = ioctl(isp_fd, AE_IOC_GET_MAX_GAIN, &tmp2);
            if(ret < 0){
                printf("AE_IOC_GET_MAX_GAIN fail!\n");
                break;
            }
            printf("min_gain = %d, max_gain = %d\n", tmp1, tmp2);

            do{
                printf("new min_gain (0x40[1X]~0x1000[16X*4X]) >> ");
                tmp1 = get_choose_int();

                if((tmp1 < 0x40) || (tmp1 > 0x1000)){
                    printf("Please enter reasonable value again\n");
                }
            }while((tmp1 < 0x40) || (tmp1 > 0x1000));

            do{
                printf("new max_gain (0x40[1X]~0x1000[16X*4X]) >> ");
                tmp2 = get_choose_int();

                if((tmp2 < 0x40) || (tmp2 > 0x1000)){
                    printf("Please enter reasonable value again\n");
                }
            }while ((tmp2 < 0x40) || (tmp2 > 0x1000));

            if(tmp2 >= tmp1){
                ioctl(isp_fd, AE_IOC_SET_MIN_GAIN, &tmp1);
                ioctl(isp_fd, AE_IOC_SET_MAX_GAIN, &tmp2);
            } else {
                printf("error: min_gain > max_gain!\n");
            }
        }while (tmp2 < tmp1);


_BKEAK_LOOP:
        if((option >= 0) && (option <= 2)){
            ioctl(isp_fd, AE_IOC_SET_MODE, &option);
        }else if(option == 99){
            break;
        }
    }


_end:
    close(isp_fd);

    return 0;
}

