#include <linux/module.h>
#include <linux/kernel.h>
#include "osd_api.h"
#include "scaler_trans.h"

#define MAX_VCH_NUM             128      // max virtual channel number 0~32
int max_vch_num = MAX_VCH_NUM;
module_param(max_vch_num, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(max_vch_num, "max virtual channel number");

/* Replace fscaler300.ko */
int scl_osd_set_char(int fd, osd_char_t *osd_char)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_char);

int scl_osd_set_disp_string(int fd, u32 start, u16 *font_data, u16 font_num)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_disp_string);

int scl_osd_set_smooth(int fd, osd_smooth_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_smooth);

int scl_osd_get_smooth(int fd, osd_smooth_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_smooth);

int scl_osd_win_enable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_win_enable);

int scl_osd_win_disable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_win_disable);

int scl_osd_set_win_param(int fd, int win_idx, osd_win_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_win_param);

int scl_osd_get_win_param(int fd, int win_idx, osd_win_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_win_param);

int scl_osd_set_font_zoom(int fd, int win_idx, osd_font_zoom_t zoom)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_font_zoom);

int scl_osd_get_font_zoom(int fd, int win_idx, osd_font_zoom_t *zoom)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_font_zoom);

int scl_osd_set_color(int fd, int win_idx, osd_color_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_color);

int scl_osd_get_color(int fd, int win_idx, osd_color_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_color);

int scl_osd_set_alpha(int fd, int win_idx, osd_alpha_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_alpha);

int scl_osd_get_alpha(int fd, int win_idx, osd_alpha_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_alpha);

int scl_osd_set_border_enable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_border_enable);

int scl_osd_set_border_disable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_border_disable);

int scl_osd_set_border_param(int fd, int win_idx, osd_border_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_set_border_param);

int scl_osd_get_border_param(int fd, int win_idx, osd_border_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_osd_get_border_param);

int scl_mask_win_enable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_win_enable);

int scl_mask_win_disable(int fd, int win_idx)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_win_disable);

int scl_mask_set_param(int fd, int win_idx, mask_win_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_set_param);

int scl_mask_get_param(int fd, int win_idx, mask_win_param_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_get_param);

int scl_mask_set_alpha(int fd, int win_idx, alpha_t alpha)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_set_alpha);

int scl_mask_get_alpha(int fd, int win_idx, alpha_t *alpha)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_get_alpha);

int scl_mask_set_border(int fd, int win_idx, mask_border_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_set_border);

int scl_mask_get_border(int fd, int win_idx, mask_border_t *param)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_mask_get_border);

int scl_set_palette(int fd, int idx, int crcby)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_set_palette);

int scl_get_palette(int fd, int idx, int *crcby)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_get_palette);

int scl_img_border_enable(int fd)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_img_border_enable);

int scl_img_border_disable(int fd)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_img_border_disable);

int scl_set_img_border_color(int fd, int color)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_set_img_border_color);

int scl_get_img_border_color(int fd, int *color)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_get_img_border_color);

int scl_set_img_border_width(int fd, int width)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_set_img_border_width);

int scl_get_img_border_width(int fd, int *width)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_get_img_border_width);

int scl_get_hw_ability(hw_ability_t *ability)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scl_get_hw_ability);

int fscaler300_put_job(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(fscaler300_put_job);

void scaler_trans_init(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(scaler_trans_init);

void scaler_trans_exit(void)
{
    printk("This is pseudo %s\n", __FUNCTION__);
}
EXPORT_SYMBOL(scaler_trans_exit);

int scaler_directio_snd(directio_info_t *data, int len)
{
    printk("This is pseudo %s\n", __FUNCTION__);
   return 0;
}
EXPORT_SYMBOL(scaler_directio_snd);

int scaler_directio_rcv(directio_info_t *data, int len)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(scaler_directio_rcv);

int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
}

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("scaler");
MODULE_LICENSE("GPL");