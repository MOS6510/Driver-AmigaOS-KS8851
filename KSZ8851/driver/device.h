/* 
 * Copyright (C) 1997 - 1999, 2018 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

#ifndef DEVICE_H
#define DEVICE_H

#include <dos/dostags.h>
#include <dos/rdargs.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/utility_protos.h>
#include <clib/timer_protos.h>
#include <clib/alib_stdio_protos.h>
#include <clib/intuition_protos.h>

#include <exec/types.h>
#include <exec/devices.h>
#include <exec/ports.h>
#include <exec/semaphores.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <exec/interrupts.h>
#include <dos/dos.h>
#include <devices/sana2.h>
#include <devices/serial.h>
#include <utility/hooks.h>


#include <string.h>

#define ED_MAXUNITS 1
#define ETHER_PRI 0

#define EB_DEVICEBURSTOUT 0x2000

//Maximale Anzahl der Durchläufe des Device Task nach einem Janus Signal
#define TASKLOOPMAX 30


extern short kbdDelayTicks;  // Keyboard delay when sending scan codes to PC




#define bool char
#define TRUE  1
#define true  1
#define FALSE 0
#define false 0

enum BridgeboardType { None = 0, A2088OrA2286, A2386 };



/*
** Typedef's for the SANA-II callback functions.
*/
typedef BOOL  (*SANA2_CFB)(APTR to, APTR from, LONG length);
typedef BOOL  (*SANA2_CTB)(APTR to, APTR from, LONG length);
typedef ULONG (*HOOK_FUNC)(struct Hook *hook,struct IOSana2Req * io,APTR message);


struct SuperS2PTStats
{
    struct MinNode               ss_Node;
    ULONG                        ss_PType;
    struct Sana2PacketTypeStats  ss_Stats;
};


/*
** Unit Data Structure
*/
struct EtherbridgeUnit
{
    struct Unit            eu_Unit;          /* AmigaOS Standard Unit Structure */
    UBYTE                  eu_UnitNum;       /* Unit number */
    UBYTE                  eu_Reserved0;     /* Padding */
    struct Device          *eu_Device;       /* Pointer to our device node */
    UBYTE                  eu_StAddr[6];     /* Our current "hardware" address */
    USHORT                 eu_Pad2;          /* Padding */
    ULONG                  eu_MTU;           /* Maximum Transmission Unit */
    ULONG                  eu_State;         /* Various state information */
    struct Process       * eu_Proc;          /* NB: This points to the Task, not the MsgPort */

    struct MsgPort         *eu_Tx;           /* Pending CMD_WRITE's for the Device Unit Process input.
                                                There is no semaphore lock (MessagePort). Instead Forbid and Permit is used. */

    struct MinList         eu_Events;        /* Pending S2_ONEVENT's: No Lock! Use Forbid() / Permit(). */

    struct MinList         eu_Track;         /* List of packet types being tracked */
    struct SignalSemaphore eu_TrackLock;     /* Lock for...*/

    struct MinList         eu_BuffMgmt;      /* List of all Buffer Management Entries (clients) */
    struct SignalSemaphore eu_BuffMgmtLock;  /* Lock of the List of Buffer Management Entries */

    struct MinList         eu_ReadOrphan;    // Liste aller ReadOrphan-Requests
    struct SignalSemaphore eu_ReadOrphanLock;// Semaphore fuer die "ReadOrphanliste"

    struct Sana2DeviceStats eu_Stats;        /* Global device statistics */
    struct SuperS2PTStats* eu_IPTrack;       /* For tracking IP packets */
    ULONG                  last_rx_buf_ptr;  /* letzte Kopie des Zeigers aus dem JanMem */
    ULONG                  eu_Sigbit;        /* Signalbit fuer JanusIntHandler oder VBeam Poll Handler. */
};


/*
** Message passed to the Unit Process at
** startup time.
*/
struct StartupMessage
{
   struct Message Msg;
   struct Unit *Unit;
   struct Device  *Device;
};


