/* 
 * Copyright (C) 1997 - 1999, 2018 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

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

#include <string.h>

#include "conffile.h"
#include "devdebug.h"
#include "jan_int.h"
#include "tools.h"

#include "version.h"  

// ############## LOCAL FUNCTION ##################

static ULONG AbortReqAndRemove(struct MinList *, struct IOSana2Req *,struct EtherbridgeDevice *,struct SignalSemaphore * lock);
static void  AbortReqList(struct MinList *minlist,struct EtherbridgeDevice * EtherDevice);
static BOOL  ReadConfig(struct EtherbridgeDevice *);



//############ Konstanten ######################################################
///Konstanten

// State bits for edu_State
#define ETHERUB_CONFIG     0
#define ETHERUB_ONLINE     1
#define ETHERUB_NOJANUS    3
#define ETHERUB_EXCLUSIVE  4
#define ETHERUB_LOOPBACK   5
#define ETHERUB_PROMISC    6
#define ETHERUB_NOBRIDGEBOARD 7
#define ETHERUF_CONFIG        (1<<ETHERUB_CONFIG)
#define ETHERUF_ONLINE        (1<<ETHERUB_ONLINE)
#define ETHERUF_NOJANUS       (1<<ETHERUB_NOJANUS)
#define ETHERUF_EXCLUSIVE     (1<<ETHERUB_EXCLUSIVE)
#define ETHERUF_LOOPBACK      (1<<ETHERUB_LOOPBACK)
#define ETHERUF_PROMISC       (1<<ETHERUB_PROMISC)
#define ETHERUF_NOBRIDGEBOARD (1<<ETHERUB_NOBRIDGEBOARD)



#define IS_WHITESPACE(x) (((x) == ' ') || ((x) == '='))
#define IS_STRINGTERM(x) (((x) == 0) || ((x) == 0xa) || ((x) == '\n'))



/******************************************************************************/
/*                                                                            */
/* exports (from Libnix)                                                      */
/*                                                                            */
/******************************************************************************/

//This is for defining the API version to the PC side.
const UWORD DevVersion  = (UWORD)DEVICE_VERSION;
const UWORD DevRevision = (UWORD)DEVICE_REVISION;

const char DevName[]     = DEVICE_NAME;
const char DevIdString[] = VERSION_STRING;

const char sConfigFile[] = "env:sana2/etherbridge.config";



//############ Globale Variablen ###############################################

///globale Variablen
struct bridge_ether * b_param = NULL;
struct bridge_ether * w_param = NULL;
struct Sana2DeviceStats GlobStat;
unsigned long _WBenchMsg=0;

struct EtherbridgeDevice * EtherDevice;    //Local pointer to own device device structure
char   sPCCmd[160];                  //Name der PC-Exe (PC-Server-Name)
char   sPCPktDrvCmd[160] = "";       //MSDOS-Kommando zum starten des Packet Treibers
char   sPCPktDrvPar[160] = "";       //FormatString fuer Parameter zum Packet Treiber
int    iComMode=0;                   //Kommunikationsmodus zwischen PC und Amiga
                                     // 0 : fuer Janus Signale
                                     // 1 : fuer VBeam-Handler

struct ExecBase * SysBase  = NULL;
///

//############ Externe Variablen und Funktionen ################################

extern int sigBitCntr;


//TODO: Check if "__saveds" is correct here: Loads A4 near register
__saveds ULONG FakePFHookEntry()
{
   return 1;
}


/**
 * Device Init function.
 *
 * @param DeviceSegList
 * @param DevBasePointer
 * @param exec_base
 * @return
 */
struct Library * DevInit(BPTR DeviceSegList, struct Library * DevBasePointer, struct Library * execBase) {

   CHECK_LIBRARY_IS_OPEN(DOSBase);
   CHECK_LIBRARY_IS_OPEN(IntuitionBase);

   DEBUGOUT((1, "DevInit(DeviceSegList=0x%lx)\n", DeviceSegList));

   EtherDevice = (struct EtherbridgeDevice *)DevBasePointer;
   
   //init device structure  
   EtherDevice->ed_Device.lib_Node.ln_Type = NT_DEVICE;
   EtherDevice->ed_Device.lib_Node.ln_Name = (UBYTE *)DevName;
   EtherDevice->ed_Device.lib_Flags        = LIBF_CHANGED | LIBF_SUMUSED;
   EtherDevice->ed_Device.lib_Version      = (UWORD)DevVersion;
   EtherDevice->ed_Device.lib_Revision     = (UWORD)DevRevision;
   EtherDevice->ed_Device.lib_IdString     = (char *)DevIdString;
   
   EtherDevice->ed_SegList = DeviceSegList;

   //Show user messages until config overrides it
   EtherDevice->ed_showMessages = true;

   //Create a dummy hook which do nothing...
   bzero(&EtherDevice->ed_DummyPFHook, sizeof(EtherDevice->ed_DummyPFHook));
   EtherDevice->ed_DummyPFHook.h_Entry = &FakePFHookEntry;  // Pointer to a simple "RTS" function...
   
   // Device Semaphores
   InitSemaphore((APTR)&EtherDevice->ed_DeviceLock);
   InitSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
   
   return (struct Library *)EtherDevice;
}


/**
 * Device open function.
 *
 * @param ios2
 * @param s2unit
 * @param s2flags
 * @param devPointer
 */
void DevOpen(struct IOSana2Req *ios2,
              ULONG s2unit, 
              ULONG s2flags,
              struct Library * devPointer)
{  
   struct EtherbridgeUnit *etherUnit;
   struct BufferManagement *bm;
   ULONG returncode=0;
   BOOL success = FALSE; 
   
   DEBUGOUT((VERBOSE_DEVICE, "DevOpen(ioreq=0x%lx,unit=%d,flags=0x%lx)...\n", ios2, s2unit,s2flags));

   //Only when device is opened check stack is at leased 5000 bytes big.
   //If not Etherbridge would crash for some reason somewhere because of that.
   //Brings up waring. Exit with failure.
  /*if (!checkStackSpace(5000)) {
      __request("Stack size is too small! Please increase the size to at least 5kb...");
      ios2->ios2_Req.io_Error = IOERR_OPENFAIL;
      ios2->ios2_Req.io_Unit = (struct Unit *) -1;
      ios2->ios2_Req.io_Device = (struct Device *) -1;
      return;
   }*/

   CHECK_LIBRARY_IS_OPEN(UtilityBase);

   ObtainSemaphore((APTR)&EtherDevice->ed_DeviceLock);

   EtherDevice->ed_Device.lib_OpenCnt++;

