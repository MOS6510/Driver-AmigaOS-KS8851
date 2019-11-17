#
# Amiga 1200+ Ethernet driver KSZ8851 
#
# 2019 by Heiko Prüssing
#

SOURCE_TESTTOOL   = testtool.c ks8851_mll.c

COMPILE_MODE		=
COMPILE_MODE		+= -DDEBUG=1

CC_OPTS           =  $(COMPILE_MODE)

# "strip" (from tool chain) command does not work but linker option works ????
# Use it to strip symbols from device only...
STRIP_OPTS 		 = -s

#AMIGA_EXPLORER = ./Software_Tests_and_Tools/lxamiga.pl

#If not given via command line build for "68000"
ifeq ($(ARCH),)
	ARCH=000
endif	

BUILDDIR=build/build-$(ARCH)

# Cross compile with MacOSX and GCC 3.4.1
CCPATH                       = /usr/local/cross-tools/m68k-amigaos
CC                           = $(CCPATH)/bin/gcc 
CPP                          = $(CCPATH)/bin/g++
AR                           = $(CCPATH)/bin/ar
RANLIB							  = $(CCPATH)/bin/ranlib
SHELL                        = sh
LINKER                       = $(CCPATH)/bin/gcc
AOS_INCLUDES                 = -Iincludes/os-include39/include/include_h 
OS_INCLUDES                  = -Iincludes -Iincludes/sana -Iincludes/libnix -Iincludes/mui
PRJ_INCLUDES                 = -I. -IEtherPrefs
CC_OPTS                      += -O2 -fstrength-reduce \
                              -nostdinc $(AOS_INCLUDES) -I$(CCPATH)/include -I$(CCPATH)/sys-include $(OS_INCLUDES) $(PRJ_INCLUDES)  \
                              -fno-builtin-printf \
                              -g \
                              -noixemul \
                              -Wall \
                              -fomit-frame-pointer \
                              -m68$(ARCH) \
                              -msoft-float                               
                              
CPPFLAGS								+= -O3 -D__cplusplus $(AOS_INCLUDES) $(OS_INCLUDES) -I. \
										 -m68$(ARCH) 
                                                            
LINKER_OPTS                	= -Llibs -nostdlib -noixemul -lnix -lamiga -lstubs 

OBJECTS_TESTTOOL       			= $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_TESTTOOL))

ADFIMAGE								= Amiga1200+.adf

.PHONY: all distribution

all: builddir $(BUILDDIR)/ks8851 $(BUILDDIR)/$(ADFIMAGE)

$(BUILDDIR)/$(ADFIMAGE):
	@echo "Create ADF image..."
	@$(XDFTOOL) $@ format "Amiga1200+" + makedir "network" + write $(BUILDDIR)/ks8851 "network"
	@$(XDFTOOL) $@ list
	
     
builddir:
	@echo "Create build directories..."
	@mkdir -p $(BUILDDIR)
		
$(BUILDDIR)/ks8851:	$(OBJECTS_TESTTOOL)
		@echo "Linking       " $@
		@$(LINKER) $^ $(STRIP_OPTS) -o $@ -noixemul -Llibs -O2 -Wl,-Map,$@.map 
		

clean:
		rm -r -f -d $(BUILDDIR)/* *.o *.s *.aobj *.map mapfile

#.c.o:
$(BUILDDIR)/%.o:%.c
	@echo "Compiling C   " $< " (cpu=68"$(ARCH)")"
	@$(CC) -c -nostdinc $(CC_OPTS) -o $@ $< 

.c.s:
	$(CC) -S -nostdinc $(CC_OPTS) $(COMPILE_MODE) -o $@ $< 
	
.cpp.s:
	$(CPP) -S $(CPPFLAGS) $(COMPILE_MODE) -o $@ $< 

