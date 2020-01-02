/*
 * KSZ8851 Amiga Network Driver
 */

#include "ksz8851.h"
#include "isr.h"

//Do not use these functions. They cause linker errors on the device...
#define printf(...) DoNotusePrintf
#define assert(...) DoNotusePrintf

/**
  * Temporary disable all ints from NIC. Return old IER mask...
  * @param interface
  * @return
  */
 static uint16_t ksz8851DisableInterrupts(NetInterface * interface) {
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;
    Disable();
    context->intDisabledCounter++;
    uint16_t oldMask = ksz8851ReadReg(interface, KSZ8851_REG_IER);
    Enable();
    return oldMask;
 }

 /**
  * Enable all NIC its again (after disable)
  * @param interface
  * @param ierMask
  */
 static void ksz8851EnableInterrupts(NetInterface * interface, uint16_t ierMask) {
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;
    Disable();
    context->intDisabledCounter--;
    ksz8851SetBit(interface, KSZ8851_REG_IER, ierMask);
    Enable();
 }

/**
 * @brief Enable interrupts (must be called from right Amiga Task!)
 * @param[in] interface Underlying network interface
 **/
void ksz8851Online(NetInterface *interface)
{
   installInterruptHandler(interface);

   //Clear interrupt flags
   ksz8851SetBit(interface, KSZ8851_REG_ISR, ISR_LCIS | ISR_TXIS |
         ISR_RXIS | ISR_RXOIS | ISR_TXPSIS | ISR_RXPSIS | ISR_TXSAIS |
         ISR_RXWFDIS | ISR_RXMPDIS | ISR_LDIS | ISR_EDIS | ISR_SPIBEIS);

   //Configure interrupts as desired (this enables interrupts. Be sure that the ISR is installed!)
   ksz8851SetBit(interface, KSZ8851_REG_IER, IER_LCIE | IER_TXIE | IER_RXIE | IER_RXOIE);

   //Enable TX operation
   ksz8851SetBit(interface, KSZ8851_REG_TXCR, TXCR_TXE);

   //Enable RX operation
   ksz8851SetBit(interface, KSZ8851_REG_RXCR1, RXCR1_RXE);
}


/**
 * @brief Disable interrupts
 * @param[in] interface Underlying network interface
 **/
void ksz8851Offline(NetInterface *interface)
{
   //Disable all NIC interrupts to release the interrupt line
   Disable();
   ksz8851WriteReg(interface, KSZ8851_REG_IER, 0);
   //Disable TX operation
   ksz8851ClearBit(interface, KSZ8851_REG_TXCR, TXCR_TXE);
   //Enable RX operation
   ksz8851ClearBit(interface, KSZ8851_REG_RXCR1, RXCR1_RXE);
   Enable();

   uninstallInterruptHandler(interface);
}

static void ksz8851PrintNICEndiness(Ksz8851Context * context) {
    TRACE_INFO("Current Endian Mode: %s\n", context->isInBigEndianMode ? "BE" : "LE");
 }

/**
 * Probes for the NIC, detect which mode is used (big endian oder little endian)
 * @param interface
 * @return
 */
static error_t ksz8851DetectNICEndiness(NetInterface *interface) {

   Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

   //Check in which mode the NIC is now
   if (ksz8851ReadReg(interface, KSZ8851_REG_CIDER) != KSZ8851_REV_A3_ID) {
      context->isInBigEndianMode = !context->isInBigEndianMode;
      if (ksz8851ReadReg(interface, KSZ8851_REG_CIDER) != KSZ8851_REV_A3_ID) {
         return NO_CHIP_FOUND;
      }
   }
   return NO_ERROR;
}


 /**
  * @brief KSZ8851 controller initialization
  * @param[in] interface Underlying network interface
  * @return Error code
  **/
