/*
 * KSZ8851 Amiga Network Driver
 */

 #include "ksz8851.h"

// -------------- PROTOTYPES ------------

void nicNotifyLinkChange(NetInterface * interface);
extern void ksz8851_SoftReset(NetInterface *interface, unsigned op);


// --------------------------------------- Amiga Interrupts ------------------------------------------------

#if !defined(REG)
#define REG(reg,arg) arg __asm(#reg)
#endif

 static volatile uint8_t isr( REG(a1, NetInterface * interface) )
 {
    if (ksz8851IrqHandler(interface)) {
       //*((uint16_t*)0xdff180) = 0xa;
       Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;
       Signal(context->signalTask, (1<< context->sigNumber));
       context->signalCounter++;
       return (uint8_t)1;
    }
    return (uint8_t)0;
 }

 static void installInterruptHandler(NetInterface * interface,struct Task * task, ULONG sigNumber)
 {
    Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;

    if (context->signalTask) {
       printf("Interrupt is already installed...\n");
       return;
    }

    context->signalTask = task;
    context->sigNumber  = sigNumber;

    context->AmigaInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    context->AmigaInterrupt.is_Node.ln_Name = "KSZ8851 SANA2 network driver interrupt";
    context->AmigaInterrupt.is_Node.ln_Pri  = 121;
    context->AmigaInterrupt.is_Data         = interface;
    context->AmigaInterrupt.is_Code         = (void*)(&isr);

    //Network chip uses (INT6) which is "external interrupt" on AmigaOS.
    AddIntServer(INTB_EXTER, &context->AmigaInterrupt);
 }

 static void uninstallInterruptHandler(NetInterface * interface)
 {
    Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;
    if(context->signalTask)
    {
       RemIntServer(INTB_EXTER, &context->AmigaInterrupt);
       context->signalTask = NULL;
       FreeSignal(context->sigNumber);
       context->sigNumber= -1;
    }
 }

 // --------------------------------------- Amiga Interrupts end --------------------------------------------


 /**
  * @brief Enable interrupts
  * @param[in] interface Underlying network interface
  **/

 void ksz8851EnableIrq(NetInterface *interface)
 {
    installInterruptHandler(interface,FindTask(0l),AllocSignal(-1));
 }


 /**
  * @brief Disable interrupts
  * @param[in] interface Underlying network interface
  **/

 void ksz8851DisableIrq(NetInterface *interface)
 {
    //Disable interrupts to release the interrupt line
    Disable();
    ksz8851WriteReg(interface, KSZ8851_REG_IER, 0);
    Enable();
    uninstallInterruptHandler(interface);
 }


 /**
  * @brief KSZ8851 controller initialization
  * @param[in] interface Underlying network interface
  * @return Error code
  **/

 error_t ksz8851Init(NetInterface *interface)
 {
    //Point to the driver context
    Ksz8851Context *context = (Ksz8851Context *) interface->nicContext;

    //Debug message
    TRACE_INFO("Initializing KSZ8851 Ethernet controller...\r\n");

    //Reset Chip first...
    ksz8851_SoftReset(interface,1);

    //Debug message
    TRACE_DEBUG("CIDER=0x%04" PRIX16 "\r\n",   ksz8851ReadReg(interface, KSZ8851_REG_CIDER));
    TRACE_DEBUG("PHY1ILR=0x%04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_PHY1ILR));
    TRACE_DEBUG("PHY1IHR=0x%04" PRIX16 "\r\n", ksz8851ReadReg(interface, KSZ8851_REG_PHY1IHR));

    //Check device ID and revision ID
    if(ksz8851ReadReg(interface, KSZ8851_REG_CIDER) != KSZ8851_REV_A3_ID) {
       TRACE_DEBUG("Failed!\n");
       return ERROR_WRONG_IDENTIFIER;
    }

    //Initialize driver specific variables
    context->frameId = 0;

    //Allocate TX and RX buffers
    context->txBuffer = memPoolAlloc(ETH_MAX_FRAME_SIZE);
    context->rxBuffer = memPoolAlloc(ETH_MAX_FRAME_SIZE);

    //Failed to allocate memory?
    if(context->txBuffer == NULL || context->rxBuffer == NULL)
    {
       //Clean up side effects
       memPoolFree(context->txBuffer);
       memPoolFree(context->rxBuffer);

       return ERROR_OUT_OF_MEMORY;
    }

    //Initialize MAC address
    /*ksz8851WriteReg(interface, KSZ8851_REG_MARH, htons(interface->macAddr.w[0]));
    ksz8851WriteReg(interface, KSZ8851_REG_MARM, htons(interface->macAddr.w[1]));
    ksz8851WriteReg(interface, KSZ8851_REG_MARL, htons(interface->macAddr.w[2]));*/

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
    ksz8851WriteReg(interface, KSZ8851_REG_RXQCR, RXQCR_RXFCTE | RXQCR_ADRFE);
    //Automatically increment RX data pointer
    ksz8851WriteReg(interface, KSZ8851_REG_RXFDPR, RXFDPR_RXFPAI);
    //Configure receive frame count threshold
    ksz8851WriteReg(interface, KSZ8851_REG_RXFCTR, 1);

    //Force link in half-duplex if auto-negotiation failed
    ksz8851ClearBit(interface, KSZ8851_REG_P1CR, P1CR_FORCE_DUPLEX);
    //Restart auto-negotiation
    ksz8851SetBit(interface, KSZ8851_REG_P1CR, P1CR_RESTART_AN);

    //Clear interrupt flags
    ksz8851SetBit(interface, KSZ8851_REG_ISR, ISR_LCIS | ISR_TXIS |
       ISR_RXIS | ISR_RXOIS | ISR_TXPSIS | ISR_RXPSIS | ISR_TXSAIS |
       ISR_RXWFDIS | ISR_RXMPDIS | ISR_LDIS | ISR_EDIS | ISR_SPIBEIS);

    //Configure interrupts as desired (HP: TODO do not activate until Amiga ISR is installed!)
    ksz8851SetBit(interface, KSZ8851_REG_IER, IER_LCIE | /*IER_TXIE |*/ IER_RXIE | IER_RXOIE);

    //Enable TX operation
    ksz8851SetBit(interface, KSZ8851_REG_TXCR, TXCR_TXE);
    //Enable RX operation
    ksz8851SetBit(interface, KSZ8851_REG_RXCR1, RXCR1_RXE);

    //Successful initialization
    return NO_ERROR;
 }

 void ksz8851_SoftReset(NetInterface *interface, unsigned op)
 {
    /* Disable interrupt first */
    uint16_t oldisr = ksz8851ReadReg(interface, KSZ8851_REG_ISR);

    ksz8851WriteReg(interface, KSZ8851_REG_ISR, 0x0000);
    ksz8851WriteReg(interface,  KSZ8851_REG_GRR, op);
    Delay(50);
     /* wait a short time to effect reset */
    ksz8851WriteReg(interface,  KSZ8851_REG_GRR, 0);
    /* wait for condition to clear */
    Delay(50);

    //Re-enable all int flags again...
    ksz8851WriteReg(interface, KSZ8851_REG_ISR, oldisr);
 }

 /**
  * @brief KSZ8851 interrupt service routine
  * @param[in] interface Underlying network interface
  * @return TRUE if a higher priority task must be woken. Else FALSE is returned
  **/
 bool_t ksz8851IrqHandler(NetInterface *interface)
 {
    //Ksz8851Context * context = (Ksz8851Context*)interface->nicContext;

    bool_t flag;
    uint16_t ier;
    uint16_t isr;

    //This flag will be set if a higher priority task must be woken
    flag = FALSE;

    //Read interrupt status register and check of valid interrupt from nic
    isr = ksz8851ReadReg(interface, KSZ8851_REG_ISR);
    if (0 == isr) {
       //No, not from NIC
       return false;
    }

    flag = TRUE;

    //Save IER register value
    ier = ksz8851ReadReg(interface, KSZ8851_REG_IER);

    //Disable interrupts to release the interrupt line
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
       flag = TRUE;
    }

    //Packet transmission complete?
    if(isr & ISR_TXIS)
    {
       //Clear interrupt flag
       ksz8851WriteReg(interface, KSZ8851_REG_ISR, ISR_TXIS);

       //Notify the TCP/IP stack that the transmitter is ready to send
       //flag |= osSetEventFromIsr(&interface->nicTxEvent);
       flag = TRUE;
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
       flag = TRUE;
    }

    //Overrun rx packets?
    if (isr & ISR_RXOIS) {

      ier &= ~ISR_RXOIS;

      flag = TRUE;
    }

    //Link-up status detect
    if (isr & ISR_LDIS) {
       //clear event with only PMECR "0010" => [5:2]
       ksz8851SetBit(interface, KSZ8851_REG_PMECR, 0x2 << 2);

       ier &= ~ISR_LDIS;

       flag = TRUE;
    }

    //Disable all received interrupts now.
    //Re-enable interrupts once the interrupt has been serviced!
    ksz8851WriteReg(interface, KSZ8851_REG_IER, ier);

    return flag;
 }


 /**
  * @brief KSZ8851 event handler
  * @param[in] interface Underlying network interface
  **/
 void ksz8851EventHandler(NetInterface *interface, ProcessPacketFunc processFunc)
 {
    uint16_t status;
    uint16_t frameCount;
    uint16_t enableMask = 0;

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

       enableMask |= ~ISR_LCIS;

       //Process link state change event
       nicNotifyLinkChange(interface);
    }

    //Check whether a packet has been received?
    if(status & ISR_RXIS)
    {
       //Clear interrupt flag
       ksz8851WriteReg(interface, KSZ8851_REG_ISR, ISR_RXIS);
       //Get the total number of frames that are pending in the buffer
       frameCount = MSB(ksz8851ReadReg(interface, KSZ8851_REG_RXFCTR));
       //printf("Count of packet to rx: %d\n", frameCount);

       //Process all pending packets
       while((frameCount--) > 0)
       {
          //Read incoming packet //TODO: receive method...
          ksz8851ReceivePacket(interface, processFunc);
       }
       enableMask |= ~ISR_RXIS;
    }

    //Receiver overruns?
    if (status & ISR_RXOIS) {
       ((Ksz8851Context*)interface->nicContext)->rxOverrun++;
       enableMask |= ~ISR_RXOIS;
    }

    //Wakeup from something?
    if (status & ISR_LDIS) {
       //nicNotifyLinkChange(interface);
       enableMask |= ~ISR_LDIS;
    }

    //Re-enable all interrupts again...
    ksz8851SetBit(interface, KSZ8851_REG_IER, IER_LCIE | IER_RXIE | ISR_RXOIS | ISR_LDIS);
 }

 void dumpMem(uint8_t * data, int size) {
    int i;
    for (i = 0; i < size; i++) {
       if (((i % 16) == 0)) {
          if (i != 0) {
             printf("\n");
          }
          printf("%02x: ", i);
       }
       printf("%02x ", data[i]);
    }
    printf("\n");
 }


 /**
  * @brief Send a packet
  * @param[in] interface Underlying network interface
  * @param[in] buffer Multi-part buffer containing the data to send
  * @param[in] offset Offset to the first data byte
  * @return Error code
  **/

 error_t ksz8851SendPacket(NetInterface *interface, const NetBuffer *buffer, size_t offset)
 {
    size_t n;
    size_t length;
    Ksz8851TxHeader header;
    Ksz8851Context *context;

    //Point to the driver context
    context = (Ksz8851Context *) interface->nicContext;

    //Retrieve the length of the packet
    length = netBufferGetLength(buffer) - offset;

    //Check the frame length
    if(length > ETH_MAX_FRAME_SIZE)
    {
       //The transmitter can accept another packet
       //osSetEvent(&interface->nicTxEvent);
       //Report an error
       return ERROR_INVALID_LENGTH;
    }

    //Get the amount of free memory available in the TX FIFO
    n = ksz8851ReadReg(interface, KSZ8851_REG_TXMIR) & TXMIR_TXMA_MASK;

    //Make sure enough memory is available
    if((length + 8) > n)
       return ERROR_FAILURE;

    //Copy user data
    netBufferRead(context->txBuffer, buffer, offset, length);

    //HP: the structure seems to be in little endian mode!
    //Format control word
    header.controlWord = swap(TX_CTRL_TXIC | (context->frameId++ & TX_CTRL_TXFID));
    //Total number of bytes to be transmitted
    header.byteCount = swap(length);

    //Enable TXQ write access
    ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

    //Write TX packet header
    //dumpMem((uint8_t*)&header, sizeof(Ksz8851TxHeader));
    ksz8851WriteFifo(interface, (uint8_t *) &header, sizeof(Ksz8851TxHeader));

    //Write data
    //dumpMem(context->txBuffer, length);
    ksz8851WriteFifo(interface, context->txBuffer, length);
    //End TXQ write access
    ksz8851ClearBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

    //Start transmission
    ksz8851SetBit(interface, KSZ8851_REG_TXQCR, TXQCR_METFE);

    //Successful processing
    return NO_ERROR;
 }


 /**
  * @brief Receive a packet
  * @param[in] interface Underlying network interface
  * @return Error code
  **/
 error_t ksz8851ReceivePacket(NetInterface *interface, ProcessPacketFunc processFunc)
 {
    uint16_t n;
    uint16_t status;
    Ksz8851Context *context;

    //Point to the driver context
    context = (Ksz8851Context *) interface->nicContext;

    //Read received frame status from RXFHSR
    status = ksz8851ReadReg(interface, KSZ8851_REG_RXFHSR);

    //Make sure the frame is valid
    if(status & RXFHSR_RXFV)
    {
       //Check error flags
       if(!(status & (RXFHSR_RXMR | RXFHSR_RXFTL | RXFHSR_RXRF | RXFHSR_RXCE)))
       {
          //Read received frame byte size from RXFHBCR
          n = ksz8851ReadReg(interface, KSZ8851_REG_RXFHBCR) & RXFHBCR_RXBC_MASK;
          printf(" %d: ", n);

          //Ensure the frame size is acceptable
          if(n > 0 && n <= ETH_MAX_FRAME_SIZE)
          {
             //Reset QMU RXQ frame pointer to zero
             ksz8851WriteReg(interface, KSZ8851_REG_RXFDPR, RXFDPR_RXFPAI);
             //Enable RXQ read access
             ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);
             //Read data
             ksz8851ReadFifo(interface, context->rxBuffer, n);
             //End RXQ read access
             ksz8851ClearBit(interface, KSZ8851_REG_RXQCR, RXQCR_SDA);

             //Pass the packet to the upper layer
             if (processFunc) {
                processFunc(context->rxBuffer, n);
             }

             //Valid packet received
             return NO_ERROR;
          } else {
             printf(" Invalid length %d!\n", n);
          }
       } else {
          printf(" Packet not ok status=0x%x????\n", status);
       }
    } else {
       printf(" Packet is marked as not valid 0x%x????\n", status);
    }

    //Release the current error frame from RXQ
    ksz8851SetBit(interface, KSZ8851_REG_RXQCR, RXQCR_RRXEF);

    //Report an error
    return ERROR_INVALID_PACKET;
 }


 /**
  * @brief Configure multicast MAC address filtering
  * @param[in] interface Underlying network interface
  * @return Error code
  **/
