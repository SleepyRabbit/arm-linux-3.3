sinclude $(PWD)/../module_conf

MODULE = pseudo_audio

SDK_MODULE_SRC := $(SDK_SRC)module/
STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

ifneq ($(KERNELRELEASE),)

include $(srctree)/.config

EXTRA_CFLAGS+=$(PLATFORM_CFLAGS)
EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/include/adda308
EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/audio_drv/codec

modules-yy += pseudo_audio.o

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))

else

PWD := $(shell pwd)

modules:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD)
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko

clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean
endif





