#########################################################################
#                                                                       #
#       Copyright (C) 2013-2014 Grain Media, Corp.                      #
#       All Rights Reserved.                                            #
#                                                                       #
#########################################################################

PWD := $(shell pwd)
sinclude $(PWD)/../../module_conf

SDK_MODULE_SRC := $(SDK_SRC)module
DEC_INCDIR  := $(SDK_MODULE_SRC)

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
LD := $(CROSS_COMPILE)ld
AS := $(CROSS_COMPILE)as
STRIP := $(CROSS_COMPILE)strip

EXTRA_CFLAGS+=-O2 -Wall
CFLAGS  := $(CFLAGS) -I$(DEC_INCDIR)/
LDFLAGS := $(EXTRA_FLAGS) $(LDFLAGS)

#########################################################################

AUDIO_DEV_APP := audio_dev_ioc
AUDIO_DEV_APPOBJS := audio_dev_ioc.o

.PHONY: all

all: $(AUDIO_DEV_APP) 

$(AUDIO_DEV_APPOBJS): %.o:%.c
	@echo [Complier... $^]
	@$(CC) $(CFLAGS) -c $< -o $@

$(AUDIO_DEV_APP): $(AUDIO_DEV_APPOBJS)
	@echo [Link... $@]
	@$(CC) -o $@ $(AUDIO_DEV_APPOBJS:.o=.c) $(CFLAGS) $(LDFLAGS) 

#########################################################################

clean:
	rm -f $(AUDIO_DEV_APP) $(AUDIO_DEV_APPOBJS) 
