#
# Amiga 1200+ Ethernet driver KSZ8851 
#
# 2019 by Heiko Prüssing
#

SRC      := $(wildcard *.c)
HDR 	   := $(wildcard *.h) 
SRC_MAIN := main.c
SRC_LIB  := $(filter-out $(SRC_MAIN), $(SRC))

#If not given via command line build for "MC68000"
ifeq ($(ARCH),)
	ARCH=000
endif	

BUILDDIR = build/build-$(ARCH)
OS_INCLUDES                   = -Iincludes -Iincludes/sana -Iincludes/libnix                                                    
LINKER_OPTS                	= -Llibs -nostdlib -noixemul -lnix -lamiga -lstubs 
#All objects:
OBJ       							= $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC))
#All objects of the linker lib:
OBJ_LIB    							= $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC_LIB))
#All objects of the cnsole program:
OBJ_MAIN								= $(patsubst %.c, $(BUILDDIR)/%.o, $(SRC_MAIN))

LDFLAGS += -L$(BUILDDIR)


.PHONY: all distribution builddir install clean

all: builddir $(BUILDDIR)/ksz8851

builddir:
	@mkdir -p $(BUILDDIR)
		
$(BUILDDIR)/ksz8851:	$(OBJ_MAIN) $(BUILDDIR)/libksz8851.a
		@echo "Linking       " $@
		@$(LD) $^ $(LDFLAGS) -o $@ -lksz8851 -Wl,-Map,$@.map 
		
$(BUILDDIR)/libksz8851.a: $(OBJ_LIB)
		@echo "Create Linking Library " $@ 
		@$(AR) rcs $@ $^
#		$(NM) -s $@
		
		
# Installs device on real Amiga via "Amiga Explorer"
install:
		$(AMIGA_EXPLORER) -l || true
		@sleep 1
		$(AMIGA_EXPLORER) -u RAD:/ksz8851 || true
		@sleep 1
		$(AMIGA_EXPLORER) -s $(BUILDDIR)/ksz8851 RAD:/ksz8851	
		
clean:
		rm -r -f -d $(BUILDDIR) *.o *.s *.aobj *.map mapfile

#.c.o:
$(BUILDDIR)/%.o:%.c $(HDR)
	@echo "Compiling C   " $< " (cpu=68"$(ARCH)")"
	@$(CC) -c $(CFLAGS) -o $@ $< 

#.c.s:
$(BUILDDIR)/%.s:%.c $(HDR)
	@$(CC) -S $(CFLAGS) -o $@ $< 
	