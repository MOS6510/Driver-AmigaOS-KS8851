/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the hardware specific part of the device

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

#include "hal.h"

#include "../devdebug.h"
#include "../tools.h"

#include <clib/alib_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/types.h>
#include <sys/types.h>

#include <exec/interrupts.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
//#include <stdlib.h>
//#include <string.h>
//#include <strings.h>

//#include <netinet/in.h>

// ----------------------------- CONSTS ---------------------------------------------------------------------

// ----------------------------- LOCALS ---------------------------------------------------------------------

#define MAX(a,b)  (a) > (b) ? (a) : (b);
#define printf neverUsePrintfHere
typedef void (*VoidFunc )(void);

#define IP  0x800
#define ARP 0x806

// ----------------------------- PRIVATE PROTOTYPES ---------------------------------------------------------

// ----------------------------- IMPL -----------------------------------------------------------------------

/**
 * Probing the hardware.
 * @return
 */
bool hal_probe()
{
   //printf("Test");
   DEBUGOUT((1, "hal_probe\n"));
   return true;
}

/**
 * Constructor
 */
void hal_initialization()
{
   DEBUGOUT((1, "hal_initialization\n"));
}

/**
 * Destructor
 */
void hal_deinitialization()
{
   DEBUGOUT((1, "hal_deinitialization\n"));
}

/**
 * Read packets...
 *
 * @param etherUnit
 * @param etherDevice
 * @return
 */
bool hal_serviceReadPackets(struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice)
{
   DEBUGOUT((1, "hal_serviceReadPackets\n"));
   return false;
}

/**
 * Write packets...
 *
 * @param etherUnit
 * @param etherDevice
 * @return
 */
bool hal_serviceWritePackets(struct DeviceDriverUnit *etherUnit, struct DeviceDriver *etherDevice)
{
   DEBUGOUT((1, "hal_serviceWritePackets\n"));
   return false;
}

