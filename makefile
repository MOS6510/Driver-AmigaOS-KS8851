# Main makefile for all Amiga 1200+ drivers
# https://stackoverflow.com/questions/17896751/makefile-use-multiple-makefiles/17897075

PROJ_ROOT = $(PWD)

CCPATH							   = /opt/Amiga

CC                            = $(CCPATH)/bin/m68k-amigaos-gcc 
CXX                           = $(CCPATH)/bin/m68k-amigaos-g++
AR                            = $(CCPATH)/bin/m68k-amigaos-ar
NM                            = $(CCPATH)/bin/m68k-amigaos-nm
RANLIB							   = $(CCPATH)/bin/m68k-amigaos-ranlib
LD                       		= $(CCPATH)/bin/m68k-amigaos-gcc
LHA									= lha
SHELL                         = sh
XDFTOOL								= /usr/local/bin/xdftool
AMIGA_EXPLORER                = $(PWD)/tools/lxamiga.pl
INC_SRCH_PATH						:= -I$(PROJ_ROOT)/os_includes/sana

ADFIMAGE								:= $(PROJ_ROOT)/build/Amiga1200+Tools.adf
LHAFILE                       := $(PROJ_ROOT)/build/A1200PlusNetworkDriver.lha

CFLAGS								+= -Os \
										-I../include \
										-fstrength-reduce \
									 	$(INC_SRCH_PATH) \
										-Wstrict-prototypes \
                              -noixemul \
                              -Wall \
                              -m68$(ARCH) \
										-fomit-frame-pointer \
                              -msoft-float
                                        
LDFLAGS 								+= -Llibs -noixemul                                                 
                              
#If not given via command line, build for "68020"
ifeq ($(ARCH),)
	ARCH=020
endif		

BUILDDIR								:= build

#This is the hardware near code which should be used by the device (HAL)
HWL  = ../servicetool/build/build-$(ARCH)/libksz8851.a
                          
export CCPATH CC LD CXX CFLAGS LDFLAGS RANLIB AR LD AOS_INCLUDES OS_INCLUDES CFLAGS ARCH PROJ_ROOT XDFTOOL AMIGA_EXPLORER HWL NM ADFIMAGE LHAFILE

.PHONY: install clean all debug createDistribution

# Release version: make
all: 	CFLAGS += -s
all: 	LDFLAGS += -s
all:
	@$(MAKE) -C KSZ8851/servicetool
	@$(MAKE) -C KSZ8851/devicedriver
	@$(MAKE) createADFDistribution
	@$(MAKE) lhaDistribution
	
# Debug version: make debug
debug:  CFLAGS += -DDEBUG -g
debug:
	@$(MAKE) -C KSZ8851/servicetool
	@$(MAKE) -C KSZ8851/devicedriver  
	
install:
	#@$(MAKE) -C KSZ8851/servicetool   install
	@$(MAKE) -C KSZ8851/devicedriver  install

clean:
	@$(MAKE) -C KSZ8851/servicetool  clean
	@$(MAKE) -C KSZ8851/devicedriver clean
	rm -f $(ADFIMAGE)
	rm -fr build/*
	
createADFDistribution:
	@$(XDFTOOL) $(ADFIMAGE) write DistributionTemplate/install
	@$(XDFTOOL) $(ADFIMAGE) write DistributionTemplate/install.info
	@$(XDFTOOL) $(ADFIMAGE) write DistributionTemplate/Roadshow/ksz8851      
	@$(XDFTOOL) $(ADFIMAGE) write DistributionTemplate/Roadshow/ksz8851.info
	@$(XDFTOOL) $(ADFIMAGE) write DistributionTemplate/ksz8851.config
	@$(XDFTOOL) $(ADFIMAGE) list
	
lhaDistribution:
	@$(sh) @tools/create_lha_distribution.sh $(LHAFILE) KSZ8851/devicedriver/build/build-${ARCH}/ksz8851.device.020
	