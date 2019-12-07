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
#include <string.h>

//#include "../servicetool/ks8851.h"
#include "ksz8851.h"
#include "types.h"

int build_number = 1;

static const char * VERSION = "1.2";


// ########################################## LOCAL MACROS ##################################################

#define COLOR02   "\033[32m"
#define COLOR03   "\033[33m"
#define NORMAL    "\033[0m"
#define CLRSCR    "\033[1;1H\033[2J"
#define CURSOR_UP "\033[1A"

#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

// ########################################## EXTERNALS #####################################################

static Ksz8851Context context;
static NetInterface interface = { .nicContext = &context };

// Base address of the KSZ8851 ethernet chip in Amiga memory address space
#define ETHERNET_BASE_ADDRESS (u32)0xd90000l
#define KS8851_REG_DATA_OFFSET 0x00
#define KS8851_REG_CMD_OFFSET  0x02

#define RXFDPR_EMS            (1 << 11)   /* KSZ8851-16MLL */
#define KS_MAR(_m)            (0x15 - (_m))
#define IRQ_LDI               (1 << 3)


#if 1
#define BE3             0x8000      /* Byte Enable 3 */
#define BE2             0x4000      /* Byte Enable 2 */
#define BE1             0x2000      /* Byte Enable 1 */
#define BE0             0x1000      /* Byte Enable 0 */
#endif

void done(void);

#define ETH_MAX_FRAME_SIZE   1518

u32 frameId = 0;

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
u16 swap(u16 value)
{
   register u16 h = (value >> 8) & 0xff;
   register u16 l = value & 0xff;
   return (l << 8) | h;
}
int netBufferGetLength(const NetBuffer * buffer) {
   return buffer->len;
}

void netBufferRead(const uint8_t * srcBuffer, const NetBuffer * buffer, int offset, int length) {
   //TODO:
}

