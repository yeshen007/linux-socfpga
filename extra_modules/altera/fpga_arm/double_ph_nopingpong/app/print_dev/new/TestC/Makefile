TARGET = app_print

CFLAGS := -O0 -mfpu=neon-fp16 -std=gnu99 -W -Wall -Wc++-compat -Wwrite-strings -Wstrict-prototypes -pedantic
CPPFLAGS := -O0 -fPIC -Wall -mfpu=neon-fp16
ASMFLAGS := -mcpu=cotex-a9 -mfpu=neon-fp16

CROSS_COMPILE := /home/hanglory/yezheng/toolchain/bin/arm-linux-gnueabihf-

CC := $(CROSS_COMPILE)gcc
C++ := $(CROSS_COMPILE)g++
ASM := $(CROSS_COMPILE)as
LINK := $(CROSS_COMPILE)g++ -lpthread


CFLAGS += -I.
CFLAGS += -I/home/hanglory/yezheng/linux-socfpga/arch/arm/include
CPPFLAGS += -I.
CPPFLAGS += -I/home/hanglory/yezheng/linux-socfpga/arch/arm/include
CPPFLAGS += --std=gnu++11 

ifeq ($(or $(COMSPEC),$(ComSpec)),)
RM := rm -rf
else
RM := cs-rm -rf
endif

CFILES := print_config_control.c utils.c
#CFILES += app_full_change_no_read_label_test.c 
#CFILES += app_full_change_read_label_test.c 
#CFILES += app_full_change_read_label_loop_test.c
#CFILES += app_base_change_no_read_label_test.c
#CFILES += app_base_change_read_label_test.c
CFILES += app_soft_stop_test.c
#CFILES += app_nonblock_test.c
#CFILES += app_timeout_test.c

CPPFILES :=	

ASMFILES := 
 
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.cpp=.o)
	$(LINK) -o $@ $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.cpp=.o)

clean: 
	$(RM) $(TARGET) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.S=.o)
	$(RM) ${TARGET}
	$(RM) tags 


.SUFFIXES: .cpp .c .o .S

.cpp.o:
	$(C++) -c -o $*.o $(DEFINES) $(CPPFLAGS) $*.cpp

.c.o:
	$(CC) -c -o $*.o $(DEFINES) $(CFLAGS) $*.c

.S.o:
	$(ASM) -c -o $*.o $(DEFINES) $(ASMFLAGS) $*.S
