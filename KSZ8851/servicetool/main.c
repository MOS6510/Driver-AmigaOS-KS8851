/**
 * Service tool for Ethernet chip KSZ8851 Amiga1200+ project.
 * Author: Heiko Pruessing
 */

#include <clib/alib_protos.h>
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

NetInterface * interface = NULL;

static uint8_t TEST_PACKET[] = "\xff\xff\xff\xff\xff\xff\x38\xc9\x86\x58\x1b\x4a\x08\x06\x00\x01" \
      "\x08\x00\x06\x04\x00\x01\x38\xc9\x86\x58\x1b\x4a\xc0\xa8\x12\x04" \
      "\x00\x00\x00\x00\x00\x00\xc0\xa8\x12\x3b";


#define KS_MAR(_m)            (0x15 - (_m))
#define IRQ_LDI               (1 << 3)


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
BOOL resetCmd = FALSE;


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
 * Processes all received packages (here only print out)
 * @param buffer
 * @param size
 */
void processPacket(MacAddr * src, MacAddr * dst, uint16_t packetType, uint8_t * buffer, uint16_t size ) {
   printf(" Packet src=%02x:%02x:%02x:%02x:%02x:%02x dst=%02x:%02x:%02x:%02x:%02x:%02x\n type=0x%04x\n",

         src->b[0],
         src->b[1],
         src->b[2],
         src->b[3],
         src->b[4],
         src->b[5],

         dst->b[0],
         dst->b[1],
         dst->b[2],
         dst->b[3],
         dst->b[4],
         dst->b[5],

         packetType
                );

   printf(" Packet content (length=%d):\n", size);
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
 * Print current MAC address.
 * @param interface
 */
void printMAC(NetInterface * interface)
{
   MacAddr macaddr;

   macaddr.w[0] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARH) );
   macaddr.w[1] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARM) );
   macaddr.w[2] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARL) );

   printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         macaddr.b[0],macaddr.b[1],macaddr.b[2],macaddr.b[3],macaddr.b[4],macaddr.b[5]);
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
      printf("MIB 0x%02x: 0x%08x\n", MIBRegister, val);
   }
}

void nicNotifyLinkChange(NetInterface * interface) {
   printLinkStatus(interface);
}

//Wait until NIC is connected up...
void waitNICIsUp(NetInterface * interface) {
   do {
      //Wait for any signal
      Wait(0xffffffffl);

      //process all events
      Disable();
      ksz8851EventHandler(interface);
      Enable();

      Delay(10);

      //Check state
   } while(! interface->linkState );
}

/*
 * Helper function: calculates difference of two tick counter...
 */
ULONG ticksDiff(struct DateStamp * d1, struct DateStamp * d2) {
   return (d2->ds_Minute - d1->ds_Minute) * 50 * 60 + (d2->ds_Tick - d1->ds_Tick);
}

/*
 * Send some packets out. Calculate speed.
 */
void sendPackets(NetInterface * interface, int countOfPackets) {

   #define PKT_LEN 1514

   uint8_t buffer[ETH_MAX_FRAME_SIZE];

   //maximal packet size, copy packet
   memcpy(buffer, TEST_PACKET, sizeof(TEST_PACKET));
   //set right mac address
   memcpy(&buffer[6], &interface->macAddr.b[0], 6);

   int pktCount = 0;

   struct DateStamp start;
   struct DateStamp end;

   //disable NIC ints during sending...
   uint16_t oldIER = ksz8851ReadReg(interface, KSZ8851_REG_IER);
   ksz8851WriteReg(interface, KSZ8851_REG_IER, 0);

   //Start measurement...
   DateStamp(&start);

   do {
      error_t status = interface->sendPacket(interface, buffer, PKT_LEN);
      if (status == NO_ERROR) {
         //printf("Pkt send #%d\n", pktCount);
      } else {
         printf("Pkt send failed!\n");
      }

      pktCount++;

   } while (pktCount < countOfPackets);

   //End of measurement...
   DateStamp(&end);

   //Enable NIC ints again...
   ksz8851WriteReg(interface, KSZ8851_REG_IER, oldIER);

   ULONG diffTicks = ticksDiff(&start,&end);
   printf("Ticks: %ld\n", diffTicks);

   ULONG bytesProSec = ((double)countOfPackets * PKT_LEN) * 50.0 / ((double)diffTicks);
   printf("%d pkts sent out! (in %ld ticks => %ld bytes / s)\n", pktCount, diffTicks, bytesProSec );
}

/**
 * Change endianness mode of the NIC
 * @param bigEndian
 */
void switchEndianessMode(bool bigEndian) {
   Ksz8851Context * context = (Ksz8851Context *)interface->nicContext;
   printf("Switching chip to %s mode.\n", bigEndian ? "big endian" : "little endian");
   u16 value = ksz8851ReadReg(interface, KSZ8851_REG_RXFDPR);
   if (bigEndian) {
      value |= RXFDPR_EMS; //Bit 11 = 1   => BigEndian, Bit can't read back!
   }
   ksz8851WriteReg(interface, KSZ8851_REG_RXFDPR, value);
   //Change mode so that all functions still can talk to the chip!
   context->isInBigEndianMode = bigEndian;
}



