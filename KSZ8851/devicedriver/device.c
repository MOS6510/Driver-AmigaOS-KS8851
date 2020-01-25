/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the AmigaOS part of the device driver.

// ############################## INCLUDES ####################################

#include <string.h>

#include "device.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <proto/graphics.h>

#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <dos/notify.h>
#include <dos/filehandler.h>

#include <exec/types.h>
#include <exec/devices.h>
#include <exec/ports.h>
#include <exec/semaphores.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <exec/interrupts.h>
#include <exec/lists.h>
#include <devices/sana2.h>
#include <devices/sana2specialstats.h> 
#include <devices/serial.h> 
#include <devices/timer.h>
#include <utility/hooks.h>
#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <dos/exall.h>

#include <clib/dos_protos.h>
#include <clib/utility_protos.h>
#include <clib/timer_protos.h>
#include <clib/alib_stdio_protos.h>
#include <clib/intuition_protos.h>
#include <clib/expansion_protos.h>
#include <clib/alib_protos.h>
#include <clib/debug_protos.h>

#include <devices/NewStyleDev.h>

#include "configfile.h"
#include "copybuffs.h"
#include "devdebug.h"
#include "helper.h"
#include "tools.h"
#include "version.h"

#include "hardware-interface.h"

// ############################## GLOBALS #####################################

