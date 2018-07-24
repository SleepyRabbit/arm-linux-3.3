#ifndef _NVP6114_DEV_H_
#define _NVP6114_DEV_H_

struct nvp6114_dev_t {
    int                 index;      ///< device index
    char                name[16];   ///< device name
    struct semaphore    lock;       ///< device locker
    struct miscdevice   miscdev;    ///< device node for ioctl access

    int                 (*vlos_notify)(int id, int vin, int vlos);
    int                 (*vfmt_notify)(int id, int vmode);
};

struct nvp6114_params_t {
    int nvp6114_dev_num;
    ushort* nvp6114_iaddr;
    int* nvp6114_vmode;
    int nvp6114_sample_rate;
    int nvp6114_sample_size;
    int nvp6114_audio_chnum;
    struct nvp6114_dev_t* nvp6114_dev;
    int* nvp6114_ch_map;
    int init;
    u32* TP2822_NVP6114_VCH_MAP;
};
typedef struct nvp6114_params_t* nvp6114_params_p;

int  nvp6114_proc_init(int);
void nvp6114_proc_remove(int);
int  nvp6114_miscdev_init(int);
int  nvp6114_device_init(int);
void nvp6114_set_params(nvp6114_params_p);

#endif  /* _NVP6114_DEV_H_ */

