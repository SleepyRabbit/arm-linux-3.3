/*
 * TP2823 IOCTL Test Program
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "techpoint/tp2823.h"

static void tp2823_ioc_help_msg(void)
{
    fprintf(stdout, "\nUsage: tp2823_ioc <dev_node> [-bchpsv]"
                    "\n       <dev_node>    : tp2823 device node, ex: /dev/tp2823.0"
                    "\n       -v [data]     : get/set volume"
                    "\n\n");
}

int main(int argc, char **argv)
{
    int opt, retval, tmp;
    int err = 0;
    int fd = 0;
    const char *dev_name = NULL;
    struct tp2823_ioc_data_t ioc_data;

    if(argc < 3) {
        tp2823_ioc_help_msg();
        return -EPERM;
    }

    /* get device name */
    dev_name = argv[1];

    /* open rtc device node */
    fd = open(dev_name, O_RDONLY);
    if (fd ==  -1) {
        fprintf(stderr, "open %s failed(%s)\n", dev_name, strerror(errno));
        err = errno;
        goto exit;
    }

    while((opt = getopt(argc, argv, "mvon:b:c:s:h:p:")) != -1 ) {
        switch(opt) {                           
            case 'v':   /* Get/Set Volume */
                if (optind == argc) {
                    retval = ioctl(fd, TP2823_GET_VOL, &tmp);
                    if (retval < 0) {
                        fprintf(stderr, "TP2823_GET_VOL ioctl...error\n");
                        err = -EINVAL;
                        goto exit;
                    }
                    fprintf(stdout, "VOL: %d\n\n", tmp);
                }
                else {
                    tmp = strtol(argv[optind+0], NULL, 0);
                    retval = ioctl(fd, TP2823_SET_VOL, &tmp);
                    if (retval < 0) {
                        fprintf(stderr, "TP2823_SET_VOL ioctl...error\n");
                        err = -EINVAL;
                        goto exit;
                    }
                }
                break;

            default:
                tp2823_ioc_help_msg();
                goto exit;
                break;
        }
    }

exit:
    if(fd)
        close(fd);

    return err;
}
