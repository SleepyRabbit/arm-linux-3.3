/*************************************************************************************
 *  audio device ioctl
 *************************************************************************************/
struct audio_dev_ioc_vol {
    int data;       	//read/write data value
};
	
#define DEVICE_NAME "audiodev"

#define AUDIO_DEV_IOC_MAGIC       'a'

#define AUDIO_DEV_GET_SPEAKER_VOL    _IOR(AUDIO_DEV_IOC_MAGIC,   1, struct audio_dev_ioc_vol)
#define AUDIO_DEV_SET_SPEAKER_VOL    _IOWR(AUDIO_DEV_IOC_MAGIC,  2, struct audio_dev_ioc_vol)

