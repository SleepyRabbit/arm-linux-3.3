MODULE = gm2d
sinclude $(PWD)/../module_conf

ifeq ($(CONFIG_PLATFORM_GM8287),y)
USING_GM2D_OSG=y
else
ifeq ($(CONFIG_PLATFORM_GM8136),y)
USING_LCD_BLIT=y
USING_GM2D_OSG=y
else
USING_GM2D_OSG=n
USING_LCD_BLIT=n
endif
endif

SDK_MODULE_SRC := $(SDK_SRC)module/
STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)include 
EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)include/frammap/

ifeq ($(USING_GM2D_OSG),y)
EXTRA_CFLAGS += -DGM2D_OSG_USING
endif
ifeq ($(USING_LCD_BLIT),y)
EXTRA_CFLAGS += -DGM2D_LCD_BLIT_USING
endif
ft2dge-drv-objs := gm2d_driver.o

ft2dge-objs-y$(CONFIG_PLATFORM_GM8287) += gm8287.o
ft2dge-objs-y$(CONFIG_PLATFORM_GM8139) += gm8139.o
ft2dge-objs-y$(CONFIG_PLATFORM_GM8136) += gm8136.o

$(MODULE)-objs := $(ft2dge-drv-objs) $(ft2dge-objs-yy)

obj-m += $(MODULE).o

obj-m := $(sort $(obj-m))

else

PWD := $(shell pwd)

modules:
	echo $(EXTRA_CFLAGS)
	$(MAKE) -C $(LINUX_SRC) SDK_MODULE_SRC=$(SDK_MODULE_SRC) M=$(PWD) modules
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko
	cp *.ko /tmp

.PHONEY: clean
clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean

copy:
	cp -f $(MODULE).ko $(SDK_SRC)/target/rootfs-cpio/lib/modules/

endif