error_t ksz8851Init(NetInterface *interface)
 {
    //Point to the driver context
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

    //Debug message
    TRACE_INFO("Initializing KSZ8851 Ethernet controller...\r\n");

    //Try to detect NIC and the current endian mode...
    uint8_t tries = 2;
    do {
       if (ksz8851DetectNICEndiness(interface)) {
          break;
       }
       TRACE_INFO("Problem to detect NIC and his endian mode. Hopefully leaving reset state...\n");
       context->isInBigEndianMode = true;
       ksz8851WriteReg(interface, KSZ8851_REG_GRR, 0);
       context->isInBigEndianMode = false;
       ksz8851WriteReg(interface, KSZ8851_REG_GRR, 0);
       //leaving in LE mode here...
    } while(tries--);

    //Make a QMU receiver reset only (hopefully keeps endian mode)
    ksz8851SoftReset(interface,2);

    //Re-detect NIC after reset...
    if (ksz8851ReadReg(interface, KSZ8851_REG_CIDER) != KSZ8851_REV_A3_ID) {
       TRACE_INFO("Unable to detect NIC!\n");
       ksz8851DumpReg(interface);
       return ERROR_WRONG_IDENTIFIER;
    }

    ksz8851PrintNICEndiness(context);

    TRACE_DEBUG("CIDER=0x%04"   PRIX16 "\r\n", (ULONG)ksz8851ReadReg(interface, KSZ8851_REG_CIDER));
    TRACE_DEBUG("PHY1ILR=0x%04" PRIX16 "\r\n", (ULONG)ksz8851ReadReg(interface, KSZ8851_REG_PHY1ILR));
    TRACE_DEBUG("PHY1IHR=0x%04" PRIX16 "\r\n", (ULONG)ksz8851ReadReg(interface, KSZ8851_REG_PHY1IHR));

    //Initialize driver specific variables
    context->frameId = 0;

    //Allocate TX and RX buffers (4 bytes more because DMA is always DWORD aligned, so transfer could be 3 bytes longer)
    context->rxBuffer = memPoolAlloc(ETH_MAX_FRAME_SIZE + 4);

    //Failed to allocate memory?
    if(context->rxBuffer == NULL)
    {
       //Clean up side effects
       memPoolFree(context->rxBuffer);

       return ERROR_OUT_OF_MEMORY;
    }

    //Initialize MAC address
    ksz8851WriteReg(interface, KSZ8851_REG_MARH, htons(interface->macAddr.w[0]));
    ksz8851WriteReg(interface, KSZ8851_REG_MARM, htons(interface->macAddr.w[1]));
    ksz8851WriteReg(interface, KSZ8851_REG_MARL, htons(interface->macAddr.w[2]));

    //Packets shorter than 64 bytes are padded and the CRC is automatically generated
    ksz8851WriteReg(interface, KSZ8851_REG_TXCR, TXCR_TXFCE | TXCR_TXPE | TXCR_TXCE);
    //Automatically increment TX data pointer
    ksz8851WriteReg(interface, KSZ8851_REG_TXFDPR, TXFDPR_TXFPAI);

    //Configure address filtering
    ksz8851WriteReg(interface, KSZ8851_REG_RXCR1,
       RXCR1_RXPAFMA | RXCR1_RXFCE | RXCR1_RXBE | RXCR1_RXME | RXCR1_RXUE);

    //No checksum verification
    ksz8851WriteReg(interface, KSZ8851_REG_RXCR2,
       RXCR2_SRDBL2 | RXCR2_IUFFP | RXCR2_RXIUFCEZ);

    //Enable automatic RXQ frame buffer dequeue
    //+ add 2 extra dummy byte (before dest address) for 4-byte alignment of packet data (RXQCR_RXIPHTOE):
    ksz8851WriteReg(interface, KSZ8851_REG_RXQCR, RXQCR_RXFCTE | RXQCR_ADRFE | RXQCR_RXIPHTOE);
    //Automatically increment RX data pointer
    uint16_t val = context->isInBigEndianMode ? (RXFDPR_RXFPAI | RXFDPR_EMS) : (RXFDPR_RXFPAI);
    ksz8851WriteReg(interface, KSZ8851_REG_RXFDPR, val);
    //Configure receive frame count threshold
    ksz8851WriteReg(interface, KSZ8851_REG_RXFCTR, 1);

    //Force link in half-duplex if auto-negotiation failed
    ksz8851ClearBit(interface, KSZ8851_REG_P1CR, P1CR_FORCE_DUPLEX);
    //Restart auto-negotiation
    ksz8851SetBit(interface, KSZ8851_REG_P1CR, P1CR_RESTART_AN);

    // At this point all ints are still not activated. Do this only when the interrupt handler is installed.

    //Successful initialization
    return NO_ERROR;
 }

 /**
  * Reset the NIC.
  * @param interface
  * @param op 1: Global Soft Reset, 2: QMU only reset....
  */
 void ksz8851SoftReset(NetInterface *interface, uint8_t resetOperation)
 {
    if (!ksz8851DetectNICEndiness(interface)) {
       return;
    }

    /* Disable interrupt first */
    uint16_t oldisr = ksz8851ReadReg(interface, KSZ8851_REG_ISR);
    ksz8851WriteReg(interface, KSZ8851_REG_ISR, 0x0000);

    //Perform reset command
    ksz8851WriteReg(interface, KSZ8851_REG_GRR, resetOperation);
    /* wait a short time to effect reset */
    Delay(25);
    //release soft reset again
    ksz8851WriteReg(interface, KSZ8851_REG_GRR, 0);
    /* wait for condition to clear */
    Delay(10);

    //Detect endiness after reset again...
    if (!ksz8851DetectNICEndiness(interface)) {
       return;
    }

    //Re-enable all interrupt flags again...
    ksz8851WriteReg(interface, KSZ8851_REG_ISR, oldisr);
 }

 /**
  * @brief KSZ8851 interrupt service routine (interrupt code)
  * @param[in] interface Underlying network interface
  * @return TRUE if a higher priority task must be woken. Else FALSE is returned
  **/
 bool_t ksz8851IrqHandler(register NetInterface *interface)
 {
    bool_t signaled;
    uint16_t ier;
    uint16_t isr;
    signaled = FALSE;

    //
    // The following 2 lines changes the command register! This is done by interrupt handler
    // So it is always unsure to access the NIC registers with enabled interrupts!
    //

    //Read interrupt status register
    isr = ksz8851ReadReg(interface, KSZ8851_REG_ISR);
    //Save IER register value
    ier = ksz8851ReadReg(interface, KSZ8851_REG_IER);

    //Because we here can be called from not only our own interrupts, we have to check if the signaled
    //hardware interrupt of the NIC is enabled by the NIC. If not, it's not our interrupt...
    if (0 == (isr & ier)) {
       //Interrupt is not from our hardware
       return false;
    }

    //
    // Interrupt is definitely from our hardware !
    //

    //Disable all interrupts to release the Amiga interrupt line
    ksz8851WriteReg(interface, KSZ8851_REG_IER, 0);

    //Link status change?
    if(isr & ISR_LCIS)
    {
       //Disable LCIE interrupt
       ier &= ~IER_LCIE;

       //Set event flag
       //interface->nicEvent = TRUE;
       //Notify the TCP/IP stack of the event
       //flag |= osSetEventFromIsr(&netEvent);
       signaled = TRUE;
    }

    //Packet transmission complete?
    if(isr & ISR_TXIS)
    {
       //Clear interrupt flag
       ksz8851WriteReg(interface, KSZ8851_REG_ISR, ISR_TXIS);

       ier &= ~IER_TXIE;

       //Notify the TCP/IP stack that the transmitter is ready to send
       //flag |= osSetEventFromIsr(&interface->nicTxEvent);
       signaled = TRUE;
    }

    //Packet received?
    if(isr & ISR_RXIS)
    {
       //Disable RXIE interrupt
       ier &= ~IER_RXIE;

       //Set event flag
       //interface->nicEvent = TRUE;
       //Notify the TCP/IP stack of the event
       //flag |= osSetEventFromIsr(&netEvent);
       signaled = TRUE;
    }

    //Overrun rx packets?
    if (isr & ISR_RXOIS) {

      ier &= ~IER_RXOIE;

      signaled = TRUE;
    }

    //Link-up status detect
    if (isr & ISR_LDIS) {

       //clear event with only PMECR "0010" => [5:2]
       ksz8851SetBit(interface, KSZ8851_REG_PMECR, 0x2 << 2);

       ier &= ~IER_LDIE;

       signaled = TRUE;
    }

    //Re-enable all not handles ints again. All detected ints are still disabled and must be re-enabled later!
    ksz8851WriteReg(interface, KSZ8851_REG_IER, ier);

    //true: It's our interrupt event, false: not our interrupt
    return signaled;
 }


 /**
  * @brief KSZ8851 event handler (called from non-isr, Amiga task)
  * @param[in] interface Underlying network interface
  **/
 bool ksz8851EventHandler(NetInterface *interface)
 {
    uint16_t status;
    uint8_t  frameCount;
    uint16_t enableMask = 0;

    //
    // All occurred NIC ints are still disabled when the method is called. but other ints can be still
    // process!
    //

    //Because the function can be called at any time and an interrupt call chan change register access
    //we must ensure, that we have exclusive access to the NIC registers by disabling all ints...
    uint16_t oldIER = ksz8851DisableInterrupts(interface);

    //Read interrupt status register
    status = ksz8851ReadReg(interface, KSZ8851_REG_ISR);

    //Check whether the link status has changed?
    if(status & ISR_LCIS)
    {
       //Clear interrupt flag
       ksz8851WriteReg(interface, KSZ8851_REG_ISR, ISR_LCIS);

       //Read PHY status register
       status = ksz8851ReadReg(interface, KSZ8851_REG_P1SR);

       //Check link state
       if(status & P1SR_LINK_GOOD)
       {
          //Get current speed
          if(status & P1SR_OPERATION_SPEED)
             interface->linkSpeed = NIC_LINK_SPEED_100MBPS;
          else
             interface->linkSpeed = NIC_LINK_SPEED_10MBPS;

          //Determine the new duplex mode
          if(status & P1SR_OPERATION_DUPLEX)
             interface->duplexMode = NIC_FULL_DUPLEX_MODE;
          else
             interface->duplexMode = NIC_HALF_DUPLEX_MODE;

          //Link is up
          interface->linkState = TRUE;
       }
       else
       {
          //Link is down
          interface->linkState = FALSE;
       }

       enableMask |= IER_LCIE;

       if (interface->linkChangeFunction) {
          interface->linkChangeFunction(interface);
       }
    }

    //Check whether a packet has been received?
    if(status & ISR_RXIS)
    {
       //ACK (Clear) RX interrupt
       ksz8851WriteReg(interface, KSZ8851_REG_ISR, ISR_RXIS);

       //Get the total number of frames that are pending in the buffer (bits 15-8)
       uint16_t rxfctr = ksz8851ReadReg(interface, KSZ8851_REG_RXFCTR);
       frameCount = MSB(rxfctr);
       TRACE_INFO(" FrameCount: %ld\n", (ULONG)frameCount);

       //Process all pending packets (0-255)
       while(frameCount > 0)
       {
          //Read incoming packet, process with callback...
          ksz8851ReceivePacket(interface);

          frameCount--;
       }
       enableMask |= IER_RXIE;
    }

    //Receiver overruns?
    if (status & ISR_RXOIS) {
       ((Ksz8851Context*)interface->nicContext)->rxOverrun++;

       //ACK receiver overrun status by writing "1" to status...
       ksz8851SetBit(interface, KSZ8851_REG_ISR, ISR_RXOIS);
       TRACE_INFO(" =>>>> ACK Receiver Overrun\n");

       //Reset NIC! (scheint so nichts zu nutzen! NIC bleibt im Overrun)
       //TODO: Check how to reset overruns...
       //ksz8851SoftReset(interface, 2);

       enableMask |= IER_RXOIE;
    }

    //Wakeup from something?
    if (status & IER_LDIE) {
       if (interface->linkChangeFunction) {
          interface->linkChangeFunction(interface);
       }
       enableMask |= IER_LDIE;
    }

    //Re-enable all handled interrupts again...
    //ksz8851SetBit(interface, KSZ8851_REG_IER, enableMask | oldIER);

    //Enable all ints again...
    ksz8851EnableInterrupts(interface, enableMask | oldIER);

    //Every thing should be done. No need to call again...
    return false;
 }



 /**
  * @brief Send a packet
  * @param[in] interface Underlying network interface
  * @param[in] buffer Multi-part buffer containing the data to send
  * @param[in] offset Offset to the first data byte
  * @return Error code
  **/
