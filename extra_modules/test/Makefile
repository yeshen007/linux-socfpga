#make -- 编译obj-m指定的内核模块
#make clean -- 清除编译生成的文件
#make install -- 将编译生成的ko内核模块拷贝到当前目录的build下
#make install之后再手动拷贝到开发板的模块目录/lib/modules/5.5.0-00065-gff94549704ff/extra下

ifeq ($(KERNELRELEASE),) 
KERNELDIR = /home/hanglory/yezheng/linux-socfpga
PWD := $(shell pwd)
INSPATH = /home/hanglory/yezheng/linux-socfpga/extra_modules/build
all:
	make -C $(KERNELDIR) M=$(PWD) modules
install:
	cp *.ko $(INSPATH)	
clean:
	rm -f *.ko *.o *.mod.c *.order *.symvers *.mod .*.cmd
else
obj-m := 
obj-m += hello.o
obj-m += kprobe_example.o
obj-m += kretprobe_example.o
obj-m += kmap_example.o
obj-m += atomic_example.o
obj-m += kmalloc_example.o
obj-m += vmalloc_example.o
endif
