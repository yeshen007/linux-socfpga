
#Name of the module
obj-m := wr_f2sm_random.o
#Files composing the module
wr_f2sm_random-objs :=  f2sm_wdev_ko.o

#guest architecture
ARCH := arm

#compiler
CROSS_COMPILE := /home/hanglory/yezheng/toolchain/bin/arm-linux-gnueabihf-

#path to the compiled kernel
ROOTDIR := /home/hanglory/yezheng/linux-socfpga

MODDIR := /home/hanglory/nfs_share/yz

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

KDIR ?= $(ROOTDIR)

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