error_t ksz8851SendPacket(NetInterface *interface, uint8_t * buffer, size_t length)
{
    error_t result = NO_ERROR;
    size_t n;
    Ksz8851TxHeader header;
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

    TRACE_DEBUG("ksz8851: Send packet (length=%ld)\n", length);

    //Check the frame length
    if(length > ETH_MAX_FRAME_SIZE)
    {
       TRACE_INFO("ksz8851: Pkt to send is too huge (%ld)!\n", length);
       //The transmitter can accept another packet
       //osSetEvent(&interface->nicTxEvent);
       //Report an error
       return ERROR_INVALID_LENGTH;
    }

    //Need to exclusive access the NIC registers now, store old IER mask
    uint16_t oldIERMask = ksz8851DisableInterrupts(interface);

    //Get the amount of free memory available in the TX FIFO
    n = ksz8851ReadReg(interface, KSZ8851_REG_TXMIR) & TXMIR_TXMA_MASK;

    //Make sure enough memory is available
    if((length + 8) > n) {
       TRACE_INFO("ksz8851: Not enough space to send packet!\n");
       result = ERROR_FAILURE;
       goto end;
    }

    //HP: the structure seems to be fix little endian always!
    //Format control word
    header.controlWord = swap(TX_CTRL_TXIC | (context->frameId++ & TX_CTRL_TXFID));
    //Total number of bytes to be transmitted
    header.byteCount = swap(length);

    //Enable TXQ write access
    ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

    //Write TX packet header
    ksz8851WriteFifo(interface, (uint8_t *) &header, sizeof(Ksz8851TxHeader));

    //Write data
    ksz8851WriteFifo(interface, buffer, length);
    //End TXQ write access
    ksz8851ClearBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

    //Start transmission
    ksz8851SetBit(interface, KSZ8851_REG_TXQCR, TXQCR_METFE);

    //Successful processing
    result = NO_ERROR;

end:
   //Enable all NIC ints again...
   ksz8851EnableInterrupts(interface, oldIERMask);

    return result;
}

 /**
  * @brief Receive a packet
  * @param[in] interface Underlying network interface
  * @return Error code
  **/
 error_t ksz8851ReceivePacket(NetInterface *interface)
 {
    uint16_t rxPktLength;
    uint16_t frameStatus;
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;
    volatile uint16_t dummy UNUSED; //VOLATILE! If not set, compiler would remove read from register! Also assign the reading from register!

    //Read received frame status from RXFHSR
    frameStatus = ksz8851ReadReg(interface, KSZ8851_REG_RXFHSR);

    TRACE_INFO(" Receiving frame (status=0x%04lx):\n", (ULONG)frameStatus);

   //Make sure the frame is valid (bit 15 must be 1)
   if (0 == (frameStatus & RXFHSR_RXFV)) {

      TRACE_INFO(" Pkt is not valid! ");

      if (frameStatus & (
              RXFHSR_RXMR         //MII Error
            | RXFHSR_RXFTL        //Frame too long > 2000
            | RXFHSR_RXRF         //Runt Frame Error
            | RXFHSR_RXCE         //CRC Error
            | RXFHSR_RXUDPFCS     //UDP crc error
            | RXFHSR_RXTCPFCS     //TCP crc error
            | RXFHSR_RXIPFCS      //IP crc error
            | RXFHSR_RXICMPFCS    //ICMP crc error
      )) {
         TRACE_INFO(" Pkt is error packet!\n");

         //Release the current error frame from RXQ
         ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_RRXEF);
      }

      TRACE_INFO("\n");

      //Report an error
      return ERROR_INVALID_PACKET;
   }

   //Read received frame byte size from RXFHBCR (Frame size + 4 bytes CRC)
   rxPktLength = ksz8851ReadReg(interface, KSZ8851_REG_RXFHBCR) & RXFHBCR_RXBC_MASK;

   //Ensure the frame size is acceptable (the pkt contains the 4 byte checksum, so it could be 1514 + 4 bytes size!)
   if (rxPktLength > 0 && rxPktLength <= (ETH_MAX_FRAME_SIZE + 4)) {

      //Reset QMU RXQ frame pointer to zero
      //HINT: The endian mode can't read back! So we need to set the complete register with mode bit set or
      //cleared!
      uint16_t val = context->isInBigEndianMode ? (RXFDPR_RXFPAI | RXFDPR_EMS) : (RXFDPR_RXFPAI);
      ksz8851WriteReg(interface, KSZ8851_REG_RXFDPR, val);
      //Enable RXQ read access (start DMA transfer: set bit 3)
      ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

      //16 bit NIC: Always read 2 dummy bytes, throw away...
      dummy = KSZ8851_DATA_REG;
      //Read (and ignore) packet header:
      dummy = KSZ8851_DATA_REG; //Status Word
      dummy = KSZ8851_DATA_REG; //byte count

      //TODO: There is a problem: To fulfill Amiga Sana2 device standard I need "raw packets" too which includes all infos about the packet.
      //But before I can read the whole packet, I have no clue that type it is. I can't discard the packet.
      //I have to copy twice the packet.

      //Read 16 frame header first (2 byte align, destination, source, type, payload)...
      //ksz8851ReadFifo(interface, (uint8_t*)&packetHeader, sizeof(packetHeader));
      //TODO:  Ask the receive if he really wants the packet!
      //Read rest of the frame...
      //rxPktLength -= sizeof(packetHeader);
      ksz8851ReadFifo(interface, context->rxBuffer, rxPktLength );

      //End RXQ read access (clear the bit!)
      ksz8851ClearBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

      //Pass the packet to the upper layer
      if (interface->onPacketReceived) {
         //jump over the first 2 dummy bytes of the packet //TODO:
         interface->onPacketReceived(context->rxBuffer+2, rxPktLength-2);
      }

      //Valid packet received
      return NO_ERROR;

   } else {
      TRACE_INFO(" Pkt size is invalid! (size=%ld)\n", (ULONG)rxPktLength);

      //Release the current error frame from RXQ
      ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_RRXEF);

      //Report an error
      return ERROR_INVALID_PACKET;
   }
}


 /**
  * @brief Configure multicast MAC address filtering
  * @param[in] interface Underlying network interface
  * @return Error code
  **/
