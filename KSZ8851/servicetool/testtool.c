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
#define CURSOR_UP "\033[1A"


#define TEST_ENTERED() printf("%s...", __func__);
#define TEST_LEAVE(a)  printf("%s\n", (a) ? "success" : "failed!");

typedef enum  {
   NO_ERROR = 0,
   NO_CHIP_FOUND,
   ERROR_INVALID_PACKET,
   ERROR_INVALID_LENGTH,
   ERROR_FAILURE
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
void ksz8851_SoftReset(struct ks_net *ks, unsigned op);
void ksz8851WriteFifo(struct ks_net * ks , const u8 * data, size_t length);

extern struct ks_net * ks8851_init();
void done(void);

#define TX_CTRL_TXIC      0x8000
#define TX_CTRL_TXFID     0x003F
#define TXMIR_TXMA_MASK   0x1FFF


typedef struct {

} NetBuffer;

typedef struct
 {
    u16 controlWord;
    u16 byteCount;
 } Ksz8851TxHeader;

 u32 frameId = 0;


 /**
  * @brief RX packet header
  **/

 typedef struct
 {
    u16 statusWord;
    u16 byteCount;
 } Ksz8851RxHeader;


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
u16 ksz8851ReadReg16(struct ks_net * ks, int offset) {
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
    value = ksz8851ReadReg16(ks, address);
    //Set specified bits
    ksz8851WriteReg16(ks, address, value | mask);
 }

 void ksz8851ClearBit(struct ks_net * ks, u8 address, u16 mask)
 {
    u16 value;
    //Read current register value
    value = ksz8851ReadReg16(ks, address);
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
      printf("%04x ", ksz8851ReadReg16(ks, i));
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

   //First: soft reset:
   //ksz8851_SoftReset(ks, GRR_GSR);

   //Packets shorter than 64 bytes are padded and the CRC is automatically generated
   ksz8851WriteReg16(ks, KS_TXCR, TXCR_TXFCE | TXCR_TXPE | TXCR_TXCRC);

   //Automatically increment TX data pointer
   ksz8851WriteReg16(ks, KS_TXFDPR, TXFDPR_TXFPAI);

   //Configure address filtering (but not started yet) (Receive Control Register 1) RXCR1_RXAE
   //ksz8851WriteReg16(ks, KS_RXCR1, RXCR1_RXPAFMA | RXCR1_RXFCE | RXCR1_RXBE | RXCR1_RXME | RXCR1_RXUE);
   //HP: Enable to receive EVERY packet
   ksz8851WriteReg16(ks, KS_RXCR1, RXCR1_RXAE);

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

   //Restart auto-negotiation (seems to need time time, stops the links for some amount of time)
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

#define ETH_MAX_FRAME_SIZE   1518


void ksz8851_SoftReset(struct ks_net *ks, unsigned op)
{
   /* Disable interrupt first */
   ksz8851WriteReg16(ks, KS_IER, 0x0000);
   ksz8851WriteReg16(ks, KS_GRR, op);
   mdelay(10); /* wait a short time to effect reset */
   ksz8851WriteReg16(ks, KS_GRR, 0);
   mdelay(1);  /* wait for condition to clear */
}


/**
 * Read packets
 * @param ks
 * @return
 */
error_t ksz8851ReceivePacket(struct ks_net * ks)
 {
    size_t n;
    u16 status;
    u16 isr;

    //Read received frame status from RXFHSR
    status = ksz8851ReadReg16(ks, KS_RXFHSR);
    isr    = ksz8851ReadReg16(ks, KS_ISR);

    printf("KS_RXFHSR = 0x%04x, ISR = 0x%04x  \n", status, isr);

    //Make sure the frame is valid
    if(status & RXFSHR_RXFV)
    {
       printf("Something there...\n");
       //Check error flags
       if(!(status & (RXFSHR_RXMR | RXFSHR_RXFTL | RXFSHR_RXRF | RXFSHR_RXCE)))
       {
          printf("Something there...2\n");

          //Read received frame byte size from RXFHBCR
          n = ksz8851ReadReg16(ks, KS_RXFHBCR) & RXFHBCR_CNT_MASK;

          //Ensure the frame size is acceptable
          if(n > 0 && n <= ETH_MAX_FRAME_SIZE)
          {
             printf("Something there...3\n");

             //Reset QMU RXQ frame pointer to zero
             ksz8851WriteReg16(ks, KS_RXFDPR, RXFDPR_RXFPAI);
             //Enable RXQ read access
             ksz8851SetBit(ks, KS_RXQCR, RXQCR_SDA);

             //Read data
             //ksz8851ReadFifo(ks, context->rxBuffer, n);

             //printf("read something...\n");

             //End RXQ read access
             ksz8851ClearBit(ks, KS_RXQCR, RXQCR_SDA);

             //Pass the packet to the upper layer
             //nicProcessPacket(ks, context->rxBuffer, n);

             //Valid packet received
             return NO_ERROR;
          } else {
             printf("NAK 3 \n");
          }
       }
       else {
          printf("NAK 2 \n");
       }
    }
    else {
       //printf("NAK 1 \n");
    }

    //Release the current error frame from RXQ
    //ksz8851SetBit(ks, KS_RXQCR, RXQCR_RRXEF);

    //Report an error
    return ERROR_INVALID_PACKET;
 }


error_t ksz8851SendPacket(struct ks_net * ks, const NetBuffer * buffer, size_t offset)
 {
    size_t n;
    size_t length = 1518; //85;
    Ksz8851TxHeader header;

    //u8 txBuffer[1518];
    //memset(txBuffer,length, 0x18);

    u8 * txBuffer = "\x38\x10\xd5\x0e\x8e\x78\x38\xc9\x86\x58\x1b\x4a\x08\x00\x45\x00" \
    "\x00\x47\x79\xdb\x00\x00\xff\x11\x00\x00\xc0\xa8\x12\x04\xc0\xa8" \
    "\x12\x01\xf6\xee\x00\x35\x00\x33\xa5\x9a\x7d\xc6\x01\x00\x00\x01" \
    "\x00\x00\x00\x00\x00\x00\x05\x65\x34\x35\x31\x38\x04\x64\x73\x63" \
    "\x78\x0a\x61\x6b\x61\x6d\x61\x69\x65\x64\x67\x65\x03\x6e\x65\x74" \
    "\x00\x00\x01\x00\x01";


    //Retrieve the length of the packet
    //length = netBufferGetLength(buffer) - offset;

    //Check the frame length
    if(length > ETH_MAX_FRAME_SIZE)
    {
       //The transmitter can accept another packet
       //osSetEvent(&interface->nicTxEvent);
       //Report an error
       printf("pkt too long!\n");
       return ERROR_INVALID_LENGTH;
    }

    //Get the amount of free memory available in the TX FIFO
    n = ksz8851ReadReg16(ks, KS_TXMIR) & TXMIR_TXMA_MASK;

    //Make sure enough memory is available
    if((length + 8) > n)
    {
       printf("no more free mem on chip...\n");
       return ERROR_FAILURE;
    }

    //Copy user data
    //netBufferRead(context->txBuffer, buffer, offset, length);

    //Format control word
    header.controlWord = TX_CTRL_TXIC | (frameId++ & TX_CTRL_TXFID);
    //Total number of bytes to be transmitted
    header.byteCount = length;

    //Enable TXQ write access
    ksz8851SetBit(ks, KS_RXQCR, RXQCR_SDA);
    //Write TX packet header
    ksz8851WriteFifo(ks, (u8 *) &header, sizeof(Ksz8851TxHeader));
    //Write data
    ksz8851WriteFifo(ks, txBuffer, length);
    //End TXQ write access
    ksz8851ClearBit(ks, KS_RXQCR, RXQCR_SDA);

    //Start transmission
    ksz8851SetBit(ks, KS_TXQCR, TXQCR_METFE);

    //Successful processing
    return NO_ERROR;
 }

void ksz8851WriteFifo(struct ks_net * ks , const u8 * data, size_t length)
{
    u16 i;

    //Data phase
    for(i = 0; i < length; i+=2)
       *REG_DATA = data[i] | data[i+1]<<8;

    //Maintain alignment to 4-byte boundaries
    for(; i % 4; i+=2)
       *REG_DATA = 0x0000;
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

/**
 * Display Link status
 * @param ks
 */
void printLinkStatus(struct ks_net * ks) {
   u16 val = ksz8851ReadReg16(ks, KS_P1SR);
   printf("Ethernet Link Status: %s", val & P1SR_LINK_GOOD ? "UP" : "DOWN");
   if (val & P1SR_LINK_GOOD) {
      printf(" (%s, %s)", //
            val & P1SR_OP_FDX ? "duplex" : "half duplex", //
            val & val & P1SR_OP_100M ? "100Mbps" : "10Mbps");
   } else {
      printf("                 ");
   }
   printf("\n");
}

/**
 * prints some MIB counter...
 * @param ks
 */
void printMIB(struct ks_net * ks) {

   //"MIB0" => "Rx octet count including bad packets" (32bit)
   u8 MIBRegister = 0;
   for (MIBRegister = 0; MIBRegister <= 0x00 /*0x1f*/; MIBRegister++) {
      ksz8851WriteReg16(ks, KS_IACR, MIBRegister);
      u32 h = ksz8851ReadReg16(ks, KS_IAHDR);
      u32 l = ksz8851ReadReg16(ks, KS_IADLR);
      u32 val = h << 16 | l;
      printf("MIB 0x%02x: 0x%08lx\n", MIBRegister, val);
   }
}

struct ks_net * ks = NULL;

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
      ks = ks8851_init();

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

      //
      // Probe for chip
      //
      error_t probing = ksz8851Init(ks);
      if (probing == NO_ERROR)
      {
         printCCR(ks);
         printMAC(ks);

         //Sending packets???
         if (send) {
            while(1) {
               if ( NO_ERROR == ksz8851SendPacket(ks,NULL,0)) {
                  printf("Pkt send...\n");
               } else {
                  printf("Pkt send failed!\n");
               }
            }
         }

         //Reporting endless:
         do {

            //report link status
            printLinkStatus(ks);

            //
            // Check for incoming packets (poll)...
            //
            /*if (NO_ERROR == ksz8851ReceivePacket(ks)) {
               printf("\nRead a packet!\n\n");
            }*/
            {
               u16 status = ksz8851ReadReg16(ks, KS_RXFHSR);
               printf("KS_RXFHSR = 0x%04x\n", status);

               //Service all interrupts: (polling)
               u16 isr = ksz8851ReadReg16(ks, KS_ISR);
               printf("KS_ISR    = 0x%04x  \n", isr);
               if (isr != 0) {
                  //"Link change" interrupt?
                  if (isr & IRQ_LCI) {
                     printf("                                            Link Change Interrupt detected!\n");
                     ksz8851SetBit(ks, KS_ISR, IRQ_LCI); //ACK with set same to 1
                  }
                  //"Wakeup from linkup" interrupt?
                  if (isr & IRQ_LDI) {
                     printf("                                            Wakeup from linkup detected!\n\n");
                     ksz8851SetBit(ks, KS_PMECR, 0x2 << 2); //clear event with only PMECR "0010" => [5:2]
                  }
                  continue;
               }

               //Print some MIBs
               printMIB(ks);
            }

            //3 cursor up
            printf("\033[4A");

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
   dumpRegister8BitMode(ks);
   dumpRegister16BitMode(ks);
}
