#
# Amiga 1200+ Ethernet driver KSZ8851 
#
# 2019 by Heiko Prüssing
#

SOURCE_TESTTOOL   = $(wildcard *.c)
HDR 					:= $(wildcard *.h) 

COMPILE_MODE		= -O3
#COMPILE_MODE		+= -DDEBUG=1

CC_OPTS           =  $(COMPILE_MODE)

# "strip" (from tool chain) command does not work but linker option works ????
# Use it to strip symbols from device only...
STRIP_OPTS 		 = -s


#If not given via command line build for "MC68000"
ifeq ($(ARCH),)
	ARCH=000
endif	

BUILDDIR=build/build-$(ARCH)

OS_INCLUDES                  = -Iincludes -Iincludes/sana -Iincludes/libnix -Iincludes/mui
PRJ_INCLUDES                 = -I.
                                                            
LINKER_OPTS                	= -Llibs -nostdlib -noixemul -lnix -lamiga -lstubs 

OBJECTS_TESTTOOL       			= $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_TESTTOOL))

ADFIMAGE								= Amiga1200+.adf

.PHONY: all distribution

all: builddir $(BUILDDIR)/ksz8851 $(BUILDDIR)/$(ADFIMAGE) $(BUILDDIR)/ksz8851.s

$(BUILDDIR)/$(ADFIMAGE):
	@echo "Create ADF image..."
	@$(XDFTOOL) $@ format "Amiga1200+" + makedir "network" + write $(BUILDDIR)/ksz8851 "network"
	@$(XDFTOOL) $@ list
	
     
builddir:
	@echo "Create build directories..."
	@mkdir -p $(BUILDDIR)
		
$(BUILDDIR)/ksz8851:	$(OBJECTS_TESTTOOL)
		@echo "Linking       " $@
		@$(LD) $^ $(STRIP_OPTS) -o $@ -noixemul -Llibs -O2 -Wl,-Map,$@.map 
		
		
# Installs device on real Amiga via "Amiga Explorer"
install:
		$(AMIGA_EXPLORER) -l || true
		@sleep 1
		$(AMIGA_EXPLORER) -u RAD:/ksz8851 || true
		@sleep 1
		$(AMIGA_EXPLORER) -s $(BUILDDIR)/ksz8851 RAD:/ksz8851	
		
clean:
		rm -r -f -d $(BUILDDIR)/* *.o *.s *.aobj *.map mapfile

#.c.o:
$(BUILDDIR)/%.o:%.c $(HDR)
	@echo "Compiling C   " $< " (cpu=68"$(ARCH)")"
	$(CC) -c $(CC_OPTS) -o $@ $< 

#.c.s:
$(BUILDDIR)/%.s:%.c $(HDR)
	$(CC) -S $(CC_OPTS) $(COMPILE_MODE) -o $@ $< 
	

