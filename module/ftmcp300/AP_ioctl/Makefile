# get CROSS_COMPILE
sinclude $(PWD)/../../module_conf

#AR=$(CROSS_COMPILE)ar
#LD=$(CROSS_COMPILE)ld
CC=$(CROSS_COMPILE)gcc
CFLAGS= -Wall -march=armv4 -mtune=arm9tdmi -O3
CFLAGS+=-I../


OBJDEC = dec_main.o
all:	$(OBJDEC)
	$(CC) $(CFLAGS) $(OBJDEC) -o dec_main

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f dec_main  
	rm -f *.o