uint16_t htons(uint16_t hostshort) {
   return hostshort;
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
u8 KSZ8851ReadReg8(NetInterface * ks, int offset) {
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
uint16_t ksz8851ReadReg(NetInterface *interface, uint8_t offset)
{
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
void ksz8851WriteReg(NetInterface *interface, uint8_t offset, uint16_t value)
{
   assert((offset & 1) == 0);

   if (offset & 2) {
      *REG_CMD = offset | (dwordSwapped ? ( BE0 | BE1) : (BE2 | BE3));
   } else {
      *REG_CMD = offset | (dwordSwapped ? ( BE2 | BE3) : (BE0 | BE1));
   }
   *REG_DATA = value;
}

void printCCR(NetInterface* ks) {
   u16 val = ksz8851ReadReg(ks, KSZ8851_REG_CCR);
   printf("ChipConfReg: 0x%04x (endianMode=%s,EEPROM=%s,8bit=%s,16bit=%s,48pin=%s)\n",
         val,
         val & CCR_BUS_ENDIAN_MODE     ? "LE" : "BE",
               val & CCR_EEPROM_PRESENCE  ? "extern" : "intern",
                     val & CCR_8_BIT_DATA_BUS  ? "on" : "off",
                           val & CCR_16_BIT_DATA_BUS  ? "on" : "off",
                                 val & CCR_48_PIN_PACKAGE ? "yes" : "no"
   );
}

/**
 * Dump registers in 8 bit mode
 * @param ks
 */
void dumpRegister8BitMode(NetInterface * ks) {
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
void dumpRegister16BitMode(NetInterface *interface) {
   int i;
   printf("\nRegister dump ( 128 x 16-bit ):\n");
   for (i = 0; i < 256; i+=2) {
      if (((i % 16) == 0)) {
         if (i != 0) {
            printf("\n");
         }
         printf("%02x: ", i);
      }
      printf("%04x ", ksz8851ReadReg(interface, i));
   }
   printf("\n");
}

void ksz8851_SoftReset(NetInterface *interface, unsigned op)
{
   printf("   ksz8851_SoftReset()\n");
   /* Disable interrupt first */
   u16 oldisr = ksz8851ReadReg(interface, KSZ8851_REG_ISR);

   ksz8851WriteReg(interface, KSZ8851_REG_ISR, 0x0000);
   ksz8851WriteReg(interface,  KSZ8851_REG_GRR, op);
   Delay(50);
   //mdelay(50); /* wait a short time to effect reset */
   ksz8851WriteReg(interface,  KSZ8851_REG_GRR, 0);
   //mdelay(50);  /* wait for condition to clear */
   Delay(50);

   //Re-enable all int flags again...
   ksz8851WriteReg(interface, KSZ8851_REG_ISR, oldisr);
}

void processPacket(const uint8_t * buffer, int size) {
   printf("Packet content (length=%d):\n", size);
   for (int i = 0; i < size; i++) {
      if (((i % 16) == 0)) {
         if (i != 0) {
            printf("\n");
         }
         printf("%04x: ", i);
      }
      printf("%02x ", buffer[i]);
   }
   printf("\n");
}

/**
 * Print current MAC address. TODO: I'm not sure that the direction is correct...
 * @param ks
 */
void printMAC(NetInterface * ks)
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

/**
 * Display Link status
 * @param ks
 */
void printLinkStatus(NetInterface * ks) {
   u16 val = ksz8851ReadReg(ks,  KSZ8851_REG_P1SR);
   printf("   Ethernet Link Status: %s", val & P1SR_LINK_GOOD ? "UP" : "DOWN");
   if (val & P1SR_LINK_GOOD) {
      printf(" (%s, %s)", //
            val & P1SR_OPERATION_DUPLEX ? "duplex" : "half duplex", //
            val & P1SR_OPERATION_SPEED ? "100Mbps" : "10Mbps");
   } else {
      printf("                 ");
   }
   printf("\n");
}

/**
 * prints some MIB counter...
 * @param ks
 */
void printMIB(NetInterface * ks) {

   //"MIB0" => "Rx octet count including bad packets" (32bit)
   u8 MIBRegister = 0;
   for (MIBRegister = 0; MIBRegister <= 0x00 /*0x1f*/; MIBRegister++) {
      ksz8851WriteReg(ks, KSZ8851_REG_IACR, MIBRegister);
      u32 h = ksz8851ReadReg(ks, KSZ8851_REG_IADHR);
      u32 l = ksz8851ReadReg(ks, KSZ8851_REG_IADLR);
      u32 val = h << 16 | l;
      printf("MIB 0x%02x: 0x%08lx\n", MIBRegister, val);
   }
}


int main(int argc, char * argv[])
{
   bool looping = false;
   bool send = false;

   atexit(done);

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

         if (strcmp(argv[i], "loop") == 0) {
            looping = true;
         }

         if (strcmp(argv[i], "send") == 0) {
                    send = true;
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
            " loop:  stay until key pressed. reporting link states\n"
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
      //ks = ks8851_init();

      if (swappingDataRegValue) {
         printf("%s-tool is swapping every 16 bit data during chip access.\n", argv[0]);
      }

      //Set chip to big endian?
      if (switchChipToBigEndian) {
         printf("Switching chip to big endian mode.\n");
         u16 oldValue = ksz8851ReadReg(&interface, KSZ8851_REG_RXFDPR);
         ksz8851WriteReg(&interface, KSZ8851_REG_RXFDPR, oldValue | RXFDPR_EMS); //Bit 11 = 1   => BigEndian
      }

      //Set chip to little endian?
      if (switchChipToLittleEndian) {
         printf("Switching chip to little endian mode.\n");
         u16 oldValue = ksz8851ReadReg(&interface, KSZ8851_REG_RXFDPR);
         ksz8851WriteReg(&interface, KSZ8851_REG_RXFDPR, oldValue & ~RXFDPR_EMS); //Bit 11 = 0  => LittleEndian
      }

      //
      // Probe for chip
      //
      error_t probing = ksz8851Init(&interface);
      if (probing == NO_ERROR)
      {
         printCCR(&interface);
         printMAC(&interface);

         //Sending packets???
         if (send) {
            while(1) {
               if ( NO_ERROR == ksz8851SendPacket(&interface,NULL,0)) {
                  printf("Pkt send...\n");
               } else {
                  printf("Pkt send failed!\n");
               }
            }
         }

         //Processing events (Polling):
         do {
            {
               //Service all interrupts: (polling)
               u16 isr = ksz8851ReadReg(&interface, KSZ8851_REG_ISR);
               if (isr != 0) {

                  //"Link change" interrupt?
                  if (isr & ISR_LCIS) {
                     printf("### =>Link Change Interrupt detected!\n");
                     ksz8851SetBit(&interface, KSZ8851_REG_ISR, ISR_LCIS); //ACK with set same to 1

                     printLinkStatus(&interface);
                     continue;
                  }

                  //"Wakeup from linkup" interrupt?
                  if (isr & IRQ_LDI) {
                     printf("### =>Wakeup from linkup detected!\n");
                     ksz8851SetBit(&interface, KSZ8851_REG_PMECR, 0x2 << 2); //clear event with only PMECR "0010" => [5:2]

                     printLinkStatus(&interface);
                     continue;
                  }

                  if (isr & ISR_RXIS) {
                     printf("### => RX interrupt!!\n");
                     ksz8851SetBit(&interface, KSZ8851_REG_ISR, ISR_RXIS);

                     ksz8851ReceivePacket(&interface, processPacket);
                     continue;
                  }

                  if (isr & ISR_RXOIS) {
                     printf("### => RX Overrun!!\n");
                     ksz8851SetBit(&interface, KSZ8851_REG_ISR, ISR_RXOIS);
                     ksz8851_SoftReset(&interface,1);
                     continue;
                  }

                  printf("### => unknown interrupt 0x%04x\n", isr);
                  Delay(12.5);
                  continue;
               }

               //Print some MIBs
               //printMIB(ks);

               //Check "RECEIVE FRAME HEADER STATUS REGISTER"...
               //u16 readStatus = ksz8851ReadReg16(ks, KS_RXFHSR);
               //printf("KS_RXFHSR = 0x%04x\n", readStatus);


               //RX FRAME COUNT AND THRESHOLD REGISTER
               //u16 frameCounter = ksz8851ReadReg16(ks, KS_RXFCTR);
               //printf("KS_RXFCTR = 0x%04x\n", frameCounter);

               //Read FRAME HEADER BYTE COUNT REGISTER...
               //u16 pktLength = ksz8851ReadReg16(ks, KS_RXFHBCR);
               //printf("KS_RXFHBCR= 0x%04x\n", pktLength);
            }

            //3 cursor up
            //printf("\033[1A");

            Delay(12.5);

         } while(looping);
      }
   }

   return 0;
}

void done(void) {
   //
   // Dump registers...
   //
   //dumpRegister8BitMode(&interface);
   dumpRegister16BitMode(&interface);
}
