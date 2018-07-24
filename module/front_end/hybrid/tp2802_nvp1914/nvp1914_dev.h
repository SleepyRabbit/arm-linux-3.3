#ifndef _NVP1914_DEV_H_
#define _NVP1914_DEV_H_

struct nvp1914_dev_t {
    int                 index;      ///< device index
    char                name[16];   ///< device name
    struct semaphore    lock;       ///< device locker
    struct miscdevice   miscdev;    ///< device node for ioctl access

    int                 (*vlos_notify)(int id, int vin, int vlos);
};

struct nvp1914_params_t {
    int nvp1914_dev_num;
    ushort* nvp1914_iaddr;
    int* nvp1914_vmode;
    int nvp1914_sample_rate;
    int nvp1914_sample_size;
    struct nvp1914_dev_t* nvp1914_dev;
    int init;
    u32* TP2802_NVP1914_VCH_MAP;
};
typedef struct nvp1914_params_t* nvp1914_params_p;

int  nvp1914_proc_init(int);
void nvp1914_proc_remove(int);
int  nvp1914_miscdev_init(int);
int  nvp1914_device_init(int);
int  nvp1914_sound_switch(int, int, bool);
void nvp1914_set_params(nvp1914_params_p);

#endif  /* _NVP1914_DEV_H_ */
