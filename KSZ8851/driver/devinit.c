/*
 * Copyright (C) 1997 - 1999, 2018 by Heiko Pruessing
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
*/

// Etherbridge "device" startup code.

// ------------------- INCLUDE ------------------------

// define as unresolved external references (proto/xxx.h) and compiler will link to auto(matically) open library
#include <proto/exec.h>
#include <proto/utility.h>
#include <exec/types.h>

#include "devdebug.h"
#include "tools.h"
#include "libinit.h"
#include "helper.h"
#include "device.h"
#include "version.h"


// ------------------- Prototypes ------------------------

SAVEDS struct Library * _DevInit(REG(a0,BPTR SegListPointer),
                                 REG(d0,struct Library * dev),
                                 REG(a6,struct Library * exec_base));
extern const APTR InitTab[4];

//init and deinit auto-open-library of "libnix" which has to be handles by hand here.
extern void __initlibraries(void);
extern void __exitlibraries(void);
extern long __oslibversion;

extern struct ExecBase * SysBase;

// ------------------- IMPLEMENTATION ------------------------

/**
 * First function of the device. Dummy function. "libnix" needs "exit()" to be referenced.
 * Also this is save way to prevent execution from CLI.
 */
asm(".globl _exit");
asm("_exit:");
asm("moveq #0,d0");
asm("rts");


static const struct Resident RomTag __attribute__((used)) =
{
  RTC_MATCHWORD,
  (struct Resident *)&RomTag,
  (struct Resident *)&RomTag+1,
  RTF_AUTOINIT,
  DEVICE_VERSION,
  NT_DEVICE,
  0,
  (char *)DevName,
  (char *)DevIdString,
  (APTR)&InitTab
};

const APTR InitTab[4] =
{
  (APTR)sizeof(struct EtherbridgeDevice),
  (APTR)&__FuncTable__[1],
  (APTR)NULL,           //init dev base by hand (in function "DevInit()" )
  (APTR)&_DevInit
};

      
/**
 * Init Device.
 *
 * @param REG(a0,BPTR SegListPointer)
 * @param REG(d0,struct Library * dev)
 * @param REG(a6,struct Library * exec_base)
 * @return
 */
SAVEDS
struct Library * _DevInit(REG(a0,BPTR SegListPointer),
                          REG(d0,struct Library * dev),
                          REG(a6,struct Library * exec_base))             
{   
   //Take the exec base... 
   SysBase = (struct ExecBase *)exec_base;

   //Activate "Automatic Open Library support of libnix".
   //Because we don't use an device startup here we must all the auto-open-function by hand.
   //If not all libraries are not open automatically especially the "utility.library" when using "libnix"
   //will crash dividing integer in function "ito2()".
   __oslibversion = 37l; //open all libs with at least Version 37
   __initlibraries();

   return DevInit(SegListPointer,  (struct libBase *)dev, exec_base );
}

/**
 * Open Device.
 *
 * @param REG(d0, ULONG unit)
 * @param REG(a1, APTR iorq)
 * @param REG(d1, ULONG flags)
 * @param REG(a6, struct Library * dev)
 */
SAVEDS
VOID _DevOpen(REG(d0, ULONG unit),
              REG(a1, APTR iorq),
              REG(d1, ULONG flags),
              REG(a6, struct Library * dev))
{
  DevOpen((long unsigned int)iorq, (void*)unit, flags, (struct libBase *)dev);
}

/**
 * Close Device.
 *
 * @param REG(a1,APTR iorq)
 * @param REG(a6,struct Library * DevBase)
 * @return
 */
SAVEDS
BPTR _DevClose(REG(a1,APTR iorq),REG(a6,struct Library * DevBase))
{
   return DevClose(iorq, (struct libBase *)DevBase);
}

/**
 * Expunge device.
 *
 * @param REG(a6,struct Library * DevBase)
 * @return
 */
SAVEDS
BPTR _DevExpunge(REG(a6,struct Library * DevBase))
{
   BPTR res = DevExpunge((struct libBase *)DevBase);
   DEBUGOUT((VERBOSE_DEVICE,"_DevExpunge. calling __exitlibraries() \n"));

   //Freeing all auto-open libraries again after expunge lib.
   __exitlibraries();

   DEBUGOUT((VERBOSE_DEVICE,"_DevExpunge. After calling __exitlibraries() \n"));
   DEBUGOUT((VERBOSE_DEVICE,"_DevExpunge. Back to AmigaOS with 0x%lx \n", res));

   return res;
}

/**
 * BeginIO.
 *
 * @param REG(a1,struct IOSana2Req *ios2)
 * @param REG(a6,struct Library * dev)
 */
asm ( ".globl __DevBeginIO \n"
      "__DevBeginIO: \n"
      "  movem.l d0-d7/a0-a6,-(sp) \n"

      "  move.l  a6,-(sp) \n"
      "  move.l  a1,-(sp) \n"
      "  jsr     _DevBeginIO \n"
      "  lea     8(sp),sp \n"

      "  movem.l (sp)+,d0-d7/a0-a6 \n"
      "  rts");

/**
 * AbortIO.
 *
 * @param REG(a1,struct IOSana2Req *ios2)
 * @param REG(a6,struct Library * dev)
 * @return
 */
asm ( ".globl __DevAbortIO \n"
      "__DevAbortIO: \n"
      "  movem.l d1-d7/a0-a6,-(sp) \n"

      "  move.l  a6,-(sp) \n"
      "  move.l  a1,-(sp) \n"
      "  jsr     _DevAbortIO \n"
      "  lea     8(sp),sp \n"

      "  movem.l (sp)+,d1-d7/a0-a6 \n"
      "  rts");

/**
 * Needed as dummy function for device function table.
 * WARNING: This fuction MUST exists!
 * @return null
 */
SAVEDS
LONG _DevNullFunction( void )
{
   return 0;
}

/**
 * Now add all "Device Functions" to the Library/Device.
 * WARNING: All functions must exist! If not no warning is generated and the ADD2LIST is not called.
 *   Mailfunctions here would reorder the function table and funny things happen when using the library!
 */
ADD2LIST(_DevOpen,         __FuncTable__,22);
ADD2LIST(_DevClose,        __FuncTable__,22);
ADD2LIST(_DevExpunge,      __FuncTable__,22);
ADD2LIST(_DevNullFunction, __FuncTable__,22); // => Unused AmigaOS function (but must be present in table!)
ADD2LIST(_DevBeginIO,      __FuncTable__,22);
ADD2LIST(_DevAbortIO,      __FuncTable__,22);
ADDTABL_END();


