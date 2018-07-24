PWD := $(shell pwd)
# get CROSS_COMPILE, SDK_SRC, LINUX_SRC
sinclude $(PWD)/../module_conf

STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip


MODULE := favc_dec

ifeq ($(obj),)
obj=$(PWD)
endif

#include $(obj)/userconfig.mak
#include $(obj)/Makefile.rules

SDK_MODULE_SRC := $(SDK_SRC)module
#yes for VG, others for ioctl
GMGraphInterface=yes

ifneq ($(GMGraphInterface),yes)
MODULE:=favc_dec_ioctl_drv
endif

ifneq ($(KERNELRELEASE),)

decoder-objs :=  dec_driver/decoder.o dec_driver/nalu.o dec_driver/params.o \
                 dec_driver/qsort.o dec_driver/quant.o dec_driver/refbuffer.o \
                 dec_driver/sei.o dec_driver/slice.o dec_driver/slicehdr.o \
                 dec_driver/H264V_dec.o dec_driver/bitstream.o \
				 dec_driver/favc_dec_ll.o


ifeq ($(GMGraphInterface),yes)
EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)/include/videograph.include/log 
EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)/include/videograph.include/em 
EXTRA_CFLAGS += -DVG_INTERFACE
endif

ifeq ($(CONFIG_PLATFORM_A369),y)
EXTRA_CFLAGS += -DA369
endif

ifeq ($(CONFIG_PLATFORM_GM8287),y)
EXTRA_CFLAGS += -DGM8287
endif

ifeq ($(CONFIG_PLATFORM_GM8210),y)
EXTRA_CFLAGS += -DGM8210
endif

# an example of adding new platform
ifeq ($(CONFIG_PLATFORM_GMXXXX),y)
EXTRA_CFLAGS += -DGMXXXX
endif


EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)/include/frammap 
EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)/include/ftmcp300 
EXTRA_CFLAGS += -DLINUX 

ifeq ($(GMGraphInterface),yes)
# for VG driver
top-objs := favc_dec_vg.o favc_dec_entity.o favc_dec_proc.o favc_dec_dbg.o favc_dec_rev.o
else
# for ioctl driver
top-objs := favc_dev.o favc_dec_ioctl.o 
endif
top-objs += platform.o
top-objs += $(decoder-objs)

$(MODULE)-objs := $(top-objs)
modules-yy := $(MODULE).o

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))

else

modules:
	$(MAKE) -C $(LINUX_SRC) SDK_MODULE_SRC=$(SDK_MODULE_SRC) M=$(PWD) modules 
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko

.PHONEY: clean
clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean

.PHONEY: all
all: modules

.PHONEY: copy
copy:
	cp -f $(MODULE).ko $(SDK_SRC)/target/rootfs-cpio/lib/modules/

# for check env
.PHONEY: check
check:
	@echo $(LINUX_SRC)
	@echo $(SDK_MODULE_SRC)
	@echo $(PWD)
	@echo $(PWD)/../module_conf

endif
