# Main makefile for all Amiga 1200+ drivers
# https://stackoverflow.com/questions/17896751/makefile-use-multiple-makefiles/17897075

PROJ_ROOT = $(PWD)

CCPATH							   = /opt/Amiga

CC                            = $(CCPATH)/bin/m68k-amigaos-gcc 
CXX                           = $(CCPATH)/bin/m68k-amigaos-g++
AR                            = $(CCPATH)/bin/m68k-amigaos-ar
RANLIB							   = $(CCPATH)/bin/m68k-amigaos-ranlib
LD                       		= $(CCPATH)/bin/m68k-amigaos-gcc
SHELL                         = sh
XDFTOOL								= /usr/local/bin/xdftool

INC_SRCH_PATH						:= -I$(PROJ_ROOT)/os_includes/sana

CFLAGS								+= -Os \
										-fstrength-reduce \
										$(INC_SRCH_PATH) \
										-fno-builtin-printf \
										-fomit-frame-pointer \
										-Wstrict-prototypes \
                              -noixemul \
                              -Wall \
                              -m68$(ARCH) \
                              -msoft-float 
                              
#If not given via command line, build for "68000"
ifeq ($(ARCH),)
	ARCH=000
endif			
                          
export CCPATH CC LD CXX CFLAGS LDFLAGS RANLIB LD AOS_INCLUDES OS_INCLUDES CFLAGS ARCH PROJ_ROOT XDFTOOL

all: 	CFLAGS += -s
all:
	#@$(MAKE) -C KSZ8851/servicetool
	@$(MAKE) -C KSZ8851/driver
	
debug:	CFLAGS += -DDEBUG -g
debug:
	#@$(MAKE) -C KSZ8851/servicetool 
	@$(MAKE) -C KSZ8851/driver 


.PHONY: clean

clean:
	#@$(MAKE) -C KSZ8851/servicetool clean
	@$(MAKE) -C KSZ8851/driver clean