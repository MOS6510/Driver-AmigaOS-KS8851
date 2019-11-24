/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the HAL (hardware part of the device driver)

#include <exec/interrupts.h>
#include <exec/tasks.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <hardware/intbits.h>
#include <dos/exall.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <time.h>

#include "devdebug.h"
#include "hal.h"

#include "../../os_includes/libnix/stabs.h"
#include "tools.h"
#include "version.h"


//########### PROTOTYPES #####################

typedef void (*VoidFunc )(void);

#define printf neverUsePrintfHere

inline static struct IOSana2Req * safeGetNextWriteRequest(struct DeviceDriverUnit * etherUnit);
void setErrorOnRequest(struct IOSana2Req *ios2, BYTE io_Error, ULONG ios2_WireError);


// ############ CONSTS #######################

const UBYTE BROADCAST_ADDRESS[6]={255,255,255,255,255,255};


// ############ GLOBAL VARS ##############

/** Counter for signaling via Vertical Beam Interrupt. */
int iPollInt=0;
/** Counter for general signals for unit process. */
int sigBitCntr=0;



//############################

///Konstanten
#define IP  0x800
#define ARP 0x806
///

//############################

///Prototypen...

BOOL CopyPacketToIOReq( struct DeviceDriver *etherDevice,
                        struct DeviceDriverUnit * etherUnit,
                        struct IOSana2Req * ios2);
///

//############################

#if DEBUG >0
void printEthernetAddress(UBYTE * addr) {
   DEBUGOUT((VERBOSE_HW, "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
         (ULONG)addr[0],  (ULONG)addr[1],  (ULONG)addr[2],  (ULONG)addr[3],  (ULONG)addr[4],  (ULONG)addr[5] ));
}
#else
   #define printEthernetAddress(addr)
#endif

