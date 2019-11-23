/* 
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module contains the hardware specific part of the device driver
// ("HAL" => "hardware abstraction layer")

#ifndef __HAL__H
#define __HAL__H

#include <sys/types.h>
#include "device.h"

//The default config file of the SANA2 device
#define DEFAULT_DEVICE_CONFIG_FILE "env:sana2/" DEVICE_NAME ".config"

extern const UBYTE BROADCAST_ADDRESS[6];

#if DEBUG >0
void printEthernetAddress(UBYTE * addr);
#else
   #define printEthernetAddress(addr)
#endif

bool connectPCService(struct EtherbridgeUnit *etherUnit, char * sPCCmd, char * sPCPktDrvCmd, char * sPCPktDrvPar);
void disconnectPCService();
bool isEtherbridgeServiceDeleted();
void CallPC();
bool serviceReadPackets (struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice);
bool serviceWritePackets(struct EtherbridgeUnit *etherUnit,struct EtherbridgeDevice *etherDevice) ;
char getNextFreeTransmitBufferIndexWait();
LONG milliSecondsToTicks(ULONG milliSeconds);
void SetIrqAndIOBase(int IOBase, unsigned char Irq);
bool ShutDownPCServer();
void uninstallHandler();
BOOL installPollHandler(struct Task * forTask, ULONG signalBitForTask);
void uninstallPollHandler();
void SetPromMode(BOOL);
void ShutDownPktDrv();
unsigned int waitForCmdExecuted(UBYTE Port,int MaxMilliTime);
void copyEthernetAddress(const BYTE * from, BYTE * to);
ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData);

#endif
