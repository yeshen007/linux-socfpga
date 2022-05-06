
TARGET = app_base_change_dubiao


CFLAGS := -O0 -mfpu=neon-fp16 -std=gnu99 -W -Wall -Werror -Wc++-compat -Wwrite-strings -Wstrict-prototypes -pedantic
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

CFILES = 
CPPFILES =	base_change_dubiao.cpp YxmIntf.cpp 
ASMFILES = 
 
.PHONY: all

all: $(TARGET)

$(TARGET):$(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.cpp=.o)
	$(LINK) -o $@ $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.cpp=.o)

.PHONY:
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
