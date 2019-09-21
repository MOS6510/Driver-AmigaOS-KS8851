/**
 * Testtool for Ethernet chip KSZ8851 at Amiga1200+ project.
 * Author: Heiko Pruessing
 */


#include <clib/alib_protos.h>
#include <clib/alib_stdio_protos.h>
#include <clib/graphics_protos.h>
#include <strings.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/types.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include "unix_adapter.h"
#include "ks8851.h"


//#include "libnix/libinit.h"

#define BYTE char
#define UBYTE unsigned char

#define BOOL char
#define ULONG unsigned long

/*
//I can't include <stdio.h> for some reason so I need to define some little stuff here:
typedef void FILE;
int    setvbuf __P((void *, char *, int, size_t));
#define  stdout   (__sF[1])
extern FILE **__sF;
#define  _IONBF   2 // setvbuf should set unbuffered
FILE  *fopen __P((const char *, const char *));
size_t fread __P((void *, size_t, size_t, FILE *));
*/

#define COLOR02   "\033[32m"
#define COLOR03   "\033[33m"
#define NORMAL    "\033[0m"


// ######## LOCAL MACROS ########

#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

// ###### EXTERNALS ########

extern void exit(int);
extern struct ExecBase * SysBase;

ULONG ETHERNET_BASE_ADDRESS = (ULONG)0xd90000l;

/**
 * Should 16 bit access be swapped the the software?
 */
BOOL swappingChipAccess       = false;
BOOL switchChipToBigEndian    = false;
BOOL switchChipToLittleEndian = false;


u16 swap(u16 value) {
   u16 h = (value >> 8) & 0xff;
   u16 l = value & 0xff;
   return l << 8 | h;
}


void iowrite16(u16 value, void __iomem *addr)
{
   *((u16*)addr) = swappingChipAccess ? swap(value) : value;
   //printf("iowrite16(0x%x, 0x%lx)\n", value, addr);
}

unsigned int ioread16(void __iomem *addr)
{
   //printf("ioread16(0x%x)\n", addr);
   u16 value = *((u16*)addr);
   return swappingChipAccess? swap(value) : value;
}


int main(int argc, char * argv[])
{
   printf("Amiga1200+ KSZ8851-16MLL Service Tool, version 1.0 (%s, %s)\n", __DATE__, __TIME__);
   printf("Memory base address of ethernet chip: 0x%lx\n", ETHERNET_BASE_ADDRESS);

   int i;

   //check command line...
   if (argc > 1)
   {
      for(i=1;i<argc;i++)
      {
         if (strcmp( argv[i], "be") == 0) {
            switchChipToBigEndian = true;
         }

         if (strcmp( argv[i], "le") == 0) {
            switchChipToLittleEndian = true;
         }

         if (strcmp( argv[i], "swap") == 0) {
            swappingChipAccess = true;
         }
      }
   }
   else
   {
      printf("usage: %s le eb swap\n"
            " le: set chip to little endian mode\n"
            " eb: set chip to big endian mode\n"
            " swap: swapping every 16 bit during chip access\n"

            , argv[0]);
      exit(0);
   }
   printf("\n");

   if (switchChipToBigEndian && switchChipToLittleEndian)
   {
      printf(":-) These options does not make any sense!\n");
      exit(0);
   }

   {
      struct ks_net ks = {0};
      ks.cmd_reg_cache = 0;
      ks.cmd_reg_cache_int = 0;
      ks.hw_addr = (u16*)(ETHERNET_BASE_ADDRESS + 0);
      ks.hw_addr_cmd = (u16*) (ETHERNET_BASE_ADDRESS + 2);

      if (swappingChipAccess) {
         printf("%s is swapping every 16 bit access during access.\n", argv[0]);
      }

      //Set to big endian...
      if (switchChipToBigEndian) {
         u16 oldValue = ks_rdreg16(&ks, KS_RXFDPR);
         ks_wrreg16(&ks, KS_RXFDPR, oldValue | RXFDPR_EMS);
         printf("Switching chip to big endian mode.\n");
      }

      //Set to big endian...
      if (switchChipToLittleEndian) {
         u16 oldValue = ks_rdreg16(&ks, KS_RXFDPR);
         ks_wrreg16(&ks, KS_RXFDPR, oldValue & ~RXFDPR_EMS);
         printf("Switching chip to little mode.\n");
      }

      //
      // Chip probe
      //
      printf("Chip probe: ");
      u16 val = ks_rdreg16(&ks, KS_CIDER);
      if ((val & ~CIDER_REV_MASK) != CIDER_ID) {
         printf("failed");
      } else {
         printf("ok!");
      }
      printf(" (value on register 0xc0 is 0x%04x, should be 0x887x)\n", (val & ~CIDER_REV_MASK));

      //
      // readout registers...
      //
      int i;
      printf("Dumping all 256 chip registers:\n");
      for(i=0;i<256;i++)
      {
         if (((i % 16) == 0)) {
            if (i != 0) {
               printf("\n");
            }
            printf("%04x: ", i);
         }
         printf("%02x ", ks_rdreg8(&ks,i));
      }
      printf("\n");
   }

   return 0;
}
