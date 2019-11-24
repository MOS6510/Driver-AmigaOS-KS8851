/*
 * Copyright (C) 2019 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// This module defines the assembler stub functions of the SANA2 buffer management.


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

#include <devices/sana2.h>

#include <time.h>

#include "../../os_includes/libnix/stabs.h"
#include "helper.h"


/**
 * Main copy buffer management function. Used for both: CopyToBuf and CopyFromBuf calls
 *
 * @param funcPtr
 * @param to
 * @param from
 * @param len
 * @return
 */
SAVEDS
ULONG CopyBuf(APTR funcPtr, ULONG to, ULONG from, ULONG len) {
   register ULONG _res __asm("d0");
   register APTR a2  __asm("a2") = funcPtr;
   register ULONG a1 __asm("a1") = from;
   register ULONG a0 __asm("a0") = to;
   register ULONG d0 __asm("d0") = len;

   //TODO: I'm not sure if register save is needed here
   __asm volatile ("movem.l  d1-d7/a0-a6,-(a7)");

   __asm volatile ("jsr a2@"
         : "=r" (_res)
         : "r" (a0), "r" (a1) ,"r" (a2),"r" (d0)
         : "memory");

   __asm volatile ("movem.l  (a7)+,d1-d7/a0-a6");

   return _res;
}

/**
 * SANA2 filter function callback sub entry.
 *
 * @param hook
 * @param ioreq
 * @param rawPktData
 * @return
 */
SAVEDS
ULONG CallFilterHook(struct Hook * hook, struct IOSana2Req * ioreq, APTR rawPktData) {
   register ULONG _res __asm("d0");
   register APTR  a3 __asm("a3") = hook->h_Entry; //Assembler entry point function of the hook.
   register ULONG a0 __asm("a0") = (ULONG)hook;
   register APTR  a2 __asm("a2") = ioreq;
   register APTR  a1 __asm("a1") = rawPktData;

   //TODO: I'm not sure if register save is needed here
   __asm volatile ("movem.l  d1-d7/a0-a6,-(a7)");

   __asm __volatile ("jsr a3@"
         : "=r" (_res)
         : "r" (a0), "r" (a1) ,"r" (a2),"r" (a3)
         : "memory");

   __asm volatile ("movem.l  (a7)+,d1-d7/a0-a6");

   return _res;
}

