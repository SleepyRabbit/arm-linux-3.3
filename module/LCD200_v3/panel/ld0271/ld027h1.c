/*******************************************************************************
* Copyright  Grain-Media, Inc. 2011-2012.  All rights reserved.                *
*------------------------------------------------------------------------------*
* Name: ld027h1.c                                                           *
* Description:  ld027h1 control code                                         *
*                                                                              *
* Author: giggs_hu                                                             *
*******************************************************************************/

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/ftpmu010.h>
#include "ld027h1.h"


//==============================================================================
//
//                              DEFINES
//
//==============================================================================

/* Definition for Driver Version */
#define ld027h1_VERSION        "0.1.0"
#define BIT0    0
#define BIT1    1
#define BIT7    7

/* Definition for Serial Command Control PIN */
#define ld027h1_CS_PIN         28   /* GPIO0_28 */
#define ld027h1_SCL_PIN        31   /* GPIO0_31 */
#define ld027h1_SDA_PIN        29   /* GPIO0_29 */
#define ld027h1_RST_PIN        56   /* GPIO1_24 */

static pmuReg_t pmu_reg_8136[] = {
    {0x44, 0x1 << 30, 0x0, 0x1 << 30, 0x1 << 30},   /* enhance tv clock slew rate */
    {0x58, 0x3 << 16 | 0x3 << 14 | 0x3 << 12 | 0x3 << 10, 0x0, 0x0, 0x3 << 16 | 0x3 << 14 | 0x3 << 12 | 0x3 << 10},
    {0x5C, 0xFFFF, 0x0, 0xFFFF, 0xFFFF},
    {0x64, 0x3 << 8 | 0x3 << 2, 0x0, 0x3 << 8 | 0x0 << 2, 0x3 << 8 | 0x3 << 2},
};

static pmuRegInfo_t pmu_reg_info_8136 = {
    "ld027h1",
    ARRAY_SIZE(pmu_reg_8136),
    ATTR_TYPE_NONE,
    &pmu_reg_8136[0]
};
static int ld027h1_fd = -1;

//==============================================================================
//
//                              TYPE DEFINES
//
//==============================================================================

//==============================================================================
//
//                              STRUCTURES
//
//==============================================================================
/* Definition for Serial Command Data Format */
typedef struct ld027h1_sc_data {
  unsigned char reg;
  unsigned char value;
} ld027h1_SC_DATA_t;

//==============================================================================
//
//                              EXTERNAL VARIABLES REFERENCE
//
//==============================================================================

static ld027h1_SC_DATA_t ld027h1_DEFAULT_CFG[] = {
    // Reset
    {0x05, 0x5F},
    {0x05, 0x1F},
	{0x05, 0x5F},
    //exit standby mode
    {0x2B, 0x01},
    //VCOMDC ENABLE
    {0x0B, 0x80},
	//VCOMAC  
    {0x00, 0x06},
    //VCOMDC  
    {0x01, 0x9F},
	//YUV720 interface
    {0x04, 0x69},
    //Default Gamma setting  2.2 
    {0x16, 0x04},
};

u32 ld027h1_SC_DATA_NUM = sizeof(ld027h1_DEFAULT_CFG)/sizeof(ld027h1_SC_DATA_t);

//==============================================================================
//
//                       HELPER FUNCTIONS
//
//==============================================================================
static __inline void delay(int tick)
{
    return mdelay((tick * HZ) / 1000);
}

//==============================================================================
//
//                      PRIVATE FUNCTIONS
//
//==============================================================================
static s32 ld027h1_pinmux_init(void)
{
    ld027h1_fd = ftpmu010_register_reg(&pmu_reg_info_8136);
    if (ld027h1_fd < 0) {
        printk("ld027h1_pinmux_init fail! \n");
        return -1;
    }
    gpio_direction_output(ld027h1_RST_PIN, 1);
    gpio_set_value(ld027h1_RST_PIN, 1);
    delay(20);
    gpio_set_value(ld027h1_RST_PIN, 0);
    delay(20);
    gpio_set_value(ld027h1_RST_PIN, 1);
    delay(20);

    /* Config CS pin direction to output and active high */
    gpio_direction_output(ld027h1_CS_PIN, 1);

    /* Config SCL pin direction to output and active high */
    gpio_direction_output(ld027h1_SCL_PIN, 1);

    /* Config SDA pin direction to input */
    gpio_direction_output(ld027h1_SDA_PIN, 0);

    return 0;
}

