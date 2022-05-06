TARGET := app_print

RM := rm -rf
TAGS := tags
TOPDIR := $(shell pwd)

export RM TAGS TOPDIR

CROSS_COMPILE := /home/hanglory/yezheng/toolchain/bin/arm-linux-gnueabihf-

AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
CC := $(CROSS_COMPILE)gcc
CPP := $(CC) -E
AR := $(CROSS_COMPILE)ar
NM := $(CROSS_COMPILE)nm

STRIP := $(CROSS_COMPILE)strip
OBJCOPY	:= $(CROSS_COMPILE)objcopy
OBJDUMP	:= $(CROSS_COMPILE)objdump
READELF	:= $(CROSS_COMPILE)readelf
GDB	:= $(CROSS_COMPILE)gdb

export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP READELF GDB

CFLAGS := -Wall -O2 -g 
CFLAGS += -I $(shell pwd)/include
#CFLAGS += -mfpu=neon-fp16 

LDFLAGS := 

export CFLAGS LDFLAGS 

#不能obj-y := 因为Makefile.build中有定义
obj-y += src/
obj-y += apps/

.PHONY: all recursive_build clean distclean

all: recursive_build $(TARGET)
	@echo $(TARGET) has been built!

recursive_build:
	make -C ./ -f $(TOPDIR)/Makefile.build 

$(TARGET): built-in.o	
	$(CC) built-in.o $(LDFLAGS) -o $@
	
clean:
	$(RM) $(shell find -name "*.o")
	$(RM) $(TARGET)

distclean:
	$(RM) $(shell find -name "*.o")
	$(RM) $(shell find -name "*.d")
	$(RM) $(TAGS)
	$(RM) $(TARGET)
	