BOOL isBroadcastEthernetAddress(UBYTE * addr) {
   return 0 == memcmp(addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
}

/**
 * Probing the hardware. Todo...
 * @return
 */
bool hal_probe()
{
   return true;
}

void hal_initialization()
{
}

void hal_deinitialization()
{
}


/**
 * Diese Funktion wird vom Device-task angesprungen, sobald er ein Packet zum
 * Bridgeboard senden soll (Tx), oder wenn die NIC-Software auf der Brueckenkarte
 * Nachrichten fuer uns hat (Packete Rx)
 *
 * @param etherUnit
 * @param etherDevice
 * @return true if something was done. False if nothing was done.
 */
bool serviceReadPackets(struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice)
{
   bool result = false;

#if 0
    struct IOSana2Req *ios2;
    struct BufferManagement *bm;
    struct ether_packet * b_packet;
    char resultIsPacketDelivered;
    int packet_anz;

    struct jan_ring * b_packetPtr = NULL; //packet pointer ("byte" view)

    if(securityStop) {
       print(200,"***Security-Stop***\n");
       return false;
    }

    DEBUGOUT((VERBOSE_HW, "serviceReadPackets():\n"));

    //
    // Packete lesen (wenn welche da)
    //
    packet_anz = 0;
    // Packete da ? ( last <> aktuell )
    while ((w_param->rec_ptr_cur != etherUnit->last_rx_buf_ptr) && (!securityStop))
    {                       
       print(5,"\nNew Packet from PC Server available:\n");

       print(5,"  w_param  ->rec_ptr_cur     = "); printi(5,w_param->rec_ptr_cur);
       print(5,"  etherUnit->last_rx_buf_ptr = "); printi(5,etherUnit->last_rx_buf_ptr);
          

       // bei Fehler kompletter stop!!!
       if ((packet_anz++) > 100)
       {
          DevCmdFlush(NULL,etherUnit,etherDevice);
          print(100,"  more than 100 times in Read loop !!!");
             etherUnit->last_rx_buf_ptr = 0;
          securityStop = 1;         
          break;
       }   

loop:

       // Zeiger auf dieses Packet ("byte view")
       b_packetPtr = (struct jan_ring *)
                  (((ULONG)(etherUnit->last_rx_buf_ptr)) +
                  ((ULONG)(b_param->ringbuf)));


       //Mark "End of Buffer" in ring buffer?
       if(b_packetPtr->rsr == EOB)
       {
          print(3,"  EOB mark found !!! => wrap ring buffer\n");
          etherUnit->last_rx_buf_ptr = 0;
          goto loop;
       }

       b_packet = (struct ether_packet *)(b_packetPtr->buf);
       DEBUGOUT((VERBOSE_HW, "  Reading packet type   0x%04lx\n", b_packet->type ));
  
      //
      // Ask every stack if he wants to get the new packet...
      //
      resultIsPacketDelivered = FALSE;
      ObtainSemaphore(&etherUnit->eu_BuffMgmtLock);
      for (bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head;
           bm->bm_Node.mln_Succ;
           bm = (struct BufferManagement *) bm->bm_Node.mln_Succ) {
         /*
          ** wurde Packet noch nicht an diesen Stack kopiert ?  UND
          ** gibt es IOReq-Reads in der Liste des Stacks ?
          */
         ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
         for (ios2 = GET_FIRST(bm->bm_RxQueue); //
              ios2->ios2_Req.io_Message.mn_Node.ln_Succ; //
              ios2 = (struct IOSana2Req *) (ios2->ios2_Req.io_Message.mn_Node.ln_Succ)) {

            /* stimmt Packettyp ? EthernetII or IEEE 802.3 */
            if ((ios2->ios2_PacketType == b_packet->type)
                  || ((ios2->ios2_PacketType <= 1500) && (b_packet->type <= 1500))) {

               // Copy packet to IORequest...
               resultIsPacketDelivered = CopyPacketToIOReq(etherDevice, etherUnit, b_packetPtr, ios2);
               break; // Enough for this stack
            }
         }
         ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);
      }
      ReleaseSemaphore(&etherUnit->eu_BuffMgmtLock);

      /*
       * If nobody wants that packet, try to find a read orphan listener who wants that.
       */
      if (!resultIsPacketDelivered) {
         BOOL pktTransfered = FALSE;
         DEBUGOUT((VERBOSE_HW, "No regular IORequest fits. Check 'Read Orphan List'...\n"));
         ObtainSemaphore((APTR) &etherUnit->eu_ReadOrphanLock);
         if (!IsListEmpty((struct List* )&etherUnit->eu_ReadOrphan)) {
            struct IOSana2Req * firstOrphan = GET_FIRST(etherUnit->eu_ReadOrphan);
            DEBUGOUT((VERBOSE_HW, "At least one ReadOrphan is waiting (ioreq=0x%lx)\n", firstOrphan));
            pktTransfered = CopyPacketToIOReq(etherDevice, etherUnit, b_packetPtr, firstOrphan);
         }
         if (!pktTransfered) {
            DEBUGOUT((VERBOSE_HW, "No request for packet. Packet dropped!\n"));
         }
         ReleaseSemaphore((APTR) &etherUnit->eu_ReadOrphanLock);
      }


       /*
       ** naechstes JanusPacket
       */

       int lastPacketLen = JanWord(&b_packetPtr->len);
       if(lastPacketLen & 1) {
          lastPacketLen++;
       }

       //lesezeiger merken...
       etherUnit->last_rx_buf_ptr = (ULONG)b_packetPtr - (ULONG)(b_param->ringbuf) + lastPacketLen + 4;

       //und PC mitteilen, dass wir das Paket nun gelesen haben...
       w_param->RecPtrRead = etherUnit->last_rx_buf_ptr;

       /* Word align */
       if(etherUnit->last_rx_buf_ptr & 1)
       {
          etherUnit->last_rx_buf_ptr++; //"last_rx_buf_ptr" Is ULONG not a pointer so that's correct to increment it as BYTE
          b_packetPtr = (struct jan_ring*)(((UBYTE*)b_packetPtr) + 1);
       }

       if(b_packetPtr->rsr == EOB)
       {
          etherUnit->last_rx_buf_ptr = 0;
       }

       //Something was done...
       result = true;
    }

#endif

    return result;
}