u16 ld027h1_read(u8 reg)
{
    int i;
    u8  bit_val;
    u16  rec_data = 0;
    u16 out_data = 0;

    /*
     * prepare 16 bit output data format
     *
     * [A6][R/W][A5][A4][A3][A2][A1][A0][D7][D6][D5][D4][D3][D2][D1][D0]
     *
     * [R/W]=> 0:write, 1:read
     */
     out_data = ((reg&0x40)<<9) | (1<<14) | ((reg&0x3F)<<8);

    /* config SDA pin direction to output and active high */
     gpio_direction_output(ld027h1_SDA_PIN, 1);

     /* active CS to low */
     gpio_set_value(ld027h1_CS_PIN, 0);

     for(i=15; i>=0; i--) {
         bit_val = (out_data>>i) & 0x01;

         /* active SCL to low */
         gpio_set_value(ld027h1_SCL_PIN, 0);

         /* output register address data */
         if(i >= 8)
             gpio_set_value(ld027h1_SDA_PIN, bit_val);
         else {
             /* receive register data */
             if(i==7)
                 gpio_direction_input(ld027h1_SDA_PIN); //switch SDA pin to input direction

             //if(gpio_get_value(ld027h1_SDA_PIN))
             //    rec_data |= (0x1<<i);
         }

         ndelay(50);

         /* active SCL to high */
         gpio_set_value(ld027h1_SCL_PIN, 1);

         //ndelay(50);
         ndelay(20);

         if(i < 8)
         {
             /* receive register data */
             int ret = gpio_get_value(ld027h1_SDA_PIN);
             //printf("%d ret = %d\n", i, ret);
             if(ret)
             {
                 rec_data |= (0x1<<i);
             }
          }

          ndelay(30);
       }

     /*active CS to high */
     gpio_set_value(ld027h1_CS_PIN, 1);

     ndelay(400);

    return rec_data;
}

int ld027h1_write(u8 reg, u8 data)
{
    int i;
    u8  bit_val;
    u16 out_data = 0;

    /*
      * prepare 16 bit output data format
      *
      * [A6][R/W][A5][A4][A3][A2][A1][A0][D7][D6][D5][D4][D3][D2][D1][D0]
      *
      * [R/W]=> 0:write, 1:read
      */
     out_data = ((reg&0x40)<<9) | ((reg&0x3F)<<8) | data;

     /* config SDA pin direction to output and active high */
     gpio_direction_output(ld027h1_SDA_PIN, 1);

      /*active CS to low */
      gpio_set_value(ld027h1_CS_PIN, 0);

      for(i=15; i>=0; i--) {
           bit_val = (out_data>>i) & 0x01;

           /* active SCL to low */
           gpio_set_value(ld027h1_SCL_PIN, 0);

           /* set output data */
           gpio_set_value(ld027h1_SDA_PIN, bit_val);

           ndelay(50);

           /* active SCL to high */
           gpio_set_value(ld027h1_SCL_PIN, 1);

           ndelay(50);
       }

      /* active CS to high */
       gpio_set_value(ld027h1_CS_PIN, 1);

       /* switch SDA pin to input direction */
       gpio_direction_input(ld027h1_SDA_PIN);

       ndelay(400);


    return 0;
}


int ld027h1_write_cmd(u16 out_data)
{
    int i;
    u8  bit_val;

    /*
      * prepare 16 bit output data format
      *
      * [A6][R/W][A5][A4][A3][A2][A1][A0][D7][D6][D5][D4][D3][D2][D1][D0]
      *
      * [R/W]=> 0:write, 1:read
      */

     /* config SDA pin direction to output and active high */
      gpio_direction_output(ld027h1_SDA_PIN, 1);
      /*active CS to low */
      gpio_set_value(ld027h1_CS_PIN, 0);

      for(i=15; i>=0; i--) {
           bit_val = (out_data>>i) & 0x01;

           /* active SCL to low */
           gpio_set_value(ld027h1_SCL_PIN, 0);

           /* set output data */
           gpio_set_value(ld027h1_SDA_PIN, bit_val);

           ndelay(50);

           /* active SCL to high */
           gpio_set_value(ld027h1_SCL_PIN, 1);

           ndelay(50);
       }

      /* active CS to high */
       gpio_set_value(ld027h1_CS_PIN, 1);

       /* switch SDA pin to input direction */
       gpio_direction_input(ld027h1_SDA_PIN);

       ndelay(400);


    return 0;
}

