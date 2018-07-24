#ifndef _TP2822_DEV_H_
#define _TP2822_DEV_H_

struct tp2822_dev_t {
    int                     index;                          ///< device index
    char                    name[16];                       ///< device name
    struct semaphore        lock;                           ///< device locker
    struct miscdevice       miscdev;                        ///< device node for ioctl access

    int                     pre_plugin[TP2822_DEV_CH_MAX];  ///< device channel previous cable plugin status
    int                     cur_plugin[TP2822_DEV_CH_MAX];  ///< device channel current  cable plugin status

    int                     (*vfmt_notify)(int id, int ch, struct tp2822_video_fmt_t *vfmt);
    int                     (*vlos_notify)(int id, int ch, int vlos);
};

struct tp2822_params_t {
    int tp2822_dev_num;
    ushort* tp2822_iaddr;
    int* tp2822_vout_format;
    int* tp2822_vout_mode;
    int tp2822_sample_rate;
    struct tp2822_dev_t* tp2822_dev;
    int init;
    u32* TP2822_NVP6114_VCH_MAP;
};
typedef struct tp2822_params_t* tp2822_params_p;

int  tp2822_proc_init(int);
void tp2822_proc_remove(int);
int  tp2822_miscdev_init(int);
int  tp2822_device_init(int);
void tp2822_set_params(tp2822_params_p);

#endif  /* _TP2822_DEV_H_ */

