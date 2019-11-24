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

#include "devdebug.h"
#include "hal.h"
#include "tools.h"
#include "version.h"

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