#if 1
 error_t ksz8851SetMulticastFilter(NetInterface *interface)
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
    for(i = 0; i < MAC_MULTICAST_FILTER_SIZE; i++)
    {
       //Point to the current entry
       entry = &interface->macMulticastFilter[i];

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
  * @brief Write TX FIFO
  * @param[in] interface Underlying network interface
  * @param[in] data Pointer to the data being written
  * @param[in] length Number of data to write
  **/

 void ksz8851WriteFifo(NetInterface *interface, const uint8_t *data, size_t length)
 {
    size_t i;

    //Data phase => HP: SLOW!!!!!
    for(i = 0; i < length; i+=2)
       KSZ8851_DATA_REG = data[i] | data[i+1]<<8;

    //Maintain alignment to 4-byte boundaries
    for(; i % 4; i+=2)
       KSZ8851_DATA_REG = 0x0000;
 }


 /**
  * @brief Read RX FIFO
  * @param[in] interface Underlying network interface
  * @param[in] data Buffer where to store the incoming data
  * @param[in] length Number of data to read
  **/

 void ksz8851ReadFifo(NetInterface *interface, uint8_t *data, size_t length)
 {
    uint_t i;
    uint16_t temp;

    //The first 2 bytes are dummy data and must be discarded
    temp = KSZ8851_DATA_REG;

    //Ignore RX packet header
    temp = KSZ8851_DATA_REG;
    temp = KSZ8851_DATA_REG;

    //Data phase => Slow!!!
    for(i = 0; i < length; i+=2)
    {
       temp = KSZ8851_DATA_REG;
       data [i] = temp & 0xFF;
       data [i+1] = (temp>>8) & 0xFF;
    }

    //Maintain alignment to 4-byte boundaries
    for(; i % 4; i+=2)
       temp = KSZ8851_DATA_REG;
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

