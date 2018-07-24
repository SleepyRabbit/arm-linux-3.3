sinclude $(PWD)/../../module_conf

ifeq ($(obj),)
obj=$(PWD)
endif

ifndef SDK_MODULE_SRC
VERS3300 = $(findstring arm-linux-3.3,$(obj))

ifneq ($(VERS3300),)
SDK_SRC := /usr/src/arm-linux-3.3/
SDK_MODULE_SRC = $(SDK_SRC)module/
SDK_MODULE_INC = $(SDK_MODULE_SRC)include/
#CROSS_COMPILE=/usr/src/arm-linux-3.16/toolchain_gnueabi-4.8.x_ARM926/opt/ext-toolchain/bin/arm-none-linux-gnueabi-

CROSS_COMPILE=/opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-

else
SDK_SRC := /usr/src/arm-linux-2.6/
SDK_MODULE_SRC = $(SDK_SRC)module/
SDK_MODULE_INC = $(SDK_MODULE_SRC)

endif

endif



RAM_TARGET = $(SDK_SRC)target/rootfs-cpio/lib/modules

AR=$(CROSS_COMPILE)ar
LD=$(CROSS_COMPILE)ld
CC=$(CROSS_COMPILE)gcc


CFLAGS+=-I$(SDK_MODULE_INC)  -I./

OBJTEST = ft2driver_gm2d.o gm2d_demo.o

all:  test

test:	$(OBJTEST)
	$(CC) -lc -lm $(OBJTEST) -o gm2dge_demo

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	$(AR) rc libgm2d.a  ft2driver_gm2d.o

Makefile.depend depend:
	$(CC) $(CFLAGS) -MM -MG *.c > Makefile.depend
	
	 
clean:
	rm -f *.o
	rm -f *.a	
	rm -f gm2dge_demo
	
