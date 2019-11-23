#
# Amiga SANA-II Amiga 1200Plus KSZ8851 network device driver 
#
# (c) 2019 Heiko Prüssing <heiko@heiko-pruessing.de>
#

DEVICE_NAME   = ksz8851.device
		   
#CFLAGS		+= -DDEBUG=1

# Strip symbols if needed...
#LDOPTS += -s

# Build subdirectory for every supported microprocessor (68000-68080)
BUILDDIR=build/build-$(ARCH)
                            
SRC = $(wildcard *.c)
HDR = $(wildcard *.h)
OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))                            
                                                            
LDOPTS += -noixemul -nostartfiles -lnix -lamiga -ldebug -lstubs
LDOPTS += -Wl,-Map,$@.map -Wl,--cref 
CFLAGS += -Wno-pointer-sign
CFLAGS += -DDEVICE_NAME=\"$(DEVICE_NAME)\"
#LDOPTS += --verbose

.PHONY: all distribution

all: builddir $(BUILDDIR)/$(DEVICE_NAME)
     
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
		ls -la $@.$(ARCH)
		
clean:
		@rm -r -f -d *.o *.s *.aobj *.device *.map mapfile $(BUILDDIR)/*.o $(BUILDDIR)/*.device
		@rm -fr $(BUILDDIR)

$(BUILDDIR)/%.o:%.c $(HDR)
	@echo "Compiling C   " $< " (68"$(ARCH)")"
	@$(CC) -c $(CFLAGS) -o $@ $< 

.c.s:
	@$(CC) -S -nostdinc $(CFLAGS) -o $@ $< 
	