#
# Amiga SANA-II Amiga 1200Plus KSZ8851 network device driver 
#
# (c) 2019 Heiko Prüssing <heiko@heiko-pruessing.de>
#

DEVICE_NAME   = ksz8851.device
		   
# Base relative a4 register test (020):
#CFLAGS		+= -fbaserel32 -msmall-code -DBASEREL=1
#LDOPTS      += -fbaserel32 -msmall-code

# Strip symbols if needed...
#LDOPTS += -s

# Build subdirectory for every supported microprocessor (68000-68080)
BUILDDIR=build/build-$(ARCH)
                            
SRC = $(wildcard *.c)
HDR = $(wildcard *.h) 

#Make sure "devinit.o" is the first object file when linking...
FIRST_LINK_OBJECT = devinit.c 
SRC_TMP := $(SRC)
SRC = $(filter-out $(FIRST_LINK_OBJECT), $(SRC_TMP))
SRC := $(FIRST_LINK_OBJECT) $(SRC)

#Create List of objects from list of sources...
OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))
                    
                                                            
LDOPTS += -noixemul -nostartfiles -lnix -lamiga -ldebug -lstubs
LDOPTS += -Wl,-Map,$@.map -Wl,--cref 
CFLAGS += -Wno-pointer-sign
CFLAGS += -DDEVICE_NAME=\"$(DEVICE_NAME)\"
#LDOPTS += --verbose


.PHONY: all distribution

all: builddir $(BUILDDIR)/$(DEVICE_NAME) devinit.s device.s
     
builddir:
	@mkdir -p $(BUILDDIR)

# Creates distribution (and build all architechtures and LHA file at the end)
distribution:	
	@make clean all    ARCH=000
	@make clean device ARCH=020
	@make clean device ARCH=040
	@make clean device ARCH=060
	@./build-installation.sh
	
device: $(BUILDDIR)/$(DEVICE_NAME) 

$(BUILDDIR)/$(DEVICE_NAME):	$(OBJS)
		@echo "Linking       " $@
		@$(LD) $^ -o $@.$(ARCH) $(LDOPTS)
		@strings $@.$(ARCH) | grep '$$VER'
		@ls -la $@.$(ARCH)
		
clean:
		rm -rf -d *.o *.s *.aobj *.device *.map mapfile $(BUILDDIR)/*.o $(BUILDDIR)/*.device
		rm -fr build/  $(BUILDDIR)

$(BUILDDIR)/%.o:%.c $(HDR)
	@echo "Compiling C   " $< " (68"$(ARCH)")"
	@$(CC) -c $(CFLAGS) -o $@ $< 

.c.s: $(HDR)
	@echo "Compiling C (only to ASM) " $< " (68"$(ARCH)")"
	@$(CC) -S $(CFLAGS) -o $@ $< 
	