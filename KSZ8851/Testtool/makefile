#
# Amiga SANA-II Etherbridge device driver GNU-GCC make file.
#
# (C) Copyright 1996-2004, 2018 by Heiko Prüssing
#

SOURCE_TESTTOOL   = testtool.c


#SOURCE_DEVICE     = devinit.c conffile.c keyboard.c etherbridge.c jan_int.c devdebug.c
#SOURCE_UNITTEST   = unittest.c etherbridge.c jan_int.c keyboard.c devdebug.c conffile.c
#SOURCE_ETHERPREFS = EtherPrefs/mainform.cpp EtherPrefs/device.cpp EtherPrefs/ConfFile.cpp \
#                    EtherPrefs/Application.cpp EtherPrefs/DriverConfigForm.cpp \
#                    EtherPrefs/EtherPrefs.cpp EtherPrefs/lostfiles.cpp EtherPrefs/HPHook.cpp EtherPrefs/DosDriver.cpp
								   
#DEVICE_NAME       = etherbridge.device

COMPILE_MODE		=
COMPILE_MODE		+= -DDEBUG=1


CC_OPTS           =  $(COMPILE_MODE)

# Separat "strip" (from tool chain) command does not work but linker option works ????
# Use it to strip symbols from device only...
STRIP_OPTS 		 = -s


AMIGA_EXPLORER = ./Software_Tests_and_Tools/lxamiga.pl

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
                              
                              #-msmall-code \
                              #-mstackcheck -mstackextend \       
                              
CPPFLAGS								+= -O3 -D__cplusplus $(AOS_INCLUDES) $(OS_INCLUDES) -I. -IEtherPrefs \
										 -m68$(ARCH) 
                                                            
#LINKER_OPTS                 = -Llibs -nostdlib -noixemul -lnix -lamiga -ldebug -lstubs 
LINKER_OPTS                 = -Llibs -nostdlib -noixemul -lnix -lamiga -lstubs 

#OBJECTS_DEVICE         = $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_DEVICE))
#OBJECTS_UNITTEST       = $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_UNITTEST))
#OBJECTS_ReadPCMem      = $(patsubst %.c, $(BUILDDIR)/%.o, Snippets/ReadPCMem.c)
#OBJECTS_ReceiveSignals = $(patsubst %.c, $(BUILDDIR)/%.o, Snippets/ReceiveSignalsMain.c)
#OBJECTS_Roundtrips     = $(patsubst %.c, $(BUILDDIR)/%.o, Snippets/roundtrips.c keyboard.c devdebug.c)
#OBJECTS_RunPCCmd       = $(patsubst %.c, $(BUILDDIR)/%.o, runpccmd.c keyboard.c)
#OBJECTS_EtherPrefs     = $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SOURCE_ETHERPREFS))

OBJECTS_TESTTOOL       = $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_DEVICE))



.PHONY: all distribution

all: builddir $(BUILDDIR)/testtool
#     $(BUILDDIR)/$(DEVICE_NAME) \
#     $(BUILDDIR)/unittest   $(BUILDDIR)/ReadPCMem      $(BUILDDIR)/ReceiveSignals \
#     $(BUILDDIR)/Roundtrips $(BUILDDIR)/stackchecktest $(BUILDDIR)/EtherPrefs.run \
#     $(BUILDDIR)/runpccmd
     
builddir:
	@echo "Create build directories..."
	@mkdir -p $(BUILDDIR)

# Creates distribution (and build all architechtures and LHA file at the end)
distribution:	
	@make clean all    ARCH=000
	@make clean device ARCH=020
	@make clean device ARCH=040
	@make clean device ARCH=060
	@./build-installation.sh
	
#device: $(BUILDDIR)/$(DEVICE_NAME)

# Installs device on real Amiga via "Amiga Explorer"
install:
		$(AMIGA_EXPLORER) -l || true
		@sleep 1
		$(AMIGA_EXPLORER) -u DEVS:/networks/$(DEVICE_NAME) || true
		@sleep 1
		$(AMIGA_EXPLORER) -s $(DEVICE_NAME) DEVS:/networks/$(DEVICE_NAME)
		@sleep 1
		$(AMIGA_EXPLORER) -s unittest ram:/unittest
		