#if 1
 error_t ksz8851SetMulticastFilter(NetInterface *interface, MacFilterEntry filter[], uint8_t fileEntries)
 {
    uint_t i;
    uint_t k;
    uint32_t crc;
    uint16_t hashTable[4];
    MacFilterEntry *entry;

    //Debug message
    TRACE_DEBUG("Updating KSZ8851 hash table...\r\n");

    //Clear hash table
    memset(hashTable, 0, sizeof(hashTable));

    //The MAC filter table contains the multicast MAC addresses
    //to accept when receiving an Ethernet frame
    for(i = 0; i < fileEntries; i++)
    {
       //Point to the current entry
       entry = &filter[i];

       //Valid entry?
       if(entry->refCount > 0)
       {
          //Compute CRC over the current MAC address
          crc = ksz8851CalcCrc(&entry->addr, sizeof(MacAddr));
          //Calculate the corresponding index in the table
          k = (crc >> 26) & 0x3F;
          //Update hash table contents
          hashTable[k / 16] |= (1 << (k % 16));
       }
    }

    //Write the hash table to the KSZ8851 controller
    ksz8851WriteReg(interface, KSZ8851_REG_MAHTR0, hashTable[0]);
    ksz8851WriteReg(interface, KSZ8851_REG_MAHTR1, hashTable[1]);
    ksz8851WriteReg(interface, KSZ8851_REG_MAHTR2, hashTable[2]);
    ksz8851WriteReg(interface, KSZ8851_REG_MAHTR3, hashTable[3]);

    //Debug message
    TRACE_DEBUG("  MAHTR0 = %04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_MAHTR0));
    TRACE_DEBUG("  MAHTR1 = %04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_MAHTR1));
    TRACE_DEBUG("  MAHTR2 = %04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_MAHTR2));
    TRACE_DEBUG("  MAHTR3 = %04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_MAHTR3));

    //Successful processing
    return NO_ERROR;
 }
#endif

 /**
  * Own read 16 bit registers
  * @param ks
  * @param offset
  */
 uint16_t ksz8851ReadReg(NetInterface *interface, uint8_t offset)
 {
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

    if (offset & 2) {
       KSZ8851_CMD_REG = offset | (context->isInBigEndianMode ? ( KSZ8851_CMD_B0 | KSZ8851_CMD_B1) : (KSZ8851_CMD_B2 | KSZ8851_CMD_B3));
    } else {
       KSZ8851_CMD_REG = offset | (context->isInBigEndianMode ? ( KSZ8851_CMD_B2 | KSZ8851_CMD_B3) : (KSZ8851_CMD_B0 | KSZ8851_CMD_B1));
    }
    return KSZ8851_DATA_REG;
 }

 /**
  * Own read 16 bit registers
  * @param ks
  * @param offset
  */
 void ksz8851WriteReg(NetInterface *interface, uint8_t offset, uint16_t value)
 {
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

    if (offset & 2) {
       KSZ8851_CMD_REG = offset | (context->isInBigEndianMode ? ( KSZ8851_CMD_B0 | KSZ8851_CMD_B1) : (KSZ8851_CMD_B2 | KSZ8851_CMD_B3));
    } else {
       KSZ8851_CMD_REG = offset | (context->isInBigEndianMode ? ( KSZ8851_CMD_B2 | KSZ8851_CMD_B3) : (KSZ8851_CMD_B0 | KSZ8851_CMD_B1));
    }
    KSZ8851_DATA_REG = value;
 }

 /**
  * @brief Write KSZ8851 register
  * @param[in] interface Underlying network interface
  * @param[in] address Register address
  * @param[in] data Register value
  **/
#if 0
 void ksz8851WriteReg(NetInterface *interface, uint8_t address, uint16_t data)
 {
    //Set register address
    if(address & 0x02)
       KSZ8851_CMD_REG = KSZ8851_CMD_B3 | KSZ8851_CMD_B2 | address;
    else
       KSZ8851_CMD_REG = KSZ8851_CMD_B1 | KSZ8851_CMD_B0 | address;

    //Write register value
    KSZ8851_DATA_REG = data;
 }
#endif


 /**
  * @brief Read KSZ8851 register
  * @param[in] interface Underlying network interface
  * @param[in] address Register address
  * @return Register value
  **/
#if 0
 uint16_t ksz8851ReadReg(NetInterface *interface, uint8_t address)
 {
    //Set register address
    if(address & 0x02)
       KSZ8851_CMD_REG = KSZ8851_CMD_B3 | KSZ8851_CMD_B2 | address;
    else
       KSZ8851_CMD_REG = KSZ8851_CMD_B1 | KSZ8851_CMD_B0 | address;

    //Return register value
    return KSZ8851_DATA_REG;
 }
#endif

 /**
  * Own very read register 8 bit version
  * @param ks
  * @param offset
  * @return
  */
#if 0
 uint8_t ksz8851ReadReg8(NetInterface * ks, uint8_t offset) {
    uint8_t result = 0;
    Ksz8851Context *context = (Ksz8851Context *)ks->nicContext;
    /**
     * Some miracle things happen when chip is set to big endian mode:
     * Not only it swapped 16 bit values (as expected), it seems to switch all registers in a 32 bit mode
     * behavior. => BE (Byte Enable logic) seems mirrored too. Not clear in chip manual.
     */
    if (!context->isInBigEndianMode) {
       //Normal default LE (from chip manual)
       switch (offset & 3) {
          case 0:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B0;
             result = KSZ8851_DATA_REG;
             break;
          case 1:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B1;
             result = KSZ8851_DATA_REG >> 8;
             break;
          case 2:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B2;
             result = KSZ8851_DATA_REG;
             break;
          case 3:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B3;
             result = KSZ8851_DATA_REG >> 8;
             break;
          default:
             break;
       }
    } else {
       //When chip is set to big endian mode ???? This switches all internal registers on a 32 bit
       //boundary to ???? Logic for BEx seems mixed too.
       switch (offset & 3) {
          case 0:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B2;
             result = KSZ8851_DATA_REG;
             break;
          case 1:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B3;
             result = KSZ8851_DATA_REG >> 8;
             break;
          case 2:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B0;
             result = KSZ8851_DATA_REG;
             break;
          case 3:
             KSZ8851_CMD_REG = offset | KSZ8851_CMD_B1;
             result = KSZ8851_DATA_REG >> 8;
             break;
          default:
             break;
       }
    }
    return result & 0xff;
 }
#endif

 /**
  * @brief Write TX FIFO
  * @param[in] interface Underlying network interface
  * @param[in] data Pointer to the data being written
  * @param[in] length Number of data to write
  **/

 void ksz8851WriteFifo(register NetInterface *interface, register const uint8_t *data, register size_t length)
 {
    //register size_t i;
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;
    register uint16_t sizeInWord = ((length + 3) & ~0x03) >> 1;


    if (context->isInBigEndianMode) {
       //BE mode:
       //copy in BE16 mode...which is faster because we do not mix the words
       register uint16_t * wordData = (uint16_t*)data;
       while (sizeInWord--) {
          KSZ8851_DATA_REG = *(wordData++);
       }

    } else {
       //LE mode: (twisting)
       register uint16_t val;
       while (sizeInWord--) {
          val = *(data++);
          val |= ((*(data++)) << 8);
          KSZ8851_DATA_REG = val;
       }
    }
 }

 /**
  * Checks if enough space to send packet
  * @param interface
  * @param packetSize
  * @return
  */
 bool ksz8851SendPacketPossible(NetInterface *interface, uint16_t packetSize) {
    return (ksz8851ReadReg(interface, KSZ8851_REG_TXMIR) + 8) <= packetSize;
 }

 /**
  * @brief Read RX FIFO
  * @param[in] interface Underlying network interface
  * @param[in] data Buffer where to store the incoming data
  * @param[in] length Number of data to read
  **/
void ksz8851ReadFifo(NetInterface *interface, uint8_t *data, size_t length) {
   register uint16_t value;
   Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;

   //Size in WORDs that are DWORD aligned!
   register uint16_t sizeInWord = ((length + 3) & ~0x03) >> 1;

   //Data phase:
   if (context->isInBigEndianMode) {
      //copy in BE16 mode...which is faster because we do not mix the words
      register uint16_t * wordData = (uint16_t*)data;
      while (sizeInWord--) {
         *(wordData++) = KSZ8851_DATA_REG;
      }

   } else {
      //Copy LE Mode (twisting bytes)
      while (sizeInWord--) {
         value = KSZ8851_DATA_REG;
         *(data++) = value & 0xFF;
         *(data++) = (value >> 8) & 0xFF;
      }
   }
}


 /**
  * @brief Set bit field
  * @param[in] interface Underlying network interface
  * @param[in] address Register address
  * @param[in] mask Bits to set in the target register
  **/
 void ksz8851SetBit(NetInterface *interface, uint8_t address, uint16_t mask)
 {
    uint16_t value;

    //Read current register value
    value = ksz8851ReadReg(interface, address);
    //Set specified bits
    ksz8851WriteReg(interface, address, value | mask);
 }


 /**
  * @brief Clear bit field
  * @param[in] interface Underlying network interface
  * @param[in] address Register address
  * @param[in] mask Bits to clear in the target register
  **/

 void ksz8851ClearBit(NetInterface *interface, uint8_t address, uint16_t mask)
 {
    uint16_t value;

    //Read current register value
    value = ksz8851ReadReg(interface, address);
    //Clear specified bits
    ksz8851WriteReg(interface, address, value & ~mask);
 }

 /**
  * @brief CRC calculation
  * @param[in] data Pointer to the data over which to calculate the CRC
  * @param[in] length Number of bytes to process
  * @return Resulting CRC value
  **/

 uint32_t ksz8851CalcCrc(const void *data, size_t length)
 {
    uint_t i;
    uint_t j;

    //Point to the data over which to calculate the CRC
    const uint8_t *p = (uint8_t *) data;
    //CRC preset value
    uint32_t crc = 0xFFFFFFFF;

    //Loop through data
    for(i = 0; i < length; i++)
    {
       //The message is processed bit by bit
       for(j = 0; j < 8; j++)
       {
          //Update CRC value
          if(((crc >> 31) ^ (p[i] >> j)) & 0x01)
             crc = (crc << 1) ^ 0x04C11DB7;
          else
             crc = crc << 1;
       }
    }

    //Return CRC value
    return crc;
 }

 /**
  * Dump registers in 16 bit mode
  * @param ks
  */
 void ksz8851DumpReg(NetInterface *interface) {
    int i;
    TRACE_INFO("\nRegister dump ( 128 x 16-bit ):\n");
    for (i = 0; i < 256; i+=2) {
       if (((i % 16) == 0)) {
          if (i != 0) {
             TRACE_INFO("\n");
          }
          TRACE_INFO("%02lx: ", (ULONG)i);
       }
       TRACE_INFO("%04lx ", (ULONG)ksz8851ReadReg(interface, i));
    }
    TRACE_INFO("\n");
 }

 /**
  * Swaps a 16 bit value...0x1234 => 0x3412
  * @param value
  */
 uint16_t swap(uint16_t value)
 {
    register uint16_t h = (value >> 8) & 0xff;
    register uint16_t l = value & 0xff;
    return (l << 8) | h;
 }


 // -------------------------------- Driver Interface Level -------------------------------------------------

 static ULONG ksz8851GetUsedSignalNumber(NetInterface * interface) {
    Ksz8851Context *context = (Ksz8851Context *)interface->nicContext;
    return context->sigNumber;
 }


 static void ksz8851GetStationAddress(NetInterface * interface, MacAddr * addr) {
     addr->w[0] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARH) );
     addr->w[1] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARM) );
     addr->w[2] = ntohs( ksz8851ReadReg(interface, KSZ8851_REG_MARL) );
  }

 static bool macInit = false;
 static error_t init(NetInterface * interface) {

    if (!macInit) {

       //Set a default MAC address:
       interface->macAddr.b[0] = 0x02;
       interface->macAddr.b[1] = 0x34;
       interface->macAddr.b[2] = 0x56;
       interface->macAddr.b[3] = 0x78;
       interface->macAddr.b[4] = 0x9a;
       interface->macAddr.b[5] = 0xbc;

       macInit = true;
    }

    return ksz8851Init(interface);
 }

 static void deinit(NetInterface * interface) {
    ksz8851Offline(interface);
    ksz8851DumpReg(interface);
 }

 static const char * ksz8851GetConfigFileName(void) {
    return "env:sana2/ksz8851.config";
 }

 static Ksz8851Context context = {
       .signalTask = NULL,
       .sigNumber = -1
 };
 static NetInterface driverInterface = {
       .init                     = init,
       .deinit                   = deinit,
       .probe                    = ksz8851DetectNICEndiness,
       .online                   = ksz8851Online,
       .offline                  = ksz8851Offline,
       .getUsedSignalNumber      = ksz8851GetUsedSignalNumber,
       .reset                    = ksz8851SoftReset,
       .processEvents            = ksz8851EventHandler,
       .sendPacketPossible       = ksz8851SendPacketPossible,
       .sendPacket               = ksz8851SendPacket,
       .getDefaultNetworkAddress = ksz8851GetStationAddress,
       .getConfigFileName        = ksz8851GetConfigFileName,
       .nicContext = (Ksz8851Context*)&context,
 };

 extern NetInterface * initModule() {
    return &driverInterface;
 }



 // ------------------ Entry points from old device --------------------------------------------------------

 bool hal_serviceReadPackets (APTR device,APTR a) {
    return TRUE;
 }

 bool hal_serviceWritePackets(APTR a,APTR b) {
    return TRUE;
 }

