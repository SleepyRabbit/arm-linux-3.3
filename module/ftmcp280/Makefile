sinclude $(PWD)/../module_conf
PWD := $(shell pwd)

#include $(LINUX_SRC).config

MODULE = favc_enc

ifeq ($(obj),)
obj=$(PWD)
endif

include $(obj)/config.mak

SDK_MODULE_SRC := $(SDK_SRC)module/

GMGraphInterface=yes
ReferenceBufferPool=yes
ifeq ($(CONFIG_PLATFORM_GM8139),y)
AESEncrypt=no
else
AESEncrypt=no
endif

encoder-objs := enc_driver/encoder.o enc_driver/params.o enc_driver/quant.o enc_driver/refbuffer.o enc_driver/slice.o enc_driver/slice_header.o enc_driver/vlc.o

ifeq ($(ReferenceBufferPool),yes)
EXTRA_CFLAGS+=-DREF_POOL_MANAGEMENT
encoder-objs += mem_pool.o
endif

ifeq ($(AESEncrypt),yes)
EXTRA_CFLAGS+=-DAES_ENCRYPT
encoder-objs += enc_driver/encrypt.o
endif

ifeq ($(GMGraphInterface),yes)
EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/include/videograph.include/log -I$(SDK_MODULE_SRC)/include/videograph.include/em -I$(SDK_MODULE_SRC)/include/videograph.include/util
EXTRA_CFLAGS+=-DVG_INTERFACE
EXTRA_CFLAGS+=-DREORDER_JOB
EXTRA_CFLAGS+=-DRELEASE_ALL_REFERENCE
else
EXTRA_CFLAGS+=-DALLOCATE_FROM_FRAMMAP
endif
EXTRA_CFLAGS+=-I$(SDK_MODULE_SRC)/include/frammap -I$(SDK_MODULE_SRC)/include/ftmcp280

ifneq ($(KERNELRELEASE),)
ifeq ($(GMGraphInterface),yes)
top-objs := favc_enc_entity.o favc_enc_vg.o favc_enc_proc.o platform.o favce_buffer.o
else
top-objs := favc_dev.o favc_enc_ioctl.o favc_enc_proc.o platform.o favce_buffer.o
endif
top-objs += $(encoder-objs)

$(MODULE)-objs := $(top-objs)
modules-yy := $(MODULE).o

ifeq ($(CONFIG_FPGA),y)
EXTRA_CFLAGS+=-DFPGA_PLATFORM
endif

EXTRA_CFLAGS+=-DLINUX
ifeq ($(CONFIG_PLATFORM_GM8136),y)
EXTRA_CFLAGS+=-DGM8136
EXTRA_CFLAGS+=-DSUPPORT_PWM
endif
ifeq ($(CONFIG_PLATFORM_GM8139),y)
EXTRA_CFLAGS+=-DGM8139
EXTRA_CFLAGS+=-DSUPPORT_PWM
endif
ifeq ($(CONFIG_PLATFORM_GM8210),y)
EXTRA_CFLAGS+=-DGM8210 -DFPGA_8210
endif
ifeq ($(CONFIG_PLATFORM_GM8287),y)
EXTRA_CFLAGS+=-DGM8287
endif
EXTRA_CFLAGS+=-DRC_ENABLE
#EXTRA_CFLAGS+=-DUSE_TRIGGER_WORKQ
EXTRA_CFLAGS+=-DTEMPORAL_DENOISE_ENABLE
#EXTRA_CFLAGS+=-DRECORD_BS

ifeq ($(CONFIG_VIDEO_FASTTHROUGH),y)
EXTRA_CFLAGS+=-DSUPPORT_VG2
endif

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))

else

modules:
	$(MAKE) -C $(LINUX_SRC) SDK_MODULE_SRC=$(SDK_MODULE_SRC) SDK_MODULE_INC=$(SDK_MODULE_INC) LINUX_SRC=$(LINUX_SRC) M=$(PWD) modules
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) modules
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko

clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean

copy:
ifeq ($(GMGraphInterface),yes)
	cp $(MODULE).ko /data/test_bench/platform8210/module/
else
	cp $(MODULE).ko /data/mcp280/
endif

endif
