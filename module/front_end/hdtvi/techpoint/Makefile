PWD := $(shell pwd)
sinclude $(PWD)/../../../module_conf

STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

SDK_MODULE_SRC := $(SDK_SRC)module
KERNELDIR := $(LINUX_SRC)
FE_TVI_INCDIR := $(SDK_MODULE_SRC)/include/front_end/hdtvi

ifneq ($(KERNELRELEASE),)

include $(srctree)/.config

EXTRA_CFLAGS += -Wall -g -I$(obj)/../../ -I$(FE_TVI_INCDIR)/

tp2802-objs := tp2802_drv.o tp2802_lib.o
obj-m += tp2802.o
tp2806-objs := tp2806_drv.o tp2806_lib.o
obj-m += tp2806.o
tp2822-objs := tp2822_drv.o tp2822_lib.o
obj-m += tp2822.o
tp2823-objs := tp2823_drv.o tp2823_lib.o
obj-m += tp2823.o
tp2824-objs := tp2824_drv.o tp2824_lib.o
obj-m += tp2824.o
tp2833-objs := tp2833_drv.o tp2833_lib.o
obj-m += tp2833.o
tp2834-objs := tp2834_drv.o tp2834_lib.o
obj-m += tp2834.o

obj-m += $(modules-y)
obj-m := $(sort $(obj-m))

else

all modules:
	$(MAKE) -C $(KERNELDIR) SDK_MODULE_SRC=$(SDK_MODULE_SRC) M=$(PWD) modules
	@$(STRIP) $(STRIPFLAGS) *.ko

module_strip:
	@$(STRIP) $(STRIPFLAGS) *.ko

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
