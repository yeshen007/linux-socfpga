
#Name of the module
obj-m := pingpong_driver.o
#Files composing the module
pingpong_driver-objs :=  pingpong_driver_ko.o

#guest architecture
ARCH := arm

#compiler
CROSS_COMPILE := /home/hanglory/yezheng/toolchain/bin/arm-linux-gnueabihf-

#path to the compiled kernel
ROOTDIR := /home/hanglory/yezheng/linux-socfpga

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

KDIR ?= $(ROOTDIR)

MODDIR := /home/hanglory/nfs_share/yz

default:
	$(MAKEARCH) -C $(KDIR) M=${shell pwd}

clean:
	$(MAKEARCH) -C $(KDIR) M=${shell pwd} clean

help:
	$(MAKEARCH) -C $(KDIR) M=${shell pwd} help

modules:
	$(MAKEARCH) -C $(KDIR) M=${shell pwd} modules

modules_install:
	$(MAKEARCH) -C $(KDIR) M=${shell pwd} INSTALL_MOD_PATH=$(MODDIR) modules_install