   //Detect used Bridgeboards...
   //EtherDevice->ed_BBType = detectBridgeboards();

   // Bridgeboard, Janus-Software da ? Bisheriger Zugriff nich exklusiv ?
   if(!(EtherDevice->ed_Device.lib_Flags & ETHERUF_NOBRIDGEBOARD) &&
      !(EtherDevice->ed_Device.lib_Flags & ETHERUF_NOJANUS) &&
      !(EtherDevice->ed_Device.lib_Flags & ETHERUF_EXCLUSIVE ) )
   {
      //jetzt exklusiven Zugriff ?
      if (s2flags & SANA2OPF_MINE)
      {
         //schon von jemanden anderen geoeffnet ?
         if (!(EtherDevice->ed_Device.lib_OpenCnt > 1))
            EtherDevice->ed_Device.lib_Flags |= ETHERUF_EXCLUSIVE;
         else
            goto end;
      }

      //open in promisc mode ? (only in conjunction with flag MINE!!!)
      if (s2flags & SANA2OPF_PROM)
      {
         if (s2flags & SANA2OPF_MINE)
            EtherDevice->ed_Device.lib_Flags |= ETHERUF_PROMISC;
         else
            goto end;
      }

      /* Only UNIT 0 is allowed to open */
      if(s2unit < ED_MAXUNITS)
      {
         //Bring up unit device process...
         etherUnit = InitETHERUnit(s2unit,EtherDevice);
         if( etherUnit )
         {
            //PromMode setzen/löschen wenn Device schon online
            if (etherUnit->eu_State              & ETHERUF_ONLINE)
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
               //CHECK_ALREADY_QUEUED((struct List *)&etherUnit->eu_BuffMgmt, (struct Node *)bm);
               AddTail((struct List *)&etherUnit->eu_BuffMgmt,(struct Node *)bm);


               if(ios2->ios2_BufferManagement)
               {
                  print(5,"\n");
                  bufftag = FindTagItem(S2_CopyToBuff, (struct TagItem *)ios2->ios2_BufferManagement);
                  if(bufftag && !bm->bm_CopyToBuffer)
                  {
                     bm->bm_CopyToBuffer = (SANA2_CTB) bufftag->ti_Data;
                     print(5,"  SANA2_CTB-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyToBuff16, (struct TagItem *) ios2->ios2_BufferManagement);
                  if (bufftag) {
                     bm->bm_CopyToBuffer = (SANA2_CTB) bufftag->ti_Data;
                     print(5, "  SANA2_CTB16-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyFromBuff, (struct TagItem *)ios2->ios2_BufferManagement);
                  if(bufftag && !bm->bm_CopyFromBuffer)
                  {
                     bm->bm_CopyFromBuffer = (SANA2_CFB) bufftag->ti_Data;
                     print(5,"  SANA2_CFB-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_CopyFromBuff16, (struct TagItem *) ios2->ios2_BufferManagement);
                  if (bufftag) {
                     bm->bm_CopyFromBuffer = (SANA2_CFB) bufftag->ti_Data;
                     print(5, "  SANA2_CFB16-Tag  found...\n");
                  }

                  bufftag = FindTagItem(S2_PacketFilter, (struct TagItem *)ios2->ios2_BufferManagement);
                  if(bufftag)
                  {
                     bm->bm_PacketFilterHook = (struct Hook *) bufftag->ti_Data;
                     print(5,"  SANA2_PacketFilter-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_DMACopyFromBuff32, (struct TagItem *) ios2->ios2_BufferManagement);
                  if (bufftag) {
                     //TODO:
                     print(5, "  S2_DMACopyFromBuff32-Tag found...\n");
                  }

                  bufftag = FindTagItem(S2_DMACopyToBuff32, (struct TagItem *) ios2->ios2_BufferManagement);
                  if (bufftag) {
                     //TODO:
                     print(5, "  S2_DMACopyToBuff32-Tag found...\n");
                  }

                  //TODO: Add SANA2R3 Hook extensions functions...
                  //print out all tags:
                  DEBUGOUT((5, "All tags: "));
                  struct TagItem *tstate;
                  struct TagItem *tag;
                  tstate = (struct TagItem *) ios2->ios2_BufferManagement;
                  while ((tag = NextTagItem(&tstate)) != NULL)
                  {
                     DEBUGOUT((5, " 0x%lx", (ULONG)tag->ti_Tag));
                  }
                  DEBUGOUT((5, "\n"));

                  /*
                  struct TagItem * item = ((struct TagItem *) ios2->ios2_BufferManagement);
                  struct TagItem ** item2 = &item;
                  do
                  {
                     item = (struct TagItem *) ios2->ios2_BufferManagement;
                     DEBUGOUT((5, " tagnum = 0x%lx", (ULONG)item->ti_Tag));
                     item = NextTagItem(item2);
                  } while(item != NULL);*/

               }

               /*
                * Using dummy buffer management function when no callback are given.
                */
               if (!bm->bm_PacketFilterHook)
                  bm->bm_PacketFilterHook = &EtherDevice->ed_DummyPFHook;
               if (!bm->bm_CopyToBuffer)
                  bm->bm_CopyToBuffer     = (APTR)&EtherDevice->ed_DummyPFHook.h_Entry;
               if (!bm->bm_CopyFromBuffer)
                  bm->bm_CopyFromBuffer   = (APTR)&EtherDevice->ed_DummyPFHook.h_Entry;




               //Success!
               success = TRUE;
               returncode = 0;
               EtherDevice->ed_Device.lib_OpenCnt++;
               EtherDevice->ed_Device.lib_Flags &=~LIBF_DELEXP;
               etherUnit->eu_Unit.unit_OpenCnt++;

               /* Fix up the initial IO request */
               ios2->ios2_BufferManagement = (void *)bm; // <= replaces the original list tags with new BM struct!
               ios2->ios2_Req.io_Error = S2ERR_NO_ERROR;
               ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
               ios2->ios2_Req.io_Unit = (struct Unit *)etherUnit;
               ios2->ios2_Req.io_Device = (struct Device *)EtherDevice;
            }
         }
      }
   }

   end:

   /* irgendetwas fehlgeschlagen ? */
   if (!success) {
      if (EtherDevice->ed_Device.lib_Flags & ETHERUF_NOBRIDGEBOARD) {
         if (EtherDevice->ed_Device.lib_Flags & ETHERUF_NOJANUS) {
            //
         } else {
            //
         }
      }
      ios2->ios2_Req.io_Error = IOERR_OPENFAIL;
      ios2->ios2_Req.io_Unit = (struct Unit *) -1;
      ios2->ios2_Req.io_Device = (struct Device *) -1;
      returncode = IOERR_OPENFAIL;
   }

   EtherDevice->ed_Device.lib_OpenCnt--;  
   ReleaseSemaphore((APTR)&EtherDevice->ed_DeviceLock);

   DEBUGOUT((VERBOSE_DEVICE, "DevOpen()...ends.\n"));
}

/**
 * Device close function.
 *
 * @param ios2
 * @param DevBase
 * @return
 */
BPTR DevClose(struct IOSana2Req *ios2, struct Library * DevBase) {
   struct EtherbridgeUnit *etherUnit;
   BPTR seglist = 0L;

   DEBUGOUT((VERBOSE_DEVICE, "*DevClose(ioreq=0x%lx)\n", ios2));

   //Check IORequest for validity to not close device twice!
   if (ios2->ios2_Req.io_Device != (struct Device *) -1 //
      && ios2->ios2_Req.io_Unit != (struct Unit *) -1) {

      ObtainSemaphore((APTR) &EtherDevice->ed_DeviceLock);

      etherUnit = (struct EtherbridgeUnit *) ios2->ios2_Req.io_Unit;

      //Abort every read request of the caller...
      struct BufferManagement * bm = (struct BufferManagement *) ios2->ios2_BufferManagement;
      if (bm) {
         ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
         AbortReqList(&bm->bm_RxQueue, EtherDevice);
         ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);

         // Find the single buffer management from the caller and free it
         ObtainSemaphore((APTR) &etherUnit->eu_BuffMgmtLock);
         for (bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head; //
               bm->bm_Node.mln_Succ; //
               bm = (struct BufferManagement *) bm->bm_Node.mln_Succ) {
            //BufferManagement gefunden ?
            if (bm == ios2->ios2_BufferManagement) {
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
      EtherDevice->ed_Device.lib_Flags &= ~ETHERUF_EXCLUSIVE;
      EtherDevice->ed_Device.lib_Flags &= ~ETHERUF_LOOPBACK;
      EtherDevice->ed_Device.lib_Flags &= ~ETHERUF_PROMISC;

      /* Trash the io_Device and io_Unit fields so that any attempt to use this
       request will die immediately. */
      ios2->ios2_Req.io_Device = (struct Device *) -1;
      ios2->ios2_Req.io_Unit = (struct Unit *) -1;

      /* I always shut the unit process down if the open count drops to zero.
       That way, if I need to expunge, I never have to Wait(). */
      etherUnit->eu_Unit.unit_OpenCnt--;

      if (etherUnit->eu_Unit.unit_OpenCnt == 0) {
         ExpungeUnit(etherUnit->eu_UnitNum, EtherDevice);
         etherUnit = NULL;
      }

      EtherDevice->ed_Device.lib_OpenCnt--;

      ReleaseSemaphore((APTR) &EtherDevice->ed_DeviceLock);

      // Check to see if we've been asked to expunge.
      // In this case expunge Device now...
      if (EtherDevice->ed_Device.lib_Flags & LIBF_DELEXP) {
         seglist = DevExpunge(DevBase);
         DEBUGOUT((VERBOSE_DEVICE, "DevClose(): Device was expunged (cause of an delayed expunge)!\n"));
         DevBase = NULL;
         EtherDevice = NULL;
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
BPTR DevExpunge(struct Library * DevBase)
{
    BPTR seglist = (BPTR)0L;

    DEBUGOUT((VERBOSE_DEVICE, "\nDevExpunge(devbase=0x%lx)\n", DevBase));

    //Check if device is still used?
    if(EtherDevice->ed_Device.lib_OpenCnt) {
        //Device is still in use! Mark device itself that it should expunge after next DevClose()...
        EtherDevice->ed_Device.lib_Flags |= LIBF_DELEXP;
        DEBUGOUT((VERBOSE_DEVICE, "DevExpunge(): Can't expunge because device is still in use (cntr=%ld)!\n",
              EtherDevice->ed_Device.lib_OpenCnt));
    } else {

       //At this time the unit MUST have been shutdown (on last DevClose()).
       //Because of the special problem that we can't wait here when Expunge is called.
       if (EtherDevice->ed_Units[0] != NULL) {
          Alert(0x1818);
       }

      /* Free up our library base and function table after
         removing ourselves from the library list. */
      Disable();
      Remove((struct Node *)DevBase);
      Enable();

      seglist          = ((struct EtherbridgeDevice*)DevBase)->ed_SegList;
      ULONG devbase    = (ULONG)DevBase;
      LONG devbasesize = (ULONG)DevBase->lib_NegSize;
      devbase          = devbase - devbasesize;
      devbasesize      += (ULONG)DevBase->lib_PosSize;

      FreeMem((APTR)devbase,devbasesize);
      EtherDevice = NULL;
      //The device base is now gone! Do not access them anymore!

      DEBUGOUT((VERBOSE_DEVICE, "DevExpunge() ends. Device is expunged now...\n\n"));
    }

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
ULONG DevAbortIO( struct IOSana2Req *ios2,
                  struct Library * DevPointer)                  
{
   ULONG result = IOERR_NOCMD; //Default: Error
   struct EtherbridgeUnit *etherUnit;

   DEBUGOUT((VERBOSE_DEVICE, "\nDevAbortIO(ioreq.cmd=0x%lx)\n", (ULONG)ios2->ios2_Req.io_Command));
   etherUnit = (struct EtherbridgeUnit *)ios2->ios2_Req.io_Unit;
   //Abort only pending requests....NT_MESSAGE means "pending"...
   if (ios2->ios2_Req.io_Message.mn_Node.ln_Type == NT_MESSAGE) {

      switch (ios2->ios2_Req.io_Command) {

         case CMD_READ: {
            struct BufferManagement *bm = ios2->ios2_BufferManagement;
            result = AbortReqAndRemove(&bm->bm_RxQueue, ios2, EtherDevice, &bm->bm_RxQueueLock);
            break;
         }

         case S2_BROADCAST:
         case CMD_WRITE: {
            Forbid();
            result = AbortReqAndRemove((struct MinList *)&etherUnit->eu_Tx->mp_MsgList,
                  ios2, EtherDevice, NULL);
            Permit();
            break;
         }

         case S2_ONEVENT: {
            Forbid();
            result = AbortReqAndRemove(&etherUnit->eu_Events, ios2, EtherDevice, NULL);
            Permit();
            break;
         }

         case S2_READORPHAN: {
            result = AbortReqAndRemove(&etherUnit->eu_ReadOrphan, ios2, EtherDevice, &etherUnit->eu_ReadOrphanLock);
            break;
         }

         default: {
            result = S2ERR_SOFTWARE;
            DEBUGOUT((VERBOSE_DEVICE,"Unable to break this iorequest: 0x%x", ios2));
            break;
         }
        }
    } else {
       DEBUGOUT((VERBOSE_DEVICE,"DevAbortIO(): Request was not pending! Not removed!\n"));
       result = S2ERR_NO_ERROR; //Silent ok.
    }
    return(result);
}

/**
 * This function is used to locate an IO request in a linked
 * list and abort it if found. Request is removed from List.
 *
 * @param minlist
 * @param ios2
 * @param EtherDevice
 * @param lock the list lock (Semaphore) in which the Request is located or NULL.
 *
 * @return io_error code: if found => IOERR_ABORTED, if NOT found => S2ERR_BAD_ARGUMENT
 */
static
ULONG AbortReqAndRemove(struct MinList *minlist,
               struct IOSana2Req *ios2,
               struct EtherbridgeDevice *EtherDevice,
               struct SignalSemaphore * lock)
{
   struct Node *node, *next;
   ULONG result = S2ERR_BAD_ARGUMENT; //If IORequest was not found...

   DEBUGOUT((VERBOSE_DEVICE, "AbortReq(ioreq.cmd=0x%x)\n", ios2->ios2_Req.io_Command));

   if (lock) ObtainSemaphore(lock);
   {
      node = (struct Node *) minlist->mlh_Head;
      while (node->ln_Succ) {
         next = node->ln_Succ;
         if (node == (struct Node *) ios2) {
            Remove((struct Node *) ios2);
            result = ios2->ios2_Req.io_Error = IOERR_ABORTED;
            TermIO(ios2, EtherDevice);
            break;
         }
         node = next;
      }
   }
   if (lock) ReleaseSemaphore(lock);

   if (result != IOERR_ABORTED) {
      DEBUGOUT((VERBOSE_DEVICE, "AbortReqAndRemove(): IORequest not found! Nothing to abort!"));
   }

   return (result);
}

/**
 * Abort an list of requests...
 *
 * @param minlist
 * @param EtherDevice
 */
static void  AbortReqList(struct MinList *minlist,struct EtherbridgeDevice * EtherDevice)
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
VOID DevCmdFlush(STDETHERARGS) {
   struct BufferManagement *bm;

   DEBUGOUT((VERBOSE_DEVICE, "\nDevCmdFlush(ioreq=0x%lx)\n", (ULONG)ios2));

   //Abort all READ Requests of all opener...
   ObtainSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);
   bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head;
   while (bm->bm_Node.mln_Succ) {
      ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
      AbortReqList(&bm->bm_RxQueue, EtherDevice);
      ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);
      bm = (struct BufferManagement *) bm->bm_Node.mln_Succ;
   }
   ReleaseSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);


   Forbid();
   //Stop all pending Write Requests
   AbortReqList((struct MinList *) &etherUnit->eu_Tx->mp_MsgList, EtherDevice);
   //Stop all pending S2_ONEVENT:
   AbortReqList(&etherUnit->eu_Events, EtherDevice);
   Permit();

   if (NULL != ios2)
      TermIO(ios2, EtherDevice);

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
struct EtherbridgeUnit *InitETHERUnit(ULONG s2unit, struct EtherbridgeDevice *EtherDevice)
{
    struct EtherbridgeUnit *etherUnit;
    struct TagItem NPTags[]={{NP_Entry, (ULONG)&DevProcEntry},
                             {NP_Name , (ULONG)"Etherbridge Unit Process"},
                             {NP_Priority, ETHER_PRI} ,
                             {NP_FreeSeglist , FALSE} ,
                             {NP_StackSize, 8000},
                             {TAG_DONE, 0}};

    /* Check to see if the Unit is already up and running.  If
       it is, just drop through.  If not, try to start it up. */
    if(!EtherDevice->ed_Units[s2unit])
    {
            /* Allocate a new Unit structure */
            etherUnit = AllocMem(sizeof(struct EtherbridgeUnit), MEMF_CLEAR | MEMF_PUBLIC);
            if( etherUnit )
            {
                /* Do some initialization on the Unit structure */
                NewList(&etherUnit->eu_Unit.unit_MsgPort.mp_MsgList);

                etherUnit->eu_Unit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;
                etherUnit->eu_Unit.unit_MsgPort.mp_Flags = PA_IGNORE;
                etherUnit->eu_Unit.unit_MsgPort.mp_Node.ln_Name = "etherbridge.port";

                etherUnit->eu_UnitNum = s2unit;
                etherUnit->eu_Device  = (struct Device *) EtherDevice;

                // Setze MaxiumTransmitUnit auf 1500 Byte (Ethernet)
                etherUnit->eu_MTU = 1500;

                InitSemaphore((APTR)&etherUnit->eu_BuffMgmtLock);
                NewList((struct List *)&etherUnit->eu_BuffMgmt);

                InitSemaphore((void *)&etherUnit->eu_ReadOrphanLock);
                NewList((struct List *)&etherUnit->eu_ReadOrphan);

                //Event list has no lock
                NewList((struct List *)&etherUnit->eu_Events);

                InitSemaphore((void *)&etherUnit->eu_TrackLock);
                NewList((struct List *)&etherUnit->eu_Track);

                /* Try to read in our configuration file (again) */
                if(ReadConfig(EtherDevice))
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
                    FreeMem(etherUnit, sizeof(struct EtherbridgeUnit));
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
static BOOL ReadConfig( struct EtherbridgeDevice *EtherDevice )
{
    int iUsePktDrvNo;

    DEBUGOUT((VERBOSE_DEVICE, "ReadConfig()\n"));

    RegistryInit( sConfigFile );
    {
       char tmpStringBuffer[20] = {0};

       debugLevel =                       ReadKeyInt("DEBUGLEV"       ,0);
       strncpy( sPCCmd,                   ReadKeyStr("PCSERVER"       ,"c:\\ebserver.exe"),80-1 );
       iComMode =                         ReadKeyInt("COMMODE"        ,0);
       iUsePktDrvNo =                     ReadKeyInt("USEPKTDRVNO"    ,0);
       EtherDevice->ed_showMessages   =   ReadKeyInt("SHOWMESSAGES"   ,1);
       EtherDevice->ed_startDosServer =   ReadKeyInt("STARTPCSERVER"  ,1);
       EtherDevice->ed_startPacketDriver =ReadKeyInt("STARTPKTDRV"    ,1);


       if ((iUsePktDrvNo >= 10) || (iUsePktDrvNo < 0)) iUsePktDrvNo = 0;

       //TODO: Check why snprintf throws a compiler warning?
       sprintf(tmpStringBuffer, "PKTDRV%d",iUsePktDrvNo );
       strncpy(sPCPktDrvCmd, ReadKeyStr(tmpStringBuffer,""), sizeof(sPCPktDrvCmd)-1 );
       sprintf(tmpStringBuffer, "PKTDRVPARAM%d",iUsePktDrvNo );
       strncpy(sPCPktDrvPar, ReadKeyStr(tmpStringBuffer,""), sizeof(sPCPktDrvPar)-1 );
    }
    RegistryDestroy();


    // There is a special "Commode" value: "-1"
    // This means chose the right mode for the installed Bridgeboard card automatically:
    // A2088 + A2286 => "1" (Polling)
    // A2386 => 0 (Janus Int Signals)
    if (iComMode == -1) {
       if (EtherDevice->ed_BBType == A2386) {
          iComMode = 0; //Use interrupts: A2386
       } else {
          iComMode = 1; //Use polling: A2286 + 2088
       }
    }

    DEBUGOUT((VERBOSE_DEVICE,"\n\n ##### Etherbridge Device #####\n"));
    DEBUGOUT((VERBOSE_DEVICE,"%s\n", DevIdString));
    DEBUGOUT((VERBOSE_DEVICE,"DebugLev= %ld\n", (LONG)debugLevel));
    DEBUGOUT((VERBOSE_DEVICE,"DrvCmd  = %s\n", sPCPktDrvCmd));
    DEBUGOUT((VERBOSE_DEVICE,"DrvPar  = %s\n", sPCPktDrvPar));
    DEBUGOUT((VERBOSE_DEVICE,"sPCCmd  = %s\n", sPCCmd));
    DEBUGOUT((VERBOSE_DEVICE,"ComMode = %ld\n", (LONG)iComMode));
    DEBUGOUT((VERBOSE_DEVICE,"PktDrvNo= %ld\n", (LONG)iUsePktDrvNo));
    DEBUGOUT((VERBOSE_DEVICE,"verboseonerror= %ld\n", (LONG)EtherDevice->ed_showMessages));
    DEBUGOUT((VERBOSE_DEVICE,"STARTPCSERVER = %d\n", EtherDevice->ed_startDosServer));
    DEBUGOUT((VERBOSE_DEVICE,"STARTPKTDRV = %d\n", EtherDevice->ed_startPacketDriver));

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
VOID ExpungeUnit(UBYTE unitNumber, struct EtherbridgeDevice *EtherDevice) {
   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit(%d)...\n", unitNumber));
   struct EtherbridgeUnit * etherUnit = EtherDevice->ed_Units[unitNumber];
   if (etherUnit) {

      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():1 (deactivate unit number %d)\n", unitNumber));

      struct Task * unittask = (struct Task *) etherUnit->eu_Proc;
      etherUnit->eu_Proc = (struct Process *) FindTask(0L);

      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():2 (waiting for unit process is terminated...)\n"));

      //The unit process itself frees all used interrupt handlers...

      //Shutdown Device Unit Process. Wait until he knowledges...
      Signal(unittask, SIGBREAKF_CTRL_F);
      Wait(SIGBREAKF_CTRL_F);

      DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():22\n"));

      FreeMem(etherUnit, sizeof(struct EtherbridgeUnit));

      EtherDevice->ed_Units[unitNumber] = NULL;
      etherUnit = NULL;
   }

   //Now safety deallocate any Janus Service.
   //disconnectPCService();

   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit():3\n"));

   DEBUGOUT((VERBOSE_DEVICE,"ExpungeUnit()...end.\n"));
}

/**
 * This is the entry point for the Device Unit process.
 */
__saveds
void DevProcEntry()
{     
    struct Process *proc;
    struct EtherbridgeUnit *etherUnit;
    struct StartupMessage *startupMessage;
    struct IOSana2Req *ios2;
    ULONG waitmask,signals;
    BYTE msgPortSignalBit;
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
    etherUnit    = (struct EtherbridgeUnit *)startupMessage->Unit;

    /* Attempt to allocate a signal bit for our Unit MsgPort. */
    msgPortSignalBit = AllocSignal(-1L);
    if(msgPortSignalBit != -1)
    {
        /* Set up our Unit's MsgPort. */
        etherUnit->eu_Unit.unit_MsgPort.mp_SigBit  = msgPortSignalBit;
        etherUnit->eu_Unit.unit_MsgPort.mp_SigTask = (struct Task *)proc;
        etherUnit->eu_Unit.unit_MsgPort.mp_Flags   = PA_SIGNAL;

        /* Initialize our Transmit Request lists. Because it's a Message port
         * Forbid and Permit is used. */
        etherUnit->eu_Tx = CreateMsgPort();
 
        /* alles in Ordnung */
        etherUnit->eu_Proc = proc;
    }    

    /* Reply to our startup message */
    ReplyMsg((struct Message *)startupMessage);    

    /* Check etherUnit->eu_Proc to see if everything went okay up
       above. */
    if(etherUnit->eu_Proc)    
    {    
        // Signal for Janus Service Funktion beschaffen
        etherUnit->eu_Sigbit = AllocSignal(-1);
    
        // Init DOS Notify for etherbridge.config file
        configFileNotifySigBit = AllocSignal(-1);
        configNotify.nr_Name                         = (char*)sConfigFile;
        configNotify.nr_Flags                        = NRF_SEND_SIGNAL;
        configNotify.nr_stuff.nr_Signal.nr_Task      = &etherUnit->eu_Proc->pr_Task;
        configNotify.nr_stuff.nr_Signal.nr_SignalNum = configFileNotifySigBit;
        StartNotify(&configNotify);
        
        ULONG transportMask = (1L << etherUnit->eu_Tx->mp_SigBit) //<= Signal Message Port (TX packet)
                            | (1L << etherUnit->eu_Sigbit);       //<= JanusIntHandler or PollHandler (RX Packet)

        waitmask = (1L<<msgPortSignalBit)  |  transportMask | SIGBREAKF_CTRL_F | (1L<<configFileNotifySigBit);

        for(;;)
        {
            DEBUGOUT((VERBOSE_DEVICE,"\nDevice Unit Process: Just wait for signals\n"));
            signals = Wait(waitmask);
            DEBUGOUT((VERBOSE_DEVICE,"\nDevice Unit Process: Process event (signals=0x%lx)\n",signals));

            /* Have we been signaled to shut down? */
            if (signals & SIGBREAKF_CTRL_F) {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: SIGBREAKF_CTRL_F received.\n"));
               break;
            }
         


            //Read / Writes ?
            while((etherUnit->eu_State & ETHERUF_ONLINE) &&
                  (serviceWritePackets(etherUnit, EtherDevice) || serviceReadPackets(etherUnit, EtherDevice))){
               //Nothing. Repeat until nothing can be done.
            };

            // An new or more IO Request(s) to perform?
            if ((signals & (1l << msgPortSignalBit))) {
               while ((ios2 = (void *) GetMsg((void *) etherUnit)) != NULL) {
                  DEBUGOUT((VERBOSE_DEVICE, "Device Unit Process: Perform IORequest: cmd=%ld\n\n", ios2->ios2_Req.io_Command));
                  PerformIO(ios2, EtherDevice);
                  DEBUGOUT((VERBOSE_DEVICE, "Device Unit Process: IORequest finished\n"));
               }
            }

            // Wurde die Config-Datei veraendert ? (Dos-Notify)
            if (signals & (1L<<configFileNotifySigBit))
            {
                //configdatei neu einlesen
                DEBUGOUT((VERBOSE_DEVICE,"receive file notify signal!\n\n"));
                ReadConfig(EtherDevice);
            }

            // Count Signals from PC ( "VBeam Poll" and "Janus Interrupt Handler", both together )
            if (signals & (1L << etherUnit->eu_Sigbit)) {
               DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Signaling from PC Side\n"));
               sigBitCntr++;
            }
         }

         DEBUGOUT((VERBOSE_DEVICE,"Unit Process: Cleaning up all signal handlers...\n"));

         //Uninstall any interrupt handler. Do this only by the unit process because signals from it is
         //used by the signals handlers...
         //uninstallHandler();

         //Terminate the PC Side Server (send him a command), after the unit process is terminated.
         //ShutDownPCServer();

         //Disconnect Janus Service only after the unit process had died.
         //He may used the janus structures...
        // disconnectPCService();

         // Notify der Configdatei beenden
         EndNotify(&configNotify);
         bzero(&configNotify, sizeof (configNotify));

         FreeSignal(etherUnit->eu_Sigbit);
         etherUnit->eu_Sigbit = -1;
         FreeSignal(msgPortSignalBit);
         msgPortSignalBit = -1;
         FreeSignal(configFileNotifySigBit);
         configFileNotifySigBit = -1;


         /* Signal the other side we're exiting.  Make sure that
            we exit before they wake up by using the same trick
            when replying to Workbench startup messages. */
         DEBUGOUT((VERBOSE_DEVICE,"Device Unit Process: Terminating...bye bye\n"));
         Forbid();
         Signal((struct Task *)etherUnit->eu_Proc,SIGBREAKF_CTRL_F);
    }
    else {
        /* Something went wrong in the init code.  Drop out. */
        if(msgPortSignalBit != -1) {
           FreeSignal(msgPortSignalBit);
           msgPortSignalBit = -1;
        }
    } 
}


/*
 * Hinweis: Folgende Kommandos dürfen NUR vom Etherunit Task ausgeführt werden (weil Signalhandler installiert werden,
 * die vom Device task empfange werden müssen):
 *  - S2_ONLINE
 *  - S2_GetStationAddress
 *  - S2_CONFIGINTERFACE
 *  - DevCmdBurstOut
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
VOID DevBeginIO(struct IOSana2Req *ios2,
                struct Library * deviceBase)
{
   register UWORD Cmd = ios2->ios2_Req.io_Command;
   if(((1L << Cmd) & PERFORM_NOW) || (Cmd >= NSCMD_DEVICEQUERY) )
   {
      //Processed now!
      PerformIO(ios2,EtherDevice);
   }
   else
   {
      //Processed later by the unit process...
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
               struct EtherbridgeDevice *EtherDevice)
{
    struct EtherbridgeUnit *etherUnit;

    etherUnit    = (struct EtherbridgeUnit *)ios2->ios2_Req.io_Unit;

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
VOID TermIO(struct IOSana2Req *ios2,struct EtherbridgeDevice *EtherDevice)
{
    DEBUGOUT((VERBOSE_DEVICE,"TermIO ioreq=0x%lx\n", ios2));

    if(!(ios2->ios2_Req.io_Flags & IOF_QUICK)) {
       if (ios2->ios2_Req.io_Message.mn_ReplyPort != NULL ) {
          ReplyMsg(&ios2->ios2_Req.io_Message);
       } else {
          Alert(1); //no reply port????
       }
    }
}

/*
** This routine is called whenever an "important" SANA-II event occurs.
*/
VOID DoEvent(ULONG events,
             struct EtherbridgeUnit *etherUnit,
             struct EtherbridgeDevice *EtherDevice)
{
   struct IOSana2Req *ios2;
   struct IOSana2Req *ios2_next;
   BOOL eventFired = false;

   DEBUGOUT((VERBOSE_DEVICE,"### DoEvent: Fire Event 0x%lx\n", events));

   Forbid();
   //Check ALL pending event requests
   ios2 = (struct IOSana2Req *) etherUnit->eu_Events.mlh_Head;
   while (ios2->ios2_Req.io_Message.mn_Node.ln_Succ) {
      ios2_next = (struct IOSana2Req *) ios2->ios2_Req.io_Message.mn_Node.ln_Succ;
      /* Are they waiting for any of these events? */
      if (ios2->ios2_WireError & events) {
         ios2->ios2_Req.io_Error = 0;
         ios2->ios2_WireError = events;
         Remove((struct Node *) ios2);
         TermIO(ios2, EtherDevice);
         eventFired = true;
      }
      ios2 = ios2_next;
   }
   Permit();

   if (!eventFired) {
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
            /* Queue the Write Request for the etherbridge.device process. */
            ios2->ios2_Req.io_Flags &= ~IOF_QUICK;
            Forbid();
            ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
            PutMsg((APTR)etherUnit->eu_Tx,(APTR)ios2);
            Permit();
        }
        else
        {
            /* Sorry, the packet is too long! */
            ios2->ios2_Req.io_Error = S2ERR_MTU_EXCEEDED;
            ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
            TermIO(ios2,EtherDevice);
            DoEvent(S2EVENT_TX,etherUnit,EtherDevice);
        }
    }
    else
    {
        /* Sorry, we're offline */
        ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        ios2->ios2_WireError = S2WERR_UNIT_OFFLINE;
        TermIO(ios2,EtherDevice);
    }
}
///
 
/*
** This function handles CMD_ONLINE commands.
*/
VOID DevCmdOnline(STDETHERARGS)
{
   struct DateStamp StartTime;

   DEBUGOUT((VERBOSE_DEVICE,"DevCmdOnline()\n"));

   if(!(etherUnit->eu_State & ETHERUF_CONFIG)) {
      ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
      ios2->ios2_WireError    = S2WERR_NOT_CONFIGURED;
      goto end;
   }

   if (!(etherUnit->eu_State & ETHERUF_ONLINE)) {

      //TODO connect

      /* In case someone wants to know...*/
      DoEvent(S2EVENT_ONLINE, etherUnit, EtherDevice);

      // Zeit des auf Online-gehends merken
      DateStamp(&StartTime);
      GlobStat.LastStart.tv_secs = StartTime.ds_Days * 24 * 60 * 60 + StartTime.ds_Minute * 60;
      GlobStat.LastStart.tv_micro = StartTime.ds_Tick;

      //Promisc Modus ?
      /*
      if (EtherDevice->ed_Device.lib_Flags & ETHERUF_PROMISC)
         SetPromMode(true);
      else
         SetPromMode(false);
         */
   }

   end:

   TermIO(ios2,EtherDevice);
   DEBUGOUT((VERBOSE_DEVICE,"DevCmdOnline() ends.\n"));
}
///


void DevCmdOffline(STDETHERARGS)
{
      DEBUGOUT((VERBOSE_DEVICE,"\nDevCmdOffline()\n"));

      if (etherUnit->eu_State & ETHERUF_ONLINE)
      {
         etherUnit->eu_State &= (~ETHERUF_ONLINE);

         DoEvent(S2EVENT_OFFLINE, etherUnit, EtherDevice);

         // Alle evtl. ReadRequests abbrechen....
         DevCmdFlush(NULL, etherUnit, EtherDevice);

         //TODO go Offline
      }

      // going offline without an iorequest is possible!
      if (ios2)
         TermIO(ios2, EtherDevice);

      DEBUGOUT((VERBOSE_DEVICE,"DevCmdOffline() end\n"));
   }
///

///VOID DevCmdReadPacket(STDETHERARGS)
VOID DevCmdReadPacket(STDETHERARGS)
{
   struct BufferManagement *bm;

   DEBUGOUT((VERBOSE_DEVICE,"\n*DevCmdReadPacket(type=%ld,ioreq=0x%lx)\n", (ULONG)ios2->ios2_PacketType, ios2));
   if (etherUnit->eu_State & ETHERUF_ONLINE) {
      /* Find the appropriate queue for this IO request */
      ObtainSemaphore(&etherUnit->eu_BuffMgmtLock);
      bm = (struct BufferManagement *) etherUnit->eu_BuffMgmt.mlh_Head;
      while (bm->bm_Node.mln_Succ) {
         if (bm == (struct BufferManagement *) ios2->ios2_BufferManagement) {
            ObtainSemaphore((APTR) &bm->bm_RxQueueLock);
            CHECK_ALREADY_QUEUED((struct List *) &bm->bm_RxQueue, (struct Node *) ios2);

            //Ensure that the request is marked as "pending" because the request can only be queued here...
            ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;

            AddTail((struct List *) &bm->bm_RxQueue, (struct Node *) ios2);
            ReleaseSemaphore((APTR) &bm->bm_RxQueueLock);
            break;
         }
         bm = (struct BufferManagement *) bm->bm_Node.mln_Succ;
      }
      ReleaseSemaphore(&etherUnit->eu_BuffMgmtLock);
      /* Did we fall of the end of the list? */
      if (!bm->bm_Node.mln_Succ) {
         ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
         ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
         TermIO(ios2, EtherDevice);
      }
   } else {
      /* Sorry, we're offline */
      ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
      ios2->ios2_WireError = S2WERR_UNIT_OFFLINE;
      TermIO(ios2, EtherDevice);
   }
}
///

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
        CHECK_ALREADY_QUEUED((struct List *)&etherUnit->eu_ReadOrphan, (struct Node *)ios2);
        AddTail((struct List *)&etherUnit->eu_ReadOrphan, (struct Node *)ios2);
        ReleaseSemaphore((APTR)&etherUnit->eu_ReadOrphanLock);
    }
    else
    {
        /* Sorry, we're offline */
       DEBUGOUT((VERBOSE_DEVICE,"  Failed readorphan. not online!...\n"));
        ios2->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        ios2->ios2_WireError    = S2WERR_UNIT_OFFLINE;
        TermIO(ios2,EtherDevice);
    }
}
///

/*
** This function handles S2_DEVICEQUERY comands.
*/
VOID DevCmdDeviceQuery(STDETHERARGS) {
   DEBUGOUT((VERBOSE_DEVICE,"\n*DeviceQuery(ios2=0x%lx, ios2_StatData=0x%lx)\n", ios2, ios2->ios2_StatData));

   struct Sana2DeviceQuery *deviceStatisticData = (struct Sana2DeviceQuery *) ios2->ios2_StatData;
   if (deviceStatisticData) {

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
   } else {
      ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
      ios2->ios2_WireError = S2WERR_NULL_POINTER;
   }

   DEBUGOUT((VERBOSE_DEVICE,"  SizeSupplied=%ld\n", deviceStatisticData->SizeSupplied));

   DEBUGOUT((VERBOSE_DEVICE,"DeviceQuery finished. Error=0x%ld\n", ios2->ios2_Req.io_Error));

   TermIO(ios2, etherDevice);
}

///

///VOID DevCmdGetStationAddress(STDETHERARGS)
/*
** This function handles S2_GETSTATIONADDRESS commands.
** This is usually the first command that is called by an stack.
**
*/
VOID DevCmdGetStationAddress(STDETHERARGS)
{
    DEBUGOUT((VERBOSE_DEVICE,"\n*DevCmdGetStationAddress\n"));

    //TODO get station address

    TermIO(ios2,EtherDevice);
}
///


/**
 * This function handles S2_CONFIGINTERFACE commands.
 * Device unit is configured (only once) AND set online!
 */
VOID DevCmdConfigInterface(STDETHERARGS)
{
   DEBUGOUT((VERBOSE_DEVICE,"*ConfigInterface(addr = ")); printEthernetAddress(ios2->ios2_SrcAddr); DEBUGOUT((VERBOSE_DEVICE,")\n"));

    /* Note: we may only be configured once. */
    if(!(etherUnit->eu_State & ETHERUF_CONFIG))
    {
       /* Now sets our address to whatever the protocol stack says to use. */
       copyEthernetAddress(ios2->ios2_SrcAddr,etherUnit->eu_StAddr);
       etherUnit->eu_State |= ETHERUF_CONFIG;
   
       //Set device also online
       DevCmdOnline(ios2,etherUnit,EtherDevice);
    }
    else
    {
        DEBUGOUT((VERBOSE_DEVICE,"Error: Device is already configured!"));

        /* Sorry, we're already configured. */
        ios2->ios2_Req.io_Error = S2ERR_BAD_STATE;
        ios2->ios2_WireError = S2WERR_IS_CONFIGURED;
        TermIO(ios2,EtherDevice);
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
            CHECK_ALREADY_QUEUED((struct List *)&etherUnit->eu_Track,(struct Node     *)stats);
            AddTail((struct List *)&etherUnit->eu_Track,(struct Node     *)stats);
            print(5,"Track packet type: "); printi(5,ios2->ios2_PacketType);
        }
    }
    ReleaseSemaphore((APTR)&etherUnit->eu_TrackLock);

    TermIO(ios2,EtherDevice);
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

    TermIO(ios2,EtherDevice);
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

      if ((etherUnit->eu_State & ETHERUF_ONLINE) && w_param)
      {
         //Nur wenn online , da "w_param" nur definiert zw. Online und Offline !!!
         //Stat->PacketsReceived      = w_param->irec;
         //Stat->PacketsSent          = w_param->ixmit;
         //Stat->BadData              = w_param->ierrors;
      }

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

   TermIO(ios2,EtherDevice);
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
      TermIO(ios2, EtherDevice);
      return;
   }

    /* Queue anything else */
    Forbid();
    CHECK_ALREADY_QUEUED((struct List *)&etherUnit->eu_Events,(struct Node *)ios2);
    AddTail((struct List *)&etherUnit->eu_Events,(struct Node *)ios2);
    Permit();
}

///void getmcaf(struct ETHERDevice *EtherDevice, ULONG * af)
void getmcaf(struct EtherbridgeDevice *EtherDevice, ULONG * af)      
{
   register UBYTE *cp, c;
   register ULONG crc;
   register int i, len;
   struct MCAF_Adresse * ActNode;


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

///VOID DevCmdAddMulti(STDETHERARGS)
VOID DevCmdAddMulti(STDETHERARGS)
{
   // neue Multicastadresse
   
   // speicher fuer struktur besorgen
   struct MCAF_Adresse * NewMCAF;
   struct MCAF_Adresse * ActNode;

   print(VERBOSE_DEVICE,"*AddMulticast\n");

   ObtainSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
   //Gibts die Adresse bereits in der Liste ?
   ActNode = GET_FIRST(EtherDevice->ed_MCAF);
   while(IS_VALID(ActNode))
   { 
      // ist das unsere Adresse ?
      if (memcmp( ActNode->MCAF_Adr, ios2->ios2_SrcAddr, 6)==0)
      {
         //gibts bereits => ende !
         TermIO(ios2,EtherDevice);
         goto end;
      }
      ActNode = GET_NEXT(ActNode);
   }

   //Adresse existiert noch nicht!!

   //Speicher besorgen
   NewMCAF = (APTR)AllocMem(sizeof(struct MCAF_Adresse), MEMF_ANY);
   if (NewMCAF)
   { 
      //Adresse kopieren
      copyEthernetAddress( ios2->ios2_SrcAddr, NewMCAF->MCAF_Adr);
   
      // und in Liste der MCAF's 
      AddHead((APTR)&EtherDevice->ed_MCAF,(APTR)NewMCAF);
      print(VERBOSE_DEVICE," Neue Multicastadresse :\n");
      printEthernetAddress(NewMCAF->MCAF_Adr);
      TermIO(ios2,EtherDevice);
      
      //TODO:
   }
   else      
    {
        //kein speicher mehr frei ????????
        ios2->ios2_Req.io_Error = S2ERR_SOFTWARE;
        ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
        TermIO(ios2,EtherDevice);
    }

   end:
   ReleaseSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
}
///

///VOID DevCmdRemMulti(STDETHERARGS)
VOID DevCmdRemMulti(STDETHERARGS)
{
   bool found = FALSE;
   struct MCAF_Adresse * ActNode;
   int i;

   ObtainSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);

   ActNode = (struct MCAF_Adresse * )GET_FIRST(EtherDevice->ed_MCAF);
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
         FreeMem(ActNode, sizeof(struct MCAF_Adresse));
         print(VERBOSE_DEVICE," MulticastAdresse wieder entfernt!\n");
         TermIO(ios2,EtherDevice);               
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
        TermIO(ios2,EtherDevice);
   }
   else
   {
      //Multicastfilter aktualisieren wenn moeglich
      //TODO:
   }

   ReleaseSemaphore((APTR)&EtherDevice->ed_MCAF_Lock);
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
      /*DevAddSpecialStat(Stat, statType++, "NIC_Overrun", w_param ? w_param->NIC_rec_overrun : 0);
      DevAddSpecialStat(Stat, statType++, "CmdReg0", w_param ? w_param->cmd[0] : 0);
      DevAddSpecialStat(Stat, statType++, "CmdReg1", w_param ? w_param->cmd[1] : 0);
      DevAddSpecialStat(Stat, statType++, "PCRelRingPtr", w_param ? w_param->rec_ptr_cur : 0);
      DevAddSpecialStat(Stat, statType++, "AmiRelRingPtr", etherUnit ? etherUnit->last_rx_buf_ptr : 0);
      DevAddSpecialStat(Stat, statType++, "PollInts", iPollInt);
      DevAddSpecialStat(Stat, statType++, "eu_SigBitCnts", sigBitCntr);
      DevAddSpecialStat(Stat, 100, "Transmit Errors", w_param ? w_param->collisions : 0); //Is requested by EtherPrefs...
      */
   }
   else
   {
      ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
      ios2->ios2_WireError    = S2WERR_NULL_POINTER;
   }


   DEBUGOUT((VERBOSE_DEVICE, "*DevCmdGetSpecialStats...end\n"));

   TermIO(ios2,EtherDevice);
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
                               EB_DEVICEBURSTOUT,
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
    TermIO(ios2,EtherDevice);
}

/**
 * To calculate the remaining stacksize is not as easy as it seems. See Amiga Guru Book.
 * So we check the total stack only, not the real used stack space...
 */
BOOL checkStackSpace(int minStackSize) {
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


/*
 * Checks if the given node is already queued in the device list.
 * If so alert!
 */
void CHECK_ALREADY_QUEUED(struct List * list, struct Node * node) {

#if DEBUG > 0
   struct Node * minNode = list->lh_Head;
   while (minNode->ln_Succ) {
      minNode = minNode->ln_Succ;

      //Is this node already in list? This is an software Error!
      if (node == minNode) {
         Alert(1820);
      }
   }
#endif

}