/**
 * Copy Ethernet Address (6 bytes) as fast as possible!
 * This is the fastest version in C I know.
 *
 * @param src
 * @param dst
 */
void copyEthernetAddress(const BYTE * from, BYTE * to) {
   ULONG * lsrc = (ULONG*) from;
   ULONG * ldst = (ULONG*) to;

   //Will result into 3 Assembler lines. But could be made by hand in 2 lines!

   *ldst++ = *lsrc++;                  //first 4 bytes of the address
   *(USHORT*) ldst = *(USHORT*) lsrc;  //last  2 bytes of the address
}

#define MAX(a,b)  (a) > (b) ? (a) : (b);



/**
 * Writing Packets. Check if packets should be send out.
 * Returns TRUE if something was send. False if not...
 *
 * @param etherUnit
 * @param etherDevice
 * @return ...
 */
bool serviceWritePackets(struct DeviceDriverUnit *etherUnit, struct DeviceDriver *etherDevice) {

#if 0
   struct IOSana2Req *ios2;
   short cmdRegisterIndex;
   struct BufferManagement *bm;
   short pad;
   struct ether_packet * packet;

   DEBUGOUT((VERBOSE_HW, "serviceWritePackets():\n"));

   //Free transmit buffer?
   if ((cmdRegisterIndex = getNextFreeTransmitBufferIndexDoNotWait()) >= 0) {

      /* Packets for sending available? */
      if ((ios2 = safeGetNextWriteRequest(etherUnit)) != NULL) {

         // Hat der Stack ueberhaupt Buffermanagement-Funktionen
         // wenn nicht wird das Packet verworfen !!!!!
         bm = (struct BufferManagement *) ios2->ios2_BufferManagement;
         if (!bm || !bm->bm_CopyFromBuffer) {
            setErrorOnRequest(ios2, S2ERR_NO_RESOURCES, S2WERR_BUFF_ERROR);
            goto end;
         }

         //Raw packet?
         if (ios2->ios2_Req.io_Flags & SANA2IOF_RAW) {
            print(3, "  Packet wird RAW abgeschickt...\n");
            print(3, "  from =");
            printi(3, (ULONG) ios2->ios2_Data);
            print(3, "  to   =");
            printi(3, (ULONG) b_param->buf[cmdRegisterIndex]);
            print(3, "  len  =");
            printi(3, (ULONG) ios2->ios2_DataLength);

            if (ios2->ios2_DataLength <= sizeof(struct ether_packet)) {
               if (CopyFromBuffer((ULONG )b_param->buf[cmdRegisterIndex], (ULONG )ios2->ios2_Data,
                     ios2->ios2_DataLength)) {
                  w_param->len[cmdRegisterIndex] = ios2->ios2_DataLength;
                  w_param->cmd[cmdRegisterIndex] = PTX;
                  CallPC();
               } else {
                  setErrorOnRequest(ios2, S2ERR_NO_RESOURCES, S2WERR_BUFF_ERROR);
               }
            } else {
               setErrorOnRequest(ios2, S2ERR_MTU_EXCEEDED, S2WERR_BUFF_ERROR);
            }

         } else {
            /* Normales Packet verschicken */

            if (ios2->ios2_DataLength <= ETHERNET_MTU) {

               /* Packet mindestlaenge beachten: 46 bytes */
               pad = MAX(0, (short )46 - (short )ios2->ios2_DataLength)

               packet = (struct ether_packet*) (b_param->buf[cmdRegisterIndex]);

               //SRC + DST
               copyEthernetAddress(ios2->ios2_DstAddr, packet->dst_addr);
               copyEthernetAddress(etherUnit->eu_StAddr, packet->src_addr);

               // Packet Type
               packet->type = ios2->ios2_PacketType;

               DEBUGOUT((VERBOSE_HW, "  Send normal packet:\n"));
               DEBUGOUT((VERBOSE_HW, "    from (ios2_Data): 0x%lx\n", ios2->ios2_Data));
               DEBUGOUT((VERBOSE_HW, "    to (TX register): %ld\n", (ULONG) cmdRegisterIndex));
               DEBUGOUT((VERBOSE_HW, "    len             : %ld\n", (ULONG) ios2->ios2_DataLength));
               DEBUGOUT((VERBOSE_HW, "    + padding       : %ld\n", (ULONG)pad));
               DEBUGOUT((VERBOSE_HW, "    type            : 0x%lx\n", (ULONG)ios2->ios2_PacketType));
               DEBUGOUT((VERBOSE_HW, "    Src MAC         : ")); printEthernetAddress(packet->src_addr); DEBUGOUT((VERBOSE_HW,"\n"));
               DEBUGOUT((VERBOSE_HW, "    Dst MAC         : ")); printEthernetAddress(packet->dst_addr); DEBUGOUT((VERBOSE_HW,"\n"));

               //Copy packet content and send....
               if (CopyFromBuffer((ULONG )packet->buf, (ULONG )ios2->ios2_Data, ios2->ios2_DataLength)) {
                  w_param->len[cmdRegisterIndex] = ios2->ios2_DataLength + ETHER_PACKET_HEAD_SIZE + pad;
                  w_param->cmd[cmdRegisterIndex] = PTX;
                  CallPC();
               } else {
                  setErrorOnRequest(ios2, S2ERR_NO_RESOURCES, S2WERR_BUFF_ERROR);
                  DEBUGOUT((VERBOSE_HW, "   CopyFromBuffer error\n"));
               }
            } else {
               setErrorOnRequest(ios2, S2ERR_MTU_EXCEEDED, S2WERR_BUFF_ERROR);
               DEBUGOUT((VERBOSE_HW, "   Pkt exceeds the MTU!\n"));
            }
         }
end:
         if (ios2->ios2_WireError != 0) {
            DoEvent(S2EVENT_BUFF, etherUnit, etherDevice);
         }
         TermIO(ios2, etherDevice);
         return TRUE;
      }
   }
#endif

   //Nothing to be done...
   return FALSE;
}

