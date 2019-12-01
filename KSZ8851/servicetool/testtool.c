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

int build_number = 1;

static const char * VERSION = "1.2";


// ########################################## LOCAL MACROS ##################################################

#define COLOR02   "\033[32m"
#define COLOR03   "\033[33m"
#define NORMAL    "\033[0m"
#define CLRSCR    "\033[1;1H\033[2J"

#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

typedef enum  {
   NO_ERROR = 0,
   NO_CHIP_FOUND
} error_t;

// ########################################## EXTERNALS #####################################################

//Dummy forward private linux driver struct. Real defined in the driver itself.
struct ks_net;

#define BE3             0x8000      /* Byte Enable 3 */
#define BE2             0x4000      /* Byte Enable 2 */
#define BE1             0x2000      /* Byte Enable 1 */
#define BE0             0x1000      /* Byte Enable 0 */

extern void ks_wrreg16(struct ks_net *ks, int offset, u16 value);
extern u16 ks_rdreg16(struct ks_net *ks, int offset);
extern void ks_wrreg16(struct ks_net *ks, int offset, u16 value);
extern u8 ks_rdreg8(struct ks_net *ks, int offset);

void dumpRegister8BitMode(struct ks_net * ks);
u8 KSZ8851ReadReg8(struct ks_net * ks, int offset);

extern struct ks_net * ks8851_init();


/**
 * Should 16 bit access be swapped the the software?
 */
BOOL swappingDataRegValue     = false;
BOOL swappingCmdRegValue      = false;
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
      //CMD register swap
      value = swappingCmdRegValue ? swap(value) : value;
   }
   else
   {
      //DATA register swap:
      value = swappingDataRegValue ? swap(value) : value;
   }

   //printf("iowrite16(value=0x%04x, addr=0x%lx)\n", value, addr);
   *((volatile u16*)addr) = value;
}

/**
 * Support function reading registers
 * @param addr
 * @return
 */
unsigned int ioread16(void __iomem *addr)
{
   u16 value = *((volatile u16*)addr);

   //Access to the command register MUST always be swapped due to a fix hardware swapping in design.
   if ((ULONG) addr & KS8851_REG_CMD_OFFSET)
   {
      //CMD register fixed swap
      return swappingCmdRegValue ? swap(value) : value;
   }
   else
   {
      //DATA register optional swap:
      return swappingDataRegValue ? swap(value) : value;
   }
}

static volatile u16 * REG_DATA = (volatile u16 *)(ETHERNET_BASE_ADDRESS + KS8851_REG_DATA_OFFSET);
static volatile u16 * REG_CMD  = (volatile u16 *)(ETHERNET_BASE_ADDRESS + KS8851_REG_CMD_OFFSET);


const bool dwordSwapped = false;

/**
 * Own very read register 8 bit version
 * @param ks
 * @param offset
 * @return
 */
u8 KSZ8851ReadReg8(struct ks_net * ks, int offset) {
   u8 result = 0;
   /**
    * Some miracle things happen when chip is set to big endian mode:
    * Not only it swapped 16 bit values (as expected), it seems to switch all registers in a 32 bit mode
    * behavior. => BE (Byte Enable logic) seems mirrored too. Not clear in chip manual.
    */
   if (!dwordSwapped) {
      //Normal default LE (from chip manual)
      switch (offset & 3) {
         case 0:
            *REG_CMD = offset | BE0;
            result = *REG_DATA;
            break;
         case 1:
            *REG_CMD = offset | BE1;
            result = *REG_DATA >> 8;
            break;
         case 2:
            *REG_CMD = offset | BE2;
            result = *REG_DATA;
            break;
         case 3:
            *REG_CMD = offset | BE3;
            result = *REG_DATA >> 8;
            break;
         default:
            break;
      }
   } else {
      //When chip is set to big endian mode ???? This switches all internal registers on a 32 bit
      //boundary to ???? Logic for BEx seems mixed too.
      switch (offset & 3) {
         case 0:
            *REG_CMD = offset | BE2;
            result = *REG_DATA;
            break;
         case 1:
            *REG_CMD = offset | BE3;
            result = *REG_DATA >> 8;
            break;
         case 2:
            *REG_CMD = offset | BE0;
            result = *REG_DATA;
            break;
         case 3:
            *REG_CMD = offset | BE1;
            result = *REG_DATA >> 8;
            break;
         default:
            break;
      }
   }
   return result & 0xff;
}

/**
 * Own read 16 bit registers
 * @param ks
 * @param offset
 */
u16 KSZ8851ReadReg16(struct ks_net * ks, int offset) {
   assert((offset & 1) == 0);

   if (offset & 2) {
      *REG_CMD = offset | (dwordSwapped ? ( BE0 | BE1) : (BE2 | BE3));
   } else {
      *REG_CMD = offset | (dwordSwapped ? ( BE2 | BE3) : (BE0 | BE1));
   }
   return *REG_DATA & 0xffff;
}

