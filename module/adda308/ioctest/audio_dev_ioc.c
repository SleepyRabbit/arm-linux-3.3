/*
 * AUDIO DEV IOCTL Test Program
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

#include "include/adda308/audio_dev_ioctl.h"

#define USE_SHARED_LIB

const char *dev_name = NULL;

static void audio_dev_ioc_help_msg(void)
{
    fprintf(stdout, "\nUsage: audio_dev_ioc <dev_node> [-v]"
                    "\n       <dev_node>    : audio device node, ex: /dev/audiodev.0"
                    "\n       -v [data]     : get/set volume"
                    "\n\n");
}

int API_set_speaker(int volume)
{
	int fd  = 0;
	int err = 0;
	
	struct audio_dev_ioc_vol vol_data;
	vol_data.data = volume;

	fd = open(dev_name, O_RDONLY);
    if (fd ==  -1) {
        fprintf(stderr, "open %s failed(%s)\n", dev_name, strerror(errno));
        err = -1;
		goto exit;
    }

	if (ioctl(fd, AUDIO_DEV_SET_SPEAKER_VOL, &vol_data.data) < 0) {
		fprintf(stderr, "AUDIO_DEV_SET_SPEAKER_VOL ioctl failed(%s)\n", strerror(errno));
        err = -1;
		goto exit;
	}
		

exit:
	if(fd)
		close(fd);
	
	return err;
}

int API_get_speaker(int *volume)
{
	int fd  = 0;
	int err = 0;
	struct audio_dev_ioc_vol vol_data;
	
	fd = open(dev_name, O_RDONLY);
    if (fd ==  -1) {
        fprintf(stderr, "open %s failed(%s)\n", dev_name, strerror(errno));
        err = -1;
		goto exit;
    }

	if (ioctl(fd, AUDIO_DEV_GET_SPEAKER_VOL, &vol_data.data) < 0) {
		fprintf(stderr, "AUDIO_DEV_GET_SPEAKER_VOL ioctl failed(%s)\n", strerror(errno));
		err = -1;
		goto exit;
	}

	*volume = vol_data.data;

exit:
	if(fd)
		close(fd);
	
	return err;
}

int main(int argc, char **argv)
{
    int opt, retval, tmp;
    int err = 0;
    int fd = 0;
	
    if(argc < 3) {
        audio_dev_ioc_help_msg();
        return -EPERM;
    }

    /* get device name */
    dev_name = argv[1];

	while((opt = getopt(argc, argv, "mvon:b:c:s:h:p:")) != -1 ) {
        switch(opt) {

            case 'v':   /* Get/Set Volume */
                if (optind == argc) {
                    retval = API_get_speaker(&tmp);
                    if (retval < 0) {
                        fprintf(stderr, "AUDIO_DEV_GET_VOL...error\n");
                        err = -EINVAL;
                        goto exit;
                    }
                    fprintf(stdout, "VOL: %d\n\n", tmp);
                }
                else {
                    tmp = strtol(argv[optind+0], NULL, 0);
                    retval = API_set_speaker(tmp);
                    if (retval < 0) {
                        fprintf(stderr, "AUDIO_DEV_SET_VOL...error\n");
                        err = -EINVAL;
                        goto exit;
                    }
                }
                break;

            default:
                audio_dev_ioc_help_msg();
                goto exit;
                break;
        }
    }

exit:

    return err;
}

