PWD := $(shell pwd)
sinclude $(PWD)/../../module_conf

STRIPFLAGS = -g --strip-unneeded
STRIP = $(CROSS_COMPILE)strip

SDK_MODULE_SRC := $(SDK_SRC)module
KERNELDIR := $(LINUX_SRC)

ifneq ($(KERNELRELEASE),)

include $(srctree)/.config

obj-m += dh9901_tw2964/
obj-m += tp2802_tw2964/
obj-m += tp2802_nvp1914/
obj-m += tp2822_nvp6114/
obj-m := $(sort $(obj-m))

else

all modules:
	$(MAKE) -C $(KERNELDIR) SDK_MODULE_SRC=$(SDK_MODULE_SRC) M=$(PWD) modules
	@$(MAKE) -C dh9901_tw2964/  module_strip
	@$(MAKE) -C tp2802_tw2964/  module_strip
	@$(MAKE) -C tp2802_nvp1914/ module_strip
	@$(MAKE) -C tp2822_nvp6114/ module_strip

module_strip:
	@$(MAKE) -C dh9901_tw2964/  module_strip
	@$(MAKE) -C tp2802_tw2964/  module_strip
	@$(MAKE) -C tp2802_nvp1914/ module_strip
	@$(MAKE) -C tp2822_nvp6114/ module_strip

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