/**
 * Own read 16 bit registers
 * @param ks
 * @param offset
 */
u16 ksz8851WriteReg16(struct ks_net * ks, int offset, u16 value) {
   assert((offset & 1) == 0);

   if (offset & 2) {
      *REG_CMD = offset | (dwordSwapped ? ( BE0 | BE1) : (BE2 | BE3));
   } else {
      *REG_CMD = offset | (dwordSwapped ? ( BE2 | BE3) : (BE0 | BE1));
   }
   return *REG_DATA = value;
}

void ksz8851SetBit(struct ks_net * ks, u8 address, u16 mask)
 {
    u16 value;
    //Read current register value
    value = KSZ8851ReadReg16(ks, address);
    //Set specified bits
    ksz8851WriteReg16(ks, address, value | mask);
 }

 void ksz8851ClearBit(struct ks_net * ks, u8 address, u16 mask)
 {
    u16 value;
    //Read current register value
    value = KSZ8851ReadReg16(ks, address);
    //Clear specified bits
    ksz8851WriteReg16(ks, address, value & ~mask);
 }

void printCCR(struct ks_net * ks) {
   u16 val = ks_rdreg16(ks, KS_CCR);
   printf("ChipConfReg: 0x%04x (endianMode=%s,EEPROM=%s,8bit=%s,16bit=%s,48pin=%s)\n",
         val,
         val & CCR_LE     ? "LE" : "BE",
         val & CCR_EEPROM ? "extern" : "intern",
         val & CCR_8BIT   ? "on" : "off",
         val & CCR_16BIT  ? "on" : "off",
         val & CCR_48PIN  ? "yes" : "no"
   );
}

/**
 * Dump registers in 8 bit mode
 * @param ks
 */
void dumpRegister8BitMode(struct ks_net * ks) {
   int i;
   printf("\nRegister dump ( 256 x 8-bit ):\n");
   for (i = 0; i < 256; i++) {
      if (((i % 16) == 0)) {
         if (i != 0) {
            printf("\n");
         }
         printf("%02x: ", i);
      }
      printf("%02x ", KSZ8851ReadReg8(ks, i));
   }
   printf("\n");
}

/**
 * Dump registers in 16 bit mode
 * @param ks
 */
void dumpRegister16BitMode(struct ks_net * ks) {
   int i;
   printf("\nRegister dump ( 128 x 16-bit ):\n");
   for (i = 0; i < 256; i+=2) {
      if (((i % 16) == 0)) {
         if (i != 0) {
            printf("\n");
         }
         printf("%02x: ", i);
      }
      printf("%04x ", KSZ8851ReadReg16(ks, i));
   }
   printf("\n");
}

/**
 * Init the ethernet chip (inclusive probing).
 * @return NO_ERROR if no error occurred
 */
error_t ksz8851Init(struct ks_net * ks) {

   printf("Probing chip: ");
   u16 val = ks_rdreg16(ks, KS_CIDER);
   if ((val & ~CIDER_REV_MASK) != CIDER_ID) {
      printf(" FAILED! Value on register 0xc0 is 0x%04x but should be 0x887x)\n", (val & ~CIDER_REV_MASK));
      return NO_CHIP_FOUND;
   } else {
      printf("ok!\n");
   }

   //Packets shorter than 64 bytes are padded and the CRC is automatically generated
   ksz8851WriteReg16(ks, KS_TXCR, TXCR_TXFCE | TXCR_TXPE | TXCR_TXCRC);

   //Automatically increment TX data pointer
   ksz8851WriteReg16(ks, KS_TXFDPR, TXFDPR_TXFPAI);

   //Configure address filtering
   ksz8851WriteReg16(ks, KS_RXCR1, RXCR1_RXPAFMA | RXCR1_RXFCE | RXCR1_RXBE | RXCR1_RXME | RXCR1_RXUE);

   //No checksum verification
   ksz8851WriteReg16(ks, KS_RXCR2, RXCR2_SRDBL_FRAME | RXCR2_IUFFP | RXCR2_RXIUFCEZ); // orig: RXCR2_SRDBL2 ???

   //Enable automatic RXQ frame buffer dequeue
   ksz8851WriteReg16(ks, KS_RXQCR, RXQCR_RXFCTE | RXQCR_ADRFE);

   //Automatically increment RX data pointer
   ksz8851WriteReg16(ks, KS_RXFDPR, RXFDPR_RXFPAI);

   //Configure receive frame count threshold
   ksz8851WriteReg16(ks, KS_RXFCTR, 1);

   //Force link in half-duplex if auto-negotiation failed
   ksz8851ClearBit(ks, KS_P1CR, P1CR_FORCEFDX);

   //Restart auto-negotiation
   ksz8851SetBit(ks, KS_P1CR, P1CR_RESTARTAN);

   //Clear interrupt flags
   ksz8851SetBit(ks, KS_ISR, IRQ_LCI | IRQ_TXI |
         IRQ_RXI | IRQ_RXOI | IRQ_TXPSI | IRQ_RXPSI | IRQ_TXSAI |
         IRQ_RXWFDI | IRQ_RXMPDI | IRQ_LDI | IRQ_EDI | IRQ_SPIBEI);

   //Configure interrupts as desired
   //ksz8851SetBit(ks, KS_IER, IRQ_LCI | IRQ_TXI | IRQ_RXI);

   //Enable TX operation
   ksz8851SetBit(ks, KS_TXCR, TXCR_TXE);

   //Enable RX operation
   ksz8851SetBit(ks, KS_RXCR1, RXCR1_RXE);

   //Successful initialization
   return NO_ERROR;
}