//==============================================================================
//
//                      PUBLIC FUNCTIONS
//
//==============================================================================
static int __init panel_ld027h1_init(void)
{
    int i;

    gpio_request(ld027h1_CS_PIN, "ld027h1_CS_PIN");
    gpio_request(ld027h1_SCL_PIN, "ld027h1_SCL_PIN");
    gpio_request(ld027h1_SDA_PIN, "ld027h1_SDA_PIN");
    gpio_request(ld027h1_RST_PIN, "ld027h1_RST_PIN");

     /* Config PinMux */
    if (ld027h1_pinmux_init() < 0)
        return -1;

 #ifdef _DEBUG_OTA5182
/*    for(i = 0; i < ld027h1_SC_DATA_NUM; i++) {
        if (i < 2)
            continue;

        reg = ld027h1_DEFAULT_CFG[i].reg;
        printf("%x, value:%x \n", reg, ld027h1_read(reg));
    } */
#endif

    /* Init ld027h1 */
    for(i = 0; i < ld027h1_SC_DATA_NUM; i++) {
        ld027h1_write(ld027h1_DEFAULT_CFG[i].reg, ld027h1_DEFAULT_CFG[i].value);

        if (i == 0)
            mdelay(5);
        if (i == 1)
            mdelay(10);
		 if (i == 2)
            mdelay(50);
    }
   /* for(i = 0; i < sizeof(ld027h1_DEFAULT_CMD)/sizeof(u16); i++) {
         ld027h1_write_cmd(ld027h1_DEFAULT_CMD[i]);

         //if (i == 0)
         //    mdelay(1);
         //if (i == 1)
         //    mdelay(20);
    }
    */
//#ifdef _DEBUG_OTA5182
    for(i = 0; i < ld027h1_SC_DATA_NUM; i++) {
        unsigned char reg;

		if (i < 2)
            continue;

        reg = ld027h1_DEFAULT_CFG[i].reg;
        printk("%x, value:%x \n", reg, ld027h1_read(reg));
    }
//#endif

    //reg = reg;
    //val = val;

    printk("* Panel A05DN01 Init .....................[OK]\n");

    return 0;
}

static void __exit panel_ld027h1_exit(void)
{
    gpio_free(ld027h1_CS_PIN);
    gpio_free(ld027h1_SCL_PIN);
    gpio_free(ld027h1_SDA_PIN);
    gpio_free(ld027h1_RST_PIN);

    if (ld027h1_fd >= 0)
        ftpmu010_deregister_reg(ld027h1_fd);
}

int panel_ld027h1_set_mirror(u32 enable)
{
    u8 val;
    //ld027h1_write(0x05, 0x5f);
    val = ld027h1_read(0x04);
    if (enable)
    {
        val |= (BIT0|BIT1);
    }
    else
    {
        val &= ~(BIT0|BIT1);
    }
    val=ld027h1_write(0x04, val);
    //ld027h1_write(0x05, 0x5e);
    return val;
}

int panel_ld027h1_get_mirror()
{
    return (ld027h1_read(0x04) & BIT0);
}

int panel_ld027h1_set_flip(u32 enable)
{
#if 0
    u8 val;

    //ld027h1_write(0x05, 0x5f);
    val = ld027h1_read(0x04);
    if (enable)
    {
        val |= BIT1;
    }
    else
    {
        val &= ~BIT1;
    }
    val = ld027h1_write(0x04, val);
    //ld027h1_write(0x05, 0x5e);

    return val;
#endif
    return 1;
}

int panel_ld027h1_get_flip()
{
    return (ld027h1_read(0x04) & BIT1);
}

int panel_ld027h1_set_narrow(u32 enable)
{
    u8 val;

    val = ld027h1_read(0x04);
    if (enable)
    {
        val |= BIT7;
    }
    else
    {
        val &= ~BIT7;
    }

    return ld027h1_write(0x04, val);
}

int panel_ld027h1_get_narrow()
{
    return (ld027h1_read(0x04) & BIT7);
}

void panel_ld027h1_set_standby(u8 ON_off)
{
	if(ON_off)
		ld027h1_write(0x2B, 0x00);
	else
		ld027h1_write(0x2B, 0x01);
}

module_init(panel_ld027h1_init);
module_exit(panel_ld027h1_exit);
MODULE_AUTHOR("GM Technology Corp.");
MODULE_DESCRIPTION("ld027h1 Function Driver");
MODULE_LICENSE("GPL");