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


bool hal_serviceReadPackets (struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice);

bool hal_serviceWritePackets(struct DeviceDriverUnit *etherUnit,struct DeviceDriver *etherDevice);

ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData);

#endif