/*
** Device Data Structure itself
*/
struct EtherbridgeDevice
{
   struct Library          ed_Device;              // Needed Device structure
   BPTR                    ed_SegList;             // Segment list when open device. Used when unload device.
   struct EtherbridgeUnit *ed_Units[ED_MAXUNITS];  // Only "one" unit at this time
   struct StartupMessage   ed_Startup;
   struct Hook             ed_DummyPFHook;         // Default Dummy Hook (inkl. Asm-Funktion)
   struct MinList          ed_MCAF;                // Liste der benutzten Multicastadressen
   struct SignalSemaphore  ed_MCAF_Lock;           // Semaphore fuer MCAF-Liste
   struct SignalSemaphore  ed_DeviceLock;          // General List Lock. Used only when open or close device
   enum BridgeboardType    ed_BBType;              // Type of used Bridgeboard

   BOOL                    ed_showMessages;      // allowed to display user messages when an error occurred.
   BOOL                    ed_startDosServer;      // Start DOS Server automatically?
   BOOL                    ed_startPacketDriver;   // Start DOS packet driver automatically
};

/**
 * A Buffer Management Entry
 */
struct BufferManagement
{
    struct MinNode         bm_Node;
    SANA2_CFB              bm_CopyFromBuffer;
    SANA2_CTB              bm_CopyToBuffer;
    struct Hook          *  bm_PacketFilterHook;   // SANA-II V2 Callback Hook
    struct MinList          bm_RxQueue;            // Pending CMD_READ Requests
    struct SignalSemaphore  bm_RxQueueLock;        // Lock for this bm_RxQueue
};


// Struktur zum merken der Multicastadressen
struct MCAF_Adresse
{
    struct MinNode      MCAF_Node;
    UBYTE               MCAF_Adr[6];
    UBYTE               MCAF_Unit;
};


//############ MACROS #################################
 
#define CopyFromBuffer(to,from,len)      CopyBuf(bm->bm_CopyFromBuffer  ,to,from,len)
#define CopyToBuffer(to,from,len)        CopyBuf(bm->bm_CopyToBuffer    ,to,from,len)

#define STDETHERARGS struct IOSana2Req *ios2,struct EtherbridgeUnit *etherUnit, struct EtherbridgeDevice *etherDevice


#define GET_FIRST(a) ((APTR)(((struct MinList)(a)).mlh_Head))
#define GET_NEXT(a) ((APTR)(((struct MinNode *)a)->mln_Succ))
#define IS_VALID(a) (((struct MinNode *)a)->mln_Succ)


//############# prototypes ###################

//Device Interface (public)
//struct Library * DevInit(BPTR DeviceSegList, struct Library * DevBasePointer, struct Library * execBase);
//VOID  DevOpen (struct IOSana2Req *ios2, ULONG s2unit, ULONG s2flags, struct Library * devPointer);
//BPTR  DevClose(struct IOSana2Req *ios2, struct Library * Devbase);
//BPTR  DevExpunge(struct Library * DevBase);
VOID  DevBeginIO(struct IOSana2Req *,struct Library * DevBase);
ULONG DevAbortIO( struct IOSana2Req *,struct Library * DevBase);

//internal private functions
void  PerformIO(struct IOSana2Req *,struct EtherbridgeDevice * DevBase);
void  TermIO(struct IOSana2Req * ,struct EtherbridgeDevice * DevBase);
VOID  ExpungeUnit(UBYTE unitNumber, struct EtherbridgeDevice *etherDevice);
struct EtherbridgeUnit *InitETHERUnit(ULONG,struct EtherbridgeDevice * ETHERDevice);
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
VOID DevCmdBurstOut(STDETHERARGS);

VOID DoEvent(ULONG events,struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice);
bool serviceReadPackets(struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice);
VOID getmcaf(struct EtherbridgeDevice *etherDevice, ULONG * af);
VOID OpenBusyWIn(char*);
VOID CloseBusyWin();
void ShowMessage(char * cText, void * ArgList);
void DevProcEntry();

BOOL checkStackSpace();



#endif
