sinclude $(PWD)/../module_conf

VIDEOGRAPH_INC=1
MODULE = ft3dnr200

SDK_MODULE_SRC := $(SDK_SRC)module/
STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

ifneq ($(KERNELRELEASE),)

include $(srctree)/.config

$(MODULE)-objs-y += gm8136.o

EXTRA_CFLAGS+=$(PLATFORM_CFLAGS)

ifdef VIDEOGRAPH_INC
EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/include/videograph.include/log -I$(SDK_MODULE_SRC)/include/videograph.include/em
EXTRA_CFLAGS+=-DVIDEOGRAPH_INC=1
endif

EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/include/frammap -I$(SDK_MODULE_SRC)/include/ft3dnr200

$(MODULE)-objs := ft3dnr200_isp_api.o ft3dnr200_drv.o ft3dnr200_hw.o ft3dnr200_vg.o ft3dnr200_mem.o ft3dnr200_ctrl.o ft3dnr200_mrnr.o ft3dnr200_tmnr.o ft3dnr200_ee.o ft3dnr200_dma.o $(ft3dnr200-objs-y)

modules-yy += $(MODULE).o

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))

else

PWD := $(shell pwd)

modules:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) VIDEOGRAPH_INC=$(VIDEOGRAPH_INC) modules
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko

clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean
endif





