# Main makefile for all Amiga 1200+ drivers
# https://stackoverflow.com/questions/17896751/makefile-use-multiple-makefiles/17897075

ROOT_DIR = $(PWD)


CCPATH                       = /usr/local/cross-tools/m68k-amigaos
CC                           = $(CCPATH)/bin/gcc 
CPP                          = $(CCPATH)/bin/g++
AR                           = $(CCPATH)/bin/ar
RANLIB							  = $(CCPATH)/bin/ranlib
SHELL                        = sh
LINKER                       = $(CCPATH)/bin/gcc

INC_SRCH_PATH						:= -I$(ROOT_DIR)/os_includes/sana -I$(ROOT_DIR)/os_includes/libnix

CC_OPTS								+= -O2 -fstrength-reduce \
										-nostdinc -I$(CCPATH)/include -I$(CCPATH)/sys-include $(INC_SRCH_PATH) \
										-fno-builtin-printf \
										-g \
                              -noixemul \
                              -Wall \
                              -fomit-frame-pointer \
                              -m68$(ARCH) \
                              -msoft-float 
LD_OPTS								:=	-L${ROOT_DIR}/libs
                              
#If not given via command line, build for "68000"
ifeq ($(ARCH),)
	ARCH=000
endif	


                              
export CCPATH CC LD CPP CFLAGS LDFLAGS RANLIB LINKER AOS_INCLUDES OS_INCLUDES CC_OPTS ARCH ROOT_DIR LD_OPTS

all:
	#@$(MAKE) -C KSZ8851/servicetool
	@$(MAKE) -C KSZ8851/driver

.PHONY: clean
clean:
	#@$(MAKE) -C KSZ8851/servicetool clean
	@$(MAKE) -C KSZ8851/driver clean