#$(BUILDDIR)/$(DEVICE_NAME):	$(OBJECTS_DEVICE)
#		@echo "Linking       " $@
#		@$(LINKER) $^ $(STRIP_OPTS) -o $@.$(ARCH) -nostdlib $(LINKER_OPTS) -Wl,-Map,$@.map -Wl,--cref

#$(BUILDDIR)/unittest:	$(OBJECTS_UNITTEST)
#		@echo "Linking       " $@
#		@$(LINKER) $^ $(STRIP_OPTS) -o $@ -noixemul -Llibs -ldebug -O2 -Wl,-Map,$@.map 

$(BUILDDIR)/testtool:	$(OBJECTS_UNITTEST)
		@echo "Linking       " $@
		@$(LINKER) $^ $(STRIP_OPTS) -o $@ -noixemul -Llibs -O2 -Wl,-Map,$@.map 

		
#$(BUILDDIR)/ReadPCMem:	$(OBJECTS_ReadPCMem)
#		@echo "Linking       " $@
#		@$(LINKER) $^ -o $@ -noixemul -O2 -Wl,-Map,$@.map
				
#$(BUILDDIR)/ReceiveSignals:	$(OBJECTS_ReceiveSignals) 
#		@echo "Linking       " $@
#		@$(LINKER) $^ -o $@ -noixemul -O2 -Wl,-Map,$@.map
		
#$(BUILDDIR)/runpccmd:	$(OBJECTS_RunPCCmd)
#		@echo "Linking       " $@
#		@$(LINKER) $^ -o $@ -noixemul -O2 -Wl,-Map,$@.map
		
#$(BUILDDIR)/Roundtrips:		$(OBJECTS_Roundtrips) 
#		@echo "Linking       " $@
#		@$(LINKER) $^ -o $@ -noixemul -Llibs -ldebug -O2 -Wl,-Map,$@.map
		
#Special build with "libnix/gcc stackcheck feature": Compile & Link here...link with -lstack not manually needed???? 
#$(BUILDDIR)/stackchecktest:		Snippets/stackchecktest.c
#		@echo "Comp&Link     " $@ " (cpu=68"$(ARCH)")"
#		@$(CC) $^ -o $@  $(PRJ_INCLUDES) -mstackcheck -noixemul -Llibs -Lstack -O3 -Wl,-Map,$@.map
		
#$(BUILDDIR)/EtherPrefs/libmui++.a:	$(BUILDDIR)/EtherPrefs/mui++.o  
#		@echo "Create Link Lib " $@ " (cpu=68"$(ARCH)")"
#		@$(AR) qc $(BUILDDIR)/EtherPrefs/libmui++.a $(BUILDDIR)/EtherPrefs/mui++.o
#		@$(RANLIB) $(BUILDDIR)/EtherPrefs/libmui++.a
				
#$(BUILDDIR)/EtherPrefs.run:	$(OBJECTS_EtherPrefs) $(BUILDDIR)/EtherPrefs/libmui++.a
#		@echo "Linking (c++) " $@
#		@$(CPP) $^ $(STRIP_OPTS) -o $@ -noixemul -L$(BUILDDIR)/EtherPrefs -lgcc -lnix -lamiga -lmui++ -lstubs -Wl,-Map,$@.map

clean:
		rm -r -f -d $(BUILDDIR)/* *.o *.s *.aobj *.map mapfile

#.c.o:
$(BUILDDIR)/%.o:%.c
	@echo "Compiling C   " $< " (cpu=68"$(ARCH)")"
	@$(CC) -c -nostdinc $(CC_OPTS) -o $@ $< 

$(BUILDDIR)/%.o:%.cpp
	@echo "Compiling C++ " $< " (cpu=68"$(ARCH)")"
	@$(CPP) -c $(CPPFLAGS) -D__cplusplus -fno-rtti $(COMPILE_MODE) -o $@ $< 

.c.s:
	$(CC) -S -nostdinc $(CC_OPTS) $(COMPILE_MODE) -o $@ $< 
	
.cpp.s:
	$(CPP) -S $(CPPFLAGS) $(COMPILE_MODE) -o $@ $< 

