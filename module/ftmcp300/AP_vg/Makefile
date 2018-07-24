sinclude $(PWD)/../../module_conf
CUR_PATH := $(PWD)

LINUX_INC = $(LINUX_SRC)include    
SDK_MODULE_SRC := $(SDK_SRC)module
SDK_MODULE_INC = $(SDK_SRC)module/include
VG_MODULE_SRC = $(SDK_MODULE_SRC)/videograph.src
VG_MODULE_INC = $(SDK_MODULE_INC)/videograph.include

CC=$(CROSS_COMPILE)gcc
CPP=$(CC) -E
LD=$(CROSS_COMPILE)ld
AS=$(CROSS_COMPILE)as
MAKE=make


EXTRA_CFLAGS+=-Werror -g -I$(LINUX_INC) -I$(VG_MODULE_INC) -I$(VG_MODULE_INC)/log -I$(VG_MODULE_INC)/em -I$(CUS_INC)

#include $(LINUX_SRC).config

PWD := $(shell pwd)

all: test

test: test.c
	$(CC) -g test.c -I$(LINUX_INC) -I$(SDK_MODULE_INC) -I$(VG_MODULE_INC)/em -o test

check:
	@echo SDK_MODULE_INC: $(SDK_MODULE_INC) # remove me
	@echo VG_MODULE_SRC: $(VG_MODULE_SRC) # remove me
	@echo VG_MODULE_INC: $(VG_MODULE_INC) # remove me
	@echo LINUX_INC: $(LINUX_INC) # remove me
	
clean:
	rm -f test

