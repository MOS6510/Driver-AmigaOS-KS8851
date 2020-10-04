#
# Amiga SANA-II Amiga 1200Plus KSZ8851 network device driver 
#
# (c) 2019 Heiko Prüssing <heiko@heiko-pruessing.de>
#

DEVICE_NAME   = ksz8851.device
HWL_LIB       = libdummyhwl.a

# Build subdirectory for every supported microprocessor (68000-68080)
BUILDDIR=build/build-$(ARCH)
                            
SRC     += $(wildcard *.c)
HDR     += $(wildcard *.h) 
SRC_HWL += $(wildcard hwl/*.c)

HWL_LIB_PATH := $(BUILDDIR)/$(HWL_LIB)

ifeq ($(HWL),)
else
	HWL_LIB_PATH = $(HWL)
endif	

#Make sure "devinit.o" is the first object file during linking!
FIRST_LINK_OBJECT = devinit.c 
SRC_TMP := $(SRC)
SRC = $(filter-out $(FIRST_LINK_OBJECT), $(SRC_TMP))
SRC := $(FIRST_LINK_OBJECT) $(SRC)

#Create List of objects from list of sources...
OBJS     = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))
OBJS_HWL = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC_HWL))
                    
                                                           
LDOPTS += -noixemul -nostartfiles -lnix -lamiga -ldebug -lstubs
LDOPTS += -Wl,-Map,$@.map -Wl,--cref 
CFLAGS += -Wno-pointer-sign
CFLAGS += -DDEVICE_NAME=\"$(DEVICE_NAME)\"

.PHONY: all distribution builddir clean

all: builddir $(BUILDDIR)/$(DEVICE_NAME).$(ARCH)

addToAdf:
	@echo "Add to ADF file:"
	@$(XDFTOOL) $(ADFIMAGE) format "Amiga1200+Tools" + makedir "network" || true
	@$(XDFTOOL) $(ADFIMAGE) delete "network/"$(DEVICE_NAME).$(ARCH) || true
	@$(XDFTOOL) $(ADFIMAGE) write  $(BUILDDIR)/$(DEVICE_NAME).$(ARCH) "network"
	
# Installs device on real Amiga via "Amiga Explorer"
install:
		$(AMIGA_EXPLORER) -u DEVS:/networks/ksz8851.device || true
		@sleep 2
		$(AMIGA_EXPLORER) -s $(BUILDDIR)/$(DEVICE_NAME).$(ARCH) DEVS:/networks/ksz8851.device	
     
builddir:
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(BUILDDIR)/hwl
	
# This is a dummy lib for a dummy hardware...
$(BUILDDIR)/libdummyhwl.a: $(OBJS_HWL)
		@echo "Create Linking Library..." $@
		@$(AR) rcs $@ $^

# Creates distribution (and build all architechtures and LHA file at the end)
distribution:	
	@make clean all    ARCH=000
	@make clean device ARCH=020
	@make clean device ARCH=040
	@make clean device ARCH=060
	@./build-installation.sh
	
$(BUILDDIR)/$(DEVICE_NAME).$(ARCH):	$(OBJS)
		@echo "Linking       " $@
		@$(LD) $^ $(HWL_LIB_PATH) -o $@ $(LDOPTS) 
		@strings $@ | grep '$$VER'
		@$(MAKE) addToAdf
		@ls -la $@
		
clean:
		rm -rf -d *.o *.s *.aobj *.device *.map mapfile $(BUILDDIR)/*.o $(BUILDDIR)/*.device
		rm -fr build/  $(BUILDDIR)

$(BUILDDIR)/%.o:%.c
	@echo "Compiling C   " $< " (68"$(ARCH)")"
	@$(CC) -c $(CFLAGS) -o $@ $< 

.c.s: $(HDR)
	@echo "Compiling C (only to ASM) " $< " (68"$(ARCH)")"
	@$(CC) -S $(CFLAGS) -o $@ $< 
	