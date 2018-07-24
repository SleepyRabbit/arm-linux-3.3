#include <linux/module.h>
#include <linux/kernel.h>
#include "gm2d_osg_if.h"


/* Replace gm2d.ko */
int gm2d_osg_remainder_fire(void)
{
    return 0;
}
EXPORT_SYMBOL(gm2d_osg_remainder_fire);

int gm2d_osg_do_blit(gm2d_osg_blit_t* blit_pam,int fire)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(gm2d_osg_do_blit);

int gm2d_osg_do_mask(gm2d_osg_mask_t* mask_pam,int fire)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(gm2d_osg_do_mask);

int gm2d_lcd_do_blit(gm2d_lcd_blit_t* blit_pam,int fire)
{
    printk("This is pseudo %s\n", __FUNCTION__);
    return 0;
}
EXPORT_SYMBOL(gm2d_lcd_do_blit);


int init_module(void)
{
    return 0;
}

void cleanup_module(void)
{
}

MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("gm2d");
MODULE_LICENSE("GPL");