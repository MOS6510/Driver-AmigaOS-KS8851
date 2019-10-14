#
# Amiga SANA-II Amiga 1200Plus KSZ8851 device driver 
#
# (C) 2019 Heiko Prüssing <heiko@heiko-pruessing.de>
#

SOURCE_DEVICE = $(wildcard *.c)								   
DEVICE_NAME       = ksz8851.device

#CC_OPTS		+= -DDEBUG=1

# Separat "strip" (from tool chain) command does not work but linker option works ????
# Use it to strip symbols from device only...
STRIP_OPTS 		 = -s

BUILDDIR=build/build-$(ARCH)
                                                            
LD_OPTS += -L${ROOT_DIR}/libs -nostdlib -noixemul -lnix -lamiga -ldebug -lstubs 

OBJECTS_DEVICE         = $(patsubst %.c, $(BUILDDIR)/%.o, $(SOURCE_DEVICE))

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

		
$(BUILDDIR)/$(DEVICE_NAME):	$(OBJECTS_DEVICE)
		@echo "Linking       " $@
		@$(LINKER) $^ $(STRIP_OPTS) -o $@.$(ARCH) -nostdlib $(LD_OPTS) -Wl,-Map,$@.map -Wl,--cref
		
clean:
		rm -r -f -d *.o *.s *.aobj *.device *.map mapfile $(BUILDDIR)/*.o $(BUILDDIR)/*.device

#.c.o:
$(BUILDDIR)/%.o:%.c
	@echo "Compiling C   " $< " (68"$(ARCH)")"
	@$(CC) -c -nostdinc $(CC_OPTS) -o $@ $< 

.c.s:
	$(CC) -S -nostdinc $(CC_OPTS) $(COMPILE_MODE) -o $@ $< 
	