const UBYTE BROADCAST_ADDRESS[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

// ############################## LOCALS  #####################################

static const char * DEVICE_TASK_NAME = DEVICE_NAME " unit task";

// ############################## Local prototypes ############################

static ULONG AbortRequestAndRemove(struct MinList *, struct IOSana2Req *,struct DeviceDriver *,struct SignalSemaphore * lock);
static void  AbortReqList(struct MinList *minlist,struct DeviceDriver * EtherDevice);
static BOOL  ReadConfig(struct DeviceDriver *,  const char * sConfigFile);
static void DevProcEntry(void);
static BOOL checkStackSpace(int minStackSize);
void printEthernetAddress(unsigned char * addr);

//############################### CONST #######################################

// State bits for edu_State
#define ETHERUB_CONFIG     0
#define ETHERUB_ONLINE     1
#define ETHERUB_EXCLUSIVE  2
#define ETHERUB_LOOPBACK   3
#define ETHERUB_PROMISC    4

#define ETHERUF_CONFIG        (1<<ETHERUB_CONFIG)
#define ETHERUF_ONLINE        (1<<ETHERUB_ONLINE)
#define ETHERUF_EXCLUSIVE     (1<<ETHERUB_EXCLUSIVE)
#define ETHERUF_LOOPBACK      (1<<ETHERUB_LOOPBACK)
#define ETHERUF_PROMISC       (1<<ETHERUB_PROMISC)


#define ETHERNET_MTU 1500
#define ETHER_PACKET_HEAD_SIZE 14

//This is for defining the API version to the PC side.
const UWORD DevVersion  = (UWORD)DEVICE_VERSION;
const UWORD DevRevision = (UWORD)DEVICE_REVISION;

const char DevName[]     = DEVICE_NAME;
const char DevIdString[] = VERSION_STRING;

//############ Globale Variablen ###############################################

struct ExecBase * SysBase  = NULL;
struct DeviceDriver * globEtherDevice = NULL;
struct Sana2DeviceStats GlobStat;

//init and deinit auto-open-library of "libnix" which has to be handles by hand here.
extern void __initlibraries(void);
extern void __exitlibraries(void);
extern long __oslibversion;

// --------------------- INTERNAL PROTOTYPES -----------------------------

void  PerformIO(struct IOSana2Req *,struct DeviceDriver * DevBase);
void  TermIO(struct IOSana2Req * ,struct DeviceDriver * DevBase);
VOID  ExpungeUnit(UBYTE unitNumber, struct DeviceDriver *etherDevice);
struct DeviceDriverUnit *InitUnitProcess(ULONG,struct DeviceDriver * ETHERDevice);
VOID DevCmdTrackType(STDETHERARGS);
VOID DevCmdUnTrackType(STDETHERARGS);
VOID DevCmdConfigInterface(STDETHERARGS);
VOID DevCmdReadPacket(STDETHERARGS);
VOID DevCmdReadOrphan(STDETHERARGS);
VOID DevCmdGetStationAddress(STDETHERARGS);
VOID DevCmdOffline(STDETHERARGS);
VOID DevCmdOnline(STDETHERARGS);
VOID DevCmdDeviceQuery(STDETHERARGS);
VOID DevCmdWritePacket(STDETHERARGS);
VOID DevCmdFlush(STDETHERARGS);
VOID DevCmdOnEvent(STDETHERARGS);
VOID DevCmdGlobStats(STDETHERARGS);
VOID DevCmdAddMulti(STDETHERARGS);
VOID DevCmdRemMulti(STDETHERARGS);
VOID DevCmdGetSpecialStats(STDETHERARGS);
VOID DevCmdNSDeviceQuery(STDETHERARGS);

static void dumpMem(uint8_t * mem, int16_t len);
static void sendAllPackets(struct DeviceDriverUnit *etherUnit, struct DeviceDriver *etherDevice);

//############ Externe Variablen und Funktionen ################################

SAVEDS
ULONG FakePFHookEntry(void)
{
   return 1;
}

/**
 * Device init function. Called when the OS initializes/loads the device driver into memory.
 *
 * @param DeviceSegList
 * @param DevBasePointer
 * @param exec_base
 * @return
 */
struct Library * DeviceInit(BPTR DeviceSegList, struct Library * DevBasePointer, struct Library * execBase)
{
   //Activate "Automatic Open Library support of libnix".
   //Because we don't use an device startup here we must all the auto-open-function by hand.
   //If not all libraries are not open automatically especially the "utility.library" when using "libnix"
   //will crash dividing integer in function "ito2()".
   __oslibversion = 37l; //open all libs with at least Version 37
   __initlibraries();

   DEBUGOUT((1, "DevInit: DeviceSegList=0x%lx, BasePointer=0x%lx, execBase=0x%lx\n", DeviceSegList, DevBasePointer, execBase));

   //take over Device Base Pointer...
   globEtherDevice = (struct DeviceDriver *)DevBasePointer;


   CHECK_LIBRARY_IS_OPEN(DOSBase);
   CHECK_LIBRARY_IS_OPEN(IntuitionBase);

   //init device structure  
   globEtherDevice->ed_Device.lib_Node.ln_Type = NT_DEVICE;
   globEtherDevice->ed_Device.lib_Node.ln_Name = (UBYTE *)DevName;
   globEtherDevice->ed_Device.lib_Flags        = LIBF_CHANGED | LIBF_SUMUSED;
   globEtherDevice->ed_Device.lib_Version      = (UWORD)DevVersion;
   globEtherDevice->ed_Device.lib_Revision     = (UWORD)DevRevision;
   globEtherDevice->ed_Device.lib_IdString     = (char *)DevIdString;
   
   globEtherDevice->ed_SegList = DeviceSegList;

   //Create a dummy hook which do nothing...
   bzero(&globEtherDevice->ed_DummyPFHook, sizeof(globEtherDevice->ed_DummyPFHook));
   globEtherDevice->ed_DummyPFHook.h_Entry = &FakePFHookEntry;  // Pointer to a simple "RTS" function...
   
   // Device Semaphores
   InitSemaphore((APTR)&globEtherDevice->ed_DeviceLock);
   InitSemaphore((APTR)&globEtherDevice->ed_MCAF_Lock);
   
   DEBUGOUT((1, "DevInit end, returning 0x%lx\n", globEtherDevice));
   return (struct Library *)globEtherDevice;
}


/**
 * Device open function. Called from OS when the device is opened.
 *
 * @param ios2
 * @param s2unit
 * @param s2flags
 * @param devPointer
 */
void DeviceOpen(struct IOSana2Req *ios2,
      ULONG s2unit,
      ULONG s2flags,
      struct Library * devPointer)
{  
   struct DeviceDriverUnit *etherUnit;
   struct BufferManagement *bm;
   BOOL success = FALSE; 

   /*
   DEBUGOUT((VERBOSE_DEVICE, "Parameter Test: '1' == %ld (16 bit)\n", (uint16_t) 1));
   DEBUGOUT((VERBOSE_DEVICE, "Parameter Test: '2' == %ld (32 bit)\n", (uint32_t) 2));
   TRACE_DEBUG("Parameter Test: '0x5678'     == 0x%04lx (32 bit)\n", (uint32_t)0x5678);
   TRACE_DEBUG("Parameter Test: '0x02'       == 0x%02lx (8 bit)\n",  (uint32_t)0x02);
   DEBUGOUT((VERBOSE_DEVICE, "Parameter Test: '0x12345678' == 0x%lx (32 bit)\n", (uint32_t) 0x12345678));*/

   DEBUGOUT((VERBOSE_DEVICE, "DevOpen(ioreq=0x%lx, unit=%ld, flags=0x%lx, devPointer=0x%lx)...\n", ios2, s2unit, s2flags, devPointer));

   CHECK_LIBRARY_IS_OPEN(UtilityBase);

   ObtainSemaphore((APTR)&globEtherDevice->ed_DeviceLock);

   globEtherDevice->ed_Device.lib_OpenCnt++;

   //Only when device is opened check that the stack is at leased 5000 bytes in size.
   //If not device would crash for some reason somewhere because of that.
   //Bring up waring. Exit with failure.
   if (!checkStackSpace(STACK_SIZE_MINIMUM))
   {
      DEBUGOUT((VERBOSE_DEVICE, "Stack too small!!!!\n"));
      goto end;
   }

   //At this time unit "0" is only supported.
   if(s2unit < ED_MAXUNITS)
   {
      //jetzt exklusiven Zugriff ?
      if (s2flags & SANA2OPF_MINE)
      {
         //schon von jemanden anderen geoeffnet ?
         if (!(globEtherDevice->ed_Device.lib_OpenCnt > 1))
            globEtherDevice->ed_Device.lib_Flags |= ETHERUF_EXCLUSIVE;
         else
            goto end;
      }

      //open in promisc mode ? (only in conjunction with flag MINE!!!)
      if (s2flags & SANA2OPF_PROM)
      {
         if (s2flags & SANA2OPF_MINE)
            globEtherDevice->ed_Device.lib_Flags |= ETHERUF_PROMISC;
         else
            goto end;
      }

      //get access to the low level driver
      NetInterface * lowLevelDriver = initModule();

      //Test and check hardware
      if (NO_ERROR == lowLevelDriver->probe(lowLevelDriver))
      {
         //Bring up unit device process...
         etherUnit = InitUnitProcess(s2unit,globEtherDevice);
         if( etherUnit )
         {
            etherUnit->eu_lowLevelDriver = lowLevelDriver;

            //PromMode setzen/löschen wenn Device schon online
            if (etherUnit->eu_State & ETHERUF_ONLINE)
            {
               //TODO:
            }

            /*
             * Get Memory for storing buffer management functions
             */
            bm = AllocMem(sizeof(struct BufferManagement), MEMF_CLEAR | MEMF_PUBLIC);
            if( bm )
            {
               struct TagItem *bufftag;
               bzero(bm, sizeof(struct BufferManagement));

               /* Init the list for CMD_READ requests */
               InitSemaphore((APTR) &bm->bm_RxQueueLock);
               NewList((struct List *) &bm->bm_RxQueue);

               //Add BM to the list of all BM's...
               AddTail((struct List *)&etherUnit->eu_BuffMgmt,(struct Node *)bm);

               const struct TagItem * tagItem = ios2->ios2_BufferManagement;
               if(tagItem)
               {
                  print(5,"\n");
                  bufftag = FindTagItem(S2_CopyToBuff, tagItem);
                  if(bufftag && !bm->bm_CopyToBuffer)
                  {
                     bm->bm_CopyToBuffer = (SANA2_CTB) bufftag->ti_Data;
                     print(5,"  SANA2_CTB-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyToBuff16, tagItem);
                  if (bufftag) {
                     bm->bm_CopyToBuffer = (SANA2_CTB) bufftag->ti_Data;
                     print(5, "  SANA2_CTB16-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyFromBuff, tagItem);
                  if(bufftag && !bm->bm_CopyFromBuffer)
                  {
                     bm->bm_CopyFromBuffer = (SANA2_CFB) bufftag->ti_Data;
                     print(5,"  SANA2_CFB-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyFromBuff16, tagItem);
                  if (bufftag) {
                     bm->bm_CopyFromBuffer = (SANA2_CFB) bufftag->ti_Data;
                     print(5, "  SANA2_CFB16-Tag  found...\n");
                  }

                  bufftag = FindTagItem(S2_PacketFilter, tagItem);
                  if(bufftag)
                  {
                     bm->bm_PacketFilterHook = (struct Hook *) bufftag->ti_Data;
                     print(5,"  SANA2_PacketFilter-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_DMACopyFromBuff32, tagItem);
                  if (bufftag) {
                     bm->bm_CopyFromBufferDMA = (SANA2_CFB) bufftag->ti_Data;
                     print(5, "  S2_DMACopyFromBuff32-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_DMACopyToBuff32, tagItem);
                  if (bufftag) {
                     bm->bm_CopyToBufferDMA = (SANA2_CFB) bufftag->ti_Data;
                     print(5, "  S2_DMACopyToBuff32-Tag found...\n");
                  }

                  //TODO: Add SANA2R3 Hook extensions functions...

                  //print out all tags:
#if DEBUG > 0
                  DEBUGOUT((5, "All buffer management tags: "));
                  struct TagItem *tstate;
                  struct TagItem *tag;
                  tstate = (struct TagItem *) ios2->ios2_BufferManagement;
                  while ((tag = NextTagItem(&tstate)) != NULL)
                  {
                     DEBUGOUT((5, " 0x%lx", (ULONG)tag->ti_Tag));
                  }
                  DEBUGOUT((5, "\n"));
#endif
               }

               /*
                * Using dummy buffer management function when no callbacks are given.
                */
               if (!bm->bm_PacketFilterHook)
                  bm->bm_PacketFilterHook = &globEtherDevice->ed_DummyPFHook;
               if (!bm->bm_CopyToBuffer)
                  bm->bm_CopyToBuffer     = (APTR)&globEtherDevice->ed_DummyPFHook.h_Entry;
               if (!bm->bm_CopyFromBuffer)
                  bm->bm_CopyFromBuffer   = (APTR)&globEtherDevice->ed_DummyPFHook.h_Entry;

               success = TRUE;

               globEtherDevice->ed_Device.lib_OpenCnt++;
               globEtherDevice->ed_Device.lib_Flags &=~LIBF_DELEXP;
               etherUnit->eu_Unit.unit_OpenCnt++;

               /* Fix up the initial IO request */
               ios2->ios2_BufferManagement = (void *)bm; // <= replaces the original list tags with new BM struct!
               ios2->ios2_Req.io_Error = S2ERR_NO_ERROR;
               ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
               ios2->ios2_Req.io_Unit = (struct Unit *)etherUnit;
               ios2->ios2_Req.io_Device = (struct Device *)globEtherDevice;
            }
         }
      }
   }

   end:

   if (!success)
   {
      ios2->ios2_Req.io_Error = IOERR_OPENFAIL;
      ios2->ios2_Req.io_Unit = (struct Unit *) -1;
      ios2->ios2_Req.io_Device = (struct Device *) -1;
   }

   globEtherDevice->ed_Device.lib_OpenCnt--;  
   ReleaseSemaphore((APTR)&globEtherDevice->ed_DeviceLock);

   DEBUGOUT((VERBOSE_DEVICE, "DevOpen()...ends.\n"));
}



/**
 * Device close function.
 *
 * @param ios2
 * @param DevBase
 * @return
 */
BPTR DeviceClose(struct IOSana2Req *ios2, struct Library * DevBase)
{
   struct DeviceDriverUnit *etherUnit;
   BPTR seglist = 0L;

   DEBUGOUT((VERBOSE_DEVICE, "*DevClose(ioreq=0x%lx)\n", ios2));

   //Check IORequest for validity to not close device twice!
   if (ios2->ios2_Req.io_Device != (struct Device *) -1 //
      && ios2->ios2_Req.io_Unit != (struct Unit *) -1)
   {
      ObtainSemaphore((APTR) &globEtherDevice->ed_DeviceLock);

      etherUnit = (struct DeviceDriverUnit *) ios2->ios2_Req.io_Unit;

      //Abort every read request of the caller...
      struct BufferManagement * bm = (struct BufferManagement *) ios2->ios2_BufferManagement;
      if (bm)
      {
         ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
         AbortReqList(&bm->bm_RxQueue, globEtherDevice);
         ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);

         // Find the single buffer management from the caller and free it
         ObtainSemaphore((APTR) &etherUnit->eu_BuffMgmtLock);
         for (bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head; //
               bm->bm_Node.mln_Succ; //
               bm = (struct BufferManagement *) bm->bm_Node.mln_Succ)
         {
            if (bm == ios2->ios2_BufferManagement)
            {
               Remove((struct Node *) bm);
               FreeMem(bm, sizeof(struct BufferManagement));
               ios2->ios2_BufferManagement = NULL;
               break;
            }
         }
         ReleaseSemaphore((APTR) &etherUnit->eu_BuffMgmtLock);
      } else {
         DEBUGOUT((VERBOSE_DEVICE, "DevClose() BM is already NULL.\n"));
      }

      //Reset always exclusive, loopback or promiscue mode...
      globEtherDevice->ed_Device.lib_Flags &= ~ETHERUF_EXCLUSIVE;
      globEtherDevice->ed_Device.lib_Flags &= ~ETHERUF_LOOPBACK;
      globEtherDevice->ed_Device.lib_Flags &= ~ETHERUF_PROMISC;

      /* Trash the io_Device and io_Unit fields so that any attempt to use this
       request will die immediately. */
      ios2->ios2_Req.io_Device = (struct Device *) -1;
      ios2->ios2_Req.io_Unit = (struct Unit *) -1;

      /* I always shut the unit process down if the open count drops to zero.
       That way, if I need to expunge, I never have to Wait(). */
      etherUnit->eu_Unit.unit_OpenCnt--;

      if (etherUnit->eu_Unit.unit_OpenCnt == 0)
      {
         ExpungeUnit(etherUnit->eu_UnitNum, globEtherDevice);
         etherUnit = NULL;
      }

      globEtherDevice->ed_Device.lib_OpenCnt--;

      ReleaseSemaphore((APTR) &globEtherDevice->ed_DeviceLock);

      // Check to see if we've been asked to expunge.
      // In this case expunge Device now...
      if (globEtherDevice->ed_Device.lib_Flags & LIBF_DELEXP)
      {
         seglist = DeviceExpunge(DevBase);
         DEBUGOUT((VERBOSE_DEVICE, "DevClose(): Device was expunged (cause of an delayed expunge)!\n"));
         DevBase = NULL;
         globEtherDevice = NULL;
      }
   } else {
      DEBUGOUT((VERBOSE_DEVICE, "DevClose() invalid iorequest!\n"));
   }

   DEBUGOUT((VERBOSE_DEVICE, "DevClose() ends.\n\n"));

   //If a seglist is returned the device will be unloaded by the system...
   return seglist;
}

/**
 * Device Expunge function.
 *
 * ; IMPORTANT:
 * ;   These calls (Open/Close/Expunge) are guaranteed to be single-threaded; only one task
 * ;   will execute your Open/Close/Expunge at a time.
 * ;
 * ;   For Kickstart V33/34, the single-threading method involves "Forbid".
 * ;   There is a good chance this will change.  Anything inside your
 * ;   Open/Close/Expunge that causes a direct or indirect Wait() will break
 * ;   the Forbid().  If the Forbid() is broken, some other task might
 * ;   manage to enter your Open/Close/Expunge code at the same time.
 * ;   Take care!
 * ;
 * ; Since exec has turned off task switching while in these routines
 * ; (via Forbid/Permit), we should not take too long in them.
 *
 * One other important note: because Expunge is called from the memory
 * allocator, it may NEVER Wait() or otherwise take long time to complete.
 *
 * Note: You may NEVER EVER Wait() in expunge. Period.
 *       Don't even *think* about it.
 *
 * @param DevBase
 * @return
 */
BPTR DeviceExpunge(struct Library * DevBase)
{
    BPTR seglist = (BPTR)0L;

    DEBUGOUT((VERBOSE_DEVICE, "\nDevExpunge(devbase=0x%lx)\n", DevBase));

    //Check if device is still used?
    if(globEtherDevice->ed_Device.lib_OpenCnt)
    {
        //Device is still in use! Mark device itself that it should expunge after next DevClose()...
        globEtherDevice->ed_Device.lib_Flags |= LIBF_DELEXP;
        DEBUGOUT((VERBOSE_DEVICE, "DevExpunge(): Can't expunge because device is still in use (cntr=%ld)!\n",
              globEtherDevice->ed_Device.lib_OpenCnt));
    }
    else
    {
       //At this time the unit MUST have been shutdown (on last DevClose()).
       //Because of the special problem that we can't wait here when Expunge is called.
       if (globEtherDevice->ed_Units[0] != NULL)
       {
          Alert(0x1818);
       }

       /* Free up our library base and function table after
         removing ourselves from the library list. */
       Disable();
       Remove((struct Node *)DevBase);
       Enable();

       seglist          = ((struct DeviceDriver*)DevBase)->ed_SegList;
       ULONG devbase    = (ULONG)DevBase;
       LONG devbasesize = (ULONG)DevBase->lib_NegSize;
       devbase          = devbase - devbasesize;
       devbasesize      += (ULONG)DevBase->lib_PosSize;

       FreeMem((APTR)devbase,devbasesize);
       globEtherDevice = NULL;
       //The device base is now gone! Do not access them anymore!

       DEBUGOUT((VERBOSE_DEVICE, "DevExpunge() ends. Device is expunged now...\n\n"));
    }

    //Freeing all auto-open libraries again after expunge lib.
    __exitlibraries();

    //If we return a real segment list it will be freed by AmigaOS...
    return seglist;
}

/**
 * Abort IORequest.
 *
 * @param ios2
 * @param DevPointer
 * @return io_error code again
 */
ULONG DeviceAbortIO( struct IOSana2Req *ios2, struct Library * DevPointer)
{
   ULONG result = IOERR_NOCMD; //Default: Error
   struct DeviceDriverUnit *etherUnit;

   DEBUGOUT((VERBOSE_DEVICE, "\nDevAbortIO(ioreq.cmd=0x%lx)\n", (ULONG)ios2->ios2_Req.io_Command));
   etherUnit = (struct DeviceDriverUnit *)ios2->ios2_Req.io_Unit;
   //Abort only pending requests....NT_MESSAGE means "pending"...
   if (ios2->ios2_Req.io_Message.mn_Node.ln_Type == NT_MESSAGE)
   {
      switch (ios2->ios2_Req.io_Command)
      {
         case CMD_READ:
         {
            struct BufferManagement *bm = ios2->ios2_BufferManagement;
            result = AbortRequestAndRemove(&bm->bm_RxQueue, ios2, globEtherDevice, &bm->bm_RxQueueLock);
            break;
         }

         case S2_BROADCAST:
         case CMD_WRITE:
         {
            Forbid();
            result = AbortRequestAndRemove((struct MinList *)&etherUnit->eu_Tx->mp_MsgList,
                  ios2, globEtherDevice, NULL);
            Permit();
            break;
         }

         case S2_ONEVENT:
         {
            Forbid();
            result = AbortRequestAndRemove(&etherUnit->eu_Events, ios2, globEtherDevice, NULL);
            Permit();
            break;
         }

         case S2_READORPHAN:
         {
            result = AbortRequestAndRemove(&etherUnit->eu_ReadOrphan, ios2, globEtherDevice,
                  &etherUnit->eu_ReadOrphanLock);
            break;
         }

         default: {
            result = S2ERR_SOFTWARE;
            DEBUGOUT((VERBOSE_DEVICE,"Unable to break this iorequest: 0x%x", ios2));
            break;
         }
      }
   }
   else
   {
      DEBUGOUT((VERBOSE_DEVICE,"DevAbortIO(): Request was not pending! Not removed!\n"));
      result = S2ERR_NO_ERROR; //Silent ok.
   }
   return(result);
}

/**
 * This function is used to locate an IO request in a linked
 * list and abort it if found. Request is removed from that list.
 *
 * @param minlist
 * @param ios2
 * @param EtherDevice
 * @param lock the list lock (Semaphore) in which the Request is located or NULL.
 *
 * @return io_error code: if found => IOERR_ABORTED, if NOT found => S2ERR_BAD_ARGUMENT
 */
static
ULONG AbortRequestAndRemove(struct MinList *minlist,
               struct IOSana2Req *ios2,
               struct DeviceDriver *EtherDevice,
               struct SignalSemaphore * lock)
{
   struct Node *node, *next;
   ULONG result = S2ERR_BAD_ARGUMENT; //If IORequest was not found...

   DEBUGOUT((VERBOSE_DEVICE, "AbortReq(ioreq.cmd=0x%x)\n", ios2->ios2_Req.io_Command));

   if (lock)
      ObtainSemaphore(lock);

   node = (struct Node *) minlist->mlh_Head;
   while (node->ln_Succ)
   {
      next = node->ln_Succ;
      if (node == (struct Node *) ios2)
      {
         Remove((struct Node *) ios2);
         result = ios2->ios2_Req.io_Error = IOERR_ABORTED;
         TermIO(ios2, EtherDevice);
         break;
      }
      node = next;
   }

   if (lock)
      ReleaseSemaphore(lock);

   if (result != IOERR_ABORTED)
   {
      DEBUGOUT((VERBOSE_DEVICE, "AbortReqAndRemove(): IORequest not found! Nothing to abort!"));
   }

   return (result);
}

/**
 * Abort an complete list of IO requests...
 *
 * @param minlist
 * @param EtherDevice
 */
static void  AbortReqList(struct MinList *minlist,struct DeviceDriver * EtherDevice)
{
    struct Node *node, *next;
    struct IOSana2Req * ActIos2;

    node = (struct Node *)minlist->mlh_Head;
    while(node->ln_Succ)
    {
        next = node->ln_Succ;
        ActIos2 = ((struct IOSana2Req *)node);

        DEBUGOUT((VERBOSE_DEVICE, "AbortReqList(ioreq.cmd=0x%x)\n", ActIos2->ios2_Req.io_Command));
         
        Remove((struct Node *)ActIos2);
        ActIos2->ios2_Req.io_Error = IOERR_ABORTED;
        TermIO(ActIos2,EtherDevice);                
        
        node = next;
    }    
}

/**
 * Flush device. All current IORequests are aborted...
 * Method can be invoked with iorequest == null.
 *
 * @param STDETHERARGS
 * @param STDETHERARGS
 * @param STDETHERARGS
 */
VOID DevCmdFlush(STDETHERARGS)
{
   struct BufferManagement *bm;

   DEBUGOUT((VERBOSE_DEVICE, "\nDevCmdFlush(ioreq=0x%lx)\n", (ULONG)ios2));

   //Abort all READ Requests of all opener...
   ObtainSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);
   bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head;
   while (bm->bm_Node.mln_Succ)
   {
      ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
      AbortReqList(&bm->bm_RxQueue, globEtherDevice);
      ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);
      bm = (struct BufferManagement *) bm->bm_Node.mln_Succ;
   }
   ReleaseSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);


   Forbid();
   //Stop all pending Write Requests
   AbortReqList((struct MinList *) &etherUnit->eu_Tx->mp_MsgList, globEtherDevice);
   //Stop all pending S2_ONEVENT:
   AbortReqList(&etherUnit->eu_Events, globEtherDevice);
   Permit();

   if (NULL != ios2)
      TermIO(ios2, globEtherDevice);

   DEBUGOUT((VERBOSE_DEVICE, "\nDevCmdFlush() ends.\n"));
}

/**
 * Initialize (if needed) a new ETHER device Unit and process.
 * There is only one unit process.
 *
 * @param s2unit
 * @param EtherDevice
 * @return
 */
struct DeviceDriverUnit *InitUnitProcess(ULONG s2unit, struct DeviceDriver *EtherDevice)
{
    struct DeviceDriverUnit *etherUnit;
    struct TagItem NPTags[]={{NP_Entry,      (ULONG)&DevProcEntry},
                             {NP_Name ,      (ULONG) DEVICE_TASK_NAME },
                             {NP_Priority,   UNIT_PROCESS_PRIORITY} ,
                             {NP_FreeSeglist,FALSE} ,
                             {NP_StackSize,  8000},
                             {TAG_DONE,      0}};

    /* Check to see if the Unit is already up and running.  If
       it is, just drop through.  If not, try to start it up. */
    if(!EtherDevice->ed_Units[s2unit])
    {
            /* Allocate a new Unit structure */
            etherUnit = AllocMem(sizeof(struct DeviceDriverUnit), MEMF_CLEAR | MEMF_PUBLIC);
            if( etherUnit )
            {
                /* Do some initialization on the Unit structure */
                NewList(&etherUnit->eu_Unit.unit_MsgPort.mp_MsgList);

                etherUnit->eu_Unit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;
                etherUnit->eu_Unit.unit_MsgPort.mp_Flags = PA_IGNORE;
                etherUnit->eu_Unit.unit_MsgPort.mp_Node.ln_Name = DEVICE_NAME ".port";

                etherUnit->eu_UnitNum = s2unit;
                etherUnit->eu_Device  = (struct Device *) EtherDevice;

                etherUnit->eu_MTU = 1500;

                InitSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);
                NewList((struct List *)&etherUnit->eu_BuffMgmt);

                InitSemaphore((void *)&etherUnit->eu_ReadOrphanLock);
                NewList((struct List *)&etherUnit->eu_ReadOrphan);

                //Event list has no lock
                NewList((struct List *)&etherUnit->eu_Events);

                InitSemaphore((void *)&etherUnit->eu_TrackLock);
                NewList((struct List *)&etherUnit->eu_Track);

                /* Try to read in the configuration file (again) */
                NetInterface * lowLevelDriver = initModule();
                if(ReadConfig(EtherDevice, lowLevelDriver->getConfigFileName()))
                {
                    /* Start up the unit process */
                    struct MsgPort * replyport = CreateMsgPort();
                    if( replyport )
                    {
                        EtherDevice->ed_Startup.Msg.mn_ReplyPort = replyport;
                        EtherDevice->ed_Startup.Device = (struct Device *) EtherDevice;
                        EtherDevice->ed_Startup.Unit = (struct Unit *)etherUnit;

                        etherUnit->eu_Proc = CreateNewProc(NPTags);
                        if(etherUnit->eu_Proc)
                        {
                            PutMsg((APTR)&etherUnit->eu_Proc->pr_MsgPort,
                                   (APTR)&EtherDevice->ed_Startup);
                            WaitPort((APTR)replyport);
                            GetMsg((APTR)replyport);
                        }                                                
                        DeleteMsgPort((APTR)replyport);         
                    }
                }
                if(!etherUnit->eu_Proc)
                {
                    // The Unit process couldn't start for some reason, 
                    // so free the Unit structure.
                    FreeMem(etherUnit, sizeof(struct DeviceDriverUnit));
                    etherUnit = NULL;
                }
                else
                {
                    /* Set up the Unit structure pointer in the device base */
                    EtherDevice->ed_Units[s2unit] = etherUnit;
                }
            }

    }    
    return(EtherDevice->ed_Units[s2unit]);
}

/**
 * Attempt to read in and parse the driver's configuration file.
 *
 * The file is named by ENV:SANA2/etherbridge.config for all units
 *
 * @param EtherDevice
 * @return
 */
static BOOL ReadConfig( struct DeviceDriver *etherDevice, const char * sConfigFile )
{
    DEBUGOUT((VERBOSE_DEVICE, "ReadConfigFile: %s\n", sConfigFile));

    RegistryInit( sConfigFile );
    {
       debugLevel = ReadKeyInt("DEBUGLEV" , false);
    }
    RegistryDestroy();

    DEBUGOUT((VERBOSE_DEVICE,"\n\n ##### " DEVICE_NAME " Device #####\n"));
    DEBUGOUT((VERBOSE_DEVICE,"%s\n", DevIdString));
    DEBUGOUT((VERBOSE_DEVICE,"DebugLev= %ld\n", (LONG)debugLevel));

    return true;
}

/**
 * Expunge Unit. The unit process is terminated and the unit structure is freed...
 *
 * This function is called from the DevClose routine when the open count for a
 * unit reaches zero.  This routine signals the unit process to exit and then
 * waits for the unit process to acknowledge.  The unit structure is then
 * freed.
 * @param etherUnit
 * @param EtherDevice
 */
VOID ExpungeUnit(UBYTE unitNumber, struct DeviceDriver *etherDevice)
{
   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit(%d)...\n", unitNumber));
   struct DeviceDriverUnit * etherUnit = etherDevice->ed_Units[unitNumber];
   if (etherUnit)
   {
      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():1 (deactivate unit number %d)\n", unitNumber));

      struct Task * unittask = (struct Task *) etherUnit->eu_Proc;
      etherUnit->eu_Proc = (struct Process *) FindTask(0L);

      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():2 (waiting for unit process is terminated...)\n"));

      //The unit process itself frees all used interrupt handlers...

      //Shutdown Device Unit Process. Wait until he knowledges...
      Signal(unittask, SIGBREAKF_CTRL_F);
      Wait(SIGBREAKF_CTRL_F);

      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():22\n"));

      FreeMem(etherUnit, sizeof(struct DeviceDriverUnit));

      etherDevice->ed_Units[unitNumber] = NULL;
      etherUnit = NULL;
   }

   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():3\n"));

   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit()...end.\n"));
}

/**
 * This is the entry point for the Device Unit process.
 */
SAVEDS void DevProcEntry(void)
{     
    struct Process *proc;
    struct DeviceDriverUnit *etherUnit;
    struct StartupMessage *startupMessage;
    struct IOSana2Req *ios2;
    ULONG receivedSignals;
    BYTE configFileNotifySigBit;
    struct NotifyRequest configNotify = {0};

    /* Find our Process pointer and wait for our startup
       message to arrive. */      
    proc = (struct Process *)FindTask(0L); 
    WaitPort((void *)&proc->pr_MsgPort);

    DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process is coming up!\nStack Size: %ld\nthisTask=0x%lx", proc->pr_StackSize, proc));

    /* Pull the startup message off of our process messageport. */
    startupMessage = (void *)GetMsg((void*)&proc->pr_MsgPort);

    /* Grab our Unit pointer. */
    etherUnit    = (struct DeviceDriverUnit *)startupMessage->Unit;

    /* Attempt to allocate a signal bit for our Unit MsgPort. */

    /* Set up our Unit's MsgPort. */
    etherUnit->eu_Unit.unit_MsgPort.mp_SigBit  = AllocSignal(-1L);
    etherUnit->eu_Unit.unit_MsgPort.mp_SigTask = (struct Task *)proc;
    etherUnit->eu_Unit.unit_MsgPort.mp_Flags   = PA_SIGNAL;

    /* Initialize our Transmit Request lists. Because it's a Message port
     * Forbid and Permit is used for locks. */
    etherUnit->eu_Tx = CreateMsgPort();
    /*etherUnit->eu_Tx->mp_SigBit  = AllocSignal(-1L);
    etherUnit->eu_Tx->mp_SigTask = (struct Task *)proc;
    etherUnit->eu_Tx->mp_Flags   = PA_SIGNAL;*/

    /* alles in Ordnung */
    etherUnit->eu_Proc = proc;

    /* Reply to our startup message */
    ReplyMsg((struct Message *)startupMessage);    

    /* Check etherUnit->eu_Proc to see if everything went okay up
       above. */
    if(etherUnit->eu_Proc)    
    {
        NetInterface * lowLevelDriver = initModule();
        const char * sConfigFile = lowLevelDriver->getConfigFileName();
    
        // Init DOS Notify for etherbridge.config file
        configFileNotifySigBit                       = AllocSignal(-1);
        configNotify.nr_Name                         = (char*)sConfigFile;
        configNotify.nr_Flags                        = NRF_SEND_SIGNAL;
        configNotify.nr_stuff.nr_Signal.nr_Task      = &etherUnit->eu_Proc->pr_Task;
        configNotify.nr_stuff.nr_Signal.nr_SignalNum = configFileNotifySigBit;
        StartNotify(&configNotify);

        DEBUGOUT((VERBOSE_DEVICE,"Unit Process: Enter service loop...\n"));
        //Unit Task Service Loop:
        for(;;)
        {
            //Waiting for any signal:
            receivedSignals = Wait(0xffffffff);

            /* Have we been signaled to shut down? */
            if (receivedSignals & SIGBREAKF_CTRL_F) {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: SIGBREAKF_CTRL_F received.\n"));
               break;
            }
         
            //lowlevel hardware activity?
            if (receivedSignals & (1l << etherUnit->eu_lowLevelDriverSignalNumber)) {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: process low level event.\n"));
               etherUnit->eu_lowLevelDriver->processEvents(etherUnit->eu_lowLevelDriver);
            }

            //New packets to send?
            /*if (receivedSignals & (1l << etherUnit->eu_Tx->mp_SigBit)) {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Processing TX packets.\n"));
               sendAllPackets(etherUnit,globEtherDevice);
            }*/

            //perform IORequest that should be done by the uni process...
            if ((receivedSignals & (1l << etherUnit->eu_Unit.unit_MsgPort.mp_SigBit)))
            {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Processing new IORequests.\n"));
               while ((ios2 = (void *) GetMsg((void *) etherUnit)) != NULL)
               {
                  PerformIO(ios2, globEtherDevice);
               }
            }

            // Configuration file changed?
            if (receivedSignals & (1L << configFileNotifySigBit))
            {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Config file changed.\n"));
               ReadConfig(globEtherDevice, lowLevelDriver->getConfigFileName());
            }
         }

         DEBUGOUT((VERBOSE_DEVICE,"Unit Process: Leaving service loop...\n"));

         //Set driver offline (TODO: event needed?)
         etherUnit->eu_State &= ~ETHERUF_ONLINE;

         //stops access to the hardware in any way...
         etherUnit->eu_lowLevelDriver->deinit(etherUnit->eu_lowLevelDriver);

         //Free signal bit from TX packets
         if (etherUnit->eu_Tx->mp_SigBit != -1) {
            FreeSignal( etherUnit->eu_Tx->mp_SigBit );
            etherUnit->eu_Tx->mp_SigBit = -1;
         }

         // Stop notification the config file
         EndNotify(&configNotify);
         bzero(&configNotify, sizeof (configNotify));

         FreeSignal(etherUnit->eu_Unit.unit_MsgPort.mp_SigBit);
         etherUnit->eu_Unit.unit_MsgPort.mp_SigBit = -1;
         FreeSignal(configFileNotifySigBit);
         configFileNotifySigBit = -1;

         /* Signal the other side we're exiting.  Make sure that
            we exit before they wake up by using the same trick
            when replying to Workbench startup messages. */
         DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Terminating...bye bye\n"));
         Forbid();
         Signal((struct Task *)etherUnit->eu_Proc,SIGBREAKF_CTRL_F);
    }
    else
    {
        /* Something went wrong in the init code.  Drop out. */
        if(etherUnit->eu_Unit.unit_MsgPort.mp_SigBit != -1)
        {
           FreeSignal(etherUnit->eu_Unit.unit_MsgPort.mp_SigBit);
           etherUnit->eu_Unit.unit_MsgPort.mp_SigBit = -1;
        }
    } 
}


/*
 * Hinweis: Folgende Kommandos dürfen NUR vom Etherunit Task ausgeführt werden (weil Signalhandler installiert werden,
 * die vom Device task empfange werden müssen):
 *  - S2_ONLINE
 *  - S2_GetStationAddress
 *  - S2_CONFIGINTERFACE
 **/
#define PERFORM_NOW ((1L<<CMD_WRITE)     |            \
                     (1L<<CMD_READ)      |            \
                     (1L<<S2_BROADCAST)  |            \
                     (1L<<S2_READORPHAN) |            \
                     (1L<<S2_MULTICAST) )
/**
 * BeginIO: This is the dispatch point for the driver's incoming IORequests.
 *
 * @param ios2
 * @param deviceBase
 */
void DeviceBeginIO(struct IOSana2Req * ios2,
                struct Library * deviceBase)
{
   register UWORD Cmd = ios2->ios2_Req.io_Command;
   if(((1L << Cmd) & PERFORM_NOW) || (Cmd >= NSCMD_DEVICEQUERY) )
   {
      //Processed request now!
      PerformIO(ios2,globEtherDevice);
   }
   else
   {
      //Let IOrequest process by the unit process...
      ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
      ios2->ios2_Req.io_Flags &= ~IOF_QUICK;
      PutMsg((void *)ios2->ios2_Req.io_Unit,(void *)ios2);
   }
}

/**
 * This routine is used to dispatch an IO request either from BeginIO
 * or from the Unit process.
 *
 * @param ios2
 * @param EtherDevice
 */
VOID PerformIO(struct IOSana2Req *ios2,
               struct DeviceDriver *EtherDevice)
{
    struct DeviceDriverUnit *etherUnit;

    etherUnit    = (struct DeviceDriverUnit *)ios2->ios2_Req.io_Unit;
    ios2->ios2_Req.io_Error = 0;
    switch(ios2->ios2_Req.io_Command)
    {
       case CMD_WRITE:                DevCmdWritePacket(ios2,etherUnit,EtherDevice);
                                      break;

       case CMD_READ:                 DevCmdReadPacket(ios2,etherUnit,EtherDevice);
                                      break;

       case S2_BROADCAST:             copyEthernetAddress((const APTR) BROADCAST_ADDRESS, ios2->ios2_DstAddr);
                                      DevCmdWritePacket(ios2,etherUnit,EtherDevice);
                                      break;

       case S2_MULTICAST:             DevCmdWritePacket(ios2,etherUnit,EtherDevice);
                                      break;

       case CMD_FLUSH:                DevCmdFlush(ios2,etherUnit,EtherDevice);
                                      break;

       //TODO: Add support:
/*        case S2_ADDMULTICASTADDRESS:   DevCmdAddMulti(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_DELMULTICASTADDRESS:   DevCmdRemMulti(ios2,etherUnit,EtherDevice);
                                       break;
*/
        case S2_ONEVENT:               DevCmdOnEvent(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_ONLINE:                DevCmdOnline(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_OFFLINE:               DevCmdOffline(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_DEVICEQUERY:           DevCmdDeviceQuery(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_GETSTATIONADDRESS:     DevCmdGetStationAddress(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_CONFIGINTERFACE:       DevCmdConfigInterface(ios2,etherUnit,EtherDevice);
                                       break;
     
        case S2_READORPHAN:            DevCmdReadOrphan(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_GETGLOBALSTATS:        DevCmdGlobStats(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_TRACKTYPE:             DevCmdTrackType(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_UNTRACKTYPE:           DevCmdUnTrackType(ios2,etherUnit,EtherDevice);
                                       break;

        case S2_GETSPECIALSTATS:       DevCmdGetSpecialStats(ios2,etherUnit,EtherDevice);
                                       break;

        case NSCMD_DEVICEQUERY:        DevCmdNSDeviceQuery(ios2,etherUnit,EtherDevice);
                                       break;

        // These commands are currently not supported by Etherbridge

        case S2_GETTYPESTATS:       
                                       DEBUGOUT((VERBOSE_DEVICE,"*GetTypeStats nicht unterstuetzt!!\n"));
                                       ios2->ios2_Req.io_Error = S2ERR_NOT_SUPPORTED;
                                       ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
                                       TermIO(ios2,EtherDevice);
                                       break;

        default:
                                       DEBUGOUT((VERBOSE_DEVICE,"*####### RECEIVED UNKNOWN DEVICE COMMAND!!! 0x%lx \n", (ULONG)ios2->ios2_Req.io_Command));
                                       ios2->ios2_Req.io_Error = S2ERR_NOT_SUPPORTED;
                                       ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
                                       TermIO(ios2,EtherDevice);
                                       break;
    }
}

/**
 * This function is used to return an IO request back to the sender.
 * @param ios2
 * @param EtherDevice
 */
VOID TermIO(struct IOSana2Req *ios2,struct DeviceDriver *EtherDevice)
{
    DEBUGOUT((VERBOSE_DEVICE,"TermIO ioreq=0x%lx\n", ios2));

    if(!(ios2->ios2_Req.io_Flags & IOF_QUICK))
    {
       if (ios2->ios2_Req.io_Message.mn_ReplyPort != NULL )
       {
          ReplyMsg(&ios2->ios2_Req.io_Message);
       }
       else
       {
          Alert(1); //no reply port????
       }
    }
}

/*
** This routine is called whenever an "important" SANA-II event occurs.
*/
VOID DoEvent(ULONG events,
             struct DeviceDriverUnit *etherUnit,
             struct DeviceDriver *EtherDevice)
{
   struct IOSana2Req *ios2;
   struct IOSana2Req *ios2_next;
   BOOL eventFired = false;

   DEBUGOUT((VERBOSE_DEVICE,"### DoEvent: Fire Event 0x%lx\n", events));

   Forbid();

   //Check ALL pending event requests
   ios2 = (struct IOSana2Req *) etherUnit->eu_Events.mlh_Head;
   while (ios2->ios2_Req.io_Message.mn_Node.ln_Succ)
   {
      ios2_next = (struct IOSana2Req *) ios2->ios2_Req.io_Message.mn_Node.ln_Succ;
      /* Are they waiting for any of these events? */
      if (ios2->ios2_WireError & events)
      {
         ios2->ios2_Req.io_Error = 0;
         ios2->ios2_WireError = events;
         Remove((struct Node *) ios2);
         TermIO(ios2, EtherDevice);
         eventFired = true;
      }
      ios2 = ios2_next;
   }
   Permit();

   if (!eventFired)
   {
      DEBUGOUT((VERBOSE_DEVICE,"  No one was waiting for that event. Event discarded.\n", events));
   }
}


/**
 * This function is used for handling CMD_WRITE.
 * commands. (from PerformIO)
 * @param STDETHERARGS
 * @param STDETHERARGS
 * @param STDETHERARGS
 */
VOID DevCmdWritePacket(STDETHERARGS)
{
    DEBUGOUT((VERBOSE_DEVICE,"\n*DevCmdWritePacket(type = %ld)\n", (ULONG)ios2->ios2_PacketType));
    /* Make sure that we are online. */
    if(etherUnit->eu_State & ETHERUF_ONLINE)
    {
        /* Make sure it's a legal length. */
        if(ios2->ios2_DataLength <= etherUnit->eu_MTU)
        {
            /* Queue the Write Request for the unit process. */
            ios2->ios2_Req.io_Flags &= ~IOF_QUICK;
            ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
            PutMsg((APTR)etherUnit->eu_Tx,(APTR)ios2);

            //Let's process packet is now!
            sendAllPackets(etherUnit,etherDevice);
        }
        else
        {
            /* Sorry, the packet is too long! */
            ios2->ios2_Req.io_Error = S2ERR_MTU_EXCEEDED;
            ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
            TermIO(ios2,globEtherDevice);
            DoEvent(S2EVENT_TX,etherUnit,globEtherDevice);
        }
    }
    else
    {
        /* Sorry, we're offline */
        ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        ios2->ios2_WireError = S2WERR_UNIT_OFFLINE;
        TermIO(ios2,globEtherDevice);
    }
}

#define MIN(a,b) ((a) < (b)) ? (a) : (b)


/*
 * Copy packet to given Sana 2 request. Returns true if packet was copied to the request.
 * Returns falls when not for some reason.
 */
BOOL fulfillReadIORequest( struct DeviceDriver * etherDevice,
                        struct DeviceDriverUnit * etherUnit,
                        struct IOSana2Req * ios2,
                        uint8_t * rawEtherPacket,
                        uint16_t rawPacketLength
                        )
{
   struct BufferManagement * bm;
   BOOL bStatus=FALSE;
   MacAddr * dstAddress   = (MacAddr *) (rawEtherPacket+0);
   MacAddr * srcAddress   = (MacAddr *) (rawEtherPacket+6);
   uint16_t pktType       = *(uint16_t*)(rawEtherPacket+12);
   uint8_t * pktDataForIORequest = rawEtherPacket;


   bm = ios2->ios2_BufferManagement;
   if (bm)
   {
      if(!(ios2->ios2_Req.io_Flags & SANA2IOF_RAW)) {

         //
         // Cooked:
         //

         //Tighten the packet to only the payload itself...
         pktDataForIORequest += 14;
         rawPacketLength -= 14;

         //Respect MTU...
         if (rawPacketLength > 1500) {
            rawPacketLength = 1500;
         }
      } else {

         //
         // RAW:
         //

         //Respect MTU 1500 + Ethernet Header
         if (rawPacketLength > 1514) {
            rawPacketLength = 1514;
         }
      }

      //Copy addresses
      copyEthernetAddress((uint8_t*)dstAddress ,ios2->ios2_DstAddr);  // Destination address
      copyEthernetAddress((uint8_t*)srcAddress ,ios2->ios2_SrcAddr);  // Source Address
      //Copy packet type + packet length....
      ios2->ios2_PacketType = pktType;
      ios2->ios2_DataLength = rawPacketLength;

      //Check Broadcast / Multicast (bit 0 of first byte is "1" if multicast or broadcast)
      if ((dstAddress->b[0] & 1) != 0 ) {
         if (isBroadcastEthernetAddress((uint8_t*)dstAddress)) {
            ios2->ios2_Req.io_Flags |= SANA2IOF_BCAST;
         } else {
            ios2->ios2_Req.io_Flags |= SANA2IOF_MCAST;
         }
      }

      DEBUGOUT((VERBOSE_HW, "  IOReq DST    : ")); printEthernetAddress(ios2->ios2_DstAddr);  DEBUGOUT((VERBOSE_HW, "\n"));
      DEBUGOUT((VERBOSE_HW, "  IOReq SRC    : ")); printEthernetAddress(ios2->ios2_SrcAddr);  DEBUGOUT((VERBOSE_HW, "\n"));
      DEBUGOUT((VERBOSE_HW, "  IOReq LEN    : %ld\n", (ULONG)ios2->ios2_DataLength));
      DEBUGOUT((VERBOSE_HW, "  IOReq TYP    : 0x%lx\n", (ULONG)ios2->ios2_PacketType));

      // Frage Stack, ob er das Packet auch wirklich moechte (Paketfilter)!
      if(CallFilterHook(bm->bm_PacketFilterHook, ios2, pktDataForIORequest))
      {
         //DEBUGOUT((VERBOSE_HW,"  Filter hook passed. Result: Not skipped.\n"));
         if(CopyToBuffer((APTR)ios2->ios2_Data,(APTR)pktDataForIORequest, rawPacketLength ))
         {
            DEBUGOUT((VERBOSE_HW,"Pkt copied successfully:\n"));
            dumpMem(pktDataForIORequest,rawPacketLength);
            bStatus = true;
         }
         else
         {
            DEBUGOUT((VERBOSE_HW,"  Error CopyToBuffer()!\n"));
            ios2->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            ios2->ios2_WireError    = S2WERR_BUFF_ERROR;
            DoEvent(S2EVENT_BUFF,etherUnit,globEtherDevice);
         }

         //Remove() is safe because we are in List lock when this function is called.
         Remove((APTR)ios2);
         TermIO(ios2,etherDevice);
      } else {
         DEBUGOUT((VERBOSE_HW,"  Filter hook passed. Result: Packet SKIPPED!\n"));
      }
   }

   return bStatus;
}


/**
 * Entry point of the lowleveldriver, an packet was received...
 */
static void onPktReceived(uint8_t * buffer, uint16_t size ) {
   TRACE_DEBUG("onPktReceived:\n");

   bool resultIsPacketDelivered;
   struct BufferManagement *bm;
   struct IOSana2Req *ios2;
   const uint16_t packetType = *(uint16_t*)(buffer+12);

   //TODO: Find a way to get back the driver unit that is used here. Currently this is always unit "0".
   struct DeviceDriverUnit * etherUnit = globEtherDevice->ed_Units[0];


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
         if ((ios2->ios2_PacketType == packetType)
               || ((ios2->ios2_PacketType <= 1500) && (packetType <= 1500))) {

            // Copy packet to IORequest...
            resultIsPacketDelivered = fulfillReadIORequest(globEtherDevice, etherUnit, ios2, buffer, size );
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
      struct MinList * readOrphanList = &etherUnit->eu_ReadOrphan;
      if (!IsListEmpty((struct List* )readOrphanList)) {
         struct IOSana2Req * firstOrphan = GET_FIRST(etherUnit->eu_ReadOrphan);
         DEBUGOUT((VERBOSE_HW, "At least one ReadOrphan is waiting (ioreq=0x%lx)\n", firstOrphan));
         pktTransfered = fulfillReadIORequest(globEtherDevice, etherUnit, firstOrphan, buffer, size );
      }
      if (!pktTransfered) {
         DEBUGOUT((VERBOSE_HW, "No request for packet. Packet dropped!\n"));
      }
      ReleaseSemaphore((APTR) &etherUnit->eu_ReadOrphanLock);
   }
}


/*
** This function handles CMD_ONLINE commands.
*/
VOID DevCmdOnline(STDETHERARGS)
{
   struct DateStamp StartTime;

   DEBUGOUT((VERBOSE_DEVICE,"DevCmdOnline()\n"));

   if(!(etherUnit->eu_State & ETHERUF_CONFIG))
   {
      ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
      ios2->ios2_WireError    = S2WERR_NOT_CONFIGURED;
      goto end;
   }

   if (!(etherUnit->eu_State & ETHERUF_ONLINE))
   {
      DEBUGOUT((VERBOSE_DEVICE,"###Set onReceivePkt callback...\n"));

      //Set the callback receive function:
      etherUnit->eu_lowLevelDriver->onPacketReceived = onPktReceived;
      //Set driver online:
      etherUnit->eu_lowLevelDriver->init  (etherUnit->eu_lowLevelDriver);
      etherUnit->eu_lowLevelDriver->online(etherUnit->eu_lowLevelDriver);
      //Get the signal number that is used in the lowLevelDriver...
      etherUnit->eu_lowLevelDriverSignalNumber
         = etherUnit->eu_lowLevelDriver->getUsedSignalNumber(etherUnit->eu_lowLevelDriver);

      //TODO: Wait until NIC is really online! You can't wait here!
      Delay(2*50);

      etherUnit->eu_State |= ETHERUF_ONLINE;

      DoEvent(S2EVENT_ONLINE, etherUnit, globEtherDevice);

      DateStamp(&StartTime);
      GlobStat.LastStart.tv_secs = StartTime.ds_Days * 24 * 60 * 60 + StartTime.ds_Minute * 60;
      GlobStat.LastStart.tv_micro = StartTime.ds_Tick;

      //TODO: set Promisc mode or not
   }

   end:

   TermIO(ios2,globEtherDevice);
   DEBUGOUT((VERBOSE_DEVICE,"DevCmdOnline() ends.\n"));
}


void DevCmdOffline(STDETHERARGS)
{
      DEBUGOUT((VERBOSE_DEVICE,"\nDevCmdOffline()\n"));

      if (etherUnit->eu_State & ETHERUF_ONLINE)
      {
         etherUnit->eu_State &= (~ETHERUF_ONLINE);

         DoEvent(S2EVENT_OFFLINE, etherUnit, globEtherDevice);

         // Alle evtl. ReadRequests abbrechen....
         DevCmdFlush(NULL, etherUnit, globEtherDevice);

         NetInterface * lowLevelDriver = (NetInterface*)initModule();
         lowLevelDriver->offline(lowLevelDriver);
      }

      // going offline without an iorequest is possible!
      if (ios2)
         TermIO(ios2, globEtherDevice);

      DEBUGOUT((VERBOSE_DEVICE,"DevCmdOffline() end\n"));
   }
///


VOID DevCmdReadPacket(STDETHERARGS)
{
   struct BufferManagement *bm;

   DEBUGOUT((VERBOSE_DEVICE,"\n*DevCmdReadPacket(type=%ld,ioreq=0x%lx, %s, datlen=%ld)\n",
         (ULONG)ios2->ios2_PacketType,
         ios2,
         ios2->ios2_Req.io_Flags & SANA2IOB_RAW ? "RAW" : "COOKED",
         ios2->ios2_DataLength));

   if (etherUnit->eu_State & ETHERUF_ONLINE)
   {
      /* Find the appropriate queue for this IO request */
      ObtainSemaphore(&etherUnit->eu_BuffMgmtLock);
      bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head;
      while (bm->bm_Node.mln_Succ) {
         if (bm == (struct BufferManagement *) ios2->ios2_BufferManagement)
         {
            ObtainSemaphore((APTR) &bm->bm_RxQueueLock);

            //Ensure that the request is marked as "pending" because the request can only be queued here...
            ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;

            AddTail((struct List *) &bm->bm_RxQueue, (struct Node *) ios2);
            ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);
            break;
         }
         bm = (struct BufferManagement *) bm->bm_Node.mln_Succ;
      }
      ReleaseSemaphore(&etherUnit->eu_BuffMgmtLock);

      //Check if something went wrong, fire error
      if (!bm->bm_Node.mln_Succ)
      {
         ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
         ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
         TermIO(ios2, globEtherDevice);
      }
   }
   else
   {
      /* Sorry, we're offline */
      ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
      ios2->ios2_WireError = S2WERR_UNIT_OFFLINE;
      TermIO(ios2, globEtherDevice);
   }
}


///VOID DevCmdReadOrphan(STDETHERARGS)
VOID DevCmdReadOrphan(STDETHERARGS)
{
    DEBUGOUT((VERBOSE_DEVICE,"\n*ReadOrphan(ioreq=0x%lx)\n", ios2));
    if(etherUnit->eu_State & ETHERUF_ONLINE)
    {
        DEBUGOUT((VERBOSE_DEVICE,"  Queuing request...\n"));

        //Ensure request is marked as "Pending" because it can only be queued...
        ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;

        ObtainSemaphore( (APTR)&etherUnit->eu_ReadOrphanLock);
        AddTail((struct List *)&etherUnit->eu_ReadOrphan, (struct Node *)ios2);
        ReleaseSemaphore((APTR)&etherUnit->eu_ReadOrphanLock);
    }
    else
    {
        /* Sorry, we're offline */
        DEBUGOUT((VERBOSE_DEVICE,"  Failed readorphan. not online!...\n"));
        ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        ios2->ios2_WireError    = S2WERR_UNIT_OFFLINE;
        TermIO(ios2,globEtherDevice);
    }
}
///

/*
** This function handles S2_DEVICEQUERY comands.
*/
VOID DevCmdDeviceQuery(STDETHERARGS)
{
   DEBUGOUT((VERBOSE_DEVICE,"\n*DeviceQuery(ios2=0x%lx, ios2_StatData=0x%lx)\n", ios2, ios2->ios2_StatData));

   struct Sana2DeviceQuery *deviceStatisticData = (struct Sana2DeviceQuery *) ios2->ios2_StatData;
   if (deviceStatisticData)
   {
      DEBUGOUT((VERBOSE_DEVICE,"  SizeAvailable=%ld\n", deviceStatisticData->SizeAvailable));

      //We copy all or nothing of the device information.
      if (deviceStatisticData->SizeAvailable >= sizeof(struct Sana2DeviceQuery)) {
         deviceStatisticData->AddrFieldSize = 6 * 8;              //Address: 6*8 Bits for its Ethernet Address
         deviceStatisticData->MTU = etherUnit->eu_MTU;            //MTU:     1500 Bytes
         deviceStatisticData->BPS = 10000000;                     //Speed:   10 MBit
         deviceStatisticData->HardwareType = S2WireType_Ethernet; //Type:    Ethernet

         deviceStatisticData->SizeSupplied = //
               4*4 +    /* Standard Information: 4 * DWORD */
               3*4+1*2; /* Common Information:   3 * DWORD + 1 WORD */
      } else {
         ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
         ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
      }
   }
   else
   {
      ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
      ios2->ios2_WireError = S2WERR_NULL_POINTER;
   }

   DEBUGOUT((VERBOSE_DEVICE,"  SizeSupplied=%ld\n", deviceStatisticData->SizeSupplied));

   DEBUGOUT((VERBOSE_DEVICE,"DeviceQuery finished. Error=0x%ld\n", ios2->ios2_Req.io_Error));

   TermIO(ios2, etherDevice);
}

/*
** This function handles S2_GETSTATIONADDRESS commands.
** This is usually the first command that is called by an stack.
**
*/
VOID DevCmdGetStationAddress(STDETHERARGS)
{
    DEBUGOUT((VERBOSE_DEVICE,"\n*DevCmdGetStationAddress\n"));

    NetInterface * lowLevelDriver = (NetInterface*)initModule();
    lowLevelDriver->getDefaultNetworkAddress(lowLevelDriver, (MacAddr*)(etherUnit->eu_StAddr));
    PRINT_ETHERNET_ADDRESS(etherUnit->eu_StAddr);

    copyEthernetAddress(etherUnit->eu_StAddr,ios2->ios2_SrcAddr);
    copyEthernetAddress(etherUnit->eu_StAddr,ios2->ios2_DstAddr);

    TermIO(ios2,globEtherDevice);
}

/**
 * This function handles S2_CONFIGINTERFACE commands.
 * Device unit is configured (only once) AND set online!
 */
VOID DevCmdConfigInterface(STDETHERARGS)
{
   DEBUGOUT((VERBOSE_DEVICE,"*ConfigInterface(addr = "));
   PRINT_ETHERNET_ADDRESS(ios2->ios2_SrcAddr);
   DEBUGOUT((VERBOSE_DEVICE,")\n"));

    /* Note: we may only be configured once. */
    if(!(etherUnit->eu_State & ETHERUF_CONFIG))
    {
       /* Now sets our address to whatever the protocol stack says to use. */
       copyEthernetAddress(ios2->ios2_SrcAddr,etherUnit->eu_StAddr);
       etherUnit->eu_State |= ETHERUF_CONFIG;
   
       //Set device also online
       DevCmdOnline(ios2,etherUnit,globEtherDevice);
    }
    else
    {
        DEBUGOUT((VERBOSE_DEVICE,"Error: Device is already configured!"));

        /* Sorry, we're already configured. */
        ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
        ios2->ios2_WireError = S2WERR_IS_CONFIGURED;
        TermIO(ios2,globEtherDevice);
    }
}


///VOID DevCmdTrackType(STDETHERARGS)
/*
** This function adds a packet type to the list
** of those that are being tracked.
*/
VOID DevCmdTrackType(STDETHERARGS)
{
    struct SuperS2PTStats *stats;

    DEBUGOUT((VERBOSE_DEVICE,"*TrackType\n"));
    ObtainSemaphore((APTR)&etherUnit->eu_TrackLock);

    stats = (struct SuperS2PTStats *)etherUnit->eu_Track.mlh_Head;
    while(stats->ss_Node.mln_Succ)
    {
        if(ios2->ios2_PacketType == stats->ss_PType)
        {
            ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
            ios2->ios2_WireError = S2WERR_ALREADY_TRACKED;
            break;
        }
        stats = (struct SuperS2PTStats *)stats->ss_Node.mln_Succ;
    }

    if(!stats->ss_Node.mln_Succ)
    {
        stats = AllocMem(sizeof(struct SuperS2PTStats),MEMF_CLEAR|MEMF_PUBLIC);
        if(stats)
        {
            stats->ss_PType = ios2->ios2_PacketType;
            if(ios2->ios2_PacketType == 2048)
                etherUnit->eu_IPTrack = stats;
            AddTail((struct List *)&etherUnit->eu_Track,(struct Node     *)stats);
            print(5,"Track packet type: "); printi(5,ios2->ios2_PacketType);
        }
    }
    ReleaseSemaphore((APTR)&etherUnit->eu_TrackLock);

    TermIO(ios2,globEtherDevice);
}
///

///VOID DevCmdUnTrackType(STDETHERARGS)
/*
** This function removes a packet type from the
** list of those that are being tracked.
*/
VOID DevCmdUnTrackType(STDETHERARGS)
{
    struct SuperS2PTStats * stats;
    struct SuperS2PTStats * stats_next;
    
    DEBUGOUT((VERBOSE_DEVICE,"*DevCmdUnTrackType\n"));
    ObtainSemaphore((APTR)&etherUnit->eu_TrackLock);

    stats = (struct SuperS2PTStats *)etherUnit->eu_Track.mlh_Head;
    while(stats->ss_Node.mln_Succ)
    {
        stats_next = ((struct SuperS2PTStats *)stats->ss_Node.mln_Succ);
        if(ios2->ios2_PacketType == stats->ss_PType)
        {
            if(ios2->ios2_PacketType == 2048)
                etherUnit->eu_IPTrack = NULL;
            Remove((struct Node *)stats);
            FreeMem(stats,sizeof(struct SuperS2PTStats));
            stats = NULL;
            break;
        }
        stats = stats_next;
    }
    if(stats)
    {
        ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
        ios2->ios2_WireError = S2WERR_NOT_TRACKED;
    }
    ReleaseSemaphore((APTR)&etherUnit->eu_TrackLock);

    TermIO(ios2,globEtherDevice);
}

///

///void DevCmdGlobStats(STDETHERARGS)
void DevCmdGlobStats(STDETHERARGS)
{                                                                 
   struct Sana2DeviceStats * Stat = ios2->ios2_StatData;

   DEBUGOUT((VERBOSE_DEVICE,"*DevCmdGlobStats\n"));

   // gibts den Zeiger auch wirklich ?
   if (Stat)
   {
      Stat->PacketsReceived      = GlobStat.PacketsReceived;
      Stat->PacketsSent          = GlobStat.PacketsSent;
      Stat->BadData              = 0;
      Stat->Overruns             = 0;
      Stat->UnknownTypesReceived = GlobStat.UnknownTypesReceived;
      Stat->Reconfigurations     = 0;
      
      Stat->LastStart.tv_secs    = GlobStat.LastStart.tv_secs;  
      Stat->LastStart.tv_micro   = GlobStat.LastStart.tv_micro;  
   }
   else
   {
      ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
      ios2->ios2_WireError = S2WERR_NULL_POINTER;
   }   

   TermIO(ios2,globEtherDevice);
}
///


/*
** This routine handles S2_ONEVNET commands.
*/
VOID DevCmdOnEvent(STDETHERARGS)
{
   DEBUGOUT((VERBOSE_DEVICE,"*DevCmdOnEvent()"));

    /* Two special cases. S2EVENT_ONLINE and S2EVENT_OFFLINE are supposed to
       return immediately if we are already in the state that they are waiting
       for. */
    if( ((ios2->ios2_WireError & S2EVENT_ONLINE)  && (etherUnit->eu_State & ETHERUF_ONLINE)) ||
        ((ios2->ios2_WireError & S2EVENT_OFFLINE) && (!(etherUnit->eu_State & ETHERUF_ONLINE))))
    {
      ios2->ios2_Req.io_Error = 0;
      ios2->ios2_WireError &= (S2EVENT_ONLINE | S2EVENT_OFFLINE);
      TermIO(ios2, globEtherDevice);
      return;
   }

    /* Queue anything else */
    Forbid();
    AddTail((struct List *)&etherUnit->eu_Events,(struct Node *)ios2);
    Permit();
}

void getmcaf(struct DeviceDriver *EtherDevice, ULONG * af)      
{
   register UBYTE *cp, c;
   register ULONG crc;
   register int i, len;
   struct MCAF_Address * ActNode;

   // * Set up multicast address filter by passing all multicast addresses
   // * through a crc generator, and then using the high order 6 bits as an
   // * index into the 64 bit logical address filter.  The high order bit
   // * selects the word, while the rest of the bits select the bit within
   // * the word.


   if (EtherDevice->ed_Device.lib_Flags & ETHERUF_PROMISC) {      
      af[0] = af[1] = 0xffffffff;
      return;
   }

   ObtainSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
   af[0] = af[1] = 0;
   ActNode = GET_FIRST(EtherDevice->ed_MCAF);   
   while (IS_VALID(ActNode)) {

      cp = ActNode->MCAF_Adr;
      crc = 0xffffffff;
      for (len = sizeof(ActNode->MCAF_Adr); --len >= 0;) {
         c = *cp++;
         for (i = 8; --i >= 0;) {
            if (((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01)) {
               crc <<= 1;
               crc ^= 0x04c11db6 | 1;
            } else
               crc <<= 1;
            c >>= 1;
         }
      }
      // Just want the 6 most significant bits.
      crc >>= 26;

      // Turn on the corresponding bit in the filter.
      af[crc >> 5] |= 1 << ((crc & 0x1f) ^ 24);

      ActNode = GET_NEXT(ActNode);
   }  
   ReleaseSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
}
///

/**
 * Device command. Adds a new mutlicast address to the device driver...
 * @param STDETHERARGS
 * @param STDETHERARGS
 * @param STDETHERARGS
 */
VOID DevCmdAddMulti(STDETHERARGS)
{
   struct MCAF_Address * NewMCAF;
   struct MCAF_Address * ActNode;

   print(VERBOSE_DEVICE,"*AddMulticast\n");

   ObtainSemaphore((APTR)&globEtherDevice->ed_MCAF_Lock);

   //Check if address already exists
   ActNode = GET_FIRST(globEtherDevice->ed_MCAF);
   while(IS_VALID(ActNode))
   { 
      if (0 == memcmp( ActNode->MCAF_Adr, ios2->ios2_SrcAddr, 6))
      {
         //Address already exists. End.
         TermIO(ios2,globEtherDevice);
         goto end;
      }
      ActNode = GET_NEXT(ActNode);
   }

   //Create a new entry and add it to the list of used mulicast addresses
   NewMCAF = (APTR)AllocMem(sizeof(struct MCAF_Address), MEMF_ANY);
   if (NewMCAF)
   { 
      copyEthernetAddress( ios2->ios2_SrcAddr, NewMCAF->MCAF_Adr);
   
      AddHead((APTR)&globEtherDevice->ed_MCAF,(APTR)NewMCAF);
      print(VERBOSE_DEVICE," new multicast address :\n");
      PRINT_ETHERNET_ADDRESS(NewMCAF->MCAF_Adr);
      TermIO(ios2,globEtherDevice);
      
      //TODO: hardware access
   }
   else      
   {
      ios2->ios2_Req.io_Error = S2ERR_SOFTWARE;
      ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
      TermIO(ios2,globEtherDevice);
   }

   end:
   ReleaseSemaphore((APTR)&globEtherDevice->ed_MCAF_Lock);
}

///VOID DevCmdRemMulti(STDETHERARGS)
VOID DevCmdRemMulti(STDETHERARGS)
{
   bool found = FALSE;
   struct MCAF_Address * ActNode;
   int i;

   ObtainSemaphore((APTR)&globEtherDevice->ed_MCAF_Lock);

   ActNode = (struct MCAF_Address * )GET_FIRST(globEtherDevice->ed_MCAF);
   print(VERBOSE_DEVICE,"*DelMulticast\n");
   while(IS_VALID(ActNode))
   {
      print(VERBOSE_DEVICE," gefundene Multicastadresse :\n");
      for(i=0;i<6;i++)
      {
         printi(5,ActNode->MCAF_Adr[i]);
      }

      // ist das unsere Adresse ?
      if (memcmp( ActNode->MCAF_Adr, ios2->ios2_SrcAddr, 6)==0)
      {
         //gefunden!
         Remove((APTR)ActNode);
         FreeMem(ActNode, sizeof(struct MCAF_Address));
         print(VERBOSE_DEVICE," MulticastAdresse wieder entfernt!\n");
         TermIO(ios2,globEtherDevice);               
         found = TRUE;
         break;
      }
      
      ActNode = GET_NEXT(ActNode);
   }
   
   if (!found)
   {
        print(VERBOSE_DEVICE," MulticastAdresse wurde NICHT gefunden !!!\n");
        ios2->ios2_Req.io_Error = S2ERR_BAD_ADDRESS;
        ios2->ios2_WireError = S2WERR_BAD_MULTICAST;
        TermIO(ios2,globEtherDevice);
   }
   else
   {
      //TODO: Deactivate multicast address
   }

   ReleaseSemaphore((APTR)&globEtherDevice->ed_MCAF_Lock);
}
///

/**
 * Adds a single statistic.
 *
 * @param dst
 * @param type
 * @param string
 * @param count
 */
static BOOL DevAddSpecialStat(struct Sana2SpecialStatHeader * statistics, int type, char * string, int count)
{
   if (statistics->RecordCountSupplied < statistics->RecordCountMax )
   {
      //The record begin directly behind the "Sana2SpecialStatHeader".
      //I had to change the header file again. I'm not sure what are the latest version.
      //In newer sana2.h the entry "Sana2SpecialStatRecord" is not in Sana2SpecialStatHeader anymore.

      struct Sana2SpecialStatRecord * nextRecord = (struct Sana2SpecialStatRecord *) ((ULONG) statistics
            + sizeof(struct Sana2SpecialStatHeader)
            + sizeof(struct Sana2SpecialStatRecord) * statistics->RecordCountSupplied);

      nextRecord->Count  = count;
      nextRecord->Type   = type;
      nextRecord->String = string;

      statistics->RecordCountSupplied++;
      return true;
   }
   return false;
}

void DevCmdGetSpecialStats(STDETHERARGS)
{
   struct Sana2SpecialStatHeader * Stat = ios2->ios2_StatData;
   int statType = 1;

   DEBUGOUT((VERBOSE_DEVICE, "*DevCmdGetSpecialStats(max=%ld)\n", Stat ? Stat->RecordCountMax : -1));

   if (Stat) {
      //Beginning with zero statistics. All entries are added one by one...
      Stat->RecordCountSupplied = 0;

      DevAddSpecialStat(Stat, statType++, "Test Statistic", 0);
      //TODO...
   }
   else
   {
      ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
      ios2->ios2_WireError    = S2WERR_NULL_POINTER;
   }

   DEBUGOUT((VERBOSE_DEVICE, "*DevCmdGetSpecialStats...end\n"));
   TermIO(ios2,globEtherDevice);
}

/*
 * New Style Device Kommando
 * Device Query
 *
 * unterstuetzte Device Kommandos
 */
static UWORD auwSupportCmds[]={CMD_READ,
                               CMD_WRITE,
                               CMD_FLUSH,
                               S2_DEVICEQUERY,
                               S2_GETSTATIONADDRESS,
                               S2_CONFIGINTERFACE,
                               //S2_ADDMULTICASTADDRESS,
                               //S2_DELMULTICASTADDRESS,
                               S2_MULTICAST,
                               S2_BROADCAST,
                               S2_TRACKTYPE,
                               S2_UNTRACKTYPE,
                               //S2_GETTYPESTATS,
                               S2_GETGLOBALSTATS,
                               S2_ONEVENT,
                               S2_READORPHAN,
                               S2_ONLINE,
                               S2_OFFLINE,
                               NSCMD_DEVICEQUERY,
                               0};

VOID DevCmdNSDeviceQuery(STDETHERARGS)
{
    struct IOStdReq * io = (struct IOStdReq *)ios2;
    ULONG ulSize;
    struct NSDeviceQueryResult *nsdqr;

    nsdqr  = (struct NSDeviceQueryResult *)io->io_Data;
    ulSize = (ULONG)io->io_Length;

    print(2,"*NewStyleDeviceQuery\n");

    // Datenzeiger ok ? UND
    // genug Platz fuer die NewDeviceStyleQuery - Struktur ?
    if ((ULONG)nsdqr &&
        (ulSize >= sizeof(struct NSDeviceQueryResult)))
    {
       nsdqr->DevQueryFormat    = 0; //Typ 0 (immer NULL)
       nsdqr->SizeAvailable     = sizeof(struct NSDeviceQueryResult);
       nsdqr->DeviceType        = NSDEVTYPE_SANA2;
       nsdqr->DeviceSubType     = 0;
       nsdqr->SupportedCommands = auwSupportCmds;
    }

    io->io_Actual = nsdqr->SizeAvailable;
    TermIO(ios2,globEtherDevice);
}

//Temp buffer to send packets which are of type "cooked" only...
static uint8_t sendBuffer[2000];


/**
 * Writing Packets. Check if packets should be send out.
 * Returns TRUE if something was send. False if not...
 *
 * @param etherUnit
 * @param etherDevice
 * @return ...
 */
static void sendAllPackets(struct DeviceDriverUnit *etherUnit, struct DeviceDriver *etherDevice) {

   struct IOSana2Req *ios2;
   struct BufferManagement *bm;

   DEBUGOUT((VERBOSE_HW, "serviceWritePackets():\n"));

   // Send all packets which are in queue...
   while ((ios2 = (struct IOSana2Req *)GetMsg((APTR)etherUnit->eu_Tx)) != NULL) {

      bm = (struct BufferManagement *) ios2->ios2_BufferManagement;

      //Check if the stack tells us it's packet buffer (SANA2 V3 extension)
      APTR dmaPacketBuffer = (bm->bm_CopyFromBufferDMA != 0l) ? CopyFromBufferDMA(ios2->ios2_Data) : NULL;

      //Raw or Cooked packet?
      if (ios2->ios2_Req.io_Flags & SANA2IOF_RAW) {
         //
         // RAW:
         //
         TRACE_INFO("  Send raw packet:\n");

         if (ios2->ios2_DataLength > ETHER_PACKET_HEAD_SIZE) {
            if (CopyFromBuffer((APTR)sendBuffer,ios2->ios2_Data, ios2->ios2_DataLength)) {
               dumpMem(sendBuffer, ios2->ios2_DataLength);
               etherUnit->eu_lowLevelDriver->sendPacket(etherUnit->eu_lowLevelDriver, sendBuffer, ios2->ios2_DataLength);
            } else {
               setErrorOnRequest(ios2, S2ERR_NO_RESOURCES, S2WERR_BUFF_ERROR);
            }
         } else {
            setErrorOnRequest(ios2, S2ERR_MTU_EXCEEDED, S2WERR_BUFF_ERROR);
         }

      } else {
         //
         // COOKED:
         //
         if (ios2->ios2_DataLength <= ETHERNET_MTU) {

            DEBUGOUT((VERBOSE_HW, "  Send cooked packet:\n"));
            DEBUGOUT((VERBOSE_HW, "    NetIORequest    : 0x%lx\n", ios2));
            if (ios2->ios2_DataLength < 46) {
               DEBUGOUT((VERBOSE_HW, "      Pkt < 46 bytes payload (%ld bytes)\n", ios2->ios2_DataLength));
            }
            DEBUGOUT((VERBOSE_HW, "      DstAddr         : ")); printEthernetAddress(ios2->ios2_DstAddr);   DEBUGOUT((VERBOSE_HW,"\n"));
            DEBUGOUT((VERBOSE_HW, "      SrcAddr         : ")); printEthernetAddress(etherUnit->eu_StAddr); DEBUGOUT((VERBOSE_HW,"\n"));
            DEBUGOUT((VERBOSE_HW, "      PacketType      : 0x%lx\n", (ULONG)ios2->ios2_PacketType));
            DEBUGOUT((VERBOSE_HW, "      DataLength      : 0x%lx\n", (ULONG)ios2->ios2_DataLength));
            DEBUGOUT((VERBOSE_HW, "      DMABufferPtr    : 0x%lx\n", (ULONG)dmaPacketBuffer));

            //Test: Stop using DMA!!!!
            //dmaPacketBuffer = NULL;

            if (dmaPacketBuffer) {
               //Yes, use SANA2 V3 extension: Direct copy without any second buffer...
               dumpMem(dmaPacketBuffer, ios2->ios2_DataLength);
               etherUnit->eu_lowLevelDriver->sendPacketCooked(etherUnit->eu_lowLevelDriver,
                     (MacAddr*)ios2->ios2_DstAddr,
                     (MacAddr*)etherUnit->eu_StAddr,
                     (uint16_t)ios2->ios2_PacketType,
                     dmaPacketBuffer,
                     ios2->ios2_DataLength);

            } else {
               //No, go ahead with old V2 copy functions (let stack copy the data)

               //Build a raw ethernet packet (with a temp buffer): SRC + DST + Type:
               copyEthernetAddress(ios2->ios2_DstAddr,   sendBuffer+0);
               copyEthernetAddress(etherUnit->eu_StAddr, sendBuffer+6);
               *(uint16_t*)(sendBuffer+12) = ios2->ios2_PacketType;

               //copy packet data from request at offset ETHER_PACKET_HEAD_SIZE as the ethernet content:
               if (CopyFromBuffer((APTR)(sendBuffer + ETHER_PACKET_HEAD_SIZE), ios2->ios2_Data, ios2->ios2_DataLength)) {

                  /*
                  //Send packet (old send method working):
                  //respect minimal size of 46 bytes Ethernet Frame
                  int len = (ios2->ios2_DataLength < 46) ? 46 : ios2->ios2_DataLength;
                  len += ETHER_PACKET_HEAD_SIZE;
                  dumpMem(sendBuffer, len);
                  etherUnit->eu_lowLevelDriver->sendPacket(etherUnit->eu_lowLevelDriver, sendBuffer, len);
                  */

                  //New method:
                  dumpMem(sendBuffer, ios2->ios2_DataLength + ETHER_PACKET_HEAD_SIZE);
                  etherUnit->eu_lowLevelDriver->sendPacketCooked(etherUnit->eu_lowLevelDriver,
                        (MacAddr*)(sendBuffer+0),
                        (MacAddr*)(sendBuffer+6),
                        *(uint16_t*)(sendBuffer+12),
                        (sendBuffer+ETHER_PACKET_HEAD_SIZE),
                        ios2->ios2_DataLength);

               } else {
                  setErrorOnRequest(ios2, S2ERR_NO_RESOURCES, S2WERR_BUFF_ERROR);
                  DEBUGOUT((VERBOSE_HW, "   CopyFromBuffer error\n"));
               }
            }
         } else {
            setErrorOnRequest(ios2, S2ERR_MTU_EXCEEDED, S2WERR_BUFF_ERROR);
            DEBUGOUT((VERBOSE_HW, "   Pkt exceeds the MTU!\n"));
         }
      }
      //end:
      if (ios2->ios2_WireError != 0) {
         DoEvent(S2EVENT_BUFF, etherUnit, etherDevice);
      }
      TermIO(ios2, etherDevice);
   }
}

/**
 * To calculate the remaining stacksize is not as easy as it seems. See Amiga Guru Book.
 * So we check the total stack only, not the real used stack space...
 */
BOOL checkStackSpace(int minStackSize)
{
   struct Task * thisTask = FindTask(0l);
   if (thisTask) {
      int total = thisTask->tc_SPUpper - thisTask->tc_SPLower;
      if (total < minStackSize)
      {
         return FALSE;
      }
   }
   return TRUE;
}

static void dumpMem(uint8_t * mem, int16_t len) {
#if DEBUG > 0
    int i;
    for (i = 0; i < len; i++) {
       if (((i % 16) == 0)) {
          if (i != 0) {
             TRACE_INFO("\n");
          }
          TRACE_INFO("%03lx: ", (ULONG)i);
       }
       TRACE_INFO("%02lx ", *mem);
       mem++;
    }
    TRACE_INFO("\n");
#endif
 }



void printEthernetAddress(unsigned char * addr)
{
#if DEBUG > 0
   DEBUGOUT((VERBOSE_HW, "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
         (ULONG)addr[0],  (ULONG)addr[1],  (ULONG)addr[2],  (ULONG)addr[3],  (ULONG)addr[4],  (ULONG)addr[5] ));
#endif
}


BOOL isBroadcastEthernetAddress(UBYTE * addr)
{
   return 0 == memcmp(addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
}

/**
 * Copy Ethernet Address (6 bytes) as fast as possible!
 * This is the fastest version in C I know.
 *
 * @param src
 * @param dst
 */
void copyEthernetAddress(const BYTE * from, BYTE * to)
{
   ULONG * lsrc = (ULONG*) from;
   ULONG * ldst = (ULONG*) to;

   //Will result into 3 Assembler lines. But could be made by hand in 2 lines!

   *ldst++ = *lsrc++;                  //first 4 bytes of the address
   *(USHORT*) ldst = *(USHORT*) lsrc;  //last  2 bytes of the address
}

void setErrorOnRequest(struct IOSana2Req *ios2, BYTE io_Error, ULONG ios2_WireError)
{
   print(3, "Something went wrong!!\n");
   ios2->ios2_WireError = ios2_WireError;
   ios2->ios2_Req.io_Error = io_Error;
}
