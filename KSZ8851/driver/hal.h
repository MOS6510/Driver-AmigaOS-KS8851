/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the hardware specific part of the device driver (KSZ8851 Ethernet Chip)
// ("HAL" => "hardware abstraction layer")

#ifndef __HAL__H
#define __HAL__H

#include <sys/types.h>
#include "device.h"

//The default config file of the SANA2 device
#define DEFAULT_DEVICE_CONFIG_FILE "env:sana2/" DEVICE_NAME ".config"

//Defines the minimal stack size in bytes that must exists when device is opened.
#define STACK_SIZE_MINIMUM 5000

/**
 * Probing the hardware.
 * @return true: hardware is ok and detected. Fals if not.
 */
bool hal_probe(void);

/**
 * Init the hardware in any way...
 */
void hal_initialization(void);

/**
 * prepare for destroying.
 */
void hal_deinitialization(void);


bool connectPCService(struct DeviceDriverUnit *etherUnit, char * sPCCmd, char * sPCPktDrvCmd, char * sPCPktDrvPar);
bool serviceReadPackets (struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice);
bool serviceWritePackets(struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice) ;
LONG milliSecondsToTicks(ULONG milliSeconds);
void SetIrqAndIOBase(int IOBase, unsigned char Irq);
void SetPromMode(BOOL);
unsigned int waitForCmdExecuted(UBYTE Port,int MaxMilliTime);
void copyEthernetAddress(const BYTE * from, BYTE * to);
ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData);

#endif