void setErrorOnRequest(struct IOSana2Req *ios2, BYTE io_Error, ULONG ios2_WireError) {
   print(3, "Something went wrong!!\n");
   ios2->ios2_WireError = ios2_WireError;
   ios2->ios2_Req.io_Error = io_Error;
}

/**
 * Tries to retrieve next Write packet. returns NULL if no packets to be send.
 * @param etherUnit etherunit
 * @return next write packet or NULL
 */
inline struct IOSana2Req * safeGetNextWriteRequest(struct DeviceDriverUnit * etherUnit) {
   Forbid();
   struct IOSana2Req * req = (struct IOSana2Req *)GetMsg((APTR)etherUnit->eu_Tx);
   Permit();
   return req;
}



/*
 * Copy packet to given Sana 2 request. Returns true if packet was copied to the request.
 * Returns falls when not for some reason.
 */
BOOL CopyPacketToIOReq( struct DeviceDriver * etherDevice,
                        struct DeviceDriverUnit * etherUnit,
                        struct IOSana2Req * ios2)
{
   BOOL bStatus=FALSE;

#if 0
   struct ether_packet * rawEtherPacket;
   struct BufferManagement * bm;
   int packetLength;


   packetLength = JanWord(&PacketPtr->len) - ETHER_PACKET_HEAD_SIZE;
   rawEtherPacket = (struct ether_packet *)(PacketPtr->buf);

   DEBUGOUT((VERBOSE_HW, "Satisfying ioReq 0x%lx (mode=%s)\n", (ULONG)ios2, ios2->ios2_Req.io_Flags & SANA2IOF_RAW ? "raw" : "cooked"));
   print(3,"  Janus mem addr from: "); printi(3,(ULONG)PacketPtr);
   print(3,"  packet length:       "); printi(3,(ULONG)packetLength);

