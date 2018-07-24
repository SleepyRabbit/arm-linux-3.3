TARGET ?= ptp
CROSS_COMPILE=/opt/gm8136/toolchain_gnueabi-4.4.0_ARMv5TE/usr/bin/arm-unknown-linux-uclibcgnueabi-

LIB_DIR=lib
INC_DIR=include

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
STRIP=$(CROSS_COMPILE)strip

CFLAGS += -Wall -O2
CFLAGS += -I${INC_DIR} -I${INC_DIR}/glib-2.0/
LDFLAGS=-lglib-2.0 -lintl -lz -lrt -liconv -pthread -L${LIB_DIR}
LDFLAGS+=-static

SRCS =	ptp.c

OBJS = $(patsubst %.c, %.o, $(SRCS))
DEPS = $(patsubst %.o, %.d, $(OBJS))


all: $(TARGET)

$(TARGET): $(OBJS) 
	@echo LD "<"$@">"
	@$(CC) -o $@ $^ $(LDFLAGS)
	@$(STRIP) $@

clean:
	-@rm -f $(OBJS) $(DEPS) $(TARGET)

%.o: %.c
	@echo CC "<"$<">"
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)