int main(int argc, char * argv[])
{
   bool send = false;
   int pktSendCnt = 1;

   printf(CLRSCR);
   printf("Amiga1200+ NIC KSZ8851-16MLL Service Tool\nVersion %s (build %d, %s, %s)\n",
         VERSION, build_number, __DATE__, __TIME__);
   printf("Memory base address of NIC ksz8851: 0x%x\n", ETHERNET_BASE_ADDRESS);

   atexit(done);

   interface = (NetInterface*)initModule();
   Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;

   //Add Stack methods (callbacks) to the driver...
   interface->rxPacketFunction   = processPacket;
   interface->linkChangeFunction = nicNotifyLinkChange;

   //First setup: Set a valid Ethernet mac address (not in EEPROM???)
   MacAddr * address = &interface->macAddr;
   address->b[0] = 0x02; //"Local + Individual address"
   address->b[1] = 0x34;
   address->b[2] = 0x56;
   address->b[3] = 0x78;
   address->b[4] = 0x9a;
   address->b[5] = 0xbc;

   //Set the Multicast cache filter:
   MacFilterEntry multicastsFilter[1];
   memset(multicastsFilter,0,sizeof(MacFilterEntry));


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

         if (strcmp(argv[i], "reset") == 0) {
                     resetCmd = true;
                  }

         if (strcmp(argv[i], "send") == 0) {
            send = true;
            //if (argc >= i )
            {
               pktSendCnt = atoi(argv[i+1]);
            }
         }

         if (strcmp(argv[i], "setmulticast") == 0) {



              multicastsFilter[0].refCount = 1;
               //Set "239.12.255.253" => SMA Energy Meter Protocol
               multicastsFilter[0].addr.b[0] = 0x01; //"Local + Individual address"
               multicastsFilter[0].addr.b[1] = 0x0;
               multicastsFilter[0].addr.b[2] = 0x5e;
               multicastsFilter[0].addr.b[3] = 0x0c; //12
               multicastsFilter[0].addr.b[4] = 0xff; //255
               multicastsFilter[0].addr.b[5] = 0xfe; //254
         }

         if (strcmp(argv[i], "help") == 0) {
            printf("\nUsage: %s le eb swapdata swapcmd\n"
                  " le: switch chip to little endian mode\n"
                  " eb: switch chip to big endian mode\n"
                  " swapdata: swap every 16 bit of data register value\n"
                  " swapcmd:  swap every 16 bit of cmd register value\n"
                  " send x:  send small packets\n"
                  " setmulticast: sets the multicast address 239.12.255.254\n"
                  , argv[0]);
            exit(0);
         }
      }
   }

   printf("\n");

   if (switchChipToBigEndian && switchChipToLittleEndian)
   {
      printf(":-) These options together does not make sense!\n");
      exit(0);
   }

   {
      if (swappingDataRegValue) {
         printf("%s-tool is swapping every 16 bit data during chip access.\n", argv[0]);
      }

      // Probe for chip
      error_t probing = interface->init(interface);
      if (probing == NO_ERROR)
      {
         //Updates the multicast filer...
         ksz8851SetMulticastFilter(interface, multicastsFilter, 1);


         if (resetCmd) {
            interface->reset(interface,1); //=> Global Soft Reset!
            exit(0);
         }

         //Set chip to big endian?
         if (switchChipToBigEndian){
            switchEndianessMode(true);
            exit(0);
         }

         //Set chip to little endian?
         if (switchChipToLittleEndian) {
            switchEndianessMode(false);
            exit(0);
         }

         printCCR(interface);
         printMAC(interface);

         //Send packets first?
         if (send) {

            //Wait NIC is up and ready...
            printf("Wait NIC is ready...\n");
            waitNICIsUp(interface);
            printf("Sending packets %d...\n", pktSendCnt);
            sendPackets(interface, pktSendCnt);
         }

         //Processing events (Polling):
         do {
            //Wait for any signal:
            ULONG receivedSignalMask = Wait(0xffffffff);
            printf("Signal: signal=#%ld, overrun=#%ld\n",
                  context->signalCounter, context->rxOverrun);

            //Stop if ctrl-c
            if (receivedSignalMask & SIGBREAKF_CTRL_C)
               break;

            //Process event on event handler...
            Disable();
            interface->processEvents(interface);
            Enable();

            //Print some MIBs
            //printMIB(ks);

            Delay(15);

         } while(true);
      }
   }

   return 0;
}

/**
 * For debugging the hardware near part...
 * @param format
 */
void traceout(char * format, ...) {
   va_list args;
   va_start(args,format);
   vprintf(format, args);
   va_end( args );
}

/**
 * Exit handler. Dump register.
 */
void done(void) {
   interface->deinit(interface);
   interface = NULL;
}