/**
 * Print current MAC address. TODO: I'm not sure that the direction is correct...
 * @param ks
 */
void printMAC(struct ks_net * ks)
{
   u8 mac[6];

   mac[0] = KSZ8851ReadReg8(ks, KS_MAR(0));
   mac[1] = KSZ8851ReadReg8(ks, KS_MAR(1));
   mac[2] = KSZ8851ReadReg8(ks, KS_MAR(2));
   mac[3] = KSZ8851ReadReg8(ks, KS_MAR(3));
   mac[4] = KSZ8851ReadReg8(ks, KS_MAR(4));
   mac[5] = KSZ8851ReadReg8(ks, KS_MAR(5));

   printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

int main(int argc, char * argv[])
{
   printf(CLRSCR);
   printf("Amiga1200+ KSZ8851-16MLL Service Tool\nVersion %s (build %d, %s, %s)\n", VERSION, build_number, __DATE__, __TIME__);
   printf("Memory base address of ethernet chip: 0x%lx\n", ETHERNET_BASE_ADDRESS);
   printf("Data register at: 0x%lx (16 bit)\n", ETHERNET_BASE_ADDRESS + KS8851_REG_DATA_OFFSET);
   printf("CMD  register at: 0x%lx (16 bit)\n", ETHERNET_BASE_ADDRESS + KS8851_REG_CMD_OFFSET );

   //check all command line...
   if (argc > 1)
   {
      for(int i=1;i<argc;i++)
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
            swappingDataRegValue = true;
         }

         if (strcmp(argv[i], "swapcmd") == 0) {
            swappingCmdRegValue = true;
         }
      }
   }
   else
   {
      printf("\nUsage: %s le eb swapdata swapcmd\n"
            " le: switch chip to little endian mode\n"
            " eb: switch chip to big endian mode\n"
            " swapdata: swap every 16 bit of data register value\n"
            " swapcmd:  swap every 16 bit of cmd register value\n"
            , argv[0]);
      exit(0);
   }
   printf("\n");

   if (switchChipToBigEndian && switchChipToLittleEndian)
   {
      printf(":-) These options together does not make sense!\n");
      exit(0);
   }

   {
      //Init network structure
      struct ks_net * ks = ks8851_init();

      if (swappingDataRegValue) {
         printf("%s-tool is swapping every 16 bit data during chip access.\n", argv[0]);
      }

      //Set chip to big endian?
      if (switchChipToBigEndian) {
         printf("Switching chip to big endian mode.\n");
         u16 oldValue = ks_rdreg16(ks, KS_RXFDPR);
         ks_wrreg16(ks, KS_RXFDPR, oldValue | RXFDPR_EMS); //Bit 11 = 1   => BigEndian
      }

      //Set chip to little endian?
      if (switchChipToLittleEndian) {
         printf("Switching chip to little endian mode.\n");
         u16 oldValue = ks_rdreg16(ks, KS_RXFDPR);
         ks_wrreg16(ks, KS_RXFDPR, oldValue & ~RXFDPR_EMS); //Bit 11 = 0  => LittleEndian
      }

      error_t probing = ksz8851Init(ks);
      if (probing == NO_ERROR)
      {
         printCCR(ks);
         printMAC(ks);

         //
         // Get Link Status:
         //
         u16 val = KSZ8851ReadReg16(ks, KS_P1SR);
         printf("Ethernet Link Status: %s\n", val & P1SR_LINK_GOOD ? "Up" : "Down" );
      }

      //
      // Dump registers...
      //
      dumpRegister8BitMode(ks);
      dumpRegister16BitMode(ks);
   }

   return 0;
}
