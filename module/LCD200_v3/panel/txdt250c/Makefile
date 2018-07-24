sinclude $(PWD)/../../../module_conf

MODULE = txdt250cqr

SDK_MODULE_SRC := $(SDK_SRC)module/
STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

ifneq ($(KERNELRELEASE),)

include $(srctree)/.config

#txdt250c-objs := txdt250cqr.o

modules-yy += txdt250cqr.o

obj-m += $(modules-yy)
obj-m := $(sort $(obj-m))

else

PWD := $(shell pwd)

modules:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD)
	$(STRIP) $(STRIPFLAGS) $(MODULE).ko
	cp *.ko /tmp1/
clean:
	$(MAKE) -C $(LINUX_SRC) M=$(PWD) clean
endif





