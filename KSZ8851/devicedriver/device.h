/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the AmigaOS part of the device driver.

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
#include <stdbool.h>

#include "copybuffs.h"
#include "hardware-interface.h"


//Number of supported device units of the driver
#define ED_MAXUNITS 1

//Priority of the device unit task
#define UNIT_PROCESS_PRIORITY 0

//Initialized Ethernet broadcast address
extern const UBYTE BROADCAST_ADDRESS[6];

// --------------------------------- MACROS -----------------------------------------------------------------

#define GET_FIRST(a) ((APTR)(((struct MinList)(a)).mlh_Head))
#define GET_NEXT(a) ((APTR)(((struct MinNode *)a)->mln_Succ))
#define IS_VALID(a) (((struct MinNode *)a)->mln_Succ)

#define STDETHERARGS struct IOSana2Req *ios2,struct DeviceDriverUnit *etherUnit, struct DeviceDriver *etherDevice

#if DEBUG > 0
void printEthernetAddress(unsigned char * addr );
#define PRINT_ETHERNET_ADDRESS(a) printEthernetAddress(a)
#else
#define PRINT_ETHERNET_ADDRESS(a)
#endif

// --------------------------------- TYPES ------------------------------------------------------------------

struct SuperS2PTStats
{
    struct MinNode               ss_Node;
    ULONG                        ss_PType;
    struct Sana2PacketTypeStats  ss_Stats;
};

/*
** Device Unit Data Structure
*/
struct DeviceDriverUnit
{
    struct Unit            eu_Unit;          /* AmigaOS Standard Unit Structure */
    UBYTE                  eu_UnitNum;       /* Unit number */
    UBYTE                  eu_Reserved0;     /* Padding */
    struct Device          *eu_Device;       /* Pointer to our device node */
    UBYTE                  eu_StAddr[6];     /* Our current "hardware" address */
    USHORT                 eu_Pad2;          /* Padding */
    ULONG                  eu_MTU;           /* Maximum Transmission Unit */
    ULONG                  eu_State;         /* Various state information */
    struct Process         *eu_Proc;         /* NB: This points to the Task, not the MsgPort */

    struct MsgPort         *eu_Tx;           /* Pending CMD_WRITE's for the Device Unit Process input.
                                                There is no semaphore lock (MessagePort).
                                                Instead Forbid and Permit is used. */

    struct MinList         eu_Events;        /* Pending S2_ONEVENT's: No Lock! Use Forbid() / Permit(). */

    struct MinList         eu_Track;         /* List of packet types being tracked */
    struct SignalSemaphore eu_TrackLock;     /* Lock for...*/

    struct MinList         eu_BuffMgmt;      /* List of all Buffer Management Entries (clients) */
    struct SignalSemaphore eu_BuffMgmtLock;  /* Lock of the List of Buffer Management Entries */

    struct MinList         eu_ReadOrphan;    // Liste aller ReadOrphan-Requests
    struct SignalSemaphore eu_ReadOrphanLock;// Semaphore fuer die "ReadOrphanliste"

    struct Sana2DeviceStats eu_Stats;        /* Global device statistics */
    struct SuperS2PTStats* eu_IPTrack;       /* For tracking IP packets */

    NetInterface *         eu_lowLevelDriver; //Access to the low level hardware driver...
    ULONG                  eu_lowLevelDriverSignalNumber; //The signal number used for low level signaling...
};


/*
** Message passed to the Unit Process at
** startup time.
*/
struct StartupMessage
{
   struct Message Msg;
   struct Unit    *Unit;
   struct Device  *Device;
};


/*
** Device Data Structure itself. Holds all official and private stuff of this SANA2 device.
*/
struct DeviceDriver
{
   struct Library          ed_Device;              // Needed Device structure
   BPTR                    ed_SegList;             // Segment list when open device. Used when unload device.
   struct DeviceDriverUnit *ed_Units[ED_MAXUNITS]; // All units of the device (currently only one)
   struct StartupMessage   ed_Startup;             //
   struct Hook             ed_DummyPFHook;         // Default Dummy Hook (assembler function)
   struct MinList          ed_MCAF;                // List of used multicast addresses
   struct SignalSemaphore  ed_MCAF_Lock;           // Semaphore for MCAF list
   struct SignalSemaphore  ed_DeviceLock;          // General List Lock. Used only when open or close device
};

/**
 * A Buffer Management Entry
 */
struct BufferManagement
{
    struct MinNode         bm_Node;
    SANA2_CFB              bm_CopyFromBuffer;
    SANA2_CTB              bm_CopyToBuffer;
    APTR                   bm_CopyFromBufferDMA;
    APTR                   bm_CopyToBufferDMA;
    struct Hook          *  bm_PacketFilterHook;   // SANA-II V2 Callback Hook
    struct MinList          bm_RxQueue;            // Pending CMD_READ Requests
    struct SignalSemaphore  bm_RxQueueLock;        // Lock for this bm_RxQueue
};

// Stores a used multicast address
struct MCAF_Address
{
    struct MinNode      MCAF_Node;
    UBYTE               MCAF_Adr[6];               // Multicast MAC address
    UBYTE               MCAF_Unit;                 // Count of using
};

// --------------------------------- PROTOTYPES -------------------------------------------------------------

struct Library * DeviceInit(BPTR DeviceSegList, struct Library * DevBasePointer, struct Library * execBase);
void DeviceOpen(struct IOSana2Req *ios2, ULONG s2unit, ULONG s2flags, struct Library * devPointer);
BPTR DeviceClose(struct IOSana2Req *ios2, struct Library * DevBase);
BPTR DeviceExpunge(struct Library * DevBase);
VOID DeviceBeginIO(struct IOSana2Req * ios2, struct Library * deviceBase);
ULONG DeviceAbortIO( struct IOSana2Req *ios2, struct Library * DevPointer);

BOOL isBroadcastEthernetAddress(UBYTE * addr);
void copyEthernetAddress(const BYTE * from, BYTE * to);
void setErrorOnRequest(struct IOSana2Req *ios2, BYTE io_Error, ULONG ios2_WireError);
struct IOSana2Req * safeGetNextWriteRequest(struct DeviceDriverUnit * etherUnit);
VOID DoEvent(ULONG events,struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice);
bool serviceReadPackets(struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice);

#endif