   //If the size is bigger than MTU of Ethernet something went wrong!
   //Software Error!
   if (packetLength > ETHERNET_MTU) {
      Alert(0x80001818); //Dead end
      return FALSE;
   }

   // Zeiger auf die Buffermanagementstruktur dieses IORequests beschaffen
   // wenn es keinen gibt, dann hat der Stack pech gehabt und bekommt
   // ihn nicht !!!
   bm = ios2->ios2_BufferManagement;
   if (bm)
   {
      APTR packetDataPtr;
      //IORequest for "raw" packets?
      if(ios2->ios2_Req.io_Flags & SANA2IOF_RAW) {
         packetLength += ETHER_PACKET_HEAD_SIZE;
         packetDataPtr = (APTR)&rawEtherPacket;
      }
      else {
         packetDataPtr = (APTR)rawEtherPacket->buf;
      }

      copyEthernetAddress(rawEtherPacket->dst_addr ,ios2->ios2_DstAddr);  // Destination address
      copyEthernetAddress(rawEtherPacket->src_addr ,ios2->ios2_SrcAddr);  // Source Address
      ios2->ios2_PacketType = rawEtherPacket->type;
      ios2->ios2_DataLength = packetLength;

      //TODO: Check Broadcast: Should be a PC side thing..
      //Check Broadcast / Multicast (bit 0 of first byte is "1" if multicast or broadcast)
      if ((rawEtherPacket->dst_addr[0] & 1) != 0 ) {
         if (isBroadcastEthernetAddress(rawEtherPacket->dst_addr)) {
            ios2->ios2_Req.io_Flags |= SANA2IOF_BCAST;
         } else {
            ios2->ios2_Req.io_Flags |= SANA2IOF_MCAST;
         }
      }

      DEBUGOUT((VERBOSE_HW, "  Req DST    : ")); printEthernetAddress(ios2->ios2_DstAddr);  DEBUGOUT((VERBOSE_HW, "\n"));
      DEBUGOUT((VERBOSE_HW, "  Req SRC    : ")); printEthernetAddress(ios2->ios2_SrcAddr);  DEBUGOUT((VERBOSE_HW, "\n"));
      DEBUGOUT((VERBOSE_HW, "  Req LEN    : %ld\n", (ULONG)ios2->ios2_DataLength));
      DEBUGOUT((VERBOSE_HW, "  Req TYP    : 0x%lx\n", (ULONG)ios2->ios2_PacketType));
      DEBUGOUT((VERBOSE_HW, "  Req FLAGS  : 0x%lx\n", (ULONG)ios2->ios2_Req.io_Flags));


      // Frage Stack, ob er das Packet auch wirklich moechte (Paketfilter)!
      if(CallFilterHook(bm->bm_PacketFilterHook, ios2, packetDataPtr))
      {
         DEBUGOUT((VERBOSE_HW,"  Filter hook passed. Result: Not skipped.\n"));
         // Packet zum Stack kopieren !!
         if(CopyToBuffer((ULONG)ios2->ios2_Data,(ULONG)packetDataPtr, packetLength ))
         {
            DEBUGOUT((VERBOSE_HW,"CopyToBuffer() ok.\n"));
            bStatus = true;
         }
         else
         {
            DEBUGOUT((VERBOSE_HW,"  Error CopyToBuffer()!\n"));
            ios2->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            ios2->ios2_WireError    = S2WERR_BUFF_ERROR;
            DoEvent(S2EVENT_BUFF,etherUnit,etherDevice);
         }

         /* ioreq aus liste entfernen und an Stack zurueckschicken */
         //Remove() is safe because we are in List lock when this function is called.
         Remove((APTR)ios2);
         TermIO(ios2,etherDevice);
      } else {
         DEBUGOUT((VERBOSE_HW,"  Filter hook passed. Result: Packet SKIPPED!\n"));
      }
   }
#endif

   // status an Aufrufer
   return bStatus;
}
///
