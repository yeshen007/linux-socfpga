TARGETS = pingpong_app_read pingpong_app_write pingpong_app_fddr

CFLAGS := -O0 -mfpu=neon-fp16 -std=gnu99 -W -Wall -Werror -Wc++-compat -Wwrite-strings -Wstrict-prototypes -pedantic
CPPFLAGS := -O0 -fPIC -Wall -mfpu=neon-fp16
ASMFLAGS := -mcpu=cotex-a9 -mfpu=neon-fp16

CROSS_COMPILE := /home/hanglory/yezheng/toolchain/bin/arm-linux-gnueabihf-

CC := $(CROSS_COMPILE)gcc
C++ := $(CROSS_COMPILE)g++
ASM := $(CROSS_COMPILE)as
LINK := $(CROSS_COMPILE)g++

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
CPPFILES = pingpong_app_read.cpp pingpong_app_fddr.cpp pingpong_app_write.cpp F2smIntf.cpp utils.cpp	
ASMFILES = 

READCFILES =
READCPPFILES = pingpong_app_read.cpp F2smIntf.cpp utils.cpp	
READASMFILES = 

WRITECFILES =
WRITECPPFILES = pingpong_app_write.cpp F2smIntf.cpp utils.cpp	
WRITEASMFILES = 

FDDRCFILES =
FDDRCPPFILES = pingpong_app_fddr.cpp F2smIntf.cpp utils.cpp	
FDDRASMFILES = 
 
.PHONY: all apps

all: apps

apps: pingpong_app_read pingpong_app_write pingpong_app_fddr


pingpong_app_read: $(READCFILES:.c=.o) $(READCPPFILES:.cpp=.o) $(READASMFILES:.cpp=.o)
	$(LINK) -o $@ $(READCFILES:.c=.o) $(READCPPFILES:.cpp=.o) $(READASMFILES:.cpp=.o)

pingpong_app_write: $(WRITECFILES:.c=.o) $(WRITECPPFILES:.cpp=.o) $(WRITEASMFILES:.cpp=.o)
	$(LINK) -o $@ $(WRITECFILES:.c=.o) $(WRITECPPFILES:.cpp=.o) $(WRITEASMFILES:.cpp=.o)

pingpong_app_fddr: $(FDDRCFILES:.c=.o) $(FDDRCPPFILES:.cpp=.o) $(FDDRASMFILES:.cpp=.o)
	$(LINK) -o $@ $(FDDRCFILES:.c=.o) $(FDDRCPPFILES:.cpp=.o) $(FDDRASMFILES:.cpp=.o)
	
.PHONY:
clean:
	$(RM) $(TARGETS) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(ASMFILES:.S=.o)


.SUFFIXES: .cpp .c .o .S

.cpp.o:
	$(C++) -c -o $*.o  $(CPPFLAGS) $*.cpp

.c.o:
	$(CC) -c -o $*.o  $(CFLAGS) $*.c

.S.o:
	$(ASM) -c -o $*.o  $(ASMFLAGS) $*.S
