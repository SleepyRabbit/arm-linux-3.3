sinclude $(PWD)/../module_conf
PWD := $(shell pwd)
LINUX_SRC=$(SDK_SRC)linux-3.3-fa

EXTRA_CFLAGS += -I$(SDK_MODULE_SRC)/include/
STRIPFLAGS = -g --strip-unneeded
MODULE = pmonitor
STRIP = $(CROSS_COMPILE)strip

ifneq ($(KERNELRELEASE),)

pmonitor-objs-y$(CONFIG_PLATFORM_GM8139) += gm8139.o
pmonitor-objs-y$(CONFIG_PLATFORM_GM8136) += gm8139.o
pmonitor-objs := process_monitor.o $(pmonitor-objs-yy)
modules-yy += pmonitor.o

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))
else
ifndef SDK_MODULE_SRC
SDK_MODULE_SRC = $(SDK_SRC)/module/
endif

modules:
	$(MAKE) -C $(LINUX_SRC) SDK_MODULE_SRC=$(SDK_MODULE_SRC) M=$(PWD) modules	
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko

.PHONEY: clean
clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean

.PHONEY: all
all: modules

copy:
	cp -f $(MODULE).ko $(SDK_SRC)/target/rootfs-cpio/lib/modules/
endif
