/**
 * Service tool for Ethernet chip KSZ8851 Amiga1200+ project.
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

#include "../servicetool/ks8851.h"
#include "../servicetool/unix_adapter.h"

// ########################################## LOCAL MACROS ##################################################

#define COLOR02   "\033[32m"
#define COLOR03   "\033[33m"
#define NORMAL    "\033[0m"

#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

// ########################################## EXTERNALS #####################################################

//Dummy forward private linux driver struct. Real defined in the driver itself.
struct ks_net;

extern void ks_wrreg16(struct ks_net *ks, int offset, u16 value);
extern u16 ks_rdreg16(struct ks_net *ks, int offset);
extern void ks_wrreg16(struct ks_net *ks, int offset, u16 value);

extern struct ks_net * ks8851_init();


/**
 * Should 16 bit access be swapped the the software?
 */
BOOL swappingChipAccess       = false;
BOOL switchChipToBigEndian    = false;
BOOL switchChipToLittleEndian = false;

/**
 * Swaps a 16 bit value...0x1234 => 0x3412
 * @param value
 */
inline u16 swap(u16 value)
{
   register u16 h = (value >> 8) & 0xff;
   register u16 l = value & 0xff;
   return (l << 8) | h;
}

/**
 * Support function writing register
 * @param value
 * @param addr
 */
void iowrite16(u16 value, void __iomem *addr)
{
   //Access to the command register MUST always be swapped due to a fix hardware swapping in design.
   if ((ULONG)addr & KS8851_REG_CMD_OFFSET)
   {
      //CMD register fixed swap
      value = swap(value);
   }
   else
   {
      //DATA register optional swap:
      value = swappingChipAccess ? swap(value) : value;
   }

   *((u16*)addr) = value;

   //printf("iowrite16(0x%x, 0x%lx)\n", value, addr);
}

/**
 * Support function reading registers
 * @param addr
 * @return
 */
unsigned int ioread16(void __iomem *addr)
{
   u16 value = *((u16*)addr);

   //Access to the command register MUST always be swapped due to a fix hardware swapping in design.
   if ((ULONG) addr & KS8851_REG_CMD_OFFSET)
   {
      //CMD register fixed swap
      return swap(value);
   }
   else
   {
      //DATA register optional swap:
      return swappingChipAccess ? swap(value) : value;
   }
}


int main(int argc, char * argv[])
{
   int i;

   printf("Amiga1200+ KSZ8851-16MLL Service Tool, version 1.0 (%s, %s)\n", __DATE__, __TIME__);
   printf("Memory base address of ethernet chip: 0x%lx\n", ETHERNET_BASE_ADDRESS);
   printf("Data Register at: 0x%lx (16 bit)\n", ETHERNET_BASE_ADDRESS + KS8851_REG_DATA_OFFSET);
   printf("CMD Register at:  0x%lx (16 bit)\n", ETHERNET_BASE_ADDRESS + KS8851_REG_CMD_OFFSET );

   //check command line...
   if (argc > 1)
   {
      for(i=1;i<argc;i++)
      {
         if (strcmp( argv[i], "be") == 0)
         {
            switchChipToBigEndian = true;
         }

         if (strcmp( argv[i], "le") == 0)
         {
            switchChipToLittleEndian = true;
         }

         if (strcmp( argv[i], "swapdata") == 0)
         {
            swappingChipAccess = true;
         }
      }
   }
   else
   {
      printf("usage: %s le eb swapdata\n"
            " le: set chip to little endian mode\n"
            " eb: set chip to big endian mode\n"
            " swapdata: swapping every 16 bit data during chip access\n"

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
      //Init network structure
      struct ks_net * ks = ks8851_init();

      if (swappingChipAccess) {
         printf("%s-tool is swapping every 16 bit data during chip access.\n", argv[0]);
      }

      //Set to big endian?
      if (switchChipToBigEndian) {
         u16 oldValue = ks_rdreg16(ks, KS_RXFDPR);
         ks_wrreg16(ks, KS_RXFDPR, oldValue | RXFDPR_EMS);
         printf("Switching chip to big endian mode.\n");
      }

      //Set to little endian?
      if (switchChipToLittleEndian) {
         u16 oldValue = ks_rdreg16(ks, KS_RXFDPR);
         oldValue = 0xffff;
         printf("old value 0x%x, new value 0x%x", oldValue, oldValue & ~RXFDPR_EMS);
         ks_wrreg16(ks, KS_RXFDPR, oldValue & ~RXFDPR_EMS);
         printf("Switching chip to little endian mode.\n");
      }

      //
      // Chip probe
      //
      printf("Chip probe: ");
      u16 val = ks_rdreg16(ks, KS_CIDER);
      if ((val & ~CIDER_REV_MASK) != CIDER_ID) {
         printf("failed");
      } else {
         printf("ok!");
      }
      printf(" (value on register 0xc0 is 0x%04x, should be 0x887x)\n", (val & ~CIDER_REV_MASK));


      //
      // Get Link Status:
      //
      val = ks_rdreg16(ks, KS_P1SR);
      printf("Ethernet Link Status: %s\n", val & P1SR_LINK_GOOD ? "Up" : "Down" );

      //
      // readout registers...
      //
      int i;
      printf("\nDumping all 128 16-bit registers:\n");
      for(i=0;i<256;i+=2)
      {
         if (((i % 16) == 0)) {
            if (i != 0) {
               printf("\n");
            }
            printf("%04x: ", i);
         }
         printf("%04x ", ks_rdreg16(ks,i));
      }
      printf("\n");
   }

   return 0;
}
